/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXMLElement_h___
#define nsXMLElement_h___

#include "mozilla/Attributes.h"
#include "nsIDOMElement.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/DOMRect.h"

class nsXMLElement : public mozilla::dom::Element,
                     public nsIDOMElement
{
public:
  explicit nsXMLElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
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
  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                         bool aPreallocateChildren) const override;

  virtual nsIDOMNode* AsDOMNode() override { return this; }

  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) override;

protected:
  virtual ~nsXMLElement() {}

  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;
};

#endif // nsXMLElement_h___
