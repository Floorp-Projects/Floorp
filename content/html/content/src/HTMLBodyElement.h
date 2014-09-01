/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
class HTMLBodyElement;

class BodyRule: public nsIStyleRule
{
  virtual ~BodyRule();

public:
  BodyRule(HTMLBodyElement* aPart);

  NS_DECL_ISUPPORTS

  // nsIStyleRule interface
  virtual void MapRuleInfoInto(nsRuleData* aRuleData) MOZ_OVERRIDE;
#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const MOZ_OVERRIDE;
#endif

  HTMLBodyElement*  mPart;  // not ref-counted, cleared by content 
};

class HTMLBodyElement MOZ_FINAL : public nsGenericHTMLElement,
                                  public nsIDOMHTMLBodyElement
{
public:
  using Element::GetText;
  using Element::SetText;

  HTMLBodyElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
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

  void GetText(nsString& aText)
  {
    GetHTMLAttr(nsGkAtoms::text, aText);
  }
  void SetText(const nsAString& aText, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::text, aText, aError);
  }
  void GetLink(nsString& aLink)
  {
    GetHTMLAttr(nsGkAtoms::link, aLink);
  }
  void SetLink(const nsAString& aLink, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::link, aLink, aError);
  }
  void GetVLink(nsString& aVLink)
  {
    GetHTMLAttr(nsGkAtoms::vlink, aVLink);
  }
  void SetVLink(const nsAString& aVLink, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::vlink, aVLink, aError);
  }
  void GetALink(nsString& aALink)
  {
    GetHTMLAttr(nsGkAtoms::alink, aALink);
  }
  void SetALink(const nsAString& aALink, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::alink, aALink, aError);
  }
  void GetBgColor(nsString& aBgColor)
  {
    GetHTMLAttr(nsGkAtoms::bgcolor, aBgColor);
  }
  void SetBgColor(const nsAString& aBgColor, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::bgcolor, aBgColor, aError);
  }
  void GetBackground(nsString& aBackground)
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
                              nsAttrValue& aResult) MOZ_OVERRIDE;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) MOZ_OVERRIDE;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const MOZ_OVERRIDE;
  NS_IMETHOD WalkContentStyleRules(nsRuleWalker* aRuleWalker) MOZ_OVERRIDE;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const MOZ_OVERRIDE;
  virtual already_AddRefed<nsIEditor> GetAssociatedEditor() MOZ_OVERRIDE;
  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  virtual bool IsEventAttributeName(nsIAtom* aName) MOZ_OVERRIDE;

protected:
  virtual ~HTMLBodyElement();

  virtual JSObject* WrapNode(JSContext *aCx) MOZ_OVERRIDE;

  nsRefPtr<BodyRule> mContentStyleRule;

private:
  static void MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                    nsRuleData* aData);
};

} // namespace dom
} // namespace mozilla

#endif /* HTMLBodyElement_h___ */
