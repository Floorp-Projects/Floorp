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

#ifndef nsIDOMDocumentType_h__
#define nsIDOMDocumentType_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMNode.h"

class nsIDOMNamedNodeMap;

#define NS_IDOMDOCUMENTTYPE_IID \
 { 0xa6cf9077, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMDocumentType : public nsIDOMNode {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMDOCUMENTTYPE_IID; return iid; }

  NS_IMETHOD    GetName(nsAWritableString& aName)=0;

  NS_IMETHOD    GetEntities(nsIDOMNamedNodeMap** aEntities)=0;

  NS_IMETHOD    GetNotations(nsIDOMNamedNodeMap** aNotations)=0;

  NS_IMETHOD    GetPublicId(nsAWritableString& aPublicId)=0;

  NS_IMETHOD    GetSystemId(nsAWritableString& aSystemId)=0;

  NS_IMETHOD    GetInternalSubset(nsAWritableString& aInternalSubset)=0;
};


#define NS_DECL_IDOMDOCUMENTTYPE   \
  NS_IMETHOD    GetName(nsAWritableString& aName);  \
  NS_IMETHOD    GetEntities(nsIDOMNamedNodeMap** aEntities);  \
  NS_IMETHOD    GetNotations(nsIDOMNamedNodeMap** aNotations);  \
  NS_IMETHOD    GetPublicId(nsAWritableString& aPublicId);  \
  NS_IMETHOD    GetSystemId(nsAWritableString& aSystemId);  \
  NS_IMETHOD    GetInternalSubset(nsAWritableString& aInternalSubset);  \



#define NS_FORWARD_IDOMDOCUMENTTYPE(_to)  \
  NS_IMETHOD    GetName(nsAWritableString& aName) { return _to GetName(aName); } \
  NS_IMETHOD    GetEntities(nsIDOMNamedNodeMap** aEntities) { return _to GetEntities(aEntities); } \
  NS_IMETHOD    GetNotations(nsIDOMNamedNodeMap** aNotations) { return _to GetNotations(aNotations); } \
  NS_IMETHOD    GetPublicId(nsAWritableString& aPublicId) { return _to GetPublicId(aPublicId); } \
  NS_IMETHOD    GetSystemId(nsAWritableString& aSystemId) { return _to GetSystemId(aSystemId); } \
  NS_IMETHOD    GetInternalSubset(nsAWritableString& aInternalSubset) { return _to GetInternalSubset(aInternalSubset); } \


extern "C" NS_DOM nsresult NS_InitDocumentTypeClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptDocumentType(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMDocumentType_h__
