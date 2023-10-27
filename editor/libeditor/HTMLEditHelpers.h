/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HTMLEditHelpers_h
#define HTMLEditHelpers_h

/**
 * This header declares/defines trivial helper classes which are used by
 * HTMLEditor.  If you want to create or look for static utility methods,
 * see HTMLEditUtils.h.
 */

#include "EditorDOMPoint.h"
#include "EditorForwards.h"
#include "EditorUtils.h"  // for CaretPoint

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Attributes.h"
#include "mozilla/ContentIterator.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/Maybe.h"
#include "mozilla/RangeBoundary.h"
#include "mozilla/Result.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/StaticRange.h"

#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsRange.h"
#include "nsString.h"

class nsISimpleEnumerator;

namespace mozilla {

enum class BlockInlineCheck : uint8_t {
  // BlockInlineCheck is not expected by the root caller.
  Unused,
  // Refer only the HTML default style at considering whether block or inline.
  // All non-HTML elements are treated as inline.
  UseHTMLDefaultStyle,
  // Refer the element's computed style of display-outside at considering
  // whether block or inline.
  // FYI: If editor.block_inline_check.use_computed_style pref is set to false,
  // this is same as HTMLDefaultStyle.
  UseComputedDisplayOutsideStyle,
  // Refer the element's computed style of display at considering whether block
  // or inline.  I.e., this is a good value to look for any block boundary.
  // E.g., this is proper value when:
  // * Checking visibility of collapsible white-spaces or <br>
  // * Looking for whether a padding <br> is required
  // * Looking for a caret position
  // FYI: If editor.block_inline_check.use_computed_style pref is set to false,
  // this is same as HTMLDefaultStyle.
  UseComputedDisplayStyle,
};

/**
 * Even if the caller wants block boundary caused by display-inline: flow-root
 * like inline-block, because it's required only when scanning from in it.
 * I.e., if scanning needs to go to siblings, we don't want to treat
 * inline-block siblings as inline.
 */
[[nodiscard]] inline BlockInlineCheck IgnoreInsideBlockBoundary(
    BlockInlineCheck aBlockInlineCheck) {
  return aBlockInlineCheck == BlockInlineCheck::UseComputedDisplayStyle
             ? BlockInlineCheck::UseComputedDisplayOutsideStyle
             : aBlockInlineCheck;
}

enum class WithTransaction { No, Yes };
inline std::ostream& operator<<(std::ostream& aStream,
                                WithTransaction aWithTransaction) {
  aStream << "WithTransaction::"
          << (aWithTransaction == WithTransaction::Yes ? "Yes" : "No");
  return aStream;
}

/*****************************************************************************
 * MoveNodeResult is a simple class for MoveSomething() methods.
 * This stores whether it's handled or not, and next insertion point and a
 * suggestion for new caret position.
 *****************************************************************************/
class MOZ_STACK_CLASS MoveNodeResult final : public CaretPoint {
 public:
  constexpr bool Handled() const { return mHandled; }
  constexpr bool Ignored() const { return !Handled(); }
  constexpr const EditorDOMPoint& NextInsertionPointRef() const {
    return mNextInsertionPoint;
  }
  constexpr EditorDOMPoint&& UnwrapNextInsertionPoint() {
    return std::move(mNextInsertionPoint);
  }
  template <typename EditorDOMPointType>
  EditorDOMPointType NextInsertionPoint() const {
    return mNextInsertionPoint.To<EditorDOMPointType>();
  }

  /**
   * Override the result as "handled" forcibly.
   */
  void MarkAsHandled() { mHandled = true; }

  MoveNodeResult(const MoveNodeResult& aOther) = delete;
  MoveNodeResult& operator=(const MoveNodeResult& aOther) = delete;
  MoveNodeResult(MoveNodeResult&& aOther) = default;
  MoveNodeResult& operator=(MoveNodeResult&& aOther) = default;

  MoveNodeResult& operator|=(const MoveNodeResult& aOther) {
    MOZ_ASSERT(this != &aOther);
    // aOther is merged with this instance so that its caret suggestion
    // shouldn't be handled anymore.
    aOther.IgnoreCaretPointSuggestion();
    // Should be handled again even if it's already handled
    UnmarkAsHandledCaretPoint();

    mHandled |= aOther.mHandled;

    // Take the new one for the next insertion point.
    mNextInsertionPoint = aOther.mNextInsertionPoint;

    // Take the new caret point if and only if it's suggested.
    if (aOther.HasCaretPointSuggestion()) {
      SetCaretPoint(aOther.CaretPointRef());
    }
    return *this;
  }

#ifdef DEBUG
  ~MoveNodeResult() {
    MOZ_ASSERT_IF(Handled(), !HasCaretPointSuggestion() || CaretPointHandled());
  }
#endif

  /*****************************************************************************
   * When a move node handler (or its helper) does nothing,
   * the result of these factory methods should be returned.
   * aNextInsertionPoint Must be set and valid.
   *****************************************************************************/
  static MoveNodeResult IgnoredResult(
      const EditorDOMPoint& aNextInsertionPoint) {
    return MoveNodeResult(aNextInsertionPoint, false);
  }
  static MoveNodeResult IgnoredResult(EditorDOMPoint&& aNextInsertionPoint) {
    return MoveNodeResult(std::move(aNextInsertionPoint), false);
  }

  /*****************************************************************************
   * When a move node handler (or its helper) handled and not canceled,
   * the result of these factory methods should be returned.
   * aNextInsertionPoint Must be set and valid.
   *****************************************************************************/
  static MoveNodeResult HandledResult(
      const EditorDOMPoint& aNextInsertionPoint) {
    return MoveNodeResult(aNextInsertionPoint, true);
  }

  static MoveNodeResult HandledResult(EditorDOMPoint&& aNextInsertionPoint) {
    return MoveNodeResult(std::move(aNextInsertionPoint), true);
  }

  static MoveNodeResult HandledResult(const EditorDOMPoint& aNextInsertionPoint,
                                      const EditorDOMPoint& aPointToPutCaret) {
    return MoveNodeResult(aNextInsertionPoint, aPointToPutCaret);
  }

  static MoveNodeResult HandledResult(EditorDOMPoint&& aNextInsertionPoint,
                                      const EditorDOMPoint& aPointToPutCaret) {
    return MoveNodeResult(std::move(aNextInsertionPoint), aPointToPutCaret);
  }

  static MoveNodeResult HandledResult(const EditorDOMPoint& aNextInsertionPoint,
                                      EditorDOMPoint&& aPointToPutCaret) {
    return MoveNodeResult(aNextInsertionPoint, std::move(aPointToPutCaret));
  }

  static MoveNodeResult HandledResult(EditorDOMPoint&& aNextInsertionPoint,
                                      EditorDOMPoint&& aPointToPutCaret) {
    return MoveNodeResult(std::move(aNextInsertionPoint),
                          std::move(aPointToPutCaret));
  }

 private:
  explicit MoveNodeResult(const EditorDOMPoint& aNextInsertionPoint,
                          bool aHandled)
      : mNextInsertionPoint(aNextInsertionPoint),
        mHandled(aHandled && aNextInsertionPoint.IsSet()) {
    AutoEditorDOMPointChildInvalidator computeOffsetAndForgetChild(
        mNextInsertionPoint);
  }
  explicit MoveNodeResult(EditorDOMPoint&& aNextInsertionPoint, bool aHandled)
      : mNextInsertionPoint(std::move(aNextInsertionPoint)),
        mHandled(aHandled && mNextInsertionPoint.IsSet()) {
    AutoEditorDOMPointChildInvalidator computeOffsetAndForgetChild(
        mNextInsertionPoint);
  }
  explicit MoveNodeResult(const EditorDOMPoint& aNextInsertionPoint,
                          const EditorDOMPoint& aPointToPutCaret)
      : CaretPoint(aPointToPutCaret),
        mNextInsertionPoint(aNextInsertionPoint),
        mHandled(mNextInsertionPoint.IsSet()) {
    AutoEditorDOMPointChildInvalidator computeOffsetAndForgetChild(
        mNextInsertionPoint);
  }
  explicit MoveNodeResult(EditorDOMPoint&& aNextInsertionPoint,
                          const EditorDOMPoint& aPointToPutCaret)
      : CaretPoint(aPointToPutCaret),
        mNextInsertionPoint(std::move(aNextInsertionPoint)),
        mHandled(mNextInsertionPoint.IsSet()) {
    AutoEditorDOMPointChildInvalidator computeOffsetAndForgetChild(
        mNextInsertionPoint);
  }
  explicit MoveNodeResult(const EditorDOMPoint& aNextInsertionPoint,
                          EditorDOMPoint&& aPointToPutCaret)
      : CaretPoint(std::move(aPointToPutCaret)),
        mNextInsertionPoint(aNextInsertionPoint),
        mHandled(mNextInsertionPoint.IsSet()) {
    AutoEditorDOMPointChildInvalidator computeOffsetAndForgetChild(
        mNextInsertionPoint);
  }
  explicit MoveNodeResult(EditorDOMPoint&& aNextInsertionPoint,
                          EditorDOMPoint&& aPointToPutCaret)
      : CaretPoint(std::move(aPointToPutCaret)),
        mNextInsertionPoint(std::move(aNextInsertionPoint)),
        mHandled(mNextInsertionPoint.IsSet()) {
    AutoEditorDOMPointChildInvalidator computeOffsetAndForgetChild(
        mNextInsertionPoint);
  }

  EditorDOMPoint mNextInsertionPoint;
  bool mHandled;
};

/*****************************************************************************
 * SplitNodeResult is a simple class for
 * HTMLEditor::SplitNodeDeepWithTransaction().
 * This makes the callers' code easier to read.
 *****************************************************************************/
class MOZ_STACK_CLASS SplitNodeResult final : public CaretPoint {
 public:
  bool Handled() const { return mPreviousNode || mNextNode; }

  /**
   * DidSplit() returns true if a node was actually split.
   */
  bool DidSplit() const { return mPreviousNode && mNextNode; }

  /**
   * GetPreviousContent() returns previous content node at the split point.
   */
  MOZ_KNOWN_LIVE nsIContent* GetPreviousContent() const {
    if (mGivenSplitPoint.IsSet()) {
      return mGivenSplitPoint.IsEndOfContainer() ? mGivenSplitPoint.GetChild()
                                                 : nullptr;
    }
    return mPreviousNode;
  }
  template <typename NodeType>
  MOZ_KNOWN_LIVE NodeType* GetPreviousContentAs() const {
    return NodeType::FromNodeOrNull(GetPreviousContent());
  }
  template <typename EditorDOMPointType>
  EditorDOMPointType AtPreviousContent() const {
    if (nsIContent* previousContent = GetPreviousContent()) {
      return EditorDOMPointType(previousContent);
    }
    return EditorDOMPointType();
  }

  /**
   * GetNextContent() returns next content node at the split point.
   */
  MOZ_KNOWN_LIVE nsIContent* GetNextContent() const {
    if (mGivenSplitPoint.IsSet()) {
      return !mGivenSplitPoint.IsEndOfContainer() ? mGivenSplitPoint.GetChild()
                                                  : nullptr;
    }
    return mNextNode;
  }
  template <typename NodeType>
  MOZ_KNOWN_LIVE NodeType* GetNextContentAs() const {
    return NodeType::FromNodeOrNull(GetNextContent());
  }
  template <typename EditorDOMPointType>
  EditorDOMPointType AtNextContent() const {
    if (nsIContent* nextContent = GetNextContent()) {
      return EditorDOMPointType(nextContent);
    }
    return EditorDOMPointType();
  }

  /**
   * Returns new content node which is created at splitting a node.  I.e., this
   * returns nullptr if no node was split.
   */
  MOZ_KNOWN_LIVE nsIContent* GetNewContent() const {
    if (!DidSplit()) {
      return nullptr;
    }
    return mNextNode;
  }
  template <typename NodeType>
  MOZ_KNOWN_LIVE NodeType* GetNewContentAs() const {
    return NodeType::FromNodeOrNull(GetNewContent());
  }
  template <typename EditorDOMPointType>
  EditorDOMPointType AtNewContent() const {
    if (nsIContent* newContent = GetNewContent()) {
      return EditorDOMPointType(newContent);
    }
    return EditorDOMPointType();
  }

  /**
   * Returns original content node which is (or is just tried to be) split.
   */
  MOZ_KNOWN_LIVE nsIContent* GetOriginalContent() const {
    if (mGivenSplitPoint.IsSet()) {
      // Different from previous/next content, if the creator didn't split a
      // node, the container of the split point is the original node.
      return mGivenSplitPoint.GetContainerAs<nsIContent>();
    }
    return mPreviousNode ? mPreviousNode : mNextNode;
  }
  template <typename NodeType>
  MOZ_KNOWN_LIVE NodeType* GetOriginalContentAs() const {
    return NodeType::FromNodeOrNull(GetOriginalContent());
  }
  template <typename EditorDOMPointType>
  EditorDOMPointType AtOriginalContent() const {
    if (nsIContent* originalContent = GetOriginalContent()) {
      return EditorDOMPointType(originalContent);
    }
    return EditorDOMPointType();
  }

  /**
   * AtSplitPoint() returns the split point in the container.
   * HTMLEditor::CreateAndInsertElement() or something similar methods.
   */
  template <typename EditorDOMPointType>
  EditorDOMPointType AtSplitPoint() const {
    if (mGivenSplitPoint.IsSet()) {
      return mGivenSplitPoint.To<EditorDOMPointType>();
    }
    if (!mPreviousNode) {
      return EditorDOMPointType(mNextNode);
    }
    return EditorDOMPointType::After(mPreviousNode);
  }

  SplitNodeResult(const SplitNodeResult&) = delete;
  SplitNodeResult& operator=(const SplitNodeResult&) = delete;
  SplitNodeResult(SplitNodeResult&&) = default;
  SplitNodeResult& operator=(SplitNodeResult&&) = default;

  /**
   * This constructor should be used for setting specific caret point instead of
   * aSplitResult's one.
   */
  SplitNodeResult(SplitNodeResult&& aSplitResult,
                  const EditorDOMPoint& aNewCaretPoint)
      : SplitNodeResult(std::move(aSplitResult)) {
    SetCaretPoint(aNewCaretPoint);
  }
  SplitNodeResult(SplitNodeResult&& aSplitResult,
                  EditorDOMPoint&& aNewCaretPoint)
      : SplitNodeResult(std::move(aSplitResult)) {
    SetCaretPoint(std::move(aNewCaretPoint));
  }

  /**
   * This constructor shouldn't be used by anybody except methods which
   * use this as result when it succeeds.
   *
   * @param aNewNode    The node which is newly created.
   * @param aSplitNode  The node which was split.
   * @param aNewCaretPoint
   *                    An optional new caret position.  If this is omitted,
   *                    the point between new node and split node will be
   *                    suggested.
   */
  SplitNodeResult(nsIContent& aNewNode, nsIContent& aSplitNode,
                  const Maybe<EditorDOMPoint>& aNewCaretPoint = Nothing())
      : CaretPoint(aNewCaretPoint.isSome()
                       ? aNewCaretPoint.ref()
                       : EditorDOMPoint::AtEndOf(aSplitNode)),
        mPreviousNode(&aSplitNode),
        mNextNode(&aNewNode) {}

  SplitNodeResult ToHandledResult() const {
    CaretPointHandled();
    SplitNodeResult result;
    result.mPreviousNode = GetPreviousContent();
    result.mNextNode = GetNextContent();
    MOZ_DIAGNOSTIC_ASSERT(result.Handled());
    // Don't recompute the caret position because in this case, split has not
    // occurred yet.  In the case,  the caller shouldn't need to update
    // selection.
    result.SetCaretPoint(CaretPointRef());
    return result;
  }

  /**
   * The following factory methods creates a SplitNodeResult instance for the
   * special cases.
   *
   * @param aDeeperSplitNodeResult
   *                    If the splitter has already split a child or a
   *                    descendant of the latest split node, the split node
   *                    result should be specified.
   */
  static inline SplitNodeResult HandledButDidNotSplitDueToEndOfContainer(
      nsIContent& aNotSplitNode,
      const SplitNodeResult* aDeeperSplitNodeResult = nullptr) {
    SplitNodeResult result;
    result.mPreviousNode = &aNotSplitNode;
    // Caret should be put at the last split point instead of current node.
    if (aDeeperSplitNodeResult) {
      result.SetCaretPoint(aDeeperSplitNodeResult->CaretPointRef());
      aDeeperSplitNodeResult->IgnoreCaretPointSuggestion();
    }
    return result;
  }

  static inline SplitNodeResult HandledButDidNotSplitDueToStartOfContainer(
      nsIContent& aNotSplitNode,
      const SplitNodeResult* aDeeperSplitNodeResult = nullptr) {
    SplitNodeResult result;
    result.mNextNode = &aNotSplitNode;
    // Caret should be put at the last split point instead of current node.
    if (aDeeperSplitNodeResult) {
      result.SetCaretPoint(aDeeperSplitNodeResult->CaretPointRef());
      aDeeperSplitNodeResult->IgnoreCaretPointSuggestion();
    }
    return result;
  }

  template <typename PT, typename CT>
  static inline SplitNodeResult NotHandled(
      const EditorDOMPointBase<PT, CT>& aGivenSplitPoint,
      const SplitNodeResult* aDeeperSplitNodeResult = nullptr) {
    SplitNodeResult result;
    result.mGivenSplitPoint = aGivenSplitPoint;
    // Caret should be put at the last split point instead of current node.
    if (aDeeperSplitNodeResult) {
      result.SetCaretPoint(aDeeperSplitNodeResult->CaretPointRef());
      aDeeperSplitNodeResult->IgnoreCaretPointSuggestion();
    }
    return result;
  }

  /**
   * Returns aSplitNodeResult as-is unless it didn't split a node but
   * aDeeperSplitNodeResult has already split a child or a descendant and has a
   * valid point to put caret around there.  In the case, this return
   * aSplitNodeResult which suggests a caret position around the last split
   * point.
   */
  static inline SplitNodeResult MergeWithDeeperSplitNodeResult(
      SplitNodeResult&& aSplitNodeResult,
      const SplitNodeResult& aDeeperSplitNodeResult) {
    aSplitNodeResult.UnmarkAsHandledCaretPoint();
    aDeeperSplitNodeResult.IgnoreCaretPointSuggestion();
    if (aSplitNodeResult.DidSplit() ||
        !aDeeperSplitNodeResult.HasCaretPointSuggestion()) {
      return std::move(aSplitNodeResult);
    }
    SplitNodeResult result(std::move(aSplitNodeResult));
    result.SetCaretPoint(aDeeperSplitNodeResult.CaretPointRef());
    return result;
  }

#ifdef DEBUG
  ~SplitNodeResult() {
    MOZ_ASSERT(!HasCaretPointSuggestion() || CaretPointHandled());
  }
#endif

 private:
  SplitNodeResult() = default;

  // When methods which return this class split some nodes actually, they
  // need to set a set of left node and right node to this class.  However,
  // one or both of them may be moved or removed by mutation observer.
  // In such case, we cannot represent the point with EditorDOMPoint since
  // it requires current container node.  Therefore, we need to use
  // nsCOMPtr<nsIContent> here instead.
  nsCOMPtr<nsIContent> mPreviousNode;
  nsCOMPtr<nsIContent> mNextNode;

  // Methods which return this class may not split any nodes actually.  Then,
  // they may want to return given split point as is since such behavior makes
  // their callers simpler.  In this case, the point may be in a text node
  // which cannot be represented as a node.  Therefore, we need EditorDOMPoint
  // for representing the point.
  EditorDOMPoint mGivenSplitPoint;
};

/*****************************************************************************
 * JoinNodesResult is a simple class for HTMLEditor::JoinNodesWithTransaction().
 * This makes the callers' code easier to read.
 *****************************************************************************/
class MOZ_STACK_CLASS JoinNodesResult final {
 public:
  MOZ_KNOWN_LIVE nsIContent* ExistingContent() const {
    return mJoinedPoint.ContainerAs<nsIContent>();
  }
  template <typename EditorDOMPointType>
  EditorDOMPointType AtExistingContent() const {
    return EditorDOMPointType(mJoinedPoint.ContainerAs<nsIContent>());
  }

  MOZ_KNOWN_LIVE nsIContent* RemovedContent() const { return mRemovedContent; }
  template <typename EditorDOMPointType>
  EditorDOMPointType AtRemovedContent() const {
    if (mRemovedContent) {
      return EditorDOMPointType(mRemovedContent);
    }
    return EditorDOMPointType();
  }

  template <typename EditorDOMPointType>
  EditorDOMPointType AtJoinedPoint() const {
    return mJoinedPoint.To<EditorDOMPointType>();
  }

  JoinNodesResult() = delete;

  /**
   * This constructor shouldn't be used by anybody except methods which
   * use this as result when it succeeds.
   *
   * @param aJoinedPoint        First child of right node or first character.
   * @param aRemovedContent     The node which was removed from the parent.
   */
  JoinNodesResult(const EditorDOMPoint& aJoinedPoint,
                  nsIContent& aRemovedContent)
      : mJoinedPoint(aJoinedPoint), mRemovedContent(&aRemovedContent) {
    MOZ_DIAGNOSTIC_ASSERT(aJoinedPoint.IsInContentNode());
  }

  JoinNodesResult(const JoinNodesResult& aOther) = delete;
  JoinNodesResult& operator=(const JoinNodesResult& aOther) = delete;
  JoinNodesResult(JoinNodesResult&& aOther) = default;
  JoinNodesResult& operator=(JoinNodesResult&& aOther) = default;

 private:
  EditorDOMPoint mJoinedPoint;
  MOZ_KNOWN_LIVE nsCOMPtr<nsIContent> mRemovedContent;
};

/*****************************************************************************
 * SplitRangeOffFromNodeResult class is a simple class for methods which split a
 * node at 2 points for making part of the node split off from the node.
 *****************************************************************************/
class MOZ_STACK_CLASS SplitRangeOffFromNodeResult final : public CaretPoint {
 public:
  /**
   * GetLeftContent() returns new created node before the part of quarried out.
   * This may return nullptr if the method didn't split at start edge of
   * the node.
   */
  MOZ_KNOWN_LIVE nsIContent* GetLeftContent() const { return mLeftContent; }
  template <typename ContentNodeType>
  MOZ_KNOWN_LIVE ContentNodeType* GetLeftContentAs() const {
    return ContentNodeType::FromNodeOrNull(GetLeftContent());
  }
  constexpr nsCOMPtr<nsIContent>&& UnwrapLeftContent() {
    mMovedContent = true;
    return std::move(mLeftContent);
  }

  /**
   * GetMiddleContent() returns new created node between left node and right
   * node.  I.e., this is quarried out from the node.  This may return nullptr
   * if the method unwrapped the middle node.
   */
  MOZ_KNOWN_LIVE nsIContent* GetMiddleContent() const { return mMiddleContent; }
  template <typename ContentNodeType>
  MOZ_KNOWN_LIVE ContentNodeType* GetMiddleContentAs() const {
    return ContentNodeType::FromNodeOrNull(GetMiddleContent());
  }
  constexpr nsCOMPtr<nsIContent>&& UnwrapMiddleContent() {
    mMovedContent = true;
    return std::move(mMiddleContent);
  }

  /**
   * GetRightContent() returns the right node after the part of quarried out.
   * This may return nullptr it the method didn't split at end edge of the
   * node.
   */
  MOZ_KNOWN_LIVE nsIContent* GetRightContent() const { return mRightContent; }
  template <typename ContentNodeType>
  MOZ_KNOWN_LIVE ContentNodeType* GetRightContentAs() const {
    return ContentNodeType::FromNodeOrNull(GetRightContent());
  }
  constexpr nsCOMPtr<nsIContent>&& UnwrapRightContent() {
    mMovedContent = true;
    return std::move(mRightContent);
  }

  /**
   * GetLeftmostContent() returns the leftmost content after trying to
   * split twice.  If the node was not split, this returns the original node.
   */
  MOZ_KNOWN_LIVE nsIContent* GetLeftmostContent() const {
    MOZ_ASSERT(!mMovedContent);
    return mLeftContent ? mLeftContent
                        : (mMiddleContent ? mMiddleContent : mRightContent);
  }
  template <typename ContentNodeType>
  MOZ_KNOWN_LIVE ContentNodeType* GetLeftmostContentAs() const {
    return ContentNodeType::FromNodeOrNull(GetLeftmostContent());
  }

  /**
   * GetRightmostContent() returns the rightmost content after trying to
   * split twice.  If the node was not split, this returns the original node.
   */
  MOZ_KNOWN_LIVE nsIContent* GetRightmostContent() const {
    MOZ_ASSERT(!mMovedContent);
    return mRightContent ? mRightContent
                         : (mMiddleContent ? mMiddleContent : mLeftContent);
  }
  template <typename ContentNodeType>
  MOZ_KNOWN_LIVE ContentNodeType* GetRightmostContentAs() const {
    return ContentNodeType::FromNodeOrNull(GetRightmostContent());
  }

  [[nodiscard]] bool DidSplit() const { return mLeftContent || mRightContent; }

  SplitRangeOffFromNodeResult() = delete;

  SplitRangeOffFromNodeResult(nsIContent* aLeftContent,
                              nsIContent* aMiddleContent,
                              nsIContent* aRightContent)
      : mLeftContent(aLeftContent),
        mMiddleContent(aMiddleContent),
        mRightContent(aRightContent) {}

  SplitRangeOffFromNodeResult(nsIContent* aLeftContent,
                              nsIContent* aMiddleContent,
                              nsIContent* aRightContent,
                              EditorDOMPoint&& aPointToPutCaret)
      : CaretPoint(std::move(aPointToPutCaret)),
        mLeftContent(aLeftContent),
        mMiddleContent(aMiddleContent),
        mRightContent(aRightContent) {}

  SplitRangeOffFromNodeResult(const SplitRangeOffFromNodeResult& aOther) =
      delete;
  SplitRangeOffFromNodeResult& operator=(
      const SplitRangeOffFromNodeResult& aOther) = delete;
  SplitRangeOffFromNodeResult(SplitRangeOffFromNodeResult&& aOther) noexcept
      : CaretPoint(aOther.UnwrapCaretPoint()),
        mLeftContent(std::move(aOther.mLeftContent)),
        mMiddleContent(std::move(aOther.mMiddleContent)),
        mRightContent(std::move(aOther.mRightContent)) {
    MOZ_ASSERT(!aOther.mMovedContent);
  }
  SplitRangeOffFromNodeResult& operator=(SplitRangeOffFromNodeResult&& aOther) =
      delete;  // due to bug 1792638

#ifdef DEBUG
  ~SplitRangeOffFromNodeResult() {
    MOZ_ASSERT(!HasCaretPointSuggestion() || CaretPointHandled());
  }
#endif

 private:
  MOZ_KNOWN_LIVE nsCOMPtr<nsIContent> mLeftContent;
  MOZ_KNOWN_LIVE nsCOMPtr<nsIContent> mMiddleContent;
  MOZ_KNOWN_LIVE nsCOMPtr<nsIContent> mRightContent;

  bool mutable mMovedContent = false;
};

/*****************************************************************************
 * SplitRangeOffResult class is a simple class for methods which splits
 * specific ancestor elements at 2 DOM points.
 *****************************************************************************/
class MOZ_STACK_CLASS SplitRangeOffResult final : public CaretPoint {
 public:
  constexpr bool Handled() const { return mHandled; }

  /**
   * The start boundary is at the right of split at split point.  The end
   * boundary is at right node of split at end point, i.e., the end boundary
   * points out of the range to have been split off.
   */
  constexpr const EditorDOMRange& RangeRef() const { return mRange; }

  SplitRangeOffResult() = delete;

  /**
   * Constructor for success case.
   *
   * @param aTrackedRangeStart          The range whose start is at topmost
   *                                    right node child at start point if
   *                                    actually split there, or at the point
   *                                    to be tried to split, and whose end is
   *                                    at topmost right node child at end point
   *                                    if actually split there, or at the point
   *                                    to be tried to split.  Note that if the
   *                                    method allows to run script after
   *                                    splitting the range boundaries, they
   *                                    should be tracked with
   *                                    AutoTrackDOMRange.
   * @param aSplitNodeResultAtStart     Raw split node result at start point.
   * @param aSplitNodeResultAtEnd       Raw split node result at start point.
   */
  SplitRangeOffResult(EditorDOMRange&& aTrackedRange,
                      SplitNodeResult&& aSplitNodeResultAtStart,
                      SplitNodeResult&& aSplitNodeResultAtEnd)
      : mRange(std::move(aTrackedRange)),
        mHandled(aSplitNodeResultAtStart.Handled() ||
                 aSplitNodeResultAtEnd.Handled()) {
    MOZ_ASSERT(mRange.StartRef().IsSet());
    MOZ_ASSERT(mRange.EndRef().IsSet());
    // The given results are created for creating this instance so that the
    // caller may not need to handle with them.  For making who taking the
    // responsible clearer, we should move them into this constructor.
    EditorDOMPoint pointToPutCaret;
    SplitNodeResult splitNodeResultAtStart(std::move(aSplitNodeResultAtStart));
    SplitNodeResult splitNodeResultAtEnd(std::move(aSplitNodeResultAtEnd));
    splitNodeResultAtStart.MoveCaretPointTo(
        pointToPutCaret, {SuggestCaret::OnlyIfHasSuggestion});
    splitNodeResultAtEnd.MoveCaretPointTo(pointToPutCaret,
                                          {SuggestCaret::OnlyIfHasSuggestion});
    SetCaretPoint(std::move(pointToPutCaret));
  }

  SplitRangeOffResult(const SplitRangeOffResult& aOther) = delete;
  SplitRangeOffResult& operator=(const SplitRangeOffResult& aOther) = delete;
  SplitRangeOffResult(SplitRangeOffResult&& aOther) = default;
  SplitRangeOffResult& operator=(SplitRangeOffResult&& aOther) = default;

 private:
  EditorDOMRange mRange;

  // If you need to store previous and/or next node at start/end point,
  // you might be able to use `SplitNodeResult::GetPreviousNode()` etc in the
  // constructor only when `SplitNodeResult::Handled()` returns true.  But
  // the node might have gone with another DOM tree mutation.  So, be careful
  // if you do it.

  bool mHandled;
};

/******************************************************************************
 * DOM tree iterators
 *****************************************************************************/

class MOZ_RAII DOMIterator {
 public:
  explicit DOMIterator();
  explicit DOMIterator(nsINode& aNode);
  virtual ~DOMIterator() = default;

  nsresult Init(nsRange& aRange);
  nsresult Init(const RawRangeBoundary& aStartRef,
                const RawRangeBoundary& aEndRef);

  template <class NodeClass>
  void AppendAllNodesToArray(
      nsTArray<OwningNonNull<NodeClass>>& aArrayOfNodes) const;

  /**
   * AppendNodesToArray() calls aFunctor before appending found node to
   * aArrayOfNodes.  If aFunctor returns false, the node will be ignored.
   * You can use aClosure instead of capturing something with lambda.
   * Note that aNode is guaranteed that it's an instance of NodeClass
   * or its sub-class.
   * XXX If we can make type of aNode templated without std::function,
   *     it'd be better, though.
   */
  using BoolFunctor = bool (*)(nsINode& aNode, void* aClosure);
  template <class NodeClass>
  void AppendNodesToArray(BoolFunctor aFunctor,
                          nsTArray<OwningNonNull<NodeClass>>& aArrayOfNodes,
                          void* aClosure = nullptr) const;

 protected:
  SafeContentIteratorBase* mIter;
  PostContentIterator mPostOrderIter;
};

class MOZ_RAII DOMSubtreeIterator final : public DOMIterator {
 public:
  explicit DOMSubtreeIterator();
  virtual ~DOMSubtreeIterator() = default;

  nsresult Init(nsRange& aRange);

 private:
  ContentSubtreeIterator mSubtreeIter;
  explicit DOMSubtreeIterator(nsINode& aNode) = delete;
};

/******************************************************************************
 * ReplaceRangeData
 *
 * This represents range to be replaced and replacing string.
 *****************************************************************************/

template <typename EditorDOMPointType>
class MOZ_STACK_CLASS ReplaceRangeDataBase final {
 public:
  ReplaceRangeDataBase() = default;
  template <typename OtherEditorDOMRangeType>
  ReplaceRangeDataBase(const OtherEditorDOMRangeType& aRange,
                       const nsAString& aReplaceString)
      : mRange(aRange), mReplaceString(aReplaceString) {}
  template <typename StartPointType, typename EndPointType>
  ReplaceRangeDataBase(const StartPointType& aStart, const EndPointType& aEnd,
                       const nsAString& aReplaceString)
      : mRange(aStart, aEnd), mReplaceString(aReplaceString) {}

  bool IsSet() const { return mRange.IsPositioned(); }
  bool IsSetAndValid() const { return mRange.IsPositionedAndValid(); }
  bool Collapsed() const { return mRange.Collapsed(); }
  bool HasReplaceString() const { return !mReplaceString.IsEmpty(); }
  const EditorDOMPointType& StartRef() const { return mRange.StartRef(); }
  const EditorDOMPointType& EndRef() const { return mRange.EndRef(); }
  const EditorDOMRangeBase<EditorDOMPointType>& RangeRef() const {
    return mRange;
  }
  const nsString& ReplaceStringRef() const { return mReplaceString; }

  template <typename PointType>
  MOZ_NEVER_INLINE_DEBUG void SetStart(const PointType& aStart) {
    mRange.SetStart(aStart);
  }
  template <typename PointType>
  MOZ_NEVER_INLINE_DEBUG void SetEnd(const PointType& aEnd) {
    mRange.SetEnd(aEnd);
  }
  template <typename StartPointType, typename EndPointType>
  MOZ_NEVER_INLINE_DEBUG void SetStartAndEnd(const StartPointType& aStart,
                                             const EndPointType& aEnd) {
    mRange.SetRange(aStart, aEnd);
  }
  template <typename OtherEditorDOMRangeType>
  MOZ_NEVER_INLINE_DEBUG void SetRange(const OtherEditorDOMRangeType& aRange) {
    mRange = aRange;
  }
  void SetReplaceString(const nsAString& aReplaceString) {
    mReplaceString = aReplaceString;
  }
  template <typename StartPointType, typename EndPointType>
  MOZ_NEVER_INLINE_DEBUG void SetStartAndEnd(const StartPointType& aStart,
                                             const EndPointType& aEnd,
                                             const nsAString& aReplaceString) {
    SetStartAndEnd(aStart, aEnd);
    SetReplaceString(aReplaceString);
  }
  template <typename OtherEditorDOMRangeType>
  MOZ_NEVER_INLINE_DEBUG void Set(const OtherEditorDOMRangeType& aRange,
                                  const nsAString& aReplaceString) {
    SetRange(aRange);
    SetReplaceString(aReplaceString);
  }

 private:
  EditorDOMRangeBase<EditorDOMPointType> mRange;
  // This string may be used with ReplaceTextTransaction.  Therefore, for
  // avoiding memory copy, we should store it with nsString rather than
  // nsAutoString.
  nsString mReplaceString;
};

/******************************************************************************
 * EditorElementStyle represents a generic style of element
 ******************************************************************************/

class MOZ_STACK_CLASS EditorElementStyle {
 public:
#define DEFINE_FACTORY(aName, aAttr)            \
  constexpr static EditorElementStyle aName() { \
    return EditorElementStyle(*(aAttr));        \
  }

  // text-align, caption-side, a pair of margin-left and margin-right
  DEFINE_FACTORY(Align, nsGkAtoms::align)
  // background-color
  DEFINE_FACTORY(BGColor, nsGkAtoms::bgcolor)
  // background-image
  DEFINE_FACTORY(Background, nsGkAtoms::background)
  // border
  DEFINE_FACTORY(Border, nsGkAtoms::border)
  // height
  DEFINE_FACTORY(Height, nsGkAtoms::height)
  // color
  DEFINE_FACTORY(Text, nsGkAtoms::text)
  // list-style-type
  DEFINE_FACTORY(Type, nsGkAtoms::type)
  // vertical-align
  DEFINE_FACTORY(VAlign, nsGkAtoms::valign)
  // width
  DEFINE_FACTORY(Width, nsGkAtoms::width)

  static EditorElementStyle Create(const nsAtom& aAttribute) {
    MOZ_DIAGNOSTIC_ASSERT(IsHTMLStyle(&aAttribute));
    return EditorElementStyle(*aAttribute.AsStatic());
  }

  [[nodiscard]] static bool IsHTMLStyle(const nsAtom* aAttribute) {
    return aAttribute == nsGkAtoms::align || aAttribute == nsGkAtoms::bgcolor ||
           aAttribute == nsGkAtoms::background ||
           aAttribute == nsGkAtoms::border || aAttribute == nsGkAtoms::height ||
           aAttribute == nsGkAtoms::text || aAttribute == nsGkAtoms::type ||
           aAttribute == nsGkAtoms::valign || aAttribute == nsGkAtoms::width;
  }

  /**
   * Returns true if the style can be represented by CSS and it's possible to
   * apply the style with CSS.
   */
  [[nodiscard]] bool IsCSSSettable(const nsStaticAtom& aTagName) const;
  [[nodiscard]] bool IsCSSSettable(const dom::Element& aElement) const;

  /**
   * Returns true if the style can be represented by CSS and it's possible to
   * remove the style with CSS.
   */
  [[nodiscard]] bool IsCSSRemovable(const nsStaticAtom& aTagName) const;
  [[nodiscard]] bool IsCSSRemovable(const dom::Element& aElement) const;

  nsStaticAtom* Style() const { return mStyle; }

  [[nodiscard]] bool IsInlineStyle() const { return !mStyle; }
  inline EditorInlineStyle& AsInlineStyle();
  inline const EditorInlineStyle& AsInlineStyle() const;

 protected:
  MOZ_KNOWN_LIVE nsStaticAtom* mStyle = nullptr;
  EditorElementStyle() = default;

 private:
  constexpr explicit EditorElementStyle(const nsStaticAtom& aStyle)
      // Needs const_cast hack here because the this class users may want
      // non-const nsStaticAtom pointer due to bug 1794954
      : mStyle(const_cast<nsStaticAtom*>(&aStyle)) {}
};

/******************************************************************************
 * EditorInlineStyle represents an inline style.
 ******************************************************************************/

struct MOZ_STACK_CLASS EditorInlineStyle : public EditorElementStyle {
  // nullptr if you want to remove all inline styles.
  // Otherwise, one of the presentation tag names which we support in style
  // editor, and there special cases: nsGkAtoms::href means <a href="...">,
  // and nsGkAtoms::name means <a name="...">.
  MOZ_KNOWN_LIVE nsStaticAtom* const mHTMLProperty = nullptr;
  // For some mHTMLProperty values, need to be set to its attribute name.
  // E.g., nsGkAtoms::size and nsGkAtoms::face for nsGkAtoms::font.
  // Otherwise, nullptr.
  // TODO: Once we stop using these structure to wrap selected content nodes
  //       with <a href> elements, we can make this nsStaticAtom*.
  MOZ_KNOWN_LIVE const RefPtr<nsAtom> mAttribute;

  /**
   * Returns true if the style means that all inline styles should be removed.
   */
  [[nodiscard]] bool IsStyleToClearAllInlineStyles() const {
    return !mHTMLProperty;
  }

  /**
   * Returns true if the style is about <a>.
   */
  [[nodiscard]] bool IsStyleOfAnchorElement() const {
    return mHTMLProperty == nsGkAtoms::a || mHTMLProperty == nsGkAtoms::href ||
           mHTMLProperty == nsGkAtoms::name;
  }

  /**
   * Returns true if the style is invertible with CSS.
   */
  [[nodiscard]] bool IsInvertibleWithCSS() const {
    return mHTMLProperty == nsGkAtoms::b;
  }

  /**
   * Returns true if the style can be specified with text-decoration.
   */
  enum class IgnoreSElement { No, Yes };
  [[nodiscard]] bool IsStyleOfTextDecoration(
      IgnoreSElement aIgnoreSElement) const {
    return mHTMLProperty == nsGkAtoms::u ||
           mHTMLProperty == nsGkAtoms::strike ||
           (aIgnoreSElement == IgnoreSElement::No &&
            mHTMLProperty == nsGkAtoms::s);
  }

  /**
   * Returns true if the style can be represented with <font>.
   */
  [[nodiscard]] bool IsStyleOfFontElement() const {
    MOZ_ASSERT_IF(
        mHTMLProperty == nsGkAtoms::font,
        mAttribute == nsGkAtoms::bgcolor || mAttribute == nsGkAtoms::color ||
            mAttribute == nsGkAtoms::face || mAttribute == nsGkAtoms::size);
    return mHTMLProperty == nsGkAtoms::font && mAttribute != nsGkAtoms::bgcolor;
  }

  /**
   * Returns true if the style is font-size or <font size="...">.
   */
  [[nodiscard]] bool IsStyleOfFontSize() const {
    return mHTMLProperty == nsGkAtoms::font && mAttribute == nsGkAtoms::size;
  }

  /**
   * Returns true if the style is conflict with vertical-align even though
   * they are not mapped to vertical-align in the CSS mode.
   */
  [[nodiscard]] bool IsStyleConflictingWithVerticalAlign() const {
    return mHTMLProperty == nsGkAtoms::sup || mHTMLProperty == nsGkAtoms::sub;
  }

  /**
   * If the style has a similar element  which should be removed when applying
   * the style, this retuns an element name.  Otherwise, returns nullptr.
   */
  [[nodiscard]] nsStaticAtom* GetSimilarElementNameAtom() const {
    if (mHTMLProperty == nsGkAtoms::b) {
      return nsGkAtoms::strong;
    }
    if (mHTMLProperty == nsGkAtoms::i) {
      return nsGkAtoms::em;
    }
    if (mHTMLProperty == nsGkAtoms::strike) {
      return nsGkAtoms::s;
    }
    return nullptr;
  }

  /**
   * Returns true if aContent is an HTML element and represents the style.
   */
  [[nodiscard]] bool IsRepresentedBy(const nsIContent& aContent) const;

  /**
   * Returns true if aElement has style attribute and specifies this style.
   *
   * TODO: Make aElement be constant, but it needs to touch CSSEditUtils a lot.
   */
  [[nodiscard]] Result<bool, nsresult> IsSpecifiedBy(
      const HTMLEditor& aHTMLEditor, dom::Element& aElement) const;

  explicit EditorInlineStyle(const nsStaticAtom& aHTMLProperty,
                             nsAtom* aAttribute = nullptr)
      : EditorInlineStyle(aHTMLProperty, aAttribute, HasValue::No) {}
  EditorInlineStyle(const nsStaticAtom& aHTMLProperty,
                    RefPtr<nsAtom>&& aAttribute)
      : EditorInlineStyle(aHTMLProperty, aAttribute, HasValue::No) {}

  /**
   * Returns the instance which means remove all inline styles.
   */
  static EditorInlineStyle RemoveAllStyles() { return EditorInlineStyle(); }

  PendingStyleCache ToPendingStyleCache(nsAString&& aValue) const;

  bool operator==(const EditorInlineStyle& aOther) const {
    return mHTMLProperty == aOther.mHTMLProperty &&
           mAttribute == aOther.mAttribute;
  }

  bool MaybeHasValue() const { return mMaybeHasValue; }
  inline EditorInlineStyleAndValue& AsInlineStyleAndValue();
  inline const EditorInlineStyleAndValue& AsInlineStyleAndValue() const;

 protected:
  const bool mMaybeHasValue = false;

  enum class HasValue { No, Yes };
  EditorInlineStyle(const nsStaticAtom& aHTMLProperty, nsAtom* aAttribute,
                    HasValue aHasValue)
      // Needs const_cast hack here because the struct users may want
      // non-const nsStaticAtom pointer due to bug 1794954
      : mHTMLProperty(const_cast<nsStaticAtom*>(&aHTMLProperty)),
        mAttribute(aAttribute),
        mMaybeHasValue(aHasValue == HasValue::Yes) {}
  EditorInlineStyle(const nsStaticAtom& aHTMLProperty,
                    RefPtr<nsAtom>&& aAttribute, HasValue aHasValue)
      // Needs const_cast hack here because the struct users may want
      // non-const nsStaticAtom pointer due to bug 1794954
      : mHTMLProperty(const_cast<nsStaticAtom*>(&aHTMLProperty)),
        mAttribute(std::move(aAttribute)),
        mMaybeHasValue(aHasValue == HasValue::Yes) {}
  EditorInlineStyle(const EditorInlineStyle& aStyle, HasValue aHasValue)
      : mHTMLProperty(aStyle.mHTMLProperty),
        mAttribute(aStyle.mAttribute),
        mMaybeHasValue(aHasValue == HasValue::Yes) {}

 private:
  EditorInlineStyle() = default;

  using EditorElementStyle::AsInlineStyle;
  using EditorElementStyle::IsInlineStyle;
  using EditorElementStyle::Style;
};

inline EditorInlineStyle& EditorElementStyle::AsInlineStyle() {
  return reinterpret_cast<EditorInlineStyle&>(*this);
}

inline const EditorInlineStyle& EditorElementStyle::AsInlineStyle() const {
  return reinterpret_cast<const EditorInlineStyle&>(*this);
}

/******************************************************************************
 * EditorInlineStyleAndValue represents an inline style and stores its value.
 ******************************************************************************/

struct MOZ_STACK_CLASS EditorInlineStyleAndValue : public EditorInlineStyle {
  // Stores the value of mAttribute.
  nsString const mAttributeValue;

  bool IsStyleToClearAllInlineStyles() const = delete;
  EditorInlineStyleAndValue() = delete;

  explicit EditorInlineStyleAndValue(nsStaticAtom& aHTMLProperty)
      : EditorInlineStyle(aHTMLProperty, nullptr, HasValue::No) {}
  EditorInlineStyleAndValue(nsStaticAtom& aHTMLProperty, nsAtom& aAttribute,
                            const nsAString& aValue)
      : EditorInlineStyle(aHTMLProperty, &aAttribute, HasValue::Yes),
        mAttributeValue(aValue) {}
  EditorInlineStyleAndValue(nsStaticAtom& aHTMLProperty,
                            RefPtr<nsAtom>&& aAttribute,
                            const nsAString& aValue)
      : EditorInlineStyle(aHTMLProperty, std::move(aAttribute), HasValue::Yes),
        mAttributeValue(aValue) {
    MOZ_ASSERT(mAttribute);
  }
  EditorInlineStyleAndValue(nsStaticAtom& aHTMLProperty, nsAtom& aAttribute,
                            nsString&& aValue)
      : EditorInlineStyle(aHTMLProperty, &aAttribute, HasValue::Yes),
        mAttributeValue(std::move(aValue)) {}
  EditorInlineStyleAndValue(nsStaticAtom& aHTMLProperty,
                            RefPtr<nsAtom>&& aAttribute, nsString&& aValue)
      : EditorInlineStyle(aHTMLProperty, std::move(aAttribute), HasValue::Yes),
        mAttributeValue(aValue) {}

  [[nodiscard]] static EditorInlineStyleAndValue ToInvert(
      const EditorInlineStyle& aStyle) {
    MOZ_ASSERT(aStyle.IsInvertibleWithCSS());
    return EditorInlineStyleAndValue(aStyle, u"-moz-editor-invert-value"_ns);
  }

  // mHTMLProperty is never nullptr since all constructors guarantee it.
  // Therefore, hide it and expose its reference instead.
  MOZ_KNOWN_LIVE nsStaticAtom& HTMLPropertyRef() const {
    MOZ_DIAGNOSTIC_ASSERT(mHTMLProperty);
    return *mHTMLProperty;
  }

  [[nodiscard]] bool IsStyleToInvert() const {
    return mAttributeValue.EqualsLiteral(u"-moz-editor-invert-value");
  }

  /**
   * Returns true if this style is representable with HTML.
   */
  [[nodiscard]] bool IsRepresentableWithHTML() const {
    // Use background-color in any elements
    if (mAttribute == nsGkAtoms::bgcolor) {
      return false;
    }
    // Inverting the style means that it's invertible with CSS
    if (IsStyleToInvert()) {
      return false;
    }
    return true;
  }

 private:
  using EditorInlineStyle::mHTMLProperty;

  EditorInlineStyleAndValue(const EditorInlineStyle& aStyle,
                            const nsAString& aValue)
      : EditorInlineStyle(aStyle, HasValue::Yes), mAttributeValue(aValue) {}

  using EditorInlineStyle::AsInlineStyleAndValue;
  using EditorInlineStyle::HasValue;
};

inline EditorInlineStyleAndValue& EditorInlineStyle::AsInlineStyleAndValue() {
  return reinterpret_cast<EditorInlineStyleAndValue&>(*this);
}

inline const EditorInlineStyleAndValue&
EditorInlineStyle::AsInlineStyleAndValue() const {
  return reinterpret_cast<const EditorInlineStyleAndValue&>(*this);
}

}  // namespace mozilla

#endif  // #ifndef HTMLEditHelpers_h
