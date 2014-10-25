/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set tw=80 expandtab softtabstop=2 ts=2 sw=2: */
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

class HTMLAreaElement MOZ_FINAL : public nsGenericHTMLElement,
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

  virtual int32_t TabIndexDefault() MOZ_OVERRIDE;

  // nsIDOMHTMLAreaElement
  NS_DECL_NSIDOMHTMLAREAELEMENT

  virtual nsresult PreHandleEvent(EventChainPreVisitor& aVisitor) MOZ_OVERRIDE;
  virtual nsresult PostHandleEvent(EventChainPostVisitor& aVisitor) MOZ_OVERRIDE;
  virtual bool IsLink(nsIURI** aURI) const MOZ_OVERRIDE;
  virtual void GetLinkTarget(nsAString& aTarget) MOZ_OVERRIDE;
  virtual already_AddRefed<nsIURI> GetHrefURI() const MOZ_OVERRIDE;

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) MOZ_OVERRIDE;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) MOZ_OVERRIDE;
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

  virtual nsresult Clone(mozilla::dom::NodeInfo* aNodeInfo, nsINode** aResult) const MOZ_OVERRIDE;

  virtual EventStates IntrinsicState() const MOZ_OVERRIDE;

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

  void GetHref(nsAString& aHref, ErrorResult& aError)
  {
    aError = GetHref(aHref);
  }
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
  
  void GetRel(nsString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::rel, aValue);
  }

  void SetRel(const nsAString& aRel, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::rel, aRel, aError);
  } 
  nsDOMTokenList* RelList();

  // The Link::GetOrigin is OK for us

  using Link::GetProtocol;
  using Link::SetProtocol;

  // The Link::GetUsername is OK for us
  // The Link::SetUsername is OK for us

  // The Link::GetPassword is OK for us
  // The Link::SetPassword is OK for us

  using Link::GetHost;
  using Link::SetHost;

  using Link::GetHostname;
  using Link::SetHostname;

  using Link::GetPort;
  using Link::SetPort;

  using Link::GetPathname;
  using Link::SetPathname;

  using Link::GetSearch;
  using Link::SetSearch;

  using Link::GetHash;
  using Link::SetHash;

  // The Link::GetSearchParams is OK for us
  // The Link::SetSearchParams is OK for us

  bool NoHref() const
  {
    return GetBoolAttr(nsGkAtoms::nohref);
  }

  void SetNoHref(bool aValue, ErrorResult& aError)
  {
    SetHTMLBoolAttr(nsGkAtoms::nohref, aValue, aError);
  }

  void Stringify(nsAString& aResult, ErrorResult& aError)
  {
    GetHref(aResult, aError);
  }

protected:
  virtual ~HTMLAreaElement();

  virtual JSObject* WrapNode(JSContext* aCx) MOZ_OVERRIDE;

  virtual void GetItemValueText(nsAString& text) MOZ_OVERRIDE;
  virtual void SetItemValueText(const nsAString& text) MOZ_OVERRIDE;
  nsRefPtr<nsDOMTokenList > mRelList;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_HTMLAreaElement_h */
