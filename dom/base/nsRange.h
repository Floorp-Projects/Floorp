/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of the DOM nsIDOMRange object.
 */

#ifndef nsRange_h___
#define nsRange_h___

#include "nsIDOMRange.h"
#include "nsCOMPtr.h"
#include "nsINode.h"
#include "nsIDocument.h"
#include "nsIDOMNode.h"
#include "nsLayoutUtils.h"
#include "prmon.h"
#include "nsStubMutationObserver.h"
#include "nsWrapperCache.h"
#include "mozilla/Attributes.h"
#include "mozilla/GuardObjects.h"

namespace mozilla {
class ErrorResult;
namespace dom {
struct ClientRectsAndTexts;
class DocumentFragment;
class DOMRect;
class DOMRectList;
class Selection;
} // namespace dom
} // namespace mozilla

class nsRange final : public nsIDOMRange,
                      public nsStubMutationObserver,
                      public nsWrapperCache
{
  typedef mozilla::ErrorResult ErrorResult;
  typedef mozilla::dom::DOMRect DOMRect;
  typedef mozilla::dom::DOMRectList DOMRectList;

  virtual ~nsRange();

public:
  explicit nsRange(nsINode* aNode);

  static nsresult CreateRange(nsIDOMNode* aStartParent, int32_t aStartOffset,
                              nsIDOMNode* aEndParent, int32_t aEndOffset,
                              nsRange** aRange);
  static nsresult CreateRange(nsIDOMNode* aStartParent, int32_t aStartOffset,
                              nsIDOMNode* aEndParent, int32_t aEndOffset,
                              nsIDOMRange** aRange);
  static nsresult CreateRange(nsINode* aStartParent, int32_t aStartOffset,
                              nsINode* aEndParent, int32_t aEndOffset,
                              nsRange** aRange);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsRange, nsIDOMRange)

  nsrefcnt GetRefCount() const
  {
    return mRefCnt;
  }

  // nsIDOMRange interface
  NS_DECL_NSIDOMRANGE

  nsINode* GetRoot() const
  {
    return mRoot;
  }

  nsINode* GetStartParent() const
  {
    return mStartParent;
  }

  nsINode* GetEndParent() const
  {
    return mEndParent;
  }

  int32_t StartOffset() const
  {
    return mStartOffset;
  }

  int32_t EndOffset() const
  {
    return mEndOffset;
  }
  
  bool IsPositioned() const
  {
    return mIsPositioned;
  }

  void SetMaySpanAnonymousSubtrees(bool aMaySpanAnonymousSubtrees)
  {
    mMaySpanAnonymousSubtrees = aMaySpanAnonymousSubtrees;
  }
  
  /**
   * Return true iff this range is part of a Selection object
   * and isn't detached.
   */
  bool IsInSelection() const
  {
    return !!mSelection;
  }

  /**
   * Called when the range is added/removed from a Selection.
   */
  void SetSelection(mozilla::dom::Selection* aSelection);

  /**
   * Return true if this range was generated.
   * @see SetIsGenerated
   */
  bool IsGenerated() const
  {
    return mIsGenerated;
  }

  /**
   * Mark this range as being generated or not.
   * Currently it is used for marking ranges that are created when splitting up
   * a range to exclude a -moz-user-select:none region.
   * @see Selection::AddItem
   * @see ExcludeNonSelectableNodes
   */
  void SetIsGenerated(bool aIsGenerated)
  {
    mIsGenerated = aIsGenerated;
  }

  nsINode* GetCommonAncestor() const;
  void Reset();

  /**
   * SetStart() and SetEnd() sets start point or end point separately.
   * However, this is expensive especially when it's a range of Selection.
   * When you set both start and end of a range, you should use
   * SetStartAndEnd() instead.
   */
  nsresult SetStart(nsINode* aParent, int32_t aOffset);
  nsresult SetEnd(nsINode* aParent, int32_t aOffset);

  already_AddRefed<nsRange> CloneRange() const;

  /**
   * SetStartAndEnd() works similar to call both SetStart() and SetEnd().
   * Different from calls them separately, this does nothing if either
   * the start point or the end point is invalid point.
   * If the specified start point is after the end point, the range will be
   * collapsed at the end point.  Similarly, if they are in different root,
   * the range will be collapsed at the end point.
   */
  nsresult SetStartAndEnd(nsINode* aStartParent, int32_t aStartOffset,
                          nsINode* aEndParent, int32_t aEndOffset);

  /**
   * CollapseTo() works similar to call both SetStart() and SetEnd() with
   * same node and offset.  This just calls SetStartAndParent() to set
   * collapsed range at aParent and aOffset.
   */
  nsresult CollapseTo(nsINode* aParent, int32_t aOffset)
  {
    return SetStartAndEnd(aParent, aOffset, aParent, aOffset);
  }

  /**
   * Retrieves node and offset for setting start or end of a range to
   * before or after aNode.
   */
  static nsINode* GetParentAndOffsetAfter(nsINode* aNode, int32_t* aOffset)
  {
    MOZ_ASSERT(aNode);
    MOZ_ASSERT(aOffset);
    nsINode* parentNode = aNode->GetParentNode();
    *aOffset = parentNode ? parentNode->IndexOf(aNode) : -1;
    if (*aOffset >= 0) {
      (*aOffset)++;
    }
    return parentNode;
  }
  static nsINode* GetParentAndOffsetBefore(nsINode* aNode, int32_t* aOffset)
  {
    MOZ_ASSERT(aNode);
    MOZ_ASSERT(aOffset);
    nsINode* parentNode = aNode->GetParentNode();
    *aOffset = parentNode ? parentNode->IndexOf(aNode) : -1;
    return parentNode;
  }

  NS_IMETHOD GetUsedFontFaces(nsIDOMFontFaceList** aResult);

  // nsIMutationObserver methods
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
  NS_DECL_NSIMUTATIONOBSERVER_PARENTCHAINCHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED

  // WebIDL
  static already_AddRefed<nsRange>
  Constructor(const mozilla::dom::GlobalObject& global,
              mozilla::ErrorResult& aRv);

  bool Collapsed() const
  {
    return mIsPositioned && mStartParent == mEndParent &&
           mStartOffset == mEndOffset;
  }
  already_AddRefed<mozilla::dom::DocumentFragment>
  CreateContextualFragment(const nsAString& aString, ErrorResult& aError);
  already_AddRefed<mozilla::dom::DocumentFragment>
  CloneContents(ErrorResult& aErr);
  int16_t CompareBoundaryPoints(uint16_t aHow, nsRange& aOther,
                                ErrorResult& aErr);
  int16_t ComparePoint(nsINode& aParent, uint32_t aOffset, ErrorResult& aErr);
  void DeleteContents(ErrorResult& aRv);
  already_AddRefed<mozilla::dom::DocumentFragment>
    ExtractContents(ErrorResult& aErr);
  nsINode* GetCommonAncestorContainer(ErrorResult& aRv) const;
  nsINode* GetStartContainer(ErrorResult& aRv) const;
  uint32_t GetStartOffset(ErrorResult& aRv) const;
  nsINode* GetEndContainer(ErrorResult& aRv) const;
  uint32_t GetEndOffset(ErrorResult& aRv) const;
  void InsertNode(nsINode& aNode, ErrorResult& aErr);
  bool IntersectsNode(nsINode& aNode, ErrorResult& aRv);
  bool IsPointInRange(nsINode& aParent, uint32_t aOffset, ErrorResult& aErr);

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
  void GetClientRectsAndTexts(
    mozilla::dom::ClientRectsAndTexts& aResult,
    ErrorResult& aErr);

  // Following methods should be used for internal use instead of *JS().
  void SelectNode(nsINode& aNode, ErrorResult& aErr);
  void SelectNodeContents(nsINode& aNode, ErrorResult& aErr);
  void SetEnd(nsINode& aNode, uint32_t aOffset, ErrorResult& aErr);
  void SetEndAfter(nsINode& aNode, ErrorResult& aErr);
  void SetEndBefore(nsINode& aNode, ErrorResult& aErr);
  void SetStart(nsINode& aNode, uint32_t aOffset, ErrorResult& aErr);
  void SetStartAfter(nsINode& aNode, ErrorResult& aErr);
  void SetStartBefore(nsINode& aNode, ErrorResult& aErr);

  static void GetInnerTextNoFlush(mozilla::dom::DOMString& aValue,
                                  mozilla::ErrorResult& aError,
                                  nsIContent* aStartParent,
                                  uint32_t aStartOffset,
                                  nsIContent* aEndParent,
                                  uint32_t aEndOffset);

  nsINode* GetParentObject() const { return mOwner; }
  virtual JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override final;

private:
  // no copy's or assigns
  nsRange(const nsRange&);
  nsRange& operator=(const nsRange&);

  /**
   * Cut or delete the range's contents.
   *
   * @param aFragment nsIDOMDocumentFragment containing the nodes.
   *                  May be null to indicate the caller doesn't want a fragment.
   */
  nsresult CutContents(mozilla::dom::DocumentFragment** frag);

  static nsresult CloneParentsBetween(nsINode* aAncestor,
                                      nsINode* aNode,
                                      nsINode** aClosestAncestor,
                                      nsINode** aFarthestAncestor);

public:
/******************************************************************************
 *  Utility routine to detect if a content node starts before a range and/or
 *  ends after a range.  If neither it is contained inside the range.
 *
 *  XXX - callers responsibility to ensure node in same doc as range!
 *
 *****************************************************************************/
  static nsresult CompareNodeToRange(nsINode* aNode, nsRange* aRange,
                                     bool *outNodeBefore,
                                     bool *outNodeAfter);

  /**
   * Return true if any part of (aNode, aStartOffset) .. (aNode, aEndOffset)
   * overlaps any nsRange in aNode's GetNextRangeCommonAncestor ranges (i.e.
   * where aNode is a descendant of a range's common ancestor node).
   * If a nsRange starts in (aNode, aEndOffset) or if it ends in
   * (aNode, aStartOffset) then it is non-overlapping and the result is false
   * for that nsRange.  Collapsed ranges always counts as non-overlapping.
   */
  static bool IsNodeSelected(nsINode* aNode, uint32_t aStartOffset,
                             uint32_t aEndOffset);

  /**
   * This helper function gets rects and correlated text for the given range.
   * @param aTextList optional where nullptr = don't retrieve text
   */
  static void CollectClientRectsAndText(nsLayoutUtils::RectCallback* aCollector,
                                        mozilla::dom::Sequence<nsString>* aTextList,
                                        nsRange* aRange,
                                        nsINode* aStartParent, int32_t aStartOffset,
                                        nsINode* aEndParent, int32_t aEndOffset,
                                        bool aClampToEdge, bool aFlushLayout);

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

  typedef nsTHashtable<nsPtrHashKey<nsRange> > RangeHashTable;
protected:
  void RegisterCommonAncestor(nsINode* aNode);
  void UnregisterCommonAncestor(nsINode* aNode);
  nsINode* IsValidBoundary(nsINode* aNode);
  static bool IsValidOffset(nsINode* aNode, int32_t aOffset);

  // CharacterDataChanged set aNotInsertedYet to true to disable an assertion
  // and suppress re-registering a range common ancestor node since
  // the new text node of a splitText hasn't been inserted yet.
  // CharacterDataChanged does the re-registering when needed.
  void DoSetRange(nsINode* aStartN, int32_t aStartOffset,
                  nsINode* aEndN, int32_t aEndOffset,
                  nsINode* aRoot, bool aNotInsertedYet = false);

  /**
   * For a range for which IsInSelection() is true, return the common
   * ancestor for the range.  This method uses the selection bits and
   * nsGkAtoms::range property on the nodes to quickly find the ancestor.
   * That is, it's a faster version of GetCommonAncestor that only works
   * for ranges in a Selection.  The method will assert and the behavior
   * is undefined if called on a range where IsInSelection() is false.
   */
  nsINode* GetRegisteredCommonAncestor();

  // Helper to IsNodeSelected.
  static bool IsNodeInSortedRanges(nsINode* aNode,
                                   uint32_t aStartOffset,
                                   uint32_t aEndOffset,
                                   const nsTArray<const nsRange*>& aRanges,
                                   size_t aRangeStart,
                                   size_t aRangeEnd);

  // Assume that this is guaranteed that this is held by the caller when
  // this is used.  (Note that we cannot use AutoRestore for mCalledByJS
  // due to a bit field.)
  class MOZ_RAII AutoCalledByJSRestore final
  {
  private:
    nsRange& mRange;
    bool mOldValue;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

  public:
    explicit AutoCalledByJSRestore(nsRange& aRange
                                   MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : mRange(aRange)
      , mOldValue(aRange.mCalledByJS)
    {
      MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }
    ~AutoCalledByJSRestore()
    {
      mRange.mCalledByJS = mOldValue;
    }
    bool SavedValue() const { return mOldValue; }
  };

  struct MOZ_STACK_CLASS AutoInvalidateSelection
  {
    explicit AutoInvalidateSelection(nsRange* aRange) : mRange(aRange)
    {
#ifdef DEBUG
      mWasInSelection = mRange->IsInSelection();
#endif
      if (!mRange->IsInSelection() || mIsNested) {
        return;
      }
      mIsNested = true;
      mCommonAncestor = mRange->GetRegisteredCommonAncestor();
    }
    ~AutoInvalidateSelection();
    nsRange* mRange;
    RefPtr<nsINode> mCommonAncestor;
#ifdef DEBUG
    bool mWasInSelection;
#endif
    static bool mIsNested;
  };

  nsCOMPtr<nsIDocument> mOwner;
  nsCOMPtr<nsINode> mRoot;
  nsCOMPtr<nsINode> mStartParent;
  nsCOMPtr<nsINode> mEndParent;
  RefPtr<mozilla::dom::Selection> mSelection;
  int32_t mStartOffset;
  int32_t mEndOffset;

  bool mIsPositioned : 1;
  bool mMaySpanAnonymousSubtrees : 1;
  bool mIsGenerated : 1;
  bool mStartOffsetWasIncremented : 1;
  bool mEndOffsetWasIncremented : 1;
  bool mCalledByJS : 1;
#ifdef DEBUG
  int32_t  mAssertNextInsertOrAppendIndex;
  nsINode* mAssertNextInsertOrAppendNode;
#endif
};

inline nsISupports*
ToCanonicalSupports(nsRange* aRange)
{
  return static_cast<nsIDOMRange*>(aRange);
}

inline nsISupports*
ToSupports(nsRange* aRange)
{
  return static_cast<nsIDOMRange*>(aRange);
}

#endif /* nsRange_h___ */
