/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef HTMLDivElement_h___
#define HTMLDivElement_h___

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"

namespace mozilla::dom {

class HTMLDivElement final : public nsGenericHTMLElement {
 public:
  explicit HTMLDivElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : nsGenericHTMLElement(std::move(aNodeInfo)) {
    MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::div),
               "HTMLDivElement should be a div");
  }

  void GetAlign(DOMString& aAlign) { GetHTMLAttr(nsGkAtoms::align, aAlign); }
  void SetAlign(const nsAString& aAlign, mozilla::ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::align, aAlign, aError);
  }

  bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                      const nsAString& aValue,
                      nsIPrincipal* aMaybeScriptedPrincipal,
                      nsAttrValue& aResult) override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;
  nsMapRuleToAttributesFunc GetAttributeMappingFunction() const override;
  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

 protected:
  virtual ~HTMLDivElement();

  JSObject* WrapNode(JSContext*, JS::Handle<JSObject*> aGivenProto) override;

 private:
  static void MapAttributesIntoRule(MappedDeclarationsBuilder&);
};

}  // namespace mozilla::dom

#endif /* HTMLDivElement_h___ */
