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

#ifndef nsIDOMCharacterData_h__
#define nsIDOMCharacterData_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMNode.h"


#define NS_IDOMCHARACTERDATA_IID \
 { 0xa6cf9072, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMCharacterData : public nsIDOMNode {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMCHARACTERDATA_IID; return iid; }

  NS_IMETHOD    GetData(nsString& aData)=0;
  NS_IMETHOD    SetData(const nsString& aData)=0;

  NS_IMETHOD    GetLength(PRUint32* aLength)=0;

  NS_IMETHOD    SubstringData(PRUint32 aOffset, PRUint32 aCount, nsString& aReturn)=0;

  NS_IMETHOD    AppendData(const nsString& aArg)=0;

  NS_IMETHOD    InsertData(PRUint32 aOffset, const nsString& aArg)=0;

  NS_IMETHOD    DeleteData(PRUint32 aOffset, PRUint32 aCount)=0;

  NS_IMETHOD    ReplaceData(PRUint32 aOffset, PRUint32 aCount, const nsString& aArg)=0;
};


#define NS_DECL_IDOMCHARACTERDATA   \
  NS_IMETHOD    GetData(nsString& aData);  \
  NS_IMETHOD    SetData(const nsString& aData);  \
  NS_IMETHOD    GetLength(PRUint32* aLength);  \
  NS_IMETHOD    SubstringData(PRUint32 aOffset, PRUint32 aCount, nsString& aReturn);  \
  NS_IMETHOD    AppendData(const nsString& aArg);  \
  NS_IMETHOD    InsertData(PRUint32 aOffset, const nsString& aArg);  \
  NS_IMETHOD    DeleteData(PRUint32 aOffset, PRUint32 aCount);  \
  NS_IMETHOD    ReplaceData(PRUint32 aOffset, PRUint32 aCount, const nsString& aArg);  \



#define NS_FORWARD_IDOMCHARACTERDATA(_to)  \
  NS_IMETHOD    GetData(nsString& aData) { return _to GetData(aData); } \
  NS_IMETHOD    SetData(const nsString& aData) { return _to SetData(aData); } \
  NS_IMETHOD    GetLength(PRUint32* aLength) { return _to GetLength(aLength); } \
  NS_IMETHOD    SubstringData(PRUint32 aOffset, PRUint32 aCount, nsString& aReturn) { return _to SubstringData(aOffset, aCount, aReturn); }  \
  NS_IMETHOD    AppendData(const nsString& aArg) { return _to AppendData(aArg); }  \
  NS_IMETHOD    InsertData(PRUint32 aOffset, const nsString& aArg) { return _to InsertData(aOffset, aArg); }  \
  NS_IMETHOD    DeleteData(PRUint32 aOffset, PRUint32 aCount) { return _to DeleteData(aOffset, aCount); }  \
  NS_IMETHOD    ReplaceData(PRUint32 aOffset, PRUint32 aCount, const nsString& aArg) { return _to ReplaceData(aOffset, aCount, aArg); }  \


extern "C" NS_DOM nsresult NS_InitCharacterDataClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptCharacterData(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMCharacterData_h__
