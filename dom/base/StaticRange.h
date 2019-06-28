/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StaticRange_h
#define mozilla_dom_StaticRange_h

#include "mozilla/ErrorResult.h"
#include "mozilla/RangeBoundary.h"
#include "mozilla/dom/AbstractRange.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class StaticRange final : public AbstractRange {
 public:
  explicit StaticRange(nsINode* aNode) : AbstractRange(aNode) {}
  StaticRange() = delete;
  explicit StaticRange(const StaticRange& aOther) = delete;

  /**
   * Create() may return `StaticRange` instance which is initialized with
   * given range or points.  If it fails initializing new range with the
   * arguments, returns `nullptr`.  `ErrorResult` is set to an error only
   * when this returns `nullptr`.  The error code indicates the reason why
   * it couldn't initialize the instance.
   */
  static already_AddRefed<StaticRange> Create(
      const AbstractRange* aAbstractRange, ErrorResult& aRv) {
    MOZ_ASSERT(aAbstractRange);
    return StaticRange::Create(aAbstractRange->StartRef(),
                               aAbstractRange->EndRef(), aRv);
  }
  static already_AddRefed<StaticRange> Create(nsINode* aStartContainer,
                                              uint32_t aStartOffset,
                                              nsINode* aEndContainer,
                                              uint32_t aEndOffset,
                                              ErrorResult& aRv) {
    return StaticRange::Create(RawRangeBoundary(aStartContainer, aStartOffset),
                               RawRangeBoundary(aEndContainer, aEndOffset),
                               aRv);
  }
  template <typename SPT, typename SRT, typename EPT, typename ERT>
  static already_AddRefed<StaticRange> Create(
      const RangeBoundaryBase<SPT, SRT>& aStartBoundary,
      const RangeBoundaryBase<EPT, ERT>& aEndBoundary, ErrorResult& aRv);

 protected:
  virtual ~StaticRange() = default;

 public:
  NS_DECL_ISUPPORTS_INHERITED
  // Need to override this for calling `DoSetRange()` from
  // `NS_IMPL_MAIN_THREAD_ONLY_CYCLE_COLLECTING_RELEASE_WITH_LAST_RELEASE()`.
  NS_IMETHODIMP_(void) DeleteCycleCollectable(void) override;
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(StaticRange,
                                                         AbstractRange)

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;

  /**
   * SetStartAndEnd() works similar to call both SetStart() and SetEnd().
   * Different from calls them separately, this does nothing if either
   * the start point or the end point is invalid point.
   * If the specified start point is after the end point, the range will be
   * collapsed at the end point.  Similarly, if they are in different root,
   * the range will be collapsed at the end point.
   */
  nsresult SetStartAndEnd(nsINode* aStartContainer, uint32_t aStartOffset,
                          nsINode* aEndContainer, uint32_t aEndOffset) {
    return SetStartAndEnd(RawRangeBoundary(aStartContainer, aStartOffset),
                          RawRangeBoundary(aEndContainer, aEndOffset));
  }
  template <typename SPT, typename SRT, typename EPT, typename ERT>
  nsresult SetStartAndEnd(const RangeBoundaryBase<SPT, SRT>& aStartBoundary,
                          const RangeBoundaryBase<EPT, ERT>& aEndBoundary) {
    return AbstractRange::SetStartAndEndInternal(aStartBoundary, aEndBoundary,
                                                 this);
  }

 protected:
  /**
   * DoSetRange() is called when `AbstractRange::SetStartAndEndInternal()` sets
   * mStart and mEnd.
   *
   * @param aStartBoundary  Computed start point.  This must equals or be before
   *                        aEndBoundary in the DOM tree order.
   * @param aEndBoundary    Computed end point.
   * @param aRootNode       The root node.
   */
  template <typename SPT, typename SRT, typename EPT, typename ERT>
  void DoSetRange(const RangeBoundaryBase<SPT, SRT>& aStartBoundary,
                  const RangeBoundaryBase<EPT, ERT>& aEndBoundary,
                  nsINode* aRootNode);

  friend class AbstractRange;
};

}  // namespace dom
}  // namespace mozilla

#endif  // #ifndef mozilla_dom_StaticRange_h
