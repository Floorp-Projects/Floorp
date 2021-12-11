/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLHRElement_h
#define mozilla_dom_HTMLHRElement_h

#include "nsGenericHTMLElement.h"
#include "nsMappedAttributes.h"
#include "nsAttrValueInlines.h"

namespace mozilla {
namespace dom {

class HTMLHRElement final : public nsGenericHTMLElement {
 public:
  explicit HTMLHRElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  // nsISupports
  NS_INLINE_DECL_REFCOUNTING_INHERITED(HTMLHRElement, nsGenericHTMLElement)

  virtual bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                              const nsAString& aValue,
                              nsIPrincipal* aMaybeScriptedPrincipal,
                              nsAttrValue& aResult) override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction()
      const override;
  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // WebIDL API
  void GetAlign(nsAString& aValue) const {
    GetHTMLAttr(nsGkAtoms::align, aValue);
  }
  void SetAlign(const nsAString& aAlign, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::align, aAlign, aError);
  }

  void GetColor(nsAString& aValue) const {
    GetHTMLAttr(nsGkAtoms::color, aValue);
  }
  void SetColor(const nsAString& aColor, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::color, aColor, aError);
  }

  bool NoShade() const { return GetBoolAttr(nsGkAtoms::noshade); }
  void SetNoShade(bool aNoShade, ErrorResult& aError) {
    SetHTMLBoolAttr(nsGkAtoms::noshade, aNoShade, aError);
  }

  void GetSize(nsAString& aValue) const {
    GetHTMLAttr(nsGkAtoms::size, aValue);
  }
  void SetSize(const nsAString& aSize, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::size, aSize, aError);
  }

  void GetWidth(nsAString& aValue) const {
    GetHTMLAttr(nsGkAtoms::width, aValue);
  }
  void SetWidth(const nsAString& aWidth, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::width, aWidth, aError);
  }

 protected:
  virtual ~HTMLHRElement();

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

 private:
  static void MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                    MappedDeclarations&);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_HTMLHRElement_h
