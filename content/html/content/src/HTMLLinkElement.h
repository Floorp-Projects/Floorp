/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLLinkElement_h
#define mozilla_dom_HTMLLinkElement_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/Link.h"
#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLLinkElement.h"
#include "nsILink.h"
#include "nsStyleLinkElement.h"

namespace mozilla {
namespace dom {

class HTMLLinkElement MOZ_FINAL : public nsGenericHTMLElement,
                                  public nsIDOMHTMLLinkElement,
                                  public nsILink,
                                  public nsStyleLinkElement,
                                  public Link
{
public:
  HTMLLinkElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~HTMLLinkElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // CC
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLLinkElement,
                                           nsGenericHTMLElement)

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_TO_GENERIC

  // nsIDOMHTMLLinkElement
  NS_DECL_NSIDOMHTMLLINKELEMENT

  // DOM memory reporter participant
  NS_DECL_SIZEOF_EXCLUDING_THIS

  // nsILink
  NS_IMETHOD    LinkAdded() MOZ_OVERRIDE;
  NS_IMETHOD    LinkRemoved() MOZ_OVERRIDE;

  // nsIDOMEventTarget
  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor) MOZ_OVERRIDE;
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor) MOZ_OVERRIDE;

  // nsINode
  virtual nsresult Clone(nsINodeInfo* aNodeInfo, nsINode** aResult) const MOZ_OVERRIDE;
  virtual nsIDOMNode* AsDOMNode() MOZ_OVERRIDE { return this; }
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  // nsIContent
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
  virtual bool IsLink(nsIURI** aURI) const MOZ_OVERRIDE;
  virtual already_AddRefed<nsIURI> GetHrefURI() const MOZ_OVERRIDE;

  // Element
  virtual bool ParseAttribute(int32_t aNamespaceID,
                              nsIAtom* aAttribute,
                              const nsAString& aValue,
                              nsAttrValue& aResult) MOZ_OVERRIDE;
  virtual void GetLinkTarget(nsAString& aTarget) MOZ_OVERRIDE;
  virtual nsEventStates IntrinsicState() const MOZ_OVERRIDE;

  void CreateAndDispatchEvent(nsIDocument* aDoc, const nsAString& aEventName);

  // WebIDL
  bool Disabled();
  void SetDisabled(bool aDisabled);
  // XPCOM GetHref is fine.
  void SetHref(const nsAString& aHref, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::href, aHref, aRv);
  }
  // XPCOM GetCrossOrigin is fine.
  void SetCrossOrigin(const nsAString& aCrossOrigin, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::crossorigin, aCrossOrigin, aRv);
  }
  // XPCOM GetRel is fine.
  void SetRel(const nsAString& aRel, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::rel, aRel, aRv);
  }
  // XPCOM GetMedia is fine.
  void SetMedia(const nsAString& aMedia, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::media, aMedia, aRv);
  }
  // XPCOM GetHreflang is fine.
  void SetHreflang(const nsAString& aHreflang, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::hreflang, aHreflang, aRv);
  }
  // XPCOM GetType is fine.
  void SetType(const nsAString& aType, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::type, aType, aRv);
  }
  // XPCOM GetCharset is fine.
  void SetCharset(const nsAString& aCharset, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::charset, aCharset, aRv);
  }
  // XPCOM GetRev is fine.
  void SetRev(const nsAString& aRev, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::rev, aRev, aRv);
  }
  // XPCOM GetTarget is fine.
  void SetTarget(const nsAString& aTarget, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::target, aTarget, aRv);
  }

protected:
  // nsStyleLinkElement
  virtual already_AddRefed<nsIURI> GetStyleSheetURL(bool* aIsInline) MOZ_OVERRIDE;
  virtual void GetStyleSheetInfo(nsAString& aTitle,
                                 nsAString& aType,
                                 nsAString& aMedia,
                                 bool* aIsScoped,
                                 bool* aIsAlternate) MOZ_OVERRIDE;
  virtual CORSMode GetCORSMode() const;
protected:
  // nsGenericHTMLElement
  virtual void GetItemValueText(nsAString& text) MOZ_OVERRIDE;
  virtual void SetItemValueText(const nsAString& text) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLLinkElement_h
