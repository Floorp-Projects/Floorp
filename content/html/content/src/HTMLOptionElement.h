/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLOptionElement_h__
#define mozilla_dom_HTMLOptionElement_h__

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLOptionElement.h"
#include "mozilla/dom/HTMLFormElement.h"

namespace mozilla {
namespace dom {

class HTMLSelectElement;

class HTMLOptionElement MOZ_FINAL : public nsGenericHTMLElement,
                                    public nsIDOMHTMLOptionElement
{
public:
  explicit HTMLOptionElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);

  static already_AddRefed<HTMLOptionElement>
    Option(const GlobalObject& aGlobal,
           const Optional<nsAString>& aText,
           const Optional<nsAString>& aValue,
           const Optional<bool>& aDefaultSelected,
           const Optional<bool>& aSelected, ErrorResult& aError);

  NS_IMPL_FROMCONTENT_HTML_WITH_TAG(HTMLOptionElement, option)

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMHTMLOptionElement
  using mozilla::dom::Element::SetText;
  using mozilla::dom::Element::GetText;
  NS_DECL_NSIDOMHTMLOPTIONELEMENT

  bool Selected() const;
  bool DefaultSelected() const;

  virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              int32_t aModType) const MOZ_OVERRIDE;

  virtual nsresult BeforeSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                 const nsAttrValueOrString* aValue,
                                 bool aNotify) MOZ_OVERRIDE;
  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify) MOZ_OVERRIDE;

  void SetSelectedInternal(bool aValue, bool aNotify);

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) MOZ_OVERRIDE;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) MOZ_OVERRIDE;

  // nsIContent
  virtual EventStates IntrinsicState() const MOZ_OVERRIDE;

  virtual nsresult Clone(mozilla::dom::NodeInfo* aNodeInfo, nsINode** aResult) const MOZ_OVERRIDE;

  nsresult CopyInnerTo(mozilla::dom::Element* aDest);

  virtual bool IsDisabled() const MOZ_OVERRIDE {
    return HasAttr(kNameSpaceID_None, nsGkAtoms::disabled);
  }

  bool Disabled() const
  {
    return GetBoolAttr(nsGkAtoms::disabled);
  }

  void SetDisabled(bool aValue, ErrorResult& aRv)
  {
    SetHTMLBoolAttr(nsGkAtoms::disabled, aValue, aRv);
  }

  HTMLFormElement* GetForm();

  // The XPCOM GetLabel is OK for us
  void SetLabel(const nsAString& aLabel, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::label, aLabel, aError);
  }

  // The XPCOM DefaultSelected is OK for us
  void SetDefaultSelected(bool aValue, ErrorResult& aRv)
  {
    SetHTMLBoolAttr(nsGkAtoms::selected, aValue, aRv);
  }

  // The XPCOM Selected is OK for us
  void SetSelected(bool aValue, ErrorResult& aRv)
  {
    aRv = SetSelected(aValue);
  }

  // The XPCOM GetValue is OK for us
  void SetValue(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::value, aValue, aRv);
  }

  // The XPCOM GetText is OK for us
  void SetText(const nsAString& aValue, ErrorResult& aRv)
  {
    aRv = SetText(aValue);
  }

  int32_t Index();

protected:
  virtual ~HTMLOptionElement();

  virtual JSObject* WrapNode(JSContext* aCx) MOZ_OVERRIDE;

  /**
   * Get the select content element that contains this option, this
   * intentionally does not return nsresult, all we care about is if
   * there's a select associated with this option or not.
   */
  HTMLSelectElement* GetSelect();

  bool mSelectedChanged;
  bool mIsSelected;

  // True only while we're under the SetOptionsSelectedByIndex call when our
  // "selected" attribute is changing and mSelectedChanged is false.
  bool mIsInSetDefaultSelected;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLOptionElement_h__
