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

#ifndef nsIDOMHTMLFrameSetElement_h__
#define nsIDOMHTMLFrameSetElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"


#define NS_IDOMHTMLFRAMESETELEMENT_IID \
 { 0xa6cf90b8, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLFrameSetElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLFRAMESETELEMENT_IID; return iid; }

  NS_IMETHOD    GetCols(nsAWritableString& aCols)=0;
  NS_IMETHOD    SetCols(const nsAReadableString& aCols)=0;

  NS_IMETHOD    GetRows(nsAWritableString& aRows)=0;
  NS_IMETHOD    SetRows(const nsAReadableString& aRows)=0;
};


#define NS_DECL_IDOMHTMLFRAMESETELEMENT   \
  NS_IMETHOD    GetCols(nsAWritableString& aCols);  \
  NS_IMETHOD    SetCols(const nsAReadableString& aCols);  \
  NS_IMETHOD    GetRows(nsAWritableString& aRows);  \
  NS_IMETHOD    SetRows(const nsAReadableString& aRows);  \



#define NS_FORWARD_IDOMHTMLFRAMESETELEMENT(_to)  \
  NS_IMETHOD    GetCols(nsAWritableString& aCols) { return _to GetCols(aCols); } \
  NS_IMETHOD    SetCols(const nsAReadableString& aCols) { return _to SetCols(aCols); } \
  NS_IMETHOD    GetRows(nsAWritableString& aRows) { return _to GetRows(aRows); } \
  NS_IMETHOD    SetRows(const nsAReadableString& aRows) { return _to SetRows(aRows); } \


extern "C" NS_DOM nsresult NS_InitHTMLFrameSetElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLFrameSetElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLFrameSetElement_h__
