// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

// jet finder task
//
// Author: Jochen Klein, Nima Zardoshti, Raymond Ehlers

#include "PWGJE/TableProducer/jetfinder.h"

using namespace o2;
using namespace o2::framework;
using namespace o2::framework::expressions;

#include "Framework/runDataProcessing.h"

template <typename JetTable, typename ConstituentTable, typename ConstituentSubTable>
struct JetFinderTask {
  Produces<JetTable> jetsTable;
  Produces<ConstituentTable> constituentsTable;
  Produces<ConstituentSubTable> constituentsSubTable;
  OutputObj<TH2F> h2JetPt{"h2_jet_pt"};
  OutputObj<TH2F> h2JetPhi{"h2_jet_phi"};
  OutputObj<TH2F> h2JetEta{"h2_jet_eta"};
  OutputObj<TH2F> h2JetNTracks{"h2_jet_ntracks"};
  OutputObj<TH1F> hJetPt{"h_jet_pt"};
  OutputObj<TH1F> hJetPhi{"h_jet_phi"};
  OutputObj<TH1F> hJetEta{"h_jet_eta"};
  OutputObj<TH1F> hJetNTracks{"h_jet_ntracks"};

  // event level configurables
  Configurable<float> vertexZCut{"vertexZCut", 10.0f, "Accepted z-vertex range"};

  // track level configurables
  Configurable<float> trackPtMin{"trackPtMin", 0.15, "minimum track pT"};
  Configurable<float> trackPtMax{"trackPtMax", 1000.0, "maximum track pT"};
  Configurable<float> trackEtaMin{"trackEtaMin", -0.8, "minimum track eta"};
  Configurable<float> trackEtaMax{"trackEtaMax", 0.8, "maximum track eta"};
  Configurable<float> trackPhiMin{"trackPhiMin", -999, "minimum track phi"};
  Configurable<float> trackPhiMax{"trackPhiMax", 999, "maximum track phi"};
  Configurable<std::string> trackSelections{"trackSelections", "globalTracks", "set track selections"};

  // cluster level configurables
  Configurable<std::string> clusterDefinitionS{"clusterDefinition", "kV3Default", "cluster definition to be selected, e.g. V3Default"};
  Configurable<float> clusterEtaMin{"clusterEtaMin", -0.7, "minimum cluster eta"}; // For ECMAL: |eta| < 0.7, phi = 1.40 - 3.26
  Configurable<float> clusterEtaMax{"clusterEtaMax", 0.7, "maximum cluster eta"};  // For ECMAL: |eta| < 0.7, phi = 1.40 - 3.26
  Configurable<float> clusterPhiMin{"clusterPhiMin", -999, "minimum cluster phi"};
  Configurable<float> clusterPhiMax{"clusterPhiMax", 999, "maximum cluster phi"};
  Configurable<float> clusterEnergyMin{"clusterEnergyMin", 0.5, "minimum cluster energy in EMCAL (GeV)"};
  Configurable<float> clusterTimeMin{"clusterTimeMin", -999., "minimum Cluster time (ns)"};
  Configurable<float> clusterTimeMax{"clusterTimeMax", 999., "maximum Cluster time (ns)"};
  Configurable<bool> clusterRejectExotics{"clusterRejectExotics", true, "Reject exotic clusters"};

  // jet level configurables
  Configurable<std::vector<double>> jetRadius{"jetRadius", {0.4}, "jet resolution parameters"};
  Configurable<float> jetPtMin{"jetPtMin", 0.0, "minimum jet pT"};
  Configurable<float> jetPtMax{"jetPtMax", 1000.0, "maximum jet pT"};
  Configurable<int> jetTypeParticleLevel{"jetTypeParticleLevel", 1, "Type of stored jets at MC particle leevel. 0 = full, 1 = charged, 2 = neutral"};
  Configurable<int> jetAlgorithm{"jetAlgorithm", 2, "jet clustering algorithm. 0 = kT, 1 = C/A, 2 = Anti-kT"};
  Configurable<int> jetRecombScheme{"jetRecombScheme", 0, "jet recombination scheme. 0 = E-scheme, 1 = pT-scheme, 2 = pT2-scheme"};
  Configurable<float> jetGhostArea{"jetGhostArea", 0.005, "jet ghost area"};
  Configurable<int> ghostRepeat{"ghostRepeat", 1, "set to 0 to gain speed if you dont need area calculation"};
  Configurable<bool> DoTriggering{"DoTriggering", false, "used for the charged jet trigger to remove the eta constraint on the jet axis"};
  Configurable<bool> DoRhoAreaSub{"DoRhoAreaSub", false, "do rho area subtraction"};
  Configurable<bool> DoConstSub{"DoConstSub", false, "do constituent subtraction"};

  Service<O2DatabasePDG> pdg;
  std::string trackSelection;

  JetFinder jetFinder;
  std::vector<fastjet::PseudoJet> inputParticles;

  void init(InitContext const&)
  {
    trackSelection = static_cast<std::string>(trackSelections);

    h2JetPt.setObject(new TH2F("h2_jet_pt", "jet p_{T};p_{T} (GeV/#it{c})",
                               100, 0., 100., 10, 0.05, 1.05));
    h2JetPhi.setObject(new TH2F("h2_jet_phi", "jet #phi;#phi",
                                80, -1., 7., 10, 0.05, 1.05));
    h2JetEta.setObject(new TH2F("h2_jet_eta", "jet #eta;#eta",
                                70, -0.7, 0.7, 10, 0.05, 1.05));
    h2JetNTracks.setObject(new TH2F("h2_jet_ntracks", "jet n;n constituents",
                                    30, 0., 30., 10, 0.05, 1.05));

    hJetPt.setObject(new TH1F("h_jet_pt", "jet p_{T};p_{T} (GeV/#it{c})",
                              100, 0., 100.));
    hJetPhi.setObject(new TH1F("h_jet_phi", "jet #phi; #phi",
                               140, -7.0, 7.0));
    hJetEta.setObject(new TH1F("h_jet_eta", "jet #eta; #eta",
                               30, -1.5, 1.5));
    hJetNTracks.setObject(new TH1F("h_jet_ntracks", "jet N tracks ; N tracks",
                                   150, -0.5, 99.5));

    if (DoRhoAreaSub) {
      jetFinder.setBkgSubMode(JetFinder::BkgSubMode::rhoAreaSub);
    }
    if (DoConstSub) {
      jetFinder.setBkgSubMode(JetFinder::BkgSubMode::constSub);
    }

    jetFinder.etaMin = trackEtaMin;
    jetFinder.etaMax = trackEtaMax;
    jetFinder.jetPtMin = jetPtMin;
    jetFinder.jetPtMax = jetPtMax;
    jetFinder.algorithm = static_cast<fastjet::JetAlgorithm>(static_cast<int>(jetAlgorithm));
    jetFinder.recombScheme = static_cast<fastjet::RecombinationScheme>(static_cast<int>(jetRecombScheme));
    jetFinder.ghostArea = jetGhostArea;
    jetFinder.ghostRepeatN = ghostRepeat;
    if (DoTriggering) {
      jetFinder.isTriggering = true;
    }
  }

  o2::aod::EMCALClusterDefinition clusterDefinition = o2::aod::emcalcluster::getClusterDefinitionFromString(clusterDefinitionS.value);
  Filter collisionFilter = nabs(aod::collision::posZ) < vertexZCut;
  Filter trackCuts = (aod::track::pt >= trackPtMin && aod::track::pt < trackPtMax && aod::track::eta > trackEtaMin && aod::track::eta < trackEtaMax && aod::track::phi >= trackPhiMin && aod::track::phi <= trackPhiMax); // do we need eta cut both here and in globalselection?
  Filter partCuts = (aod::mcparticle::pt >= trackPtMin && aod::mcparticle::pt < trackPtMax);
  Filter clusterFilter = (o2::aod::emcalcluster::definition == static_cast<int>(clusterDefinition) && aod::emcalcluster::eta > clusterEtaMin && aod::emcalcluster::eta < clusterEtaMax && aod::emcalcluster::phi >= clusterPhiMin && aod::emcalcluster::phi <= clusterPhiMax && aod::emcalcluster::energy >= clusterEnergyMin && aod::emcalcluster::time > clusterTimeMin && aod::emcalcluster::time < clusterTimeMax && (clusterRejectExotics && aod::emcalcluster::isExotic != true));

  void processDummy(aod::Collisions const& collision)
  {
  }

  PROCESS_SWITCH(JetFinderTask, processDummy, "Dummy process function turned on by default", true);

  void processChargedJets(soa::Filtered<soa::Join<aod::Collisions, aod::EvSels>>::iterator const& collision,
                          JetTracks const& tracks)
  {
    if (!selectCollision(collision)) {
      return;
    }

    LOG(debug) << "Process data charged!";
    inputParticles.clear();
    analyseTracks<JetTracks, JetTracks::iterator>(inputParticles, tracks, trackSelection);
    findJets(jetFinder, inputParticles, jetRadius, collision, jetsTable, constituentsTable, constituentsSubTable, DoConstSub);
  }

  PROCESS_SWITCH(JetFinderTask, processChargedJets, "Data jet finding for charged jets", false);

  void processNeutralJets(soa::Filtered<soa::Join<aod::Collisions, aod::EvSels>>::iterator const& collision,
                          JetClusters const& clusters)
  {
    if (!collision.alias()[kTVXinEMC]) {
      return;
    }
    LOG(debug) << "Process data neutral!";
    inputParticles.clear();
    analyseClusters(inputParticles, &clusters);
    findJets(jetFinder, inputParticles, jetRadius, collision, jetsTable, constituentsTable, constituentsSubTable, DoConstSub);
  }
  PROCESS_SWITCH(JetFinderTask, processNeutralJets, "Data jet finding for neutral jets", false);

  void processFullJets(soa::Filtered<soa::Join<aod::Collisions, aod::EvSels>>::iterator const& collision,
                       JetTracks const& tracks,
                       JetClusters const& clusters)
  {
    if (!collision.alias()[kTVXinEMC]) {
      return;
    }
    LOG(debug) << "Process data full!";
    inputParticles.clear();
    analyseTracks<JetTracks, JetTracks::iterator>(inputParticles, tracks, trackSelection);
    analyseClusters(inputParticles, &clusters);
    findJets(jetFinder, inputParticles, jetRadius, collision, jetsTable, constituentsTable, constituentsSubTable, DoConstSub);
  }

  PROCESS_SWITCH(JetFinderTask, processFullJets, "Data jet finding for full and neutral jets", false);

  void processParticleLevelJets(aod::McCollision const& collision, aod::McParticles const& particles)
  {
    // TODO: MC event selection?
    analyseParticles<aod::McParticles, aod::McParticles::iterator>(inputParticles, trackEtaMin, trackEtaMax, jetTypeParticleLevel, particles, pdg->Instance());
    findJets(jetFinder, inputParticles, jetRadius, collision, jetsTable, constituentsTable, constituentsSubTable, DoConstSub);
  }

  PROCESS_SWITCH(JetFinderTask, processParticleLevelJets, "Particle level jet finding", false);
};

using JetFinderDataCharged = JetFinderTask<o2::aod::Jets, o2::aod::JetConstituents, o2::aod::JetConstituentsSub>;
using JetFinderDataFull = JetFinderTask<o2::aod::FullJets, o2::aod::FullJetConstituents, o2::aod::FullJetConstituentsSub>;
using JetFinderDataNeutral = JetFinderTask<o2::aod::NeutralJets, o2::aod::NeutralJetConstituents, o2::aod::NeutralJetConstituentsSub>;
using JetFinderMCDetectorLevelCharged = JetFinderTask<o2::aod::MCDetectorLevelJets, o2::aod::MCDetectorLevelJetConstituents, o2::aod::MCDetectorLevelJetConstituentsSub>;
using JetFinderMCDetectorLevelFull = JetFinderTask<o2::aod::MCDetectorLevelFullJets, o2::aod::MCDetectorLevelFullJetConstituents, o2::aod::MCDetectorLevelFullJetConstituentsSub>;
using JetFinderMCDetectorLevelNeutral = JetFinderTask<o2::aod::MCDetectorLevelNeutralJets, o2::aod::MCDetectorLevelNeutralJetConstituents, o2::aod::MCDetectorLevelNeutralJetConstituentsSub>;
using JetFinderMCParticleLevelCharged = JetFinderTask<o2::aod::MCParticleLevelJets, o2::aod::MCParticleLevelJetConstituents, o2::aod::MCParticleLevelJetConstituentsSub>;
using JetFinderMCParticleLevelFull = JetFinderTask<o2::aod::MCParticleLevelFullJets, o2::aod::MCParticleLevelFullJetConstituents, o2::aod::MCParticleLevelFullJetConstituentsSub>;
using JetFinderMCParticleLevelNeutral = JetFinderTask<o2::aod::MCParticleLevelNeutralJets, o2::aod::MCParticleLevelNeutralJetConstituents, o2::aod::MCParticleLevelNeutralJetConstituentsSub>;
// using JetFinderHybridIntermediate = JetFinderTask<o2::aod::HybridIntermediateJets, o2::aod::HybridIntermediateJetConstituents, o2::aod::HybridIntermediateJetConstituentsSub>;

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{

  std::vector<o2::framework::DataProcessorSpec> tasks;

  tasks.emplace_back(
    adaptAnalysisTask<JetFinderDataCharged>(cfgc,
                                            SetDefaultProcesses{}, TaskName{"jet-finder-data-charged"}));

  tasks.emplace_back(
    adaptAnalysisTask<JetFinderDataFull>(cfgc,
                                         SetDefaultProcesses{}, TaskName{"jet-finder-data-full"}));

  tasks.emplace_back(
    adaptAnalysisTask<JetFinderDataNeutral>(cfgc,
                                            SetDefaultProcesses{}, TaskName{"jet-finder-data-neutral"}));

  tasks.emplace_back(
    adaptAnalysisTask<JetFinderMCDetectorLevelCharged>(cfgc,
                                                       SetDefaultProcesses{}, TaskName{"jet-finder-mcd-charged"}));

  tasks.emplace_back(
    adaptAnalysisTask<JetFinderMCDetectorLevelFull>(cfgc,
                                                    SetDefaultProcesses{}, TaskName{"jet-finder-mcd-full"}));

  tasks.emplace_back(
    adaptAnalysisTask<JetFinderMCDetectorLevelNeutral>(cfgc,
                                                       SetDefaultProcesses{}, TaskName{"jet-finder-mcd-neutral"}));

  // tasks.emplace_back(
  // adaptAnalysisTask<JetFinderHybridIntermediate>(cfgc,
  // SetDefaultProcesses{}, TaskName{"jet-finder-hybrid-intermedaite-full"}));

  tasks.emplace_back(
    adaptAnalysisTask<JetFinderMCParticleLevelCharged>(cfgc,
                                                       SetDefaultProcesses{}, TaskName{"jet-finder-mcp-charged"}));

  tasks.emplace_back(
    adaptAnalysisTask<JetFinderMCParticleLevelFull>(cfgc,
                                                    SetDefaultProcesses{}, TaskName{"jet-finder-mcp-full"}));

  tasks.emplace_back(
    adaptAnalysisTask<JetFinderMCParticleLevelNeutral>(cfgc,
                                                       SetDefaultProcesses{}, TaskName{"jet-finder-mcp-neutral"}));

  return WorkflowSpec{tasks};
}
