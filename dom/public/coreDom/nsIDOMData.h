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

#ifndef nsIDOMData_h__
#define nsIDOMData_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMNode.h"


#define NS_IDOMDATA_IID \
{ 0x6f7652e3,  0xee43, 0x11d1, \
 { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMData : public nsIDOMNode {
public:

  NS_IMETHOD    GetData(nsString& aData)=0;
  NS_IMETHOD    SetData(const nsString& aData)=0;

  NS_IMETHOD    GetSize(PRUint32* aSize)=0;

  NS_IMETHOD    Substring(PRUint32 aStart, PRUint32 aEnd, nsString& aReturn)=0;

  NS_IMETHOD    Append(const nsString& aData)=0;

  NS_IMETHOD    Insert(PRUint32 aOffset, const nsString& aData)=0;

  NS_IMETHOD    Remove(PRUint32 aOffset, PRUint32 aCount)=0;

  NS_IMETHOD    Replace(PRUint32 aOffset, PRUint32 aCount, const nsString& aData)=0;
};


#define NS_DECL_IDOMDATA   \
  NS_IMETHOD    GetData(nsString& aData);  \
  NS_IMETHOD    SetData(const nsString& aData);  \
  NS_IMETHOD    GetSize(PRUint32* aSize);  \
  NS_IMETHOD    Substring(PRUint32 aStart, PRUint32 aEnd, nsString& aReturn);  \
  NS_IMETHOD    Append(const nsString& aData);  \
  NS_IMETHOD    Insert(PRUint32 aOffset, const nsString& aData);  \
  NS_IMETHOD    Remove(PRUint32 aOffset, PRUint32 aCount);  \
  NS_IMETHOD    Replace(PRUint32 aOffset, PRUint32 aCount, const nsString& aData);  \



#define NS_FORWARD_IDOMDATA(superClass)  \
  NS_IMETHOD    GetData(nsString& aData) { return superClass::GetData(aData); } \
  NS_IMETHOD    SetData(const nsString& aData) { return superClass::SetData(aData); } \
  NS_IMETHOD    GetSize(PRUint32* aSize) { return superClass::GetSize(aSize); } \
  NS_IMETHOD    Substring(PRUint32 aStart, PRUint32 aEnd, nsString& aReturn) { return superClass::Substring(aStart, aEnd, aReturn); }  \
  NS_IMETHOD    Append(const nsString& aData) { return superClass::Append(aData); }  \
  NS_IMETHOD    Insert(PRUint32 aOffset, const nsString& aData) { return superClass::Insert(aOffset, aData); }  \
  NS_IMETHOD    Remove(PRUint32 aOffset, PRUint32 aCount) { return superClass::Remove(aOffset, aCount); }  \
  NS_IMETHOD    Replace(PRUint32 aOffset, PRUint32 aCount, const nsString& aData) { return superClass::Replace(aOffset, aCount, aData); }  \


extern nsresult NS_InitDataClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptData(nsIScriptContext *aContext, nsIDOMData *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMData_h__
