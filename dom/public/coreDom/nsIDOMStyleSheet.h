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

#ifndef nsIDOMStyleSheet_h__
#define nsIDOMStyleSheet_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"


#define NS_IDOMSTYLESHEET_IID \
 { 0xa6cf9080, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMStyleSheet : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMSTYLESHEET_IID; return iid; }

  NS_IMETHOD    GetType(nsString& aType)=0;

  NS_IMETHOD    GetDisabled(PRBool* aDisabled)=0;
  NS_IMETHOD    SetDisabled(PRBool aDisabled)=0;

  NS_IMETHOD    GetReadOnly(PRBool* aReadOnly)=0;
};


#define NS_DECL_IDOMSTYLESHEET   \
  NS_IMETHOD    GetType(nsString& aType);  \
  NS_IMETHOD    GetDisabled(PRBool* aDisabled);  \
  NS_IMETHOD    SetDisabled(PRBool aDisabled);  \
  NS_IMETHOD    GetReadOnly(PRBool* aReadOnly);  \



#define NS_FORWARD_IDOMSTYLESHEET(_to)  \
  NS_IMETHOD    GetType(nsString& aType) { return _to GetType(aType); } \
  NS_IMETHOD    GetDisabled(PRBool* aDisabled) { return _to GetDisabled(aDisabled); } \
  NS_IMETHOD    SetDisabled(PRBool aDisabled) { return _to SetDisabled(aDisabled); } \
  NS_IMETHOD    GetReadOnly(PRBool* aReadOnly) { return _to GetReadOnly(aReadOnly); } \


extern "C" NS_DOM nsresult NS_InitStyleSheetClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptStyleSheet(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMStyleSheet_h__
