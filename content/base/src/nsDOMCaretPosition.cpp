/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMCaretPosition.h"
#include "mozilla/dom/CaretPositionBinding.h"
#include "nsContentUtils.h"

nsDOMCaretPosition::nsDOMCaretPosition(nsINode* aNode, uint32_t aOffset)
  : mOffset(aOffset), mOffsetNode(aNode)
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

JSObject*
nsDOMCaretPosition::WrapObject(JSContext *aCx, JSObject *aScope,
                              bool *aTried)
{
  return mozilla::dom::CaretPositionBinding::Wrap(aCx, aScope, this, aTried);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(nsDOMCaretPosition, mOffsetNode)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMCaretPosition)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMCaretPosition)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMCaretPosition)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

