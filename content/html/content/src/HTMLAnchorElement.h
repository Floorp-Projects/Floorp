/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set tw=80 expandtab softtabstop=2 ts=2 sw=2: */
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

class HTMLAnchorElement MOZ_FINAL : public nsGenericHTMLElement,
                                    public nsIDOMHTMLAnchorElement,
                                    public Link
{
public:
  using Element::GetText;
  using Element::SetText;

  HTMLAnchorElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : nsGenericHTMLElement(aNodeInfo)
    , Link(MOZ_THIS_IN_INITIALIZER_LIST())
  {
  }
  virtual ~HTMLAnchorElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // CC
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLAnchorElement,
                                           nsGenericHTMLElement)

  virtual int32_t TabIndexDefault() MOZ_OVERRIDE;
  virtual bool Draggable() const MOZ_OVERRIDE;

  // nsIDOMHTMLAnchorElement
  NS_DECL_NSIDOMHTMLANCHORELEMENT

  // DOM memory reporter participant
  NS_DECL_SIZEOF_EXCLUDING_THIS

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) MOZ_OVERRIDE;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) MOZ_OVERRIDE;
  virtual bool IsHTMLFocusable(bool aWithMouse, bool *aIsFocusable, int32_t *aTabIndex) MOZ_OVERRIDE;

  virtual nsresult PreHandleEvent(EventChainPreVisitor& aVisitor) MOZ_OVERRIDE;
  virtual nsresult PostHandleEvent(
                     EventChainPostVisitor& aVisitor) MOZ_OVERRIDE;
  virtual bool IsLink(nsIURI** aURI) const MOZ_OVERRIDE;
  virtual void GetLinkTarget(nsAString& aTarget) MOZ_OVERRIDE;
  virtual already_AddRefed<nsIURI> GetHrefURI() const MOZ_OVERRIDE;

  nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify) MOZ_OVERRIDE;
  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify) MOZ_OVERRIDE;
  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult) MOZ_OVERRIDE;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  virtual EventStates IntrinsicState() const MOZ_OVERRIDE;

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
  nsDOMTokenList* RelList();
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

  // Link::GetOrigin is OK for us

  // Link::GetUsername is OK for us
  // Link::SetUsername is OK for us

  // Link::Getpassword is OK for us
  // Link::Setpassword is OK for us

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
  virtual void GetItemValueText(nsAString& text) MOZ_OVERRIDE;
  virtual void SetItemValueText(const nsAString& text) MOZ_OVERRIDE;
  virtual JSObject* WrapNode(JSContext *aCx) MOZ_OVERRIDE;
  nsRefPtr<nsDOMTokenList > mRelList;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLAnchorElement_h
