/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CrossShadowBoundaryRange_h
#define mozilla_dom_CrossShadowBoundaryRange_h

#include "mozilla/RangeBoundary.h"
#include "mozilla/RangeUtils.h"
#include "mozilla/dom/AbstractRange.h"
#include "mozilla/dom/StaticRange.h"
#include "nsTArray.h"

namespace mozilla {
class ErrorResult;

namespace dom {

class CrossShadowBoundaryRange final : public StaticRange,
                                       public nsStubMutationObserver {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_IMETHODIMP_(void) DeleteCycleCollectable(void) override;
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(
      CrossShadowBoundaryRange, StaticRange)

  CrossShadowBoundaryRange() = delete;
  explicit CrossShadowBoundaryRange(const StaticRange& aOther) = delete;

  template <typename SPT, typename SRT, typename EPT, typename ERT>
  static already_AddRefed<CrossShadowBoundaryRange> Create(
      const RangeBoundaryBase<SPT, SRT>& aStartBoundary,
      const RangeBoundaryBase<EPT, ERT>& aEndBoundary, nsRange* aOwner);

  void NotifyNodeBecomesShadowHost(nsINode* aNode) {
    if (aNode == mStart.Container()) {
      mStart.NotifyParentBecomesShadowHost();
    }

    if (aNode == mEnd.Container()) {
      mEnd.NotifyParentBecomesShadowHost();
    }
  }

  nsINode* GetCommonAncestor() const { return mCommonAncestor; }

  // CrossShadowBoundaryRange should have a very limited usage.
  nsresult SetStartAndEnd(nsINode* aStartContainer, uint32_t aStartOffset,
                          nsINode* aEndContainer, uint32_t aEndOffset) = delete;

  template <typename SPT, typename SRT, typename EPT, typename ERT>
  nsresult SetStartAndEnd(const RangeBoundaryBase<SPT, SRT>& aStartBoundary,
                          const RangeBoundaryBase<EPT, ERT>& aEndBoundary) {
    return StaticRange::SetStartAndEnd(aStartBoundary, aEndBoundary);
  }

  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

 private:
  explicit CrossShadowBoundaryRange(nsINode* aNode, nsRange* aOwner)
      : StaticRange(aNode), mOwner(aOwner) {}
  virtual ~CrossShadowBoundaryRange() = default;

  /**
   * DoSetRange() is called when `AbstractRange::SetStartAndEndInternal()` sets
   * mStart and mEnd.
   *
   * @param aStartBoundary  Computed start point.  This must equals or be before
   *                        aEndBoundary in the DOM tree order.
   * @param aEndBoundary    Computed end point.
   * @param aRootNode       The root node of aStartBoundary or aEndBoundary.
   *                        It's useless to CrossShadowBoundaryRange.
   * @param aOwner          The nsRange that owns this CrossShadowBoundaryRange.
   */
  template <typename SPT, typename SRT, typename EPT, typename ERT>
  void DoSetRange(const RangeBoundaryBase<SPT, SRT>& aStartBoundary,
                  const RangeBoundaryBase<EPT, ERT>& aEndBoundary,
                  nsINode* aRootNode, nsRange* aOwner);

  // This is either NULL if this CrossShadowBoundaryRange has been
  // reset by Release() or the closest common shadow-including ancestor
  // of mStart and mEnd.
  nsCOMPtr<nsINode> mCommonAncestor;

  static nsTArray<RefPtr<CrossShadowBoundaryRange>>* sCachedRanges;

  friend class AbstractRange;

  // nsRange owns CrossShadowBoundaryRange; it always outlives
  // CrossShadowBoundaryRange, so it's safe to use raw pointer here.
  nsRange* mOwner;
};
}  // namespace dom
}  // namespace mozilla

#endif  // #ifndef mozilla_dom_CrossShadowBoundaryRange_h
