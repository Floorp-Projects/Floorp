/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsIScriptEventListener_h__
#define nsIScriptEventListener_h__

#include "nsISupports.h"

class nsIDOMEventListener;
class nsIScriptContext;

/*
 * Event listener interface.
 */

#define NS_ISCRIPTEVENTLISTENER_IID \
{ /* e34ed820-1b62-11d2-bd89-00805f8ae3f4 */ \
0xe34ed820, 0x1b62, 0x11d2, \
{0xbd, 0x89, 0x00, 0x80, 0x5f, 0x8a, 0xe3, 0xf4} }

class nsIScriptEventListener : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISCRIPTEVENTLISTENER_IID)

 /**
  * Checks equality of internal script function pointer with the one passed in.
  */
  NS_IMETHOD CheckIfEqual(nsIScriptEventListener *aListener, PRBool* aResult) = 0;

 /**
  * Gets internal data for equality checking..
  */
  NS_IMETHOD GetInternals(void** aTarget, void** aHandler) = 0;

};

extern "C" NS_DOM nsresult
NS_NewScriptEventListener(nsIDOMEventListener ** aInstancePtrResult,
                          nsIScriptContext *aContext,
                          void *aTarget,
                          void *aHandler);

#endif // nsIScriptEventListener_h__
