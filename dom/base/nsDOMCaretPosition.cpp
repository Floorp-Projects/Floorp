/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMCaretPosition.h"

#include "mozilla/dom/CaretPositionBinding.h"
#include "mozilla/dom/DOMRect.h"
#include "mozilla/ErrorResult.h"
#include "nsRange.h"

using namespace mozilla::dom;

nsDOMCaretPosition::nsDOMCaretPosition(nsINode* aNode, uint32_t aOffset)
    : mOffset(aOffset), mOffsetNode(aNode), mAnonymousContentNode(nullptr) {}

nsDOMCaretPosition::~nsDOMCaretPosition() = default;

nsINode* nsDOMCaretPosition::GetOffsetNode() const { return mOffsetNode; }

already_AddRefed<DOMRect> nsDOMCaretPosition::GetClientRect() const {
  if (!mOffsetNode) {
    return nullptr;
  }

  nsCOMPtr<nsINode> node;
  if (mAnonymousContentNode) {
    node = mAnonymousContentNode;
  } else {
    node = mOffsetNode;
  }

  RefPtr<nsRange> range =
      nsRange::Create(node, mOffset, node, mOffset, mozilla::IgnoreErrors());
  if (!range) {
    return nullptr;
  }
  RefPtr<DOMRect> rect = range->GetBoundingClientRect(false);
  return rect.forget();
}

JSObject* nsDOMCaretPosition::WrapObject(JSContext* aCx,
                                         JS::Handle<JSObject*> aGivenProto) {
  return mozilla::dom::CaretPosition_Binding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsDOMCaretPosition, mOffsetNode,
                                      mAnonymousContentNode)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMCaretPosition)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMCaretPosition)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMCaretPosition)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END
