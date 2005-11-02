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
#ifndef nsCodingStateMachine_h__
#define nsCodingStateMachine_h__

#include "nsPkgInt.h"

typedef enum {
   eStart = 0,
   eError = 1,
   eItsMe = 2 
} nsSMState;

#define GETCLASS(c) GETFROMPCK(((unsigned char)(c)), mModel->classTable)

//state machine model
typedef struct 
{
  nsPkgInt classTable;
  PRUint32 classFactor;
  nsPkgInt stateTable;
  PRUint32* charLenTable;
  const char* name;
} SMModel;

class nsCodingStateMachine {
public:
  nsCodingStateMachine(SMModel* sm){
          mCurrentState = eStart;
          mModel = sm;
        };
  nsSMState NextState(char c){
    //for each byte we get its class , if it is first byte, we also get byte length
    PRUint32 byteCls = GETCLASS(c);
    if (mCurrentState == eStart)
    { 
      mCurrentBytePos = 0; 
      mCurrentCharLen = mModel->charLenTable[byteCls];
    }
    //from byte's class and stateTable, we get its next state
    mCurrentState=(nsSMState)GETFROMPCK(mCurrentState*(mModel->classFactor)+byteCls,
                                       mModel->stateTable);
    mCurrentBytePos++;
    return mCurrentState;
  };
  PRUint32  GetCurrentCharLen(void) {return mCurrentCharLen;};
  void      Reset(void) {mCurrentState = eStart;};
  const char * GetCodingStateMachine() {return mModel->name;};

protected:
  nsSMState mCurrentState;
  PRUint32 mCurrentCharLen;
  PRUint32 mCurrentBytePos;

  SMModel *mModel;
};

extern SMModel UTF8SMModel;
extern SMModel Big5SMModel;
extern SMModel EUCJPSMModel;
extern SMModel EUCKRSMModel;
extern SMModel EUCTWSMModel;
extern SMModel GB18030SMModel;
extern SMModel SJISSMModel;
extern SMModel UCS2BESMModel;


extern SMModel HZSMModel;
extern SMModel ISO2022CNSMModel;
extern SMModel ISO2022JPSMModel;
extern SMModel ISO2022KRSMModel;

#endif /* nsCodingStateMachine_h__ */

