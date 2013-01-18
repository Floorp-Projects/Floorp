/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsIDOMHTMLMapElement.h"
#include "nsIDOMEventTarget.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsContentList.h"
#include "nsCOMPtr.h"

using namespace mozilla::dom;

class nsHTMLMapElement : public nsGenericHTMLElement,
                         public nsIDOMHTMLMapElement
{
public:
  nsHTMLMapElement(already_AddRefed<nsINodeInfo> aNodeInfo);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_TO_GENERIC

  // nsIDOMHTMLMapElement
  NS_DECL_NSIDOMHTMLMAPELEMENT

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_NO_UNLINK(nsHTMLMapElement,
                                                     nsGenericHTMLElement)

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
protected:
  nsRefPtr<nsContentList> mAreas;
};


NS_IMPL_NS_NEW_HTML_ELEMENT(Map)


nsHTMLMapElement::nsHTMLMapElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsHTMLMapElement,
                                                  nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAreas)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(nsHTMLMapElement, Element)
NS_IMPL_RELEASE_INHERITED(nsHTMLMapElement, Element)


DOMCI_NODE_DATA(HTMLMapElement, nsHTMLMapElement)

// QueryInterface implementation for nsHTMLMapElement
NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(nsHTMLMapElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(nsHTMLMapElement, nsIDOMHTMLMapElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLMapElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLMapElement)

NS_IMPL_ELEMENT_CLONE(nsHTMLMapElement)


NS_IMETHODIMP
nsHTMLMapElement::GetAreas(nsIDOMHTMLCollection** aAreas)
{
  NS_ENSURE_ARG_POINTER(aAreas);

  if (!mAreas) {
    // Not using NS_GetContentList because this should not be cached
    mAreas = new nsContentList(this,
                               kNameSpaceID_XHTML,
                               nsGkAtoms::area,
                               nsGkAtoms::area,
                               false);
  }

  NS_ADDREF(*aAreas = mAreas);
  return NS_OK;
}


NS_IMPL_STRING_ATTR(nsHTMLMapElement, Name, name)
