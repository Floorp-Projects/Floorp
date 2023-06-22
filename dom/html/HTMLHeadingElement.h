/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLHeadingElement_h
#define mozilla_dom_HTMLHeadingElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"

namespace mozilla::dom {

class HTMLHeadingElement final : public nsGenericHTMLElement {
 public:
  explicit HTMLHeadingElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : nsGenericHTMLElement(std::move(aNodeInfo)) {
    MOZ_ASSERT(IsHTMLHeadingElement());
  }

  bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                      const nsAString& aValue,
                      nsIPrincipal* aMaybeScriptedPrincipal,
                      nsAttrValue& aResult) override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;
  nsMapRuleToAttributesFunc GetAttributeMappingFunction() const override;
  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  void SetAlign(const nsAString& aAlign, ErrorResult& aError) {
    return SetHTMLAttr(nsGkAtoms::align, aAlign, aError);
  }
  void GetAlign(DOMString& aAlign) const {
    return GetHTMLAttr(nsGkAtoms::align, aAlign);
  }

  int32_t AccessibilityLevel() const {
    nsAtom* name = NodeInfo()->NameAtom();
    if (name == nsGkAtoms::h1) {
      return 1;
    }
    if (name == nsGkAtoms::h2) {
      return 2;
    }
    if (name == nsGkAtoms::h3) {
      return 3;
    }
    if (name == nsGkAtoms::h4) {
      return 4;
    }
    if (name == nsGkAtoms::h5) {
      return 5;
    }
    MOZ_ASSERT(name == nsGkAtoms::h6);
    return 6;
  }

  NS_IMPL_FROMNODE_HELPER(HTMLHeadingElement, IsHTMLHeadingElement())

 protected:
  virtual ~HTMLHeadingElement();

  JSObject* WrapNode(JSContext*, JS::Handle<JSObject*> aGivenProto) override;

 private:
  static void MapAttributesIntoRule(MappedDeclarationsBuilder&);
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_HTMLHeadingElement_h
