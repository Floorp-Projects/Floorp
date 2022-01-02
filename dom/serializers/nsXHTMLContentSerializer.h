/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsIContentSerializer implementation that can be used with an
 * nsIDocumentEncoder to convert an XHTML (not HTML!) DOM to an XHTML
 * string that could be parsed into more or less the original DOM.
 */

#ifndef nsXHTMLContentSerializer_h__
#define nsXHTMLContentSerializer_h__

#include "mozilla/Attributes.h"
#include "nsXMLContentSerializer.h"
#include "nsString.h"
#include "nsTArray.h"

class nsIContent;
class nsAtom;

namespace mozilla {
class Encoding;
}

class nsXHTMLContentSerializer : public nsXMLContentSerializer {
 public:
  nsXHTMLContentSerializer();
  virtual ~nsXHTMLContentSerializer();

  NS_IMETHOD Init(uint32_t flags, uint32_t aWrapColumn,
                  const mozilla::Encoding* aEncoding, bool aIsCopying,
                  bool aRewriteEncodingDeclaration,
                  bool* aNeedsPreformatScanning, nsAString& aOutput) override;

  NS_IMETHOD AppendText(nsIContent* aText, int32_t aStartOffset,
                        int32_t aEndOffset) override;

  NS_IMETHOD AppendDocumentStart(mozilla::dom::Document* aDocument) override;

 protected:
  virtual bool CheckElementStart(mozilla::dom::Element* aElement,
                                 bool& aForceFormat, nsAString& aStr,
                                 nsresult& aResult) override;

  [[nodiscard]] virtual bool AfterElementStart(nsIContent* aContent,
                                               nsIContent* aOriginalElement,
                                               nsAString& aStr) override;

  virtual bool CheckElementEnd(mozilla::dom::Element* aContent,
                               mozilla::dom::Element* aOriginalElement,
                               bool& aForceFormat, nsAString& aStr) override;

  virtual void AfterElementEnd(nsIContent* aContent, nsAString& aStr) override;

  virtual bool LineBreakBeforeOpen(int32_t aNamespaceID,
                                   nsAtom* aName) override;
  virtual bool LineBreakAfterOpen(int32_t aNamespaceID, nsAtom* aName) override;
  virtual bool LineBreakBeforeClose(int32_t aNamespaceID,
                                    nsAtom* aName) override;
  virtual bool LineBreakAfterClose(int32_t aNamespaceID,
                                   nsAtom* aName) override;

  bool HasLongLines(const nsString& text, int32_t& aLastNewlineOffset);

  // functions to check if we enter in or leave from a preformated content
  virtual void MaybeEnterInPreContent(nsIContent* aNode) override;
  virtual void MaybeLeaveFromPreContent(nsIContent* aNode) override;

  [[nodiscard]] virtual bool SerializeAttributes(
      mozilla::dom::Element* aContent, mozilla::dom::Element* aOriginalElement,
      nsAString& aTagPrefix, const nsAString& aTagNamespaceURI,
      nsAtom* aTagName, nsAString& aStr, uint32_t aSkipAttr,
      bool aAddNSAttr) override;

  bool IsFirstChildOfOL(nsIContent* aElement);

  [[nodiscard]] bool SerializeLIValueAttribute(nsIContent* aElement,
                                               nsAString& aStr);
  bool IsShorthandAttr(const nsAtom* aAttrName, const nsAtom* aElementName);

  [[nodiscard]] virtual bool AppendAndTranslateEntities(
      const nsAString& aStr, nsAString& aOutputStr) override;

 private:
  bool IsElementPreformatted(nsIContent* aNode);

 protected:
  /*
   * isHTMLParser should be set to true by the HTML parser which inherits from
   * this class. It avoids to redefine methods just for few changes.
   */
  bool mIsHTMLSerializer;

  bool mIsCopying;  // Set to true only while copying

  /*
   * mDisableEntityEncoding is higher than 0 while the serializer is serializing
   * the content of a element whose content is considerd CDATA by the
   * serializer (such elements are 'script', 'style', 'noscript' and
   * possibly others in XHTML) This doesn't have anything to do with if the
   * element is defined as CDATA in the DTD, it simply means we'll
   * output the content of the element without doing any entity encoding
   * what so ever.
   */
  int32_t mDisableEntityEncoding;

  // This is to ensure that we only do meta tag fixups when dealing with
  // whole documents.
  bool mRewriteEncodingDeclaration;

  // To keep track of First LI child of OL in selected range
  bool mIsFirstChildOfOL;

  // To keep track of startvalue of OL and first list item for nested lists
  struct olState {
    olState(int32_t aStart, bool aIsFirst)
        : startVal(aStart), isFirstListItem(aIsFirst) {}

    olState(const olState& aOlState) {
      startVal = aOlState.startVal;
      isFirstListItem = aOlState.isFirstListItem;
    }

    // the value of the start attribute in the OL
    int32_t startVal;

    // is true only before the serialization of the first li of an ol
    // should be false for other li in the list
    bool isFirstListItem;
  };

  // Stack to store one olState struct per <OL>.
  AutoTArray<olState, 8> mOLStateStack;

  bool HasNoChildren(nsIContent* aContent);
};

nsresult NS_NewXHTMLContentSerializer(nsIContentSerializer** aSerializer);

#endif
