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

#ifndef nsIDOMMimeType_h__
#define nsIDOMMimeType_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMPlugin;

#define NS_IDOMMIMETYPE_IID \
 { 0xf6134682, 0xf28b, 0x11d2, { 0x83, 0x60, 0xc9, 0x08, 0x99, 0x04, 0x9c, 0x3c } } 

class nsIDOMMimeType : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMMIMETYPE_IID; return iid; }

  NS_IMETHOD    GetDescription(nsString& aDescription)=0;

  NS_IMETHOD    GetEnabledPlugin(nsIDOMPlugin** aEnabledPlugin)=0;

  NS_IMETHOD    GetSuffixes(nsString& aSuffixes)=0;

  NS_IMETHOD    GetType(nsString& aType)=0;
};


#define NS_DECL_IDOMMIMETYPE   \
  NS_IMETHOD    GetDescription(nsString& aDescription);  \
  NS_IMETHOD    GetEnabledPlugin(nsIDOMPlugin** aEnabledPlugin);  \
  NS_IMETHOD    GetSuffixes(nsString& aSuffixes);  \
  NS_IMETHOD    GetType(nsString& aType);  \



#define NS_FORWARD_IDOMMIMETYPE(_to)  \
  NS_IMETHOD    GetDescription(nsString& aDescription) { return _to##GetDescription(aDescription); } \
  NS_IMETHOD    GetEnabledPlugin(nsIDOMPlugin** aEnabledPlugin) { return _to##GetEnabledPlugin(aEnabledPlugin); } \
  NS_IMETHOD    GetSuffixes(nsString& aSuffixes) { return _to##GetSuffixes(aSuffixes); } \
  NS_IMETHOD    GetType(nsString& aType) { return _to##GetType(aType); } \


extern "C" NS_DOM nsresult NS_InitMimeTypeClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptMimeType(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMMimeType_h__
