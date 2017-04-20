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

class HTMLLegendElement final : public nsGenericHTMLElement
{
public:
  explicit HTMLLegendElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : nsGenericHTMLElement(aNodeInfo)
  {
  }

  NS_IMPL_FROMCONTENT_HTML_WITH_TAG(HTMLLegendElement, legend)

  using nsGenericHTMLElement::Focus;
  virtual void Focus(ErrorResult& aError) override;

  virtual bool PerformAccesskey(bool aKeyCausesActivation,
                                bool aIsTrustedEvent) override;

  // nsIContent
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) override;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) override;
  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult) override;
  virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              int32_t aModType) const override;
  nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify) override;
  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify) override;

  virtual nsresult Clone(mozilla::dom::NodeInfo* aNodeInfo, nsINode** aResult,
                         bool aPreallocateChildren) const override;

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

  nsINode* GetScopeChainParent() const override
  {
    Element* form = GetFormElement();
    return form ? form : nsGenericHTMLElement::GetScopeChainParent();
  }

protected:
  virtual ~HTMLLegendElement();

  virtual JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  /**
   * Get the fieldset content element that contains this legend.
   * Returns null if there is no fieldset containing this legend.
   */
  nsIContent* GetFieldSet() const;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_HTMLLegendElement_h */
