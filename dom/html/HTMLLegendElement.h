/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

class HTMLLegendElement MOZ_FINAL : public nsGenericHTMLElement
{
public:
  explicit HTMLLegendElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : nsGenericHTMLElement(aNodeInfo)
  {
  }

  NS_IMPL_FROMCONTENT_HTML_WITH_TAG(HTMLLegendElement, legend)

  using nsGenericHTMLElement::Focus;
  virtual void Focus(ErrorResult& aError) MOZ_OVERRIDE;

  virtual void PerformAccesskey(bool aKeyCausesActivation,
                                bool aIsTrustedEvent) MOZ_OVERRIDE;

  // nsIContent
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) MOZ_OVERRIDE;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) MOZ_OVERRIDE;
  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult) MOZ_OVERRIDE;
  virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              int32_t aModType) const MOZ_OVERRIDE;
  nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify) MOZ_OVERRIDE;
  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify) MOZ_OVERRIDE;

  virtual nsresult Clone(mozilla::dom::NodeInfo* aNodeInfo, nsINode** aResult) const MOZ_OVERRIDE;

  Element* GetFormElement() const
  {
    nsCOMPtr<nsIFormControl> fieldsetControl = do_QueryInterface(GetFieldSet());

    return fieldsetControl ? fieldsetControl->GetFormElement() : nullptr;
  }

  /**
   * WebIDL Interface
   */

  already_AddRefed<HTMLFormElement> GetForm();

  void GetAlign(DOMString& aAlign)
  {
    GetHTMLAttr(nsGkAtoms::align, aAlign);
  }

  void SetAlign(const nsAString& aAlign, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::align, aAlign, aError);
  }

  nsINode* GetScopeChainParent() const MOZ_OVERRIDE
  {
    Element* form = GetFormElement();
    return form ? form : nsGenericHTMLElement::GetScopeChainParent();
  }

protected:
  virtual ~HTMLLegendElement();

  virtual JSObject* WrapNode(JSContext* aCx) MOZ_OVERRIDE;

  /**
   * Get the fieldset content element that contains this legend.
   * Returns null if there is no fieldset containing this legend.
   */
  nsIContent* GetFieldSet() const;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_HTMLLegendElement_h */
