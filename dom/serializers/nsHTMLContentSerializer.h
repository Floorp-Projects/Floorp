/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsIContentSerializer implementation that can be used with an
 * nsIDocumentEncoder to convert an HTML (not XHTML!) DOM to an HTML
 * string that could be parsed into more or less the original DOM.
 */

#ifndef nsHTMLContentSerializer_h__
#define nsHTMLContentSerializer_h__

#include "mozilla/Attributes.h"
#include "nsXHTMLContentSerializer.h"
#include "nsString.h"

class nsAtom;

class nsHTMLContentSerializer final : public nsXHTMLContentSerializer {
 public:
  nsHTMLContentSerializer();
  virtual ~nsHTMLContentSerializer();

  NS_IMETHOD AppendElementStart(
      mozilla::dom::Element* aElement,
      mozilla::dom::Element* aOriginalElement) override;

  NS_IMETHOD AppendElementEnd(mozilla::dom::Element* aElement,
                              mozilla::dom::Element* aOriginalElement) override;

  NS_IMETHOD AppendDocumentStart(mozilla::dom::Document* aDocument) override;

 protected:
  [[nodiscard]] virtual bool SerializeHTMLAttributes(
      mozilla::dom::Element* aContent, mozilla::dom::Element* aOriginalElement,
      nsAString& aTagPrefix, const nsAString& aTagNamespaceURI,
      nsAtom* aTagName, int32_t aNamespace, nsAString& aStr);

  [[nodiscard]] virtual bool AppendAndTranslateEntities(
      const nsAString& aStr, nsAString& aOutputStr) override;

 private:
  static const uint8_t kEntities[];
  static const uint8_t kAttrEntities[];
  static const char* const kEntityStrings[];
};

nsresult NS_NewHTMLContentSerializer(nsIContentSerializer** aSerializer);

#endif
