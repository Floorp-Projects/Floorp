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

#ifndef nsIScriptEventListener_h__
#define nsIScriptEventListener_h__

#include "nsISupports.h"

class nsIDOMEventListener;

/*
 * Event listener interface.
 */

#define NS_ISCRIPTEVENTLISTENER_IID \
{ /* e34ed820-1b62-11d2-bd89-00805f8ae3f4 */ \
0xe34ed820, 0x1b62, 0x11d2, \
{0xbd, 0x89, 0x00, 0x80, 0x5f, 0x8a, 0xe3, 0xf4} }

class nsIScriptEventListener : public nsISupports {

public:

 /**
  * Checks equality of internal script function pointer with the one passed in.
  */

  virtual nsresult CheckIfEqual(nsIScriptEventListener *aListener) = 0;

};

extern "C" NS_DOM nsresult NS_NewScriptEventListener(nsIDOMEventListener ** aInstancePtrResult, nsIScriptContext *aContext, void* aObj, void *aFun);
extern "C" NS_DOM nsresult NS_NewJSEventListener(nsIDOMEventListener ** aInstancePtrResult, nsIScriptContext *aContext, void *aObj);

#endif // nsIScriptEventListener_h__
