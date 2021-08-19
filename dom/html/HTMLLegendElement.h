/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLLegendElement_h
#define mozilla_dom_HTMLLegendElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"
#include "mozilla/dom/HTMLFormElement.h"

namespace mozilla {
namespace dom {

class HTMLLegendElement final : public nsGenericHTMLElement {
 public:
  explicit HTMLLegendElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : nsGenericHTMLElement(std::move(aNodeInfo)) {}

  NS_IMPL_FROMNODE_HTML_WITH_TAG(HTMLLegendElement, legend)

  using nsGenericHTMLElement::Focus;
  virtual void Focus(const FocusOptions& aOptions,
                     const mozilla::dom::CallerType aCallerType,
                     ErrorResult& aError) override;

  virtual bool PerformAccesskey(bool aKeyCausesActivation,
                                bool aIsTrustedEvent) override;

  // nsIContent
  virtual nsresult BindToTree(BindContext&, nsINode& aParent) override;
  virtual void UnbindFromTree(bool aNullParent = true) override;
  virtual bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                              const nsAString& aValue,
                              nsIPrincipal* aMaybeScriptedPrincipal,
                              nsAttrValue& aResult) override;
  virtual nsChangeHint GetAttributeChangeHint(const nsAtom* aAttribute,
                                              int32_t aModType) const override;

  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  HTMLFormElement* GetFormElement() const {
    nsCOMPtr<nsIFormControl> fieldsetControl = do_QueryInterface(GetFieldSet());

    return fieldsetControl ? fieldsetControl->GetForm() : nullptr;
  }

  enum class LegendAlignValue : uint8_t {
    Left,
    Right,
    Center,
    Bottom,
    Top,
    InlineStart,
    InlineEnd,
  };

  /**
   * Return the align value to use for the given fieldset writing-mode.
   * (This method resolves Left/Right to the appropriate InlineStart/InlineEnd).
   * @param aCBWM the fieldset writing-mode
   * @note we only parse left/right/center, so this method returns Center,
   * InlineStart or InlineEnd.
   */
  LegendAlignValue LogicalAlign(mozilla::WritingMode aCBWM) const;

  /**
   * WebIDL Interface
   */

  already_AddRefed<HTMLFormElement> GetForm();

  void GetAlign(DOMString& aAlign) { GetHTMLAttr(nsGkAtoms::align, aAlign); }

  void SetAlign(const nsAString& aAlign, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::align, aAlign, aError);
  }

  nsINode* GetScopeChainParent() const override {
    Element* form = GetFormElement();
    return form ? form : nsGenericHTMLElement::GetScopeChainParent();
  }

 protected:
  virtual ~HTMLLegendElement();

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

  /**
   * Get the fieldset content element that contains this legend.
   * Returns null if there is no fieldset containing this legend.
   */
  nsIContent* GetFieldSet() const;
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_HTMLLegendElement_h */
