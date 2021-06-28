#include "classes/DelphesClasses.h"
#include "ExRootAnalysis/ExRootTreeReader.h"
#include "ExRootAnalysis/ExRootResult.h"

#include <TClonesArray.h>
#include <TLorentzVector.h>
#include <TSystem.h>
#include <unistd.h>
#include <iostream>

#include "DelphesUtils/DelphesNtuple.hpp"
#include "DelphesUtils/GhostAssociation.hpp"

using namespace std;

void AnalysisEvents(ExRootTreeReader* treeReader, DelphesNtuple* ntuple, bool debug=false) {
  // TODO: 
  // 1) add 4 vector of the generated tau
  // 2) label gen jets produced with tau
  // 3) match tracks to jets
  // TODO: add di-tau
  TClonesArray *branchParticle = treeReader->UseBranch("Particle");
  TClonesArray *branchElectron = treeReader->UseBranch("Electron");
  TClonesArray *branchPhoton = treeReader->UseBranch("Photon");
  TClonesArray *branchMuon = treeReader->UseBranch("Muon");
  
  TClonesArray *branchTrack = treeReader->UseBranch("Track");
  TClonesArray *branchTower = treeReader->UseBranch("Tower");

  // TClonesArray *branchEFlowTrack = treeReader->UseBranch("EFlowTrack");
  // TClonesArray *branchEFlowPhoton = treeReader->UseBranch("EFlowPhoton");
  // TClonesArray *branchEFlowNeutralHadron = treeReader->UseBranch("EFlowNeutralHadron");

  TClonesArray *branchJet = treeReader->UseBranch("Jet");
  TClonesArray *branchGenJet = treeReader->UseBranch("GenJet");

  Long64_t allEntries = treeReader->GetEntries();
  printf("Total %lld entries\n", allEntries);

  Long64_t entry;
  Jet* jet;
  TObject *object;

  GenParticle *particle;
  Track *track;
  Tower *tower;

  Int_t i, j, pdgCode;
  unsigned k;
  float jetRadius, jetPtmin;
  jetRadius = 0.4, jetPtmin = 20.0;
  GhostAssociation::Config jetConfig = {jetRadius, jetPtmin, fastjet::antikt_algorithm};

  for(entry = 39; entry < allEntries; ++entry) {
    treeReader->ReadEntry(entry);
    ntuple->Clear();

    // at least one reco jet in the event
    if (branchJet->GetEntriesFast() < 1) continue;
    
    // save tracks
    // tracks will be matched to reco jets via ghost matching.
    vector<Track*> trackContainer;
    for(i = 0; i < branchTrack->GetEntriesFast(); ++i ) {
      track = (Track*) branchTrack->At(i);
      ntuple->FillTrack(track);
      trackContainer.push_back(track);
    }

    // Loop over all reco jets in event
    int n_jets, n_bjets, n_taujets;

    vector<Jet*> jetContainer;
    n_jets=0, n_bjets=0, n_taujets=0;
    for(i = 0; i < branchJet->GetEntriesFast(); ++i) {
      jet = (Jet*) branchJet->At(i);
      if (jet->PT < 25 || abs(jet->Eta) > 2.5) continue;

      ntuple->FillRecoJet(jet);
      n_jets ++;
      if(jet->BTag) n_bjets ++;
      if(jet->TauTag) n_taujets ++;
      if(debug) printf("Reco Jet: %d, %d, %.2f %.2f %.2f\n", i, jet->Constituents.GetEntriesFast(), jet->PT, jet->Eta, jet->Phi);

      // see which tracks are matched to this jet
      vector<int> trackIdx = GhostAssociation::Associate(jet, trackContainer, jetConfig);
      ntuple->FillRecoJetGhostTracks(trackIdx);
    }
    ntuple->FillRecoJetCnt(n_jets, n_bjets, n_taujets);
    if (n_jets < 1) continue;

    if (debug) {
      cout << "Event " << entry << ", Electrons: " << branchElectron->GetEntriesFast() \
           << ", Muons: " << branchMuon->GetEntriesFast() \
           << ", Photons: " << branchPhoton->GetEntriesFast() \
           << ", Jets: " << branchJet->GetEntriesFast() \
           << ", Gen Jets:" << branchGenJet->GetEntriesFast() \
           << ", Tracks: " << branchTrack->GetEntriesFast() \
           << ", Towers: " << branchTower->GetEntriesFast() \
           << endl;
    }
    
    // Loop over all truth jets in event
    n_jets=0, n_bjets=0, n_taujets=0;
    for(i = 0; i < branchGenJet->GetEntriesFast(); ++i) {
      jet = (Jet*) branchGenJet->At(i);
      n_jets ++;

      if (debug) printf("Truth Jet: %d, %d, %.2f %.2f %.2f\n", i, jet->Constituents.GetEntriesFast(), jet->PT, jet->Eta, jet->Phi);
      bool hasB = false;
      bool hasTau = false;
      int n_prongs = 0;
      for(j = 0; j < jet->Constituents.GetEntriesFast(); ++j) {
        object = jet->Constituents.At(j);
        if(object == 0) continue;
        if(object->IsA() == GenParticle::Class())
        {
          particle = (GenParticle*) object;
          GenParticle* m1 = (GenParticle*) branchParticle->At(particle->M1);
          if(debug) cout << "    GenPart pt: " << particle->PT << ", eta: " << particle->Eta << ", phi: " \
            << particle->Phi << ", ID: " << particle->PID << ", M1: " << m1->PID << endl;
          if(m1 && abs(m1->PID) == 15) {
            hasTau = true;
            n_prongs ++;
          } 
          if(m1 && abs(m1->PID) == 5) hasB = true;
        }
      }
      if (hasTau) {
        n_taujets ++; jet->TauTag = n_prongs;
      } else jet->TauTag = 0;

      if (hasB) {
        n_bjets ++; jet->BTag = 1;
      }
      ntuple->FillGenJet(jet);
    }
    ntuple->FillGenJetsCnt(n_jets, n_bjets, n_taujets);

    // save towers
    vector<Tower*> towerContainer;
    for(i = 0; i < branchTower->GetEntriesFast(); ++i) {
      tower = (Tower*) branchTower->At(i);
      ntuple->FillTower(tower);
      towerContainer.push_back(tower);
    }

    // loop over gen particles to find tau lepton
    for(i = 0; i < branchParticle->GetEntriesFast(); ++i) {
      particle = (GenParticle*) branchParticle->At(i);
      if(abs(particle->PID) == 15) {
        ntuple->FillTau(particle);
        if(debug) printf("GenParticle %d %d", particle->Status, particle->Charge);
      }
    }

    if (debug) printf("Jet container size: %ld, track container size: %ld, tower container size: %ld",
      jetContainer.size(), trackContainer.size(), towerContainer.size());

    ntuple->Fill();
    if(debug) break;
  }
}


int main(int argc, char** argv)
{
  bool help = false;
  bool debug = false;
  int opt;
  std::string file("/global/cfs/cdirs/m3443/usr/xju/TauStudies/run_Ztautau/Ntuple_ditau.root");
  std::string outname("test.root");

  while ((opt = getopt(argc, argv, "hf:o:d")) != -1) {
    switch(opt) {
    case 'f':
      file = optarg;
      break;
    case 'o':
      outname = optarg;
      break;
    case 'd':
      debug = true;
      break;
    case 'h':
      help = true;
    default:
      fprintf(stderr, "Usage: %s [-f FILE] [-o OUTNAME] [-h] [-d]\n", argv[0]);
      if (help) {
        printf("   -f FILE : read input file\n");
        printf("   -o OUTNAME: output anme\n");
        printf("   -h HELP: help message\n");
        printf("   -d DEBUG: print debug messages\n");
      }
      exit(1);
    }
  }

  TChain *chain = new TChain("Delphes");
  chain->Add(file.c_str());
  printf("Processing %s\n", file.c_str());

  auto ntuple = new DelphesNtuple(outname);
  ntuple->BookGenJets();
  bool withTowers = true;
  ntuple->BookRecoJets(withTowers);
  ntuple->BookTracks();
  ntuple->BookTowers();

  ExRootTreeReader *treeReader = new ExRootTreeReader(chain);

  AnalysisEvents(treeReader, ntuple, debug);

  delete chain;
  delete ntuple;
  delete treeReader;
  return 0;
}
