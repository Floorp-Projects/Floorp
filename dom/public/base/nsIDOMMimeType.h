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

  NS_IMETHOD    GetDescription(nsAWritableString& aDescription)=0;

  NS_IMETHOD    GetEnabledPlugin(nsIDOMPlugin** aEnabledPlugin)=0;

  NS_IMETHOD    GetSuffixes(nsAWritableString& aSuffixes)=0;

  NS_IMETHOD    GetType(nsAWritableString& aType)=0;
};


#define NS_DECL_IDOMMIMETYPE   \
  NS_IMETHOD    GetDescription(nsAWritableString& aDescription);  \
  NS_IMETHOD    GetEnabledPlugin(nsIDOMPlugin** aEnabledPlugin);  \
  NS_IMETHOD    GetSuffixes(nsAWritableString& aSuffixes);  \
  NS_IMETHOD    GetType(nsAWritableString& aType);  \



#define NS_FORWARD_IDOMMIMETYPE(_to)  \
  NS_IMETHOD    GetDescription(nsAWritableString& aDescription) { return _to GetDescription(aDescription); } \
  NS_IMETHOD    GetEnabledPlugin(nsIDOMPlugin** aEnabledPlugin) { return _to GetEnabledPlugin(aEnabledPlugin); } \
  NS_IMETHOD    GetSuffixes(nsAWritableString& aSuffixes) { return _to GetSuffixes(aSuffixes); } \
  NS_IMETHOD    GetType(nsAWritableString& aType) { return _to GetType(aType); } \


extern "C" NS_DOM nsresult NS_InitMimeTypeClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptMimeType(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMMimeType_h__
