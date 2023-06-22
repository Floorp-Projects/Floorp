/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLPreElement_h
#define mozilla_dom_HTMLPreElement_h

#include "mozilla/Attributes.h"

#include "nsGenericHTMLElement.h"

namespace mozilla::dom {

class HTMLPreElement final : public nsGenericHTMLElement {
 public:
  explicit HTMLPreElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : nsGenericHTMLElement(std::move(aNodeInfo)) {}

  // nsISupports
  NS_INLINE_DECL_REFCOUNTING_INHERITED(HTMLPreElement, nsGenericHTMLElement)

  bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                      const nsAString& aValue,
                      nsIPrincipal* aMaybeScriptedPrincipal,
                      nsAttrValue& aResult) override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;
  nsMapRuleToAttributesFunc GetAttributeMappingFunction() const override;

  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // WebIDL API
  int32_t Width() const { return GetIntAttr(nsGkAtoms::width, 0); }
  void SetWidth(int32_t aWidth, mozilla::ErrorResult& rv) {
    rv = SetIntAttr(nsGkAtoms::width, aWidth);
  }

 protected:
  virtual ~HTMLPreElement();

  JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*>) override;

 private:
  static void MapAttributesIntoRule(MappedDeclarationsBuilder&);
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_HTMLPreElement_h
