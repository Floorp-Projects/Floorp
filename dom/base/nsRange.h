/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of the DOM Range object.
 */

#ifndef nsRange_h___
#define nsRange_h___

#include "nsCOMPtr.h"
#include "mozilla/dom/AbstractRange.h"
#include "nsLayoutUtils.h"
#include "prmon.h"
#include "nsStubMutationObserver.h"
#include "nsWrapperCache.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/LinkedList.h"
#include "mozilla/RangeBoundary.h"

namespace mozilla {
namespace dom {
struct ClientRectsAndTexts;
class DocGroup;
class DocumentFragment;
class DOMRect;
class DOMRectList;
class InspectorFontFace;
class Selection;
}  // namespace dom
}  // namespace mozilla

class nsRange final : public mozilla::dom::AbstractRange,
                      public nsStubMutationObserver,
                      // For linking together selection-associated ranges.
                      public mozilla::LinkedListElement<nsRange> {
  typedef mozilla::ErrorResult ErrorResult;
  typedef mozilla::dom::AbstractRange AbstractRange;
  typedef mozilla::dom::DocGroup DocGroup;
  typedef mozilla::dom::DOMRect DOMRect;
  typedef mozilla::dom::DOMRectList DOMRectList;
  typedef mozilla::RangeBoundary RangeBoundary;
  typedef mozilla::RawRangeBoundary RawRangeBoundary;

  virtual ~nsRange();
  explicit nsRange(nsINode* aNode);

 public:
  /**
   * The following Create() returns `nsRange` instance which is initialized
   * only with aNode.  The result is never positioned.
   */
  static already_AddRefed<nsRange> Create(nsINode* aNode);

  /**
   * The following Create() may return `nsRange` instance which is initialized
   * with given range or points.  If it fails initializing new range with the
   * arguments, returns `nullptr`.  `ErrorResult` is set to an error only
   * when this returns `nullptr`.  The error code indicates the reason why
   * it couldn't initialize the instance.
   */
  static already_AddRefed<nsRange> Create(const AbstractRange* aAbstractRange,
                                          ErrorResult& aRv) {
    return nsRange::Create(aAbstractRange->StartRef(), aAbstractRange->EndRef(),
                           aRv);
  }
  static already_AddRefed<nsRange> Create(nsINode* aStartContainer,
                                          uint32_t aStartOffset,
                                          nsINode* aEndContainer,
                                          uint32_t aEndOffset,
                                          ErrorResult& aRv) {
    return nsRange::Create(RawRangeBoundary(aStartContainer, aStartOffset),
                           RawRangeBoundary(aEndContainer, aEndOffset), aRv);
  }
  template <typename SPT, typename SRT, typename EPT, typename ERT>
  static already_AddRefed<nsRange> Create(
      const mozilla::RangeBoundaryBase<SPT, SRT>& aStartBoundary,
      const mozilla::RangeBoundaryBase<EPT, ERT>& aEndBoundary,
      ErrorResult& aRv);

  NS_DECL_ISUPPORTS_INHERITED
  NS_IMETHODIMP_(void) DeleteCycleCollectable(void) override;
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(nsRange, AbstractRange)

  nsrefcnt GetRefCount() const { return mRefCnt; }

  nsINode* GetRoot() const { return mRoot; }

  /**
   * Return true iff this range is part of a Selection object
   * and isn't detached.
   */
  bool IsInSelection() const { return !!mSelection; }

  MOZ_CAN_RUN_SCRIPT void RegisterSelection(
      mozilla::dom::Selection& aSelection);

  void UnregisterSelection();

  /**
   * Returns pointer to a Selection if the range is associated with a Selection.
   */
  mozilla::dom::Selection* GetSelection() const { return mSelection; }

  /**
   * Return true if this range was generated.
   * @see SetIsGenerated
   */
  bool IsGenerated() const { return mIsGenerated; }

  /**
   * Mark this range as being generated or not.
   * Currently it is used for marking ranges that are created when splitting up
   * a range to exclude a -moz-user-select:none region.
   * @see Selection::AddRangesForSelectableNodes
   * @see ExcludeNonSelectableNodes
   */
  void SetIsGenerated(bool aIsGenerated) { mIsGenerated = aIsGenerated; }

  void Reset();

  /**
   * SetStart() and SetEnd() sets start point or end point separately.
   * However, this is expensive especially when it's a range of Selection.
   * When you set both start and end of a range, you should use
   * SetStartAndEnd() instead.
   */
  nsresult SetStart(nsINode* aContainer, uint32_t aOffset) {
    ErrorResult error;
    SetStart(RawRangeBoundary(aContainer, aOffset), error);
    return error.StealNSResult();
  }
  nsresult SetEnd(nsINode* aContainer, uint32_t aOffset) {
    ErrorResult error;
    SetEnd(RawRangeBoundary(aContainer, aOffset), error);
    return error.StealNSResult();
  }

  already_AddRefed<nsRange> CloneRange() const;

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
  nsresult SetStartAndEnd(
      const mozilla::RangeBoundaryBase<SPT, SRT>& aStartBoundary,
      const mozilla::RangeBoundaryBase<EPT, ERT>& aEndBoundary) {
    return AbstractRange::SetStartAndEndInternal(aStartBoundary, aEndBoundary,
                                                 this);
  }

  /**
   * Adds all nodes between |aStartContent| and |aEndContent| to the range.
   * The start offset will be set before |aStartContent|,
   * while the end offset will be set immediately after |aEndContent|.
   *
   * Caller must guarantee both nodes are non null and
   * children of |aContainer| and that |aEndContent| is after |aStartContent|.
   */
  void SelectNodesInContainer(nsINode* aContainer, nsIContent* aStartContent,
                              nsIContent* aEndContent);

  /**
   * CollapseTo() works similar to call both SetStart() and SetEnd() with
   * same node and offset.  This just calls SetStartAndParent() to set
   * collapsed range at aContainer and aOffset.
   */
  nsresult CollapseTo(nsINode* aContainer, uint32_t aOffset) {
    return CollapseTo(RawRangeBoundary(aContainer, aOffset));
  }
  nsresult CollapseTo(const RawRangeBoundary& aPoint) {
    return SetStartAndEnd(aPoint, aPoint);
  }

  // aMaxRanges is the maximum number of text ranges to record for each face
  // (pass 0 to just get the list of faces, without recording exact ranges
  // where each face was used).
  nsresult GetUsedFontFaces(
      nsTArray<mozilla::UniquePtr<mozilla::dom::InspectorFontFace>>& aResult,
      uint32_t aMaxRanges, bool aSkipCollapsedWhitespace);

  // nsIMutationObserver methods
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
  NS_DECL_NSIMUTATIONOBSERVER_PARENTCHAINCHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED

  // WebIDL
  static already_AddRefed<nsRange> Constructor(
      const mozilla::dom::GlobalObject& global, mozilla::ErrorResult& aRv);

  already_AddRefed<mozilla::dom::DocumentFragment> CreateContextualFragment(
      const nsAString& aString, ErrorResult& aError);
  already_AddRefed<mozilla::dom::DocumentFragment> CloneContents(
      ErrorResult& aErr);
  int16_t CompareBoundaryPoints(uint16_t aHow, nsRange& aOther,
                                ErrorResult& aErr);
  int16_t ComparePoint(nsINode& aContainer, uint32_t aOffset,
                       ErrorResult& aErr) const;
  void DeleteContents(ErrorResult& aRv);
  already_AddRefed<mozilla::dom::DocumentFragment> ExtractContents(
      ErrorResult& aErr);
  nsINode* GetCommonAncestorContainer(ErrorResult& aRv) const {
    if (!mIsPositioned) {
      aRv.Throw(NS_ERROR_NOT_INITIALIZED);
      return nullptr;
    }
    return GetClosestCommonInclusiveAncestor();
  }
  void InsertNode(nsINode& aNode, ErrorResult& aErr);
  bool IntersectsNode(nsINode& aNode, ErrorResult& aRv);
  bool IsPointInRange(nsINode& aContainer, uint32_t aOffset,
                      ErrorResult& aErr) const;
  void ToString(nsAString& aReturn, ErrorResult& aErr);
  void Detach();

  // *JS() methods are mapped to Range.*() of DOM.
  // They may move focus only when the range represents normal selection.
  // These methods shouldn't be used from internal.
  void CollapseJS(bool aToStart);
  void SelectNodeJS(nsINode& aNode, ErrorResult& aErr);
  void SelectNodeContentsJS(nsINode& aNode, ErrorResult& aErr);
  void SetEndJS(nsINode& aNode, uint32_t aOffset, ErrorResult& aErr);
  void SetEndAfterJS(nsINode& aNode, ErrorResult& aErr);
  void SetEndBeforeJS(nsINode& aNode, ErrorResult& aErr);
  void SetStartJS(nsINode& aNode, uint32_t aOffset, ErrorResult& aErr);
  void SetStartAfterJS(nsINode& aNode, ErrorResult& aErr);
  void SetStartBeforeJS(nsINode& aNode, ErrorResult& aErr);

  void SurroundContents(nsINode& aNode, ErrorResult& aErr);
  already_AddRefed<DOMRect> GetBoundingClientRect(bool aClampToEdge = true,
                                                  bool aFlushLayout = true);
  already_AddRefed<DOMRectList> GetClientRects(bool aClampToEdge = true,
                                               bool aFlushLayout = true);
  void GetClientRectsAndTexts(mozilla::dom::ClientRectsAndTexts& aResult,
                              ErrorResult& aErr);

  // Following methods should be used for internal use instead of *JS().
  void SelectNode(nsINode& aNode, ErrorResult& aErr);
  void SelectNodeContents(nsINode& aNode, ErrorResult& aErr);
  void SetEnd(nsINode& aNode, uint32_t aOffset, ErrorResult& aErr);
  void SetEnd(const RawRangeBoundary& aPoint, ErrorResult& aErr);
  void SetEndAfter(nsINode& aNode, ErrorResult& aErr);
  void SetEndBefore(nsINode& aNode, ErrorResult& aErr);
  void SetStart(nsINode& aNode, uint32_t aOffset, ErrorResult& aErr);
  void SetStart(const RawRangeBoundary& aPoint, ErrorResult& aErr);
  void SetStartAfter(nsINode& aNode, ErrorResult& aErr);
  void SetStartBefore(nsINode& aNode, ErrorResult& aErr);
  void Collapse(bool aToStart);

  static void GetInnerTextNoFlush(mozilla::dom::DOMString& aValue,
                                  mozilla::ErrorResult& aError,
                                  nsIContent* aContainer);

  virtual JSObject* WrapObject(JSContext* cx,
                               JS::Handle<JSObject*> aGivenProto) final;
  DocGroup* GetDocGroup() const;

 private:
  // no copy's or assigns
  nsRange(const nsRange&);
  nsRange& operator=(const nsRange&);

  /**
   * Cut or delete the range's contents.
   *
   * @param aFragment DocumentFragment containing the nodes.
   *                  May be null to indicate the caller doesn't want a
   * fragment.
   */
  nsresult CutContents(mozilla::dom::DocumentFragment** frag);

  static nsresult CloneParentsBetween(nsINode* aAncestor, nsINode* aNode,
                                      nsINode** aClosestAncestor,
                                      nsINode** aFarthestAncestor);

  /**
   * Returns whether a node is safe to be accessed by the current caller.
   */
  bool CanAccess(const nsINode&) const;

  void AdjustNextRefsOnCharacterDataSplit(const nsIContent& aContent,
                                          const CharacterDataChangeInfo& aInfo);

  struct RangeBoundariesAndRoot {
    RawRangeBoundary mStart;
    RawRangeBoundary mEnd;
    nsINode* mRoot = nullptr;
  };

  /**
   * @param aContent Must be non-nullptr.
   */
  RangeBoundariesAndRoot DetermineNewRangeBoundariesAndRootOnCharacterDataMerge(
      nsIContent* aContent, const CharacterDataChangeInfo& aInfo) const;

  // @return true iff the range is positioned, aContainer belongs to the same
  //         document as the range, aContainer is a DOCUMENT_TYPE_NODE and
  //         aOffset doesn't exceed aContainer's length.
  bool IsPointComparableToRange(const nsINode& aContainer, uint32_t aOffset,
                                ErrorResult& aErrorResult) const;

 public:
  /**
   * This helper function gets rects and correlated text for the given range.
   * @param aTextList optional where nullptr = don't retrieve text
   */
  static void CollectClientRectsAndText(
      nsLayoutUtils::RectCallback* aCollector,
      mozilla::dom::Sequence<nsString>* aTextList, nsRange* aRange,
      nsINode* aStartContainer, uint32_t aStartOffset, nsINode* aEndContainer,
      uint32_t aEndOffset, bool aClampToEdge, bool aFlushLayout);

  /**
   * Scan this range for -moz-user-select:none nodes and split it up into
   * multiple ranges to exclude those nodes.  The resulting ranges are put
   * in aOutRanges.  If no -moz-user-select:none node is found in the range
   * then |this| is unmodified and is the only range in aOutRanges.
   * Otherwise, |this| will be modified so that it ends before the first
   * -moz-user-select:none node and additional ranges may also be created.
   * If all nodes in the range are -moz-user-select:none then aOutRanges
   * will be empty.
   * @param aOutRanges the resulting set of ranges
   */
  void ExcludeNonSelectableNodes(nsTArray<RefPtr<nsRange>>* aOutRanges);

  /**
   * Notify the selection listeners after a range has been modified.
   */
  MOZ_CAN_RUN_SCRIPT void NotifySelectionListenersAfterRangeSet();

  typedef nsTHashtable<nsPtrHashKey<nsRange>> RangeHashTable;

 protected:
  /**
   * https://dom.spec.whatwg.org/#concept-tree-inclusive-ancestor
   */
  void RegisterClosestCommonInclusiveAncestor(nsINode* aNode);
  /**
   * https://dom.spec.whatwg.org/#concept-tree-inclusive-ancestor
   */
  void UnregisterClosestCommonInclusiveAncestor(nsINode* aNode,
                                                bool aIsUnlinking);

  /**
   * DoSetRange() is called when `AbstractRange::SetStartAndEndInternal()` sets
   * mStart and mEnd, or some other internal methods modify `mStart` and/or
   * `mEnd`.  Therefore, this shouldn't be a virtual method.
   *
   * @param aStartBoundary      Computed start point.  This must equals or be
   *                            before aEndBoundary in the DOM tree order.
   * @param aEndBoundary        Computed end point.
   * @param aRootNode           The root node.
   * @param aNotInsertedYet     true if this is called by CharacterDataChanged()
   *                            to disable assertion and suppress re-registering
   *                            a range common ancestor node since the new text
   *                            node of a splitText hasn't been inserted yet.
   *                            CharacterDataChanged() does the re-registering
   *                            when needed.  Otherwise, false.
   */
  template <typename SPT, typename SRT, typename EPT, typename ERT>
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void DoSetRange(
      const mozilla::RangeBoundaryBase<SPT, SRT>& aStartBoundary,
      const mozilla::RangeBoundaryBase<EPT, ERT>& aEndBoundary,
      nsINode* aRootNode, bool aNotInsertedYet = false);

  /**
   * For a range for which IsInSelection() is true, return the closest common
   * inclusive ancestor
   * (https://dom.spec.whatwg.org/#concept-tree-inclusive-ancestor)
   * for the range, which we had to compute when the common ancestor changed or
   * IsInSelection became true, so we could register with it. That is, it's a
   * faster version of GetClosestCommonInclusiveAncestor that only works for
   * ranges in a Selection. The method will assert and the behavior is undefined
   * if called on a range where IsInSelection() is false.
   */
  nsINode* GetRegisteredClosestCommonInclusiveAncestor();

  // Assume that this is guaranteed that this is held by the caller when
  // this is used.  (Note that we cannot use AutoRestore for mCalledByJS
  // due to a bit field.)
  class MOZ_RAII AutoCalledByJSRestore final {
   private:
    nsRange& mRange;
    bool mOldValue;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

   public:
    explicit AutoCalledByJSRestore(
        nsRange& aRange MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
        : mRange(aRange), mOldValue(aRange.mCalledByJS) {
      MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }
    ~AutoCalledByJSRestore() { mRange.mCalledByJS = mOldValue; }
    bool SavedValue() const { return mOldValue; }
  };

  struct MOZ_STACK_CLASS AutoInvalidateSelection {
    explicit AutoInvalidateSelection(nsRange* aRange) : mRange(aRange) {
      if (!mRange->IsInSelection() || sIsNested) {
        return;
      }
      sIsNested = true;
      mCommonAncestor = mRange->GetRegisteredClosestCommonInclusiveAncestor();
    }
    ~AutoInvalidateSelection();
    nsRange* mRange;
    RefPtr<nsINode> mCommonAncestor;
    static bool sIsNested;
  };

  bool MaybeInterruptLastRelease();

#ifdef DEBUG
  bool IsCleared() const {
    return !mRoot && !mRegisteredClosestCommonInclusiveAncestor &&
           !mSelection && !mNextStartRef && !mNextEndRef;
  }
#endif  // #ifdef DEBUG

  nsCOMPtr<nsINode> mRoot;
  // mRegisteredClosestCommonInclusiveAncestor is only non-null when the range
  // IsInSelection().  It's kept alive via mStart/mEnd,
  // because we update it any time those could become disconnected from it.
  nsINode* MOZ_NON_OWNING_REF mRegisteredClosestCommonInclusiveAncestor;
  mozilla::WeakPtr<mozilla::dom::Selection> mSelection;

  // These raw pointers are used to remember a child that is about
  // to be inserted between a CharacterData call and a subsequent
  // ContentInserted or ContentAppended call. It is safe to store
  // these refs because the caller is guaranteed to trigger both
  // notifications while holding a strong reference to the new child.
  nsIContent* MOZ_NON_OWNING_REF mNextStartRef;
  nsIContent* MOZ_NON_OWNING_REF mNextEndRef;

  static nsTArray<RefPtr<nsRange>>* sCachedRanges;

  friend class mozilla::dom::AbstractRange;
};

#endif /* nsRange_h___ */
