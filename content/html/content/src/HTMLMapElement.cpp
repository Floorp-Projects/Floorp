/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLMapElement.h"
#include "mozilla/dom/HTMLMapElementBinding.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsContentList.h"
#include "nsCOMPtr.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Map)

namespace mozilla {
namespace dom {

HTMLMapElement::HTMLMapElement(already_AddRefed<nsINodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLMapElement)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLMapElement,
                                                  nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAreas)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(HTMLMapElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLMapElement, Element)


// QueryInterface implementation for HTMLMapElement
NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(HTMLMapElement)
  NS_INTERFACE_TABLE_INHERITED(HTMLMapElement, nsIDOMHTMLMapElement)
NS_INTERFACE_TABLE_TAIL_INHERITING(nsGenericHTMLElement)

NS_IMPL_ELEMENT_CLONE(HTMLMapElement)


nsIHTMLCollection*
HTMLMapElement::Areas()
{
  if (!mAreas) {
    // Not using NS_GetContentList because this should not be cached
    mAreas = new nsContentList(this,
                               kNameSpaceID_XHTML,
                               nsGkAtoms::area,
                               nsGkAtoms::area,
                               false);
  }

  return mAreas;
}

NS_IMETHODIMP
HTMLMapElement::GetAreas(nsIDOMHTMLCollection** aAreas)
{
  NS_ENSURE_ARG_POINTER(aAreas);
  NS_ADDREF(*aAreas = Areas());
  return NS_OK;
}


NS_IMPL_STRING_ATTR(HTMLMapElement, Name, name)


JSObject*
HTMLMapElement::WrapNode(JSContext* aCx)
{
  return HTMLMapElementBinding::Wrap(aCx, this);
}

} // namespace dom
} // namespace mozilla
