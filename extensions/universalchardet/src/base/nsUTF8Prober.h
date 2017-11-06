/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUTF8Prober_h__
#define nsUTF8Prober_h__

#include "nsCharSetProber.h"
#include "nsCodingStateMachine.h"

class nsUTF8Prober: public nsCharSetProber {
public:
  nsUTF8Prober(){mNumOfMBChar = 0;
                mCodingSM = new nsCodingStateMachine(&UTF8SMModel);
                Reset(); }
  virtual ~nsUTF8Prober(){delete mCodingSM;}
  nsProbingState HandleData(const char* aBuf, uint32_t aLen) override;
  const char* GetCharSetName() override {return "UTF-8";}
  nsProbingState GetState(void) override {return mState;}
  void      Reset(void) override;
  float     GetConfidence(void) override;

protected:
  nsCodingStateMachine* mCodingSM;
  nsProbingState mState;
  uint32_t mNumOfMBChar;
};

#endif /* nsUTF8Prober_h__ */
