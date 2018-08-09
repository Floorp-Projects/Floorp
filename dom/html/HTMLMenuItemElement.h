/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLMenuItemElement_h
#define mozilla_dom_HTMLMenuItemElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"

namespace mozilla {

class EventChainPreVisitor;

namespace dom {

class Visitor;

class HTMLMenuItemElement final : public nsGenericHTMLElement
{
public:
  using mozilla::dom::Element::GetText;

  HTMLMenuItemElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
                      mozilla::dom::FromParser aFromParser);

  NS_IMPL_FROMNODE_HTML_WITH_TAG(HTMLMenuItemElement, menuitem)

  // nsISupports
  NS_INLINE_DECL_REFCOUNTING_INHERITED(HTMLMenuItemElement,
                                       nsGenericHTMLElement)

  void GetEventTargetParent(EventChainPreVisitor& aVisitor) override;
  virtual nsresult PostHandleEvent(
                     EventChainPostVisitor& aVisitor) override;

  virtual nsresult BindToTree(nsIDocument* aDocument,
                              nsIContent* aParent,
                              nsIContent* aBindingParent) override;

  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsAtom* aAttribute,
                                const nsAString& aValue,
                                nsIPrincipal* aMaybeScriptedPrincipal,
                                nsAttrValue& aResult) override;

  virtual void DoneCreatingElement() override;

  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  uint8_t GetType() const { return mType; }

  /**
   * Syntax sugar to make it easier to check for checked and checked dirty
   */
  bool IsChecked() const { return mChecked; }
  bool IsCheckedDirty() const { return mCheckedDirty; }

  void GetText(nsAString& aText);

  // WebIDL

  void GetType(DOMString& aValue);
  void SetType(const nsAString& aType, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::type, aType, aError);
  }

  // nsAString needed for HTMLMenuElement
  void GetLabel(nsAString& aValue)
  {
    if (!GetAttr(kNameSpaceID_None, nsGkAtoms::label, aValue)) {
      GetText(aValue);
    }
  }
  void SetLabel(const nsAString& aLabel, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::label, aLabel, aError);
  }

  // nsAString needed for HTMLMenuElement
  void GetIcon(nsAString& aValue)
  {
    GetURIAttr(nsGkAtoms::icon, nullptr, aValue);
  }
  void SetIcon(const nsAString& aIcon, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::icon, aIcon, aError);
  }

  bool Disabled() const
  {
    return GetBoolAttr(nsGkAtoms::disabled);
  }
  void SetDisabled(bool aDisabled, ErrorResult& aError)
  {
    SetHTMLBoolAttr(nsGkAtoms::disabled, aDisabled, aError);
  }

  bool Checked() const
  {
    return mChecked;
  }
  void SetChecked(bool aChecked);

  void GetRadiogroup(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::radiogroup, aValue);
  }
  void SetRadiogroup(const nsAString& aRadiogroup, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::radiogroup, aRadiogroup, aError);
  }

  bool DefaultChecked() const
  {
    return GetBoolAttr(nsGkAtoms::checked);
  }
  void SetDefaultChecked(bool aDefault, ErrorResult& aError)
  {
    SetHTMLBoolAttr(nsGkAtoms::checked, aDefault, aError);
  }

protected:
  virtual ~HTMLMenuItemElement();

  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;


protected:
  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aSubjectPrincipal,
                                bool aNotify) override;

  void WalkRadioGroup(Visitor* aVisitor);

  HTMLMenuItemElement* GetSelectedRadio();

  void AddedToRadioGroup();

  void InitChecked();

  friend class ClearCheckedVisitor;
  friend class SetCheckedDirtyVisitor;

  void ClearChecked() { mChecked = false; }
  void SetCheckedDirty() { mCheckedDirty = true; }

private:
  uint8_t mType : 2;
  bool mParserCreating : 1;
  bool mShouldInitChecked : 1;
  bool mCheckedDirty : 1;
  bool mChecked : 1;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLMenuItemElement_h
