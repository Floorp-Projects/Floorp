/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXMLElement_h___
#define nsXMLElement_h___

#include "mozilla/Attributes.h"
#include "nsIDOMElement.h"
#include "mozilla/dom/ElementInlines.h"
#include "mozilla/dom/DOMRect.h"

class nsXMLElement : public mozilla::dom::Element,
                     public nsIDOMElement
{
public:
  nsXMLElement(already_AddRefed<nsINodeInfo>& aNodeInfo)
    : mozilla::dom::Element(aNodeInfo)
  {
  }

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsINode interface methods
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  virtual nsIDOMNode* AsDOMNode() MOZ_OVERRIDE { return this; }

protected:
  virtual JSObject* WrapNode(JSContext *aCx) MOZ_OVERRIDE;
};

#endif // nsXMLElement_h___
