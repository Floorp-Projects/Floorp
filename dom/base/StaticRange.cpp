/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/StaticRange.h"
#include "mozilla/dom/StaticRangeBinding.h"
#include "nsINode.h"

namespace mozilla::dom {

template already_AddRefed<StaticRange> StaticRange::Create(
    const RangeBoundary& aStartBoundary, const RangeBoundary& aEndBoundary,
    ErrorResult& aRv);
template already_AddRefed<StaticRange> StaticRange::Create(
    const RangeBoundary& aStartBoundary, const RawRangeBoundary& aEndBoundary,
    ErrorResult& aRv);
template already_AddRefed<StaticRange> StaticRange::Create(
    const RawRangeBoundary& aStartBoundary, const RangeBoundary& aEndBoundary,
    ErrorResult& aRv);
template already_AddRefed<StaticRange> StaticRange::Create(
    const RawRangeBoundary& aStartBoundary,
    const RawRangeBoundary& aEndBoundary, ErrorResult& aRv);
template nsresult StaticRange::SetStartAndEnd(
    const RangeBoundary& aStartBoundary, const RangeBoundary& aEndBoundary);
template nsresult StaticRange::SetStartAndEnd(
    const RangeBoundary& aStartBoundary, const RawRangeBoundary& aEndBoundary);
template nsresult StaticRange::SetStartAndEnd(
    const RawRangeBoundary& aStartBoundary, const RangeBoundary& aEndBoundary);
template nsresult StaticRange::SetStartAndEnd(
    const RawRangeBoundary& aStartBoundary,
    const RawRangeBoundary& aEndBoundary);
template void StaticRange::DoSetRange(const RangeBoundary& aStartBoundary,
                                      const RangeBoundary& aEndBoundary,
                                      nsINode* aRootNode);
template void StaticRange::DoSetRange(const RangeBoundary& aStartBoundary,
                                      const RawRangeBoundary& aEndBoundary,
                                      nsINode* aRootNode);
template void StaticRange::DoSetRange(const RawRangeBoundary& aStartBoundary,
                                      const RangeBoundary& aEndBoundary,
                                      nsINode* aRootNode);
template void StaticRange::DoSetRange(const RawRangeBoundary& aStartBoundary,
                                      const RawRangeBoundary& aEndBoundary,
                                      nsINode* aRootNode);

nsTArray<RefPtr<StaticRange>>* StaticRange::sCachedRanges = nullptr;

NS_IMPL_CYCLE_COLLECTING_ADDREF(StaticRange)
NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_INTERRUPTABLE_LAST_RELEASE(
    StaticRange, DoSetRange(RawRangeBoundary(), RawRangeBoundary(), nullptr),
    AbstractRange::MaybeCacheToReuse(*this))

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(StaticRange)
NS_INTERFACE_MAP_END_INHERITING(AbstractRange)

NS_IMPL_CYCLE_COLLECTION_CLASS(StaticRange)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(StaticRange, AbstractRange)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mStart)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mEnd)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(StaticRange, AbstractRange)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(StaticRange, AbstractRange)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

// static
already_AddRefed<StaticRange> StaticRange::Create(nsINode* aNode) {
  MOZ_ASSERT(aNode);
  if (!sCachedRanges || sCachedRanges->IsEmpty()) {
    return do_AddRef(new StaticRange(aNode));
  }
  RefPtr<StaticRange> staticRange = sCachedRanges->PopLastElement().forget();
  staticRange->Init(aNode);
  return staticRange.forget();
}

// static
template <typename SPT, typename SRT, typename EPT, typename ERT>
already_AddRefed<StaticRange> StaticRange::Create(
    const RangeBoundaryBase<SPT, SRT>& aStartBoundary,
    const RangeBoundaryBase<EPT, ERT>& aEndBoundary, ErrorResult& aRv) {
  RefPtr<StaticRange> staticRange =
      StaticRange::Create(aStartBoundary.Container());
  staticRange->DoSetRange(aStartBoundary, aEndBoundary, nullptr);

  return staticRange.forget();
}

StaticRange::~StaticRange() {
  DoSetRange(RawRangeBoundary(), RawRangeBoundary(), nullptr);
}

template <typename SPT, typename SRT, typename EPT, typename ERT>
void StaticRange::DoSetRange(const RangeBoundaryBase<SPT, SRT>& aStartBoundary,
                             const RangeBoundaryBase<EPT, ERT>& aEndBoundary,
                             nsINode* aRootNode) {
  bool checkCommonAncestor =
      IsInAnySelection() && (mStart.Container() != aStartBoundary.Container() ||
                             mEnd.Container() != aEndBoundary.Container());
  mStart.CopyFrom(aStartBoundary, RangeBoundaryIsMutationObserved::No);
  mEnd.CopyFrom(aEndBoundary, RangeBoundaryIsMutationObserved::No);
  MOZ_ASSERT(mStart.IsSet() == mEnd.IsSet());
  mIsPositioned = mStart.IsSet() && mEnd.IsSet();

  if (checkCommonAncestor) {
    UpdateCommonAncestorIfNecessary();
  }
}

/* static */
already_AddRefed<StaticRange> StaticRange::Constructor(
    const GlobalObject& global, const StaticRangeInit& init, ErrorResult& aRv) {
  if (init.mStartContainer->NodeType() == nsINode::DOCUMENT_TYPE_NODE ||
      init.mStartContainer->NodeType() == nsINode::ATTRIBUTE_NODE ||
      init.mEndContainer->NodeType() == nsINode::DOCUMENT_TYPE_NODE ||
      init.mEndContainer->NodeType() == nsINode::ATTRIBUTE_NODE) {
    aRv.Throw(NS_ERROR_DOM_INVALID_NODE_TYPE_ERR);
    return nullptr;
  }

  return Create(init.mStartContainer, init.mStartOffset, init.mEndContainer,
                init.mEndOffset, aRv);
}

JSObject* StaticRange::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return StaticRange_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
