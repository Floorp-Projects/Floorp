/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

// for S-JIS encoding, obeserve characteristic:
// 1, kana character (or hankaku?) often have hight frequency of appereance
// 2, kana character often exist in group
// 3, certain combination of kana is never used in japanese language

#ifndef nsEUCJPProber_h__
#define nsEUCJPProber_h__

#include "nsCharSetProber.h"
#include "nsCodingStateMachine.h"
#include "JpCntx.h"
#include "CharDistribution.h"

class nsEUCJPProber: public nsCharSetProber {
public:
  nsEUCJPProber(void){mCodingSM = new nsCodingStateMachine(&EUCJPSMModel);
                      Reset();};
  virtual ~nsEUCJPProber(void){delete mCodingSM;};
  nsProbingState HandleData(const char* aBuf, PRUint32 aLen);
  const char* GetCharSetName() {return "EUC-JP";};
  nsProbingState GetState(void) {return mState;};
  void      Reset(void);
  float     GetConfidence(void);
  void      SetOpion() {};

protected:
  void      GetDistribution(PRUint32 aCharLen, const char* aStr);
  
  nsCodingStateMachine* mCodingSM;
  nsProbingState mState;
  PRUint32 mNumOfRoman;
  PRUint32 mNumOfHankaku;
  PRUint32 mNumOfKana;
  PRUint32 mNumOfKanji;
  PRUint32 mNumOfMisc;

  EUCJPContextAnalysis mContextAnalyser;
  EUCJPDistributionAnalysis mDistributionAnalyser;

  char mLastChar[2];
};


#endif /* nsEUCJPProber_h__ */

