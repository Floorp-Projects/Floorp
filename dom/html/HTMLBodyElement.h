/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef HTMLBodyElement_h___
#define HTMLBodyElement_h___

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"

namespace mozilla {

class EditorBase;

namespace dom {

class OnBeforeUnloadEventHandlerNonNull;

class HTMLBodyElement final : public nsGenericHTMLElement {
 public:
  using Element::GetText;

  explicit HTMLBodyElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : nsGenericHTMLElement(std::move(aNodeInfo)) {}

  // nsISupports
  NS_INLINE_DECL_REFCOUNTING_INHERITED(HTMLBodyElement, nsGenericHTMLElement)

  NS_IMPL_FROMNODE_HTML_WITH_TAG(HTMLBodyElement, body);

  // Event listener stuff; we need to declare only the ones we need to
  // forward to window that don't come from nsIDOMHTMLBodyElement.
#define EVENT(name_, id_, type_, struct_) /* nothing; handled by the shim */
#define WINDOW_EVENT_HELPER(name_, type_) \
  type_* GetOn##name_();                  \
  void SetOn##name_(type_* handler);
#define WINDOW_EVENT(name_, id_, type_, struct_) \
  WINDOW_EVENT_HELPER(name_, EventHandlerNonNull)
#define BEFOREUNLOAD_EVENT(name_, id_, type_, struct_) \
  WINDOW_EVENT_HELPER(name_, OnBeforeUnloadEventHandlerNonNull)
#include "mozilla/EventNameList.h"  // IWYU pragma: keep
#undef BEFOREUNLOAD_EVENT
#undef WINDOW_EVENT
#undef WINDOW_EVENT_HELPER
#undef EVENT

  void GetText(nsAString& aText) { GetHTMLAttr(nsGkAtoms::text, aText); }
  void SetText(const nsAString& aText) { SetHTMLAttr(nsGkAtoms::text, aText); }
  void SetText(const nsAString& aText, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::text, aText, aError);
  }
  void GetLink(nsAString& aLink) { GetHTMLAttr(nsGkAtoms::link, aLink); }
  void SetLink(const nsAString& aLink) { SetHTMLAttr(nsGkAtoms::link, aLink); }
  void SetLink(const nsAString& aLink, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::link, aLink, aError);
  }
  void GetVLink(nsAString& aVLink) { GetHTMLAttr(nsGkAtoms::vlink, aVLink); }
  void SetVLink(const nsAString& aVLink) {
    SetHTMLAttr(nsGkAtoms::vlink, aVLink);
  }
  void SetVLink(const nsAString& aVLink, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::vlink, aVLink, aError);
  }
  void GetALink(nsAString& aALink) { GetHTMLAttr(nsGkAtoms::alink, aALink); }
  void SetALink(const nsAString& aALink) {
    SetHTMLAttr(nsGkAtoms::alink, aALink);
  }
  void SetALink(const nsAString& aALink, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::alink, aALink, aError);
  }
  void GetBgColor(nsAString& aBgColor) {
    GetHTMLAttr(nsGkAtoms::bgcolor, aBgColor);
  }
  void SetBgColor(const nsAString& aBgColor) {
    SetHTMLAttr(nsGkAtoms::bgcolor, aBgColor);
  }
  void SetBgColor(const nsAString& aBgColor, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::bgcolor, aBgColor, aError);
  }
  void GetBackground(DOMString& aBackground) {
    GetHTMLAttr(nsGkAtoms::background, aBackground);
  }
  void GetBackground(nsAString& aBackground) {
    GetHTMLAttr(nsGkAtoms::background, aBackground);
  }
  void SetBackground(const nsAString& aBackground, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::background, aBackground, aError);
  }

  virtual bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                              const nsAString& aValue,
                              nsIPrincipal* aMaybeScriptedPrincipal,
                              nsAttrValue& aResult) override;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction()
      const override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;
  virtual already_AddRefed<EditorBase> GetAssociatedEditor() override;
  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  virtual bool IsEventAttributeNameInternal(nsAtom* aName) override;

  virtual nsresult BindToTree(BindContext&, nsINode& aParent) override;
  /**
   * Called when an attribute has just been changed
   */
  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aSubjectPrincipal,
                                bool aNotify) override;

 protected:
  virtual ~HTMLBodyElement();

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

 private:
  static void MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                    MappedDeclarations&);
};

}  // namespace dom
}  // namespace mozilla

#endif /* HTMLBodyElement_h___ */
