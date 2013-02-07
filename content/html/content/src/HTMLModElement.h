/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLModElement_h
#define mozilla_dom_HTMLModElement_h

#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLModElement.h"

namespace mozilla {
namespace dom {

class HTMLModElement : public nsGenericHTMLElement,
                       public nsIDOMHTMLModElement
{
public:
  HTMLModElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~HTMLModElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_TO_GENERIC

  // nsIDOMHTMLModElement
  NS_DECL_NSIDOMHTMLMODELEMENT

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLModElement_h
