/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLLIElement_h
#define mozilla_dom_HTMLLIElement_h

#include "mozilla/Attributes.h"

#include "nsGenericHTMLElement.h"

namespace mozilla::dom {

class HTMLLIElement final : public nsGenericHTMLElement {
 public:
  explicit HTMLLIElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : nsGenericHTMLElement(std::move(aNodeInfo)) {}

  // nsISupports
  NS_INLINE_DECL_REFCOUNTING_INHERITED(HTMLLIElement, nsGenericHTMLElement)

  bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                      const nsAString& aValue,
                      nsIPrincipal* aMaybeScriptedPrincipal,
                      nsAttrValue& aResult) override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;
  nsMapRuleToAttributesFunc GetAttributeMappingFunction() const override;
  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // WebIDL API
  void GetType(DOMString& aType) { GetHTMLAttr(nsGkAtoms::type, aType); }
  void SetType(const nsAString& aType, mozilla::ErrorResult& rv) {
    SetHTMLAttr(nsGkAtoms::type, aType, rv);
  }
  int32_t Value() const { return GetIntAttr(nsGkAtoms::value, 0); }
  void SetValue(int32_t aValue, mozilla::ErrorResult& rv) {
    SetHTMLIntAttr(nsGkAtoms::value, aValue, rv);
  }

  static const nsAttrValue::EnumTable kULTypeTable[];
  static const nsAttrValue::EnumTable kOLTypeTable[];

 protected:
  virtual ~HTMLLIElement();

  JSObject* WrapNode(JSContext*, JS::Handle<JSObject*> aGivenProto) override;

 private:
  static void MapAttributesIntoRule(MappedDeclarationsBuilder&);
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_HTMLLIElement_h
