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
/* AUTO-GENERATED. DO NOT EDIT!!! */

#ifndef nsIDOMSelectionListener_h__
#define nsIDOMSelectionListener_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"


#define NS_IDOMSELECTIONLISTENER_IID \
 { 0xa6cf90e2, 0x15b3, 0x11d2,             \
        { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }		

class nsIDOMSelectionListener : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMSELECTIONLISTENER_IID; return iid; }

  NS_IMETHOD    NotifySelectionChanged()=0;
};


#define NS_DECL_IDOMSELECTIONLISTENER   \
  NS_IMETHOD    NotifySelectionChanged();  \



#define NS_FORWARD_IDOMSELECTIONLISTENER(_to)  \
  NS_IMETHOD    NotifySelectionChanged() { return _to NotifySelectionChanged(); }  \


extern "C" NS_DOM nsresult NS_InitSelectionListenerClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptSelectionListener(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMSelectionListener_h__
