/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLAreaElement_h
#define mozilla_dom_HTMLAreaElement_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/Link.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"

namespace mozilla {
class EventChainPostVisitor;
class EventChainPreVisitor;
namespace dom {

class HTMLAreaElement final : public nsGenericHTMLElement, public Link {
 public:
  explicit HTMLAreaElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // CC
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLAreaElement,
                                           nsGenericHTMLElement)

  NS_DECL_ADDSIZEOFEXCLUDINGTHIS

  NS_IMPL_FROMNODE_HTML_WITH_TAG(HTMLAreaElement, area)

  virtual int32_t TabIndexDefault() override;

  void GetEventTargetParent(EventChainPreVisitor& aVisitor) override;
  MOZ_CAN_RUN_SCRIPT
  nsresult PostHandleEvent(EventChainPostVisitor& aVisitor) override;
  virtual bool IsLink(nsIURI** aURI) const override;
  virtual void GetLinkTarget(nsAString& aTarget) override;
  virtual already_AddRefed<nsIURI> GetHrefURI() const override;

  virtual nsresult BindToTree(BindContext&, nsINode& aParent) override;
  virtual void UnbindFromTree(bool aNullParent = true) override;

  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  virtual EventStates IntrinsicState() const override;

  // WebIDL
  void GetAlt(DOMString& aValue) { GetHTMLAttr(nsGkAtoms::alt, aValue); }
  void SetAlt(const nsAString& aAlt, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::alt, aAlt, aError);
  }

  void GetCoords(DOMString& aValue) { GetHTMLAttr(nsGkAtoms::coords, aValue); }
  void SetCoords(const nsAString& aCoords, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::coords, aCoords, aError);
  }

  // argument type nsAString for HTMLImageMapAccessible
  void GetShape(nsAString& aValue) { GetHTMLAttr(nsGkAtoms::shape, aValue); }
  void SetShape(const nsAString& aShape, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::shape, aShape, aError);
  }

  // argument type nsAString for nsContextMenuInfo
  void GetHref(nsAString& aValue) {
    GetURIAttr(nsGkAtoms::href, nullptr, aValue);
  }
  void SetHref(const nsAString& aHref, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::href, aHref, aError);
  }

  void GetTarget(DOMString& aValue);
  void SetTarget(const nsAString& aTarget, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::target, aTarget, aError);
  }

  void GetDownload(DOMString& aValue) {
    GetHTMLAttr(nsGkAtoms::download, aValue);
  }
  void SetDownload(const nsAString& aDownload, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::download, aDownload, aError);
  }

  void GetPing(DOMString& aValue) { GetHTMLAttr(nsGkAtoms::ping, aValue); }

  void SetPing(const nsAString& aPing, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::ping, aPing, aError);
  }

  void GetRel(DOMString& aValue) { GetHTMLAttr(nsGkAtoms::rel, aValue); }

  void SetRel(const nsAString& aRel, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::rel, aRel, aError);
  }
  nsDOMTokenList* RelList();

  void SetReferrerPolicy(const nsAString& aValue, mozilla::ErrorResult& rv) {
    SetHTMLAttr(nsGkAtoms::referrerpolicy, aValue, rv);
  }
  void GetReferrerPolicy(nsAString& aReferrer) {
    GetEnumAttr(nsGkAtoms::referrerpolicy, "", aReferrer);
  }

  // The Link::GetOrigin is OK for us

  // Link::Link::GetProtocol is OK for us
  // Link::Link::SetProtocol is OK for us

  // The Link::GetUsername is OK for us
  // The Link::SetUsername is OK for us

  // The Link::GetPassword is OK for us
  // The Link::SetPassword is OK for us

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

  // The Link::GetSearchParams is OK for us

  bool NoHref() const { return GetBoolAttr(nsGkAtoms::nohref); }

  void SetNoHref(bool aValue, ErrorResult& aError) {
    SetHTMLBoolAttr(nsGkAtoms::nohref, aValue, aError);
  }

  void ToString(nsAString& aSource);
  void Stringify(nsAString& aResult) { GetHref(aResult); }

  void NodeInfoChanged(Document* aOldDoc) final {
    ClearHasPendingLinkUpdate();
    nsGenericHTMLElement::NodeInfoChanged(aOldDoc);
  }

 protected:
  virtual ~HTMLAreaElement();

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aSubjectPrincipal,
                                bool aNotify) override;

  RefPtr<nsDOMTokenList> mRelList;
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_HTMLAreaElement_h */
