/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGB2312Prober_h__
#define nsGB2312Prober_h__

#include "nsCharSetProber.h"
#include "nsCodingStateMachine.h"
#include "CharDistribution.h"

// We use gb18030 to replace gb2312, because 18030 is a superset. 

class nsGB18030Prober: public nsCharSetProber {
public:
  nsGB18030Prober(bool aIsPreferredLanguage)
    :mIsPreferredLanguage(aIsPreferredLanguage)
  {mCodingSM = new nsCodingStateMachine(&GB18030SMModel);
    Reset();}
  virtual ~nsGB18030Prober(void){delete mCodingSM;}
  nsProbingState HandleData(const char* aBuf, PRUint32 aLen);
  const char* GetCharSetName() {return "gb18030";}
  nsProbingState GetState(void) {return mState;}
  void      Reset(void);
  float     GetConfidence(void);

protected:
  void      GetDistribution(PRUint32 aCharLen, const char* aStr);
  
  nsCodingStateMachine* mCodingSM;
  nsProbingState mState;

  //GB2312ContextAnalysis mContextAnalyser;
  GB2312DistributionAnalysis mDistributionAnalyser;
  char mLastChar[2];
  bool mIsPreferredLanguage;

};


#endif /* nsGB2312Prober_h__ */

