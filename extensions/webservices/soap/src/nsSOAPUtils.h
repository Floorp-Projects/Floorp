/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsSOAPUtils_h__
#define nsSOAPUtils_h__

#include "nsString.h"
#include "nsIDOMElement.h"
#include "nsISOAPEncoding.h"

class nsSOAPUtils {
private:
  nsSOAPUtils() {}  //  Never create a member of this class
  ~nsSOAPUtils() {}
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
};

struct nsSOAPStrings
{
  const nsLiteralString  kSOAPEnvURI1;
  const nsLiteralString  kSOAPEnvURI2;
  const nsLiteralString *kSOAPEnvURI[2];

  const nsLiteralString  kSOAPEncURI;
  const nsLiteralString  kSOAPEncURI11;
  const nsLiteralString  kXSIURI;
  const nsLiteralString  kXSURI;
  const nsLiteralString  kXSIURI1999;
  const nsLiteralString  kXSURI1999;
  const nsLiteralString  kSOAPEnvPrefix;
  const nsLiteralString  kSOAPEncPrefix;
  const nsLiteralString  kXSIPrefix;
  const nsLiteralString  kXSITypeAttribute;
  const nsLiteralString  kXSPrefix;
  const nsLiteralString  kEncodingStyleAttribute;
  const nsLiteralString  kActorAttribute;
  const nsLiteralString  kMustUnderstandAttribute;
  const nsLiteralString  kEnvelopeTagName;
  const nsLiteralString  kHeaderTagName;
  const nsLiteralString  kBodyTagName;
  const nsLiteralString  kFaultTagName;
  const nsLiteralString  kFaultCodeTagName;
  const nsLiteralString  kFaultStringTagName;
  const nsLiteralString  kFaultActorTagName;
  const nsLiteralString  kFaultDetailTagName;
  const nsLiteralString  kEncodingSeparator;
  const nsLiteralString  kQualifiedSeparator;
  const nsLiteralString  kXMLNamespaceNamespaceURI;
  const nsLiteralString  kXMLNamespaceURI;
  const nsLiteralString  kXMLNamespacePrefix;
  const nsLiteralString  kXMLPrefix;
  const nsLiteralString  kTrue;
  const nsLiteralString  kTrueA;
  const nsLiteralString  kFalse;
  const nsLiteralString  kFalseA;
  const nsLiteralString  kVerifySourceHeader;
  const nsLiteralString  kVerifySourceURI;
  const nsLiteralString  kVerifySourceNamespaceURI;

  // used by nsDefaultSOAPEncoder.cpp
  const nsLiteralString  kEmpty;
  const nsLiteralString  kNull;
  const nsLiteralString  kSOAPArrayTypeAttribute;
  const nsLiteralString  kSOAPArrayOffsetAttribute;
  const nsLiteralString  kSOAPArrayPositionAttribute;
  const nsLiteralString  kAnyTypeSchemaType;
  const nsLiteralString  kAnySimpleTypeSchemaType;
  const nsLiteralString  kArraySOAPType;
  const nsLiteralString  kStructSOAPType;
  const nsLiteralString  kStringSchemaType;
  const nsLiteralString  kBooleanSchemaType;
  const nsLiteralString  kFloatSchemaType;
  const nsLiteralString  kDoubleSchemaType;
  const nsLiteralString  kLongSchemaType;
  const nsLiteralString  kIntSchemaType;
  const nsLiteralString  kShortSchemaType;
  const nsLiteralString  kByteSchemaType;
  const nsLiteralString  kUnsignedLongSchemaType;
  const nsLiteralString  kUnsignedIntSchemaType;
  const nsLiteralString  kUnsignedShortSchemaType;
  const nsLiteralString  kUnsignedByteSchemaType;
  const nsLiteralString  kNormalizedStringSchemaType;
  const nsLiteralString  kTokenSchemaType;
  const nsLiteralString  kNameSchemaType;
  const nsLiteralString  kNCNameSchemaType;
  const nsLiteralString  kDecimalSchemaType;
  const nsLiteralString  kIntegerSchemaType;
  const nsLiteralString  kNonPositiveIntegerSchemaType;
  const nsLiteralString  kNonNegativeIntegerSchemaType;
  const nsLiteralString  kBase64BinarySchemaType;

  nsSOAPStrings();
};

extern nsSOAPStrings *gSOAPStrings;

//  Used to support null strings.

inline PRBool AStringIsNull(const nsAString & aString)
{
  return aString.IsVoid() || aString.IsEmpty();        // Get rid of empty hack when string implementations support.
}

inline void SetAStringToNull(nsAString & aString)
{
  aString.SetIsVoid(PR_TRUE);
}

#define NS_SOAP_ENSURE_ARG_STRING(arg) \
NS_ENSURE_FALSE(AStringIsNull(arg), NS_ERROR_INVALID_ARG)

inline void
SOAPEncodingKey(const nsAString & aURI, const nsAString & aType,
                nsAString & result)
{
  result = aURI + gSOAPStrings->kEncodingSeparator + aType;
}

#endif
