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

#ifndef nsIDOMImage_h__
#define nsIDOMImage_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"


#define NS_IDOMIMAGE_IID \
 { 0xa6cf90c7, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMImage : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMIMAGE_IID; return iid; }

  NS_IMETHOD    GetLowsrc(nsAWritableString& aLowsrc)=0;
  NS_IMETHOD    SetLowsrc(const nsAReadableString& aLowsrc)=0;

  NS_IMETHOD    GetComplete(PRBool* aComplete)=0;

  NS_IMETHOD    GetBorder(PRInt32* aBorder)=0;
  NS_IMETHOD    SetBorder(PRInt32 aBorder)=0;

  NS_IMETHOD    GetHeight(PRInt32* aHeight)=0;
  NS_IMETHOD    SetHeight(PRInt32 aHeight)=0;

  NS_IMETHOD    GetHspace(PRInt32* aHspace)=0;
  NS_IMETHOD    SetHspace(PRInt32 aHspace)=0;

  NS_IMETHOD    GetVspace(PRInt32* aVspace)=0;
  NS_IMETHOD    SetVspace(PRInt32 aVspace)=0;

  NS_IMETHOD    GetWidth(PRInt32* aWidth)=0;
  NS_IMETHOD    SetWidth(PRInt32 aWidth)=0;

  NS_IMETHOD    GetNaturalHeight(PRInt32* aNaturalHeight)=0;

  NS_IMETHOD    GetNaturalWidth(PRInt32* aNaturalWidth)=0;
};


#define NS_DECL_IDOMIMAGE   \
  NS_IMETHOD    GetLowsrc(nsAWritableString& aLowsrc);  \
  NS_IMETHOD    SetLowsrc(const nsAReadableString& aLowsrc);  \
  NS_IMETHOD    GetComplete(PRBool* aComplete);  \
  NS_IMETHOD    GetBorder(PRInt32* aBorder);  \
  NS_IMETHOD    SetBorder(PRInt32 aBorder);  \
  NS_IMETHOD    GetHeight(PRInt32* aHeight);  \
  NS_IMETHOD    SetHeight(PRInt32 aHeight);  \
  NS_IMETHOD    GetHspace(PRInt32* aHspace);  \
  NS_IMETHOD    SetHspace(PRInt32 aHspace);  \
  NS_IMETHOD    GetVspace(PRInt32* aVspace);  \
  NS_IMETHOD    SetVspace(PRInt32 aVspace);  \
  NS_IMETHOD    GetWidth(PRInt32* aWidth);  \
  NS_IMETHOD    SetWidth(PRInt32 aWidth);  \
  NS_IMETHOD    GetNaturalHeight(PRInt32* aNaturalHeight);  \
  NS_IMETHOD    GetNaturalWidth(PRInt32* aNaturalWidth);  \



#define NS_FORWARD_IDOMIMAGE(_to)  \
  NS_IMETHOD    GetLowsrc(nsAWritableString& aLowsrc) { return _to GetLowsrc(aLowsrc); } \
  NS_IMETHOD    SetLowsrc(const nsAReadableString& aLowsrc) { return _to SetLowsrc(aLowsrc); } \
  NS_IMETHOD    GetComplete(PRBool* aComplete) { return _to GetComplete(aComplete); } \
  NS_IMETHOD    GetBorder(PRInt32* aBorder) { return _to GetBorder(aBorder); } \
  NS_IMETHOD    SetBorder(PRInt32 aBorder) { return _to SetBorder(aBorder); } \
  NS_IMETHOD    GetHeight(PRInt32* aHeight) { return _to GetHeight(aHeight); } \
  NS_IMETHOD    SetHeight(PRInt32 aHeight) { return _to SetHeight(aHeight); } \
  NS_IMETHOD    GetHspace(PRInt32* aHspace) { return _to GetHspace(aHspace); } \
  NS_IMETHOD    SetHspace(PRInt32 aHspace) { return _to SetHspace(aHspace); } \
  NS_IMETHOD    GetVspace(PRInt32* aVspace) { return _to GetVspace(aVspace); } \
  NS_IMETHOD    SetVspace(PRInt32 aVspace) { return _to SetVspace(aVspace); } \
  NS_IMETHOD    GetWidth(PRInt32* aWidth) { return _to GetWidth(aWidth); } \
  NS_IMETHOD    SetWidth(PRInt32 aWidth) { return _to SetWidth(aWidth); } \
  NS_IMETHOD    GetNaturalHeight(PRInt32* aNaturalHeight) { return _to GetNaturalHeight(aNaturalHeight); } \
  NS_IMETHOD    GetNaturalWidth(PRInt32* aNaturalWidth) { return _to GetNaturalWidth(aNaturalWidth); } \


#endif // nsIDOMImage_h__
