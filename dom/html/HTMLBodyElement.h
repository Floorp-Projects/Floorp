/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef HTMLBodyElement_h___
#define HTMLBodyElement_h___

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLBodyElement.h"
#include "nsIStyleRule.h"

namespace mozilla {
namespace dom {

class OnBeforeUnloadEventHandlerNonNull;

class HTMLBodyElement final : public nsGenericHTMLElement,
                              public nsIDOMHTMLBodyElement
{
public:
  using Element::GetText;
  using Element::SetText;

  explicit HTMLBodyElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : nsGenericHTMLElement(aNodeInfo)
  {
  }

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMHTMLBodyElement
  NS_DECL_NSIDOMHTMLBODYELEMENT

  // Event listener stuff; we need to declare only the ones we need to
  // forward to window that don't come from nsIDOMHTMLBodyElement.
#define EVENT(name_, id_, type_, struct_) /* nothing; handled by the shim */
#define WINDOW_EVENT_HELPER(name_, type_)                               \
  type_* GetOn##name_();                                                \
  void SetOn##name_(type_* handler);
#define WINDOW_EVENT(name_, id_, type_, struct_)                        \
  WINDOW_EVENT_HELPER(name_, EventHandlerNonNull)
#define BEFOREUNLOAD_EVENT(name_, id_, type_, struct_)                  \
  WINDOW_EVENT_HELPER(name_, OnBeforeUnloadEventHandlerNonNull)
#include "mozilla/EventNameList.h" // IWYU pragma: keep
#undef BEFOREUNLOAD_EVENT
#undef WINDOW_EVENT
#undef WINDOW_EVENT_HELPER
#undef EVENT

  void GetText(DOMString& aText)
  {
    GetHTMLAttr(nsGkAtoms::text, aText);
  }
  void SetText(const nsAString& aText, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::text, aText, aError);
  }
  void GetLink(DOMString& aLink)
  {
    GetHTMLAttr(nsGkAtoms::link, aLink);
  }
  void SetLink(const nsAString& aLink, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::link, aLink, aError);
  }
  void GetVLink(DOMString& aVLink)
  {
    GetHTMLAttr(nsGkAtoms::vlink, aVLink);
  }
  void SetVLink(const nsAString& aVLink, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::vlink, aVLink, aError);
  }
  void GetALink(DOMString& aALink)
  {
    GetHTMLAttr(nsGkAtoms::alink, aALink);
  }
  void SetALink(const nsAString& aALink, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::alink, aALink, aError);
  }
  void GetBgColor(DOMString& aBgColor)
  {
    GetHTMLAttr(nsGkAtoms::bgcolor, aBgColor);
  }
  void SetBgColor(const nsAString& aBgColor, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::bgcolor, aBgColor, aError);
  }
  void GetBackground(DOMString& aBackground)
  {
    GetHTMLAttr(nsGkAtoms::background, aBackground);
  }
  void SetBackground(const nsAString& aBackground, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::background, aBackground, aError);
  }

  virtual bool ParseAttribute(int32_t aNamespaceID,
                              nsIAtom* aAttribute,
                              const nsAString& aValue,
                              nsAttrValue& aResult) override;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const override;
  virtual already_AddRefed<nsIEditor> GetAssociatedEditor() override;
  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                         bool aPreallocateChildren) const override;

  virtual bool IsEventAttributeName(nsIAtom* aName) override;


  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) override;
  /**
   * Called when an attribute has just been changed
   */
  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify) override;

protected:
  virtual ~HTMLBodyElement();

  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

private:
  static void MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                    GenericSpecifiedValues* aGenericData);
};

} // namespace dom
} // namespace mozilla

#endif /* HTMLBodyElement_h___ */
