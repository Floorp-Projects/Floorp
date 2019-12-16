/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AbstractRange_h
#define mozilla_dom_AbstractRange_h

#include "mozilla/RangeBoundary.h"
#include "mozilla/dom/Document.h"
#include "nsIContent.h"
#include "nsINode.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class AbstractRange : public nsISupports, public nsWrapperCache {
 protected:
  explicit AbstractRange(nsINode* aNode);
  virtual ~AbstractRange() = default;

 public:
  AbstractRange() = delete;
  explicit AbstractRange(const AbstractRange& aOther) = delete;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(AbstractRange)

  const RangeBoundary& StartRef() const { return mStart; }
  const RangeBoundary& EndRef() const { return mEnd; }

  nsIContent* GetChildAtStartOffset() const {
    return mStart.GetChildAtOffset();
  }
  nsIContent* GetChildAtEndOffset() const { return mEnd.GetChildAtOffset(); }
  bool IsPositioned() const { return mIsPositioned; }
  nsINode* GetCommonAncestor() const;

  // WebIDL

  // If Range is created from JS, it's initialized with Document.createRange()
  // and it collaps the range to start of the Document.  Therefore, the
  // following WebIDL methods are called only when `mIsPositioned` is true.
  // So, it does not make sense to take `ErrorResult` as their parameter
  // since its destruction cost may appear in profile.  If you create range
  // object from C++ and needs to check whether it's positioned, should call
  // `IsPositioned()` directly.

  nsINode* GetStartContainer() const { return mStart.Container(); }
  nsINode* GetEndContainer() const { return mEnd.Container(); }
  uint32_t StartOffset() const {
    // FYI: Returns 0 if it's not positioned.
    return static_cast<uint32_t>(mStart.Offset());
  }
  uint32_t EndOffset() const {
    // FYI: Returns 0 if it's not positioned.
    return static_cast<uint32_t>(mEnd.Offset());
  }
  bool Collapsed() const {
    return !mIsPositioned || (mStart.Container() == mEnd.Container() &&
                              mStart.Offset() == mEnd.Offset());
  }

  nsINode* GetParentObject() const { return mOwner; }
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

 protected:
  template <typename SPT, typename SRT, typename EPT, typename ERT,
            typename RangeType>
  static nsresult SetStartAndEndInternal(
      const RangeBoundaryBase<SPT, SRT>& aStartBoundary,
      const RangeBoundaryBase<EPT, ERT>& aEndBoundary, RangeType* aRange);

  RefPtr<Document> mOwner;
  RangeBoundary mStart;
  RangeBoundary mEnd;
  // `true` if `mStart` has a container and potentially other conditions are
  // fulfilled.
  bool mIsPositioned;

  // Used by nsRange, but this should have this for minimizing the size.
  bool mIsGenerated;
  // Used by nsRange, but this should have this for minimizing the size.
  bool mCalledByJS;
};

}  // namespace dom
}  // namespace mozilla

#endif  // #ifndef mozilla_dom_AbstractRange_h
