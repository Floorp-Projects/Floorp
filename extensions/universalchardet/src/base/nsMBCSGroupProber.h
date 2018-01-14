/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMBCSGroupProber_h__
#define nsMBCSGroupProber_h__

#include "nsSJISProber.h"
#include "nsUTF8Prober.h"
#include "nsEUCJPProber.h"

#define NUM_OF_PROBERS    3

class nsMBCSGroupProber: public nsCharSetProber {
public:
  nsMBCSGroupProber();
  virtual ~nsMBCSGroupProber();
  nsProbingState HandleData(const char* aBuf, uint32_t aLen) override;
  const char* GetCharSetName() override;
  nsProbingState GetState(void) override {return mState;}
  void      Reset(void) override;
  float     GetConfidence(void) override;

#ifdef DEBUG_chardet
  void  DumpStatus();
#endif
#ifdef DEBUG_jgmyers
  void GetDetectorState(nsUniversalDetector::DetectorState (&states)[nsUniversalDetector::NumDetectors], uint32_t &offset);
#endif

protected:
  nsProbingState mState;
  nsCharSetProber* mProbers[NUM_OF_PROBERS];
  bool            mIsActive[NUM_OF_PROBERS];
  int32_t mBestGuess;
  uint32_t mActiveNum;
  uint32_t mKeepNext;
};

#endif /* nsMBCSGroupProber_h__ */
