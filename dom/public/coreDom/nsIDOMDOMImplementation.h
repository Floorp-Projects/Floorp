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

#ifndef nsIDOMDOMImplementation_h__
#define nsIDOMDOMImplementation_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMDocument;
class nsIDOMDocumentType;

#define NS_IDOMDOMIMPLEMENTATION_IID \
 { 0xa6cf9074, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMDOMImplementation : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMDOMIMPLEMENTATION_IID; return iid; }

  NS_IMETHOD    HasFeature(const nsAReadableString& aFeature, const nsAReadableString& aVersion, PRBool* aReturn)=0;

  NS_IMETHOD    CreateDocumentType(const nsAReadableString& aQualifiedName, const nsAReadableString& aPublicId, const nsAReadableString& aSystemId, nsIDOMDocumentType** aReturn)=0;

  NS_IMETHOD    CreateDocument(const nsAReadableString& aNamespaceURI, const nsAReadableString& aQualifiedName, nsIDOMDocumentType* aDoctype, nsIDOMDocument** aReturn)=0;
};


#define NS_DECL_IDOMDOMIMPLEMENTATION   \
  NS_IMETHOD    HasFeature(const nsAReadableString& aFeature, const nsAReadableString& aVersion, PRBool* aReturn);  \
  NS_IMETHOD    CreateDocumentType(const nsAReadableString& aQualifiedName, const nsAReadableString& aPublicId, const nsAReadableString& aSystemId, nsIDOMDocumentType** aReturn);  \
  NS_IMETHOD    CreateDocument(const nsAReadableString& aNamespaceURI, const nsAReadableString& aQualifiedName, nsIDOMDocumentType* aDoctype, nsIDOMDocument** aReturn);  \



#define NS_FORWARD_IDOMDOMIMPLEMENTATION(_to)  \
  NS_IMETHOD    HasFeature(const nsAReadableString& aFeature, const nsAReadableString& aVersion, PRBool* aReturn) { return _to HasFeature(aFeature, aVersion, aReturn); }  \
  NS_IMETHOD    CreateDocumentType(const nsAReadableString& aQualifiedName, const nsAReadableString& aPublicId, const nsAReadableString& aSystemId, nsIDOMDocumentType** aReturn) { return _to CreateDocumentType(aQualifiedName, aPublicId, aSystemId, aReturn); }  \
  NS_IMETHOD    CreateDocument(const nsAReadableString& aNamespaceURI, const nsAReadableString& aQualifiedName, nsIDOMDocumentType* aDoctype, nsIDOMDocument** aReturn) { return _to CreateDocument(aNamespaceURI, aQualifiedName, aDoctype, aReturn); }  \


extern "C" NS_DOM nsresult NS_InitDOMImplementationClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptDOMImplementation(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMDOMImplementation_h__
