/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLLinkElement_h
#define mozilla_dom_HTMLLinkElement_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/Link.h"
#include "nsGenericHTMLElement.h"
#include "nsStyleLinkElement.h"

namespace mozilla {
class EventChainPostVisitor;
class EventChainPreVisitor;
namespace dom {

class HTMLLinkElement final : public nsGenericHTMLElement,
                              public nsStyleLinkElement,
                              public Link {
 public:
  explicit HTMLLinkElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // CC
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLLinkElement,
                                           nsGenericHTMLElement)

  NS_IMPL_FROMNODE_HTML_WITH_TAG(HTMLLinkElement, link);
  NS_DECL_ADDSIZEOFEXCLUDINGTHIS

  void LinkAdded();
  void LinkRemoved();

  // EventTarget
  void GetEventTargetParent(EventChainPreVisitor& aVisitor) override;
  MOZ_CAN_RUN_SCRIPT
  nsresult PostHandleEvent(EventChainPostVisitor& aVisitor) override;

  // nsINode
  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

  // nsIContent
  virtual nsresult BindToTree(Document* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent) override;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) override;
  virtual nsresult BeforeSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                 const nsAttrValueOrString* aValue,
                                 bool aNotify) override;
  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aSubjectPrincipal,
                                bool aNotify) override;
  virtual bool IsLink(nsIURI** aURI) const override;
  virtual already_AddRefed<nsIURI> GetHrefURI() const override;

  // Element
  virtual bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                              const nsAString& aValue,
                              nsIPrincipal* aMaybeScriptedPrincipal,
                              nsAttrValue& aResult) override;
  virtual void GetLinkTarget(nsAString& aTarget) override;
  virtual EventStates IntrinsicState() const override;

  void CreateAndDispatchEvent(Document* aDoc, const nsAString& aEventName);

  virtual void OnDNSPrefetchDeferred() override;
  virtual void OnDNSPrefetchRequested() override;
  virtual bool HasDeferredDNSPrefetchRequest() override;

  // WebIDL
  bool Disabled() const;
  void SetDisabled(bool aDisabled, ErrorResult& aRv);

  void GetHref(nsAString& aValue) {
    GetURIAttr(nsGkAtoms::href, nullptr, aValue);
  }
  void SetHref(const nsAString& aHref, nsIPrincipal* aTriggeringPrincipal,
               ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::href, aHref, aTriggeringPrincipal, aRv);
  }
  void SetHref(const nsAString& aHref, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::href, aHref, aRv);
  }
  void GetCrossOrigin(nsAString& aResult) {
    // Null for both missing and invalid defaults is ok, since we
    // always parse to an enum value, so we don't need an invalid
    // default, and we _want_ the missing default to be null.
    GetEnumAttr(nsGkAtoms::crossorigin, nullptr, aResult);
  }
  void SetCrossOrigin(const nsAString& aCrossOrigin, ErrorResult& aError) {
    SetOrRemoveNullableStringAttr(nsGkAtoms::crossorigin, aCrossOrigin, aError);
  }
  // nsAString for WebBrowserPersistLocalDocument
  void GetRel(nsAString& aValue) { GetHTMLAttr(nsGkAtoms::rel, aValue); }
  void SetRel(const nsAString& aRel, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::rel, aRel, aRv);
  }
  nsDOMTokenList* RelList();
  void GetMedia(DOMString& aValue) { GetHTMLAttr(nsGkAtoms::media, aValue); }
  void SetMedia(const nsAString& aMedia, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::media, aMedia, aRv);
  }
  void GetHreflang(DOMString& aValue) {
    GetHTMLAttr(nsGkAtoms::hreflang, aValue);
  }
  void SetHreflang(const nsAString& aHreflang, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::hreflang, aHreflang, aRv);
  }
  void GetAs(nsAString& aResult);
  void SetAs(const nsAString& aAs, ErrorResult& aRv) {
    SetAttr(nsGkAtoms::as, aAs, aRv);
  }
  nsDOMTokenList* Sizes() { return GetTokenList(nsGkAtoms::sizes); }
  void GetType(DOMString& aValue) { GetHTMLAttr(nsGkAtoms::type, aValue); }
  void SetType(const nsAString& aType, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::type, aType, aRv);
  }
  void GetCharset(nsAString& aValue) override {
    GetHTMLAttr(nsGkAtoms::charset, aValue);
  }
  void SetCharset(const nsAString& aCharset, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::charset, aCharset, aRv);
  }
  void GetRev(DOMString& aValue) { GetHTMLAttr(nsGkAtoms::rev, aValue); }
  void SetRev(const nsAString& aRev, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::rev, aRev, aRv);
  }
  void GetTarget(DOMString& aValue) { GetHTMLAttr(nsGkAtoms::target, aValue); }
  void SetTarget(const nsAString& aTarget, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::target, aTarget, aRv);
  }
  void GetIntegrity(nsAString& aIntegrity) const {
    GetHTMLAttr(nsGkAtoms::integrity, aIntegrity);
  }
  void SetIntegrity(const nsAString& aIntegrity, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::integrity, aIntegrity, aRv);
  }
  void SetReferrerPolicy(const nsAString& aReferrer, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::referrerpolicy, aReferrer, aError);
  }
  void GetReferrerPolicy(nsAString& aReferrer) {
    GetEnumAttr(nsGkAtoms::referrerpolicy, EmptyCString().get(), aReferrer);
  }

  CORSMode GetCORSMode() const {
    return AttrValueToCORSMode(GetParsedAttr(nsGkAtoms::crossorigin));
  }

  void NodeInfoChanged(Document* aOldDoc) final {
    ClearHasPendingLinkUpdate();
    nsGenericHTMLElement::NodeInfoChanged(aOldDoc);
  }

  static bool CheckPreloadAttrs(const nsAttrValue& aAs, const nsAString& aType,
                                const nsAString& aMedia, Document* aDocument);

 protected:
  virtual ~HTMLLinkElement();

  // nsStyleLinkElement
  Maybe<SheetInfo> GetStyleSheetInfo() final;

  RefPtr<nsDOMTokenList> mRelList;

  // The "explicitly enabled" flag. This flag is set whenever the `disabled`
  // attribute is explicitly unset, and makes alternate stylesheets not be
  // disabled by default anymore.
  //
  // See https://github.com/whatwg/html/issues/3840#issuecomment-481034206.
  bool mExplicitlyEnabled = false;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_HTMLLinkElement_h
