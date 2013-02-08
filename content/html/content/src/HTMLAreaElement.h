/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set tw=80 expandtab softtabstop=2 ts=2 sw=2: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLAreaElement_h
#define mozilla_dom_HTMLAreaElement_h

#include "nsIDOMHTMLAreaElement.h"
#include "nsGenericHTMLElement.h"
#include "nsILink.h"
#include "nsGkAtoms.h"
#include "nsIURL.h"
#include "nsIDocument.h"
#include "Link.h"

namespace mozilla {
namespace dom {

class HTMLAreaElement : public nsGenericHTMLElement,
                        public nsIDOMHTMLAreaElement,
                        public nsILink,
                        public Link
{
public:
  HTMLAreaElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~HTMLAreaElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // DOM memory reporter participant
  NS_DECL_SIZEOF_EXCLUDING_THIS

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_TO_GENERIC

  virtual int32_t TabIndexDefault() MOZ_OVERRIDE;

  // nsIDOMHTMLAreaElement
  NS_DECL_NSIDOMHTMLAREAELEMENT

  // nsILink
  NS_IMETHOD LinkAdded() { return NS_OK; }
  NS_IMETHOD LinkRemoved() { return NS_OK; }

  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);
  virtual bool IsLink(nsIURI** aURI) const;
  virtual void GetLinkTarget(nsAString& aTarget);
  virtual nsLinkState GetLinkState() const;
  virtual already_AddRefed<nsIURI> GetHrefURI() const;

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);
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

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsEventStates IntrinsicState() const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }

protected:
  virtual void GetItemValueText(nsAString& text);
  virtual void SetItemValueText(const nsAString& text);
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_HTMLAreaElement_h */
