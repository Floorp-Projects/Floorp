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
#include "nsISOAPEncoding.h"

class nsSOAPUtils {
public:
  static void GetSpecificChildElement(nsISOAPEncoding * aEncoding,
                                      nsIDOMElement * aParent,
                                      const nsAString & aNamespace,
                                      const nsAString & aType,
                                      nsIDOMElement * *aElement);
  static void GetSpecificSiblingElement(nsISOAPEncoding * aEncoding,
                                        nsIDOMElement * aSibling,
                                        const nsAString & aNamespace,
                                        const nsAString & aType,
                                        nsIDOMElement * *aElement);
  static void GetFirstChildElement(nsIDOMElement * aParent,
                                   nsIDOMElement ** aElement);
  static void GetNextSiblingElement(nsIDOMElement * aStart,
                                    nsIDOMElement ** aElement);
  static nsresult GetElementTextContent(nsIDOMElement * aElement,
                                        nsAString & aText);
  static PRBool HasChildElements(nsIDOMElement * aElement);

  static void GetNextSibling(nsIDOMNode * aSibling, nsIDOMNode ** aNext);
  static nsresult MakeNamespacePrefix(nsISOAPEncoding *aEncoding,
                                      nsIDOMElement * aElement,
                                      const nsAString & aURI,
                                      nsAString & aPrefix);
  static nsresult GetNamespaceURI(nsISOAPEncoding *aEncoding,
                                  nsIDOMElement * aElement,
                                  const nsAString & aQName,
                                  nsAString & aURI);
  static nsresult GetLocalName(const nsAString & aQName,
                               nsAString & aLocalName);

  static PRBool GetAttribute(nsISOAPEncoding *aEncoding,
                                  nsIDOMElement * aElement,
                                  const nsAString & aNamespaceURI,
                                  const nsAString & aLocalName,
                                  nsAString & aValue);

// All those missing string functions have to come from somewhere...

  static PRBool StartsWith(nsAString & aSuper, nsAString & aSub);

  static const nsAString *kSOAPEnvURI[];
  static const nsAString *kSOAPEncURI[];

  static const nsAString & kXSIURI;
  static const nsAString & kXSURI;
  static const nsAString & kXSIURI1999;
  static const nsAString & kXSURI1999;
  static const nsAString & kSOAPEnvPrefix;
  static const nsAString & kSOAPEncPrefix;
  static const nsAString & kXSIPrefix;
  static const nsAString & kXSITypeAttribute;
  static const nsAString & kXSPrefix;
  static const nsAString & kEncodingStyleAttribute;
  static const nsAString & kActorAttribute;
  static const nsAString & kMustUnderstandAttribute;
  static const nsAString & kEnvelopeTagName;
  static const nsAString & kHeaderTagName;
  static const nsAString & kBodyTagName;
  static const nsAString & kFaultTagName;
  static const nsAString & kFaultCodeTagName;
  static const nsAString & kFaultStringTagName;
  static const nsAString & kFaultActorTagName;
  static const nsAString & kFaultDetailTagName;
  static const nsAString & kEncodingSeparator;
  static const nsAString & kQualifiedSeparator;
  static const nsAString & kXMLNamespaceNamespaceURI;
  static const nsAString & kXMLNamespaceURI;
  static const nsAString & kXMLNamespacePrefix;
  static const nsAString & kXMLPrefix;
  static const nsAString & kTrue;
  static const nsAString & kTrueA;
  static const nsAString & kFalse;
  static const nsAString & kFalseA;
  static const nsAString & kVerifySourceHeader;
  static const nsAString & kVerifySourceURI;
  static const nsAString & kVerifySourceNamespaceURI;
};

//  Used to support null strings.

inline PRBool AStringIsNull(const nsAString & aString)
{
  return aString.IsVoid() || aString.IsEmpty();        // Get rid of empty hack when string implementations support.
}

inline void SetAStringToNull(nsAString & aString)
{
  aString.Truncate();
  aString.SetIsVoid(PR_TRUE);
}

#define NS_SOAP_ENSURE_ARG_STRING(arg) \
NS_ENSURE_FALSE(AStringIsNull(arg), NS_ERROR_INVALID_ARG)

inline void
SOAPEncodingKey(const nsAString & aURI, const nsAString & aType,
                nsAString & result)
{
  result.Assign(aURI);
  result.Append(nsSOAPUtils::kEncodingSeparator);
  result.Append(aType);
}

#endif
