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
#include "nsIDOMHTMLAnchorElement.h"
#include "nsDOMTokenList.h"

namespace mozilla {
class EventChainPostVisitor;
class EventChainPreVisitor;
namespace dom {

class HTMLAnchorElement final : public nsGenericHTMLElement,
                                public nsIDOMHTMLAnchorElement,
                                public Link
{
public:
  using Element::GetText;
  using Element::SetText;

  explicit HTMLAnchorElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : nsGenericHTMLElement(aNodeInfo)
    , Link(this)
  {
  }

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // CC
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLAnchorElement,
                                           nsGenericHTMLElement)

  virtual int32_t TabIndexDefault() override;
  virtual bool Draggable() const override;

  // Element
  virtual bool IsInteractiveHTMLContent(bool aIgnoreTabindex) const override;

  // nsIDOMHTMLAnchorElement
  NS_DECL_NSIDOMHTMLANCHORELEMENT

  // DOM memory reporter participant
  NS_DECL_SIZEOF_EXCLUDING_THIS

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) override;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) override;
  virtual bool IsHTMLFocusable(bool aWithMouse, bool *aIsFocusable, int32_t *aTabIndex) override;

  virtual nsresult GetEventTargetParent(
                     EventChainPreVisitor& aVisitor) override;
  virtual nsresult PostHandleEvent(
                     EventChainPostVisitor& aVisitor) override;
  virtual bool IsLink(nsIURI** aURI) const override;
  virtual void GetLinkTarget(nsAString& aTarget) override;
  virtual already_AddRefed<nsIURI> GetHrefURI() const override;

  virtual nsresult BeforeSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                 const nsAttrValueOrString* aValue,
                                 bool aNotify) override;
  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                bool aNotify) override;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                         bool aPreallocateChildren) const override;

  virtual EventStates IntrinsicState() const override;

  virtual void OnDNSPrefetchDeferred() override;
  virtual void OnDNSPrefetchRequested() override;
  virtual bool HasDeferredDNSPrefetchRequest() override;

  // WebIDL API

  // The XPCOM GetHref is OK for us
  void SetHref(const nsAString& aValue, mozilla::ErrorResult& rv)
  {
    SetHTMLAttr(nsGkAtoms::href, aValue, rv);
  }
  // The XPCOM GetTarget is OK for us
  void SetTarget(const nsAString& aValue, mozilla::ErrorResult& rv)
  {
    SetHTMLAttr(nsGkAtoms::target, aValue, rv);
  }
  void GetDownload(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::download, aValue);
  }
  void SetDownload(const nsAString& aValue, mozilla::ErrorResult& rv)
  {
    SetHTMLAttr(nsGkAtoms::download, aValue, rv);
  }
  // The XPCOM GetPing is OK for us
  void SetPing(const nsAString& aValue, mozilla::ErrorResult& rv)
  {
    SetHTMLAttr(nsGkAtoms::ping, aValue, rv);
  }
  void GetRel(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::rel, aValue);
  }
  void SetRel(const nsAString& aValue, mozilla::ErrorResult& rv)
  {
    SetHTMLAttr(nsGkAtoms::rel, aValue, rv);
  }
  void SetReferrerPolicy(const nsAString& aValue, mozilla::ErrorResult& rv)
  {
    SetHTMLAttr(nsGkAtoms::referrerpolicy, aValue, rv);
  }
  void GetReferrerPolicy(nsAString& aReferrer)
  {
    GetEnumAttr(nsGkAtoms::referrerpolicy, EmptyCString().get(), aReferrer);
  }
  nsDOMTokenList* RelList();
  void GetHreflang(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::hreflang, aValue);
  }
  void SetHreflang(const nsAString& aValue, mozilla::ErrorResult& rv)
  {
    SetHTMLAttr(nsGkAtoms::hreflang, aValue, rv);
  }
  void GetType(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::type, aValue);
  }
  void SetType(const nsAString& aValue, mozilla::ErrorResult& rv)
  {
    SetHTMLAttr(nsGkAtoms::type, aValue, rv);
  }
  // The XPCOM GetText is OK for us
  void SetText(const nsAString& aValue, mozilla::ErrorResult& rv)
  {
    rv = SetText(aValue);
  }

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

  // The XPCOM URI decomposition attributes are fine for us
  void GetCoords(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::coords, aValue);
  }
  void SetCoords(const nsAString& aValue, mozilla::ErrorResult& rv)
  {
    SetHTMLAttr(nsGkAtoms::coords, aValue, rv);
  }
  void GetCharset(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::charset, aValue);
  }
  void SetCharset(const nsAString& aValue, mozilla::ErrorResult& rv)
  {
    SetHTMLAttr(nsGkAtoms::charset, aValue, rv);
  }
  void GetName(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::name, aValue);
  }
  void SetName(const nsAString& aValue, mozilla::ErrorResult& rv)
  {
    SetHTMLAttr(nsGkAtoms::name, aValue, rv);
  }
  void GetRev(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::rev, aValue);
  }
  void SetRev(const nsAString& aValue, mozilla::ErrorResult& rv)
  {
    SetHTMLAttr(nsGkAtoms::rev, aValue, rv);
  }
  void GetShape(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::shape, aValue);
  }
  void SetShape(const nsAString& aValue, mozilla::ErrorResult& rv)
  {
    SetHTMLAttr(nsGkAtoms::shape, aValue, rv);
  }
  void Stringify(nsAString& aResult)
  {
    GetHref(aResult);
  }

  virtual void NodeInfoChanged(nsIDocument* aOldDoc) final override
  {
    ClearHasPendingLinkUpdate();
    nsGenericHTMLElement::NodeInfoChanged(aOldDoc);
  }

  static DOMTokenListSupportedToken sSupportedRelValues[];

protected:
  virtual ~HTMLAnchorElement();

  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;
  RefPtr<nsDOMTokenList > mRelList;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLAnchorElement_h
