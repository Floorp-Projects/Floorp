/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set tw=80 expandtab softtabstop=2 ts=2 sw=2: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLAnchorElement_h
#define mozilla_dom_HTMLAnchorElement_h

#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsILink.h"
#include "Link.h"

namespace mozilla {
namespace dom {

class HTMLAnchorElement : public nsGenericHTMLElement,
                          public nsIDOMHTMLAnchorElement,
                          public nsILink,
                          public Link
{
public:
  using Element::GetText;
  using Element::SetText;

  HTMLAnchorElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsGenericHTMLElement(aNodeInfo)
    , Link(this)
  {
    SetIsDOMBinding();
  }
  virtual ~HTMLAnchorElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_TO_GENERIC

  virtual int32_t TabIndexDefault() MOZ_OVERRIDE;
  virtual bool Draggable() const MOZ_OVERRIDE;

  // nsIDOMHTMLAnchorElement
  NS_DECL_NSIDOMHTMLANCHORELEMENT

  // DOM memory reporter participant
  NS_DECL_SIZEOF_EXCLUDING_THIS

  // nsILink
  NS_IMETHOD LinkAdded() { return NS_OK; }
  NS_IMETHOD LinkRemoved() { return NS_OK; }

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);
  virtual bool IsHTMLFocusable(bool aWithMouse, bool *aIsFocusable, int32_t *aTabIndex);

  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);
  virtual bool IsLink(nsIURI** aURI) const;
  virtual void GetLinkTarget(nsAString& aTarget);
  virtual nsLinkState GetLinkState() const;
  virtual already_AddRefed<nsIURI> GetHrefURI() const;

  nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify);
  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify);
  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsEventStates IntrinsicState() const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }

  virtual void OnDNSPrefetchDeferred();
  virtual void OnDNSPrefetchRequested();
  virtual bool HasDeferredDNSPrefetchRequest();

  // WebIDL API
  void GetHref(nsString& aValue)
  {
    GetHTMLURIAttr(nsGkAtoms::href, aValue);
  }
  void SetHref(const nsAString& aValue, mozilla::ErrorResult& rv)
  {
    SetHTMLAttr(nsGkAtoms::href, aValue, rv);
  }
  // The XPCOM GetTarget is OK for us
  void SetTarget(const nsAString& aValue, mozilla::ErrorResult& rv)
  {
    SetHTMLAttr(nsGkAtoms::target, aValue, rv);
  }
  void GetDownload(nsString& aValue)
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
  void GetRel(nsString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::rel, aValue);
  }
  void SetRel(const nsAString& aValue, mozilla::ErrorResult& rv)
  {
    SetHTMLAttr(nsGkAtoms::rel, aValue, rv);
  }
  void GetHreflang(nsString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::hreflang, aValue);
  }
  void SetHreflang(const nsAString& aValue, mozilla::ErrorResult& rv)
  {
    SetHTMLAttr(nsGkAtoms::hreflang, aValue, rv);
  }
  void GetType(nsString& aValue)
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
  // The XPCOM URI decomposition attributes are fine for us
  void GetCoords(nsString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::coords, aValue);
  }
  void SetCoords(const nsAString& aValue, mozilla::ErrorResult& rv)
  {
    SetHTMLAttr(nsGkAtoms::coords, aValue, rv);
  }
  void GetCharset(nsString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::charset, aValue);
  }
  void SetCharset(const nsAString& aValue, mozilla::ErrorResult& rv)
  {
    SetHTMLAttr(nsGkAtoms::charset, aValue, rv);
  }
  void GetName(nsString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::name, aValue);
  }
  void SetName(const nsAString& aValue, mozilla::ErrorResult& rv)
  {
    SetHTMLAttr(nsGkAtoms::name, aValue, rv);
  }
  void GetRev(nsString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::rev, aValue);
  }
  void SetRev(const nsAString& aValue, mozilla::ErrorResult& rv)
  {
    SetHTMLAttr(nsGkAtoms::rev, aValue, rv);
  }
  void GetShape(nsString& aValue)
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

protected:
  virtual void GetItemValueText(nsAString& text);
  virtual void SetItemValueText(const nsAString& text);
  virtual JSObject* WrapNode(JSContext *aCx, JSObject *aScope,
                             bool *aTriedToWrap) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLAnchorElement_h
