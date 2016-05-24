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
#include "nsDOMTokenList.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIURL.h"

class nsIDocument;

namespace mozilla {
class EventChainPostVisitor;
class EventChainPreVisitor;
namespace dom {

class HTMLAreaElement final : public nsGenericHTMLElement,
                              public nsIDOMHTMLAreaElement,
                              public Link
{
public:
  explicit HTMLAreaElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // CC
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLAreaElement,
                                           nsGenericHTMLElement)

  // DOM memory reporter participant
  NS_DECL_SIZEOF_EXCLUDING_THIS

  virtual int32_t TabIndexDefault() override;

  // nsIDOMHTMLAreaElement
  NS_DECL_NSIDOMHTMLAREAELEMENT

  virtual nsresult PreHandleEvent(EventChainPreVisitor& aVisitor) override;
  virtual nsresult PostHandleEvent(EventChainPostVisitor& aVisitor) override;
  virtual bool IsLink(nsIURI** aURI) const override;
  virtual void GetLinkTarget(nsAString& aTarget) override;
  virtual already_AddRefed<nsIURI> GetHrefURI() const override;

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) override;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) override;
  nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify) override;
  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify) override;

  virtual nsresult Clone(mozilla::dom::NodeInfo* aNodeInfo, nsINode** aResult) const override;

  virtual EventStates IntrinsicState() const override;

  // WebIDL

  // The XPCOM GetAlt is OK for us
  void SetAlt(const nsAString& aAlt, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::alt, aAlt, aError);
  }

  // The XPCOM GetCoords is OK for us
  void SetCoords(const nsAString& aCoords, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::coords, aCoords, aError);
  }

  // The XPCOM GetShape is OK for us
  void SetShape(const nsAString& aShape, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::shape, aShape, aError);
  }

  // The XPCOM GetHref is OK for us
  void SetHref(const nsAString& aHref, ErrorResult& aError)
  {
    aError = SetHref(aHref);
  }

  // The XPCOM GetTarget is OK for us
  void SetTarget(const nsAString& aTarget, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::target, aTarget, aError);
  }

  // The XPCOM GetDownload is OK for us
  void SetDownload(const nsAString& aDownload, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::download, aDownload, aError);
  }

  // The XPCOM GetPing is OK for us
  void SetPing(const nsAString& aPing, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::ping, aPing, aError);
  }
  
  void GetRel(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::rel, aValue);
  }

  void SetRel(const nsAString& aRel, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::rel, aRel, aError);
  } 
  nsDOMTokenList* RelList();

  void SetReferrerPolicy(const nsAString& aValue, mozilla::ErrorResult& rv)
  {
    SetHTMLAttr(nsGkAtoms::referrerpolicy, aValue, rv);
  }
  void GetReferrerPolicy(nsAString& aReferrer)
  {
    GetEnumAttr(nsGkAtoms::referrerpolicy, EmptyCString().get(), aReferrer);
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

  bool NoHref() const
  {
    return GetBoolAttr(nsGkAtoms::nohref);
  }

  void SetNoHref(bool aValue, ErrorResult& aError)
  {
    SetHTMLBoolAttr(nsGkAtoms::nohref, aValue, aError);
  }

  void Stringify(nsAString& aResult)
  {
    GetHref(aResult);
  }

protected:
  virtual ~HTMLAreaElement();

  virtual JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  RefPtr<nsDOMTokenList > mRelList;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_HTMLAreaElement_h */
