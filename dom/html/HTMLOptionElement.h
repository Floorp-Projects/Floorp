/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLOptionElement_h__
#define mozilla_dom_HTMLOptionElement_h__

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"
#include "mozilla/dom/HTMLFormElement.h"

namespace mozilla::dom {

class HTMLSelectElement;

class HTMLOptionElement final : public nsGenericHTMLElement {
 public:
  explicit HTMLOptionElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  static already_AddRefed<HTMLOptionElement> Option(
      const GlobalObject& aGlobal, const nsAString& aText,
      const Optional<nsAString>& aValue, bool aDefaultSelected, bool aSelected,
      ErrorResult& aError);

  NS_IMPL_FROMNODE_HTML_WITH_TAG(HTMLOptionElement, option)

  // nsISupports
  NS_INLINE_DECL_REFCOUNTING_INHERITED(HTMLOptionElement, nsGenericHTMLElement)

  using mozilla::dom::Element::GetText;

  bool Selected() const { return State().HasState(ElementState::CHECKED); }
  void SetSelected(bool aValue);

  void SetSelectedChanged(bool aValue) { mSelectedChanged = aValue; }

  nsChangeHint GetAttributeChangeHint(const nsAtom* aAttribute,
                                      int32_t aModType) const override;

  void BeforeSetAttr(int32_t aNamespaceID, nsAtom* aName,
                     const nsAttrValue* aValue, bool aNotify) override;
  void AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                    const nsAttrValue* aValue, const nsAttrValue* aOldValue,
                    nsIPrincipal* aSubjectPrincipal, bool aNotify) override;

  void SetSelectedInternal(bool aValue, bool aNotify);

  /**
   * This callback is called by an optgroup on all its option elements whenever
   * its disabled state is changed so that option elements can know their
   * disabled state might have changed.
   */
  void OptGroupDisabledChanged(bool aNotify);

  /**
   * Check our disabled content attribute and optgroup's (if it exists) disabled
   * state to decide whether our disabled flag should be toggled.
   */
  void UpdateDisabledState(bool aNotify);

  nsresult BindToTree(BindContext&, nsINode& aParent) override;
  void UnbindFromTree(bool aNullParent = true) override;

  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  nsresult CopyInnerTo(mozilla::dom::Element* aDest);

  bool Disabled() const { return GetBoolAttr(nsGkAtoms::disabled); }

  void SetDisabled(bool aValue, ErrorResult& aRv) {
    SetHTMLBoolAttr(nsGkAtoms::disabled, aValue, aRv);
  }

  HTMLFormElement* GetForm();

  void GetRenderedLabel(nsAString& aLabel) {
    if (!GetAttr(nsGkAtoms::label, aLabel) || aLabel.IsEmpty()) {
      GetText(aLabel);
    }
  }

  void GetLabel(nsAString& aLabel) {
    if (!GetAttr(nsGkAtoms::label, aLabel)) {
      GetText(aLabel);
    }
  }
  void SetLabel(const nsAString& aLabel, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::label, aLabel, aError);
  }

  bool DefaultSelected() const { return HasAttr(nsGkAtoms::selected); }
  void SetDefaultSelected(bool aValue, ErrorResult& aRv) {
    SetHTMLBoolAttr(nsGkAtoms::selected, aValue, aRv);
  }

  void GetValue(nsAString& aValue) {
    if (!GetAttr(nsGkAtoms::value, aValue)) {
      GetText(aValue);
    }
  }
  void SetValue(const nsAString& aValue, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::value, aValue, aRv);
  }

  void GetText(nsAString& aText);
  void SetText(const nsAString& aText, ErrorResult& aRv);

  int32_t Index();

 protected:
  virtual ~HTMLOptionElement();

  JSObject* WrapNode(JSContext*, JS::Handle<JSObject*> aGivenProto) override;

  /**
   * Get the select content element that contains this option, this
   * intentionally does not return nsresult, all we care about is if
   * there's a select associated with this option or not.
   */
  HTMLSelectElement* GetSelect();

  bool mSelectedChanged = false;

  // True only while we're under the SetOptionsSelectedByIndex call when our
  // "selected" attribute is changing and mSelectedChanged is false.
  bool mIsInSetDefaultSelected = false;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_HTMLOptionElement_h__
