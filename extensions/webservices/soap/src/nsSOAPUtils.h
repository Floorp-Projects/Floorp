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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsSOAPUtils_h__
#define nsSOAPUtils_h__

#include "nsString.h"
#include "nsIDOMElement.h"

class nsSOAPUtils {
public:
  static void GetSpecificChildElement(nsIDOMElement *aParent, 
                                      const nsAReadableString& aNamespace, 
                                      const nsAReadableString& aType, 
                                      nsIDOMElement * *aElement);
  static void GetSpecificSiblingElement(nsIDOMElement *aSibling, 
                                        const nsAReadableString& aNamespace, 
                                        const nsAReadableString& aType, 
                                        nsIDOMElement * *aElement);
  static void GetFirstChildElement(nsIDOMElement* aParent, 
                                   nsIDOMElement** aElement);
  static void GetNextSiblingElement(nsIDOMElement* aStart, 
                                    nsIDOMElement** aElement);
  static nsresult GetElementTextContent(nsIDOMElement* aElement, 
                                    nsAWritableString& aText);
  static PRBool HasChildElements(nsIDOMElement* aElement);

  static void GetNextSibling(nsIDOMNode* aSibling, 
                             nsIDOMNode **aNext);
  static nsresult MakeNamespacePrefix(nsIDOMElement* aElement,
                                     const nsAReadableString & aURI,
                                     nsAWritableString & aPrefix);
  static nsresult MakeNamespacePrefixFixed(nsIDOMElement* aElement,
                                     const nsAReadableString & aURI,
                                     nsAWritableString & aPrefix);
  static nsresult GetNamespaceURI(nsIDOMElement* aElement,
                                  const nsAReadableString & aQName, 
                                  nsAWritableString & aURI);
  static nsresult GetLocalName(const nsAReadableString & aQName, 
                                  nsAWritableString & aLocalName);

// All those missing string functions have to come from somewhere...

  static PRBool StartsWith(nsAReadableString& aSuper,
		           nsAReadableString& aSub);
  static nsDependentString kSOAPEnvURI;
  static nsDependentString kSOAPEncodingURI;
  static nsDependentString kSOAPEnvPrefix;
  static nsDependentString kSOAPEncodingPrefix;
  static nsDependentString kXSURI;
  static nsDependentString kXSIURI;
  static nsDependentString kXSDURI;
  static nsDependentString kXSPrefix;
  static nsDependentString kXSIPrefix;
  static nsDependentString kXSITypeAttribute;
  static nsDependentString kXSDPrefix;
  static nsDependentString kEncodingStyleAttribute;
  static nsDependentString kActorAttribute;
  static nsDependentString kMustUnderstandAttribute;
  static nsDependentString kEnvelopeTagName;
  static nsDependentString kHeaderTagName;
  static nsDependentString kBodyTagName;
  static nsDependentString kFaultTagName;
  static nsDependentString kFaultCodeTagName;
  static nsDependentString kFaultStringTagName;
  static nsDependentString kFaultActorTagName;
  static nsDependentString kFaultDetailTagName;
  static nsDependentString kEncodingSeparator;
  static nsDependentString kQualifiedSeparator;
  static nsDependentString kXMLNamespaceNamespaceURI;
  static nsDependentString kXMLNamespaceURI;
  static nsDependentString kXMLNamespacePrefix;
  static nsDependentString kXMLPrefix;
  static nsDependentString kTrue;
  static nsDependentString kTrueA;
  static nsDependentString kFalse;
  static nsDependentString kFalseA;
};

//  Used to support null strings.

inline PRBool AStringIsNull(const nsAReadableString& aString)
{
  return aString.IsVoid() || aString.IsEmpty(); // Get rid of empty hack when string implementations support.
}

inline void SetAStringToNull(nsAWritableString& aString)
{
  aString.Truncate();
  aString.SetIsVoid(PR_TRUE);
}

#define NS_SOAP_ENSURE_ARG_STRING(arg) \
NS_ENSURE_FALSE(AStringIsNull(arg), NS_ERROR_INVALID_ARG)


#endif
