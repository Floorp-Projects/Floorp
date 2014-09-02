/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsEUCKRProber_h__
#define nsEUCKRProber_h__

#include "nsCharSetProber.h"
#include "nsCodingStateMachine.h"
#include "CharDistribution.h"

class nsEUCKRProber: public nsCharSetProber {
public:
  explicit nsEUCKRProber(bool aIsPreferredLanguage)
    :mIsPreferredLanguage(aIsPreferredLanguage)
  {mCodingSM = new nsCodingStateMachine(&EUCKRSMModel);
    Reset();
  }
  virtual ~nsEUCKRProber(void){delete mCodingSM;}
  nsProbingState HandleData(const char* aBuf, uint32_t aLen);
  const char* GetCharSetName() {return "EUC-KR";}
  nsProbingState GetState(void) {return mState;}
  void      Reset(void);
  float     GetConfidence(void);

protected:
  void      GetDistribution(uint32_t aCharLen, const char* aStr);
  
  nsCodingStateMachine* mCodingSM;
  nsProbingState mState;

  //EUCKRContextAnalysis mContextAnalyser;
  EUCKRDistributionAnalysis mDistributionAnalyser;
  char mLastChar[2];
  bool mIsPreferredLanguage;

};


#endif /* nsEUCKRProber_h__ */

