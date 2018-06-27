/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

HTMLMapElement::HTMLMapElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLMapElement)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLMapElement,
                                                  nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAreas)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(HTMLMapElement,
                                               nsGenericHTMLElement)

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


JSObject*
HTMLMapElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLMapElement_Binding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
