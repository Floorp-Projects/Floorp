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

#ifndef nsIScriptGlobalObject_h__
#define nsIScriptGlobalObject_h__

#include "nsISupports.h"
#include "nsIScriptContext.h"

class nsIScriptContext;
class nsIDOMDocument;

#define NS_ISCRIPTGLOBALOBJECT_IID \
{ 0x2b16fc80, 0xfa41, 0x11d1,  \
{ 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3} }

/**
 * The JavaScript specific global object. This often used to store
 * per-window global state.
 */

class nsIScriptGlobalObject : public nsISupports {
public:
  NS_IMETHOD_(void)       SetContext(nsIScriptContext *aContext)=0;
  NS_IMETHOD_(void)       SetNewDocument(nsIDOMDocument *aDocument)=0;
};

extern "C" NS_DOM NS_NewScriptGlobalObject(nsIScriptGlobalObject **aGlobal);

#endif
