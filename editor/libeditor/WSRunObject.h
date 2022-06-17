/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WSRunObject_h
#define WSRunObject_h

#include "EditAction.h"
#include "EditorBase.h"
#include "EditorForwards.h"
#include "EditorDOMPoint.h"  // for EditorDOMPoint
#include "HTMLEditor.h"

#include "HTMLEditUtils.h"

#include "mozilla/Assertions.h"
#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLBRElement.h"
#include "mozilla/dom/Text.h"
#include "nsCOMPtr.h"
#include "nsIContent.h"

namespace mozilla {

using namespace dom;

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
    // Could be the DOM tree is broken as like crash tests.
    UnexpectedError,
    // The run is maybe collapsible white-spaces at start of a hard line.
    LeadingWhiteSpaces,
    // The run is maybe collapsible white-spaces at end of a hard line.
    TrailingWhiteSpaces,
    // Collapsible, but visible white-spaces.
    CollapsibleWhiteSpaces,
    // Visible characters except collapsible white-spaces.
    NonCollapsibleCharacters,
    // Special content such as `<img>`, etc.
    SpecialContent,
    // <br> element.
    BRElement,
    // A linefeed which is preformatted.
    PreformattedLineBreak,
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
    MOZ_ASSERT(mReason == WSType::UnexpectedError ||
               mReason == WSType::NonCollapsibleCharacters ||
               mReason == WSType::CollapsibleWhiteSpaces ||
               mReason == WSType::BRElement ||
               mReason == WSType::PreformattedLineBreak ||
               mReason == WSType::SpecialContent ||
               mReason == WSType::CurrentBlockBoundary ||
               mReason == WSType::OtherBlockBoundary);
    MOZ_ASSERT_IF(mReason == WSType::UnexpectedError, !mContent);
    MOZ_ASSERT_IF(mReason == WSType::NonCollapsibleCharacters ||
                      mReason == WSType::CollapsibleWhiteSpaces,
                  mContent && mContent->IsText());
    MOZ_ASSERT_IF(mReason == WSType::BRElement,
                  mContent && mContent->IsHTMLElement(nsGkAtoms::br));
    MOZ_ASSERT_IF(mReason == WSType::PreformattedLineBreak,
                  mContent && mContent->IsText() &&
                      EditorUtils::IsNewLinePreformatted(*mContent));
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

  bool Failed() const {
    return mReason == WSType::NotInitialized ||
           mReason == WSType::UnexpectedError;
  }

  /**
   * GetContent() returns found visible and editable content/element.
   * See MOZ_ASSERT_IF()s in AssertIfInvalidData() for the detail.
   */
  nsIContent* GetContent() const { return mContent; }

  /**
   * The following accessors makes it easier to understand each callers.
   */
  MOZ_NEVER_INLINE_DEBUG Element* ElementPtr() const {
    MOZ_DIAGNOSTIC_ASSERT(mContent->IsElement());
    return mContent->AsElement();
  }
  MOZ_NEVER_INLINE_DEBUG HTMLBRElement* BRElementPtr() const {
    MOZ_DIAGNOSTIC_ASSERT(mContent->IsHTMLElement(nsGkAtoms::br));
    return static_cast<HTMLBRElement*>(mContent.get());
  }
  MOZ_NEVER_INLINE_DEBUG Text* TextPtr() const {
    MOZ_DIAGNOSTIC_ASSERT(mContent->IsText());
    return mContent->AsText();
  }

  /**
   * Returns true if found or reached content is ediable.
   */
  bool IsContentEditable() const { return mContent && mContent->IsEditable(); }

  /**
   *  Offset() returns meaningful value only when
   * InVisibleOrCollapsibleCharacters() returns true or the scanner
   * reached to start or end of its scanning range and that is same as start or
   * end container which are specified when the scanner is initialized.  If it's
   * result of scanning backward, this offset means before the found point.
   * Otherwise, i.e., scanning forward, this offset means after the found point.
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
  template <typename EditorDOMPointType>
  EditorDOMPointType Point() const {
    NS_ASSERTION(mOffset.isSome(), "Retrieved non-meaningful point");
    return EditorDOMPointType(mContent, mOffset.valueOr(0));
  }

  /**
   * PointAtContent() and RawPointAtContent() return the position of found
   * visible content or reached block element.
   */
  template <typename EditorDOMPointType>
  EditorDOMPointType PointAtContent() const {
    MOZ_ASSERT(mContent);
    return EditorDOMPointType(mContent);
  }

  /**
   * PointAfterContent() and RawPointAfterContent() retrun the position after
   * found visible content or reached block element.
   */
  template <typename EditorDOMPointType>
  EditorDOMPointType PointAfterContent() const {
    MOZ_ASSERT(mContent);
    return mContent ? EditorDOMPointType::After(mContent)
                    : EditorDOMPointType();
  }

  /**
   * The scanner reached <img> or something which is inline and is not a
   * container.
   */
  bool ReachedSpecialContent() const {
    return mReason == WSType::SpecialContent;
  }

  /**
   * The point is in visible characters or collapsible white-spaces.
   */
  bool InVisibleOrCollapsibleCharacters() const {
    return mReason == WSType::CollapsibleWhiteSpaces ||
           mReason == WSType::NonCollapsibleCharacters;
  }

  /**
   * The point is in collapsible white-spaces.
   */
  bool InCollapsibleWhiteSpaces() const {
    return mReason == WSType::CollapsibleWhiteSpaces;
  }

  /**
   * The point is in visible non-collapsible characters.
   */
  bool InNonCollapsibleCharacters() const {
    return mReason == WSType::NonCollapsibleCharacters;
  }

  /**
   * The scanner reached a <br> element.
   */
  bool ReachedBRElement() const { return mReason == WSType::BRElement; }
  bool ReachedVisibleBRElement() const {
    return ReachedBRElement() &&
           HTMLEditUtils::IsVisibleBRElement(*BRElementPtr());
  }
  bool ReachedInvisibleBRElement() const {
    return ReachedBRElement() &&
           HTMLEditUtils::IsInvisibleBRElement(*BRElementPtr());
  }

  bool ReachedPreformattedLineBreak() const {
    return mReason == WSType::PreformattedLineBreak;
  }

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
   * The scanner reached other block element that isn't editable
   */
  bool ReachedNonEditableOtherBlockElement() const {
    return ReachedOtherBlockElement() && !GetContent()->IsEditable();
  }

  /**
   * The scanner reached something non-text node.
   */
  bool ReachedSomethingNonTextContent() const {
    return !InVisibleOrCollapsibleCharacters();
  }

 private:
  nsCOMPtr<nsIContent> mContent;
  Maybe<uint32_t> mOffset;
  WSType mReason;
};

class MOZ_STACK_CLASS WSRunScanner final {
 public:
  using WSType = WSScanResult::WSType;

  template <typename EditorDOMPointType>
  WSRunScanner(const Element* aEditingHost,
               const EditorDOMPointType& aScanStartPoint)
      : mScanStartPoint(aScanStartPoint.template To<EditorDOMPoint>()),
        mEditingHost(const_cast<Element*>(aEditingHost)),
        mTextFragmentDataAtStart(mScanStartPoint, mEditingHost) {}

  // ScanNextVisibleNodeOrBlockBoundaryForwardFrom() returns the first visible
  // node after aPoint.  If there is no visible nodes after aPoint, returns
  // topmost editable inline ancestor at end of current block.  See comments
  // around WSScanResult for the detail.
  template <typename PT, typename CT>
  WSScanResult ScanNextVisibleNodeOrBlockBoundaryFrom(
      const EditorDOMPointBase<PT, CT>& aPoint) const;
  template <typename PT, typename CT>
  static WSScanResult ScanNextVisibleNodeOrBlockBoundary(
      const Element* aEditingHost, const EditorDOMPointBase<PT, CT>& aPoint) {
    return WSRunScanner(aEditingHost, aPoint)
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
      const Element* aEditingHost, const EditorDOMPointBase<PT, CT>& aPoint) {
    return WSRunScanner(aEditingHost, aPoint)
        .ScanPreviousVisibleNodeOrBlockBoundaryFrom(aPoint);
  }

  /**
   * GetInclusiveNextEditableCharPoint() returns a point in a text node which
   * is at current editable character or next editable character if aPoint
   * does not points an editable character.
   */
  template <typename EditorDOMPointType = EditorDOMPointInText, typename PT,
            typename CT>
  static EditorDOMPointType GetInclusiveNextEditableCharPoint(
      Element* aEditingHost, const EditorDOMPointBase<PT, CT>& aPoint) {
    if (aPoint.IsInTextNode() && !aPoint.IsEndOfContainer() &&
        HTMLEditUtils::IsSimplyEditableNode(*aPoint.ContainerAsText())) {
      return EditorDOMPointType(aPoint.ContainerAsText(), aPoint.Offset());
    }
    return WSRunScanner(aEditingHost, aPoint)
        .GetInclusiveNextEditableCharPoint<EditorDOMPointType>(aPoint);
  }

  /**
   * GetPreviousEditableCharPoint() returns a point in a text node which
   * is at previous editable character.
   */
  template <typename EditorDOMPointType = EditorDOMPointInText, typename PT,
            typename CT>
  static EditorDOMPointType GetPreviousEditableCharPoint(
      Element* aEditingHost, const EditorDOMPointBase<PT, CT>& aPoint) {
    if (aPoint.IsInTextNode() && !aPoint.IsStartOfContainer() &&
        HTMLEditUtils::IsSimplyEditableNode(*aPoint.ContainerAsText())) {
      return EditorDOMPointType(aPoint.ContainerAsText(), aPoint.Offset() - 1);
    }
    return WSRunScanner(aEditingHost, aPoint)
        .GetPreviousEditableCharPoint<EditorDOMPointType>(aPoint);
  }

  /**
   * Scan aTextNode from end or start to find last or first visible things.
   * I.e., this returns a point immediately before or after invisible
   * white-spaces of aTextNode if aTextNode ends or begins with some invisible
   * white-spaces.
   * Note that the result may not be in different text node if aTextNode has
   * only invisible white-spaces and there is previous or next text node.
   */
  template <typename EditorDOMPointType>
  static EditorDOMPointType GetAfterLastVisiblePoint(
      Text& aTextNode, const Element* aAncestorLimiter);
  template <typename EditorDOMPointType>
  static EditorDOMPointType GetFirstVisiblePoint(
      Text& aTextNode, const Element* aAncestorLimiter);

  /**
   * GetRangeInTextNodesToForwardDeleteFrom() returns the range to remove
   * text when caret is at aPoint.
   */
  static Result<EditorDOMRangeInTexts, nsresult>
  GetRangeInTextNodesToForwardDeleteFrom(Element* aEditingHost,
                                         const EditorDOMPoint& aPoint);

  /**
   * GetRangeInTextNodesToBackspaceFrom() returns the range to remove text
   * when caret is at aPoint.
   */
  static Result<EditorDOMRangeInTexts, nsresult>
  GetRangeInTextNodesToBackspaceFrom(Element* aEditingHost,
                                     const EditorDOMPoint& aPoint);

  /**
   * GetRangesForDeletingAtomicContent() returns the range to delete
   * aAtomicContent.  If it's followed by invisible white-spaces, they will
   * be included into the range.
   */
  static EditorDOMRange GetRangesForDeletingAtomicContent(
      Element* aEditingHost, const nsIContent& aAtomicContent);

  /**
   * GetRangeForDeleteBlockElementBoundaries() returns a range starting from end
   * of aLeftBlockElement to start of aRightBlockElement and extend invisible
   * white-spaces around them.
   *
   * @param aHTMLEditor         The HTML editor.
   * @param aLeftBlockElement   The block element which will be joined with
   *                            aRightBlockElement.
   * @param aRightBlockElement  The block element which will be joined with
   *                            aLeftBlockElement.  This must be an element
   *                            after aLeftBlockElement.
   * @param aPointContainingTheOtherBlock
   *                            When aRightBlockElement is an ancestor of
   *                            aLeftBlockElement, this must be set and the
   *                            container must be aRightBlockElement.
   *                            When aLeftBlockElement is an ancestor of
   *                            aRightBlockElement, this must be set and the
   *                            container must be aLeftBlockElement.
   *                            Otherwise, must not be set.
   */
  static EditorDOMRange GetRangeForDeletingBlockElementBoundaries(
      const HTMLEditor& aHTMLEditor, const Element& aLeftBlockElement,
      const Element& aRightBlockElement,
      const EditorDOMPoint& aPointContainingTheOtherBlock);

  /**
   * ShrinkRangeIfStartsFromOrEndsAfterAtomicContent() may shrink aRange if it
   * starts and/or ends with an atomic content, but the range boundary
   * is in adjacent text nodes.  Returns true if this modifies the range.
   */
  static Result<bool, nsresult> ShrinkRangeIfStartsFromOrEndsAfterAtomicContent(
      const HTMLEditor& aHTMLEditor, nsRange& aRange,
      const Element* aEditingHost);

  /**
   * GetRangeContainingInvisibleWhiteSpacesAtRangeBoundaries() returns
   * extended range if range boundaries of aRange are in invisible white-spaces.
   */
  static EditorDOMRange GetRangeContainingInvisibleWhiteSpacesAtRangeBoundaries(
      Element* aEditingHost, const EditorDOMRange& aRange);

  /**
   * GetPrecedingBRElementUnlessVisibleContentFound() scans a `<br>` element
   * backward, but stops scanning it if the scanner finds visible character
   * or something.  In other words, this method ignores only invisible
   * white-spaces between `<br>` element and aPoint.
   */
  template <typename EditorDOMPointType>
  MOZ_NEVER_INLINE_DEBUG static HTMLBRElement*
  GetPrecedingBRElementUnlessVisibleContentFound(
      Element* aEditingHost, const EditorDOMPointType& aPoint) {
    MOZ_ASSERT(aPoint.IsSetAndValid());
    // XXX This method behaves differently even in similar point.
    //     If aPoint is in a text node following `<br>` element, reaches the
    //     `<br>` element when all characters between the `<br>` and
    //     aPoint are ASCII whitespaces.
    //     But if aPoint is not in a text node, e.g., at start of an inline
    //     element which is immediately after a `<br>` element, returns the
    //     `<br>` element even if there is no invisible white-spaces.
    if (aPoint.IsStartOfContainer()) {
      return nullptr;
    }
    // TODO: Scan for end boundary is redundant in this case, we should optimize
    //       it.
    TextFragmentData textFragmentData(aPoint, aEditingHost);
    return textFragmentData.StartsFromBRElement()
               ? textFragmentData.StartReasonBRElementPtr()
               : nullptr;
  }

  const EditorDOMPoint& ScanStartRef() const { return mScanStartPoint; }

  /**
   * GetStartReasonContent() and GetEndReasonContent() return a node which
   * was found by scanning from mScanStartPoint backward or  forward.  If there
   * was white-spaces or text from the point, returns the text node.  Otherwise,
   * returns an element which is explained by the following methods.  Note that
   * when the reason is WSType::CurrentBlockBoundary, In most cases, it's
   * current block element which is editable, but also may be non-element and/or
   * non-editable.  See MOZ_ASSERT_IF()s in WSScanResult::AssertIfInvalidData()
   * for the detail.
   */
  nsIContent* GetStartReasonContent() const {
    return TextFragmentDataAtStartRef().GetStartReasonContent();
  }
  nsIContent* GetEndReasonContent() const {
    return TextFragmentDataAtStartRef().GetEndReasonContent();
  }

  bool StartsFromNonCollapsibleCharacters() const {
    return TextFragmentDataAtStartRef().StartsFromNonCollapsibleCharacters();
  }
  bool StartsFromSpecialContent() const {
    return TextFragmentDataAtStartRef().StartsFromSpecialContent();
  }
  bool StartsFromBRElement() const {
    return TextFragmentDataAtStartRef().StartsFromBRElement();
  }
  bool StartsFromVisibleBRElement() const {
    return TextFragmentDataAtStartRef().StartsFromVisibleBRElement();
  }
  bool StartsFromInvisibleBRElement() const {
    return TextFragmentDataAtStartRef().StartsFromInvisibleBRElement();
  }
  bool StartsFromPreformattedLineBreak() const {
    return TextFragmentDataAtStartRef().StartsFromPreformattedLineBreak();
  }
  bool StartsFromCurrentBlockBoundary() const {
    return TextFragmentDataAtStartRef().StartsFromCurrentBlockBoundary();
  }
  bool StartsFromOtherBlockElement() const {
    return TextFragmentDataAtStartRef().StartsFromOtherBlockElement();
  }
  bool StartsFromBlockBoundary() const {
    return TextFragmentDataAtStartRef().StartsFromBlockBoundary();
  }
  bool StartsFromHardLineBreak() const {
    return TextFragmentDataAtStartRef().StartsFromHardLineBreak();
  }
  bool EndsByNonCollapsibleCharacters() const {
    return TextFragmentDataAtStartRef().EndsByNonCollapsibleCharacters();
  }
  bool EndsBySpecialContent() const {
    return TextFragmentDataAtStartRef().EndsBySpecialContent();
  }
  bool EndsByBRElement() const {
    return TextFragmentDataAtStartRef().EndsByBRElement();
  }
  bool EndsByVisibleBRElement() const {
    return TextFragmentDataAtStartRef().EndsByVisibleBRElement();
  }
  bool EndsByInvisibleBRElement() const {
    return TextFragmentDataAtStartRef().EndsByInvisibleBRElement();
  }
  bool EndsByPreformattedLineBreak() const {
    return TextFragmentDataAtStartRef().EndsByPreformattedLineBreak();
  }
  bool EndsByCurrentBlockBoundary() const {
    return TextFragmentDataAtStartRef().EndsByCurrentBlockBoundary();
  }
  bool EndsByOtherBlockElement() const {
    return TextFragmentDataAtStartRef().EndsByOtherBlockElement();
  }
  bool EndsByBlockBoundary() const {
    return TextFragmentDataAtStartRef().EndsByBlockBoundary();
  }

  MOZ_NEVER_INLINE_DEBUG Element* StartReasonOtherBlockElementPtr() const {
    return TextFragmentDataAtStartRef().StartReasonOtherBlockElementPtr();
  }
  MOZ_NEVER_INLINE_DEBUG HTMLBRElement* StartReasonBRElementPtr() const {
    return TextFragmentDataAtStartRef().StartReasonBRElementPtr();
  }
  MOZ_NEVER_INLINE_DEBUG Element* EndReasonOtherBlockElementPtr() const {
    return TextFragmentDataAtStartRef().EndReasonOtherBlockElementPtr();
  }
  MOZ_NEVER_INLINE_DEBUG HTMLBRElement* EndReasonBRElementPtr() const {
    return TextFragmentDataAtStartRef().EndReasonBRElementPtr();
  }

  /**
   * Active editing host when this instance is created.
   */
  Element* GetEditingHost() const { return mEditingHost; }

 protected:
  using EditorType = EditorBase::EditorType;

  class TextFragmentData;

  // VisibleWhiteSpacesData represents 0 or more visible white-spaces.
  class MOZ_STACK_CLASS VisibleWhiteSpacesData final {
   public:
    bool IsInitialized() const {
      return mLeftWSType != WSType::NotInitialized ||
             mRightWSType != WSType::NotInitialized;
    }

    EditorDOMPoint StartRef() const { return mStartPoint; }
    EditorDOMPoint EndRef() const { return mEndPoint; }

    /**
     * Information why the white-spaces start from (i.e., this indicates the
     * previous content type of the fragment).
     */
    bool StartsFromNonCollapsibleCharacters() const {
      return mLeftWSType == WSType::NonCollapsibleCharacters;
    }
    bool StartsFromSpecialContent() const {
      return mLeftWSType == WSType::SpecialContent;
    }
    bool StartsFromPreformattedLineBreak() const {
      return mLeftWSType == WSType::PreformattedLineBreak;
    }

    /**
     * Information why the white-spaces end by (i.e., this indicates the
     * next content type of the fragment).
     */
    bool EndsByNonCollapsibleCharacters() const {
      return mRightWSType == WSType::NonCollapsibleCharacters;
    }
    bool EndsByTrailingWhiteSpaces() const {
      return mRightWSType == WSType::TrailingWhiteSpaces;
    }
    bool EndsBySpecialContent() const {
      return mRightWSType == WSType::SpecialContent;
    }
    bool EndsByBRElement() const { return mRightWSType == WSType::BRElement; }
    bool EndsByPreformattedLineBreak() const {
      return mRightWSType == WSType::PreformattedLineBreak;
    }
    bool EndsByBlockBoundary() const {
      return mRightWSType == WSType::CurrentBlockBoundary ||
             mRightWSType == WSType::OtherBlockBoundary;
    }

    /**
     * ComparePoint() compares aPoint with the white-spaces.
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
      if (StartRef() == aPoint) {
        return PointPosition::StartOfFragment;
      }
      if (EndRef() == aPoint) {
        return PointPosition::EndOfFragment;
      }
      const bool startIsBeforePoint = StartRef().IsBefore(aPoint);
      const bool pointIsBeforeEnd = aPoint.IsBefore(EndRef());
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
    // Initializers should be accessible only from `TextFragmentData`.
    friend class WSRunScanner::TextFragmentData;
    VisibleWhiteSpacesData()
        : mLeftWSType(WSType::NotInitialized),
          mRightWSType(WSType::NotInitialized) {}

    template <typename EditorDOMPointType>
    void SetStartPoint(const EditorDOMPointType& aStartPoint) {
      mStartPoint = aStartPoint;
    }
    template <typename EditorDOMPointType>
    void SetEndPoint(const EditorDOMPointType& aEndPoint) {
      mEndPoint = aEndPoint;
    }
    void SetStartFrom(WSType aLeftWSType) { mLeftWSType = aLeftWSType; }
    void SetStartFromLeadingWhiteSpaces() {
      mLeftWSType = WSType::LeadingWhiteSpaces;
    }
    void SetEndBy(WSType aRightWSType) { mRightWSType = aRightWSType; }
    void SetEndByTrailingWhiteSpaces() {
      mRightWSType = WSType::TrailingWhiteSpaces;
    }

    EditorDOMPoint mStartPoint;
    EditorDOMPoint mEndPoint;
    WSType mLeftWSType, mRightWSType;
  };

  using PointPosition = VisibleWhiteSpacesData::PointPosition;

  /**
   * GetInclusiveNextEditableCharPoint() returns aPoint if it points a character
   * in an editable text node, or start of next editable text node otherwise.
   * FYI: For the performance, this does not check whether given container
   *      is not after mStart.mReasonContent or not.
   */
  template <typename EditorDOMPointType = EditorDOMPointInText, typename PT,
            typename CT>
  EditorDOMPointType GetInclusiveNextEditableCharPoint(
      const EditorDOMPointBase<PT, CT>& aPoint) const {
    return TextFragmentDataAtStartRef()
        .GetInclusiveNextEditableCharPoint<EditorDOMPointType>(aPoint);
  }

  /**
   * GetPreviousEditableCharPoint() returns previous editable point in a
   * text node.  Note that this returns last character point when it meets
   * non-empty text node, otherwise, returns a point in an empty text node.
   * FYI: For the performance, this does not check whether given container
   *      is not before mEnd.mReasonContent or not.
   */
  template <typename EditorDOMPointType = EditorDOMPointInText, typename PT,
            typename CT>
  EditorDOMPointType GetPreviousEditableCharPoint(
      const EditorDOMPointBase<PT, CT>& aPoint) const {
    return TextFragmentDataAtStartRef()
        .GetPreviousEditableCharPoint<EditorDOMPointType>(aPoint);
  }

  /**
   * GetEndOfCollapsibleASCIIWhiteSpaces() returns the next visible char
   * (meaning a character except ASCII white-spaces) point or end of last text
   * node scanning from aPointAtASCIIWhiteSpace.
   * Note that this may return different text node from the container of
   * aPointAtASCIIWhiteSpace.
   */
  template <typename EditorDOMPointType = EditorDOMPointInText>
  EditorDOMPointType GetEndOfCollapsibleASCIIWhiteSpaces(
      const EditorDOMPointInText& aPointAtASCIIWhiteSpace,
      nsIEditor::EDirection aDirectionToDelete) const {
    MOZ_ASSERT(aDirectionToDelete == nsIEditor::eNone ||
               aDirectionToDelete == nsIEditor::eNext ||
               aDirectionToDelete == nsIEditor::ePrevious);
    return TextFragmentDataAtStartRef()
        .GetEndOfCollapsibleASCIIWhiteSpaces<EditorDOMPointType>(
            aPointAtASCIIWhiteSpace, aDirectionToDelete);
  }

  /**
   * GetFirstASCIIWhiteSpacePointCollapsedTo() returns the first ASCII
   * white-space which aPointAtASCIIWhiteSpace belongs to.  In other words,
   * the white-space at aPointAtASCIIWhiteSpace should be collapsed into
   * the result.
   * Note that this may return different text node from the container of
   * aPointAtASCIIWhiteSpace.
   */
  template <typename EditorDOMPointType = EditorDOMPointInText>
  EditorDOMPointType GetFirstASCIIWhiteSpacePointCollapsedTo(
      const EditorDOMPointInText& aPointAtASCIIWhiteSpace,
      nsIEditor::EDirection aDirectionToDelete) const {
    MOZ_ASSERT(aDirectionToDelete == nsIEditor::eNone ||
               aDirectionToDelete == nsIEditor::eNext ||
               aDirectionToDelete == nsIEditor::ePrevious);
    return TextFragmentDataAtStartRef()
        .GetFirstASCIIWhiteSpacePointCollapsedTo<EditorDOMPointType>(
            aPointAtASCIIWhiteSpace, aDirectionToDelete);
  }

  EditorDOMPointInText GetPreviousCharPointFromPointInText(
      const EditorDOMPointInText& aPoint) const;

  char16_t GetCharAt(Text* aTextNode, uint32_t aOffset) const;

  /**
   * TextFragmentData stores the information of white-space sequence which
   * contains `aPoint` of the constructor.
   */
  class MOZ_STACK_CLASS TextFragmentData final {
   private:
    class NoBreakingSpaceData;
    class MOZ_STACK_CLASS BoundaryData final {
     public:
      using NoBreakingSpaceData =
          WSRunScanner::TextFragmentData::NoBreakingSpaceData;

      /**
       * ScanCollapsibleWhiteSpaceStartFrom() returns start boundary data of
       * white-spaces containing aPoint.  When aPoint is in a text node and
       * points a non-white-space character or the text node is preformatted,
       * this returns the data at aPoint.
       *
       * @param aPoint            Scan start point.
       * @param aEditableBlockParentOrTopmostEditableInlineElement
       *                          Nearest editable block parent element of
       *                          aPoint if there is.  Otherwise, inline editing
       *                          host.
       * @param aEditingHost      Active editing host.
       * @param aNBSPData         Optional.  If set, this recodes first and last
       *                          NBSP positions.
       */
      template <typename EditorDOMPointType>
      static BoundaryData ScanCollapsibleWhiteSpaceStartFrom(
          const EditorDOMPointType& aPoint,
          const Element& aEditableBlockParentOrTopmostEditableInlineElement,
          const Element* aEditingHost, NoBreakingSpaceData* aNBSPData);

      /**
       * ScanCollapsibleWhiteSpaceEndFrom() returns end boundary data of
       * white-spaces containing aPoint.  When aPoint is in a text node and
       * points a non-white-space character or the text node is preformatted,
       * this returns the data at aPoint.
       *
       * @param aPoint            Scan start point.
       * @param aEditableBlockParentOrTopmostEditableInlineElement
       *                          Nearest editable block parent element of
       *                          aPoint if there is.  Otherwise, inline editing
       *                          host.
       * @param aEditingHost      Active editing host.
       * @param aNBSPData         Optional.  If set, this recodes first and last
       *                          NBSP positions.
       */
      template <typename EditorDOMPointType>
      static BoundaryData ScanCollapsibleWhiteSpaceEndFrom(
          const EditorDOMPointType& aPoint,
          const Element& aEditableBlockParentOrTopmostEditableInlineElement,
          const Element* aEditingHost, NoBreakingSpaceData* aNBSPData);

      BoundaryData() : mReason(WSType::NotInitialized) {}
      template <typename EditorDOMPointType>
      BoundaryData(const EditorDOMPointType& aPoint, nsIContent& aReasonContent,
                   WSType aReason)
          : mReasonContent(&aReasonContent),
            mPoint(aPoint.template To<EditorDOMPoint>()),
            mReason(aReason) {}
      bool Initialized() const { return mReasonContent && mPoint.IsSet(); }

      nsIContent* GetReasonContent() const { return mReasonContent; }
      const EditorDOMPoint& PointRef() const { return mPoint; }
      WSType RawReason() const { return mReason; }

      bool IsNonCollapsibleCharacters() const {
        return mReason == WSType::NonCollapsibleCharacters;
      }
      bool IsSpecialContent() const {
        return mReason == WSType::SpecialContent;
      }
      bool IsBRElement() const { return mReason == WSType::BRElement; }
      bool IsPreformattedLineBreak() const {
        return mReason == WSType::PreformattedLineBreak;
      }
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
               mReason == WSType::BRElement ||
               mReason == WSType::PreformattedLineBreak;
      }
      MOZ_NEVER_INLINE_DEBUG Element* OtherBlockElementPtr() const {
        MOZ_DIAGNOSTIC_ASSERT(mReasonContent->IsElement());
        return mReasonContent->AsElement();
      }
      MOZ_NEVER_INLINE_DEBUG HTMLBRElement* BRElementPtr() const {
        MOZ_DIAGNOSTIC_ASSERT(mReasonContent->IsHTMLElement(nsGkAtoms::br));
        return static_cast<HTMLBRElement*>(mReasonContent.get());
      }

     private:
      /**
       * Helper methods of ScanCollapsibleWhiteSpaceStartFrom() and
       * ScanCollapsibleWhiteSpaceEndFrom() when they need to scan in a text
       * node.
       */
      template <typename EditorDOMPointType>
      static Maybe<BoundaryData> ScanCollapsibleWhiteSpaceStartInTextNode(
          const EditorDOMPointType& aPoint, NoBreakingSpaceData* aNBSPData);
      template <typename EditorDOMPointType>
      static Maybe<BoundaryData> ScanCollapsibleWhiteSpaceEndInTextNode(
          const EditorDOMPointType& aPoint, NoBreakingSpaceData* aNBSPData);

      nsCOMPtr<nsIContent> mReasonContent;
      EditorDOMPoint mPoint;
      // Must be one of WSType::NotInitialized,
      // WSType::NonCollapsibleCharacters, WSType::SpecialContent,
      // WSType::BRElement, WSType::CurrentBlockBoundary or
      // WSType::OtherBlockBoundary.
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

   public:
    TextFragmentData() = delete;
    template <typename EditorDOMPointType>
    TextFragmentData(const EditorDOMPointType& aPoint,
                     const Element* aEditingHost);

    bool IsInitialized() const {
      return mStart.Initialized() && mEnd.Initialized();
    }

    nsIContent* GetStartReasonContent() const {
      return mStart.GetReasonContent();
    }
    nsIContent* GetEndReasonContent() const { return mEnd.GetReasonContent(); }

    bool StartsFromNonCollapsibleCharacters() const {
      return mStart.IsNonCollapsibleCharacters();
    }
    bool StartsFromSpecialContent() const { return mStart.IsSpecialContent(); }
    bool StartsFromBRElement() const { return mStart.IsBRElement(); }
    bool StartsFromVisibleBRElement() const {
      return StartsFromBRElement() &&
             HTMLEditUtils::IsVisibleBRElement(*GetStartReasonContent());
    }
    bool StartsFromInvisibleBRElement() const {
      return StartsFromBRElement() &&
             HTMLEditUtils::IsInvisibleBRElement(*GetStartReasonContent());
    }
    bool StartsFromPreformattedLineBreak() const {
      return mStart.IsPreformattedLineBreak();
    }
    bool StartsFromCurrentBlockBoundary() const {
      return mStart.IsCurrentBlockBoundary();
    }
    bool StartsFromOtherBlockElement() const {
      return mStart.IsOtherBlockBoundary();
    }
    bool StartsFromBlockBoundary() const { return mStart.IsBlockBoundary(); }
    bool StartsFromHardLineBreak() const { return mStart.IsHardLineBreak(); }
    bool EndsByNonCollapsibleCharacters() const {
      return mEnd.IsNonCollapsibleCharacters();
    }
    bool EndsBySpecialContent() const { return mEnd.IsSpecialContent(); }
    bool EndsByBRElement() const { return mEnd.IsBRElement(); }
    bool EndsByVisibleBRElement() const {
      return EndsByBRElement() &&
             HTMLEditUtils::IsVisibleBRElement(*GetEndReasonContent());
    }
    bool EndsByInvisibleBRElement() const {
      return EndsByBRElement() &&
             HTMLEditUtils::IsInvisibleBRElement(*GetEndReasonContent());
    }
    bool EndsByPreformattedLineBreak() const {
      return mEnd.IsPreformattedLineBreak();
    }
    bool EndsByInvisiblePreformattedLineBreak() const {
      return mEnd.IsPreformattedLineBreak() &&
             HTMLEditUtils::IsInvisiblePreformattedNewLine(mEnd.PointRef());
    }
    bool EndsByCurrentBlockBoundary() const {
      return mEnd.IsCurrentBlockBoundary();
    }
    bool EndsByOtherBlockElement() const { return mEnd.IsOtherBlockBoundary(); }
    bool EndsByBlockBoundary() const { return mEnd.IsBlockBoundary(); }

    WSType StartRawReason() const { return mStart.RawReason(); }
    WSType EndRawReason() const { return mEnd.RawReason(); }

    MOZ_NEVER_INLINE_DEBUG Element* StartReasonOtherBlockElementPtr() const {
      return mStart.OtherBlockElementPtr();
    }
    MOZ_NEVER_INLINE_DEBUG HTMLBRElement* StartReasonBRElementPtr() const {
      return mStart.BRElementPtr();
    }
    MOZ_NEVER_INLINE_DEBUG Element* EndReasonOtherBlockElementPtr() const {
      return mEnd.OtherBlockElementPtr();
    }
    MOZ_NEVER_INLINE_DEBUG HTMLBRElement* EndReasonBRElementPtr() const {
      return mEnd.BRElementPtr();
    }

    const EditorDOMPoint& StartRef() const { return mStart.PointRef(); }
    const EditorDOMPoint& EndRef() const { return mEnd.PointRef(); }

    const EditorDOMPoint& ScanStartRef() const { return mScanStartPoint; }

    bool FoundNoBreakingWhiteSpaces() const { return mNBSPData.FoundNBSP(); }
    const EditorDOMPointInText& FirstNBSPPointRef() const {
      return mNBSPData.FirstPointRef();
    }
    const EditorDOMPointInText& LastNBSPPointRef() const {
      return mNBSPData.LastPointRef();
    }

    template <typename EditorDOMPointType = EditorDOMPointInText, typename PT,
              typename CT>
    EditorDOMPointType GetInclusiveNextEditableCharPoint(
        const EditorDOMPointBase<PT, CT>& aPoint) const;
    template <typename EditorDOMPointType = EditorDOMPointInText, typename PT,
              typename CT>
    EditorDOMPointType GetPreviousEditableCharPoint(
        const EditorDOMPointBase<PT, CT>& aPoint) const;

    template <typename EditorDOMPointType = EditorDOMPointInText>
    EditorDOMPointType GetEndOfCollapsibleASCIIWhiteSpaces(
        const EditorDOMPointInText& aPointAtASCIIWhiteSpace,
        nsIEditor::EDirection aDirectionToDelete) const;
    template <typename EditorDOMPointType = EditorDOMPointInText>
    EditorDOMPointType GetFirstASCIIWhiteSpacePointCollapsedTo(
        const EditorDOMPointInText& aPointAtASCIIWhiteSpace,
        nsIEditor::EDirection aDirectionToDelete) const;

    /**
     * GetNonCollapsedRangeInTexts() returns non-empty range in texts which
     * is the largest range in aRange if there is some text nodes.
     */
    EditorDOMRangeInTexts GetNonCollapsedRangeInTexts(
        const EditorDOMRange& aRange) const;

    /**
     * InvisibleLeadingWhiteSpaceRangeRef() retruns reference to two DOM points,
     * start of the line and first visible point or end of the hard line.  When
     * this returns non-positioned range or positioned but collapsed range,
     * there is no invisible leading white-spaces.
     * Note that if there are only invisible white-spaces in a hard line,
     * this returns all of the white-spaces.
     */
    const EditorDOMRange& InvisibleLeadingWhiteSpaceRangeRef() const;

    /**
     * InvisibleTrailingWhiteSpaceRangeRef() returns reference to two DOM
     * points, first invisible white-space and end of the hard line.  When this
     * returns non-positioned range or positioned but collapsed range,
     * there is no invisible trailing white-spaces.
     * Note that if there are only invisible white-spaces in a hard line,
     * this returns all of the white-spaces.
     */
    const EditorDOMRange& InvisibleTrailingWhiteSpaceRangeRef() const;

    /**
     * GetNewInvisibleLeadingWhiteSpaceRangeIfSplittingAt() returns new
     * invisible leading white-space range which should be removed if
     * splitting invisible white-space sequence at aPointToSplit creates
     * new invisible leading white-spaces in the new line.
     * Note that the result may be collapsed range if the point is around
     * invisible white-spaces.
     */
    template <typename EditorDOMPointType>
    EditorDOMRange GetNewInvisibleLeadingWhiteSpaceRangeIfSplittingAt(
        const EditorDOMPointType& aPointToSplit) const {
      // If there are invisible trailing white-spaces and some or all of them
      // become invisible leading white-spaces in the new line, although we
      // don't need to delete them, but for aesthetically and backward
      // compatibility, we should remove them.
      const EditorDOMRange& trailingWhiteSpaceRange =
          InvisibleTrailingWhiteSpaceRangeRef();
      // XXX Why don't we check leading white-spaces too?
      if (!trailingWhiteSpaceRange.IsPositioned()) {
        return trailingWhiteSpaceRange;
      }
      // If the point is before the trailing white-spaces, the new line won't
      // start with leading white-spaces.
      if (aPointToSplit.IsBefore(trailingWhiteSpaceRange.StartRef())) {
        return EditorDOMRange();
      }
      // If the point is in the trailing white-spaces, the new line may
      // start with some leading white-spaces.  Returning collapsed range
      // is intentional because the caller may want to know whether the
      // point is in trailing white-spaces or not.
      if (aPointToSplit.EqualsOrIsBefore(trailingWhiteSpaceRange.EndRef())) {
        return EditorDOMRange(trailingWhiteSpaceRange.StartRef(),
                              aPointToSplit);
      }
      // Otherwise, if the point is after the trailing white-spaces, it may
      // be just outside of the text node.  E.g., end of parent element.
      // This is possible case but the validation cost is not worthwhile
      // due to the runtime cost in the worst case.  Therefore, we should just
      // return collapsed range at the end of trailing white-spaces.  Then,
      // callers can know the point is immediately after the trailing
      // white-spaces.
      return EditorDOMRange(trailingWhiteSpaceRange.EndRef());
    }

    /**
     * GetNewInvisibleTrailingWhiteSpaceRangeIfSplittingAt() returns new
     * invisible trailing white-space range which should be removed if
     * splitting invisible white-space sequence at aPointToSplit creates
     * new invisible trailing white-spaces in the new line.
     * Note that the result may be collapsed range if the point is around
     * invisible white-spaces.
     */
    template <typename EditorDOMPointType>
    EditorDOMRange GetNewInvisibleTrailingWhiteSpaceRangeIfSplittingAt(
        const EditorDOMPointType& aPointToSplit) const {
      // If there are invisible leading white-spaces and some or all of them
      // become end of current line, they will become visible.  Therefore, we
      // need to delete the invisible leading white-spaces before insertion
      // point.
      const EditorDOMRange& leadingWhiteSpaceRange =
          InvisibleLeadingWhiteSpaceRangeRef();
      if (!leadingWhiteSpaceRange.IsPositioned()) {
        return leadingWhiteSpaceRange;
      }
      // If the point equals or is after the leading white-spaces, the line
      // will end without trailing white-spaces.
      if (leadingWhiteSpaceRange.EndRef().IsBefore(aPointToSplit)) {
        return EditorDOMRange();
      }
      // If the point is in the leading white-spaces, the line may
      // end with some trailing white-spaces.  Returning collapsed range
      // is intentional because the caller may want to know whether the
      // point is in leading white-spaces or not.
      if (leadingWhiteSpaceRange.StartRef().EqualsOrIsBefore(aPointToSplit)) {
        return EditorDOMRange(aPointToSplit, leadingWhiteSpaceRange.EndRef());
      }
      // Otherwise, if the point is before the leading white-spaces, it may
      // be just outside of the text node.  E.g., start of parent element.
      // This is possible case but the validation cost is not worthwhile
      // due to the runtime cost in the worst case.  Therefore, we should
      // just return collapsed range at start of the leading white-spaces.
      // Then, callers can know the point is immediately before the leading
      // white-spaces.
      return EditorDOMRange(leadingWhiteSpaceRange.StartRef());
    }

    /**
     * FollowingContentMayBecomeFirstVisibleContent() returns true if some
     * content may be first visible content after removing content after aPoint.
     * Note that it's completely broken what this does.  Don't use this method
     * with new code.
     */
    template <typename EditorDOMPointType>
    bool FollowingContentMayBecomeFirstVisibleContent(
        const EditorDOMPointType& aPoint) const {
      MOZ_ASSERT(aPoint.IsSetAndValid());
      if (!mStart.IsHardLineBreak()) {
        return false;
      }
      // If the point is before start of text fragment, that means that the
      // point may be at the block boundary or inline element boundary.
      if (aPoint.EqualsOrIsBefore(mStart.PointRef())) {
        return true;
      }
      // VisibleWhiteSpacesData is marked as start of line only when it
      // represents leading white-spaces.
      const EditorDOMRange& leadingWhiteSpaceRange =
          InvisibleLeadingWhiteSpaceRangeRef();
      if (!leadingWhiteSpaceRange.StartRef().IsSet()) {
        return false;
      }
      if (aPoint.EqualsOrIsBefore(leadingWhiteSpaceRange.StartRef())) {
        return true;
      }
      if (!leadingWhiteSpaceRange.EndRef().IsSet()) {
        return false;
      }
      return aPoint.EqualsOrIsBefore(leadingWhiteSpaceRange.EndRef());
    }

    /**
     * PrecedingContentMayBecomeInvisible() returns true if end of preceding
     * content is collapsed (when ends with an ASCII white-space).
     * Note that it's completely broken what this does.  Don't use this method
     * with new code.
     */
    template <typename EditorDOMPointType>
    bool PrecedingContentMayBecomeInvisible(
        const EditorDOMPointType& aPoint) const {
      MOZ_ASSERT(aPoint.IsSetAndValid());
      // If this fragment is ends by block boundary, always the caller needs
      // additional check.
      if (mEnd.IsBlockBoundary()) {
        return true;
      }

      // If the point is in visible white-spaces and ends with an ASCII
      // white-space, it may be collapsed even if it won't be end of line.
      const VisibleWhiteSpacesData& visibleWhiteSpaces =
          VisibleWhiteSpacesDataRef();
      if (!visibleWhiteSpaces.IsInitialized()) {
        return false;
      }
      // XXX Odd case, but keep traditional behavior of `FindNearestRun()`.
      if (!visibleWhiteSpaces.StartRef().IsSet()) {
        return true;
      }
      if (!visibleWhiteSpaces.StartRef().EqualsOrIsBefore(aPoint)) {
        return false;
      }
      // XXX Odd case, but keep traditional behavior of `FindNearestRun()`.
      if (visibleWhiteSpaces.EndsByTrailingWhiteSpaces()) {
        return true;
      }
      // XXX Must be a bug.  This claims that the caller needs additional
      // check even when there is no white-spaces.
      if (visibleWhiteSpaces.StartRef() == visibleWhiteSpaces.EndRef()) {
        return true;
      }
      return aPoint.IsBefore(visibleWhiteSpaces.EndRef());
    }

    /**
     * GetPreviousNBSPPointIfNeedToReplaceWithASCIIWhiteSpace() may return an
     * NBSP point which should be replaced with an ASCII white-space when we're
     * inserting text into aPointToInsert. Note that this is a helper method for
     * the traditional white-space normalizer.  Don't use this with the new
     * white-space normalizer.
     * Must be called only when VisibleWhiteSpacesDataRef() returns initialized
     * instance and previous character of aPointToInsert is in the range.
     */
    EditorDOMPointInText GetPreviousNBSPPointIfNeedToReplaceWithASCIIWhiteSpace(
        const EditorDOMPoint& aPointToInsert) const;

    /**
     * GetInclusiveNextNBSPPointIfNeedToReplaceWithASCIIWhiteSpace() may return
     * an NBSP point which should be replaced with an ASCII white-space when
     * the caller inserts text into aPointToInsert.
     * Note that this is a helper method for the traditional white-space
     * normalizer.  Don't use this with the new white-space normalizer.
     * Must be called only when VisibleWhiteSpacesDataRef() returns initialized
     * instance, and inclusive next char of aPointToInsert is in the range.
     */
    EditorDOMPointInText
    GetInclusiveNextNBSPPointIfNeedToReplaceWithASCIIWhiteSpace(
        const EditorDOMPoint& aPointToInsert) const;

    /**
     * GetReplaceRangeDataAtEndOfDeletionRange() and
     * GetReplaceRangeDataAtStartOfDeletionRange() return delete range if
     * end or start of deleting range splits invisible trailing/leading
     * white-spaces and it may become visible, or return replace range if
     * end or start of deleting range splits visible white-spaces and it
     * causes some ASCII white-spaces become invisible unless replacing
     * with an NBSP.
     */
    ReplaceRangeData GetReplaceRangeDataAtEndOfDeletionRange(
        const TextFragmentData& aTextFragmentDataAtStartToDelete) const;
    ReplaceRangeData GetReplaceRangeDataAtStartOfDeletionRange(
        const TextFragmentData& aTextFragmentDataAtEndToDelete) const;

    /**
     * VisibleWhiteSpacesDataRef() returns reference to visible white-spaces
     * data. That is zero or more white-spaces which are visible.
     * Note that when there is no visible content, it's not initialized.
     * Otherwise, even if there is no white-spaces, it's initialized and
     * the range is collapsed in such case.
     */
    const VisibleWhiteSpacesData& VisibleWhiteSpacesDataRef() const;

   private:
    EditorDOMPoint mScanStartPoint;
    BoundaryData mStart;
    BoundaryData mEnd;
    NoBreakingSpaceData mNBSPData;
    RefPtr<const Element> mEditingHost;
    mutable Maybe<EditorDOMRange> mLeadingWhiteSpaceRange;
    mutable Maybe<EditorDOMRange> mTrailingWhiteSpaceRange;
    mutable Maybe<VisibleWhiteSpacesData> mVisibleWhiteSpacesData;
  };

  const TextFragmentData& TextFragmentDataAtStartRef() const {
    return mTextFragmentDataAtStart;
  }

  // The node passed to our constructor.
  EditorDOMPoint mScanStartPoint;
  // Together, the above represent the point at which we are building up ws
  // info.

  // The editing host when the instance is created.
  RefPtr<Element> mEditingHost;

 private:
  /**
   * ComputeRangeInTextNodesContainingInvisibleWhiteSpaces() returns range
   * containing invisible white-spaces if deleting between aStart and aEnd
   * causes them become visible.
   *
   * @param aStart      TextFragmentData at start of deleting range.
   *                    This must be initialized with DOM point in a text node.
   * @param aEnd        TextFragmentData at end of deleting range.
   *                    This must be initialized with DOM point in a text node.
   */
  static EditorDOMRangeInTexts
  ComputeRangeInTextNodesContainingInvisibleWhiteSpaces(
      const TextFragmentData& aStart, const TextFragmentData& aEnd);

  TextFragmentData mTextFragmentDataAtStart;

  friend class WhiteSpaceVisibilityKeeper;
};

/**
 * WhiteSpaceVisibilityKeeper class helps `HTMLEditor` modifying the DOM tree
 * with keeps white-space sequence visibility automatically.  E.g., invisible
 * leading/trailing white-spaces becomes visible, this class members delete
 * them.  E.g., when splitting visible-white-space sequence, this class may
 * replace ASCII white-spaces at split edges with NBSPs.
 */
class WhiteSpaceVisibilityKeeper final {
 private:
  using AutoTransactionsConserveSelection =
      EditorBase::AutoTransactionsConserveSelection;
  using EditorType = EditorBase::EditorType;
  using PointPosition = WSRunScanner::PointPosition;
  using TextFragmentData = WSRunScanner::TextFragmentData;
  using VisibleWhiteSpacesData = WSRunScanner::VisibleWhiteSpacesData;

 public:
  WhiteSpaceVisibilityKeeper() = delete;
  explicit WhiteSpaceVisibilityKeeper(
      const WhiteSpaceVisibilityKeeper& aOther) = delete;
  WhiteSpaceVisibilityKeeper(WhiteSpaceVisibilityKeeper&& aOther) = delete;

  /**
   * DeleteInvisibleASCIIWhiteSpaces() removes invisible leading white-spaces
   * and trailing white-spaces if there are around aPoint.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static nsresult
  DeleteInvisibleASCIIWhiteSpaces(HTMLEditor& aHTMLEditor,
                                  const EditorDOMPoint& aPoint);

  // PrepareToDeleteRange fixes up ws before aStartPoint and after aEndPoint in
  // preperation for content in that range to be deleted.  Note that the nodes
  // and offsets are adjusted in response to any dom changes we make while
  // adjusting ws.
  // example of fixup: trailingws before aStartPoint needs to be removed.
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static nsresult
  PrepareToDeleteRangeAndTrackPoints(HTMLEditor& aHTMLEditor,
                                     EditorDOMPoint* aStartPoint,
                                     EditorDOMPoint* aEndPoint) {
    MOZ_ASSERT(aStartPoint->IsSetAndValid());
    MOZ_ASSERT(aEndPoint->IsSetAndValid());
    AutoTrackDOMPoint trackerStart(aHTMLEditor.RangeUpdaterRef(), aStartPoint);
    AutoTrackDOMPoint trackerEnd(aHTMLEditor.RangeUpdaterRef(), aEndPoint);
    return WhiteSpaceVisibilityKeeper::PrepareToDeleteRange(
        aHTMLEditor, EditorDOMRange(*aStartPoint, *aEndPoint));
  }
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static nsresult PrepareToDeleteRange(
      HTMLEditor& aHTMLEditor, const EditorDOMPoint& aStartPoint,
      const EditorDOMPoint& aEndPoint) {
    MOZ_ASSERT(aStartPoint.IsSetAndValid());
    MOZ_ASSERT(aEndPoint.IsSetAndValid());
    return WhiteSpaceVisibilityKeeper::PrepareToDeleteRange(
        aHTMLEditor, EditorDOMRange(aStartPoint, aEndPoint));
  }
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static nsresult PrepareToDeleteRange(
      HTMLEditor& aHTMLEditor, const EditorDOMRange& aRange) {
    MOZ_ASSERT(aRange.IsPositionedAndValid());
    nsresult rv = WhiteSpaceVisibilityKeeper::
        MakeSureToKeepVisibleStateOfWhiteSpacesAroundDeletingRange(aHTMLEditor,
                                                                   aRange);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "WhiteSpaceVisibilityKeeper::"
        "MakeSureToKeepVisibleStateOfWhiteSpacesAroundDeletingRange() failed");
    return rv;
  }

  /**
   * PrepareToSplitBlockElement() makes sure that the invisible white-spaces
   * not to become visible and returns splittable point.
   *
   * @param aHTMLEditor         The HTML editor.
   * @param aPointToSplit       The splitting point in aSplittingBlockElement.
   * @param aSplittingBlockElement  A block element which will be split.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<EditorDOMPoint, nsresult>
  PrepareToSplitBlockElement(HTMLEditor& aHTMLEditor,
                             const EditorDOMPoint& aPointToSplit,
                             const Element& aSplittingBlockElement);

  /**
   * MergeFirstLineOfRightBlockElementIntoDescendantLeftBlockElement() merges
   * first line in aRightBlockElement into end of aLeftBlockElement which
   * is a descendant of aRightBlockElement.
   *
   * @param aHTMLEditor         The HTML editor.
   * @param aLeftBlockElement   The content will be merged into end of
   *                            this element.
   * @param aRightBlockElement  The first line in this element will be
   *                            moved to aLeftBlockElement.
   * @param aAtRightBlockChild  At a child of aRightBlockElement and inclusive
   *                            ancestor of aLeftBlockElement.
   * @param aListElementTagName Set some if aRightBlockElement is a list
   *                            element and it'll be merged with another
   *                            list element.
   * @param aEditingHost        The editing host.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static EditActionResult
  MergeFirstLineOfRightBlockElementIntoDescendantLeftBlockElement(
      HTMLEditor& aHTMLEditor, Element& aLeftBlockElement,
      Element& aRightBlockElement, const EditorDOMPoint& aAtRightBlockChild,
      const Maybe<nsAtom*>& aListElementTagName,
      const HTMLBRElement* aPrecedingInvisibleBRElement,
      const Element& aEditingHost);

  /**
   * MergeFirstLineOfRightBlockElementIntoAncestorLeftBlockElement() merges
   * first line in aRightBlockElement into end of aLeftBlockElement which
   * is an ancestor of aRightBlockElement, then, removes aRightBlockElement
   * if it becomes empty.
   *
   * @param aHTMLEditor         The HTML editor.
   * @param aLeftBlockElement   The content will be merged into end of
   *                            this element.
   * @param aRightBlockElement  The first line in this element will be
   *                            moved to aLeftBlockElement and maybe
   *                            removed when this becomes empty.
   * @param aAtLeftBlockChild   At a child of aLeftBlockElement and inclusive
   *                            ancestor of aRightBlockElement.
   * @param aLeftContentInBlock The content whose inclusive ancestor is
   *                            aLeftBlockElement.
   * @param aListElementTagName Set some if aRightBlockElement is a list
   *                            element and it'll be merged with another
   *                            list element.
   * @param aEditingHost        The editing host.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static EditActionResult
  MergeFirstLineOfRightBlockElementIntoAncestorLeftBlockElement(
      HTMLEditor& aHTMLEditor, Element& aLeftBlockElement,
      Element& aRightBlockElement, const EditorDOMPoint& aAtLeftBlockChild,
      nsIContent& aLeftContentInBlock,
      const Maybe<nsAtom*>& aListElementTagName,
      const HTMLBRElement* aPrecedingInvisibleBRElement,
      const Element& aEditingHost);

  /**
   * MergeFirstLineOfRightBlockElementIntoLeftBlockElement() merges first
   * line in aRightBlockElement into end of aLeftBlockElement and removes
   * aRightBlockElement when it has only one line.
   *
   * @param aHTMLEditor         The HTML editor.
   * @param aLeftBlockElement   The content will be merged into end of
   *                            this element.
   * @param aRightBlockElement  The first line in this element will be
   *                            moved to aLeftBlockElement and maybe
   *                            removed when this becomes empty.
   * @param aListElementTagName Set some if aRightBlockElement is a list
   *                            element and its type needs to be changed.
   * @param aEditingHost        The editing host.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static EditActionResult
  MergeFirstLineOfRightBlockElementIntoLeftBlockElement(
      HTMLEditor& aHTMLEditor, Element& aLeftBlockElement,
      Element& aRightBlockElement, const Maybe<nsAtom*>& aListElementTagName,
      const HTMLBRElement* aPrecedingInvisibleBRElement,
      const Element& aEditingHost);

  /**
   * InsertBRElement() inserts a <br> node at (before) aPointToInsert and delete
   * unnecessary white-spaces around there and/or replaces white-spaces with
   * non-breaking spaces.  Note that if the point is in a text node, the
   * text node will be split and insert new <br> node between the left node
   * and the right node.
   *
   * @param aPointToInsert  The point to insert new <br> element.  Note that
   *                        it'll be inserted before this point.  I.e., the
   *                        point will be the point of new <br>.
   * @return                If succeeded, returns the new <br> element and
   *                        point to put caret.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static CreateElementResult InsertBRElement(
      HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPointToInsert,
      const Element& aEditingHost);

  /**
   * InsertText() inserts aStringToInsert to aPointToInsert and makes any needed
   * adjustments to white-spaces around the insertion point.
   *
   * @param aStringToInsert     The string to insert.
   * @param aRangeToBeReplaced  The range to be deleted.
   * @return                    If succeeded, returns the point after inserted
   *                            aStringToInsert. So, when this method actually
   *                            inserts string, returns a point in the text
   *                            node. Otherwise, may return mScanStartPoint.
   */
  template <typename EditorDOMPointType>
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<EditorDOMPoint, nsresult>
  InsertText(HTMLEditor& aHTMLEditor, const nsAString& aStringToInsert,
             const EditorDOMPointType& aPointToInsert) {
    return WhiteSpaceVisibilityKeeper::ReplaceText(
        aHTMLEditor, aStringToInsert, EditorDOMRange(aPointToInsert));
  }

  /**
   * ReplaceText() repaces aRangeToReplace with aStringToInsert and makes any
   * needed adjustments to white-spaces around both start of the range and
   * end of the range.
   *
   * @param aStringToInsert     The string to insert.
   * @param aRangeToBeReplaced  The range to be deleted.
   * @return                    If succeeded, returns the point after inserted
   *                            aStringToInsert. So, when this method actually
   *                            inserts string, returns a point in the text
   *                            node. Otherwise, may return mScanStartPoint.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<EditorDOMPoint, nsresult>
  ReplaceText(HTMLEditor& aHTMLEditor, const nsAString& aStringToInsert,
              const EditorDOMRange& aRangeToBeReplaced);

  /**
   * DeletePreviousWhiteSpace() deletes previous white-space of aPoint.
   * This automatically keeps visibility of white-spaces around aPoint.
   * E.g., may remove invisible leading white-spaces.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static nsresult DeletePreviousWhiteSpace(
      HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPoint);

  /**
   * DeleteInclusiveNextWhiteSpace() delete inclusive next white-space of
   * aPoint.  This automatically keeps visiblity of white-spaces around aPoint.
   * E.g., may remove invisible trailing white-spaces.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static nsresult
  DeleteInclusiveNextWhiteSpace(HTMLEditor& aHTMLEditor,
                                const EditorDOMPoint& aPoint);

  /**
   * DeleteContentNodeAndJoinTextNodesAroundIt() deletes aContentToDelete and
   * may remove/replace white-spaces around it.  Then, if deleting content makes
   * 2 text nodes around it are adjacent siblings, this joins them and put
   * selection at the joined point.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static nsresult
  DeleteContentNodeAndJoinTextNodesAroundIt(HTMLEditor& aHTMLEditor,
                                            nsIContent& aContentToDelete,
                                            const EditorDOMPoint& aCaretPoint);

  /**
   * NormalizeVisibleWhiteSpacesAt() tries to normalize visible white-space
   * sequence around aPoint.
   */
  template <typename EditorDOMPointType>
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static nsresult
  NormalizeVisibleWhiteSpacesAt(HTMLEditor& aHTMLEditor,
                                const EditorDOMPointType& aPoint);

 private:
  /**
   * MakeSureToKeepVisibleStateOfWhiteSpacesAroundDeletingRange() may delete
   * invisible white-spaces for keeping make them invisible and/or may replace
   * ASCII white-spaces with NBSPs for making visible white-spaces to keep
   * visible.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static nsresult
  MakeSureToKeepVisibleStateOfWhiteSpacesAroundDeletingRange(
      HTMLEditor& aHTMLEditor, const EditorDOMRange& aRangeToDelete);

  /**
   * MakeSureToKeepVisibleWhiteSpacesVisibleAfterSplit() replaces ASCII white-
   * spaces which becomes invisible after split with NBSPs.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static nsresult
  MakeSureToKeepVisibleWhiteSpacesVisibleAfterSplit(
      HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPointToSplit);

  /**
   * ReplaceTextAndRemoveEmptyTextNodes() replaces the range between
   * aRangeToReplace with aReplaceString simply.  Additionally, removes
   * empty text nodes in the range.
   *
   * @param aRangeToReplace     Range to replace text.
   * @param aReplaceString      The new string.  Empty string is allowed.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static nsresult
  ReplaceTextAndRemoveEmptyTextNodes(
      HTMLEditor& aHTMLEditor, const EditorDOMRangeInTexts& aRangeToReplace,
      const nsAString& aReplaceString);
};

}  // namespace mozilla

#endif  // #ifndef WSRunObject_h
