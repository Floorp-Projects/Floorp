/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
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
  const PRUint32* charLenTable;
  const char* name;
} SMModel;

class nsCodingStateMachine {
public:
  nsCodingStateMachine(const SMModel* sm) : mModel(sm) { mCurrentState = eStart; }
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
  }
  PRUint32  GetCurrentCharLen(void) {return mCurrentCharLen;}
  void      Reset(void) {mCurrentState = eStart;}
  const char * GetCodingStateMachine() {return mModel->name;}

protected:
  nsSMState mCurrentState;
  PRUint32 mCurrentCharLen;
  PRUint32 mCurrentBytePos;

  const SMModel *mModel;
};

extern const SMModel UTF8SMModel;
extern const SMModel Big5SMModel;
extern const SMModel EUCJPSMModel;
extern const SMModel EUCKRSMModel;
extern const SMModel EUCTWSMModel;
extern const SMModel GB18030SMModel;
extern const SMModel SJISSMModel;


extern const SMModel HZSMModel;
extern const SMModel ISO2022CNSMModel;
extern const SMModel ISO2022JPSMModel;
extern const SMModel ISO2022KRSMModel;

#endif /* nsCodingStateMachine_h__ */

