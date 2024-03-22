/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScriptableContentIterator.h"

#include "mozilla/ContentIterator.h"
#include "nsINode.h"
#include "nsRange.h"

namespace mozilla {

NS_IMPL_CYCLE_COLLECTING_ADDREF(ScriptableContentIterator)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ScriptableContentIterator)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ScriptableContentIterator)
  NS_INTERFACE_MAP_ENTRY(nsIScriptableContentIterator)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(ScriptableContentIterator)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ScriptableContentIterator)
  if (tmp->mContentIterator) {
    switch (tmp->mIteratorType) {
      case POST_ORDER_ITERATOR:
      default:
        ImplCycleCollectionUnlink(
            static_cast<PostContentIterator&>(*tmp->mContentIterator));
        break;
      case PRE_ORDER_ITERATOR:
        ImplCycleCollectionUnlink(
            static_cast<PreContentIterator&>(*tmp->mContentIterator));
        break;
      case SUBTREE_ITERATOR:
        ImplCycleCollectionUnlink(
            static_cast<ContentSubtreeIterator&>(*tmp->mContentIterator));
        break;
    }
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ScriptableContentIterator)
  if (tmp->mContentIterator) {
    switch (tmp->mIteratorType) {
      case POST_ORDER_ITERATOR:
      default:
        ImplCycleCollectionTraverse(
            cb, static_cast<PostContentIterator&>(*tmp->mContentIterator),
            "mContentIterator");
        break;
      case PRE_ORDER_ITERATOR:
        ImplCycleCollectionTraverse(
            cb, static_cast<PreContentIterator&>(*tmp->mContentIterator),
            "mContentIterator");
        break;
      case SUBTREE_ITERATOR:
        ImplCycleCollectionTraverse(
            cb, static_cast<ContentSubtreeIterator&>(*tmp->mContentIterator),
            "mContentIterator");
        break;
    }
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

ScriptableContentIterator::ScriptableContentIterator()
    : mIteratorType(NOT_INITIALIZED) {}

void ScriptableContentIterator::EnsureContentIterator() {
  if (mContentIterator) {
    return;
  }
  switch (mIteratorType) {
    case POST_ORDER_ITERATOR:
    default:
      mContentIterator = MakeUnique<PostContentIterator>();
      break;
    case PRE_ORDER_ITERATOR:
      mContentIterator = MakeUnique<PreContentIterator>();
      break;
    case SUBTREE_ITERATOR:
      mContentIterator = MakeUnique<ContentSubtreeIterator>();
      break;
  }
}

NS_IMETHODIMP
ScriptableContentIterator::InitWithRootNode(IteratorType aType,
                                            nsINode* aRoot) {
  if (aType == NOT_INITIALIZED ||
      (mIteratorType != NOT_INITIALIZED && aType != mIteratorType)) {
    return NS_ERROR_INVALID_ARG;
  }
  mIteratorType = aType;
  EnsureContentIterator();
  return mContentIterator->Init(aRoot);
}

NS_IMETHODIMP
ScriptableContentIterator::InitWithRange(IteratorType aType, nsRange* aRange) {
  if (aType == NOT_INITIALIZED ||
      (mIteratorType != NOT_INITIALIZED && aType != mIteratorType)) {
    return NS_ERROR_INVALID_ARG;
  }
  mIteratorType = aType;
  EnsureContentIterator();
  return mContentIterator->Init(aRange);
}

NS_IMETHODIMP
ScriptableContentIterator::InitWithRangeAllowCrossShadowBoundary(
    IteratorType aType, nsRange* aRange) {
  if (aType == NOT_INITIALIZED ||
      (mIteratorType != NOT_INITIALIZED && aType != mIteratorType) ||
      aType != SUBTREE_ITERATOR) {
    return NS_ERROR_INVALID_ARG;
  }

  mIteratorType = aType;
  MOZ_ASSERT(mIteratorType == SUBTREE_ITERATOR);
  EnsureContentIterator();
  return static_cast<ContentSubtreeIterator*>(mContentIterator.get())
      ->InitWithAllowCrossShadowBoundary(aRange);
}

NS_IMETHODIMP
ScriptableContentIterator::InitWithPositions(IteratorType aType,
                                             nsINode* aStartContainer,
                                             uint32_t aStartOffset,
                                             nsINode* aEndContainer,
                                             uint32_t aEndOffset) {
  if (aType == NOT_INITIALIZED ||
      (mIteratorType != NOT_INITIALIZED && aType != mIteratorType)) {
    return NS_ERROR_INVALID_ARG;
  }
  mIteratorType = aType;
  EnsureContentIterator();
  return mContentIterator->Init(aStartContainer, aStartOffset, aEndContainer,
                                aEndOffset);
}

NS_IMETHODIMP
ScriptableContentIterator::First() {
  if (!mContentIterator) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  mContentIterator->First();
  return NS_OK;
}

NS_IMETHODIMP
ScriptableContentIterator::Last() {
  if (!mContentIterator) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  mContentIterator->Last();
  return NS_OK;
}

NS_IMETHODIMP
ScriptableContentIterator::Next() {
  if (!mContentIterator) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  mContentIterator->Next();
  return NS_OK;
}

NS_IMETHODIMP
ScriptableContentIterator::Prev() {
  if (!mContentIterator) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  mContentIterator->Prev();
  return NS_OK;
}

NS_IMETHODIMP
ScriptableContentIterator::GetCurrentNode(nsINode** aNode) {
  if (!mContentIterator) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  NS_IF_ADDREF(*aNode = mContentIterator->GetCurrentNode());
  return NS_OK;
}

NS_IMETHODIMP
ScriptableContentIterator::GetIsDone(bool* aIsDone) {
  if (!mContentIterator) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  *aIsDone = mContentIterator->IsDone();
  return NS_OK;
}

NS_IMETHODIMP
ScriptableContentIterator::PositionAt(nsINode* aNode) {
  if (!mContentIterator) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  return mContentIterator->PositionAt(aNode);
}

}  // namespace mozilla
