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

#ifndef nsIDOMText_h__
#define nsIDOMText_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMCharacterData.h"

class nsIDOMText;

#define NS_IDOMTEXT_IID \
 { 0xa6cf9082, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMText : public nsIDOMCharacterData {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMTEXT_IID; return iid; }

  NS_IMETHOD    SplitText(PRUint32 aOffset, nsIDOMText** aReturn)=0;
};


#define NS_DECL_IDOMTEXT   \
  NS_IMETHOD    SplitText(PRUint32 aOffset, nsIDOMText** aReturn);  \



#define NS_FORWARD_IDOMTEXT(_to)  \
  NS_IMETHOD    SplitText(PRUint32 aOffset, nsIDOMText** aReturn) { return _to SplitText(aOffset, aReturn); }  \


extern "C" NS_DOM nsresult NS_InitTextClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptText(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMText_h__
