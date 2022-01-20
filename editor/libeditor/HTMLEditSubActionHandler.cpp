/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLEditor.h"

#include <algorithm>
#include <utility>

#include "CSSEditUtils.h"
#include "EditAction.h"
#include "EditorDOMPoint.h"
#include "EditorUtils.h"
#include "HTMLEditHelpers.h"
#include "HTMLEditUtils.h"
#include "TypeInState.h"  // for SpecifiedStyle
#include "WSRunObject.h"

#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/ContentIterator.h"
#include "mozilla/InternalMutationEvent.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/Preferences.h"
#include "mozilla/RangeUtils.h"
#include "mozilla/StaticPrefs_editor.h"  // for StaticPrefs::editor_*
#include "mozilla/TextComposition.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/AncestorIterator.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLBRElement.h"
#include "mozilla/dom/RangeBinding.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/StaticRange.h"
#include "mozilla/mozalloc.h"
#include "nsAString.h"
#include "nsAlgorithm.h"
#include "nsAtom.h"
#include "nsCRT.h"
#include "nsCRTGlue.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsFrameSelection.h"
#include "nsGkAtoms.h"
#include "nsHTMLDocument.h"
#include "nsIContent.h"
#include "nsID.h"
#include "nsIFrame.h"
#include "nsINode.h"
#include "nsLiteralString.h"
#include "nsPrintfCString.h"
#include "nsRange.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsStyledElement.h"
#include "nsTArray.h"
#include "nsTextNode.h"
#include "nsThreadUtils.h"
#include "nsUnicharUtils.h"

// Workaround for windows headers
#ifdef SetProp
#  undef SetProp
#endif

class nsISupports;

namespace mozilla {

using namespace dom;
using EmptyCheckOption = HTMLEditUtils::EmptyCheckOption;
using LeafNodeType = HTMLEditUtils::LeafNodeType;
using LeafNodeTypes = HTMLEditUtils::LeafNodeTypes;
using StyleDifference = HTMLEditUtils::StyleDifference;
using WalkTextOption = HTMLEditUtils::WalkTextOption;
using WalkTreeDirection = HTMLEditUtils::WalkTreeDirection;
using WalkTreeOption = HTMLEditUtils::WalkTreeOption;

/********************************************************
 *  first some helpful functors we will use
 ********************************************************/

static bool IsStyleCachePreservingSubAction(EditSubAction aEditSubAction) {
  switch (aEditSubAction) {
    case EditSubAction::eDeleteSelectedContent:
    case EditSubAction::eInsertLineBreak:
    case EditSubAction::eInsertParagraphSeparator:
    case EditSubAction::eCreateOrChangeList:
    case EditSubAction::eIndent:
    case EditSubAction::eOutdent:
    case EditSubAction::eSetOrClearAlignment:
    case EditSubAction::eCreateOrRemoveBlock:
    case EditSubAction::eMergeBlockContents:
    case EditSubAction::eRemoveList:
    case EditSubAction::eCreateOrChangeDefinitionListItem:
    case EditSubAction::eInsertElement:
    case EditSubAction::eInsertQuotation:
    case EditSubAction::eInsertQuotedText:
      return true;
    default:
      return false;
  }
}

template void HTMLEditor::SelectBRElementIfCollapsedInEmptyBlock(
    RangeBoundary& aStartRef, RangeBoundary& aEndRef) const;
template void HTMLEditor::SelectBRElementIfCollapsedInEmptyBlock(
    RawRangeBoundary& aStartRef, RangeBoundary& aEndRef) const;
template void HTMLEditor::SelectBRElementIfCollapsedInEmptyBlock(
    RangeBoundary& aStartRef, RawRangeBoundary& aEndRef) const;
template void HTMLEditor::SelectBRElementIfCollapsedInEmptyBlock(
    RawRangeBoundary& aStartRef, RawRangeBoundary& aEndRef) const;
template already_AddRefed<nsRange>
HTMLEditor::CreateRangeIncludingAdjuscentWhiteSpaces(
    const RangeBoundary& aStartRef, const RangeBoundary& aEndRef);
template already_AddRefed<nsRange>
HTMLEditor::CreateRangeIncludingAdjuscentWhiteSpaces(
    const RawRangeBoundary& aStartRef, const RangeBoundary& aEndRef);
template already_AddRefed<nsRange>
HTMLEditor::CreateRangeIncludingAdjuscentWhiteSpaces(
    const RangeBoundary& aStartRef, const RawRangeBoundary& aEndRef);
template already_AddRefed<nsRange>
HTMLEditor::CreateRangeIncludingAdjuscentWhiteSpaces(
    const RawRangeBoundary& aStartRef, const RawRangeBoundary& aEndRef);
template already_AddRefed<nsRange>
HTMLEditor::CreateRangeExtendedToHardLineStartAndEnd(
    const RangeBoundary& aStartRef, const RangeBoundary& aEndRef,
    EditSubAction aEditSubAction) const;
template already_AddRefed<nsRange>
HTMLEditor::CreateRangeExtendedToHardLineStartAndEnd(
    const RawRangeBoundary& aStartRef, const RangeBoundary& aEndRef,
    EditSubAction aEditSubAction) const;
template already_AddRefed<nsRange>
HTMLEditor::CreateRangeExtendedToHardLineStartAndEnd(
    const RangeBoundary& aStartRef, const RawRangeBoundary& aEndRef,
    EditSubAction aEditSubAction) const;
template already_AddRefed<nsRange>
HTMLEditor::CreateRangeExtendedToHardLineStartAndEnd(
    const RawRangeBoundary& aStartRef, const RawRangeBoundary& aEndRef,
    EditSubAction aEditSubAction) const;
template EditorDOMPoint HTMLEditor::GetCurrentHardLineStartPoint(
    const RangeBoundary& aPoint, EditSubAction aEditSubAction) const;
template EditorDOMPoint HTMLEditor::GetCurrentHardLineStartPoint(
    const RawRangeBoundary& aPoint, EditSubAction aEditSubAction) const;
template EditorDOMPoint HTMLEditor::GetCurrentHardLineEndPoint(
    const RangeBoundary& aPoint) const;
template EditorDOMPoint HTMLEditor::GetCurrentHardLineEndPoint(
    const RawRangeBoundary& aPoint) const;

nsresult HTMLEditor::InitEditorContentAndSelection() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  nsresult rv = EditorBase::InitEditorContentAndSelection();
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::InitEditorContentAndSelection() failed");
    return rv;
  }

  Element* bodyOrDocumentElement = GetRoot();
  if (NS_WARN_IF(!bodyOrDocumentElement && !GetDocument())) {
    return NS_ERROR_FAILURE;
  }

  if (!bodyOrDocumentElement) {
    return NS_OK;
  }

  rv = InsertBRElementToEmptyListItemsAndTableCellsInRange(
      RawRangeBoundary(bodyOrDocumentElement, 0u),
      RawRangeBoundary(bodyOrDocumentElement,
                       bodyOrDocumentElement->GetChildCount()));
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::InsertBRElementToEmptyListItemsAndTableCellsInRange() "
      "failed, but ignored");
  return NS_OK;
}

void HTMLEditor::OnStartToHandleTopLevelEditSubAction(
    EditSubAction aTopLevelEditSubAction,
    nsIEditor::EDirection aDirectionOfTopLevelEditSubAction, ErrorResult& aRv) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!aRv.Failed());

  EditorBase::OnStartToHandleTopLevelEditSubAction(
      aTopLevelEditSubAction, aDirectionOfTopLevelEditSubAction, aRv);

  MOZ_ASSERT(GetTopLevelEditSubAction() == aTopLevelEditSubAction);
  MOZ_ASSERT(GetDirectionOfTopLevelEditSubAction() ==
             aDirectionOfTopLevelEditSubAction);

  if (NS_WARN_IF(Destroyed())) {
    aRv.Throw(NS_ERROR_EDITOR_DESTROYED);
    return;
  }

  if (!mInitSucceeded) {
    return;  // We should do nothing if we're being initialized.
  }

  NS_WARNING_ASSERTION(
      !aRv.Failed(),
      "EditorBase::OnStartToHandleTopLevelEditSubAction() failed");

  // Remember where our selection was before edit action took place:
  if (GetCompositionStartPoint().IsSet()) {
    // If there is composition string, let's remember current composition
    // range.
    TopLevelEditSubActionDataRef().mSelectedRange->StoreRange(
        GetCompositionStartPoint(), GetCompositionEndPoint());
  } else {
    // Get the selection location
    // XXX This may occur so that I think that we shouldn't throw exception
    //     in this case.
    if (NS_WARN_IF(!SelectionRef().RangeCount())) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return;
    }
    if (const nsRange* range = SelectionRef().GetRangeAt(0)) {
      TopLevelEditSubActionDataRef().mSelectedRange->StoreRange(*range);
    }
  }

  // Register with range updater to track this as we perturb the doc
  RangeUpdaterRef().RegisterRangeItem(
      *TopLevelEditSubActionDataRef().mSelectedRange);

  // Remember current inline styles for deletion and normal insertion ops
  bool cacheInlineStyles;
  switch (aTopLevelEditSubAction) {
    case EditSubAction::eInsertText:
    case EditSubAction::eInsertTextComingFromIME:
    case EditSubAction::eDeleteSelectedContent:
      cacheInlineStyles = true;
      break;
    default:
      cacheInlineStyles =
          IsStyleCachePreservingSubAction(aTopLevelEditSubAction);
      break;
  }
  if (cacheInlineStyles) {
    nsCOMPtr<nsIContent> containerContent = nsIContent::FromNodeOrNull(
        aDirectionOfTopLevelEditSubAction == nsIEditor::eNext
            ? TopLevelEditSubActionDataRef().mSelectedRange->mEndContainer
            : TopLevelEditSubActionDataRef().mSelectedRange->mStartContainer);
    if (NS_WARN_IF(!containerContent)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
    nsresult rv = CacheInlineStyles(*containerContent);
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::CacheInlineStyles() failed");
      aRv.Throw(rv);
      return;
    }
  }

  // Stabilize the document against contenteditable count changes
  Document* document = GetDocument();
  if (NS_WARN_IF(!document)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
  if (document->GetEditingState() == Document::EditingState::eContentEditable) {
    document->ChangeContentEditableCount(nullptr, +1);
    TopLevelEditSubActionDataRef().mRestoreContentEditableCount = true;
  }

  // Check that selection is in subtree defined by body node
  nsresult rv = EnsureSelectionInBodyOrDocumentElement();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    aRv.Throw(NS_ERROR_EDITOR_DESTROYED);
    return;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::EnsureSelectionInBodyOrDocumentElement() "
                       "failed, but ignored");
}

nsresult HTMLEditor::OnEndHandlingTopLevelEditSubAction() {
  MOZ_ASSERT(IsTopLevelEditSubActionDataAvailable());

  nsresult rv;
  while (true) {
    if (NS_WARN_IF(Destroyed())) {
      rv = NS_ERROR_EDITOR_DESTROYED;
      break;
    }

    if (!mInitSucceeded) {
      rv = NS_OK;  // We should do nothing if we're being initialized.
      break;
    }

    // Do all the tricky stuff
    rv = OnEndHandlingTopLevelEditSubActionInternal();
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::OnEndHandlingTopLevelEditSubActionInternal() failied");
    // Perhaps, we need to do the following jobs even if the editor has been
    // destroyed since they adjust some states of HTML document but don't
    // modify the DOM tree nor Selection.

    // Free up selectionState range item
    if (TopLevelEditSubActionDataRef().mSelectedRange) {
      RangeUpdaterRef().DropRangeItem(
          *TopLevelEditSubActionDataRef().mSelectedRange);
    }

    // Reset the contenteditable count to its previous value
    if (TopLevelEditSubActionDataRef().mRestoreContentEditableCount) {
      Document* document = GetDocument();
      if (NS_WARN_IF(!document)) {
        rv = NS_ERROR_FAILURE;
        break;
      }
      if (document->GetEditingState() ==
          Document::EditingState::eContentEditable) {
        document->ChangeContentEditableCount(nullptr, -1);
      }
    }
    break;
  }
  DebugOnly<nsresult> rvIgnored =
      EditorBase::OnEndHandlingTopLevelEditSubAction();
  NS_WARNING_ASSERTION(
      NS_FAILED(rv) || NS_SUCCEEDED(rvIgnored),
      "EditorBase::OnEndHandlingTopLevelEditSubAction() failed, but ignored");
  MOZ_ASSERT(!GetTopLevelEditSubAction());
  MOZ_ASSERT(GetDirectionOfTopLevelEditSubAction() == eNone);
  return rv;
}

nsresult HTMLEditor::OnEndHandlingTopLevelEditSubActionInternal() {
  MOZ_ASSERT(IsTopLevelEditSubActionDataAvailable());

  nsresult rv = EnsureSelectionInBodyOrDocumentElement();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::EnsureSelectionInBodyOrDocumentElement() "
                       "failed, but ignored");

  switch (GetTopLevelEditSubAction()) {
    case EditSubAction::eReplaceHeadWithHTMLSource:
    case EditSubAction::eCreatePaddingBRElementForEmptyEditor:
      return NS_OK;
    default:
      break;
  }

  if (TopLevelEditSubActionDataRef().mChangedRange->IsPositioned() &&
      GetTopLevelEditSubAction() != EditSubAction::eUndo &&
      GetTopLevelEditSubAction() != EditSubAction::eRedo) {
    // don't let any txns in here move the selection around behind our back.
    // Note that this won't prevent explicit selection setting from working.
    AutoTransactionsConserveSelection dontChangeMySelection(*this);

    switch (GetTopLevelEditSubAction()) {
      case EditSubAction::eInsertText:
      case EditSubAction::eInsertTextComingFromIME:
      case EditSubAction::eInsertLineBreak:
      case EditSubAction::eInsertParagraphSeparator:
      case EditSubAction::eDeleteText: {
        // XXX We should investigate whether this is really needed because it
        //     seems that the following code does not handle the white-spaces.
        RefPtr<nsRange> extendedChangedRange =
            CreateRangeIncludingAdjuscentWhiteSpaces(
                *TopLevelEditSubActionDataRef().mChangedRange);
        if (extendedChangedRange) {
          MOZ_ASSERT(extendedChangedRange->IsPositioned());
          // Use extended range temporarily.
          TopLevelEditSubActionDataRef().mChangedRange =
              std::move(extendedChangedRange);
        }
        break;
      }
      default: {
        RefPtr<nsRange> extendedChangedRange =
            CreateRangeExtendedToHardLineStartAndEnd(
                *TopLevelEditSubActionDataRef().mChangedRange,
                GetTopLevelEditSubAction());
        if (extendedChangedRange) {
          MOZ_ASSERT(extendedChangedRange->IsPositioned());
          // Use extended range temporarily.
          TopLevelEditSubActionDataRef().mChangedRange =
              std::move(extendedChangedRange);
        }
        break;
      }
    }

    // if we did a ranged deletion or handling backspace key, make sure we have
    // a place to put caret.
    // Note we only want to do this if the overall operation was deletion,
    // not if deletion was done along the way for
    // EditSubAction::eInsertHTMLSource, EditSubAction::eInsertText, etc.
    // That's why this is here rather than DeleteSelectionAsSubAction().
    // However, we shouldn't insert <br> elements if we've already removed
    // empty block parents because users may want to disappear the line by
    // the deletion.
    // XXX We should make HandleDeleteSelection() store expected container
    //     for handling this here since we cannot trust current selection is
    //     collapsed at deleted point.
    if (GetTopLevelEditSubAction() == EditSubAction::eDeleteSelectedContent &&
        TopLevelEditSubActionDataRef().mDidDeleteNonCollapsedRange &&
        !TopLevelEditSubActionDataRef().mDidDeleteEmptyParentBlocks) {
      EditorDOMPoint newCaretPosition =
          EditorBase::GetStartPoint(SelectionRef());
      if (!newCaretPosition.IsSet()) {
        NS_WARNING("There was no selection range");
        return NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE;
      }
      nsresult rv = InsertBRElementIfHardLineIsEmptyAndEndsWithBlockBoundary(
          newCaretPosition);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "HTMLEditor::"
            "InsertBRElementIfHardLineIsEmptyAndEndsWithBlockBoundary() "
            "failed");
        return rv;
      }
    }

    // add in any needed <br>s, and remove any unneeded ones.
    nsresult rv = InsertBRElementToEmptyListItemsAndTableCellsInRange(
        TopLevelEditSubActionDataRef().mChangedRange->StartRef().AsRaw(),
        TopLevelEditSubActionDataRef().mChangedRange->EndRef().AsRaw());
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::InsertBRElementToEmptyListItemsAndTableCellsInRange()"
        " failed, but ignored");

    // merge any adjacent text nodes
    switch (GetTopLevelEditSubAction()) {
      case EditSubAction::eInsertText:
      case EditSubAction::eInsertTextComingFromIME:
        break;
      default: {
        nsresult rv = CollapseAdjacentTextNodes(
            MOZ_KnownLive(*TopLevelEditSubActionDataRef().mChangedRange));
        if (NS_WARN_IF(Destroyed())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_FAILED(rv)) {
          NS_WARNING("HTMLEditor::CollapseAdjacentTextNodes() failed");
          return rv;
        }
        break;
      }
    }

    // clean up any empty nodes in the selection
    rv = RemoveEmptyNodesIn(
        MOZ_KnownLive(*TopLevelEditSubActionDataRef().mChangedRange));
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::RemoveEmptyNodesIn() failed");
      return rv;
    }

    // attempt to transform any unneeded nbsp's into spaces after doing various
    // operations
    switch (GetTopLevelEditSubAction()) {
      case EditSubAction::eDeleteSelectedContent:
        if (TopLevelEditSubActionDataRef().mDidNormalizeWhitespaces) {
          break;
        }
        [[fallthrough]];
      case EditSubAction::eInsertText:
      case EditSubAction::eInsertTextComingFromIME:
      case EditSubAction::eInsertLineBreak:
      case EditSubAction::eInsertParagraphSeparator:
      case EditSubAction::ePasteHTMLContent:
      case EditSubAction::eInsertHTMLSource: {
        // TODO: Temporarily, WhiteSpaceVisibilityKeeper replaces ASCII
        //       white-spaces with NPSPs and then, we'll replace them with ASCII
        //       white-spaces here.  We should avoid this overwriting things as
        //       far as possible because replacing characters in text nodes
        //       causes running mutation event listeners which are really
        //       expensive.
        // Adjust end of composition string if there is composition string.
        EditorDOMPoint pointToAdjust(GetCompositionEndPoint());
        if (!pointToAdjust.IsInContentNode()) {
          // Otherwise, adjust current selection start point.
          pointToAdjust = EditorBase::GetStartPoint(SelectionRef());
          if (NS_WARN_IF(!pointToAdjust.IsInContentNode())) {
            return NS_ERROR_FAILURE;
          }
        }
        if (EditorUtils::IsEditableContent(*pointToAdjust.ContainerAsContent(),
                                           EditorType::HTML)) {
          AutoTrackDOMPoint trackPointToAdjust(RangeUpdaterRef(),
                                               &pointToAdjust);
          nsresult rv =
              WhiteSpaceVisibilityKeeper::NormalizeVisibleWhiteSpacesAt(
                  *this, pointToAdjust);
          if (NS_FAILED(rv)) {
            NS_WARNING(
                "WhiteSpaceVisibilityKeeper::NormalizeVisibleWhiteSpacesAt() "
                "failed");
            return rv;
          }
        }

        // also do this for original selection endpoints.
        // XXX Hmm, if `NormalizeVisibleWhiteSpacesAt()` runs mutation event
        //     listener and that causes changing `mSelectedRange`, what we
        //     should do?
        if (NS_WARN_IF(
                !TopLevelEditSubActionDataRef().mSelectedRange->IsSet())) {
          return NS_ERROR_FAILURE;
        }

        EditorDOMPoint atStart =
            TopLevelEditSubActionDataRef().mSelectedRange->StartPoint();
        if (atStart != pointToAdjust && atStart.IsInContentNode() &&
            EditorUtils::IsEditableContent(*atStart.ContainerAsContent(),
                                           EditorType::HTML)) {
          AutoTrackDOMPoint trackPointToAdjust(RangeUpdaterRef(),
                                               &pointToAdjust);
          AutoTrackDOMPoint trackStartPoint(RangeUpdaterRef(), &atStart);
          nsresult rv =
              WhiteSpaceVisibilityKeeper::NormalizeVisibleWhiteSpacesAt(
                  *this, atStart);
          if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
            return NS_ERROR_EDITOR_DESTROYED;
          }
          NS_WARNING_ASSERTION(
              NS_SUCCEEDED(rv),
              "WhiteSpaceVisibilityKeeper::NormalizeVisibleWhiteSpacesAt() "
              "failed, but ignored");
        }
        // we only need to handle old selection endpoint if it was different
        // from start
        EditorDOMPoint atEnd =
            TopLevelEditSubActionDataRef().mSelectedRange->EndPoint();
        if (!TopLevelEditSubActionDataRef().mSelectedRange->IsCollapsed() &&
            atEnd != pointToAdjust && atEnd != atStart &&
            atEnd.IsInContentNode() &&
            EditorUtils::IsEditableContent(*atEnd.ContainerAsContent(),
                                           EditorType::HTML)) {
          nsresult rv =
              WhiteSpaceVisibilityKeeper::NormalizeVisibleWhiteSpacesAt(*this,
                                                                        atEnd);
          if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
            return NS_ERROR_EDITOR_DESTROYED;
          }
          NS_WARNING_ASSERTION(
              NS_SUCCEEDED(rv),
              "WhiteSpaceVisibilityKeeper::NormalizeVisibleWhiteSpacesAt() "
              "failed, but ignored");
        }
        break;
      }
      default:
        break;
    }

    // If we created a new block, make sure caret is in it.
    if (TopLevelEditSubActionDataRef().mNewBlockElement &&
        SelectionRef().IsCollapsed()) {
      nsresult rv = EnsureCaretInBlockElement(
          MOZ_KnownLive(*TopLevelEditSubActionDataRef().mNewBlockElement));
      if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "HTMLEditor::EnsureSelectionInBlockElement() failed, but ignored");
    }

    // Adjust selection for insert text, html paste, and delete actions if
    // we haven't removed new empty blocks.  Note that if empty block parents
    // are removed, Selection should've been adjusted by the method which
    // did it.
    if (!TopLevelEditSubActionDataRef().mDidDeleteEmptyParentBlocks &&
        SelectionRef().IsCollapsed()) {
      switch (GetTopLevelEditSubAction()) {
        case EditSubAction::eInsertText:
        case EditSubAction::eInsertTextComingFromIME:
        case EditSubAction::eDeleteSelectedContent:
        case EditSubAction::eInsertLineBreak:
        case EditSubAction::eInsertParagraphSeparator:
        case EditSubAction::ePasteHTMLContent:
        case EditSubAction::eInsertHTMLSource:
          // XXX AdjustCaretPositionAndEnsurePaddingBRElement() intentionally
          //     does not create padding `<br>` element for empty editor.
          //     Investigate which is better that whether this should does it
          //     or wait MaybeCreatePaddingBRElementForEmptyEditor().
          rv = AdjustCaretPositionAndEnsurePaddingBRElement(
              GetDirectionOfTopLevelEditSubAction());
          if (NS_FAILED(rv)) {
            NS_WARNING(
                "HTMLEditor::AdjustCaretPositionAndEnsurePaddingBRElement() "
                "failed");
            return rv;
          }
          break;
        default:
          break;
      }
    }

    // check for any styles which were removed inappropriately
    bool reapplyCachedStyle;
    switch (GetTopLevelEditSubAction()) {
      case EditSubAction::eInsertText:
      case EditSubAction::eInsertTextComingFromIME:
      case EditSubAction::eDeleteSelectedContent:
        reapplyCachedStyle = true;
        break;
      default:
        reapplyCachedStyle =
            IsStyleCachePreservingSubAction(GetTopLevelEditSubAction());
        break;
    }

    // If the selection is in empty inline HTML elements, we should delete
    // them.
    if (mPlaceholderBatch && SelectionRef().IsCollapsed() &&
        SelectionRef().GetFocusNode()) {
      RefPtr<Element> mostDistantEmptyInlineAncestor = nullptr;
      for (Element* ancestor :
           SelectionRef().GetFocusNode()->InclusiveAncestorsOfType<Element>()) {
        if (!ancestor->IsHTMLElement() ||
            !HTMLEditUtils::IsRemovableFromParentNode(*ancestor) ||
            !HTMLEditUtils::IsEmptyInlineContent(*ancestor)) {
          break;
        }
        mostDistantEmptyInlineAncestor = ancestor;
      }
      if (mostDistantEmptyInlineAncestor) {
        nsresult rv =
            DeleteNodeWithTransaction(*mostDistantEmptyInlineAncestor);
        if (Destroyed()) {
          NS_WARNING(
              "HTMLEditor::DeleteNodeWithTransaction() caused destroying the "
              "editor at deleting empty inline ancestors");
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "HTMLEditor::DeleteNodeWithTransaction() failed at deleting "
              "empty inline ancestors");
          return rv;
        }
      }
    }

    // But the cached inline styles should be restored from type-in-state later.
    if (reapplyCachedStyle) {
      DebugOnly<nsresult> rvIgnored =
          mTypeInState->UpdateSelState(SelectionRef());
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                           "TypeInState::UpdateSelState() failed, but ignored");
      rvIgnored = ReapplyCachedStyles();
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "HTMLEditor::ReapplyCachedStyles() failed, but ignored");
      TopLevelEditSubActionDataRef().mCachedInlineStyles->Clear();
    }
  }

  rv = HandleInlineSpellCheck(
      TopLevelEditSubActionDataRef().mSelectedRange->StartPoint(),
      TopLevelEditSubActionDataRef().mChangedRange);
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::HandleInlineSpellCheck() failed");
    return rv;
  }

  // detect empty doc
  // XXX Need to investigate when the padding <br> element is removed because
  //     I don't see the <br> element with testing manually.  If it won't be
  //     used, we can get rid of this cost.
  rv = MaybeCreatePaddingBRElementForEmptyEditor();
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "EditorBase::MaybeCreatePaddingBRElementForEmptyEditor() failed");
    return rv;
  }

  // adjust selection HINT if needed
  if (!TopLevelEditSubActionDataRef().mDidExplicitlySetInterLine &&
      SelectionRef().IsCollapsed()) {
    SetSelectionInterlinePosition();
  }

  return NS_OK;
}

EditActionResult HTMLEditor::CanHandleHTMLEditSubAction() const {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(Destroyed())) {
    return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
  }

  // If there is not selection ranges, we should ignore the result.
  if (!SelectionRef().RangeCount()) {
    return EditActionCanceled();
  }

  const nsRange* range = SelectionRef().GetRangeAt(0);
  nsINode* selStartNode = range->GetStartContainer();
  if (NS_WARN_IF(!selStartNode) || NS_WARN_IF(!selStartNode->IsContent())) {
    return EditActionResult(NS_ERROR_FAILURE);
  }

  if (!HTMLEditUtils::IsSimplyEditableNode(*selStartNode) ||
      HTMLEditUtils::IsNonEditableReplacedContent(*selStartNode->AsContent())) {
    return EditActionCanceled();
  }

  nsINode* selEndNode = range->GetEndContainer();
  if (NS_WARN_IF(!selEndNode) || NS_WARN_IF(!selEndNode->IsContent())) {
    return EditActionResult(NS_ERROR_FAILURE);
  }

  if (selStartNode == selEndNode) {
    return EditActionIgnored();
  }

  if (!HTMLEditUtils::IsSimplyEditableNode(*selEndNode) ||
      HTMLEditUtils::IsNonEditableReplacedContent(*selEndNode->AsContent())) {
    return EditActionCanceled();
  }

  // XXX What does it mean the common ancestor is editable?  I have no idea.
  //     It should be in same (active) editing host, and even if it's editable,
  //     there may be non-editable contents in the range.
  nsINode* commonAncestor = range->GetClosestCommonInclusiveAncestor();
  if (!commonAncestor) {
    NS_WARNING(
        "AbstractRange::GetClosestCommonInclusiveAncestor() returned nullptr");
    return EditActionResult(NS_ERROR_FAILURE);
  }
  return HTMLEditUtils::IsSimplyEditableNode(*commonAncestor)
             ? EditActionIgnored()
             : EditActionCanceled();
}

MOZ_CAN_RUN_SCRIPT static nsStaticAtom& MarginPropertyAtomForIndent(
    nsIContent& aContent) {
  nsAutoString direction;
  DebugOnly<nsresult> rvIgnored = CSSEditUtils::GetComputedProperty(
      aContent, *nsGkAtoms::direction, direction);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "CSSEditUtils::GetComputedProperty(nsGkAtoms::direction)"
                       " failed, but ignored");
  return direction.EqualsLiteral("rtl") ? *nsGkAtoms::marginRight
                                        : *nsGkAtoms::marginLeft;
}

nsresult HTMLEditor::EnsureCaretNotAfterInvisibleBRElement() {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(SelectionRef().IsCollapsed());

  // If we are after a padding `<br>` element for empty last line in the same
  // block, then move selection to be before it
  const nsRange* firstRange = SelectionRef().GetRangeAt(0);
  if (NS_WARN_IF(!firstRange)) {
    return NS_ERROR_FAILURE;
  }

  EditorRawDOMPoint atSelectionStart(firstRange->StartRef());
  if (NS_WARN_IF(!atSelectionStart.IsSet())) {
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(atSelectionStart.IsSetAndValid());

  if (!atSelectionStart.IsInContentNode()) {
    return NS_OK;
  }

  Element* editingHost = GetActiveEditingHost();
  if (!editingHost) {
    NS_WARNING(
        "HTMLEditor::EnsureCaretNotAfterInvisibleBRElement() did nothing "
        "because of no editing host");
    return NS_OK;
  }

  nsIContent* previousBRElement =
      HTMLEditUtils::GetPreviousContent(atSelectionStart, {}, editingHost);
  if (!previousBRElement || !previousBRElement->IsHTMLElement(nsGkAtoms::br) ||
      !previousBRElement->GetParent() ||
      !EditorUtils::IsEditableContent(*previousBRElement->GetParent(),
                                      EditorType::HTML) ||
      !HTMLEditUtils::IsInvisibleBRElement(*previousBRElement)) {
    return NS_OK;
  }

  const RefPtr<const Element> blockElementAtSelectionStart =
      HTMLEditUtils::GetInclusiveAncestorElement(
          *atSelectionStart.ContainerAsContent(),
          HTMLEditUtils::ClosestBlockElement);
  const RefPtr<const Element> parentBlockElementOfBRElement =
      HTMLEditUtils::GetAncestorElement(*previousBRElement,
                                        HTMLEditUtils::ClosestBlockElement);

  if (!blockElementAtSelectionStart ||
      blockElementAtSelectionStart != parentBlockElementOfBRElement) {
    return NS_OK;
  }

  // If we are here then the selection is right after a padding <br>
  // element for empty last line that is in the same block as the
  // selection.  We need to move the selection start to be before the
  // padding <br> element.
  EditorRawDOMPoint atInvisibleBRElement(previousBRElement);
  nsresult rv = CollapseSelectionTo(atInvisibleBRElement);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::CollapseSelectionTo() failed");
  return rv;
}

nsresult HTMLEditor::MaybeCreatePaddingBRElementForEmptyEditor() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (mPaddingBRElementForEmptyEditor) {
    return NS_OK;
  }

  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eCreatePaddingBRElementForEmptyEditor,
      nsIEditor::eNone, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "TextEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");
  ignoredError.SuppressException();

  RefPtr<Element> rootElement = GetRoot();
  if (!rootElement) {
    return NS_OK;
  }

  // Now we've got the body element. Iterate over the body element's children,
  // looking for editable content. If no editable content is found, insert the
  // padding <br> element.
  EditorType editorType = GetEditorType();
  bool isRootEditable =
      EditorUtils::IsEditableContent(*rootElement, editorType);
  for (nsIContent* rootChild = rootElement->GetFirstChild(); rootChild;
       rootChild = rootChild->GetNextSibling()) {
    if (EditorUtils::IsPaddingBRElementForEmptyEditor(*rootChild) ||
        !isRootEditable ||
        EditorUtils::IsEditableContent(*rootChild, editorType) ||
        HTMLEditUtils::IsBlockElement(*rootChild)) {
      return NS_OK;
    }
  }

  // Skip adding the padding <br> element for empty editor if body
  // is read-only.
  if (IsHTMLEditor() && !HTMLEditUtils::IsSimplyEditableNode(*rootElement)) {
    return NS_OK;
  }

  // Create a br.
  RefPtr<Element> newBRElement = CreateHTMLContent(nsGkAtoms::br);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(!newBRElement)) {
    return NS_ERROR_FAILURE;
  }

  mPaddingBRElementForEmptyEditor =
      static_cast<HTMLBRElement*>(newBRElement.get());

  // Give it a special attribute.
  newBRElement->SetFlags(NS_PADDING_FOR_EMPTY_EDITOR);

  // Put the node in the document.
  nsresult rv =
      InsertNodeWithTransaction(*newBRElement, EditorDOMPoint(rootElement, 0));
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::InsertNodeWithTransaction() failed");
    return rv;
  }

  // Set selection.
  SelectionRef().CollapseInLimiter(EditorRawDOMPoint(rootElement, 0),
                                   ignoredError);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(!ignoredError.Failed(),
                       "Selection::CollapseInLimiter() failed, but ignored");
  return NS_OK;
}

nsresult HTMLEditor::EnsureNoPaddingBRElementForEmptyEditor() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (!mPaddingBRElementForEmptyEditor) {
    return NS_OK;
  }

  // If we're an HTML editor, a mutation event listener may recreate padding
  // <br> element for empty editor again during the call of
  // DeleteNodeWithTransaction().  So, move it first.
  RefPtr<HTMLBRElement> paddingBRElement(
      std::move(mPaddingBRElementForEmptyEditor));
  nsresult rv = DeleteNodeWithTransaction(*paddingBRElement);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::DeleteNodeWithTransaction() failed");
  return rv;
}

nsresult HTMLEditor::ReflectPaddingBRElementForEmptyEditor() {
  if (NS_WARN_IF(!mRootElement)) {
    NS_WARNING("Failed to handle padding BR element due to no root element");
    return NS_ERROR_FAILURE;
  }
  // The idea here is to see if the magic empty node has suddenly reappeared. If
  // it has, set our state so we remember it. There is a tradeoff between doing
  // here and at redo, or doing it everywhere else that might care.  Since undo
  // and redo are relatively rare, it makes sense to take the (small)
  // performance hit here.
  nsIContent* firstLeafChild = HTMLEditUtils::GetFirstLeafContent(
      *mRootElement, {LeafNodeType::OnlyLeafNode});
  if (firstLeafChild &&
      EditorUtils::IsPaddingBRElementForEmptyEditor(*firstLeafChild)) {
    mPaddingBRElementForEmptyEditor =
        static_cast<HTMLBRElement*>(firstLeafChild);
  } else {
    mPaddingBRElementForEmptyEditor = nullptr;
  }
  return NS_OK;
}

nsresult HTMLEditor::PrepareInlineStylesForCaret() {
  MOZ_ASSERT(IsTopLevelEditSubActionDataAvailable());
  MOZ_ASSERT(SelectionRef().IsCollapsed());

  // XXX This method works with the top level edit sub-action, but this
  //     must be wrong if we are handling nested edit action.

  if (TopLevelEditSubActionDataRef().mDidDeleteSelection) {
    switch (GetTopLevelEditSubAction()) {
      case EditSubAction::eInsertText:
      case EditSubAction::eInsertTextComingFromIME:
      case EditSubAction::eDeleteSelectedContent: {
        nsresult rv = ReapplyCachedStyles();
        if (NS_FAILED(rv)) {
          NS_WARNING("HTMLEditor::ReapplyCachedStyles() failed");
          return rv;
        }
        break;
      }
      default:
        break;
    }
  }
  // For most actions we want to clear the cached styles, but there are
  // exceptions
  if (!IsStyleCachePreservingSubAction(GetTopLevelEditSubAction())) {
    TopLevelEditSubActionDataRef().mCachedInlineStyles->Clear();
  }
  return NS_OK;
}

EditActionResult HTMLEditor::HandleInsertText(
    EditSubAction aEditSubAction, const nsAString& aInsertionString,
    SelectionHandling aSelectionHandling) {
  MOZ_ASSERT(IsTopLevelEditSubActionDataAvailable());
  MOZ_ASSERT(aEditSubAction == EditSubAction::eInsertText ||
             aEditSubAction == EditSubAction::eInsertTextComingFromIME);
  MOZ_ASSERT_IF(aSelectionHandling == SelectionHandling::Ignore,
                aEditSubAction == EditSubAction::eInsertTextComingFromIME);

  EditActionResult result = CanHandleHTMLEditSubAction();
  if (result.Failed() || result.Canceled()) {
    NS_WARNING_ASSERTION(result.Succeeded(),
                         "HTMLEditor::CanHandleHTMLEditSubAction() failed");
    return result;
  }

  UndefineCaretBidiLevel();

  // If the selection isn't collapsed, delete it.  Don't delete existing inline
  // tags, because we're hopefully going to insert text (bug 787432).
  if (!SelectionRef().IsCollapsed() &&
      aSelectionHandling == SelectionHandling::Delete) {
    nsresult rv =
        DeleteSelectionAsSubAction(nsIEditor::eNone, nsIEditor::eNoStrip);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "EditorBase::DeleteSelectionAsSubAction(nsIEditor::eNone, "
          "nsIEditor::eNoStrip) failed");
      return EditActionHandled(rv);
    }
  }

  nsresult rv = EnsureNoPaddingBRElementForEmptyEditor();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return EditActionHandled(NS_ERROR_EDITOR_DESTROYED);
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::EnsureNoPaddingBRElementForEmptyEditor() "
                       "failed, but ignored");

  if (NS_SUCCEEDED(rv) && SelectionRef().IsCollapsed()) {
    nsresult rv = EnsureCaretNotAfterInvisibleBRElement();
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return EditActionHandled(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::EnsureCaretNotAfterInvisibleBRElement() "
                         "failed, but ignored");
    if (NS_SUCCEEDED(rv)) {
      nsresult rv = PrepareInlineStylesForCaret();
      if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
        return EditActionHandled(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "HTMLEditor::PrepareInlineStylesForCaret() failed, but ignored");
    }
  }

  RefPtr<Document> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return EditActionHandled(NS_ERROR_FAILURE);
  }

  RefPtr<const nsRange> firstRange = SelectionRef().GetRangeAt(0);
  if (NS_WARN_IF(!firstRange)) {
    return EditActionHandled(NS_ERROR_FAILURE);
  }

  // for every property that is set, insert a new inline style node
  // XXX CreateStyleForInsertText() adjusts selection automatically, but
  //     it should just return the insertion point instead.
  rv = CreateStyleForInsertText(*firstRange);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::CreateStyleForInsertText() failed");
    return EditActionHandled(rv);
  }

  firstRange = SelectionRef().GetRangeAt(0);
  if (NS_WARN_IF(!firstRange)) {
    return EditActionHandled(NS_ERROR_FAILURE);
  }

  EditorDOMPoint pointToInsert(firstRange->StartRef());
  if (NS_WARN_IF(!pointToInsert.IsSet()) ||
      NS_WARN_IF(!pointToInsert.IsInContentNode())) {
    return EditActionHandled(NS_ERROR_FAILURE);
  }
  MOZ_ASSERT(pointToInsert.IsSetAndValid());

  // If the point is not in an element which can contain text nodes, climb up
  // the DOM tree.
  if (!pointToInsert.IsInTextNode()) {
    Element* editingHost = GetActiveEditingHost(GetDocument()->IsXMLDocument()
                                                    ? LimitInBodyElement::No
                                                    : LimitInBodyElement::Yes);
    if (NS_WARN_IF(!editingHost)) {
      return EditActionHandled(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
    }
    while (!HTMLEditUtils::CanNodeContain(*pointToInsert.GetContainer(),
                                          *nsGkAtoms::textTagName)) {
      if (NS_WARN_IF(pointToInsert.GetContainer() == editingHost) ||
          NS_WARN_IF(!pointToInsert.GetContainerParentAsContent())) {
        NS_WARNING("Selection start point couldn't have text nodes");
        return EditActionHandled(NS_ERROR_FAILURE);
      }
      pointToInsert.Set(pointToInsert.ContainerAsContent());
    }
  }

  if (aEditSubAction == EditSubAction::eInsertTextComingFromIME) {
    EditorRawDOMPoint compositionStartPoint = GetCompositionStartPoint();
    if (!compositionStartPoint.IsSet()) {
      compositionStartPoint = pointToInsert;
    }

    if (aInsertionString.IsEmpty()) {
      // Right now the WhiteSpaceVisibilityKeeper code bails on empty strings,
      // but IME needs the InsertTextWithTransaction() call to still happen
      // since empty strings are meaningful there.
      nsresult rv = InsertTextWithTransaction(*document, aInsertionString,
                                              compositionStartPoint);
      if (NS_WARN_IF(Destroyed())) {
        return EditActionHandled(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "HTMLEditor::InsertTextWithTransaction() failed");
      return EditActionHandled(rv);
    }

    EditorRawDOMPoint compositionEndPoint = GetCompositionEndPoint();
    if (!compositionEndPoint.IsSet()) {
      compositionEndPoint = compositionStartPoint;
    }
    nsresult rv = WhiteSpaceVisibilityKeeper::ReplaceText(
        *this, aInsertionString,
        EditorDOMRange(compositionStartPoint, compositionEndPoint));
    if (NS_FAILED(rv)) {
      NS_WARNING("WhiteSpaceVisibilityKeeper::ReplaceText() failed");
      return EditActionHandled(rv);
    }

    compositionStartPoint = GetCompositionStartPoint();
    compositionEndPoint = GetCompositionEndPoint();
    if (NS_WARN_IF(!compositionStartPoint.IsSet()) ||
        NS_WARN_IF(!compositionEndPoint.IsSet())) {
      // Mutation event listener has changed the DOM tree...
      return EditActionHandled();
    }
    rv = TopLevelEditSubActionDataRef().mChangedRange->SetStartAndEnd(
        compositionStartPoint.ToRawRangeBoundary(),
        compositionEndPoint.ToRawRangeBoundary());
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "nsRange::SetStartAndEnd() failed");
    return EditActionHandled(rv);
  }

  MOZ_ASSERT(aEditSubAction == EditSubAction::eInsertText);

  // find where we are
  EditorDOMPoint currentPoint(pointToInsert);

  // is our text going to be PREformatted?
  // We remember this so that we know how to handle tabs.
  const bool isWhiteSpaceCollapsible = !EditorUtils::IsWhiteSpacePreformatted(
      *pointToInsert.ContainerAsContent());

  // turn off the edit listener: we know how to
  // build the "doc changed range" ourselves, and it's
  // must faster to do it once here than to track all
  // the changes one at a time.
  AutoRestore<bool> disableListener(
      EditSubActionDataRef().mAdjustChangedRangeFromListener);
  EditSubActionDataRef().mAdjustChangedRangeFromListener = false;

  // don't change my selection in subtransactions
  AutoTransactionsConserveSelection dontChangeMySelection(*this);
  int32_t pos = 0;
  constexpr auto newlineStr = NS_LITERAL_STRING_FROM_CSTRING(LFSTR);

  {
    AutoTrackDOMPoint tracker(RangeUpdaterRef(), &pointToInsert);

    // for efficiency, break out the pre case separately.  This is because
    // its a lot cheaper to search the input string for only newlines than
    // it is to search for both tabs and newlines.
    if (!isWhiteSpaceCollapsible || IsInPlaintextMode()) {
      while (pos != -1 &&
             pos < AssertedCast<int32_t>(aInsertionString.Length())) {
        int32_t oldPos = pos;
        int32_t subStrLen;
        pos = aInsertionString.FindChar(nsCRT::LF, oldPos);

        if (pos != -1) {
          subStrLen = pos - oldPos;
          // if first char is newline, then use just it
          if (!subStrLen) {
            subStrLen = 1;
          }
        } else {
          subStrLen = aInsertionString.Length() - oldPos;
          pos = aInsertionString.Length();
        }

        nsDependentSubstring subStr(aInsertionString, oldPos, subStrLen);

        // is it a return?
        if (subStr.Equals(newlineStr)) {
          Result<RefPtr<Element>, nsresult> resultOfInsertingBRElement =
              InsertBRElementWithTransaction(currentPoint, nsIEditor::eNone);
          if (resultOfInsertingBRElement.isErr()) {
            NS_WARNING(
                "HTMLEditor::InsertBRElementWithTransaction(eNone) failed");
            return EditActionHandled(resultOfInsertingBRElement.unwrapErr());
          }
          pos++;
          RefPtr<Element> brElement(
              resultOfInsertingBRElement.unwrap().forget());
          if (brElement->GetNextSibling()) {
            pointToInsert.Set(brElement->GetNextSibling());
          } else {
            pointToInsert.SetToEndOf(currentPoint.GetContainer());
          }
          // XXX In most cases, pointToInsert and currentPoint are same here.
          //     But if the <br> element has been moved to different point by
          //     mutation observer, those points become different.
          currentPoint.SetAfter(brElement);
          NS_WARNING_ASSERTION(currentPoint.IsSet(),
                               "Failed to set after the <br> element");
          NS_WARNING_ASSERTION(currentPoint == pointToInsert,
                               "Perhaps, <br> element position has been moved "
                               "to different point "
                               "by mutation observer");
        } else {
          EditorRawDOMPoint pointAfterInsertedString;
          nsresult rv = InsertTextWithTransaction(
              *document, subStr, EditorRawDOMPoint(currentPoint),
              &pointAfterInsertedString);
          if (NS_WARN_IF(Destroyed())) {
            return EditActionHandled(NS_ERROR_EDITOR_DESTROYED);
          }
          if (NS_FAILED(rv)) {
            NS_WARNING("HTMLEditor::InsertTextWithTransaction() failed");
            return EditActionHandled(rv);
          }
          currentPoint = pointAfterInsertedString;
          pointToInsert = pointAfterInsertedString;
        }
      }
    } else {
      constexpr auto tabStr = u"\t"_ns;
      constexpr auto spacesStr = u"    "_ns;
      char specialChars[] = {TAB, nsCRT::LF, 0};
      nsAutoString insertionString(aInsertionString);  // For FindCharInSet().
      while (pos != -1 &&
             pos < AssertedCast<int32_t>(insertionString.Length())) {
        int32_t oldPos = pos;
        int32_t subStrLen;
        pos = insertionString.FindCharInSet(specialChars, oldPos);

        if (pos != -1) {
          subStrLen = pos - oldPos;
          // if first char is newline, then use just it
          if (!subStrLen) {
            subStrLen = 1;
          }
        } else {
          subStrLen = insertionString.Length() - oldPos;
          pos = insertionString.Length();
        }

        nsDependentSubstring subStr(insertionString, oldPos, subStrLen);

        // is it a tab?
        if (subStr.Equals(tabStr)) {
          EditorRawDOMPoint pointAfterInsertedSpaces;
          nsresult rv = WhiteSpaceVisibilityKeeper::InsertText(
              *this, spacesStr, currentPoint, &pointAfterInsertedSpaces);
          if (NS_FAILED(rv)) {
            NS_WARNING("WhiteSpaceVisibilityKeeper::InsertText() failed");
            return EditActionHandled(rv);
          }
          pos++;
          MOZ_ASSERT(pointAfterInsertedSpaces.IsSet());
          currentPoint = pointAfterInsertedSpaces;
          pointToInsert = pointAfterInsertedSpaces;
        }
        // is it a return?
        else if (subStr.Equals(newlineStr)) {
          Result<RefPtr<Element>, nsresult> result =
              WhiteSpaceVisibilityKeeper::InsertBRElement(*this, currentPoint);
          if (result.isErr()) {
            NS_WARNING("WhiteSpaceVisibilityKeeper::InsertBRElement() failed");
            return EditActionHandled(result.inspectErr());
          }
          pos++;
          RefPtr<Element> newBRElement = result.unwrap();
          MOZ_DIAGNOSTIC_ASSERT(newBRElement);
          if (newBRElement->GetNextSibling()) {
            pointToInsert.Set(newBRElement->GetNextSibling());
          } else {
            pointToInsert.SetToEndOf(currentPoint.GetContainer());
          }
          currentPoint.SetAfter(newBRElement);
          NS_WARNING_ASSERTION(currentPoint.IsSet(),
                               "Failed to set after the new <br> element");
          // XXX If the newBRElement has been moved or removed by mutation
          //     observer, we hit this assert.  We need to check if
          //     newBRElement is in expected point, though, we must have
          //     a lot of same bugs...
          NS_WARNING_ASSERTION(
              currentPoint == pointToInsert,
              "Perhaps, newBRElement has been moved or removed unexpectedly");
        } else {
          EditorRawDOMPoint pointAfterInsertedString;
          nsresult rv = WhiteSpaceVisibilityKeeper::InsertText(
              *this, subStr, currentPoint, &pointAfterInsertedString);
          if (NS_FAILED(rv)) {
            NS_WARNING("WhiteSpaceVisibilityKeeper::InsertText() failed");
            return EditActionHandled(rv);
          }
          MOZ_ASSERT(pointAfterInsertedString.IsSet());
          currentPoint = pointAfterInsertedString;
          pointToInsert = pointAfterInsertedString;
        }
      }
    }

    // After this block, pointToInsert is updated by AutoTrackDOMPoint.
  }

  IgnoredErrorResult ignoredError;
  SelectionRef().SetInterlinePosition(false, ignoredError);
  NS_WARNING_ASSERTION(!ignoredError.Failed(),
                       "Failed to unset interline position");

  if (currentPoint.IsSet()) {
    nsresult rv = CollapseSelectionTo(currentPoint);
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return EditActionHandled(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "Selection::Collapse() failed, but ignored");
  }

  // manually update the doc changed range so that AfterEdit will clean up
  // the correct portion of the document.
  if (currentPoint.IsSet()) {
    nsresult rv = TopLevelEditSubActionDataRef().mChangedRange->SetStartAndEnd(
        pointToInsert.ToRawRangeBoundary(), currentPoint.ToRawRangeBoundary());
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "nsRange::SetStartAndEnd() failed");
    return EditActionHandled(rv);
  }

  rv = TopLevelEditSubActionDataRef().mChangedRange->CollapseTo(pointToInsert);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "nsRange::CollapseTo() failed");
  return EditActionHandled(rv);
}

nsresult HTMLEditor::InsertLineBreakAsSubAction() {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!IsSelectionRangeContainerNotContent());

  if (NS_WARN_IF(!mInitSucceeded)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  EditActionResult result = CanHandleHTMLEditSubAction();
  if (result.Failed() || result.Canceled()) {
    NS_WARNING_ASSERTION(result.Succeeded(),
                         "HTMLEditor::CanHandleHTMLEditSubAction() failed");
    return result.Rv();
  }

  // XXX This may be called by execCommand() with "insertLineBreak".
  //     In such case, naming the transaction "TypingTxnName" is odd.
  AutoPlaceholderBatch treatAsOneTransaction(*this, *nsGkAtoms::TypingTxnName,
                                             ScrollSelectionIntoView::Yes);

  // calling it text insertion to trigger moz br treatment by rules
  // XXX Why do we use EditSubAction::eInsertText here?  Looks like
  //     EditSubAction::eInsertLineBreak or EditSubAction::eInsertNode
  //     is better.
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eInsertText, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  UndefineCaretBidiLevel();

  // If the selection isn't collapsed, delete it.
  if (!SelectionRef().IsCollapsed()) {
    nsresult rv =
        DeleteSelectionAsSubAction(nsIEditor::eNone, nsIEditor::eStrip);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "EditorBase::DeleteSelectionAsSubAction(eNone, eStrip) failed");
      return rv;
    }
  }

  const nsRange* firstRange = SelectionRef().GetRangeAt(0);
  if (NS_WARN_IF(!firstRange)) {
    return NS_ERROR_FAILURE;
  }

  EditorDOMPoint atStartOfSelection(firstRange->StartRef());
  if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(atStartOfSelection.IsSetAndValid());

  RefPtr<Element> editingHost = GetActiveEditingHost();
  if (NS_WARN_IF(!editingHost)) {
    return NS_ERROR_FAILURE;
  }

  // For backward compatibility, we should not insert a linefeed if
  // paragraph separator is set to "br" which is Gecko-specific mode.
  if (GetDefaultParagraphSeparator() == ParagraphSeparator::br ||
      !HTMLEditUtils::ShouldInsertLinefeedCharacter(atStartOfSelection,
                                                    *editingHost)) {
    // InsertBRElementWithTransaction() will set selection after the new <br>
    // element.
    Result<RefPtr<Element>, nsresult> resultOfInsertingBRElement =
        InsertBRElementWithTransaction(atStartOfSelection, nsIEditor::eNext);
    if (resultOfInsertingBRElement.isErr()) {
      NS_WARNING("HTMLEditor::InsertBRElementWithTransaction() failed");
      return resultOfInsertingBRElement.unwrapErr();
    }
    MOZ_ASSERT(resultOfInsertingBRElement.inspect());
    return NS_OK;
  }

  nsresult rv = EnsureNoPaddingBRElementForEmptyEditor();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::EnsureNoPaddingBRElementForEmptyEditor() "
                       "failed, but ignored");

  if (NS_SUCCEEDED(rv) && SelectionRef().IsCollapsed()) {
    nsresult rv = EnsureCaretNotAfterInvisibleBRElement();
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::EnsureCaretNotAfterInvisibleBRElement() "
                         "failed, but ignored");
    if (NS_SUCCEEDED(rv)) {
      nsresult rv = PrepareInlineStylesForCaret();
      if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "HTMLEditor::PrepareInlineStylesForCaret() failed, but ignored");
    }
  }

  firstRange = SelectionRef().GetRangeAt(0);
  if (NS_WARN_IF(!firstRange)) {
    return NS_ERROR_FAILURE;
  }

  atStartOfSelection = EditorDOMPoint(firstRange->StartRef());
  if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(atStartOfSelection.IsSetAndValid());

  // Do nothing if the node is read-only
  if (!HTMLEditUtils::IsSimplyEditableNode(
          *atStartOfSelection.GetContainer())) {
    return NS_SUCCESS_DOM_NO_OPERATION;
  }

  rv = HandleInsertLinefeed(atStartOfSelection, *editingHost);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::HandleInsertLinefeed() failed");
  return rv;
}

EditActionResult HTMLEditor::InsertParagraphSeparatorAsSubAction() {
  if (NS_WARN_IF(!mInitSucceeded)) {
    return EditActionIgnored(NS_ERROR_NOT_INITIALIZED);
  }

  EditActionResult result = CanHandleHTMLEditSubAction();
  if (result.Failed() || result.Canceled()) {
    NS_WARNING_ASSERTION(result.Succeeded(),
                         "HTMLEditor::CanHandleHTMLEditSubAction() failed");
    return result;
  }

  // XXX This may be called by execCommand() with "insertParagraph".
  //     In such case, naming the transaction "TypingTxnName" is odd.
  AutoPlaceholderBatch treatAsOneTransaction(*this, *nsGkAtoms::TypingTxnName,
                                             ScrollSelectionIntoView::Yes);

  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eInsertParagraphSeparator, nsIEditor::eNext,
      ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return EditActionResult(ignoredError.StealNSResult());
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  UndefineCaretBidiLevel();

  // If the selection isn't collapsed, delete it.
  if (!SelectionRef().IsCollapsed()) {
    nsresult rv =
        DeleteSelectionAsSubAction(nsIEditor::eNone, nsIEditor::eStrip);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "EditorBase::DeleteSelectionAsSubAction(eNone, eStrip) failed");
      return EditActionIgnored(rv);
    }
  }

  nsresult rv = EnsureNoPaddingBRElementForEmptyEditor();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::EnsureNoPaddingBRElementForEmptyEditor() "
                       "failed, but ignored");

  if (NS_SUCCEEDED(rv) && SelectionRef().IsCollapsed()) {
    nsresult rv = EnsureCaretNotAfterInvisibleBRElement();
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::EnsureCaretNotAfterInvisibleBRElement() "
                         "failed, but ignored");
    if (NS_SUCCEEDED(rv)) {
      nsresult rv = PrepareInlineStylesForCaret();
      if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
        return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "HTMLEditor::PrepareInlineStylesForCaret() failed, but ignored");
    }
  }

  // Split any mailcites in the way.  Should we abort this if we encounter
  // table cell boundaries?
  if (IsMailEditor()) {
    EditorDOMPoint pointToSplit(EditorBase::GetStartPoint(SelectionRef()));
    if (NS_WARN_IF(!pointToSplit.IsSet())) {
      return EditActionIgnored(NS_ERROR_FAILURE);
    }

    EditActionResult result = SplitMailCiteElements(pointToSplit);
    if (result.Failed()) {
      NS_WARNING("HTMLEditor::SplitMailCiteElements() failed");
      return result;
    }
    if (result.Handled()) {
      return result;
    }
  }

  // Smart splitting rules
  const nsRange* firstRange = SelectionRef().GetRangeAt(0);
  if (NS_WARN_IF(!firstRange)) {
    return EditActionIgnored(NS_ERROR_FAILURE);
  }

  EditorDOMPoint atStartOfSelection(firstRange->StartRef());
  if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
    return EditActionIgnored(NS_ERROR_FAILURE);
  }
  MOZ_ASSERT(atStartOfSelection.IsSetAndValid());

  // Do nothing if the node is read-only
  if (!HTMLEditUtils::IsSimplyEditableNode(
          *atStartOfSelection.GetContainer())) {
    return EditActionCanceled();
  }

  // If the active editing host is an inline element, or if the active editing
  // host is the block parent itself and we're configured to use <br> as a
  // paragraph separator, just append a <br>.
  RefPtr<Element> editingHost = GetActiveEditingHost();
  if (NS_WARN_IF(!editingHost)) {
    return EditActionIgnored(NS_ERROR_FAILURE);
  }
  // If the editing host parent element is editable, it means that the editing
  // host must be a <body> element and the selection may be outside the body
  // element.  If the selection is outside the editing host, we should not
  // insert new paragraph nor <br> element.
  // XXX Currently, we don't support editing outside <body> element, but Blink
  //     does it.
  if (editingHost->GetParentElement() &&
      HTMLEditUtils::IsSimplyEditableNode(*editingHost->GetParentElement()) &&
      (!atStartOfSelection.IsInContentNode() ||
       !nsContentUtils::ContentIsFlattenedTreeDescendantOf(
           atStartOfSelection.ContainerAsContent(), editingHost))) {
    return EditActionHandled(NS_ERROR_EDITOR_NO_EDITABLE_RANGE);
  }

  // Look for the nearest parent block.  However, don't return error even if
  // there is no block parent here because in such case, i.e., editing host
  // is an inline element, we should insert <br> simply.
  RefPtr<Element> editableBlockElement =
      atStartOfSelection.IsInContentNode()
          ? HTMLEditUtils::GetInclusiveAncestorElement(
                *atStartOfSelection.ContainerAsContent(),
                HTMLEditUtils::ClosestEditableBlockElement)
          : nullptr;

  ParagraphSeparator separator = GetDefaultParagraphSeparator();
  bool insertLineBreak;
  // If there is no block parent in the editing host, i.e., the editing host
  // itself is also a non-block element, we should insert a line break.
  if (!editableBlockElement) {
    // XXX Chromium checks if the CSS box of the editing host is a block.
    insertLineBreak = true;
  }
  // If the editable block element is not splittable, e.g., it's an editing
  // host, and the default paragraph separator is <br> or the element cannot
  // contain a <p> element, we should insert a <br> element.
  else if (!HTMLEditUtils::IsSplittableNode(*editableBlockElement)) {
    insertLineBreak =
        separator == ParagraphSeparator::br ||
        !HTMLEditUtils::CanElementContainParagraph(*editingHost) ||
        HTMLEditUtils::ShouldInsertLinefeedCharacter(atStartOfSelection,
                                                     *editingHost);
  }
  // If the nearest block parent is a single-line container declared in
  // the execCommand spec and not the editing host, we should separate the
  // block even if the default paragraph separator is <br> element.
  else if (HTMLEditUtils::IsSingleLineContainer(*editableBlockElement)) {
    insertLineBreak = false;
  }
  // Otherwise, unless there is no block ancestor which can contain <p>
  // element, we shouldn't insert a line break here.
  else {
    insertLineBreak = true;
    for (const Element* editableBlockAncestor = editableBlockElement;
         editableBlockAncestor && insertLineBreak;
         editableBlockAncestor = HTMLEditUtils::GetAncestorElement(
             *editableBlockAncestor,
             HTMLEditUtils::ClosestEditableBlockElement)) {
      insertLineBreak =
          !HTMLEditUtils::CanElementContainParagraph(*editableBlockAncestor);
    }
  }

  // If we cannot insert a <p>/<div> element at the selection, we should insert
  // a <br> element or a linefeed instead.
  if (insertLineBreak) {
    // For backward compatibility, we should not insert a linefeed if
    // paragraph separator is set to "br" which is Gecko-specific mode.
    if (separator != ParagraphSeparator::br &&
        HTMLEditUtils::ShouldInsertLinefeedCharacter(atStartOfSelection,
                                                     *editingHost)) {
      nsresult rv = HandleInsertLinefeed(atStartOfSelection, *editingHost);
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::HandleInsertLinefeed() failed");
        return EditActionIgnored(rv);
      }
      return EditActionHandled();
    }

    nsresult rv = HandleInsertBRElement(atStartOfSelection, *editingHost);
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::HandleInsertBRElement() failed");
      return EditActionIgnored(rv);
    }
    return EditActionHandled();
  }

  if (!HTMLEditUtils::IsSplittableNode(*editableBlockElement) &&
      separator != ParagraphSeparator::br) {
    // Insert a new block first
    MOZ_ASSERT(separator == ParagraphSeparator::div ||
               separator == ParagraphSeparator::p);
    // FormatBlockContainerWithTransaction() creates AutoSelectionRestorer.
    // Therefore, even if it returns NS_OK, editor might have been destroyed
    // at restoring Selection.
    nsresult rv = FormatBlockContainerWithTransaction(
        MOZ_KnownLive(HTMLEditor::ToParagraphSeparatorTagName(separator)));
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED) ||
        NS_WARN_IF(Destroyed())) {
      return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
    }
    // We warn on failure, but don't handle it, because it might be harmless.
    // Instead we just check that a new block was actually created.
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::FormatBlockContainerWithTransaction() "
                         "failed, but ignored");

    firstRange = SelectionRef().GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return EditActionIgnored(NS_ERROR_FAILURE);
    }

    atStartOfSelection = firstRange->StartRef();
    if (NS_WARN_IF(!atStartOfSelection.IsInContentNode())) {
      return EditActionIgnored(NS_ERROR_FAILURE);
    }
    MOZ_ASSERT(atStartOfSelection.IsSetAndValid());

    editableBlockElement = HTMLEditUtils::GetInclusiveAncestorElement(
        *atStartOfSelection.ContainerAsContent(),
        HTMLEditUtils::ClosestEditableBlockElement);
    if (NS_WARN_IF(!editableBlockElement)) {
      return EditActionIgnored(NS_ERROR_UNEXPECTED);
    }
    if (NS_WARN_IF(!HTMLEditUtils::IsSplittableNode(*editableBlockElement))) {
      // Didn't create a new block for some reason, fall back to <br>
      nsresult rv = HandleInsertBRElement(atStartOfSelection, *editingHost);
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::HandleInsertBRElement() failed");
        return EditActionIgnored(rv);
      }
      return EditActionHandled();
    }
    // Now, mNewBlockElement is last created block element for wrapping inline
    // elements around the caret position and AfterEditInner() will move
    // caret into it.  However, it may be different from block parent of
    // the caret position.  E.g., FormatBlockContainerWithTransaction() may
    // wrap following inline elements of a <br> element which is next sibling
    // of container of the caret.  So, we need to adjust mNewBlockElement here
    // for avoiding jumping caret to odd position.
    TopLevelEditSubActionDataRef().mNewBlockElement = editableBlockElement;
  }

  // If block is empty, populate with br.  (For example, imagine a div that
  // contains the word "text".  The user selects "text" and types return.
  // "Text" is deleted leaving an empty block.  We want to put in one br to
  // make block have a line.  Then code further below will put in a second br.)
  if (HTMLEditUtils::IsEmptyBlockElement(
          *editableBlockElement,
          {EmptyCheckOption::TreatSingleBRElementAsVisible})) {
    AutoEditorDOMPointChildInvalidator lockOffset(atStartOfSelection);
    EditorDOMPoint endOfBlockParent;
    endOfBlockParent.SetToEndOf(editableBlockElement);
    Result<RefPtr<Element>, nsresult> resultOfInsertingBRElement =
        InsertBRElementWithTransaction(endOfBlockParent);
    if (resultOfInsertingBRElement.isErr()) {
      NS_WARNING("HTMLEditor::InsertBRElementWithTransaction() failed");
      return EditActionIgnored(resultOfInsertingBRElement.unwrapErr());
    }
    MOZ_ASSERT(resultOfInsertingBRElement.inspect());
  }

  RefPtr<Element> maybeNonEditableListItem =
      HTMLEditUtils::GetClosestAncestorListItemElement(*editableBlockElement,
                                                       editingHost);
  if (maybeNonEditableListItem &&
      HTMLEditUtils::IsSplittableNode(*maybeNonEditableListItem)) {
    nsresult rv = HandleInsertParagraphInListItemElement(
        *maybeNonEditableListItem, atStartOfSelection);
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::HandleInsertParagraphInListItemElement() "
                         "failed, but ignored");
    return EditActionHandled();
  }

  if (HTMLEditUtils::IsHeader(*editableBlockElement)) {
    // Headers: close (or split) header
    nsresult rv = HandleInsertParagraphInHeadingElement(*editableBlockElement,
                                                        atStartOfSelection);
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::HandleInsertParagraphInHeadingElement() "
                         "failed, but ignored");
    return EditActionHandled();
  }

  // XXX Ideally, we should take same behavior with both <p> container and
  //     <div> container.  However, we are still using <br> as default
  //     paragraph separator (non-standard) and we've split only <p> container
  //     long time.  Therefore, some web apps may depend on this behavior like
  //     Gmail.  So, let's use traditional odd behavior only when the default
  //     paragraph separator is <br>.  Otherwise, take consistent behavior
  //     between <p> container and <div> container.
  if ((separator == ParagraphSeparator::br &&
       editableBlockElement->IsHTMLElement(nsGkAtoms::p)) ||
      (separator != ParagraphSeparator::br &&
       editableBlockElement->IsAnyOfHTMLElements(nsGkAtoms::p,
                                                 nsGkAtoms::div))) {
    AutoEditorDOMPointChildInvalidator lockOffset(atStartOfSelection);
    // Paragraphs: special rules to look for <br>s
    EditActionResult result =
        HandleInsertParagraphInParagraph(*editableBlockElement);
    if (result.Failed()) {
      NS_WARNING("HTMLEditor::HandleInsertParagraphInParagraph() failed");
      return result;
    }
    if (result.Handled()) {
      // Now, atStartOfSelection may be invalid because the left paragraph
      // may have less children than its offset.  For avoiding warnings of
      // validation of EditorDOMPoint, we should not touch it anymore.
      lockOffset.Cancel();
      return result;
    }
    // Fall through, if HandleInsertParagraphInParagraph() didn't handle it.
    MOZ_ASSERT(!result.Canceled(),
               "HandleInsertParagraphInParagraph() canceled this edit action, "
               "InsertParagraphSeparatorAsSubAction() needs to handle this "
               "action instead");
  }

  // If nobody handles this edit action, let's insert new <br> at the selection.
  rv = HandleInsertBRElement(atStartOfSelection, *editingHost);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::HandleInsertBRElement() failed");
    return EditActionIgnored(rv);
  }
  return EditActionHandled();
}

nsresult HTMLEditor::HandleInsertBRElement(const EditorDOMPoint& aPointToBreak,
                                           Element& aEditingHost) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!aPointToBreak.IsSet())) {
    return NS_ERROR_INVALID_ARG;
  }

  bool brElementIsAfterBlock = false, brElementIsBeforeBlock = false;

  // First, insert a <br> element.
  RefPtr<Element> brElement;
  if (IsInPlaintextMode()) {
    Result<RefPtr<Element>, nsresult> resultOfInsertingBRElement =
        InsertBRElementWithTransaction(aPointToBreak);
    if (resultOfInsertingBRElement.isErr()) {
      NS_WARNING("HTMLEditor::InsertBRElementWithTransaction() failed");
      return resultOfInsertingBRElement.unwrapErr();
    }
    MOZ_ASSERT(resultOfInsertingBRElement.inspect());
    brElement = resultOfInsertingBRElement.unwrap().forget();
  } else {
    EditorDOMPoint pointToBreak(aPointToBreak);
    WSRunScanner wsRunScanner(&aEditingHost, pointToBreak);
    WSScanResult backwardScanResult =
        wsRunScanner.ScanPreviousVisibleNodeOrBlockBoundaryFrom(pointToBreak);
    if (backwardScanResult.Failed()) {
      NS_WARNING(
          "WSRunScanner::ScanPreviousVisibleNodeOrBlockBoundaryFrom() failed");
      return NS_ERROR_FAILURE;
    }
    brElementIsAfterBlock = backwardScanResult.ReachedBlockBoundary();
    WSScanResult forwardScanResult =
        wsRunScanner.ScanNextVisibleNodeOrBlockBoundaryFrom(pointToBreak);
    if (forwardScanResult.Failed()) {
      NS_WARNING(
          "WSRunScanner::ScanNextVisibleNodeOrBlockBoundaryFrom() failed");
      return NS_ERROR_FAILURE;
    }
    brElementIsBeforeBlock = forwardScanResult.ReachedBlockBoundary();
    // If the container of the break is a link, we need to split it and
    // insert new <br> between the split links.
    RefPtr<Element> linkNode =
        HTMLEditor::GetLinkElement(pointToBreak.GetContainer());
    if (linkNode) {
      SplitNodeResult splitLinkNodeResult = SplitNodeDeepWithTransaction(
          *linkNode, pointToBreak, SplitAtEdges::eDoNotCreateEmptyContainer);
      if (MOZ_UNLIKELY(splitLinkNodeResult.Failed())) {
        NS_WARNING(
            "HTMLEditor::SplitNodeDeepWithTransaction(SplitAtEdges::"
            "eDoNotCreateEmptyContainer) failed");
        return splitLinkNodeResult.Rv();
      }
      pointToBreak = splitLinkNodeResult.AtSplitPoint<EditorDOMPoint>();
    }
    Result<RefPtr<Element>, nsresult> result =
        WhiteSpaceVisibilityKeeper::InsertBRElement(*this, pointToBreak);
    if (result.isErr()) {
      NS_WARNING("WhiteSpaceVisibilityKeeper::InsertBRElement() failed");
      return result.inspectErr();
    }
    brElement = result.unwrap();
    MOZ_ASSERT(brElement);
  }

  // If the <br> element has already been removed from the DOM tree by a
  // mutation event listener, don't continue handling this.
  if (NS_WARN_IF(!brElement->GetParentNode())) {
    return NS_ERROR_FAILURE;
  }

  if (brElementIsAfterBlock && brElementIsBeforeBlock) {
    // We just placed a <br> between block boundaries.  This is the one case
    // where we want the selection to be before the br we just placed, as the
    // br will be on a new line, rather than at end of prior line.
    // XXX brElementIsAfterBlock and brElementIsBeforeBlock were set before
    //     modifying the DOM tree.  So, now, the <br> element may not be
    //     between blocks.
    IgnoredErrorResult ignoredError;
    SelectionRef().SetInterlinePosition(true, ignoredError);
    NS_WARNING_ASSERTION(
        !ignoredError.Failed(),
        "Selection::SetInterlinePosition(true) failed, but ignored");
    nsresult rv = CollapseSelectionTo(EditorRawDOMPoint(brElement));
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::CollapseSelectionTo() failed");
    return rv;
  }

  EditorDOMPoint afterBRElement(brElement);
  DebugOnly<bool> advanced = afterBRElement.AdvanceOffset();
  NS_WARNING_ASSERTION(advanced,
                       "Failed to advance offset after the new <br> element");
  WSScanResult forwardScanFromAfterBRElementResult =
      WSRunScanner::ScanNextVisibleNodeOrBlockBoundary(&aEditingHost,
                                                       afterBRElement);
  if (forwardScanFromAfterBRElementResult.Failed()) {
    NS_WARNING("WSRunScanner::ScanNextVisibleNodeOrBlockBoundary() failed");
    return NS_ERROR_FAILURE;
  }
  if (forwardScanFromAfterBRElementResult.ReachedBRElement()) {
    // The next thing after the break we inserted is another break.  Move the
    // second break to be the first break's sibling.  This will prevent them
    // from being in different inline nodes, which would break
    // SetInterlinePosition().  It will also assure that if the user clicks
    // away and then clicks back on their new blank line, they will still get
    // the style from the line above.
    if (brElement->GetNextSibling() !=
        forwardScanFromAfterBRElementResult.BRElementPtr()) {
      MOZ_ASSERT(forwardScanFromAfterBRElementResult.BRElementPtr());
      nsresult rv = MoveNodeWithTransaction(
          MOZ_KnownLive(*forwardScanFromAfterBRElementResult.BRElementPtr()),
          afterBRElement);
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::MoveNodeWithTransaction() failed");
        return rv;
      }
    }
  }

  // SetInterlinePosition(true) means we want the caret to stick to the
  // content on the "right".  We want the caret to stick to whatever is past
  // the break.  This is because the break is on the same line we were on,
  // but the next content will be on the following line.

  // An exception to this is if the break has a next sibling that is a block
  // node.  Then we stick to the left to avoid an uber caret.
  nsIContent* nextSiblingOfBRElement = brElement->GetNextSibling();
  IgnoredErrorResult ignoredError;
  SelectionRef().SetInterlinePosition(
      !(nextSiblingOfBRElement &&
        HTMLEditUtils::IsBlockElement(*nextSiblingOfBRElement)),
      ignoredError);
  NS_WARNING_ASSERTION(!ignoredError.Failed(),
                       "Selection::SetInterlinePosition() failed, but ignored");
  nsresult rv = CollapseSelectionTo(afterBRElement);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::CollapseSelectionTo() failed");
  return rv;
}

nsresult HTMLEditor::HandleInsertLinefeed(const EditorDOMPoint& aPointToBreak,
                                          Element& aEditingHost) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!aPointToBreak.IsSet())) {
    return NS_ERROR_INVALID_ARG;
  }

  // TODO: The following code is duplicated from `HandleInsertText`.  They
  //       should be merged when we fix bug 92921.

  RefPtr<const nsRange> caretRange =
      nsRange::Create(aPointToBreak.ToRawRangeBoundary(),
                      aPointToBreak.ToRawRangeBoundary(), IgnoreErrors());
  if (NS_WARN_IF(!caretRange)) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = CreateStyleForInsertText(*caretRange);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::CreateStyleForInsertText() failed");
    return rv;
  }

  caretRange = SelectionRef().GetRangeAt(0);
  if (NS_WARN_IF(!caretRange)) {
    return NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE;
  }

  EditorDOMPoint pointToInsert(caretRange->StartRef());
  if (NS_WARN_IF(!pointToInsert.IsSet()) ||
      NS_WARN_IF(!pointToInsert.IsInContentNode())) {
    return NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE;
  }
  MOZ_ASSERT(pointToInsert.IsSetAndValid());
  MOZ_ASSERT_IF(
      !pointToInsert.IsInTextNode(),
      HTMLEditUtils::CanNodeContain(*pointToInsert.ContainerAsContent(),
                                    *nsGkAtoms::textTagName));
  RefPtr<Document> document = GetDocument();
  MOZ_ASSERT(document);
  if (NS_WARN_IF(!document)) {
    return NS_ERROR_FAILURE;
  }

  AutoRestore<bool> disableListener(
      EditSubActionDataRef().mAdjustChangedRangeFromListener);
  EditSubActionDataRef().mAdjustChangedRangeFromListener = false;
  AutoTransactionsConserveSelection dontChangeMySelection(*this);
  EditorRawDOMPoint caretAfterInsert;
  {
    AutoTrackDOMPoint trackingInsertingPosition(RangeUpdaterRef(),
                                                &pointToInsert);
    nsresult rv = InsertTextWithTransaction(*document, u"\n"_ns, pointToInsert,
                                            &caretAfterInsert);
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::InsertTextWithTransaction() failed");
      return rv;
    }
  }

  // Insert a padding <br> element at the end of the block element if there is
  // no content between the inserted linefeed and the following block boundary
  // to make sure that the last line is visible.
  // XXX Blink/WebKit inserts another linefeed character in this case.  However,
  //     for doing it, we need more work, e.g., updating serializer, deleting
  //     unnecessary padding <br> element at modifying the last line.
  if (caretAfterInsert.IsInContentNode() &&
      caretAfterInsert.IsEndOfContainer()) {
    WSRunScanner wsScannerAtCaret(&aEditingHost, caretAfterInsert);
    if (wsScannerAtCaret.StartsFromPreformattedLineBreak() &&
        wsScannerAtCaret.EndsByBlockBoundary() &&
        HTMLEditUtils::CanNodeContain(*wsScannerAtCaret.GetEndReasonContent(),
                                      *nsGkAtoms::br)) {
      EditorDOMPoint newCaretPosition(caretAfterInsert);
      {
        AutoTrackDOMPoint trackingInsertedPosition(RangeUpdaterRef(),
                                                   &pointToInsert);
        AutoTrackDOMPoint trackingNewCaretPosition(RangeUpdaterRef(),
                                                   &newCaretPosition);
        Result<RefPtr<Element>, nsresult> resultOfInsertingBRElement =
            InsertBRElementWithTransaction(newCaretPosition,
                                           nsIEditor::ePrevious);
        if (resultOfInsertingBRElement.isErr()) {
          NS_WARNING("HTMLEditor::InsertBRElementWithTransaction() failed");
          return resultOfInsertingBRElement.unwrapErr();
        }
        MOZ_ASSERT(resultOfInsertingBRElement.inspect());
      }
      caretAfterInsert = newCaretPosition;
    }
  }

  IgnoredErrorResult ignoredError;
  SelectionRef().SetInterlinePosition(false, ignoredError);
  NS_WARNING_ASSERTION(!ignoredError.Failed(),
                       "Failed to unset interline position, but ignored");

  // manually update the doc changed range so that AfterEdit will clean up
  // the correct portion of the document.
  if (!caretAfterInsert.IsSet()) {
    if (NS_FAILED(TopLevelEditSubActionDataRef().mChangedRange->CollapseTo(
            pointToInsert))) {
      NS_WARNING("nsRange::CollapseTo() failed");
      return NS_ERROR_FAILURE;
    }
  }

  if (NS_FAILED(TopLevelEditSubActionDataRef().mChangedRange->SetStartAndEnd(
          pointToInsert.ToRawRangeBoundary(),
          caretAfterInsert.ToRawRangeBoundary()))) {
    NS_WARNING("nsRange::SetStartAndEnd() failed");
    return NS_ERROR_FAILURE;
  }

  rv = CollapseSelectionTo(caretAfterInsert);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::CollapseSelectionTo() failed");
    return rv;
  }
  return NS_OK;
}

EditActionResult HTMLEditor::SplitMailCiteElements(
    const EditorDOMPoint& aPointToSplit) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(aPointToSplit.IsSet());

  RefPtr<Element> citeNode =
      GetMostAncestorMailCiteElement(*aPointToSplit.GetContainer());
  if (!citeNode) {
    return EditActionIgnored();
  }

  EditorDOMPoint pointToSplit(aPointToSplit);

  // If our selection is just before a break, nudge it to be just after it.
  // This does two things for us.  It saves us the trouble of having to add
  // a break here ourselves to preserve the "blockness" of the inline span
  // mailquote (in the inline case), and :
  // it means the break won't end up making an empty line that happens to be
  // inside a mailquote (in either inline or block case).
  // The latter can confuse a user if they click there and start typing,
  // because being in the mailquote may affect wrapping behavior, or font
  // color, etc.
  RefPtr<Element> editingHost = GetActiveEditingHost();
  WSScanResult forwardScanFromPointToSplitResult =
      WSRunScanner::ScanNextVisibleNodeOrBlockBoundary(editingHost,
                                                       pointToSplit);
  if (forwardScanFromPointToSplitResult.Failed()) {
    return EditActionResult(NS_ERROR_FAILURE);
  }
  // If selection start point is before a break and it's inside the mailquote,
  // let's split it after the visible node.
  if (forwardScanFromPointToSplitResult.ReachedBRElement() &&
      forwardScanFromPointToSplitResult.BRElementPtr() != citeNode &&
      citeNode->Contains(forwardScanFromPointToSplitResult.BRElementPtr())) {
    pointToSplit = forwardScanFromPointToSplitResult.PointAfterContent();
  }

  if (NS_WARN_IF(!pointToSplit.GetContainerAsContent())) {
    return EditActionIgnored(NS_ERROR_FAILURE);
  }

  SplitNodeResult splitCiteNodeResult = SplitNodeDeepWithTransaction(
      *citeNode, pointToSplit, SplitAtEdges::eDoNotCreateEmptyContainer);
  if (MOZ_UNLIKELY(splitCiteNodeResult.Failed())) {
    NS_WARNING(
        "HTMLEditor::SplitNodeDeepWithTransaction(SplitAtEdges::"
        "eDoNotCreateEmptyContainer) failed");
    return EditActionIgnored(splitCiteNodeResult.Rv());
  }
  pointToSplit.Clear();

  // Add an invisible <br> to the end of current cite node (If new left cite
  // has not been created, we're at the end of it.  Otherwise, we're still at
  // the right node) if it was a <span> of style="display: block". This is
  // important, since when serializing the cite to plain text, the span which
  // caused the visual break is discarded.  So the added <br> will guarantee
  // that the serializer will insert a break where the user saw one.
  // FYI: splitCiteNodeResult grabs the previous node with nsCOMPtr.  So, it's
  //      safe to access previousNodeOfSplitPoint even after changing the DOM
  //      tree and/or selection even though it's raw pointer.
  nsIContent* previousNodeOfSplitPoint =
      splitCiteNodeResult.GetPreviousContent();
  if (previousNodeOfSplitPoint &&
      previousNodeOfSplitPoint->IsHTMLElement(nsGkAtoms::span) &&
      previousNodeOfSplitPoint->GetPrimaryFrame() &&
      previousNodeOfSplitPoint->GetPrimaryFrame()->IsBlockFrameOrSubclass()) {
    nsCOMPtr<nsINode> lastChild = previousNodeOfSplitPoint->GetLastChild();
    if (lastChild && !lastChild->IsHTMLElement(nsGkAtoms::br)) {
      // We ignore the result here.
      EditorDOMPoint endOfPreviousNodeOfSplitPoint;
      endOfPreviousNodeOfSplitPoint.SetToEndOf(previousNodeOfSplitPoint);
      Result<RefPtr<Element>, nsresult> resultOfInsertingInvisibleBRElement =
          InsertBRElementWithTransaction(endOfPreviousNodeOfSplitPoint);
      if (resultOfInsertingInvisibleBRElement.isErr()) {
        NS_WARNING("HTMLEditor::InsertBRElementWithTransaction() failed");
        return EditActionIgnored(
            resultOfInsertingInvisibleBRElement.unwrapErr());
      }
      MOZ_ASSERT(resultOfInsertingInvisibleBRElement.inspect());
    }
  }

  // In most cases, <br> should be inserted after current cite.  However, if
  // left cite hasn't been created because the split point was start of the
  // cite node, <br> should be inserted before the current cite.
  Result<RefPtr<Element>, nsresult> resultOfInsertingBRElement =
      InsertBRElementWithTransaction(
          splitCiteNodeResult.AtSplitPoint<EditorDOMPoint>());
  if (resultOfInsertingBRElement.isErr()) {
    NS_WARNING("HTMLEditor::InsertBRElementWithTransaction() failed");
    return EditActionIgnored(resultOfInsertingBRElement.unwrapErr());
  }
  MOZ_ASSERT(resultOfInsertingBRElement.inspect());

  // Want selection before the break, and on same line.
  EditorDOMPoint atBRElement(resultOfInsertingBRElement.inspect());
  {
    AutoEditorDOMPointChildInvalidator lockOffset(atBRElement);
    IgnoredErrorResult ignoredError;
    SelectionRef().SetInterlinePosition(true, ignoredError);
    NS_WARNING_ASSERTION(
        !ignoredError.Failed(),
        "Selection::SetInterlinePosition(true) failed, but ignored");
    nsresult rv = CollapseSelectionTo(atBRElement);
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::CollapseSelectionTo() failed");
      return EditActionIgnored(rv);
    }
  }

  // if citeNode wasn't a block, we might also want another break before it.
  // We need to examine the content both before the br we just added and also
  // just after it.  If we don't have another br or block boundary adjacent,
  // then we will need a 2nd br added to achieve blank line that user expects.
  if (HTMLEditUtils::IsInlineElement(*citeNode)) {
    // Use DOM point which we tried to collapse to.
    EditorDOMPoint pointToCreateNewBRElement(atBRElement);

    WSScanResult backwardScanFromPointToCreateNewBRElementResult =
        WSRunScanner::ScanPreviousVisibleNodeOrBlockBoundary(
            editingHost, pointToCreateNewBRElement);
    if (backwardScanFromPointToCreateNewBRElementResult.Failed()) {
      NS_WARNING(
          "WSRunScanner::ScanPreviousVisibleNodeOrBlockBoundary() failed");
      return EditActionResult(NS_ERROR_FAILURE);
    }
    if (backwardScanFromPointToCreateNewBRElementResult
            .InVisibleOrCollapsibleCharacters() ||
        backwardScanFromPointToCreateNewBRElementResult
            .ReachedSpecialContent()) {
      EditorRawDOMPoint pointAfterNewBRElement(
          EditorRawDOMPoint::After(pointToCreateNewBRElement));
      NS_WARNING_ASSERTION(pointAfterNewBRElement.IsSet(),
                           "Failed to set to after the <br> node");
      WSScanResult forwardScanFromPointAfterNewBRElementResult =
          WSRunScanner::ScanNextVisibleNodeOrBlockBoundary(
              editingHost, pointAfterNewBRElement);
      if (forwardScanFromPointAfterNewBRElementResult.Failed()) {
        NS_WARNING("WSRunScanner::ScanNextVisibleNodeOrBlockBoundary() failed");
        return EditActionResult(NS_ERROR_FAILURE);
      }
      if (forwardScanFromPointAfterNewBRElementResult
              .InVisibleOrCollapsibleCharacters() ||
          forwardScanFromPointAfterNewBRElementResult.ReachedSpecialContent() ||
          // In case we're at the very end.
          forwardScanFromPointAfterNewBRElementResult
              .ReachedCurrentBlockBoundary()) {
        Result<RefPtr<Element>, nsresult> resultOfInsertingBRElement =
            InsertBRElementWithTransaction(pointToCreateNewBRElement);
        if (resultOfInsertingBRElement.isErr()) {
          NS_WARNING("HTMLEditor::InsertBRElementWithTransaction() failed");
          return EditActionIgnored(resultOfInsertingBRElement.unwrapErr());
        }
        MOZ_ASSERT(resultOfInsertingBRElement.inspect());
        // Now, those points may be invalid.
        pointToCreateNewBRElement.Clear();
        pointAfterNewBRElement.Clear();
      }
    }
  }

  // delete any empty cites
  if (previousNodeOfSplitPoint &&
      HTMLEditUtils::IsEmptyNode(*previousNodeOfSplitPoint)) {
    nsresult rv =
        DeleteNodeWithTransaction(MOZ_KnownLive(*previousNodeOfSplitPoint));
    if (NS_WARN_IF(Destroyed())) {
      return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
      return EditActionIgnored(rv);
    }
  }

  if (citeNode && HTMLEditUtils::IsEmptyNode(*citeNode)) {
    nsresult rv = DeleteNodeWithTransaction(*citeNode);
    if (NS_WARN_IF(Destroyed())) {
      return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
      return EditActionIgnored(rv);
    }
  }

  return EditActionHandled();
}

HTMLEditor::CharPointData
HTMLEditor::GetPreviousCharPointDataForNormalizingWhiteSpaces(
    const EditorDOMPointInText& aPoint) const {
  MOZ_ASSERT(aPoint.IsSetAndValid());

  if (!aPoint.IsStartOfContainer()) {
    return CharPointData::InSameTextNode(
        HTMLEditor::GetPreviousCharPointType(aPoint));
  }
  EditorDOMPointInText previousCharPoint =
      WSRunScanner::GetPreviousEditableCharPoint(GetActiveEditingHost(),
                                                 aPoint);
  if (!previousCharPoint.IsSet()) {
    return CharPointData::InDifferentTextNode(CharPointType::TextEnd);
  }
  return CharPointData::InDifferentTextNode(
      HTMLEditor::GetCharPointType(previousCharPoint));
}

HTMLEditor::CharPointData
HTMLEditor::GetInclusiveNextCharPointDataForNormalizingWhiteSpaces(
    const EditorDOMPointInText& aPoint) const {
  MOZ_ASSERT(aPoint.IsSetAndValid());

  if (!aPoint.IsEndOfContainer()) {
    return CharPointData::InSameTextNode(HTMLEditor::GetCharPointType(aPoint));
  }
  EditorDOMPointInText nextCharPoint =
      WSRunScanner::GetInclusiveNextEditableCharPoint(GetActiveEditingHost(),
                                                      aPoint);
  if (!nextCharPoint.IsSet()) {
    return CharPointData::InDifferentTextNode(CharPointType::TextEnd);
  }
  return CharPointData::InDifferentTextNode(
      HTMLEditor::GetCharPointType(nextCharPoint));
}

// static
void HTMLEditor::GenerateWhiteSpaceSequence(
    nsAString& aResult, uint32_t aLength,
    const CharPointData& aPreviousCharPointData,
    const CharPointData& aNextCharPointData) {
  MOZ_ASSERT(aResult.IsEmpty());
  MOZ_ASSERT(aLength);
  // For now, this method does not assume that result will be append to
  // white-space sequence in the text node.
  MOZ_ASSERT(aPreviousCharPointData.AcrossTextNodeBoundary() ||
             !aPreviousCharPointData.IsCollapsibleWhiteSpace());
  // For now, this method does not assume that the result will be inserted
  // into white-space sequence nor start of white-space sequence.
  MOZ_ASSERT(aNextCharPointData.AcrossTextNodeBoundary() ||
             !aNextCharPointData.IsCollapsibleWhiteSpace());

  if (aLength == 1) {
    // Even if previous/next char is in different text node, we should put
    // an ASCII white-space between visible characters.
    // XXX This means that this does not allow to put an NBSP in HTML editor
    //     without preformatted style.  However, Chrome has same issue too.
    if (aPreviousCharPointData.Type() == CharPointType::VisibleChar &&
        aNextCharPointData.Type() == CharPointType::VisibleChar) {
      aResult.Assign(HTMLEditUtils::kSpace);
      return;
    }
    // If it's start or end of text, put an NBSP.
    if (aPreviousCharPointData.Type() == CharPointType::TextEnd ||
        aNextCharPointData.Type() == CharPointType::TextEnd) {
      aResult.Assign(HTMLEditUtils::kNBSP);
      return;
    }
    // If the character is next to a preformatted linefeed, we need to put
    // an NBSP for avoiding collapsed into the linefeed.
    if (aPreviousCharPointData.Type() == CharPointType::PreformattedLineBreak ||
        aNextCharPointData.Type() == CharPointType::PreformattedLineBreak) {
      aResult.Assign(HTMLEditUtils::kNBSP);
      return;
    }
    // Now, the white-space will be inserted to a white-space sequence, but not
    // end of text.  We can put an ASCII white-space only when both sides are
    // not ASCII white-spaces.
    aResult.Assign(
        aPreviousCharPointData.Type() == CharPointType::ASCIIWhiteSpace ||
                aNextCharPointData.Type() == CharPointType::ASCIIWhiteSpace
            ? HTMLEditUtils::kNBSP
            : HTMLEditUtils::kSpace);
    return;
  }

  // Generate pairs of NBSP and ASCII white-space.
  aResult.SetLength(aLength);
  bool appendNBSP = true;  // Basically, starts with an NBSP.
  char16_t* lastChar = aResult.EndWriting() - 1;
  for (char16_t* iter = aResult.BeginWriting(); iter != lastChar; iter++) {
    *iter = appendNBSP ? HTMLEditUtils::kNBSP : HTMLEditUtils::kSpace;
    appendNBSP = !appendNBSP;
  }

  // If the final one is expected to an NBSP, we can put an NBSP simply.
  if (appendNBSP) {
    *lastChar = HTMLEditUtils::kNBSP;
    return;
  }

  // If next char point is end of text node, an ASCII white-space or
  // preformatted linefeed, we need to put an NBSP.
  *lastChar =
      aNextCharPointData.AcrossTextNodeBoundary() ||
              aNextCharPointData.Type() == CharPointType::ASCIIWhiteSpace ||
              aNextCharPointData.Type() == CharPointType::PreformattedLineBreak
          ? HTMLEditUtils::kNBSP
          : HTMLEditUtils::kSpace;
}

void HTMLEditor::ExtendRangeToDeleteWithNormalizingWhiteSpaces(
    EditorDOMPointInText& aStartToDelete, EditorDOMPointInText& aEndToDelete,
    nsAString& aNormalizedWhiteSpacesInStartNode,
    nsAString& aNormalizedWhiteSpacesInEndNode) const {
  MOZ_ASSERT(aStartToDelete.IsSetAndValid());
  MOZ_ASSERT(aEndToDelete.IsSetAndValid());
  MOZ_ASSERT(aStartToDelete.EqualsOrIsBefore(aEndToDelete));
  MOZ_ASSERT(aNormalizedWhiteSpacesInStartNode.IsEmpty());
  MOZ_ASSERT(aNormalizedWhiteSpacesInEndNode.IsEmpty());

  // First, check whether there is surrounding white-spaces or not, and if there
  // are, check whether they are collapsible or not.  Note that we shouldn't
  // touch white-spaces in different text nodes for performance, but we need
  // adjacent text node's first or last character information in some cases.
  Element* editingHost = GetActiveEditingHost();
  EditorDOMPointInText precedingCharPoint =
      WSRunScanner::GetPreviousEditableCharPoint(editingHost, aStartToDelete);
  EditorDOMPointInText followingCharPoint =
      WSRunScanner::GetInclusiveNextEditableCharPoint(editingHost,
                                                      aEndToDelete);
  // Blink-compat: Normalize white-spaces in first node only when not removing
  //               its last character or no text nodes follow the first node.
  //               If removing last character of first node and there are
  //               following text nodes, white-spaces in following text node are
  //               normalized instead.
  const bool removingLastCharOfStartNode =
      aStartToDelete.ContainerAsText() != aEndToDelete.ContainerAsText() ||
      (aEndToDelete.IsEndOfContainer() && followingCharPoint.IsSet());
  const bool maybeNormalizePrecedingWhiteSpaces =
      !removingLastCharOfStartNode && precedingCharPoint.IsSet() &&
      !precedingCharPoint.IsEndOfContainer() &&
      precedingCharPoint.ContainerAsText() ==
          aStartToDelete.ContainerAsText() &&
      precedingCharPoint.IsCharCollapsibleASCIISpaceOrNBSP();
  const bool maybeNormalizeFollowingWhiteSpaces =
      followingCharPoint.IsSet() && !followingCharPoint.IsEndOfContainer() &&
      (followingCharPoint.ContainerAsText() == aEndToDelete.ContainerAsText() ||
       removingLastCharOfStartNode) &&
      followingCharPoint.IsCharCollapsibleASCIISpaceOrNBSP();

  if (!maybeNormalizePrecedingWhiteSpaces &&
      !maybeNormalizeFollowingWhiteSpaces) {
    return;  // There are no white-spaces.
  }

  // Next, consider the range to normalize.
  EditorDOMPointInText startToNormalize, endToNormalize;
  if (maybeNormalizePrecedingWhiteSpaces) {
    Maybe<uint32_t> previousCharOffsetOfWhiteSpaces =
        HTMLEditUtils::GetPreviousNonCollapsibleCharOffset(
            precedingCharPoint, {WalkTextOption::TreatNBSPsCollapsible});
    startToNormalize.Set(precedingCharPoint.ContainerAsText(),
                         previousCharOffsetOfWhiteSpaces.isSome()
                             ? previousCharOffsetOfWhiteSpaces.value() + 1
                             : 0);
    MOZ_ASSERT(!startToNormalize.IsEndOfContainer());
  }
  if (maybeNormalizeFollowingWhiteSpaces) {
    Maybe<uint32_t> nextCharOffsetOfWhiteSpaces =
        HTMLEditUtils::GetInclusiveNextNonCollapsibleCharOffset(
            followingCharPoint, {WalkTextOption::TreatNBSPsCollapsible});
    if (nextCharOffsetOfWhiteSpaces.isSome()) {
      endToNormalize.Set(followingCharPoint.ContainerAsText(),
                         nextCharOffsetOfWhiteSpaces.value());
    } else {
      endToNormalize.SetToEndOf(followingCharPoint.ContainerAsText());
    }
    MOZ_ASSERT(!endToNormalize.IsStartOfContainer());
  }

  // Next, retrieve surrounding information of white-space sequence.
  // If we're removing first text node's last character, we need to
  // normalize white-spaces starts from another text node.  In this case,
  // we need to lie for avoiding assertion in GenerateWhiteSpaceSequence().
  CharPointData previousCharPointData =
      removingLastCharOfStartNode
          ? CharPointData::InDifferentTextNode(CharPointType::TextEnd)
          : GetPreviousCharPointDataForNormalizingWhiteSpaces(
                startToNormalize.IsSet() ? startToNormalize : aStartToDelete);
  CharPointData nextCharPointData =
      GetInclusiveNextCharPointDataForNormalizingWhiteSpaces(
          endToNormalize.IsSet() ? endToNormalize : aEndToDelete);

  // Next, compute number of white-spaces in start/end node.
  uint32_t lengthInStartNode = 0, lengthInEndNode = 0;
  if (startToNormalize.IsSet()) {
    MOZ_ASSERT(startToNormalize.ContainerAsText() ==
               aStartToDelete.ContainerAsText());
    lengthInStartNode = aStartToDelete.Offset() - startToNormalize.Offset();
    MOZ_ASSERT(lengthInStartNode);
  }
  if (endToNormalize.IsSet()) {
    lengthInEndNode =
        endToNormalize.ContainerAsText() == aEndToDelete.ContainerAsText()
            ? endToNormalize.Offset() - aEndToDelete.Offset()
            : endToNormalize.Offset();
    MOZ_ASSERT(lengthInEndNode);
    // If we normalize white-spaces in a text node, we can replace all of them
    // with one ReplaceTextTransaction.
    if (endToNormalize.ContainerAsText() == aStartToDelete.ContainerAsText()) {
      lengthInStartNode += lengthInEndNode;
      lengthInEndNode = 0;
    }
  }

  MOZ_ASSERT(lengthInStartNode + lengthInEndNode);

  // Next, generate normalized white-spaces.
  if (!lengthInEndNode) {
    HTMLEditor::GenerateWhiteSpaceSequence(
        aNormalizedWhiteSpacesInStartNode, lengthInStartNode,
        previousCharPointData, nextCharPointData);
  } else if (!lengthInStartNode) {
    HTMLEditor::GenerateWhiteSpaceSequence(
        aNormalizedWhiteSpacesInEndNode, lengthInEndNode, previousCharPointData,
        nextCharPointData);
  } else {
    // For making `GenerateWhiteSpaceSequence()` simpler, we should create
    // whole white-space sequence first, then, copy to the out params.
    nsAutoString whiteSpaces;
    HTMLEditor::GenerateWhiteSpaceSequence(
        whiteSpaces, lengthInStartNode + lengthInEndNode, previousCharPointData,
        nextCharPointData);
    aNormalizedWhiteSpacesInStartNode =
        Substring(whiteSpaces, 0, lengthInStartNode);
    aNormalizedWhiteSpacesInEndNode = Substring(whiteSpaces, lengthInStartNode);
    MOZ_ASSERT(aNormalizedWhiteSpacesInEndNode.Length() == lengthInEndNode);
  }

  // TODO: Shrink the replacing range and string as far as possible because
  //       this may run a lot, i.e., HTMLEditor creates ReplaceTextTransaction
  //       a lot for normalizing white-spaces.  Then, each transaction shouldn't
  //       have all white-spaces every time because once it's normalized, we
  //       don't need to normalize all of the sequence again, but currently
  //       we do.

  // Finally, extend the range.
  if (startToNormalize.IsSet()) {
    aStartToDelete = startToNormalize;
  }
  if (endToNormalize.IsSet()) {
    aEndToDelete = endToNormalize;
  }
}

Result<EditorDOMPoint, nsresult>
HTMLEditor::DeleteTextAndNormalizeSurroundingWhiteSpaces(
    const EditorDOMPointInText& aStartToDelete,
    const EditorDOMPointInText& aEndToDelete,
    TreatEmptyTextNodes aTreatEmptyTextNodes,
    DeleteDirection aDeleteDirection) {
  MOZ_ASSERT(aStartToDelete.IsSetAndValid());
  MOZ_ASSERT(aEndToDelete.IsSetAndValid());
  MOZ_ASSERT(aStartToDelete.EqualsOrIsBefore(aEndToDelete));

  // Use nsString for these replacing string because we should avoid to copy
  // the buffer from auto storange to ReplaceTextTransaction.
  nsString normalizedWhiteSpacesInFirstNode, normalizedWhiteSpacesInLastNode;

  // First, check whether we need to normalize white-spaces after deleting
  // the given range.
  EditorDOMPointInText startToDelete(aStartToDelete);
  EditorDOMPointInText endToDelete(aEndToDelete);
  ExtendRangeToDeleteWithNormalizingWhiteSpaces(
      startToDelete, endToDelete, normalizedWhiteSpacesInFirstNode,
      normalizedWhiteSpacesInLastNode);

  // If extended range is still collapsed, i.e., the caller just wants to
  // normalize white-space sequence, but there is no white-spaces which need to
  // be replaced, we need to do nothing here.
  if (startToDelete == endToDelete) {
    return EditorDOMPoint(aStartToDelete);
  }

  // Note that the container text node of startToDelete may be removed from
  // the tree if it becomes empty.  Therefore, we need to track the point.
  EditorDOMPoint newCaretPosition;
  if (aStartToDelete.ContainerAsText() == aEndToDelete.ContainerAsText()) {
    newCaretPosition = aEndToDelete;
  } else if (aDeleteDirection == DeleteDirection::Forward) {
    newCaretPosition.SetToEndOf(aStartToDelete.ContainerAsText());
  } else {
    newCaretPosition.Set(aEndToDelete.ContainerAsText(), 0);
  }

  // Then, modify the text nodes in the range.
  while (true) {
    AutoTrackDOMPoint trackingNewCaretPosition(RangeUpdaterRef(),
                                               &newCaretPosition);
    // Use ReplaceTextTransaction if we need to normalize white-spaces in
    // the first text node.
    if (!normalizedWhiteSpacesInFirstNode.IsEmpty()) {
      EditorDOMPoint trackingEndToDelete(endToDelete.ContainerAsText(),
                                         endToDelete.Offset());
      {
        AutoTrackDOMPoint trackEndToDelete(RangeUpdaterRef(),
                                           &trackingEndToDelete);
        uint32_t lengthToReplaceInFirstTextNode =
            startToDelete.ContainerAsText() ==
                    trackingEndToDelete.ContainerAsText()
                ? trackingEndToDelete.Offset() - startToDelete.Offset()
                : startToDelete.ContainerAsText()->TextLength() -
                      startToDelete.Offset();
        nsresult rv = ReplaceTextWithTransaction(
            MOZ_KnownLive(*startToDelete.ContainerAsText()),
            startToDelete.Offset(), lengthToReplaceInFirstTextNode,
            normalizedWhiteSpacesInFirstNode);
        if (NS_FAILED(rv)) {
          NS_WARNING("HTMLEditor::ReplaceTextWithTransaction() failed");
          return Err(rv);
        }
        if (startToDelete.ContainerAsText() ==
            trackingEndToDelete.ContainerAsText()) {
          MOZ_ASSERT(normalizedWhiteSpacesInLastNode.IsEmpty());
          break;  // There is no more text which we need to delete.
        }
      }
      if (MayHaveMutationEventListeners(
              NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED) &&
          (NS_WARN_IF(!trackingEndToDelete.IsSetAndValid()) ||
           NS_WARN_IF(!trackingEndToDelete.IsInTextNode()))) {
        return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
      }
      MOZ_ASSERT(trackingEndToDelete.IsInTextNode());
      endToDelete.Set(trackingEndToDelete.ContainerAsText(),
                      trackingEndToDelete.Offset());
      // If the remaining range was modified by mutation event listener,
      // we should stop handling the deletion.
      startToDelete =
          EditorDOMPointInText::AtEndOf(*startToDelete.ContainerAsText());
      if (MayHaveMutationEventListeners(
              NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED) &&
          NS_WARN_IF(!startToDelete.IsBefore(endToDelete))) {
        return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
      }
    }
    // Delete ASCII whiteSpaces in the range simpley if there are some text
    // nodes which we don't need to replace their text.
    if (normalizedWhiteSpacesInLastNode.IsEmpty() ||
        startToDelete.ContainerAsText() != endToDelete.ContainerAsText()) {
      // If we need to replace text in the last text node, we should
      // delete text before its previous text node.
      EditorDOMPointInText endToDeleteExceptReplaceRange =
          normalizedWhiteSpacesInLastNode.IsEmpty()
              ? endToDelete
              : EditorDOMPointInText(endToDelete.ContainerAsText(), 0);
      if (startToDelete != endToDeleteExceptReplaceRange) {
        nsresult rv = DeleteTextAndTextNodesWithTransaction(
            startToDelete, endToDeleteExceptReplaceRange, aTreatEmptyTextNodes);
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
          return Err(rv);
        }
        if (normalizedWhiteSpacesInLastNode.IsEmpty()) {
          break;  // There is no more text which we need to delete.
        }
        if (MayHaveMutationEventListeners(
                NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED |
                NS_EVENT_BITS_MUTATION_NODEREMOVED |
                NS_EVENT_BITS_MUTATION_NODEREMOVEDFROMDOCUMENT |
                NS_EVENT_BITS_MUTATION_SUBTREEMODIFIED) &&
            (NS_WARN_IF(!endToDeleteExceptReplaceRange.IsSetAndValid()) ||
             NS_WARN_IF(!endToDelete.IsSetAndValid()) ||
             NS_WARN_IF(endToDelete.IsStartOfContainer()))) {
          return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
        }
        // Then, replace the text in the last text node.
        startToDelete = endToDeleteExceptReplaceRange;
      }
    }

    // Replace ASCII whiteSpaces in the range and following character in the
    // last text node.
    MOZ_ASSERT(!normalizedWhiteSpacesInLastNode.IsEmpty());
    MOZ_ASSERT(startToDelete.ContainerAsText() ==
               endToDelete.ContainerAsText());
    nsresult rv = ReplaceTextWithTransaction(
        MOZ_KnownLive(*startToDelete.ContainerAsText()), startToDelete.Offset(),
        endToDelete.Offset() - startToDelete.Offset(),
        normalizedWhiteSpacesInLastNode);
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::ReplaceTextWithTransaction() failed");
      return Err(rv);
    }
    break;
  }

  if (!newCaretPosition.IsSetAndValid() ||
      !newCaretPosition.GetContainer()->IsInComposedDoc()) {
    NS_WARNING(
        "HTMLEditor::DeleteTextAndNormalizeSurroundingWhiteSpaces() got lost "
        "the modifying line");
    return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }

  // Look for leaf node to put caret if we remove some empty inline ancestors
  // at new caret position.
  if (!newCaretPosition.IsInTextNode()) {
    if (const Element* editableBlockElementOrInlineEditingHost =
            HTMLEditUtils::GetInclusiveAncestorElement(
                *newCaretPosition.ContainerAsContent(),
                HTMLEditUtils::
                    ClosestEditableBlockElementOrInlineEditingHost)) {
      Element* editingHost = GetActiveEditingHost();
      // Try to put caret next to immediately after previous editable leaf.
      nsIContent* previousContent =
          HTMLEditUtils::GetPreviousLeafContentOrPreviousBlockElement(
              newCaretPosition, *editableBlockElementOrInlineEditingHost,
              {LeafNodeType::LeafNodeOrNonEditableNode}, editingHost);
      if (previousContent && !HTMLEditUtils::IsBlockElement(*previousContent)) {
        newCaretPosition =
            previousContent->IsText() ||
                    HTMLEditUtils::IsContainerNode(*previousContent)
                ? EditorDOMPoint::AtEndOf(*previousContent)
                : EditorDOMPoint::After(*previousContent);
      }
      // But if the point is very first of a block element or immediately after
      // a child block, look for next editable leaf instead.
      else if (nsIContent* nextContent =
                   HTMLEditUtils::GetNextLeafContentOrNextBlockElement(
                       newCaretPosition,
                       *editableBlockElementOrInlineEditingHost,
                       {LeafNodeType::LeafNodeOrNonEditableNode},
                       editingHost)) {
        newCaretPosition = nextContent->IsText() ||
                                   HTMLEditUtils::IsContainerNode(*nextContent)
                               ? EditorDOMPoint(nextContent, 0)
                               : EditorDOMPoint(nextContent);
      }
    }
  }

  // For compatibility with Blink, we should move caret to end of previous
  // text node if it's direct previous sibling of the first text node in the
  // range.
  if (newCaretPosition.IsStartOfContainer() &&
      newCaretPosition.IsInTextNode() &&
      newCaretPosition.GetContainer()->GetPreviousSibling() &&
      newCaretPosition.GetContainer()->GetPreviousSibling()->IsText()) {
    newCaretPosition.SetToEndOf(
        newCaretPosition.GetContainer()->GetPreviousSibling()->AsText());
  }

  {
    AutoTrackDOMPoint trackingNewCaretPosition(RangeUpdaterRef(),
                                               &newCaretPosition);
    nsresult rv = InsertBRElementIfHardLineIsEmptyAndEndsWithBlockBoundary(
        newCaretPosition);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "HTMLEditor::"
          "InsertBRElementIfHardLineIsEmptyAndEndsWithBlockBoundary() failed");
      return Err(rv);
    }
  }
  if (!newCaretPosition.IsSetAndValid()) {
    NS_WARNING("Inserting <br> element caused unexpected DOM tree");
    return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }
  return newCaretPosition;
}

nsresult HTMLEditor::InsertBRElementIfHardLineIsEmptyAndEndsWithBlockBoundary(
    const EditorDOMPoint& aPointToInsert) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(aPointToInsert.IsSet());

  if (!aPointToInsert.GetContainerAsContent()) {
    return NS_OK;
  }

  // If container of the point is not in a block, we don't need to put a
  // `<br>` element here.
  if (!HTMLEditUtils::IsBlockElement(*aPointToInsert.ContainerAsContent())) {
    return NS_OK;
  }

  WSRunScanner wsRunScanner(GetActiveEditingHost(), aPointToInsert);
  // If the point is not start of a hard line, we don't need to put a `<br>`
  // element here.
  if (!wsRunScanner.StartsFromHardLineBreak()) {
    return NS_OK;
  }
  // If the point is not end of a hard line or the hard line does not end with
  // block boundary, we don't need to put a `<br>` element here.
  if (!wsRunScanner.EndsByBlockBoundary()) {
    return NS_OK;
  }

  // If we cannot insert a `<br>` element here, do nothing.
  if (!HTMLEditUtils::CanNodeContain(*aPointToInsert.GetContainer(),
                                     *nsGkAtoms::br)) {
    return NS_OK;
  }

  Result<RefPtr<Element>, nsresult> resultOfInsertingBRElement =
      InsertBRElementWithTransaction(aPointToInsert, nsIEditor::ePrevious);
  if (resultOfInsertingBRElement.isErr()) {
    NS_WARNING("HTMLEditor::InsertBRElementWithTransaction() failed");
    return resultOfInsertingBRElement.unwrapErr();
  }
  MOZ_ASSERT(resultOfInsertingBRElement.inspect());
  return NS_OK;
}

EditActionResult HTMLEditor::MakeOrChangeListAndListItemAsSubAction(
    nsAtom& aListElementOrListItemElementTagName, const nsAString& aBulletType,
    SelectAllOfCurrentList aSelectAllOfCurrentList) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(&aListElementOrListItemElementTagName == nsGkAtoms::ul ||
             &aListElementOrListItemElementTagName == nsGkAtoms::ol ||
             &aListElementOrListItemElementTagName == nsGkAtoms::dl ||
             &aListElementOrListItemElementTagName == nsGkAtoms::dd ||
             &aListElementOrListItemElementTagName == nsGkAtoms::dt);

  if (NS_WARN_IF(!mInitSucceeded)) {
    return EditActionIgnored(NS_ERROR_NOT_INITIALIZED);
  }

  EditActionResult result = CanHandleHTMLEditSubAction();
  if (result.Failed() || result.Canceled()) {
    NS_WARNING_ASSERTION(result.Succeeded(),
                         "HTMLEditor::CanHandleHTMLEditSubAction() failed");
    return result;
  }

  if (IsSelectionRangeContainerNotContent()) {
    NS_WARNING("Some selection containers are not content node, but ignored");
    return EditActionIgnored();
  }

  AutoPlaceholderBatch treatAsOneTransaction(*this,
                                             ScrollSelectionIntoView::Yes);

  // XXX EditSubAction::eCreateOrChangeDefinitionListItem and
  //     EditSubAction::eCreateOrChangeList are treated differently in
  //     HTMLEditor::MaybeSplitElementsAtEveryBRElement().  Only when
  //     EditSubAction::eCreateOrChangeList, it splits inline nodes.
  //     Currently, it shouldn't be done when we called for formatting
  //     `<dd>` or `<dt>` by
  //     HTMLEditor::MakeDefinitionListItemWithTransaction().  But this
  //     difference may be a bug.  We should investigate this later.
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this,
      &aListElementOrListItemElementTagName == nsGkAtoms::dd ||
              &aListElementOrListItemElementTagName == nsGkAtoms::dt
          ? EditSubAction::eCreateOrChangeDefinitionListItem
          : EditSubAction::eCreateOrChangeList,
      nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return EditActionResult(ignoredError.StealNSResult());
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  nsresult rv = EnsureNoPaddingBRElementForEmptyEditor();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::EnsureNoPaddingBRElementForEmptyEditor() "
                       "failed, but ignored");

  if (NS_SUCCEEDED(rv) && SelectionRef().IsCollapsed()) {
    nsresult rv = EnsureCaretNotAfterInvisibleBRElement();
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::EnsureCaretNotAfterInvisibleBRElement() "
                         "failed, but ignored");
    if (NS_SUCCEEDED(rv)) {
      nsresult rv = PrepareInlineStylesForCaret();
      if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
        return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "HTMLEditor::PrepareInlineStylesForCaret() failed, but ignored");
    }
  }

  nsAtom* listTagName = nullptr;
  nsAtom* listItemTagName = nullptr;
  if (&aListElementOrListItemElementTagName == nsGkAtoms::ul ||
      &aListElementOrListItemElementTagName == nsGkAtoms::ol) {
    listTagName = &aListElementOrListItemElementTagName;
    listItemTagName = nsGkAtoms::li;
  } else if (&aListElementOrListItemElementTagName == nsGkAtoms::dl) {
    listTagName = &aListElementOrListItemElementTagName;
    listItemTagName = nsGkAtoms::dd;
  } else if (&aListElementOrListItemElementTagName == nsGkAtoms::dd ||
             &aListElementOrListItemElementTagName == nsGkAtoms::dt) {
    listTagName = nsGkAtoms::dl;
    listItemTagName = &aListElementOrListItemElementTagName;
  } else {
    NS_WARNING(
        "aListElementOrListItemElementTagName was neither list element name "
        "nor "
        "definition listitem element name");
    return EditActionResult(NS_ERROR_INVALID_ARG);
  }

  // Expands selection range to include the immediate block parent, and then
  // further expands to include any ancestors whose children are all in the
  // range.
  if (!SelectionRef().IsCollapsed()) {
    nsresult rv = MaybeExtendSelectionToHardLineEdgesForBlockEditAction();
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "HTMLEditor::MaybeExtendSelectionToHardLineEdgesForBlockEditAction() "
          "failed");
      return EditActionResult(rv);
    }
  }

  // ChangeSelectedHardLinesToList() creates AutoSelectionRestorer.
  // Therefore, even if it returns NS_OK, editor might have been destroyed
  // at restoring Selection.
  result = ChangeSelectedHardLinesToList(MOZ_KnownLive(*listTagName),
                                         MOZ_KnownLive(*listItemTagName),
                                         aBulletType, aSelectAllOfCurrentList);
  if (NS_WARN_IF(Destroyed())) {
    return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
  }
  NS_WARNING_ASSERTION(result.Succeeded(),
                       "HTMLEditor::ChangeSelectedHardLinesToList() failed");
  return result;
}

EditActionResult HTMLEditor::ChangeSelectedHardLinesToList(
    nsAtom& aListElementTagName, nsAtom& aListItemElementTagName,
    const nsAString& aBulletType,
    SelectAllOfCurrentList aSelectAllOfCurrentList) {
  MOZ_ASSERT(IsTopLevelEditSubActionDataAvailable());
  MOZ_ASSERT(!IsSelectionRangeContainerNotContent());

  AutoSelectionRestorer restoreSelectionLater(*this);

  AutoTArray<OwningNonNull<nsIContent>, 64> arrayOfContents;
  Element* parentListElement =
      aSelectAllOfCurrentList == SelectAllOfCurrentList::Yes
          ? GetParentListElementAtSelection()
          : nullptr;
  if (parentListElement) {
    arrayOfContents.AppendElement(
        OwningNonNull<nsIContent>(*parentListElement));
  } else {
    AutoTransactionsConserveSelection dontChangeMySelection(*this);
    nsresult rv =
        SplitInlinesAndCollectEditTargetNodesInExtendedSelectionRanges(
            arrayOfContents, EditSubAction::eCreateOrChangeList,
            CollectNonEditableNodes::No);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "HTMLEditor::"
          "SplitInlinesAndCollectEditTargetNodesInExtendedSelectionRanges("
          "eCreateOrChangeList, CollectNonEditableNodes::No) failed");
      return EditActionResult(rv);
    }
  }

  // check if all our nodes are <br>s, or empty inlines
  bool bOnlyBreaks = true;
  for (auto& content : arrayOfContents) {
    // if content is not a Break or empty inline, we're done
    if (!content->IsHTMLElement(nsGkAtoms::br) &&
        !HTMLEditUtils::IsEmptyInlineContent(content)) {
      bOnlyBreaks = false;
      break;
    }
  }

  // if no nodes, we make empty list.  Ditto if the user tried to make a list
  // of some # of breaks.
  if (arrayOfContents.IsEmpty() || bOnlyBreaks) {
    // if only breaks, delete them
    if (bOnlyBreaks) {
      for (auto& content : arrayOfContents) {
        // MOZ_KnownLive because 'arrayOfContents' is guaranteed to
        // keep it alive.
        nsresult rv = DeleteNodeWithTransaction(MOZ_KnownLive(*content));
        if (NS_WARN_IF(Destroyed())) {
          return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
        }
        if (NS_FAILED(rv)) {
          NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
          return EditActionResult(rv);
        }
      }
    }

    const nsRange* firstRange = SelectionRef().GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return EditActionResult(NS_ERROR_FAILURE);
    }

    EditorDOMPoint atStartOfSelection(firstRange->StartRef());
    if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
      return EditActionResult(NS_ERROR_FAILURE);
    }

    // Make sure we can put a list here.
    if (!HTMLEditUtils::CanNodeContain(*atStartOfSelection.GetContainer(),
                                       aListElementTagName)) {
      return EditActionCanceled();
    }

    Result<RefPtr<Element>, nsresult> newListElementOrError =
        InsertElementWithSplittingAncestorsWithTransaction(
            aListElementTagName, atStartOfSelection,
            BRElementNextToSplitPoint::Keep);
    if (MOZ_UNLIKELY(newListElementOrError.isErr())) {
      NS_WARNING(
          nsPrintfCString(
              "HTMLEditor::InsertElementWithSplittingAncestorsWithTransaction("
              "%s) failed",
              nsAtomCString(&aListElementTagName).get())
              .get());
      return EditActionResult(newListElementOrError.unwrapErr());
    }
    MOZ_ASSERT(newListElementOrError.inspect());

    Result<RefPtr<Element>, nsresult> newListItemElementOrError =
        CreateAndInsertElementWithTransaction(
            aListItemElementTagName,
            EditorDOMPoint(newListElementOrError.inspect(), 0));
    if (newListElementOrError.isErr()) {
      NS_WARNING("HTMLEditor::CreateAndInsertElementWithTransaction() failed");
      return EditActionResult(newListElementOrError.unwrapErr());
    }
    MOZ_ASSERT(newListElementOrError.inspect());

    // remember our new block for postprocessing
    TopLevelEditSubActionDataRef().mNewBlockElement =
        newListElementOrError.inspect();
    // Put selection in new list item and don't restore the Selection.
    restoreSelectionLater.Abort();
    nsresult rv = CollapseSelectionToStartOf(
        MOZ_KnownLive(*newListElementOrError.inspect()));
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::CollapseSelectionToStartOf() failed");
    return EditActionResult(rv);
  }

  // if there is only one node in the array, and it is a list, div, or
  // blockquote, then look inside of it until we find inner list or content.
  if (arrayOfContents.Length() == 1) {
    if (Element* deepestDivBlockquoteOrListElement =
            HTMLEditUtils::GetInclusiveDeepestFirstChildWhichHasOneChild(
                arrayOfContents[0], {WalkTreeOption::IgnoreNonEditableNode},
                nsGkAtoms::div, nsGkAtoms::blockquote, nsGkAtoms::ul,
                nsGkAtoms::ol, nsGkAtoms::dl)) {
      if (deepestDivBlockquoteOrListElement->IsAnyOfHTMLElements(
              nsGkAtoms::div, nsGkAtoms::blockquote)) {
        arrayOfContents.Clear();
        CollectChildren(*deepestDivBlockquoteOrListElement, arrayOfContents, 0,
                        CollectListChildren::No, CollectTableChildren::No,
                        CollectNonEditableNodes::Yes);
      } else {
        arrayOfContents.ReplaceElementAt(
            0, OwningNonNull<nsIContent>(*deepestDivBlockquoteOrListElement));
      }
    }
  }

  // Ok, now go through all the nodes and put then in the list,
  // or whatever is approriate.  Wohoo!

  uint32_t countOfCollectedContents = arrayOfContents.Length();
  RefPtr<Element> curList, prevListItem;

  for (uint32_t i = 0; i < countOfCollectedContents; i++) {
    // here's where we actually figure out what to do
    OwningNonNull<nsIContent> content = arrayOfContents[i];

    // make sure we don't assemble content that is in different table cells
    // into the same list.  respect table cell boundaries when listifying.
    if (curList &&
        HTMLEditUtils::GetInclusiveAncestorAnyTableElement(*curList) !=
            HTMLEditUtils::GetInclusiveAncestorAnyTableElement(content)) {
      curList = nullptr;
    }

    // If current node is a `<br>` element, delete it and forget previous
    // list item element.
    // If current node is an empty inline node, just delete it.
    if (EditorUtils::IsEditableContent(content, EditorType::HTML) &&
        (content->IsHTMLElement(nsGkAtoms::br) ||
         HTMLEditUtils::IsEmptyInlineContent(content))) {
      nsresult rv = DeleteNodeWithTransaction(*content);
      if (NS_WARN_IF(Destroyed())) {
        return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
      }
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
        return EditActionResult(rv);
      }
      if (content->IsHTMLElement(nsGkAtoms::br)) {
        prevListItem = nullptr;
      }
      continue;
    }

    if (HTMLEditUtils::IsAnyListElement(content)) {
      // If we met a list element and current list element is not a descendant
      // of the list, append current node to end of the current list element.
      // Then, wrap it with list item element and delete the old container.
      if (curList && !EditorUtils::IsDescendantOf(*content, *curList)) {
        nsresult rv = MoveNodeToEndWithTransaction(*content, *curList);
        if (NS_WARN_IF(Destroyed())) {
          return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
        }
        if (NS_FAILED(rv)) {
          NS_WARNING("HTMLEditor::MoveNodeToEndWithTransaction() failed");
          return EditActionResult(rv);
        }
        CreateElementResult convertListTypeResult =
            ChangeListElementType(MOZ_KnownLive(*content->AsElement()),
                                  aListElementTagName, aListItemElementTagName);
        if (convertListTypeResult.Failed()) {
          NS_WARNING("HTMLEditor::ChangeListElementType() failed");
          return EditActionResult(convertListTypeResult.Rv());
        }
        rv = RemoveBlockContainerWithTransaction(
            MOZ_KnownLive(*convertListTypeResult.GetNewNode()));
        if (NS_WARN_IF(Destroyed())) {
          return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
        }
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "HTMLEditor::RemoveBlockContainerWithTransaction() failed");
          return EditActionResult(rv);
        }
        prevListItem = nullptr;
        continue;
      }

      // If current list element is in found list element or we've not met a
      // list element, convert current list element to proper type.
      CreateElementResult convertListTypeResult =
          ChangeListElementType(MOZ_KnownLive(*content->AsElement()),
                                aListElementTagName, aListItemElementTagName);
      if (convertListTypeResult.Failed()) {
        NS_WARNING("HTMLEditor::ChangeListElementType() failed");
        return EditActionResult(convertListTypeResult.Rv());
      }
      curList = convertListTypeResult.forget();
      prevListItem = nullptr;
      continue;
    }

    EditorDOMPoint atContent(content);
    if (NS_WARN_IF(!atContent.IsSet())) {
      return EditActionResult(NS_ERROR_FAILURE);
    }
    MOZ_ASSERT(atContent.IsSetAndValid());
    if (HTMLEditUtils::IsListItem(content)) {
      // If current list item element is not in proper list element, we need
      // to conver the list element.
      if (!atContent.IsContainerHTMLElement(&aListElementTagName)) {
        // If we've not met a list element or current node is not in current
        // list element, insert a list element at current node and set
        // current list element to the new one.
        if (!curList || EditorUtils::IsDescendantOf(*content, *curList)) {
          if (NS_WARN_IF(!atContent.GetContainerAsContent())) {
            return EditActionResult(NS_ERROR_FAILURE);
          }
          SplitNodeResult splitListItemParentResult =
              SplitNodeWithTransaction(atContent);
          if (MOZ_UNLIKELY(splitListItemParentResult.Failed())) {
            NS_WARNING("HTMLEditor::SplitNodeWithTransaction() failed");
            return EditActionResult(splitListItemParentResult.Rv());
          }
          MOZ_ASSERT(splitListItemParentResult.DidSplit());
          Result<RefPtr<Element>, nsresult> maybeNewListElement =
              CreateAndInsertElementWithTransaction(
                  aListElementTagName,
                  splitListItemParentResult.AtNextContent<EditorDOMPoint>());
          if (maybeNewListElement.isErr()) {
            NS_WARNING(
                "HTMLEditor::CreateAndInsertElementWithTransaction() failed");
            return EditActionResult(maybeNewListElement.unwrapErr());
          }
          MOZ_ASSERT(maybeNewListElement.inspect());
          curList = maybeNewListElement.unwrap();
        }
        // Then, move current node into current list element.
        nsresult rv = MoveNodeToEndWithTransaction(*content, *curList);
        if (NS_WARN_IF(Destroyed())) {
          return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
        }
        if (NS_FAILED(rv)) {
          NS_WARNING("HTMLEditor::MoveNodeToEndWithTransaction() failed");
          return EditActionResult(rv);
        }
        // Convert list item type if current node is different list item type.
        if (!content->IsHTMLElement(&aListItemElementTagName)) {
          RefPtr<Element> newListItemElement = ReplaceContainerWithTransaction(
              MOZ_KnownLive(*content->AsElement()), aListItemElementTagName);
          if (NS_WARN_IF(Destroyed())) {
            return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
          }
          if (!newListItemElement) {
            NS_WARNING("HTMLEditor::ReplaceContainerWithTransaction() failed");
            return EditActionResult(NS_ERROR_FAILURE);
          }
        }
      } else {
        // If we've not met a list element, set current list element to the
        // parent of current list item element.
        if (!curList) {
          curList = atContent.GetContainerAsElement();
          NS_WARNING_ASSERTION(
              HTMLEditUtils::IsAnyListElement(curList),
              "Current list item parent is not a list element");
        }
        // If current list item element is not a child of current list element,
        // move it into current list item.
        else if (atContent.GetContainer() != curList) {
          nsresult rv = MoveNodeToEndWithTransaction(*content, *curList);
          if (NS_WARN_IF(Destroyed())) {
            return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
          }
          if (NS_FAILED(rv)) {
            NS_WARNING("HTMLEditor::MoveNodeToEndWithTransaction() failed");
            return EditActionResult(rv);
          }
        }
        // Then, if current list item element is not proper type for current
        // list element, convert list item element to proper element.
        if (!content->IsHTMLElement(&aListItemElementTagName)) {
          RefPtr<Element> newListItemElement = ReplaceContainerWithTransaction(
              MOZ_KnownLive(*content->AsElement()), aListItemElementTagName);
          if (NS_WARN_IF(Destroyed())) {
            return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
          }
          if (!newListItemElement) {
            NS_WARNING("HTMLEditor::ReplaceContainerWithTransaction() failed");
            return EditActionResult(NS_ERROR_FAILURE);
          }
        }
      }
      Element* element = Element::FromNode(content);
      if (NS_WARN_IF(!element)) {
        return EditActionResult(NS_ERROR_FAILURE);
      }
      // If bullet type is specified, set list type attribute.
      // XXX Cannot we set type attribute before inserting the list item
      //     element into the DOM tree?
      if (!aBulletType.IsEmpty()) {
        nsresult rv = SetAttributeWithTransaction(
            MOZ_KnownLive(*element), *nsGkAtoms::type, aBulletType);
        if (NS_WARN_IF(Destroyed())) {
          return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
        }
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "EditorBase::SetAttributeWithTransaction(nsGkAtoms::type) "
              "failed");
          return EditActionResult(rv);
        }
        continue;
      }

      // Otherwise, remove list type attribute if there is.
      if (!element->HasAttr(nsGkAtoms::type)) {
        continue;
      }
      nsresult rv = RemoveAttributeWithTransaction(MOZ_KnownLive(*element),
                                                   *nsGkAtoms::type);
      if (NS_WARN_IF(Destroyed())) {
        return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
      }
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "EditorBase::RemoveAttributeWithTransaction(nsGkAtoms::type) "
            "failed");
        return EditActionResult(rv);
      }
      continue;
    }

    MOZ_ASSERT(!HTMLEditUtils::IsAnyListElement(content) &&
               !HTMLEditUtils::IsListItem(content));

    // If current node is a `<div>` element, replace it in the array with
    // its children.
    // XXX I think that this should be done when we collect the nodes above.
    //     Then, we can change this `for` loop to ranged-for loop.
    if (content->IsHTMLElement(nsGkAtoms::div)) {
      prevListItem = nullptr;
      CollectChildren(*content, arrayOfContents, i + 1,
                      CollectListChildren::Yes, CollectTableChildren::Yes,
                      CollectNonEditableNodes::Yes);
      nsresult rv =
          RemoveContainerWithTransaction(MOZ_KnownLive(*content->AsElement()));
      if (NS_WARN_IF(Destroyed())) {
        return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
      }
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::RemoveContainerWithTransaction() failed");
        return EditActionResult(rv);
      }
      // Extend the loop length to handle all children collected here.
      countOfCollectedContents = arrayOfContents.Length();
      continue;
    }

    // If we've not met a list element, create a list element and make it
    // current list element.
    if (!curList) {
      prevListItem = nullptr;
      Result<RefPtr<Element>, nsresult> newListElementOrError =
          InsertElementWithSplittingAncestorsWithTransaction(
              aListElementTagName, atContent, BRElementNextToSplitPoint::Keep);
      if (MOZ_UNLIKELY(newListElementOrError.isErr())) {
        NS_WARNING(
            nsPrintfCString(
                "HTMLEditor::"
                "InsertElementWithSplittingAncestorsWithTransaction(%s) failed",
                nsAtomCString(&aListElementTagName).get())
                .get());
        return EditActionResult(newListElementOrError.unwrapErr());
      }
      MOZ_ASSERT(newListElementOrError.inspect());
      curList = newListElementOrError.unwrap();
      // Set new block element of top level edit sub-action to the new list
      // element for setting selection into it.
      // XXX This must be wrong.  If we're handling nested edit action,
      //     we shouldn't overwrite the new block element.
      TopLevelEditSubActionDataRef().mNewBlockElement = curList;

      // atContent is now referring the right node with mOffset but
      // referring the left node with mRef.  So, invalidate it now.
      atContent.Clear();
    }

    // If we're currently handling contents of a list item and current node
    // is not a block element, move current node into the list item.
    if (HTMLEditUtils::IsInlineElement(content) && prevListItem) {
      nsresult rv = MoveNodeToEndWithTransaction(*content, *prevListItem);
      if (NS_WARN_IF(Destroyed())) {
        return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
      }
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::MoveNodeToEndWithTransaction() failed");
        return EditActionResult(rv);
      }
      continue;
    }

    // If current node is a paragraph, that means that it does not contain
    // block children so that we can just replace it with new list item
    // element and move it into current list element.
    // XXX This is too rough handling.  If web apps modifies DOM tree directly,
    //     any elements can have block elements as children.
    if (content->IsHTMLElement(nsGkAtoms::p)) {
      RefPtr<Element> newListItemElement = ReplaceContainerWithTransaction(
          MOZ_KnownLive(*content->AsElement()), aListItemElementTagName);
      if (NS_WARN_IF(Destroyed())) {
        return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
      }
      if (!newListItemElement) {
        NS_WARNING("HTMLEditor::ReplaceContainerWithTransaction() failed");
        return EditActionResult(NS_ERROR_FAILURE);
      }
      prevListItem = nullptr;
      nsresult rv = MoveNodeToEndWithTransaction(*newListItemElement, *curList);
      if (NS_WARN_IF(Destroyed())) {
        return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
      }
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::MoveNodeToEndWithTransaction() failed");
        return EditActionResult(rv);
      }
      // XXX Why don't we set `type` attribute here??
      continue;
    }

    // If current node is not a paragraph, wrap current node with new list
    // item element and move it into current list element.
    RefPtr<Element> newListItemElement =
        InsertContainerWithTransaction(*content, aListItemElementTagName);
    if (NS_WARN_IF(Destroyed())) {
      return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
    }
    if (!newListItemElement) {
      NS_WARNING("HTMLEditor::InsertContainerWithTransaction() failed");
      return EditActionResult(NS_ERROR_FAILURE);
    }
    // If current node is not a block element, new list item should have
    // following inline nodes too.
    if (HTMLEditUtils::IsInlineElement(content)) {
      prevListItem = newListItemElement;
    } else {
      prevListItem = nullptr;
    }
    nsresult rv = MoveNodeToEndWithTransaction(*newListItemElement, *curList);
    if (NS_WARN_IF(Destroyed())) {
      return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::MoveNodeToEndWithTransaction() failed");
      return EditActionResult(rv);
    }
    // XXX Why don't we set `type` attribute here??
  }

  return EditActionHandled();
}

nsresult HTMLEditor::RemoveListAtSelectionAsSubAction() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  EditActionResult result = CanHandleHTMLEditSubAction();
  if (result.Failed() || result.Canceled()) {
    NS_WARNING_ASSERTION(result.Succeeded(),
                         "HTMLEditor::CanHandleHTMLEditSubAction() failed");
    return result.Rv();
  }

  AutoPlaceholderBatch treatAsOneTransaction(*this,
                                             ScrollSelectionIntoView::Yes);
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eRemoveList, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  if (!SelectionRef().IsCollapsed()) {
    nsresult rv = MaybeExtendSelectionToHardLineEdgesForBlockEditAction();
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "HTMLEditor::MaybeExtendSelectionToHardLineEdgesForBlockEditAction() "
          "failed");
      return rv;
    }
  }

  AutoSelectionRestorer restoreSelectionLater(*this);

  AutoTArray<OwningNonNull<nsIContent>, 64> arrayOfContents;
  {
    AutoTransactionsConserveSelection dontChangeMySelection(*this);
    nsresult rv =
        SplitInlinesAndCollectEditTargetNodesInExtendedSelectionRanges(
            arrayOfContents, EditSubAction::eCreateOrChangeList,
            CollectNonEditableNodes::No);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "HTMLEditor::"
          "SplitInlinesAndCollectEditTargetNodesInExtendedSelectionRanges("
          "eCreateOrChangeList, CollectNonEditableNodes::No) failed");
      return rv;
    }
  }

  // Remove all non-editable nodes.  Leave them be.
  // XXX SplitInlinesAndCollectEditTargetNodesInExtendedSelectionRanges()
  //     should return only editable contents when it's called with
  //     CollectNonEditableNodes::No.
  for (int32_t i = arrayOfContents.Length() - 1; i >= 0; i--) {
    OwningNonNull<nsIContent>& content = arrayOfContents[i];
    if (!EditorUtils::IsEditableContent(content, EditorType::HTML)) {
      arrayOfContents.RemoveElementAt(i);
    }
  }

  // Only act on lists or list items in the array
  for (auto& content : arrayOfContents) {
    // here's where we actually figure out what to do
    if (HTMLEditUtils::IsListItem(content)) {
      // unlist this listitem
      nsresult rv = LiftUpListItemElement(MOZ_KnownLive(*content->AsElement()),
                                          LiftUpFromAllParentListElements::Yes);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "HTMLEditor::LiftUpListItemElement(LiftUpFromAllParentListElements:"
            ":Yes) failed");
        return rv;
      }
      continue;
    }
    if (HTMLEditUtils::IsAnyListElement(content)) {
      // node is a list, move list items out
      nsresult rv =
          DestroyListStructureRecursively(MOZ_KnownLive(*content->AsElement()));
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::DestroyListStructureRecursively() failed");
        return rv;
      }
      continue;
    }
  }
  return NS_OK;
}

nsresult HTMLEditor::FormatBlockContainerWithTransaction(nsAtom& blockType) {
  MOZ_ASSERT(IsTopLevelEditSubActionDataAvailable());

  if (!SelectionRef().IsCollapsed()) {
    nsresult rv = MaybeExtendSelectionToHardLineEdgesForBlockEditAction();
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "HTMLEditor::MaybeExtendSelectionToHardLineEdgesForBlockEditAction() "
          "failed");
      return rv;
    }
  }

  AutoSelectionRestorer restoreSelectionLater(*this);
  AutoTransactionsConserveSelection dontChangeMySelection(*this);

  AutoTArray<OwningNonNull<nsIContent>, 64> arrayOfContents;
  nsresult rv = SplitInlinesAndCollectEditTargetNodesInExtendedSelectionRanges(
      arrayOfContents, EditSubAction::eCreateOrRemoveBlock,
      CollectNonEditableNodes::Yes);
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "HTMLEditor::"
        "SplitInlinesAndCollectEditTargetNodesInExtendedSelectionRanges("
        "eCreateOrRemoveBlock, CollectNonEditableNodes::Yes) failed");
    return rv;
  }

  // If there is no visible and editable nodes in the edit targets, make an
  // empty block.
  // XXX Isn't this odd if there are only non-editable visible nodes?
  if (HTMLEditUtils::IsEmptyOneHardLine(arrayOfContents)) {
    const nsRange* firstRange = SelectionRef().GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return NS_ERROR_FAILURE;
    }

    EditorDOMPoint pointToInsertBlock(firstRange->StartRef());
    if (&blockType == nsGkAtoms::normal || &blockType == nsGkAtoms::_empty) {
      if (!pointToInsertBlock.IsInContentNode()) {
        NS_WARNING(
            "HTMLEditor::FormatBlockContainerWithTransaction() couldn't find "
            "block parent because container of the point is not content");
        return NS_ERROR_FAILURE;
      }
      // We are removing blocks (going to "body text")
      const RefPtr<Element> editableBlockElement =
          HTMLEditUtils::GetInclusiveAncestorElement(
              *pointToInsertBlock.ContainerAsContent(),
              HTMLEditUtils::ClosestEditableBlockElement);
      if (!editableBlockElement) {
        NS_WARNING(
            "HTMLEditor::FormatBlockContainerWithTransaction() couldn't find "
            "block parent");
        return NS_ERROR_FAILURE;
      }
      if (!HTMLEditUtils::IsFormatNode(editableBlockElement)) {
        return NS_OK;
      }

      // If the first editable node after selection is a br, consume it.
      // Otherwise it gets pushed into a following block after the split,
      // which is visually bad.
      // XXX Why do we keep handling it when there is no editing host?
      if (Element* editingHost = GetActiveEditingHost()) {
        if (nsCOMPtr<nsIContent> brContent = HTMLEditUtils::GetNextContent(
                pointToInsertBlock, {WalkTreeOption::IgnoreNonEditableNode},
                editingHost)) {
          if (brContent && brContent->IsHTMLElement(nsGkAtoms::br)) {
            AutoEditorDOMPointChildInvalidator lockOffset(pointToInsertBlock);
            rv = DeleteNodeWithTransaction(*brContent);
            if (NS_WARN_IF(Destroyed())) {
              return NS_ERROR_EDITOR_DESTROYED;
            }
            if (NS_FAILED(rv)) {
              NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
              return rv;
            }
          }
        }
      }
      // Do the splits!
      SplitNodeResult splitNodeResult = SplitNodeDeepWithTransaction(
          *editableBlockElement, pointToInsertBlock,
          SplitAtEdges::eDoNotCreateEmptyContainer);
      if (MOZ_UNLIKELY(splitNodeResult.Failed())) {
        NS_WARNING("HTMLEditor::SplitNodeDeepWithTransaction() failed");
        return splitNodeResult.Rv();
      }
      // Put a <br> element at the split point
      Result<RefPtr<Element>, nsresult> resultOfInsertingBRElement =
          InsertBRElementWithTransaction(
              splitNodeResult.AtSplitPoint<EditorDOMPoint>());
      if (resultOfInsertingBRElement.isErr()) {
        NS_WARNING("HTMLEditor::InsertBRElementWithTransaction() failed");
        return resultOfInsertingBRElement.unwrapErr();
      }
      MOZ_ASSERT(resultOfInsertingBRElement.inspect());
      // Don't restore the selection
      restoreSelectionLater.Abort();
      // Put selection at the split point
      nsresult rv = CollapseSelectionTo(
          EditorRawDOMPoint(resultOfInsertingBRElement.inspect()));
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "HTMLEditor::CollapseSelectionTo() failed");
      return rv;
    }

    // We are making a block.  Consume a br, if needed.
    // XXX Perhaps, we should stop handling this method when there is no
    //     editing host.
    if (Element* editingHost = GetActiveEditingHost()) {
      if (nsCOMPtr<nsIContent> maybeBRContent = HTMLEditUtils::GetNextContent(
              pointToInsertBlock,
              {WalkTreeOption::IgnoreNonEditableNode,
               WalkTreeOption::StopAtBlockBoundary},
              editingHost)) {
        if (maybeBRContent->IsHTMLElement(nsGkAtoms::br)) {
          AutoEditorDOMPointChildInvalidator lockOffset(pointToInsertBlock);
          rv = DeleteNodeWithTransaction(*maybeBRContent);
          if (NS_WARN_IF(Destroyed())) {
            return NS_ERROR_EDITOR_DESTROYED;
          }
          if (NS_FAILED(rv)) {
            NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
            return rv;
          }
          // We don't need to act on this node any more
          arrayOfContents.RemoveElement(maybeBRContent);
        }
      }
    }
    // Make sure we can put a block here.
    Result<RefPtr<Element>, nsresult> newBlockElementOrError =
        InsertElementWithSplittingAncestorsWithTransaction(
            blockType, pointToInsertBlock, BRElementNextToSplitPoint::Keep);
    if (MOZ_UNLIKELY(newBlockElementOrError.isErr())) {
      NS_WARNING(
          nsPrintfCString(
              "HTMLEditor::InsertElementWithSplittingAncestorsWithTransaction("
              "%s) failed",
              nsAtomCString(&blockType).get())
              .get());
      return newBlockElementOrError.unwrapErr();
    }
    MOZ_ASSERT(newBlockElementOrError.inspect());
    // Remember our new block for postprocessing
    TopLevelEditSubActionDataRef().mNewBlockElement =
        newBlockElementOrError.inspect();
    // Delete anything that was in the list of nodes
    while (!arrayOfContents.IsEmpty()) {
      OwningNonNull<nsIContent>& content = arrayOfContents[0];
      // MOZ_KnownLive because 'arrayOfContents' is guaranteed to
      // keep it alive.
      rv = DeleteNodeWithTransaction(MOZ_KnownLive(*content));
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
        return rv;
      }
      arrayOfContents.RemoveElementAt(0);
    }
    // Don't restore the selection
    restoreSelectionLater.Abort();
    // Put selection in new block
    rv = CollapseSelectionToStartOf(
        MOZ_KnownLive(*newBlockElementOrError.inspect()));
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::CollapseSelectionToStartOf() failed");
    return rv;
  }
  // Okay, now go through all the nodes and make the right kind of blocks, or
  // whatever is approriate.  Woohoo!  Note: blockquote is handled a little
  // differently.
  if (&blockType == nsGkAtoms::blockquote) {
    nsresult rv = MoveNodesIntoNewBlockquoteElement(arrayOfContents);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::MoveNodesIntoNewBlockquoteElement() failed");
    return rv;
  }
  if (&blockType == nsGkAtoms::normal || &blockType == nsGkAtoms::_empty) {
    nsresult rv = RemoveBlockContainerElements(arrayOfContents);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::RemoveBlockContainerElements() failed");
    return rv;
  }
  rv = CreateOrChangeBlockContainerElement(arrayOfContents, blockType);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::CreateOrChangeBlockContainerElement() failed");
  return rv;
}

nsresult HTMLEditor::MaybeInsertPaddingBRElementForEmptyLastLineAtSelection() {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!IsSelectionRangeContainerNotContent());

  if (!SelectionRef().IsCollapsed()) {
    return NS_OK;
  }

  const nsRange* firstRange = SelectionRef().GetRangeAt(0);
  if (NS_WARN_IF(!firstRange)) {
    return NS_ERROR_FAILURE;
  }
  const RangeBoundary& atStartOfSelection = firstRange->StartRef();
  if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
    return NS_ERROR_FAILURE;
  }
  if (!atStartOfSelection.Container()->IsElement()) {
    return NS_OK;
  }
  OwningNonNull<Element> startContainerElement =
      *atStartOfSelection.Container()->AsElement();
  nsresult rv =
      InsertPaddingBRElementForEmptyLastLineIfNeeded(startContainerElement);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::InsertPaddingBRElementForEmptyLastLineIfNeeded() failed");
  return rv;
}

EditActionResult HTMLEditor::IndentAsSubAction() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  AutoPlaceholderBatch treatAsOneTransaction(*this,
                                             ScrollSelectionIntoView::Yes);
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eIndent, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return EditActionResult(ignoredError.StealNSResult());
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  EditActionResult result = CanHandleHTMLEditSubAction();
  if (result.Failed() || result.Canceled()) {
    NS_WARNING_ASSERTION(result.Succeeded(),
                         "HTMLEditor::CanHandleHTMLEditSubAction() failed");
    return result;
  }

  if (IsSelectionRangeContainerNotContent()) {
    NS_WARNING("Some selection containers are not content node, but ignored");
    return EditActionIgnored();
  }

  result |= HandleIndentAtSelection();
  if (result.Failed() || result.Canceled()) {
    NS_WARNING_ASSERTION(result.Succeeded(),
                         "HTMLEditor::HandleIndentAtSelection() failed");
    return result;
  }

  if (IsSelectionRangeContainerNotContent()) {
    NS_WARNING("Mutation event listener might have changed selection");
    return EditActionHandled(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }

  nsresult rv = MaybeInsertPaddingBRElementForEmptyLastLineAtSelection();
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "MaybeInsertPaddingBRElementForEmptyLastLineAtSelection() failed");
  return result.SetResult(rv);
}

// Helper for Handle[CSS|HTML]IndentAtSelectionInternal
nsresult HTMLEditor::IndentListChild(RefPtr<Element>* aCurList,
                                     const EditorDOMPoint& aCurPoint,
                                     nsIContent& aContent) {
  MOZ_ASSERT(HTMLEditUtils::IsAnyListElement(aCurPoint.GetContainer()),
             "unexpected container");
  MOZ_ASSERT(IsTopLevelEditSubActionDataAvailable());

  // some logic for putting list items into nested lists...

  // Check for whether we should join a list that follows aContent.
  // We do this if the next element is a list, and the list is of the
  // same type (li/ol) as aContent was a part it.
  if (nsIContent* nextEditableSibling = HTMLEditUtils::GetNextSibling(
          aContent, {WalkTreeOption::IgnoreWhiteSpaceOnlyText,
                     WalkTreeOption::IgnoreNonEditableNode})) {
    if (HTMLEditUtils::IsAnyListElement(nextEditableSibling) &&
        aCurPoint.GetContainer()->NodeInfo()->NameAtom() ==
            nextEditableSibling->NodeInfo()->NameAtom() &&
        aCurPoint.GetContainer()->NodeInfo()->NamespaceID() ==
            nextEditableSibling->NodeInfo()->NamespaceID()) {
      nsresult rv = MoveNodeWithTransaction(
          aContent, EditorDOMPoint(nextEditableSibling, 0));
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "EdigtorBase::MoveNodeWithTransaction() failed");
      return rv;
    }
  }

  // Check for whether we should join a list that preceeds aContent.
  // We do this if the previous element is a list, and the list is of
  // the same type (li/ol) as aContent was a part of.
  if (nsCOMPtr<nsIContent> previousEditableSibling =
          HTMLEditUtils::GetPreviousSibling(
              aContent, {WalkTreeOption::IgnoreWhiteSpaceOnlyText,
                         WalkTreeOption::IgnoreNonEditableNode})) {
    if (HTMLEditUtils::IsAnyListElement(previousEditableSibling) &&
        aCurPoint.GetContainer()->NodeInfo()->NameAtom() ==
            previousEditableSibling->NodeInfo()->NameAtom() &&
        aCurPoint.GetContainer()->NodeInfo()->NamespaceID() ==
            previousEditableSibling->NodeInfo()->NamespaceID()) {
      nsresult rv =
          MoveNodeToEndWithTransaction(aContent, *previousEditableSibling);
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "HTMLEditor::MoveNodeToEndWithTransaction() failed");
      return rv;
    }
  }

  // check to see if aCurList is still appropriate.  Which it is if
  // aContent is still right after it in the same list.
  nsIContent* previousEditableSibling =
      *aCurList ? HTMLEditUtils::GetPreviousSibling(
                      aContent, {WalkTreeOption::IgnoreWhiteSpaceOnlyText,
                                 WalkTreeOption::IgnoreNonEditableNode})
                : nullptr;
  if (!*aCurList ||
      (previousEditableSibling && previousEditableSibling != *aCurList)) {
    nsAtom* containerName = aCurPoint.GetContainer()->NodeInfo()->NameAtom();
    // Create a new nested list of correct type.
    Result<RefPtr<Element>, nsresult> newListElementOrError =
        InsertElementWithSplittingAncestorsWithTransaction(
            MOZ_KnownLive(*containerName), aCurPoint,
            BRElementNextToSplitPoint::Keep);
    if (MOZ_UNLIKELY(newListElementOrError.isErr())) {
      NS_WARNING(
          nsPrintfCString(
              "HTMLEditor::InsertElementWithSplittingAncestorsWithTransaction("
              "%s) failed",
              nsAtomCString(containerName).get())
              .get());
      return newListElementOrError.unwrapErr();
    }
    MOZ_ASSERT(newListElementOrError.inspect());
    // aCurList is now the correct thing to put aContent in
    // remember our new block for postprocessing
    TopLevelEditSubActionDataRef().mNewBlockElement =
        newListElementOrError.inspect();
    *aCurList = newListElementOrError.unwrap();
  }
  // tuck the node into the end of the active list
  RefPtr<nsINode> container = *aCurList;
  nsresult rv = MoveNodeToEndWithTransaction(aContent, *container);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::MoveNodeToEndWithTransaction() failed");
  return rv;
}

EditActionResult HTMLEditor::HandleIndentAtSelection() {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!IsSelectionRangeContainerNotContent());

  nsresult rv = EnsureNoPaddingBRElementForEmptyEditor();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::EnsureNoPaddingBRElementForEmptyEditor() "
                       "failed, but ignored");

  if (NS_SUCCEEDED(rv) && SelectionRef().IsCollapsed()) {
    nsresult rv = EnsureCaretNotAfterInvisibleBRElement();
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::EnsureCaretNotAfterInvisibleBRElement() "
                         "failed, but ignored");
    if (NS_SUCCEEDED(rv)) {
      nsresult rv = PrepareInlineStylesForCaret();
      if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
        return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "HTMLEditor::PrepareInlineStylesForCaret() failed, but ignored");
    }
  }

  if (IsSelectionRangeContainerNotContent()) {
    NS_WARNING("Mutation event listener might have changed the selection");
    return EditActionHandled(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }

  if (IsCSSEnabled()) {
    nsresult rv = HandleCSSIndentAtSelection();
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::HandleCSSIndentAtSelection() failed");
    return EditActionHandled(rv);
  }
  rv = HandleHTMLIndentAtSelection();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::HandleHTMLIndent() failed");
  return EditActionHandled(rv);
}

nsresult HTMLEditor::HandleCSSIndentAtSelection() {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!IsSelectionRangeContainerNotContent());

  if (!SelectionRef().IsCollapsed()) {
    nsresult rv = MaybeExtendSelectionToHardLineEdgesForBlockEditAction();
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "HTMLEditor::MaybeExtendSelectionToHardLineEdgesForBlockEditAction() "
          "failed");
      return rv;
    }
  }

  // HandleCSSIndentAtSelectionInternal() creates AutoSelectionRestorer.
  // Therefore, even if it returns NS_OK, editor might have been destroyed
  // at restoring Selection.
  nsresult rv = HandleCSSIndentAtSelectionInternal();
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::HandleCSSIndentAtSelectionInternal() failed");
  return rv;
}

nsresult HTMLEditor::HandleCSSIndentAtSelectionInternal() {
  MOZ_ASSERT(IsTopLevelEditSubActionDataAvailable());
  MOZ_ASSERT(!IsSelectionRangeContainerNotContent());

  AutoSelectionRestorer restoreSelectionLater(*this);
  AutoTArray<OwningNonNull<nsIContent>, 64> arrayOfContents;

  // short circuit: detect case of collapsed selection inside an <li>.
  // just sublist that <li>.  This prevents bug 97797.

  if (SelectionRef().IsCollapsed()) {
    EditorRawDOMPoint atCaret(EditorBase::GetStartPoint(SelectionRef()));
    if (NS_WARN_IF(!atCaret.IsSet())) {
      return NS_ERROR_FAILURE;
    }
    MOZ_ASSERT(atCaret.IsInContentNode());
    Element* const editableBlockElement =
        HTMLEditUtils::GetInclusiveAncestorElement(
            *atCaret.ContainerAsContent(),
            HTMLEditUtils::ClosestEditableBlockElement);
    if (editableBlockElement &&
        HTMLEditUtils::IsListItem(editableBlockElement)) {
      arrayOfContents.AppendElement(*editableBlockElement);
    }
  }

  if (arrayOfContents.IsEmpty()) {
    nsresult rv =
        SplitInlinesAndCollectEditTargetNodesInExtendedSelectionRanges(
            arrayOfContents, EditSubAction::eIndent,
            CollectNonEditableNodes::Yes);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "HTMLEditor::"
          "SplitInlinesAndCollectEditTargetNodesInExtendedSelectionRanges("
          "eIndent, CollectNonEditableNodes::Yes) failed");
      return rv;
    }
  }

  // If there is no visible and editable nodes in the edit targets, make an
  // empty block.
  // XXX Isn't this odd if there are only non-editable visible nodes?
  if (HTMLEditUtils::IsEmptyOneHardLine(arrayOfContents)) {
    // get selection location
    const nsRange* firstRange = SelectionRef().GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return NS_ERROR_FAILURE;
    }

    EditorDOMPoint atStartOfSelection(firstRange->StartRef());
    if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
      return NS_ERROR_FAILURE;
    }

    // make sure we can put a block here
    Result<RefPtr<Element>, nsresult> newDivElementOrError =
        InsertElementWithSplittingAncestorsWithTransaction(
            *nsGkAtoms::div, atStartOfSelection,
            BRElementNextToSplitPoint::Keep);
    if (MOZ_UNLIKELY(newDivElementOrError.isErr())) {
      NS_WARNING(
          "HTMLEditor::InsertElementWithSplittingAncestorsWithTransaction("
          "nsGkAtoms::div) failed");
      return newDivElementOrError.unwrapErr();
    }
    MOZ_ASSERT(newDivElementOrError.inspect());
    // remember our new block for postprocessing
    TopLevelEditSubActionDataRef().mNewBlockElement =
        newDivElementOrError.inspect();
    nsresult rv = ChangeMarginStart(
        MOZ_KnownLive(*newDivElementOrError.inspect()), ChangeMargin::Increase);
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::ChangeMarginStart() failed, but ignored");
    // delete anything that was in the list of nodes
    // XXX We don't need to remove the nodes from the array for performance.
    while (!arrayOfContents.IsEmpty()) {
      OwningNonNull<nsIContent>& content = arrayOfContents[0];
      // MOZ_KnownLive because 'arrayOfContents' is guaranteed to
      // keep it alive.
      nsresult rv = DeleteNodeWithTransaction(MOZ_KnownLive(*content));
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
        return rv;
      }
      arrayOfContents.RemoveElementAt(0);
    }
    // Don't restore the selection
    restoreSelectionLater.Abort();
    // put selection in new block
    rv = CollapseSelectionToStartOf(
        MOZ_KnownLive(*newDivElementOrError.inspect()));
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::CollapseSelectionToStartOf() failed");
    return rv;
  }

  // Ok, now go through all the nodes and put them in a blockquote,
  // or whatever is appropriate.
  RefPtr<Element> curList, curQuote;
  for (OwningNonNull<nsIContent>& content : arrayOfContents) {
    // Here's where we actually figure out what to do.
    EditorDOMPoint atContent(content);
    if (NS_WARN_IF(!atContent.IsSet())) {
      continue;
    }

    // Ignore all non-editable nodes.  Leave them be.
    // XXX We ignore non-editable nodes here, but not so in the above block.
    if (!EditorUtils::IsEditableContent(content, EditorType::HTML)) {
      continue;
    }

    if (HTMLEditUtils::IsAnyListElement(atContent.GetContainer())) {
      // MOZ_KnownLive because 'arrayOfContents' is guaranteed to
      // keep it alive.
      nsresult rv =
          IndentListChild(&curList, atContent, MOZ_KnownLive(content));
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::IndentListChild() failed");
        return rv;
      }
      continue;
    }

    // Not a list item.

    if (HTMLEditUtils::IsBlockElement(content)) {
      nsresult rv = ChangeMarginStart(MOZ_KnownLive(*content->AsElement()),
                                      ChangeMargin::Increase);
      if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "HTMLEditor::ChangeMarginStart() failed, but ignored");
      curQuote = nullptr;
      continue;
    }

    if (!curQuote) {
      // First, check that our element can contain a div.
      if (!HTMLEditUtils::CanNodeContain(*atContent.GetContainer(),
                                         *nsGkAtoms::div)) {
        return NS_OK;  // cancelled
      }

      Result<RefPtr<Element>, nsresult> newDivElementOrError =
          InsertElementWithSplittingAncestorsWithTransaction(
              *nsGkAtoms::div, atContent, BRElementNextToSplitPoint::Keep);
      if (MOZ_UNLIKELY(newDivElementOrError.isErr())) {
        NS_WARNING(
            "HTMLEditor::InsertElementWithSplittingAncestorsWithTransaction("
            "nsGkAtoms::div) failed");
        return newDivElementOrError.unwrapErr();
      }
      MOZ_ASSERT(newDivElementOrError.inspect());
      nsresult rv =
          ChangeMarginStart(MOZ_KnownLive(*newDivElementOrError.inspect()),
                            ChangeMargin::Increase);
      if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "HTMLEditor::ChangeMarginStart() failed, but ignored");
      // remember our new block for postprocessing
      TopLevelEditSubActionDataRef().mNewBlockElement =
          newDivElementOrError.inspect();
      curQuote = newDivElementOrError.unwrap();
      // curQuote is now the correct thing to put content in
    }

    // tuck the node into the end of the active blockquote
    // MOZ_KnownLive because 'arrayOfContents' is guaranteed to
    // keep it alive.
    nsresult rv =
        MoveNodeToEndWithTransaction(MOZ_KnownLive(content), *curQuote);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::MoveNodeToEndWithTransaction() failed");
      return rv;
    }
  }
  return NS_OK;
}

nsresult HTMLEditor::HandleHTMLIndentAtSelection() {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!IsSelectionRangeContainerNotContent());

  if (!SelectionRef().IsCollapsed()) {
    nsresult rv = MaybeExtendSelectionToHardLineEdgesForBlockEditAction();
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "HTMLEditor::MaybeExtendSelectionToHardLineEdgesForBlockEditAction() "
          "failed");
      return rv;
    }
  }

  // HandleHTMLIndentAtSelectionInternal() creates AutoSelectionRestorer.
  // Therefore, even if it returns NS_OK, editor might have been destroyed
  // at restoring Selection.
  nsresult rv = HandleHTMLIndentAtSelectionInternal();
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::HandleHTMLIndentAtSelectionInternal() failed");
  return rv;
}

nsresult HTMLEditor::HandleHTMLIndentAtSelectionInternal() {
  MOZ_ASSERT(IsTopLevelEditSubActionDataAvailable());

  AutoSelectionRestorer restoreSelectionLater(*this);

  // convert the selection ranges into "promoted" selection ranges:
  // this basically just expands the range to include the immediate
  // block parent, and then further expands to include any ancestors
  // whose children are all in the range

  AutoTArray<RefPtr<nsRange>, 4> arrayOfRanges;
  GetSelectionRangesExtendedToHardLineStartAndEnd(arrayOfRanges,
                                                  EditSubAction::eIndent);

  // use these ranges to construct a list of nodes to act on.
  AutoTArray<OwningNonNull<nsIContent>, 64> arrayOfContents;
  nsresult rv = SplitInlinesAndCollectEditTargetNodes(
      arrayOfRanges, arrayOfContents, EditSubAction::eIndent,
      CollectNonEditableNodes::Yes);
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "HTMLEditor::SplitInlinesAndCollectEditTargetNodes(eIndent, "
        "CollectNonEditableNodes::Yes) failed");
    return rv;
  }

  // If there is no visible and editable nodes in the edit targets, make an
  // empty block.
  // XXX Isn't this odd if there are only non-editable visible nodes?
  if (HTMLEditUtils::IsEmptyOneHardLine(arrayOfContents)) {
    const nsRange* firstRange = SelectionRef().GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return NS_ERROR_FAILURE;
    }

    EditorDOMPoint atStartOfSelection(firstRange->StartRef());
    if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
      return NS_ERROR_FAILURE;
    }

    // Make sure we can put a block here.
    Result<RefPtr<Element>, nsresult> newBlockQuoteElementOrError =
        InsertElementWithSplittingAncestorsWithTransaction(
            *nsGkAtoms::blockquote, atStartOfSelection,
            BRElementNextToSplitPoint::Keep);
    if (MOZ_UNLIKELY(newBlockQuoteElementOrError.isErr())) {
      NS_WARNING(
          "HTMLEditor::InsertElementWithSplittingAncestorsWithTransaction("
          "nsGkAtoms::blockquote) failed");
      return newBlockQuoteElementOrError.unwrapErr();
    }
    MOZ_ASSERT(newBlockQuoteElementOrError.inspect());
    // remember our new block for postprocessing
    TopLevelEditSubActionDataRef().mNewBlockElement =
        newBlockQuoteElementOrError.inspect();
    // delete anything that was in the list of nodes
    // XXX We don't need to remove the nodes from the array for performance.
    while (!arrayOfContents.IsEmpty()) {
      OwningNonNull<nsIContent>& content = arrayOfContents[0];
      // MOZ_KnownLive because 'arrayOfContents' is guaranteed to
      // keep it alive.
      rv = DeleteNodeWithTransaction(MOZ_KnownLive(*content));
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
        return rv;
      }
      arrayOfContents.RemoveElementAt(0);
    }
    // Don't restore the selection
    restoreSelectionLater.Abort();
    nsresult rv = CollapseSelectionToStartOf(
        MOZ_KnownLive(*newBlockQuoteElementOrError.inspect()));
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::CollapseSelectionToStartOf() failed");
    return rv;
  }

  RefPtr<Element> editingHost = GetActiveEditingHost();
  if (NS_WARN_IF(!editingHost)) {
    return NS_ERROR_FAILURE;
  }

  // Ok, now go through all the nodes and put them in a blockquote,
  // or whatever is appropriate.  Wohoo!
  RefPtr<Element> curList, curQuote, indentedLI;
  for (OwningNonNull<nsIContent>& content : arrayOfContents) {
    // Here's where we actually figure out what to do.
    EditorDOMPoint atContent(content);
    if (NS_WARN_IF(!atContent.IsSet())) {
      continue;
    }

    // Ignore all non-editable nodes.  Leave them be.
    // XXX We ignore non-editable nodes here, but not so in the above block.
    if (!EditorUtils::IsEditableContent(content, EditorType::HTML)) {
      continue;
    }

    if (HTMLEditUtils::IsAnyListElement(atContent.GetContainer())) {
      // MOZ_KnownLive because 'arrayOfContents' is guaranteed to
      // keep it alive.
      nsresult rv =
          IndentListChild(&curList, atContent, MOZ_KnownLive(content));
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::IndentListChild() failed");
        return rv;
      }
      // forget curQuote, if any
      curQuote = nullptr;
      continue;
    }

    // Not a list item, use blockquote?

    // if we are inside a list item, we don't want to blockquote, we want
    // to sublist the list item.  We may have several nodes listed in the
    // array of nodes to act on, that are in the same list item.  Since
    // we only want to indent that li once, we must keep track of the most
    // recent indented list item, and not indent it if we find another node
    // to act on that is still inside the same li.
    if (RefPtr<Element> listItem =
            HTMLEditUtils::GetClosestAncestorListItemElement(content,
                                                             editingHost)) {
      if (indentedLI == listItem) {
        // already indented this list item
        continue;
      }
      // check to see if curList is still appropriate.  Which it is if
      // content is still right after it in the same list.
      nsIContent* previousEditableSibling =
          curList ? HTMLEditUtils::GetPreviousSibling(
                        *listItem, {WalkTreeOption::IgnoreNonEditableNode})
                  : nullptr;
      if (!curList ||
          (previousEditableSibling && previousEditableSibling != curList)) {
        EditorDOMPoint atListItem(listItem);
        if (NS_WARN_IF(!listItem)) {
          return NS_ERROR_FAILURE;
        }
        nsAtom* containerName =
            atListItem.GetContainer()->NodeInfo()->NameAtom();
        // Create a new nested list of correct type.
        Result<RefPtr<Element>, nsresult> newListElementOrError =
            InsertElementWithSplittingAncestorsWithTransaction(
                MOZ_KnownLive(*containerName), atListItem,
                BRElementNextToSplitPoint::Keep);
        if (MOZ_UNLIKELY(newListElementOrError.isErr())) {
          NS_WARNING(nsPrintfCString("HTMLEditor::"
                                     "InsertElementWithSplittingAncestorsWithTr"
                                     "ansaction(%s) failed",
                                     nsAtomCString(containerName).get())
                         .get());
          return newListElementOrError.unwrapErr();
        }
        MOZ_ASSERT(newListElementOrError.inspect());
        curList = newListElementOrError.unwrap();
      }

      rv = MoveNodeToEndWithTransaction(*listItem, *curList);
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::MoveNodeToEndWithTransaction() failed");
        return rv;
      }

      // remember we indented this li
      indentedLI = listItem;

      continue;
    }

    // need to make a blockquote to put things in if we haven't already,
    // or if this node doesn't go in blockquote we used earlier.
    // One reason it might not go in prio blockquote is if we are now
    // in a different table cell.
    if (curQuote &&
        HTMLEditUtils::GetInclusiveAncestorAnyTableElement(*curQuote) !=
            HTMLEditUtils::GetInclusiveAncestorAnyTableElement(content)) {
      curQuote = nullptr;
    }

    if (!curQuote) {
      // First, check that our element can contain a blockquote.
      if (!HTMLEditUtils::CanNodeContain(*atContent.GetContainer(),
                                         *nsGkAtoms::blockquote)) {
        return NS_OK;  // cancelled
      }

      Result<RefPtr<Element>, nsresult> newBlockQuoteElementOrError =
          InsertElementWithSplittingAncestorsWithTransaction(
              *nsGkAtoms::blockquote, atContent,
              BRElementNextToSplitPoint::Keep);
      if (MOZ_UNLIKELY(newBlockQuoteElementOrError.isErr())) {
        NS_WARNING(
            "HTMLEditor::InsertElementWithSplittingAncestorsWithTransaction("
            "nsGkAtoms::blockquote) failed");
        return newBlockQuoteElementOrError.unwrapErr();
      }
      MOZ_ASSERT(newBlockQuoteElementOrError.inspect());
      // remember our new block for postprocessing
      TopLevelEditSubActionDataRef().mNewBlockElement =
          newBlockQuoteElementOrError.inspect();
      curQuote = newBlockQuoteElementOrError.unwrap();
      // curQuote is now the correct thing to put curNode in
    }

    // tuck the node into the end of the active blockquote
    // MOZ_KnownLive because 'arrayOfContents' is guaranteed to
    // keep it alive.
    rv = MoveNodeToEndWithTransaction(MOZ_KnownLive(content), *curQuote);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::MoveNodeToEndWithTransaction() failed");
      return rv;
    }
    // forget curList, if any
    curList = nullptr;
  }
  return NS_OK;
}

EditActionResult HTMLEditor::OutdentAsSubAction() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  AutoPlaceholderBatch treatAsOneTransaction(*this,
                                             ScrollSelectionIntoView::Yes);
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eOutdent, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return EditActionResult(ignoredError.StealNSResult());
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  EditActionResult result = CanHandleHTMLEditSubAction();
  if (result.Failed() || result.Canceled()) {
    NS_WARNING_ASSERTION(result.Succeeded(),
                         "HTMLEditor::CanHandleHTMLEditSubAction() failed");
    return result;
  }

  if (IsSelectionRangeContainerNotContent()) {
    NS_WARNING("Some selection containers are not content node, but ignored");
    return EditActionIgnored();
  }

  result |= HandleOutdentAtSelection();
  if (result.Failed() || result.Canceled()) {
    NS_WARNING_ASSERTION(result.Succeeded(),
                         "HTMLEditor::HandleOutdentAtSelection() failed");
    return result;
  }

  if (IsSelectionRangeContainerNotContent()) {
    NS_WARNING("Mutation event listener might have changed the selection");
    return EditActionHandled(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }

  nsresult rv = MaybeInsertPaddingBRElementForEmptyLastLineAtSelection();
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::MaybeInsertPaddingBRElementForEmptyLastLineAtSelection() "
      "failed");
  return result.SetResult(rv);
}

EditActionResult HTMLEditor::HandleOutdentAtSelection() {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!IsSelectionRangeContainerNotContent());

  if (!SelectionRef().IsCollapsed()) {
    nsresult rv = MaybeExtendSelectionToHardLineEdgesForBlockEditAction();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return EditActionHandled(rv);
    }
  }

  // HandleOutdentAtSelectionInternal() creates AutoSelectionRestorer.
  // Therefore, even if it returns NS_OK, the editor might have been destroyed
  // at restoring Selection.
  SplitRangeOffFromNodeResult outdentResult =
      HandleOutdentAtSelectionInternal();
  if (NS_WARN_IF(Destroyed())) {
    return EditActionHandled(NS_ERROR_EDITOR_DESTROYED);
  }
  if (outdentResult.Failed()) {
    NS_WARNING("HTMLEditor::HandleOutdentAtSelectionInternal() failed");
    return EditActionHandled(outdentResult.Rv());
  }

  // Make sure selection didn't stick to last piece of content in old bq (only
  // a problem for collapsed selections)
  if (!outdentResult.GetLeftContent() && !outdentResult.GetRightContent()) {
    return EditActionHandled();
  }

  if (!SelectionRef().IsCollapsed()) {
    return EditActionHandled();
  }

  // Push selection past end of left element of last split indented element.
  if (outdentResult.GetLeftContent()) {
    const nsRange* firstRange = SelectionRef().GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return EditActionHandled();
    }
    const RangeBoundary& atStartOfSelection = firstRange->StartRef();
    if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
      return EditActionHandled(NS_ERROR_FAILURE);
    }
    if (atStartOfSelection.Container() == outdentResult.GetLeftContent() ||
        EditorUtils::IsDescendantOf(*atStartOfSelection.Container(),
                                    *outdentResult.GetLeftContent())) {
      // Selection is inside the left node - push it past it.
      EditorRawDOMPoint afterRememberedLeftBQ(
          EditorRawDOMPoint::After(*outdentResult.GetLeftContent()));
      NS_WARNING_ASSERTION(
          afterRememberedLeftBQ.IsSet(),
          "Failed to set after remembered left blockquote element");
      nsresult rv = CollapseSelectionTo(afterRememberedLeftBQ);
      if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
        return EditActionHandled(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "HTMLEditor::CollapseSelectionTo() failed, but ignored");
    }
  }
  // And pull selection before beginning of right element of last split
  // indented element.
  if (outdentResult.GetRightContent()) {
    const nsRange* firstRange = SelectionRef().GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return EditActionHandled();
    }
    const RangeBoundary& atStartOfSelection = firstRange->StartRef();
    if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
      return EditActionHandled(NS_ERROR_FAILURE);
    }
    if (atStartOfSelection.Container() == outdentResult.GetRightContent() ||
        EditorUtils::IsDescendantOf(*atStartOfSelection.Container(),
                                    *outdentResult.GetRightContent())) {
      // Selection is inside the right element - push it before it.
      EditorRawDOMPoint atRememberedRightBQ(outdentResult.GetRightContent());
      nsresult rv = CollapseSelectionTo(atRememberedRightBQ);
      if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
        return EditActionHandled(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "HTMLEditor::CollapseSelectionTo() failed, but ignored");
    }
  }
  return EditActionHandled();
}

SplitRangeOffFromNodeResult HTMLEditor::HandleOutdentAtSelectionInternal() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  AutoSelectionRestorer restoreSelectionLater(*this);

  bool useCSS = IsCSSEnabled();

  // Convert the selection ranges into "promoted" selection ranges: this
  // basically just expands the range to include the immediate block parent,
  // and then further expands to include any ancestors whose children are all
  // in the range
  AutoTArray<OwningNonNull<nsIContent>, 64> arrayOfContents;
  nsresult rv = SplitInlinesAndCollectEditTargetNodesInExtendedSelectionRanges(
      arrayOfContents, EditSubAction::eOutdent, CollectNonEditableNodes::Yes);
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "HTMLEditor::"
        "SplitInlinesAndCollectEditTargetNodesInExtendedSelectionRanges() "
        "failed");
    return SplitRangeOffFromNodeResult(rv);
  }

  nsCOMPtr<nsIContent> leftContentOfLastOutdented;
  nsCOMPtr<nsIContent> middleContentOfLastOutdented;
  nsCOMPtr<nsIContent> rightContentOfLastOutdented;
  RefPtr<Element> indentedParentElement;
  nsCOMPtr<nsIContent> firstContentToBeOutdented, lastContentToBeOutdented;
  BlockIndentedWith indentedParentIndentedWith = BlockIndentedWith::HTML;
  for (OwningNonNull<nsIContent>& content : arrayOfContents) {
    // Here's where we actually figure out what to do
    EditorDOMPoint atContent(content);
    if (!atContent.IsSet()) {
      continue;
    }

    // If it's a `<blockquote>`, remove it to outdent its children.
    if (content->IsHTMLElement(nsGkAtoms::blockquote)) {
      // If we've already found an ancestor block element indented, we need to
      // split it and remove the block element first.
      if (indentedParentElement) {
        MOZ_ASSERT(indentedParentElement == content);
        SplitRangeOffFromNodeResult outdentResult = OutdentPartOfBlock(
            *indentedParentElement, *firstContentToBeOutdented,
            *lastContentToBeOutdented, indentedParentIndentedWith);
        if (outdentResult.Failed()) {
          NS_WARNING("HTMLEditor::OutdentPartOfBlock() failed");
          return outdentResult;
        }
        leftContentOfLastOutdented = outdentResult.GetLeftContent();
        middleContentOfLastOutdented = outdentResult.GetMiddleContent();
        rightContentOfLastOutdented = outdentResult.GetRightContent();
        indentedParentElement = nullptr;
        firstContentToBeOutdented = nullptr;
        lastContentToBeOutdented = nullptr;
        indentedParentIndentedWith = BlockIndentedWith::HTML;
      }
      nsresult rv = RemoveBlockContainerWithTransaction(
          MOZ_KnownLive(*content->AsElement()));
      if (NS_WARN_IF(Destroyed())) {
        return SplitRangeOffFromNodeResult(NS_ERROR_EDITOR_DESTROYED);
      }
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::RemoveBlockContainerWithTransaction() failed");
        return SplitRangeOffFromNodeResult(rv);
      }
      continue;
    }

    // If we're using CSS and the node is a block element, check its start
    // margin whether it's indented with CSS.
    if (useCSS && HTMLEditUtils::IsBlockElement(content)) {
      nsStaticAtom& marginProperty =
          MarginPropertyAtomForIndent(MOZ_KnownLive(content));
      if (NS_WARN_IF(Destroyed())) {
        return SplitRangeOffFromNodeResult(NS_ERROR_EDITOR_DESTROYED);
      }
      nsAutoString value;
      DebugOnly<nsresult> rvIgnored =
          CSSEditUtils::GetSpecifiedProperty(content, marginProperty, value);
      if (NS_WARN_IF(Destroyed())) {
        return SplitRangeOffFromNodeResult(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "CSSEditUtils::GetSpecifiedProperty() failed, but ignored");
      float startMargin = 0;
      RefPtr<nsAtom> unit;
      CSSEditUtils::ParseLength(value, &startMargin, getter_AddRefs(unit));
      // If indented with CSS, we should decrease the start mergin.
      if (startMargin > 0) {
        nsresult rv = ChangeMarginStart(MOZ_KnownLive(*content->AsElement()),
                                        ChangeMargin::Decrease);
        if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
          return SplitRangeOffFromNodeResult(NS_ERROR_EDITOR_DESTROYED);
        }
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "HTMLEditor::ChangeMarginStart(ChangeMargin::"
                             "Decrease) failed, but ignored");
        continue;
      }
    }

    // If it's a list item, we should treat as that it "indents" its children.
    if (HTMLEditUtils::IsListItem(content)) {
      // If it is a list item, that means we are not outdenting whole list.
      // XXX I don't understand this sentence...  We may meet parent list
      //     element, no?
      if (indentedParentElement) {
        SplitRangeOffFromNodeResult outdentResult = OutdentPartOfBlock(
            *indentedParentElement, *firstContentToBeOutdented,
            *lastContentToBeOutdented, indentedParentIndentedWith);
        if (NS_WARN_IF(outdentResult.Failed())) {
          return outdentResult;
        }
        leftContentOfLastOutdented = outdentResult.GetLeftContent();
        middleContentOfLastOutdented = outdentResult.GetMiddleContent();
        rightContentOfLastOutdented = outdentResult.GetRightContent();
        indentedParentElement = nullptr;
        firstContentToBeOutdented = nullptr;
        lastContentToBeOutdented = nullptr;
        indentedParentIndentedWith = BlockIndentedWith::HTML;
      }
      // XXX `content` could become different element since
      //     `OutdentPartOfBlock()` may run mutation event listeners.
      rv = LiftUpListItemElement(MOZ_KnownLive(*content->AsElement()),
                                 LiftUpFromAllParentListElements::No);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "HTMLEditor::LiftUpListItemElement(LiftUpFromAllParentListElements:"
            ":No) failed");
        return SplitRangeOffFromNodeResult(rv);
      }
      continue;
    }

    // If we've found an ancestor block element which indents its children
    // and the current node is NOT a descendant of it, we should remove it to
    // outdent its children.  Otherwise, i.e., current node is a descendant of
    // it, we meet new node which should be outdented when the indented parent
    // is removed.
    if (indentedParentElement) {
      if (EditorUtils::IsDescendantOf(*content, *indentedParentElement)) {
        // Extend the range to be outdented at removing the
        // indentedParentElement.
        lastContentToBeOutdented = content;
        continue;
      }
      SplitRangeOffFromNodeResult outdentResult = OutdentPartOfBlock(
          *indentedParentElement, *firstContentToBeOutdented,
          *lastContentToBeOutdented, indentedParentIndentedWith);
      if (outdentResult.Failed()) {
        NS_WARNING("HTMLEditor::OutdentPartOfBlock() failed");
        return outdentResult;
      }
      leftContentOfLastOutdented = outdentResult.GetLeftContent();
      middleContentOfLastOutdented = outdentResult.GetMiddleContent();
      rightContentOfLastOutdented = outdentResult.GetRightContent();
      indentedParentElement = nullptr;
      firstContentToBeOutdented = nullptr;
      lastContentToBeOutdented = nullptr;
      // curBlockIndentedWith = HTMLEditor::BlockIndentedWith::HTML;

      // Then, we need to look for next indentedParentElement.
    }

    indentedParentIndentedWith = BlockIndentedWith::HTML;
    RefPtr<Element> editingHost = GetActiveEditingHost();
    for (nsCOMPtr<nsIContent> parentContent = content->GetParent();
         parentContent && !parentContent->IsHTMLElement(nsGkAtoms::body) &&
         parentContent != editingHost &&
         (parentContent->IsHTMLElement(nsGkAtoms::table) ||
          !HTMLEditUtils::IsAnyTableElement(parentContent));
         parentContent = parentContent->GetParent()) {
      // If we reach a `<blockquote>` ancestor, it should be split at next
      // time at least for outdenting current node.
      if (parentContent->IsHTMLElement(nsGkAtoms::blockquote)) {
        indentedParentElement = parentContent->AsElement();
        firstContentToBeOutdented = content;
        lastContentToBeOutdented = content;
        break;
      }

      if (!useCSS) {
        continue;
      }

      nsCOMPtr<nsINode> grandParentNode = parentContent->GetParentNode();
      nsStaticAtom& marginProperty =
          MarginPropertyAtomForIndent(MOZ_KnownLive(content));
      if (NS_WARN_IF(Destroyed())) {
        return SplitRangeOffFromNodeResult(NS_ERROR_EDITOR_DESTROYED);
      }
      if (NS_WARN_IF(grandParentNode != parentContent->GetParentNode())) {
        return SplitRangeOffFromNodeResult(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
      }
      nsAutoString value;
      DebugOnly<nsresult> rvIgnored = CSSEditUtils::GetSpecifiedProperty(
          *parentContent, marginProperty, value);
      if (NS_WARN_IF(Destroyed())) {
        return SplitRangeOffFromNodeResult(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "CSSEditUtils::GetSpecifiedProperty() failed, but ignored");
      // XXX Now, editing host may become different element.  If so, shouldn't
      //     we stop this handling?
      float startMargin;
      RefPtr<nsAtom> unit;
      CSSEditUtils::ParseLength(value, &startMargin, getter_AddRefs(unit));
      // If we reach a block element which indents its children with start
      // margin, we should remove it at next time.
      if (startMargin > 0 &&
          !(HTMLEditUtils::IsAnyListElement(atContent.GetContainer()) &&
            HTMLEditUtils::IsAnyListElement(content))) {
        indentedParentElement = parentContent->AsElement();
        firstContentToBeOutdented = content;
        lastContentToBeOutdented = content;
        indentedParentIndentedWith = BlockIndentedWith::CSS;
        break;
      }
    }

    if (indentedParentElement) {
      continue;
    }

    // If we don't have any block elements which indents current node and
    // both current node and its parent are list element, remove current
    // node to move all its children to the parent list.
    // XXX This is buggy.  When both lists' item types are different,
    //     we create invalid tree.  E.g., `<ul>` may have `<dd>` as its
    //     list item element.
    if (HTMLEditUtils::IsAnyListElement(atContent.GetContainer())) {
      // Move node out of list
      if (HTMLEditUtils::IsAnyListElement(content)) {
        // Just unwrap this sublist
        nsresult rv = RemoveBlockContainerWithTransaction(
            MOZ_KnownLive(*content->AsElement()));
        if (NS_WARN_IF(Destroyed())) {
          return SplitRangeOffFromNodeResult(NS_ERROR_EDITOR_DESTROYED);
        }
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "HTMLEditor::RemoveBlockContainerWithTransaction() failed");
          return SplitRangeOffFromNodeResult(rv);
        }
      }
      continue;
    }

    // If current content is a list element but its parent is not a list
    // element, move children to where it is and remove it from the tree.
    if (HTMLEditUtils::IsAnyListElement(content)) {
      // XXX If mutation event listener appends new children forever, this
      //     becomes an infinite loop so that we should set limitation from
      //     first child count.
      for (nsCOMPtr<nsIContent> lastChildContent = content->GetLastChild();
           lastChildContent; lastChildContent = content->GetLastChild()) {
        if (HTMLEditUtils::IsListItem(lastChildContent)) {
          nsresult rv = LiftUpListItemElement(
              MOZ_KnownLive(*lastChildContent->AsElement()),
              LiftUpFromAllParentListElements::No);
          if (NS_FAILED(rv)) {
            NS_WARNING(
                "HTMLEditor::LiftUpListItemElement("
                "LiftUpFromAllParentListElements::No) failed");
            return SplitRangeOffFromNodeResult(rv);
          }
          continue;
        }

        if (HTMLEditUtils::IsAnyListElement(lastChildContent)) {
          // We have an embedded list, so move it out from under the parent
          // list. Be sure to put it after the parent list because this
          // loop iterates backwards through the parent's list of children.
          EditorDOMPoint afterCurrentList(EditorDOMPoint::After(atContent));
          NS_WARNING_ASSERTION(
              afterCurrentList.IsSet(),
              "Failed to set it to after current list element");
          nsresult rv =
              MoveNodeWithTransaction(*lastChildContent, afterCurrentList);
          if (NS_WARN_IF(Destroyed())) {
            return SplitRangeOffFromNodeResult(NS_ERROR_EDITOR_DESTROYED);
          }
          if (NS_FAILED(rv)) {
            NS_WARNING("HTMLEditor::MoveNodeWithTransaction() failed");
            return SplitRangeOffFromNodeResult(rv);
          }
          continue;
        }

        // Delete any non-list items for now
        // XXX Chrome moves it from the list element.  We should follow it.
        nsresult rv = DeleteNodeWithTransaction(*lastChildContent);
        if (NS_WARN_IF(Destroyed())) {
          return SplitRangeOffFromNodeResult(NS_ERROR_EDITOR_DESTROYED);
        }
        if (NS_FAILED(rv)) {
          NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
          return SplitRangeOffFromNodeResult(rv);
        }
      }
      // Delete the now-empty list
      nsresult rv = RemoveBlockContainerWithTransaction(
          MOZ_KnownLive(*content->AsElement()));
      if (NS_WARN_IF(Destroyed())) {
        return SplitRangeOffFromNodeResult(NS_ERROR_EDITOR_DESTROYED);
      }
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::RemoveBlockContainerWithTransaction() failed");
        return SplitRangeOffFromNodeResult(rv);
      }
      continue;
    }

    if (useCSS) {
      if (RefPtr<Element> element = content->GetAsElementOrParentElement()) {
        nsresult rv = ChangeMarginStart(*element, ChangeMargin::Decrease);
        if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
          return SplitRangeOffFromNodeResult(NS_ERROR_EDITOR_DESTROYED);
        }
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "HTMLEditor::ChangeMarginStart(ChangeMargin::"
                             "Decrease) failed, but ignored");
      }
      continue;
    }
  }

  if (!indentedParentElement) {
    return SplitRangeOffFromNodeResult(leftContentOfLastOutdented,
                                       middleContentOfLastOutdented,
                                       rightContentOfLastOutdented);
  }

  // We have a <blockquote> we haven't finished handling.
  SplitRangeOffFromNodeResult outdentResult =
      OutdentPartOfBlock(*indentedParentElement, *firstContentToBeOutdented,
                         *lastContentToBeOutdented, indentedParentIndentedWith);
  NS_WARNING_ASSERTION(outdentResult.Succeeded(),
                       "HTMLEditor::OutdentPartOfBlock() failed");
  return outdentResult;
}

SplitRangeOffFromNodeResult
HTMLEditor::SplitRangeOffFromBlockAndRemoveMiddleContainer(
    Element& aBlockElement, nsIContent& aStartOfRange,
    nsIContent& aEndOfRange) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  SplitRangeOffFromNodeResult splitResult =
      SplitRangeOffFromBlock(aBlockElement, aStartOfRange, aEndOfRange);
  if (NS_WARN_IF(splitResult.Rv() == NS_ERROR_EDITOR_DESTROYED)) {
    return splitResult;
  }
  NS_WARNING_ASSERTION(
      splitResult.Succeeded(),
      "HTMLEditor::SplitRangeOffFromBlock() failed, but might be ignored");
  nsresult rv = RemoveBlockContainerWithTransaction(aBlockElement);
  if (NS_WARN_IF(Destroyed())) {
    return SplitRangeOffFromNodeResult(NS_ERROR_EDITOR_DESTROYED);
  }
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::RemoveBlockContainerWithTransaction() failed");
    return SplitRangeOffFromNodeResult(rv);
  }
  return SplitRangeOffFromNodeResult(splitResult.GetLeftContent(), nullptr,
                                     splitResult.GetRightContent());
}

SplitRangeOffFromNodeResult HTMLEditor::SplitRangeOffFromBlock(
    Element& aBlockElement, nsIContent& aStartOfMiddleElement,
    nsIContent& aEndOfMiddleElement) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // aStartOfMiddleElement and aEndOfMiddleElement must be exclusive
  // descendants of aBlockElement.
  MOZ_ASSERT(EditorUtils::IsDescendantOf(aStartOfMiddleElement, aBlockElement));
  MOZ_ASSERT(EditorUtils::IsDescendantOf(aEndOfMiddleElement, aBlockElement));

  // Split at the start.
  SplitNodeResult splitAtStartResult = SplitNodeDeepWithTransaction(
      aBlockElement, EditorDOMPoint(&aStartOfMiddleElement),
      SplitAtEdges::eDoNotCreateEmptyContainer);
  if (MOZ_UNLIKELY(NS_WARN_IF(splitAtStartResult.EditorDestroyed()))) {
    return SplitRangeOffFromNodeResult(NS_ERROR_EDITOR_DESTROYED);
  }
  NS_WARNING_ASSERTION(splitAtStartResult.Succeeded(),
                       "HTMLEditor::SplitNodeDeepWithTransaction(SplitAtEdges::"
                       "eDoNotCreateEmptyContainer) failed");

  // Split at after the end
  EditorDOMPoint atAfterEnd(&aEndOfMiddleElement);
  DebugOnly<bool> advanced = atAfterEnd.AdvanceOffset();
  NS_WARNING_ASSERTION(advanced, "Failed to advance offset after the end node");
  SplitNodeResult splitAtEndResult = SplitNodeDeepWithTransaction(
      aBlockElement, atAfterEnd, SplitAtEdges::eDoNotCreateEmptyContainer);
  if (MOZ_UNLIKELY(NS_WARN_IF(splitAtEndResult.EditorDestroyed()))) {
    return SplitRangeOffFromNodeResult(NS_ERROR_EDITOR_DESTROYED);
  }
  NS_WARNING_ASSERTION(splitAtEndResult.Succeeded(),
                       "HTMLEditor::SplitNodeDeepWithTransaction(SplitAtEdges::"
                       "eDoNotCreateEmptyContainer) failed");

  return SplitRangeOffFromNodeResult(splitAtStartResult, splitAtEndResult);
}

SplitRangeOffFromNodeResult HTMLEditor::OutdentPartOfBlock(
    Element& aBlockElement, nsIContent& aStartOfOutdent,
    nsIContent& aEndOfOutdent, BlockIndentedWith aBlockIndentedWith) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  SplitRangeOffFromNodeResult splitResult =
      SplitRangeOffFromBlock(aBlockElement, aStartOfOutdent, aEndOfOutdent);
  if (NS_WARN_IF(splitResult.EditorDestroyed())) {
    return SplitRangeOffFromNodeResult(NS_ERROR_EDITOR_DESTROYED);
  }

  if (!splitResult.GetMiddleContentAsElement()) {
    NS_WARNING(
        "HTMLEditor::SplitRangeOffFromBlock() didn't return middle content");
    return SplitRangeOffFromNodeResult(NS_ERROR_FAILURE);
  }
  NS_WARNING_ASSERTION(
      splitResult.Succeeded(),
      "HTMLEditor::SplitRangeOffFromBlock() failed, but might be ignored");

  if (aBlockIndentedWith == BlockIndentedWith::HTML) {
    nsresult rv = RemoveBlockContainerWithTransaction(
        MOZ_KnownLive(*splitResult.GetMiddleContentAsElement()));
    if (NS_WARN_IF(Destroyed())) {
      return SplitRangeOffFromNodeResult(NS_ERROR_EDITOR_DESTROYED);
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::RemoveBlockContainerWithTransaction() failed");
      return SplitRangeOffFromNodeResult(rv);
    }
    return SplitRangeOffFromNodeResult(splitResult.GetLeftContent(), nullptr,
                                       splitResult.GetRightContent());
  }

  if (splitResult.GetMiddleContentAsElement()) {
    nsresult rv = ChangeMarginStart(
        MOZ_KnownLive(*splitResult.GetMiddleContentAsElement()),
        ChangeMargin::Decrease);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "HTMLEditor::ChangeMarginStart(ChangeMargin::Decrease) failed");
      return SplitRangeOffFromNodeResult(rv);
    }
    return splitResult;
  }

  return splitResult;
}

CreateElementResult HTMLEditor::ChangeListElementType(Element& aListElement,
                                                      nsAtom& aNewListTag,
                                                      nsAtom& aNewListItemTag) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  for (nsIContent* childContent = aListElement.GetFirstChild(); childContent;
       childContent = childContent->GetNextSibling()) {
    if (!childContent->IsElement()) {
      continue;
    }
    if (HTMLEditUtils::IsListItem(childContent->AsElement()) &&
        !childContent->IsHTMLElement(&aNewListItemTag)) {
      OwningNonNull<Element> listItemElement = *childContent->AsElement();
      RefPtr<Element> newListItemElement =
          ReplaceContainerWithTransaction(listItemElement, aNewListItemTag);
      if (NS_WARN_IF(Destroyed())) {
        return CreateElementResult(NS_ERROR_EDITOR_DESTROYED);
      }
      if (!newListItemElement) {
        NS_WARNING("HTMLEditor::ReplaceContainerWithTransaction() failed");
        return CreateElementResult(NS_ERROR_FAILURE);
      }
      childContent = newListItemElement;
      continue;
    }
    if (HTMLEditUtils::IsAnyListElement(childContent->AsElement()) &&
        !childContent->IsHTMLElement(&aNewListTag)) {
      // XXX List elements shouldn't have other list elements as their
      //     child.  Why do we handle such invalid tree?
      //     -> Maybe, for bug 525888.
      OwningNonNull<Element> listElement = *childContent->AsElement();
      CreateElementResult convertListTypeResult =
          ChangeListElementType(listElement, aNewListTag, aNewListItemTag);
      if (convertListTypeResult.Failed()) {
        NS_WARNING("HTMLEditor::ChangeListElementType() failed");
        return convertListTypeResult;
      }
      childContent = convertListTypeResult.GetNewNode();
      continue;
    }
  }

  if (aListElement.IsHTMLElement(&aNewListTag)) {
    return CreateElementResult(&aListElement);
  }

  RefPtr<Element> listElement =
      ReplaceContainerWithTransaction(aListElement, aNewListTag);
  if (NS_WARN_IF(Destroyed())) {
    return CreateElementResult(NS_ERROR_EDITOR_DESTROYED);
  }
  NS_WARNING_ASSERTION(listElement,
                       "HTMLEditor::ReplaceContainerWithTransaction() failed");
  return CreateElementResult(std::move(listElement));
}

nsresult HTMLEditor::CreateStyleForInsertText(
    const AbstractRange& aAbstractRange) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(aAbstractRange.IsPositioned());
  MOZ_ASSERT(mTypeInState);

  RefPtr<Element> documentRootElement = GetDocument()->GetRootElement();
  if (NS_WARN_IF(!documentRootElement)) {
    return NS_ERROR_FAILURE;
  }

  // process clearing any styles first
  UniquePtr<PropItem> item = mTypeInState->TakeClearProperty();

  EditorDOMPoint pointToPutCaret(aAbstractRange.StartRef());
  bool putCaret = false;
  {
    // Transactions may set selection, but we will set selection if necessary.
    AutoTransactionsConserveSelection dontChangeMySelection(*this);

    while (item && pointToPutCaret.GetContainer() != documentRootElement) {
      EditResult result =
          ClearStyleAt(pointToPutCaret, MOZ_KnownLive(item->tag),
                       MOZ_KnownLive(item->attr), item->specifiedStyle);
      if (result.Failed()) {
        NS_WARNING("HTMLEditor::ClearStyleAt() failed");
        return result.Rv();
      }
      pointToPutCaret = result.PointRefToCollapseSelection();
      item = mTypeInState->TakeClearProperty();
      putCaret = true;
    }
  }

  // then process setting any styles
  int32_t relFontSize = mTypeInState->TakeRelativeFontSize();
  item = mTypeInState->TakeSetProperty();

  if (item || relFontSize) {
    // we have at least one style to add; make a new text node to insert style
    // nodes above.
    if (pointToPutCaret.IsInTextNode()) {
      // if we are in a text node, split it
      SplitNodeResult splitTextNodeResult = SplitNodeDeepWithTransaction(
          MOZ_KnownLive(*pointToPutCaret.GetContainerAsText()), pointToPutCaret,
          SplitAtEdges::eAllowToCreateEmptyContainer);
      if (MOZ_UNLIKELY(splitTextNodeResult.Failed())) {
        NS_WARNING(
            "HTMLEditor::SplitNodeDeepWithTransaction(SplitAtEdges::"
            "eAllowToCreateEmptyContainer) failed");
        return splitTextNodeResult.Rv();
      }
      pointToPutCaret = splitTextNodeResult.AtSplitPoint<EditorDOMPoint>();
    }
    if (!pointToPutCaret.IsInContentNode() ||
        !HTMLEditUtils::IsContainerNode(
            *pointToPutCaret.ContainerAsContent())) {
      return NS_OK;
    }
    RefPtr<Text> newEmptyTextNode = CreateTextNode(u""_ns);
    if (!newEmptyTextNode) {
      NS_WARNING("EditorBase::CreateTextNode() failed");
      return NS_ERROR_FAILURE;
    }
    nsresult rv = InsertNodeWithTransaction(*newEmptyTextNode, pointToPutCaret);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::InsertNodeWithTransaction() failed");
      return rv;
    }
    pointToPutCaret.Set(newEmptyTextNode, 0);
    putCaret = true;

    if (relFontSize) {
      // dir indicated bigger versus smaller.  1 = bigger, -1 = smaller
      HTMLEditor::FontSize dir = relFontSize > 0 ? HTMLEditor::FontSize::incr
                                                 : HTMLEditor::FontSize::decr;
      for (int32_t j = 0; j < DeprecatedAbs(relFontSize); j++) {
        nsresult rv =
            RelativeFontChangeOnTextNode(dir, *newEmptyTextNode, 0, UINT32_MAX);
        if (NS_WARN_IF(Destroyed())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_FAILED(rv)) {
          NS_WARNING("HTMLEditor::RelativeFontChangeOnTextNode() failed");
          return rv;
        }
      }
    }

    while (item) {
      nsresult rv = SetInlinePropertyOnNode(
          MOZ_KnownLive(*pointToPutCaret.GetContainerAsContent()),
          MOZ_KnownLive(*item->tag), MOZ_KnownLive(item->attr), item->value);
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::SetInlinePropertyOnNode() failed");
        return rv;
      }
      item = mTypeInState->TakeSetProperty();
    }
  }

  if (!putCaret) {
    return NS_OK;
  }

  nsresult rv = CollapseSelectionTo(pointToPutCaret);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::CollapseSelectionTo() failed");
  return rv;
}

EditActionResult HTMLEditor::AlignAsSubAction(const nsAString& aAlignType) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  AutoPlaceholderBatch treatAsOneTransaction(*this,
                                             ScrollSelectionIntoView::Yes);
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eSetOrClearAlignment, nsIEditor::eNext,
      ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return EditActionResult(ignoredError.StealNSResult());
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  EditActionResult result = CanHandleHTMLEditSubAction();
  if (result.Failed() || result.Canceled()) {
    NS_WARNING_ASSERTION(result.Succeeded(),
                         "HTMLEditor::CanHandleHTMLEditSubAction() failed");
    return result;
  }

  if (IsSelectionRangeContainerNotContent()) {
    NS_WARNING("Some selection containers are not content node, but ignored");
    return EditActionIgnored();
  }

  nsresult rv = EnsureNoPaddingBRElementForEmptyEditor();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::EnsureNoPaddingBRElementForEmptyEditor() "
                       "failed, but ignored");

  if (IsSelectionRangeContainerNotContent()) {
    NS_WARNING("Mutation event listener might have changed the selection");
    return EditActionHandled(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }

  if (NS_SUCCEEDED(rv) && SelectionRef().IsCollapsed()) {
    nsresult rv = EnsureCaretNotAfterInvisibleBRElement();
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::EnsureCaretNotAfterInvisibleBRElement() "
                         "failed, but ignored");
    if (NS_SUCCEEDED(rv)) {
      nsresult rv = PrepareInlineStylesForCaret();
      if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
        return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "HTMLEditor::PrepareInlineStylesForCaret() failed, but ignored");
    }
  }

  if (!SelectionRef().IsCollapsed()) {
    nsresult rv = MaybeExtendSelectionToHardLineEdgesForBlockEditAction();
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "HTMLEditor::MaybeExtendSelectionToHardLineEdgesForBlockEditAction() "
          "failed");
      return EditActionResult(rv);
    }
  }

  // AlignContentsAtSelection() creates AutoSelectionRestorer.  Therefore,
  // we need to check whether we've been destroyed or not even if it returns
  // NS_OK.
  rv = AlignContentsAtSelection(aAlignType);
  if (NS_WARN_IF(Destroyed())) {
    return EditActionHandled(NS_ERROR_EDITOR_DESTROYED);
  }
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::AlignContentsAtSelection() failed");
    return EditActionHandled(rv);
  }

  if (IsSelectionRangeContainerNotContent()) {
    NS_WARNING("Mutation event listener might have changed the selection");
    return EditActionHandled(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }

  rv = MaybeInsertPaddingBRElementForEmptyLastLineAtSelection();
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::MaybeInsertPaddingBRElementForEmptyLastLineAtSelection() "
      "failed");
  return EditActionHandled(rv);
}

nsresult HTMLEditor::AlignContentsAtSelection(const nsAString& aAlignType) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!IsSelectionRangeContainerNotContent());

  AutoSelectionRestorer restoreSelectionLater(*this);

  // Convert the selection ranges into "promoted" selection ranges: This
  // basically just expands the range to include the immediate block parent,
  // and then further expands to include any ancestors whose children are all
  // in the range
  AutoTArray<OwningNonNull<nsIContent>, 64> arrayOfContents;
  nsresult rv = SplitInlinesAndCollectEditTargetNodesInExtendedSelectionRanges(
      arrayOfContents, EditSubAction::eSetOrClearAlignment,
      CollectNonEditableNodes::Yes);
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "HTMLEditor::"
        "SplitInlinesAndCollectEditTargetNodesInExtendedSelectionRanges("
        "eSetOrClearAlignment, CollectNonEditableNodes::Yes) failed");
    return rv;
  }

  // If we don't have any nodes, or we have only a single br, then we are
  // creating an empty alignment div.  We have to do some different things for
  // these.
  bool createEmptyDivElement = arrayOfContents.IsEmpty();
  if (arrayOfContents.Length() == 1) {
    OwningNonNull<nsIContent>& content = arrayOfContents[0];

    if (HTMLEditUtils::SupportsAlignAttr(content)) {
      // The node is a table element, an hr, a paragraph, a div or a section
      // header; in HTML 4, it can directly carry the ALIGN attribute and we
      // don't need to make a div! If we are in CSS mode, all the work is done
      // in SetBlockElementAlign().
      nsresult rv =
          SetBlockElementAlign(MOZ_KnownLive(*content->AsElement()), aAlignType,
                               EditTarget::OnlyDescendantsExceptTable);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "HTMLEditor::SetBlockElementAlign() failed");
      return rv;
    }

    if (content->IsHTMLElement(nsGkAtoms::br)) {
      // The special case createEmptyDivElement code (below) that consumes
      // `<br>` elements can cause tables to split if the start node of the
      // selection is not in a table cell or caption, for example parent is a
      // `<tr>`.  Avoid this unnecessary splitting if possible by leaving
      // createEmptyDivElement false so that we fall through to the normal case
      // alignment code.
      //
      // XXX: It seems a little error prone for the createEmptyDivElement
      //      special case code to assume that the start node of the selection
      //      is the parent of the single node in the arrayOfContents, as the
      //      paragraph above points out. Do we rely on the selection start
      //      node because of the fact that arrayOfContents can be empty?  We
      //      should probably revisit this issue. - kin

      const nsRange* firstRange = SelectionRef().GetRangeAt(0);
      if (NS_WARN_IF(!firstRange)) {
        return NS_ERROR_FAILURE;
      }
      const RangeBoundary& atStartOfSelection = firstRange->StartRef();
      if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
        return NS_ERROR_FAILURE;
      }
      nsINode* parent = atStartOfSelection.Container();
      createEmptyDivElement = !HTMLEditUtils::IsAnyTableElement(parent) ||
                              HTMLEditUtils::IsTableCellOrCaption(*parent);
    }
  }

  if (createEmptyDivElement) {
    if (IsSelectionRangeContainerNotContent()) {
      NS_WARNING("Mutaiton event listener might have changed the selection");
      return NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE;
    }
    EditActionResult result =
        AlignContentsAtSelectionWithEmptyDivElement(aAlignType);
    NS_WARNING_ASSERTION(
        result.Succeeded(),
        "HTMLEditor::AlignContentsAtSelectionWithEmptyDivElement() failed");
    if (result.Handled()) {
      restoreSelectionLater.Abort();
    }
    return rv;
  }

  rv = AlignNodesAndDescendants(arrayOfContents, aAlignType);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::AlignNodesAndDescendants() failed");
  return rv;
}

EditActionResult HTMLEditor::AlignContentsAtSelectionWithEmptyDivElement(
    const nsAString& aAlignType) {
  MOZ_ASSERT(IsTopLevelEditSubActionDataAvailable());
  MOZ_ASSERT(!IsSelectionRangeContainerNotContent());

  const nsRange* firstRange = SelectionRef().GetRangeAt(0);
  if (NS_WARN_IF(!firstRange)) {
    return EditActionResult(NS_ERROR_FAILURE);
  }

  EditorDOMPoint atStartOfSelection(firstRange->StartRef());
  if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
    return EditActionResult(NS_ERROR_FAILURE);
  }

  Result<RefPtr<Element>, nsresult> newDivElementOrError =
      InsertElementWithSplittingAncestorsWithTransaction(
          *nsGkAtoms::div, atStartOfSelection,
          BRElementNextToSplitPoint::Delete);
  if (newDivElementOrError.isErr()) {
    NS_WARNING(
        "HTMLEditor::InsertElementWithSplittingAncestorsWithTransaction("
        "nsGkAtoms::div, BRElementNextToSplitPoint::Delete) failed");
    return EditActionResult(newDivElementOrError.unwrapErr());
  }
  MOZ_ASSERT(newDivElementOrError.inspect());
  // Remember our new block for postprocessing
  TopLevelEditSubActionDataRef().mNewBlockElement =
      newDivElementOrError.inspect();
  // Set up the alignment on the div, using HTML or CSS
  nsresult rv =
      SetBlockElementAlign(MOZ_KnownLive(*newDivElementOrError.inspect()),
                           aAlignType, EditTarget::OnlyDescendantsExceptTable);
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "HTMLEditor::SetBlockElementAlign(EditTarget::"
        "OnlyDescendantsExceptTable) failed");
    return EditActionResult(rv);
  }
  // Put in a padding <br> element for empty last line so that it won't get
  // deleted.
  CreateElementResult createPaddingBRResult =
      InsertPaddingBRElementForEmptyLastLineWithTransaction(
          EditorDOMPoint(newDivElementOrError.inspect(), 0));
  if (createPaddingBRResult.Failed()) {
    NS_WARNING(
        "HTMLEditor::InsertPaddingBRElementForEmptyLastLineWithTransaction() "
        "failed");
    return EditActionResult(createPaddingBRResult.Rv());
  }
  rv = CollapseSelectionToStartOf(
      MOZ_KnownLive(*newDivElementOrError.inspect()));
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::CollapseSelectionToStartOf() failed");
  return EditActionHandled(rv);
}

nsresult HTMLEditor::AlignNodesAndDescendants(
    nsTArray<OwningNonNull<nsIContent>>& aArrayOfContents,
    const nsAString& aAlignType) {
  // Detect all the transitions in the array, where a transition means that
  // adjacent nodes in the array don't have the same parent.
  AutoTArray<bool, 64> transitionList;
  HTMLEditor::MakeTransitionList(aArrayOfContents, transitionList);

  // Okay, now go through all the nodes and give them an align attrib or put
  // them in a div, or whatever is appropriate.  Woohoo!

  RefPtr<Element> createdDivElement;
  bool useCSS = IsCSSEnabled();
  int32_t indexOfTransitionList = -1;
  for (OwningNonNull<nsIContent>& content : aArrayOfContents) {
    ++indexOfTransitionList;

    // Ignore all non-editable nodes.  Leave them be.
    if (!EditorUtils::IsEditableContent(content, EditorType::HTML)) {
      continue;
    }

    // The node is a table element, an hr, a paragraph, a div or a section
    // header; in HTML 4, it can directly carry the ALIGN attribute and we
    // don't need to nest it, just set the alignment.  In CSS, assign the
    // corresponding CSS styles in SetBlockElementAlign().
    if (HTMLEditUtils::SupportsAlignAttr(content)) {
      nsresult rv =
          SetBlockElementAlign(MOZ_KnownLive(*content->AsElement()), aAlignType,
                               EditTarget::NodeAndDescendantsExceptTable);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "HTMLEditor::SetBlockElementAlign(EditTarget::"
            "NodeAndDescendantsExceptTable) failed");
        return rv;
      }
      // Clear out createdDivElement so that we don't put nodes after this one
      // into it
      createdDivElement = nullptr;
      continue;
    }

    EditorDOMPoint atContent(content);
    if (NS_WARN_IF(!atContent.IsSet())) {
      continue;
    }

    // Skip insignificant formatting text nodes to prevent unnecessary
    // structure splitting!
    if (content->IsText() &&
        ((HTMLEditUtils::IsAnyTableElement(atContent.GetContainer()) &&
          !HTMLEditUtils::IsTableCellOrCaption(*atContent.GetContainer())) ||
         HTMLEditUtils::IsAnyListElement(atContent.GetContainer()) ||
         HTMLEditUtils::IsEmptyNode(
             *content, {EmptyCheckOption::TreatSingleBRElementAsVisible}))) {
      continue;
    }

    // If it's a list item, or a list inside a list, forget any "current" div,
    // and instead put divs inside the appropriate block (td, li, etc.)
    if (HTMLEditUtils::IsListItem(content) ||
        HTMLEditUtils::IsAnyListElement(content)) {
      Element* listOrListItemElement = content->AsElement();
      AutoEditorDOMPointOffsetInvalidator lockChild(atContent);
      // MOZ_KnownLive(*listOrListItemElement): An element of aArrayOfContents
      // which is array of OwningNonNull.
      nsresult rv = RemoveAlignFromDescendants(
          MOZ_KnownLive(*listOrListItemElement), aAlignType,
          EditTarget::OnlyDescendantsExceptTable);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "HTMLEditor::RemoveAlignFromDescendants(EditTarget::"
            "OnlyDescendantsExceptTable) failed");
        return rv;
      }

      if (useCSS) {
        if (nsStyledElement* styledListOrListItemElement =
                nsStyledElement::FromNode(listOrListItemElement)) {
          // MOZ_KnownLive(*styledListOrListItemElement): An element of
          // aArrayOfContents which is array of OwningNonNull.
          Result<int32_t, nsresult> result =
              mCSSEditUtils->SetCSSEquivalentToHTMLStyleWithTransaction(
                  MOZ_KnownLive(*styledListOrListItemElement), nullptr,
                  nsGkAtoms::align, &aAlignType);
          if (result.isErr()) {
            if (result.inspectErr() == NS_ERROR_EDITOR_DESTROYED) {
              NS_WARNING(
                  "CSSEditUtils::SetCSSEquivalentToHTMLStyleWithTransaction("
                  "nsGkAtoms::align) destroyed the editor");
              return NS_ERROR_EDITOR_DESTROYED;
            }
            NS_WARNING(
                "CSSEditUtils::SetCSSEquivalentToHTMLStyleWithTransaction("
                "nsGkAtoms::align) failed, but ignored");
          }
        }
        createdDivElement = nullptr;
        continue;
      }

      if (HTMLEditUtils::IsAnyListElement(atContent.GetContainer())) {
        // If we don't use CSS, add a content to list element: they have to
        // be inside another list, i.e., >= second level of nesting.
        // XXX AlignContentsInAllTableCellsAndListItems() handles only list
        //     item elements and table cells.  Is it intentional?  Why don't
        //     we need to align contents in other type blocks?
        // MOZ_KnownLive(*listOrListItemElement): An element of aArrayOfContents
        // which is array of OwningNonNull.
        nsresult rv = AlignContentsInAllTableCellsAndListItems(
            MOZ_KnownLive(*listOrListItemElement), aAlignType);
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "HTMLEditor::AlignContentsInAllTableCellsAndListItems() failed");
          return rv;
        }
        createdDivElement = nullptr;
        continue;
      }

      // Clear out createdDivElement so that we don't put nodes after this one
      // into it
    }

    // Need to make a div to put things in if we haven't already, or if this
    // node doesn't go in div we used earlier.
    if (!createdDivElement || transitionList[indexOfTransitionList]) {
      // First, check that our element can contain a div.
      if (!HTMLEditUtils::CanNodeContain(*atContent.GetContainer(),
                                         *nsGkAtoms::div)) {
        // XXX Why do we return NS_OK here rather than returning error or
        //     doing continue?
        return NS_OK;
      }

      Result<RefPtr<Element>, nsresult> newDivElementOrError =
          InsertElementWithSplittingAncestorsWithTransaction(
              *nsGkAtoms::div, atContent, BRElementNextToSplitPoint::Keep);
      if (MOZ_UNLIKELY(newDivElementOrError.isErr())) {
        NS_WARNING(
            "HTMLEditor::InsertElementWithSplittingAncestorsWithTransaction("
            "nsGkAtoms::div) failed");
        return newDivElementOrError.unwrapErr();
      }
      MOZ_ASSERT(newDivElementOrError.inspect());
      // Remember our new block for postprocessing
      TopLevelEditSubActionDataRef().mNewBlockElement =
          newDivElementOrError.inspect();
      // Set up the alignment on the div
      nsresult rv = SetBlockElementAlign(
          MOZ_KnownLive(*newDivElementOrError.inspect()), aAlignType,
          EditTarget::OnlyDescendantsExceptTable);
      if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "HTMLEditor::SetBlockElementAlign(EditTarget::"
                           "OnlyDescendantsExceptTable) failed, but ignored");
      createdDivElement = newDivElementOrError.unwrap();
    }

    // Tuck the node into the end of the active div
    //
    // MOZ_KnownLive because 'aArrayOfContents' is guaranteed to keep it alive.
    nsresult rv = MoveNodeToEndWithTransaction(MOZ_KnownLive(content),
                                               *createdDivElement);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::MoveNodeToEndWithTransaction() failed");
      return rv;
    }
  }

  return NS_OK;
}

nsresult HTMLEditor::AlignContentsInAllTableCellsAndListItems(
    Element& aElement, const nsAString& aAlignType) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // Gather list of table cells or list items
  AutoTArray<OwningNonNull<Element>, 64> arrayOfTableCellsAndListItems;
  DOMIterator iter(aElement);
  iter.AppendNodesToArray(
      +[](nsINode& aNode, void*) -> bool {
        MOZ_ASSERT(Element::FromNode(&aNode));
        return HTMLEditUtils::IsTableCell(&aNode) ||
               HTMLEditUtils::IsListItem(&aNode);
      },
      arrayOfTableCellsAndListItems);

  // Now that we have the list, align their contents as requested
  for (auto& tableCellOrListItemElement : arrayOfTableCellsAndListItems) {
    // MOZ_KnownLive because 'arrayOfTableCellsAndListItems' is guaranteed to
    // keep it alive.
    nsresult rv = AlignBlockContentsWithDivElement(
        MOZ_KnownLive(tableCellOrListItemElement), aAlignType);
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::AlignBlockContentsWithDivElement() failed");
      return rv;
    }
  }

  return NS_OK;
}

nsresult HTMLEditor::AlignBlockContentsWithDivElement(
    Element& aBlockElement, const nsAString& aAlignType) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // XXX I don't understand why we should NOT align non-editable children
  //     with modifying EDITABLE `<div>` element.
  nsCOMPtr<nsIContent> firstEditableContent = HTMLEditUtils::GetFirstChild(
      aBlockElement, {WalkTreeOption::IgnoreNonEditableNode});
  if (!firstEditableContent) {
    // This block has no editable content, nothing to align.
    return NS_OK;
  }

  // If there is only one editable content and it's a `<div>` element,
  // just set `align` attribute of it.
  nsCOMPtr<nsIContent> lastEditableContent = HTMLEditUtils::GetLastChild(
      aBlockElement, {WalkTreeOption::IgnoreNonEditableNode});
  if (firstEditableContent == lastEditableContent &&
      firstEditableContent->IsHTMLElement(nsGkAtoms::div)) {
    nsresult rv = SetAttributeOrEquivalent(
        MOZ_KnownLive(firstEditableContent->AsElement()), nsGkAtoms::align,
        aAlignType, false);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "EditorBase::SetAttributeOrEquivalent(nsGkAtoms::align) failed");
    return rv;
  }

  // Otherwise, we need to insert a `<div>` element to set `align` attribute.
  // XXX Don't insert the new `<div>` element until we set `align` attribute
  //     for avoiding running mutation event listeners.
  Result<RefPtr<Element>, nsresult> maybeNewDivElement =
      CreateAndInsertElementWithTransaction(*nsGkAtoms::div,
                                            EditorDOMPoint(&aBlockElement, 0));
  if (maybeNewDivElement.isErr()) {
    NS_WARNING(
        "HTMLEditor::CreateAndInsertElementWithTransaction(nsGkAtoms::div) "
        "failed");
    return maybeNewDivElement.unwrapErr();
  }
  MOZ_ASSERT(maybeNewDivElement.inspect());
  nsresult rv =
      SetAttributeOrEquivalent(MOZ_KnownLive(maybeNewDivElement.inspect()),
                               nsGkAtoms::align, aAlignType, false);
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::SetAttributeOrEquivalent(nsGkAtoms::align) failed");
    return rv;
  }
  // XXX This is tricky and does not work with mutation event listeners.
  //     But I'm not sure what we should do if new content is inserted.
  //     Anyway, I don't think that we should move editable contents
  //     over non-editable contents.  Chrome does no do that.
  while (lastEditableContent &&
         (lastEditableContent != maybeNewDivElement.inspect())) {
    nsresult rv = MoveNodeWithTransaction(
        *lastEditableContent, EditorDOMPoint(maybeNewDivElement.inspect(), 0));
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::MoveNodeWithTransaction() failed");
      return rv;
    }
    lastEditableContent = HTMLEditUtils::GetLastChild(
        aBlockElement, {WalkTreeOption::IgnoreNonEditableNode});
  }
  return NS_OK;
}

size_t HTMLEditor::CollectChildren(
    nsINode& aNode, nsTArray<OwningNonNull<nsIContent>>& aOutArrayOfContents,
    size_t aIndexToInsertChildren, CollectListChildren aCollectListChildren,
    CollectTableChildren aCollectTableChildren,
    CollectNonEditableNodes aCollectNonEditableNodes) const {
  MOZ_ASSERT(IsEditActionDataAvailable());

  size_t numberOfFoundChildren = 0;
  for (nsIContent* content = HTMLEditUtils::GetFirstChild(
           aNode, {WalkTreeOption::IgnoreNonEditableNode});
       content; content = content->GetNextSibling()) {
    if ((aCollectListChildren == CollectListChildren::Yes &&
         (HTMLEditUtils::IsAnyListElement(content) ||
          HTMLEditUtils::IsListItem(content))) ||
        (aCollectTableChildren == CollectTableChildren::Yes &&
         HTMLEditUtils::IsAnyTableElement(content))) {
      numberOfFoundChildren += CollectChildren(
          *content, aOutArrayOfContents,
          aIndexToInsertChildren + numberOfFoundChildren, aCollectListChildren,
          aCollectTableChildren, aCollectNonEditableNodes);
    } else if (aCollectNonEditableNodes == CollectNonEditableNodes::Yes ||
               EditorUtils::IsEditableContent(*content, EditorType::HTML)) {
      aOutArrayOfContents.InsertElementAt(
          aIndexToInsertChildren + numberOfFoundChildren++, *content);
    }
  }
  return numberOfFoundChildren;
}

nsresult HTMLEditor::MaybeExtendSelectionToHardLineEdgesForBlockEditAction() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // This tweaks selections to be more "natural".
  // Idea here is to adjust edges of selection ranges so that they do not cross
  // breaks or block boundaries unless something editable beyond that boundary
  // is also selected.  This adjustment makes it much easier for the various
  // block operations to determine what nodes to act on.

  // We don't need to mess with cell selections, and we assume multirange
  // selections are those.
  // XXX Why?  Even in <input>, user can select 2 or more ranges.
  if (SelectionRef().RangeCount() != 1) {
    return NS_OK;
  }

  const RefPtr<nsRange> range = SelectionRef().GetRangeAt(0);
  if (NS_WARN_IF(!range)) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(!range->IsPositioned())) {
    return NS_ERROR_FAILURE;
  }

  const EditorDOMPoint startPoint(range->StartRef());
  if (NS_WARN_IF(!startPoint.IsSet())) {
    return NS_ERROR_FAILURE;
  }
  const EditorDOMPoint endPoint(range->EndRef());
  if (NS_WARN_IF(!endPoint.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  // adjusted values default to original values
  EditorDOMPoint newStartPoint(startPoint);
  EditorDOMPoint newEndPoint(endPoint);

  // Is there any intervening visible white-space?  If so we can't push
  // selection past that, it would visibly change meaning of users selection.
  WSRunScanner wsScannerAtEnd(GetActiveEditingHost(), endPoint);
  WSScanResult scanResultAtEnd =
      wsScannerAtEnd.ScanPreviousVisibleNodeOrBlockBoundaryFrom(endPoint);
  if (scanResultAtEnd.Failed()) {
    NS_WARNING(
        "WSRunScanner::ScanPreviousVisibleNodeOrBlockBoundaryFrom() failed");
    return NS_ERROR_FAILURE;
  }
  if (scanResultAtEnd.ReachedSomethingNonTextContent()) {
    // eThisBlock and eOtherBlock conveniently distinguish cases
    // of going "down" into a block and "up" out of a block.
    if (wsScannerAtEnd.StartsFromOtherBlockElement()) {
      // endpoint is just after the close of a block.
      nsIContent* child = HTMLEditUtils::GetLastLeafContent(
          *wsScannerAtEnd.StartReasonOtherBlockElementPtr(),
          {LeafNodeType::LeafNodeOrChildBlock});
      if (child) {
        newEndPoint.SetAfter(child);
      }
      // else block is empty - we can leave selection alone here, i think.
    } else if (wsScannerAtEnd.StartsFromCurrentBlockBoundary()) {
      if (wsScannerAtEnd.GetEditingHost()) {
        // endpoint is just after start of this block
        if (nsIContent* child = HTMLEditUtils::GetPreviousContent(
                endPoint, {WalkTreeOption::IgnoreNonEditableNode},
                wsScannerAtEnd.GetEditingHost())) {
          newEndPoint.SetAfter(child);
        }
      }
      // else block is empty - we can leave selection alone here, i think.
    } else if (wsScannerAtEnd.StartsFromBRElement()) {
      // endpoint is just after break.  lets adjust it to before it.
      newEndPoint.Set(wsScannerAtEnd.StartReasonBRElementPtr());
    }
  }

  // Is there any intervening visible white-space?  If so we can't push
  // selection past that, it would visibly change meaning of users selection.
  WSRunScanner wsScannerAtStart(wsScannerAtEnd.GetEditingHost(), startPoint);
  WSScanResult scanResultAtStart =
      wsScannerAtStart.ScanNextVisibleNodeOrBlockBoundaryFrom(startPoint);
  if (scanResultAtStart.Failed()) {
    NS_WARNING("WSRunScanner::ScanNextVisibleNodeOrBlockBoundaryFrom() failed");
    return NS_ERROR_FAILURE;
  }
  if (scanResultAtStart.ReachedSomethingNonTextContent()) {
    // eThisBlock and eOtherBlock conveniently distinguish cases
    // of going "down" into a block and "up" out of a block.
    if (wsScannerAtStart.EndsByOtherBlockElement()) {
      // startpoint is just before the start of a block.
      nsINode* child = HTMLEditUtils::GetFirstLeafContent(
          *wsScannerAtStart.EndReasonOtherBlockElementPtr(),
          {LeafNodeType::LeafNodeOrChildBlock});
      if (child) {
        newStartPoint.Set(child);
      }
      // else block is empty - we can leave selection alone here, i think.
    } else if (wsScannerAtStart.EndsByCurrentBlockBoundary()) {
      if (wsScannerAtStart.GetEditingHost()) {
        // startpoint is just before end of this block
        if (nsIContent* child = HTMLEditUtils::GetNextContent(
                startPoint, {WalkTreeOption::IgnoreNonEditableNode},
                wsScannerAtStart.GetEditingHost())) {
          newStartPoint.Set(child);
        }
      }
      // else block is empty - we can leave selection alone here, i think.
    } else if (wsScannerAtStart.EndsByBRElement()) {
      // startpoint is just before a break.  lets adjust it to after it.
      newStartPoint.SetAfter(wsScannerAtStart.EndReasonBRElementPtr());
    }
  }

  // There is a demented possibility we have to check for.  We might have a very
  // strange selection that is not collapsed and yet does not contain any
  // editable content, and satisfies some of the above conditions that cause
  // tweaking.  In this case we don't want to tweak the selection into a block
  // it was never in, etc.  There are a variety of strategies one might use to
  // try to detect these cases, but I think the most straightforward is to see
  // if the adjusted locations "cross" the old values: i.e., new end before old
  // start, or new start after old end.  If so then just leave things alone.

  Maybe<int32_t> comp = nsContentUtils::ComparePoints(
      startPoint.ToRawRangeBoundary(), newEndPoint.ToRawRangeBoundary());

  if (NS_WARN_IF(!comp)) {
    return NS_ERROR_FAILURE;
  }

  if (*comp == 1) {
    return NS_OK;  // New end before old start.
  }

  comp = nsContentUtils::ComparePoints(newStartPoint.ToRawRangeBoundary(),
                                       endPoint.ToRawRangeBoundary());

  if (NS_WARN_IF(!comp)) {
    return NS_ERROR_FAILURE;
  }

  if (*comp == 1) {
    return NS_OK;  // New start after old end.
  }

  // Otherwise set selection to new values.  Note that end point may be prior
  // to start point.  So, we cannot use Selection::SetStartAndEndInLimit() here.
  ErrorResult error;
  SelectionRef().SetBaseAndExtentInLimiter(newStartPoint, newEndPoint, error);
  if (NS_WARN_IF(Destroyed())) {
    error = NS_ERROR_EDITOR_DESTROYED;
  } else if (error.Failed()) {
    NS_WARNING("Selection::SetBaseAndExtentInLimiter() failed");
  }
  return error.StealNSResult();
}

template <typename PT, typename RT>
EditorDOMPoint HTMLEditor::GetCurrentHardLineStartPoint(
    const RangeBoundaryBase<PT, RT>& aPoint,
    EditSubAction aEditSubAction) const {
  if (NS_WARN_IF(!aPoint.IsSet())) {
    return EditorDOMPoint();
  }

  Element* editingHost = GetActiveEditingHost();
  if (!editingHost) {
    NS_WARNING(
        "No editing host, HTMLEditor::GetCurrentHardLineStartPoint() returned "
        "given point");
    return EditorDOMPoint(aPoint);
  }

  EditorDOMPoint point(aPoint);
  // Start scanning from the container node if aPoint is in a text node.
  // XXX Perhaps, IsInDataNode() must be expected.
  if (point.IsInTextNode()) {
    if (!point.GetContainer()->GetParentNode()) {
      // Okay, can't promote any further
      // XXX Why don't we return start of the text node?
      return point;
    }
    // If there is a preformatted linefeed in the text node, let's return
    // the point after it.
    EditorDOMPoint atLastPreformattedNewLine =
        HTMLEditUtils::GetPreviousPreformattedNewLineInTextNode<EditorDOMPoint>(
            point);
    if (atLastPreformattedNewLine.IsSet()) {
      return atLastPreformattedNewLine.NextPoint();
    }
    point.Set(point.GetContainer());
  }

  // Look back through any further inline nodes that aren't across a <br>
  // from us, and that are enclosed in the same block.
  // I.e., looking for start of current hard line.
  constexpr HTMLEditUtils::WalkTreeOptions
      ignoreNonEditableNodeAndStopAtBlockBoundary{
          WalkTreeOption::IgnoreNonEditableNode,
          WalkTreeOption::StopAtBlockBoundary};
  for (nsIContent* previousEditableContent = HTMLEditUtils::GetPreviousContent(
           point, ignoreNonEditableNodeAndStopAtBlockBoundary, editingHost);
       previousEditableContent && previousEditableContent->GetParentNode() &&
       !HTMLEditUtils::IsVisibleBRElement(*previousEditableContent) &&
       !HTMLEditUtils::IsBlockElement(*previousEditableContent);
       previousEditableContent = HTMLEditUtils::GetPreviousContent(
           point, ignoreNonEditableNodeAndStopAtBlockBoundary, editingHost)) {
    EditorDOMPoint atLastPreformattedNewLine =
        HTMLEditUtils::GetPreviousPreformattedNewLineInTextNode<EditorDOMPoint>(
            EditorRawDOMPoint::AtEndOf(*previousEditableContent));
    if (atLastPreformattedNewLine.IsSet()) {
      return atLastPreformattedNewLine.NextPoint();
    }
    point.Set(previousEditableContent);
  }

  // Finding the real start for this point unless current line starts after
  // <br> element.  Look up the tree for as long as we are the first node in
  // the container (typically, start of nearest block ancestor), and as long
  // as we haven't hit the body node.
  for (nsIContent* nearContent = HTMLEditUtils::GetPreviousContent(
           point, ignoreNonEditableNodeAndStopAtBlockBoundary, editingHost);
       !nearContent && !point.IsContainerHTMLElement(nsGkAtoms::body) &&
       point.GetContainer()->GetParentNode();
       nearContent = HTMLEditUtils::GetPreviousContent(
           point, ignoreNonEditableNodeAndStopAtBlockBoundary, editingHost)) {
    // Don't keep looking up if we have found a blockquote element to act on
    // when we handle outdent.
    // XXX Sounds like this is hacky.  If possible, it should be check in
    //     outdent handler for consistency between edit sub-actions.
    //     We should check Chromium's behavior of outdent when Selection
    //     starts from `<blockquote>` and starts from first child of
    //     `<blockquote>`.
    if (aEditSubAction == EditSubAction::eOutdent &&
        point.IsContainerHTMLElement(nsGkAtoms::blockquote)) {
      break;
    }

    // Don't walk past the editable section. Note that we need to check
    // before walking up to a parent because we need to return the parent
    // object, so the parent itself might not be in the editable area, but
    // it's OK if we're not performing a block-level action.
    // XXX Here is too slow.  Let's cache active editing host first, then,
    //     compair with container of the point when we climb up the tree.
    bool blockLevelAction =
        aEditSubAction == EditSubAction::eIndent ||
        aEditSubAction == EditSubAction::eOutdent ||
        aEditSubAction == EditSubAction::eSetOrClearAlignment ||
        aEditSubAction == EditSubAction::eCreateOrRemoveBlock;
    if (!IsDescendantOfEditorRoot(point.GetContainer()->GetParentNode()) &&
        (blockLevelAction || !IsDescendantOfEditorRoot(point.GetContainer()))) {
      break;
    }

    point.Set(point.GetContainer());
  }
  return point;
}

template <typename PT, typename RT>
EditorDOMPoint HTMLEditor::GetCurrentHardLineEndPoint(
    const RangeBoundaryBase<PT, RT>& aPoint) const {
  if (NS_WARN_IF(!aPoint.IsSet())) {
    return EditorDOMPoint();
  }

  Element* editingHost = GetActiveEditingHost();
  if (!editingHost) {
    NS_WARNING(
        "No editing host, HTMLEditor::GetCurrentHardLineEndPoint() returned "
        "given point");
    return EditorDOMPoint(aPoint);
  }

  EditorDOMPoint point(aPoint);
  // Start scanning from the container node if aPoint is in a text node.
  // XXX Perhaps, IsInDataNode() must be expected.
  if (point.IsInTextNode()) {
    if (NS_WARN_IF(!point.GetContainer()->GetParentNode())) {
      // Okay, can't promote any further
      // XXX Why don't we return end of the text node?
      return point;
    }
    EditorDOMPoint atNextPreformattedNewLine =
        HTMLEditUtils::GetInclusiveNextPreformattedNewLineInTextNode<
            EditorDOMPoint>(point);
    if (atNextPreformattedNewLine.IsSet()) {
      // If the linefeed is last character of the text node, it may be
      // invisible if it's immediately before a block boundary.  In such
      // case, we should retrun the block boundary.
      Element* maybeNonEditableBlockElement = nullptr;
      if (HTMLEditUtils::IsInvisiblePreformattedNewLine(
              atNextPreformattedNewLine, &maybeNonEditableBlockElement) &&
          maybeNonEditableBlockElement) {
        // If the block is a parent of the editing host, let's return end
        // of editing host.
        if (maybeNonEditableBlockElement == editingHost ||
            !maybeNonEditableBlockElement->IsInclusiveDescendantOf(
                editingHost)) {
          return EditorDOMPoint::AtEndOf(*maybeNonEditableBlockElement);
        }
        // If it's invisible because of parent block boundary, return end
        // of the block.  Otherwise, i.e., it's followed by a child block,
        // returns the point of the child block.
        if (atNextPreformattedNewLine.ContainerAsText()
                ->IsInclusiveDescendantOf(maybeNonEditableBlockElement)) {
          return EditorDOMPoint::AtEndOf(*maybeNonEditableBlockElement);
        }
        return EditorDOMPoint(maybeNonEditableBlockElement);
      }
      // Otherwise, return the point after the preformatted linefeed.
      return atNextPreformattedNewLine.NextPoint();
    }
    // want to be after the text node
    point.SetAfter(point.GetContainer());
    NS_WARNING_ASSERTION(point.IsSet(), "Failed to set to after the text node");
  }

  // Look ahead through any further inline nodes that aren't across a <br> from
  // us, and that are enclosed in the same block.
  // XXX Currently, we stop block-extending when finding visible <br> element.
  //     This might be different from "block-extend" of execCommand spec.
  //     However, the spec is really unclear.
  // XXX Probably, scanning only editable nodes is wrong for
  //     EditSubAction::eCreateOrRemoveBlock because it might be better to wrap
  //     existing inline elements even if it's non-editable.  For example,
  //     following examples with insertParagraph causes different result:
  //     * <div contenteditable>foo[]<b contenteditable="false">bar</b></div>
  //     * <div contenteditable>foo[]<b>bar</b></div>
  //     * <div contenteditable>foo[]<b contenteditable="false">bar</b>baz</div>
  //     Only in the first case, after the caret position isn't wrapped with
  //     new <div> element.
  constexpr HTMLEditUtils::WalkTreeOptions
      ignoreNonEditableNodeAndStopAtBlockBoundary{
          WalkTreeOption::IgnoreNonEditableNode,
          WalkTreeOption::StopAtBlockBoundary};
  for (nsIContent* nextEditableContent = HTMLEditUtils::GetNextContent(
           point, ignoreNonEditableNodeAndStopAtBlockBoundary, editingHost);
       nextEditableContent &&
       !HTMLEditUtils::IsBlockElement(*nextEditableContent) &&
       nextEditableContent->GetParent();
       nextEditableContent = HTMLEditUtils::GetNextContent(
           point, ignoreNonEditableNodeAndStopAtBlockBoundary, editingHost)) {
    EditorDOMPoint atFirstPreformattedNewLine =
        HTMLEditUtils::GetInclusiveNextPreformattedNewLineInTextNode<
            EditorDOMPoint>(EditorRawDOMPoint(nextEditableContent, 0));
    if (atFirstPreformattedNewLine.IsSet()) {
      // If the linefeed is last character of the text node, it may be
      // invisible if it's immediately before a block boundary.  In such
      // case, we should retrun the block boundary.
      Element* maybeNonEditableBlockElement = nullptr;
      if (HTMLEditUtils::IsInvisiblePreformattedNewLine(
              atFirstPreformattedNewLine, &maybeNonEditableBlockElement) &&
          maybeNonEditableBlockElement) {
        // If the block is a parent of the editing host, let's return end
        // of editing host.
        if (maybeNonEditableBlockElement == editingHost ||
            !maybeNonEditableBlockElement->IsInclusiveDescendantOf(
                editingHost)) {
          return EditorDOMPoint::AtEndOf(*maybeNonEditableBlockElement);
        }
        // If it's invisible because of parent block boundary, return end
        // of the block.  Otherwise, i.e., it's followed by a child block,
        // returns the point of the child block.
        if (atFirstPreformattedNewLine.ContainerAsText()
                ->IsInclusiveDescendantOf(maybeNonEditableBlockElement)) {
          return EditorDOMPoint::AtEndOf(*maybeNonEditableBlockElement);
        }
        return EditorDOMPoint(maybeNonEditableBlockElement);
      }
      // Otherwise, return the point after the preformatted linefeed.
      return atFirstPreformattedNewLine.NextPoint();
    }
    point.SetAfter(nextEditableContent);
    if (NS_WARN_IF(!point.IsSet())) {
      break;
    }
    if (HTMLEditUtils::IsVisibleBRElement(*nextEditableContent)) {
      break;
    }
  }

  // Finding the real end for this point unless current line ends with a <br>
  // element.  Look up the tree for as long as we are the last node in the
  // container (typically, block node), and as long as we haven't hit the body
  // node.
  for (nsIContent* nearContent = HTMLEditUtils::GetNextContent(
           point, ignoreNonEditableNodeAndStopAtBlockBoundary, editingHost);
       !nearContent && !point.IsContainerHTMLElement(nsGkAtoms::body) &&
       point.GetContainer()->GetParentNode();
       nearContent = HTMLEditUtils::GetNextContent(
           point, ignoreNonEditableNodeAndStopAtBlockBoundary, editingHost)) {
    // Don't walk past the editable section. Note that we need to check before
    // walking up to a parent because we need to return the parent object, so
    // the parent itself might not be in the editable area, but it's OK.
    // XXX Maybe returning parent of editing host is really error prone since
    //     everybody need to check whether the end point is in editing host
    //     when they touch there.
    // XXX Here is too slow.  Let's cache active editing host first, then,
    //     compair with container of the point when we climb up the tree.
    if (!IsDescendantOfEditorRoot(point.GetContainer()) &&
        !IsDescendantOfEditorRoot(point.GetContainer()->GetParentNode())) {
      break;
    }

    point.SetAfter(point.GetContainer());
    if (NS_WARN_IF(!point.IsSet())) {
      break;
    }
  }
  return point;
}

void HTMLEditor::GetSelectionRangesExtendedToIncludeAdjuscentWhiteSpaces(
    nsTArray<RefPtr<nsRange>>& aOutArrayOfRanges) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(aOutArrayOfRanges.IsEmpty());

  aOutArrayOfRanges.SetCapacity(SelectionRef().RangeCount());
  for (uint32_t i = 0; i < SelectionRef().RangeCount(); i++) {
    const nsRange* selectionRange = SelectionRef().GetRangeAt(i);
    MOZ_ASSERT(selectionRange);

    RefPtr<nsRange> extendedRange =
        CreateRangeIncludingAdjuscentWhiteSpaces(*selectionRange);
    if (!extendedRange) {
      extendedRange = selectionRange->CloneRange();
    }

    aOutArrayOfRanges.AppendElement(extendedRange);
  }
}
void HTMLEditor::GetSelectionRangesExtendedToHardLineStartAndEnd(
    nsTArray<RefPtr<nsRange>>& aOutArrayOfRanges,
    EditSubAction aEditSubAction) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(aOutArrayOfRanges.IsEmpty());

  aOutArrayOfRanges.SetCapacity(SelectionRef().RangeCount());
  for (uint32_t i = 0; i < SelectionRef().RangeCount(); i++) {
    // Make a new adjusted range to represent the appropriate block content.
    // The basic idea is to push out the range endpoints to truly enclose the
    // blocks that we will affect.  This call alters opRange.
    nsRange* selectionRange = SelectionRef().GetRangeAt(i);
    MOZ_ASSERT(selectionRange);

    RefPtr<nsRange> extendedRange = CreateRangeExtendedToHardLineStartAndEnd(
        *selectionRange, aEditSubAction);
    if (!extendedRange) {
      extendedRange = selectionRange->CloneRange();
    }

    aOutArrayOfRanges.AppendElement(extendedRange);
  }
}

template <typename SPT, typename SRT, typename EPT, typename ERT>
void HTMLEditor::SelectBRElementIfCollapsedInEmptyBlock(
    RangeBoundaryBase<SPT, SRT>& aStartRef,
    RangeBoundaryBase<EPT, ERT>& aEndRef) const {
  // MOOSE major hack:
  // The GetCurrentHardLineStartPoint() and GetCurrentHardLineEndPoint() don't
  // really do the right thing for collapsed ranges inside block elements that
  // contain nothing but a solo <br>.  It's easier/ to put a workaround here
  // than to revamp them.  :-(
  if (aStartRef != aEndRef) {
    return;
  }

  if (!aStartRef.Container()->IsContent()) {
    return;
  }

  // XXX Perhaps, this should be more careful.  This may not select only one
  //     node because this just check whether the block is empty or not,
  //     and may not select in non-editable block.  However, for inline
  //     editing host case, it's right to look for block element without
  //     editable state check.  Now, this method is used for preparation for
  //     other things.  So, cannot write test for this method behavior.
  //     So, perhaps, we should get rid of this method and each caller should
  //     handle its job better.
  const Element* const maybeNonEditableBlockElement =
      HTMLEditUtils::GetInclusiveAncestorElement(
          *aStartRef.Container()->AsContent(),
          HTMLEditUtils::ClosestBlockElement);
  if (!maybeNonEditableBlockElement) {
    return;
  }

  Element* editingHost = GetActiveEditingHost();
  if (NS_WARN_IF(!editingHost)) {
    return;
  }

  // Make sure we don't go higher than our root element in the content tree
  if (editingHost->IsInclusiveDescendantOf(maybeNonEditableBlockElement)) {
    return;
  }

  if (HTMLEditUtils::IsEmptyNode(*maybeNonEditableBlockElement)) {
    aStartRef = {const_cast<Element*>(maybeNonEditableBlockElement), 0u};
    aEndRef = {const_cast<Element*>(maybeNonEditableBlockElement),
               maybeNonEditableBlockElement->Length()};
  }
}

already_AddRefed<nsRange> HTMLEditor::CreateRangeIncludingAdjuscentWhiteSpaces(
    const AbstractRange& aAbstractRange) {
  if (!aAbstractRange.IsPositioned()) {
    return nullptr;
  }
  return CreateRangeIncludingAdjuscentWhiteSpaces(aAbstractRange.StartRef(),
                                                  aAbstractRange.EndRef());
}

template <typename SPT, typename SRT, typename EPT, typename ERT>
already_AddRefed<nsRange> HTMLEditor::CreateRangeIncludingAdjuscentWhiteSpaces(
    const RangeBoundaryBase<SPT, SRT>& aStartRef,
    const RangeBoundaryBase<EPT, ERT>& aEndRef) {
  if (NS_WARN_IF(!aStartRef.IsSet()) || NS_WARN_IF(!aEndRef.IsSet())) {
    return nullptr;
  }

  if (!aStartRef.Container()->IsContent() ||
      !aEndRef.Container()->IsContent()) {
    return nullptr;
  }

  RangeBoundaryBase<SPT, SRT> startRef(aStartRef);
  RangeBoundaryBase<EPT, ERT> endRef(aEndRef);
  SelectBRElementIfCollapsedInEmptyBlock(startRef, endRef);

  if (NS_WARN_IF(!startRef.IsSet()) ||
      NS_WARN_IF(!startRef.Container()->IsContent()) ||
      NS_WARN_IF(!endRef.IsSet()) ||
      NS_WARN_IF(!endRef.Container()->IsContent())) {
    NS_WARNING("HTMLEditor::SelectBRElementIfCollapsedInEmptyBlock() failed");
    return nullptr;
  }

  // For text actions, we want to look backwards (or forwards, as
  // appropriate) for additional white-space or nbsp's.  We may have to act
  // on these later even though they are outside of the initial selection.
  // Even if they are in another node!
  // XXX Those scanners do not treat siblings of the text nodes.  Perhaps,
  //     we should use `WSRunScanner::GetFirstASCIIWhiteSpacePointCollapsedTo()`
  //     and `WSRunScanner::GetEndOfCollapsibleASCIIWhiteSpaces()` instead.
  EditorRawDOMPoint startPoint(startRef);
  if (startPoint.IsInTextNode()) {
    while (!startPoint.IsStartOfContainer()) {
      if (!startPoint.IsPreviousCharASCIISpaceOrNBSP()) {
        break;
      }
      MOZ_ALWAYS_TRUE(startPoint.RewindOffset());
    }
  }
  if (!IsDescendantOfEditorRoot(startPoint.GetChildOrContainerIfDataNode())) {
    return nullptr;
  }
  EditorRawDOMPoint endPoint(endRef);
  if (endPoint.IsInTextNode()) {
    while (!endPoint.IsEndOfContainer()) {
      if (!endPoint.IsCharASCIISpaceOrNBSP()) {
        break;
      }
      MOZ_ALWAYS_TRUE(endPoint.AdvanceOffset());
    }
  }
  EditorRawDOMPoint lastRawPoint(endPoint);
  if (!lastRawPoint.IsStartOfContainer()) {
    lastRawPoint.RewindOffset();
  }
  if (!IsDescendantOfEditorRoot(lastRawPoint.GetChildOrContainerIfDataNode())) {
    return nullptr;
  }

  RefPtr<nsRange> range =
      nsRange::Create(startPoint.ToRawRangeBoundary(),
                      endPoint.ToRawRangeBoundary(), IgnoreErrors());
  NS_WARNING_ASSERTION(range, "nsRange::Create() failed");
  return range.forget();
}

already_AddRefed<nsRange> HTMLEditor::CreateRangeExtendedToHardLineStartAndEnd(
    const AbstractRange& aAbstractRange, EditSubAction aEditSubAction) const {
  if (!aAbstractRange.IsPositioned()) {
    return nullptr;
  }
  return CreateRangeExtendedToHardLineStartAndEnd(
      aAbstractRange.StartRef(), aAbstractRange.EndRef(), aEditSubAction);
}

template <typename SPT, typename SRT, typename EPT, typename ERT>
already_AddRefed<nsRange> HTMLEditor::CreateRangeExtendedToHardLineStartAndEnd(
    const RangeBoundaryBase<SPT, SRT>& aStartRef,
    const RangeBoundaryBase<EPT, ERT>& aEndRef,
    EditSubAction aEditSubAction) const {
  if (NS_WARN_IF(!aStartRef.IsSet()) || NS_WARN_IF(!aEndRef.IsSet())) {
    return nullptr;
  }

  RangeBoundaryBase<SPT, SRT> startRef(aStartRef);
  RangeBoundaryBase<EPT, ERT> endRef(aEndRef);
  SelectBRElementIfCollapsedInEmptyBlock(startRef, endRef);

  // Make a new adjusted range to represent the appropriate block content.
  // This is tricky.  The basic idea is to push out the range endpoints to
  // truly enclose the blocks that we will affect.

  // Make sure that the new range ends up to be in the editable section.
  // XXX Looks like that this check wastes the time.  Perhaps, we should
  //     implement a method which checks both two DOM points in the editor
  //     root.

  EditorDOMPoint startPoint =
      GetCurrentHardLineStartPoint(startRef, aEditSubAction);
  // XXX GetCurrentHardLineStartPoint() may return point of editing
  //     host.  Perhaps, we should change it and stop checking it here
  //     since this check may be expensive.
  if (!IsDescendantOfEditorRoot(startPoint.GetChildOrContainerIfDataNode())) {
    return nullptr;
  }
  EditorDOMPoint endPoint = GetCurrentHardLineEndPoint(endRef);
  EditorRawDOMPoint lastRawPoint(endPoint);
  lastRawPoint.RewindOffset();
  // XXX GetCurrentHardLineEndPoint() may return point of editing host.
  //     Perhaps, we should change it and stop checking it here since this
  //     check may be expensive.
  if (!IsDescendantOfEditorRoot(lastRawPoint.GetChildOrContainerIfDataNode())) {
    return nullptr;
  }

  RefPtr<nsRange> range =
      nsRange::Create(startPoint.ToRawRangeBoundary(),
                      endPoint.ToRawRangeBoundary(), IgnoreErrors());
  NS_WARNING_ASSERTION(range, "nsRange::Create() failed");
  return range.forget();
}

nsresult HTMLEditor::SplitInlinesAndCollectEditTargetNodes(
    nsTArray<RefPtr<nsRange>>& aArrayOfRanges,
    nsTArray<OwningNonNull<nsIContent>>& aOutArrayOfContents,
    EditSubAction aEditSubAction,
    CollectNonEditableNodes aCollectNonEditableNodes) {
  nsresult rv = SplitTextNodesAtRangeEnd(aArrayOfRanges);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::SplitTextNodesAtRangeEnd() failed");
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = SplitParentInlineElementsAtRangeEdges(aArrayOfRanges);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::SplitParentInlineElementsAtRangeEdges() failed");
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = CollectEditTargetNodes(aArrayOfRanges, aOutArrayOfContents,
                              aEditSubAction, aCollectNonEditableNodes);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::CollectEditTargetNodes() failed");
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = MaybeSplitElementsAtEveryBRElement(aOutArrayOfContents, aEditSubAction);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::MaybeSplitElementsAtEveryBRElement() failed");
  return rv;
}

nsresult HTMLEditor::SplitTextNodesAtRangeEnd(
    nsTArray<RefPtr<nsRange>>& aArrayOfRanges) {
  // Split text nodes. This is necessary, since given ranges may end in text
  // nodes in case where part of a pre-formatted elements needs to be moved.
  IgnoredErrorResult ignoredError;
  for (RefPtr<nsRange>& range : aArrayOfRanges) {
    EditorDOMPoint atEnd(range->EndRef());
    if (NS_WARN_IF(!atEnd.IsSet()) || !atEnd.IsInTextNode()) {
      continue;
    }

    if (!atEnd.IsStartOfContainer() && !atEnd.IsEndOfContainer()) {
      // Split the text node.
      SplitNodeResult splitAtEndResult = SplitNodeWithTransaction(atEnd);
      if (MOZ_UNLIKELY(splitAtEndResult.Failed())) {
        NS_WARNING("HTMLEditor::SplitNodeWithTransaction() failed");
        return splitAtEndResult.Rv();
      }

      // Correct the range.
      // The new end parent becomes the parent node of the text.
      MOZ_ASSERT(!range->IsInSelection());
      range->SetEnd(splitAtEndResult.AtNextContent<EditorRawDOMPoint>()
                        .ToRawRangeBoundary(),
                    ignoredError);
      NS_WARNING_ASSERTION(!ignoredError.Failed(),
                           "nsRange::SetEnd() failed, but ignored");
      ignoredError.SuppressException();
    }
  }
  return NS_OK;
}

nsresult HTMLEditor::SplitParentInlineElementsAtRangeEdges(
    nsTArray<RefPtr<nsRange>>& aArrayOfRanges) {
  nsTArray<OwningNonNull<RangeItem>> rangeItemArray;
  rangeItemArray.AppendElements(aArrayOfRanges.Length());

  // First register ranges for special editor gravity
  for (auto& rangeItem : rangeItemArray) {
    rangeItem = new RangeItem();
    rangeItem->StoreRange(*aArrayOfRanges[0]);
    RangeUpdaterRef().RegisterRangeItem(*rangeItem);
    aArrayOfRanges.RemoveElementAt(0);
  }
  // Now bust up inlines.
  nsresult rv = NS_OK;
  for (auto& item : Reversed(rangeItemArray)) {
    // MOZ_KnownLive because 'rangeItemArray' is guaranteed to keep it alive.
    rv = SplitParentInlineElementsAtRangeEdges(MOZ_KnownLive(*item));
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::SplitParentInlineElementsAtRangeEdges() failed");
      break;
    }
  }
  // Then unregister the ranges
  for (auto& item : rangeItemArray) {
    RangeUpdaterRef().DropRangeItem(item);
    RefPtr<nsRange> range = item->GetRange();
    if (range) {
      aArrayOfRanges.AppendElement(range);
    }
  }

  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  return NS_OK;
}

nsresult HTMLEditor::CollectEditTargetNodes(
    nsTArray<RefPtr<nsRange>>& aArrayOfRanges,
    nsTArray<OwningNonNull<nsIContent>>& aOutArrayOfContents,
    EditSubAction aEditSubAction,
    CollectNonEditableNodes aCollectNonEditableNodes) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // Gather up a list of all the nodes
  for (auto& range : aArrayOfRanges) {
    DOMSubtreeIterator iter;
    nsresult rv = iter.Init(*range);
    if (NS_FAILED(rv)) {
      NS_WARNING("DOMSubtreeIterator::Init() failed");
      return rv;
    }
    if (aOutArrayOfContents.IsEmpty()) {
      iter.AppendAllNodesToArray(aOutArrayOfContents);
    } else {
      AutoTArray<OwningNonNull<nsIContent>, 24> arrayOfTopChildren;
      iter.AppendNodesToArray(
          +[](nsINode& aNode, void* aArray) -> bool {
            MOZ_ASSERT(aArray);
            return !static_cast<nsTArray<OwningNonNull<nsIContent>>*>(aArray)
                        ->Contains(&aNode);
          },
          arrayOfTopChildren, &aOutArrayOfContents);
      aOutArrayOfContents.AppendElements(std::move(arrayOfTopChildren));
    }
    if (aCollectNonEditableNodes == CollectNonEditableNodes::No) {
      for (size_t i = aOutArrayOfContents.Length(); i > 0; --i) {
        if (!EditorUtils::IsEditableContent(aOutArrayOfContents[i - 1],
                                            EditorType::HTML)) {
          aOutArrayOfContents.RemoveElementAt(i - 1);
        }
      }
    }
  }

  switch (aEditSubAction) {
    case EditSubAction::eCreateOrRemoveBlock:
      // Certain operations should not act on li's and td's, but rather inside
      // them.  Alter the list as needed.
      for (int32_t i = aOutArrayOfContents.Length() - 1; i >= 0; i--) {
        OwningNonNull<nsIContent> content = aOutArrayOfContents[i];
        if (HTMLEditUtils::IsListItem(content)) {
          aOutArrayOfContents.RemoveElementAt(i);
          CollectChildren(*content, aOutArrayOfContents, i,
                          CollectListChildren::Yes, CollectTableChildren::Yes,
                          aCollectNonEditableNodes);
        }
      }
      // Empty text node shouldn't be selected if unnecessary
      for (int32_t i = aOutArrayOfContents.Length() - 1; i >= 0; i--) {
        if (Text* text = aOutArrayOfContents[i]->GetAsText()) {
          // Don't select empty text except to empty block
          if (!HTMLEditUtils::IsVisibleTextNode(*text)) {
            aOutArrayOfContents.RemoveElementAt(i);
          }
        }
      }
      break;
    case EditSubAction::eCreateOrChangeList: {
      for (size_t i = aOutArrayOfContents.Length(); i > 0; i--) {
        // Scan for table elements.  If we find table elements other than
        // table, replace it with a list of any editable non-table content
        // because if a selection range starts from end in a table-cell and
        // ends at or starts from outside the `<table>`, we need to make
        // lists in each selected table-cells.
        OwningNonNull<nsIContent> content = aOutArrayOfContents[i - 1];
        if (HTMLEditUtils::IsAnyTableElementButNotTable(content)) {
          // XXX aCollectNonEditableNodes is ignored here.  Maybe a bug.
          aOutArrayOfContents.RemoveElementAt(i - 1);
          CollectChildren(content, aOutArrayOfContents, i - 1,
                          CollectListChildren::No, CollectTableChildren::Yes,
                          CollectNonEditableNodes::Yes);
        }
      }
      // If there is only one node in the array, and it is a `<div>`,
      // `<blockquote>` or a list element, then look inside of it until we
      // find inner list or content.
      if (aOutArrayOfContents.Length() != 1) {
        break;
      }
      Element* deepestDivBlockquoteOrListElement =
          HTMLEditUtils::GetInclusiveDeepestFirstChildWhichHasOneChild(
              aOutArrayOfContents[0], {WalkTreeOption::IgnoreNonEditableNode},
              nsGkAtoms::div, nsGkAtoms::blockquote, nsGkAtoms::ul,
              nsGkAtoms::ol, nsGkAtoms::dl);
      if (!deepestDivBlockquoteOrListElement) {
        break;
      }
      if (deepestDivBlockquoteOrListElement->IsAnyOfHTMLElements(
              nsGkAtoms::div, nsGkAtoms::blockquote)) {
        aOutArrayOfContents.Clear();
        // XXX Before we're called, non-editable nodes are ignored.  However,
        //     we may append non-editable nodes here.
        CollectChildren(*deepestDivBlockquoteOrListElement, aOutArrayOfContents,
                        0, CollectListChildren::No, CollectTableChildren::No,
                        CollectNonEditableNodes::Yes);
        break;
      }
      aOutArrayOfContents.ReplaceElementAt(
          0, OwningNonNull<nsIContent>(*deepestDivBlockquoteOrListElement));
      break;
    }
    case EditSubAction::eOutdent:
    case EditSubAction::eIndent:
    case EditSubAction::eSetPositionToAbsolute:
      // Indent/outdent already do something special for list items, but we
      // still need to make sure we don't act on table elements
      for (int32_t i = aOutArrayOfContents.Length() - 1; i >= 0; i--) {
        OwningNonNull<nsIContent> content = aOutArrayOfContents[i];
        if (HTMLEditUtils::IsAnyTableElementButNotTable(content)) {
          aOutArrayOfContents.RemoveElementAt(i);
          CollectChildren(*content, aOutArrayOfContents, i,
                          CollectListChildren::Yes, CollectTableChildren::Yes,
                          aCollectNonEditableNodes);
        }
      }
      break;
    default:
      break;
  }

  // Outdent should look inside of divs.
  if (aEditSubAction == EditSubAction::eOutdent && !IsCSSEnabled()) {
    for (int32_t i = aOutArrayOfContents.Length() - 1; i >= 0; i--) {
      OwningNonNull<nsIContent> content = aOutArrayOfContents[i];
      if (content->IsHTMLElement(nsGkAtoms::div)) {
        aOutArrayOfContents.RemoveElementAt(i);
        CollectChildren(*content, aOutArrayOfContents, i,
                        CollectListChildren::No, CollectTableChildren::No,
                        aCollectNonEditableNodes);
      }
    }
  }

  return NS_OK;
}

nsresult HTMLEditor::MaybeSplitElementsAtEveryBRElement(
    nsTArray<OwningNonNull<nsIContent>>& aArrayOfContents,
    EditSubAction aEditSubAction) {
  // Post-process the list to break up inline containers that contain br's, but
  // only for operations that might care, like making lists or paragraphs
  switch (aEditSubAction) {
    case EditSubAction::eCreateOrRemoveBlock:
    case EditSubAction::eMergeBlockContents:
    case EditSubAction::eCreateOrChangeList:
    case EditSubAction::eSetOrClearAlignment:
    case EditSubAction::eSetPositionToAbsolute:
    case EditSubAction::eIndent:
    case EditSubAction::eOutdent:
      for (int32_t i = aArrayOfContents.Length() - 1; i >= 0; i--) {
        OwningNonNull<nsIContent>& content = aArrayOfContents[i];
        if (HTMLEditUtils::IsInlineElement(content) &&
            HTMLEditUtils::IsContainerNode(content) && !content->IsText()) {
          AutoTArray<OwningNonNull<nsIContent>, 24> arrayOfInlineContents;
          // MOZ_KnownLive because 'aArrayOfContents' is guaranteed to keep it
          // alive.
          nsresult rv = SplitElementsAtEveryBRElement(MOZ_KnownLive(content),
                                                      arrayOfInlineContents);
          if (NS_FAILED(rv)) {
            NS_WARNING("HTMLEditor::SplitElementsAtEveryBRElement() failed");
            return rv;
          }

          // Put these nodes in aArrayOfContents, replacing the current node
          aArrayOfContents.RemoveElementAt(i);
          aArrayOfContents.InsertElementsAt(i, arrayOfInlineContents);
        }
      }
      return NS_OK;
    default:
      return NS_OK;
  }
}

Element* HTMLEditor::GetParentListElementAtSelection() const {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!IsSelectionRangeContainerNotContent());

  for (uint32_t i = 0; i < SelectionRef().RangeCount(); ++i) {
    nsRange* range = SelectionRef().GetRangeAt(i);
    for (nsINode* parent = range->GetClosestCommonInclusiveAncestor(); parent;
         parent = parent->GetParentNode()) {
      if (HTMLEditUtils::IsAnyListElement(parent)) {
        return parent->AsElement();
      }
    }
  }
  return nullptr;
}

nsresult HTMLEditor::SplitParentInlineElementsAtRangeEdges(
    RangeItem& aRangeItem) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  RefPtr<Element> editingHost = GetActiveEditingHost();
  if (NS_WARN_IF(!editingHost)) {
    return NS_OK;
  }

  if (!aRangeItem.IsCollapsed() && aRangeItem.mEndContainer &&
      aRangeItem.mEndContainer->IsContent()) {
    nsCOMPtr<nsIContent> mostAncestorInlineContentAtEnd =
        HTMLEditUtils::GetMostDistantAncestorInlineElement(
            *aRangeItem.mEndContainer->AsContent(), editingHost);

    if (mostAncestorInlineContentAtEnd) {
      SplitNodeResult splitEndInlineResult = SplitNodeDeepWithTransaction(
          *mostAncestorInlineContentAtEnd, aRangeItem.EndPoint(),
          SplitAtEdges::eDoNotCreateEmptyContainer);
      if (MOZ_UNLIKELY(splitEndInlineResult.Failed())) {
        NS_WARNING(
            "HTMLEditor::SplitNodeDeepWithTransaction(SplitAtEdges::"
            "eDoNotCreateEmptyContainer) failed");
        return splitEndInlineResult.Rv();
      }
      if (MOZ_UNLIKELY(editingHost != GetActiveEditingHost())) {
        NS_WARNING(
            "HTMLEditor::SplitNodeDeepWithTransaction(SplitAtEdges::"
            "eDoNotCreateEmptyContainer) caused changing editing host");
        return NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE;
      }
      const EditorRawDOMPoint& splitPointAtEnd =
          splitEndInlineResult.AtSplitPoint<EditorRawDOMPoint>();
      if (!splitPointAtEnd.IsSet()) {
        NS_WARNING(
            "HTMLEditor::SplitNodeDeepWithTransaction(SplitAtEdges::"
            "eDoNotCreateEmptyContainer) didn't return split point");
        return NS_ERROR_FAILURE;
      }
      aRangeItem.mEndContainer = splitPointAtEnd.GetContainer();
      aRangeItem.mEndOffset = splitPointAtEnd.Offset();
    }
  }

  if (!aRangeItem.mStartContainer || !aRangeItem.mStartContainer->IsContent()) {
    return NS_OK;
  }

  nsCOMPtr<nsIContent> mostAncestorInlineContentAtStart =
      HTMLEditUtils::GetMostDistantAncestorInlineElement(
          *aRangeItem.mStartContainer->AsContent(), editingHost);

  if (mostAncestorInlineContentAtStart) {
    SplitNodeResult splitStartInlineResult = SplitNodeDeepWithTransaction(
        *mostAncestorInlineContentAtStart, aRangeItem.StartPoint(),
        SplitAtEdges::eDoNotCreateEmptyContainer);
    if (MOZ_UNLIKELY(splitStartInlineResult.Failed())) {
      NS_WARNING(
          "HTMLEditor::SplitNodeDeepWithTransaction(SplitAtEdges::"
          "eDoNotCreateEmptyContainer) failed");
      return splitStartInlineResult.Rv();
    }
    // XXX If we split only here because of collapsed range, we're modifying
    //     only start point of aRangeItem.  Shouldn't we modify end point here
    //     if it's collapsed?
    const EditorRawDOMPoint& splitPointAtStart =
        splitStartInlineResult.AtSplitPoint<EditorRawDOMPoint>();
    if (MOZ_UNLIKELY(!splitPointAtStart.IsSet())) {
      NS_WARNING(
          "HTMLEditor::SplitNodeDeepWithTransaction(SplitAtEdges::"
          "eDoNotCreateEmptyContainer) didn't return split point");
      return NS_ERROR_FAILURE;
    }
    aRangeItem.mStartContainer = splitPointAtStart.GetContainer();
    aRangeItem.mStartOffset = splitPointAtStart.Offset();
  }

  return NS_OK;
}

nsresult HTMLEditor::SplitElementsAtEveryBRElement(
    nsIContent& aMostAncestorToBeSplit,
    nsTArray<OwningNonNull<nsIContent>>& aOutArrayOfContents) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // First build up a list of all the break nodes inside the inline container.
  AutoTArray<OwningNonNull<HTMLBRElement>, 24> arrayOfBRElements;
  DOMIterator iter(aMostAncestorToBeSplit);
  iter.AppendAllNodesToArray(arrayOfBRElements);

  // If there aren't any breaks, just put inNode itself in the array
  if (arrayOfBRElements.IsEmpty()) {
    aOutArrayOfContents.AppendElement(aMostAncestorToBeSplit);
    return NS_OK;
  }

  // Else we need to bust up aMostAncestorToBeSplit along all the breaks
  nsCOMPtr<nsIContent> nextContent = &aMostAncestorToBeSplit;
  for (OwningNonNull<HTMLBRElement>& brElement : arrayOfBRElements) {
    EditorDOMPoint atBRNode(brElement);
    if (NS_WARN_IF(!atBRNode.IsSet())) {
      return NS_ERROR_FAILURE;
    }
    SplitNodeResult splitNodeResult = SplitNodeDeepWithTransaction(
        *nextContent, atBRNode, SplitAtEdges::eAllowToCreateEmptyContainer);
    if (MOZ_UNLIKELY(splitNodeResult.Failed())) {
      NS_WARNING("HTMLEditor::SplitNodeDeepWithTransaction() failed");
      return splitNodeResult.Rv();
    }

    // Put previous node at the split point.
    if (nsIContent* previousContent = splitNodeResult.GetPreviousContent()) {
      // Might not be a left node.  A break might have been at the very
      // beginning of inline container, in which case
      // SplitNodeDeepWithTransaction() would not actually split anything.
      aOutArrayOfContents.AppendElement(*previousContent);
    }

    // Move break outside of container and also put in node list
    // MOZ_KnownLive because 'arrayOfBRElements' is guaranteed to keep it alive.
    nsresult rv = MoveNodeWithTransaction(
        MOZ_KnownLive(brElement),
        splitNodeResult.AtNextContent<EditorDOMPoint>());
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::MoveNodeWithTransaction() failed");
      return rv;
    }
    aOutArrayOfContents.AppendElement(brElement);

    nextContent = splitNodeResult.GetNextContent();
  }

  // Now tack on remaining next node.
  aOutArrayOfContents.AppendElement(*nextContent);

  return NS_OK;
}

// static
void HTMLEditor::MakeTransitionList(
    const nsTArray<OwningNonNull<nsIContent>>& aArrayOfContents,
    nsTArray<bool>& aTransitionArray) {
  nsINode* prevParent = nullptr;
  aTransitionArray.EnsureLengthAtLeast(aArrayOfContents.Length());
  for (uint32_t i = 0; i < aArrayOfContents.Length(); i++) {
    aTransitionArray[i] = aArrayOfContents[i]->GetParentNode() != prevParent;
    prevParent = aArrayOfContents[i]->GetParentNode();
  }
}

nsresult HTMLEditor::HandleInsertParagraphInHeadingElement(
    Element& aHeader, const EditorDOMPoint& aPointToSplit) {
  MOZ_ASSERT(IsTopLevelEditSubActionDataAvailable());

  // Remember where the header is
  EditorDOMPoint atHeader(&aHeader);
  if (NS_WARN_IF(!atHeader.IsInContentNode())) {
    return NS_ERROR_FAILURE;
  }

  {
    // Forget child node.
    AutoEditorDOMPointChildInvalidator rememberOnlyOffset(atHeader);
  }

  {
    // Get ws code to adjust any ws
    Result<EditorDOMPoint, nsresult> preparationResult =
        WhiteSpaceVisibilityKeeper::PrepareToSplitBlockElement(
            *this, aPointToSplit, aHeader);
    if (preparationResult.isErr()) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::PrepareToSplitBlockElement() failed");
      return preparationResult.unwrapErr();
    }
    EditorDOMPoint pointToSplit = preparationResult.unwrap();
    MOZ_ASSERT(pointToSplit.IsInContentNode());

    // Split the header
    SplitNodeResult splitHeaderResult = SplitNodeDeepWithTransaction(
        aHeader, pointToSplit, SplitAtEdges::eAllowToCreateEmptyContainer);
    if (MOZ_UNLIKELY(splitHeaderResult.Failed())) {
      NS_WARNING("HTMLEditor::SplitNodeDeepWithTransaction() failed");
      return NS_ERROR_FAILURE;
    }
  }

  // If the previous heading of split point is empty, put a padding <br>
  // element for empty last line into it.
  nsCOMPtr<nsIContent> prevItem = HTMLEditUtils::GetPreviousSibling(
      aHeader, {WalkTreeOption::IgnoreNonEditableNode});
  if (prevItem) {
    MOZ_DIAGNOSTIC_ASSERT(HTMLEditUtils::IsHeader(*prevItem));
    if (HTMLEditUtils::IsEmptyNode(
            *prevItem, {EmptyCheckOption::TreatSingleBRElementAsVisible})) {
      CreateElementResult createPaddingBRResult =
          InsertPaddingBRElementForEmptyLastLineWithTransaction(
              EditorDOMPoint(prevItem, 0));
      if (createPaddingBRResult.Failed()) {
        NS_WARNING(
            "HTMLEditor::InsertPaddingBRElementForEmptyLastLineWithTransaction("
            ") failed");
        return createPaddingBRResult.Rv();
      }
    }
  }

  // If the new (righthand) header node is empty, delete it
  if (HTMLEditUtils::IsEmptyBlockElement(aHeader, {})) {
    nsresult rv = DeleteNodeWithTransaction(aHeader);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
      return rv;
    }
    // Layout tells the caret to blink in a weird place if we don't place a
    // break after the header.
    nsCOMPtr<nsIContent> sibling;
    if (aHeader.GetNextSibling()) {
      sibling = HTMLEditUtils::GetNextSibling(
          *aHeader.GetNextSibling(), {WalkTreeOption::IgnoreNonEditableNode});
    }
    if (!sibling || !sibling->IsHTMLElement(nsGkAtoms::br)) {
      TopLevelEditSubActionDataRef().mCachedInlineStyles->Clear();
      mTypeInState->ClearAllProps();

      // Create a paragraph
      nsStaticAtom& paraAtom = DefaultParagraphSeparatorTagName();
      // We want a wrapper element even if we separate with <br>
      Result<RefPtr<Element>, nsresult> maybeNewParagraphElement =
          CreateAndInsertElementWithTransaction(&paraAtom == nsGkAtoms::br
                                                    ? *nsGkAtoms::p
                                                    : MOZ_KnownLive(paraAtom),
                                                atHeader.NextPoint());
      if (maybeNewParagraphElement.isErr()) {
        NS_WARNING(
            "HTMLEditor::CreateAndInsertElementWithTransaction() failed");
        return maybeNewParagraphElement.unwrapErr();
      }
      MOZ_ASSERT(maybeNewParagraphElement.inspect());

      // Append a <br> to it
      Result<RefPtr<Element>, nsresult> resultOfInsertingBRElement =
          InsertBRElementWithTransaction(
              EditorDOMPoint(maybeNewParagraphElement.inspect(), 0));
      if (resultOfInsertingBRElement.isErr()) {
        NS_WARNING("HTMLEditor::InsertBRElementWithTransaction() failed");
        return resultOfInsertingBRElement.unwrapErr();
      }
      MOZ_ASSERT(resultOfInsertingBRElement.inspect());

      // Set selection to before the break
      nsresult rv = CollapseSelectionToStartOf(
          MOZ_KnownLive(*maybeNewParagraphElement.inspect()));
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "HTMLEditor::CollapseSelectionToStartOf() failed");
      return rv;
    }

    EditorRawDOMPoint afterSibling(EditorRawDOMPoint::After(*sibling));
    if (NS_WARN_IF(!afterSibling.IsSet())) {
      return NS_ERROR_FAILURE;
    }
    // Put selection after break
    rv = CollapseSelectionTo(afterSibling);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::CollapseSelectionTo() failed");
    return rv;
  }

  // Put selection at front of righthand heading
  nsresult rv = CollapseSelectionToStartOf(aHeader);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::CollapseSelectionToStartOf() failed");
  return rv;
}

EditActionResult HTMLEditor::HandleInsertParagraphInParagraph(
    Element& aParentDivOrP) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  nsRange* firstRange = SelectionRef().GetRangeAt(0);
  if (NS_WARN_IF(!firstRange)) {
    return EditActionResult(NS_ERROR_FAILURE);
  }

  EditorDOMPoint atStartOfSelection(firstRange->StartRef());
  if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
    return EditActionResult(NS_ERROR_FAILURE);
  }
  MOZ_ASSERT(atStartOfSelection.IsSetAndValid());

  // We shouldn't create new anchor element which has non-empty href unless
  // splitting middle of it because we assume that users don't want to create
  // *same* anchor element across two or more paragraphs in most cases.
  // So, adjust selection start if it's edge of anchor element(s).
  // XXX We don't support white-space collapsing in these cases since it needs
  //     some additional work with WhiteSpaceVisibilityKeeper but it's not usual
  //     case. E.g., |<a href="foo"><b>foo []</b> </a>|
  if (atStartOfSelection.IsStartOfContainer()) {
    for (nsIContent* container = atStartOfSelection.GetContainerAsContent();
         container && container != &aParentDivOrP;
         container = container->GetParent()) {
      if (HTMLEditUtils::IsLink(container)) {
        // Found link should be only in right node.  So, we shouldn't split
        // it.
        atStartOfSelection.Set(container);
        // Even if we found an anchor element, don't break because DOM API
        // allows to nest anchor elements.
      }
      // If the container is middle of its parent, stop adjusting split point.
      if (container->GetPreviousSibling()) {
        // XXX Should we check if previous sibling is visible content?
        //     E.g., should we ignore comment node, invisible <br> element?
        break;
      }
    }
  }
  // We also need to check if selection is at invisible <br> element at end
  // of an <a href="foo"> element because editor inserts a <br> element when
  // user types Enter key after a white-space which is at middle of
  // <a href="foo"> element and when setting selection at end of the element,
  // selection becomes referring the <br> element.  We may need to change this
  // behavior later if it'd be standardized.
  else if (atStartOfSelection.IsEndOfContainer() ||
           atStartOfSelection.IsBRElementAtEndOfContainer()) {
    // If there are 2 <br> elements, the first <br> element is visible.  E.g.,
    // |<a href="foo"><b>boo[]<br></b><br></a>|, we should split the <a>
    // element.  Otherwise, E.g., |<a href="foo"><b>boo[]<br></b></a>|,
    // we should not split the <a> element and ignore inline elements in it.
    bool foundBRElement = atStartOfSelection.IsBRElementAtEndOfContainer();
    for (nsIContent* container = atStartOfSelection.GetContainerAsContent();
         container && container != &aParentDivOrP;
         container = container->GetParent()) {
      if (HTMLEditUtils::IsLink(container)) {
        // Found link should be only in left node.  So, we shouldn't split it.
        atStartOfSelection.SetAfter(container);
        // Even if we found an anchor element, don't break because DOM API
        // allows to nest anchor elements.
      }
      // If the container is middle of its parent, stop adjusting split point.
      if (nsIContent* nextSibling = container->GetNextSibling()) {
        if (foundBRElement) {
          // If we've already found a <br> element, we assume found node is
          // visible <br> or something other node.
          // XXX Should we check if non-text data node like comment?
          break;
        }

        // XXX Should we check if non-text data node like comment?
        if (!nextSibling->IsHTMLElement(nsGkAtoms::br)) {
          break;
        }
        foundBRElement = true;
      }
    }
  }

  bool doesCRCreateNewP = GetReturnInParagraphCreatesNewParagraph();
  bool splitAfterNewBR = false;
  nsCOMPtr<nsIContent> brContent;

  EditorDOMPoint pointToSplitParentDivOrP(atStartOfSelection);

  EditorDOMPoint pointToInsertBR;
  if (doesCRCreateNewP && atStartOfSelection.GetContainer() == &aParentDivOrP) {
    // We are at the edges of the block, so, we don't need to create new <br>.
    brContent = nullptr;
  } else if (atStartOfSelection.IsInTextNode()) {
    // at beginning of text node?
    if (atStartOfSelection.IsStartOfContainer()) {
      // is there a BR prior to it?
      brContent = atStartOfSelection.IsInContentNode()
                      ? HTMLEditUtils::GetPreviousSibling(
                            *atStartOfSelection.ContainerAsContent(),
                            {WalkTreeOption::IgnoreNonEditableNode})
                      : nullptr;
      if (!brContent || !HTMLEditUtils::IsVisibleBRElement(*brContent) ||
          EditorUtils::IsPaddingBRElementForEmptyLastLine(*brContent)) {
        pointToInsertBR.Set(atStartOfSelection.GetContainer());
        brContent = nullptr;
      }
    } else if (atStartOfSelection.IsEndOfContainer()) {
      // we're at the end of text node...
      // is there a BR after to it?
      brContent = atStartOfSelection.IsInContentNode()
                      ? HTMLEditUtils::GetNextSibling(
                            *atStartOfSelection.ContainerAsContent(),
                            {WalkTreeOption::IgnoreNonEditableNode})
                      : nullptr;
      if (!brContent || !HTMLEditUtils::IsVisibleBRElement(*brContent) ||
          EditorUtils::IsPaddingBRElementForEmptyLastLine(*brContent)) {
        pointToInsertBR.SetAfter(atStartOfSelection.GetContainer());
        NS_WARNING_ASSERTION(
            pointToInsertBR.IsSet(),
            "Failed to set to after the container  of selection start");
        brContent = nullptr;
      }
    } else {
      if (doesCRCreateNewP) {
        // XXX We split a text node here if caret is middle of it to insert
        //     <br> element **before** splitting aParentDivOrP.  Then, if
        //     the <br> element becomes unnecessary, it'll be removed again.
        //     So this does much more complicated things than what we want to
        //     do here.  We should handle this case separately to make the code
        //     much simpler.
        Result<EditorDOMPoint, nsresult> pointToSplitOrError =
            WhiteSpaceVisibilityKeeper::PrepareToSplitBlockElement(
                *this, pointToSplitParentDivOrP, aParentDivOrP);
        if (MOZ_UNLIKELY(NS_WARN_IF(Destroyed()))) {
          return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
        }
        if (MOZ_UNLIKELY(pointToSplitOrError.isErr())) {
          NS_WARNING(
              "WhiteSpaceVisibilityKeeper::PrepareToSplitBlockElement() "
              "failed");
          return EditActionResult(pointToSplitOrError.unwrapErr());
        }
        MOZ_ASSERT(pointToSplitOrError.inspect().IsSetAndValid());
        if (pointToSplitOrError.inspect().IsSet()) {
          pointToSplitParentDivOrP = pointToSplitOrError.unwrap();
        }
        SplitNodeResult splitParentDivOrPResult =
            SplitNodeWithTransaction(pointToSplitParentDivOrP);
        if (MOZ_UNLIKELY(splitParentDivOrPResult.Failed())) {
          NS_WARNING("HTMLEditor::SplitNodeWithTransaction() failed");
          return EditActionResult(splitParentDivOrPResult.Rv());
        }
        pointToSplitParentDivOrP.SetToEndOf(
            splitParentDivOrPResult.GetPreviousContent());
      }

      // We need to put new <br> after the left node if given node was split
      // above.
      pointToInsertBR.SetAfter(pointToSplitParentDivOrP.GetContainer());
    }
  } else {
    // not in a text node.
    // is there a BR prior to it?
    Element* editingHost = GetActiveEditingHost();
    nsIContent* nearContent =
        editingHost ? HTMLEditUtils::GetPreviousContent(
                          atStartOfSelection,
                          {WalkTreeOption::IgnoreNonEditableNode}, editingHost)
                    : nullptr;
    if (!nearContent || !HTMLEditUtils::IsVisibleBRElement(*nearContent) ||
        EditorUtils::IsPaddingBRElementForEmptyLastLine(*nearContent)) {
      // is there a BR after it?
      nearContent = editingHost ? HTMLEditUtils::GetNextContent(
                                      atStartOfSelection,
                                      {WalkTreeOption::IgnoreNonEditableNode},
                                      editingHost)
                                : nullptr;
      if (!nearContent || !HTMLEditUtils::IsVisibleBRElement(*nearContent) ||
          EditorUtils::IsPaddingBRElementForEmptyLastLine(*nearContent)) {
        pointToInsertBR = atStartOfSelection;
        splitAfterNewBR = true;
      }
    }
    if (!pointToInsertBR.IsSet() && nearContent->IsHTMLElement(nsGkAtoms::br)) {
      brContent = nearContent;
    }
  }
  if (pointToInsertBR.IsSet()) {
    // if CR does not create a new P, default to BR creation
    if (NS_WARN_IF(!doesCRCreateNewP)) {
      return EditActionResult(NS_OK);
    }

    Result<RefPtr<Element>, nsresult> resultOfInsertingBRElement =
        InsertBRElementWithTransaction(pointToInsertBR);
    if (resultOfInsertingBRElement.isErr()) {
      NS_WARNING("HTMLEditor::InsertBRElementWithTransaction() failed");
      return EditActionResult(resultOfInsertingBRElement.unwrapErr());
    }
    MOZ_ASSERT(resultOfInsertingBRElement.inspect());
    if (splitAfterNewBR) {
      // We split the parent after the br we've just inserted.
      pointToSplitParentDivOrP.SetAfter(resultOfInsertingBRElement.inspect());
      NS_WARNING_ASSERTION(pointToSplitParentDivOrP.IsSet(),
                           "Failed to set after the new <br>");
    }
    brContent = resultOfInsertingBRElement.unwrap().forget();
  }
  EditActionResult result(
      SplitParagraph(aParentDivOrP, pointToSplitParentDivOrP, brContent));
  NS_WARNING_ASSERTION(result.Succeeded(),
                       "HTMLEditor::SplitParagraph() failed");
  result.MarkAsHandled();
  return result;
}

nsresult HTMLEditor::SplitParagraph(Element& aParentDivOrP,
                                    const EditorDOMPoint& aStartOfRightNode,
                                    nsIContent* aNextBRNode) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  Result<EditorDOMPoint, nsresult> preparationResult =
      WhiteSpaceVisibilityKeeper::PrepareToSplitBlockElement(
          *this, aStartOfRightNode, aParentDivOrP);
  if (preparationResult.isErr()) {
    NS_WARNING(
        "WhiteSpaceVisibilityKeeper::PrepareToSplitBlockElement() failed");
    return preparationResult.unwrapErr();
  }
  EditorDOMPoint pointToSplit = preparationResult.unwrap();
  MOZ_ASSERT(pointToSplit.IsInContentNode());

  // Split the paragraph.
  SplitNodeResult splitDivOrPResult = SplitNodeDeepWithTransaction(
      aParentDivOrP, pointToSplit, SplitAtEdges::eAllowToCreateEmptyContainer);
  if (MOZ_UNLIKELY(splitDivOrPResult.Failed())) {
    NS_WARNING("HTMLEditor::SplitNodeDeepWithTransaction() failed");
    return splitDivOrPResult.Rv();
  }
  if (MOZ_UNLIKELY(!splitDivOrPResult.DidSplit())) {
    NS_WARNING(
        "HTMLEditor::SplitNodeDeepWithTransaction() didn't split any nodes");
    return NS_ERROR_FAILURE;
  }

  // Get rid of the break, if it is visible (otherwise it may be needed to
  // prevent an empty p).
  if (aNextBRNode && HTMLEditUtils::IsVisibleBRElement(*aNextBRNode)) {
    nsresult rv = DeleteNodeWithTransaction(*aNextBRNode);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
      return rv;
    }
  }

  // Remove ID attribute on the paragraph from the existing right node.
  nsresult rv = RemoveAttributeWithTransaction(aParentDivOrP, *nsGkAtoms::id);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "EditorBase::RemoveAttributeWithTransaction(nsGkAtoms::id) failed");
    return rv;
  }

  // We need to ensure to both paragraphs visible even if they are empty.
  // However, padding <br> element for empty last line isn't useful in this
  // case because it'll be ignored by PlaintextSerializer.  Additionally,
  // it'll be exposed as <br> with Element.innerHTML.  Therefore, we can use
  // normal <br> elements for placeholder in this case.  Note that Chromium
  // also behaves so.
  if (Element* previousElementOfSplitPoint =
          Element::FromNode(splitDivOrPResult.GetPreviousContent())) {
    // MOZ_KnownLive(previousElementAtSplitPoint):
    // It's grabbed by splitDivOrResult.
    nsresult rv = InsertBRElementIfEmptyBlockElement(
        MOZ_KnownLive(*previousElementOfSplitPoint));
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::InsertBRElementIfEmptyBlockElement() failed");
      return rv;
    }
  }
  if (Element* nextElementOfSplitPoint =
          Element::FromNode(splitDivOrPResult.GetNextContent())) {
    // MOZ_KnownLive(nextElementAtSplitPoint):
    // It's grabbed by splitDivOrResult.
    nsresult rv = InsertBRElementIfEmptyBlockElement(
        MOZ_KnownLive(*nextElementOfSplitPoint));
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::InsertBRElementIfEmptyBlockElement() failed");
      return rv;
    }
  }

  // selection to beginning of right hand para;
  // look inside any containers that are up front.
  nsCOMPtr<nsIContent> child = HTMLEditUtils::GetFirstLeafContent(
      aParentDivOrP, {LeafNodeType::LeafNodeOrChildBlock});
  if (child && (child->IsText() || HTMLEditUtils::IsContainerNode(*child))) {
    nsresult rv = CollapseSelectionToStartOf(*child);
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::CollapseSelectionToStartOf() failed, but ignored");
  } else {
    nsresult rv = CollapseSelectionTo(EditorRawDOMPoint(child));
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::CollapseSelectionTo() failed, but ignored");
  }
  return NS_OK;
}

nsresult HTMLEditor::HandleInsertParagraphInListItemElement(
    Element& aListItem, const EditorDOMPoint& aPointToSplit) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(HTMLEditUtils::IsListItem(&aListItem));

  // Get the item parent and the active editing host.
  RefPtr<Element> editingHost = GetActiveEditingHost();

  // If we are in an empty item, then we want to pop up out of the list, but
  // only if prefs say it's okay and if the parent isn't the active editing
  // host.
  if (editingHost != aListItem.GetParentElement() &&
      HTMLEditUtils::IsEmptyBlockElement(aListItem, {})) {
    nsCOMPtr<nsIContent> leftListNode = aListItem.GetParent();
    // Are we the last list item in the list?
    if (!HTMLEditUtils::IsLastChild(aListItem,
                                    {WalkTreeOption::IgnoreNonEditableNode})) {
      // We need to split the list!
      SplitNodeResult splitListItemParentResult =
          SplitNodeWithTransaction(EditorDOMPoint(&aListItem));
      if (MOZ_UNLIKELY(splitListItemParentResult.Failed())) {
        NS_WARNING("HTMLEditor::SplitNodeWithTransaction() failed");
        return splitListItemParentResult.Rv();
      }
      leftListNode = splitListItemParentResult.GetPreviousContent();
    }

    // Are we in a sublist?
    EditorDOMPoint atNextSiblingOfLeftList(leftListNode);
    DebugOnly<bool> advanced = atNextSiblingOfLeftList.AdvanceOffset();
    NS_WARNING_ASSERTION(advanced,
                         "Failed to advance offset after the right list node");
    if (HTMLEditUtils::IsAnyListElement(
            atNextSiblingOfLeftList.GetContainer())) {
      // If so, move item out of this list and into the grandparent list
      nsresult rv = MoveNodeWithTransaction(aListItem, atNextSiblingOfLeftList);
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::MoveNodeWithTransaction() failed");
        return rv;
      }
      rv = CollapseSelectionToStartOf(aListItem);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "HTMLEditor::CollapseSelectionToStartOf() failed");
      return rv;
    }

    // Otherwise kill this item
    nsresult rv = DeleteNodeWithTransaction(aListItem);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
      return rv;
    }

    // Time to insert a paragraph
    nsStaticAtom& paraAtom = DefaultParagraphSeparatorTagName();
    // We want a wrapper even if we separate with <br>
    Result<RefPtr<Element>, nsresult> maybeNewParagraphElement =
        CreateAndInsertElementWithTransaction(&paraAtom == nsGkAtoms::br
                                                  ? *nsGkAtoms::p
                                                  : MOZ_KnownLive(paraAtom),
                                              atNextSiblingOfLeftList);
    if (maybeNewParagraphElement.isErr()) {
      NS_WARNING("HTMLEditor::CreateAndInsertElementWithTransaction() failed");
      return maybeNewParagraphElement.unwrapErr();
    }
    MOZ_ASSERT(maybeNewParagraphElement.inspect());

    // Append a <br> to it
    Result<RefPtr<Element>, nsresult> resultOfInsertingBRElement =
        InsertBRElementWithTransaction(
            EditorDOMPoint(maybeNewParagraphElement.inspect(), 0));
    if (resultOfInsertingBRElement.isErr()) {
      NS_WARNING("HTMLEditor::InsertBRElementWithTransaction() failed");
      return resultOfInsertingBRElement.unwrapErr();
    }
    MOZ_ASSERT(resultOfInsertingBRElement.inspect());

    // Set selection to before the break
    rv = CollapseSelectionToStartOf(
        MOZ_KnownLive(*maybeNewParagraphElement.inspect()));
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::CollapseSelectionToStartOf() failed");
    return rv;
  }

  // Else we want a new list item at the same list level.  Get ws code to
  // adjust any ws.
  Result<EditorDOMPoint, nsresult> preparationResult =
      WhiteSpaceVisibilityKeeper::PrepareToSplitBlockElement(
          *this, aPointToSplit, aListItem);
  if (preparationResult.isErr()) {
    NS_WARNING(
        "WhiteSpaceVisibilityKeeper::PrepareToSplitBlockElement() failed");
    return preparationResult.unwrapErr();
  }
  EditorDOMPoint pointToSplit = preparationResult.unwrap();
  MOZ_ASSERT(pointToSplit.IsInContentNode());

  // Now split the list item.
  SplitNodeResult splitListItemResult = SplitNodeDeepWithTransaction(
      aListItem, pointToSplit, SplitAtEdges::eAllowToCreateEmptyContainer);
  if (MOZ_UNLIKELY(NS_WARN_IF(splitListItemResult.EditorDestroyed()))) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(
      splitListItemResult.Succeeded(),
      "HTMLEditor::SplitNodeDeepWithTransaction() failed, but ignored");

  // Hack: until I can change the damaged doc range code back to being
  // extra-inclusive, I have to manually detect certain list items that may be
  // left empty.
  nsCOMPtr<nsIContent> prevItem = HTMLEditUtils::GetPreviousSibling(
      aListItem, {WalkTreeOption::IgnoreNonEditableNode});
  if (prevItem && HTMLEditUtils::IsListItem(prevItem)) {
    if (HTMLEditUtils::IsEmptyNode(
            *prevItem, {EmptyCheckOption::TreatSingleBRElementAsVisible})) {
      CreateElementResult createPaddingBRResult =
          InsertPaddingBRElementForEmptyLastLineWithTransaction(
              EditorDOMPoint(prevItem, 0));
      if (createPaddingBRResult.Failed()) {
        NS_WARNING(
            "HTMLEditor::InsertPaddingBRElementForEmptyLastLineWithTransaction("
            ") failed");
        return createPaddingBRResult.Rv();
      }
    } else {
      if (HTMLEditUtils::IsEmptyNode(aListItem)) {
        if (aListItem.IsAnyOfHTMLElements(nsGkAtoms::dd, nsGkAtoms::dt)) {
          nsCOMPtr<nsINode> list = aListItem.GetParentNode();
          int32_t itemOffset = list ? list->ComputeIndexOf(&aListItem) : -1;

          nsStaticAtom* nextDefinitionListItemTagName =
              aListItem.IsHTMLElement(nsGkAtoms::dt) ? nsGkAtoms::dd
                                                     : nsGkAtoms::dt;
          MOZ_DIAGNOSTIC_ASSERT(itemOffset != -1);
          EditorDOMPoint atNextListItem(list, aListItem.GetNextSibling(),
                                        AssertedCast<uint32_t>(itemOffset + 1));
          Result<RefPtr<Element>, nsresult> maybeNewListItemElement =
              CreateAndInsertElementWithTransaction(
                  MOZ_KnownLive(*nextDefinitionListItemTagName),
                  atNextListItem);
          if (maybeNewListItemElement.isErr()) {
            NS_WARNING(
                "HTMLEditor::CreateAndInsertElementWithTransaction() failed");
            return maybeNewListItemElement.unwrapErr();
          }
          MOZ_ASSERT(maybeNewListItemElement.inspect());
          nsresult rv = DeleteNodeWithTransaction(aListItem);
          if (NS_WARN_IF(Destroyed())) {
            return NS_ERROR_EDITOR_DESTROYED;
          }
          if (NS_FAILED(rv)) {
            NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
            return rv;
          }
          rv = CollapseSelectionToStartOf(
              MOZ_KnownLive(*maybeNewListItemElement.inspect()));
          NS_WARNING_ASSERTION(
              NS_SUCCEEDED(rv),
              "HTMLEditor::CollapseSelectionToStartOf() failed");
          return rv;
        }

        RefPtr<Element> brElement;
        nsresult rv = CopyLastEditableChildStylesWithTransaction(
            MOZ_KnownLive(*prevItem->AsElement()), aListItem,
            address_of(brElement));
        if (NS_WARN_IF(Destroyed())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "HTMLEditor::CopyLastEditableChildStylesWithTransaction() "
              "failed");
          return NS_ERROR_FAILURE;
        }
        if (brElement) {
          EditorRawDOMPoint atBRNode(brElement);
          if (NS_WARN_IF(!atBRNode.IsSetAndValid())) {
            return NS_ERROR_FAILURE;
          }
          nsresult rv = CollapseSelectionTo(atBRNode);
          NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                               "HTMLEditor::CollapseSelectionTo() failed");
          return rv;
        }
      } else {
        WSScanResult forwardScanFromStartOfListItemResult =
            WSRunScanner::ScanNextVisibleNodeOrBlockBoundary(
                editingHost, EditorRawDOMPoint(&aListItem, 0));
        if (forwardScanFromStartOfListItemResult.Failed()) {
          NS_WARNING(
              "WSRunScanner::ScanNextVisibleNodeOrBlockBoundary() failed");
          return NS_ERROR_FAILURE;
        }
        if (forwardScanFromStartOfListItemResult.ReachedSpecialContent() ||
            forwardScanFromStartOfListItemResult.ReachedBRElement() ||
            forwardScanFromStartOfListItemResult.ReachedHRElement()) {
          EditorRawDOMPoint atFoundElement(
              forwardScanFromStartOfListItemResult.RawPointAtContent());
          if (NS_WARN_IF(!atFoundElement.IsSetAndValid())) {
            return NS_ERROR_FAILURE;
          }
          nsresult rv = CollapseSelectionTo(atFoundElement);
          NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                               "HTMLEditor::CollapseSelectionTo() failed");
          return rv;
        }

        // XXX This may be not meaningful position if it reached block element
        //     in aListItem.
        nsresult rv = CollapseSelectionTo(
            forwardScanFromStartOfListItemResult.RawPoint());
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "HTMLEditor::CollapseSelectionTo() failed");
        return rv;
      }
    }
  }

  nsresult rv = CollapseSelectionToStartOf(aListItem);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::CollapseSelectionToStartOf() failed");
  return rv;
}

nsresult HTMLEditor::MoveNodesIntoNewBlockquoteElement(
    nsTArray<OwningNonNull<nsIContent>>& aArrayOfContents) {
  MOZ_ASSERT(IsTopLevelEditSubActionDataAvailable());

  // The idea here is to put the nodes into a minimal number of blockquotes.
  // When the user blockquotes something, they expect one blockquote.  That
  // may not be possible (for instance, if they have two table cells selected,
  // you need two blockquotes inside the cells).
  RefPtr<Element> curBlock;
  nsCOMPtr<nsINode> prevParent;

  for (auto& content : aArrayOfContents) {
    // If the node is a table element or list item, dive inside
    if (HTMLEditUtils::IsAnyTableElementButNotTable(content) ||
        HTMLEditUtils::IsListItem(content)) {
      // Forget any previous block
      curBlock = nullptr;
      // Recursion time
      AutoTArray<OwningNonNull<nsIContent>, 24> childContents;
      HTMLEditor::GetChildNodesOf(*content, childContents);
      nsresult rv = MoveNodesIntoNewBlockquoteElement(childContents);
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::MoveNodesIntoNewBlockquoteElement() failed");
        return rv;
      }
    }

    // If the node has different parent than previous node, further nodes in a
    // new parent
    if (prevParent) {
      if (prevParent != content->GetParentNode()) {
        // Forget any previous blockquote node we were using
        curBlock = nullptr;
        prevParent = content->GetParentNode();
      }
    } else {
      prevParent = content->GetParentNode();
    }

    // If no curBlock, make one
    if (!curBlock) {
      Result<RefPtr<Element>, nsresult> newBlockQuoteElementOrError =
          InsertElementWithSplittingAncestorsWithTransaction(
              *nsGkAtoms::blockquote, EditorDOMPoint(content),
              BRElementNextToSplitPoint::Keep);
      if (MOZ_UNLIKELY(newBlockQuoteElementOrError.isErr())) {
        NS_WARNING(
            "HTMLEditor::InsertElementWithSplittingAncestorsWithTransaction("
            "nsGkAtoms::blockquote) failed");
        return newBlockQuoteElementOrError.unwrapErr();
      }
      MOZ_ASSERT(newBlockQuoteElementOrError.inspect());
      // remember our new block for postprocessing
      // note: doesn't matter if we set mNewBlockElement multiple times.
      TopLevelEditSubActionDataRef().mNewBlockElement =
          newBlockQuoteElementOrError.inspect();
      curBlock = newBlockQuoteElementOrError.unwrap();
    }

    // MOZ_KnownLive because 'aArrayOfContents' is guaranteed to/ keep it alive.
    nsresult rv =
        MoveNodeToEndWithTransaction(MOZ_KnownLive(content), *curBlock);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::MoveNodeToEndWithTransaction() failed");
      return rv;
    }
  }
  return NS_OK;
}

nsresult HTMLEditor::RemoveBlockContainerElements(
    nsTArray<OwningNonNull<nsIContent>>& aArrayOfContents) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // Intent of this routine is to be used for converting to/from headers,
  // paragraphs, pre, and address.  Those blocks that pretty much just contain
  // inline things...
  RefPtr<Element> blockElement;
  nsCOMPtr<nsIContent> firstContent, lastContent;
  for (auto& content : aArrayOfContents) {
    // If curNode is an <address>, <p>, <hn>, or <pre>, remove it.
    if (HTMLEditUtils::IsFormatNode(content)) {
      // Process any partial progress saved
      if (blockElement) {
        SplitRangeOffFromNodeResult removeMiddleContainerResult =
            SplitRangeOffFromBlockAndRemoveMiddleContainer(
                *blockElement, *firstContent, *lastContent);
        if (removeMiddleContainerResult.Failed()) {
          NS_WARNING(
              "HTMLEditor::SplitRangeOffFromBlockAndRemoveMiddleContainer() "
              "failed");
          return removeMiddleContainerResult.Rv();
        }
        firstContent = lastContent = blockElement = nullptr;
      }
      if (!EditorUtils::IsEditableContent(content, EditorType::HTML)) {
        continue;
      }
      // Remove current block
      nsresult rv = RemoveBlockContainerWithTransaction(
          MOZ_KnownLive(*content->AsElement()));
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::RemoveBlockContainerWithTransaction() failed");
        return rv;
      }
      continue;
    }

    // XXX How about, <th>, <thead>, <tfoot>, <dt>, <dl>?
    if (content->IsAnyOfHTMLElements(
            nsGkAtoms::table, nsGkAtoms::tr, nsGkAtoms::tbody, nsGkAtoms::td,
            nsGkAtoms::li, nsGkAtoms::blockquote, nsGkAtoms::div) ||
        HTMLEditUtils::IsAnyListElement(content)) {
      // Process any partial progress saved
      if (blockElement) {
        SplitRangeOffFromNodeResult removeMiddleContainerResult =
            SplitRangeOffFromBlockAndRemoveMiddleContainer(
                *blockElement, *firstContent, *lastContent);
        if (removeMiddleContainerResult.Failed()) {
          NS_WARNING(
              "HTMLEditor::SplitRangeOffFromBlockAndRemoveMiddleContainer() "
              "failed");
          return removeMiddleContainerResult.Rv();
        }
        firstContent = lastContent = blockElement = nullptr;
      }
      if (!EditorUtils::IsEditableContent(content, EditorType::HTML)) {
        continue;
      }
      // Recursion time
      AutoTArray<OwningNonNull<nsIContent>, 24> childContents;
      HTMLEditor::GetChildNodesOf(*content, childContents);
      nsresult rv = RemoveBlockContainerElements(childContents);
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::RemoveBlockContainerElements() failed");
        return rv;
      }
      continue;
    }

    if (HTMLEditUtils::IsInlineElement(content)) {
      if (blockElement) {
        // If so, is this node a descendant?
        if (EditorUtils::IsDescendantOf(*content, *blockElement)) {
          // Then we don't need to do anything different for this node
          lastContent = content;
          continue;
        }
        // Otherwise, we have progressed beyond end of blockElement, so let's
        // handle it now.  We need to remove the portion of blockElement that
        // contains [firstContent - lastContent].
        SplitRangeOffFromNodeResult removeMiddleContainerResult =
            SplitRangeOffFromBlockAndRemoveMiddleContainer(
                *blockElement, *firstContent, *lastContent);
        if (removeMiddleContainerResult.Failed()) {
          NS_WARNING(
              "HTMLEditor::SplitRangeOffFromBlockAndRemoveMiddleContainer() "
              "failed");
          return removeMiddleContainerResult.Rv();
        }
        firstContent = lastContent = blockElement = nullptr;
        // Fall out and handle content
      }
      blockElement = HTMLEditUtils::GetAncestorElement(
          content, HTMLEditUtils::ClosestEditableBlockElement);
      if (!blockElement || !HTMLEditUtils::IsFormatNode(blockElement) ||
          !HTMLEditUtils::IsRemovableNode(*blockElement)) {
        // Not a block kind that we care about.
        blockElement = nullptr;
      } else {
        firstContent = lastContent = content;
      }
      continue;
    }

    if (blockElement) {
      // Some node that is already sans block style.  Skip over it and process
      // any partial progress saved.
      SplitRangeOffFromNodeResult removeMiddleContainerResult =
          SplitRangeOffFromBlockAndRemoveMiddleContainer(
              *blockElement, *firstContent, *lastContent);
      if (removeMiddleContainerResult.Failed()) {
        NS_WARNING(
            "HTMLEditor::SplitRangeOffFromBlockAndRemoveMiddleContainer() "
            "failed");
        return removeMiddleContainerResult.Rv();
      }
      firstContent = lastContent = blockElement = nullptr;
      continue;
    }
  }
  // Process any partial progress saved
  if (blockElement) {
    SplitRangeOffFromNodeResult removeMiddleContainerResult =
        SplitRangeOffFromBlockAndRemoveMiddleContainer(
            *blockElement, *firstContent, *lastContent);
    if (removeMiddleContainerResult.Failed()) {
      NS_WARNING(
          "HTMLEditor::SplitRangeOffFromBlockAndRemoveMiddleContainer() "
          "failed");
      return removeMiddleContainerResult.Rv();
    }
    firstContent = lastContent = blockElement = nullptr;
  }
  return NS_OK;
}

nsresult HTMLEditor::CreateOrChangeBlockContainerElement(
    nsTArray<OwningNonNull<nsIContent>>& aArrayOfContents, nsAtom& aBlockTag) {
  MOZ_ASSERT(IsTopLevelEditSubActionDataAvailable());

  // Intent of this routine is to be used for converting to/from headers,
  // paragraphs, pre, and address.  Those blocks that pretty much just contain
  // inline things...
  nsCOMPtr<Element> newBlock;
  nsCOMPtr<Element> curBlock;
  for (auto& content : aArrayOfContents) {
    EditorDOMPoint atContent(content);
    if (NS_WARN_IF(!atContent.IsInContentNode())) {
      // If given node has been removed from the document, let's ignore it
      // since the following code may need its parent replace it with new
      // block.
      curBlock = nullptr;
      newBlock = nullptr;
      continue;
    }

    // Is it already the right kind of block, or an uneditable block?
    if (content->IsHTMLElement(&aBlockTag) ||
        (!EditorUtils::IsEditableContent(content, EditorType::HTML) &&
         HTMLEditUtils::IsBlockElement(content))) {
      // Forget any previous block used for previous inline nodes
      curBlock = nullptr;
      // Do nothing to this block
      continue;
    }

    // If content is a address, p, header, address, or pre, replace it with a
    // new block of correct type.
    // XXX: pre can't hold everything the others can
    if (HTMLEditUtils::IsMozDiv(content) ||
        HTMLEditUtils::IsFormatNode(content)) {
      // Forget any previous block used for previous inline nodes
      curBlock = nullptr;
      newBlock = ReplaceContainerAndCloneAttributesWithTransaction(
          MOZ_KnownLive(*content->AsElement()), aBlockTag);
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (!newBlock) {
        NS_WARNING(
            "EditorBase::ReplaceContainerAndCloneAttributesWithTransaction() "
            "failed");
        return NS_ERROR_FAILURE;
      }
      // If the new block element was moved to different element or removed by
      // the web app via mutation event listener, we should stop handling this
      // action since we cannot handle each of a lot of edge cases.
      if (NS_WARN_IF(newBlock->GetParentNode() != atContent.GetContainer())) {
        return NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE;
      }
      continue;
    }

    if (HTMLEditUtils::IsTable(content) ||
        HTMLEditUtils::IsAnyListElement(content) ||
        content->IsAnyOfHTMLElements(nsGkAtoms::tbody, nsGkAtoms::tr,
                                     nsGkAtoms::td, nsGkAtoms::li,
                                     nsGkAtoms::blockquote, nsGkAtoms::div)) {
      // Forget any previous block used for previous inline nodes
      curBlock = nullptr;
      // Recursion time
      AutoTArray<OwningNonNull<nsIContent>, 24> childContents;
      HTMLEditor::GetChildNodesOf(*content, childContents);
      if (!childContents.IsEmpty()) {
        nsresult rv =
            CreateOrChangeBlockContainerElement(childContents, aBlockTag);
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "HTMLEditor::CreateOrChangeBlockContainerElement() failed");
          return rv;
        }
        continue;
      }

      // Make sure we can put a block here
      Result<RefPtr<Element>, nsresult> newBlockElementOrError =
          InsertElementWithSplittingAncestorsWithTransaction(
              aBlockTag, atContent, BRElementNextToSplitPoint::Keep);
      if (MOZ_UNLIKELY(newBlockElementOrError.isErr())) {
        NS_WARNING(
            nsPrintfCString(
                "HTMLEditor::"
                "InsertElementWithSplittingAncestorsWithTransaction(%s) failed",
                nsAtomCString(&aBlockTag).get())
                .get());
        return newBlockElementOrError.unwrapErr();
      }
      MOZ_ASSERT(newBlockElementOrError.inspect());
      // Remember our new block for postprocessing
      TopLevelEditSubActionDataRef().mNewBlockElement =
          newBlockElementOrError.unwrap();
      continue;
    }

    if (content->IsHTMLElement(nsGkAtoms::br)) {
      // If the node is a break, we honor it by putting further nodes in a new
      // parent
      if (curBlock) {
        // Forget any previous block used for previous inline nodes
        curBlock = nullptr;
        // MOZ_KnownLive because 'aArrayOfContents' is guaranteed to keep it
        // alive.
        nsresult rv = DeleteNodeWithTransaction(MOZ_KnownLive(*content));
        if (NS_WARN_IF(Destroyed())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_FAILED(rv)) {
          NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
          return rv;
        }
        continue;
      }

      // The break is the first (or even only) node we encountered.  Create a
      // block for it.
      Result<RefPtr<Element>, nsresult> newBlockElementOrError =
          InsertElementWithSplittingAncestorsWithTransaction(
              aBlockTag, atContent, BRElementNextToSplitPoint::Keep);
      if (MOZ_UNLIKELY(newBlockElementOrError.isErr())) {
        NS_WARNING(
            nsPrintfCString(
                "HTMLEditor::"
                "InsertElementWithSplittingAncestorsWithTransaction(%s) failed",
                nsAtomCString(&aBlockTag).get())
                .get());
        return newBlockElementOrError.unwrapErr();
      }
      MOZ_ASSERT(newBlockElementOrError.inspect());
      curBlock = newBlockElementOrError.unwrap();
      // Remember our new block for postprocessing
      TopLevelEditSubActionDataRef().mNewBlockElement = curBlock;
      // Note: doesn't matter if we set mNewBlockElement multiple times.
      //
      // MOZ_KnownLive because 'aArrayOfContents' is guaranteed to keep it
      // alive.
      nsresult rv =
          MoveNodeToEndWithTransaction(MOZ_KnownLive(content), *curBlock);
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::MoveNodeToEndWithTransaction() failed");
        return rv;
      }
      continue;
    }

    if (HTMLEditUtils::IsInlineElement(content)) {
      // If content is inline, pull it into curBlock.  Note: it's assumed that
      // consecutive inline nodes in aNodeArray are actually members of the
      // same block parent.  This happens to be true now as a side effect of
      // how aNodeArray is contructed, but some additional logic should be
      // added here if that should change
      //
      // If content is a non editable, drop it if we are going to <pre>.
      if (&aBlockTag == nsGkAtoms::pre &&
          !EditorUtils::IsEditableContent(content, EditorType::HTML)) {
        // Do nothing to this block
        continue;
      }

      // If no curBlock, make one
      if (!curBlock) {
        Result<RefPtr<Element>, nsresult> newBlockElementOrError =
            InsertElementWithSplittingAncestorsWithTransaction(
                aBlockTag, atContent, BRElementNextToSplitPoint::Keep);
        if (MOZ_UNLIKELY(newBlockElementOrError.isErr())) {
          NS_WARNING(nsPrintfCString("HTMLEditor::"
                                     "InsertElementWithSplittingAncestorsWithTr"
                                     "ansaction(%s) failed",
                                     nsAtomCString(&aBlockTag).get())
                         .get());
          return newBlockElementOrError.unwrapErr();
        }
        MOZ_ASSERT(newBlockElementOrError.inspect());
        curBlock = newBlockElementOrError.unwrap();

        // Update container of content.
        atContent.Set(content);

        // Remember our new block for postprocessing
        TopLevelEditSubActionDataRef().mNewBlockElement = curBlock;
        // Note: doesn't matter if we set mNewBlockElement multiple times.
      }

      if (NS_WARN_IF(!atContent.IsSet())) {
        // This is possible due to mutation events, let's not assert
        return NS_ERROR_UNEXPECTED;
      }

      // XXX If content is a br, replace it with a return if going to <pre>

      // This is a continuation of some inline nodes that belong together in
      // the same block item.  Use curBlock.
      //
      // MOZ_KnownLive because 'aArrayOfContents' is guaranteed to keep it
      // alive.  We could try to make that a rvalue ref and create a const array
      // on the stack here, but callers are passing in auto arrays, and we don't
      // want to introduce copies..
      nsresult rv =
          MoveNodeToEndWithTransaction(MOZ_KnownLive(content), *curBlock);
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::MoveNodeToEndWithTransaction() failed");
        return rv;
      }
    }
  }
  return NS_OK;
}

SplitNodeResult HTMLEditor::MaybeSplitAncestorsForInsertWithTransaction(
    nsAtom& aTag, const EditorDOMPoint& aStartOfDeepestRightNode) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!aStartOfDeepestRightNode.IsSet())) {
    return SplitNodeResult(NS_ERROR_INVALID_ARG);
  }
  MOZ_ASSERT(aStartOfDeepestRightNode.IsSetAndValid());

  RefPtr<Element> host = GetActiveEditingHost();
  if (NS_WARN_IF(!host)) {
    return SplitNodeResult(NS_ERROR_FAILURE);
  }

  // The point must be descendant of editing host.
  if (aStartOfDeepestRightNode.GetContainer() != host &&
      !EditorUtils::IsDescendantOf(*aStartOfDeepestRightNode.GetContainer(),
                                   *host)) {
    NS_WARNING("aStartOfDeepestRightNode was not in editing host");
    return SplitNodeResult(NS_ERROR_INVALID_ARG);
  }

  // Look for a node that can legally contain the tag.
  EditorDOMPoint pointToInsert(aStartOfDeepestRightNode);
  for (; pointToInsert.IsSet();
       pointToInsert.Set(pointToInsert.GetContainer())) {
    // We cannot split active editing host and its ancestor.  So, there is
    // no element to contain the specified element.
    if (pointToInsert.GetChild() == host) {
      NS_WARNING(
          "HTMLEditor::MaybeSplitAncestorsForInsertWithTransaction() reached "
          "editing host");
      return SplitNodeResult(NS_ERROR_FAILURE);
    }

    if (HTMLEditUtils::CanNodeContain(*pointToInsert.GetContainer(), aTag)) {
      // Found an ancestor node which can contain the element.
      break;
    }
  }

  MOZ_DIAGNOSTIC_ASSERT(pointToInsert.IsSet());

  // If the point itself can contain the tag, we don't need to split any
  // ancestor nodes.  In this case, we should return the given split point
  // as is.
  if (pointToInsert.GetContainer() == aStartOfDeepestRightNode.GetContainer()) {
    return SplitNodeResult(aStartOfDeepestRightNode);
  }

  SplitNodeResult splitNodeResult = SplitNodeDeepWithTransaction(
      MOZ_KnownLive(*pointToInsert.GetChild()), aStartOfDeepestRightNode,
      SplitAtEdges::eAllowToCreateEmptyContainer);
  NS_WARNING_ASSERTION(splitNodeResult.Succeeded(),
                       "HTMLEditor::SplitNodeDeepWithTransaction(SplitAtEdges::"
                       "eAllowToCreateEmptyContainer) failed");
  return splitNodeResult;
}

Result<RefPtr<Element>, nsresult>
HTMLEditor::InsertElementWithSplittingAncestorsWithTransaction(
    nsAtom& aTagName, const EditorDOMPoint& aPointToInsert,
    BRElementNextToSplitPoint aBRElementNextToSplitPoint) {
  MOZ_ASSERT(aPointToInsert.IsSetAndValid());

  SplitNodeResult splitNodeResult =
      MaybeSplitAncestorsForInsertWithTransaction(aTagName, aPointToInsert);
  if (MOZ_UNLIKELY(splitNodeResult.Failed())) {
    NS_WARNING(
        "HTMLEditor::MaybeSplitAncestorsForInsertWithTransaction() failed");
    return Err(splitNodeResult.Rv());
  }

  // If current handling node has been moved from the container by a
  // mutation event listener when we need to do something more for it,
  // we should stop handling this action since we cannot handle each of
  // a lot of edge cases.
  if (MOZ_UNLIKELY(NS_WARN_IF(aPointToInsert.HasChildMovedFromContainer()))) {
    return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }

  EditorDOMPoint splitPoint = splitNodeResult.AtSplitPoint<EditorDOMPoint>();

  if (aBRElementNextToSplitPoint == BRElementNextToSplitPoint::Delete) {
    // Consume a trailing br, if any.  This is to keep an alignment from
    // creating extra lines, if possible.
    // XXX Perhaps, we should stop handling this if there is no editing host.
    if (Element* editingHost = GetActiveEditingHost()) {
      if (nsCOMPtr<nsIContent> maybeBRContent = HTMLEditUtils::GetNextContent(
              splitPoint,
              {WalkTreeOption::IgnoreNonEditableNode,
               WalkTreeOption::StopAtBlockBoundary},
              editingHost)) {
        if (maybeBRContent->IsHTMLElement(nsGkAtoms::br) &&
            splitPoint.GetChild()) {
          // Making use of html structure... if next node after where we are
          // putting our div is not a block, then the br we found is in same
          // block we are, so it's safe to consume it.
          if (nsIContent* nextEditableSibling = HTMLEditUtils::GetNextSibling(
                  *splitPoint.GetChild(),
                  {WalkTreeOption::IgnoreNonEditableNode})) {
            if (!HTMLEditUtils::IsBlockElement(*nextEditableSibling)) {
              AutoEditorDOMPointChildInvalidator lockOffset(splitPoint);
              nsresult rv = DeleteNodeWithTransaction(*maybeBRContent);
              if (NS_WARN_IF(Destroyed())) {
                return Err(NS_ERROR_EDITOR_DESTROYED);
              }
              if (NS_FAILED(rv)) {
                NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
                return Err(rv);
              }
            }
          }
        }
      }
    }
  }

  Result<RefPtr<Element>, nsresult> newElementOrError =
      CreateAndInsertElementWithTransaction(aTagName, splitPoint);
  if (MOZ_UNLIKELY(newElementOrError.isErr())) {
    NS_WARNING("HTMLEditor::CreateAndInsertElementWithTransaction() failed");
    return newElementOrError;
  }
  MOZ_ASSERT(newElementOrError.inspect());

  // If the new block element was moved to different element or removed by
  // the web app via mutation event listener, we should stop handling this
  // action since we cannot handle each of a lot of edge cases.
  if (MOZ_UNLIKELY(NS_WARN_IF(newElementOrError.inspect()->GetParentNode() !=
                              splitPoint.GetContainer()))) {
    return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }

  return newElementOrError;
}

nsresult HTMLEditor::JoinNearestEditableNodesWithTransaction(
    nsIContent& aNodeLeft, nsIContent& aNodeRight,
    EditorDOMPoint* aNewFirstChildOfRightNode) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(aNewFirstChildOfRightNode);

  // Caller responsible for left and right node being the same type
  if (NS_WARN_IF(!aNodeLeft.GetParentNode())) {
    return NS_ERROR_FAILURE;
  }
  // If they don't have the same parent, first move the right node to after
  // the left one
  if (aNodeLeft.GetParentNode() != aNodeRight.GetParentNode()) {
    nsresult rv =
        MoveNodeWithTransaction(aNodeRight, EditorDOMPoint(&aNodeLeft));
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::MoveNodeWithTransaction() failed");
      return rv;
    }
  }

  // Separate join rules for differing blocks
  if (HTMLEditUtils::IsAnyListElement(&aNodeLeft) || aNodeLeft.IsText()) {
    // For lists, merge shallow (wouldn't want to combine list items)
    JoinNodesResult joinNodesResult =
        JoinNodesWithTransaction(aNodeLeft, aNodeRight);
    if (MOZ_UNLIKELY(joinNodesResult.Failed())) {
      NS_WARNING("HTMLEditor::JoinNodesWithTransaction failed");
      return joinNodesResult.Rv();
    }
    *aNewFirstChildOfRightNode =
        joinNodesResult.AtJoinedPoint<EditorDOMPoint>();
    return joinNodesResult.Rv();
  }

  // Remember the last left child, and first right child
  nsCOMPtr<nsIContent> lastEditableChildOfLeftContent =
      HTMLEditUtils::GetLastChild(aNodeLeft,
                                  {WalkTreeOption::IgnoreNonEditableNode});
  if (MOZ_UNLIKELY(NS_WARN_IF(!lastEditableChildOfLeftContent))) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIContent> firstEditableChildOfRightContent =
      HTMLEditUtils::GetFirstChild(aNodeRight,
                                   {WalkTreeOption::IgnoreNonEditableNode});
  if (NS_WARN_IF(!firstEditableChildOfRightContent)) {
    return NS_ERROR_FAILURE;
  }

  // For list items, divs, etc., merge smart
  JoinNodesResult joinNodesResult =
      JoinNodesWithTransaction(aNodeLeft, aNodeRight);
  if (MOZ_UNLIKELY(joinNodesResult.Failed())) {
    NS_WARNING("HTMLEditor::JoinNodesWithTransaction() failed");
    return joinNodesResult.Rv();
  }

  if ((lastEditableChildOfLeftContent->IsText() ||
       lastEditableChildOfLeftContent->IsElement()) &&
      HTMLEditUtils::CanContentsBeJoined(*lastEditableChildOfLeftContent,
                                         *firstEditableChildOfRightContent,
                                         StyleDifference::CompareIfElements)) {
    nsresult rv = JoinNearestEditableNodesWithTransaction(
        *lastEditableChildOfLeftContent, *firstEditableChildOfRightContent,
        aNewFirstChildOfRightNode);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::JoinNearestEditableNodesWithTransaction() failed");
    return rv;
  }
  *aNewFirstChildOfRightNode = joinNodesResult.AtJoinedPoint<EditorDOMPoint>();
  return NS_OK;
}

Element* HTMLEditor::GetMostAncestorMailCiteElement(nsINode& aNode) const {
  Element* mailCiteElement = nullptr;
  const bool isPlaintextEditor = IsInPlaintextMode();
  for (nsINode* node = &aNode; node; node = node->GetParentNode()) {
    if ((isPlaintextEditor && node->IsHTMLElement(nsGkAtoms::pre)) ||
        HTMLEditUtils::IsMailCite(node)) {
      mailCiteElement = node->AsElement();
    }
    if (node->IsHTMLElement(nsGkAtoms::body)) {
      break;
    }
  }
  return mailCiteElement;
}

nsresult HTMLEditor::CacheInlineStyles(nsIContent& aContent) {
  MOZ_ASSERT(IsTopLevelEditSubActionDataAvailable());

  nsresult rv = GetInlineStyles(
      aContent, *TopLevelEditSubActionDataRef().mCachedInlineStyles);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::GetInlineStyles() failed");
  return rv;
}

nsresult HTMLEditor::GetInlineStyles(nsIContent& aContent,
                                     AutoStyleCacheArray& aStyleCacheArray) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(aStyleCacheArray.IsEmpty());

  bool useCSS = IsCSSEnabled();

  for (nsStaticAtom* property : {nsGkAtoms::b,
                                 nsGkAtoms::i,
                                 nsGkAtoms::u,
                                 nsGkAtoms::s,
                                 nsGkAtoms::strike,
                                 nsGkAtoms::face,
                                 nsGkAtoms::size,
                                 nsGkAtoms::color,
                                 nsGkAtoms::tt,
                                 nsGkAtoms::em,
                                 nsGkAtoms::strong,
                                 nsGkAtoms::dfn,
                                 nsGkAtoms::code,
                                 nsGkAtoms::samp,
                                 nsGkAtoms::var,
                                 nsGkAtoms::cite,
                                 nsGkAtoms::abbr,
                                 nsGkAtoms::acronym,
                                 nsGkAtoms::backgroundColor,
                                 nsGkAtoms::sub,
                                 nsGkAtoms::sup}) {
    nsStaticAtom *tag, *attribute;
    if (property == nsGkAtoms::face || property == nsGkAtoms::size ||
        property == nsGkAtoms::color) {
      tag = nsGkAtoms::font;
      attribute = property;
    } else {
      tag = property;
      attribute = nullptr;
    }
    // If type-in state is set, don't intervene
    bool typeInSet, unused;
    mTypeInState->GetTypingState(typeInSet, unused, tag, attribute, nullptr);
    if (typeInSet) {
      continue;
    }
    bool isSet = false;
    nsString value;  // Don't use nsAutoString here because it requires memcpy
                     // at creating new StyleCache instance.
    // Don't use CSS for <font size>, we don't support it usefully (bug 780035)
    if (!useCSS || (property == nsGkAtoms::size)) {
      isSet = HTMLEditUtils::IsInlineStyleSetByElement(
          aContent, *tag, attribute, nullptr, &value);
    } else {
      isSet = CSSEditUtils::IsComputedCSSEquivalentToHTMLInlineStyleSet(
          aContent, MOZ_KnownLive(tag), MOZ_KnownLive(attribute), value);
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
    }
    if (isSet) {
      aStyleCacheArray.AppendElement(StyleCache(tag, attribute, value));
    }
  }
  return NS_OK;
}

nsresult HTMLEditor::ReapplyCachedStyles() {
  MOZ_ASSERT(IsTopLevelEditSubActionDataAvailable());

  // The idea here is to examine our cached list of styles and see if any have
  // been removed.  If so, add typeinstate for them, so that they will be
  // reinserted when new content is added.

  if (TopLevelEditSubActionDataRef().mCachedInlineStyles->IsEmpty() ||
      !SelectionRef().RangeCount()) {
    return NS_OK;
  }

  // remember if we are in css mode
  bool useCSS = IsCSSEnabled();

  const RangeBoundary& atStartOfSelection =
      SelectionRef().GetRangeAt(0)->StartRef();
  nsCOMPtr<nsIContent> startContainerContent =
      atStartOfSelection.Container() &&
              atStartOfSelection.Container()->IsContent()
          ? atStartOfSelection.Container()->AsContent()
          : nullptr;
  if (NS_WARN_IF(!startContainerContent)) {
    return NS_OK;
  }

  AutoStyleCacheArray styleCacheArrayAtInsertionPoint;
  nsresult rv =
      GetInlineStyles(*startContainerContent, styleCacheArrayAtInsertionPoint);
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::GetInlineStyles() failed, but ignored");
    return NS_OK;
  }

  for (StyleCache& styleCacheBeforeEdit :
       *TopLevelEditSubActionDataRef().mCachedInlineStyles) {
    bool isFirst = false, isAny = false, isAll = false;
    nsAutoString currentValue;
    if (useCSS) {
      // check computed style first in css case
      isAny = CSSEditUtils::IsComputedCSSEquivalentToHTMLInlineStyleSet(
          *startContainerContent, MOZ_KnownLive(styleCacheBeforeEdit.Tag()),
          MOZ_KnownLive(styleCacheBeforeEdit.GetAttribute()), currentValue);
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
    }
    if (!isAny) {
      // then check typeinstate and html style
      nsresult rv = GetInlinePropertyBase(
          MOZ_KnownLive(*styleCacheBeforeEdit.Tag()),
          MOZ_KnownLive(styleCacheBeforeEdit.GetAttribute()),
          &styleCacheBeforeEdit.Value(), &isFirst, &isAny, &isAll,
          &currentValue);
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::GetInlinePropertyBase() failed");
        return rv;
      }
    }
    // This style has disappeared through deletion.  Let's add the styles to
    // mTypeInState when same style isn't applied to the node already.
    if (isAny && !IsStyleCachePreservingSubAction(GetTopLevelEditSubAction())) {
      continue;
    }
    AutoStyleCacheArray::index_type index =
        styleCacheArrayAtInsertionPoint.IndexOf(
            styleCacheBeforeEdit.Tag(), styleCacheBeforeEdit.GetAttribute());
    if (index == AutoStyleCacheArray::NoIndex ||
        styleCacheBeforeEdit.Value() !=
            styleCacheArrayAtInsertionPoint.ElementAt(index).Value()) {
      mTypeInState->SetProp(styleCacheBeforeEdit.Tag(),
                            styleCacheBeforeEdit.GetAttribute(),
                            styleCacheBeforeEdit.Value());
    }
  }
  return NS_OK;
}

nsresult HTMLEditor::InsertBRElementToEmptyListItemsAndTableCellsInRange(
    const RawRangeBoundary& aStartRef, const RawRangeBoundary& aEndRef) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  AutoTArray<OwningNonNull<Element>, 64> arrayOfEmptyElements;
  DOMIterator iter;
  if (NS_FAILED(iter.Init(aStartRef, aEndRef))) {
    NS_WARNING("DOMIterator::Init() failed");
    return NS_ERROR_FAILURE;
  }
  iter.AppendNodesToArray(
      +[](nsINode& aNode, void* aSelf) {
        MOZ_ASSERT(Element::FromNode(&aNode));
        MOZ_ASSERT(aSelf);
        Element* element = aNode.AsElement();
        if (!EditorUtils::IsEditableContent(*element, EditorType::HTML) ||
            (!HTMLEditUtils::IsListItem(element) &&
             !HTMLEditUtils::IsTableCellOrCaption(*element))) {
          return false;
        }
        return HTMLEditUtils::IsEmptyNode(
            *element, {EmptyCheckOption::TreatSingleBRElementAsVisible});
      },
      arrayOfEmptyElements, this);

  // Put padding <br> elements for empty <li> and <td>.
  for (auto& emptyElement : arrayOfEmptyElements) {
    // Need to put br at END of node.  It may have empty containers in it and
    // still pass the "IsEmptyNode" test, and we want the br's to be after
    // them.  Also, we want the br to be after the selection if the selection
    // is in this node.
    EditorDOMPoint endOfNode(EditorDOMPoint::AtEndOf(emptyElement));
    CreateElementResult createPaddingBRResult =
        InsertPaddingBRElementForEmptyLastLineWithTransaction(endOfNode);
    if (createPaddingBRResult.Failed()) {
      NS_WARNING(
          "HTMLEditor::InsertPaddingBRElementForEmptyLastLineWithTransaction() "
          "failed");
      return createPaddingBRResult.Rv();
    }
  }
  return NS_OK;
}

nsresult HTMLEditor::EnsureCaretInBlockElement(Element& aElement) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(SelectionRef().IsCollapsed());

  EditorRawDOMPoint atCaret(EditorBase::GetStartPoint(SelectionRef()));
  if (NS_WARN_IF(!atCaret.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  // Use ranges and RangeUtils::CompareNodeToRange() to compare selection
  // start to new block.
  RefPtr<StaticRange> staticRange =
      StaticRange::Create(atCaret.ToRawRangeBoundary(),
                          atCaret.ToRawRangeBoundary(), IgnoreErrors());
  if (!staticRange) {
    NS_WARNING("StaticRange::Create() failed");
    return NS_ERROR_FAILURE;
  }

  bool nodeBefore, nodeAfter;
  nsresult rv = RangeUtils::CompareNodeToRange(&aElement, staticRange,
                                               &nodeBefore, &nodeAfter);
  if (NS_FAILED(rv)) {
    NS_WARNING("RangeUtils::CompareNodeToRange() failed");
    return rv;
  }

  if (nodeBefore && nodeAfter) {
    return NS_OK;  // selection is inside block
  }

  if (nodeBefore) {
    // selection is after block.  put at end of block.
    nsIContent* lastEditableContent = HTMLEditUtils::GetLastChild(
        aElement, {WalkTreeOption::IgnoreNonEditableNode});
    if (!lastEditableContent) {
      lastEditableContent = &aElement;
    }
    EditorRawDOMPoint endPoint;
    if (lastEditableContent->IsText() ||
        HTMLEditUtils::IsContainerNode(*lastEditableContent)) {
      endPoint.SetToEndOf(lastEditableContent);
    } else {
      endPoint.SetAfter(lastEditableContent);
      if (NS_WARN_IF(!endPoint.IsSet())) {
        return NS_ERROR_FAILURE;
      }
    }
    nsresult rv = CollapseSelectionTo(endPoint);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::CollapseSelectionTo() failed");
    return rv;
  }

  // selection is before block.  put at start of block.
  nsIContent* firstEditableContent = HTMLEditUtils::GetFirstChild(
      aElement, {WalkTreeOption::IgnoreNonEditableNode});
  if (!firstEditableContent) {
    firstEditableContent = &aElement;
  }
  EditorRawDOMPoint atStartOfBlock;
  if (firstEditableContent->IsText() ||
      HTMLEditUtils::IsContainerNode(*firstEditableContent)) {
    atStartOfBlock.Set(firstEditableContent);
  } else {
    atStartOfBlock.Set(firstEditableContent, 0);
  }
  rv = CollapseSelectionTo(atStartOfBlock);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::CollapseSelectionTo() failed");
  return rv;
}

void HTMLEditor::SetSelectionInterlinePosition() {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(SelectionRef().IsCollapsed());

  // Get the (collapsed) selection location
  const nsRange* firstRange = SelectionRef().GetRangeAt(0);
  if (NS_WARN_IF(!firstRange)) {
    return;
  }

  EditorDOMPoint atCaret(firstRange->StartRef());
  if (NS_WARN_IF(!atCaret.IsSet())) {
    return;
  }
  MOZ_ASSERT(atCaret.IsSetAndValid());

  // First, let's check to see if we are after a `<br>`.  We take care of this
  // special-case first so that we don't accidentally fall through into one of
  // the other conditionals.
  // XXX Although I don't understand "interline position", if caret is
  //     immediately after non-editable contents, but previous editable
  //     content is `<br>`, does this do right thing?
  if (Element* editingHost = GetActiveEditingHost()) {
    if (nsIContent* previousEditableContentInBlock =
            HTMLEditUtils::GetPreviousContent(
                atCaret,
                {WalkTreeOption::IgnoreNonEditableNode,
                 WalkTreeOption::StopAtBlockBoundary},
                editingHost)) {
      if (previousEditableContentInBlock->IsHTMLElement(nsGkAtoms::br)) {
        IgnoredErrorResult ignoredError;
        SelectionRef().SetInterlinePosition(true, ignoredError);
        NS_WARNING_ASSERTION(
            !ignoredError.Failed(),
            "Selection::SetInterlinePosition(true) failed, but ignored");
        return;
      }
    }
  }

  if (!atCaret.GetChild()) {
    return;
  }

  // If caret is immediately after a block, set interline position to "right".
  // XXX Although I don't understand "interline position", if caret is
  //     immediately after non-editable contents, but previous editable
  //     content is a block, does this do right thing?
  if (nsIContent* previousEditableContentInBlockAtCaret =
          HTMLEditUtils::GetPreviousSibling(
              *atCaret.GetChild(), {WalkTreeOption::IgnoreNonEditableNode})) {
    if (HTMLEditUtils::IsBlockElement(*previousEditableContentInBlockAtCaret)) {
      IgnoredErrorResult ignoredError;
      SelectionRef().SetInterlinePosition(true, ignoredError);
      NS_WARNING_ASSERTION(
          !ignoredError.Failed(),
          "Selection::SetInterlinePosition(true) failed, but ignored");
      return;
    }
  }

  // If caret is immediately before a block, set interline position to "left".
  // XXX Although I don't understand "interline position", if caret is
  //     immediately before non-editable contents, but next editable
  //     content is a block, does this do right thing?
  if (nsIContent* nextEditableContentInBlockAtCaret =
          HTMLEditUtils::GetNextSibling(
              *atCaret.GetChild(), {WalkTreeOption::IgnoreNonEditableNode})) {
    if (HTMLEditUtils::IsBlockElement(*nextEditableContentInBlockAtCaret)) {
      IgnoredErrorResult ignoredError;
      SelectionRef().SetInterlinePosition(false, ignoredError);
      NS_WARNING_ASSERTION(
          !ignoredError.Failed(),
          "Selection::SetInterlinePosition(false) failed, but ignored");
    }
  }
}

nsresult HTMLEditor::AdjustCaretPositionAndEnsurePaddingBRElement(
    nsIEditor::EDirection aDirectionAndAmount) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(SelectionRef().IsCollapsed());

  EditorDOMPoint point(EditorBase::GetStartPoint(SelectionRef()));
  if (NS_WARN_IF(!point.IsInContentNode())) {
    return NS_ERROR_FAILURE;
  }

  // If selection start is not editable, climb up the tree until editable one.
  while (!EditorUtils::IsEditableContent(*point.ContainerAsContent(),
                                         EditorType::HTML)) {
    point.Set(point.GetContainer());
    if (NS_WARN_IF(!point.IsInContentNode())) {
      return NS_ERROR_FAILURE;
    }
  }

  // If caret is in empty block element, we need to insert a `<br>` element
  // because the block should have one-line height.
  // XXX Even if only a part of the block is editable, shouldn't we put
  //     caret if the block element is now empty?
  if (Element* const editableBlockElement =
          HTMLEditUtils::GetInclusiveAncestorElement(
              *point.ContainerAsContent(),
              HTMLEditUtils::ClosestEditableBlockElement)) {
    if (editableBlockElement &&
        HTMLEditUtils::IsEmptyNode(
            *editableBlockElement,
            {EmptyCheckOption::TreatSingleBRElementAsVisible}) &&
        HTMLEditUtils::CanNodeContain(*point.GetContainer(), *nsGkAtoms::br)) {
      Element* bodyOrDocumentElement = GetRoot();
      if (NS_WARN_IF(!bodyOrDocumentElement)) {
        return NS_ERROR_FAILURE;
      }
      if (point.GetContainer() == bodyOrDocumentElement) {
        // Our root node is completely empty. Don't add a <br> here.
        // AfterEditInner() will add one for us when it calls
        // EditorBase::MaybeCreatePaddingBRElementForEmptyEditor().
        // XXX This kind of dependency between methods makes us spaghetti.
        //     Let's handle it here later.
        // XXX This looks odd check.  If active editing host is not a
        //     `<body>`, what are we doing?
        return NS_OK;
      }
      CreateElementResult createPaddingBRResult =
          InsertPaddingBRElementForEmptyLastLineWithTransaction(point);
      NS_WARNING_ASSERTION(
          createPaddingBRResult.Succeeded(),
          "HTMLEditor::InsertPaddingBRElementForEmptyLastLineWithTransaction() "
          "failed");
      return createPaddingBRResult.Rv();
    }
  }

  // XXX Perhaps, we should do something if we're in a data node but not
  //     a text node.
  if (point.IsInTextNode()) {
    return NS_OK;
  }

  // Do we need to insert a padding <br> element for empty last line?  We do
  // if we are:
  // 1) prior node is in same block where selection is AND
  // 2) prior node is a br AND
  // 3) that br is not visible
  RefPtr<Element> editingHost = GetActiveEditingHost();
  if (!editingHost) {
    return NS_OK;
  }

  if (nsCOMPtr<nsIContent> previousEditableContent =
          HTMLEditUtils::GetPreviousContent(
              point, {WalkTreeOption::IgnoreNonEditableNode}, editingHost)) {
    // If caret and previous editable content are in same block element
    // (even if it's a non-editable element), we should put a padding <br>
    // element at end of the block.
    const Element* const blockElementContainingCaret =
        HTMLEditUtils::GetInclusiveAncestorElement(
            *point.ContainerAsContent(), HTMLEditUtils::ClosestBlockElement);
    const Element* const blockElementContainingPreviousEditableContent =
        HTMLEditUtils::GetAncestorElement(*previousEditableContent,
                                          HTMLEditUtils::ClosestBlockElement);
    // If previous editable content of caret is in same block and a `<br>`
    // element, we need to adjust interline position.
    if (blockElementContainingCaret &&
        blockElementContainingCaret ==
            blockElementContainingPreviousEditableContent &&
        point.ContainerAsContent()->GetEditingHost() ==
            previousEditableContent->GetEditingHost() &&
        previousEditableContent &&
        previousEditableContent->IsHTMLElement(nsGkAtoms::br)) {
      // If it's an invisible `<br>` element, we need to insert a padding
      // `<br>` element for making empty line have one-line height.
      if (HTMLEditUtils::IsInvisibleBRElement(*previousEditableContent)) {
        CreateElementResult createPaddingBRResult =
            InsertPaddingBRElementForEmptyLastLineWithTransaction(point);
        if (createPaddingBRResult.Failed()) {
          NS_WARNING(
              "HTMLEditor::"
              "InsertPaddingBRElementForEmptyLastLineWithTransaction() "
              "failed");
          return createPaddingBRResult.Rv();
        }
        point.Set(createPaddingBRResult.GetNewNode());
        // Selection stays *before* padding `<br>` element for empty last
        // line, sticking to it.
        IgnoredErrorResult ignoredError;
        SelectionRef().SetInterlinePosition(true, ignoredError);
        if (NS_WARN_IF(Destroyed())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        NS_WARNING_ASSERTION(
            !ignoredError.Failed(),
            "Selection::SetInterlinePosition(true) failed, but ignored");
        nsresult rv = CollapseSelectionTo(point);
        if (NS_FAILED(rv)) {
          NS_WARNING("HTMLEditor::CollapseSelectionTo() failed");
          return rv;
        }
      }
      // If it's a visible `<br>` element and next editable content is a
      // padding `<br>` element, we need to set interline position.
      else if (nsIContent* nextEditableContentInBlock =
                   HTMLEditUtils::GetNextContent(
                       *previousEditableContent,
                       {WalkTreeOption::IgnoreNonEditableNode,
                        WalkTreeOption::StopAtBlockBoundary},
                       editingHost)) {
        if (EditorUtils::IsPaddingBRElementForEmptyLastLine(
                *nextEditableContentInBlock)) {
          // Make it stick to the padding `<br>` element so that it will be
          // on blank line.
          IgnoredErrorResult ignoredError;
          SelectionRef().SetInterlinePosition(true, ignoredError);
          NS_WARNING_ASSERTION(
              !ignoredError.Failed(),
              "Selection::SetInterlinePosition(true) failed, but ignored");
        }
      }
    }
  }

  // If previous editable content in same block is `<br>`, text node, `<img>`
  //  or `<hr>`, current caret position is fine.
  if (nsIContent* previousEditableContentInBlock =
          HTMLEditUtils::GetPreviousContent(
              point,
              {WalkTreeOption::IgnoreNonEditableNode,
               WalkTreeOption::StopAtBlockBoundary},
              editingHost)) {
    if (previousEditableContentInBlock->IsHTMLElement(nsGkAtoms::br) ||
        previousEditableContentInBlock->IsText() ||
        HTMLEditUtils::IsImage(previousEditableContentInBlock) ||
        previousEditableContentInBlock->IsHTMLElement(nsGkAtoms::hr)) {
      return NS_OK;
    }
  }

  // If next editable content in same block is `<br>`, text node, `<img>` or
  // `<hr>`, current caret position is fine.
  if (nsIContent* nextEditableContentInBlock =
          HTMLEditUtils::GetNextContent(point,
                                        {WalkTreeOption::IgnoreNonEditableNode,
                                         WalkTreeOption::StopAtBlockBoundary},
                                        editingHost)) {
    if (nextEditableContentInBlock->IsText() ||
        nextEditableContentInBlock->IsAnyOfHTMLElements(
            nsGkAtoms::br, nsGkAtoms::img, nsGkAtoms::hr)) {
      return NS_OK;
    }
  }

  // Otherwise, look for a near editable content towards edit action direction.

  // If there is no editable content, keep current caret position.
  // XXX Why do we treat `nsIEditor::ePreviousWord` etc as forward direction?
  nsIContent* nearEditableContent = HTMLEditUtils::GetAdjacentContentToPutCaret(
      point,
      aDirectionAndAmount == nsIEditor::ePrevious ? WalkTreeDirection::Backward
                                                  : WalkTreeDirection::Forward,
      *editingHost);
  if (!nearEditableContent) {
    return NS_OK;
  }

  EditorRawDOMPoint pointToPutCaret =
      HTMLEditUtils::GetGoodCaretPointFor<EditorRawDOMPoint>(
          *nearEditableContent, aDirectionAndAmount);
  if (!pointToPutCaret.IsSet()) {
    NS_WARNING("HTMLEditUtils::GetGoodCaretPointFor() failed");
    return NS_ERROR_FAILURE;
  }
  nsresult rv = CollapseSelectionTo(pointToPutCaret);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::CollapseSelectionTo() failed");
  return rv;
}

nsresult HTMLEditor::RemoveEmptyNodesIn(nsRange& aRange) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(aRange.IsPositioned());

  // Some general notes on the algorithm used here: the goal is to examine all
  // the nodes in aRange, and remove the empty ones.  We do this by
  // using a content iterator to traverse all the nodes in the range, and
  // placing the empty nodes into an array.  After finishing the iteration,
  // we delete the empty nodes in the array.  (They cannot be deleted as we
  // find them because that would invalidate the iterator.)
  //
  // Since checking to see if a node is empty can be costly for nodes with
  // many descendants, there are some optimizations made.  I rely on the fact
  // that the iterator is post-order: it will visit children of a node before
  // visiting the parent node.  So if I find that a child node is not empty, I
  // know that its parent is not empty without even checking.  So I put the
  // parent on a "skipList" which is just a voidArray of nodes I can skip the
  // empty check on.  If I encounter a node on the skiplist, i skip the
  // processing for that node and replace its slot in the skiplist with that
  // node's parent.
  //
  // An interesting idea is to go ahead and regard parent nodes that are NOT
  // on the skiplist as being empty (without even doing the IsEmptyNode check)
  // on the theory that if they weren't empty, we would have encountered a
  // non-empty child earlier and thus put this parent node on the skiplist.
  //
  // Unfortunately I can't use that strategy here, because the range may
  // include some children of a node while excluding others.  Thus I could
  // find all the _examined_ children empty, but still not have an empty
  // parent.

  PostContentIterator postOrderIter;
  nsresult rv = postOrderIter.Init(&aRange);
  if (NS_FAILED(rv)) {
    NS_WARNING("PostContentIterator::Init() failed");
    return rv;
  }

  AutoTArray<OwningNonNull<nsIContent>, 64> arrayOfEmptyContents,
      arrayOfEmptyCites, skipList;

  // Check for empty nodes
  for (; !postOrderIter.IsDone(); postOrderIter.Next()) {
    MOZ_ASSERT(postOrderIter.GetCurrentNode()->IsContent());

    nsIContent* content = postOrderIter.GetCurrentNode()->AsContent();
    nsIContent* parentContent = content->GetParent();

    size_t idx = skipList.IndexOf(content);
    if (idx != skipList.NoIndex) {
      // This node is on our skip list.  Skip processing for this node, and
      // replace its value in the skip list with the value of its parent
      if (parentContent) {
        skipList[idx] = parentContent;
      }
      continue;
    }

    bool isCandidate = false;
    bool isMailCite = false;
    if (content->IsElement()) {
      if (content->IsHTMLElement(nsGkAtoms::body)) {
        // Don't delete the body
      } else if ((isMailCite = HTMLEditUtils::IsMailCite(content)) ||
                 content->IsHTMLElement(nsGkAtoms::a) ||
                 HTMLEditUtils::IsInlineStyle(content) ||
                 HTMLEditUtils::IsAnyListElement(content) ||
                 content->IsHTMLElement(nsGkAtoms::div)) {
        // Only consider certain nodes to be empty for purposes of removal
        isCandidate = true;
      } else if (HTMLEditUtils::IsFormatNode(content) ||
                 HTMLEditUtils::IsListItem(content) ||
                 content->IsHTMLElement(nsGkAtoms::blockquote)) {
        // These node types are candidates if selection is not in them.  If
        // it is one of these, don't delete if selection inside.  This is so
        // we can create empty headings, etc., for the user to type into.
        AutoRangeArray selectionRanges(SelectionRef());
        isCandidate =
            !selectionRanges
                 .IsAtLeastOneContainerOfRangeBoundariesInclusiveDescendantOf(
                     *content);
      }
    }

    bool isEmptyNode = false;
    if (isCandidate) {
      // We delete mailcites even if they have a solo br in them.  Other
      // nodes we require to be empty.
      HTMLEditUtils::EmptyCheckOptions options{
          EmptyCheckOption::TreatListItemAsVisible,
          EmptyCheckOption::TreatTableCellAsVisible};
      if (!isMailCite) {
        options += EmptyCheckOption::TreatSingleBRElementAsVisible;
      }
      isEmptyNode = HTMLEditUtils::IsEmptyNode(*content, options);
      if (isEmptyNode) {
        if (isMailCite) {
          // mailcites go on a separate list from other empty nodes
          arrayOfEmptyCites.AppendElement(*content);
        } else {
          arrayOfEmptyContents.AppendElement(*content);
        }
      }
    }

    if (!isEmptyNode && parentContent) {
      // put parent on skip list
      skipList.AppendElement(*parentContent);
    }
  }

  // now delete the empty nodes
  for (OwningNonNull<nsIContent>& emptyContent : arrayOfEmptyContents) {
    // XXX Shouldn't we check whether it's removable from its parent??
    if (HTMLEditUtils::IsSimplyEditableNode(emptyContent)) {
      // MOZ_KnownLive because 'arrayOfEmptyContents' is guaranteed to keep it
      // alive.
      rv = DeleteNodeWithTransaction(MOZ_KnownLive(emptyContent));
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
        return rv;
      }
    }
  }

  // Now delete the empty mailcites.  This is a separate step because we want
  // to pull out any br's and preserve them.
  for (OwningNonNull<nsIContent>& emptyCite : arrayOfEmptyCites) {
    if (!HTMLEditUtils::IsEmptyNode(
            emptyCite, {EmptyCheckOption::TreatSingleBRElementAsVisible,
                        EmptyCheckOption::TreatListItemAsVisible,
                        EmptyCheckOption::TreatTableCellAsVisible})) {
      // We are deleting a cite that has just a `<br>`.  We want to delete cite,
      // but preserve `<br>`.
      Result<RefPtr<Element>, nsresult> resultOfInsertingBRElement =
          InsertBRElementWithTransaction(EditorDOMPoint(emptyCite));
      if (resultOfInsertingBRElement.isErr()) {
        NS_WARNING("HTMLEditor::InsertBRElementWithTransaction() failed");
        return resultOfInsertingBRElement.unwrapErr();
      }
      MOZ_ASSERT(resultOfInsertingBRElement.inspect());
    }
    // MOZ_KnownLive because 'arrayOfEmptyCites' is guaranteed to keep it alive.
    rv = DeleteNodeWithTransaction(MOZ_KnownLive(emptyCite));
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
      return rv;
    }
  }

  return NS_OK;
}

nsresult HTMLEditor::LiftUpListItemElement(
    Element& aListItemElement,
    LiftUpFromAllParentListElements aLiftUpFromAllParentListElements) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (!HTMLEditUtils::IsListItem(&aListItemElement)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (NS_WARN_IF(!aListItemElement.GetParentElement()) ||
      NS_WARN_IF(!aListItemElement.GetParentElement()->GetParentNode())) {
    return NS_ERROR_FAILURE;
  }

  // if it's first or last list item, don't need to split the list
  // otherwise we do.
  bool isFirstListItem = HTMLEditUtils::IsFirstChild(
      aListItemElement, {WalkTreeOption::IgnoreNonEditableNode});
  bool isLastListItem = HTMLEditUtils::IsLastChild(
      aListItemElement, {WalkTreeOption::IgnoreNonEditableNode});

  Element* leftListElement = aListItemElement.GetParentElement();
  if (NS_WARN_IF(!leftListElement)) {
    return NS_ERROR_FAILURE;
  }

  // If it's at middle of parent list element, split the parent list element.
  // Then, aListItem becomes the first list item of the right list element.
  if (!isFirstListItem && !isLastListItem) {
    EditorDOMPoint atListItemElement(&aListItemElement);
    if (NS_WARN_IF(!atListItemElement.IsSet())) {
      return NS_ERROR_FAILURE;
    }
    MOZ_ASSERT(atListItemElement.IsSetAndValid());
    SplitNodeResult splitListItemParentResult =
        SplitNodeWithTransaction(atListItemElement);
    if (MOZ_UNLIKELY(splitListItemParentResult.Failed())) {
      NS_WARNING("HTMLEditor::SplitNodeWithTransaction() failed");
      return splitListItemParentResult.Rv();
    }
    leftListElement =
        Element::FromNodeOrNull(splitListItemParentResult.GetPreviousContent());
    if (MOZ_UNLIKELY(!leftListElement)) {
      NS_WARNING(
          "HTMLEditor::SplitNodeWithTransaction() didn't return left list "
          "element");
      return NS_ERROR_FAILURE;
    }
  }

  // In most cases, insert the list item into the new left list node..
  EditorDOMPoint pointToInsertListItem(leftListElement);
  if (NS_WARN_IF(!pointToInsertListItem.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  // But when the list item was the first child of the right list, it should
  // be inserted between the both list elements.  This allows user to hit
  // Enter twice at a list item breaks the parent list node.
  if (!isFirstListItem) {
    DebugOnly<bool> advanced = pointToInsertListItem.AdvanceOffset();
    NS_WARNING_ASSERTION(advanced,
                         "Failed to advance offset to right list node");
  }

  nsresult rv =
      MoveNodeWithTransaction(aListItemElement, pointToInsertListItem);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::MoveNodeWithTransaction() failed");
    return rv;
  }

  // Unwrap list item contents if they are no longer in a list
  // XXX If the parent list element is a child of another list element
  //     (although invalid tree), the list item element won't be unwrapped.
  //     That makes the parent ancestor element tree valid, but might be
  //     unexpected result.
  // XXX If aListItemElement is <dl> or <dd> and current parent is <ul> or <ol>,
  //     the list items won't be unwrapped.  If aListItemElement is <li> and its
  //     current parent is <dl>, there is same issue.
  if (!HTMLEditUtils::IsAnyListElement(pointToInsertListItem.GetContainer()) &&
      HTMLEditUtils::IsListItem(&aListItemElement)) {
    nsresult rv = RemoveBlockContainerWithTransaction(aListItemElement);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::RemoveBlockContainerWithTransaction() failed");
    return rv;
  }
  if (aLiftUpFromAllParentListElements == LiftUpFromAllParentListElements::No) {
    return NS_OK;
  }
  // XXX If aListItemElement is moved to unexpected element by mutation event
  //     listener, shouldn't we stop calling this?
  rv = LiftUpListItemElement(aListItemElement,
                             LiftUpFromAllParentListElements::Yes);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::LiftUpListItemElement("
                       "LiftUpFromAllParentListElements::Yes) failed");
  return rv;
}

nsresult HTMLEditor::DestroyListStructureRecursively(Element& aListElement) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(HTMLEditUtils::IsAnyListElement(&aListElement));

  // XXX If mutation event listener inserts new child into `aListElement`,
  //     this becomes infinite loop so that we should set limit of the
  //     loop count from original child count.
  while (aListElement.GetFirstChild()) {
    OwningNonNull<nsIContent> child = *aListElement.GetFirstChild();

    if (HTMLEditUtils::IsListItem(child)) {
      // XXX Using LiftUpListItemElement() is too expensive for this purpose.
      //     Looks like the reason why this method uses it is, only this loop
      //     wants to work with first child of aListElement.  However, what it
      //     actually does is removing <li> as container.  Perhaps, we should
      //     decide destination first, and then, move contents in `child`.
      // XXX If aListElement is is a child of another list element (although
      //     it's invalid tree), this moves the list item to outside of
      //     aListElement's parent.  Is that really intentional behavior?
      nsresult rv = LiftUpListItemElement(
          MOZ_KnownLive(*child->AsElement()),
          HTMLEditor::LiftUpFromAllParentListElements::Yes);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "HTMLEditor::LiftUpListItemElement(LiftUpFromAllParentListElements:"
            ":Yes) failed");
        return rv;
      }
      continue;
    }

    if (HTMLEditUtils::IsAnyListElement(child)) {
      nsresult rv =
          DestroyListStructureRecursively(MOZ_KnownLive(*child->AsElement()));
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::DestroyListStructureRecursively() failed");
        return rv;
      }
      continue;
    }

    // Delete any non-list items for now
    // XXX This is not HTML5 aware.  HTML5 allows all list elements to have
    //     <script> and <template> and <dl> element to have <div> to group
    //     some <dt> and <dd> elements.  So, this may break valid children.
    nsresult rv = DeleteNodeWithTransaction(*child);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
      return rv;
    }
  }

  // Delete the now-empty list
  nsresult rv = RemoveBlockContainerWithTransaction(aListElement);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::RemoveBlockContainerWithTransaction() failed");
  return rv;
}

nsresult HTMLEditor::EnsureSelectionInBodyOrDocumentElement() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  RefPtr<Element> bodyOrDocumentElement = GetRoot();
  if (NS_WARN_IF(!bodyOrDocumentElement)) {
    return NS_ERROR_FAILURE;
  }

  EditorRawDOMPoint atCaret(EditorBase::GetStartPoint(SelectionRef()));
  if (NS_WARN_IF(!atCaret.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  // XXX This does wrong things.  Web apps can put any elements as sibling
  //     of `<body>` element.  Therefore, this collapses `Selection` into
  //     the `<body>` element which `HTMLDocument.body` is set to.  So,
  //     this makes users impossible to modify content outside of the
  //     `<body>` element even if caret is in an editing host.

  // Check that selection start container is inside the <body> element.
  // XXXsmaug this code is insane.
  nsINode* temp = atCaret.GetContainer();
  while (temp && !temp->IsHTMLElement(nsGkAtoms::body)) {
    temp = temp->GetParentOrShadowHostNode();
  }

  // If we aren't in the <body> element, force the issue.
  if (!temp) {
    nsresult rv = CollapseSelectionToStartOf(*bodyOrDocumentElement);
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::CollapseSelectionToStartOf() failed, but ignored");
    return NS_OK;
  }

  EditorRawDOMPoint selectionEndPoint(EditorBase::GetEndPoint(SelectionRef()));
  if (NS_WARN_IF(!selectionEndPoint.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  // check that selNode is inside body
  // XXXsmaug this code is insane.
  temp = selectionEndPoint.GetContainer();
  while (temp && !temp->IsHTMLElement(nsGkAtoms::body)) {
    temp = temp->GetParentOrShadowHostNode();
  }

  // If we aren't in the <body> element, force the issue.
  if (!temp) {
    nsresult rv = CollapseSelectionToStartOf(*bodyOrDocumentElement);
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::CollapseSelectionToStartOf() failed, but ignored");
  }

  return NS_OK;
}

nsresult HTMLEditor::InsertPaddingBRElementForEmptyLastLineIfNeeded(
    Element& aElement) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (!HTMLEditUtils::IsBlockElement(aElement)) {
    return NS_OK;
  }

  if (!HTMLEditUtils::IsEmptyNode(
          aElement, {EmptyCheckOption::TreatSingleBRElementAsVisible})) {
    return NS_OK;
  }

  CreateElementResult createBRResult =
      InsertPaddingBRElementForEmptyLastLineWithTransaction(
          EditorDOMPoint(&aElement, 0));
  NS_WARNING_ASSERTION(
      createBRResult.Succeeded(),
      "HTMLEditor::InsertPaddingBRElementForEmptyLastLineWithTransaction() "
      "failed");
  return createBRResult.Rv();
}

nsresult HTMLEditor::InsertBRElementIfEmptyBlockElement(Element& aElement) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (!HTMLEditUtils::IsBlockElement(aElement)) {
    return NS_OK;
  }

  if (!HTMLEditUtils::IsEmptyNode(
          aElement, {EmptyCheckOption::TreatSingleBRElementAsVisible})) {
    return NS_OK;
  }

  Result<RefPtr<Element>, nsresult> resultOfInsertingBRElement =
      InsertBRElementWithTransaction(EditorDOMPoint(&aElement, 0));
  if (resultOfInsertingBRElement.isErr()) {
    NS_WARNING("HTMLEditor::InsertBRElementWithTransaction() failed");
    return resultOfInsertingBRElement.unwrapErr();
  }
  MOZ_ASSERT(resultOfInsertingBRElement.inspect());
  return NS_OK;
}

nsresult HTMLEditor::RemoveAlignFromDescendants(Element& aElement,
                                                const nsAString& aAlignType,
                                                EditTarget aEditTarget) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!aElement.IsHTMLElement(nsGkAtoms::table));

  bool useCSS = IsCSSEnabled();

  // Let's remove all alignment hints in the children of aNode; it can
  // be an ALIGN attribute (in case we just remove it) or a CENTER
  // element (here we have to remove the container and keep its
  // children). We break on tables and don't look at their children.
  nsCOMPtr<nsIContent> nextSibling;
  for (nsIContent* content =
           aEditTarget == EditTarget::NodeAndDescendantsExceptTable
               ? &aElement
               : aElement.GetFirstChild();
       content; content = nextSibling) {
    // Get the next sibling before removing content from the DOM tree.
    // XXX If next sibling is removed from the parent and/or inserted to
    //     different parent, we will behave unexpectedly.  I think that
    //     we should create child list and handle it with checking whether
    //     it's still a child of expected parent.
    nextSibling = aEditTarget == EditTarget::NodeAndDescendantsExceptTable
                      ? nullptr
                      : content->GetNextSibling();

    if (content->IsHTMLElement(nsGkAtoms::center)) {
      OwningNonNull<Element> centerElement = *content->AsElement();
      nsresult rv = RemoveAlignFromDescendants(
          centerElement, aAlignType, EditTarget::OnlyDescendantsExceptTable);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "HTMLEditor::RemoveAlignFromDescendants(EditTarget::"
            "OnlyDescendantsExceptTable) failed");
        return rv;
      }

      // We may have to insert a `<br>` element before first child of the
      // `<center>` element because it should be first element of a hard line
      // even after removing the `<center>` element.
      rv = EnsureHardLineBeginsWithFirstChildOf(centerElement);
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::EnsureHardLineBeginsWithFirstChildOf() failed");
        return rv;
      }

      // We may have to insert a `<br>` element after last child of the
      // `<center>` element because it should be last element of a hard line
      // even after removing the `<center>` element.
      rv = EnsureHardLineEndsWithLastChildOf(centerElement);
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::EnsureHardLineEndsWithLastChildOf() failed");
        return rv;
      }

      rv = RemoveContainerWithTransaction(centerElement);
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::RemoveContainerWithTransaction() failed");
        return rv;
      }
      continue;
    }

    if (!HTMLEditUtils::IsBlockElement(*content) &&
        !content->IsHTMLElement(nsGkAtoms::hr)) {
      continue;
    }

    OwningNonNull<Element> blockOrHRElement = *content->AsElement();
    if (HTMLEditUtils::SupportsAlignAttr(blockOrHRElement)) {
      nsresult rv =
          RemoveAttributeWithTransaction(blockOrHRElement, *nsGkAtoms::align);
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "EditorBase::RemoveAttributeWithTransaction(nsGkAtoms::align) "
            "failed");
        return rv;
      }
    }
    if (useCSS) {
      if (blockOrHRElement->IsAnyOfHTMLElements(nsGkAtoms::table,
                                                nsGkAtoms::hr)) {
        nsresult rv = SetAttributeOrEquivalent(
            blockOrHRElement, nsGkAtoms::align, aAlignType, false);
        if (NS_WARN_IF(Destroyed())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "EditorBase::SetAttributeOrEquivalent(nsGkAtoms::align) failed");
          return rv;
        }
      } else {
        nsStyledElement* styledBlockOrHRElement =
            nsStyledElement::FromNode(blockOrHRElement);
        if (NS_WARN_IF(!styledBlockOrHRElement)) {
          return NS_ERROR_FAILURE;
        }
        // MOZ_KnownLive(*styledBlockOrHRElement): It's `blockOrHRElement
        // which is OwningNonNull.
        nsAutoString dummyCssValue;
        nsresult rv = mCSSEditUtils->RemoveCSSInlineStyleWithTransaction(
            MOZ_KnownLive(*styledBlockOrHRElement), nsGkAtoms::textAlign,
            dummyCssValue);
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "CSSEditUtils::RemoveCSSInlineStyleWithTransaction(nsGkAtoms::"
              "textAlign) failed");
          return rv;
        }
      }
    }
    if (!blockOrHRElement->IsHTMLElement(nsGkAtoms::table)) {
      // unless this is a table, look at children
      nsresult rv = RemoveAlignFromDescendants(
          blockOrHRElement, aAlignType, EditTarget::OnlyDescendantsExceptTable);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "HTMLEditor::RemoveAlignFromDescendants(EditTarget::"
            "OnlyDescendantsExceptTable) failed");
        return rv;
      }
    }
  }
  return NS_OK;
}

nsresult HTMLEditor::EnsureHardLineBeginsWithFirstChildOf(
    Element& aRemovingContainerElement) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  nsIContent* firstEditableChild = HTMLEditUtils::GetFirstChild(
      aRemovingContainerElement, {WalkTreeOption::IgnoreNonEditableNode});
  if (!firstEditableChild) {
    return NS_OK;
  }

  if (HTMLEditUtils::IsBlockElement(*firstEditableChild) ||
      firstEditableChild->IsHTMLElement(nsGkAtoms::br)) {
    return NS_OK;
  }

  nsIContent* previousEditableContent = HTMLEditUtils::GetPreviousSibling(
      aRemovingContainerElement, {WalkTreeOption::IgnoreNonEditableNode});
  if (!previousEditableContent) {
    return NS_OK;
  }

  if (HTMLEditUtils::IsBlockElement(*previousEditableContent) ||
      previousEditableContent->IsHTMLElement(nsGkAtoms::br)) {
    return NS_OK;
  }

  Result<RefPtr<Element>, nsresult> resultOfInsertingBRElement =
      InsertBRElementWithTransaction(
          EditorDOMPoint(&aRemovingContainerElement, 0));
  if (resultOfInsertingBRElement.isErr()) {
    NS_WARNING("HTMLEditor::InsertBRElementWithTransaction() failed");
    return resultOfInsertingBRElement.unwrapErr();
  }
  MOZ_ASSERT(resultOfInsertingBRElement.inspect());
  return NS_OK;
}

nsresult HTMLEditor::EnsureHardLineEndsWithLastChildOf(
    Element& aRemovingContainerElement) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  nsIContent* firstEditableContent = HTMLEditUtils::GetLastChild(
      aRemovingContainerElement, {WalkTreeOption::IgnoreNonEditableNode});
  if (!firstEditableContent) {
    return NS_OK;
  }

  if (HTMLEditUtils::IsBlockElement(*firstEditableContent) ||
      firstEditableContent->IsHTMLElement(nsGkAtoms::br)) {
    return NS_OK;
  }

  nsIContent* nextEditableContent = HTMLEditUtils::GetPreviousSibling(
      aRemovingContainerElement, {WalkTreeOption::IgnoreNonEditableNode});
  if (!nextEditableContent) {
    return NS_OK;
  }

  if (HTMLEditUtils::IsBlockElement(*nextEditableContent) ||
      nextEditableContent->IsHTMLElement(nsGkAtoms::br)) {
    return NS_OK;
  }

  Result<RefPtr<Element>, nsresult> resultOfInsertingBRElement =
      InsertBRElementWithTransaction(
          EditorDOMPoint::AtEndOf(aRemovingContainerElement));
  if (resultOfInsertingBRElement.isErr()) {
    NS_WARNING("HTMLEditor::InsertBRElementWithTransaction() failed");
    return resultOfInsertingBRElement.unwrapErr();
  }
  MOZ_ASSERT(resultOfInsertingBRElement.inspect());
  return NS_OK;
}

nsresult HTMLEditor::SetBlockElementAlign(Element& aBlockOrHRElement,
                                          const nsAString& aAlignType,
                                          EditTarget aEditTarget) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(HTMLEditUtils::IsBlockElement(aBlockOrHRElement) ||
             aBlockOrHRElement.IsHTMLElement(nsGkAtoms::hr));
  MOZ_ASSERT(IsCSSEnabled() ||
             HTMLEditUtils::SupportsAlignAttr(aBlockOrHRElement));

  if (!aBlockOrHRElement.IsHTMLElement(nsGkAtoms::table)) {
    nsresult rv =
        RemoveAlignFromDescendants(aBlockOrHRElement, aAlignType, aEditTarget);
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::RemoveAlignFromDescendants() failed");
      return rv;
    }
  }
  nsresult rv = SetAttributeOrEquivalent(&aBlockOrHRElement, nsGkAtoms::align,
                                         aAlignType, false);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::SetAttributeOrEquivalent(nsGkAtoms::align) failed");
  return rv;
}

nsresult HTMLEditor::ChangeMarginStart(Element& aElement,
                                       ChangeMargin aChangeMargin) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  nsStaticAtom& marginProperty = MarginPropertyAtomForIndent(aElement);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  nsAutoString value;
  DebugOnly<nsresult> rvIgnored =
      CSSEditUtils::GetSpecifiedProperty(aElement, marginProperty, value);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "CSSEditUtils::GetSpecifiedProperty() failed, but ignored");
  float f;
  RefPtr<nsAtom> unit;
  CSSEditUtils::ParseLength(value, &f, getter_AddRefs(unit));
  if (!f) {
    nsAutoString defaultLengthUnit;
    CSSEditUtils::GetDefaultLengthUnit(defaultLengthUnit);
    unit = NS_Atomize(defaultLengthUnit);
  }
  int8_t multiplier = aChangeMargin == ChangeMargin::Increase ? 1 : -1;
  if (nsGkAtoms::in == unit) {
    f += NS_EDITOR_INDENT_INCREMENT_IN * multiplier;
  } else if (nsGkAtoms::cm == unit) {
    f += NS_EDITOR_INDENT_INCREMENT_CM * multiplier;
  } else if (nsGkAtoms::mm == unit) {
    f += NS_EDITOR_INDENT_INCREMENT_MM * multiplier;
  } else if (nsGkAtoms::pt == unit) {
    f += NS_EDITOR_INDENT_INCREMENT_PT * multiplier;
  } else if (nsGkAtoms::pc == unit) {
    f += NS_EDITOR_INDENT_INCREMENT_PC * multiplier;
  } else if (nsGkAtoms::em == unit) {
    f += NS_EDITOR_INDENT_INCREMENT_EM * multiplier;
  } else if (nsGkAtoms::ex == unit) {
    f += NS_EDITOR_INDENT_INCREMENT_EX * multiplier;
  } else if (nsGkAtoms::px == unit) {
    f += NS_EDITOR_INDENT_INCREMENT_PX * multiplier;
  } else if (nsGkAtoms::percentage == unit) {
    f += NS_EDITOR_INDENT_INCREMENT_PERCENT * multiplier;
  }

  if (0 < f) {
    if (nsStyledElement* styledElement = nsStyledElement::FromNode(&aElement)) {
      nsAutoString newValue;
      newValue.AppendFloat(f);
      newValue.Append(nsDependentAtomString(unit));
      // MOZ_KnownLive(*styledElement): It's aElement and its lifetime must
      // be guaranteed by caller because of MOZ_CAN_RUN_SCRIPT method.
      // MOZ_KnownLive(merginProperty): It's nsStaticAtom.
      nsresult rv = mCSSEditUtils->SetCSSPropertyWithTransaction(
          MOZ_KnownLive(*styledElement), MOZ_KnownLive(marginProperty),
          newValue);
      if (rv == NS_ERROR_EDITOR_DESTROYED) {
        NS_WARNING(
            "CSSEditUtils::SetCSSPropertyWithTransaction() destroyed the "
            "editor");
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "CSSEditUtils::SetCSSPropertyWithTransaction() failed, but ignored");
    }
    return NS_OK;
  }

  if (nsStyledElement* styledElement = nsStyledElement::FromNode(&aElement)) {
    // MOZ_KnownLive(*styledElement): It's aElement and its lifetime must
    // be guaranteed by caller because of MOZ_CAN_RUN_SCRIPT method.
    // MOZ_KnownLive(merginProperty): It's nsStaticAtom.
    nsresult rv = mCSSEditUtils->RemoveCSSPropertyWithTransaction(
        MOZ_KnownLive(*styledElement), MOZ_KnownLive(marginProperty), value);
    if (rv == NS_ERROR_EDITOR_DESTROYED) {
      NS_WARNING(
          "CSSEditUtils::RemoveCSSPropertyWithTransaction() destroyed the "
          "editor");
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "CSSEditUtils::RemoveCSSPropertyWithTransaction() failed, but ignored");
  }

  // Remove unnecessary divs
  if (!aElement.IsHTMLElement(nsGkAtoms::div) ||
      HTMLEditor::HasAttributes(&aElement)) {
    return NS_OK;
  }
  // Don't touch editiong host nor node which is outside of it.
  Element* editingHost = GetActiveEditingHost();
  if (&aElement == editingHost ||
      !aElement.IsInclusiveDescendantOf(editingHost)) {
    return NS_OK;
  }

  nsresult rv = RemoveContainerWithTransaction(aElement);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::RemoveContainerWithTransaction() failed");
  return rv;
}

EditActionResult HTMLEditor::SetSelectionToAbsoluteAsSubAction() {
  MOZ_ASSERT(IsTopLevelEditSubActionDataAvailable());

  AutoPlaceholderBatch treatAsOneTransaction(*this,
                                             ScrollSelectionIntoView::Yes);
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eSetPositionToAbsolute, nsIEditor::eNext,
      ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return EditActionResult(ignoredError.StealNSResult());
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  EditActionResult result = CanHandleHTMLEditSubAction();
  if (result.Failed() || result.Canceled()) {
    NS_WARNING_ASSERTION(result.Succeeded(),
                         "HTMLEditor::CanHandleHTMLEditSubAction() failed");
    return result;
  }

  nsresult rv = EnsureNoPaddingBRElementForEmptyEditor();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::EnsureNoPaddingBRElementForEmptyEditor() "
                       "failed, but ignored");

  if (NS_SUCCEEDED(rv) && SelectionRef().IsCollapsed()) {
    nsresult rv = EnsureCaretNotAfterInvisibleBRElement();
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::EnsureCaretNotAfterInvisibleBRElement() "
                         "failed, but ignored");
    if (NS_SUCCEEDED(rv)) {
      nsresult rv = PrepareInlineStylesForCaret();
      if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
        return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "HTMLEditgor::PrepareInlineStylesForCaret() failed, but ignored");
    }
  }

  RefPtr<Element> focusElement = GetSelectionContainerElement();
  if (focusElement && HTMLEditUtils::IsImage(focusElement)) {
    TopLevelEditSubActionDataRef().mNewBlockElement = std::move(focusElement);
    return EditActionHandled();
  }

  if (!SelectionRef().IsCollapsed()) {
    nsresult rv = MaybeExtendSelectionToHardLineEdgesForBlockEditAction();
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "HTMLEditor::MaybeExtendSelectionToHardLineEdgesForBlockEditAction() "
          "failed");
      return EditActionHandled(rv);
    }
  }

  // XXX Is this right thing?
  TopLevelEditSubActionDataRef().mNewBlockElement = nullptr;

  RefPtr<Element> divElement;
  rv = MoveSelectedContentsToDivElementToMakeItAbsolutePosition(
      address_of(divElement));
  // MoveSelectedContentsToDivElementToMakeItAbsolutePosition() may restore
  // selection with AutoSelectionRestorer.  Therefore, the editor might have
  // already been destroyed now.
  if (NS_WARN_IF(Destroyed())) {
    return EditActionHandled(NS_ERROR_EDITOR_DESTROYED);
  }
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "HTMLEditor::MoveSelectedContentsToDivElementToMakeItAbsolutePosition()"
        " failed");
    return EditActionHandled(rv);
  }

  if (IsSelectionRangeContainerNotContent()) {
    NS_WARNING("Mutation event listener might have changed the selection");
    return EditActionHandled(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }

  rv = MaybeInsertPaddingBRElementForEmptyLastLineAtSelection();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditActionHandled(rv);
  }

  if (!divElement) {
    return EditActionHandled();
  }

  rv = SetPositionToAbsoluteOrStatic(*divElement, true);
  if (NS_WARN_IF(Destroyed())) {
    return EditActionHandled(NS_ERROR_EDITOR_DESTROYED);
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::SetPositionToAbsoluteOrStatic() failed");

  TopLevelEditSubActionDataRef().mNewBlockElement = std::move(divElement);
  return EditActionHandled(rv);
}

nsresult HTMLEditor::MoveSelectedContentsToDivElementToMakeItAbsolutePosition(
    RefPtr<Element>* aTargetElement) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(aTargetElement);

  AutoSelectionRestorer restoreSelectionLater(*this);

  AutoTArray<RefPtr<nsRange>, 4> arrayOfRanges;
  GetSelectionRangesExtendedToHardLineStartAndEnd(
      arrayOfRanges, EditSubAction::eSetPositionToAbsolute);

  // Use these ranges to construct a list of nodes to act on.
  AutoTArray<OwningNonNull<nsIContent>, 64> arrayOfContents;
  nsresult rv = SplitInlinesAndCollectEditTargetNodes(
      arrayOfRanges, arrayOfContents, EditSubAction::eSetPositionToAbsolute,
      CollectNonEditableNodes::Yes);
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "HTMLEditor::SplitInlinesAndCollectEditTargetNodes("
        "eSetPositionToAbsolute, CollectNonEditableNodes::Yes) failed");
    return rv;
  }

  // If there is no visible and editable nodes in the edit targets, make an
  // empty block.
  // XXX Isn't this odd if there are only non-editable visible nodes?
  if (HTMLEditUtils::IsEmptyOneHardLine(arrayOfContents)) {
    const nsRange* firstRange = SelectionRef().GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return NS_ERROR_FAILURE;
    }

    EditorDOMPoint atCaret(firstRange->StartRef());
    if (NS_WARN_IF(!atCaret.IsSet())) {
      return NS_ERROR_FAILURE;
    }

    // Make sure we can put a block here.
    Result<RefPtr<Element>, nsresult> newDivElementOrError =
        InsertElementWithSplittingAncestorsWithTransaction(
            *nsGkAtoms::div, atCaret, BRElementNextToSplitPoint::Keep);
    if (MOZ_UNLIKELY(newDivElementOrError.isErr())) {
      NS_WARNING(
          "HTMLEditor::InsertElementWithSplittingAncestorsWithTransaction("
          "nsGkAtoms::div) failed");
      return newDivElementOrError.unwrapErr();
    }
    MOZ_ASSERT(newDivElementOrError.inspect());
    // Delete anything that was in the list of nodes
    // XXX We don't need to remove items from the array.
    while (!arrayOfContents.IsEmpty()) {
      OwningNonNull<nsIContent>& curNode = arrayOfContents[0];
      // MOZ_KnownLive because 'arrayOfContents' is guaranteed to keep it alive.
      nsresult rv = DeleteNodeWithTransaction(MOZ_KnownLive(*curNode));
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
        return rv;
      }
      arrayOfContents.RemoveElementAt(0);
    }
    // Don't restore the selection
    restoreSelectionLater.Abort();
    nsresult rv = CollapseSelectionToStartOf(
        MOZ_KnownLive(*newDivElementOrError.inspect()));
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::CollapseSelectionToStartOf() failed");
    *aTargetElement = newDivElementOrError.unwrap();
    return rv;
  }

  RefPtr<Element> editingHost = GetActiveEditingHost();
  if (NS_WARN_IF(!editingHost)) {
    return NS_ERROR_FAILURE;
  }

  // `<div>` element to be positioned absolutely.  This may have already
  // existed or newly created by this method.
  RefPtr<Element> targetDivElement;
  // Newly created list element for moving selected list item elements into
  // targetDivElement.  I.e., this is created in the `<div>` element.
  RefPtr<Element> createdListElement;
  // If we handle a parent list item element, this is set to it.  In such case,
  // we should handle its children again.
  RefPtr<Element> handledListItemElement;
  for (OwningNonNull<nsIContent>& content : arrayOfContents) {
    // Here's where we actually figure out what to do.
    EditorDOMPoint atContent(content);
    if (NS_WARN_IF(!atContent.IsSet())) {
      return NS_ERROR_FAILURE;  // XXX not continue??
    }

    // Ignore all non-editable nodes.  Leave them be.
    if (!EditorUtils::IsEditableContent(content, EditorType::HTML)) {
      continue;
    }

    // If current node is a child of a list element, we need another list
    // element in absolute-positioned `<div>` element to avoid non-selected
    // list items are moved into the `<div>` element.
    if (HTMLEditUtils::IsAnyListElement(atContent.GetContainer())) {
      // If we cannot move current node to created list element, we need a
      // list element in the target `<div>` element for the destination.
      // Therefore, duplicate same list element into the target `<div>`
      // element.
      nsIContent* previousEditableContent =
          createdListElement
              ? HTMLEditUtils::GetPreviousSibling(
                    content, {WalkTreeOption::IgnoreNonEditableNode})
              : nullptr;
      if (!createdListElement ||
          (previousEditableContent &&
           previousEditableContent != createdListElement)) {
        nsAtom* ULOrOLOrDLTagName =
            atContent.GetContainer()->NodeInfo()->NameAtom();
        if (targetDivElement) {
          // XXX Do we need to split the container? Since we'll append new
          //     element at end of the <div> element.
          SplitNodeResult splitNodeResult =
              MaybeSplitAncestorsForInsertWithTransaction(
                  MOZ_KnownLive(*ULOrOLOrDLTagName), atContent);
          if (NS_WARN_IF(splitNodeResult.Failed())) {
            return splitNodeResult.Rv();
          }
        } else {
          // If we've not had a target <div> element yet, let's insert a <div>
          // element with splitting the ancestors.
          Result<RefPtr<Element>, nsresult> newDivElementOrError =
              InsertElementWithSplittingAncestorsWithTransaction(
                  *nsGkAtoms::div, atContent, BRElementNextToSplitPoint::Keep);
          if (MOZ_UNLIKELY(newDivElementOrError.isErr())) {
            NS_WARNING(
                "HTMLEditor::"
                "InsertElementWithSplittingAncestorsWithTransaction(nsGkAtoms::"
                "div) failed");
            return newDivElementOrError.unwrapErr();
          }
          MOZ_ASSERT(newDivElementOrError.inspect());
          targetDivElement = newDivElementOrError.unwrap();
        }
        Result<RefPtr<Element>, nsresult> maybeNewListElement =
            CreateAndInsertElementWithTransaction(
                MOZ_KnownLive(*ULOrOLOrDLTagName),
                EditorDOMPoint::AtEndOf(targetDivElement));
        if (maybeNewListElement.isErr()) {
          NS_WARNING(
              "HTMLEditor::CreateAndInsertElementWithTransaction() failed");
          return maybeNewListElement.unwrapErr();
        }
        MOZ_ASSERT(maybeNewListElement.inspect());
        createdListElement = maybeNewListElement.unwrap();
      }
      // Move current node (maybe, assumed as a list item element) into the
      // new list element in the target `<div>` element to be positioned
      // absolutely.
      // MOZ_KnownLive because 'arrayOfContents' is guaranteed to keep it alive.
      nsresult rv = MoveNodeToEndWithTransaction(MOZ_KnownLive(content),
                                                 *createdListElement);
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      continue;
    }

    // If contents in a list item element is selected, we should move current
    // node into the target `<div>` element with the list item element itself
    // because we want to keep indent level of the contents.
    if (RefPtr<Element> listItemElement =
            HTMLEditUtils::GetClosestAncestorListItemElement(content,
                                                             editingHost)) {
      if (handledListItemElement == listItemElement) {
        // Current node has already been moved into the `<div>` element.
        continue;
      }
      // If we cannot move the list item element into created list element,
      // we need another list element in the target `<div>` element.
      nsIContent* previousEditableContent =
          createdListElement
              ? HTMLEditUtils::GetPreviousSibling(
                    *listItemElement, {WalkTreeOption::IgnoreNonEditableNode})
              : nullptr;
      if (!createdListElement ||
          (previousEditableContent &&
           previousEditableContent != createdListElement)) {
        EditorDOMPoint atListItem(listItemElement);
        if (NS_WARN_IF(!atListItem.IsSet())) {
          return NS_ERROR_FAILURE;
        }
        // XXX If content is the listItemElement and not in a list element,
        //     we duplicate wrong element into the target `<div>` element.
        nsAtom* containerName =
            atListItem.GetContainer()->NodeInfo()->NameAtom();
        if (targetDivElement) {
          // XXX Do we need to split the container? Since we'll append new
          //     element at end of the <div> element.
          SplitNodeResult splitNodeResult =
              MaybeSplitAncestorsForInsertWithTransaction(
                  MOZ_KnownLive(*containerName), atListItem);
          if (NS_WARN_IF(splitNodeResult.Failed())) {
            return splitNodeResult.Rv();
          }
        } else {
          // If we've not had a target <div> element yet, let's insert a <div>
          // element with splitting the ancestors.
          Result<RefPtr<Element>, nsresult> newDivElementOrError =
              InsertElementWithSplittingAncestorsWithTransaction(
                  *nsGkAtoms::div, atContent, BRElementNextToSplitPoint::Keep);
          if (MOZ_UNLIKELY(newDivElementOrError.isErr())) {
            NS_WARNING(
                "HTMLEditor::"
                "InsertElementWithSplittingAncestorsWithTransaction("
                "nsGkAtoms::div) failed");
            return newDivElementOrError.unwrapErr();
          }
          MOZ_ASSERT(newDivElementOrError.inspect());
          targetDivElement = newDivElementOrError.unwrap();
        }
        // XXX So, createdListElement may be set to a non-list element.
        Result<RefPtr<Element>, nsresult> maybeNewListElement =
            CreateAndInsertElementWithTransaction(
                MOZ_KnownLive(*containerName),
                EditorDOMPoint::AtEndOf(targetDivElement));
        if (maybeNewListElement.isErr()) {
          NS_WARNING(
              "HTMLEditor::CreateAndInsertElementWithTransaction() failed");
          return maybeNewListElement.unwrapErr();
        }
        MOZ_ASSERT(maybeNewListElement.inspect());
        createdListElement = maybeNewListElement.unwrap();
      }
      // Move current list item element into the createdListElement (could be
      // non-list element due to the above bug) in a candidate `<div>` element
      // to be positioned absolutely.
      nsresult rv =
          MoveNodeToEndWithTransaction(*listItemElement, *createdListElement);
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::MoveNodeToEndWithTransaction() failed");
        return rv;
      }
      handledListItemElement = std::move(listItemElement);
      continue;
    }

    if (!targetDivElement) {
      // If we meet a `<div>` element, use it as the absolute-position
      // container.
      // XXX This looks odd.  If there are 2 or more `<div>` elements are
      //     selected, first found `<div>` element will have all other
      //     selected nodes.
      if (content->IsHTMLElement(nsGkAtoms::div)) {
        targetDivElement = content->AsElement();
        MOZ_ASSERT(!createdListElement);
        MOZ_ASSERT(!handledListItemElement);
        continue;
      }
      // Otherwise, create new `<div>` element to be positioned absolutely
      // and to contain all selected nodes.
      Result<RefPtr<Element>, nsresult> newDivElementOrError =
          InsertElementWithSplittingAncestorsWithTransaction(
              *nsGkAtoms::div, atContent, BRElementNextToSplitPoint::Keep);
      if (newDivElementOrError.isErr()) {
        NS_WARNING(
            "HTMLEditor::InsertElementWithSplittingAncestorsWithTransaction("
            "nsGkAtoms::div) failed");
        return newDivElementOrError.unwrapErr();
      }
      MOZ_ASSERT(newDivElementOrError.inspect());
      targetDivElement = newDivElementOrError.unwrap();
    }

    // MOZ_KnownLive because 'arrayOfContents' is guaranteed to keep it alive.
    nsresult rv =
        MoveNodeToEndWithTransaction(MOZ_KnownLive(content), *targetDivElement);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::MoveNodeToEndWithTransaction() failed");
      return rv;
    }
    // Forget createdListElement, if any
    createdListElement = nullptr;
  }
  *aTargetElement = std::move(targetDivElement);
  return NS_OK;
}

EditActionResult HTMLEditor::SetSelectionToStaticAsSubAction() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  AutoPlaceholderBatch treatAsOneTransaction(*this,
                                             ScrollSelectionIntoView::Yes);
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eSetPositionToStatic, nsIEditor::eNext,
      ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return EditActionResult(ignoredError.StealNSResult());
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  EditActionResult result = CanHandleHTMLEditSubAction();
  if (result.Failed() || result.Canceled()) {
    NS_WARNING_ASSERTION(result.Succeeded(),
                         "HTMLEditor::CanHandleHTMLEditSubAction() failed");
    return result;
  }

  nsresult rv = EnsureNoPaddingBRElementForEmptyEditor();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::EnsureNoPaddingBRElementForEmptyEditor() "
                       "failed, but ignored");

  if (NS_SUCCEEDED(rv) && SelectionRef().IsCollapsed()) {
    nsresult rv = EnsureCaretNotAfterInvisibleBRElement();
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::EnsureCaretNotAfterInvisibleBRElement() "
                         "failed, but ignored");
    if (NS_SUCCEEDED(rv)) {
      nsresult rv = PrepareInlineStylesForCaret();
      if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
        return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "HTMLEditor::PrepareInlineStylesForCaret() failed, but ignored");
    }
  }

  RefPtr<Element> element = GetAbsolutelyPositionedSelectionContainer();
  if (!element) {
    if (NS_WARN_IF(Destroyed())) {
      return EditActionHandled(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING(
        "HTMLEditor::GetAbsolutelyPositionedSelectionContainer() returned "
        "nullptr");
    return EditActionHandled(NS_ERROR_FAILURE);
  }

  {
    AutoSelectionRestorer restoreSelectionLater(*this);

    nsresult rv = SetPositionToAbsoluteOrStatic(*element, false);
    if (NS_WARN_IF(Destroyed())) {
      return EditActionHandled(NS_ERROR_EDITOR_DESTROYED);
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::SetPositionToAbsoluteOrStatic() failed");
      return EditActionHandled(rv);
    }
  }

  // Restoring Selection might cause destroying the HTML editor.
  return NS_WARN_IF(Destroyed()) ? EditActionHandled(NS_ERROR_EDITOR_DESTROYED)
                                 : EditActionHandled(NS_OK);
}

EditActionResult HTMLEditor::AddZIndexAsSubAction(int32_t aChange) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  AutoPlaceholderBatch treatAsOneTransaction(*this,
                                             ScrollSelectionIntoView::Yes);
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this,
      aChange < 0 ? EditSubAction::eDecreaseZIndex
                  : EditSubAction::eIncreaseZIndex,
      nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return EditActionResult(ignoredError.StealNSResult());
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  EditActionResult result = CanHandleHTMLEditSubAction();
  if (result.Failed() || result.Canceled()) {
    NS_WARNING_ASSERTION(result.Succeeded(),
                         "HTMLEditor::CanHandleHTMLEditSubAction() failed");
    return result;
  }

  nsresult rv = EnsureNoPaddingBRElementForEmptyEditor();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return EditActionHandled(NS_ERROR_EDITOR_DESTROYED);
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::EnsureNoPaddingBRElementForEmptyEditor() "
                       "failed, but ignored");

  if (NS_SUCCEEDED(rv) && SelectionRef().IsCollapsed()) {
    nsresult rv = EnsureCaretNotAfterInvisibleBRElement();
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return EditActionHandled(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::EnsureCaretNotAfterInvisibleBRElement() "
                         "failed, but ignored");
    if (NS_SUCCEEDED(rv)) {
      nsresult rv = PrepareInlineStylesForCaret();
      if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
        return EditActionHandled(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "HTMLEditor::PrepareInlineStylesForCaret() failed, but ignored");
    }
  }

  RefPtr<Element> absolutelyPositionedElement =
      GetAbsolutelyPositionedSelectionContainer();
  if (!absolutelyPositionedElement) {
    if (NS_WARN_IF(Destroyed())) {
      return EditActionHandled(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING(
        "HTMLEditor::GetAbsolutelyPositionedSelectionContainer() returned "
        "nullptr");
    return EditActionHandled(NS_ERROR_FAILURE);
  }

  nsStyledElement* absolutelyPositionedStyledElement =
      nsStyledElement::FromNode(absolutelyPositionedElement);
  if (NS_WARN_IF(!absolutelyPositionedStyledElement)) {
    return EditActionHandled(NS_ERROR_FAILURE);
  }

  {
    AutoSelectionRestorer restoreSelectionLater(*this);

    // MOZ_KnownLive(*absolutelyPositionedStyledElement): It's
    // absolutelyPositionedElement whose type is RefPtr.
    Result<int32_t, nsresult> result = AddZIndexWithTransaction(
        MOZ_KnownLive(*absolutelyPositionedStyledElement), aChange);
    if (result.isErr()) {
      NS_WARNING("HTMLEditor::AddZIndexWithTransaction() failed");
      return EditActionHandled(result.unwrapErr());
    }
  }

  // Restoring Selection might cause destroying the HTML editor.
  return NS_WARN_IF(Destroyed()) ? EditActionHandled(NS_ERROR_EDITOR_DESTROYED)
                                 : EditActionHandled(NS_OK);
}

nsresult HTMLEditor::OnDocumentModified() {
  if (mPendingDocumentModifiedRunner) {
    return NS_OK;  // We've already posted same runnable into the queue.
  }
  mPendingDocumentModifiedRunner = NewRunnableMethod(
      "HTMLEditor::OnModifyDocument", this, &HTMLEditor::OnModifyDocument);
  nsContentUtils::AddScriptRunner(do_AddRef(mPendingDocumentModifiedRunner));
  // Be aware, if OnModifyDocument() may be called synchronously, the
  // editor might have been destroyed here.
  return NS_WARN_IF(Destroyed()) ? NS_ERROR_EDITOR_DESTROYED : NS_OK;
}

}  // namespace mozilla
