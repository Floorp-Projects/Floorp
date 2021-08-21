/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLEditor.h"

#include <algorithm>
#include <utility>

#include "HTMLEditUtils.h"
#include "WSRunObject.h"

#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/ContentIterator.h"
#include "mozilla/EditAction.h"
#include "mozilla/EditorDOMPoint.h"
#include "mozilla/EditorUtils.h"
#include "mozilla/InternalMutationEvent.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/StaticPrefs_editor.h"  // for StaticPrefs::editor_*
#include "mozilla/Unused.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLBRElement.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/mozalloc.h"
#include "nsAString.h"
#include "nsAtom.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsFrameSelection.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsINode.h"
#include "nsRange.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsTArray.h"

// NOTE: This file was split from:
//   https://searchfox.org/mozilla-central/rev/c409dd9235c133ab41eba635f906aa16e050c197/editor/libeditor/HTMLEditSubActionHandler.cpp

namespace mozilla {

using namespace dom;
using EmptyCheckOption = HTMLEditUtils::EmptyCheckOption;
using InvisibleWhiteSpaces = HTMLEditUtils::InvisibleWhiteSpaces;
using LeafNodeType = HTMLEditUtils::LeafNodeType;
using StyleDifference = HTMLEditUtils::StyleDifference;
using TableBoundary = HTMLEditUtils::TableBoundary;
using WalkTreeOption = HTMLEditUtils::WalkTreeOption;

template nsresult HTMLEditor::DeleteTextAndTextNodesWithTransaction(
    const EditorDOMPoint& aStartPoint, const EditorDOMPoint& aEndPoint,
    TreatEmptyTextNodes aTreatEmptyTextNodes);
template nsresult HTMLEditor::DeleteTextAndTextNodesWithTransaction(
    const EditorDOMPointInText& aStartPoint,
    const EditorDOMPointInText& aEndPoint,
    TreatEmptyTextNodes aTreatEmptyTextNodes);
template Result<bool, nsresult> HTMLEditor::CanMoveOrDeleteSomethingInHardLine(
    const EditorDOMPoint& aPointInHardLine) const;
template Result<bool, nsresult> HTMLEditor::CanMoveOrDeleteSomethingInHardLine(
    const EditorRawDOMPoint& aPointInHardLine) const;

/*****************************************************************************
 * AutoSetTemporaryAncestorLimiter
 ****************************************************************************/

class MOZ_RAII AutoSetTemporaryAncestorLimiter final {
 public:
  AutoSetTemporaryAncestorLimiter(const HTMLEditor& aHTMLEditor,
                                  Selection& aSelection,
                                  nsINode& aStartPointNode,
                                  AutoRangeArray* aRanges = nullptr) {
    MOZ_ASSERT(aSelection.GetType() == SelectionType::eNormal);

    if (aSelection.GetAncestorLimiter()) {
      return;
    }

    Element* root = aHTMLEditor.FindSelectionRoot(&aStartPointNode);
    if (root) {
      aHTMLEditor.InitializeSelectionAncestorLimit(*root);
      mSelection = &aSelection;
      // Setting ancestor limiter may change ranges which were outer of
      // the new limiter.  Therefore, we need to reinitialize aRanges.
      if (aRanges) {
        aRanges->Initialize(aSelection);
      }
    }
  }

  ~AutoSetTemporaryAncestorLimiter() {
    if (mSelection) {
      mSelection->SetAncestorLimiter(nullptr);
    }
  }

 private:
  RefPtr<Selection> mSelection;
};

/*****************************************************************************
 * AutoDeleteRangesHandler
 ****************************************************************************/

class MOZ_STACK_CLASS HTMLEditor::AutoDeleteRangesHandler final {
 public:
  explicit AutoDeleteRangesHandler(
      const AutoDeleteRangesHandler* aParent = nullptr)
      : mParent(aParent),
        mOriginalDirectionAndAmount(nsIEditor::eNone),
        mOriginalStripWrappers(nsIEditor::eNoStrip) {}

  /**
   * ComputeRangesToDelete() computes actual deletion ranges.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult ComputeRangesToDelete(
      const HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
      AutoRangeArray& aRangesToDelete);

  /**
   * Deletes content in or around aRangesToDelete.
   * NOTE: This method creates SelectionBatcher.  Therefore, each caller
   *       needs to check if the editor is still available even if this returns
   *       NS_OK.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT EditActionResult
  Run(HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
      nsIEditor::EStripWrappers aStripWrappers,
      AutoRangeArray& aRangesToDelete);

 private:
  bool IsHandlingRecursively() const { return mParent != nullptr; }

  bool CanFallbackToDeleteRangesWithTransaction(
      const AutoRangeArray& aRangesToDelete) const {
    return !IsHandlingRecursively() && !aRangesToDelete.Ranges().IsEmpty() &&
           (!aRangesToDelete.IsCollapsed() ||
            EditorBase::HowToHandleCollapsedRangeFor(
                mOriginalDirectionAndAmount) !=
                EditorBase::HowToHandleCollapsedRange::Ignore);
  }

  /**
   * HandleDeleteAroundCollapsedRanges() handles deletion with collapsed
   * ranges.  Callers must guarantee that this is called only when
   * aRangesToDelete.IsCollapsed() returns true.
   *
   * @param aDirectionAndAmount Direction of the deletion.
   * @param aStripWrappers      Must be eStrip or eNoStrip.
   * @param aRangesToDelete     Ranges to delete.  This `IsCollapsed()` must
   *                            return true.
   * @param aWSRunScannerAtCaret        Scanner instance which scanned from
   *                                    caret point.
   * @param aScanFromCaretPointResult   Scan result of aWSRunScannerAtCaret
   *                                    toward aDirectionAndAmount.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT EditActionResult
  HandleDeleteAroundCollapsedRanges(
      HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
      nsIEditor::EStripWrappers aStripWrappers, AutoRangeArray& aRangesToDelete,
      const WSRunScanner& aWSRunScannerAtCaret,
      const WSScanResult& aScanFromCaretPointResult);
  nsresult ComputeRangesToDeleteAroundCollapsedRanges(
      const HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
      AutoRangeArray& aRangesToDelete, const WSRunScanner& aWSRunScannerAtCaret,
      const WSScanResult& aScanFromCaretPointResult) const;

  /**
   * HandleDeleteNonCollapsedRanges() handles deletion with non-collapsed
   * ranges.  Callers must guarantee that this is called only when
   * aRangesToDelete.IsCollapsed() returns false.
   *
   * @param aDirectionAndAmount         Direction of the deletion.
   * @param aStripWrappers              Must be eStrip or eNoStrip.
   * @param aRangesToDelete             The ranges to delete.
   * @param aSelectionWasCollapsed      If the caller extended `Selection`
   *                                    from collapsed, set this to `Yes`.
   *                                    Otherwise, i.e., `Selection` is not
   *                                    collapsed from the beginning, set
   *                                    this to `No`.
   */
  enum class SelectionWasCollapsed { Yes, No };
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT EditActionResult
  HandleDeleteNonCollapsedRanges(HTMLEditor& aHTMLEditor,
                                 nsIEditor::EDirection aDirectionAndAmount,
                                 nsIEditor::EStripWrappers aStripWrappers,
                                 AutoRangeArray& aRangesToDelete,
                                 SelectionWasCollapsed aSelectionWasCollapsed);
  nsresult ComputeRangesToDeleteNonCollapsedRanges(
      const HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
      AutoRangeArray& aRangesToDelete,
      SelectionWasCollapsed aSelectionWasCollapsed) const;

  /**
   * HandleDeleteTextAroundCollapsedRanges() handles deletion of collapsed
   * ranges in a text node.
   *
   * @param aDirectionAndAmount Must be eNext or ePrevious.
   * @param aCaretPoisition     The position where caret is.  This container
   *                            must be a text node.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT EditActionResult
  HandleDeleteTextAroundCollapsedRanges(
      HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
      AutoRangeArray& aRangesToDelete);
  nsresult ComputeRangesToDeleteTextAroundCollapsedRanges(
      Element* aEditingHost, nsIEditor::EDirection aDirectionAndAmount,
      AutoRangeArray& aRangesToDelete) const;

  /**
   * HandleDeleteCollapsedSelectionAtWhiteSpaces() handles deletion of
   * collapsed selection at white-spaces in a text node.
   *
   * @param aDirectionAndAmount Direction of the deletion.
   * @param aPointToDelete      The point to delete.  I.e., typically, caret
   *                            position.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT EditActionResult
  HandleDeleteCollapsedSelectionAtWhiteSpaces(
      HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
      const EditorDOMPoint& aPointToDelete);

  /**
   * HandleDeleteCollapsedSelectionAtVisibleChar() handles deletion of
   * collapsed selection in a text node.
   *
   * @param aDirectionAndAmount Direction of the deletion.
   * @param aPointToDelete      The point in a text node to delete character(s).
   *                            Caller must guarantee that this is in a text
   *                            node.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT EditActionResult
  HandleDeleteCollapsedSelectionAtVisibleChar(
      HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
      const EditorDOMPoint& aPointToDelete);

  /**
   * HandleDeleteAtomicContent() handles deletion of atomic elements like
   * `<br>`, `<hr>`, `<img>`, `<input>`, etc and data nodes except text node
   * (e.g., comment node). Note that don't call this directly with `<hr>`
   * element.  Instead, call `HandleDeleteHRElement()`. Note that don't call
   * this for invisible `<br>` element.
   *
   * @param aAtomicContent      The atomic content to be deleted.
   * @param aCaretPoint         The caret point (i.e., selection start or
   *                            end).
   * @param aWSRunScannerAtCaret WSRunScanner instance which was initialized
   *                             with the caret point.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT EditActionResult
  HandleDeleteAtomicContent(HTMLEditor& aHTMLEditor, nsIContent& aAtomicContent,
                            const EditorDOMPoint& aCaretPoint,
                            const WSRunScanner& aWSRunScannerAtCaret);
  nsresult ComputeRangesToDeleteAtomicContent(
      Element* aEditingHost, const nsIContent& aAtomicContent,
      AutoRangeArray& aRangesToDelete) const;

  /**
   * GetAtomicContnetToDelete() returns better content that is deletion of
   * atomic element.  If aScanFromCaretPointResult is special, since this
   * point may not be editable, we look for better point to remove atomic
   * content.
   *
   * @param aDirectionAndAmount       Direction of the deletion.
   * @param aWSRunScannerAtCaret      WSRunScanner instance which was
   *                                  initialized with the caret point.
   * @param aScanFromCaretPointResult Scan result of aWSRunScannerAtCaret
   *                                  toward aDirectionAndAmount.
   */
  static nsIContent* GetAtomicContentToDelete(
      nsIEditor::EDirection aDirectionAndAmount,
      const WSRunScanner& aWSRunScannerAtCaret,
      const WSScanResult& aScanFromCaretPointResult) MOZ_NONNULL_RETURN;

  /**
   * HandleDeleteHRElement() handles deletion around `<hr>` element.  If
   * aDirectionAndAmount is nsIEditor::ePrevious, aHTElement is removed only
   * when caret is at next sibling of the `<hr>` element and inter line position
   * is "left".  Otherwise, caret is moved and does not remove the `<hr>`
   * elemnent.
   * XXX Perhaps, we can get rid of this special handling because the other
   *     browsers don't do this, and our `<hr>` element handling is really
   *     odd.
   *
   * @param aDirectionAndAmount Direction of the deletion.
   * @param aHRElement          The `<hr>` element to be removed.
   * @param aCaretPoint         The caret point (i.e., selection start or
   *                            end).
   * @param aWSRunScannerAtCaret WSRunScanner instance which was initialized
   *                             with the caret point.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT EditActionResult HandleDeleteHRElement(
      HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
      Element& aHRElement, const EditorDOMPoint& aCaretPoint,
      const WSRunScanner& aWSRunScannerAtCaret);
  nsresult ComputeRangesToDeleteHRElement(
      const HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
      Element& aHRElement, const EditorDOMPoint& aCaretPoint,
      const WSRunScanner& aWSRunScannerAtCaret,
      AutoRangeArray& aRangesToDelete) const;

  /**
   * HandleDeleteAtOtherBlockBoundary() handles deletion at other block boundary
   * (i.e., immediately before or after a block). If this does not join blocks,
   * `Run()` may be called recursively with creating another instance.
   *
   * @param aDirectionAndAmount Direction of the deletion.
   * @param aStripWrappers      Must be eStrip or eNoStrip.
   * @param aOtherBlockElement  The block element which follows the caret or
   *                            is followed by caret.
   * @param aCaretPoint         The caret point (i.e., selection start or
   *                            end).
   * @param aWSRunScannerAtCaret WSRunScanner instance which was initialized
   *                             with the caret point.
   * @param aRangesToDelete     Ranges to delete of the caller.  This should
   *                            be collapsed and the point should match with
   *                            aCaretPoint.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT EditActionResult
  HandleDeleteAtOtherBlockBoundary(HTMLEditor& aHTMLEditor,
                                   nsIEditor::EDirection aDirectionAndAmount,
                                   nsIEditor::EStripWrappers aStripWrappers,
                                   Element& aOtherBlockElement,
                                   const EditorDOMPoint& aCaretPoint,
                                   WSRunScanner& aWSRunScannerAtCaret,
                                   AutoRangeArray& aRangesToDelete);

  /**
   * ExtendRangeToIncludeInvisibleNodes() extends aRange if there are some
   * invisible nodes around it.
   *
   * @param aFrameSelection     If the caller wants range in selection limiter,
   *                            set this to non-nullptr which knows the limiter.
   * @param aRange              The range to be extended.  This must not be
   *                            collapsed, must be positioned, and must not be
   *                            in selection.
   * @return                    true if succeeded to set the range.
   */
  bool ExtendRangeToIncludeInvisibleNodes(
      const HTMLEditor& aHTMLEditor, const nsFrameSelection* aFrameSelection,
      nsRange& aRange) const;

  /**
   * ShouldDeleteHRElement() checks whether aHRElement should be deleted
   * when selection is collapsed at aCaretPoint.
   */
  Result<bool, nsresult> ShouldDeleteHRElement(
      const HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
      Element& aHRElement, const EditorDOMPoint& aCaretPoint) const;

  /**
   * DeleteUnnecessaryNodesAndCollapseSelection() removes unnecessary nodes
   * around aSelectionStartPoint and aSelectionEndPoint.  Then, collapse
   * selection at aSelectionStartPoint or aSelectionEndPoint (depending on
   * aDirectionAndAmount).
   *
   * @param aDirectionAndAmount         Direction of the deletion.
   *                                    If nsIEditor::ePrevious, selection
   * will be collapsed to aSelectionEndPoint. Otherwise, selection will be
   * collapsed to aSelectionStartPoint.
   * @param aSelectionStartPoint        First selection range start after
   *                                    computing the deleting range.
   * @param aSelectionEndPoint          First selection range end after
   *                                    computing the deleting range.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
  DeleteUnnecessaryNodesAndCollapseSelection(
      HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
      const EditorDOMPoint& aSelectionStartPoint,
      const EditorDOMPoint& aSelectionEndPoint);

  /**
   * If aContent is a text node that contains only collapsed white-space or
   * empty and editable.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
  DeleteNodeIfInvisibleAndEditableTextNode(HTMLEditor& aHTMLEditor,
                                           nsIContent& aContent);

  /**
   * DeleteParentBlocksIfEmpty() removes parent block elements if they
   * don't have visible contents.  Note that due performance issue of
   * WhiteSpaceVisibilityKeeper, this call may be expensive.  And also note that
   * this removes a empty block with a transaction.  So, please make sure that
   * you've already created `AutoPlaceholderBatch`.
   *
   * @param aPoint      The point whether this method climbing up the DOM
   *                    tree to remove empty parent blocks.
   * @return            NS_OK if one or more empty block parents are deleted.
   *                    NS_SUCCESS_EDITOR_ELEMENT_NOT_FOUND if the point is
   *                    not in empty block.
   *                    Or NS_ERROR_* if something unexpected occurs.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
  DeleteParentBlocksWithTransactionIfEmpty(HTMLEditor& aHTMLEditor,
                                           const EditorDOMPoint& aPoint);

  [[nodiscard]] MOZ_CAN_RUN_SCRIPT EditActionResult
  FallbackToDeleteRangesWithTransaction(HTMLEditor& aHTMLEditor,
                                        AutoRangeArray& aRangesToDelete) {
    MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());
    MOZ_ASSERT(CanFallbackToDeleteRangesWithTransaction(aRangesToDelete));
    nsresult rv = aHTMLEditor.DeleteRangesWithTransaction(
        mOriginalDirectionAndAmount, mOriginalStripWrappers, aRangesToDelete);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::DeleteRangesWithTransaction() failed");
    return EditActionHandled(rv);  // Don't return "ignored" for avoiding to
                                   // fall it back again.
  }

  /**
   * ComputeRangesToDeleteRangesWithTransaction() computes target ranges
   * which will be called by `EditorBase::DeleteRangesWithTransaction()`.
   * TODO: We should not use it for consistency with each deletion handler
   *       in this and nested classes.
   */
  nsresult ComputeRangesToDeleteRangesWithTransaction(
      const HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
      AutoRangeArray& aRangesToDelete) const;

  nsresult FallbackToComputeRangesToDeleteRangesWithTransaction(
      const HTMLEditor& aHTMLEditor, AutoRangeArray& aRangesToDelete) const {
    MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());
    MOZ_ASSERT(CanFallbackToDeleteRangesWithTransaction(aRangesToDelete));
    nsresult rv = ComputeRangesToDeleteRangesWithTransaction(
        aHTMLEditor, mOriginalDirectionAndAmount, aRangesToDelete);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "AutoDeleteRangesHandler::"
                         "ComputeRangesToDeleteRangesWithTransaction() failed");
    return rv;
  }

  class MOZ_STACK_CLASS AutoBlockElementsJoiner final {
   public:
    AutoBlockElementsJoiner() = delete;
    explicit AutoBlockElementsJoiner(
        AutoDeleteRangesHandler& aDeleteRangesHandler)
        : mDeleteRangesHandler(&aDeleteRangesHandler),
          mDeleteRangesHandlerConst(aDeleteRangesHandler) {}
    explicit AutoBlockElementsJoiner(
        const AutoDeleteRangesHandler& aDeleteRangesHandler)
        : mDeleteRangesHandler(nullptr),
          mDeleteRangesHandlerConst(aDeleteRangesHandler) {}

    /**
     * PrepareToDeleteAtCurrentBlockBoundary() considers left content and right
     * content which are joined for handling deletion at current block boundary
     * (i.e., at start or end of the current block).
     *
     * @param aHTMLEditor               The HTML editor.
     * @param aDirectionAndAmount       Direction of the deletion.
     * @param aCurrentBlockElement      The current block element.
     * @param aCaretPoint               The caret point (i.e., selection start
     *                                  or end).
     * @return                          true if can continue to handle the
     *                                  deletion.
     */
    bool PrepareToDeleteAtCurrentBlockBoundary(
        const HTMLEditor& aHTMLEditor,
        nsIEditor::EDirection aDirectionAndAmount,
        Element& aCurrentBlockElement, const EditorDOMPoint& aCaretPoint);

    /**
     * PrepareToDeleteAtOtherBlockBoundary() considers left content and right
     * content which are joined for handling deletion at other block boundary
     * (i.e., immediately before or after a block).
     *
     * @param aHTMLEditor               The HTML editor.
     * @param aDirectionAndAmount       Direction of the deletion.
     * @param aOtherBlockElement        The block element which follows the
     *                                  caret or is followed by caret.
     * @param aCaretPoint               The caret point (i.e., selection start
     *                                  or end).
     * @param aWSRunScannerAtCaret      WSRunScanner instance which was
     *                                  initialized with the caret point.
     * @return                          true if can continue to handle the
     *                                  deletion.
     */
    bool PrepareToDeleteAtOtherBlockBoundary(
        const HTMLEditor& aHTMLEditor,
        nsIEditor::EDirection aDirectionAndAmount, Element& aOtherBlockElement,
        const EditorDOMPoint& aCaretPoint,
        const WSRunScanner& aWSRunScannerAtCaret);

    /**
     * PrepareToDeleteNonCollapsedRanges() considers left block element and
     * right block element which are inclusive ancestor block element of
     * start and end container of first range of aRangesToDelete.
     *
     * @param aHTMLEditor               The HTML editor.
     * @param aRangesToDelete           Ranges to delete.  Must not be
     *                                  collapsed.
     * @return                          true if can continue to handle the
     *                                  deletion.
     */
    bool PrepareToDeleteNonCollapsedRanges(
        const HTMLEditor& aHTMLEditor, const AutoRangeArray& aRangesToDelete);

    /**
     * Run() executes the joining.
     *
     * @param aHTMLEditor               The HTML editor.
     * @param aDirectionAndAmount       Direction of the deletion.
     * @param aStripWrappers            Must be eStrip or eNoStrip.
     * @param aCaretPoint               The caret point (i.e., selection start
     *                                  or end).
     * @param aRangesToDelete           Ranges to delete of the caller.
     *                                  This should be collapsed and match
     *                                  with aCaretPoint.
     */
    [[nodiscard]] MOZ_CAN_RUN_SCRIPT EditActionResult
    Run(HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
        nsIEditor::EStripWrappers aStripWrappers,
        const EditorDOMPoint& aCaretPoint, AutoRangeArray& aRangesToDelete) {
      switch (mMode) {
        case Mode::JoinCurrentBlock: {
          EditActionResult result =
              HandleDeleteAtCurrentBlockBoundary(aHTMLEditor, aCaretPoint);
          NS_WARNING_ASSERTION(result.Succeeded(),
                               "AutoBlockElementsJoiner::"
                               "HandleDeleteAtCurrentBlockBoundary() failed");
          return result;
        }
        case Mode::JoinOtherBlock: {
          EditActionResult result = HandleDeleteAtOtherBlockBoundary(
              aHTMLEditor, aDirectionAndAmount, aStripWrappers, aCaretPoint,
              aRangesToDelete);
          NS_WARNING_ASSERTION(result.Succeeded(),
                               "AutoBlockElementsJoiner::"
                               "HandleDeleteAtOtherBlockBoundary() failed");
          return result;
        }
        case Mode::DeleteBRElement: {
          EditActionResult result =
              DeleteBRElement(aHTMLEditor, aDirectionAndAmount, aCaretPoint);
          NS_WARNING_ASSERTION(
              result.Succeeded(),
              "AutoBlockElementsJoiner::DeleteBRElement() failed");
          return result;
        }
        case Mode::JoinBlocksInSameParent:
        case Mode::DeleteContentInRanges:
        case Mode::DeleteNonCollapsedRanges:
          MOZ_ASSERT_UNREACHABLE(
              "This mode should be handled in the other Run()");
          return EditActionResult(NS_ERROR_UNEXPECTED);
        case Mode::NotInitialized:
          return EditActionIgnored();
      }
      return EditActionResult(NS_ERROR_NOT_INITIALIZED);
    }

    nsresult ComputeRangesToDelete(const HTMLEditor& aHTMLEditor,
                                   nsIEditor::EDirection aDirectionAndAmount,
                                   const EditorDOMPoint& aCaretPoint,
                                   AutoRangeArray& aRangesToDelete) const {
      switch (mMode) {
        case Mode::JoinCurrentBlock: {
          nsresult rv = ComputeRangesToDeleteAtCurrentBlockBoundary(
              aHTMLEditor, aCaretPoint, aRangesToDelete);
          NS_WARNING_ASSERTION(
              NS_SUCCEEDED(rv),
              "AutoBlockElementsJoiner::"
              "ComputeRangesToDeleteAtCurrentBlockBoundary() failed");
          return rv;
        }
        case Mode::JoinOtherBlock: {
          nsresult rv = ComputeRangesToDeleteAtOtherBlockBoundary(
              aHTMLEditor, aDirectionAndAmount, aCaretPoint, aRangesToDelete);
          NS_WARNING_ASSERTION(
              NS_SUCCEEDED(rv),
              "AutoBlockElementsJoiner::"
              "ComputeRangesToDeleteAtOtherBlockBoundary() failed");
          return rv;
        }
        case Mode::DeleteBRElement: {
          nsresult rv = ComputeRangesToDeleteBRElement(aRangesToDelete);
          NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                               "AutoBlockElementsJoiner::"
                               "ComputeRangesToDeleteBRElement() failed");
          return rv;
        }
        case Mode::JoinBlocksInSameParent:
        case Mode::DeleteContentInRanges:
        case Mode::DeleteNonCollapsedRanges:
          MOZ_ASSERT_UNREACHABLE(
              "This mode should be handled in the other "
              "ComputeRangesToDelete()");
          return NS_ERROR_UNEXPECTED;
        case Mode::NotInitialized:
          return NS_OK;
      }
      return NS_ERROR_NOT_IMPLEMENTED;
    }

    /**
     * Run() executes the joining.
     *
     * @param aHTMLEditor               The HTML editor.
     * @param aDirectionAndAmount       Direction of the deletion.
     * @param aStripWrappers            Whether delete or keep new empty
     *                                  ancestor elements.
     * @param aRangesToDelete           Ranges to delete.  Must not be
     *                                  collapsed.
     * @param aSelectionWasCollapsed    Whether selection was or was not
     *                                  collapsed when starting to handle
     *                                  deletion.
     */
    [[nodiscard]] MOZ_CAN_RUN_SCRIPT EditActionResult
    Run(HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
        nsIEditor::EStripWrappers aStripWrappers,
        AutoRangeArray& aRangesToDelete,
        AutoDeleteRangesHandler::SelectionWasCollapsed aSelectionWasCollapsed) {
      switch (mMode) {
        case Mode::JoinCurrentBlock:
        case Mode::JoinOtherBlock:
        case Mode::DeleteBRElement:
          MOZ_ASSERT_UNREACHABLE(
              "This mode should be handled in the other Run()");
          return EditActionResult(NS_ERROR_UNEXPECTED);
        case Mode::JoinBlocksInSameParent: {
          EditActionResult result =
              JoinBlockElementsInSameParent(aHTMLEditor, aDirectionAndAmount,
                                            aStripWrappers, aRangesToDelete);
          NS_WARNING_ASSERTION(result.Succeeded(),
                               "AutoBlockElementsJoiner::"
                               "JoinBlockElementsInSameParent() failed");
          return result;
        }
        case Mode::DeleteContentInRanges: {
          EditActionResult result =
              DeleteContentInRanges(aHTMLEditor, aDirectionAndAmount,
                                    aStripWrappers, aRangesToDelete);
          NS_WARNING_ASSERTION(
              result.Succeeded(),
              "AutoBlockElementsJoiner::DeleteContentInRanges() failed");
          return result;
        }
        case Mode::DeleteNonCollapsedRanges: {
          EditActionResult result = HandleDeleteNonCollapsedRanges(
              aHTMLEditor, aDirectionAndAmount, aStripWrappers, aRangesToDelete,
              aSelectionWasCollapsed);
          NS_WARNING_ASSERTION(result.Succeeded(),
                               "AutoBlockElementsJoiner::"
                               "HandleDeleteNonCollapsedRange() failed");
          return result;
        }
        case Mode::NotInitialized:
          MOZ_ASSERT_UNREACHABLE(
              "Call Run() after calling a preparation method");
          return EditActionIgnored();
      }
      return EditActionResult(NS_ERROR_NOT_INITIALIZED);
    }

    nsresult ComputeRangesToDelete(
        const HTMLEditor& aHTMLEditor,
        nsIEditor::EDirection aDirectionAndAmount,
        AutoRangeArray& aRangesToDelete,
        AutoDeleteRangesHandler::SelectionWasCollapsed aSelectionWasCollapsed)
        const {
      switch (mMode) {
        case Mode::JoinCurrentBlock:
        case Mode::JoinOtherBlock:
        case Mode::DeleteBRElement:
          MOZ_ASSERT_UNREACHABLE(
              "This mode should be handled in the other "
              "ComputeRangesToDelete()");
          return NS_ERROR_UNEXPECTED;
        case Mode::JoinBlocksInSameParent: {
          nsresult rv = ComputeRangesToJoinBlockElementsInSameParent(
              aHTMLEditor, aDirectionAndAmount, aRangesToDelete);
          NS_WARNING_ASSERTION(
              NS_SUCCEEDED(rv),
              "AutoBlockElementsJoiner::"
              "ComputeRangesToJoinBlockElementsInSameParent() failed");
          return rv;
        }
        case Mode::DeleteContentInRanges: {
          nsresult rv = ComputeRangesToDeleteContentInRanges(
              aHTMLEditor, aDirectionAndAmount, aRangesToDelete);
          NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                               "AutoBlockElementsJoiner::"
                               "ComputeRangesToDeleteContentInRanges() failed");
          return rv;
        }
        case Mode::DeleteNonCollapsedRanges: {
          nsresult rv = ComputeRangesToDeleteNonCollapsedRanges(
              aHTMLEditor, aDirectionAndAmount, aRangesToDelete,
              aSelectionWasCollapsed);
          NS_WARNING_ASSERTION(
              NS_SUCCEEDED(rv),
              "AutoBlockElementsJoiner::"
              "ComputeRangesToDeleteNonCollapsedRanges() failed");
          return rv;
        }
        case Mode::NotInitialized:
          MOZ_ASSERT_UNREACHABLE(
              "Call ComputeRangesToDelete() after calling a preparation "
              "method");
          return NS_ERROR_NOT_INITIALIZED;
      }
      return NS_ERROR_NOT_INITIALIZED;
    }

    nsIContent* GetLeafContentInOtherBlockElement() const {
      MOZ_ASSERT(mMode == Mode::JoinOtherBlock);
      return mLeafContentInOtherBlock;
    }

   private:
    [[nodiscard]] MOZ_CAN_RUN_SCRIPT EditActionResult
    HandleDeleteAtCurrentBlockBoundary(HTMLEditor& aHTMLEditor,
                                       const EditorDOMPoint& aCaretPoint);
    nsresult ComputeRangesToDeleteAtCurrentBlockBoundary(
        const HTMLEditor& aHTMLEditor, const EditorDOMPoint& aCaretPoint,
        AutoRangeArray& aRangesToDelete) const;
    [[nodiscard]] MOZ_CAN_RUN_SCRIPT EditActionResult
    HandleDeleteAtOtherBlockBoundary(HTMLEditor& aHTMLEditor,
                                     nsIEditor::EDirection aDirectionAndAmount,
                                     nsIEditor::EStripWrappers aStripWrappers,
                                     const EditorDOMPoint& aCaretPoint,
                                     AutoRangeArray& aRangesToDelete);
    // FYI: This method may modify selection, but it won't cause running
    //      script because of `AutoHideSelectionChanges` which blocks
    //      selection change listeners and the selection change event
    //      dispatcher.
    MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult
    ComputeRangesToDeleteAtOtherBlockBoundary(
        const HTMLEditor& aHTMLEditor,
        nsIEditor::EDirection aDirectionAndAmount,
        const EditorDOMPoint& aCaretPoint,
        AutoRangeArray& aRangesToDelete) const;
    [[nodiscard]] MOZ_CAN_RUN_SCRIPT EditActionResult
    JoinBlockElementsInSameParent(HTMLEditor& aHTMLEditor,
                                  nsIEditor::EDirection aDirectionAndAmount,
                                  nsIEditor::EStripWrappers aStripWrappers,
                                  AutoRangeArray& aRangesToDelete);
    nsresult ComputeRangesToJoinBlockElementsInSameParent(
        const HTMLEditor& aHTMLEditor,
        nsIEditor::EDirection aDirectionAndAmount,
        AutoRangeArray& aRangesToDelete) const;
    [[nodiscard]] MOZ_CAN_RUN_SCRIPT EditActionResult DeleteBRElement(
        HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
        const EditorDOMPoint& aCaretPoint);
    nsresult ComputeRangesToDeleteBRElement(
        AutoRangeArray& aRangesToDelete) const;
    [[nodiscard]] MOZ_CAN_RUN_SCRIPT EditActionResult DeleteContentInRanges(
        HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
        nsIEditor::EStripWrappers aStripWrappers,
        AutoRangeArray& aRangesToDelete);
    nsresult ComputeRangesToDeleteContentInRanges(
        const HTMLEditor& aHTMLEditor,
        nsIEditor::EDirection aDirectionAndAmount,
        AutoRangeArray& aRangesToDelete) const;
    [[nodiscard]] MOZ_CAN_RUN_SCRIPT EditActionResult
    HandleDeleteNonCollapsedRanges(
        HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
        nsIEditor::EStripWrappers aStripWrappers,
        AutoRangeArray& aRangesToDelete,
        AutoDeleteRangesHandler::SelectionWasCollapsed aSelectionWasCollapsed);
    nsresult ComputeRangesToDeleteNonCollapsedRanges(
        const HTMLEditor& aHTMLEditor,
        nsIEditor::EDirection aDirectionAndAmount,
        AutoRangeArray& aRangesToDelete,
        AutoDeleteRangesHandler::SelectionWasCollapsed aSelectionWasCollapsed)
        const;

    /**
     * JoinNodesDeepWithTransaction() joins aLeftNode and aRightNode "deeply".
     * First, they are joined simply, then, new right node is assumed as the
     * child at length of the left node before joined and new left node is
     * assumed as its previous sibling.  Then, they will be joined again.
     * And then, these steps are repeated.
     *
     * @param aLeftContent    The node which will be removed form the tree.
     * @param aRightContent   The node which will be inserted the contents of
     *                        aRightContent.
     * @return                The point of the first child of the last right
     * node. The result is always set if this succeeded.
     */
    MOZ_CAN_RUN_SCRIPT Result<EditorDOMPoint, nsresult>
    JoinNodesDeepWithTransaction(HTMLEditor& aHTMLEditor,
                                 nsIContent& aLeftContent,
                                 nsIContent& aRightContent);

    /**
     * DeleteNodesEntirelyInRangeButKeepTableStructure() removes nodes which are
     * entirely in aRange.  Howevers, if some nodes are part of a table,
     * removes all children of them instead.  I.e., this does not make damage to
     * table structure at the range, but may remove table entirely if it's
     * in the range.
     *
     * @return                  true if inclusive ancestor block elements at
     *                          start and end of the range should be joined.
     */
    MOZ_CAN_RUN_SCRIPT Result<bool, nsresult>
    DeleteNodesEntirelyInRangeButKeepTableStructure(
        HTMLEditor& aHTMLEditor, nsRange& aRange,
        AutoDeleteRangesHandler::SelectionWasCollapsed aSelectionWasCollapsed);
    bool NeedsToJoinNodesAfterDeleteNodesEntirelyInRangeButKeepTableStructure(
        const HTMLEditor& aHTMLEditor,
        const nsTArray<OwningNonNull<nsIContent>>& aArrayOfContents,
        AutoDeleteRangesHandler::SelectionWasCollapsed aSelectionWasCollapsed)
        const;
    Result<bool, nsresult>
    ComputeRangesToDeleteNodesEntirelyInRangeButKeepTableStructure(
        const HTMLEditor& aHTMLEditor, nsRange& aRange,
        AutoDeleteRangesHandler::SelectionWasCollapsed aSelectionWasCollapsed)
        const;

    /**
     * DeleteContentButKeepTableStructure() removes aContent if it's an element
     * which is part of a table structure.  If it's a part of table structure,
     * removes its all children recursively.  I.e., this may delete all of a
     * table, but won't break table structure partially.
     *
     * @param aContent            The content which or whose all children should
     *                            be removed.
     */
    [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
    DeleteContentButKeepTableStructure(HTMLEditor& aHTMLEditor,
                                       nsIContent& aContent);

    /**
     * DeleteTextAtStartAndEndOfRange() removes text if start and/or end of
     * aRange is in a text node.
     */
    [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
    DeleteTextAtStartAndEndOfRange(HTMLEditor& aHTMLEditor, nsRange& aRange);

    class MOZ_STACK_CLASS AutoInclusiveAncestorBlockElementsJoiner final {
     public:
      AutoInclusiveAncestorBlockElementsJoiner() = delete;
      AutoInclusiveAncestorBlockElementsJoiner(
          nsIContent& aInclusiveDescendantOfLeftBlockElement,
          nsIContent& aInclusiveDescendantOfRightBlockElement)
          : mInclusiveDescendantOfLeftBlockElement(
                aInclusiveDescendantOfLeftBlockElement),
            mInclusiveDescendantOfRightBlockElement(
                aInclusiveDescendantOfRightBlockElement),
            mCanJoinBlocks(false),
            mFallbackToDeleteLeafContent(false) {}

      bool IsSet() const { return mLeftBlockElement && mRightBlockElement; }
      bool IsSameBlockElement() const {
        return mLeftBlockElement && mLeftBlockElement == mRightBlockElement;
      }

      /**
       * Prepare for joining inclusive ancestor block elements.  When this
       * returns false, the deletion should be canceled.
       */
      Result<bool, nsresult> Prepare(const HTMLEditor& aHTMLEditor);

      /**
       * When this returns true, this can join the blocks with `Run()`.
       */
      bool CanJoinBlocks() const { return mCanJoinBlocks; }

      /**
       * When this returns true, `Run()` must return "ignored" so that
       * caller can skip calling `Run()`.  This is available only when
       * `CanJoinBlocks()` returns `true`.
       * TODO: This should be merged into `CanJoinBlocks()` in the future.
       */
      bool ShouldDeleteLeafContentInstead() const {
        MOZ_ASSERT(CanJoinBlocks());
        return mFallbackToDeleteLeafContent;
      }

      /**
       * ComputeRangesToDelete() extends aRangesToDelete includes the element
       * boundaries between joining blocks.  If they won't be joined, this
       * collapses the range to aCaretPoint.
       */
      nsresult ComputeRangesToDelete(const HTMLEditor& aHTMLEditor,
                                     const EditorDOMPoint& aCaretPoint,
                                     AutoRangeArray& aRangesToDelete) const;

      /**
       * Join inclusive ancestor block elements which are found by preceding
       * Preare() call.
       * The right element is always joined to the left element.
       * If the elements are the same type and not nested within each other,
       * JoinEditableNodesWithTransaction() is called (example, joining two
       * list items together into one).
       * If the elements are not the same type, or one is a descendant of the
       * other, we instead destroy the right block placing its children into
       * left block.
       */
      [[nodiscard]] MOZ_CAN_RUN_SCRIPT EditActionResult
      Run(HTMLEditor& aHTMLEditor);

     private:
      /**
       * This method returns true when
       * `MergeFirstLineOfRightBlockElementIntoDescendantLeftBlockElement()`,
       * `MergeFirstLineOfRightBlockElementIntoAncestorLeftBlockElement()` and
       * `MergeFirstLineOfRightBlockElementIntoLeftBlockElement()` handle it
       * with the `if` block of their main blocks.
       */
      bool CanMergeLeftAndRightBlockElements() const {
        if (!IsSet()) {
          return false;
        }
        // `MergeFirstLineOfRightBlockElementIntoDescendantLeftBlockElement()`
        if (mPointContainingTheOtherBlockElement.GetContainer() ==
            mRightBlockElement) {
          return mNewListElementTagNameOfRightListElement.isSome();
        }
        // `MergeFirstLineOfRightBlockElementIntoAncestorLeftBlockElement()`
        if (mPointContainingTheOtherBlockElement.GetContainer() ==
            mLeftBlockElement) {
          return mNewListElementTagNameOfRightListElement.isSome() &&
                 !mRightBlockElement->GetChildCount();
        }
        MOZ_ASSERT(!mPointContainingTheOtherBlockElement.IsSet());
        // `MergeFirstLineOfRightBlockElementIntoLeftBlockElement()`
        return mNewListElementTagNameOfRightListElement.isSome() ||
               mLeftBlockElement->NodeInfo()->NameAtom() ==
                   mRightBlockElement->NodeInfo()->NameAtom();
      }

      OwningNonNull<nsIContent> mInclusiveDescendantOfLeftBlockElement;
      OwningNonNull<nsIContent> mInclusiveDescendantOfRightBlockElement;
      RefPtr<Element> mLeftBlockElement;
      RefPtr<Element> mRightBlockElement;
      Maybe<nsAtom*> mNewListElementTagNameOfRightListElement;
      EditorDOMPoint mPointContainingTheOtherBlockElement;
      RefPtr<dom::HTMLBRElement> mPrecedingInvisibleBRElement;
      bool mCanJoinBlocks;
      bool mFallbackToDeleteLeafContent;
    };  // HTMLEditor::AutoDeleteRangesHandler::AutoBlockElementsJoiner::
        // AutoInclusiveAncestorBlockElementsJoiner

    enum class Mode {
      NotInitialized,
      JoinCurrentBlock,
      JoinOtherBlock,
      JoinBlocksInSameParent,
      DeleteBRElement,
      DeleteContentInRanges,
      DeleteNonCollapsedRanges,
    };
    AutoDeleteRangesHandler* mDeleteRangesHandler;
    const AutoDeleteRangesHandler& mDeleteRangesHandlerConst;
    nsCOMPtr<nsIContent> mLeftContent;
    nsCOMPtr<nsIContent> mRightContent;
    nsCOMPtr<nsIContent> mLeafContentInOtherBlock;
    RefPtr<dom::HTMLBRElement> mBRElement;
    Mode mMode = Mode::NotInitialized;
  };  // HTMLEditor::AutoDeleteRangesHandler::AutoBlockElementsJoiner

  class MOZ_STACK_CLASS AutoEmptyBlockAncestorDeleter final {
   public:
    /**
     * ScanEmptyBlockInclusiveAncestor() scans an inclusive ancestor element
     * which is empty and a block element.  Then, stores the result and
     * returns the found empty block element.
     *
     * @param aHTMLEditor         The HTMLEditor.
     * @param aStartContent       Start content to look for empty ancestors.
     */
    [[nodiscard]] Element* ScanEmptyBlockInclusiveAncestor(
        const HTMLEditor& aHTMLEditor, nsIContent& aStartContent);

    /**
     * ComputeTargetRanges() computes "target ranges" for deleting
     * `mEmptyInclusiveAncestorBlockElement`.
     */
    nsresult ComputeTargetRanges(const HTMLEditor& aHTMLEditor,
                                 nsIEditor::EDirection aDirectionAndAmount,
                                 const Element& aEditingHost,
                                 AutoRangeArray& aRangesToDelete) const;

    /**
     * Deletes found empty block element by `ScanEmptyBlockInclusiveAncestor()`.
     * If found one is a list item element, calls
     * `MaybeInsertBRElementBeforeEmptyListItemElement()` before deleting
     * the list item element.
     * If found empty ancestor is not a list item element,
     * `GetNewCaretPoisition()` will be called to determine new caret position.
     * Finally, removes the empty block ancestor.
     *
     * @param aHTMLEditor         The HTMLEditor.
     * @param aDirectionAndAmount If found empty ancestor block is a list item
     *                            element, this is ignored.  Otherwise:
     *                            - If eNext, eNextWord or eToEndOfLine,
     *                              collapse Selection to after found empty
     *                              ancestor.
     *                            - If ePrevious, ePreviousWord or
     *                              eToBeginningOfLine, collapse Selection to
     *                              end of previous editable node.
     *                            - Otherwise, eNone is allowed but does
     *                              nothing.
     */
    [[nodiscard]] MOZ_CAN_RUN_SCRIPT EditActionResult
    Run(HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount);

   private:
    /**
     * MaybeInsertBRElementBeforeEmptyListItemElement() inserts a `<br>` element
     * if `mEmptyInclusiveAncestorBlockElement` is a list item element which
     * is first editable element in its parent, and its grand parent is not a
     * list element, inserts a `<br>` element before the empty list item.
     */
    [[nodiscard]] MOZ_CAN_RUN_SCRIPT Result<RefPtr<Element>, nsresult>
    MaybeInsertBRElementBeforeEmptyListItemElement(HTMLEditor& aHTMLEditor);

    /**
     * GetNewCaretPoisition() returns new caret position after deleting
     * `mEmptyInclusiveAncestorBlockElement`.
     */
    [[nodiscard]] Result<EditorDOMPoint, nsresult> GetNewCaretPoisition(
        const HTMLEditor& aHTMLEditor,
        nsIEditor::EDirection aDirectionAndAmount) const;

    RefPtr<Element> mEmptyInclusiveAncestorBlockElement;
  };  // HTMLEditor::AutoDeleteRangesHandler::AutoEmptyBlockAncestorDeleter

  const AutoDeleteRangesHandler* const mParent;
  nsIEditor::EDirection mOriginalDirectionAndAmount;
  nsIEditor::EStripWrappers mOriginalStripWrappers;
};  // HTMLEditor::AutoDeleteRangesHandler

nsresult HTMLEditor::ComputeTargetRanges(
    nsIEditor::EDirection aDirectionAndAmount,
    AutoRangeArray& aRangesToDelete) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  Element* editingHost = GetActiveEditingHost();
  if (!editingHost) {
    aRangesToDelete.RemoveAllRanges();
    return NS_ERROR_EDITOR_NO_EDITABLE_RANGE;
  }

  // First check for table selection mode.  If so, hand off to table editor.
  SelectedTableCellScanner scanner(aRangesToDelete);
  if (scanner.IsInTableCellSelectionMode()) {
    // If it's in table cell selection mode, we'll delete all childen in
    // the all selected table cell elements,
    if (scanner.ElementsRef().Length() == aRangesToDelete.Ranges().Length()) {
      return NS_OK;
    }
    // but will ignore all ranges which does not select a table cell.
    size_t removedRanges = 0;
    for (size_t i = 1; i < scanner.ElementsRef().Length(); i++) {
      if (HTMLEditUtils::GetTableCellElementIfOnlyOneSelected(
              aRangesToDelete.Ranges()[i - removedRanges]) !=
          scanner.ElementsRef()[i]) {
        // XXX Need to manage anchor-focus range too!
        aRangesToDelete.Ranges().RemoveElementAt(i - removedRanges);
        removedRanges++;
      }
    }
    return NS_OK;
  }

  aRangesToDelete.EnsureOnlyEditableRanges(*editingHost);
  if (aRangesToDelete.Ranges().IsEmpty()) {
    NS_WARNING(
        "There is no range which we can delete entire of or around the caret");
    return NS_ERROR_EDITOR_NO_EDITABLE_RANGE;
  }
  AutoDeleteRangesHandler deleteHandler;
  // Should we delete target ranges which cannot delete actually?
  nsresult rv = deleteHandler.ComputeRangesToDelete(*this, aDirectionAndAmount,
                                                    aRangesToDelete);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "AutoDeleteRangesHandler::ComputeRangesToDelete() failed");
  return rv;
}

EditActionResult HTMLEditor::HandleDeleteSelection(
    nsIEditor::EDirection aDirectionAndAmount,
    nsIEditor::EStripWrappers aStripWrappers) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(aStripWrappers == nsIEditor::eStrip ||
             aStripWrappers == nsIEditor::eNoStrip);

  if (!SelectionRef().RangeCount()) {
    return EditActionHandled(NS_ERROR_EDITOR_NO_EDITABLE_RANGE);
  }

  Element* editingHost = GetActiveEditingHost();
  if (!editingHost) {
    return EditActionHandled(NS_ERROR_EDITOR_NO_EDITABLE_RANGE);
  }

  // Remember that we did a selection deletion.  Used by
  // CreateStyleForInsertText()
  TopLevelEditSubActionDataRef().mDidDeleteSelection = true;

  if (IsEmpty()) {
    return EditActionCanceled();
  }

  // First check for table selection mode.  If so, hand off to table editor.
  if (HTMLEditUtils::IsInTableCellSelectionMode(SelectionRef())) {
    nsresult rv = DeleteTableCellContentsWithTransaction();
    if (NS_WARN_IF(Destroyed())) {
      return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::DeleteTableCellContentsWithTransaction() failed");
    return EditActionHandled(rv);
  }

  AutoRangeArray rangesToDelete(SelectionRef());
  rangesToDelete.EnsureOnlyEditableRanges(*editingHost);
  if (rangesToDelete.Ranges().IsEmpty()) {
    NS_WARNING(
        "There is no range which we can delete entire the ranges or around the "
        "caret");
    return EditActionHandled(NS_ERROR_EDITOR_NO_EDITABLE_RANGE);
  }
  AutoDeleteRangesHandler deleteHandler;
  EditActionResult result = deleteHandler.Run(*this, aDirectionAndAmount,
                                              aStripWrappers, rangesToDelete);
  if (result.Failed() || result.Canceled()) {
    NS_WARNING_ASSERTION(result.Succeeded(),
                         "AutoDeleteRangesHandler::Run() failed");
    return result;
  }

  // XXX At here, selection may have no range because of mutation event
  //     listeners can do anything so that we should just return NS_OK instead
  //     of returning error.
  EditorDOMPoint atNewStartOfSelection(
      EditorBase::GetStartPoint(SelectionRef()));
  if (NS_WARN_IF(!atNewStartOfSelection.IsSet())) {
    return EditActionHandled(NS_ERROR_FAILURE);
  }
  if (atNewStartOfSelection.GetContainerAsContent()) {
    nsresult rv = DeleteMostAncestorMailCiteElementIfEmpty(
        MOZ_KnownLive(*atNewStartOfSelection.GetContainerAsContent()));
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "HTMLEditor::DeleteMostAncestorMailCiteElementIfEmpty() failed");
      return EditActionHandled(rv);
    }
  }
  return EditActionHandled();
}

nsresult HTMLEditor::AutoDeleteRangesHandler::ComputeRangesToDelete(
    const HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
    AutoRangeArray& aRangesToDelete) {
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());
  MOZ_ASSERT(!aRangesToDelete.Ranges().IsEmpty());

  mOriginalDirectionAndAmount = aDirectionAndAmount;
  mOriginalStripWrappers = nsIEditor::eNoStrip;

  if (aHTMLEditor.mPaddingBRElementForEmptyEditor) {
    nsresult rv = aRangesToDelete.Collapse(
        EditorRawDOMPoint(aHTMLEditor.mPaddingBRElementForEmptyEditor));
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "AutoRangeArray::Collapse() failed");
    return rv;
  }

  SelectionWasCollapsed selectionWasCollapsed = aRangesToDelete.IsCollapsed()
                                                    ? SelectionWasCollapsed::Yes
                                                    : SelectionWasCollapsed::No;
  if (selectionWasCollapsed == SelectionWasCollapsed::Yes) {
    EditorDOMPoint startPoint(aRangesToDelete.GetStartPointOfFirstRange());
    if (NS_WARN_IF(!startPoint.IsSet())) {
      return NS_ERROR_FAILURE;
    }
    RefPtr<Element> editingHost = aHTMLEditor.GetActiveEditingHost();
    if (NS_WARN_IF(!editingHost)) {
      return NS_ERROR_FAILURE;
    }
    if (startPoint.GetContainerAsContent()) {
      AutoEmptyBlockAncestorDeleter deleter;
      if (deleter.ScanEmptyBlockInclusiveAncestor(
              aHTMLEditor, *startPoint.GetContainerAsContent())) {
        nsresult rv = deleter.ComputeTargetRanges(
            aHTMLEditor, aDirectionAndAmount, *editingHost, aRangesToDelete);
        NS_WARNING_ASSERTION(
            NS_SUCCEEDED(rv),
            "AutoEmptyBlockAncestorDeleter::ComputeTargetRanges() failed");
        return rv;
      }
    }

    // We shouldn't update caret bidi level right now, but we need to check
    // whether the deletion will be canceled or not.
    AutoCaretBidiLevelManager bidiLevelManager(aHTMLEditor, aDirectionAndAmount,
                                               startPoint);
    if (bidiLevelManager.Failed()) {
      NS_WARNING(
          "EditorBase::AutoCaretBidiLevelManager failed to initialize itself");
      return NS_ERROR_FAILURE;
    }
    if (bidiLevelManager.Canceled()) {
      return NS_SUCCESS_DOM_NO_OPERATION;
    }

    // AutoRangeArray::ExtendAnchorFocusRangeFor() will use `nsFrameSelection`
    // to extend the range for deletion.  But if focus event doesn't receive
    // yet, ancestor isn't set.  So we must set root element of editor to
    // ancestor temporarily.
    AutoSetTemporaryAncestorLimiter autoSetter(
        aHTMLEditor, aHTMLEditor.SelectionRef(), *startPoint.GetContainer(),
        &aRangesToDelete);

    Result<nsIEditor::EDirection, nsresult> extendResult =
        aRangesToDelete.ExtendAnchorFocusRangeFor(aHTMLEditor,
                                                  aDirectionAndAmount);
    if (extendResult.isErr()) {
      NS_WARNING("AutoRangeArray::ExtendAnchorFocusRangeFor() failed");
      return extendResult.unwrapErr();
    }

    // For compatibility with other browsers, we should set target ranges
    // to start from and/or end after an atomic content rather than start
    // from preceding text node end nor end at following text node start.
    Result<bool, nsresult> shrunkenResult =
        aRangesToDelete.ShrinkRangesIfStartFromOrEndAfterAtomicContent(
            aHTMLEditor, aDirectionAndAmount,
            AutoRangeArray::IfSelectingOnlyOneAtomicContent::Collapse,
            editingHost);
    if (shrunkenResult.isErr()) {
      NS_WARNING(
          "AutoRangeArray::ShrinkRangesIfStartFromOrEndAfterAtomicContent() "
          "failed");
      return shrunkenResult.unwrapErr();
    }

    if (!shrunkenResult.inspect() || !aRangesToDelete.IsCollapsed()) {
      aDirectionAndAmount = extendResult.unwrap();
    }

    if (aDirectionAndAmount == nsIEditor::eNone) {
      MOZ_ASSERT(aRangesToDelete.Ranges().Length() == 1);
      if (!CanFallbackToDeleteRangesWithTransaction(aRangesToDelete)) {
        // XXX In this case, do we need to modify the range again?
        return NS_SUCCESS_DOM_NO_OPERATION;
      }
      nsresult rv = FallbackToComputeRangesToDeleteRangesWithTransaction(
          aHTMLEditor, aRangesToDelete);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "AutoDeleteRangesHandler::"
          "FallbackToComputeRangesToDeleteRangesWithTransaction() failed");
      return rv;
    }

    if (aRangesToDelete.IsCollapsed()) {
      EditorDOMPoint caretPoint(aRangesToDelete.GetStartPointOfFirstRange());
      if (NS_WARN_IF(!caretPoint.IsInContentNode())) {
        return NS_ERROR_FAILURE;
      }
      if (!EditorUtils::IsEditableContent(*caretPoint.ContainerAsContent(),
                                          EditorType::HTML)) {
        return NS_SUCCESS_DOM_NO_OPERATION;
      }
      WSRunScanner wsRunScannerAtCaret(editingHost, caretPoint);
      WSScanResult scanFromCaretPointResult =
          aDirectionAndAmount == nsIEditor::eNext
              ? wsRunScannerAtCaret.ScanNextVisibleNodeOrBlockBoundaryFrom(
                    caretPoint)
              : wsRunScannerAtCaret.ScanPreviousVisibleNodeOrBlockBoundaryFrom(
                    caretPoint);
      if (scanFromCaretPointResult.Failed()) {
        NS_WARNING(
            "WSRunScanner::Scan(Next|Previous)VisibleNodeOrBlockBoundaryFrom() "
            "failed");
        return NS_ERROR_FAILURE;
      }
      if (!scanFromCaretPointResult.GetContent()) {
        return NS_SUCCESS_DOM_NO_OPERATION;
      }

      if (scanFromCaretPointResult.ReachedBRElement()) {
        if (scanFromCaretPointResult.BRElementPtr() ==
            wsRunScannerAtCaret.GetEditingHost()) {
          return NS_OK;
        }
        if (!EditorUtils::IsEditableContent(
                *scanFromCaretPointResult.BRElementPtr(), EditorType::HTML)) {
          return NS_SUCCESS_DOM_NO_OPERATION;
        }
        if (HTMLEditUtils::IsInvisibleBRElement(
                *scanFromCaretPointResult.BRElementPtr(), editingHost)) {
          EditorDOMPoint newCaretPosition =
              aDirectionAndAmount == nsIEditor::eNext
                  ? EditorDOMPoint::After(
                        *scanFromCaretPointResult.BRElementPtr())
                  : EditorDOMPoint(scanFromCaretPointResult.BRElementPtr());
          if (NS_WARN_IF(!newCaretPosition.IsSet())) {
            return NS_ERROR_FAILURE;
          }
          AutoHideSelectionChanges blockSelectionListeners(
              aHTMLEditor.SelectionRef());
          nsresult rv = aHTMLEditor.CollapseSelectionTo(newCaretPosition);
          if (NS_FAILED(rv)) {
            NS_WARNING("HTMLEditor::CollapseSelectionTo() failed");
            return NS_ERROR_FAILURE;
          }
          if (NS_WARN_IF(!aHTMLEditor.SelectionRef().RangeCount())) {
            return NS_ERROR_UNEXPECTED;
          }
          aRangesToDelete.Initialize(aHTMLEditor.SelectionRef());
          AutoDeleteRangesHandler anotherHandler(this);
          rv = anotherHandler.ComputeRangesToDelete(
              aHTMLEditor, aDirectionAndAmount, aRangesToDelete);
          NS_WARNING_ASSERTION(
              NS_SUCCEEDED(rv),
              "Recursive AutoDeleteRangesHandler::ComputeRangesToDelete() "
              "failed");

          DebugOnly<nsresult> rvIgnored =
              aHTMLEditor.CollapseSelectionTo(caretPoint);
          NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                               "HTMLEditor::CollapseSelectionTo() failed to "
                               "restore original selection");

          MOZ_ASSERT(aRangesToDelete.Ranges().Length() == 1);
          // If the range is collapsed, there is no content which should
          // be removed together.  In this case, only the invisible `<br>`
          // element should be selected.
          if (aRangesToDelete.IsCollapsed()) {
            nsresult rv = aRangesToDelete.SelectNode(
                *scanFromCaretPointResult.BRElementPtr());
            NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                                 "AutoRangeArray::SelectNode() failed");
            return rv;
          }

          // Otherwise, extend the range to contain the invisible `<br>`
          // element.
          if (EditorRawDOMPoint(scanFromCaretPointResult.BRElementPtr())
                  .IsBefore(aRangesToDelete.GetStartPointOfFirstRange())) {
            nsresult rv = aRangesToDelete.FirstRangeRef()->SetStartAndEnd(
                EditorRawDOMPoint(scanFromCaretPointResult.BRElementPtr())
                    .ToRawRangeBoundary(),
                aRangesToDelete.FirstRangeRef()->EndRef());
            NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                                 "nsRange::SetStartAndEnd() failed");
            return rv;
          }
          if (aRangesToDelete.GetEndPointOfFirstRange().IsBefore(
                  EditorRawDOMPoint::After(
                      *scanFromCaretPointResult.BRElementPtr()))) {
            nsresult rv = aRangesToDelete.FirstRangeRef()->SetStartAndEnd(
                aRangesToDelete.FirstRangeRef()->StartRef(),
                EditorRawDOMPoint::After(
                    *scanFromCaretPointResult.BRElementPtr())
                    .ToRawRangeBoundary());
            NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                                 "nsRange::SetStartAndEnd() failed");
            return rv;
          }
          NS_WARNING("Was the invisible `<br>` element selected?");
          return NS_OK;
        }
      }

      nsresult rv = ComputeRangesToDeleteAroundCollapsedRanges(
          aHTMLEditor, aDirectionAndAmount, aRangesToDelete,
          wsRunScannerAtCaret, scanFromCaretPointResult);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "AutoDeleteRangesHandler::ComputeRangesToDeleteAroundCollapsedRanges("
          ") failed");
      return rv;
    }
  }

  nsresult rv = ComputeRangesToDeleteNonCollapsedRanges(
      aHTMLEditor, aDirectionAndAmount, aRangesToDelete, selectionWasCollapsed);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "AutoDeleteRangesHandler::"
                       "ComputeRangesToDeleteNonCollapsedRanges() failed");
  return rv;
}

EditActionResult HTMLEditor::AutoDeleteRangesHandler::Run(
    HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
    nsIEditor::EStripWrappers aStripWrappers, AutoRangeArray& aRangesToDelete) {
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());
  MOZ_ASSERT(aStripWrappers == nsIEditor::eStrip ||
             aStripWrappers == nsIEditor::eNoStrip);
  MOZ_ASSERT(!aRangesToDelete.Ranges().IsEmpty());

  mOriginalDirectionAndAmount = aDirectionAndAmount;
  mOriginalStripWrappers = aStripWrappers;

  if (aHTMLEditor.IsEmpty()) {
    return EditActionCanceled();
  }

  // selectionWasCollapsed is used later to determine whether we should join
  // blocks in HandleDeleteNonCollapsedRanges(). We don't really care about
  // collapsed because it will be modified by
  // AutoRangeArray::ExtendAnchorFocusRangeFor() later.
  // AutoBlockElementsJoiner::AutoInclusiveAncestorBlockElementsJoiner should
  // happen if the original selection is collapsed and the cursor is at the end
  // of a block element, in which case
  // AutoRangeArray::ExtendAnchorFocusRangeFor() would always make the selection
  // not collapsed.
  SelectionWasCollapsed selectionWasCollapsed = aRangesToDelete.IsCollapsed()
                                                    ? SelectionWasCollapsed::Yes
                                                    : SelectionWasCollapsed::No;

  if (selectionWasCollapsed == SelectionWasCollapsed::Yes) {
    EditorDOMPoint startPoint(aRangesToDelete.GetStartPointOfFirstRange());
    if (NS_WARN_IF(!startPoint.IsSet())) {
      return EditActionResult(NS_ERROR_FAILURE);
    }

    // If we are inside an empty block, delete it.
    RefPtr<Element> editingHost = aHTMLEditor.GetActiveEditingHost();
    if (startPoint.GetContainerAsContent()) {
      if (NS_WARN_IF(!editingHost)) {
        return EditActionResult(NS_ERROR_FAILURE);
      }
#ifdef DEBUG
      nsMutationGuard debugMutation;
#endif  // #ifdef DEBUG
      AutoEmptyBlockAncestorDeleter deleter;
      if (deleter.ScanEmptyBlockInclusiveAncestor(
              aHTMLEditor, *startPoint.GetContainerAsContent())) {
        EditActionResult result = deleter.Run(aHTMLEditor, aDirectionAndAmount);
        if (result.Failed() || result.Handled()) {
          NS_WARNING_ASSERTION(result.Succeeded(),
                               "AutoEmptyBlockAncestorDeleter::Run() failed");
          return result;
        }
      }
      MOZ_ASSERT(!debugMutation.Mutated(0),
                 "AutoEmptyBlockAncestorDeleter shouldn't modify the DOM tree "
                 "if it returns not handled nor error");
    }

    // Test for distance between caret and text that will be deleted.
    // Note that this call modifies `nsFrameSelection` without modifying
    // `Selection`.  However, it does not have problem for now because
    // it'll be referred by `AutoRangeArray::ExtendAnchorFocusRangeFor()`
    // before modifying `Selection`.
    // XXX This looks odd.  `ExtendAnchorFocusRangeFor()` will extend
    //     anchor-focus range, but here refers the first range.
    AutoCaretBidiLevelManager bidiLevelManager(aHTMLEditor, aDirectionAndAmount,
                                               startPoint);
    if (bidiLevelManager.Failed()) {
      NS_WARNING(
          "EditorBase::AutoCaretBidiLevelManager failed to initialize itself");
      return EditActionResult(NS_ERROR_FAILURE);
    }
    bidiLevelManager.MaybeUpdateCaretBidiLevel(aHTMLEditor);
    if (bidiLevelManager.Canceled()) {
      return EditActionCanceled();
    }

    // AutoRangeArray::ExtendAnchorFocusRangeFor() will use `nsFrameSelection`
    // to extend the range for deletion.  But if focus event doesn't receive
    // yet, ancestor isn't set.  So we must set root element of editor to
    // ancestor temporarily.
    AutoSetTemporaryAncestorLimiter autoSetter(
        aHTMLEditor, aHTMLEditor.SelectionRef(), *startPoint.GetContainer(),
        &aRangesToDelete);

    // Calling `ExtendAnchorFocusRangeFor()` and
    // `ShrinkRangesIfStartFromOrEndAfterAtomicContent()` may move caret to
    // the container of deleting atomic content.  However, it may be different
    // from the original caret's container.  The original caret container may
    // be important to put caret after deletion so that let's cache the
    // original position.
    Maybe<EditorDOMPoint> caretPoint;
    if (aRangesToDelete.IsCollapsed() && !aRangesToDelete.Ranges().IsEmpty()) {
      caretPoint = Some(aRangesToDelete.GetStartPointOfFirstRange());
      if (NS_WARN_IF(!caretPoint.ref().IsInContentNode())) {
        return EditActionResult(NS_ERROR_FAILURE);
      }
    }

    Result<nsIEditor::EDirection, nsresult> extendResult =
        aRangesToDelete.ExtendAnchorFocusRangeFor(aHTMLEditor,
                                                  aDirectionAndAmount);
    if (extendResult.isErr()) {
      NS_WARNING("AutoRangeArray::ExtendAnchorFocusRangeFor() failed");
      return EditActionResult(extendResult.unwrapErr());
    }
    if (caretPoint.isSome() && !caretPoint.ref().IsSetAndValid()) {
      NS_WARNING("The caret position became invalid");
      return EditActionHandled(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
    }

    // If there is only one range and it selects an atomic content, we should
    // delete it with collapsed range path for making consistent behavior
    // between both cases, the content is selected case and caret is at it or
    // after it case.
    Result<bool, nsresult> shrunkenResult =
        aRangesToDelete.ShrinkRangesIfStartFromOrEndAfterAtomicContent(
            aHTMLEditor, aDirectionAndAmount,
            AutoRangeArray::IfSelectingOnlyOneAtomicContent::Collapse,
            editingHost);
    if (shrunkenResult.isErr()) {
      NS_WARNING(
          "AutoRangeArray::ShrinkRangesIfStartFromOrEndAfterAtomicContent() "
          "failed");
      return EditActionResult(shrunkenResult.unwrapErr());
    }

    if (!shrunkenResult.inspect() || !aRangesToDelete.IsCollapsed()) {
      aDirectionAndAmount = extendResult.unwrap();
    }

    if (aDirectionAndAmount == nsIEditor::eNone) {
      MOZ_ASSERT(aRangesToDelete.Ranges().Length() == 1);
      if (!CanFallbackToDeleteRangesWithTransaction(aRangesToDelete)) {
        return EditActionIgnored();
      }
      EditActionResult result =
          FallbackToDeleteRangesWithTransaction(aHTMLEditor, aRangesToDelete);
      NS_WARNING_ASSERTION(result.Succeeded(),
                           "AutoDeleteRangesHandler::"
                           "FallbackToDeleteRangesWithTransaction() failed");
      return result;
    }

    if (aRangesToDelete.IsCollapsed()) {
      // Use the original caret position for handling the deletion around
      // collapsed range because the container may be different from the
      // new collapsed position's container.
      if (!EditorUtils::IsEditableContent(
              *caretPoint.ref().ContainerAsContent(), EditorType::HTML)) {
        return EditActionCanceled();
      }
      WSRunScanner wsRunScannerAtCaret(editingHost, caretPoint.ref());
      WSScanResult scanFromCaretPointResult =
          aDirectionAndAmount == nsIEditor::eNext
              ? wsRunScannerAtCaret.ScanNextVisibleNodeOrBlockBoundaryFrom(
                    caretPoint.ref())
              : wsRunScannerAtCaret.ScanPreviousVisibleNodeOrBlockBoundaryFrom(
                    caretPoint.ref());
      if (scanFromCaretPointResult.Failed()) {
        NS_WARNING(
            "WSRunScanner::Scan(Next|Previous)VisibleNodeOrBlockBoundaryFrom() "
            "failed");
        return EditActionResult(NS_ERROR_FAILURE);
      }
      if (!scanFromCaretPointResult.GetContent()) {
        return EditActionCanceled();
      }
      // Short circuit for invisible breaks.  delete them and recurse.
      if (scanFromCaretPointResult.ReachedBRElement()) {
        if (scanFromCaretPointResult.BRElementPtr() == editingHost) {
          return EditActionHandled();
        }
        if (!EditorUtils::IsEditableContent(
                *scanFromCaretPointResult.BRElementPtr(), EditorType::HTML)) {
          return EditActionCanceled();
        }
        if (HTMLEditUtils::IsInvisibleBRElement(
                *scanFromCaretPointResult.BRElementPtr(), editingHost)) {
          // TODO: We should extend the range to delete again before/after
          //       the caret point and use `HandleDeleteNonCollapsedRanges()`
          //       instead after we would create delete range computation
          //       method at switching to the new white-space normalizer.
          nsresult rv = WhiteSpaceVisibilityKeeper::
              DeleteContentNodeAndJoinTextNodesAroundIt(
                  aHTMLEditor,
                  MOZ_KnownLive(*scanFromCaretPointResult.BRElementPtr()),
                  caretPoint.ref());
          if (NS_FAILED(rv)) {
            NS_WARNING(
                "WhiteSpaceVisibilityKeeper::"
                "DeleteContentNodeAndJoinTextNodesAroundIt() failed");
            return EditActionHandled(rv);
          }
          if (aHTMLEditor.SelectionRef().RangeCount() != 1) {
            NS_WARNING(
                "Selection was unexpected after removing an invisible `<br>` "
                "element");
            return EditActionHandled(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
          }
          AutoRangeArray rangesToDelete(aHTMLEditor.SelectionRef());
          caretPoint = Some(aRangesToDelete.GetStartPointOfFirstRange());
          if (!caretPoint.ref().IsSet()) {
            NS_WARNING(
                "New selection after deleting invisible `<br>` element was "
                "invalid");
            return EditActionHandled(NS_ERROR_FAILURE);
          }
          if (aHTMLEditor.MayHaveMutationEventListeners(
                  NS_EVENT_BITS_MUTATION_SUBTREEMODIFIED |
                  NS_EVENT_BITS_MUTATION_NODEREMOVED |
                  NS_EVENT_BITS_MUTATION_NODEREMOVEDFROMDOCUMENT)) {
            // Let's check whether there is new invisible `<br>` element
            // for avoiding infinit recursive calls.
            WSRunScanner wsRunScannerAtCaret(editingHost, caretPoint.ref());
            WSScanResult scanFromCaretPointResult =
                aDirectionAndAmount == nsIEditor::eNext
                    ? wsRunScannerAtCaret
                          .ScanNextVisibleNodeOrBlockBoundaryFrom(
                              caretPoint.ref())
                    : wsRunScannerAtCaret
                          .ScanPreviousVisibleNodeOrBlockBoundaryFrom(
                              caretPoint.ref());
            if (scanFromCaretPointResult.Failed()) {
              NS_WARNING(
                  "WSRunScanner::Scan(Next|Previous)"
                  "VisibleNodeOrBlockBoundaryFrom() failed");
              return EditActionResult(NS_ERROR_FAILURE);
            }
            if (scanFromCaretPointResult.ReachedInvisibleBRElement()) {
              return EditActionHandled(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
            }
          }
          AutoDeleteRangesHandler anotherHandler(this);
          EditActionResult result = anotherHandler.Run(
              aHTMLEditor, aDirectionAndAmount, aStripWrappers, rangesToDelete);
          NS_WARNING_ASSERTION(
              result.Succeeded(),
              "Recursive AutoDeleteRangesHandler::Run() failed");
          return result;
        }
      }

      EditActionResult result = HandleDeleteAroundCollapsedRanges(
          aHTMLEditor, aDirectionAndAmount, aStripWrappers, aRangesToDelete,
          wsRunScannerAtCaret, scanFromCaretPointResult);
      NS_WARNING_ASSERTION(result.Succeeded(),
                           "AutoDeleteRangesHandler::"
                           "HandleDeleteAroundCollapsedRanges() failed");
      return result;
    }
  }

  EditActionResult result = HandleDeleteNonCollapsedRanges(
      aHTMLEditor, aDirectionAndAmount, aStripWrappers, aRangesToDelete,
      selectionWasCollapsed);
  NS_WARNING_ASSERTION(
      result.Succeeded(),
      "AutoDeleteRangesHandler::HandleDeleteNonCollapsedRanges() failed");
  return result;
}

nsresult
HTMLEditor::AutoDeleteRangesHandler::ComputeRangesToDeleteAroundCollapsedRanges(
    const HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
    AutoRangeArray& aRangesToDelete, const WSRunScanner& aWSRunScannerAtCaret,
    const WSScanResult& aScanFromCaretPointResult) const {
  if (aScanFromCaretPointResult.InNormalWhiteSpaces() ||
      aScanFromCaretPointResult.InNormalText()) {
    nsresult rv = aRangesToDelete.Collapse(aScanFromCaretPointResult.Point());
    if (NS_FAILED(rv)) {
      NS_WARNING("AutoRangeArray::Collapse() failed");
      return NS_ERROR_FAILURE;
    }
    rv = ComputeRangesToDeleteTextAroundCollapsedRanges(
        aWSRunScannerAtCaret.GetEditingHost(), aDirectionAndAmount,
        aRangesToDelete);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "AutoDeleteRangesHandler::"
        "ComputeRangesToDeleteTextAroundCollapsedRanges() failed");
    return rv;
  }

  if (aScanFromCaretPointResult.ReachedSpecialContent() ||
      aScanFromCaretPointResult.ReachedBRElement() ||
      aScanFromCaretPointResult.ReachedNonEditableOtherBlockElement()) {
    if (aScanFromCaretPointResult.GetContent() ==
        aWSRunScannerAtCaret.GetEditingHost()) {
      return NS_OK;
    }
    nsIContent* atomicContent = GetAtomicContentToDelete(
        aDirectionAndAmount, aWSRunScannerAtCaret, aScanFromCaretPointResult);
    if (!HTMLEditUtils::IsRemovableNode(*atomicContent)) {
      NS_WARNING(
          "AutoDeleteRangesHandler::GetAtomicContentToDelete() cannot find "
          "removable atomic content");
      return NS_ERROR_FAILURE;
    }
    nsresult rv = ComputeRangesToDeleteAtomicContent(
        aWSRunScannerAtCaret.GetEditingHost(), *atomicContent, aRangesToDelete);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "AutoDeleteRangesHandler::ComputeRangesToDeleteAtomicContent() failed");
    return rv;
  }

  if (aScanFromCaretPointResult.ReachedHRElement()) {
    if (aScanFromCaretPointResult.GetContent() ==
        aWSRunScannerAtCaret.GetEditingHost()) {
      return NS_OK;
    }
    nsresult rv =
        ComputeRangesToDeleteHRElement(aHTMLEditor, aDirectionAndAmount,
                                       *aScanFromCaretPointResult.ElementPtr(),
                                       aWSRunScannerAtCaret.ScanStartRef(),
                                       aWSRunScannerAtCaret, aRangesToDelete);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "AutoDeleteRangesHandler::ComputeRangesToDeleteHRElement() failed");
    return rv;
  }

  if (aScanFromCaretPointResult.ReachedOtherBlockElement()) {
    if (NS_WARN_IF(!aScanFromCaretPointResult.GetContent()->IsElement())) {
      return NS_ERROR_FAILURE;
    }
    AutoBlockElementsJoiner joiner(*this);
    if (!joiner.PrepareToDeleteAtOtherBlockBoundary(
            aHTMLEditor, aDirectionAndAmount,
            *aScanFromCaretPointResult.ElementPtr(),
            aWSRunScannerAtCaret.ScanStartRef(), aWSRunScannerAtCaret)) {
      return NS_SUCCESS_DOM_NO_OPERATION;
    }
    nsresult rv = joiner.ComputeRangesToDelete(
        aHTMLEditor, aDirectionAndAmount, aWSRunScannerAtCaret.ScanStartRef(),
        aRangesToDelete);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "AutoBlockElementsJoiner::ComputeRangesToDelete() "
                         "failed (other block boundary)");
    return rv;
  }

  if (aScanFromCaretPointResult.ReachedCurrentBlockBoundary()) {
    if (NS_WARN_IF(!aScanFromCaretPointResult.GetContent()->IsElement())) {
      return NS_ERROR_FAILURE;
    }
    AutoBlockElementsJoiner joiner(*this);
    if (!joiner.PrepareToDeleteAtCurrentBlockBoundary(
            aHTMLEditor, aDirectionAndAmount,
            *aScanFromCaretPointResult.ElementPtr(),
            aWSRunScannerAtCaret.ScanStartRef())) {
      return NS_SUCCESS_DOM_NO_OPERATION;
    }
    nsresult rv = joiner.ComputeRangesToDelete(
        aHTMLEditor, aDirectionAndAmount, aWSRunScannerAtCaret.ScanStartRef(),
        aRangesToDelete);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "AutoBlockElementsJoiner::ComputeRangesToDelete() "
                         "failed (current block boundary)");
    return rv;
  }

  return NS_OK;
}

EditActionResult
HTMLEditor::AutoDeleteRangesHandler::HandleDeleteAroundCollapsedRanges(
    HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
    nsIEditor::EStripWrappers aStripWrappers, AutoRangeArray& aRangesToDelete,
    const WSRunScanner& aWSRunScannerAtCaret,
    const WSScanResult& aScanFromCaretPointResult) {
  MOZ_ASSERT(aHTMLEditor.IsTopLevelEditSubActionDataAvailable());
  MOZ_ASSERT(aRangesToDelete.IsCollapsed());
  MOZ_ASSERT(aDirectionAndAmount != nsIEditor::eNone);
  MOZ_ASSERT(aWSRunScannerAtCaret.ScanStartRef().IsInContentNode());
  MOZ_ASSERT(EditorUtils::IsEditableContent(
      *aWSRunScannerAtCaret.ScanStartRef().ContainerAsContent(),
      EditorType::HTML));

  if (StaticPrefs::editor_white_space_normalization_blink_compatible()) {
    if (aScanFromCaretPointResult.InNormalWhiteSpaces() ||
        aScanFromCaretPointResult.InNormalText()) {
      nsresult rv = aRangesToDelete.Collapse(aScanFromCaretPointResult.Point());
      if (NS_FAILED(rv)) {
        NS_WARNING("AutoRangeArray::Collapse() failed");
        return EditActionResult(NS_ERROR_FAILURE);
      }
      EditActionResult result = HandleDeleteTextAroundCollapsedRanges(
          aHTMLEditor, aDirectionAndAmount, aRangesToDelete);
      NS_WARNING_ASSERTION(result.Succeeded(),
                           "AutoDeleteRangesHandler::"
                           "HandleDeleteTextAroundCollapsedRanges() failed");
      return result;
    }
  }

  if (aScanFromCaretPointResult.InNormalWhiteSpaces()) {
    EditActionResult result = HandleDeleteCollapsedSelectionAtWhiteSpaces(
        aHTMLEditor, aDirectionAndAmount, aWSRunScannerAtCaret.ScanStartRef());
    NS_WARNING_ASSERTION(result.Succeeded(),
                         "AutoDeleteRangesHandler::"
                         "HandleDelectCollapsedSelectionAtWhiteSpaces() "
                         "failed");
    return result;
  }

  if (aScanFromCaretPointResult.InNormalText()) {
    if (NS_WARN_IF(!aScanFromCaretPointResult.GetContent()->IsText())) {
      return EditActionResult(NS_ERROR_FAILURE);
    }
    EditActionResult result = HandleDeleteCollapsedSelectionAtVisibleChar(
        aHTMLEditor, aDirectionAndAmount, aScanFromCaretPointResult.Point());
    NS_WARNING_ASSERTION(result.Succeeded(),
                         "AutoDeleteRangesHandler::"
                         "HandleDeleteCollapsedSelectionAtVisibleChar() "
                         "failed");
    return result;
  }

  if (aScanFromCaretPointResult.ReachedSpecialContent() ||
      aScanFromCaretPointResult.ReachedBRElement() ||
      aScanFromCaretPointResult.ReachedNonEditableOtherBlockElement()) {
    if (aScanFromCaretPointResult.GetContent() ==
        aWSRunScannerAtCaret.GetEditingHost()) {
      return EditActionHandled();
    }
    nsCOMPtr<nsIContent> atomicContent = GetAtomicContentToDelete(
        aDirectionAndAmount, aWSRunScannerAtCaret, aScanFromCaretPointResult);
    if (!HTMLEditUtils::IsRemovableNode(*atomicContent)) {
      NS_WARNING(
          "AutoDeleteRangesHandler::GetAtomicContentToDelete() cannot find "
          "removable atomic content");
      return EditActionResult(NS_ERROR_FAILURE);
    }
    EditActionResult result = HandleDeleteAtomicContent(
        aHTMLEditor, *atomicContent, aWSRunScannerAtCaret.ScanStartRef(),
        aWSRunScannerAtCaret);
    NS_WARNING_ASSERTION(
        result.Succeeded(),
        "AutoDeleteRangesHandler::HandleDeleteAtomicContent() failed");
    return result;
  }

  if (aScanFromCaretPointResult.ReachedHRElement()) {
    if (aScanFromCaretPointResult.GetContent() ==
        aWSRunScannerAtCaret.GetEditingHost()) {
      return EditActionHandled();
    }
    EditActionResult result = HandleDeleteHRElement(
        aHTMLEditor, aDirectionAndAmount,
        MOZ_KnownLive(*aScanFromCaretPointResult.ElementPtr()),
        aWSRunScannerAtCaret.ScanStartRef(), aWSRunScannerAtCaret);
    NS_WARNING_ASSERTION(
        result.Succeeded(),
        "AutoDeleteRangesHandler::HandleDeleteHRElement() failed");
    return result;
  }

  if (aScanFromCaretPointResult.ReachedOtherBlockElement()) {
    if (NS_WARN_IF(!aScanFromCaretPointResult.GetContent()->IsElement())) {
      return EditActionResult(NS_ERROR_FAILURE);
    }
    AutoBlockElementsJoiner joiner(*this);
    if (!joiner.PrepareToDeleteAtOtherBlockBoundary(
            aHTMLEditor, aDirectionAndAmount,
            *aScanFromCaretPointResult.ElementPtr(),
            aWSRunScannerAtCaret.ScanStartRef(), aWSRunScannerAtCaret)) {
      return EditActionCanceled();
    }
    EditActionResult result =
        joiner.Run(aHTMLEditor, aDirectionAndAmount, aStripWrappers,
                   aWSRunScannerAtCaret.ScanStartRef(), aRangesToDelete);
    NS_WARNING_ASSERTION(
        result.Succeeded(),
        "AutoBlockElementsJoiner::Run() failed (other block boundary)");
    return result;
  }

  if (aScanFromCaretPointResult.ReachedCurrentBlockBoundary()) {
    if (NS_WARN_IF(!aScanFromCaretPointResult.GetContent()->IsElement())) {
      return EditActionResult(NS_ERROR_FAILURE);
    }
    AutoBlockElementsJoiner joiner(*this);
    if (!joiner.PrepareToDeleteAtCurrentBlockBoundary(
            aHTMLEditor, aDirectionAndAmount,
            *aScanFromCaretPointResult.ElementPtr(),
            aWSRunScannerAtCaret.ScanStartRef())) {
      return EditActionCanceled();
    }
    EditActionResult result =
        joiner.Run(aHTMLEditor, aDirectionAndAmount, aStripWrappers,
                   aWSRunScannerAtCaret.ScanStartRef(), aRangesToDelete);
    NS_WARNING_ASSERTION(
        result.Succeeded(),
        "AutoBlockElementsJoiner::Run() failed (current block boundary)");
    return result;
  }

  MOZ_ASSERT_UNREACHABLE("New type of reached content hasn't been handled yet");
  return EditActionIgnored();
}

nsresult HTMLEditor::AutoDeleteRangesHandler::
    ComputeRangesToDeleteTextAroundCollapsedRanges(
        Element* aEditingHost, nsIEditor::EDirection aDirectionAndAmount,
        AutoRangeArray& aRangesToDelete) const {
  MOZ_ASSERT(aDirectionAndAmount == nsIEditor::eNext ||
             aDirectionAndAmount == nsIEditor::ePrevious);

  EditorDOMPoint caretPosition(aRangesToDelete.GetStartPointOfFirstRange());
  MOZ_ASSERT(caretPosition.IsSetAndValid());
  if (NS_WARN_IF(!caretPosition.IsInContentNode())) {
    return NS_ERROR_FAILURE;
  }

  EditorDOMRangeInTexts rangeToDelete;
  if (aDirectionAndAmount == nsIEditor::eNext) {
    Result<EditorDOMRangeInTexts, nsresult> result =
        WSRunScanner::GetRangeInTextNodesToForwardDeleteFrom(aEditingHost,
                                                             caretPosition);
    if (result.isErr()) {
      NS_WARNING(
          "WSRunScanner::GetRangeInTextNodesToForwardDeleteFrom() failed");
      return result.unwrapErr();
    }
    rangeToDelete = result.unwrap();
    if (!rangeToDelete.IsPositioned()) {
      return NS_OK;  // no range to delete, but consume it.
    }
  } else {
    Result<EditorDOMRangeInTexts, nsresult> result =
        WSRunScanner::GetRangeInTextNodesToBackspaceFrom(aEditingHost,
                                                         caretPosition);
    if (result.isErr()) {
      NS_WARNING("WSRunScanner::GetRangeInTextNodesToBackspaceFrom() failed");
      return result.unwrapErr();
    }
    rangeToDelete = result.unwrap();
    if (!rangeToDelete.IsPositioned()) {
      return NS_OK;  // no range to delete, but consume it.
    }
  }

  nsresult rv = aRangesToDelete.SetStartAndEnd(rangeToDelete.StartRef(),
                                               rangeToDelete.EndRef());
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "AutoArrayRanges::SetStartAndEnd() failed");
  return rv;
}

EditActionResult
HTMLEditor::AutoDeleteRangesHandler::HandleDeleteTextAroundCollapsedRanges(
    HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
    AutoRangeArray& aRangesToDelete) {
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());
  MOZ_ASSERT(aDirectionAndAmount == nsIEditor::eNext ||
             aDirectionAndAmount == nsIEditor::ePrevious);

  RefPtr<Element> editingHost = aHTMLEditor.GetActiveEditingHost();
  if (NS_WARN_IF(!editingHost)) {
    return EditActionResult(NS_ERROR_FAILURE);
  }

  nsresult rv = ComputeRangesToDeleteTextAroundCollapsedRanges(
      editingHost, aDirectionAndAmount, aRangesToDelete);
  if (NS_FAILED(rv)) {
    return EditActionResult(NS_ERROR_FAILURE);
  }
  if (aRangesToDelete.IsCollapsed()) {
    return EditActionHandled();  // no range to delete
  }

  // FYI: rangeToDelete does not contain newly empty inline ancestors which
  //      are removed by DeleteTextAndNormalizeSurroundingWhiteSpaces().
  //      So, if `getTargetRanges()` needs to include parent empty elements,
  //      we need to extend the range with
  //      HTMLEditUtils::GetMostDistantAnscestorEditableEmptyInlineElement().
  EditorRawDOMRange rangeToDelete(aRangesToDelete.FirstRangeRef());
  if (!rangeToDelete.IsInTextNodes()) {
    NS_WARNING("The extended range to delete character was not in text nodes");
    return EditActionResult(NS_ERROR_FAILURE);
  }

  AutoTransactionsConserveSelection dontChangeMySelection(aHTMLEditor);
  Result<EditorDOMPoint, nsresult> result =
      aHTMLEditor.DeleteTextAndNormalizeSurroundingWhiteSpaces(
          rangeToDelete.StartRef().AsInText(),
          rangeToDelete.EndRef().AsInText(),
          TreatEmptyTextNodes::RemoveAllEmptyInlineAncestors,
          aDirectionAndAmount == nsIEditor::eNext ? DeleteDirection::Forward
                                                  : DeleteDirection::Backward);
  aHTMLEditor.TopLevelEditSubActionDataRef().mDidNormalizeWhitespaces = true;
  if (result.isErr()) {
    NS_WARNING(
        "HTMLEditor::DeleteTextAndNormalizeSurroundingWhiteSpaces() failed");
    return EditActionHandled(result.unwrapErr());
  }
  const EditorDOMPoint& newCaretPosition = result.inspect();
  MOZ_ASSERT(newCaretPosition.IsSetAndValid());

  DebugOnly<nsresult> rvIgnored = aHTMLEditor.SelectionRef().CollapseInLimiter(
      newCaretPosition.ToRawRangeBoundary());
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "Selection::Collapse() failed, but ignored");
  return EditActionHandled();
}

EditActionResult HTMLEditor::AutoDeleteRangesHandler::
    HandleDeleteCollapsedSelectionAtWhiteSpaces(
        HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
        const EditorDOMPoint& aPointToDelete) {
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());
  MOZ_ASSERT(!StaticPrefs::editor_white_space_normalization_blink_compatible());

  if (aDirectionAndAmount == nsIEditor::eNext) {
    nsresult rv = WhiteSpaceVisibilityKeeper::DeleteInclusiveNextWhiteSpace(
        aHTMLEditor, aPointToDelete);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::DeleteInclusiveNextWhiteSpace() failed");
      return EditActionHandled(rv);
    }
  } else {
    nsresult rv = WhiteSpaceVisibilityKeeper::DeletePreviousWhiteSpace(
        aHTMLEditor, aPointToDelete);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::DeletePreviousWhiteSpace() failed");
      return EditActionHandled(rv);
    }
  }
  EditorDOMPoint newCaretPosition =
      EditorBase::GetStartPoint(aHTMLEditor.SelectionRef());
  if (!newCaretPosition.IsSet()) {
    NS_WARNING("There was no selection range");
    return EditActionHandled(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }
  nsresult rv =
      aHTMLEditor.InsertBRElementIfHardLineIsEmptyAndEndsWithBlockBoundary(
          newCaretPosition);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::InsertBRElementIfHardLineIsEmptyAndEndsWithBlockBoundary() "
      "failed");
  return EditActionHandled(rv);
}

EditActionResult HTMLEditor::AutoDeleteRangesHandler::
    HandleDeleteCollapsedSelectionAtVisibleChar(
        HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
        const EditorDOMPoint& aPointToDelete) {
  MOZ_ASSERT(aHTMLEditor.IsTopLevelEditSubActionDataAvailable());
  MOZ_ASSERT(!StaticPrefs::editor_white_space_normalization_blink_compatible());
  MOZ_ASSERT(aPointToDelete.IsSet());
  MOZ_ASSERT(aPointToDelete.IsInTextNode());

  OwningNonNull<Text> visibleTextNode = *aPointToDelete.GetContainerAsText();
  EditorDOMPoint startToDelete, endToDelete;
  if (aDirectionAndAmount == nsIEditor::ePrevious) {
    if (aPointToDelete.IsStartOfContainer()) {
      return EditActionResult(NS_ERROR_UNEXPECTED);
    }
    startToDelete = aPointToDelete.PreviousPoint();
    endToDelete = aPointToDelete;
    // Bug 1068979: delete both codepoints if surrogate pair
    if (!startToDelete.IsStartOfContainer()) {
      const nsTextFragment* text = &visibleTextNode->TextFragment();
      if (text->IsLowSurrogateFollowingHighSurrogateAt(
              startToDelete.Offset())) {
        startToDelete.RewindOffset();
      }
    }
  } else {
    RefPtr<const nsRange> range = aHTMLEditor.SelectionRef().GetRangeAt(0);
    if (NS_WARN_IF(!range) ||
        NS_WARN_IF(range->GetStartContainer() !=
                   aPointToDelete.GetContainer()) ||
        NS_WARN_IF(range->GetEndContainer() != aPointToDelete.GetContainer())) {
      return EditActionResult(NS_ERROR_FAILURE);
    }
    startToDelete = range->StartRef();
    endToDelete = range->EndRef();
  }
  nsresult rv = WhiteSpaceVisibilityKeeper::PrepareToDeleteRangeAndTrackPoints(
      aHTMLEditor, &startToDelete, &endToDelete);
  if (NS_WARN_IF(aHTMLEditor.Destroyed())) {
    return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
  }
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "WhiteSpaceVisibilityKeeper::PrepareToDeleteRangeAndTrackPoints() "
        "failed");
    return EditActionResult(rv);
  }
  if (aHTMLEditor.MayHaveMutationEventListeners(
          NS_EVENT_BITS_MUTATION_NODEREMOVED |
          NS_EVENT_BITS_MUTATION_NODEREMOVEDFROMDOCUMENT |
          NS_EVENT_BITS_MUTATION_ATTRMODIFIED |
          NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED) &&
      (NS_WARN_IF(!startToDelete.IsSetAndValid()) ||
       NS_WARN_IF(!startToDelete.IsInTextNode()) ||
       NS_WARN_IF(!endToDelete.IsSetAndValid()) ||
       NS_WARN_IF(!endToDelete.IsInTextNode()) ||
       NS_WARN_IF(startToDelete.ContainerAsText() != visibleTextNode) ||
       NS_WARN_IF(endToDelete.ContainerAsText() != visibleTextNode) ||
       NS_WARN_IF(startToDelete.Offset() >= endToDelete.Offset()))) {
    NS_WARNING("Mutation event listener changed the DOM tree");
    return EditActionHandled(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }
  rv = aHTMLEditor.DeleteTextWithTransaction(
      visibleTextNode, startToDelete.Offset(),
      endToDelete.Offset() - startToDelete.Offset());
  if (NS_WARN_IF(aHTMLEditor.Destroyed())) {
    return EditActionHandled(NS_ERROR_EDITOR_DESTROYED);
  }
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::DeleteTextWithTransaction() failed");
    return EditActionHandled(rv);
  }

  // XXX When Backspace key is pressed, Chromium removes following empty
  //     text nodes when removing the last character of the non-empty text
  //     node.  However, Edge never removes empty text nodes even if
  //     selection is in the following empty text node(s).  For now, we
  //     should keep our traditional behavior same as Edge for backward
  //     compatibility.
  // XXX When Delete key is pressed, Edge removes all preceding empty
  //     text nodes when removing the first character of the non-empty
  //     text node.  Chromium removes only selected empty text node and
  //     following empty text nodes and the first character of the
  //     non-empty text node.  For now, we should keep our traditional
  //     behavior same as Chromium for backward compatibility.

  rv = DeleteNodeIfInvisibleAndEditableTextNode(aHTMLEditor, visibleTextNode);
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return EditActionHandled(NS_ERROR_EDITOR_DESTROYED);
  }
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "AutoDeleteRangesHandler::DeleteNodeIfInvisibleAndEditableTextNode() "
      "failed, but ignored");

  EditorDOMPoint newCaretPosition =
      EditorBase::GetStartPoint(aHTMLEditor.SelectionRef());
  if (!newCaretPosition.IsSet()) {
    NS_WARNING("There was no selection range");
    return EditActionHandled(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }

  // XXX `Selection` may be modified by mutation event listeners so
  //     that we should use EditorDOMPoint::AtEndOf(visibleTextNode)
  //     instead.  (Perhaps, we don't and/or shouldn't need to do this
  //     if the text node is preformatted.)
  rv = aHTMLEditor.InsertBRElementIfHardLineIsEmptyAndEndsWithBlockBoundary(
      newCaretPosition);
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "HTMLEditor::InsertBRElementIfHardLineIsEmptyAndEndsWithBlockBoundary()"
        " failed");
    return EditActionHandled(rv);
  }

  // Remember that we did a ranged delete for the benefit of
  // AfterEditInner().
  aHTMLEditor.TopLevelEditSubActionDataRef().mDidDeleteNonCollapsedRange = true;

  return EditActionHandled();
}

Result<bool, nsresult>
HTMLEditor::AutoDeleteRangesHandler::ShouldDeleteHRElement(
    const HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
    Element& aHRElement, const EditorDOMPoint& aCaretPoint) const {
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());

  if (StaticPrefs::editor_hr_element_allow_to_delete_from_following_line()) {
    return true;
  }

  if (aDirectionAndAmount != nsIEditor::ePrevious) {
    return true;
  }

  // Only if the caret is positioned at the end-of-hr-line position, we
  // want to delete the <hr>.
  //
  // In other words, we only want to delete, if our selection position
  // (indicated by aCaretPoint) is the position directly
  // after the <hr>, on the same line as the <hr>.
  //
  // To detect this case we check:
  // aCaretPoint's container == parent of `<hr>` element
  // and
  // aCaretPoint's offset -1 == `<hr>` element offset
  // and
  // interline position is false (left)
  //
  // In any other case we set the position to aCaretPoint's container -1
  // and interlineposition to false, only moving the caret to the
  // end-of-hr-line position.
  EditorRawDOMPoint atHRElement(&aHRElement);

  ErrorResult error;
  bool interLineIsRight =
      aHTMLEditor.SelectionRef().GetInterlinePosition(error);
  if (error.Failed()) {
    NS_WARNING("Selection::GetInterlinePosition() failed");
    nsresult rv = error.StealNSResult();
    return Err(rv);
  }

  return !interLineIsRight &&
         aCaretPoint.GetContainer() == atHRElement.GetContainer() &&
         aCaretPoint.Offset() - 1 == atHRElement.Offset();
}

nsresult HTMLEditor::AutoDeleteRangesHandler::ComputeRangesToDeleteHRElement(
    const HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
    Element& aHRElement, const EditorDOMPoint& aCaretPoint,
    const WSRunScanner& aWSRunScannerAtCaret,
    AutoRangeArray& aRangesToDelete) const {
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());
  MOZ_ASSERT(aHRElement.IsHTMLElement(nsGkAtoms::hr));
  MOZ_ASSERT(&aHRElement != aWSRunScannerAtCaret.GetEditingHost());

  Result<bool, nsresult> canDeleteHRElement = ShouldDeleteHRElement(
      aHTMLEditor, aDirectionAndAmount, aHRElement, aCaretPoint);
  if (canDeleteHRElement.isErr()) {
    NS_WARNING("AutoDeleteRangesHandler::ShouldDeleteHRElement() failed");
    return canDeleteHRElement.unwrapErr();
  }
  if (canDeleteHRElement.inspect()) {
    nsresult rv = ComputeRangesToDeleteAtomicContent(
        aWSRunScannerAtCaret.GetEditingHost(), aHRElement, aRangesToDelete);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "AutoDeleteRangesHandler::ComputeRangesToDeleteAtomicContent() failed");
    return rv;
  }

  WSScanResult forwardScanFromCaretResult =
      aWSRunScannerAtCaret.ScanNextVisibleNodeOrBlockBoundaryFrom(aCaretPoint);
  if (forwardScanFromCaretResult.Failed()) {
    NS_WARNING("WSRunScanner::ScanNextVisibleNodeOrBlockBoundaryFrom() failed");
    return NS_ERROR_FAILURE;
  }
  if (!forwardScanFromCaretResult.ReachedBRElement()) {
    // Restore original caret position if we won't delete anyting.
    nsresult rv = aRangesToDelete.Collapse(aCaretPoint);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "AutoRangeArray::Collapse() failed");
    return rv;
  }

  // If we'll just move caret position, but if it's followed by a `<br>`
  // element, we'll delete it.
  nsresult rv = ComputeRangesToDeleteAtomicContent(
      aWSRunScannerAtCaret.GetEditingHost(),
      *forwardScanFromCaretResult.ElementPtr(), aRangesToDelete);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "AutoDeleteRangesHandler::ComputeRangesToDeleteAtomicContent() failed");
  return rv;
}

EditActionResult HTMLEditor::AutoDeleteRangesHandler::HandleDeleteHRElement(
    HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
    Element& aHRElement, const EditorDOMPoint& aCaretPoint,
    const WSRunScanner& aWSRunScannerAtCaret) {
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());
  MOZ_ASSERT(aHRElement.IsHTMLElement(nsGkAtoms::hr));
  MOZ_ASSERT(&aHRElement != aWSRunScannerAtCaret.GetEditingHost());

  Result<bool, nsresult> canDeleteHRElement = ShouldDeleteHRElement(
      aHTMLEditor, aDirectionAndAmount, aHRElement, aCaretPoint);
  if (canDeleteHRElement.isErr()) {
    NS_WARNING("AutoDeleteRangesHandler::ShouldDeleteHRElement() failed");
    return EditActionHandled(canDeleteHRElement.unwrapErr());
  }
  if (canDeleteHRElement.inspect()) {
    EditActionResult result = HandleDeleteAtomicContent(
        aHTMLEditor, aHRElement, aCaretPoint, aWSRunScannerAtCaret);
    NS_WARNING_ASSERTION(
        result.Succeeded(),
        "AutoDeleteRangesHandler::HandleDeleteAtomicContent() failed");
    return result;
  }

  // Go to the position after the <hr>, but to the end of the <hr> line
  // by setting the interline position to left.
  EditorDOMPoint atNextOfHRElement(EditorDOMPoint::After(aHRElement));
  NS_WARNING_ASSERTION(atNextOfHRElement.IsSet(),
                       "Failed to set after <hr> element");

  {
    AutoEditorDOMPointChildInvalidator lockOffset(atNextOfHRElement);

    nsresult rv = aHTMLEditor.CollapseSelectionTo(atNextOfHRElement);
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::CollapseSelectionTo() failed, but ignored");
  }

  IgnoredErrorResult ignoredError;
  aHTMLEditor.SelectionRef().SetInterlinePosition(false, ignoredError);
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "Selection::SetInterlinePosition(false) failed, but ignored");
  aHTMLEditor.TopLevelEditSubActionDataRef().mDidExplicitlySetInterLine = true;

  // There is one exception to the move only case.  If the <hr> is
  // followed by a <br> we want to delete the <br>.

  WSScanResult forwardScanFromCaretResult =
      aWSRunScannerAtCaret.ScanNextVisibleNodeOrBlockBoundaryFrom(aCaretPoint);
  if (forwardScanFromCaretResult.Failed()) {
    NS_WARNING("WSRunScanner::ScanNextVisibleNodeOrBlockBoundaryFrom() failed");
    return EditActionResult(NS_ERROR_FAILURE);
  }
  if (!forwardScanFromCaretResult.ReachedBRElement()) {
    return EditActionHandled();
  }

  // Delete the <br>
  nsresult rv =
      WhiteSpaceVisibilityKeeper::DeleteContentNodeAndJoinTextNodesAroundIt(
          aHTMLEditor,
          MOZ_KnownLive(*forwardScanFromCaretResult.BRElementPtr()),
          aCaretPoint);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "WhiteSpaceVisibilityKeeper::"
                       "DeleteContentNodeAndJoinTextNodesAroundIt() failed");
  return EditActionHandled(rv);
}

// static
nsIContent* HTMLEditor::AutoDeleteRangesHandler::GetAtomicContentToDelete(
    nsIEditor::EDirection aDirectionAndAmount,
    const WSRunScanner& aWSRunScannerAtCaret,
    const WSScanResult& aScanFromCaretPointResult) {
  MOZ_ASSERT(aScanFromCaretPointResult.GetContent());

  if (!aScanFromCaretPointResult.ReachedSpecialContent()) {
    return aScanFromCaretPointResult.GetContent();
  }

  if (!aScanFromCaretPointResult.GetContent()->IsText() ||
      HTMLEditUtils::IsRemovableNode(*aScanFromCaretPointResult.GetContent())) {
    return aScanFromCaretPointResult.GetContent();
  }

  // aScanFromCaretPointResult is non-removable text node.
  // Since we try removing atomic content, we look for removable node from
  // scanned point that is non-removable text.
  nsIContent* removableRoot = aScanFromCaretPointResult.GetContent();
  while (removableRoot && !HTMLEditUtils::IsRemovableNode(*removableRoot)) {
    removableRoot = removableRoot->GetParent();
  }

  if (removableRoot) {
    return removableRoot;
  }

  // Not found better content. This content may not be removable.
  return aScanFromCaretPointResult.GetContent();
}

nsresult
HTMLEditor::AutoDeleteRangesHandler::ComputeRangesToDeleteAtomicContent(
    Element* aEditingHost, const nsIContent& aAtomicContent,
    AutoRangeArray& aRangesToDelete) const {
  EditorDOMRange rangeToDelete =
      WSRunScanner::GetRangesForDeletingAtomicContent(aEditingHost,
                                                      aAtomicContent);
  if (!rangeToDelete.IsPositioned()) {
    NS_WARNING("WSRunScanner::GetRangeForDeleteAContentNode() failed");
    return NS_ERROR_FAILURE;
  }
  nsresult rv = aRangesToDelete.SetStartAndEnd(rangeToDelete.StartRef(),
                                               rangeToDelete.EndRef());
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "AutoRangeArray::SetStartAndEnd() failed");
  return rv;
}

EditActionResult HTMLEditor::AutoDeleteRangesHandler::HandleDeleteAtomicContent(
    HTMLEditor& aHTMLEditor, nsIContent& aAtomicContent,
    const EditorDOMPoint& aCaretPoint,
    const WSRunScanner& aWSRunScannerAtCaret) {
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());
  MOZ_ASSERT(!HTMLEditUtils::IsInvisibleBRElement(
      aAtomicContent, aWSRunScannerAtCaret.GetEditingHost()));
  MOZ_ASSERT(&aAtomicContent != aWSRunScannerAtCaret.GetEditingHost());

  nsresult rv =
      WhiteSpaceVisibilityKeeper::DeleteContentNodeAndJoinTextNodesAroundIt(
          aHTMLEditor, aAtomicContent, aCaretPoint);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    NS_WARNING(
        "WhiteSpaceVisibilityKeeper::DeleteContentNodeAndJoinTextNodesAroundIt("
        ") failed");
    return EditActionHandled(rv);
  }

  EditorDOMPoint newCaretPosition =
      EditorBase::GetStartPoint(aHTMLEditor.SelectionRef());
  if (!newCaretPosition.IsSet()) {
    NS_WARNING("There was no selection range");
    return EditActionHandled(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }

  rv = aHTMLEditor.InsertBRElementIfHardLineIsEmptyAndEndsWithBlockBoundary(
      newCaretPosition);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::InsertBRElementIfHardLineIsEmptyAndEndsWithBlockBoundary() "
      "failed");
  return EditActionHandled(rv);
}

bool HTMLEditor::AutoDeleteRangesHandler::AutoBlockElementsJoiner::
    PrepareToDeleteAtOtherBlockBoundary(
        const HTMLEditor& aHTMLEditor,
        nsIEditor::EDirection aDirectionAndAmount, Element& aOtherBlockElement,
        const EditorDOMPoint& aCaretPoint,
        const WSRunScanner& aWSRunScannerAtCaret) {
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());
  MOZ_ASSERT(aCaretPoint.IsSetAndValid());

  mMode = Mode::JoinOtherBlock;

  // Make sure it's not a table element.  If so, cancel the operation
  // (translation: users cannot backspace or delete across table cells)
  if (HTMLEditUtils::IsAnyTableElement(&aOtherBlockElement)) {
    return false;
  }

  // First find the adjacent node in the block
  if (aDirectionAndAmount == nsIEditor::ePrevious) {
    mLeafContentInOtherBlock = HTMLEditUtils::GetLastLeafContent(
        aOtherBlockElement, {LeafNodeType::OnlyEditableLeafNode},
        &aOtherBlockElement);
    mLeftContent = mLeafContentInOtherBlock;
    mRightContent = aCaretPoint.GetContainerAsContent();
  } else {
    mLeafContentInOtherBlock = HTMLEditUtils::GetFirstLeafContent(
        aOtherBlockElement, {LeafNodeType::OnlyEditableLeafNode},
        &aOtherBlockElement);
    mLeftContent = aCaretPoint.GetContainerAsContent();
    mRightContent = mLeafContentInOtherBlock;
  }

  // Next to a block.  See if we are between the block and a `<br>`.
  // If so, we really want to delete the `<br>`.  Else join content at
  // selection to the block.
  WSScanResult scanFromCaretResult =
      aDirectionAndAmount == nsIEditor::eNext
          ? aWSRunScannerAtCaret.ScanPreviousVisibleNodeOrBlockBoundaryFrom(
                aCaretPoint)
          : aWSRunScannerAtCaret.ScanNextVisibleNodeOrBlockBoundaryFrom(
                aCaretPoint);
  // If we found a `<br>` element, we need to delete it instead of joining the
  // contents.
  if (scanFromCaretResult.ReachedBRElement()) {
    mBRElement = scanFromCaretResult.BRElementPtr();
    mMode = Mode::DeleteBRElement;
    return true;
  }

  return mLeftContent && mRightContent;
}

nsresult HTMLEditor::AutoDeleteRangesHandler::AutoBlockElementsJoiner::
    ComputeRangesToDeleteBRElement(AutoRangeArray& aRangesToDelete) const {
  MOZ_ASSERT(mBRElement);
  // XXX Why don't we scan invisible leading white-spaces which follows the
  //     `<br>` element?
  nsresult rv = aRangesToDelete.SelectNode(*mBRElement);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "AutoRangeArray::SelectNode() failed");
  return rv;
}

EditActionResult
HTMLEditor::AutoDeleteRangesHandler::AutoBlockElementsJoiner::DeleteBRElement(
    HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
    const EditorDOMPoint& aCaretPoint) {
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());
  MOZ_ASSERT(aCaretPoint.IsSetAndValid());
  MOZ_ASSERT(mBRElement);

  // If we found a `<br>` element, we should delete it instead of joining the
  // contents.
  nsresult rv =
      aHTMLEditor.DeleteNodeWithTransaction(MOZ_KnownLive(*mBRElement));
  if (NS_WARN_IF(aHTMLEditor.Destroyed())) {
    return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
  }
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
    return EditActionResult(rv);
  }

  if (mLeftContent && mRightContent &&
      HTMLEditUtils::GetInclusiveAncestorAnyTableElement(*mLeftContent) !=
          HTMLEditUtils::GetInclusiveAncestorAnyTableElement(*mRightContent)) {
    return EditActionHandled();
  }

  // Put selection at edge of block and we are done.
  if (NS_WARN_IF(!mLeafContentInOtherBlock)) {
    // XXX This must be odd case.  The other block can be empty.
    return EditActionHandled(NS_ERROR_FAILURE);
  }
  EditorRawDOMPoint newCaretPosition =
      HTMLEditUtils::GetGoodCaretPointFor<EditorRawDOMPoint>(
          *mLeafContentInOtherBlock, aDirectionAndAmount);
  if (!newCaretPosition.IsSet()) {
    NS_WARNING("HTMLEditUtils::GetGoodCaretPointFor() failed");
    return EditActionHandled(NS_ERROR_FAILURE);
  }
  rv = aHTMLEditor.CollapseSelectionTo(newCaretPosition);
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return EditActionHandled(NS_ERROR_EDITOR_DESTROYED);
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::CollapseSelectionTo() failed, but ignored");
  return EditActionHandled();
}

nsresult HTMLEditor::AutoDeleteRangesHandler::AutoBlockElementsJoiner::
    ComputeRangesToDeleteAtOtherBlockBoundary(
        const HTMLEditor& aHTMLEditor,
        nsIEditor::EDirection aDirectionAndAmount,
        const EditorDOMPoint& aCaretPoint,
        AutoRangeArray& aRangesToDelete) const {
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());
  MOZ_ASSERT(aCaretPoint.IsSetAndValid());
  MOZ_ASSERT(mLeftContent);
  MOZ_ASSERT(mRightContent);

  if (HTMLEditUtils::GetInclusiveAncestorAnyTableElement(*mLeftContent) !=
      HTMLEditUtils::GetInclusiveAncestorAnyTableElement(*mRightContent)) {
    if (!mDeleteRangesHandlerConst.CanFallbackToDeleteRangesWithTransaction(
            aRangesToDelete)) {
      nsresult rv = aRangesToDelete.Collapse(aCaretPoint);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "AutoRangeArray::Collapse() failed");
      return rv;
    }
    nsresult rv = mDeleteRangesHandlerConst
                      .FallbackToComputeRangesToDeleteRangesWithTransaction(
                          aHTMLEditor, aRangesToDelete);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "AutoDeleteRangesHandler::"
        "FallbackToComputeRangesToDeleteRangesWithTransaction() failed");
    return rv;
  }

  AutoInclusiveAncestorBlockElementsJoiner joiner(*mLeftContent,
                                                  *mRightContent);
  Result<bool, nsresult> canJoinThem = joiner.Prepare(aHTMLEditor);
  if (canJoinThem.isErr()) {
    NS_WARNING("AutoInclusiveAncestorBlockElementsJoiner::Prepare() failed");
    return canJoinThem.unwrapErr();
  }
  if (canJoinThem.inspect() && joiner.CanJoinBlocks() &&
      !joiner.ShouldDeleteLeafContentInstead()) {
    nsresult rv =
        joiner.ComputeRangesToDelete(aHTMLEditor, aCaretPoint, aRangesToDelete);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "AutoInclusiveAncestorBlockElementsJoiner::ComputeRangesToDelete() "
        "failed");
    return rv;
  }

  // If AutoInclusiveAncestorBlockElementsJoiner didn't handle it and it's not
  // canceled, user may want to modify the start leaf node or the last leaf
  // node of the block.
  if (mLeafContentInOtherBlock == aCaretPoint.GetContainer()) {
    return NS_OK;
  }

  AutoHideSelectionChanges hideSelectionChanges(aHTMLEditor.SelectionRef());

  // If it's ignored, it didn't modify the DOM tree.  In this case, user must
  // want to delete nearest leaf node in the other block element.
  // TODO: We need to consider this before calling ComputeRangesToDelete() for
  //       computing the deleting range.
  EditorRawDOMPoint newCaretPoint =
      aDirectionAndAmount == nsIEditor::ePrevious
          ? EditorRawDOMPoint::AtEndOf(*mLeafContentInOtherBlock)
          : EditorRawDOMPoint(mLeafContentInOtherBlock, 0);
  // If new caret position is same as current caret position, we can do
  // nothing anymore.
  if (aRangesToDelete.IsCollapsed() &&
      aRangesToDelete.FocusRef() == newCaretPoint.ToRawRangeBoundary()) {
    return NS_OK;
  }
  // TODO: Stop modifying the `Selection` for computing the targer ranges.
  nsresult rv = aHTMLEditor.CollapseSelectionTo(newCaretPoint);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::CollapseSelectionTo() failed");
  if (NS_SUCCEEDED(rv)) {
    aRangesToDelete.Initialize(aHTMLEditor.SelectionRef());
    AutoDeleteRangesHandler anotherHandler(mDeleteRangesHandlerConst);
    rv = anotherHandler.ComputeRangesToDelete(aHTMLEditor, aDirectionAndAmount,
                                              aRangesToDelete);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "Recursive AutoDeleteRangesHandler::ComputeRangesToDelete() failed");
  }
  // Restore selection.
  nsresult rvCollapsingSelectionTo =
      aHTMLEditor.CollapseSelectionTo(aCaretPoint);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvCollapsingSelectionTo),
      "HTMLEditor::CollapseSelectionTo() failed to restore caret position");
  return NS_SUCCEEDED(rv) && NS_SUCCEEDED(rvCollapsingSelectionTo)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

EditActionResult HTMLEditor::AutoDeleteRangesHandler::AutoBlockElementsJoiner::
    HandleDeleteAtOtherBlockBoundary(HTMLEditor& aHTMLEditor,
                                     nsIEditor::EDirection aDirectionAndAmount,
                                     nsIEditor::EStripWrappers aStripWrappers,
                                     const EditorDOMPoint& aCaretPoint,
                                     AutoRangeArray& aRangesToDelete) {
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());
  MOZ_ASSERT(aCaretPoint.IsSetAndValid());
  MOZ_ASSERT(mDeleteRangesHandler);
  MOZ_ASSERT(mLeftContent);
  MOZ_ASSERT(mRightContent);

  if (HTMLEditUtils::GetInclusiveAncestorAnyTableElement(*mLeftContent) !=
      HTMLEditUtils::GetInclusiveAncestorAnyTableElement(*mRightContent)) {
    // If we have not deleted `<br>` element and are not called recursively,
    // we should call `DeleteRangesWithTransaction()` here.
    if (!mDeleteRangesHandler->CanFallbackToDeleteRangesWithTransaction(
            aRangesToDelete)) {
      return EditActionIgnored();
    }
    EditActionResult result =
        mDeleteRangesHandler->FallbackToDeleteRangesWithTransaction(
            aHTMLEditor, aRangesToDelete);
    NS_WARNING_ASSERTION(
        result.Succeeded(),
        "AutoDeleteRangesHandler::FallbackToDeleteRangesWithTransaction() "
        "failed to delete leaf content in the block");
    return result;
  }

  // Else we are joining content to block
  AutoInclusiveAncestorBlockElementsJoiner joiner(*mLeftContent,
                                                  *mRightContent);
  Result<bool, nsresult> canJoinThem = joiner.Prepare(aHTMLEditor);
  if (canJoinThem.isErr()) {
    NS_WARNING("AutoInclusiveAncestorBlockElementsJoiner::Prepare() failed");
    return EditActionResult(canJoinThem.unwrapErr());
  }

  if (!canJoinThem.inspect()) {
    nsresult rv = aHTMLEditor.CollapseSelectionTo(aCaretPoint);
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::CollapseSelectionTo() failed, but ignored");
    return EditActionCanceled();
  }

  EditActionResult result(NS_OK);
  EditorDOMPoint pointToPutCaret(aCaretPoint);
  if (joiner.CanJoinBlocks()) {
    {
      AutoTrackDOMPoint tracker(aHTMLEditor.RangeUpdaterRef(),
                                &pointToPutCaret);
      result |= joiner.Run(aHTMLEditor);
      if (result.Failed()) {
        NS_WARNING("AutoInclusiveAncestorBlockElementsJoiner::Run() failed");
        return result;
      }
#ifdef DEBUG
      if (joiner.ShouldDeleteLeafContentInstead()) {
        NS_ASSERTION(
            result.Ignored(),
            "Assumed `AutoInclusiveAncestorBlockElementsJoiner::Run()` "
            "retruning ignored, but returned not ignored");
      } else {
        NS_ASSERTION(
            !result.Ignored(),
            "Assumed `AutoInclusiveAncestorBlockElementsJoiner::Run()` "
            "retruning handled, but returned ignored");
      }
#endif  // #ifdef DEBUG
    }

    // If AutoInclusiveAncestorBlockElementsJoiner didn't handle it and it's not
    // canceled, user may want to modify the start leaf node or the last leaf
    // node of the block.
    if (result.Ignored() &&
        mLeafContentInOtherBlock != aCaretPoint.GetContainer()) {
      // If it's ignored, it didn't modify the DOM tree.  In this case, user
      // must want to delete nearest leaf node in the other block element.
      // TODO: We need to consider this before calling Run() for computing the
      //       deleting range.
      EditorRawDOMPoint newCaretPoint =
          aDirectionAndAmount == nsIEditor::ePrevious
              ? EditorRawDOMPoint::AtEndOf(*mLeafContentInOtherBlock)
              : EditorRawDOMPoint(mLeafContentInOtherBlock, 0);
      // If new caret position is same as current caret position, we can do
      // nothing anymore.
      if (aRangesToDelete.IsCollapsed() &&
          aRangesToDelete.FocusRef() == newCaretPoint.ToRawRangeBoundary()) {
        return EditActionCanceled();
      }
      nsresult rv = aHTMLEditor.CollapseSelectionTo(newCaretPoint);
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::CollapseSelectionTo() failed");
        return result.SetResult(rv);
      }
      AutoRangeArray rangesToDelete(aHTMLEditor.SelectionRef());
      AutoDeleteRangesHandler anotherHandler(mDeleteRangesHandler);
      result = anotherHandler.Run(aHTMLEditor, aDirectionAndAmount,
                                  aStripWrappers, rangesToDelete);
      NS_WARNING_ASSERTION(result.Succeeded(),
                           "Recursive AutoDeleteRangesHandler::Run() failed");
      return result;
    }
  } else {
    result.MarkAsHandled();
  }

  // Otherwise, we must have deleted the selection as user expected.
  nsresult rv = aHTMLEditor.CollapseSelectionTo(pointToPutCaret);
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return result.SetResult(NS_ERROR_EDITOR_DESTROYED);
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::CollapseSelectionTo() failed, but ignored");
  return result;
}

bool HTMLEditor::AutoDeleteRangesHandler::AutoBlockElementsJoiner::
    PrepareToDeleteAtCurrentBlockBoundary(
        const HTMLEditor& aHTMLEditor,
        nsIEditor::EDirection aDirectionAndAmount,
        Element& aCurrentBlockElement, const EditorDOMPoint& aCaretPoint) {
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());

  // At edge of our block.  Look beside it and see if we can join to an
  // adjacent block
  mMode = Mode::JoinCurrentBlock;

  // Don't break the basic structure of the HTML document.
  if (aCurrentBlockElement.IsAnyOfHTMLElements(nsGkAtoms::html, nsGkAtoms::head,
                                               nsGkAtoms::body)) {
    return false;
  }

  // Make sure it's not a table element.  If so, cancel the operation
  // (translation: users cannot backspace or delete across table cells)
  if (HTMLEditUtils::IsAnyTableElement(&aCurrentBlockElement)) {
    return false;
  }

  Element* editingHost = aHTMLEditor.GetActiveEditingHost();
  if (NS_WARN_IF(!editingHost)) {
    return false;
  }

  if (aDirectionAndAmount == nsIEditor::ePrevious) {
    mLeftContent = HTMLEditUtils::GetPreviousContent(
        aCurrentBlockElement, {WalkTreeOption::IgnoreNonEditableNode},
        editingHost);
    mRightContent = aCaretPoint.GetContainerAsContent();
  } else {
    mRightContent = HTMLEditUtils::GetNextContent(
        aCurrentBlockElement, {WalkTreeOption::IgnoreNonEditableNode},
        editingHost);
    mLeftContent = aCaretPoint.GetContainerAsContent();
  }

  // Nothing to join
  if (!mLeftContent || !mRightContent) {
    return false;
  }

  // Don't cross table boundaries.
  return HTMLEditUtils::GetInclusiveAncestorAnyTableElement(*mLeftContent) ==
         HTMLEditUtils::GetInclusiveAncestorAnyTableElement(*mRightContent);
}

nsresult HTMLEditor::AutoDeleteRangesHandler::AutoBlockElementsJoiner::
    ComputeRangesToDeleteAtCurrentBlockBoundary(
        const HTMLEditor& aHTMLEditor, const EditorDOMPoint& aCaretPoint,
        AutoRangeArray& aRangesToDelete) const {
  MOZ_ASSERT(mLeftContent);
  MOZ_ASSERT(mRightContent);

  AutoInclusiveAncestorBlockElementsJoiner joiner(*mLeftContent,
                                                  *mRightContent);
  Result<bool, nsresult> canJoinThem = joiner.Prepare(aHTMLEditor);
  if (canJoinThem.isErr()) {
    NS_WARNING("AutoInclusiveAncestorBlockElementsJoiner::Prepare() failed");
    return canJoinThem.unwrapErr();
  }
  if (canJoinThem.inspect()) {
    nsresult rv =
        joiner.ComputeRangesToDelete(aHTMLEditor, aCaretPoint, aRangesToDelete);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "AutoInclusiveAncestorBlockElementsJoiner::"
                         "ComputeRangesToDelete() failed");
    return rv;
  }

  // In this case, nothing will be deleted so that the affected range should
  // be collapsed.
  nsresult rv = aRangesToDelete.Collapse(aCaretPoint);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "AutoRangeArray::Collapse() failed");
  return rv;
}

EditActionResult HTMLEditor::AutoDeleteRangesHandler::AutoBlockElementsJoiner::
    HandleDeleteAtCurrentBlockBoundary(HTMLEditor& aHTMLEditor,
                                       const EditorDOMPoint& aCaretPoint) {
  MOZ_ASSERT(mLeftContent);
  MOZ_ASSERT(mRightContent);

  AutoInclusiveAncestorBlockElementsJoiner joiner(*mLeftContent,
                                                  *mRightContent);
  Result<bool, nsresult> canJoinThem = joiner.Prepare(aHTMLEditor);
  if (canJoinThem.isErr()) {
    NS_WARNING("AutoInclusiveAncestorBlockElementsJoiner::Prepare() failed");
    return EditActionResult(canJoinThem.unwrapErr());
  }

  if (!canJoinThem.inspect()) {
    nsresult rv = aHTMLEditor.CollapseSelectionTo(aCaretPoint);
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::CollapseSelectionTo() failed, but ignored");
    return EditActionCanceled();
  }

  EditActionResult result(NS_OK);
  EditorDOMPoint pointToPutCaret(aCaretPoint);
  if (joiner.CanJoinBlocks()) {
    AutoTrackDOMPoint tracker(aHTMLEditor.RangeUpdaterRef(), &pointToPutCaret);
    result |= joiner.Run(aHTMLEditor);
    if (result.Failed()) {
      NS_WARNING("AutoInclusiveAncestorBlockElementsJoiner::Run() failed");
      return result;
    }
#ifdef DEBUG
    if (joiner.ShouldDeleteLeafContentInstead()) {
      NS_ASSERTION(result.Ignored(),
                   "Assumed `AutoInclusiveAncestorBlockElementsJoiner::Run()` "
                   "retruning ignored, but returned not ignored");
    } else {
      NS_ASSERTION(!result.Ignored(),
                   "Assumed `AutoInclusiveAncestorBlockElementsJoiner::Run()` "
                   "retruning handled, but returned ignored");
    }
#endif  // #ifdef DEBUG
  }
  // This should claim that trying to join the block means that
  // this handles the action because the caller shouldn't do anything
  // anymore in this case.
  result.MarkAsHandled();

  nsresult rv = aHTMLEditor.CollapseSelectionTo(pointToPutCaret);
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return result.SetResult(NS_ERROR_EDITOR_DESTROYED);
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::CollapseSelectionTo() failed, but ignored");
  return result;
}

nsresult
HTMLEditor::AutoDeleteRangesHandler::ComputeRangesToDeleteNonCollapsedRanges(
    const HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
    AutoRangeArray& aRangesToDelete,
    AutoDeleteRangesHandler::SelectionWasCollapsed aSelectionWasCollapsed)
    const {
  MOZ_ASSERT(!aRangesToDelete.IsCollapsed());

  if (NS_WARN_IF(!aRangesToDelete.FirstRangeRef()->StartRef().IsSet()) ||
      NS_WARN_IF(!aRangesToDelete.FirstRangeRef()->EndRef().IsSet())) {
    return NS_ERROR_FAILURE;
  }

  if (aRangesToDelete.Ranges().Length() == 1) {
    nsFrameSelection* frameSelection =
        aHTMLEditor.SelectionRef().GetFrameSelection();
    if (NS_WARN_IF(!frameSelection)) {
      return NS_ERROR_FAILURE;
    }
    if (!ExtendRangeToIncludeInvisibleNodes(aHTMLEditor, frameSelection,
                                            aRangesToDelete.FirstRangeRef())) {
      NS_WARNING(
          "AutoDeleteRangesHandler::ExtendRangeToIncludeInvisibleNodes() "
          "failed");
      return NS_ERROR_FAILURE;
    }
  }

  if (!aHTMLEditor.IsInPlaintextMode()) {
    EditorDOMRange firstRange(aRangesToDelete.FirstRangeRef());
    EditorDOMRange extendedRange =
        WSRunScanner::GetRangeContainingInvisibleWhiteSpacesAtRangeBoundaries(
            aHTMLEditor.GetActiveEditingHost(),
            EditorDOMRange(aRangesToDelete.FirstRangeRef()));
    if (firstRange != extendedRange) {
      nsresult rv = aRangesToDelete.FirstRangeRef()->SetStartAndEnd(
          extendedRange.StartRef().ToRawRangeBoundary(),
          extendedRange.EndRef().ToRawRangeBoundary());
      if (NS_FAILED(rv)) {
        NS_WARNING("nsRange::SetStartAndEnd() failed");
        return NS_ERROR_FAILURE;
      }
    }
  }

  if (aRangesToDelete.FirstRangeRef()->GetStartContainer() ==
      aRangesToDelete.FirstRangeRef()->GetEndContainer()) {
    if (!aRangesToDelete.FirstRangeRef()->Collapsed()) {
      nsresult rv = ComputeRangesToDeleteRangesWithTransaction(
          aHTMLEditor, aDirectionAndAmount, aRangesToDelete);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "AutoDeleteRangesHandler::ComputeRangesToDeleteRangesWithTransaction("
          ") failed");
      return rv;
    }
    // `DeleteUnnecessaryNodesAndCollapseSelection()` may delete parent
    // elements, but it does not affect computing target ranges.  Therefore,
    // we don't need to touch aRangesToDelete in this case.
    return NS_OK;
  }

  Element* startCiteNode = aHTMLEditor.GetMostAncestorMailCiteElement(
      *aRangesToDelete.FirstRangeRef()->GetStartContainer());
  Element* endCiteNode = aHTMLEditor.GetMostAncestorMailCiteElement(
      *aRangesToDelete.FirstRangeRef()->GetEndContainer());

  if (startCiteNode && !endCiteNode) {
    aDirectionAndAmount = nsIEditor::eNext;
  } else if (!startCiteNode && endCiteNode) {
    aDirectionAndAmount = nsIEditor::ePrevious;
  }

  AutoBlockElementsJoiner joiner(*this);
  if (!joiner.PrepareToDeleteNonCollapsedRanges(aHTMLEditor, aRangesToDelete)) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv =
      joiner.ComputeRangesToDelete(aHTMLEditor, aDirectionAndAmount,
                                   aRangesToDelete, aSelectionWasCollapsed);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "AutoBlockElementsJoiner::ComputeRangesToDelete() failed");
  return rv;
}

EditActionResult
HTMLEditor::AutoDeleteRangesHandler::HandleDeleteNonCollapsedRanges(
    HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
    nsIEditor::EStripWrappers aStripWrappers, AutoRangeArray& aRangesToDelete,
    SelectionWasCollapsed aSelectionWasCollapsed) {
  MOZ_ASSERT(aHTMLEditor.IsTopLevelEditSubActionDataAvailable());
  MOZ_ASSERT(!aRangesToDelete.IsCollapsed());

  if (NS_WARN_IF(!aRangesToDelete.FirstRangeRef()->StartRef().IsSet()) ||
      NS_WARN_IF(!aRangesToDelete.FirstRangeRef()->EndRef().IsSet())) {
    return EditActionResult(NS_ERROR_FAILURE);
  }

  // Else we have a non-collapsed selection.  First adjust the selection.
  // XXX Why do we extend selection only when there is only one range?
  if (aRangesToDelete.Ranges().Length() == 1) {
    nsFrameSelection* frameSelection =
        aHTMLEditor.SelectionRef().GetFrameSelection();
    if (NS_WARN_IF(!frameSelection)) {
      return EditActionResult(NS_ERROR_FAILURE);
    }
    if (!ExtendRangeToIncludeInvisibleNodes(aHTMLEditor, frameSelection,
                                            aRangesToDelete.FirstRangeRef())) {
      NS_WARNING(
          "AutoDeleteRangesHandler::ExtendRangeToIncludeInvisibleNodes() "
          "failed");
      return EditActionResult(NS_ERROR_FAILURE);
    }
  }

  // Remember that we did a ranged delete for the benefit of AfterEditInner().
  aHTMLEditor.TopLevelEditSubActionDataRef().mDidDeleteNonCollapsedRange = true;

  // Figure out if the endpoints are in nodes that can be merged.  Adjust
  // surrounding white-space in preparation to delete selection.
  if (!aHTMLEditor.IsInPlaintextMode()) {
    AutoTransactionsConserveSelection dontChangeMySelection(aHTMLEditor);
    AutoTrackDOMRange firstRangeTracker(aHTMLEditor.RangeUpdaterRef(),
                                        &aRangesToDelete.FirstRangeRef());
    nsresult rv = WhiteSpaceVisibilityKeeper::PrepareToDeleteRange(
        aHTMLEditor, EditorDOMRange(aRangesToDelete.FirstRangeRef()));
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::PrepareToDeleteRange() "
          "failed");
      return EditActionResult(rv);
    }
    if (NS_WARN_IF(
            !aRangesToDelete.FirstRangeRef()->StartRef().IsSetAndValid()) ||
        NS_WARN_IF(
            !aRangesToDelete.FirstRangeRef()->EndRef().IsSetAndValid())) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::PrepareToDeleteRange() made the firstr "
          "range invalid");
      return EditActionHandled(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
    }
  }

  // XXX This is odd.  We do we simply use `DeleteRangesWithTransaction()`
  //     only when **first** range is in same container?
  if (aRangesToDelete.FirstRangeRef()->GetStartContainer() ==
      aRangesToDelete.FirstRangeRef()->GetEndContainer()) {
    // Because of previous DOM tree changes, the range may be collapsed.
    // If we've already removed all contents in the range, we shouldn't
    // delete anything around the caret.
    if (!aRangesToDelete.FirstRangeRef()->Collapsed()) {
      AutoTrackDOMRange firstRangeTracker(aHTMLEditor.RangeUpdaterRef(),
                                          &aRangesToDelete.FirstRangeRef());
      nsresult rv = aHTMLEditor.DeleteRangesWithTransaction(
          aDirectionAndAmount, aStripWrappers, aRangesToDelete);
      if (NS_FAILED(rv)) {
        NS_WARNING("EditorBase::DeleteRangesWithTransaction() failed");
        return EditActionHandled(rv);
      }
    }
    // However, even if the range is removed, we may need to clean up the
    // containers which become empty.
    nsresult rv = DeleteUnnecessaryNodesAndCollapseSelection(
        aHTMLEditor, aDirectionAndAmount,
        EditorDOMPoint(aRangesToDelete.FirstRangeRef()->StartRef()),
        EditorDOMPoint(aRangesToDelete.FirstRangeRef()->EndRef()));
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "AutoDeleteRangesHandler::"
                         "DeleteUnnecessaryNodesAndCollapseSelection() failed");
    return EditActionHandled(rv);
  }

  if (NS_WARN_IF(
          !aRangesToDelete.FirstRangeRef()->GetStartContainer()->IsContent()) ||
      NS_WARN_IF(
          !aRangesToDelete.FirstRangeRef()->GetEndContainer()->IsContent())) {
    return EditActionHandled(NS_ERROR_FAILURE);  // XXX "handled"?
  }

  // Figure out mailcite ancestors
  RefPtr<Element> startCiteNode = aHTMLEditor.GetMostAncestorMailCiteElement(
      *aRangesToDelete.FirstRangeRef()->GetStartContainer());
  RefPtr<Element> endCiteNode = aHTMLEditor.GetMostAncestorMailCiteElement(
      *aRangesToDelete.FirstRangeRef()->GetEndContainer());

  // If we only have a mailcite at one of the two endpoints, set the
  // directionality of the deletion so that the selection will end up
  // outside the mailcite.
  if (startCiteNode && !endCiteNode) {
    aDirectionAndAmount = nsIEditor::eNext;
  } else if (!startCiteNode && endCiteNode) {
    aDirectionAndAmount = nsIEditor::ePrevious;
  }

  AutoBlockElementsJoiner joiner(*this);
  if (!joiner.PrepareToDeleteNonCollapsedRanges(aHTMLEditor, aRangesToDelete)) {
    return EditActionHandled(NS_ERROR_FAILURE);
  }
  EditActionResult result =
      joiner.Run(aHTMLEditor, aDirectionAndAmount, aStripWrappers,
                 aRangesToDelete, aSelectionWasCollapsed);
  NS_WARNING_ASSERTION(result.Succeeded(),
                       "AutoBlockElementsJoiner::Run() failed");
  return result;
}

bool HTMLEditor::AutoDeleteRangesHandler::AutoBlockElementsJoiner::
    PrepareToDeleteNonCollapsedRanges(const HTMLEditor& aHTMLEditor,
                                      const AutoRangeArray& aRangesToDelete) {
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());
  MOZ_ASSERT(!aRangesToDelete.IsCollapsed());

  mLeftContent = HTMLEditUtils::GetInclusiveAncestorElement(
      *aRangesToDelete.FirstRangeRef()->GetStartContainer()->AsContent(),
      HTMLEditUtils::ClosestEditableBlockElement);
  mRightContent = HTMLEditUtils::GetInclusiveAncestorElement(
      *aRangesToDelete.FirstRangeRef()->GetEndContainer()->AsContent(),
      HTMLEditUtils::ClosestEditableBlockElement);
  if (NS_WARN_IF(!mLeftContent) || NS_WARN_IF(!mRightContent)) {
    return false;
  }
  if (mLeftContent == mRightContent) {
    mMode = Mode::DeleteContentInRanges;
    return true;
  }
  NS_ASSERTION(
      mLeftContent->GetEditingHost() == mRightContent->GetEditingHost(),
      "Trying to delete across editing host boundaries");

  // If left block and right block are adjuscent siblings and they are same
  // type of elements, we can merge them after deleting the selected contents.
  // MOOSE: this could conceivably screw up a table.. fix me.
  if (mLeftContent->GetParentNode() == mRightContent->GetParentNode() &&
      HTMLEditUtils::CanContentsBeJoined(
          *mLeftContent, *mRightContent,
          aHTMLEditor.IsCSSEnabled() ? StyleDifference::CompareIfSpanElements
                                     : StyleDifference::Ignore) &&
      // XXX What's special about these three types of block?
      (mLeftContent->IsHTMLElement(nsGkAtoms::p) ||
       HTMLEditUtils::IsListItem(mLeftContent) ||
       HTMLEditUtils::IsHeader(*mLeftContent))) {
    mMode = Mode::JoinBlocksInSameParent;
    return true;
  }

  mMode = Mode::DeleteNonCollapsedRanges;
  return true;
}

nsresult HTMLEditor::AutoDeleteRangesHandler::AutoBlockElementsJoiner::
    ComputeRangesToDeleteContentInRanges(
        const HTMLEditor& aHTMLEditor,
        nsIEditor::EDirection aDirectionAndAmount,
        AutoRangeArray& aRangesToDelete) const {
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());
  MOZ_ASSERT(!aRangesToDelete.IsCollapsed());
  MOZ_ASSERT(mMode == Mode::DeleteContentInRanges);
  MOZ_ASSERT(mLeftContent);
  MOZ_ASSERT(mLeftContent->IsElement());
  MOZ_ASSERT(aRangesToDelete.FirstRangeRef()
                 ->GetStartContainer()
                 ->IsInclusiveDescendantOf(mLeftContent));
  MOZ_ASSERT(mRightContent);
  MOZ_ASSERT(mRightContent->IsElement());
  MOZ_ASSERT(aRangesToDelete.FirstRangeRef()
                 ->GetEndContainer()
                 ->IsInclusiveDescendantOf(mRightContent));

  nsresult rv =
      mDeleteRangesHandlerConst.ComputeRangesToDeleteRangesWithTransaction(
          aHTMLEditor, aDirectionAndAmount, aRangesToDelete);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "AutoDeleteRangesHandler::"
                       "ComputeRangesToDeleteRangesWithTransaction() failed");
  return rv;
}

EditActionResult HTMLEditor::AutoDeleteRangesHandler::AutoBlockElementsJoiner::
    DeleteContentInRanges(HTMLEditor& aHTMLEditor,
                          nsIEditor::EDirection aDirectionAndAmount,
                          nsIEditor::EStripWrappers aStripWrappers,
                          AutoRangeArray& aRangesToDelete) {
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());
  MOZ_ASSERT(!aRangesToDelete.IsCollapsed());
  MOZ_ASSERT(mMode == Mode::DeleteContentInRanges);
  MOZ_ASSERT(mDeleteRangesHandler);
  MOZ_ASSERT(mLeftContent);
  MOZ_ASSERT(mLeftContent->IsElement());
  MOZ_ASSERT(aRangesToDelete.FirstRangeRef()
                 ->GetStartContainer()
                 ->IsInclusiveDescendantOf(mLeftContent));
  MOZ_ASSERT(mRightContent);
  MOZ_ASSERT(mRightContent->IsElement());
  MOZ_ASSERT(aRangesToDelete.FirstRangeRef()
                 ->GetEndContainer()
                 ->IsInclusiveDescendantOf(mRightContent));

  // XXX This is also odd.  We do we simply use
  //     `DeleteRangesWithTransaction()` only when **first** range is in
  //     same block?
  MOZ_ASSERT(mLeftContent == mRightContent);
  {
    AutoTrackDOMRange firstRangeTracker(aHTMLEditor.RangeUpdaterRef(),
                                        &aRangesToDelete.FirstRangeRef());
    nsresult rv = aHTMLEditor.DeleteRangesWithTransaction(
        aDirectionAndAmount, aStripWrappers, aRangesToDelete);
    if (rv == NS_ERROR_EDITOR_DESTROYED) {
      NS_WARNING(
          "EditorBase::DeleteRangesWithTransaction() caused destroying the "
          "editor");
      return EditActionHandled(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "EditorBase::DeleteRangesWithTransaction() failed, but ignored");
  }
  nsresult rv =
      mDeleteRangesHandler->DeleteUnnecessaryNodesAndCollapseSelection(
          aHTMLEditor, aDirectionAndAmount,
          EditorDOMPoint(aRangesToDelete.FirstRangeRef()->StartRef()),
          EditorDOMPoint(aRangesToDelete.FirstRangeRef()->EndRef()));
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "AutoDeleteRangesHandler::"
                       "DeleteUnnecessaryNodesAndCollapseSelection() failed");
  return EditActionHandled(rv);
}

nsresult HTMLEditor::AutoDeleteRangesHandler::AutoBlockElementsJoiner::
    ComputeRangesToJoinBlockElementsInSameParent(
        const HTMLEditor& aHTMLEditor,
        nsIEditor::EDirection aDirectionAndAmount,
        AutoRangeArray& aRangesToDelete) const {
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());
  MOZ_ASSERT(!aRangesToDelete.IsCollapsed());
  MOZ_ASSERT(mMode == Mode::JoinBlocksInSameParent);
  MOZ_ASSERT(mLeftContent);
  MOZ_ASSERT(mLeftContent->IsElement());
  MOZ_ASSERT(aRangesToDelete.FirstRangeRef()
                 ->GetStartContainer()
                 ->IsInclusiveDescendantOf(mLeftContent));
  MOZ_ASSERT(mRightContent);
  MOZ_ASSERT(mRightContent->IsElement());
  MOZ_ASSERT(aRangesToDelete.FirstRangeRef()
                 ->GetEndContainer()
                 ->IsInclusiveDescendantOf(mRightContent));
  MOZ_ASSERT(mLeftContent->GetParentNode() == mRightContent->GetParentNode());

  nsresult rv =
      mDeleteRangesHandlerConst.ComputeRangesToDeleteRangesWithTransaction(
          aHTMLEditor, aDirectionAndAmount, aRangesToDelete);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "AutoDeleteRangesHandler::"
                       "ComputeRangesToDeleteRangesWithTransaction() failed");
  return rv;
}

EditActionResult HTMLEditor::AutoDeleteRangesHandler::AutoBlockElementsJoiner::
    JoinBlockElementsInSameParent(HTMLEditor& aHTMLEditor,
                                  nsIEditor::EDirection aDirectionAndAmount,
                                  nsIEditor::EStripWrappers aStripWrappers,
                                  AutoRangeArray& aRangesToDelete) {
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());
  MOZ_ASSERT(!aRangesToDelete.IsCollapsed());
  MOZ_ASSERT(mMode == Mode::JoinBlocksInSameParent);
  MOZ_ASSERT(mLeftContent);
  MOZ_ASSERT(mLeftContent->IsElement());
  MOZ_ASSERT(aRangesToDelete.FirstRangeRef()
                 ->GetStartContainer()
                 ->IsInclusiveDescendantOf(mLeftContent));
  MOZ_ASSERT(mRightContent);
  MOZ_ASSERT(mRightContent->IsElement());
  MOZ_ASSERT(aRangesToDelete.FirstRangeRef()
                 ->GetEndContainer()
                 ->IsInclusiveDescendantOf(mRightContent));
  MOZ_ASSERT(mLeftContent->GetParentNode() == mRightContent->GetParentNode());

  nsresult rv = aHTMLEditor.DeleteRangesWithTransaction(
      aDirectionAndAmount, aStripWrappers, aRangesToDelete);
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::DeleteRangesWithTransaction() failed");
    return EditActionHandled(rv);
  }

  Result<EditorDOMPoint, nsresult> atFirstChildOfTheLastRightNodeOrError =
      JoinNodesDeepWithTransaction(aHTMLEditor, MOZ_KnownLive(*mLeftContent),
                                   MOZ_KnownLive(*mRightContent));
  if (atFirstChildOfTheLastRightNodeOrError.isErr()) {
    NS_WARNING("HTMLEditor::JoinNodesDeepWithTransaction() failed");
    return EditActionHandled(atFirstChildOfTheLastRightNodeOrError.unwrapErr());
  }
  MOZ_ASSERT(atFirstChildOfTheLastRightNodeOrError.inspect().IsSet());

  rv = aHTMLEditor.CollapseSelectionTo(
      atFirstChildOfTheLastRightNodeOrError.inspect());
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::CollapseSelectionTo() failed");
  return EditActionHandled(rv);
}

Result<bool, nsresult>
HTMLEditor::AutoDeleteRangesHandler::AutoBlockElementsJoiner::
    ComputeRangesToDeleteNodesEntirelyInRangeButKeepTableStructure(
        const HTMLEditor& aHTMLEditor, nsRange& aRange,
        AutoDeleteRangesHandler::SelectionWasCollapsed aSelectionWasCollapsed)
        const {
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());

  AutoTArray<OwningNonNull<nsIContent>, 10> arrayOfTopChildren;
  DOMSubtreeIterator iter;
  nsresult rv = iter.Init(aRange);
  if (NS_FAILED(rv)) {
    NS_WARNING("DOMSubtreeIterator::Init() failed");
    return Err(rv);
  }
  iter.AppendAllNodesToArray(arrayOfTopChildren);
  return NeedsToJoinNodesAfterDeleteNodesEntirelyInRangeButKeepTableStructure(
      aHTMLEditor, arrayOfTopChildren, aSelectionWasCollapsed);
}

Result<bool, nsresult> HTMLEditor::AutoDeleteRangesHandler::
    AutoBlockElementsJoiner::DeleteNodesEntirelyInRangeButKeepTableStructure(
        HTMLEditor& aHTMLEditor, nsRange& aRange,
        AutoDeleteRangesHandler::SelectionWasCollapsed aSelectionWasCollapsed) {
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());

  // Build a list of direct child nodes in the range
  AutoTArray<OwningNonNull<nsIContent>, 10> arrayOfTopChildren;
  DOMSubtreeIterator iter;
  nsresult rv = iter.Init(aRange);
  if (NS_FAILED(rv)) {
    NS_WARNING("DOMSubtreeIterator::Init() failed");
    return Err(rv);
  }
  iter.AppendAllNodesToArray(arrayOfTopChildren);

  // Now that we have the list, delete non-table elements
  bool needsToJoinLater =
      NeedsToJoinNodesAfterDeleteNodesEntirelyInRangeButKeepTableStructure(
          aHTMLEditor, arrayOfTopChildren, aSelectionWasCollapsed);
  for (auto& content : arrayOfTopChildren) {
    // XXX After here, the child contents in the array may have been moved
    //     to somewhere or removed.  We should handle it.
    //
    // MOZ_KnownLive because 'arrayOfTopChildren' is guaranteed to
    // keep it alive.
    //
    // Even with https://bugzilla.mozilla.org/show_bug.cgi?id=1620312 fixed
    // this might need to stay, because 'arrayOfTopChildren' is not const,
    // so it's not obvious how to prove via static analysis that it won't
    // change and release us.
    nsresult rv =
        DeleteContentButKeepTableStructure(aHTMLEditor, MOZ_KnownLive(content));
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return Err(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "AutoBlockElementsJoiner::DeleteContentButKeepTableStructure() failed, "
        "but ignored");
  }
  return needsToJoinLater;
}

bool HTMLEditor::AutoDeleteRangesHandler::AutoBlockElementsJoiner::
    NeedsToJoinNodesAfterDeleteNodesEntirelyInRangeButKeepTableStructure(
        const HTMLEditor& aHTMLEditor,
        const nsTArray<OwningNonNull<nsIContent>>& aArrayOfContents,
        AutoDeleteRangesHandler::SelectionWasCollapsed aSelectionWasCollapsed)
        const {
  // If original selection was collapsed, we need always to join the nodes.
  // XXX Why?
  if (aSelectionWasCollapsed ==
      AutoDeleteRangesHandler::SelectionWasCollapsed::No) {
    return true;
  }
  // If something visible is deleted, no need to join.  Visible means
  // all nodes except non-visible textnodes and breaks.
  if (aArrayOfContents.IsEmpty()) {
    return true;
  }
  for (const OwningNonNull<nsIContent>& content : aArrayOfContents) {
    if (content->IsText()) {
      if (HTMLEditUtils::IsInVisibleTextFrames(aHTMLEditor.GetPresContext(),
                                               *content->AsText())) {
        return false;
      }
      continue;
    }
    // XXX If it's an element node, we should check whether it has visible
    //     frames or not.
    if (!content->IsElement() ||
        HTMLEditUtils::IsEmptyNode(
            *content->AsElement(),
            {EmptyCheckOption::TreatSingleBRElementAsVisible})) {
      continue;
    }
    if (!HTMLEditUtils::IsInvisibleBRElement(*content)) {
      return false;
    }
  }
  return true;
}

nsresult HTMLEditor::AutoDeleteRangesHandler::AutoBlockElementsJoiner::
    DeleteTextAtStartAndEndOfRange(HTMLEditor& aHTMLEditor, nsRange& aRange) {
  EditorDOMPoint rangeStart(aRange.StartRef());
  EditorDOMPoint rangeEnd(aRange.EndRef());
  if (rangeStart.IsInTextNode() && !rangeStart.IsEndOfContainer()) {
    // Delete to last character
    OwningNonNull<Text> textNode = *rangeStart.GetContainerAsText();
    nsresult rv = aHTMLEditor.DeleteTextWithTransaction(
        textNode, rangeStart.Offset(),
        rangeStart.GetContainer()->Length() - rangeStart.Offset());
    if (NS_WARN_IF(aHTMLEditor.Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::DeleteTextWithTransaction() failed");
      return rv;
    }
  }
  if (rangeEnd.IsInTextNode() && !rangeEnd.IsStartOfContainer()) {
    // Delete to first character
    OwningNonNull<Text> textNode = *rangeEnd.GetContainerAsText();
    nsresult rv =
        aHTMLEditor.DeleteTextWithTransaction(textNode, 0, rangeEnd.Offset());
    if (NS_WARN_IF(aHTMLEditor.Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::DeleteTextWithTransaction() failed");
      return rv;
    }
  }
  return NS_OK;
}

nsresult HTMLEditor::AutoDeleteRangesHandler::AutoBlockElementsJoiner::
    ComputeRangesToDeleteNonCollapsedRanges(
        const HTMLEditor& aHTMLEditor,
        nsIEditor::EDirection aDirectionAndAmount,
        AutoRangeArray& aRangesToDelete,
        AutoDeleteRangesHandler::SelectionWasCollapsed aSelectionWasCollapsed)
        const {
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());
  MOZ_ASSERT(!aRangesToDelete.IsCollapsed());
  MOZ_ASSERT(mLeftContent);
  MOZ_ASSERT(mLeftContent->IsElement());
  MOZ_ASSERT(aRangesToDelete.FirstRangeRef()
                 ->GetStartContainer()
                 ->IsInclusiveDescendantOf(mLeftContent));
  MOZ_ASSERT(mRightContent);
  MOZ_ASSERT(mRightContent->IsElement());
  MOZ_ASSERT(aRangesToDelete.FirstRangeRef()
                 ->GetEndContainer()
                 ->IsInclusiveDescendantOf(mRightContent));

  for (OwningNonNull<nsRange>& range : aRangesToDelete.Ranges()) {
    Result<bool, nsresult> result =
        ComputeRangesToDeleteNodesEntirelyInRangeButKeepTableStructure(
            aHTMLEditor, range, aSelectionWasCollapsed);
    if (result.isErr()) {
      NS_WARNING(
          "AutoBlockElementsJoiner::"
          "ComputeRangesToDeleteNodesEntirelyInRangeButKeepTableStructure() "
          "failed");
      return result.unwrapErr();
    }
    if (!result.unwrap()) {
      return NS_OK;
    }
  }

  AutoInclusiveAncestorBlockElementsJoiner joiner(*mLeftContent,
                                                  *mRightContent);
  Result<bool, nsresult> canJoinThem = joiner.Prepare(aHTMLEditor);
  if (canJoinThem.isErr()) {
    NS_WARNING("AutoInclusiveAncestorBlockElementsJoiner::Prepare() failed");
    return canJoinThem.unwrapErr();
  }

  if (!canJoinThem.unwrap()) {
    return NS_SUCCESS_DOM_NO_OPERATION;
  }

  if (!joiner.CanJoinBlocks()) {
    return NS_OK;
  }

  nsresult rv = joiner.ComputeRangesToDelete(aHTMLEditor, EditorDOMPoint(),
                                             aRangesToDelete);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "AutoInclusiveAncestorBlockElementsJoiner::ComputeRangesToDelete() "
      "failed");
  return rv;
}

EditActionResult HTMLEditor::AutoDeleteRangesHandler::AutoBlockElementsJoiner::
    HandleDeleteNonCollapsedRanges(
        HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
        nsIEditor::EStripWrappers aStripWrappers,
        AutoRangeArray& aRangesToDelete,
        AutoDeleteRangesHandler::SelectionWasCollapsed aSelectionWasCollapsed) {
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());
  MOZ_ASSERT(!aRangesToDelete.IsCollapsed());
  MOZ_ASSERT(mDeleteRangesHandler);
  MOZ_ASSERT(mLeftContent);
  MOZ_ASSERT(mLeftContent->IsElement());
  MOZ_ASSERT(aRangesToDelete.FirstRangeRef()
                 ->GetStartContainer()
                 ->IsInclusiveDescendantOf(mLeftContent));
  MOZ_ASSERT(mRightContent);
  MOZ_ASSERT(mRightContent->IsElement());
  MOZ_ASSERT(aRangesToDelete.FirstRangeRef()
                 ->GetEndContainer()
                 ->IsInclusiveDescendantOf(mRightContent));

  // Otherwise, delete every nodes in all ranges, then, clean up something.
  EditActionResult result(NS_OK);
  while (true) {
    AutoTrackDOMRange firstRangeTracker(aHTMLEditor.RangeUpdaterRef(),
                                        &aRangesToDelete.FirstRangeRef());

    bool joinInclusiveAncestorBlockElements = true;
    for (auto& range : aRangesToDelete.Ranges()) {
      Result<bool, nsresult> deleteResult =
          DeleteNodesEntirelyInRangeButKeepTableStructure(
              aHTMLEditor, MOZ_KnownLive(range), aSelectionWasCollapsed);
      if (deleteResult.isErr()) {
        NS_WARNING(
            "AutoBlockElementsJoiner::"
            "DeleteNodesEntirelyInRangeButKeepTableStructure() failed");
        return EditActionResult(deleteResult.unwrapErr());
      }
      // XXX Completely odd.  Why don't we join blocks around each range?
      joinInclusiveAncestorBlockElements &= deleteResult.unwrap();
    }

    // Check endpoints for possible text deletion.  We can assume that if
    // text node is found, we can delete to end or to begining as
    // appropriate, since the case where both sel endpoints in same text
    // node was already handled (we wouldn't be here)
    nsresult rv = DeleteTextAtStartAndEndOfRange(
        aHTMLEditor, MOZ_KnownLive(aRangesToDelete.FirstRangeRef()));
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "AutoBlockElementsJoiner::DeleteTextAtStartAndEndOfRange() failed");
      return EditActionHandled(rv);
    }

    if (!joinInclusiveAncestorBlockElements) {
      break;
    }

    AutoInclusiveAncestorBlockElementsJoiner joiner(*mLeftContent,
                                                    *mRightContent);
    Result<bool, nsresult> canJoinThem = joiner.Prepare(aHTMLEditor);
    if (canJoinThem.isErr()) {
      NS_WARNING("AutoInclusiveAncestorBlockElementsJoiner::Prepare() failed");
      return EditActionResult(canJoinThem.unwrapErr());
    }

    // If we're joining blocks: if deleting forward the selection should
    // be collapsed to the end of the selection, if deleting backward the
    // selection should be collapsed to the beginning of the selection.
    // But if we're not joining then the selection should collapse to the
    // beginning of the selection if we'redeleting forward, because the
    // end of the selection will still be in the next block. And same
    // thing for deleting backwards (selection should collapse to the end,
    // because the beginning will still be in the first block). See Bug
    // 507936.
    if (aDirectionAndAmount == nsIEditor::eNext) {
      aDirectionAndAmount = nsIEditor::ePrevious;
    } else {
      aDirectionAndAmount = nsIEditor::eNext;
    }

    if (!canJoinThem.inspect()) {
      result.MarkAsCanceled();
      break;
    }

    if (!joiner.CanJoinBlocks()) {
      break;
    }

    result |= joiner.Run(aHTMLEditor);
    if (result.Failed()) {
      NS_WARNING("AutoInclusiveAncestorBlockElementsJoiner::Run() failed");
      return result;
    }
#ifdef DEBUG
    if (joiner.ShouldDeleteLeafContentInstead()) {
      NS_ASSERTION(result.Ignored(),
                   "Assumed `AutoInclusiveAncestorBlockElementsJoiner::Run()` "
                   "retruning ignored, but returned not ignored");
    } else {
      NS_ASSERTION(!result.Ignored(),
                   "Assumed `AutoInclusiveAncestorBlockElementsJoiner::Run()` "
                   "retruning handled, but returned ignored");
    }
#endif  // #ifdef DEBUG
    break;
  }

  nsresult rv =
      mDeleteRangesHandler->DeleteUnnecessaryNodesAndCollapseSelection(
          aHTMLEditor, aDirectionAndAmount,
          EditorDOMPoint(aRangesToDelete.FirstRangeRef()->StartRef()),
          EditorDOMPoint(aRangesToDelete.FirstRangeRef()->EndRef()));
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "AutoDeleteRangesHandler::DeleteUnnecessaryNodesAndCollapseSelection() "
        "failed");
    return EditActionResult(rv);
  }

  return result.MarkAsHandled();
}

nsresult
HTMLEditor::AutoDeleteRangesHandler::DeleteUnnecessaryNodesAndCollapseSelection(
    HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
    const EditorDOMPoint& aSelectionStartPoint,
    const EditorDOMPoint& aSelectionEndPoint) {
  MOZ_ASSERT(aHTMLEditor.IsTopLevelEditSubActionDataAvailable());
  MOZ_ASSERT(EditorUtils::IsEditableContent(
      *aSelectionStartPoint.ContainerAsContent(), EditorType::HTML));
  MOZ_ASSERT(EditorUtils::IsEditableContent(
      *aSelectionEndPoint.ContainerAsContent(), EditorType::HTML));

  EditorDOMPoint atCaret(aSelectionStartPoint);
  EditorDOMPoint selectionEndPoint(aSelectionEndPoint);

  // If we're handling D&D, this is called to delete dragging item from the
  // tree.  In this case, we should remove parent blocks if it becomes empty.
  if (aHTMLEditor.GetEditAction() == EditAction::eDrop ||
      aHTMLEditor.GetEditAction() == EditAction::eDeleteByDrag) {
    MOZ_ASSERT((atCaret.GetContainer() == selectionEndPoint.GetContainer() &&
                atCaret.Offset() == selectionEndPoint.Offset()) ||
               (atCaret.GetContainer()->GetNextSibling() ==
                    selectionEndPoint.GetContainer() &&
                atCaret.IsEndOfContainer() &&
                selectionEndPoint.IsStartOfContainer()));
    {
      AutoTrackDOMPoint startTracker(aHTMLEditor.RangeUpdaterRef(), &atCaret);
      AutoTrackDOMPoint endTracker(aHTMLEditor.RangeUpdaterRef(),
                                   &selectionEndPoint);

      nsresult rv =
          DeleteParentBlocksWithTransactionIfEmpty(aHTMLEditor, atCaret);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "HTMLEditor::DeleteParentBlocksWithTransactionIfEmpty() failed");
        return rv;
      }
      aHTMLEditor.TopLevelEditSubActionDataRef().mDidDeleteEmptyParentBlocks =
          rv == NS_OK;
    }
    // If we removed parent blocks, Selection should be collapsed at where
    // the most ancestor empty block has been.
    if (aHTMLEditor.TopLevelEditSubActionDataRef()
            .mDidDeleteEmptyParentBlocks) {
      nsresult rv = aHTMLEditor.CollapseSelectionTo(atCaret);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "HTMLEditor::CollapseSelectionTo() failed");
      return rv;
    }
  }

  if (NS_WARN_IF(!atCaret.IsInContentNode()) ||
      NS_WARN_IF(!selectionEndPoint.IsInContentNode()) ||
      NS_WARN_IF(!EditorUtils::IsEditableContent(*atCaret.ContainerAsContent(),
                                                 EditorType::HTML)) ||
      NS_WARN_IF(!EditorUtils::IsEditableContent(
          *selectionEndPoint.ContainerAsContent(), EditorType::HTML))) {
    return NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE;
  }

  // We might have left only collapsed white-space in the start/end nodes
  {
    AutoTrackDOMPoint startTracker(aHTMLEditor.RangeUpdaterRef(), &atCaret);
    AutoTrackDOMPoint endTracker(aHTMLEditor.RangeUpdaterRef(),
                                 &selectionEndPoint);

    nsresult rv = DeleteNodeIfInvisibleAndEditableTextNode(
        aHTMLEditor, MOZ_KnownLive(*atCaret.ContainerAsContent()));
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "AutoDeleteRangesHandler::DeleteNodeIfInvisibleAndEditableTextNode() "
        "failed to remove start node, but ignored");
    // If we've not handled the selection end container, and it's still
    // editable, let's handle it.
    if (atCaret.ContainerAsContent() !=
            selectionEndPoint.ContainerAsContent() &&
        EditorUtils::IsEditableContent(*selectionEndPoint.ContainerAsContent(),
                                       EditorType::HTML)) {
      nsresult rv = DeleteNodeIfInvisibleAndEditableTextNode(
          aHTMLEditor, MOZ_KnownLive(*selectionEndPoint.ContainerAsContent()));
      if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "AutoDeleteRangesHandler::DeleteNodeIfInvisibleAndEditableTextNode() "
          "failed to remove end node, but ignored");
    }
  }

  nsresult rv = aHTMLEditor.CollapseSelectionTo(
      aDirectionAndAmount == nsIEditor::ePrevious ? selectionEndPoint
                                                  : atCaret);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::CollapseSelectionTo() failed");
  return rv;
}

nsresult
HTMLEditor::AutoDeleteRangesHandler::DeleteNodeIfInvisibleAndEditableTextNode(
    HTMLEditor& aHTMLEditor, nsIContent& aContent) {
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());

  Text* text = aContent.GetAsText();
  if (!text) {
    return NS_OK;
  }

  if (!HTMLEditUtils::IsRemovableFromParentNode(*text) ||
      HTMLEditUtils::IsVisibleTextNode(*text,
                                       aHTMLEditor.GetActiveEditingHost())) {
    return NS_OK;
  }

  nsresult rv = aHTMLEditor.DeleteNodeWithTransaction(aContent);
  if (NS_WARN_IF(aHTMLEditor.Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::DeleteNodeWithTransaction() failed");
  return rv;
}

nsresult
HTMLEditor::AutoDeleteRangesHandler::DeleteParentBlocksWithTransactionIfEmpty(
    HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPoint) {
  MOZ_ASSERT(aPoint.IsSet());
  MOZ_ASSERT(aHTMLEditor.mPlaceholderBatch);

  // First, check there is visible contents before the point in current block.
  RefPtr<Element> editingHost = aHTMLEditor.GetActiveEditingHost();
  WSRunScanner wsScannerForPoint(editingHost, aPoint);
  if (!wsScannerForPoint.StartsFromCurrentBlockBoundary()) {
    // If there is visible node before the point, we shouldn't remove the
    // parent block.
    return NS_SUCCESS_EDITOR_ELEMENT_NOT_FOUND;
  }
  if (NS_WARN_IF(!wsScannerForPoint.GetStartReasonContent()) ||
      NS_WARN_IF(!wsScannerForPoint.GetStartReasonContent()->GetParentNode())) {
    return NS_ERROR_FAILURE;
  }
  if (editingHost == wsScannerForPoint.GetStartReasonContent()) {
    // If we reach editing host, there is no parent blocks which can be removed.
    return NS_SUCCESS_EDITOR_ELEMENT_NOT_FOUND;
  }
  if (HTMLEditUtils::IsTableCellOrCaption(
          *wsScannerForPoint.GetStartReasonContent())) {
    // If we reach a <td>, <th> or <caption>, we shouldn't remove it even
    // becomes empty because removing such element changes the structure of
    // the <table>.
    return NS_SUCCESS_EDITOR_ELEMENT_NOT_FOUND;
  }

  // Next, check there is visible contents after the point in current block.
  WSScanResult forwardScanFromPointResult =
      wsScannerForPoint.ScanNextVisibleNodeOrBlockBoundaryFrom(aPoint);
  if (forwardScanFromPointResult.Failed()) {
    NS_WARNING("WSRunScanner::ScanNextVisibleNodeOrBlockBoundaryFrom() failed");
    return NS_ERROR_FAILURE;
  }
  if (forwardScanFromPointResult.ReachedBRElement()) {
    // XXX In my understanding, this is odd.  The end reason may not be
    //     same as the reached <br> element because the equality is
    //     guaranteed only when ReachedCurrentBlockBoundary() returns true.
    //     However, looks like that this code assumes that
    //     GetEndReasonContent() returns the (or a) <br> element.
    NS_ASSERTION(wsScannerForPoint.GetEndReasonContent() ==
                     forwardScanFromPointResult.BRElementPtr(),
                 "End reason is not the reached <br> element");
    // If the <br> element is visible, we shouldn't remove the parent block.
    if (HTMLEditUtils::IsVisibleBRElement(
            *wsScannerForPoint.GetEndReasonContent(), editingHost)) {
      return NS_SUCCESS_EDITOR_ELEMENT_NOT_FOUND;
    }
    if (wsScannerForPoint.GetEndReasonContent()->GetNextSibling()) {
      WSScanResult scanResult =
          WSRunScanner::ScanNextVisibleNodeOrBlockBoundary(
              editingHost, EditorRawDOMPoint::After(
                               *wsScannerForPoint.GetEndReasonContent()));
      if (scanResult.Failed()) {
        NS_WARNING("WSRunScanner::ScanNextVisibleNodeOrBlockBoundary() failed");
        return NS_ERROR_FAILURE;
      }
      if (!scanResult.ReachedCurrentBlockBoundary()) {
        // If we couldn't reach the block's end after the invisible <br>,
        // that means that there is visible content.
        return NS_SUCCESS_EDITOR_ELEMENT_NOT_FOUND;
      }
    }
  } else if (!forwardScanFromPointResult.ReachedCurrentBlockBoundary()) {
    // If we couldn't reach the block's end, the block has visible content.
    return NS_SUCCESS_EDITOR_ELEMENT_NOT_FOUND;
  }

  // Delete the parent block.
  EditorDOMPoint nextPoint(
      wsScannerForPoint.GetStartReasonContent()->GetParentNode(), 0);
  nsresult rv = aHTMLEditor.DeleteNodeWithTransaction(
      MOZ_KnownLive(*wsScannerForPoint.GetStartReasonContent()));
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
    return rv;
  }
  // If we reach editing host, return NS_OK.
  if (nextPoint.GetContainer() == editingHost) {
    return NS_OK;
  }

  // Otherwise, we need to check whether we're still in empty block or not.

  // If we have mutation event listeners, the next point is now outside of
  // editing host or editing hos has been changed.
  if (aHTMLEditor.MayHaveMutationEventListeners(
          NS_EVENT_BITS_MUTATION_NODEREMOVED |
          NS_EVENT_BITS_MUTATION_NODEREMOVEDFROMDOCUMENT |
          NS_EVENT_BITS_MUTATION_SUBTREEMODIFIED)) {
    Element* newEditingHost = aHTMLEditor.GetActiveEditingHost();
    if (NS_WARN_IF(!newEditingHost) ||
        NS_WARN_IF(newEditingHost != editingHost)) {
      return NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE;
    }
    if (NS_WARN_IF(!EditorUtils::IsDescendantOf(*nextPoint.GetContainer(),
                                                *newEditingHost))) {
      return NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE;
    }
  }

  rv = DeleteParentBlocksWithTransactionIfEmpty(aHTMLEditor, nextPoint);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "AutoDeleteRangesHandler::"
                       "DeleteParentBlocksWithTransactionIfEmpty() failed");
  return rv;
}

nsresult
HTMLEditor::AutoDeleteRangesHandler::ComputeRangesToDeleteRangesWithTransaction(
    const HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
    AutoRangeArray& aRangesToDelete) const {
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());
  MOZ_ASSERT(!aRangesToDelete.Ranges().IsEmpty());

  EditorBase::HowToHandleCollapsedRange howToHandleCollapsedRange =
      EditorBase::HowToHandleCollapsedRangeFor(aDirectionAndAmount);
  if (NS_WARN_IF(aRangesToDelete.IsCollapsed() &&
                 howToHandleCollapsedRange ==
                     EditorBase::HowToHandleCollapsedRange::Ignore)) {
    return NS_ERROR_FAILURE;
  }

  auto extendRangeToSelectCharacterForward =
      [](nsRange& aRange, const EditorRawDOMPointInText& aCaretPoint) -> void {
    const nsTextFragment& textFragment =
        aCaretPoint.ContainerAsText()->TextFragment();
    if (!textFragment.GetLength()) {
      return;
    }
    if (textFragment.IsHighSurrogateFollowedByLowSurrogateAt(
            aCaretPoint.Offset())) {
      DebugOnly<nsresult> rvIgnored = aRange.SetStartAndEnd(
          aCaretPoint.ContainerAsText(), aCaretPoint.Offset(),
          aCaretPoint.ContainerAsText(), aCaretPoint.Offset() + 2);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                           "nsRange::SetStartAndEnd() failed");
      return;
    }
    DebugOnly<nsresult> rvIgnored = aRange.SetStartAndEnd(
        aCaretPoint.ContainerAsText(), aCaretPoint.Offset(),
        aCaretPoint.ContainerAsText(), aCaretPoint.Offset() + 1);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "nsRange::SetStartAndEnd() failed");
  };
  auto extendRangeToSelectCharacterBackward =
      [](nsRange& aRange, const EditorRawDOMPointInText& aCaretPoint) -> void {
    if (aCaretPoint.IsStartOfContainer()) {
      return;
    }
    const nsTextFragment& textFragment =
        aCaretPoint.ContainerAsText()->TextFragment();
    if (!textFragment.GetLength()) {
      return;
    }
    if (textFragment.IsLowSurrogateFollowingHighSurrogateAt(
            aCaretPoint.Offset() - 1)) {
      DebugOnly<nsresult> rvIgnored = aRange.SetStartAndEnd(
          aCaretPoint.ContainerAsText(), aCaretPoint.Offset() - 2,
          aCaretPoint.ContainerAsText(), aCaretPoint.Offset());
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                           "nsRange::SetStartAndEnd() failed");
      return;
    }
    DebugOnly<nsresult> rvIgnored = aRange.SetStartAndEnd(
        aCaretPoint.ContainerAsText(), aCaretPoint.Offset() - 1,
        aCaretPoint.ContainerAsText(), aCaretPoint.Offset());
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "nsRange::SetStartAndEnd() failed");
  };

  RefPtr<Element> editingHost = aHTMLEditor.GetActiveEditingHost();
  for (OwningNonNull<nsRange>& range : aRangesToDelete.Ranges()) {
    // If it's not collapsed, `DeleteRangeTransaction::Create()` will be called
    // with it and `DeleteRangeTransaction` won't modify the range.
    if (!range->Collapsed()) {
      continue;
    }

    if (howToHandleCollapsedRange ==
        EditorBase::HowToHandleCollapsedRange::Ignore) {
      continue;
    }

    // In the other cases, `EditorBase::CreateTransactionForCollapsedRange()`
    // will handle the collapsed range.
    EditorRawDOMPoint caretPoint(range->StartRef());
    if (howToHandleCollapsedRange ==
            EditorBase::HowToHandleCollapsedRange::ExtendBackward &&
        caretPoint.IsStartOfContainer()) {
      nsIContent* previousEditableContent = HTMLEditUtils::GetPreviousContent(
          *caretPoint.GetContainer(), {WalkTreeOption::IgnoreNonEditableNode},
          editingHost);
      if (!previousEditableContent) {
        continue;
      }
      if (!previousEditableContent->IsText()) {
        IgnoredErrorResult ignoredError;
        range->SelectNode(*previousEditableContent, ignoredError);
        NS_WARNING_ASSERTION(!ignoredError.Failed(),
                             "nsRange::SelectNode() failed");
        continue;
      }

      extendRangeToSelectCharacterBackward(
          range,
          EditorRawDOMPointInText::AtEndOf(*previousEditableContent->AsText()));
      continue;
    }

    if (howToHandleCollapsedRange ==
            EditorBase::HowToHandleCollapsedRange::ExtendForward &&
        caretPoint.IsEndOfContainer()) {
      nsIContent* nextEditableContent = HTMLEditUtils::GetNextContent(
          *caretPoint.GetContainer(), {WalkTreeOption::IgnoreNonEditableNode},
          editingHost);
      if (!nextEditableContent) {
        continue;
      }

      if (!nextEditableContent->IsText()) {
        IgnoredErrorResult ignoredError;
        range->SelectNode(*nextEditableContent, ignoredError);
        NS_WARNING_ASSERTION(!ignoredError.Failed(),
                             "nsRange::SelectNode() failed");
        continue;
      }

      extendRangeToSelectCharacterForward(
          range, EditorRawDOMPointInText(nextEditableContent->AsText(), 0));
      continue;
    }

    if (caretPoint.IsInTextNode()) {
      if (howToHandleCollapsedRange ==
          EditorBase::HowToHandleCollapsedRange::ExtendBackward) {
        extendRangeToSelectCharacterBackward(
            range, EditorRawDOMPointInText(caretPoint.ContainerAsText(),
                                           caretPoint.Offset()));
        continue;
      }
      extendRangeToSelectCharacterForward(
          range, EditorRawDOMPointInText(caretPoint.ContainerAsText(),
                                         caretPoint.Offset()));
      continue;
    }

    nsIContent* editableContent =
        howToHandleCollapsedRange ==
                EditorBase::HowToHandleCollapsedRange::ExtendBackward
            ? HTMLEditUtils::GetPreviousContent(
                  caretPoint, {WalkTreeOption::IgnoreNonEditableNode},
                  editingHost)
            : HTMLEditUtils::GetNextContent(
                  caretPoint, {WalkTreeOption::IgnoreNonEditableNode},
                  editingHost);
    if (!editableContent) {
      continue;
    }
    while (editableContent && editableContent->IsCharacterData() &&
           !editableContent->Length()) {
      editableContent =
          howToHandleCollapsedRange ==
                  EditorBase::HowToHandleCollapsedRange::ExtendBackward
              ? HTMLEditUtils::GetPreviousContent(
                    *editableContent, {WalkTreeOption::IgnoreNonEditableNode},
                    editingHost)
              : HTMLEditUtils::GetNextContent(
                    *editableContent, {WalkTreeOption::IgnoreNonEditableNode},
                    editingHost);
    }
    if (!editableContent) {
      continue;
    }

    if (!editableContent->IsText()) {
      IgnoredErrorResult ignoredError;
      range->SelectNode(*editableContent, ignoredError);
      NS_WARNING_ASSERTION(!ignoredError.Failed(),
                           "nsRange::SelectNode() failed");
      continue;
    }

    if (howToHandleCollapsedRange ==
        EditorBase::HowToHandleCollapsedRange::ExtendBackward) {
      extendRangeToSelectCharacterBackward(
          range, EditorRawDOMPointInText::AtEndOf(*editableContent->AsText()));
      continue;
    }
    extendRangeToSelectCharacterForward(
        range, EditorRawDOMPointInText(editableContent->AsText(), 0));
  }

  return NS_OK;
}

template <typename EditorDOMPointType>
nsresult HTMLEditor::DeleteTextAndTextNodesWithTransaction(
    const EditorDOMPointType& aStartPoint, const EditorDOMPointType& aEndPoint,
    TreatEmptyTextNodes aTreatEmptyTextNodes) {
  if (NS_WARN_IF(!aStartPoint.IsSet()) || NS_WARN_IF(!aEndPoint.IsSet())) {
    return NS_ERROR_INVALID_ARG;
  }

  // MOOSE: this routine needs to be modified to preserve the integrity of the
  // wsFragment info.

  if (aStartPoint == aEndPoint) {
    // Nothing to delete
    return NS_OK;
  }

  RefPtr<Element> editingHost = GetActiveEditingHost();
  auto deleteEmptyContentNodeWithTransaction =
      [this, &aTreatEmptyTextNodes, &editingHost](nsIContent& aContent)
          MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION -> nsresult {
    OwningNonNull<nsIContent> nodeToRemove = aContent;
    if (aTreatEmptyTextNodes ==
        TreatEmptyTextNodes::RemoveAllEmptyInlineAncestors) {
      Element* emptyParentElementToRemove =
          HTMLEditUtils::GetMostDistantAnscestorEditableEmptyInlineElement(
              nodeToRemove, editingHost);
      if (emptyParentElementToRemove) {
        nodeToRemove = *emptyParentElementToRemove;
      }
    }
    nsresult rv = DeleteNodeWithTransaction(nodeToRemove);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::DeleteNodeWithTransaction() failed");
    return rv;
  };

  if (aStartPoint.GetContainer() == aEndPoint.GetContainer() &&
      aStartPoint.IsInTextNode()) {
    if (aTreatEmptyTextNodes !=
            TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries &&
        aStartPoint.IsStartOfContainer() && aEndPoint.IsEndOfContainer()) {
      nsresult rv = deleteEmptyContentNodeWithTransaction(
          MOZ_KnownLive(*aStartPoint.ContainerAsText()));
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "deleteEmptyContentNodeWithTransaction() failed");
      return rv;
    }
    RefPtr<Text> textNode = aStartPoint.ContainerAsText();
    nsresult rv =
        DeleteTextWithTransaction(*textNode, aStartPoint.Offset(),
                                  aEndPoint.Offset() - aStartPoint.Offset());
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::DeleteTextWithTransaction() failed");
    return rv;
  }

  RefPtr<nsRange> range =
      nsRange::Create(aStartPoint.ToRawRangeBoundary(),
                      aEndPoint.ToRawRangeBoundary(), IgnoreErrors());
  if (!range) {
    NS_WARNING("nsRange::Create() failed");
    return NS_ERROR_FAILURE;
  }

  // Collect editable text nodes in the given range.
  AutoTArray<OwningNonNull<Text>, 16> arrayOfTextNodes;
  DOMIterator iter;
  if (NS_FAILED(iter.Init(*range))) {
    return NS_OK;  // Nothing to delete in the range.
  }
  iter.AppendNodesToArray(
      +[](nsINode& aNode, void*) {
        MOZ_ASSERT(aNode.IsText());
        return HTMLEditUtils::IsSimplyEditableNode(aNode);
      },
      arrayOfTextNodes);
  for (OwningNonNull<Text>& textNode : arrayOfTextNodes) {
    if (textNode == aStartPoint.GetContainer()) {
      if (aStartPoint.IsEndOfContainer()) {
        continue;
      }
      if (aStartPoint.IsStartOfContainer() &&
          aTreatEmptyTextNodes !=
              TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries) {
        nsresult rv = deleteEmptyContentNodeWithTransaction(
            MOZ_KnownLive(*aStartPoint.ContainerAsText()));
        if (NS_FAILED(rv)) {
          NS_WARNING("deleteEmptyContentNodeWithTransaction() failed");
          return rv;
        }
        continue;
      }
      nsresult rv = DeleteTextWithTransaction(
          MOZ_KnownLive(textNode), aStartPoint.Offset(),
          textNode->Length() - aStartPoint.Offset());
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::DeleteTextWithTransaction() failed");
        return rv;
      }
      continue;
    }

    if (textNode == aEndPoint.GetContainer()) {
      if (aEndPoint.IsStartOfContainer()) {
        break;
      }
      if (aEndPoint.IsEndOfContainer() &&
          aTreatEmptyTextNodes !=
              TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries) {
        nsresult rv = deleteEmptyContentNodeWithTransaction(
            MOZ_KnownLive(*aEndPoint.ContainerAsText()));
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "deleteEmptyContentNodeWithTransaction() failed");
        return rv;
      }
      nsresult rv = DeleteTextWithTransaction(MOZ_KnownLive(textNode), 0,
                                              aEndPoint.Offset());
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "HTMLEditor::DeleteTextWithTransaction() failed");
      return rv;
    }

    nsresult rv =
        deleteEmptyContentNodeWithTransaction(MOZ_KnownLive(textNode));
    if (NS_FAILED(rv)) {
      NS_WARNING("deleteEmptyContentNodeWithTransaction() failed");
      return rv;
    }
  }

  return NS_OK;
}

Result<EditorDOMPoint, nsresult> HTMLEditor::AutoDeleteRangesHandler::
    AutoBlockElementsJoiner::JoinNodesDeepWithTransaction(
        HTMLEditor& aHTMLEditor, nsIContent& aLeftContent,
        nsIContent& aRightContent) {
  // While the rightmost children and their descendants of the left node match
  // the leftmost children and their descendants of the right node, join them
  // up.

  nsCOMPtr<nsIContent> leftContentToJoin = &aLeftContent;
  nsCOMPtr<nsIContent> rightContentToJoin = &aRightContent;
  nsCOMPtr<nsINode> parentNode = aRightContent.GetParentNode();

  EditorDOMPoint ret;
  const HTMLEditUtils::StyleDifference kCompareStyle =
      aHTMLEditor.IsCSSEnabled() ? StyleDifference::CompareIfSpanElements
                                 : StyleDifference::Ignore;
  while (leftContentToJoin && rightContentToJoin && parentNode &&
         HTMLEditUtils::CanContentsBeJoined(
             *leftContentToJoin, *rightContentToJoin, kCompareStyle)) {
    uint32_t length = leftContentToJoin->Length();

    // Do the join
    nsresult rv = aHTMLEditor.JoinNodesWithTransaction(*leftContentToJoin,
                                                       *rightContentToJoin);
    if (NS_WARN_IF(aHTMLEditor.Destroyed())) {
      return Err(NS_ERROR_EDITOR_DESTROYED);
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::JoinNodesWithTransaction() failed");
      return Err(rv);
    }

    // XXX rightContentToJoin may have fewer children or shorter length text.
    //     So, we need some adjustment here.
    ret.Set(rightContentToJoin, length);
    if (NS_WARN_IF(!ret.IsSet())) {
      return Err(NS_ERROR_FAILURE);
    }

    if (parentNode->IsText()) {
      // We've joined all the way down to text nodes, we're done!
      return ret;
    }

    // Get new left and right nodes, and begin anew
    parentNode = rightContentToJoin;
    rightContentToJoin = parentNode->GetChildAt_Deprecated(length);
    if (rightContentToJoin) {
      leftContentToJoin = rightContentToJoin->GetPreviousSibling();
    } else {
      leftContentToJoin = nullptr;
    }

    // Skip over non-editable nodes
    while (leftContentToJoin && !EditorUtils::IsEditableContent(
                                    *leftContentToJoin, EditorType::HTML)) {
      leftContentToJoin = leftContentToJoin->GetPreviousSibling();
    }
    if (!leftContentToJoin) {
      return ret;
    }

    while (rightContentToJoin && !EditorUtils::IsEditableContent(
                                     *rightContentToJoin, EditorType::HTML)) {
      rightContentToJoin = rightContentToJoin->GetNextSibling();
    }
    if (!rightContentToJoin) {
      return ret;
    }
  }

  if (!ret.IsSet()) {
    NS_WARNING("HTMLEditor::JoinNodesDeepWithTransaction() joined no contents");
    return Err(NS_ERROR_FAILURE);
  }
  return ret;
}

Result<bool, nsresult> HTMLEditor::AutoDeleteRangesHandler::
    AutoBlockElementsJoiner::AutoInclusiveAncestorBlockElementsJoiner::Prepare(
        const HTMLEditor& aHTMLEditor) {
  mLeftBlockElement = HTMLEditUtils::GetInclusiveAncestorElement(
      mInclusiveDescendantOfLeftBlockElement,
      HTMLEditUtils::ClosestEditableBlockElementExceptHRElement);
  mRightBlockElement = HTMLEditUtils::GetInclusiveAncestorElement(
      mInclusiveDescendantOfRightBlockElement,
      HTMLEditUtils::ClosestEditableBlockElementExceptHRElement);

  if (NS_WARN_IF(!IsSet())) {
    mCanJoinBlocks = false;
    return Err(NS_ERROR_UNEXPECTED);
  }

  // Don't join the blocks if both of them are basic structure of the HTML
  // document (Note that `<body>` can be joined with its children).
  if (mLeftBlockElement->IsAnyOfHTMLElements(nsGkAtoms::html, nsGkAtoms::head,
                                             nsGkAtoms::body) &&
      mRightBlockElement->IsAnyOfHTMLElements(nsGkAtoms::html, nsGkAtoms::head,
                                              nsGkAtoms::body)) {
    mCanJoinBlocks = false;
    return false;
  }

  if (HTMLEditUtils::IsAnyTableElement(mLeftBlockElement) ||
      HTMLEditUtils::IsAnyTableElement(mRightBlockElement)) {
    // Do not try to merge table elements, cancel the deletion.
    mCanJoinBlocks = false;
    return false;
  }

  // Bail if both blocks the same
  if (IsSameBlockElement()) {
    mCanJoinBlocks = true;  // XXX Anyway, Run() will ingore this case.
    mFallbackToDeleteLeafContent = true;
    return true;
  }

  // Joining a list item to its parent is a NOP.
  if (HTMLEditUtils::IsAnyListElement(mLeftBlockElement) &&
      HTMLEditUtils::IsListItem(mRightBlockElement) &&
      mRightBlockElement->GetParentNode() == mLeftBlockElement) {
    mCanJoinBlocks = false;
    return true;
  }

  // Special rule here: if we are trying to join list items, and they are in
  // different lists, join the lists instead.
  if (HTMLEditUtils::IsListItem(mLeftBlockElement) &&
      HTMLEditUtils::IsListItem(mRightBlockElement)) {
    // XXX leftListElement and/or rightListElement may be not list elements.
    Element* leftListElement = mLeftBlockElement->GetParentElement();
    Element* rightListElement = mRightBlockElement->GetParentElement();
    EditorDOMPoint atChildInBlock;
    if (leftListElement && rightListElement &&
        leftListElement != rightListElement &&
        !EditorUtils::IsDescendantOf(*leftListElement, *mRightBlockElement,
                                     &atChildInBlock) &&
        !EditorUtils::IsDescendantOf(*rightListElement, *mLeftBlockElement,
                                     &atChildInBlock)) {
      // There are some special complications if the lists are descendants of
      // the other lists' items.  Note that it is okay for them to be
      // descendants of the other lists themselves, which is the usual case for
      // sublists in our implementation.
      MOZ_DIAGNOSTIC_ASSERT(!atChildInBlock.IsSet());
      mLeftBlockElement = leftListElement;
      mRightBlockElement = rightListElement;
      mNewListElementTagNameOfRightListElement =
          Some(leftListElement->NodeInfo()->NameAtom());
    }
  }

  if (!EditorUtils::IsDescendantOf(*mLeftBlockElement, *mRightBlockElement,
                                   &mPointContainingTheOtherBlockElement)) {
    Unused << EditorUtils::IsDescendantOf(
        *mRightBlockElement, *mLeftBlockElement,
        &mPointContainingTheOtherBlockElement);
  }

  if (mPointContainingTheOtherBlockElement.GetContainer() ==
      mRightBlockElement) {
    mPrecedingInvisibleBRElement =
        WSRunScanner::GetPrecedingBRElementUnlessVisibleContentFound(
            aHTMLEditor.GetActiveEditingHost(),
            EditorDOMPoint::AtEndOf(mLeftBlockElement));
    // `WhiteSpaceVisibilityKeeper::
    // MergeFirstLineOfRightBlockElementIntoDescendantLeftBlockElement()`
    // returns ignored when:
    // - No preceding invisible `<br>` element and
    // - mNewListElementTagNameOfRightListElement is nothing and
    // - There is no content to move from right block element.
    if (!mPrecedingInvisibleBRElement) {
      if (CanMergeLeftAndRightBlockElements()) {
        // Always marked as handled in this case.
        mFallbackToDeleteLeafContent = false;
      } else {
        // Marked as handled only when it actually moves a content node.
        Result<bool, nsresult> firstLineHasContent =
            aHTMLEditor.CanMoveOrDeleteSomethingInHardLine(
                mPointContainingTheOtherBlockElement.NextPoint());
        mFallbackToDeleteLeafContent =
            firstLineHasContent.isOk() && !firstLineHasContent.inspect();
      }
    } else {
      // Marked as handled when deleting the invisible `<br>` element.
      mFallbackToDeleteLeafContent = false;
    }
  } else if (mPointContainingTheOtherBlockElement.GetContainer() ==
             mLeftBlockElement) {
    mPrecedingInvisibleBRElement =
        WSRunScanner::GetPrecedingBRElementUnlessVisibleContentFound(
            aHTMLEditor.GetActiveEditingHost(),
            mPointContainingTheOtherBlockElement);
    // `WhiteSpaceVisibilityKeeper::
    // MergeFirstLineOfRightBlockElementIntoAncestorLeftBlockElement()`
    // returns ignored when:
    // - No preceding invisible `<br>` element and
    // - mNewListElementTagNameOfRightListElement is some and
    // - The right block element has no children
    // or,
    // - No preceding invisible `<br>` element and
    // - mNewListElementTagNameOfRightListElement is nothing and
    // - There is no content to move from right block element.
    if (!mPrecedingInvisibleBRElement) {
      if (CanMergeLeftAndRightBlockElements()) {
        // Marked as handled only when it actualy moves a content node.
        Result<bool, nsresult> rightBlockHasContent =
            aHTMLEditor.CanMoveChildren(*mRightBlockElement,
                                        *mLeftBlockElement);
        mFallbackToDeleteLeafContent =
            rightBlockHasContent.isOk() && !rightBlockHasContent.inspect();
      } else {
        // Marked as handled only when it actually moves a content node.
        Result<bool, nsresult> firstLineHasContent =
            aHTMLEditor.CanMoveOrDeleteSomethingInHardLine(
                EditorRawDOMPoint(mRightBlockElement, 0));
        mFallbackToDeleteLeafContent =
            firstLineHasContent.isOk() && !firstLineHasContent.inspect();
      }
    } else {
      // Marked as handled when deleting the invisible `<br>` element.
      mFallbackToDeleteLeafContent = false;
    }
  } else {
    mPrecedingInvisibleBRElement =
        WSRunScanner::GetPrecedingBRElementUnlessVisibleContentFound(
            aHTMLEditor.GetActiveEditingHost(),
            EditorDOMPoint::AtEndOf(mLeftBlockElement));
    // `WhiteSpaceVisibilityKeeper::
    // MergeFirstLineOfRightBlockElementIntoLeftBlockElement()` always
    // return "handled".
    mFallbackToDeleteLeafContent = false;
  }

  mCanJoinBlocks = true;
  return true;
}

nsresult HTMLEditor::AutoDeleteRangesHandler::AutoBlockElementsJoiner::
    AutoInclusiveAncestorBlockElementsJoiner::ComputeRangesToDelete(
        const HTMLEditor& aHTMLEditor, const EditorDOMPoint& aCaretPoint,
        AutoRangeArray& aRangesToDelete) const {
  MOZ_ASSERT(!aRangesToDelete.Ranges().IsEmpty());
  MOZ_ASSERT(mLeftBlockElement);
  MOZ_ASSERT(mRightBlockElement);

  if (IsSameBlockElement()) {
    if (!aCaretPoint.IsSet()) {
      return NS_OK;  // The ranges are not collapsed, keep them as-is.
    }
    nsresult rv = aRangesToDelete.Collapse(aCaretPoint);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "AutoRangeArray::Collapse() failed");
    return rv;
  }

  EditorDOMPoint pointContainingTheOtherBlock;
  if (!EditorUtils::IsDescendantOf(*mLeftBlockElement, *mRightBlockElement,
                                   &pointContainingTheOtherBlock)) {
    Unused << EditorUtils::IsDescendantOf(
        *mRightBlockElement, *mLeftBlockElement, &pointContainingTheOtherBlock);
  }
  EditorDOMRange range =
      WSRunScanner::GetRangeForDeletingBlockElementBoundaries(
          aHTMLEditor, *mLeftBlockElement, *mRightBlockElement,
          pointContainingTheOtherBlock);
  if (!range.IsPositioned()) {
    NS_WARNING(
        "WSRunScanner::GetRangeForDeletingBlockElementBoundaries() failed");
    return NS_ERROR_FAILURE;
  }
  if (!aCaretPoint.IsSet()) {
    // Don't shrink the original range.
    bool noNeedToChangeStart = false;
    EditorDOMPoint atStart(aRangesToDelete.GetStartPointOfFirstRange());
    if (atStart.IsBefore(range.StartRef())) {
      // If the range starts from end of a container, and computed block
      // boundaries range starts from an invisible `<br>` element,  we
      // may need to shrink the range.
      Element* editingHost = aHTMLEditor.GetActiveEditingHost();
      NS_WARNING_ASSERTION(editingHost, "There was no editing host");
      nsIContent* nextContent =
          atStart.IsEndOfContainer() && range.StartRef().GetChild() &&
                  HTMLEditUtils::IsInvisibleBRElement(
                      *range.StartRef().GetChild(), editingHost)
              ? HTMLEditUtils::GetNextContent(
                    *atStart.ContainerAsContent(),
                    {WalkTreeOption::IgnoreDataNodeExceptText,
                     WalkTreeOption::StopAtBlockBoundary},
                    editingHost)
              : nullptr;
      if (!nextContent || nextContent != range.StartRef().GetChild()) {
        noNeedToChangeStart = true;
        range.SetStart(aRangesToDelete.GetStartPointOfFirstRange());
      }
    }
    if (range.EndRef().IsBefore(aRangesToDelete.GetEndPointOfFirstRange())) {
      if (noNeedToChangeStart) {
        return NS_OK;  // We don't need to modify the range.
      }
      range.SetEnd(aRangesToDelete.GetEndPointOfFirstRange());
    }
  }
  // XXX Oddly, we join blocks only at the first range.
  nsresult rv = aRangesToDelete.FirstRangeRef()->SetStartAndEnd(
      range.StartRef().ToRawRangeBoundary(),
      range.EndRef().ToRawRangeBoundary());
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "AutoRangeArray::SetStartAndEnd() failed");
  return rv;
}

EditActionResult HTMLEditor::AutoDeleteRangesHandler::AutoBlockElementsJoiner::
    AutoInclusiveAncestorBlockElementsJoiner::Run(HTMLEditor& aHTMLEditor) {
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());
  MOZ_ASSERT(mLeftBlockElement);
  MOZ_ASSERT(mRightBlockElement);

  if (IsSameBlockElement()) {
    return EditActionIgnored();
  }

  if (!mCanJoinBlocks) {
    return EditActionHandled();
  }

  // If the left block element is in the right block element, move the hard
  // line including the right block element to end of the left block.
  // However, if we are merging list elements, we don't join them.
  if (mPointContainingTheOtherBlockElement.GetContainer() ==
      mRightBlockElement) {
    EditActionResult result = WhiteSpaceVisibilityKeeper::
        MergeFirstLineOfRightBlockElementIntoDescendantLeftBlockElement(
            aHTMLEditor, MOZ_KnownLive(*mLeftBlockElement),
            MOZ_KnownLive(*mRightBlockElement),
            mPointContainingTheOtherBlockElement,
            mNewListElementTagNameOfRightListElement,
            MOZ_KnownLive(mPrecedingInvisibleBRElement));
    NS_WARNING_ASSERTION(result.Succeeded(),
                         "WhiteSpaceVisibilityKeeper::"
                         "MergeFirstLineOfRightBlockElementIntoDescendantLeftBl"
                         "ockElement() failed");
    return result;
  }

  // If the right block element is in the left block element:
  // - move list item elements in the right block element to where the left
  //   list element is
  // - or first hard line in the right block element to where:
  //   - the left block element is.
  //   - or the given left content in the left block is.
  if (mPointContainingTheOtherBlockElement.GetContainer() ==
      mLeftBlockElement) {
    EditActionResult result = WhiteSpaceVisibilityKeeper::
        MergeFirstLineOfRightBlockElementIntoAncestorLeftBlockElement(
            aHTMLEditor, MOZ_KnownLive(*mLeftBlockElement),
            MOZ_KnownLive(*mRightBlockElement),
            mPointContainingTheOtherBlockElement,
            MOZ_KnownLive(*mInclusiveDescendantOfLeftBlockElement),
            mNewListElementTagNameOfRightListElement,
            MOZ_KnownLive(mPrecedingInvisibleBRElement));
    NS_WARNING_ASSERTION(result.Succeeded(),
                         "WhiteSpaceVisibilityKeeper::"
                         "MergeFirstLineOfRightBlockElementIntoAncestorLeftBloc"
                         "kElement() failed");
    return result;
  }

  MOZ_ASSERT(!mPointContainingTheOtherBlockElement.IsSet());

  // Normal case.  Blocks are siblings, or at least close enough.  An example
  // of the latter is <p>paragraph</p><ul><li>one<li>two<li>three</ul>.  The
  // first li and the p are not true siblings, but we still want to join them
  // if you backspace from li into p.
  EditActionResult result = WhiteSpaceVisibilityKeeper::
      MergeFirstLineOfRightBlockElementIntoLeftBlockElement(
          aHTMLEditor, MOZ_KnownLive(*mLeftBlockElement),
          MOZ_KnownLive(*mRightBlockElement),
          mNewListElementTagNameOfRightListElement,
          MOZ_KnownLive(mPrecedingInvisibleBRElement));
  NS_WARNING_ASSERTION(
      result.Succeeded(),
      "WhiteSpaceVisibilityKeeper::"
      "MergeFirstLineOfRightBlockElementIntoLeftBlockElement() failed");
  return result;
}

template <typename PT, typename CT>
Result<bool, nsresult> HTMLEditor::CanMoveOrDeleteSomethingInHardLine(
    const EditorDOMPointBase<PT, CT>& aPointInHardLine) const {
  RefPtr<nsRange> oneLineRange = CreateRangeExtendedToHardLineStartAndEnd(
      aPointInHardLine.ToRawRangeBoundary(),
      aPointInHardLine.ToRawRangeBoundary(),
      EditSubAction::eMergeBlockContents);
  if (!oneLineRange || oneLineRange->Collapsed() ||
      !oneLineRange->IsPositioned() ||
      !oneLineRange->GetStartContainer()->IsContent() ||
      !oneLineRange->GetEndContainer()->IsContent()) {
    return false;
  }

  // If there is only a padding `<br>` element in a empty block, it's selected
  // by `SelectBRElementIfCollapsedInEmptyBlock()`.  However, it won't be
  // moved.  Although it'll be deleted, `MoveOneHardLineContents()` returns
  // "ignored".  Therefore, we should return `false` in this case.
  if (nsIContent* childContent = oneLineRange->GetChildAtStartOffset()) {
    if (childContent->IsHTMLElement(nsGkAtoms::br) &&
        childContent->GetParent()) {
      if (const Element* blockElement =
              HTMLEditUtils::GetInclusiveAncestorElement(
                  *childContent->GetParent(),
                  HTMLEditUtils::ClosestBlockElement)) {
        if (HTMLEditUtils::IsEmptyNode(*blockElement)) {
          return false;
        }
      }
    }
  }

  nsINode* commonAncestor = oneLineRange->GetClosestCommonInclusiveAncestor();
  // Currently, we move non-editable content nodes too.
  EditorRawDOMPoint startPoint(oneLineRange->StartRef());
  if (!startPoint.IsEndOfContainer()) {
    return true;
  }
  EditorRawDOMPoint endPoint(oneLineRange->EndRef());
  if (!endPoint.IsStartOfContainer()) {
    return true;
  }
  if (startPoint.GetContainer() != commonAncestor) {
    while (true) {
      EditorRawDOMPoint pointInParent(startPoint.GetContainerAsContent());
      if (NS_WARN_IF(!pointInParent.IsInContentNode())) {
        return Err(NS_ERROR_FAILURE);
      }
      if (pointInParent.GetContainer() == commonAncestor) {
        startPoint = pointInParent;
        break;
      }
      if (!pointInParent.IsEndOfContainer()) {
        return true;
      }
    }
  }
  if (endPoint.GetContainer() != commonAncestor) {
    while (true) {
      EditorRawDOMPoint pointInParent(endPoint.GetContainerAsContent());
      if (NS_WARN_IF(!pointInParent.IsInContentNode())) {
        return Err(NS_ERROR_FAILURE);
      }
      if (pointInParent.GetContainer() == commonAncestor) {
        endPoint = pointInParent;
        break;
      }
      if (!pointInParent.IsStartOfContainer()) {
        return true;
      }
    }
  }
  // If start point and end point in the common ancestor are direct siblings,
  // there is no content to move or delete.
  // E.g., `<b>abc<br>[</b><i>]<br>def</i>`.
  return startPoint.GetNextSiblingOfChild() != endPoint.GetChild();
}

MoveNodeResult HTMLEditor::MoveOneHardLineContents(
    const EditorDOMPoint& aPointInHardLine,
    const EditorDOMPoint& aPointToInsert,
    MoveToEndOfContainer
        aMoveToEndOfContainer /* = MoveToEndOfContainer::No */) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  AutoTArray<OwningNonNull<nsIContent>, 64> arrayOfContents;
  nsresult rv = SplitInlinesAndCollectEditTargetNodesInOneHardLine(
      aPointInHardLine, arrayOfContents, EditSubAction::eMergeBlockContents,
      HTMLEditor::CollectNonEditableNodes::Yes);
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "HTMLEditor::SplitInlinesAndCollectEditTargetNodesInOneHardLine("
        "eMergeBlockContents, CollectNonEditableNodes::Yes) failed");
    return MoveNodeResult(rv);
  }
  if (arrayOfContents.IsEmpty()) {
    return MoveNodeIgnored(aPointToInsert);
  }

  uint32_t offset = aPointToInsert.Offset();
  MoveNodeResult result;
  for (auto& content : arrayOfContents) {
    if (aMoveToEndOfContainer == MoveToEndOfContainer::Yes) {
      // For backward compatibility, we should move contents to end of the
      // container if this is called with MoveToEndOfContainer::Yes.
      offset = aPointToInsert.GetContainer()->Length();
    }
    // get the node to act on
    if (HTMLEditUtils::IsBlockElement(content)) {
      // For block nodes, move their contents only, then delete block.
      result |= MoveChildrenWithTransaction(
          MOZ_KnownLive(*content->AsElement()),
          EditorDOMPoint(aPointToInsert.GetContainer(), offset));
      if (result.Failed()) {
        NS_WARNING("HTMLEditor::MoveChildrenWithTransaction() failed");
        return result;
      }
      offset = result.NextInsertionPointRef().Offset();
      // MOZ_KnownLive because 'arrayOfContents' is guaranteed to
      // keep it alive.
      DebugOnly<nsresult> rvIgnored =
          DeleteNodeWithTransaction(MOZ_KnownLive(*content));
      if (NS_WARN_IF(Destroyed())) {
        return MoveNodeResult(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "HTMLEditor::DeleteNodeWithTransaction() failed, but ignored");
      result.MarkAsHandled();
      if (MayHaveMutationEventListeners()) {
        // Mutation event listener may make `offset` value invalid with
        // removing some previous children while we call
        // `DeleteNodeWithTransaction()` so that we should adjust it here.
        offset = std::min(offset, aPointToInsert.GetContainer()->Length());
      }
      continue;
    }
    // XXX Different from the above block, we ignore error of moving nodes.
    // MOZ_KnownLive because 'arrayOfContents' is guaranteed to
    // keep it alive.
    MoveNodeResult moveNodeResult = MoveNodeOrChildrenWithTransaction(
        MOZ_KnownLive(content),
        EditorDOMPoint(aPointToInsert.GetContainer(), offset));
    if (NS_WARN_IF(moveNodeResult.EditorDestroyed())) {
      return MoveNodeResult(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(
        moveNodeResult.Succeeded(),
        "HTMLEditor::MoveNodeOrChildrenWithTransaction() failed, but ignored");
    if (moveNodeResult.Succeeded()) {
      offset = moveNodeResult.NextInsertionPointRef().Offset();
      result |= moveNodeResult;
    }
  }

  NS_WARNING_ASSERTION(
      result.Succeeded(),
      "Last HTMLEditor::MoveNodeOrChildrenWithTransaction() failed");
  return result;
}

Result<bool, nsresult> HTMLEditor::CanMoveNodeOrChildren(
    const nsIContent& aContent, const nsINode& aNewContainer) const {
  if (HTMLEditUtils::CanNodeContain(aNewContainer, aContent)) {
    return true;
  }
  if (aContent.IsElement()) {
    return CanMoveChildren(*aContent.AsElement(), aNewContainer);
  }
  return true;
}

MoveNodeResult HTMLEditor::MoveNodeOrChildrenWithTransaction(
    nsIContent& aContent, const EditorDOMPoint& aPointToInsert) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(aPointToInsert.IsSet());

  // Check if this node can go into the destination node
  if (HTMLEditUtils::CanNodeContain(*aPointToInsert.GetContainer(), aContent)) {
    // If it can, move it there.
    uint32_t offsetAtInserting = aPointToInsert.Offset();
    nsresult rv = MoveNodeWithTransaction(aContent, aPointToInsert);
    if (NS_WARN_IF(Destroyed())) {
      return MoveNodeResult(NS_ERROR_EDITOR_DESTROYED);
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::MoveNodeWithTransaction() failed");
      return MoveNodeResult(rv);
    }
    // Advance DOM point with offset for keeping backward compatibility.
    // XXX Where should we insert next content if a mutation event listener
    //     break the relation of offset and moved node?
    return MoveNodeHandled(aPointToInsert.GetContainer(), ++offsetAtInserting);
  }

  // If it can't, move its children (if any), and then delete it.
  MoveNodeResult result;
  if (aContent.IsElement()) {
    result = MoveChildrenWithTransaction(MOZ_KnownLive(*aContent.AsElement()),
                                         aPointToInsert);
    if (result.Failed()) {
      NS_WARNING("HTMLEditor::MoveChildrenWithTransaction() failed");
      return result;
    }
  } else {
    result = MoveNodeHandled(aPointToInsert);
  }

  nsresult rv = DeleteNodeWithTransaction(aContent);
  if (NS_WARN_IF(Destroyed())) {
    return MoveNodeResult(NS_ERROR_EDITOR_DESTROYED);
  }
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
    return MoveNodeResult(rv);
  }
  if (MayHaveMutationEventListeners()) {
    // Mutation event listener may make `offset` value invalid with
    // removing some previous children while we call
    // `DeleteNodeWithTransaction()` so that we should adjust it here.
    if (!result.NextInsertionPointRef().IsSetAndValid()) {
      result = MoveNodeHandled(
          EditorDOMPoint::AtEndOf(*aPointToInsert.GetContainer()));
    }
  }
  return result;
}

Result<bool, nsresult> HTMLEditor::CanMoveChildren(
    const Element& aElement, const nsINode& aNewContainer) const {
  if (NS_WARN_IF(&aElement == &aNewContainer)) {
    return Err(NS_ERROR_FAILURE);
  }
  for (nsIContent* childContent = aElement.GetFirstChild(); childContent;
       childContent = childContent->GetNextSibling()) {
    Result<bool, nsresult> result =
        CanMoveNodeOrChildren(*childContent, aNewContainer);
    if (result.isErr() || result.inspect()) {
      return result;
    }
  }
  return false;
}

MoveNodeResult HTMLEditor::MoveChildrenWithTransaction(
    Element& aElement, const EditorDOMPoint& aPointToInsert) {
  MOZ_ASSERT(aPointToInsert.IsSet());

  if (NS_WARN_IF(&aElement == aPointToInsert.GetContainer())) {
    return MoveNodeResult(NS_ERROR_INVALID_ARG);
  }

  MoveNodeResult result = MoveNodeIgnored(aPointToInsert);
  while (aElement.GetFirstChild()) {
    result |= MoveNodeOrChildrenWithTransaction(
        MOZ_KnownLive(*aElement.GetFirstChild()), result.NextInsertionPoint());
    if (result.Failed()) {
      NS_WARNING("HTMLEditor::MoveNodeOrChildrenWithTransaction() failed");
      return result;
    }
  }
  return result;
}

void HTMLEditor::MoveAllChildren(nsINode& aContainer,
                                 const EditorRawDOMPoint& aPointToInsert,
                                 ErrorResult& aError) {
  MOZ_ASSERT(!aError.Failed());

  if (!aContainer.HasChildren()) {
    return;
  }
  nsIContent* firstChild = aContainer.GetFirstChild();
  if (NS_WARN_IF(!firstChild)) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }
  nsIContent* lastChild = aContainer.GetLastChild();
  if (NS_WARN_IF(!lastChild)) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }
  MoveChildrenBetween(*firstChild, *lastChild, aPointToInsert, aError);
  NS_WARNING_ASSERTION(!aError.Failed(),
                       "HTMLEditor::MoveChildrenBetween() failed");
}

void HTMLEditor::MoveChildrenBetween(nsIContent& aFirstChild,
                                     nsIContent& aLastChild,
                                     const EditorRawDOMPoint& aPointToInsert,
                                     ErrorResult& aError) {
  nsCOMPtr<nsINode> oldContainer = aFirstChild.GetParentNode();
  if (NS_WARN_IF(oldContainer != aLastChild.GetParentNode()) ||
      NS_WARN_IF(!aPointToInsert.IsSet()) ||
      NS_WARN_IF(!aPointToInsert.CanContainerHaveChildren())) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  // First, store all children which should be moved to the new container.
  AutoTArray<nsCOMPtr<nsIContent>, 10> children;
  for (nsIContent* child = &aFirstChild; child;
       child = child->GetNextSibling()) {
    children.AppendElement(child);
    if (child == &aLastChild) {
      break;
    }
  }

  if (NS_WARN_IF(children.LastElement() != &aLastChild)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  nsCOMPtr<nsINode> newContainer = aPointToInsert.GetContainer();
  nsCOMPtr<nsIContent> nextNode = aPointToInsert.GetChild();
  for (size_t i = children.Length(); i > 0; --i) {
    nsCOMPtr<nsIContent>& child = children[i - 1];
    if (child->GetParentNode() != oldContainer) {
      // If the child has been moved to different container, we shouldn't
      // touch it.
      continue;
    }
    oldContainer->RemoveChild(*child, aError);
    if (aError.Failed()) {
      NS_WARNING("nsINode::RemoveChild() failed");
      return;
    }
    if (nextNode) {
      // If we're not appending the children to the new container, we should
      // check if referring next node of insertion point is still in the new
      // container.
      EditorRawDOMPoint pointToInsert(nextNode);
      if (NS_WARN_IF(!pointToInsert.IsSet()) ||
          NS_WARN_IF(pointToInsert.GetContainer() != newContainer)) {
        // The next node of insertion point has been moved by mutation observer.
        // Let's stop moving the remaining nodes.
        // XXX Or should we move remaining children after the last moved child?
        aError.Throw(NS_ERROR_FAILURE);
        return;
      }
    }
    newContainer->InsertBefore(*child, nextNode, aError);
    if (aError.Failed()) {
      NS_WARNING("nsINode::InsertBefore() failed");
      return;
    }
    // If the child was inserted or appended properly, the following children
    // should be inserted before it.  Otherwise, keep using current position.
    if (child->GetParentNode() == newContainer) {
      nextNode = child;
    }
  }
}

void HTMLEditor::MovePreviousSiblings(nsIContent& aChild,
                                      const EditorRawDOMPoint& aPointToInsert,
                                      ErrorResult& aError) {
  MOZ_ASSERT(!aError.Failed());

  if (NS_WARN_IF(!aChild.GetParentNode())) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return;
  }
  nsIContent* firstChild = aChild.GetParentNode()->GetFirstChild();
  if (NS_WARN_IF(!firstChild)) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }
  nsIContent* lastChild =
      &aChild == firstChild ? firstChild : aChild.GetPreviousSibling();
  if (NS_WARN_IF(!lastChild)) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }
  MoveChildrenBetween(*firstChild, *lastChild, aPointToInsert, aError);
  NS_WARNING_ASSERTION(!aError.Failed(),
                       "HTMLEditor::MoveChildrenBetween() failed");
}

nsresult HTMLEditor::AutoDeleteRangesHandler::AutoBlockElementsJoiner::
    DeleteContentButKeepTableStructure(HTMLEditor& aHTMLEditor,
                                       nsIContent& aContent) {
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());

  if (!HTMLEditUtils::IsAnyTableElementButNotTable(&aContent)) {
    nsresult rv = aHTMLEditor.DeleteNodeWithTransaction(aContent);
    if (NS_WARN_IF(aHTMLEditor.Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::DeleteNodeWithTransaction() failed");
    return rv;
  }

  // XXX For performance, this should just call
  //     DeleteContentButKeepTableStructure() while there are children in
  //     aContent.  If we need to avoid infinite loop because mutation event
  //     listeners can add unexpected nodes into aContent, we should just loop
  //     only original count of the children.
  AutoTArray<OwningNonNull<nsIContent>, 10> childList;
  for (nsIContent* child = aContent.GetFirstChild(); child;
       child = child->GetNextSibling()) {
    childList.AppendElement(*child);
  }

  for (const auto& child : childList) {
    // MOZ_KnownLive because 'childList' is guaranteed to
    // keep it alive.
    nsresult rv =
        DeleteContentButKeepTableStructure(aHTMLEditor, MOZ_KnownLive(child));
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::DeleteContentButKeepTableStructure() failed");
      return rv;
    }
  }
  return NS_OK;
}

nsresult HTMLEditor::DeleteMostAncestorMailCiteElementIfEmpty(
    nsIContent& aContent) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // The element must be `<blockquote type="cite">` or
  // `<span _moz_quote="true">`.
  RefPtr<Element> mailCiteElement = GetMostAncestorMailCiteElement(aContent);
  if (!mailCiteElement) {
    return NS_OK;
  }
  bool seenBR = false;
  if (!HTMLEditUtils::IsEmptyNode(*mailCiteElement,
                                  {EmptyCheckOption::TreatListItemAsVisible,
                                   EmptyCheckOption::TreatTableCellAsVisible},
                                  &seenBR)) {
    return NS_OK;
  }
  EditorDOMPoint atEmptyMailCiteElement(mailCiteElement);
  {
    AutoEditorDOMPointChildInvalidator lockOffset(atEmptyMailCiteElement);
    nsresult rv = DeleteNodeWithTransaction(*mailCiteElement);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
      return rv;
    }
  }

  if (!atEmptyMailCiteElement.IsSet() || !seenBR) {
    NS_WARNING_ASSERTION(
        atEmptyMailCiteElement.IsSet(),
        "Mutation event listener might changed the DOM tree during "
        "HTMLEditor::DeleteNodeWithTransaction(), but ignored");
    return NS_OK;
  }

  Result<RefPtr<Element>, nsresult> resultOfInsertingBRElement =
      InsertBRElementWithTransaction(atEmptyMailCiteElement);
  if (resultOfInsertingBRElement.isErr()) {
    NS_WARNING("HTMLEditor::InsertBRElementWithTransaction() failed");
    return resultOfInsertingBRElement.unwrapErr();
  }
  MOZ_ASSERT(resultOfInsertingBRElement.inspect());
  nsresult rv = CollapseSelectionTo(
      EditorRawDOMPoint(resultOfInsertingBRElement.inspect()));
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::::CollapseSelectionTo() failed, but ignored");
  return NS_OK;
}

Element* HTMLEditor::AutoDeleteRangesHandler::AutoEmptyBlockAncestorDeleter::
    ScanEmptyBlockInclusiveAncestor(const HTMLEditor& aHTMLEditor,
                                    nsIContent& aStartContent) {
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());

  // If we are inside an empty block, delete it.
  // Note: do NOT delete table elements this way.
  // Note: do NOT delete non-editable block element.
  Element* editableBlockElement = HTMLEditUtils::GetInclusiveAncestorElement(
      aStartContent, HTMLEditUtils::ClosestEditableBlockElement);
  if (!editableBlockElement) {
    return nullptr;
  }
  // XXX Perhaps, this is slow loop.  If empty blocks are nested, then,
  //     each block checks whether it's empty or not.  However, descendant
  //     blocks are checked again and again by IsEmptyNode().  Perhaps, it
  //     should be able to take "known empty element" for avoiding same checks.
  while (editableBlockElement &&
         HTMLEditUtils::IsRemovableFromParentNode(*editableBlockElement) &&
         !HTMLEditUtils::IsAnyTableElement(editableBlockElement) &&
         HTMLEditUtils::IsEmptyNode(*editableBlockElement)) {
    mEmptyInclusiveAncestorBlockElement = editableBlockElement;
    editableBlockElement = HTMLEditUtils::GetAncestorElement(
        *mEmptyInclusiveAncestorBlockElement,
        HTMLEditUtils::ClosestEditableBlockElement);
  }
  if (!mEmptyInclusiveAncestorBlockElement) {
    return nullptr;
  }

  // XXX Because of not checking whether found block element is editable
  //     in the above loop, empty ediable block element may be overwritten
  //     with empty non-editable clock element.  Therefore, we fail to
  //     remove the found empty nodes.
  if (NS_WARN_IF(!mEmptyInclusiveAncestorBlockElement->IsEditable()) ||
      NS_WARN_IF(!mEmptyInclusiveAncestorBlockElement->GetParentElement())) {
    mEmptyInclusiveAncestorBlockElement = nullptr;
  }
  return mEmptyInclusiveAncestorBlockElement;
}

nsresult HTMLEditor::AutoDeleteRangesHandler::AutoEmptyBlockAncestorDeleter::
    ComputeTargetRanges(const HTMLEditor& aHTMLEditor,
                        nsIEditor::EDirection aDirectionAndAmount,
                        const Element& aEditingHost,
                        AutoRangeArray& aRangesToDelete) const {
  MOZ_ASSERT(mEmptyInclusiveAncestorBlockElement);

  // We'll delete `mEmptyInclusiveAncestorBlockElement` node from the tree, but
  // we should return the range from start/end of next/previous editable content
  // to end/start of the element for compatiblity with the other browsers.
  switch (aDirectionAndAmount) {
    case nsIEditor::eNone:
      break;
    case nsIEditor::ePrevious:
    case nsIEditor::ePreviousWord:
    case nsIEditor::eToBeginningOfLine: {
      EditorRawDOMPoint startPoint =
          HTMLEditUtils::GetPreviousEditablePoint<EditorRawDOMPoint>(
              *mEmptyInclusiveAncestorBlockElement, &aEditingHost,
              // In this case, we don't join block elements so that we won't
              // delete invisible trailing whitespaces in the previous element.
              InvisibleWhiteSpaces::Preserve,
              // In this case, we won't join table cells so that we should
              // get a range which is in a table cell even if it's in a
              // table.
              TableBoundary::NoCrossAnyTableElement);
      if (!startPoint.IsSet()) {
        NS_WARNING(
            "HTMLEditUtils::GetPreviousEditablePoint() didn't return a valid "
            "point");
        return NS_ERROR_FAILURE;
      }
      nsresult rv = aRangesToDelete.SetStartAndEnd(
          startPoint,
          EditorRawDOMPoint::AtEndOf(mEmptyInclusiveAncestorBlockElement));
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "AutoRangeArray::SetStartAndEnd() failed");
      return rv;
    }
    case nsIEditor::eNext:
    case nsIEditor::eNextWord:
    case nsIEditor::eToEndOfLine: {
      EditorRawDOMPoint endPoint =
          HTMLEditUtils::GetNextEditablePoint<EditorRawDOMPoint>(
              *mEmptyInclusiveAncestorBlockElement, &aEditingHost,
              // In this case, we don't join block elements so that we won't
              // delete invisible trailing whitespaces in the next element.
              InvisibleWhiteSpaces::Preserve,
              // In this case, we won't join table cells so that we should
              // get a range which is in a table cell even if it's in a
              // table.
              TableBoundary::NoCrossAnyTableElement);
      if (!endPoint.IsSet()) {
        NS_WARNING(
            "HTMLEditUtils::GetNextEditablePoint() didn't return a valid "
            "point");
        return NS_ERROR_FAILURE;
      }
      nsresult rv = aRangesToDelete.SetStartAndEnd(
          EditorRawDOMPoint(mEmptyInclusiveAncestorBlockElement, 0), endPoint);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "AutoRangeArray::SetStartAndEnd() failed");
      return rv;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("Handle the nsIEditor::EDirection value");
      break;
  }
  // No direction, let's select the element to be deleted.
  nsresult rv =
      aRangesToDelete.SelectNode(*mEmptyInclusiveAncestorBlockElement);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "AutoRangeArray::SelectNode() failed");
  return rv;
}

Result<RefPtr<Element>, nsresult>
HTMLEditor::AutoDeleteRangesHandler::AutoEmptyBlockAncestorDeleter::
    MaybeInsertBRElementBeforeEmptyListItemElement(HTMLEditor& aHTMLEditor) {
  MOZ_ASSERT(mEmptyInclusiveAncestorBlockElement);
  MOZ_ASSERT(mEmptyInclusiveAncestorBlockElement->GetParentElement());
  MOZ_ASSERT(HTMLEditUtils::IsListItem(mEmptyInclusiveAncestorBlockElement));

  // If the found empty block is a list item element and its grand parent
  // (i.e., parent of list element) is NOT a list element, insert <br>
  // element before the list element which has the empty list item.
  // This odd list structure may occur if `Document.execCommand("indent")`
  // is performed for list items.
  // XXX Chrome does not remove empty list elements when last content in
  //     last list item is deleted.  We should follow it since current
  //     behavior is annoying when you type new list item with selecting
  //     all list items.
  if (!HTMLEditUtils::IsFirstChild(*mEmptyInclusiveAncestorBlockElement,
                                   {WalkTreeOption::IgnoreNonEditableNode})) {
    return RefPtr<Element>();
  }

  EditorDOMPoint atParentOfEmptyListItem(
      mEmptyInclusiveAncestorBlockElement->GetParentElement());
  if (NS_WARN_IF(!atParentOfEmptyListItem.IsSet())) {
    return Err(NS_ERROR_FAILURE);
  }
  if (HTMLEditUtils::IsAnyListElement(atParentOfEmptyListItem.GetContainer())) {
    return RefPtr<Element>();
  }
  Result<RefPtr<Element>, nsresult> resultOfInsertingBRElement =
      aHTMLEditor.InsertBRElementWithTransaction(atParentOfEmptyListItem);
  NS_WARNING_ASSERTION(resultOfInsertingBRElement.isOk(),
                       "HTMLEditor::InsertBRElementWithTransaction() failed");
  MOZ_ASSERT_IF(resultOfInsertingBRElement.isOk(),
                resultOfInsertingBRElement.inspect());
  return resultOfInsertingBRElement;
}

Result<EditorDOMPoint, nsresult> HTMLEditor::AutoDeleteRangesHandler::
    AutoEmptyBlockAncestorDeleter::GetNewCaretPoisition(
        const HTMLEditor& aHTMLEditor,
        nsIEditor::EDirection aDirectionAndAmount) const {
  MOZ_ASSERT(mEmptyInclusiveAncestorBlockElement);
  MOZ_ASSERT(mEmptyInclusiveAncestorBlockElement->GetParentElement());
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());

  switch (aDirectionAndAmount) {
    case nsIEditor::eNext:
    case nsIEditor::eNextWord:
    case nsIEditor::eToEndOfLine: {
      // Collapse Selection to next node of after empty block element
      // if there is.  Otherwise, to just after the empty block.
      EditorDOMPoint afterEmptyBlock(
          EditorRawDOMPoint::After(mEmptyInclusiveAncestorBlockElement));
      MOZ_ASSERT(afterEmptyBlock.IsSet());
      if (nsIContent* nextContentOfEmptyBlock = HTMLEditUtils::GetNextContent(
              afterEmptyBlock, {}, aHTMLEditor.GetActiveEditingHost())) {
        EditorDOMPoint pt = HTMLEditUtils::GetGoodCaretPointFor<EditorDOMPoint>(
            *nextContentOfEmptyBlock, aDirectionAndAmount);
        if (!pt.IsSet()) {
          NS_WARNING("HTMLEditUtils::GetGoodCaretPointFor() failed");
          return Err(NS_ERROR_FAILURE);
        }
        return pt;
      }
      if (NS_WARN_IF(!afterEmptyBlock.IsSet())) {
        return Err(NS_ERROR_FAILURE);
      }
      return afterEmptyBlock;
    }
    case nsIEditor::ePrevious:
    case nsIEditor::ePreviousWord:
    case nsIEditor::eToBeginningOfLine: {
      // Collapse Selection to previous editable node of the empty block
      // if there is.  Otherwise, to after the empty block.
      EditorRawDOMPoint atEmptyBlock(mEmptyInclusiveAncestorBlockElement);
      if (nsIContent* previousContentOfEmptyBlock =
              HTMLEditUtils::GetPreviousContent(
                  atEmptyBlock, {WalkTreeOption::IgnoreNonEditableNode},
                  aHTMLEditor.GetActiveEditingHost())) {
        EditorDOMPoint pt = HTMLEditUtils::GetGoodCaretPointFor<EditorDOMPoint>(
            *previousContentOfEmptyBlock, aDirectionAndAmount);
        if (!pt.IsSet()) {
          NS_WARNING("HTMLEditUtils::GetGoodCaretPointFor() failed");
          return Err(NS_ERROR_FAILURE);
        }
        return pt;
      }
      EditorDOMPoint afterEmptyBlock(
          EditorRawDOMPoint::After(*mEmptyInclusiveAncestorBlockElement));
      if (NS_WARN_IF(!afterEmptyBlock.IsSet())) {
        return Err(NS_ERROR_FAILURE);
      }
      return afterEmptyBlock;
    }
    case nsIEditor::eNone:
      return EditorDOMPoint();
    default:
      MOZ_CRASH(
          "AutoEmptyBlockAncestorDeleter doesn't support this action yet");
      return EditorDOMPoint();
  }
}

EditActionResult
HTMLEditor::AutoDeleteRangesHandler::AutoEmptyBlockAncestorDeleter::Run(
    HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount) {
  MOZ_ASSERT(mEmptyInclusiveAncestorBlockElement);
  MOZ_ASSERT(mEmptyInclusiveAncestorBlockElement->GetParentElement());
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());

  if (HTMLEditUtils::IsListItem(mEmptyInclusiveAncestorBlockElement)) {
    Result<RefPtr<Element>, nsresult> result =
        MaybeInsertBRElementBeforeEmptyListItemElement(aHTMLEditor);
    if (result.isErr()) {
      NS_WARNING(
          "AutoEmptyBlockAncestorDeleter::"
          "MaybeInsertBRElementBeforeEmptyListItemElement() failed");
      return EditActionResult(result.inspectErr());
    }
    // If a `<br>` element is inserted, caret should be moved to after it.
    if (RefPtr<Element> brElement = result.unwrap()) {
      nsresult rv =
          aHTMLEditor.CollapseSelectionTo(EditorRawDOMPoint(brElement));
      if (NS_FAILED(rv)) {
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "HTMLEditor::CollapseSelectionTo() failed");
        return EditActionResult(rv);
      }
    }
  } else {
    Result<EditorDOMPoint, nsresult> result =
        GetNewCaretPoisition(aHTMLEditor, aDirectionAndAmount);
    if (result.isErr()) {
      NS_WARNING(
          "AutoEmptyBlockAncestorDeleter::GetNewCaretPoisition() failed");
      return EditActionResult(result.inspectErr());
    }
    if (result.inspect().IsSet()) {
      nsresult rv = aHTMLEditor.CollapseSelectionTo(result.inspect());
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::CollapseSelectionTo() failed");
        return EditActionResult(rv);
      }
    }
  }
  nsresult rv = aHTMLEditor.DeleteNodeWithTransaction(
      MOZ_KnownLive(*mEmptyInclusiveAncestorBlockElement));
  if (NS_WARN_IF(aHTMLEditor.Destroyed())) {
    return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
  }
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
    return EditActionResult(rv);
  }
  return EditActionHandled();
}

bool HTMLEditor::AutoDeleteRangesHandler::ExtendRangeToIncludeInvisibleNodes(
    const HTMLEditor& aHTMLEditor, const nsFrameSelection* aFrameSelection,
    nsRange& aRange) const {
  MOZ_ASSERT(aHTMLEditor.IsEditActionDataAvailable());
  MOZ_ASSERT(!aRange.Collapsed());
  MOZ_ASSERT(aRange.IsPositioned());
  MOZ_ASSERT(!aRange.IsInSelection());

  EditorRawDOMPoint atStart(aRange.StartRef());
  EditorRawDOMPoint atEnd(aRange.EndRef());

  if (NS_WARN_IF(!aRange.GetClosestCommonInclusiveAncestor()->IsContent())) {
    return false;
  }

  // Find current selection common block parent
  Element* commonAncestorBlock =
      HTMLEditUtils::GetInclusiveAncestorBlockElement(
          *aRange.GetClosestCommonInclusiveAncestor()->AsContent());
  if (NS_WARN_IF(!commonAncestorBlock)) {
    return false;
  }

  // Set up for loops and cache our root element
  RefPtr<Element> editingHost = aHTMLEditor.GetActiveEditingHost();
  if (NS_WARN_IF(!editingHost)) {
    return false;
  }

  // Find previous visible things before start of selection
  if (atStart.GetContainer() != commonAncestorBlock &&
      atStart.GetContainer() != editingHost) {
    for (;;) {
      WSScanResult backwardScanFromStartResult =
          WSRunScanner::ScanPreviousVisibleNodeOrBlockBoundary(editingHost,
                                                               atStart);
      if (!backwardScanFromStartResult.ReachedCurrentBlockBoundary()) {
        break;
      }
      MOZ_ASSERT(backwardScanFromStartResult.GetContent() ==
                 WSRunScanner(editingHost, atStart).GetStartReasonContent());
      // We want to keep looking up.  But stop if we are crossing table
      // element boundaries, or if we hit the root.
      if (HTMLEditUtils::IsAnyTableElement(
              backwardScanFromStartResult.GetContent()) ||
          backwardScanFromStartResult.GetContent() == commonAncestorBlock ||
          backwardScanFromStartResult.GetContent() == editingHost) {
        break;
      }
      atStart = backwardScanFromStartResult.PointAtContent();
    }
    if (aFrameSelection &&
        !aFrameSelection->IsValidSelectionPoint(atStart.GetContainer())) {
      NS_WARNING("Computed start container was out of selection limiter");
      return false;
    }
  }

  // Expand selection endpoint only if we don't pass an invisible `<br>`, or if
  // we really needed to pass that `<br>` (i.e., its block is now totally
  // selected).

  // Find next visible things after end of selection
  if (atEnd.GetContainer() != commonAncestorBlock &&
      atEnd.GetContainer() != editingHost) {
    EditorDOMPoint atFirstInvisibleBRElement;
    for (;;) {
      WSRunScanner wsScannerAtEnd(editingHost, atEnd);
      WSScanResult forwardScanFromEndResult =
          wsScannerAtEnd.ScanNextVisibleNodeOrBlockBoundaryFrom(atEnd);
      if (forwardScanFromEndResult.ReachedBRElement()) {
        // XXX In my understanding, this is odd.  The end reason may not be
        //     same as the reached <br> element because the equality is
        //     guaranteed only when ReachedCurrentBlockBoundary() returns true.
        //     However, looks like that this code assumes that
        //     GetEndReasonContent() returns the (or a) <br> element.
        NS_ASSERTION(wsScannerAtEnd.GetEndReasonContent() ==
                         forwardScanFromEndResult.BRElementPtr(),
                     "End reason is not the reached <br> element");
        if (HTMLEditUtils::IsVisibleBRElement(
                *wsScannerAtEnd.GetEndReasonContent(), editingHost)) {
          break;
        }
        if (!atFirstInvisibleBRElement.IsSet()) {
          atFirstInvisibleBRElement = atEnd;
        }
        atEnd.SetAfter(wsScannerAtEnd.GetEndReasonContent());
        continue;
      }

      if (forwardScanFromEndResult.ReachedCurrentBlockBoundary()) {
        MOZ_ASSERT(forwardScanFromEndResult.GetContent() ==
                   wsScannerAtEnd.GetEndReasonContent());
        // We want to keep looking up.  But stop if we are crossing table
        // element boundaries, or if we hit the root.
        if (HTMLEditUtils::IsAnyTableElement(
                forwardScanFromEndResult.GetContent()) ||
            forwardScanFromEndResult.GetContent() == commonAncestorBlock ||
            forwardScanFromEndResult.GetContent() == editingHost) {
          break;
        }
        atEnd = forwardScanFromEndResult.PointAfterContent();
        continue;
      }

      break;
    }

    if (aFrameSelection &&
        !aFrameSelection->IsValidSelectionPoint(atEnd.GetContainer())) {
      NS_WARNING("Computed end container was out of selection limiter");
      return false;
    }

    if (atFirstInvisibleBRElement.IsInContentNode()) {
      // Find block node containing invisible `<br>` element.
      if (RefPtr<Element> brElementParent =
              HTMLEditUtils::GetInclusiveAncestorBlockElement(
                  *atFirstInvisibleBRElement.ContainerAsContent())) {
        EditorRawDOMRange range(atStart, atEnd);
        if (range.Contains(EditorRawDOMPoint(brElementParent))) {
          nsresult rv = aRange.SetStartAndEnd(atStart.ToRawRangeBoundary(),
                                              atEnd.ToRawRangeBoundary());
          if (NS_FAILED(rv)) {
            NS_WARNING("nsRange::SetStartAndEnd() failed to extend the range");
            return false;
          }
          return aRange.IsPositioned() && aRange.StartRef().IsSet() &&
                 aRange.EndRef().IsSet();
        }
        // Otherwise, the new range should end at the invisible `<br>`.
        if (aFrameSelection && !aFrameSelection->IsValidSelectionPoint(
                                   atFirstInvisibleBRElement.GetContainer())) {
          NS_WARNING(
              "Computed end container (`<br>` element) was out of selection "
              "limiter");
          return false;
        }
        atEnd = atFirstInvisibleBRElement;
      }
    }
  }

  // XXX This is unnecessary creation cost for us since we just want to return
  //     the start point and the end point.
  nsresult rv = aRange.SetStartAndEnd(atStart.ToRawRangeBoundary(),
                                      atEnd.ToRawRangeBoundary());
  if (NS_FAILED(rv)) {
    NS_WARNING("nsRange::SetStartAndEnd() failed to extend the range");
    return false;
  }
  return aRange.IsPositioned() && aRange.StartRef().IsSet() &&
         aRange.EndRef().IsSet();
}

}  // namespace mozilla
