/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLAnchorElement_h
#define mozilla_dom_HTMLAnchorElement_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/Link.h"
#include "nsGenericHTMLElement.h"
#include "nsDOMTokenList.h"

namespace mozilla {
class EventChainPostVisitor;
class EventChainPreVisitor;
namespace dom {

class HTMLAnchorElement final : public nsGenericHTMLElement, public Link {
 public:
  using Element::GetText;

  explicit HTMLAnchorElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : nsGenericHTMLElement(std::move(aNodeInfo)), Link(this) {}

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // CC
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLAnchorElement,
                                           nsGenericHTMLElement)

  NS_IMPL_FROMNODE_HTML_WITH_TAG(HTMLAnchorElement, a);

  virtual int32_t TabIndexDefault() override;
  virtual bool Draggable() const override;

  // Element
  virtual bool IsInteractiveHTMLContent(bool aIgnoreTabindex) const override;

  // DOM memory reporter participant
  NS_DECL_ADDSIZEOFEXCLUDINGTHIS

  virtual nsresult BindToTree(Document* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent) override;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) override;
  virtual bool IsHTMLFocusable(bool aWithMouse, bool* aIsFocusable,
                               int32_t* aTabIndex) override;

  void GetEventTargetParent(EventChainPreVisitor& aVisitor) override;
  MOZ_CAN_RUN_SCRIPT
  nsresult PostHandleEvent(EventChainPostVisitor& aVisitor) override;
  virtual bool IsLink(nsIURI** aURI) const override;
  virtual void GetLinkTarget(nsAString& aTarget) override;
  virtual already_AddRefed<nsIURI> GetHrefURI() const override;

  virtual nsresult BeforeSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                 const nsAttrValueOrString* aValue,
                                 bool aNotify) override;
  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aSubjectPrincipal,
                                bool aNotify) override;

  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  virtual EventStates IntrinsicState() const override;

  virtual void OnDNSPrefetchDeferred() override;
  virtual void OnDNSPrefetchRequested() override;
  virtual bool HasDeferredDNSPrefetchRequest() override;

  // WebIDL API

  void GetHref(nsAString& aValue) {
    GetURIAttr(nsGkAtoms::href, nullptr, aValue);
  }
  void SetHref(const nsAString& aValue, mozilla::ErrorResult& rv) {
    SetHTMLAttr(nsGkAtoms::href, aValue, rv);
  }
  void GetTarget(nsAString& aValue);
  void SetTarget(const nsAString& aValue, mozilla::ErrorResult& rv) {
    SetHTMLAttr(nsGkAtoms::target, aValue, rv);
  }
  void GetDownload(DOMString& aValue) {
    GetHTMLAttr(nsGkAtoms::download, aValue);
  }
  void SetDownload(const nsAString& aValue, mozilla::ErrorResult& rv) {
    SetHTMLAttr(nsGkAtoms::download, aValue, rv);
  }
  void GetPing(DOMString& aValue) { GetHTMLAttr(nsGkAtoms::ping, aValue); }
  void SetPing(const nsAString& aValue, mozilla::ErrorResult& rv) {
    SetHTMLAttr(nsGkAtoms::ping, aValue, rv);
  }
  void GetRel(DOMString& aValue) { GetHTMLAttr(nsGkAtoms::rel, aValue); }
  void SetRel(const nsAString& aValue, mozilla::ErrorResult& rv) {
    SetHTMLAttr(nsGkAtoms::rel, aValue, rv);
  }
  void SetReferrerPolicy(const nsAString& aValue, mozilla::ErrorResult& rv) {
    SetHTMLAttr(nsGkAtoms::referrerpolicy, aValue, rv);
  }
  void GetReferrerPolicy(DOMString& aPolicy) {
    GetEnumAttr(nsGkAtoms::referrerpolicy, EmptyCString().get(), aPolicy);
  }
  nsDOMTokenList* RelList();
  void GetHreflang(DOMString& aValue) {
    GetHTMLAttr(nsGkAtoms::hreflang, aValue);
  }
  void SetHreflang(const nsAString& aValue, mozilla::ErrorResult& rv) {
    SetHTMLAttr(nsGkAtoms::hreflang, aValue, rv);
  }
  // Needed for docshell
  void GetType(nsAString& aValue) { GetHTMLAttr(nsGkAtoms::type, aValue); }
  void GetType(DOMString& aValue) { GetHTMLAttr(nsGkAtoms::type, aValue); }
  void SetType(const nsAString& aValue, mozilla::ErrorResult& rv) {
    SetHTMLAttr(nsGkAtoms::type, aValue, rv);
  }
  void GetText(nsAString& aValue, mozilla::ErrorResult& rv);
  void SetText(const nsAString& aValue, mozilla::ErrorResult& rv);

  // Link::GetOrigin is OK for us

  // Link::GetProtocol is OK for us
  // Link::SetProtocol is OK for us

  // Link::GetUsername is OK for us
  // Link::SetUsername is OK for us

  // Link::GetPassword is OK for us
  // Link::SetPassword is OK for us

  // Link::Link::GetHost is OK for us
  // Link::Link::SetHost is OK for us

  // Link::Link::GetHostname is OK for us
  // Link::Link::SetHostname is OK for us

  // Link::Link::GetPort is OK for us
  // Link::Link::SetPort is OK for us

  // Link::Link::GetPathname is OK for us
  // Link::Link::SetPathname is OK for us

  // Link::Link::GetSearch is OK for us
  // Link::Link::SetSearch is OK for us

  // Link::Link::GetHash is OK for us
  // Link::Link::SetHash is OK for us

  void GetCoords(DOMString& aValue) { GetHTMLAttr(nsGkAtoms::coords, aValue); }
  void SetCoords(const nsAString& aValue, mozilla::ErrorResult& rv) {
    SetHTMLAttr(nsGkAtoms::coords, aValue, rv);
  }
  void GetCharset(DOMString& aValue) {
    GetHTMLAttr(nsGkAtoms::charset, aValue);
  }
  void SetCharset(const nsAString& aValue, mozilla::ErrorResult& rv) {
    SetHTMLAttr(nsGkAtoms::charset, aValue, rv);
  }
  void GetName(DOMString& aValue) { GetHTMLAttr(nsGkAtoms::name, aValue); }
  void GetName(nsAString& aValue) { GetHTMLAttr(nsGkAtoms::name, aValue); }
  void SetName(const nsAString& aValue, mozilla::ErrorResult& rv) {
    SetHTMLAttr(nsGkAtoms::name, aValue, rv);
  }
  void GetRev(DOMString& aValue) { GetHTMLAttr(nsGkAtoms::rev, aValue); }
  void SetRev(const nsAString& aValue, mozilla::ErrorResult& rv) {
    SetHTMLAttr(nsGkAtoms::rev, aValue, rv);
  }
  void GetShape(DOMString& aValue) { GetHTMLAttr(nsGkAtoms::shape, aValue); }
  void SetShape(const nsAString& aValue, mozilla::ErrorResult& rv) {
    SetHTMLAttr(nsGkAtoms::shape, aValue, rv);
  }
  void Stringify(nsAString& aResult) { GetHref(aResult); }
  void ToString(nsAString& aSource);

  void NodeInfoChanged(Document* aOldDoc) final {
    ClearHasPendingLinkUpdate();
    nsGenericHTMLElement::NodeInfoChanged(aOldDoc);
  }

  static DOMTokenListSupportedToken sSupportedRelValues[];

 protected:
  virtual ~HTMLAnchorElement();

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;
  RefPtr<nsDOMTokenList> mRelList;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_HTMLAnchorElement_h
