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

#ifndef nsIDOMDOMException_h__
#define nsIDOMDOMException_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"


#define NS_IDOMDOMEXCEPTION_IID \
 { 0xa6cf910a, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMDOMException : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMDOMEXCEPTION_IID; return iid; }
  enum {
    INDEX_SIZE_ERR = 1,
    DOMSTRING_SIZE_ERR = 2,
    HIERARCHY_REQUEST_ERR = 3,
    WRONG_DOCUMENT_ERR = 4,
    INVALID_CHARACTER_ERR = 5,
    NO_DATA_ALLOWED_ERR = 6,
    NO_MODIFICATION_ALLOWED_ERR = 7,
    NOT_FOUND_ERR = 8,
    NOT_SUPPORTED_ERR = 9,
    INUSE_ATTRIBUTE_ERR = 10
  };

  NS_IMETHOD    GetCode(PRUint32* aCode)=0;

  NS_IMETHOD    GetResult(PRUint32* aResult)=0;

  NS_IMETHOD    GetMessage(nsString& aMessage)=0;

  NS_IMETHOD    GetName(nsString& aName)=0;

  NS_IMETHOD    ToString(nsString& aReturn)=0;
};


#define NS_DECL_IDOMDOMEXCEPTION   \
  NS_IMETHOD    GetCode(PRUint32* aCode);  \
  NS_IMETHOD    GetResult(PRUint32* aResult);  \
  NS_IMETHOD    GetMessage(nsString& aMessage);  \
  NS_IMETHOD    GetName(nsString& aName);  \
  NS_IMETHOD    ToString(nsString& aReturn);  \



#define NS_FORWARD_IDOMDOMEXCEPTION(_to)  \
  NS_IMETHOD    GetCode(PRUint32* aCode) { return _to GetCode(aCode); } \
  NS_IMETHOD    GetResult(PRUint32* aResult) { return _to GetResult(aResult); } \
  NS_IMETHOD    GetMessage(nsString& aMessage) { return _to GetMessage(aMessage); } \
  NS_IMETHOD    GetName(nsString& aName) { return _to GetName(aName); } \
  NS_IMETHOD    ToString(nsString& aReturn) { return _to ToString(aReturn); }  \


extern "C" NS_DOM nsresult NS_InitDOMExceptionClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptDOMException(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMDOMException_h__
