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

#ifndef nsIDOMTextRange_h__
#define nsIDOMTextRange_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"


#define NS_IDOMTEXTRANGE_IID \
 {0xb471ab41, 0x2a79, 0x11d3, \
	{ 0x9e, 0xa4, 0x0, 0x60, 0x8, 0x9f, 0xe5, 0x9b } } 

class nsIDOMTextRange : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMTEXTRANGE_IID; return iid; }
  enum {
    TEXTRANGE_RAWINPUT = 1,
    TEXTRANGE_SELECTEDRAWTEXT = 2,
    TEXTRANGE_CONVERTEDTEXT = 3,
    TEXTRANGE_SELECTEDCONVERTEDTEXT = 4
  };

  NS_IMETHOD    GetRangeStart(PRUint16* aRangeStart)=0;
  NS_IMETHOD    SetRangeStart(PRUint16 aRangeStart)=0;

  NS_IMETHOD    GetRangeEnd(PRUint16* aRangeEnd)=0;
  NS_IMETHOD    SetRangeEnd(PRUint16 aRangeEnd)=0;

  NS_IMETHOD    GetRangeType(PRUint16* aRangeType)=0;
  NS_IMETHOD    SetRangeType(PRUint16 aRangeType)=0;
};


#define NS_DECL_IDOMTEXTRANGE   \
  NS_IMETHOD    GetRangeStart(PRUint16* aRangeStart);  \
  NS_IMETHOD    SetRangeStart(PRUint16 aRangeStart);  \
  NS_IMETHOD    GetRangeEnd(PRUint16* aRangeEnd);  \
  NS_IMETHOD    SetRangeEnd(PRUint16 aRangeEnd);  \
  NS_IMETHOD    GetRangeType(PRUint16* aRangeType);  \
  NS_IMETHOD    SetRangeType(PRUint16 aRangeType);  \



#define NS_FORWARD_IDOMTEXTRANGE(_to)  \
  NS_IMETHOD    GetRangeStart(PRUint16* aRangeStart) { return _to GetRangeStart(aRangeStart); } \
  NS_IMETHOD    SetRangeStart(PRUint16 aRangeStart) { return _to SetRangeStart(aRangeStart); } \
  NS_IMETHOD    GetRangeEnd(PRUint16* aRangeEnd) { return _to GetRangeEnd(aRangeEnd); } \
  NS_IMETHOD    SetRangeEnd(PRUint16 aRangeEnd) { return _to SetRangeEnd(aRangeEnd); } \
  NS_IMETHOD    GetRangeType(PRUint16* aRangeType) { return _to GetRangeType(aRangeType); } \
  NS_IMETHOD    SetRangeType(PRUint16 aRangeType) { return _to SetRangeType(aRangeType); } \


extern "C" NS_DOM nsresult NS_InitTextRangeClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptTextRange(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMTextRange_h__
