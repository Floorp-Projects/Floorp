/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WSRunObject_h
#define WSRunObject_h

#include "mozilla/Assertions.h"
#include "mozilla/EditAction.h"
#include "mozilla/EditorBase.h"
#include "mozilla/EditorDOMPoint.h"  // for EditorDOMPoint
#include "mozilla/HTMLEditor.h"
#include "mozilla/Maybe.h"
#include "mozilla/Tuple.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLBRElement.h"
#include "mozilla/dom/Text.h"
#include "nsCOMPtr.h"
#include "nsIContent.h"

namespace mozilla {

// class WSRunObject represents the entire whitespace situation
// around a given point.  It collects up a list of nodes that contain
// whitespace and categorizes in up to 3 different WSFragments (detailed
// below).  Each WSFragment is a collection of whitespace that is
// either all insignificant, or that is significant.  A WSFragment could
// consist of insignificant whitespace because it is after a block
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
    // The run is maybe collapsible white spaces at start of a hard line.
    LeadingWhiteSpaces,
    // The run is maybe collapsible white spaces at end of a hard line.
    TrailingWhiteSpaces,
    // Normal (perhaps, meaning visible) white spaces.
    NormalWhiteSpaces,
    // Normal text, not white spaces.
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
                      !HTMLEditor::NodeIsBlockStatic(*mContent))));
    MOZ_ASSERT_IF(mReason == WSType::OtherBlockBoundary,
                  mContent && HTMLEditor::NodeIsBlockStatic(*mContent));
    // If mReason is WSType::CurrentBlockBoundary, mContent can be any content.
    // In most cases, it's current block element which is editable.  However, if
    // there is no editable block parent, this is topmost editable inline
    // content. Additionally, if there is no editable content, this is the
    // container start of scanner and is not editable.
    MOZ_ASSERT_IF(
        mReason == WSType::CurrentBlockBoundary,
        !mContent || !mContent->GetParentElement() ||
            HTMLEditor::NodeIsBlockStatic(*mContent) ||
            HTMLEditor::NodeIsBlockStatic(*mContent->GetParentElement()) ||
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
   * The point is in normal whitespaces or text.
   */
  bool InNormalWhiteSpacesOrText() const {
    return mReason == WSType::NormalWhiteSpaces ||
           mReason == WSType::NormalText;
  }

  /**
   * The point is in normal whitespaces.
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
   * Actually, WSRunScanner scans white space type at mScanStartPoint position
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
  ~WSRunScanner();

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
   * GetStartReasonContent() and GetEndReasonContent() return a node which
   * was found by scanning from mScanStartPoint backward or mScanEndPoint
   * forward.  If there was whitespaces or text from the point, returns the
   * text node.  Otherwise, returns an element which is explained by the
   * following methods.  Note that when the reason is
   * WSType::CurrentBlockBoundary, In most cases, it's current block element
   * which is editable, but also may be non-element and/or non-editable.  See
   * MOZ_ASSERT_IF()s in WSScanResult::AssertIfInvalidData() for the detail.
   */
  nsIContent* GetStartReasonContent() const { return mStartReasonContent; }
  nsIContent* GetEndReasonContent() const { return mEndReasonContent; }

  bool StartsFromNormalText() const {
    return mStartReason == WSType::NormalText;
  }
  bool StartsFromSpecialContent() const {
    return mStartReason == WSType::SpecialContent;
  }
  bool StartsFromBRElement() const { return mStartReason == WSType::BRElement; }
  bool StartsFromCurrentBlockBoundary() const {
    return mStartReason == WSType::CurrentBlockBoundary;
  }
  bool StartsFromOtherBlockElement() const {
    return mStartReason == WSType::OtherBlockBoundary;
  }
  bool StartsFromBlockBoundary() const {
    return mStartReason == WSType::CurrentBlockBoundary ||
           mStartReason == WSType::OtherBlockBoundary;
  }
  bool StartsFromHardLineBreak() const {
    return mStartReason == WSType::CurrentBlockBoundary ||
           mStartReason == WSType::OtherBlockBoundary ||
           mStartReason == WSType::BRElement;
  }
  bool EndsByNormalText() const { return mEndReason == WSType::NormalText; }
  bool EndsBySpecialContent() const {
    return mEndReason == WSType::SpecialContent;
  }
  bool EndsByBRElement() const { return mEndReason == WSType::BRElement; }
  bool EndsByCurrentBlockBoundary() const {
    return mEndReason == WSType::CurrentBlockBoundary;
  }
  bool EndsByOtherBlockElement() const {
    return mEndReason == WSType::OtherBlockBoundary;
  }
  bool EndsByBlockBoundary() const {
    return mEndReason == WSType::CurrentBlockBoundary ||
           mEndReason == WSType::OtherBlockBoundary;
  }

  MOZ_NEVER_INLINE_DEBUG dom::Element* StartReasonOtherBlockElementPtr() const {
    MOZ_DIAGNOSTIC_ASSERT(mStartReasonContent->IsElement());
    return mStartReasonContent->AsElement();
  }
  MOZ_NEVER_INLINE_DEBUG dom::HTMLBRElement* StartReasonBRElementPtr() const {
    MOZ_DIAGNOSTIC_ASSERT(mStartReasonContent->IsHTMLElement(nsGkAtoms::br));
    return static_cast<dom::HTMLBRElement*>(mStartReasonContent.get());
  }
  MOZ_NEVER_INLINE_DEBUG dom::Element* EndReasonOtherBlockElementPtr() const {
    MOZ_DIAGNOSTIC_ASSERT(mEndReasonContent->IsElement());
    return mEndReasonContent->AsElement();
  }
  MOZ_NEVER_INLINE_DEBUG dom::HTMLBRElement* EndReasonBRElementPtr() const {
    MOZ_DIAGNOSTIC_ASSERT(mEndReasonContent->IsHTMLElement(nsGkAtoms::br));
    return static_cast<dom::HTMLBRElement*>(mEndReasonContent.get());
  }

  /**
   * Active editing host when this instance is created.
   */
  Element* GetEditingHost() const { return mEditingHost; }

 protected:
  // WSFragment represents a single run of ws (all leadingws, or all normalws,
  // or all trailingws, or all leading+trailingws).  Note that this single run
  // may still span multiple nodes.
  struct WSFragment final {
    nsCOMPtr<nsINode> mStartNode;  // node where ws run starts
    nsCOMPtr<nsINode> mEndNode;    // node where ws run ends
    int32_t mStartOffset;          // offset where ws run starts
    int32_t mEndOffset;            // offset where ws run ends
    // other ws runs to left or right.  may be null.
    WSFragment *mLeft, *mRight;

    WSFragment()
        : mStartOffset(0),
          mEndOffset(0),
          mLeft(nullptr),
          mRight(nullptr),
          mLeftWSType(WSType::NotInitialized),
          mRightWSType(WSType::NotInitialized),
          mIsVisible(Visible::No),
          mIsStartOfHardLine(StartOfHardLine::No),
          mIsEndOfHardLine(EndOfHardLine::No) {}

    EditorRawDOMPoint StartPoint() const {
      return EditorRawDOMPoint(mStartNode, mStartOffset);
    }
    EditorRawDOMPoint EndPoint() const {
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
    bool EndsBySpecialContent() const {
      return mRightWSType == WSType::SpecialContent;
    }
    bool EndsByBRElement() const { return mRightWSType == WSType::BRElement; }
    bool EndsByBlockBoundary() const {
      return mRightWSType == WSType::CurrentBlockBoundary ||
             mRightWSType == WSType::OtherBlockBoundary;
    }

   private:
    WSType mLeftWSType, mRightWSType;
    Visible mIsVisible;
    StartOfHardLine mIsStartOfHardLine;
    EndOfHardLine mIsEndOfHardLine;
  };

  /**
   * FindNearestRun() looks for a WSFragment which is closest to specified
   * direction from aPoint.
   *
   * @param aPoint      The point to start to look for.
   * @param aForward    true if caller needs to look for a WSFragment after the
   *                    point in the DOM tree.  Otherwise, i.e., before the
   *                    point, false.
   * @return            Found WSFragment instance.
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
  WSFragment* FindNearestRun(const EditorDOMPointBase<PT, CT>& aPoint,
                             bool aForward) const;

  /**
   * GetNextCharPoint() and GetNextCharPointFromPointInText() return next
   * character's point of aPoint.  If there is no character after aPoint,
   * mTextNode is set to nullptr.
   */
  template <typename PT, typename CT>
  EditorDOMPointInText GetNextCharPoint(
      const EditorDOMPointBase<PT, CT>& aPoint) const;
  EditorDOMPointInText GetNextCharPointFromPointInText(
      const EditorDOMPointInText& aPoint) const;

  /**
   * LookForNextCharPointWithinAllTextNodes() and
   * LookForPreviousCharPointWithinAllTextNodes() are helper methods of
   * GetNextCharPoint(const EditorRawDOMPoint&) and GetPreviousCharPoint(const
   * EditorRawDOMPoint&).  When the container isn't in mNodeArray, they call one
   * of these methods.  Then, these methods look for nearest text node in
   * mNodeArray from aPoint. Then, will call GetNextCharPointFromPointInText()
   * or GetPreviousCharPointFromPointInText() and returns its result.
   */
  template <typename PT, typename CT>
  EditorDOMPointInText LookForNextCharPointWithinAllTextNodes(
      const EditorDOMPointBase<PT, CT>& aPoint) const;
  template <typename PT, typename CT>
  EditorDOMPointInText LookForPreviousCharPointWithinAllTextNodes(
      const EditorDOMPointBase<PT, CT>& aPoint) const;

  nsresult GetWSNodes();

  /**
   * Return a current block element for aContent or a topmost editable inline
   * element if aContent is not in editable block element.
   */
  nsIContent* GetEditableBlockParentOrTopmotEditableInlineContent(
      nsIContent* aContent) const;

  static bool IsBlockNode(nsINode* aNode) {
    return aNode && aNode->IsElement() && HTMLEditor::NodeIsBlockStatic(*aNode);
  }

  nsIContent* GetPreviousWSNodeInner(nsINode* aStartNode,
                                     nsINode* aBlockParent) const;
  nsIContent* GetPreviousWSNode(const EditorDOMPoint& aPoint,
                                nsINode* aBlockParent) const;
  nsIContent* GetNextWSNodeInner(nsINode* aStartNode,
                                 nsINode* aBlockParent) const;
  nsIContent* GetNextWSNode(const EditorDOMPoint& aPoint,
                            nsINode* aBlockParent) const;

  /**
   * GetPreviousCharPoint() and GetPreviousCharPointFromPointInText() return
   * previous character's point of of aPoint. If there is no character before
   * aPoint, mTextNode is set to nullptr.
   */
  template <typename PT, typename CT>
  EditorDOMPointInText GetPreviousCharPoint(
      const EditorDOMPointBase<PT, CT>& aPoint) const;
  EditorDOMPointInText GetPreviousCharPointFromPointInText(
      const EditorDOMPointInText& aPoint) const;

  char16_t GetCharAt(dom::Text* aTextNode, int32_t aOffset) const;

  void GetRuns();
  void ClearRuns();
  void InitializeWithSingleFragment(
      WSFragment::Visible aIsVisible,
      WSFragment::StartOfHardLine aIsStartOfHardLine,
      WSFragment::EndOfHardLine aIsEndOfHardLine);

  // The list of nodes containing ws in this run.
  nsTArray<RefPtr<dom::Text>> mNodeArray;

  // The node passed to our constructor.
  EditorDOMPoint mScanStartPoint;
  EditorDOMPoint mScanEndPoint;
  // Together, the above represent the point at which we are building up ws
  // info.

  // The editing host when the instance is created.
  RefPtr<Element> mEditingHost;

  // true if we are in preformatted whitespace context.
  bool mPRE;

  // Node/offset where ws starts and ends.
  nsCOMPtr<nsINode> mStartNode;
  int32_t mStartOffset;

  // Node/offset where ws ends.
  nsCOMPtr<nsINode> mEndNode;
  int32_t mEndOffset;

  // Location of first nbsp in ws run, if any.
  RefPtr<dom::Text> mFirstNBSPNode;
  int32_t mFirstNBSPOffset;

  // Location of last nbsp in ws run, if any.
  RefPtr<dom::Text> mLastNBSPNode;
  int32_t mLastNBSPOffset;

  // The first WSFragment in the run.
  WSFragment* mStartRun;

  // The last WSFragment in the run, may be same as first.
  WSFragment* mEndRun;

  // See above comment for GetStartReasonContent() and GetEndReasonContent().
  nsCOMPtr<nsIContent> mStartReasonContent;
  nsCOMPtr<nsIContent> mEndReasonContent;

  // Non-owning.
  const HTMLEditor* mHTMLEditor;

 private:
  // Must be one of WSType::NotInitialized, WSType::NormalText,
  // WSType::SpecialContent, WSType::BRElement, WSType::CurrentBlockBoundary or
  // WSType::OtherBlockBoundary.  Access these values with StartsFrom*() and
  // EndsBy*() accessors.
  WSType mStartReason;
  WSType mEndReason;
};

class MOZ_STACK_CLASS WSRunObject final : public WSRunScanner {
 protected:
  typedef EditorBase::AutoTransactionsConserveSelection
      AutoTransactionsConserveSelection;

 public:
  enum BlockBoundary { kBeforeBlock, kBlockStart, kBlockEnd, kAfterBlock };

  enum { eBefore = 1 };
  enum { eAfter = 1 << 1 };
  enum { eBoth = eBefore | eAfter };

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
   * Scrub() removes any non-visible whitespaces at aPoint.
   */
  MOZ_CAN_RUN_SCRIPT MOZ_MUST_USE static nsresult Scrub(
      HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPoint);

  /**
   * PrepareToJoinBlocks() fixes up whitespaces at the end of aLeftBlockElement
   * and the start of aRightBlockElement in preperation for them to be joined.
   * For example, trailing whitespaces in aLeftBlockElement needs to be removed.
   */
  MOZ_CAN_RUN_SCRIPT MOZ_MUST_USE static nsresult PrepareToJoinBlocks(
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
   * unnecessary whitespaces around there and/or replaces whitespaces with
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
      Selection& aSelection, const EditorDOMPoint& aPointToInsert,
      nsIEditor::EDirection aSelect);

  /**
   * InsertText() inserts aStringToInsert to mScanStartPoint and makes any
   * needed adjustments to white spaces around both mScanStartPoint and
   * mScanEndPoint. E.g., trailing white spaces before mScanStartPoint needs to
   * be removed.  This calls EditorBase::InsertTextWithTransaction() after
   * adjusting white spaces.  So, please refer the method's explanation to know
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

  // AdjustWhitespace examines the ws object for nbsp's that can
  // be safely converted to regular ascii space and converts them.
  MOZ_CAN_RUN_SCRIPT nsresult AdjustWhitespace();

 protected:
  MOZ_CAN_RUN_SCRIPT nsresult PrepareToDeleteRangePriv(WSRunObject* aEndObject);
  MOZ_CAN_RUN_SCRIPT nsresult PrepareToSplitAcrossBlocksPriv();

  /**
   * DeleteRange() removes the range between aStartPoint and aEndPoint.
   * When aStartPoint and aEndPoint are same point, does nothing.
   * When aStartPoint and aEndPoint are in same text node, removes characters
   * between them.
   * When aStartPoint is in a text node, removes the text data after the point.
   * When aEndPoint is in a text node, removes the text data before the point.
   * Removes any nodes between them.
   */
  MOZ_CAN_RUN_SCRIPT nsresult DeleteRange(const EditorDOMPoint& aStartPoint,
                                          const EditorDOMPoint& aEndPoint);

  /**
   * InsertNBSPAndRemoveFollowingASCIIWhitespaces() inserts an NBSP first.
   * Then, if following characters are ASCII whitespaces, will remove them.
   */
  MOZ_CAN_RUN_SCRIPT nsresult InsertNBSPAndRemoveFollowingASCIIWhitespaces(
      const EditorDOMPointInText& aPoint);

  /**
   * GetASCIIWhitespacesBounds() returns a range from start of whitespaces
   * and end of whitespaces if the character at aPoint is an ASCII whitespace.
   * Note that the end is next character of the last whitespace.
   *
   * @param aDir            Specify eBefore if you want to scan text backward.
   *                        Specify eAfter if you want to scan text forward.
   *                        Specify eBoth if you want to scan text to both
   *                        direction.
   * @param aPoint          The point to start to scan whitespaces from.
   * @return                Start and end of the expanded range.
   */
  template <typename PT, typename CT>
  Tuple<EditorDOMPointInText, EditorDOMPointInText> GetASCIIWhitespacesBounds(
      int16_t aDir, const EditorDOMPointBase<PT, CT>& aPoint) const;

  MOZ_CAN_RUN_SCRIPT nsresult CheckTrailingNBSPOfRun(WSFragment* aRun);

  /**
   * ReplacePreviousNBSPIfUnnecessary() replaces previous character of aPoint
   * if it's a NBSP and it's unnecessary.
   *
   * @param aRun        Current text run.  aPoint must be in this run.
   * @param aPoint      Current insertion point.  Its previous character is
   *                    unnecessary NBSP will be checked.
   */
  MOZ_CAN_RUN_SCRIPT nsresult ReplacePreviousNBSPIfUnnecessary(
      WSFragment* aRun, const EditorDOMPoint& aPoint);

  MOZ_CAN_RUN_SCRIPT nsresult CheckLeadingNBSP(WSFragment* aRun, nsINode* aNode,
                                               int32_t aOffset);

  MOZ_CAN_RUN_SCRIPT nsresult Scrub();

  EditorDOMPoint StartPoint() const {
    return EditorDOMPoint(mStartNode, mStartOffset);
  }
  EditorDOMPoint EndPoint() const {
    return EditorDOMPoint(mEndNode, mEndOffset);
  }

  // Because of MOZ_CAN_RUN_SCRIPT constructors, each instanciater of this class
  // guarantees the lifetime of the HTMLEditor.
  HTMLEditor& mHTMLEditor;
};

}  // namespace mozilla

#endif  // #ifndef WSRunObject_h
