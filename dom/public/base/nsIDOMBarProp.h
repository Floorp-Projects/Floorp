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

#ifndef nsIDOMBarProp_h__
#define nsIDOMBarProp_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"


#define NS_IDOMBARPROP_IID \
 { 0x9eb2c150, 0x1d56, 0x11d3, \
  { 0x82, 0x21, 0x00, 0x60, 0x08, 0x3a, 0x0b, 0xcf } } 

class nsIDOMBarProp : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMBARPROP_IID; return iid; }

  NS_IMETHOD    GetVisible(PRBool* aVisible)=0;
  NS_IMETHOD    SetVisible(PRBool aVisible)=0;
};


#define NS_DECL_IDOMBARPROP   \
  NS_IMETHOD    GetVisible(PRBool* aVisible);  \
  NS_IMETHOD    SetVisible(PRBool aVisible);  \



#define NS_FORWARD_IDOMBARPROP(_to)  \
  NS_IMETHOD    GetVisible(PRBool* aVisible) { return _to GetVisible(aVisible); } \
  NS_IMETHOD    SetVisible(PRBool aVisible) { return _to SetVisible(aVisible); } \


extern "C" NS_DOM nsresult NS_InitBarPropClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptBarProp(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMBarProp_h__
