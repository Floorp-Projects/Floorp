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

#ifndef nsEscCharSetProber_h__
#define nsEscCharSetProber_h__

#include "nsCharSetProber.h"
#include "nsCodingStateMachine.h"

#define NUM_OF_ESC_CHARSETS   4

class nsEscCharSetProber: public nsCharSetProber {
public:
  nsEscCharSetProber(void);
  virtual ~nsEscCharSetProber(void);
  nsProbingState HandleData(const char* aBuf, PRUint32 aLen);
  const char* GetCharSetName() {return mDetectedCharset;};
  nsProbingState GetState(void) {return mState;};
  void      Reset(void);
  float     GetConfidence(void){return (float)0.99;};
  void      SetOpion() {};

protected:
  void      GetDistribution(PRUint32 aCharLen, const char* aStr);
  
  nsCodingStateMachine* mCodingSM[NUM_OF_ESC_CHARSETS] ;
  PRUint32    mActiveSM;
  nsProbingState mState;
  const char *  mDetectedCharset;
};

#endif /* nsEscCharSetProber_h__ */

