/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIContentSerializer_h
#define nsIContentSerializer_h

#include "nsISupports.h"
#include "nsStringFwd.h"

class nsIContent;

namespace mozilla {
class Encoding;
namespace dom {
class Comment;
class Document;
class DocumentType;
class Element;
class ProcessingInstruction;
}  // namespace dom
}  // namespace mozilla

#define NS_ICONTENTSERIALIZER_IID                    \
  {                                                  \
    0xb1ee32f2, 0xb8c4, 0x49b9, {                    \
      0x93, 0xdf, 0xb6, 0xfa, 0xb5, 0xd5, 0x46, 0x88 \
    }                                                \
  }

class nsIContentSerializer : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICONTENTSERIALIZER_IID)

  NS_IMETHOD Init(uint32_t flags, uint32_t aWrapColumn,
                  const mozilla::Encoding* aEncoding, bool aIsCopying,
                  bool aIsWholeDocument, bool* aNeedsPerformatScanning) = 0;

  NS_IMETHOD AppendText(nsIContent* aText, int32_t aStartOffset,
                        int32_t aEndOffset, nsAString& aStr) = 0;

  NS_IMETHOD AppendCDATASection(nsIContent* aCDATASection, int32_t aStartOffset,
                                int32_t aEndOffset, nsAString& aStr) = 0;

  NS_IMETHOD AppendProcessingInstruction(
      mozilla::dom::ProcessingInstruction* aPI, int32_t aStartOffset,
      int32_t aEndOffset, nsAString& aStr) = 0;

  NS_IMETHOD AppendComment(mozilla::dom::Comment* aComment,
                           int32_t aStartOffset, int32_t aEndOffset,
                           nsAString& aStr) = 0;

  NS_IMETHOD AppendDoctype(mozilla::dom::DocumentType* aDoctype,
                           nsAString& aStr) = 0;

  NS_IMETHOD AppendElementStart(mozilla::dom::Element* aElement,
                                mozilla::dom::Element* aOriginalElement,
                                nsAString& aStr) = 0;

  NS_IMETHOD AppendElementEnd(mozilla::dom::Element* aElement,
                              mozilla::dom::Element* aOriginalElement,
                              nsAString& aStr) = 0;

  NS_IMETHOD Flush(nsAString& aStr) = 0;

  /**
   * Append any items in the beginning of the document that won't be
   * serialized by other methods. XML declaration is the most likely
   * thing this method can produce.
   */
  NS_IMETHOD AppendDocumentStart(mozilla::dom::Document* aDocument,
                                 nsAString& aStr) = 0;

  // If Init() sets *aNeedsPerformatScanning to true, then these methods are
  // called when elements are started and ended, before AppendElementStart
  // and AppendElementEnd, respectively.  They are supposed to be used to
  // allow the implementer to keep track of whether the element is
  // preformatted.
  NS_IMETHOD ScanElementForPreformat(mozilla::dom::Element* aElement) = 0;
  NS_IMETHOD ForgetElementForPreformat(mozilla::dom::Element* aElement) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIContentSerializer, NS_ICONTENTSERIALIZER_IID)

#define NS_CONTENTSERIALIZER_CONTRACTID_PREFIX \
  "@mozilla.org/layout/contentserializer;1?mimetype="

#endif /* nsIContentSerializer_h */
