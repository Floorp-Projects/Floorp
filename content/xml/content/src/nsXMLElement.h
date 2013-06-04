/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXMLElement_h___
#define nsXMLElement_h___

#include "mozilla/Attributes.h"
#include "nsIDOMElement.h"
#include "mozilla/dom/Element.h"

class nsXMLElement : public mozilla::dom::Element,
                     public nsIDOMElement
{
public:
  nsXMLElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : mozilla::dom::Element(aNodeInfo)
  {
    SetIsDOMBinding();
  }

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsINode interface methods
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  virtual nsXPCClassInfo* GetClassInfo() MOZ_OVERRIDE;

  virtual nsIDOMNode* AsDOMNode() MOZ_OVERRIDE { return this; }

  // nsIContent interface methods
  virtual nsIAtom *GetIDAttributeName() const MOZ_OVERRIDE;
  virtual nsIAtom* DoGetID() const MOZ_OVERRIDE;
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) MOZ_OVERRIDE;
  virtual void UnbindFromTree(bool aDeep, bool aNullParent) MOZ_OVERRIDE;
  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify) MOZ_OVERRIDE;
  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult) MOZ_OVERRIDE;

  // Element overrides
  virtual void NodeInfoChanged(nsINodeInfo* aOldNodeInfo) MOZ_OVERRIDE;

protected:
  virtual JSObject* WrapNode(JSContext *aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;
};

#endif // nsXMLElement_h___
