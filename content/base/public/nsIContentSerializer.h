/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIContentSerializer_h
#define nsIContentSerializer_h

#include "nsISupports.h"

class nsIContent;
class nsIDocument;
class nsAString;

namespace mozilla {
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla

#define NS_ICONTENTSERIALIZER_IID \
{ 0xb1ee32f2, 0xb8c4, 0x49b9, \
  { 0x93, 0xdf, 0xb6, 0xfa, 0xb5, 0xd5, 0x46, 0x88 } }

class nsIContentSerializer : public nsISupports {
 public: 

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICONTENTSERIALIZER_IID)

  NS_IMETHOD Init(PRUint32 flags, PRUint32 aWrapColumn,
                  const char* aCharSet, bool aIsCopying,
                  bool aIsWholeDocument) = 0;

  NS_IMETHOD AppendText(nsIContent* aText, PRInt32 aStartOffset,
                        PRInt32 aEndOffset, nsAString& aStr) = 0;

  NS_IMETHOD AppendCDATASection(nsIContent* aCDATASection,
                                PRInt32 aStartOffset, PRInt32 aEndOffset,
                                nsAString& aStr) = 0;

  NS_IMETHOD AppendProcessingInstruction(nsIContent* aPI,
                                         PRInt32 aStartOffset,
                                         PRInt32 aEndOffset,
                                         nsAString& aStr) = 0;

  NS_IMETHOD AppendComment(nsIContent* aComment, PRInt32 aStartOffset,
                           PRInt32 aEndOffset, nsAString& aStr) = 0;

  NS_IMETHOD AppendDoctype(nsIContent *aDoctype,
                           nsAString& aStr) = 0;

  NS_IMETHOD AppendElementStart(mozilla::dom::Element* aElement,
                                mozilla::dom::Element* aOriginalElement,
                                nsAString& aStr) = 0;

  NS_IMETHOD AppendElementEnd(mozilla::dom::Element* aElement,
                              nsAString& aStr) = 0;

  NS_IMETHOD Flush(nsAString& aStr) = 0;

  /**
   * Append any items in the beginning of the document that won't be 
   * serialized by other methods. XML declaration is the most likely 
   * thing this method can produce.
   */
  NS_IMETHOD AppendDocumentStart(nsIDocument *aDocument,
                                 nsAString& aStr) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIContentSerializer, NS_ICONTENTSERIALIZER_IID)

#define NS_CONTENTSERIALIZER_CONTRACTID_PREFIX \
"@mozilla.org/layout/contentserializer;1?mimetype="

#endif /* nsIContentSerializer_h */
