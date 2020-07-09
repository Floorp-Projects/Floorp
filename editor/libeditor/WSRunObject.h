/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WSRunObject_h
#define WSRunObject_h

#include "HTMLEditUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/EditAction.h"
#include "mozilla/EditorBase.h"
#include "mozilla/EditorDOMPoint.h"  // for EditorDOMPoint
#include "mozilla/HTMLEditor.h"
#include "mozilla/Maybe.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLBRElement.h"
#include "mozilla/dom/Text.h"
#include "nsCOMPtr.h"
#include "nsIContent.h"

namespace mozilla {

// class WSRunObject represents the entire white-space situation
// around a given point.  It collects up a list of nodes that contain
// white-space and categorizes in up to 3 different WSFragments (detailed
// below).  Each WSFragment is a collection of white-space that is
// either all insignificant, or that is significant.  A WSFragment could
// consist of insignificant white-space because it is after a block
// boundary or after a break.  Or it could be insignificant because it
// is before a block.  Or it could be significant because it is
// surrounded by text, or starts and ends with nbsps, etc.

// Throughout I refer to LeadingWS, NormalWS, TrailingWS.  LeadingWS &
// TrailingWS are runs of ascii ws that are insignificant (do not render)
// because they are adjacent to block boundaries, or after a break.  NormalWS is
// ws that does cause soem rendering.  Note that not all the ws in a NormalWS
// run need render.  For example, two ascii spaces surrounded by text on both
// sides will only render as one space (in non-preformatted stlye html), yet
// both spaces count as NormalWS.  Together, they render as the one visible
// space.

class WSRunScanner;

/**
 * WSScanResult is result of ScanNextVisibleNodeOrBlockBoundaryFrom(),
 * ScanPreviousVisibleNodeOrBlockBoundaryFrom(), and their static wrapper
 * methods.  This will have information of found visible content (and its
 * position) or reached block element or topmost editable content at the
 * start of scanner.
 */
class MOZ_STACK_CLASS WSScanResult final {
 private:
  enum class WSType : uint8_t {
    NotInitialized,
    // The run is maybe collapsible white-spaces at start of a hard line.
    LeadingWhiteSpaces,
    // The run is maybe collapsible white-spaces at end of a hard line.
    TrailingWhiteSpaces,
    // Normal (perhaps, meaning visible) white-spaces.
    NormalWhiteSpaces,
    // Normal text, not white-spaces.
    NormalText,
    // Special content such as `<img>`, etc.
    SpecialContent,
    // <br> element.
    BRElement,
    // Other block's boundary (child block of current block, maybe).
    OtherBlockBoundary,
    // Current block's boundary.
    CurrentBlockBoundary,
  };

  friend class WSRunScanner;  // Because of WSType.

 public:
  WSScanResult() = delete;
  MOZ_NEVER_INLINE_DEBUG WSScanResult(nsIContent* aContent, WSType aReason)
      : mContent(aContent), mReason(aReason) {
    AssertIfInvalidData();
  }
  MOZ_NEVER_INLINE_DEBUG WSScanResult(const EditorDOMPoint& aPoint,
                                      WSType aReason)
      : mContent(aPoint.GetContainerAsContent()),
        mOffset(Some(aPoint.Offset())),
        mReason(aReason) {
    AssertIfInvalidData();
  }

  MOZ_NEVER_INLINE_DEBUG void AssertIfInvalidData() const {
#ifdef DEBUG
    MOZ_ASSERT(
        mReason == WSType::NormalText || mReason == WSType::NormalWhiteSpaces ||
        mReason == WSType::BRElement || mReason == WSType::SpecialContent ||
        mReason == WSType::CurrentBlockBoundary ||
        mReason == WSType::OtherBlockBoundary);
    MOZ_ASSERT_IF(
        mReason == WSType::NormalText || mReason == WSType::NormalWhiteSpaces,
        mContent && mContent->IsText());
    MOZ_ASSERT_IF(mReason == WSType::BRElement,
                  mContent && mContent->IsHTMLElement(nsGkAtoms::br));
    MOZ_ASSERT_IF(
        mReason == WSType::SpecialContent,
        mContent && ((mContent->IsText() && !mContent->IsEditable()) ||
                     (!mContent->IsHTMLElement(nsGkAtoms::br) &&
                      !HTMLEditUtils::IsBlockElement(*mContent))));
    MOZ_ASSERT_IF(mReason == WSType::OtherBlockBoundary,
                  mContent && HTMLEditUtils::IsBlockElement(*mContent));
    // If mReason is WSType::CurrentBlockBoundary, mContent can be any content.
    // In most cases, it's current block element which is editable.  However, if
    // there is no editable block parent, this is topmost editable inline
    // content. Additionally, if there is no editable content, this is the
    // container start of scanner and is not editable.
    MOZ_ASSERT_IF(
        mReason == WSType::CurrentBlockBoundary,
        !mContent || !mContent->GetParentElement() ||
            HTMLEditUtils::IsBlockElement(*mContent) ||
            HTMLEditUtils::IsBlockElement(*mContent->GetParentElement()) ||
            !mContent->GetParentElement()->IsEditable());
#endif  // #ifdef DEBUG
  }

  /**
   * GetContent() returns found visible and editable content/element.
   * See MOZ_ASSERT_IF()s in AssertIfInvalidData() for the detail.
   */
  nsIContent* GetContent() const { return mContent; }

  /**
   * The following accessors makes it easier to understand each callers.
   */
  MOZ_NEVER_INLINE_DEBUG dom::Element* ElementPtr() const {
    MOZ_DIAGNOSTIC_ASSERT(mContent->IsElement());
    return mContent->AsElement();
  }
  MOZ_NEVER_INLINE_DEBUG dom::HTMLBRElement* BRElementPtr() const {
    MOZ_DIAGNOSTIC_ASSERT(mContent->IsHTMLElement(nsGkAtoms::br));
    return static_cast<dom::HTMLBRElement*>(mContent.get());
  }
  MOZ_NEVER_INLINE_DEBUG dom::Text* TextPtr() const {
    MOZ_DIAGNOSTIC_ASSERT(mContent->IsText());
    return mContent->AsText();
  }

  /**
   * Returns true if found or reached content is ediable.
   */
  bool IsContentEditable() const { return mContent && mContent->IsEditable(); }

  /**
   *  Offset() returns meaningful value only when InNormalWhiteSpacesOrText()
   * returns true or the scanner reached to start or end of its scanning
   * range and that is same as start or end container which are specified
   * when the scanner is initialized.  If it's result of scanning backward,
   * this offset means before the found point.  Otherwise, i.e., scanning
   * forward, this offset means after the found point.
   */
  MOZ_NEVER_INLINE_DEBUG uint32_t Offset() const {
    NS_ASSERTION(mOffset.isSome(), "Retrieved non-meaningful offset");
    return mOffset.valueOr(0);
  }

  /**
   * Point() and RawPoint() return the position in found visible node or
   * reached block boundary.  So, they return meaningful point only when
   * Offset() returns meaningful value.
   */
  MOZ_NEVER_INLINE_DEBUG EditorDOMPoint Point() const {
    NS_ASSERTION(mOffset.isSome(), "Retrieved non-meaningful point");
    return EditorDOMPoint(mContent, mOffset.valueOr(0));
  }
  MOZ_NEVER_INLINE_DEBUG EditorRawDOMPoint RawPoint() const {
    NS_ASSERTION(mOffset.isSome(), "Retrieved non-meaningful raw point");
    return EditorRawDOMPoint(mContent, mOffset.valueOr(0));
  }

  /**
   * PointAtContent() and RawPointAtContent() return the position of found
   * visible content or reached block element.
   */
  MOZ_NEVER_INLINE_DEBUG EditorDOMPoint PointAtContent() const {
    MOZ_ASSERT(mContent);
    return EditorDOMPoint(mContent);
  }
  MOZ_NEVER_INLINE_DEBUG EditorRawDOMPoint RawPointAtContent() const {
    MOZ_ASSERT(mContent);
    return EditorRawDOMPoint(mContent);
  }

  /**
   * PointAfterContent() and RawPointAfterContent() retrun the position after
   * found visible content or reached block element.
   */
  MOZ_NEVER_INLINE_DEBUG EditorDOMPoint PointAfterContent() const {
    MOZ_ASSERT(mContent);
    return mContent ? EditorDOMPoint::After(mContent) : EditorDOMPoint();
  }
  MOZ_NEVER_INLINE_DEBUG EditorRawDOMPoint RawPointAfterContent() const {
    MOZ_ASSERT(mContent);
    return mContent ? EditorRawDOMPoint::After(mContent) : EditorRawDOMPoint();
  }

  /**
   * The scanner reached <img> or something which is inline and is not a
   * container.
   */
  bool ReachedSpecialContent() const {
    return mReason == WSType::SpecialContent;
  }

  /**
   * The point is in normal white-spaces or text.
   */
  bool InNormalWhiteSpacesOrText() const {
    return mReason == WSType::NormalWhiteSpaces ||
           mReason == WSType::NormalText;
  }

  /**
   * The point is in normal white-spaces.
   */
  bool InNormalWhiteSpaces() const {
    return mReason == WSType::NormalWhiteSpaces;
  }

  /**
   * The point is in normal text.
   */
  bool InNormalText() const { return mReason == WSType::NormalText; }

  /**
   * The scanner reached a <br> element.
   */
  bool ReachedBRElement() const { return mReason == WSType::BRElement; }

  /**
   * The scanner reached a <hr> element.
   */
  bool ReachedHRElement() const {
    return mContent && mContent->IsHTMLElement(nsGkAtoms::hr);
  }

  /**
   * The scanner reached current block boundary or other block element.
   */
  bool ReachedBlockBoundary() const {
    return mReason == WSType::CurrentBlockBoundary ||
           mReason == WSType::OtherBlockBoundary;
  }

  /**
   * The scanner reached current block element boundary.
   */
  bool ReachedCurrentBlockBoundary() const {
    return mReason == WSType::CurrentBlockBoundary;
  }

  /**
   * The scanner reached other block element.
   */
  bool ReachedOtherBlockElement() const {
    return mReason == WSType::OtherBlockBoundary;
  }

  /**
   * The scanner reached something non-text node.
   */
  bool ReachedSomething() const { return !InNormalWhiteSpacesOrText(); }

 private:
  nsCOMPtr<nsIContent> mContent;
  Maybe<uint32_t> mOffset;
  WSType mReason;
};

class MOZ_STACK_CLASS WSRunScanner {
 public:
  using WSType = WSScanResult::WSType;
  /**
   * The constructors take 2 DOM points.  They represent a range in an editing
   * host.  aScanEndPoint (aScanEndNode and aScanEndOffset) must be later
   * point from aScanStartPoint (aScanStartNode and aScanStartOffset).
   * The end point is currently used only with InsertText().  Therefore,
   * currently, this assumes that the range does not cross block boundary.
   *
   * Actually, WSRunScanner scans white-space type at mScanStartPoint position
   * only.
   *
   * If you use aScanEndPoint newly, please test enough.
   */
  template <typename PT, typename CT>
  WSRunScanner(const HTMLEditor* aHTMLEditor,
               const EditorDOMPointBase<PT, CT>& aScanStartPoint,
               const EditorDOMPointBase<PT, CT>& aScanEndPoint);
  template <typename PT, typename CT>
  WSRunScanner(const HTMLEditor* aHTMLEditor,
               const EditorDOMPointBase<PT, CT>& aScanStartPoint)
      : WSRunScanner(aHTMLEditor, aScanStartPoint, aScanStartPoint) {}
  WSRunScanner(const HTMLEditor* aHTMLEditor, nsINode* aScanStartNode,
               int32_t aScanStartOffset, nsINode* aScanEndNode,
               int32_t aScanEndOffset)
      : WSRunScanner(aHTMLEditor,
                     EditorRawDOMPoint(aScanStartNode, aScanStartOffset),
                     EditorRawDOMPoint(aScanEndNode, aScanEndOffset)) {}
  WSRunScanner(const HTMLEditor* aHTMLEditor, nsINode* aScanStartNode,
               int32_t aScanStartOffset)
      : WSRunScanner(aHTMLEditor,
                     EditorRawDOMPoint(aScanStartNode, aScanStartOffset),
                     EditorRawDOMPoint(aScanStartNode, aScanStartOffset)) {}

  // ScanNextVisibleNodeOrBlockBoundaryForwardFrom() returns the first visible
  // node after aPoint.  If there is no visible nodes after aPoint, returns
  // topmost editable inline ancestor at end of current block.  See comments
  // around WSScanResult for the detail.
  template <typename PT, typename CT>
  WSScanResult ScanNextVisibleNodeOrBlockBoundaryFrom(
      const EditorDOMPointBase<PT, CT>& aPoint) const;
  template <typename PT, typename CT>
  static WSScanResult ScanNextVisibleNodeOrBlockBoundary(
      const HTMLEditor& aHTMLEditor, const EditorDOMPointBase<PT, CT>& aPoint) {
    return WSRunScanner(&aHTMLEditor, aPoint)
        .ScanNextVisibleNodeOrBlockBoundaryFrom(aPoint);
  }

  // ScanPreviousVisibleNodeOrBlockBoundaryFrom() returns the first visible node
  // before aPoint. If there is no visible nodes before aPoint, returns topmost
  // editable inline ancestor at start of current block.  See comments around
  // WSScanResult for the detail.
  template <typename PT, typename CT>
  WSScanResult ScanPreviousVisibleNodeOrBlockBoundaryFrom(
      const EditorDOMPointBase<PT, CT>& aPoint) const;
  template <typename PT, typename CT>
  static WSScanResult ScanPreviousVisibleNodeOrBlockBoundary(
      const HTMLEditor& aHTMLEditor, const EditorDOMPointBase<PT, CT>& aPoint) {
    return WSRunScanner(&aHTMLEditor, aPoint)
        .ScanPreviousVisibleNodeOrBlockBoundaryFrom(aPoint);
  }

  /**
   * GetInclusiveNextEditableCharPoint() returns a point in a text node which
   * is at current editable character or next editable character if aPoint
   * does not points an editable character.
   */
  template <typename PT, typename CT>
  static EditorDOMPointInText GetInclusiveNextEditableCharPoint(
      const HTMLEditor& aHTMLEditor, const EditorDOMPointBase<PT, CT>& aPoint) {
    if (aPoint.IsInTextNode() && !aPoint.IsEndOfContainer() &&
        HTMLEditUtils::IsSimplyEditableNode(*aPoint.ContainerAsText())) {
      return EditorDOMPointInText(aPoint.ContainerAsText(), aPoint.Offset());
    }
    WSRunScanner scanner(&aHTMLEditor, aPoint);
    return scanner.GetInclusiveNextEditableCharPoint(aPoint);
  }

  /**
   * GetPreviousEditableCharPoint() returns a point in a text node which
   * is at previous editable character.
   */
  template <typename PT, typename CT>
  static EditorDOMPointInText GetPreviousEditableCharPoint(
      const HTMLEditor& aHTMLEditor, const EditorDOMPointBase<PT, CT>& aPoint) {
    if (aPoint.IsInTextNode() && !aPoint.IsStartOfContainer() &&
        HTMLEditUtils::IsSimplyEditableNode(*aPoint.ContainerAsText())) {
      return EditorDOMPointInText(aPoint.ContainerAsText(),
                                  aPoint.Offset() - 1);
    }
    WSRunScanner scanner(&aHTMLEditor, aPoint);
    return scanner.GetPreviousEditableCharPoint(aPoint);
  }

  /**
   * GetStartReasonContent() and GetEndReasonContent() return a node which
   * was found by scanning from mScanStartPoint backward or mScanEndPoint
   * forward.  If there was white-spaces or text from the point, returns the
   * text node.  Otherwise, returns an element which is explained by the
   * following methods.  Note that when the reason is
   * WSType::CurrentBlockBoundary, In most cases, it's current block element
   * which is editable, but also may be non-element and/or non-editable.  See
   * MOZ_ASSERT_IF()s in WSScanResult::AssertIfInvalidData() for the detail.
   */
  nsIContent* GetStartReasonContent() const {
    return mStart.GetReasonContent();
  }
  nsIContent* GetEndReasonContent() const { return mEnd.GetReasonContent(); }

  bool StartsFromNormalText() const { return mStart.IsNormalText(); }
  bool StartsFromSpecialContent() const { return mStart.IsSpecialContent(); }
  bool StartsFromBRElement() const { return mStart.IsBRElement(); }
  bool StartsFromCurrentBlockBoundary() const {
    return mStart.IsCurrentBlockBoundary();
  }
  bool StartsFromOtherBlockElement() const {
    return mStart.IsOtherBlockBoundary();
  }
  bool StartsFromBlockBoundary() const { return mStart.IsBlockBoundary(); }
  bool StartsFromHardLineBreak() const { return mStart.IsHardLineBreak(); }
  bool EndsByNormalText() const { return mEnd.IsNormalText(); }
  bool EndsBySpecialContent() const { return mEnd.IsSpecialContent(); }
  bool EndsByBRElement() const { return mEnd.IsBRElement(); }
  bool EndsByCurrentBlockBoundary() const {
    return mEnd.IsCurrentBlockBoundary();
  }
  bool EndsByOtherBlockElement() const { return mEnd.IsOtherBlockBoundary(); }
  bool EndsByBlockBoundary() const { return mEnd.IsBlockBoundary(); }

  MOZ_NEVER_INLINE_DEBUG dom::Element* StartReasonOtherBlockElementPtr() const {
    return mStart.OtherBlockElementPtr();
  }
  MOZ_NEVER_INLINE_DEBUG dom::HTMLBRElement* StartReasonBRElementPtr() const {
    return mStart.BRElementPtr();
  }
  MOZ_NEVER_INLINE_DEBUG dom::Element* EndReasonOtherBlockElementPtr() const {
    return mEnd.OtherBlockElementPtr();
  }
  MOZ_NEVER_INLINE_DEBUG dom::HTMLBRElement* EndReasonBRElementPtr() const {
    return mEnd.BRElementPtr();
  }

  /**
   * Active editing host when this instance is created.
   */
  dom::Element* GetEditingHost() const { return mEditingHost; }

 protected:
  using EditorType = EditorBase::EditorType;

  // WSFragment represents a single run of ws (all leadingws, or all normalws,
  // or all trailingws, or all leading+trailingws).  Note that this single run
  // may still span multiple nodes.
  struct MOZ_STACK_CLASS WSFragment final {
    nsCOMPtr<nsINode> mStartNode;  // node where ws run starts
    nsCOMPtr<nsINode> mEndNode;    // node where ws run ends
    int32_t mStartOffset;          // offset where ws run starts
    int32_t mEndOffset;            // offset where ws run ends

    WSFragment()
        : mStartOffset(0),
          mEndOffset(0),
          mLeftWSType(WSType::NotInitialized),
          mRightWSType(WSType::NotInitialized),
          mIsVisible(Visible::No),
          mIsStartOfHardLine(StartOfHardLine::No),
          mIsEndOfHardLine(EndOfHardLine::No) {}

    EditorDOMPoint StartPoint() const {
      return EditorDOMPoint(mStartNode, mStartOffset);
    }
    EditorDOMPoint EndPoint() const {
      return EditorDOMPoint(mEndNode, mEndOffset);
    }
    EditorRawDOMPoint RawStartPoint() const {
      return EditorRawDOMPoint(mStartNode, mStartOffset);
    }
    EditorRawDOMPoint RawEndPoint() const {
      return EditorRawDOMPoint(mEndNode, mEndOffset);
    }

    enum class Visible : bool { No, Yes };
    enum class StartOfHardLine : bool { No, Yes };
    enum class EndOfHardLine : bool { No, Yes };

    /**
     * Information about this fragment.
     * XXX "Visible" might be wrong in some situation, but not sure.
     */
    void MarkAsVisible() { mIsVisible = Visible::Yes; }
    void MarkAsStartOfHardLine() { mIsStartOfHardLine = StartOfHardLine::Yes; }
    void MarkAsEndOfHardLine() { mIsEndOfHardLine = EndOfHardLine::Yes; }
    bool IsVisible() const { return mIsVisible == Visible::Yes; }
    bool IsStartOfHardLine() const {
      return mIsStartOfHardLine == StartOfHardLine::Yes;
    }
    bool IsMiddleOfHardLine() const {
      return !IsStartOfHardLine() && !IsEndOfHardLine();
    }
    bool IsEndOfHardLine() const {
      return mIsEndOfHardLine == EndOfHardLine::Yes;
    }
    bool IsVisibleAndMiddleOfHardLine() const {
      return IsVisible() && IsMiddleOfHardLine();
    }

    /**
     * Information why this fragments starts from (i.e., this indicates the
     * previous content type of the fragment).
     */
    void SetStartFrom(WSType aLeftWSType) { mLeftWSType = aLeftWSType; }
    void SetStartFromLeadingWhiteSpaces() {
      mLeftWSType = WSType::LeadingWhiteSpaces;
    }
    void SetStartFromNormalWhiteSpaces() {
      mLeftWSType = WSType::NormalWhiteSpaces;
    }
    bool StartsFromNormalText() const {
      return mLeftWSType == WSType::NormalText;
    }
    bool StartsFromSpecialContent() const {
      return mLeftWSType == WSType::SpecialContent;
    }

    /**
     * Information why this fragments ends by (i.e., this indicates the
     * next content type of the fragment).
     */
    void SetEndBy(WSType aRightWSType) { mRightWSType = aRightWSType; }
    void SetEndByNormalWiteSpaces() {
      mRightWSType = WSType::NormalWhiteSpaces;
    }
    void SetEndByTrailingWhiteSpaces() {
      mRightWSType = WSType::TrailingWhiteSpaces;
    }
    bool EndsByNormalText() const { return mRightWSType == WSType::NormalText; }
    bool EndsByTrailingWhiteSpaces() const {
      return mRightWSType == WSType::TrailingWhiteSpaces;
    }
    bool EndsBySpecialContent() const {
      return mRightWSType == WSType::SpecialContent;
    }
    bool EndsByBRElement() const { return mRightWSType == WSType::BRElement; }
    bool EndsByBlockBoundary() const {
      return mRightWSType == WSType::CurrentBlockBoundary ||
             mRightWSType == WSType::OtherBlockBoundary;
    }

    /**
     * ComparePointWitWSFragment() compares aPoint with this fragment.
     */
    enum class PointPosition {
      BeforeStartOfFragment,
      StartOfFragment,
      MiddleOfFragment,
      EndOfFragment,
      AfterEndOfFragment,
      NotInSameDOMTree,
    };
    template <typename EditorDOMPointType>
    PointPosition ComparePoint(const EditorDOMPointType& aPoint) const {
      MOZ_ASSERT(aPoint.IsSetAndValid());
      const EditorRawDOMPoint start = RawStartPoint();
      if (start == aPoint) {
        return PointPosition::StartOfFragment;
      }
      const EditorRawDOMPoint end = RawEndPoint();
      if (end == aPoint) {
        return PointPosition::EndOfFragment;
      }
      const bool startIsBeforePoint = start.IsBefore(aPoint);
      const bool pointIsBeforeEnd = aPoint.IsBefore(end);
      if (startIsBeforePoint && pointIsBeforeEnd) {
        return PointPosition::MiddleOfFragment;
      }
      if (startIsBeforePoint) {
        return PointPosition::AfterEndOfFragment;
      }
      if (pointIsBeforeEnd) {
        return PointPosition::BeforeStartOfFragment;
      }
      return PointPosition::NotInSameDOMTree;
    }

   private:
    WSType mLeftWSType, mRightWSType;
    Visible mIsVisible;
    StartOfHardLine mIsStartOfHardLine;
    EndOfHardLine mIsEndOfHardLine;
  };

  using PointPosition = WSFragment::PointPosition;

  using WSFragmentArray = AutoTArray<WSFragment, 3>;
  const WSFragmentArray& WSFragmentArrayRef() const {
    const_cast<WSRunScanner*>(this)->EnsureWSFragments();
    return mFragments;
  }

  /**
   * FindNearestFragment() and FindNearestFragmentIndex() look for a WSFragment
   * which is closest to specified direction from aPoint.
   *
   * @param aPoint      The point to start to look for.
   * @param aForward    true if caller needs to look for a WSFragment after the
   *                    point in the DOM tree.  Otherwise, i.e., before the
   *                    point, false.
   * @return            Found WSFragment instance or index.
   *                    If aForward is true and:
   *                      if aPoint is end of a run, returns next run.
   *                      if aPoint is start of a run, returns the run.
   *                      if aPoint is before the first run, returns the first
   *                      run.
   *                      If aPoint is after the last run, returns nullptr.
   *                    If aForward is false and:
   *                      if aPoint is end of a run, returns the run.
   *                      if aPoint is start of a run, returns its next run.
   *                      if aPoint is before the first run, returns nullptr.
   *                      if aPoint is after the last run, returns the last run.
   */
  template <typename PT, typename CT>
  const WSFragment* FindNearestFragment(
      const EditorDOMPointBase<PT, CT>& aPoint, bool aForward) const {
    WSFragmentArray::index_type index =
        FindNearestFragmentIndex(aPoint, aForward);
    if (index == WSFragmentArray::NoIndex) {
      return nullptr;
    }
    return &mFragments[index];
  }
  template <typename PT, typename CT>
  WSFragmentArray::index_type FindNearestFragmentIndex(
      const EditorDOMPointBase<PT, CT>& aPoint, bool aForward) const;

  /**
   * GetInclusiveNextEditableCharPoint() returns aPoint if it points a character
   * in an editable text node, or start of next editable text node otherwise.
   * FYI: For the performance, this does not check whether given container
   *      is not after mStart.mReasonContent or not.
   */
  template <typename PT, typename CT>
  EditorDOMPointInText GetInclusiveNextEditableCharPoint(
      const EditorDOMPointBase<PT, CT>& aPoint) const;

  /**
   * GetPreviousEditableCharPoint() returns previous editable point in a
   * text node.  Note that this returns last character point when it meets
   * non-empty text node, otherwise, returns a point in an empty text node.
   * FYI: For the performance, this does not check whether given container
   *      is not before mEnd.mReasonContent or not.
   */
  template <typename PT, typename CT>
  EditorDOMPointInText GetPreviousEditableCharPoint(
      const EditorDOMPointBase<PT, CT>& aPoint) const;

  /**
   * GetEndOfCollapsibleASCIIWhiteSpaces() returns the next visible char
   * (meaning a character except ASCII white-spaces) point or end of last text
   * node scanning from aPointAtASCIIWhiteSpace.
   * Note that this may return different text node from the container of
   * aPointAtASCIIWhiteSpace.
   */
  EditorDOMPointInText GetEndOfCollapsibleASCIIWhiteSpaces(
      const EditorDOMPointInText& aPointAtASCIIWhiteSpace) const;

  /**
   * GetFirstASCIIWhiteSpacePointCollapsedTo() returns the first ASCII
   * white-space which aPointAtASCIIWhiteSpace belongs to.  In other words,
   * the white-space at aPointAtASCIIWhiteSpace should be collapsed into
   * the result.
   * Note that this may return different text node from the container of
   * aPointAtASCIIWhiteSpace.
   */
  EditorDOMPointInText GetFirstASCIIWhiteSpacePointCollapsedTo(
      const EditorDOMPointInText& aPointAtASCIIWhiteSpace) const;

  nsresult GetWSNodes();

  /**
   * Return a current block element for aContent or a topmost editable inline
   * element if aContent is not in editable block element.
   */
  nsIContent* GetEditableBlockParentOrTopmotEditableInlineContent(
      nsIContent* aContent) const;

  EditorDOMPointInText GetPreviousCharPointFromPointInText(
      const EditorDOMPointInText& aPoint) const;

  char16_t GetCharAt(dom::Text* aTextNode, int32_t aOffset) const;

  class MOZ_STACK_CLASS BoundaryData final {
   public:
    BoundaryData() : mReason(WSType::NotInitialized) {}
    template <typename EditorDOMPointType>
    BoundaryData(const EditorDOMPointType& aPoint, nsIContent& aReasonContent,
                 WSType aReason)
        : mReasonContent(&aReasonContent), mPoint(aPoint), mReason(aReason) {}
    bool Initialized() const { return mReasonContent && mPoint.IsSet(); }

    nsIContent* GetReasonContent() const { return mReasonContent; }
    const EditorDOMPoint& PointRef() const { return mPoint; }
    WSType RawReason() const { return mReason; }

    bool IsNormalText() const { return mReason == WSType::NormalText; }
    bool IsSpecialContent() const { return mReason == WSType::SpecialContent; }
    bool IsBRElement() const { return mReason == WSType::BRElement; }
    bool IsCurrentBlockBoundary() const {
      return mReason == WSType::CurrentBlockBoundary;
    }
    bool IsOtherBlockBoundary() const {
      return mReason == WSType::OtherBlockBoundary;
    }
    bool IsBlockBoundary() const {
      return mReason == WSType::CurrentBlockBoundary ||
             mReason == WSType::OtherBlockBoundary;
    }
    bool IsHardLineBreak() const {
      return mReason == WSType::CurrentBlockBoundary ||
             mReason == WSType::OtherBlockBoundary ||
             mReason == WSType::BRElement;
    }
    MOZ_NEVER_INLINE_DEBUG dom::Element* OtherBlockElementPtr() const {
      MOZ_DIAGNOSTIC_ASSERT(mReasonContent->IsElement());
      return mReasonContent->AsElement();
    }
    MOZ_NEVER_INLINE_DEBUG dom::HTMLBRElement* BRElementPtr() const {
      MOZ_DIAGNOSTIC_ASSERT(mReasonContent->IsHTMLElement(nsGkAtoms::br));
      return static_cast<dom::HTMLBRElement*>(mReasonContent.get());
    }

   private:
    nsCOMPtr<nsIContent> mReasonContent;
    EditorDOMPoint mPoint;
    // Must be one of WSType::NotInitialized, WSType::NormalText,
    // WSType::SpecialContent, WSType::BRElement, WSType::CurrentBlockBoundary
    // or WSType::OtherBlockBoundary.
    WSType mReason;
  };

  class MOZ_STACK_CLASS NoBreakingSpaceData final {
   public:
    enum class Scanning { Forward, Backward };
    void NotifyNBSP(const EditorDOMPointInText& aPoint,
                    Scanning aScanningDirection) {
      MOZ_ASSERT(aPoint.IsSetAndValid());
      MOZ_ASSERT(aPoint.IsCharNBSP());
      if (!mFirst.IsSet() || aScanningDirection == Scanning::Backward) {
        mFirst = aPoint;
      }
      if (!mLast.IsSet() || aScanningDirection == Scanning::Forward) {
        mLast = aPoint;
      }
    }

    const EditorDOMPointInText& FirstPointRef() const { return mFirst; }
    const EditorDOMPointInText& LastPointRef() const { return mLast; }

    bool FoundNBSP() const {
      MOZ_ASSERT(mFirst.IsSet() == mLast.IsSet());
      return mFirst.IsSet();
    }

   private:
    EditorDOMPointInText mFirst;
    EditorDOMPointInText mLast;
  };

  /**
   * TextFragmentData stores the information of text nodes which are in a
   * hard line.
   */
  class MOZ_STACK_CLASS TextFragmentData final {
   public:
    TextFragmentData() = delete;
    TextFragmentData(const BoundaryData& aStartBoundaryData,
                     const BoundaryData& aEndBoundaryData,
                     const NoBreakingSpaceData& aNBSPData, bool aIsPreformatted)
        : mStart(aStartBoundaryData),
          mEnd(aEndBoundaryData),
          mNBSPData(aNBSPData),
          mIsPreformatted(aIsPreformatted) {}

    void InitializeWSFragmentArray(WSFragmentArray& aFragments) const;

    bool StartsFromNormalText() const { return mStart.IsNormalText(); }
    bool StartsFromSpecialContent() const { return mStart.IsSpecialContent(); }
    bool StartsFromHardLineBreak() const { return mStart.IsHardLineBreak(); }
    bool EndsByNormalText() const { return mEnd.IsNormalText(); }
    bool EndsBySpecialContent() const { return mEnd.IsSpecialContent(); }
    bool EndsByBRElement() const { return mEnd.IsBRElement(); }
    bool EndsByBlockBoundary() const { return mEnd.IsBlockBoundary(); }

    /**
     * GetInvisibleLeadingWhiteSpaceRange() retruns two DOM points, start
     * of the line and first visible point or end of the hard line.  When
     * this returns non-positioned range or positioned but collapsed range,
     * there is no invisible leading white-spaces.
     * Note that if there are only invisible white-spaces in a hard line,
     * this returns all of the white-spaces.
     */
    template <typename EditorDOMRangeType>
    EditorDOMRangeType GetInvisibleLeadingWhiteSpaceRange() const;

    /**
     * GetInvisibleTrailingWhiteSpaceRange() returns two DOM points,
     * first invisible white-space and end of the hard line.  When this
     * returns non-positioned range or positioned but collapsed range,
     * there is no invisible trailing white-spaces.
     * Note that if there are only invisible white-spaces in a hard line,
     * this returns all of the white-spaces.
     */
    template <typename EditorDOMRangeType>
    EditorDOMRangeType GetInvisibleTrailingWhiteSpaceRange() const;

    /**
     * CreateWSFragmentForVisibleAndMiddleOfLine() creates WSFragment whose
     * `IsVisibleAndMiddleOfHardLine()` returns true.
     */
    Maybe<WSFragment> CreateWSFragmentForVisibleAndMiddleOfLine() const;

   private:
    BoundaryData mStart;
    BoundaryData mEnd;
    NoBreakingSpaceData mNBSPData;
    // XXX Currently we set mIsPreformatted to `WSRunScanner::mPRE` value
    //     even if some text nodes between mStart and mEnd are different styled
    //     nodes.  This caused some bugs actually, but we now keep traditional
    //     behavior for now.
    bool mIsPreformatted;
  };

  void EnsureWSFragments();
  template <typename EditorDOMPointType>
  void InitializeRangeStart(
      const EditorDOMPointType& aPoint,
      const nsIContent& aEditableBlockParentOrTopmostEditableInlineContent);
  template <typename EditorDOMPointType>
  void InitializeRangeEnd(
      const EditorDOMPointType& aPoint,
      const nsIContent& aEditableBlockParentOrTopmostEditableInlineContent);
  template <typename EditorDOMPointType>
  bool InitializeRangeStartWithTextNode(const EditorDOMPointType& aPoint);
  template <typename EditorDOMPointType>
  bool InitializeRangeEndWithTextNode(const EditorDOMPointType& aPoint);

  // The node passed to our constructor.
  EditorDOMPoint mScanStartPoint;
  EditorDOMPoint mScanEndPoint;
  // Together, the above represent the point at which we are building up ws
  // info.

  // The editing host when the instance is created.
  RefPtr<dom::Element> mEditingHost;

  // true if we are in preformatted white-space context.
  bool mPRE;

  // Non-owning.
  const HTMLEditor* mHTMLEditor;

 protected:
  BoundaryData mStart;
  BoundaryData mEnd;
  NoBreakingSpaceData mNBSPData;

 private:
  // Don't access `mFragments` directly.  Use `WSFragmentArrayRef()` when you
  // want to access.  Then, it initializes all fragments for you if they has
  // not been initialized yet.
  WSFragmentArray mFragments;
};

class MOZ_STACK_CLASS WSRunObject final : public WSRunScanner {
 protected:
  typedef EditorBase::AutoTransactionsConserveSelection
      AutoTransactionsConserveSelection;

 public:
  enum BlockBoundary { kBeforeBlock, kBlockStart, kBlockEnd, kAfterBlock };

  /**
   * The constructors take 2 DOM points.  They represent a range in an editing
   * host.  aScanEndPoint (aScanEndNode and aScanEndOffset) must be later
   * point from aScanStartPoint (aScanStartNode and aScanStartOffset).
   * The end point is currently used only with InsertText().  Therefore,
   * currently, this assumes that the range does not cross block boundary.  If
   * you use aScanEndPoint newly, please test enough.
   * TODO: PrepareToJoinBlocks(), PrepareToDeleteRange() and
   *       PrepareToDeleteNode() should be redesigned with aScanEndPoint.
   */
  template <typename PT, typename CT>
  MOZ_CAN_RUN_SCRIPT WSRunObject(
      HTMLEditor& aHTMLEditor,
      const EditorDOMPointBase<PT, CT>& aScanStartPoint,
      const EditorDOMPointBase<PT, CT>& aScanEndPoint);
  template <typename PT, typename CT>
  MOZ_CAN_RUN_SCRIPT WSRunObject(
      HTMLEditor& aHTMLEditor,
      const EditorDOMPointBase<PT, CT>& aScanStartPoint)
      : WSRunObject(aHTMLEditor, aScanStartPoint, aScanStartPoint) {}
  MOZ_CAN_RUN_SCRIPT WSRunObject(HTMLEditor& aHTMLEditor,
                                 nsINode* aScanStartNode,
                                 int32_t aScanStartOffset,
                                 nsINode* aScanEndNode, int32_t aScanEndOffset)
      : WSRunObject(aHTMLEditor,
                    EditorRawDOMPoint(aScanStartNode, aScanStartOffset),
                    EditorRawDOMPoint(aScanEndNode, aScanEndOffset)) {}
  MOZ_CAN_RUN_SCRIPT WSRunObject(HTMLEditor& aHTMLEditor,
                                 nsINode* aScanStartNode,
                                 int32_t aScanStartOffset)
      : WSRunObject(aHTMLEditor,
                    EditorRawDOMPoint(aScanStartNode, aScanStartOffset),
                    EditorRawDOMPoint(aScanStartNode, aScanStartOffset)) {}

  /**
   * DeleteInvisibleASCIIWhiteSpaces() removes invisible leading white-spaces
   * and trailing white-spaces if there are around aPoint.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static nsresult
  DeleteInvisibleASCIIWhiteSpaces(HTMLEditor& aHTMLEditor,
                                  const EditorDOMPoint& aPoint);

  /**
   * PrepareToJoinBlocks() fixes up white-spaces at the end of aLeftBlockElement
   * and the start of aRightBlockElement in preperation for them to be joined.
   * For example, trailing white-spaces in aLeftBlockElement needs to be
   * removed.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static nsresult PrepareToJoinBlocks(
      HTMLEditor& aHTMLEditor, dom::Element& aLeftBlockElement,
      dom::Element& aRightBlockElement);

  // PrepareToDeleteRange fixes up ws before aStartPoint and after aEndPoint in
  // preperation for content in that range to be deleted.  Note that the nodes
  // and offsets are adjusted in response to any dom changes we make while
  // adjusting ws.
  // example of fixup: trailingws before aStartPoint needs to be removed.
  MOZ_CAN_RUN_SCRIPT static nsresult PrepareToDeleteRange(
      HTMLEditor& aHTMLEditor, EditorDOMPoint* aStartPoint,
      EditorDOMPoint* aEndPoint);

  // PrepareToDeleteNode fixes up ws before and after aContent in preparation
  // for aContent to be deleted.  Example of fixup: trailingws before
  // aContent needs to be removed.
  MOZ_CAN_RUN_SCRIPT static nsresult PrepareToDeleteNode(
      HTMLEditor& aHTMLEditor, nsIContent* aContent);

  // PrepareToSplitAcrossBlocks fixes up ws before and after
  // {aSplitNode,aSplitOffset} in preparation for a block parent to be split.
  // Note that the aSplitNode and aSplitOffset are adjusted in response to
  // any DOM changes we make while adjusting ws.  Example of fixup: normalws
  // before {aSplitNode,aSplitOffset} needs to end with nbsp.
  MOZ_CAN_RUN_SCRIPT static nsresult PrepareToSplitAcrossBlocks(
      HTMLEditor& aHTMLEditor, nsCOMPtr<nsINode>* aSplitNode,
      int32_t* aSplitOffset);

  /**
   * InsertBreak() inserts a <br> node at (before) aPointToInsert and delete
   * unnecessary white-spaces around there and/or replaces white-spaces with
   * non-breaking spaces.  Note that if the point is in a text node, the
   * text node will be split and insert new <br> node between the left node
   * and the right node.
   *
   * @param aSelection      The selection for the editor.
   * @param aPointToInsert  The point to insert new <br> element.  Note that
   *                        it'll be inserted before this point.  I.e., the
   *                        point will be the point of new <br>.
   * @param aSelect         If eNone, this won't change selection.
   *                        If eNext, selection will be collapsed after the
   *                        <br> element.
   *                        If ePrevious, selection will be collapsed at the
   *                        <br> element.
   * @return                The new <br> node.  If failed to create new <br>
   *                        node, returns nullptr.
   */
  MOZ_CAN_RUN_SCRIPT already_AddRefed<dom::Element> InsertBreak(
      dom::Selection& aSelection, const EditorDOMPoint& aPointToInsert,
      nsIEditor::EDirection aSelect);

  /**
   * InsertText() inserts aStringToInsert to mScanStartPoint and makes any
   * needed adjustments to white-spaces around both mScanStartPoint and
   * mScanEndPoint. E.g., trailing white-spaces before mScanStartPoint needs to
   * be removed.  This calls EditorBase::InsertTextWithTransaction() after
   * adjusting white-spaces.  So, please refer the method's explanation to know
   * what this method exactly does.
   *
   * @param aDocument       The document of this editor.
   * @param aStringToInsert The string to insert.
   * @param aPointAfterInsertedString
   *                        The point after inserted aStringToInsert.
   *                        So, when this method actually inserts string,
   *                        this is set to a point in the text node.
   *                        Otherwise, this may be set to mScanStartPoint.
   * @return                When this succeeds to insert the string or
   *                        does nothing during composition, returns NS_OK.
   *                        Otherwise, an error code.
   */
  MOZ_CAN_RUN_SCRIPT nsresult
  InsertText(dom::Document& aDocument, const nsAString& aStringToInsert,
             EditorRawDOMPoint* aPointAfterInsertedString = nullptr);

  // DeleteWSBackward deletes a single visible piece of ws before the ws
  // point (the point to create the wsRunObject, passed to its constructor).
  // It makes any needed conversion to adjacent ws to retain its
  // significance.
  MOZ_CAN_RUN_SCRIPT nsresult DeleteWSBackward();

  // DeleteWSForward deletes a single visible piece of ws after the ws point
  // (the point to create the wsRunObject, passed to its constructor).  It
  // makes any needed conversion to adjacent ws to retain its significance.
  MOZ_CAN_RUN_SCRIPT nsresult DeleteWSForward();

  /**
   * NormalizeWhiteSpacesAround() tries to normalize white-space sequence
   * around aScanStartPoint.
   */
  template <typename EditorDOMPointType>
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static nsresult NormalizeWhiteSpacesAround(
      HTMLEditor& aHTMLEditor, const EditorDOMPointType& aSacnStartPoint);

 protected:
  MOZ_CAN_RUN_SCRIPT nsresult PrepareToDeleteRangePriv(WSRunObject* aEndObject);
  MOZ_CAN_RUN_SCRIPT nsresult PrepareToSplitAcrossBlocksPriv();

  /**
   * ReplaceASCIIWhiteSpacesWithOneNBSP() replaces the range between
   * aAtFirstASCIIWhiteSpace and aEndOfCollapsibleASCIIWhiteSpace with
   * one NBSP char.  If they span multiple text nodes, this puts an NBSP
   * into the text node at aAtFirstASCIIWhiteSpace.  Then, removes other
   * ASCII white-spaces in the following text nodes.
   * Note that this assumes that all characters in the range is ASCII
   * white-spaces.
   *
   * @param aAtFirstASCIIWhiteSpace             First ASCII white-space
   * position.
   * @param aEndOfCollapsibleASCIIWhiteSpaces   The position after last ASCII
   *                                            white-space.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult ReplaceASCIIWhiteSpacesWithOneNBSP(
      const EditorDOMPointInText& aAtFirstASCIIWhiteSpace,
      const EditorDOMPointInText& aEndOfCollapsibleASCIIWhiteSpaces);

  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
  NormalizeWhiteSpacesAtEndOf(const WSFragment& aRun);

  /**
   * MaybeReplacePreviousNBSPWithASCIIWhiteSpace() replaces previous character
   * of aPoint if it's a NBSP and it's unnecessary.
   *
   * @param aRun        Current text run.  aPoint must be in this run.
   * @param aPoint      Current insertion point.  Its previous character is
   *                    unnecessary NBSP will be checked.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
  MaybeReplacePreviousNBSPWithASCIIWhiteSpace(const WSFragment& aRun,
                                              const EditorDOMPoint& aPoint);

  /**
   * MaybeReplaceInclusiveNextNBSPWithASCIIWhiteSpace() replaces an NBSP at
   * the point with an ASCII white-space only when the point is an NBSP and
   * it's replaceable with an ASCII white-space.
   *
   * @param aPoint      If point in a text node, the character is checked
   *                    whether it's an NBSP or not.  Otherwise, first
   *                    character of next editable text node is checked.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
  MaybeReplaceInclusiveNextNBSPWithASCIIWhiteSpace(
      const WSFragment& aRun, const EditorDOMPoint& aPoint);

  /**
   * See explanation of the static method for this.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
  DeleteInvisibleASCIIWhiteSpacesInternal();

  // Because of MOZ_CAN_RUN_SCRIPT constructors, each instanciater of this class
  // guarantees the lifetime of the HTMLEditor.
  HTMLEditor& mHTMLEditor;
};

}  // namespace mozilla

#endif  // #ifndef WSRunObject_h
