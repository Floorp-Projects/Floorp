/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AbstractRange_h
#define mozilla_dom_AbstractRange_h

#include <cstdint>
#include <ostream>
#include "ErrorList.h"
#include "js/RootingAPI.h"
#include "mozilla/Assertions.h"
#include "mozilla/Maybe.h"
#include "mozilla/RangeBoundary.h"
#include "mozilla/RefPtr.h"
#include "mozilla/WeakPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

class JSObject;
class nsIContent;
class nsINode;
class nsRange;
struct JSContext;

namespace mozilla::dom {
class Document;
class Selection;
class StaticRange;

class AbstractRange : public nsISupports,
                      public nsWrapperCache,
                      // For linking together selection-associated ranges.
                      public mozilla::LinkedListElement<AbstractRange> {
 protected:
  explicit AbstractRange(nsINode* aNode, bool aIsDynamicRange);
  virtual ~AbstractRange();

 public:
  AbstractRange() = delete;
  explicit AbstractRange(const AbstractRange& aOther) = delete;

  /**
   * Called when the process is shutting down.
   */
  static void Shutdown();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(AbstractRange)

  const RangeBoundary& StartRef() const { return mStart; }
  const RangeBoundary& EndRef() const { return mEnd; }

  nsIContent* GetChildAtStartOffset() const {
    return mStart.GetChildAtOffset();
  }
  nsIContent* GetChildAtEndOffset() const { return mEnd.GetChildAtOffset(); }
  bool IsPositioned() const { return mIsPositioned; }
  /**
   * https://dom.spec.whatwg.org/#concept-tree-inclusive-ancestor
   */
  nsINode* GetClosestCommonInclusiveAncestor() const;

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

  Document* GetComposedDocOfContainers() const {
    return mStart.Container() ? mStart.Container()->GetComposedDoc() : nullptr;
  }

  // FYI: Returns 0 if it's not positioned.
  uint32_t StartOffset() const {
    return static_cast<uint32_t>(
        *mStart.Offset(RangeBoundary::OffsetFilter::kValidOrInvalidOffsets));
  }

  // FYI: Returns 0 if it's not positioned.
  uint32_t EndOffset() const {
    return static_cast<uint32_t>(
        *mEnd.Offset(RangeBoundary::OffsetFilter::kValidOrInvalidOffsets));
  }
  bool Collapsed() const {
    return !mIsPositioned || (mStart.Container() == mEnd.Container() &&
                              StartOffset() == EndOffset());
  }

  nsINode* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  bool HasEqualBoundaries(const AbstractRange& aOther) const {
    return (mStart == aOther.mStart) && (mEnd == aOther.mEnd);
  }
  bool IsDynamicRange() const { return mIsDynamicRange; }
  bool IsStaticRange() const { return !mIsDynamicRange; }
  inline nsRange* AsDynamicRange();
  inline const nsRange* AsDynamicRange() const;
  inline StaticRange* AsStaticRange();
  inline const StaticRange* AsStaticRange() const;

  /**
   * Return true if this range is part of a Selection object
   * and isn't detached.
   */
  bool IsInAnySelection() const { return !mSelections.IsEmpty(); }

  MOZ_CAN_RUN_SCRIPT void RegisterSelection(
      mozilla::dom::Selection& aSelection);

  void UnregisterSelection(const mozilla::dom::Selection& aSelection);

  /**
   * Returns a list of all Selections the range is associated with.
   */
  const nsTArray<WeakPtr<Selection>>& GetSelections() const;

  /**
   * Return true if this range is in |aSelection|.
   */
  bool IsInSelection(const mozilla::dom::Selection& aSelection) const;

 protected:
  template <typename SPT, typename SRT, typename EPT, typename ERT,
            typename RangeType>
  static nsresult SetStartAndEndInternal(
      const RangeBoundaryBase<SPT, SRT>& aStartBoundary,
      const RangeBoundaryBase<EPT, ERT>& aEndBoundary, RangeType* aRange);

  template <class RangeType>
  static bool MaybeCacheToReuse(RangeType& aInstance);

  void Init(nsINode* aNode);

  friend std::ostream& operator<<(std::ostream& aStream,
                                  const AbstractRange& aRange) {
    if (aRange.Collapsed()) {
      aStream << "{ mStart=mEnd=" << aRange.mStart;
    } else {
      aStream << "{ mStart=" << aRange.mStart << ", mEnd=" << aRange.mEnd;
    }
    return aStream << ", mIsGenerated="
                   << (aRange.mIsGenerated ? "true" : "false")
                   << ", mCalledByJS="
                   << (aRange.mIsPositioned ? "true" : "false")
                   << ", mIsDynamicRange="
                   << (aRange.mIsDynamicRange ? "true" : "false") << " }";
  }

  /**
   * https://dom.spec.whatwg.org/#concept-tree-inclusive-ancestor
   */
  void RegisterClosestCommonInclusiveAncestor(nsINode* aNode);
  /**
   * https://dom.spec.whatwg.org/#concept-tree-inclusive-ancestor
   */
  void UnregisterClosestCommonInclusiveAncestor(nsINode* aNode,
                                                bool aIsUnlinking);

  void UpdateCommonAncestorIfNecessary();

  static void MarkDescendants(const nsINode& aNode);
  static void UnmarkDescendants(const nsINode& aNode);

 private:
  void ClearForReuse();

 protected:
  RefPtr<Document> mOwner;
  RangeBoundary mStart;
  RangeBoundary mEnd;

  // A Range can be part of multiple |Selection|s. This is a very rare use case.
  AutoTArray<WeakPtr<Selection>, 1> mSelections;
  // mRegisteredClosestCommonInclusiveAncestor is only non-null when the range
  // IsInAnySelection().
  nsCOMPtr<nsINode> mRegisteredClosestCommonInclusiveAncestor;

  // `true` if `mStart` and `mEnd` are set for StaticRange or set and valid
  // for nsRange.
  bool mIsPositioned;

  // Used by nsRange, but this should have this for minimizing the size.
  bool mIsGenerated;
  // Used by nsRange, but this should have this for minimizing the size.
  bool mCalledByJS;

  // true if this is an `nsRange` object.
  const bool mIsDynamicRange;

  static bool sHasShutDown;
};

}  // namespace mozilla::dom

#endif  // #ifndef mozilla_dom_AbstractRange_h
