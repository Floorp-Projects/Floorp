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

#ifndef nsIDOMNSHTMLInputElement_h__
#define nsIDOMNSHTMLInputElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIControllers;

#define NS_IDOMNSHTMLINPUTELEMENT_IID \
 { 0x993d2efc, 0xa768, 0x11d3, \
  { 0xbc, 0xcd, 0x00, 0x60, 0xb0, 0xfc, 0x76, 0xbd } } 

class NS_NO_VTABLE nsIDOMNSHTMLInputElement : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDOMNSHTMLINPUTELEMENT_IID)

  NS_IMETHOD    GetControllers(nsIControllers** aControllers)=0;

  NS_IMETHOD    GetTextLength(PRInt32* aTextLength)=0;

  NS_IMETHOD    GetSelectionStart(PRInt32* aSelectionStart)=0;
  NS_IMETHOD    SetSelectionStart(PRInt32 aSelectionStart)=0;

  NS_IMETHOD    GetSelectionEnd(PRInt32* aSelectionEnd)=0;
  NS_IMETHOD    SetSelectionEnd(PRInt32 aSelectionEnd)=0;

  NS_IMETHOD    SetSelectionRange(PRInt32 aSelectionStart, PRInt32 aSelectionEnd)=0;
};


#define NS_DECL_IDOMNSHTMLINPUTELEMENT   \
  NS_IMETHOD    GetControllers(nsIControllers** aControllers);  \
  NS_IMETHOD    GetTextLength(PRInt32* aTextLength);  \
  NS_IMETHOD    GetSelectionStart(PRInt32* aSelectionStart);  \
  NS_IMETHOD    SetSelectionStart(PRInt32 aSelectionStart);  \
  NS_IMETHOD    GetSelectionEnd(PRInt32* aSelectionEnd);  \
  NS_IMETHOD    SetSelectionEnd(PRInt32 aSelectionEnd);  \
  NS_IMETHOD    SetSelectionRange(PRInt32 aSelectionStart, PRInt32 aSelectionEnd);  \



#define NS_FORWARD_IDOMNSHTMLINPUTELEMENT(_to)  \
  NS_IMETHOD    GetControllers(nsIControllers** aControllers) { return _to GetControllers(aControllers); } \
  NS_IMETHOD    GetTextLength(PRInt32* aTextLength) { return _to GetTextLength(aTextLength); } \
  NS_IMETHOD    GetSelectionStart(PRInt32* aSelectionStart) { return _to GetSelectionStart(aSelectionStart); } \
  NS_IMETHOD    SetSelectionStart(PRInt32 aSelectionStart) { return _to SetSelectionStart(aSelectionStart); } \
  NS_IMETHOD    GetSelectionEnd(PRInt32* aSelectionEnd) { return _to GetSelectionEnd(aSelectionEnd); } \
  NS_IMETHOD    SetSelectionEnd(PRInt32 aSelectionEnd) { return _to SetSelectionEnd(aSelectionEnd); } \
  NS_IMETHOD    SetSelectionRange(PRInt32 aSelectionStart, PRInt32 aSelectionEnd) { return _to SetSelectionRange(aSelectionStart, aSelectionEnd); }  \


#endif // nsIDOMNSHTMLInputElement_h__
