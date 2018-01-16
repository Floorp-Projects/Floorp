/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsLatin1Prober_h__
#define nsLatin1Prober_h__

#include "nsCharSetProber.h"

#define FREQ_CAT_NUM    4

class nsLatin1Prober: public nsCharSetProber {
public:
  nsLatin1Prober(void){Reset();}
  virtual ~nsLatin1Prober(void){}
  nsProbingState HandleData(const char* aBuf, uint32_t aLen) override;
  const char* GetCharSetName() override {return "windows-1252";}
  nsProbingState GetState(void) override {return mState;}
  void      Reset(void) override;
  float     GetConfidence(void) override;

#ifdef DEBUG_chardet
  virtual void  DumpStatus();
#endif

protected:

  nsProbingState mState;
  char mLastCharClass;
  uint32_t mFreqCounter[FREQ_CAT_NUM];
};

#endif /* nsLatin1Prober_h__ */

