/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsEscCharSetProber_h__
#define nsEscCharSetProber_h__

#include "nsCharSetProber.h"
#include "nsCodingStateMachine.h"

#define NUM_OF_ESC_CHARSETS   4

class nsEscCharSetProber: public nsCharSetProber {
public:
  explicit nsEscCharSetProber(uint32_t aLanguageFilter);
  virtual ~nsEscCharSetProber(void);
  nsProbingState HandleData(const char* aBuf, uint32_t aLen);
  const char* GetCharSetName() {return mDetectedCharset;}
  nsProbingState GetState(void) {return mState;}
  void      Reset(void);
  float     GetConfidence(void){return (float)0.99;}

protected:
  void      GetDistribution(uint32_t aCharLen, const char* aStr);
  
  nsCodingStateMachine* mCodingSM[NUM_OF_ESC_CHARSETS] ;
  uint32_t    mActiveSM;
  nsProbingState mState;
  const char *  mDetectedCharset;
};

#endif /* nsEscCharSetProber_h__ */

