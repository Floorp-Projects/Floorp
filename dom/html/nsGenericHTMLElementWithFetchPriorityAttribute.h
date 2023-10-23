/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_HTML_NSGENERICHTMLELEMENTWITHFETCHPRIORITYATTRIBUTE_H_
#define DOM_HTML_NSGENERICHTMLELEMENTWITHFETCHPRIORITYATTRIBUTE_H_

#include "nsGenericHTMLElement.h"

#include <cstdint>

#include "nsStringFwd.h"

namespace mozilla {

namespace dom {
enum class FetchPriority : uint8_t;
}  // namespace dom

// <https://html.spec.whatwg.org/#fetch-priority-attribute>.
class nsGenericHTMLElementWithFetchPriorityAttribute
    : public nsGenericHTMLElement {
 public:
  using nsGenericHTMLElement::nsGenericHTMLElement;

  bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                      const nsAString& aValue,
                      nsIPrincipal* aMaybeScriptedPrincipal,
                      nsAttrValue& aResult) override;

  void GetFetchPriority(nsAString& aFetchPriority) const;

  void SetFetchPriority(const nsAString& aFetchPriority) {
    SetHTMLAttr(nsGkAtoms::fetchpriority, aFetchPriority);
  }

  // <https://html.spec.whatwg.org/#fetch-priority-attribute>.
  static mozilla::dom::FetchPriority ToFetchPriority(const nsAString& aValue);

 private:
  static void ParseFetchPriority(const nsAString& aValue, nsAttrValue& aResult);
};

}  // namespace mozilla

#endif  // DOM_HTML_NSGENERICHTMLELEMENTWITHFETCHPRIORITYATTRIBUTE_H_
