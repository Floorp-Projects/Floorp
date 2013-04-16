/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMCaretPosition.h"
#include "mozilla/dom/CaretPositionBinding.h"
#include "nsContentUtils.h"

nsDOMCaretPosition::nsDOMCaretPosition(nsINode* aNode, uint32_t aOffset)
  : mOffset(aOffset), mOffsetNode(aNode), mAnonymousContentNode(nullptr)
{
  SetIsDOMBinding();
}

nsDOMCaretPosition::~nsDOMCaretPosition()
{
}

nsINode* nsDOMCaretPosition::GetOffsetNode() const
{
  return mOffsetNode;
}

already_AddRefed<nsClientRect>
nsDOMCaretPosition::GetClientRect() const
{
  if (!mOffsetNode) {
    return nullptr;
  }

  nsRefPtr<nsClientRect> rect;
  nsRefPtr<nsRange> domRange;
  nsCOMPtr<nsINode> node;

  if (mAnonymousContentNode) {
    node = mAnonymousContentNode;
  } else {
    node = mOffsetNode;
  }

  nsresult creationRv = nsRange::CreateRange(node, mOffset, node,
                                             mOffset,
                                             getter_AddRefs<nsRange>(domRange));
  if (!NS_SUCCEEDED(creationRv)) {
    return nullptr;
  }

  NS_ASSERTION(domRange, "unable to retrieve valid dom range from CaretPosition");

  rect = domRange->GetBoundingClientRect();

  return rect.forget();
}

JSObject*
nsDOMCaretPosition::WrapObject(JSContext *aCx, JSObject *aScope)
{
  return mozilla::dom::CaretPositionBinding::Wrap(aCx, aScope, this);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(nsDOMCaretPosition, mOffsetNode)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMCaretPosition)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMCaretPosition)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMCaretPosition)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

