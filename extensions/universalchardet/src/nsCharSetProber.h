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
#ifndef nsCharSetProber_h__
#define nsCharSetProber_h__

#include "nscore.h"

typedef enum {
  eDetecting = 0,   //we are still detecting, no sure answer yet, but caller can ask for confidence.
  eFoundIt = 1,     //That's a positive answer
  eNotMe = 2        //negative answer
} nsProbingState;

#define SHORTCUT_THRESHOLD      (float)0.95

class nsCharSetProber {
public:
  virtual const char* GetCharSetName() = 0;
  virtual nsProbingState HandleData(const char* aBuf, PRUint32 aLen) = 0;
  virtual nsProbingState GetState(void) = 0;
  virtual void      Reset(void)  = 0;
  virtual float     GetConfidence(void) = 0;
  virtual void      SetOpion() = 0;
};

#endif /* nsCharSetProber_h__ */

