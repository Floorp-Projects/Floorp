/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_SVGAELEMENT_H_
#define NS_SVGAELEMENT_H_

#include "DOMSVGTests.h"
#include "Link.h"
#include "nsIDOMSVGAElement.h"
#include "nsIDOMSVGURIReference.h"
#include "nsILink.h"
#include "nsSVGGraphicElement.h"
#include "nsSVGString.h"

typedef nsSVGGraphicElement nsSVGAElementBase;

class nsSVGAElement : public nsSVGAElementBase,
                      public nsIDOMSVGAElement,
                      public DOMSVGTests,
                      public nsIDOMSVGURIReference,
                      public nsILink,
                      public mozilla::dom::Link
{
protected:
  friend nsresult NS_NewSVGAElement(nsIContent **aResult,
                                    already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGAElement(already_AddRefed<nsINodeInfo> aNodeInfo);

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGAELEMENT
  NS_DECL_NSIDOMSVGURIREFERENCE

  // XXX: I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE(nsSVGAElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGAElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGAElementBase::)

  // nsINode interface methods
  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  // nsILink
  NS_IMETHOD LinkAdded() { return NS_OK; }
  NS_IMETHOD LinkRemoved() { return NS_OK; }

  // nsIContent
  virtual nsresult BindToTree(nsIDocument *aDocument, nsIContent *aParent,
                              nsIContent *aBindingParent,
                              bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  virtual bool IsFocusable(PRInt32 *aTabIndex = nullptr, bool aWithMouse = false);
  virtual bool IsLink(nsIURI** aURI) const;
  virtual void GetLinkTarget(nsAString& aTarget);
  virtual nsLinkState GetLinkState() const;
  virtual already_AddRefed<nsIURI> GetHrefURI() const;
  virtual nsEventStates IntrinsicState() const;
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify);
  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify);

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
protected:

  virtual StringAttributesInfo GetStringInfo();

  enum { HREF, TARGET };
  nsSVGString mStringAttributes[2];
  static StringInfo sStringInfo[2];
};

#endif // NS_SVGAELEMENT_H_
