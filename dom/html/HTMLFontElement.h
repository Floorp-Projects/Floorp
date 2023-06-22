/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef HTMLFontElement_h___
#define HTMLFontElement_h___

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"

namespace mozilla::dom {

class HTMLFontElement final : public nsGenericHTMLElement {
 public:
  explicit HTMLFontElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : nsGenericHTMLElement(std::move(aNodeInfo)) {}

  void GetColor(DOMString& aColor) { GetHTMLAttr(nsGkAtoms::color, aColor); }
  void SetColor(const nsAString& aColor, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::color, aColor, aError);
  }
  void GetFace(DOMString& aFace) { GetHTMLAttr(nsGkAtoms::face, aFace); }
  void SetFace(const nsAString& aFace, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::face, aFace, aError);
  }
  void GetSize(DOMString& aSize) { GetHTMLAttr(nsGkAtoms::size, aSize); }
  void SetSize(const nsAString& aSize, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::size, aSize, aError);
  }

  bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                      const nsAString& aValue,
                      nsIPrincipal* aMaybeScriptedPrincipal,
                      nsAttrValue& aResult) override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;
  nsMapRuleToAttributesFunc GetAttributeMappingFunction() const override;
  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

 protected:
  virtual ~HTMLFontElement();

  JSObject* WrapNode(JSContext*, JS::Handle<JSObject*> aGivenProto) override;

 private:
  static void MapAttributesIntoRule(MappedDeclarationsBuilder&);
};

}  // namespace mozilla::dom

#endif /* HTMLFontElement_h___ */
