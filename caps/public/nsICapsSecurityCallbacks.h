/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsICapsSecurityCallbacks_h__
#define nsICapsSecurityCallbacks_h__

#include "nsISupports.h"

#define NS_ICAPSSECURITYCALLBACKS_IID \
{ /* 480c65f0-9396-11d2-bd92-00805f8ae3f4 */ \
0x480c65f0, 0x9396, 0x11d2, \
{0xbd, 0x92, 0x00, 0x80, 0x5f, 0x8a, 0xe3, 0xf4} }

typedef struct nsFrameWrapper {
    void *iterator;
} nsFrameWrapper;

class nsICapsSecurityCallbacks : public nsISupports {

public:

  NS_IMETHOD NewNSJSJavaFrameWrapper(void *aContext, struct nsFrameWrapper ** aWrapper) = 0;
  NS_IMETHOD FreeNSJSJavaFrameWrapper(struct nsFrameWrapper *aWrapper) = 0;
  NS_IMETHOD GetStartFrame(struct nsFrameWrapper *aWrapper) = 0;
  NS_IMETHOD IsEndOfFrame(struct nsFrameWrapper *aWrapper, PRBool* aReturn) = 0;
  NS_IMETHOD IsValidFrame(struct nsFrameWrapper *aWrapper, PRBool* aReturn) = 0;
  NS_IMETHOD GetNextFrame(struct nsFrameWrapper *aWrapper, int *aDepth, void** aReturn) = 0;
  NS_IMETHOD OJIGetPrincipalArray(struct nsFrameWrapper *aWrapper, void** aReturn) = 0;
  NS_IMETHOD OJIGetAnnotation(struct nsFrameWrapper *aWrapper, void** aReturn) = 0;
  NS_IMETHOD OJISetAnnotation(struct nsFrameWrapper *aWrapper, void *aPrivTable,  void** aReturn) = 0;

};

#endif // nsICapsSecurityCallbacks_h__
