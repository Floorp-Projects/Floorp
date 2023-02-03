/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HTMLEditorNestedClasses_h
#define HTMLEditorNestedClasses_h

#include "EditorDOMPoint.h"
#include "EditorForwards.h"
#include "HTMLEditor.h"       // for HTMLEditor
#include "HTMLEditHelpers.h"  // for EditorInlineStyleAndValue

#include "mozilla/Attributes.h"
#include "mozilla/Result.h"

namespace mozilla {

/*****************************************************************************
 * AutoInlineStyleSetter is a temporary class to set an inline style to
 * specific nodes.
 ****************************************************************************/

class MOZ_STACK_CLASS HTMLEditor::AutoInlineStyleSetter final
    : private EditorInlineStyleAndValue {
  using Element = dom::Element;
  using Text = dom::Text;

 public:
  explicit AutoInlineStyleSetter(
      const EditorInlineStyleAndValue& aStyleAndValue)
      : EditorInlineStyleAndValue(aStyleAndValue) {}

  void Reset() {
    mFirstHandledPoint.Clear();
    mLastHandledPoint.Clear();
  }

  const EditorDOMPoint& FirstHandledPointRef() const {
    return mFirstHandledPoint;
  }
  const EditorDOMPoint& LastHandledPointRef() const {
    return mLastHandledPoint;
  }

  /**
   * Split aText at aStartOffset and aEndOffset (except when they are start or
   * end of its data) and wrap the middle text node in an element to apply the
   * style.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT Result<SplitRangeOffFromNodeResult, nsresult>
  SplitTextNodeAndApplyStyleToMiddleNode(HTMLEditor& aHTMLEditor, Text& aText,
                                         uint32_t aStartOffset,
                                         uint32_t aEndOffset);

  /**
   * Remove same style from children and apply the style entire (except
   * non-editable nodes) aContent.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT Result<CaretPoint, nsresult>
  ApplyStyleToNodeOrChildrenAndRemoveNestedSameStyle(HTMLEditor& aHTMLEditor,
                                                     nsIContent& aContent);

  /**
   * Invert the style with creating new element or something.  This should
   * be called only when IsInvertibleWithCSS() returns true.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
  InvertStyleIfApplied(HTMLEditor& aHTMLEditor, Element& aElement);
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT Result<SplitRangeOffFromNodeResult, nsresult>
  InvertStyleIfApplied(HTMLEditor& aHTMLEditor, Text& aTextNode,
                       uint32_t aStartOffset, uint32_t aEndOffset);

  /**
   * Extend or shrink aRange for applying the style to the range.
   * See comments in the definition what this does.
   */
  Result<EditorRawDOMRange, nsresult> ExtendOrShrinkRangeToApplyTheStyle(
      const HTMLEditor& aHTMLEditor, const EditorDOMRange& aRange,
      const Element& aEditingHost) const;

  /**
   * Returns next/previous sibling of aContent or an ancestor of it if it's
   * editable and does not cross block boundary.
   */
  [[nodiscard]] static nsIContent* GetNextEditableInlineContent(
      const nsIContent& aContent, const nsINode* aLimiter = nullptr);
  [[nodiscard]] static nsIContent* GetPreviousEditableInlineContent(
      const nsIContent& aContent, const nsINode* aLimiter = nullptr);

  /**
   * GetEmptyTextNodeToApplyNewStyle creates new empty text node to insert
   * a new element which will contain newly inserted text or returns existing
   * empty text node if aCandidatePointToInsert is around it.
   *
   * NOTE: Unfortunately, editor does not want to insert text into empty inline
   * element in some places (e.g., automatically adjusting caret position to
   * nearest text node).  Therefore, we need to create new empty text node to
   * prepare new styles for inserting text.  This method is designed for the
   * preparation.
   *
   * @param aHTMLEditor                 The editor.
   * @param aCandidatePointToInsert     The point where the caller wants to
   *                                    insert new text.
   * @param aEditingHost                The editing host.
   * @return            If this creates new empty text node returns it.
   *                    If this couldn't create new empty text node due to
   *                    the point or aEditingHost cannot have text node,
   *                    returns nullptr.
   *                    Otherwise, returns error.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<RefPtr<Text>, nsresult>
  GetEmptyTextNodeToApplyNewStyle(HTMLEditor& aHTMLEditor,
                                  const EditorDOMPoint& aCandidatePointToInsert,
                                  const Element& aEditingHost);

 private:
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT Result<CaretPoint, nsresult> ApplyStyle(
      HTMLEditor& aHTMLEditor, nsIContent& aContent);

  [[nodiscard]] MOZ_CAN_RUN_SCRIPT Result<CaretPoint, nsresult>
  ApplyCSSTextDecoration(HTMLEditor& aHTMLEditor, nsIContent& aContent);

  /**
   * Returns true if aStyledElement is a good element to set `style` attribute.
   */
  [[nodiscard]] bool ElementIsGoodContainerToSetStyle(
      nsStyledElement& aStyledElement) const;

  /**
   * ElementIsGoodContainerForTheStyle() returns true if aElement is a
   * good container for applying the style to a node.  I.e., if this returns
   * true, moving nodes into aElement is enough to apply the style to them.
   * Otherwise, you need to create new element for the style.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT Result<bool, nsresult>
  ElementIsGoodContainerForTheStyle(HTMLEditor& aHTMLEditor,
                                    Element& aElement) const;

  /**
   * Return true if the node is an element node and it represents the style or
   * sets the style (including when setting different value) with `style`
   * attribute.
   */
  [[nodiscard]] bool ContentIsElementSettingTheStyle(
      const HTMLEditor& aHTMLEditor, nsIContent& aContent) const;

  /**
   * Helper methods to shrink range to apply the style.
   */
  [[nodiscard]] EditorRawDOMPoint GetShrunkenRangeStart(
      const HTMLEditor& aHTMLEditor, const EditorDOMRange& aRange,
      const nsINode& aCommonAncestorOfRange,
      const nsIContent* aFirstEntirelySelectedContentNodeInRange) const;
  [[nodiscard]] EditorRawDOMPoint GetShrunkenRangeEnd(
      const HTMLEditor& aHTMLEditor, const EditorDOMRange& aRange,
      const nsINode& aCommonAncestorOfRange,
      const nsIContent* aLastEntirelySelectedContentNodeInRange) const;

  /**
   * Helper methods to extend the range to apply the style.
   */
  [[nodiscard]] EditorRawDOMPoint
  GetExtendedRangeStartToWrapAncestorApplyingSameStyle(
      const HTMLEditor& aHTMLEditor,
      const EditorRawDOMPoint& aStartPoint) const;
  [[nodiscard]] EditorRawDOMPoint
  GetExtendedRangeEndToWrapAncestorApplyingSameStyle(
      const HTMLEditor& aHTMLEditor, const EditorRawDOMPoint& aEndPoint) const;
  [[nodiscard]] EditorRawDOMRange
  GetExtendedRangeToMinimizeTheNumberOfNewElements(
      const HTMLEditor& aHTMLEditor, const nsINode& aCommonAncestor,
      EditorRawDOMPoint&& aStartPoint, EditorRawDOMPoint&& aEndPoint) const;

  /**
   * OnHandled() are called when this class creates new element to apply the
   * style, applies new style to existing element or ignores to apply the style
   * due to already set.
   */
  void OnHandled(const EditorDOMPoint& aStartPoint,
                 const EditorDOMPoint& aEndPoint) {
    if (!mFirstHandledPoint.IsSet()) {
      mFirstHandledPoint = aStartPoint;
    }
    mLastHandledPoint = aEndPoint;
  }
  void OnHandled(nsIContent& aContent) {
    if (!mFirstHandledPoint.IsSet()) {
      mFirstHandledPoint.Set(&aContent, 0u);
    }
    mLastHandledPoint = EditorDOMPoint::AtEndOf(aContent);
  }

  // mFirstHandledPoint and mLastHandledPoint store the first and last points
  // which are newly created or apply the new style, or just ignored at trying
  // to split a text node.
  EditorDOMPoint mFirstHandledPoint;
  EditorDOMPoint mLastHandledPoint;
};

/**
 * AutoMoveOneLineHandler moves the content in a line (between line breaks/block
 * boundaries) to specific point or end of a container element.
 */
class MOZ_STACK_CLASS HTMLEditor::AutoMoveOneLineHandler final {
 public:
  /**
   * Use this constructor when you want a line to move specific point.
   */
  explicit AutoMoveOneLineHandler(const EditorDOMPoint& aPointToInsert)
      : mPointToInsert(aPointToInsert),
        mMoveToEndOfContainer(MoveToEndOfContainer::No) {
    MOZ_ASSERT(mPointToInsert.IsSetAndValid());
  }
  /**
   * Use this constructor when you want a line to move end of
   * aNewContainerElement.
   */
  explicit AutoMoveOneLineHandler(Element& aNewContainerElement)
      : mPointToInsert(&aNewContainerElement, 0),
        mMoveToEndOfContainer(MoveToEndOfContainer::Yes) {
    MOZ_ASSERT(mPointToInsert.IsSetAndValid());
  }

  /**
   * Must be called before calling Run().
   *
   * @param aHTMLEditor         The HTML editor.
   * @param aPointInHardLine    A point in a line which you want to move.
   * @param aEditingHost        The editing host.
   */
  [[nodiscard]] nsresult Prepare(HTMLEditor& aHTMLEditor,
                                 const EditorDOMPoint& aPointInHardLine,
                                 const Element& aEditingHost);
  /**
   * Must be called if Prepare() returned NS_OK.
   *
   * @param aHTMLEditor         The HTML editor.
   * @param aEditingHost        The editing host.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT Result<MoveNodeResult, nsresult> Run(
      HTMLEditor& aHTMLEditor, const Element& aEditingHost);

  /**
   * Returns true if there are some content nodes which can be moved to another
   * place or deleted in the line containing aPointInHardLine.  Note that if
   * there is only a padding <br> element in an empty block element, this
   * returns false even though it may be deleted.
   */
  static Result<bool, nsresult> CanMoveOrDeleteSomethingInLine(
      const EditorDOMPoint& aPointInHardLine, const Element& aEditingHost);

  AutoMoveOneLineHandler(const AutoMoveOneLineHandler& aOther) = delete;
  AutoMoveOneLineHandler(AutoMoveOneLineHandler&& aOther) = delete;

 private:
  [[nodiscard]] bool ForceMoveToEndOfContainer() const {
    return mMoveToEndOfContainer == MoveToEndOfContainer::Yes;
  }
  [[nodiscard]] EditorDOMPoint& NextInsertionPointRef() {
    if (ForceMoveToEndOfContainer()) {
      mPointToInsert.SetToEndOf(mPointToInsert.GetContainer());
    }
    return mPointToInsert;
  }

  /**
   * Consider whether Run() should preserve or does not preserve white-space
   * style of moving content.
   *
   * @param aContentInLine      Specify a content node in the moving line.
   *                            Typically, container of aPointInHardLine of
   *                            Prepare().
   * @param aInclusiveAncestorBlockOfInsertionPoint
   *                            Inclusive ancestor block element of insertion
   *                            point.  Typically, computed
   *                            mDestInclusiveAncestorBlock.
   */
  [[nodiscard]] static PreserveWhiteSpaceStyle
  ConsiderWhetherPreserveWhiteSpaceStyle(
      const nsIContent* aContentInLine,
      const Element* aInclusiveAncestorBlockOfInsertionPoint);

  /**
   * Look for inclusive ancestor block element of aBlockElement and a descendant
   * of aAncestorElement.  If aBlockElement and aAncestorElement are same one,
   * this returns nullptr.
   *
   * @param aBlockElement       A block element which is a descendant of
   *                            aAncestorElement.
   * @param aAncestorElement    An inclusive ancestor block element of
   *                            aBlockElement.
   */
  [[nodiscard]] static Element*
  GetMostDistantInclusiveAncestorBlockInSpecificAncestorElement(
      Element& aBlockElement, const Element& aAncestorElement);

  /**
   * Split ancestors at the line range boundaries and collect array of contents
   * in the line to aOutArrayOfContents.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT Result<CaretPoint, nsresult>
  SplitToMakeTheLineIsolated(
      HTMLEditor& aHTMLEditor, const Element& aEditingHost,
      nsTArray<OwningNonNull<nsIContent>>& aOutArrayOfContents) const;

  /**
   * Delete unnecessary trailing line break in aMovedContentRange if there is.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
  DeleteUnnecessaryTrailingLineBreakInMovedLineEnd(
      HTMLEditor& aHTMLEditor, const EditorDOMRange& aMovedContentRange,
      const Element& aEditingHost) const;

  // Range of selected line.
  EditorDOMRange mLineRange;
  // Next insertion point.  If mMoveToEndOfContainer is `Yes`, this is
  // recomputed with its container in NextInsertionPointRef.  Therefore, this
  // should not be referred directly.
  EditorDOMPoint mPointToInsert;
  // An inclusive ancestor block element of the moving line.
  RefPtr<Element> mSrcInclusiveAncestorBlock;
  // An inclusive ancestor block element of the insertion point.
  RefPtr<Element> mDestInclusiveAncestorBlock;
  // nullptr if mMovingToParentBlock is false.
  // Must be non-nullptr if mMovingToParentBlock is true.  The topmost ancestor
  // block element which contains mSrcInclusiveAncestorBlock and a descendant of
  // mDestInclusiveAncestorBlock.  I.e., this may be same as
  // mSrcInclusiveAncestorBlock, but never same as mDestInclusiveAncestorBlock.
  RefPtr<Element> mTopmostSrcAncestorBlockInDestBlock;
  enum class MoveToEndOfContainer { No, Yes };
  MoveToEndOfContainer mMoveToEndOfContainer;
  PreserveWhiteSpaceStyle mPreserveWhiteSpaceStyle =
      PreserveWhiteSpaceStyle::No;
  // true if mDestInclusiveAncestorBlock is an ancestor of
  // mSrcInclusiveAncestorBlock.
  bool mMovingToParentBlock = false;
};

}  // namespace mozilla

#endif  // #ifndef HTMLEditorNestedClasses_h
