/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLEditRules.h"

#include <stdlib.h>

#include "HTMLEditUtils.h"
#include "TextEditUtils.h"
#include "WSRunObject.h"
#include "mozilla/Assertions.h"
#include "mozilla/ContentIterator.h"
#include "mozilla/CSSEditUtils.h"
#include "mozilla/EditAction.h"
#include "mozilla/EditorDOMPoint.h"
#include "mozilla/EditorUtils.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Move.h"
#include "mozilla/mozalloc.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/Preferences.h"
#include "mozilla/RangeUtils.h"
#include "mozilla/TextComposition.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLBRElement.h"
#include "mozilla/dom/RangeBinding.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/StaticRange.h"
#include "nsAString.h"
#include "nsAlgorithm.h"
#include "nsCRT.h"
#include "nsCRTGlue.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsAtom.h"
#include "nsHTMLDocument.h"
#include "nsIContent.h"
#include "nsID.h"
#include "nsIFrame.h"
#include "nsIHTMLAbsPosEditor.h"
#include "nsINode.h"
#include "nsLiteralString.h"
#include "nsRange.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "nsTextNode.h"
#include "nsThreadUtils.h"
#include "nsUnicharUtils.h"
#include <algorithm>

// Workaround for windows headers
#ifdef SetProp
#  undef SetProp
#endif

class nsISupports;

namespace mozilla {

using namespace dom;

enum { kLonely = 0, kPrevSib = 1, kNextSib = 2, kBothSibs = 3 };

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
    case EditSubAction::eRemoveList:
    case EditSubAction::eCreateOrChangeDefinitionList:
    case EditSubAction::eInsertElement:
    case EditSubAction::eInsertQuotation:
    case EditSubAction::eInsertQuotedText:
      return true;
    default:
      return false;
  }
}

static nsAtom& ParagraphSeparatorElement(ParagraphSeparator separator) {
  switch (separator) {
    default:
      MOZ_FALLTHROUGH_ASSERT("Unexpected paragraph separator!");

    case ParagraphSeparator::div:
      return *nsGkAtoms::div;

    case ParagraphSeparator::p:
      return *nsGkAtoms::p;

    case ParagraphSeparator::br:
      return *nsGkAtoms::br;
  }
}

class TableCellAndListItemFunctor final : public BoolDomIterFunctor {
 public:
  // Used to build list of all li's, td's & th's iterator covers
  virtual bool operator()(nsINode* aNode) const override {
    return HTMLEditUtils::IsTableCell(aNode) ||
           HTMLEditUtils::IsListItem(aNode);
  }
};

class BRNodeFunctor final : public BoolDomIterFunctor {
 public:
  virtual bool operator()(nsINode* aNode) const override {
    return aNode->IsHTMLElement(nsGkAtoms::br);
  }
};

class EmptyEditableFunctor final : public BoolDomIterFunctor {
 public:
  explicit EmptyEditableFunctor(HTMLEditor* aHTMLEditor)
      : mHTMLEditor(aHTMLEditor) {}

  virtual bool operator()(nsINode* aNode) const override {
    if (mHTMLEditor->IsEditable(aNode) &&
        (HTMLEditUtils::IsListItem(aNode) ||
         HTMLEditUtils::IsTableCellOrCaption(*aNode))) {
      bool bIsEmptyNode;
      nsresult rv =
          mHTMLEditor->IsEmptyNode(aNode, &bIsEmptyNode, false, false);
      NS_ENSURE_SUCCESS(rv, false);
      if (bIsEmptyNode) {
        return true;
      }
    }
    return false;
  }

 protected:
  HTMLEditor* mHTMLEditor;
};

class MOZ_RAII AutoSetTemporaryAncestorLimiter final {
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER;

 public:
  explicit AutoSetTemporaryAncestorLimiter(
      HTMLEditor& aHTMLEditor, Selection& aSelection,
      nsINode& aStartPointNode MOZ_GUARD_OBJECT_NOTIFIER_PARAM) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;

    if (aSelection.GetAncestorLimiter()) {
      return;
    }

    Element* root = aHTMLEditor.FindSelectionRoot(&aStartPointNode);
    if (root) {
      aHTMLEditor.InitializeSelectionAncestorLimit(*root);
      mSelection = &aSelection;
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

/********************************************************
 * mozilla::HTMLEditRules
 ********************************************************/
template void HTMLEditor::SelectBRElementIfCollapsedInEmptyBlock(
    RangeBoundary& aStartRef, RangeBoundary& aEndRef);
template void HTMLEditor::SelectBRElementIfCollapsedInEmptyBlock(
    RawRangeBoundary& aStartRef, RangeBoundary& aEndRef);
template void HTMLEditor::SelectBRElementIfCollapsedInEmptyBlock(
    RangeBoundary& aStartRef, RawRangeBoundary& aEndRef);
template void HTMLEditor::SelectBRElementIfCollapsedInEmptyBlock(
    RawRangeBoundary& aStartRef, RawRangeBoundary& aEndRef);
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
    EditSubAction aEditSubAction);
template already_AddRefed<nsRange>
HTMLEditor::CreateRangeExtendedToHardLineStartAndEnd(
    const RawRangeBoundary& aStartRef, const RangeBoundary& aEndRef,
    EditSubAction aEditSubAction);
template already_AddRefed<nsRange>
HTMLEditor::CreateRangeExtendedToHardLineStartAndEnd(
    const RangeBoundary& aStartRef, const RawRangeBoundary& aEndRef,
    EditSubAction aEditSubAction);
template already_AddRefed<nsRange>
HTMLEditor::CreateRangeExtendedToHardLineStartAndEnd(
    const RawRangeBoundary& aStartRef, const RawRangeBoundary& aEndRef,
    EditSubAction aEditSubAction);
template EditorDOMPoint HTMLEditor::GetWhiteSpaceEndPoint(
    const RangeBoundary& aPoint, ScanDirection aScanDirection);
template EditorDOMPoint HTMLEditor::GetWhiteSpaceEndPoint(
    const RawRangeBoundary& aPoint, ScanDirection aScanDirection);
template EditorDOMPoint HTMLEditor::GetCurrentHardLineStartPoint(
    const RangeBoundary& aPoint, EditSubAction aEditSubAction);
template EditorDOMPoint HTMLEditor::GetCurrentHardLineStartPoint(
    const RawRangeBoundary& aPoint, EditSubAction aEditSubAction);
template EditorDOMPoint HTMLEditor::GetCurrentHardLineEndPoint(
    const RangeBoundary& aPoint);
template EditorDOMPoint HTMLEditor::GetCurrentHardLineEndPoint(
    const RawRangeBoundary& aPoint);

HTMLEditRules::HTMLEditRules() : mHTMLEditor(nullptr), mInitialized(false) {
  mIsHTMLEditRules = true;
}

nsresult HTMLEditRules::Init(TextEditor* aTextEditor) {
  if (NS_WARN_IF(!aTextEditor) || NS_WARN_IF(!aTextEditor->AsHTMLEditor())) {
    return NS_ERROR_INVALID_ARG;
  }

  mHTMLEditor = aTextEditor->AsHTMLEditor();
  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_FAILURE;
  }

  AutoSafeEditorData setData(*this, *mHTMLEditor);

  nsresult rv = TextEditRules::Init(aTextEditor);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_FAILURE;
  }

  Element* rootElement = HTMLEditorRef().GetRoot();
  if (NS_WARN_IF(!rootElement && !HTMLEditorRef().GetDocument())) {
    return NS_ERROR_FAILURE;
  }

  // make a utility range for use by the listenter
  if (rootElement) {
    nsresult rv = InsertBRElementToEmptyListItemsAndTableCellsInRange(
        RawRangeBoundary(rootElement, 0),
        RawRangeBoundary(rootElement, rootElement->GetChildCount()));
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "Failed to insert <br> elements to empty list items and table cells");
  }

  mInitialized = true;  // Start to handle edit sub-actions.

  return NS_OK;
}

nsresult HTMLEditRules::DetachEditor() {
  mHTMLEditor = nullptr;
  return TextEditRules::DetachEditor();
}

nsresult HTMLEditRules::BeforeEdit() {
  MOZ_ASSERT(!mIsHandling);

  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }

  if (!mInitialized) {
    return NS_OK;  // We should do nothing if we're being initialized.
  }

#ifdef DEBUG
  mIsHandling = true;
#endif  // #ifdef DEBUG

  AutoSafeEditorData setData(*this, *mHTMLEditor);

  // Remember where our selection was before edit action took place:
  if (HTMLEditorRef().GetCompositionStartPoint().IsSet()) {
    // If there is composition string, let's remember current composition
    // range.
    HTMLEditorRef().TopLevelEditSubActionDataRef().mSelectedRange->StoreRange(
        HTMLEditorRef().GetCompositionStartPoint(),
        HTMLEditorRef().GetCompositionEndPoint());
  } else {
    // Get the selection location
    if (!SelectionRefPtr()->RangeCount()) {
      return NS_ERROR_UNEXPECTED;
    }
    HTMLEditorRef().TopLevelEditSubActionDataRef().mSelectedRange->StoreRange(
        SelectionRefPtr()->GetRangeAt(0));
  }
  nsCOMPtr<nsINode> selStartNode = HTMLEditorRef()
                                       .TopLevelEditSubActionDataRef()
                                       .mSelectedRange->mStartContainer;
  nsCOMPtr<nsINode> selEndNode = HTMLEditorRef()
                                     .TopLevelEditSubActionDataRef()
                                     .mSelectedRange->mEndContainer;

  // Register with range updater to track this as we perturb the doc
  HTMLEditorRef().RangeUpdaterRef().RegisterRangeItem(
      HTMLEditorRef().TopLevelEditSubActionDataRef().mSelectedRange);

  // Remember current inline styles for deletion and normal insertion ops
  bool cacheInlineStyles;
  switch (HTMLEditorRef().GetTopLevelEditSubAction()) {
    case EditSubAction::eInsertText:
    case EditSubAction::eInsertTextComingFromIME:
    case EditSubAction::eDeleteSelectedContent:
      cacheInlineStyles = true;
      break;
    default:
      cacheInlineStyles = IsStyleCachePreservingSubAction(
          HTMLEditorRef().GetTopLevelEditSubAction());
      break;
  }
  if (cacheInlineStyles) {
    nsCOMPtr<nsINode> selNode =
        HTMLEditorRef().GetDirectionOfTopLevelEditSubAction() ==
                nsIEditor::eNext
            ? selEndNode
            : selStartNode;
    if (NS_WARN_IF(!selNode)) {
      return NS_ERROR_FAILURE;
    }
    nsresult rv = MOZ_KnownLive(HTMLEditorRef()).CacheInlineStyles(*selNode);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // Stabilize the document against contenteditable count changes
  Document* doc = HTMLEditorRef().GetDocument();
  if (NS_WARN_IF(!doc)) {
    return NS_ERROR_FAILURE;
  }
  if (doc->GetEditingState() == Document::EditingState::eContentEditable) {
    doc->ChangeContentEditableCount(nullptr, +1);
    HTMLEditorRef()
        .TopLevelEditSubActionDataRef()
        .mRestoreContentEditableCount = true;
  }

  // Check that selection is in subtree defined by body node
  nsresult rv = ConfirmSelectionInBody();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return NS_ERROR_EDITOR_DESTROYED;
  }

  return NS_OK;
}

nsresult HTMLEditRules::AfterEdit() {
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }

  if (!mInitialized) {
    return NS_OK;  // We should do nothing if we're being initialized.
  }

#ifdef DEBUG
  MOZ_ASSERT(mIsHandling);
  mIsHandling = false;
#endif  // #ifdef DEBUG

  AutoSafeEditorData setData(*this, *mHTMLEditor);

  // Do all the tricky stuff
  nsresult rv = AfterEditInner();
  // Perhaps, we need to do the following jobs even if the editor has been
  // destroyed since they adjust some states of HTML document but don't
  // modify the DOM tree nor Selection.

  // Free up selectionState range item
  HTMLEditorRef().RangeUpdaterRef().DropRangeItem(
      HTMLEditorRef().TopLevelEditSubActionDataRef().mSelectedRange);

  // Reset the contenteditable count to its previous value
  if (HTMLEditorRef()
          .TopLevelEditSubActionDataRef()
          .mRestoreContentEditableCount) {
    Document* doc = HTMLEditorRef().GetDocument();
    if (NS_WARN_IF(!doc)) {
      return NS_ERROR_FAILURE;
    }
    if (doc->GetEditingState() == Document::EditingState::eContentEditable) {
      doc->ChangeContentEditableCount(nullptr, -1);
    }
  }

  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "AfterEditInner() failed");
  return rv;
}

nsresult HTMLEditRules::AfterEditInner() {
  MOZ_ASSERT(IsEditorDataAvailable());

  nsresult rv = ConfirmSelectionInBody();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to normalize Selection");
  switch (HTMLEditorRef().GetTopLevelEditSubAction()) {
    case EditSubAction::eReplaceHeadWithHTMLSource:
    case EditSubAction::eCreatePaddingBRElementForEmptyEditor:
      return NS_OK;
    default:
      break;
  }

  if (HTMLEditorRef()
          .TopLevelEditSubActionDataRef()
          .mChangedRange->IsPositioned() &&
      HTMLEditorRef().GetTopLevelEditSubAction() != EditSubAction::eUndo &&
      HTMLEditorRef().GetTopLevelEditSubAction() != EditSubAction::eRedo) {
    // don't let any txns in here move the selection around behind our back.
    // Note that this won't prevent explicit selection setting from working.
    AutoTransactionsConserveSelection dontChangeMySelection(HTMLEditorRef());

    switch (HTMLEditorRef().GetTopLevelEditSubAction()) {
      case EditSubAction::eInsertText:
      case EditSubAction::eInsertTextComingFromIME:
      case EditSubAction::eInsertLineBreak:
      case EditSubAction::eInsertParagraphSeparator:
      case EditSubAction::eDeleteText: {
        // XXX We should investigate whether this is really needed because it
        //     seems that the following code does not handle the whitespaces.
        RefPtr<nsRange> extendedChangedRange =
            HTMLEditorRef().CreateRangeIncludingAdjuscentWhiteSpaces(
                *HTMLEditorRef().TopLevelEditSubActionDataRef().mChangedRange);
        if (extendedChangedRange) {
          // Use extended range temporarily.
          HTMLEditorRef().TopLevelEditSubActionDataRef().mChangedRange =
              std::move(extendedChangedRange);
        }
        break;
      }
      default: {
        RefPtr<nsRange> extendedChangedRange =
            HTMLEditorRef().CreateRangeExtendedToHardLineStartAndEnd(
                *HTMLEditorRef().TopLevelEditSubActionDataRef().mChangedRange,
                HTMLEditorRef().GetTopLevelEditSubAction());
        if (extendedChangedRange) {
          // Use extended range temporarily.
          HTMLEditorRef().TopLevelEditSubActionDataRef().mChangedRange =
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
    // That's why this is here rather than DidDeleteSelection().
    // However, we shouldn't insert <br> elements if we've already removed
    // empty block parents because users may want to disappear the line by
    // the deletion.
    if (HTMLEditorRef().GetTopLevelEditSubAction() ==
            EditSubAction::eDeleteSelectedContent &&
        HTMLEditorRef()
            .TopLevelEditSubActionDataRef()
            .mDidDeleteNonCollapsedRange &&
        !HTMLEditorRef()
             .TopLevelEditSubActionDataRef()
             .mDidDeleteEmptyParentBlocks) {
      nsresult rv = InsertBRIfNeeded();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    // add in any needed <br>s, and remove any unneeded ones.
    nsresult rv = InsertBRElementToEmptyListItemsAndTableCellsInRange(
        HTMLEditorRef()
            .TopLevelEditSubActionDataRef()
            .mChangedRange->StartRef()
            .AsRaw(),
        HTMLEditorRef()
            .TopLevelEditSubActionDataRef()
            .mChangedRange->EndRef()
            .AsRaw());
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "Failed to insert <br> elements to empty list items and table cells");

    // merge any adjacent text nodes
    switch (HTMLEditorRef().GetTopLevelEditSubAction()) {
      case EditSubAction::eInsertText:
      case EditSubAction::eInsertTextComingFromIME:
        break;
      default: {
        nsresult rv = MOZ_KnownLive(HTMLEditorRef())
                          .CollapseAdjacentTextNodes(
                              MOZ_KnownLive(HTMLEditorRef()
                                                .TopLevelEditSubActionDataRef()
                                                .mChangedRange));
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        break;
      }
    }

    // clean up any empty nodes in the selection
    rv = RemoveEmptyNodesInChangedRange();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // attempt to transform any unneeded nbsp's into spaces after doing various
    // operations
    switch (HTMLEditorRef().GetTopLevelEditSubAction()) {
      case EditSubAction::eInsertText:
      case EditSubAction::eInsertTextComingFromIME:
      case EditSubAction::eDeleteSelectedContent:
      case EditSubAction::eInsertLineBreak:
      case EditSubAction::eInsertParagraphSeparator:
      case EditSubAction::ePasteHTMLContent:
      case EditSubAction::eInsertHTMLSource: {
        // TODO: Temporarily, WSRunObject replaces ASCII whitespaces with NPSPs
        //       and then, we'll replace them with ASCII whitespaces here.  We
        //       should avoid this overwriting things as far as possible because
        //       replacing characters in text nodes causes running mutation
        //       event listeners which are really expensive.
        // Adjust end of composition string if there is composition string.
        EditorRawDOMPoint pointToAdjust(
            HTMLEditorRef().GetCompositionEndPoint());
        if (!pointToAdjust.IsSet()) {
          // Otherwise, adjust current selection start point.
          pointToAdjust = EditorBase::GetStartPoint(*SelectionRefPtr());
          if (NS_WARN_IF(!pointToAdjust.IsSet())) {
            return NS_ERROR_FAILURE;
          }
        }
        rv = WSRunObject(&HTMLEditorRef(), pointToAdjust).AdjustWhitespace();
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        // also do this for original selection endpoints.
        if (NS_WARN_IF(!HTMLEditorRef()
                            .TopLevelEditSubActionDataRef()
                            .mSelectedRange->IsSet())) {
          return NS_ERROR_FAILURE;
        }
        WSRunObject(&HTMLEditorRef(), HTMLEditorRef()
                                          .TopLevelEditSubActionDataRef()
                                          .mSelectedRange->StartRawPoint())
            .AdjustWhitespace();
        // we only need to handle old selection endpoint if it was different
        // from start
        if (HTMLEditorRef()
                .TopLevelEditSubActionDataRef()
                .mSelectedRange->IsCollapsed()) {
          WSRunObject(&HTMLEditorRef(), HTMLEditorRef()
                                            .TopLevelEditSubActionDataRef()
                                            .mSelectedRange->EndRawPoint())
              .AdjustWhitespace();
        }
        break;
      }
      default:
        break;
    }

    // if we created a new block, make sure selection lands in it
    if (HTMLEditorRef().TopLevelEditSubActionDataRef().mNewBlockElement) {
      rv = PinSelectionToNewBlock();
      if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "Failed to pin selection to the new block");
    }

    // Adjust selection for insert text, html paste, and delete actions if
    // we haven't removed new empty blocks.  Note that if empty block parents
    // are removed, Selection should've been adjusted by the method which
    // did it.
    if (!HTMLEditorRef()
             .TopLevelEditSubActionDataRef()
             .mDidDeleteEmptyParentBlocks) {
      switch (HTMLEditorRef().GetTopLevelEditSubAction()) {
        case EditSubAction::eInsertText:
        case EditSubAction::eInsertTextComingFromIME:
        case EditSubAction::eDeleteSelectedContent:
        case EditSubAction::eInsertLineBreak:
        case EditSubAction::eInsertParagraphSeparator:
        case EditSubAction::ePasteHTMLContent:
        case EditSubAction::eInsertHTMLSource:
          rv = AdjustSelection(
              HTMLEditorRef().GetDirectionOfTopLevelEditSubAction());
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
          break;
        default:
          break;
      }
    }

    // check for any styles which were removed inappropriately
    bool reapplyCachedStyle;
    switch (HTMLEditorRef().GetTopLevelEditSubAction()) {
      case EditSubAction::eInsertText:
      case EditSubAction::eInsertTextComingFromIME:
      case EditSubAction::eDeleteSelectedContent:
        reapplyCachedStyle = true;
        break;
      default:
        reapplyCachedStyle = IsStyleCachePreservingSubAction(
            HTMLEditorRef().GetTopLevelEditSubAction());
        break;
    }
    if (reapplyCachedStyle) {
      HTMLEditorRef().mTypeInState->UpdateSelState(SelectionRefPtr());
      rv = MOZ_KnownLive(HTMLEditorRef()).ReapplyCachedStyles();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      HTMLEditorRef()
          .TopLevelEditSubActionDataRef()
          .mCachedInlineStyles.Clear();
    }
  }

  rv = HTMLEditorRef().HandleInlineSpellCheck(
      HTMLEditorRef()
          .TopLevelEditSubActionDataRef()
          .mSelectedRange->StartPoint(),
      HTMLEditorRef().TopLevelEditSubActionDataRef().mChangedRange);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // detect empty doc
  // XXX Need to investigate when the padding <br> element is removed because
  //     I don't see the <br> element with testing manually.  If it won't be
  //     used, we can get rid of this cost.
  rv = MOZ_KnownLive(HTMLEditorRef())
           .MaybeCreatePaddingBRElementForEmptyEditor();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // adjust selection HINT if needed
  if (!HTMLEditorRef()
           .TopLevelEditSubActionDataRef()
           .mDidExplicitlySetInterLine) {
    CheckInterlinePosition();
  }

  return NS_OK;
}

nsresult HTMLEditRules::WillDoAction(EditSubActionInfo& aInfo, bool* aCancel,
                                     bool* aHandled) {
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }

  MOZ_ASSERT(aCancel);
  MOZ_ASSERT(aHandled);

  *aCancel = false;
  *aHandled = false;

  // Deal with actions for which we don't need to check whether the selection is
  // editable.
  if (aInfo.mEditSubAction == EditSubAction::eComputeTextToOutput) {
    return TextEditRules::WillDoAction(aInfo, aCancel, aHandled);
  }

  AutoSafeEditorData setData(*this, *mHTMLEditor);

  // Nothing to do if there's no selection to act on
  if (NS_WARN_IF(!SelectionRefPtr()->RangeCount())) {
    return NS_OK;
  }

  RefPtr<nsRange> range = SelectionRefPtr()->GetRangeAt(0);
  nsCOMPtr<nsINode> selStartNode = range->GetStartContainer();
  if (NS_WARN_IF(!selStartNode)) {
    return NS_ERROR_FAILURE;
  }

  if (!HTMLEditorRef().IsModifiableNode(*selStartNode)) {
    *aCancel = true;
    return NS_OK;
  }

  nsCOMPtr<nsINode> selEndNode = range->GetEndContainer();
  if (NS_WARN_IF(!selEndNode)) {
    return NS_ERROR_FAILURE;
  }

  if (selStartNode != selEndNode) {
    if (!HTMLEditorRef().IsModifiableNode(*selEndNode)) {
      *aCancel = true;
      return NS_OK;
    }

    nsINode* commonAncestor = range->GetCommonAncestor();
    if (NS_WARN_IF(!commonAncestor)) {
      return NS_ERROR_FAILURE;
    }
    if (!HTMLEditorRef().IsModifiableNode(*commonAncestor)) {
      *aCancel = true;
      return NS_OK;
    }
  }

  switch (aInfo.mEditSubAction) {
    case EditSubAction::eInsertText:
    case EditSubAction::eInsertTextComingFromIME:
      UndefineCaretBidiLevel();
      return MOZ_KnownLive(HTMLEditorRef())
          .WillInsertText(aInfo.mEditSubAction, aCancel, aHandled,
                          aInfo.inString, aInfo.outString, aInfo.maxLength);
    case EditSubAction::eInsertParagraphSeparator: {
      UndefineCaretBidiLevel();
      EditActionResult result = WillInsertParagraphSeparator();
      if (NS_WARN_IF(result.Failed())) {
        return result.Rv();
      }
      *aCancel = result.Canceled();
      *aHandled = result.Handled();
      MOZ_ASSERT(!result.Ignored());
      return NS_OK;
    }
    case EditSubAction::eDeleteSelectedContent:
      return WillDeleteSelection(aInfo.collapsedAction, aInfo.stripWrappers,
                                 aCancel, aHandled);
    case EditSubAction::eCreateOrChangeList:
      return WillMakeList(aInfo.blockType, aInfo.entireList, aInfo.bulletType,
                          aCancel, aHandled);
    case EditSubAction::eIndent:
      return WillIndent(aCancel, aHandled);
    case EditSubAction::eOutdent:
      return WillOutdent(aCancel, aHandled);
    case EditSubAction::eSetPositionToAbsolute:
      return WillAbsolutePosition(aCancel, aHandled);
    case EditSubAction::eSetPositionToStatic:
      return WillRemoveAbsolutePosition(aCancel, aHandled);
    case EditSubAction::eSetOrClearAlignment:
      return WillAlign(*aInfo.alignType, aCancel, aHandled);
    case EditSubAction::eCreateOrRemoveBlock:
      return WillMakeBasicBlock(*aInfo.blockType, aCancel, aHandled);
    case EditSubAction::eRemoveList: {
      nsresult rv = WillRemoveList(aCancel, aHandled);
      if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED) ||
          NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      return NS_OK;
    }
    case EditSubAction::eCreateOrChangeDefinitionList:
      return WillMakeDefListItem(aInfo.blockType, aInfo.entireList, aCancel,
                                 aHandled);
    case EditSubAction::eInsertElement:
    case EditSubAction::eInsertQuotedText: {
      nsresult rv = MOZ_KnownLive(HTMLEditorRef()).WillInsert(aCancel);
      if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "WillInsert() failed");
      return NS_OK;
    }
    case EditSubAction::eDecreaseZIndex:
      return WillRelativeChangeZIndex(-1, aCancel, aHandled);
    case EditSubAction::eIncreaseZIndex:
      return WillRelativeChangeZIndex(1, aCancel, aHandled);
    case EditSubAction::eInsertHTMLSource:
    case EditSubAction::eUndo:
    case EditSubAction::eRedo:
      MOZ_ASSERT_UNREACHABLE("This path should've been dead code");
      return NS_ERROR_UNEXPECTED;
    default:
      return TextEditRules::WillDoAction(aInfo, aCancel, aHandled);
  }
}

nsresult HTMLEditRules::DidDoAction(EditSubActionInfo& aInfo,
                                    nsresult aResult) {
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }

  AutoSafeEditorData setData(*this, *mHTMLEditor);

  switch (aInfo.mEditSubAction) {
    case EditSubAction::eInsertText:
    case EditSubAction::eInsertLineBreak:
    case EditSubAction::eInsertParagraphSeparator:
    case EditSubAction::eInsertTextComingFromIME:
      return NS_OK;
    case EditSubAction::eDeleteSelectedContent:
      return DidDeleteSelection();
    case EditSubAction::eCreateOrRemoveBlock:
    case EditSubAction::eIndent:
    case EditSubAction::eOutdent:
    case EditSubAction::eSetOrClearAlignment:
      return DidMakeBasicBlock();
    case EditSubAction::eSetPositionToAbsolute: {
      nsresult rv = DidMakeBasicBlock();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      return DidAbsolutePosition();
    }
    case EditSubAction::eInsertElement:
    case EditSubAction::eInsertQuotedText:
      return NS_OK;
    case EditSubAction::eInsertHTMLSource:
    case EditSubAction::eUndo:
    case EditSubAction::eRedo:
      MOZ_ASSERT_UNREACHABLE("This path should've been dead code");
      return NS_ERROR_UNEXPECTED;
    default:
      return TextEditRules::DidDoAction(aInfo, aResult);
  }
}

bool HTMLEditRules::DocumentIsEmpty() const {
  // XXX This is wrong.  Even if there is no padding <br> element for empty
  //     editor, the editor may be empty.
  return HTMLEditorRef().HasPaddingBRElementForEmptyEditor();
}

nsresult HTMLEditRules::GetListState(bool* aMixed, bool* aOL, bool* aUL,
                                     bool* aDL) {
  NS_ENSURE_TRUE(aMixed && aOL && aUL && aDL, NS_ERROR_NULL_POINTER);
  *aMixed = false;
  *aOL = false;
  *aUL = false;
  *aDL = false;
  bool bNonList = false;

  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }

  AutoSafeEditorData setData(*this, *mHTMLEditor);

  nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
  nsresult rv =
      GetListActionNodes(arrayOfNodes, EntireList::no, TouchContent::no);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Examine list type for nodes in selection.
  for (const auto& curNode : arrayOfNodes) {
    if (!curNode->IsElement()) {
      bNonList = true;
    } else if (curNode->IsHTMLElement(nsGkAtoms::ul)) {
      *aUL = true;
    } else if (curNode->IsHTMLElement(nsGkAtoms::ol)) {
      *aOL = true;
    } else if (curNode->IsHTMLElement(nsGkAtoms::li)) {
      if (dom::Element* parent = curNode->GetParentElement()) {
        if (parent->IsHTMLElement(nsGkAtoms::ul)) {
          *aUL = true;
        } else if (parent->IsHTMLElement(nsGkAtoms::ol)) {
          *aOL = true;
        }
      }
    } else if (curNode->IsAnyOfHTMLElements(nsGkAtoms::dl, nsGkAtoms::dt,
                                            nsGkAtoms::dd)) {
      *aDL = true;
    } else {
      bNonList = true;
    }
  }

  // hokey arithmetic with booleans
  if ((*aUL + *aOL + *aDL + bNonList) > 1) {
    *aMixed = true;
  }

  return NS_OK;
}

nsresult HTMLEditRules::GetListItemState(bool* aMixed, bool* aLI, bool* aDT,
                                         bool* aDD) {
  NS_ENSURE_TRUE(aMixed && aLI && aDT && aDD, NS_ERROR_NULL_POINTER);
  *aMixed = false;
  *aLI = false;
  *aDT = false;
  *aDD = false;
  bool bNonList = false;

  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }

  AutoSafeEditorData setData(*this, *mHTMLEditor);

  nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
  nsresult rv =
      GetListActionNodes(arrayOfNodes, EntireList::no, TouchContent::no);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // examine list type for nodes in selection
  for (const auto& node : arrayOfNodes) {
    if (!node->IsElement()) {
      bNonList = true;
    } else if (node->IsAnyOfHTMLElements(nsGkAtoms::ul, nsGkAtoms::ol,
                                         nsGkAtoms::li)) {
      *aLI = true;
    } else if (node->IsHTMLElement(nsGkAtoms::dt)) {
      *aDT = true;
    } else if (node->IsHTMLElement(nsGkAtoms::dd)) {
      *aDD = true;
    } else if (node->IsHTMLElement(nsGkAtoms::dl)) {
      // need to look inside dl and see which types of items it has
      bool bDT, bDD;
      GetDefinitionListItemTypes(node->AsElement(), &bDT, &bDD);
      *aDT |= bDT;
      *aDD |= bDD;
    } else {
      bNonList = true;
    }
  }

  // hokey arithmetic with booleans
  if (*aDT + *aDD + bNonList > 1) {
    *aMixed = true;
  }

  return NS_OK;
}

nsresult HTMLEditRules::GetAlignment(bool* aMixed,
                                     nsIHTMLEditor::EAlignment* aAlign) {
  MOZ_ASSERT(aMixed && aAlign);

  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }

  AutoSafeEditorData setData(*this, *mHTMLEditor);

  // For now, just return first alignment.  We'll lie about if it's mixed.
  // This is for efficiency given that our current ui doesn't care if it's
  // mixed.
  // cmanske: NOT TRUE! We would like to pay attention to mixed state in Format
  // | Align submenu!

  // This routine assumes that alignment is done ONLY via divs

  // Default alignment is left
  *aMixed = false;
  *aAlign = nsIHTMLEditor::eLeft;

  // Get selection location
  if (NS_WARN_IF(!HTMLEditorRef().GetRoot())) {
    return NS_ERROR_FAILURE;
  }
  OwningNonNull<Element> root = *HTMLEditorRef().GetRoot();

  int32_t rootOffset =
      root->GetParentNode() ? root->GetParentNode()->ComputeIndexOf(root) : -1;

  nsRange* firstRange = SelectionRefPtr()->GetRangeAt(0);
  if (NS_WARN_IF(!firstRange)) {
    return NS_ERROR_FAILURE;
  }
  EditorRawDOMPoint atStartOfSelection(firstRange->StartRef());
  if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(atStartOfSelection.IsSetAndValid());

  // Is the selection collapsed?
  nsCOMPtr<nsINode> nodeToExamine;
  if (SelectionRefPtr()->IsCollapsed() ||
      atStartOfSelection.GetContainerAsText()) {
    // If selection is collapsed, we want to look at the container of selection
    // start and its ancestors for divs with alignment on them.  If we are in a
    // text node, then that is the node of interest.
    nodeToExamine = atStartOfSelection.GetContainer();
    if (NS_WARN_IF(!nodeToExamine)) {
      return NS_ERROR_FAILURE;
    }
  } else if (atStartOfSelection.IsContainerHTMLElement(nsGkAtoms::html) &&
             atStartOfSelection.Offset() == static_cast<uint32_t>(rootOffset)) {
    // If we have selected the body, let's look at the first editable node
    nodeToExamine = HTMLEditorRef().GetNextEditableNode(atStartOfSelection);
    if (NS_WARN_IF(!nodeToExamine)) {
      return NS_ERROR_FAILURE;
    }
  } else {
    AutoTArray<RefPtr<nsRange>, 4> arrayOfRanges;
    HTMLEditorRef().GetSelectionRangesExtendedToHardLineStartAndEnd(
        arrayOfRanges, EditSubAction::eSetOrClearAlignment);

    // Use these ranges to construct a list of nodes to act on.
    nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
    nsresult rv = HTMLEditorRef().CollectEditTargetNodes(
        arrayOfRanges, arrayOfNodes, EditSubAction::eSetOrClearAlignment);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    nodeToExamine = arrayOfNodes.SafeElementAt(0);
    if (NS_WARN_IF(!nodeToExamine)) {
      return NS_ERROR_FAILURE;
    }
  }

  RefPtr<Element> blockParent = HTMLEditorRef().GetBlock(*nodeToExamine);
  if (NS_WARN_IF(!blockParent)) {
    return NS_ERROR_FAILURE;
  }

  if (HTMLEditorRef().IsCSSEnabled() &&
      CSSEditUtils::IsCSSEditableProperty(blockParent, nullptr,
                                          nsGkAtoms::align)) {
    // We are in CSS mode and we know how to align this element with CSS
    nsAutoString value;
    // Let's get the value(s) of text-align or margin-left/margin-right
    CSSEditUtils::GetCSSEquivalentToHTMLInlineStyleSet(
        blockParent, nullptr, nsGkAtoms::align, value, CSSEditUtils::eComputed);
    if (value.EqualsLiteral("center") || value.EqualsLiteral("-moz-center") ||
        value.EqualsLiteral("auto auto")) {
      *aAlign = nsIHTMLEditor::eCenter;
      return NS_OK;
    }
    if (value.EqualsLiteral("right") || value.EqualsLiteral("-moz-right") ||
        value.EqualsLiteral("auto 0px")) {
      *aAlign = nsIHTMLEditor::eRight;
      return NS_OK;
    }
    if (value.EqualsLiteral("justify")) {
      *aAlign = nsIHTMLEditor::eJustify;
      return NS_OK;
    }
    *aAlign = nsIHTMLEditor::eLeft;
    return NS_OK;
  }

  // Check up the ladder for divs with alignment
  bool isFirstNodeToExamine = true;
  for (; nodeToExamine; nodeToExamine = nodeToExamine->GetParentNode()) {
    if (!isFirstNodeToExamine &&
        nodeToExamine->IsHTMLElement(nsGkAtoms::table)) {
      // The node to examine is a table and this is not the first node we
      // examine; let's break here to materialize the 'inline-block' behaviour
      // of html tables regarding to text alignment
      return NS_OK;
    }

    if (CSSEditUtils::IsCSSEditableProperty(nodeToExamine, nullptr,
                                            nsGkAtoms::align)) {
      nsAutoString value;
      CSSEditUtils::GetSpecifiedProperty(*nodeToExamine, *nsGkAtoms::textAlign,
                                         value);
      if (!value.IsEmpty()) {
        if (value.EqualsLiteral("center")) {
          *aAlign = nsIHTMLEditor::eCenter;
          return NS_OK;
        }
        if (value.EqualsLiteral("right")) {
          *aAlign = nsIHTMLEditor::eRight;
          return NS_OK;
        }
        if (value.EqualsLiteral("justify")) {
          *aAlign = nsIHTMLEditor::eJustify;
          return NS_OK;
        }
        if (value.EqualsLiteral("left")) {
          *aAlign = nsIHTMLEditor::eLeft;
          return NS_OK;
        }
        // XXX
        // text-align: start and end aren't supported yet
      }
    }

    if (HTMLEditUtils::SupportsAlignAttr(*nodeToExamine)) {
      // Check for alignment
      nsAutoString typeAttrVal;
      nodeToExamine->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::align,
                                          typeAttrVal);
      ToLowerCase(typeAttrVal);
      if (!typeAttrVal.IsEmpty()) {
        if (typeAttrVal.EqualsLiteral("center")) {
          *aAlign = nsIHTMLEditor::eCenter;
        } else if (typeAttrVal.EqualsLiteral("right")) {
          *aAlign = nsIHTMLEditor::eRight;
        } else if (typeAttrVal.EqualsLiteral("justify")) {
          *aAlign = nsIHTMLEditor::eJustify;
        } else {
          *aAlign = nsIHTMLEditor::eLeft;
        }
        return NS_OK;
      }
    }
    isFirstNodeToExamine = false;
  }
  return NS_OK;
}

static nsAtom& MarginPropertyAtomForIndent(nsINode& aNode) {
  nsAutoString direction;
  CSSEditUtils::GetComputedProperty(aNode, *nsGkAtoms::direction, direction);
  return direction.EqualsLiteral("rtl") ? *nsGkAtoms::marginRight
                                        : *nsGkAtoms::marginLeft;
}

nsresult HTMLEditRules::GetParagraphState(bool* aMixed, nsAString& outFormat) {
  if (NS_WARN_IF(!aMixed)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }

  // This routine is *heavily* tied to our ui choices in the paragraph
  // style popup.  I can't see a way around that.
  *aMixed = true;
  outFormat.Truncate(0);

  AutoSafeEditorData setData(*this, *mHTMLEditor);

  bool bMixed = false;
  // using "x" as an uninitialized value, since "" is meaningful
  nsAutoString formatStr(NS_LITERAL_STRING("x"));

  nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
  nsresult rv = GetParagraphFormatNodes(arrayOfNodes);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // post process list.  We need to replace any block nodes that are not format
  // nodes with their content.  This is so we only have to look "up" the
  // hierarchy to find format nodes, instead of both up and down.
  for (int32_t i = arrayOfNodes.Length() - 1; i >= 0; i--) {
    auto& curNode = arrayOfNodes[i];
    nsAutoString format;
    // if it is a known format node we have it easy
    if (HTMLEditor::NodeIsBlockStatic(curNode) &&
        !HTMLEditUtils::IsFormatNode(curNode)) {
      // arrayOfNodes.RemoveObject(curNode);
      rv = AppendInnerFormatNodes(arrayOfNodes, curNode);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  // we might have an empty node list.  if so, find selection parent
  // and put that on the list
  if (arrayOfNodes.IsEmpty()) {
    EditorRawDOMPoint selectionStartPoint(
        EditorBase::GetStartPoint(*SelectionRefPtr()));
    if (NS_WARN_IF(!selectionStartPoint.IsSet())) {
      return NS_ERROR_FAILURE;
    }
    arrayOfNodes.AppendElement(*selectionStartPoint.GetContainer());
  }

  // remember root node
  Element* rootElement = HTMLEditorRef().GetRoot();
  if (NS_WARN_IF(!rootElement)) {
    return NS_ERROR_FAILURE;
  }

  // loop through the nodes in selection and examine their paragraph format
  for (auto& curNode : Reversed(arrayOfNodes)) {
    nsAutoString format;
    // if it is a known format node we have it easy
    if (HTMLEditUtils::IsFormatNode(curNode)) {
      GetFormatString(curNode, format);
    } else if (HTMLEditor::NodeIsBlockStatic(curNode)) {
      // this is a div or some other non-format block.
      // we should ignore it.  Its children were appended to this list
      // by AppendInnerFormatNodes() call above.  We will get needed
      // info when we examine them instead.
      continue;
    } else {
      nsINode* node = curNode->GetParentNode();
      while (node) {
        if (node == rootElement) {
          format.Truncate(0);
          break;
        } else if (HTMLEditUtils::IsFormatNode(node)) {
          GetFormatString(node, format);
          break;
        }
        // else keep looking up
        node = node->GetParentNode();
      }
    }

    // if this is the first node, we've found, remember it as the format
    if (formatStr.EqualsLiteral("x")) {
      formatStr = format;
    }
    // else make sure it matches previously found format
    else if (format != formatStr) {
      bMixed = true;
      break;
    }
  }

  *aMixed = bMixed;
  outFormat = formatStr;
  return NS_OK;
}

nsresult HTMLEditRules::AppendInnerFormatNodes(
    nsTArray<OwningNonNull<nsINode>>& aArray, nsINode* aNode) {
  MOZ_ASSERT(aNode);

  // we only need to place any one inline inside this node onto
  // the list.  They are all the same for purposes of determining
  // paragraph style.  We use foundInline to track this as we are
  // going through the children in the loop below.
  bool foundInline = false;
  for (nsIContent* child = aNode->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    bool isBlock = HTMLEditor::NodeIsBlockStatic(*child);
    bool isFormat = HTMLEditUtils::IsFormatNode(child);
    if (isBlock && !isFormat) {
      // if it's a div, etc., recurse
      AppendInnerFormatNodes(aArray, child);
    } else if (isFormat) {
      aArray.AppendElement(*child);
    } else if (!foundInline) {
      // if this is the first inline we've found, use it
      foundInline = true;
      aArray.AppendElement(*child);
    }
  }
  return NS_OK;
}

nsresult HTMLEditRules::GetFormatString(nsINode* aNode, nsAString& outFormat) {
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);

  if (HTMLEditUtils::IsFormatNode(aNode)) {
    aNode->NodeInfo()->NameAtom()->ToString(outFormat);
  } else {
    outFormat.Truncate();
  }
  return NS_OK;
}

nsresult HTMLEditor::WillInsert(bool* aCancel) {
  MOZ_ASSERT(IsTopLevelEditSubActionDataAvailable());

  // XXX Why don't we stop handling this call if we're readonly or disabled?
  if (aCancel && (IsReadonly() || IsDisabled())) {
    *aCancel = true;
  }

  nsresult rv = EnsureNoPaddingBRElementForEmptyEditor();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Adjust selection to prevent insertion after a padding <br> element for
  // empty last line.  This next only works for collapsed selections right
  // now, because selection is a pain to work with when not collapsed.  (no
  // good way to extend start or end of selection), so we ignore those types
  // of selections.
  if (!SelectionRefPtr()->IsCollapsed()) {
    return NS_OK;
  }

  // If we are after a padding <br> element for empty last line in the same
  // block, then move selection to be before it
  nsRange* firstRange = SelectionRefPtr()->GetRangeAt(0);
  if (NS_WARN_IF(!firstRange)) {
    return NS_ERROR_FAILURE;
  }

  EditorRawDOMPoint atStartOfSelection(firstRange->StartRef());
  if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(atStartOfSelection.IsSetAndValid());

  // Get prior node
  nsCOMPtr<nsIContent> priorNode =
      GetPreviousEditableHTMLNode(atStartOfSelection);
  if (priorNode && EditorBase::IsPaddingBRElementForEmptyLastLine(*priorNode)) {
    RefPtr<Element> block1 = GetBlock(*atStartOfSelection.GetContainer());
    RefPtr<Element> block2 = GetBlockNodeParent(priorNode);

    if (block1 && block1 == block2) {
      // If we are here then the selection is right after a padding <br>
      // element for empty last line that is in the same block as the
      // selection.  We need to move the selection start to be before the
      // padding <br> element.
      EditorRawDOMPoint point(priorNode);
      ErrorResult error;
      SelectionRefPtr()->Collapse(point, error);
      if (NS_WARN_IF(Destroyed())) {
        error.SuppressException();
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(error.Failed())) {
        return error.StealNSResult();
      }
    }
  }

  if (TopLevelEditSubActionDataRef().mDidDeleteSelection) {
    switch (GetTopLevelEditSubAction()) {
      case EditSubAction::eInsertText:
      case EditSubAction::eInsertTextComingFromIME:
      case EditSubAction::eDeleteSelectedContent: {
        nsresult rv = ReapplyCachedStyles();
        if (NS_WARN_IF(NS_FAILED(rv))) {
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
    TopLevelEditSubActionDataRef().mCachedInlineStyles.Clear();
  }
  return NS_OK;
}

nsresult HTMLEditor::WillInsertText(EditSubAction aEditSubAction, bool* aCancel,
                                    bool* aHandled, const nsAString* inString,
                                    nsAString* outString, int32_t aMaxLength) {
  MOZ_ASSERT(IsTopLevelEditSubActionDataAvailable());

  if (NS_WARN_IF(!aCancel) || NS_WARN_IF(!aHandled)) {
    return NS_ERROR_NULL_POINTER;
  }

  // initialize out param
  *aCancel = false;
  *aHandled = true;
  // If the selection isn't collapsed, delete it.  Don't delete existing inline
  // tags, because we're hopefully going to insert text (bug 787432).
  if (!SelectionRefPtr()->IsCollapsed()) {
    nsresult rv =
        DeleteSelectionAsSubAction(nsIEditor::eNone, nsIEditor::eNoStrip);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // FYI: Ignore cancel result of WillInsert().
  nsresult rv = WillInsert();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "WillInsert() failed");

  // we need to get the doc
  RefPtr<Document> doc = GetDocument();
  if (NS_WARN_IF(!doc)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<nsRange> firstRange = SelectionRefPtr()->GetRangeAt(0);
  if (NS_WARN_IF(!firstRange)) {
    return NS_ERROR_FAILURE;
  }

  // for every property that is set, insert a new inline style node
  // XXX CreateStyleForInsertText() adjusts selection automatically, but
  //     it should just return the insertion point instead.
  rv = CreateStyleForInsertText(*firstRange);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  firstRange = SelectionRefPtr()->GetRangeAt(0);
  if (NS_WARN_IF(!firstRange)) {
    return NS_ERROR_FAILURE;
  }

  EditorDOMPoint pointToInsert(firstRange->StartRef());
  if (NS_WARN_IF(!pointToInsert.IsSet())) {
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(pointToInsert.IsSetAndValid());

  // dont put text in places that can't have it
  if (!EditorBase::IsTextNode(pointToInsert.GetContainer()) &&
      !CanContainTag(*pointToInsert.GetContainer(), *nsGkAtoms::textTagName)) {
    return NS_ERROR_FAILURE;
  }

  if (aEditSubAction == EditSubAction::eInsertTextComingFromIME) {
    EditorRawDOMPoint compositionStartPoint = GetCompositionStartPoint();
    if (!compositionStartPoint.IsSet()) {
      compositionStartPoint = pointToInsert;
    }

    if (inString->IsEmpty()) {
      // Right now the WSRunObject code bails on empty strings, but IME needs
      // the InsertTextWithTransaction() call to still happen since empty
      // strings are meaningful there.
      rv = InsertTextWithTransaction(*doc, *inString, compositionStartPoint);
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "InsertTextWithTransaction() failed");
      return rv;
    }

    EditorRawDOMPoint compositionEndPoint = GetCompositionEndPoint();
    if (!compositionEndPoint.IsSet()) {
      compositionEndPoint = compositionStartPoint;
    }
    WSRunObject wsObj(this, compositionStartPoint, compositionEndPoint);
    rv = wsObj.InsertText(*doc, *inString);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    compositionStartPoint = GetCompositionStartPoint();
    compositionEndPoint = GetCompositionEndPoint();
    if (NS_WARN_IF(!compositionStartPoint.IsSet()) ||
        NS_WARN_IF(!compositionEndPoint.IsSet())) {
      // Mutation event listener has changed the DOM tree...
      return NS_OK;
    }
    rv = TopLevelEditSubActionDataRef().mChangedRange->SetStartAndEnd(
        compositionStartPoint.ToRawRangeBoundary(),
        compositionEndPoint.ToRawRangeBoundary());
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "Faliled to set mChangedRange to composing range");
    return rv;
  }

  MOZ_ASSERT(aEditSubAction == EditSubAction::eInsertText);

  // find where we are
  EditorDOMPoint currentPoint(pointToInsert);

  // is our text going to be PREformatted?
  // We remember this so that we know how to handle tabs.
  bool isPRE = EditorBase::IsPreformatted(pointToInsert.GetContainer());

  // turn off the edit listener: we know how to
  // build the "doc changed range" ourselves, and it's
  // must faster to do it once here than to track all
  // the changes one at a time.
  AutoRestore<bool> disableListener(
      EditSubActionDataRef().mAdjustChangedRangeFromListener);
  EditSubActionDataRef().mAdjustChangedRangeFromListener = false;

  // don't change my selection in subtransactions
  AutoTransactionsConserveSelection dontChangeMySelection(*this);
  nsAutoString tString(*inString);
  const char16_t* unicodeBuf = tString.get();
  int32_t pos = 0;
  NS_NAMED_LITERAL_STRING(newlineStr, LFSTR);

  {
    AutoTrackDOMPoint tracker(RangeUpdaterRef(), &pointToInsert);

    // for efficiency, break out the pre case separately.  This is because
    // its a lot cheaper to search the input string for only newlines than
    // it is to search for both tabs and newlines.
    if (isPRE || IsPlaintextEditor()) {
      while (unicodeBuf && pos != -1 &&
             pos < static_cast<int32_t>(inString->Length())) {
        int32_t oldPos = pos;
        int32_t subStrLen;
        pos = tString.FindChar(nsCRT::LF, oldPos);

        if (pos != -1) {
          subStrLen = pos - oldPos;
          // if first char is newline, then use just it
          if (!subStrLen) {
            subStrLen = 1;
          }
        } else {
          subStrLen = tString.Length() - oldPos;
          pos = tString.Length();
        }

        nsDependentSubstring subStr(tString, oldPos, subStrLen);

        // is it a return?
        if (subStr.Equals(newlineStr)) {
          RefPtr<Element> brElement =
              InsertBRElementWithTransaction(currentPoint, nsIEditor::eNone);
          if (NS_WARN_IF(Destroyed())) {
            return NS_ERROR_EDITOR_DESTROYED;
          }
          if (NS_WARN_IF(!brElement)) {
            return NS_ERROR_FAILURE;
          }
          pos++;
          if (brElement->GetNextSibling()) {
            pointToInsert.Set(brElement->GetNextSibling());
          } else {
            pointToInsert.SetToEndOf(currentPoint.GetContainer());
          }
          // XXX In most cases, pointToInsert and currentPoint are same here.
          //     But if the <br> element has been moved to different point by
          //     mutation observer, those points become different.
          currentPoint.Set(brElement);
          DebugOnly<bool> advanced = currentPoint.AdvanceOffset();
          NS_WARNING_ASSERTION(
              advanced, "Failed to advance offset after the new <br> element");
          NS_WARNING_ASSERTION(currentPoint == pointToInsert,
                               "Perhaps, <br> element position has been moved "
                               "to different point "
                               "by mutation observer");
        } else {
          EditorRawDOMPoint pointAfterInsertedString;
          rv = InsertTextWithTransaction(*doc, subStr,
                                         EditorRawDOMPoint(currentPoint),
                                         &pointAfterInsertedString);
          if (NS_WARN_IF(Destroyed())) {
            return NS_ERROR_EDITOR_DESTROYED;
          }
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
          currentPoint = pointAfterInsertedString;
          pointToInsert = pointAfterInsertedString;
        }
      }
    } else {
      NS_NAMED_LITERAL_STRING(tabStr, "\t");
      NS_NAMED_LITERAL_STRING(spacesStr, "    ");
      char specialChars[] = {TAB, nsCRT::LF, 0};
      while (unicodeBuf && pos != -1 &&
             pos < static_cast<int32_t>(inString->Length())) {
        int32_t oldPos = pos;
        int32_t subStrLen;
        pos = tString.FindCharInSet(specialChars, oldPos);

        if (pos != -1) {
          subStrLen = pos - oldPos;
          // if first char is newline, then use just it
          if (!subStrLen) {
            subStrLen = 1;
          }
        } else {
          subStrLen = tString.Length() - oldPos;
          pos = tString.Length();
        }

        nsDependentSubstring subStr(tString, oldPos, subStrLen);
        WSRunObject wsObj(this, currentPoint);

        // is it a tab?
        if (subStr.Equals(tabStr)) {
          EditorRawDOMPoint pointAfterInsertedSpaces;
          rv = wsObj.InsertText(*doc, spacesStr, &pointAfterInsertedSpaces);
          if (NS_WARN_IF(Destroyed())) {
            return NS_ERROR_EDITOR_DESTROYED;
          }
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
          pos++;
          MOZ_ASSERT(pointAfterInsertedSpaces.IsSet());
          currentPoint = pointAfterInsertedSpaces;
          pointToInsert = pointAfterInsertedSpaces;
        }
        // is it a return?
        else if (subStr.Equals(newlineStr)) {
          RefPtr<Element> newBRElement =
              wsObj.InsertBreak(MOZ_KnownLive(*SelectionRefPtr()), currentPoint,
                                nsIEditor::eNone);
          if (NS_WARN_IF(Destroyed())) {
            return NS_ERROR_EDITOR_DESTROYED;
          }
          if (NS_WARN_IF(!newBRElement)) {
            return NS_ERROR_FAILURE;
          }
          pos++;
          if (newBRElement->GetNextSibling()) {
            pointToInsert.Set(newBRElement->GetNextSibling());
          } else {
            pointToInsert.SetToEndOf(currentPoint.GetContainer());
          }
          currentPoint.Set(newBRElement);
          DebugOnly<bool> advanced = currentPoint.AdvanceOffset();
          NS_WARNING_ASSERTION(
              advanced, "Failed to advance offset to after the new <br> node");
          // XXX If the newBRElement has been moved or removed by mutation
          //     observer, we hit this assert.  We need to check if
          //     newBRElement is in expected point, though, we must have
          //     a lot of same bugs...
          NS_WARNING_ASSERTION(
              currentPoint == pointToInsert,
              "Perhaps, newBRElement has been moved or removed unexpectedly");
        } else {
          EditorRawDOMPoint pointAfterInsertedString;
          rv = wsObj.InsertText(*doc, subStr, &pointAfterInsertedString);
          if (NS_WARN_IF(Destroyed())) {
            return NS_ERROR_EDITOR_DESTROYED;
          }
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
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
  SelectionRefPtr()->SetInterlinePosition(false, ignoredError);
  NS_WARNING_ASSERTION(!ignoredError.Failed(),
                       "Failed to unset interline position");

  if (currentPoint.IsSet()) {
    IgnoredErrorResult ignoredError;
    SelectionRefPtr()->Collapse(currentPoint, ignoredError);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(!ignoredError.Failed(),
                         "Failed to collapse at current point");
  }

  // manually update the doc changed range so that AfterEdit will clean up
  // the correct portion of the document.
  if (currentPoint.IsSet()) {
    rv = TopLevelEditSubActionDataRef().mChangedRange->SetStartAndEnd(
        pointToInsert.ToRawRangeBoundary(), currentPoint.ToRawRangeBoundary());
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to set mChangedRange");
    return rv;
  }

  rv = TopLevelEditSubActionDataRef().mChangedRange->CollapseTo(pointToInsert);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to collapse mChangedRange");
  return rv;
}

bool HTMLEditor::CanContainParagraph(Element& aElement) const {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (CanContainTag(aElement, *nsGkAtoms::p)) {
    return true;
  }

  // Even if the element cannot have a <p> element as a child, it can contain
  // <p> element as a descendant if it's one of the following elements.
  if (aElement.IsAnyOfHTMLElements(nsGkAtoms::ol, nsGkAtoms::ul, nsGkAtoms::dl,
                                   nsGkAtoms::table, nsGkAtoms::thead,
                                   nsGkAtoms::tbody, nsGkAtoms::tfoot,
                                   nsGkAtoms::tr)) {
    return true;
  }

  // XXX Otherwise, Chromium checks the CSS box is a block, but we don't do it
  //     for now.
  return false;
}

EditActionResult HTMLEditRules::WillInsertParagraphSeparator() {
  MOZ_ASSERT(IsEditorDataAvailable());

  // If the selection isn't collapsed, delete it.
  if (!SelectionRefPtr()->IsCollapsed()) {
    nsresult rv =
        MOZ_KnownLive(HTMLEditorRef())
            .DeleteSelectionAsSubAction(nsIEditor::eNone, nsIEditor::eStrip);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return EditActionIgnored(rv);
    }
  }

  // FYI: Ignore cancel result of WillInsert().
  nsresult rv = MOZ_KnownLive(HTMLEditorRef()).WillInsert();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "WillInsert() failed");

  // Split any mailcites in the way.  Should we abort this if we encounter
  // table cell boundaries?
  if (IsMailEditor()) {
    EditorDOMPoint pointToSplit(EditorBase::GetStartPoint(*SelectionRefPtr()));
    if (NS_WARN_IF(!pointToSplit.IsSet())) {
      return EditActionIgnored(NS_ERROR_FAILURE);
    }

    EditActionResult result =
        MOZ_KnownLive(HTMLEditorRef()).SplitMailCiteElements(pointToSplit);
    if (NS_WARN_IF(result.Failed())) {
      return result;
    }
    if (result.Handled()) {
      return result;
    }
  }

  // Smart splitting rules
  nsRange* firstRange = SelectionRefPtr()->GetRangeAt(0);
  if (NS_WARN_IF(!firstRange)) {
    return EditActionIgnored(NS_ERROR_FAILURE);
  }

  EditorDOMPoint atStartOfSelection(firstRange->StartRef());
  if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
    return EditActionIgnored(NS_ERROR_FAILURE);
  }
  MOZ_ASSERT(atStartOfSelection.IsSetAndValid());

  // Do nothing if the node is read-only
  if (!HTMLEditorRef().IsModifiableNode(*atStartOfSelection.GetContainer())) {
    return EditActionCanceled();
  }

  // If the active editing host is an inline element, or if the active editing
  // host is the block parent itself and we're configured to use <br> as a
  // paragraph separator, just append a <br>.
  RefPtr<Element> host = HTMLEditorRef().GetActiveEditingHost();
  if (NS_WARN_IF(!host)) {
    return EditActionIgnored(NS_ERROR_FAILURE);
  }

  // Look for the nearest parent block.  However, don't return error even if
  // there is no block parent here because in such case, i.e., editing host
  // is an inline element, we should insert <br> simply.
  RefPtr<Element> blockParent =
      HTMLEditor::GetBlock(*atStartOfSelection.GetContainer(), host);

  ParagraphSeparator separator = HTMLEditorRef().GetDefaultParagraphSeparator();
  bool insertBRElement;
  // If there is no block parent in the editing host, i.e., the editing host
  // itself is also a non-block element, we should insert a <br> element.
  if (!blockParent) {
    // XXX Chromium checks if the CSS box of the editing host is block.
    insertBRElement = true;
  }
  // If only the editing host is block, and the default paragraph separator
  // is <br> or the editing host cannot contain a <p> element, we should
  // insert a <br> element.
  else if (host == blockParent) {
    insertBRElement = separator == ParagraphSeparator::br ||
                      !HTMLEditorRef().CanContainParagraph(*host);
  }
  // If the nearest block parent is a single-line container declared in
  // the execCommand spec and not the editing host, we should separate the
  // block even if the default paragraph separator is <br> element.
  else if (HTMLEditUtils::IsSingleLineContainer(*blockParent)) {
    insertBRElement = false;
  }
  // Otherwise, unless there is no block ancestor which can contain <p>
  // element, we shouldn't insert a <br> element here.
  else {
    insertBRElement = true;
    for (Element* blockAncestor = blockParent; blockAncestor && insertBRElement;
         blockAncestor = HTMLEditor::GetBlockNodeParent(blockAncestor, host)) {
      insertBRElement = !HTMLEditorRef().CanContainParagraph(*blockAncestor);
    }
  }

  // If we cannot insert a <p>/<div> element at the selection, we should insert
  // a <br> element instead.
  if (insertBRElement) {
    nsresult rv =
        MOZ_KnownLive(HTMLEditorRef()).InsertBRElement(atStartOfSelection);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return EditActionIgnored(rv);
    }
    return EditActionHandled();
  }

  if (host == blockParent && separator != ParagraphSeparator::br) {
    // Insert a new block first
    MOZ_ASSERT(separator == ParagraphSeparator::div ||
               separator == ParagraphSeparator::p);
    // MakeBasicBlock() creates AutoSelectionRestorer.
    // Therefore, even if it returns NS_OK, editor might have been destroyed
    // at restoring Selection.
    nsresult rv =
        MakeBasicBlock(MOZ_KnownLive(ParagraphSeparatorElement(separator)));
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED) ||
        NS_WARN_IF(!CanHandleEditAction())) {
      return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
    }
    // We warn on failure, but don't handle it, because it might be harmless.
    // Instead we just check that a new block was actually created.
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditRules::MakeBasicBlock() failed");

    firstRange = SelectionRefPtr()->GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return EditActionIgnored(NS_ERROR_FAILURE);
    }

    atStartOfSelection = firstRange->StartRef();
    if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
      return EditActionIgnored(NS_ERROR_FAILURE);
    }
    MOZ_ASSERT(atStartOfSelection.IsSetAndValid());

    blockParent =
        HTMLEditor::GetBlock(*atStartOfSelection.GetContainer(), host);
    if (NS_WARN_IF(!blockParent)) {
      return EditActionIgnored(NS_ERROR_UNEXPECTED);
    }
    if (NS_WARN_IF(blockParent == host)) {
      // Didn't create a new block for some reason, fall back to <br>
      rv = MOZ_KnownLive(HTMLEditorRef()).InsertBRElement(atStartOfSelection);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return EditActionIgnored(rv);
      }
      return EditActionHandled();
    }
    // Now, mNewBlockElement is last created block element for wrapping inline
    // elements around the caret position and AfterEditInner() will move
    // caret into it.  However, it may be different from block parent of
    // the caret position.  E.g., MakeBasicBlock() may wrap following
    // inline elements of a <br> element which is next sibling of container
    // of the caret.  So, we need to adjust mNewBlockElement here for avoiding
    // jumping caret to odd position.
    HTMLEditorRef().TopLevelEditSubActionDataRef().mNewBlockElement =
        blockParent;
  }

  // If block is empty, populate with br.  (For example, imagine a div that
  // contains the word "text".  The user selects "text" and types return.
  // "Text" is deleted leaving an empty block.  We want to put in one br to
  // make block have a line.  Then code further below will put in a second br.)
  if (IsEmptyBlockElement(*blockParent, IgnoreSingleBR::eNo)) {
    AutoEditorDOMPointChildInvalidator lockOffset(atStartOfSelection);
    EditorDOMPoint endOfBlockParent;
    endOfBlockParent.SetToEndOf(blockParent);
    RefPtr<Element> brElement =
        MOZ_KnownLive(HTMLEditorRef())
            .InsertBRElementWithTransaction(endOfBlockParent);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
    }
    if (NS_WARN_IF(!brElement)) {
      return EditActionIgnored(NS_ERROR_FAILURE);
    }
  }

  nsCOMPtr<Element> listItem = IsInListItem(blockParent);
  if (listItem && listItem != host) {
    nsresult rv = ReturnInListItem(
        *listItem, MOZ_KnownLive(*atStartOfSelection.GetContainer()),
        atStartOfSelection.Offset());
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "Failed to insert break into list item");
    return EditActionHandled();
  }

  if (HTMLEditUtils::IsHeader(*blockParent)) {
    // Headers: close (or split) header
    nsresult rv = ReturnInHeader(
        *blockParent, MOZ_KnownLive(*atStartOfSelection.GetContainer()),
        atStartOfSelection.Offset());
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "Failed to handle insertParagraph in the heading element");
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
       blockParent->IsHTMLElement(nsGkAtoms::p)) ||
      (separator != ParagraphSeparator::br &&
       blockParent->IsAnyOfHTMLElements(nsGkAtoms::p, nsGkAtoms::div))) {
    AutoEditorDOMPointChildInvalidator lockOffset(atStartOfSelection);
    // Paragraphs: special rules to look for <br>s
    EditActionResult result = ReturnInParagraph(*blockParent);
    if (NS_WARN_IF(result.Failed())) {
      return result;
    }
    if (result.Handled()) {
      // Now, atStartOfSelection may be invalid because the left paragraph
      // may have less children than its offset.  For avoiding warnings of
      // validation of EditorDOMPoint, we should not touch it anymore.
      lockOffset.Cancel();
      return result;
    }
    // Fall through, if ReturnInParagraph() didn't handle it.
    MOZ_ASSERT(!result.Canceled(),
               "ReturnInParagraph canceled this edit action, "
               "WillInsertBreak() needs to handle such case");
  }

  // If nobody handles this edit action, let's insert new <br> at the selection.
  rv = MOZ_KnownLive(HTMLEditorRef()).InsertBRElement(atStartOfSelection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditActionIgnored(rv);
  }
  return EditActionHandled();
}

nsresult HTMLEditor::InsertBRElement(const EditorDOMPoint& aPointToBreak) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!aPointToBreak.IsSet())) {
    return NS_ERROR_INVALID_ARG;
  }

  bool brElementIsAfterBlock = false;
  bool brElementIsBeforeBlock = false;

  // First, insert a <br> element.
  RefPtr<Element> brElement;
  if (IsPlaintextEditor()) {
    brElement = InsertBRElementWithTransaction(aPointToBreak);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(!brElement)) {
      return NS_ERROR_FAILURE;
    }
  } else {
    EditorDOMPoint pointToBreak(aPointToBreak);
    WSRunObject wsObj(this, pointToBreak);
    WSType wsType;
    wsObj.PriorVisibleNode(pointToBreak, &wsType);
    if (wsType & WSType::block) {
      brElementIsAfterBlock = true;
    }
    wsObj.NextVisibleNode(pointToBreak, &wsType);
    if (wsType & WSType::block) {
      brElementIsBeforeBlock = true;
    }
    // If the container of the break is a link, we need to split it and
    // insert new <br> between the split links.
    RefPtr<Element> linkNode =
        HTMLEditor::GetLinkElement(pointToBreak.GetContainer());
    if (linkNode) {
      SplitNodeResult splitLinkNodeResult = SplitNodeDeepWithTransaction(
          *linkNode, pointToBreak, SplitAtEdges::eDoNotCreateEmptyContainer);
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(splitLinkNodeResult.Failed())) {
        return splitLinkNodeResult.Rv();
      }
      pointToBreak = splitLinkNodeResult.SplitPoint();
    }
    brElement = wsObj.InsertBreak(MOZ_KnownLive(*SelectionRefPtr()),
                                  pointToBreak, nsIEditor::eNone);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(!brElement)) {
      return NS_ERROR_FAILURE;
    }
  }

  // If the <br> element has already been removed from the DOM tree by a
  // mutation observer, don't continue handling this.
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
    ErrorResult error;
    SelectionRefPtr()->SetInterlinePosition(true, error);
    NS_WARNING_ASSERTION(!error.Failed(), "Failed to set interline position");
    EditorRawDOMPoint point(brElement);
    error = NS_OK;
    SelectionRefPtr()->Collapse(point, error);
    if (NS_WARN_IF(Destroyed())) {
      error.SuppressException();
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
    return NS_OK;
  }

  EditorDOMPoint afterBRElement(brElement);
  DebugOnly<bool> advanced = afterBRElement.AdvanceOffset();
  NS_WARNING_ASSERTION(advanced,
                       "Failed to advance offset after the new <br> element");
  WSRunObject wsObj(this, afterBRElement);
  nsCOMPtr<nsINode> maybeSecondBRNode;
  WSType wsType;
  wsObj.NextVisibleNode(afterBRElement, address_of(maybeSecondBRNode), nullptr,
                        &wsType);
  if (wsType == WSType::br) {
    // The next thing after the break we inserted is another break.  Move the
    // second break to be the first break's sibling.  This will prevent them
    // from being in different inline nodes, which would break
    // SetInterlinePosition().  It will also assure that if the user clicks
    // away and then clicks back on their new blank line, they will still get
    // the style from the line above.
    EditorDOMPoint atSecondBRElement(maybeSecondBRNode);
    if (brElement->GetNextSibling() != maybeSecondBRNode) {
      nsresult rv = MoveNodeWithTransaction(
          MOZ_KnownLive(*maybeSecondBRNode->AsContent()), afterBRElement);
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
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
  ErrorResult error;
  SelectionRefPtr()->SetInterlinePosition(
      !(nextSiblingOfBRElement &&
        HTMLEditor::NodeIsBlockStatic(*nextSiblingOfBRElement)),
      error);
  NS_WARNING_ASSERTION(!error.Failed(),
                       "Failed to set or unset interline position");
  error = NS_OK;
  SelectionRefPtr()->Collapse(afterBRElement, error);
  if (NS_WARN_IF(Destroyed())) {
    error.SuppressException();
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
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
  WSRunObject wsObj(this, pointToSplit);
  nsCOMPtr<nsINode> visNode;
  WSType wsType;
  wsObj.NextVisibleNode(pointToSplit, address_of(visNode), nullptr, &wsType);
  // If selection start point is before a break and it's inside the mailquote,
  // let's split it after the visible node.
  if (wsType == WSType::br && visNode != citeNode &&
      citeNode->Contains(visNode)) {
    pointToSplit.Set(visNode);
    DebugOnly<bool> advanced = pointToSplit.AdvanceOffset();
    NS_WARNING_ASSERTION(advanced,
                         "Failed to advance offset to after the visible node");
  }

  if (NS_WARN_IF(!pointToSplit.GetContainerAsContent())) {
    return EditActionIgnored(NS_ERROR_FAILURE);
  }

  SplitNodeResult splitCiteNodeResult = SplitNodeDeepWithTransaction(
      *citeNode, pointToSplit, SplitAtEdges::eDoNotCreateEmptyContainer);
  if (NS_WARN_IF(Destroyed())) {
    return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
  }
  if (NS_WARN_IF(splitCiteNodeResult.Failed())) {
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
  nsIContent* previousNodeOfSplitPoint = splitCiteNodeResult.GetPreviousNode();
  if (previousNodeOfSplitPoint &&
      previousNodeOfSplitPoint->IsHTMLElement(nsGkAtoms::span) &&
      previousNodeOfSplitPoint->GetPrimaryFrame() &&
      previousNodeOfSplitPoint->GetPrimaryFrame()->IsBlockFrameOrSubclass()) {
    nsCOMPtr<nsINode> lastChild = previousNodeOfSplitPoint->GetLastChild();
    if (lastChild && !lastChild->IsHTMLElement(nsGkAtoms::br)) {
      // We ignore the result here.
      EditorDOMPoint endOfPreviousNodeOfSplitPoint;
      endOfPreviousNodeOfSplitPoint.SetToEndOf(previousNodeOfSplitPoint);
      RefPtr<Element> invisibleBrElement =
          InsertBRElementWithTransaction(endOfPreviousNodeOfSplitPoint);
      if (NS_WARN_IF(Destroyed())) {
        return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(invisibleBrElement,
                           "Failed to create an invisible <br> element");
    }
  }

  // In most cases, <br> should be inserted after current cite.  However, if
  // left cite hasn't been created because the split point was start of the
  // cite node, <br> should be inserted before the current cite.
  EditorDOMPoint pointToInsertBrNode(splitCiteNodeResult.SplitPoint());
  RefPtr<Element> brElement =
      InsertBRElementWithTransaction(pointToInsertBrNode);
  if (NS_WARN_IF(Destroyed())) {
    return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
  }
  if (NS_WARN_IF(!brElement)) {
    return EditActionIgnored(NS_ERROR_FAILURE);
  }
  // Now, offset of pointToInsertBrNode is invalid.  Let's clear it.
  pointToInsertBrNode.Clear();

  // Want selection before the break, and on same line.
  EditorDOMPoint atBrNode(brElement);
  Unused << atBrNode.Offset();  // Needs offset after collapsing the selection.
  ErrorResult error;
  SelectionRefPtr()->SetInterlinePosition(true, error);
  NS_WARNING_ASSERTION(!error.Failed(), "Failed to set interline position");
  error = NS_OK;
  SelectionRefPtr()->Collapse(atBrNode, error);
  if (NS_WARN_IF(Destroyed())) {
    error.SuppressException();
    return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
  }
  if (NS_WARN_IF(error.Failed())) {
    return EditActionIgnored(error.StealNSResult());
  }

  // if citeNode wasn't a block, we might also want another break before it.
  // We need to examine the content both before the br we just added and also
  // just after it.  If we don't have another br or block boundary adjacent,
  // then we will need a 2nd br added to achieve blank line that user expects.
  if (HTMLEditor::NodeIsInlineStatic(*citeNode)) {
    // Use DOM point which we tried to collapse to.
    EditorDOMPoint pointToCreateNewBrNode(atBrNode.GetContainer(),
                                          atBrNode.Offset());

    WSRunObject wsObj(this, pointToCreateNewBrNode);
    WSType wsType;
    wsObj.PriorVisibleNode(pointToCreateNewBrNode, nullptr, nullptr, &wsType);
    if (wsType == WSType::normalWS || wsType == WSType::text ||
        wsType == WSType::special) {
      EditorRawDOMPoint pointAfterNewBrNode(pointToCreateNewBrNode);
      DebugOnly<bool> advanced = pointAfterNewBrNode.AdvanceOffset();
      NS_WARNING_ASSERTION(advanced,
                           "Failed to advance offset after the <br> node");
      WSRunObject wsObjAfterBR(this, pointAfterNewBrNode);
      wsObjAfterBR.NextVisibleNode(pointAfterNewBrNode, &wsType);
      if (wsType == WSType::normalWS || wsType == WSType::text ||
          wsType == WSType::special ||
          // In case we're at the very end.
          wsType == WSType::thisBlock) {
        brElement = InsertBRElementWithTransaction(pointToCreateNewBrNode);
        if (NS_WARN_IF(Destroyed())) {
          return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
        }
        if (NS_WARN_IF(!brElement)) {
          return EditActionIgnored(NS_ERROR_FAILURE);
        }
        // Now, those points may be invalid.
        pointToCreateNewBrNode.Clear();
        pointAfterNewBrNode.Clear();
      }
    }
  }

  // delete any empty cites
  bool bEmptyCite = false;
  if (previousNodeOfSplitPoint) {
    nsresult rv =
        IsEmptyNode(previousNodeOfSplitPoint, &bEmptyCite, true, false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return EditActionIgnored(rv);
    }
    if (bEmptyCite) {
      rv = DeleteNodeWithTransaction(MOZ_KnownLive(*previousNodeOfSplitPoint));
      if (NS_WARN_IF(Destroyed())) {
        return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return EditActionIgnored(rv);
      }
    }
  }

  if (citeNode) {
    nsresult rv = IsEmptyNode(citeNode, &bEmptyCite, true, false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return EditActionIgnored(rv);
    }
    if (bEmptyCite) {
      rv = DeleteNodeWithTransaction(*citeNode);
      if (NS_WARN_IF(Destroyed())) {
        return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return EditActionIgnored(rv);
      }
    }
  }

  return EditActionHandled();
}

nsresult HTMLEditRules::WillDeleteSelection(
    nsIEditor::EDirection aAction, nsIEditor::EStripWrappers aStripWrappers,
    bool* aCancel, bool* aHandled) {
  MOZ_ASSERT(IsEditorDataAvailable());
  MOZ_ASSERT(aStripWrappers == nsIEditor::eStrip ||
             aStripWrappers == nsIEditor::eNoStrip);

  if (NS_WARN_IF(!aCancel) || NS_WARN_IF(!aHandled)) {
    return NS_ERROR_INVALID_ARG;
  }
  // Initialize out params
  *aCancel = false;
  *aHandled = false;

  // Remember that we did a selection deletion.  Used by
  // CreateStyleForInsertText()
  HTMLEditorRef().TopLevelEditSubActionDataRef().mDidDeleteSelection = true;

  // If there is only padding <br> element for empty editor, cancel the
  // operation.
  if (HTMLEditorRef().HasPaddingBRElementForEmptyEditor()) {
    *aCancel = true;
    return NS_OK;
  }

  // First check for table selection mode.  If so, hand off to table editor.
  ErrorResult error;
  RefPtr<Element> cellElement =
      HTMLEditorRef().GetFirstSelectedTableCellElement(error);
  if (cellElement) {
    error.SuppressException();
    nsresult rv =
        MOZ_KnownLive(HTMLEditorRef()).DeleteTableCellContentsWithTransaction();
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    *aHandled = true;
    return rv;
  }
  nsresult rv = error.StealNSResult();
  cellElement = nullptr;

  // origCollapsed is used later to determine whether we should join blocks. We
  // don't really care about bCollapsed because it will be modified by
  // ExtendSelectionForDelete later. TryToJoinBlocksWithTransaction() should
  // happen if the original selection is collapsed and the cursor is at the end
  // of a block element, in which case ExtendSelectionForDelete would always
  // make the selection not collapsed.
  bool join = false;
  bool origCollapsed = SelectionRefPtr()->IsCollapsed();

  if (origCollapsed) {
    EditorDOMPoint startPoint(EditorBase::GetStartPoint(*SelectionRefPtr()));
    if (NS_WARN_IF(!startPoint.IsSet())) {
      return NS_ERROR_FAILURE;
    }

    // If we are inside an empty block, delete it.
    RefPtr<Element> host = HTMLEditorRef().GetActiveEditingHost();
    if (NS_WARN_IF(!host)) {
      return NS_ERROR_FAILURE;
    }

    {
      AutoEditorDOMPointChildInvalidator lockOffset(startPoint);

      rv = MaybeDeleteTopMostEmptyAncestor(
          MOZ_KnownLive(*startPoint.GetContainer()), *host, aAction, aHandled);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      if (*aHandled) {
        return NS_OK;
      }
    }

    // Test for distance between caret and text that will be deleted
    rv = CheckBidiLevelForDeletion(startPoint, aAction, aCancel);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    if (*aCancel) {
      return NS_OK;
    }

    // ExtendSelectionForDelete will use selection controller to set
    // selection for delete.  But if focus event doesn't receive yet,
    // ancestor isn't set.  So we must set root eleement of editor to
    // ancestor.
    AutoSetTemporaryAncestorLimiter autoSetter(
        HTMLEditorRef(), *SelectionRefPtr(), *startPoint.GetContainer());

    rv = HTMLEditorRef().ExtendSelectionForDelete(&aAction);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // We should delete nothing.
    if (aAction == nsIEditor::eNone) {
      return NS_OK;
    }
  }

  if (SelectionRefPtr()->IsCollapsed()) {
    // ExtendSelectionForDelete() won't change the selection.

    EditorDOMPoint startPoint(EditorBase::GetStartPoint(*SelectionRefPtr()));
    if (NS_WARN_IF(!startPoint.IsSet())) {
      return NS_ERROR_FAILURE;
    }

    // What's in the direction we are deleting?
    WSRunObject wsObj(&HTMLEditorRef(), startPoint);
    nsCOMPtr<nsINode> visNode;
    int32_t visOffset;
    WSType wsType;

    // Find next visible node
    if (aAction == nsIEditor::eNext) {
      wsObj.NextVisibleNode(startPoint, address_of(visNode), &visOffset,
                            &wsType);
    } else {
      wsObj.PriorVisibleNode(startPoint, address_of(visNode), &visOffset,
                             &wsType);
    }

    if (!visNode) {
      // Can't find anything to delete!
      *aCancel = true;
      // XXX This is the result of
      //     HTMLEditorRef().GetFirstSelectedTableCellElement().
      //     The value could be both an error and NS_OK.
      return rv;
    }

    if (wsType == WSType::normalWS) {
      // We found some visible ws to delete.  Let ws code handle it.
      *aHandled = true;
      if (aAction == nsIEditor::eNext) {
        rv = wsObj.DeleteWSForward();
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      } else {
        rv = wsObj.DeleteWSBackward();
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
      rv = InsertBRIfNeeded();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      return NS_OK;
    }

    if (wsType == WSType::text) {
      // Found normal text to delete.
      OwningNonNull<Text> nodeAsText = *visNode->GetAsText();
      int32_t so = visOffset;
      int32_t eo = visOffset + 1;
      if (aAction == nsIEditor::ePrevious) {
        if (!so) {
          return NS_ERROR_UNEXPECTED;
        }
        so--;
        eo--;
        // Bug 1068979: delete both codepoints if surrogate pair
        if (so > 0) {
          const nsTextFragment* text = &nodeAsText->TextFragment();
          if (text->IsLowSurrogateFollowingHighSurrogateAt(so)) {
            so--;
          }
        }
      } else {
        RefPtr<nsRange> range = SelectionRefPtr()->GetRangeAt(0);
        if (NS_WARN_IF(!range)) {
          return NS_ERROR_FAILURE;
        }

        NS_ASSERTION(range->GetStartContainer() == visNode,
                     "selection start not in visNode");
        NS_ASSERTION(range->GetEndContainer() == visNode,
                     "selection end not in visNode");

        so = range->StartOffset();
        eo = range->EndOffset();
      }
      rv = WSRunObject::PrepareToDeleteRange(MOZ_KnownLive(&HTMLEditorRef()),
                                             address_of(visNode), &so,
                                             address_of(visNode), &eo);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      *aHandled = true;
      rv = MOZ_KnownLive(HTMLEditorRef())
               .DeleteTextWithTransaction(nodeAsText, std::min(so, eo),
                                          DeprecatedAbs(eo - so));
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
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

      rv = DeleteNodeIfCollapsedText(nodeAsText);
      if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to remove collapsed text");

      rv = InsertBRIfNeeded();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      // Remember that we did a ranged delete for the benefit of
      // AfterEditInner().
      HTMLEditorRef()
          .TopLevelEditSubActionDataRef()
          .mDidDeleteNonCollapsedRange = true;

      return NS_OK;
    }

    if (wsType == WSType::special || wsType == WSType::br ||
        visNode->IsHTMLElement(nsGkAtoms::hr)) {
      // If the void element is editing host, we should do nothing.
      if (visNode == wsObj.GetEditingHost()) {
        *aHandled = true;
        return NS_OK;
      }

      // Short circuit for invisible breaks.  delete them and recurse.
      if (visNode->IsHTMLElement(nsGkAtoms::br) &&
          !HTMLEditorRef().IsVisibleBRElement(visNode)) {
        rv = MOZ_KnownLive(HTMLEditorRef()).DeleteNodeWithTransaction(*visNode);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        rv = WillDeleteSelection(aAction, aStripWrappers, aCancel, aHandled);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        return NS_OK;
      }

      // Special handling for backspace when positioned after <hr>
      if (aAction == nsIEditor::ePrevious &&
          visNode->IsHTMLElement(nsGkAtoms::hr)) {
        // Only if the caret is positioned at the end-of-hr-line position, we
        // want to delete the <hr>.
        //
        // In other words, we only want to delete, if our selection position
        // (indicated by startPoint) is the position directly
        // after the <hr>, on the same line as the <hr>.
        //
        // To detect this case we check:
        // startPoint's container == parentOfVisNode
        // and
        // startPoint's offset -1 == visNodeOffsetToVisNodeParent
        // and
        // interline position is false (left)
        //
        // In any other case we set the position to startPoint's container -1
        // and interlineposition to false, only moving the caret to the
        // end-of-hr-line position.
        bool moveOnly = true;

        EditorDOMPoint selPoint(visNode);

        ErrorResult err;
        bool interLineIsRight = SelectionRefPtr()->GetInterlinePosition(err);
        if (NS_WARN_IF(err.Failed())) {
          return err.StealNSResult();
        }

        if (startPoint.GetContainer() == selPoint.GetContainer() &&
            startPoint.Offset() - 1 == selPoint.Offset() && !interLineIsRight) {
          moveOnly = false;
        }

        if (moveOnly) {
          // Go to the position after the <hr>, but to the end of the <hr> line
          // by setting the interline position to left.
          DebugOnly<bool> advanced = selPoint.AdvanceOffset();
          NS_WARNING_ASSERTION(advanced,
                               "Failed to advance offset after <hr> element");

          {
            AutoEditorDOMPointChildInvalidator lockOffset(selPoint);

            IgnoredErrorResult ignoredError;
            SelectionRefPtr()->Collapse(selPoint, ignoredError);
            if (NS_WARN_IF(!CanHandleEditAction())) {
              return NS_ERROR_EDITOR_DESTROYED;
            }
            NS_WARNING_ASSERTION(
                !ignoredError.Failed(),
                "Failed to collapse selection at after the <hr>");
          }

          IgnoredErrorResult ignoredError;
          SelectionRefPtr()->SetInterlinePosition(false, ignoredError);
          NS_WARNING_ASSERTION(!ignoredError.Failed(),
                               "Failed to unset interline position");
          HTMLEditorRef()
              .TopLevelEditSubActionDataRef()
              .mDidExplicitlySetInterLine = true;
          *aHandled = true;

          // There is one exception to the move only case.  If the <hr> is
          // followed by a <br> we want to delete the <br>.

          WSType otherWSType;
          nsCOMPtr<nsINode> otherNode;

          wsObj.NextVisibleNode(startPoint, address_of(otherNode), nullptr,
                                &otherWSType);

          if (otherWSType == WSType::br) {
            // Delete the <br>
            if (NS_WARN_IF(!otherNode->IsContent())) {
              return NS_ERROR_FAILURE;
            }
            nsIContent* otherContent = otherNode->AsContent();
            rv = WSRunObject::PrepareToDeleteNode(
                MOZ_KnownLive(&HTMLEditorRef()), MOZ_KnownLive(otherContent));
            if (NS_WARN_IF(!CanHandleEditAction())) {
              return NS_ERROR_EDITOR_DESTROYED;
            }
            if (NS_WARN_IF(NS_FAILED(rv))) {
              return rv;
            }
            rv = MOZ_KnownLive(HTMLEditorRef())
                     .DeleteNodeWithTransaction(MOZ_KnownLive(*otherContent));
            if (NS_WARN_IF(!CanHandleEditAction())) {
              return NS_ERROR_EDITOR_DESTROYED;
            }
            if (NS_WARN_IF(NS_FAILED(rv))) {
              return rv;
            }
          }

          return NS_OK;
        }
        // Else continue with normal delete code
      }

      if (NS_WARN_IF(!visNode->IsContent())) {
        return NS_ERROR_FAILURE;
      }
      // Found break or image, or hr.
      rv = WSRunObject::PrepareToDeleteNode(
          MOZ_KnownLive(&HTMLEditorRef()), MOZ_KnownLive(visNode->AsContent()));
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      // Remember sibling to visnode, if any
      nsCOMPtr<nsIContent> sibling =
          HTMLEditorRef().GetPriorHTMLSibling(visNode);
      // Delete the node, and join like nodes if appropriate
      rv = MOZ_KnownLive(HTMLEditorRef()).DeleteNodeWithTransaction(*visNode);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      // We did something, so let's say so.
      *aHandled = true;
      // Is there a prior node and are they siblings?
      nsCOMPtr<nsINode> stepbrother;
      if (sibling) {
        stepbrother = HTMLEditorRef().GetNextHTMLSibling(sibling);
      }
      // Are they both text nodes?  If so, join them!
      if (startPoint.GetContainer() == stepbrother &&
          startPoint.GetContainerAsText() && sibling->GetAsText()) {
        EditorDOMPoint pt;
        nsresult rv = JoinNearestEditableNodesWithTransaction(
            *sibling, MOZ_KnownLive(*startPoint.GetContainerAsContent()), &pt);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        if (NS_WARN_IF(!pt.IsSet())) {
          return NS_ERROR_FAILURE;
        }
        // Fix up selection
        ErrorResult error;
        SelectionRefPtr()->Collapse(pt, error);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          error.SuppressException();
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(error.Failed())) {
          return error.StealNSResult();
        }
      }
      rv = InsertBRIfNeeded();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      return NS_OK;
    }

    if (wsType == WSType::otherBlock) {
      // Make sure it's not a table element.  If so, cancel the operation
      // (translation: users cannot backspace or delete across table cells)
      if (HTMLEditUtils::IsTableElement(visNode)) {
        *aCancel = true;
        return NS_OK;
      }

      // Next to a block.  See if we are between a block and a br.  If so, we
      // really want to delete the br.  Else join content at selection to the
      // block.
      bool bDeletedBR = false;
      WSType otherWSType;
      nsCOMPtr<nsINode> otherNode;

      // Find node in other direction
      if (aAction == nsIEditor::eNext) {
        wsObj.PriorVisibleNode(startPoint, address_of(otherNode), nullptr,
                               &otherWSType);
      } else {
        wsObj.NextVisibleNode(startPoint, address_of(otherNode), nullptr,
                              &otherWSType);
      }

      // First find the adjacent node in the block
      nsCOMPtr<nsIContent> leafNode;
      nsCOMPtr<nsINode> leftNode, rightNode;
      if (aAction == nsIEditor::ePrevious) {
        leafNode = HTMLEditorRef().GetLastEditableLeaf(*visNode);
        leftNode = leafNode;
        rightNode = startPoint.GetContainer();
      } else {
        leafNode = HTMLEditorRef().GetFirstEditableLeaf(*visNode);
        leftNode = startPoint.GetContainer();
        rightNode = leafNode;
      }

      if (otherNode->IsHTMLElement(nsGkAtoms::br)) {
        rv = MOZ_KnownLive(HTMLEditorRef())
                 .DeleteNodeWithTransaction(*otherNode);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        // XXX Only in this case, setting "handled" to true only when it
        //     succeeds?
        *aHandled = true;
        bDeletedBR = true;
      }

      // Don't cross table boundaries
      if (leftNode && rightNode &&
          InDifferentTableElements(leftNode, rightNode)) {
        return NS_OK;
      }

      if (bDeletedBR) {
        // Put selection at edge of block and we are done.
        if (NS_WARN_IF(!leafNode)) {
          return NS_ERROR_FAILURE;
        }
        EditorDOMPoint newSel = GetGoodSelPointForNode(*leafNode, aAction);
        if (NS_WARN_IF(!newSel.IsSet())) {
          return NS_ERROR_FAILURE;
        }
        IgnoredErrorResult error;
        SelectionRefPtr()->Collapse(newSel, error);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        NS_WARNING_ASSERTION(
            !error.Failed(),
            "Failed to collapse selection at edge of the block");
        return NS_OK;
      }

      // Else we are joining content to block

      EditorDOMPoint selPoint(startPoint);
      {
        AutoTrackDOMPoint tracker(HTMLEditorRef().RangeUpdaterRef(), &selPoint);
        if (NS_WARN_IF(!leftNode) || NS_WARN_IF(!leftNode->IsContent()) ||
            NS_WARN_IF(!rightNode) || NS_WARN_IF(!rightNode->IsContent())) {
          return NS_ERROR_FAILURE;
        }
        EditActionResult ret = TryToJoinBlocksWithTransaction(
            MOZ_KnownLive(*leftNode->AsContent()),
            MOZ_KnownLive(*rightNode->AsContent()));
        *aHandled |= ret.Handled();
        *aCancel |= ret.Canceled();
        if (NS_WARN_IF(ret.Failed())) {
          return ret.Rv();
        }
      }

      // If TryToJoinBlocksWithTransaction() didn't handle it  and it's not
      // canceled, user may want to modify the start leaf node or the last leaf
      // node of the block.
      if (!*aHandled && !*aCancel && leafNode != startPoint.GetContainer()) {
        int32_t offset = aAction == nsIEditor::ePrevious
                             ? static_cast<int32_t>(leafNode->Length())
                             : 0;
        rv = SelectionRefPtr()->Collapse(leafNode, offset);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "Failed to collapse selection at the leaf node");
        rv = WillDeleteSelection(aAction, aStripWrappers, aCancel, aHandled);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        return NS_OK;
      }

      // Otherwise, we must have deleted the selection as user expected.
      IgnoredErrorResult ignored;
      SelectionRefPtr()->Collapse(selPoint, ignored);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(!ignored.Failed(),
                           "Failed to selection at deleted point");
      return NS_OK;
    }

    if (wsType == WSType::thisBlock) {
      // At edge of our block.  Look beside it and see if we can join to an
      // adjacent block

      // Make sure it's not a table element.  If so, cancel the operation
      // (translation: users cannot backspace or delete across table cells)
      if (HTMLEditUtils::IsTableElement(visNode)) {
        *aCancel = true;
        return NS_OK;
      }

      // First find the relevant nodes
      nsCOMPtr<nsINode> leftNode, rightNode;
      if (aAction == nsIEditor::ePrevious) {
        leftNode = HTMLEditorRef().GetPreviousEditableHTMLNode(*visNode);
        rightNode = startPoint.GetContainer();
      } else {
        rightNode = HTMLEditorRef().GetNextEditableHTMLNode(*visNode);
        leftNode = startPoint.GetContainer();
      }

      // Nothing to join
      if (!leftNode || !rightNode) {
        *aCancel = true;
        return NS_OK;
      }

      // Don't cross table boundaries -- cancel it
      if (InDifferentTableElements(leftNode, rightNode)) {
        *aCancel = true;
        return NS_OK;
      }

      EditorDOMPoint selPoint(startPoint);
      {
        AutoTrackDOMPoint tracker(HTMLEditorRef().RangeUpdaterRef(), &selPoint);
        if (NS_WARN_IF(!leftNode->IsContent()) ||
            NS_WARN_IF(!rightNode->IsContent())) {
          return NS_ERROR_FAILURE;
        }
        EditActionResult ret = TryToJoinBlocksWithTransaction(
            MOZ_KnownLive(*leftNode->AsContent()),
            MOZ_KnownLive(*rightNode->AsContent()));
        // This should claim that trying to join the block means that
        // this handles the action because the caller shouldn't do anything
        // anymore in this case.
        *aHandled = true;
        *aCancel |= ret.Canceled();
        if (NS_WARN_IF(ret.Failed())) {
          return ret.Rv();
        }
      }
      IgnoredErrorResult ignored;
      SelectionRefPtr()->Collapse(selPoint, ignored);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(!ignored.Failed(), "Failed to collapse selection");
      return NS_OK;
    }
  }

  // Else we have a non-collapsed selection.  First adjust the selection.
  rv = ExpandSelectionForDeletion();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Remember that we did a ranged delete for the benefit of AfterEditInner().
  HTMLEditorRef().TopLevelEditSubActionDataRef().mDidDeleteNonCollapsedRange =
      true;

  // Refresh start and end points
  nsRange* firstRange = SelectionRefPtr()->GetRangeAt(0);
  if (NS_WARN_IF(!firstRange)) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsINode> startNode = firstRange->GetStartContainer();
  if (NS_WARN_IF(!startNode)) {
    return NS_ERROR_FAILURE;
  }
  int32_t startOffset = firstRange->StartOffset();
  nsCOMPtr<nsINode> endNode = firstRange->GetEndContainer();
  if (NS_WARN_IF(!endNode)) {
    return NS_ERROR_FAILURE;
  }
  int32_t endOffset = firstRange->EndOffset();

  // Figure out if the endpoints are in nodes that can be merged.  Adjust
  // surrounding whitespace in preparation to delete selection.
  if (!IsPlaintextEditor()) {
    AutoTransactionsConserveSelection dontChangeMySelection(HTMLEditorRef());
    rv = WSRunObject::PrepareToDeleteRange(MOZ_KnownLive(&HTMLEditorRef()),
                                           address_of(startNode), &startOffset,
                                           address_of(endNode), &endOffset);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  {
    // Track location of where we are deleting
    AutoTrackDOMPoint startTracker(HTMLEditorRef().RangeUpdaterRef(),
                                   address_of(startNode), &startOffset);
    AutoTrackDOMPoint endTracker(HTMLEditorRef().RangeUpdaterRef(),
                                 address_of(endNode), &endOffset);
    // We are handling all ranged deletions directly now.
    *aHandled = true;

    if (endNode == startNode) {
      rv = MOZ_KnownLive(HTMLEditorRef())
               .DeleteSelectionWithTransaction(aAction, aStripWrappers);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      // Figure out mailcite ancestors
      RefPtr<Element> startCiteNode =
          HTMLEditorRef().GetMostAncestorMailCiteElement(*startNode);
      RefPtr<Element> endCiteNode =
          HTMLEditorRef().GetMostAncestorMailCiteElement(*endNode);

      // If we only have a mailcite at one of the two endpoints, set the
      // directionality of the deletion so that the selection will end up
      // outside the mailcite.
      if (startCiteNode && !endCiteNode) {
        aAction = nsIEditor::eNext;
      } else if (!startCiteNode && endCiteNode) {
        aAction = nsIEditor::ePrevious;
      }

      // Figure out block parents
      nsCOMPtr<Element> leftParent = HTMLEditor::GetBlock(*startNode);
      nsCOMPtr<Element> rightParent = HTMLEditor::GetBlock(*endNode);

      // Are endpoint block parents the same?  Use default deletion
      if (leftParent && leftParent == rightParent) {
        MOZ_KnownLive(HTMLEditorRef())
            .DeleteSelectionWithTransaction(aAction, aStripWrappers);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
      } else {
        // Deleting across blocks.  Are the blocks of same type?
        if (NS_WARN_IF(!leftParent) || NS_WARN_IF(!rightParent)) {
          return NS_ERROR_FAILURE;
        }

        // Are the blocks siblings?
        nsCOMPtr<nsINode> leftBlockParent = leftParent->GetParentNode();
        nsCOMPtr<nsINode> rightBlockParent = rightParent->GetParentNode();

        // MOOSE: this could conceivably screw up a table.. fix me.
        if (leftBlockParent == rightBlockParent &&
            HTMLEditorRef().AreNodesSameType(*leftParent, *rightParent) &&
            // XXX What's special about these three types of block?
            (leftParent->IsHTMLElement(nsGkAtoms::p) ||
             HTMLEditUtils::IsListItem(leftParent) ||
             HTMLEditUtils::IsHeader(*leftParent))) {
          // First delete the selection
          rv = MOZ_KnownLive(HTMLEditorRef())
                   .DeleteSelectionWithTransaction(aAction, aStripWrappers);
          if (NS_WARN_IF(!CanHandleEditAction())) {
            return NS_ERROR_EDITOR_DESTROYED;
          }
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
          // Join blocks
          EditorDOMPoint pt =
              MOZ_KnownLive(HTMLEditorRef())
                  .JoinNodesDeepWithTransaction(*leftParent, *rightParent);
          if (NS_WARN_IF(!CanHandleEditAction())) {
            return NS_ERROR_EDITOR_DESTROYED;
          }
          if (NS_WARN_IF(!pt.IsSet())) {
            return NS_ERROR_FAILURE;
          }
          // Fix up selection
          ErrorResult error;
          SelectionRefPtr()->Collapse(pt, error);
          if (NS_WARN_IF(!CanHandleEditAction())) {
            error.SuppressException();
            return NS_ERROR_EDITOR_DESTROYED;
          }
          if (NS_WARN_IF(error.Failed())) {
            return error.StealNSResult();
          }
          return NS_OK;
        }

        // Else blocks not same type, or not siblings.  Delete everything
        // except table elements.
        join = true;

        AutoRangeArray arrayOfRanges(SelectionRefPtr());
        for (auto& range : arrayOfRanges.mRanges) {
          // Build a list of nodes in the range
          nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
          TrivialFunctor functor;
          DOMSubtreeIterator iter;
          nsresult rv = iter.Init(*range);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
          iter.AppendList(functor, arrayOfNodes);

          // Now that we have the list, delete non-table elements
          int32_t listCount = arrayOfNodes.Length();
          for (int32_t j = 0; j < listCount; j++) {
            OwningNonNull<nsINode> node = arrayOfNodes[0];
            nsresult rv = DeleteElementsExceptTableRelatedElements(node);
            if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
              return NS_ERROR_EDITOR_DESTROYED;
            }
            NS_WARNING_ASSERTION(
                NS_SUCCEEDED(rv),
                "Failed to elements except table related elements");
            arrayOfNodes.RemoveElementAt(0);
            // If something visible is deleted, no need to join.  Visible means
            // all nodes except non-visible textnodes and breaks.
            if (join && origCollapsed) {
              if (!node->IsContent()) {
                join = false;
                continue;
              }
              nsIContent* content = node->AsContent();
              if (Text* text = content->GetAsText()) {
                join = !HTMLEditorRef().IsInVisibleTextFrames(*text);
              } else {
                join = content->IsHTMLElement(nsGkAtoms::br) &&
                       !HTMLEditorRef().IsVisibleBRElement(node);
              }
            }
          }
        }

        // Check endpoints for possible text deletion.  We can assume that if
        // text node is found, we can delete to end or to begining as
        // appropriate, since the case where both sel endpoints in same text
        // node was already handled (we wouldn't be here)
        if (startNode->IsText() &&
            startNode->Length() > static_cast<uint32_t>(startOffset)) {
          // Delete to last character
          OwningNonNull<Text> textNode = *startNode->AsText();
          rv =
              MOZ_KnownLive(HTMLEditorRef())
                  .DeleteTextWithTransaction(textNode, startOffset,
                                             startNode->Length() - startOffset);
          if (NS_WARN_IF(!CanHandleEditAction())) {
            return NS_ERROR_EDITOR_DESTROYED;
          }
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
        }
        if (endNode->IsText() && endOffset) {
          // Delete to first character
          OwningNonNull<Text> textNode = *endNode->AsText();
          rv = MOZ_KnownLive(HTMLEditorRef())
                   .DeleteTextWithTransaction(textNode, 0, endOffset);
          if (NS_WARN_IF(!CanHandleEditAction())) {
            return NS_ERROR_EDITOR_DESTROYED;
          }
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
        }

        if (join) {
          EditActionResult ret =
              TryToJoinBlocksWithTransaction(*leftParent, *rightParent);
          MOZ_ASSERT(*aHandled);
          *aCancel |= ret.Canceled();
          if (NS_WARN_IF(ret.Failed())) {
            return ret.Rv();
          }
        }
      }
    }
  }

  // If we're handling D&D, this is called to delete dragging item from the
  // tree.  In this case, we should move parent blocks if it becomes empty.
  if (HTMLEditorRef().GetEditAction() == EditAction::eDrop) {
    MOZ_ASSERT(startNode == endNode);
    MOZ_ASSERT(startOffset == endOffset);
    {
      AutoTrackDOMPoint startTracker(HTMLEditorRef().RangeUpdaterRef(),
                                     address_of(startNode), &startOffset);
      AutoTrackDOMPoint endTracker(HTMLEditorRef().RangeUpdaterRef(),
                                   address_of(endNode), &endOffset);

      EditorDOMPoint atStart(startNode, startOffset);
      nsresult rv = MOZ_KnownLive(HTMLEditorRef())
                        .DeleteParentBlocksWithTransactionIfEmpty(atStart);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      HTMLEditorRef()
          .TopLevelEditSubActionDataRef()
          .mDidDeleteEmptyParentBlocks = rv == NS_OK;
    }
    // If we removed parent blocks, Selection should be collapsed at where
    // the most ancestor empty block has been.
    if (HTMLEditorRef()
            .TopLevelEditSubActionDataRef()
            .mDidDeleteEmptyParentBlocks) {
      rv = SelectionRefPtr()->Collapse(startNode, startOffset);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      return NS_OK;
    }
  }

  // We might have left only collapsed whitespace in the start/end nodes
  {
    AutoTrackDOMPoint startTracker(HTMLEditorRef().RangeUpdaterRef(),
                                   address_of(startNode), &startOffset);
    AutoTrackDOMPoint endTracker(HTMLEditorRef().RangeUpdaterRef(),
                                 address_of(endNode), &endOffset);

    nsresult rv = DeleteNodeIfCollapsedText(*startNode);
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "Failed to delete start node even though it's collapsed text");
    rv = DeleteNodeIfCollapsedText(*endNode);
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "Failed to delete end node even though it's collapsed text");
  }

  // If we're joining blocks: if deleting forward the selection should be
  // collapsed to the end of the selection, if deleting backward the selection
  // should be collapsed to the beginning of the selection. But if we're not
  // joining then the selection should collapse to the beginning of the
  // selection if we'redeleting forward, because the end of the selection will
  // still be in the next block. And same thing for deleting backwards
  // (selection should collapse to the end, because the beginning will still be
  // in the first block). See Bug 507936
  if (aAction == (join ? nsIEditor::eNext : nsIEditor::ePrevious)) {
    rv = SelectionRefPtr()->Collapse(endNode, endOffset);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    rv = SelectionRefPtr()->Collapse(startNode, startOffset);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  return NS_OK;
}

nsresult HTMLEditRules::DeleteNodeIfCollapsedText(nsINode& aNode) {
  MOZ_ASSERT(IsEditorDataAvailable());

  Text* text = aNode.GetAsText();
  if (!text) {
    return NS_OK;
  }

  if (HTMLEditorRef().IsVisibleTextNode(*text)) {
    return NS_OK;
  }

  nsresult rv = MOZ_KnownLive(HTMLEditorRef()).DeleteNodeWithTransaction(aNode);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult HTMLEditRules::InsertBRIfNeeded() {
  MOZ_ASSERT(IsEditorDataAvailable());

  EditorDOMPoint atStartOfSelection(
      EditorBase::GetStartPoint(*SelectionRefPtr()));
  if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  // inline elements don't need any br
  if (!HTMLEditor::NodeIsBlockStatic(*atStartOfSelection.GetContainer())) {
    return NS_OK;
  }

  // examine selection
  WSRunObject wsObj(&HTMLEditorRef(), atStartOfSelection);
  if (((wsObj.mStartReason & WSType::block) ||
       (wsObj.mStartReason & WSType::br)) &&
      (wsObj.mEndReason & WSType::block)) {
    // if we are tucked between block boundaries then insert a br
    // first check that we are allowed to
    if (HTMLEditorRef().CanContainTag(*atStartOfSelection.GetContainer(),
                                      *nsGkAtoms::br)) {
      RefPtr<Element> brElement =
          MOZ_KnownLive(HTMLEditorRef())
              .InsertBRElementWithTransaction(atStartOfSelection,
                                              nsIEditor::ePrevious);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(!brElement)) {
        return NS_ERROR_FAILURE;
      }
      return NS_OK;
    }
  }
  return NS_OK;
}

EditorDOMPoint HTMLEditRules::GetGoodSelPointForNode(
    nsINode& aNode, nsIEditor::EDirection aAction) {
  MOZ_ASSERT(IsEditorDataAvailable());
  MOZ_ASSERT(aAction == nsIEditor::eNext || aAction == nsIEditor::eNextWord ||
             aAction == nsIEditor::ePrevious ||
             aAction == nsIEditor::ePreviousWord ||
             aAction == nsIEditor::eToBeginningOfLine ||
             aAction == nsIEditor::eToEndOfLine);

  bool isPreviousAction =
      (aAction == nsIEditor::ePrevious || aAction == nsIEditor::ePreviousWord ||
       aAction == nsIEditor::eToBeginningOfLine);

  if (aNode.GetAsText() || HTMLEditorRef().IsContainer(&aNode) ||
      NS_WARN_IF(!aNode.GetParentNode())) {
    return EditorDOMPoint(&aNode, isPreviousAction ? aNode.Length() : 0);
  }

  if (NS_WARN_IF(!aNode.IsContent())) {
    return EditorDOMPoint();
  }

  EditorDOMPoint ret(&aNode);
  if ((!aNode.IsHTMLElement(nsGkAtoms::br) ||
       HTMLEditorRef().IsVisibleBRElement(&aNode)) &&
      isPreviousAction) {
    ret.AdvanceOffset();
  }
  return ret;
}

EditActionResult HTMLEditRules::TryToJoinBlocksWithTransaction(
    nsIContent& aLeftNode, nsIContent& aRightNode) {
  MOZ_ASSERT(IsEditorDataAvailable());

  RefPtr<Element> leftBlock = HTMLEditorRef().GetBlock(aLeftNode);
  RefPtr<Element> rightBlock = HTMLEditorRef().GetBlock(aRightNode);

  // Sanity checks
  if (NS_WARN_IF(!leftBlock) || NS_WARN_IF(!rightBlock)) {
    return EditActionIgnored(NS_ERROR_NULL_POINTER);
  }
  if (NS_WARN_IF(leftBlock == rightBlock)) {
    return EditActionIgnored(NS_ERROR_UNEXPECTED);
  }

  if (HTMLEditUtils::IsTableElement(leftBlock) ||
      HTMLEditUtils::IsTableElement(rightBlock)) {
    // Do not try to merge table elements
    return EditActionCanceled();
  }

  // Make sure we don't try to move things into HR's, which look like blocks
  // but aren't containers
  if (leftBlock->IsHTMLElement(nsGkAtoms::hr)) {
    leftBlock = HTMLEditorRef().GetBlockNodeParent(leftBlock);
    if (NS_WARN_IF(!leftBlock)) {
      return EditActionIgnored(NS_ERROR_UNEXPECTED);
    }
  }
  if (rightBlock->IsHTMLElement(nsGkAtoms::hr)) {
    rightBlock = HTMLEditorRef().GetBlockNodeParent(rightBlock);
    if (NS_WARN_IF(!rightBlock)) {
      return EditActionIgnored(NS_ERROR_UNEXPECTED);
    }
  }

  // Bail if both blocks the same
  if (leftBlock == rightBlock) {
    return EditActionIgnored();
  }

  // Joining a list item to its parent is a NOP.
  if (HTMLEditUtils::IsList(leftBlock) &&
      HTMLEditUtils::IsListItem(rightBlock) &&
      rightBlock->GetParentNode() == leftBlock) {
    return EditActionHandled();
  }

  // Special rule here: if we are trying to join list items, and they are in
  // different lists, join the lists instead.
  bool mergeLists = false;
  nsAtom* existingList = nsGkAtoms::_empty;
  EditorDOMPoint atChildInBlock;
  nsCOMPtr<Element> leftList, rightList;
  if (HTMLEditUtils::IsListItem(leftBlock) &&
      HTMLEditUtils::IsListItem(rightBlock)) {
    leftList = leftBlock->GetParentElement();
    rightList = rightBlock->GetParentElement();
    if (leftList && rightList && leftList != rightList &&
        !EditorUtils::IsDescendantOf(*leftList, *rightBlock, &atChildInBlock) &&
        !EditorUtils::IsDescendantOf(*rightList, *leftBlock, &atChildInBlock)) {
      // There are some special complications if the lists are descendants of
      // the other lists' items.  Note that it is okay for them to be
      // descendants of the other lists themselves, which is the usual case for
      // sublists in our implementation.
      MOZ_DIAGNOSTIC_ASSERT(!atChildInBlock.IsSet());
      leftBlock = leftList;
      rightBlock = rightList;
      mergeLists = true;
      existingList = leftList->NodeInfo()->NameAtom();
    }
  }

  AutoTransactionsConserveSelection dontChangeMySelection(HTMLEditorRef());

  // offset below is where you find yourself in rightBlock when you traverse
  // upwards from leftBlock
  EditorDOMPoint atRightBlockChild;
  if (EditorUtils::IsDescendantOf(*leftBlock, *rightBlock,
                                  &atRightBlockChild)) {
    // Tricky case.  Left block is inside right block.  Do ws adjustment.  This
    // just destroys non-visible ws at boundaries we will be joining.
    DebugOnly<bool> advanced = atRightBlockChild.AdvanceOffset();
    NS_WARNING_ASSERTION(
        advanced,
        "Failed to advance offset to after child of rightBlock, "
        "leftBlock is a descendant of the child");
    nsresult rv = WSRunObject::ScrubBlockBoundary(
        MOZ_KnownLive(&HTMLEditorRef()), WSRunObject::kBlockEnd, leftBlock);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return EditActionIgnored(rv);
    }

    {
      // We can't just track rightBlock because it's an Element.
      AutoTrackDOMPoint tracker(HTMLEditorRef().RangeUpdaterRef(),
                                &atRightBlockChild);
      rv = WSRunObject::ScrubBlockBoundary(
          MOZ_KnownLive(&HTMLEditorRef()), WSRunObject::kAfterBlock,
          MOZ_KnownLive(atRightBlockChild.GetContainer()),
          atRightBlockChild.Offset());
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return EditActionIgnored(rv);
      }

      // XXX AutoTrackDOMPoint instance, tracker, hasn't been destroyed here.
      //     Do we really need to do update rightBlock here??
      MOZ_ASSERT(rightBlock == atRightBlockChild.GetContainer());
      if (atRightBlockChild.GetContainerAsElement()) {
        rightBlock = atRightBlockChild.GetContainerAsElement();
      } else {
        if (NS_WARN_IF(!atRightBlockChild.GetContainer()->GetParentElement())) {
          return EditActionIgnored(NS_ERROR_UNEXPECTED);
        }
        rightBlock = atRightBlockChild.GetContainer()->GetParentElement();
      }
    }

    // Do br adjustment.
    RefPtr<Element> brNode =
        CheckForInvisibleBR(*leftBlock, BRLocation::blockEnd);
    EditActionResult ret(NS_OK);
    if (NS_WARN_IF(mergeLists)) {
      // Since 2002, here was the following comment:
      // > The idea here is to take all children in rightList that are past
      // > offset, and pull them into leftlist.
      // However, this has never been performed because we are here only when
      // neither left list nor right list is a descendant of the other but
      // in such case, getting a list item in the right list node almost
      // always failed since a variable for offset of rightList->GetChildAt()
      // was not initialized.  So, it might be a bug, but we should keep this
      // traditional behavior for now.  If you find when we get here, please
      // remove this comment if we don't need to do it.  Otherwise, please
      // move children of the right list node to the end of the left list node.
      MOZ_DIAGNOSTIC_ASSERT(!atChildInBlock.IsSet());

      // XXX Although, we don't do nothing here, but for keeping traditional
      //     behavior, we should mark as handled.
      ret.MarkAsHandled();
    } else {
      // XXX Why do we ignore the result of MoveBlock()?
      EditActionResult retMoveBlock =
          MoveBlock(*leftBlock, *rightBlock, -1, atRightBlockChild.Offset());
      if (NS_WARN_IF(retMoveBlock.Rv() == NS_ERROR_EDITOR_DESTROYED)) {
        return ret;
      }
      NS_WARNING_ASSERTION(
          retMoveBlock.Succeeded(),
          "Failed to move contents of the right block to the left block");
      if (retMoveBlock.Handled()) {
        ret.MarkAsHandled();
      }
      // Now, all children of rightBlock were moved to leftBlock.  So,
      // atRightBlockChild is now invalid.
      atRightBlockChild.Clear();
    }
    if (brNode) {
      nsresult rv =
          MOZ_KnownLive(HTMLEditorRef()).DeleteNodeWithTransaction(*brNode);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
      }
      if (NS_SUCCEEDED(rv)) {
        ret.MarkAsHandled();
      } else {
        NS_WARNING("Failed to remove the <br> element");
      }
    }
    return ret;
  }

  MOZ_DIAGNOSTIC_ASSERT(!atRightBlockChild.IsSet());

  // Offset below is where you find yourself in leftBlock when you traverse
  // upwards from rightBlock
  EditorDOMPoint leftBlockChild;
  if (EditorUtils::IsDescendantOf(*rightBlock, *leftBlock, &leftBlockChild)) {
    // Tricky case.  Right block is inside left block.  Do ws adjustment.  This
    // just destroys non-visible ws at boundaries we will be joining.
    nsresult rv = WSRunObject::ScrubBlockBoundary(
        MOZ_KnownLive(&HTMLEditorRef()), WSRunObject::kBlockStart, rightBlock);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return EditActionIgnored(rv);
    }

    {
      // We can't just track leftBlock because it's an Element, so track
      // something else.
      AutoTrackDOMPoint tracker(HTMLEditorRef().RangeUpdaterRef(),
                                &leftBlockChild);
      rv = WSRunObject::ScrubBlockBoundary(MOZ_KnownLive(&HTMLEditorRef()),
                                           WSRunObject::kBeforeBlock, leftBlock,
                                           leftBlockChild.Offset());
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return EditActionIgnored(rv);
      }
      // XXX AutoTrackDOMPoint instance, tracker, hasn't been destroyed here.
      //     Do we really need to do update rightBlock here??
      MOZ_DIAGNOSTIC_ASSERT(leftBlock == leftBlockChild.GetContainer());
      if (leftBlockChild.GetContainerAsElement()) {
        leftBlock = leftBlockChild.GetContainerAsElement();
      } else {
        if (NS_WARN_IF(!leftBlockChild.GetContainer()->GetParentElement())) {
          return EditActionIgnored(NS_ERROR_UNEXPECTED);
        }
        leftBlock = leftBlockChild.GetContainer()->GetParentElement();
      }
    }
    // Do br adjustment.
    RefPtr<Element> brNode = CheckForInvisibleBR(
        *leftBlock, BRLocation::beforeBlock, leftBlockChild.Offset());
    EditActionResult ret(NS_OK);
    if (mergeLists) {
      // XXX Why do we ignore the result of MoveContents()?
      int32_t offset = leftBlockChild.Offset();
      EditActionResult retMoveContents =
          MoveContents(*rightList, *leftList, &offset);
      if (NS_WARN_IF(retMoveContents.Rv() == NS_ERROR_EDITOR_DESTROYED)) {
        return ret;
      }
      NS_WARNING_ASSERTION(
          retMoveContents.Succeeded(),
          "Failed to move contents from the right list to the left list");
      if (retMoveContents.Handled()) {
        ret.MarkAsHandled();
      }
      // leftBlockChild was moved to rightList.  So, it's invalid now.
      leftBlockChild.Clear();
    } else {
      // Left block is a parent of right block, and the parent of the previous
      // visible content.  Right block is a child and contains the contents we
      // want to move.

      EditorDOMPoint previousContent;
      if (&aLeftNode == leftBlock) {
        // We are working with valid HTML, aLeftNode is a block node, and is
        // therefore allowed to contain rightBlock.  This is the simple case,
        // we will simply move the content in rightBlock out of its block.
        previousContent = leftBlockChild;
      } else {
        // We try to work as well as possible with HTML that's already invalid.
        // Although "right block" is a block, and a block must not be contained
        // in inline elements, reality is that broken documents do exist.  The
        // DIRECT parent of "left NODE" might be an inline element.  Previous
        // versions of this code skipped inline parents until the first block
        // parent was found (and used "left block" as the destination).
        // However, in some situations this strategy moves the content to an
        // unexpected position.  (see bug 200416) The new idea is to make the
        // moving content a sibling, next to the previous visible content.
        previousContent.Set(&aLeftNode);

        // We want to move our content just after the previous visible node.
        previousContent.AdvanceOffset();
      }

      // Because we don't want the moving content to receive the style of the
      // previous content, we split the previous content's style.

      RefPtr<Element> editorRoot = HTMLEditorRef().GetEditorRoot();
      if (!editorRoot || &aLeftNode != editorRoot) {
        nsCOMPtr<nsIContent> splittedPreviousContent;
        nsCOMPtr<nsINode> previousContentParent =
            previousContent.GetContainer();
        int32_t previousContentOffset = previousContent.Offset();
        rv = MOZ_KnownLive(HTMLEditorRef())
                 .SplitStyleAbovePoint(address_of(previousContentParent),
                                       &previousContentOffset, nullptr, nullptr,
                                       nullptr,
                                       getter_AddRefs(splittedPreviousContent));
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return EditActionIgnored(rv);
        }

        if (splittedPreviousContent) {
          previousContent.Set(splittedPreviousContent);
        } else {
          previousContent.Set(previousContentParent, previousContentOffset);
        }
      }

      if (NS_WARN_IF(!previousContent.IsSet())) {
        return EditActionIgnored(NS_ERROR_NULL_POINTER);
      }

      ret |= MoveBlock(MOZ_KnownLive(*previousContent.GetContainerAsElement()),
                       *rightBlock, previousContent.Offset(), 0);
      if (NS_WARN_IF(ret.Failed())) {
        return ret;
      }
    }
    if (brNode) {
      nsresult rv =
          MOZ_KnownLive(HTMLEditorRef()).DeleteNodeWithTransaction(*brNode);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return ret.SetResult(NS_ERROR_EDITOR_DESTROYED);
      }
      if (NS_SUCCEEDED(rv)) {
        ret.MarkAsHandled();
      } else {
        NS_WARNING("Failed to remove the <br> element");
      }
    }
    return ret;
  }

  MOZ_DIAGNOSTIC_ASSERT(!atRightBlockChild.IsSet());
  MOZ_DIAGNOSTIC_ASSERT(!leftBlockChild.IsSet());

  // Normal case.  Blocks are siblings, or at least close enough.  An example
  // of the latter is <p>paragraph</p><ul><li>one<li>two<li>three</ul>.  The
  // first li and the p are not true siblings, but we still want to join them
  // if you backspace from li into p.

  // Adjust whitespace at block boundaries
  nsresult rv = WSRunObject::PrepareToJoinBlocks(
      MOZ_KnownLive(&HTMLEditorRef()), leftBlock, rightBlock);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditActionIgnored(rv);
  }
  // Do br adjustment.
  RefPtr<Element> brNode =
      CheckForInvisibleBR(*leftBlock, BRLocation::blockEnd);
  EditActionResult ret(NS_OK);
  if (mergeLists ||
      leftBlock->NodeInfo()->NameAtom() == rightBlock->NodeInfo()->NameAtom()) {
    // Nodes are same type.  merge them.
    EditorDOMPoint pt;
    nsresult rv =
        JoinNearestEditableNodesWithTransaction(*leftBlock, *rightBlock, &pt);
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
    }
    if (pt.IsSet() && mergeLists) {
      CreateElementResult convertListTypeResult = ConvertListType(
          *rightBlock, MOZ_KnownLive(*existingList), *nsGkAtoms::li);
      if (NS_WARN_IF(convertListTypeResult.Rv() == NS_ERROR_EDITOR_DESTROYED)) {
        return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
      }
    }
    ret.MarkAsHandled();
  } else {
    // Nodes are dissimilar types.
    ret |= MoveBlock(*leftBlock, *rightBlock, -1, 0);
    if (NS_WARN_IF(ret.Failed())) {
      return ret;
    }
  }
  if (brNode) {
    rv = MOZ_KnownLive(HTMLEditorRef()).DeleteNodeWithTransaction(*brNode);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return ret.SetResult(NS_ERROR_EDITOR_DESTROYED);
    }
    // XXX In other top level if blocks, the result of
    //     DeleteNodeWithTransaction() is ignored.  Why does only this result
    //     is respected?
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return ret.SetResult(rv);
    }
    ret.MarkAsHandled();
  }
  return ret;
}

EditActionResult HTMLEditRules::MoveBlock(Element& aLeftBlock,
                                          Element& aRightBlock,
                                          int32_t aLeftOffset,
                                          int32_t aRightOffset) {
  MOZ_ASSERT(IsEditorDataAvailable());

  AutoTArray<OwningNonNull<nsINode>, 64> arrayOfNodes;
  nsresult rv = MOZ_KnownLive(HTMLEditorRef())
                    .SplitInlinesAndCollectEditTargetNodesInOneHardLine(
                        EditorDOMPoint(&aRightBlock, aRightOffset),
                        arrayOfNodes, EditSubAction::eCreateOrChangeList);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditActionIgnored(rv);
  }

  EditActionResult ret(NS_OK);
  for (uint32_t i = 0; i < arrayOfNodes.Length(); i++) {
    // get the node to act on
    if (HTMLEditor::NodeIsBlockStatic(arrayOfNodes[i])) {
      // For block nodes, move their contents only, then delete block.
      ret |= MoveContents(MOZ_KnownLive(*arrayOfNodes[i]->AsElement()),
                          aLeftBlock, &aLeftOffset);
      if (NS_WARN_IF(ret.Failed())) {
        return ret;
      }
      rv = MOZ_KnownLive(HTMLEditorRef())
               .DeleteNodeWithTransaction(MOZ_KnownLive(*arrayOfNodes[i]));
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return ret.SetResult(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to remove a block node");
      ret.MarkAsHandled();
    } else {
      // Otherwise move the content as is, checking against the DTD.
      ret |= MoveNodeSmart(MOZ_KnownLive(*arrayOfNodes[i]->AsContent()),
                           aLeftBlock, &aLeftOffset);
      if (NS_WARN_IF(ret.Rv() == NS_ERROR_EDITOR_DESTROYED)) {
        return ret;
      }
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "Failed to move current node to the left block");
    }
  }

  // XXX We're only checking return value of the last iteration
  if (NS_WARN_IF(ret.Failed())) {
    return ret;
  }

  return ret;
}

EditActionResult HTMLEditRules::MoveNodeSmart(nsIContent& aNode,
                                              Element& aDestElement,
                                              int32_t* aInOutDestOffset) {
  MOZ_ASSERT(IsEditorDataAvailable());
  MOZ_ASSERT(aInOutDestOffset);

  // Check if this node can go into the destination node
  if (HTMLEditorRef().CanContain(aDestElement, aNode)) {
    // If it can, move it there.
    if (*aInOutDestOffset == -1) {
      nsresult rv = MOZ_KnownLive(HTMLEditorRef())
                        .MoveNodeToEndWithTransaction(aNode, aDestElement);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return EditActionIgnored(rv);
      }
    } else {
      EditorDOMPoint pointToInsert(&aDestElement, *aInOutDestOffset);
      nsresult rv = MOZ_KnownLive(HTMLEditorRef())
                        .MoveNodeWithTransaction(aNode, pointToInsert);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return EditActionIgnored(rv);
      }
    }
    if (*aInOutDestOffset != -1) {
      (*aInOutDestOffset)++;
    }
    // XXX Should we check if the node is actually moved in this case?
    return EditActionHandled();
  }

  // If it can't, move its children (if any), and then delete it.
  EditActionResult ret(NS_OK);
  if (aNode.IsElement()) {
    ret = MoveContents(MOZ_KnownLive(*aNode.AsElement()), aDestElement,
                       aInOutDestOffset);
    if (NS_WARN_IF(ret.Failed())) {
      return ret;
    }
  }

  nsresult rv = MOZ_KnownLive(HTMLEditorRef()).DeleteNodeWithTransaction(aNode);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return ret.SetResult(NS_ERROR_EDITOR_DESTROYED);
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return ret.SetResult(rv);
  }
  return ret.MarkAsHandled();
}

EditActionResult HTMLEditRules::MoveContents(Element& aElement,
                                             Element& aDestElement,
                                             int32_t* aInOutDestOffset) {
  MOZ_ASSERT(aInOutDestOffset);

  if (NS_WARN_IF(&aElement == &aDestElement)) {
    return EditActionIgnored(NS_ERROR_ILLEGAL_VALUE);
  }

  EditActionResult ret(NS_OK);
  while (aElement.GetFirstChild()) {
    ret |= MoveNodeSmart(MOZ_KnownLive(*aElement.GetFirstChild()), aDestElement,
                         aInOutDestOffset);
    if (NS_WARN_IF(ret.Failed())) {
      return ret;
    }
  }
  return ret;
}

nsresult HTMLEditRules::DeleteElementsExceptTableRelatedElements(
    nsINode& aNode) {
  MOZ_ASSERT(IsEditorDataAvailable());

  if (!HTMLEditUtils::IsTableElementButNotTable(&aNode)) {
    nsresult rv =
        MOZ_KnownLive(HTMLEditorRef()).DeleteNodeWithTransaction(aNode);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  // XXX For performance, this should just call
  //     DeleteElementsExceptTableRelatedElements() while there are children
  //     in aNode.  If we need to avoid infinite loop because mutation event
  //     listeners can add unexpected nodes into aNode, we should just loop
  //     only original count of the children.
  AutoTArray<OwningNonNull<nsIContent>, 10> childList;
  for (nsIContent* child = aNode.GetFirstChild(); child;
       child = child->GetNextSibling()) {
    childList.AppendElement(*child);
  }

  for (const auto& child : childList) {
    nsresult rv = DeleteElementsExceptTableRelatedElements(child);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  return NS_OK;
}

nsresult HTMLEditRules::DidDeleteSelection() {
  MOZ_ASSERT(IsEditorDataAvailable());

  // find where we are
  EditorDOMPoint atStartOfSelection(
      EditorBase::GetStartPoint(*SelectionRefPtr()));
  if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  // find any enclosing mailcite
  RefPtr<Element> citeNode = HTMLEditorRef().GetMostAncestorMailCiteElement(
      *atStartOfSelection.GetContainer());
  if (citeNode) {
    bool isEmpty = true, seenBR = false;
    HTMLEditorRef().IsEmptyNodeImpl(citeNode, &isEmpty, true, true, false,
                                    &seenBR);
    if (isEmpty) {
      EditorDOMPoint atCiteNode(citeNode);
      {
        AutoEditorDOMPointChildInvalidator lockOffset(atCiteNode);
        nsresult rv =
            MOZ_KnownLive(HTMLEditorRef()).DeleteNodeWithTransaction(*citeNode);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
      if (atCiteNode.IsSet() && seenBR) {
        RefPtr<Element> brElement =
            MOZ_KnownLive(HTMLEditorRef())
                .InsertBRElementWithTransaction(atCiteNode);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(!brElement)) {
          return NS_ERROR_FAILURE;
        }
        IgnoredErrorResult error;
        SelectionRefPtr()->Collapse(EditorRawDOMPoint(brElement), error);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        NS_WARNING_ASSERTION(
            !error.Failed(),
            "Failed to collapse selection at the new <br> element");
      }
    }
  }

  // call through to base class
  nsresult rv = TextEditRules::DidDeleteSelection();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "TextEditRules::DidDeleteSelection() failed");
  return rv;
}

nsresult HTMLEditRules::WillMakeList(const nsAString* aListType,
                                     bool aEntireList,
                                     const nsAString* aBulletType,
                                     bool* aCancel, bool* aHandled,
                                     const nsAString* aItemType) {
  MOZ_ASSERT(IsEditorDataAvailable());

  if (NS_WARN_IF(!aListType) || NS_WARN_IF(!aCancel) || NS_WARN_IF(!aHandled)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aCancel = false;
  *aHandled = false;

  OwningNonNull<nsAtom> listType = NS_Atomize(*aListType);

  // FYI: Ignore cancel result of WillInsert().
  nsresult rv = MOZ_KnownLive(HTMLEditorRef()).WillInsert();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "WillInsert() failed");

  // deduce what tag to use for list items
  RefPtr<nsAtom> itemType;
  if (aItemType) {
    itemType = NS_Atomize(*aItemType);
  } else if (listType == nsGkAtoms::dl) {
    itemType = nsGkAtoms::dd;
  } else {
    itemType = nsGkAtoms::li;
  }

  // convert the selection ranges into "promoted" selection ranges:
  // this basically just expands the range to include the immediate
  // block parent, and then further expands to include any ancestors
  // whose children are all in the range

  *aHandled = true;

  rv = NormalizeSelection();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // MakeList() creates AutoSelectionRestorer.
  // Therefore, even if it returns NS_OK, editor might have been destroyed
  // at restoring Selection.
  rv = MakeList(listType, aEntireList, aBulletType, aCancel, *itemType);
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED) ||
      NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult HTMLEditRules::MakeList(nsAtom& aListType, bool aEntireList,
                                 const nsAString* aBulletType, bool* aCancel,
                                 nsAtom& aItemType) {
  AutoSelectionRestorer restoreSelectionLater(HTMLEditorRef());

  nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
  nsresult rv = GetListActionNodes(
      arrayOfNodes, aEntireList ? EntireList::yes : EntireList::no,
      TouchContent::yes);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // check if all our nodes are <br>s, or empty inlines
  bool bOnlyBreaks = true;
  for (auto& curNode : arrayOfNodes) {
    // if curNode is not a Break or empty inline, we're done
    if (!TextEditUtils::IsBreak(curNode) && !IsEmptyInline(curNode)) {
      bOnlyBreaks = false;
      break;
    }
  }

  // if no nodes, we make empty list.  Ditto if the user tried to make a list
  // of some # of breaks.
  if (arrayOfNodes.IsEmpty() || bOnlyBreaks) {
    // if only breaks, delete them
    if (bOnlyBreaks) {
      for (auto& node : arrayOfNodes) {
        rv = MOZ_KnownLive(HTMLEditorRef()).DeleteNodeWithTransaction(*node);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
    }

    nsRange* firstRange = SelectionRefPtr()->GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return NS_ERROR_FAILURE;
    }

    EditorDOMPoint atStartOfSelection(firstRange->StartRef());
    if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
      return NS_ERROR_FAILURE;
    }

    // Make sure we can put a list here.
    if (!HTMLEditorRef().CanContainTag(*atStartOfSelection.GetContainer(),
                                       aListType)) {
      *aCancel = true;
      return NS_OK;
    }

    SplitNodeResult splitAtSelectionStartResult =
        MaybeSplitAncestorsForInsertWithTransaction(aListType,
                                                    atStartOfSelection);
    if (NS_WARN_IF(splitAtSelectionStartResult.Failed())) {
      return splitAtSelectionStartResult.Rv();
    }
    RefPtr<Element> theList =
        MOZ_KnownLive(HTMLEditorRef())
            .CreateNodeWithTransaction(
                aListType, splitAtSelectionStartResult.SplitPoint());
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(!theList)) {
      return NS_ERROR_FAILURE;
    }

    RefPtr<Element> theListItem =
        MOZ_KnownLive(HTMLEditorRef())
            .CreateNodeWithTransaction(aItemType, EditorDOMPoint(theList, 0));
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(!theListItem)) {
      return NS_ERROR_FAILURE;
    }

    // remember our new block for postprocessing
    HTMLEditorRef().TopLevelEditSubActionDataRef().mNewBlockElement =
        theListItem;
    // Put selection in new list item and don't restore the Selection.
    restoreSelectionLater.Abort();
    ErrorResult error;
    SelectionRefPtr()->Collapse(EditorRawDOMPoint(theListItem, 0), error);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      error.SuppressException();
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(!error.Failed())) {
      return error.StealNSResult();
    }
    return NS_OK;
  }

  // if there is only one node in the array, and it is a list, div, or
  // blockquote, then look inside of it until we find inner list or content.

  LookInsideDivBQandList(arrayOfNodes);

  // Ok, now go through all the nodes and put then in the list,
  // or whatever is approriate.  Wohoo!

  uint32_t listCount = arrayOfNodes.Length();
  RefPtr<Element> curList, prevListItem;

  for (uint32_t i = 0; i < listCount; i++) {
    // here's where we actually figure out what to do
    RefPtr<Element> newBlock;
    if (NS_WARN_IF(!arrayOfNodes[i]->IsContent())) {
      return NS_ERROR_FAILURE;
    }
    OwningNonNull<nsIContent> curNode = *arrayOfNodes[i]->AsContent();

    // make sure we don't assemble content that is in different table cells
    // into the same list.  respect table cell boundaries when listifying.
    if (curList && InDifferentTableElements(curList, curNode)) {
      curList = nullptr;
    }

    // If curNode is a break, delete it, and quit remembering prev list item.
    // If an empty inline container, delete it, but still remember the previous
    // item.
    if (HTMLEditorRef().IsEditable(curNode) &&
        (TextEditUtils::IsBreak(curNode) || IsEmptyInline(curNode))) {
      rv = MOZ_KnownLive(HTMLEditorRef()).DeleteNodeWithTransaction(*curNode);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      if (TextEditUtils::IsBreak(curNode)) {
        prevListItem = nullptr;
      }
      continue;
    }

    if (HTMLEditUtils::IsList(curNode)) {
      // do we have a curList already?
      if (curList && !EditorUtils::IsDescendantOf(*curNode, *curList)) {
        // move all of our children into curList.  cheezy way to do it: move
        // whole list and then RemoveContainerWithTransaction() on the list.
        // ConvertListType first: that routine handles converting the list
        // item types, if needed.
        rv = MOZ_KnownLive(HTMLEditorRef())
                 .MoveNodeToEndWithTransaction(*curNode, *curList);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        CreateElementResult convertListTypeResult = ConvertListType(
            MOZ_KnownLive(*curNode->AsElement()), aListType, aItemType);
        if (NS_WARN_IF(convertListTypeResult.Failed())) {
          return convertListTypeResult.Rv();
        }
        rv = MOZ_KnownLive(HTMLEditorRef())
                 .RemoveBlockContainerWithTransaction(
                     MOZ_KnownLive(*convertListTypeResult.GetNewNode()));
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        newBlock = convertListTypeResult.forget();
      } else {
        // replace list with new list type
        CreateElementResult convertListTypeResult = ConvertListType(
            MOZ_KnownLive(*curNode->AsElement()), aListType, aItemType);
        if (NS_WARN_IF(convertListTypeResult.Failed())) {
          return convertListTypeResult.Rv();
        }
        curList = convertListTypeResult.forget();
      }
      prevListItem = nullptr;
      continue;
    }

    EditorDOMPoint atCurNode(curNode);
    if (NS_WARN_IF(!atCurNode.IsSet())) {
      return NS_ERROR_FAILURE;
    }
    MOZ_ASSERT(atCurNode.IsSetAndValid());
    if (HTMLEditUtils::IsListItem(curNode)) {
      if (!atCurNode.IsContainerHTMLElement(&aListType)) {
        // list item is in wrong type of list. if we don't have a curList,
        // split the old list and make a new list of correct type.
        if (!curList || EditorUtils::IsDescendantOf(*curNode, *curList)) {
          if (NS_WARN_IF(!atCurNode.GetContainerAsContent())) {
            return NS_ERROR_FAILURE;
          }
          ErrorResult error;
          nsCOMPtr<nsIContent> newLeftNode =
              MOZ_KnownLive(HTMLEditorRef())
                  .SplitNodeWithTransaction(atCurNode, error);
          if (NS_WARN_IF(!CanHandleEditAction())) {
            error.SuppressException();
            return NS_ERROR_EDITOR_DESTROYED;
          }
          if (NS_WARN_IF(error.Failed())) {
            return error.StealNSResult();
          }
          newBlock = newLeftNode ? newLeftNode->AsElement() : nullptr;
          curList =
              MOZ_KnownLive(HTMLEditorRef())
                  .CreateNodeWithTransaction(
                      aListType, EditorDOMPoint(atCurNode.GetContainer()));
          if (NS_WARN_IF(!CanHandleEditAction())) {
            return NS_ERROR_EDITOR_DESTROYED;
          }
          if (NS_WARN_IF(!curList)) {
            return NS_ERROR_FAILURE;
          }
        }
        // move list item to new list
        rv = MOZ_KnownLive(HTMLEditorRef())
                 .MoveNodeToEndWithTransaction(*curNode, *curList);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        // convert list item type if needed
        if (!curNode->IsHTMLElement(&aItemType)) {
          newBlock = MOZ_KnownLive(HTMLEditorRef())
                         .ReplaceContainerWithTransaction(
                             MOZ_KnownLive(*curNode->AsElement()), aItemType);
          if (NS_WARN_IF(!CanHandleEditAction())) {
            return NS_ERROR_EDITOR_DESTROYED;
          }
          if (NS_WARN_IF(!newBlock)) {
            return NS_ERROR_FAILURE;
          }
        }
      } else {
        // item is in right type of list.  But we might still have to move it.
        // and we might need to convert list item types.
        if (!curList) {
          curList = atCurNode.GetContainerAsElement();
        } else if (atCurNode.GetContainer() != curList) {
          // move list item to new list
          rv = MOZ_KnownLive(HTMLEditorRef())
                   .MoveNodeToEndWithTransaction(*curNode, *curList);
          if (NS_WARN_IF(!CanHandleEditAction())) {
            return NS_ERROR_EDITOR_DESTROYED;
          }
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
        }
        if (!curNode->IsHTMLElement(&aItemType)) {
          newBlock = MOZ_KnownLive(HTMLEditorRef())
                         .ReplaceContainerWithTransaction(
                             MOZ_KnownLive(*curNode->AsElement()), aItemType);
          if (NS_WARN_IF(!CanHandleEditAction())) {
            return NS_ERROR_EDITOR_DESTROYED;
          }
          if (NS_WARN_IF(!newBlock)) {
            return NS_ERROR_FAILURE;
          }
        }
      }
      nsCOMPtr<Element> curElement = do_QueryInterface(curNode);
      if (NS_WARN_IF(!curElement)) {
        return NS_ERROR_FAILURE;
      }
      if (aBulletType && !aBulletType->IsEmpty()) {
        rv = MOZ_KnownLive(HTMLEditorRef())
                 .SetAttributeWithTransaction(*curElement, *nsGkAtoms::type,
                                              *aBulletType);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      } else {
        rv = MOZ_KnownLive(HTMLEditorRef())
                 .RemoveAttributeWithTransaction(*curElement, *nsGkAtoms::type);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
      continue;
    }

    // if we hit a div clear our prevListItem, insert divs contents
    // into our node array, and remove the div
    if (curNode->IsHTMLElement(nsGkAtoms::div)) {
      prevListItem = nullptr;
      HTMLEditorRef().CollectChildren(*curNode, arrayOfNodes, i + 1);
      rv = MOZ_KnownLive(HTMLEditorRef())
               .RemoveContainerWithTransaction(
                   MOZ_KnownLive(*curNode->AsElement()));
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      listCount = arrayOfNodes.Length();
      continue;
    }

    // need to make a list to put things in if we haven't already,
    if (!curList) {
      SplitNodeResult splitCurNodeResult =
          MaybeSplitAncestorsForInsertWithTransaction(aListType, atCurNode);
      if (NS_WARN_IF(splitCurNodeResult.Failed())) {
        return splitCurNodeResult.Rv();
      }
      curList = MOZ_KnownLive(HTMLEditorRef())
                    .CreateNodeWithTransaction(aListType,
                                               splitCurNodeResult.SplitPoint());
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(!curList)) {
        return NS_ERROR_FAILURE;
      }
      // remember our new block for postprocessing
      HTMLEditorRef().TopLevelEditSubActionDataRef().mNewBlockElement = curList;
      // curList is now the correct thing to put curNode in
      prevListItem = nullptr;

      // atCurNode is now referring the right node with mOffset but
      // referring the left node with mRef.  So, invalidate it now.
      atCurNode.Clear();
    }

    // if curNode isn't a list item, we must wrap it in one
    nsCOMPtr<Element> listItem;
    if (!HTMLEditUtils::IsListItem(curNode)) {
      if (HTMLEditor::NodeIsInlineStatic(curNode) && prevListItem) {
        // this is a continuation of some inline nodes that belong together in
        // the same list item.  use prevListItem
        rv = MOZ_KnownLive(HTMLEditorRef())
                 .MoveNodeToEndWithTransaction(*curNode, *prevListItem);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      } else {
        // don't wrap li around a paragraph.  instead replace paragraph with li
        if (curNode->IsHTMLElement(nsGkAtoms::p)) {
          listItem = MOZ_KnownLive(HTMLEditorRef())
                         .ReplaceContainerWithTransaction(
                             MOZ_KnownLive(*curNode->AsElement()), aItemType);
          if (NS_WARN_IF(!CanHandleEditAction())) {
            return NS_ERROR_EDITOR_DESTROYED;
          }
          if (NS_WARN_IF(!listItem)) {
            return NS_ERROR_FAILURE;
          }
        } else {
          listItem = MOZ_KnownLive(HTMLEditorRef())
                         .InsertContainerWithTransaction(*curNode, aItemType);
          if (NS_WARN_IF(!CanHandleEditAction())) {
            return NS_ERROR_EDITOR_DESTROYED;
          }
          if (NS_WARN_IF(!listItem)) {
            return NS_ERROR_FAILURE;
          }
        }
        if (HTMLEditor::NodeIsInlineStatic(curNode)) {
          prevListItem = listItem;
        } else {
          prevListItem = nullptr;
        }
      }
    } else {
      listItem = curNode->AsElement();
    }

    if (listItem) {
      // if we made a new list item, deal with it: tuck the listItem into the
      // end of the active list
      rv = MOZ_KnownLive(HTMLEditorRef())
               .MoveNodeToEndWithTransaction(*listItem, *curList);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  return NS_OK;
}

nsresult HTMLEditRules::WillRemoveList(bool* aCancel, bool* aHandled) {
  MOZ_ASSERT(IsEditorDataAvailable());

  if (NS_WARN_IF(!aCancel) || NS_WARN_IF(!aHandled)) {
    return NS_ERROR_INVALID_ARG;
  }
  // initialize out param
  *aCancel = false;
  *aHandled = true;

  nsresult rv = NormalizeSelection();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  AutoSelectionRestorer restoreSelectionLater(HTMLEditorRef());

  nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
  rv = GetListActionNodes(arrayOfNodes, EntireList::no, TouchContent::yes);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Remove all non-editable nodes.  Leave them be.
  for (int32_t i = arrayOfNodes.Length() - 1; i >= 0; i--) {
    OwningNonNull<nsINode> testNode = arrayOfNodes[i];
    if (!HTMLEditorRef().IsEditable(testNode)) {
      arrayOfNodes.RemoveElementAt(i);
    }
  }

  // Only act on lists or list items in the array
  for (auto& curNode : arrayOfNodes) {
    // here's where we actually figure out what to do
    if (HTMLEditUtils::IsListItem(curNode)) {
      // unlist this listitem
      bool bOutOfList;
      do {
        rv = PopListItem(MOZ_KnownLive(*curNode->AsContent()), &bOutOfList);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      } while (
          !bOutOfList);  // keep popping it out until it's not in a list anymore
    } else if (HTMLEditUtils::IsList(curNode)) {
      // node is a list, move list items out
      rv = RemoveListStructure(MOZ_KnownLive(*curNode->AsElement()));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }
  return NS_OK;
}

nsresult HTMLEditRules::WillMakeDefListItem(const nsAString* aItemType,
                                            bool aEntireList, bool* aCancel,
                                            bool* aHandled) {
  MOZ_ASSERT(IsEditorDataAvailable());

  // for now we let WillMakeList handle this
  NS_NAMED_LITERAL_STRING(listType, "dl");
  nsresult rv = WillMakeList(&listType.AsString(), aEntireList, nullptr,
                             aCancel, aHandled, aItemType);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult HTMLEditRules::WillMakeBasicBlock(const nsAString& aBlockType,
                                           bool* aCancel, bool* aHandled) {
  MOZ_ASSERT(IsEditorDataAvailable());
  MOZ_ASSERT(aCancel && aHandled);

  OwningNonNull<nsAtom> blockType = NS_Atomize(aBlockType);

  // FYI: Ignore cancel result of WillInsert().
  nsresult rv = MOZ_KnownLive(HTMLEditorRef()).WillInsert();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "WillInsert() failed");

  *aCancel = false;
  *aHandled = true;

  // MakeBasicBlock() creates AutoSelectionRestorer.
  // Therefore, even if it returns NS_OK, editor might have been destroyed
  // at restoring Selection.
  rv = MakeBasicBlock(blockType);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult HTMLEditRules::MakeBasicBlock(nsAtom& blockType) {
  MOZ_ASSERT(IsEditorDataAvailable());

  nsresult rv = NormalizeSelection();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  AutoSelectionRestorer restoreSelectionLater(HTMLEditorRef());
  AutoTransactionsConserveSelection dontChangeMySelection(HTMLEditorRef());

  AutoTArray<OwningNonNull<nsINode>, 64> arrayOfNodes;
  rv = MOZ_KnownLive(HTMLEditorRef())
           .SplitInlinesAndCollectEditTargetNodesInExtendedSelectionRanges(
               arrayOfNodes, EditSubAction::eCreateOrRemoveBlock);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // If nothing visible in list, make an empty block
  if (ListIsEmptyLine(arrayOfNodes)) {
    nsRange* firstRange = SelectionRefPtr()->GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return NS_ERROR_FAILURE;
    }

    EditorDOMPoint pointToInsertBlock(firstRange->StartRef());
    if (&blockType == nsGkAtoms::normal || &blockType == nsGkAtoms::_empty) {
      // We are removing blocks (going to "body text")
      RefPtr<Element> curBlock =
          HTMLEditorRef().GetBlock(*pointToInsertBlock.GetContainer());
      if (NS_WARN_IF(!curBlock)) {
        return NS_ERROR_FAILURE;
      }
      if (!HTMLEditUtils::IsFormatNode(curBlock)) {
        return NS_OK;
      }

      // If the first editable node after selection is a br, consume it.
      // Otherwise it gets pushed into a following block after the split,
      // which is visually bad.
      nsCOMPtr<nsIContent> brContent =
          HTMLEditorRef().GetNextEditableHTMLNode(pointToInsertBlock);
      if (brContent && brContent->IsHTMLElement(nsGkAtoms::br)) {
        AutoEditorDOMPointChildInvalidator lockOffset(pointToInsertBlock);
        rv = MOZ_KnownLive(HTMLEditorRef())
                 .DeleteNodeWithTransaction(*brContent);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
      // Do the splits!
      SplitNodeResult splitNodeResult =
          MOZ_KnownLive(HTMLEditorRef())
              .SplitNodeDeepWithTransaction(
                  *curBlock, pointToInsertBlock,
                  SplitAtEdges::eDoNotCreateEmptyContainer);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(splitNodeResult.Failed())) {
        return splitNodeResult.Rv();
      }
      EditorDOMPoint pointToInsertBrNode(splitNodeResult.SplitPoint());
      // Put a <br> element at the split point
      brContent = MOZ_KnownLive(HTMLEditorRef())
                      .InsertBRElementWithTransaction(pointToInsertBrNode);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(!brContent)) {
        return NS_ERROR_FAILURE;
      }
      // Put selection at the split point
      EditorRawDOMPoint atBrNode(brContent);
      // Don't restore the selection
      restoreSelectionLater.Abort();
      ErrorResult error;
      SelectionRefPtr()->Collapse(atBrNode, error);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        error.SuppressException();
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(error.Failed())) {
        return error.StealNSResult();
      }
      return NS_OK;
    }

    // We are making a block.  Consume a br, if needed.
    nsCOMPtr<nsIContent> brNode =
        HTMLEditorRef().GetNextEditableHTMLNodeInBlock(pointToInsertBlock);
    if (brNode && brNode->IsHTMLElement(nsGkAtoms::br)) {
      AutoEditorDOMPointChildInvalidator lockOffset(pointToInsertBlock);
      rv = MOZ_KnownLive(HTMLEditorRef()).DeleteNodeWithTransaction(*brNode);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      // We don't need to act on this node any more
      arrayOfNodes.RemoveElement(brNode);
    }
    // Make sure we can put a block here.
    SplitNodeResult splitNodeResult =
        MaybeSplitAncestorsForInsertWithTransaction(blockType,
                                                    pointToInsertBlock);
    if (NS_WARN_IF(splitNodeResult.Failed())) {
      return splitNodeResult.Rv();
    }
    RefPtr<Element> block =
        MOZ_KnownLive(HTMLEditorRef())
            .CreateNodeWithTransaction(blockType, splitNodeResult.SplitPoint());
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(!block)) {
      return NS_ERROR_FAILURE;
    }
    // Remember our new block for postprocessing
    HTMLEditorRef().TopLevelEditSubActionDataRef().mNewBlockElement = block;
    // Delete anything that was in the list of nodes
    while (!arrayOfNodes.IsEmpty()) {
      OwningNonNull<nsINode> curNode = arrayOfNodes[0];
      rv = MOZ_KnownLive(HTMLEditorRef()).DeleteNodeWithTransaction(*curNode);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      arrayOfNodes.RemoveElementAt(0);
    }
    // Don't restore the selection
    restoreSelectionLater.Abort();
    // Put selection in new block
    rv = SelectionRefPtr()->Collapse(block, 0);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }
  // Okay, now go through all the nodes and make the right kind of blocks, or
  // whatever is approriate.  Woohoo!  Note: blockquote is handled a little
  // differently.
  if (&blockType == nsGkAtoms::blockquote) {
    rv = MakeBlockquote(arrayOfNodes);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else if (&blockType == nsGkAtoms::normal ||
             &blockType == nsGkAtoms::_empty) {
    rv = RemoveBlockStyle(arrayOfNodes);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    rv = ApplyBlockStyle(arrayOfNodes, blockType);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  return NS_OK;
}

nsresult HTMLEditRules::DidMakeBasicBlock() {
  MOZ_ASSERT(IsEditorDataAvailable());

  // check for empty block.  if so, put a moz br in it.
  if (!SelectionRefPtr()->IsCollapsed()) {
    return NS_OK;
  }

  nsRange* firstRange = SelectionRefPtr()->GetRangeAt(0);
  if (NS_WARN_IF(!firstRange)) {
    return NS_ERROR_FAILURE;
  }
  const RangeBoundary& atStartOfSelection = firstRange->StartRef();
  if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv = InsertPaddingBRElementForEmptyLastLineIfNeeded(
      MOZ_KnownLive(*atStartOfSelection.Container()));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult HTMLEditRules::WillIndent(bool* aCancel, bool* aHandled) {
  MOZ_ASSERT(IsEditorDataAvailable());

  if (HTMLEditorRef().IsCSSEnabled()) {
    nsresult rv = WillCSSIndent(aCancel, aHandled);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    nsresult rv = WillHTMLIndent(aCancel, aHandled);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  return NS_OK;
}

nsresult HTMLEditRules::WillCSSIndent(bool* aCancel, bool* aHandled) {
  MOZ_ASSERT(IsEditorDataAvailable());

  if (NS_WARN_IF(!aCancel) || NS_WARN_IF(!aHandled)) {
    return NS_ERROR_INVALID_ARG;
  }

  // FYI: Ignore cancel result of WillInsert().
  nsresult rv = MOZ_KnownLive(HTMLEditorRef()).WillInsert();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "WillInsert() failed");

  *aCancel = false;
  *aHandled = true;

  rv = NormalizeSelection();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // IndentAroundSelectionWithCSS() creates AutoSelectionRestorer.
  // Therefore, even if it returns NS_OK, editor might have been destroyed
  // at restoring Selection.
  rv = IndentAroundSelectionWithCSS();
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult HTMLEditRules::IndentAroundSelectionWithCSS() {
  MOZ_ASSERT(IsEditorDataAvailable());

  AutoSelectionRestorer restoreSelectionLater(HTMLEditorRef());
  nsTArray<OwningNonNull<nsRange>> arrayOfRanges;
  AutoTArray<OwningNonNull<nsINode>, 64> arrayOfNodes;

  // short circuit: detect case of collapsed selection inside an <li>.
  // just sublist that <li>.  This prevents bug 97797.

  nsCOMPtr<Element> liNode;
  if (SelectionRefPtr()->IsCollapsed()) {
    EditorRawDOMPoint selectionStartPoint(
        EditorBase::GetStartPoint(*SelectionRefPtr()));
    if (NS_WARN_IF(!selectionStartPoint.IsSet())) {
      return NS_ERROR_FAILURE;
    }
    Element* block =
        HTMLEditorRef().GetBlock(*selectionStartPoint.GetContainer());
    if (block && HTMLEditUtils::IsListItem(block)) {
      liNode = block;
    }
  }

  if (liNode) {
    arrayOfNodes.AppendElement(*liNode);
  } else {
    nsresult rv =
        MOZ_KnownLive(HTMLEditorRef())
            .SplitInlinesAndCollectEditTargetNodesInExtendedSelectionRanges(
                arrayOfNodes, EditSubAction::eIndent);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // if nothing visible in list, make an empty block
  if (ListIsEmptyLine(arrayOfNodes)) {
    // get selection location
    nsRange* firstRange = SelectionRefPtr()->GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return NS_ERROR_FAILURE;
    }

    EditorDOMPoint atStartOfSelection(firstRange->StartRef());
    if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
      return NS_ERROR_FAILURE;
    }

    // make sure we can put a block here
    SplitNodeResult splitNodeResult =
        MaybeSplitAncestorsForInsertWithTransaction(*nsGkAtoms::div,
                                                    atStartOfSelection);
    if (NS_WARN_IF(splitNodeResult.Failed())) {
      return splitNodeResult.Rv();
    }
    RefPtr<Element> theBlock =
        MOZ_KnownLive(HTMLEditorRef())
            .CreateNodeWithTransaction(*nsGkAtoms::div,
                                       splitNodeResult.SplitPoint());
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(!theBlock)) {
      return NS_ERROR_FAILURE;
    }
    // remember our new block for postprocessing
    HTMLEditorRef().TopLevelEditSubActionDataRef().mNewBlockElement = theBlock;
    nsresult rv = IncreaseMarginToIndent(*theBlock);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to increase indentation");
    // delete anything that was in the list of nodes
    while (!arrayOfNodes.IsEmpty()) {
      OwningNonNull<nsINode> curNode = arrayOfNodes[0];
      rv = MOZ_KnownLive(HTMLEditorRef()).DeleteNodeWithTransaction(*curNode);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      arrayOfNodes.RemoveElementAt(0);
    }
    // put selection in new block
    EditorRawDOMPoint atStartOfTheBlock(theBlock, 0);
    // Don't restore the selection
    restoreSelectionLater.Abort();
    ErrorResult error;
    SelectionRefPtr()->Collapse(atStartOfTheBlock, error);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      error.SuppressException();
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(!error.Failed())) {
      return error.StealNSResult();
    }
    return NS_OK;
  }

  // Ok, now go through all the nodes and put them in a blockquote,
  // or whatever is appropriate.  Wohoo!
  nsCOMPtr<Element> curList, curQuote;
  nsCOMPtr<nsIContent> sibling;
  for (OwningNonNull<nsINode>& curNode : arrayOfNodes) {
    // Here's where we actually figure out what to do.
    EditorDOMPoint atCurNode(curNode);
    if (NS_WARN_IF(!atCurNode.IsSet())) {
      continue;
    }

    // Ignore all non-editable nodes.  Leave them be.
    if (!HTMLEditorRef().IsEditable(curNode)) {
      continue;
    }

    // some logic for putting list items into nested lists...
    if (HTMLEditUtils::IsList(atCurNode.GetContainer())) {
      // Check for whether we should join a list that follows curNode.
      // We do this if the next element is a list, and the list is of the
      // same type (li/ol) as curNode was a part it.
      sibling = HTMLEditorRef().GetNextHTMLSibling(curNode);
      if (sibling && HTMLEditUtils::IsList(sibling) &&
          atCurNode.GetContainer()->NodeInfo()->NameAtom() ==
              sibling->NodeInfo()->NameAtom() &&
          atCurNode.GetContainer()->NodeInfo()->NamespaceID() ==
              sibling->NodeInfo()->NamespaceID()) {
        nsresult rv =
            MOZ_KnownLive(HTMLEditorRef())
                .MoveNodeWithTransaction(MOZ_KnownLive(*curNode->AsContent()),
                                         EditorDOMPoint(sibling, 0));
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        continue;
      }

      // Check for whether we should join a list that preceeds curNode.
      // We do this if the previous element is a list, and the list is of
      // the same type (li/ol) as curNode was a part of.
      sibling = HTMLEditorRef().GetPriorHTMLSibling(curNode);
      if (sibling && HTMLEditUtils::IsList(sibling) &&
          atCurNode.GetContainer()->NodeInfo()->NameAtom() ==
              sibling->NodeInfo()->NameAtom() &&
          atCurNode.GetContainer()->NodeInfo()->NamespaceID() ==
              sibling->NodeInfo()->NamespaceID()) {
        nsresult rv = MOZ_KnownLive(HTMLEditorRef())
                          .MoveNodeToEndWithTransaction(
                              MOZ_KnownLive(*curNode->AsContent()), *sibling);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        continue;
      }

      // check to see if curList is still appropriate.  Which it is if
      // curNode is still right after it in the same list.
      sibling = nullptr;
      if (curList) {
        sibling = HTMLEditorRef().GetPriorHTMLSibling(curNode);
      }

      if (!curList || (sibling && sibling != curList)) {
        nsAtom* containerName =
            atCurNode.GetContainer()->NodeInfo()->NameAtom();
        // Create a new nested list of correct type.
        SplitNodeResult splitNodeResult =
            MaybeSplitAncestorsForInsertWithTransaction(
                MOZ_KnownLive(*containerName), atCurNode);
        if (NS_WARN_IF(splitNodeResult.Failed())) {
          return splitNodeResult.Rv();
        }
        curList = MOZ_KnownLive(HTMLEditorRef())
                      .CreateNodeWithTransaction(MOZ_KnownLive(*containerName),
                                                 splitNodeResult.SplitPoint());
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(!curList)) {
          return NS_ERROR_FAILURE;
        }
        // curList is now the correct thing to put curNode in
        // remember our new block for postprocessing
        HTMLEditorRef().TopLevelEditSubActionDataRef().mNewBlockElement =
            curList;
      }
      // tuck the node into the end of the active list
      nsresult rv = MOZ_KnownLive(HTMLEditorRef())
                        .MoveNodeToEndWithTransaction(
                            MOZ_KnownLive(*curNode->AsContent()), *curList);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      continue;
    }

    // Not a list item.

    if (HTMLEditor::NodeIsBlockStatic(*curNode)) {
      nsresult rv =
          IncreaseMarginToIndent(MOZ_KnownLive(*curNode->AsElement()));
      if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to inrease indentation");
      curQuote = nullptr;
      continue;
    }

    if (!curQuote) {
      // First, check that our element can contain a div.
      if (!HTMLEditorRef().CanContainTag(*atCurNode.GetContainer(),
                                         *nsGkAtoms::div)) {
        return NS_OK;  // cancelled
      }

      SplitNodeResult splitNodeResult =
          MaybeSplitAncestorsForInsertWithTransaction(*nsGkAtoms::div,
                                                      atCurNode);
      if (NS_WARN_IF(splitNodeResult.Failed())) {
        return splitNodeResult.Rv();
      }
      curQuote = MOZ_KnownLive(HTMLEditorRef())
                     .CreateNodeWithTransaction(*nsGkAtoms::div,
                                                splitNodeResult.SplitPoint());
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(!curQuote)) {
        return NS_ERROR_FAILURE;
      }
      nsresult rv = IncreaseMarginToIndent(*curQuote);
      if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to increase indentation");
      // remember our new block for postprocessing
      HTMLEditorRef().TopLevelEditSubActionDataRef().mNewBlockElement =
          curQuote;
      // curQuote is now the correct thing to put curNode in
    }

    // tuck the node into the end of the active blockquote
    nsresult rv = MOZ_KnownLive(HTMLEditorRef())
                      .MoveNodeToEndWithTransaction(
                          MOZ_KnownLive(*curNode->AsContent()), *curQuote);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  return NS_OK;
}

nsresult HTMLEditRules::WillHTMLIndent(bool* aCancel, bool* aHandled) {
  MOZ_ASSERT(IsEditorDataAvailable());

  if (NS_WARN_IF(!aCancel) || NS_WARN_IF(!aHandled)) {
    return NS_ERROR_INVALID_ARG;
  }

  // FYI: Ignore cancel result of WillInsert().
  nsresult rv = MOZ_KnownLive(HTMLEditorRef()).WillInsert();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "WillInsert() failed");

  *aCancel = false;
  *aHandled = true;

  rv = NormalizeSelection();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // IndentAroundSelectionWithHTML() creates AutoSelectionRestorer.
  // Therefore, even if it returns NS_OK, editor might have been destroyed
  // at restoring Selection.
  rv = IndentAroundSelectionWithHTML();
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult HTMLEditRules::IndentAroundSelectionWithHTML() {
  MOZ_ASSERT(IsEditorDataAvailable());

  AutoSelectionRestorer restoreSelectionLater(HTMLEditorRef());

  // convert the selection ranges into "promoted" selection ranges:
  // this basically just expands the range to include the immediate
  // block parent, and then further expands to include any ancestors
  // whose children are all in the range

  AutoTArray<RefPtr<nsRange>, 4> arrayOfRanges;
  HTMLEditorRef().GetSelectionRangesExtendedToHardLineStartAndEnd(
      arrayOfRanges, EditSubAction::eIndent);

  // use these ranges to contruct a list of nodes to act on.
  nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
  nsresult rv = MOZ_KnownLive(HTMLEditorRef())
                    .SplitInlinesAndCollectEditTargetNodes(
                        arrayOfRanges, arrayOfNodes, EditSubAction::eIndent);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // if nothing visible in list, make an empty block
  if (ListIsEmptyLine(arrayOfNodes)) {
    nsRange* firstRange = SelectionRefPtr()->GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return NS_ERROR_FAILURE;
    }

    EditorDOMPoint atStartOfSelection(firstRange->StartRef());
    if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
      return NS_ERROR_FAILURE;
    }

    // Make sure we can put a block here.
    SplitNodeResult splitNodeResult =
        MaybeSplitAncestorsForInsertWithTransaction(*nsGkAtoms::blockquote,
                                                    atStartOfSelection);
    if (NS_WARN_IF(splitNodeResult.Failed())) {
      return splitNodeResult.Rv();
    }
    RefPtr<Element> theBlock =
        MOZ_KnownLive(HTMLEditorRef())
            .CreateNodeWithTransaction(*nsGkAtoms::blockquote,
                                       splitNodeResult.SplitPoint());
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(!theBlock)) {
      return NS_ERROR_FAILURE;
    }
    // remember our new block for postprocessing
    HTMLEditorRef().TopLevelEditSubActionDataRef().mNewBlockElement = theBlock;
    // delete anything that was in the list of nodes
    while (!arrayOfNodes.IsEmpty()) {
      OwningNonNull<nsINode> curNode = arrayOfNodes[0];
      rv = MOZ_KnownLive(HTMLEditorRef()).DeleteNodeWithTransaction(*curNode);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      arrayOfNodes.RemoveElementAt(0);
    }
    EditorRawDOMPoint atStartOfTheBlock(theBlock, 0);
    // Don't restore the selection
    restoreSelectionLater.Abort();
    ErrorResult error;
    SelectionRefPtr()->Collapse(atStartOfTheBlock, error);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      error.SuppressException();
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(!error.Failed())) {
      return error.StealNSResult();
    }
    return NS_OK;
  }

  // Ok, now go through all the nodes and put them in a blockquote,
  // or whatever is appropriate.  Wohoo!
  nsCOMPtr<nsIContent> sibling;
  nsCOMPtr<Element> curList, curQuote, indentedLI;
  for (OwningNonNull<nsINode>& curNode : arrayOfNodes) {
    // Here's where we actually figure out what to do.
    EditorDOMPoint atCurNode(curNode);
    if (NS_WARN_IF(!atCurNode.IsSet())) {
      continue;
    }

    // Ignore all non-editable nodes.  Leave them be.
    if (!HTMLEditorRef().IsEditable(curNode)) {
      continue;
    }

    // some logic for putting list items into nested lists...
    if (HTMLEditUtils::IsList(atCurNode.GetContainer())) {
      // Check for whether we should join a list that follows curNode.
      // We do this if the next element is a list, and the list is of the
      // same type (li/ol) as curNode was a part it.
      sibling = HTMLEditorRef().GetNextHTMLSibling(curNode);
      if (sibling && HTMLEditUtils::IsList(sibling) &&
          atCurNode.GetContainer()->NodeInfo()->NameAtom() ==
              sibling->NodeInfo()->NameAtom() &&
          atCurNode.GetContainer()->NodeInfo()->NamespaceID() ==
              sibling->NodeInfo()->NamespaceID()) {
        rv = MOZ_KnownLive(HTMLEditorRef())
                 .MoveNodeWithTransaction(MOZ_KnownLive(*curNode->AsContent()),
                                          EditorDOMPoint(sibling, 0));
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        continue;
      }

      // Check for whether we should join a list that preceeds curNode.
      // We do this if the previous element is a list, and the list is of
      // the same type (li/ol) as curNode was a part of.
      sibling = HTMLEditorRef().GetPriorHTMLSibling(curNode);
      if (sibling && HTMLEditUtils::IsList(sibling) &&
          atCurNode.GetContainer()->NodeInfo()->NameAtom() ==
              sibling->NodeInfo()->NameAtom() &&
          atCurNode.GetContainer()->NodeInfo()->NamespaceID() ==
              sibling->NodeInfo()->NamespaceID()) {
        rv = MOZ_KnownLive(HTMLEditorRef())
                 .MoveNodeToEndWithTransaction(
                     MOZ_KnownLive(*curNode->AsContent()), *sibling);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        continue;
      }

      // check to see if curList is still appropriate.  Which it is if
      // curNode is still right after it in the same list.
      sibling = nullptr;
      if (curList) {
        sibling = HTMLEditorRef().GetPriorHTMLSibling(curNode);
      }

      if (!curList || (sibling && sibling != curList)) {
        nsAtom* containerName =
            atCurNode.GetContainer()->NodeInfo()->NameAtom();
        // Create a new nested list of correct type.
        SplitNodeResult splitNodeResult =
            MaybeSplitAncestorsForInsertWithTransaction(
                MOZ_KnownLive(*containerName), atCurNode);
        if (NS_WARN_IF(splitNodeResult.Failed())) {
          return splitNodeResult.Rv();
        }
        curList = MOZ_KnownLive(HTMLEditorRef())
                      .CreateNodeWithTransaction(MOZ_KnownLive(*containerName),
                                                 splitNodeResult.SplitPoint());
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(!curList)) {
          return NS_ERROR_FAILURE;
        }
        // curList is now the correct thing to put curNode in
        // remember our new block for postprocessing
        HTMLEditorRef().TopLevelEditSubActionDataRef().mNewBlockElement =
            curList;
      }
      // tuck the node into the end of the active list
      rv = MOZ_KnownLive(HTMLEditorRef())
               .MoveNodeToEndWithTransaction(
                   MOZ_KnownLive(*curNode->AsContent()), *curList);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
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
    RefPtr<Element> listItem = IsInListItem(curNode);
    if (listItem) {
      if (indentedLI == listItem) {
        // already indented this list item
        continue;
      }
      // check to see if curList is still appropriate.  Which it is if
      // curNode is still right after it in the same list.
      if (curList) {
        sibling = HTMLEditorRef().GetPriorHTMLSibling(listItem);
      }

      if (!curList || (sibling && sibling != curList)) {
        EditorDOMPoint atListItem(listItem);
        if (NS_WARN_IF(!listItem)) {
          return NS_ERROR_FAILURE;
        }
        nsAtom* containerName =
            atListItem.GetContainer()->NodeInfo()->NameAtom();
        // Create a new nested list of correct type.
        SplitNodeResult splitNodeResult =
            MaybeSplitAncestorsForInsertWithTransaction(
                MOZ_KnownLive(*containerName), atListItem);
        if (NS_WARN_IF(splitNodeResult.Failed())) {
          return splitNodeResult.Rv();
        }
        curList = MOZ_KnownLive(HTMLEditorRef())
                      .CreateNodeWithTransaction(MOZ_KnownLive(*containerName),
                                                 splitNodeResult.SplitPoint());
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(!curList)) {
          return NS_ERROR_FAILURE;
        }
      }

      rv = MOZ_KnownLive(HTMLEditorRef())
               .MoveNodeToEndWithTransaction(*listItem, *curList);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
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
    if (curQuote && InDifferentTableElements(curQuote, curNode)) {
      curQuote = nullptr;
    }

    if (!curQuote) {
      // First, check that our element can contain a blockquote.
      if (!HTMLEditorRef().CanContainTag(*atCurNode.GetContainer(),
                                         *nsGkAtoms::blockquote)) {
        return NS_OK;  // cancelled
      }

      SplitNodeResult splitNodeResult =
          MaybeSplitAncestorsForInsertWithTransaction(*nsGkAtoms::blockquote,
                                                      atCurNode);
      if (NS_WARN_IF(splitNodeResult.Failed())) {
        return splitNodeResult.Rv();
      }
      curQuote = MOZ_KnownLive(HTMLEditorRef())
                     .CreateNodeWithTransaction(*nsGkAtoms::blockquote,
                                                splitNodeResult.SplitPoint());
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(!curQuote)) {
        return NS_ERROR_FAILURE;
      }
      // remember our new block for postprocessing
      HTMLEditorRef().TopLevelEditSubActionDataRef().mNewBlockElement =
          curQuote;
      // curQuote is now the correct thing to put curNode in
    }

    // tuck the node into the end of the active blockquote
    rv = MOZ_KnownLive(HTMLEditorRef())
             .MoveNodeToEndWithTransaction(MOZ_KnownLive(*curNode->AsContent()),
                                           *curQuote);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    // forget curList, if any
    curList = nullptr;
  }
  return NS_OK;
}

nsresult HTMLEditRules::WillOutdent(bool* aCancel, bool* aHandled) {
  MOZ_ASSERT(IsEditorDataAvailable());
  MOZ_ASSERT(aCancel && aHandled);
  *aCancel = false;
  *aHandled = true;

  nsresult rv = NormalizeSelection();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // OutdentAroundSelection() creates AutoSelectionRestorer.  Therefore,
  // even if it returns NS_OK, the editor might have been destroyed at
  // restoring Selection.
  SplitRangeOffFromNodeResult outdentResult = OutdentAroundSelection();
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(outdentResult.Failed())) {
    return outdentResult.Rv();
  }

  // Make sure selection didn't stick to last piece of content in old bq (only
  // a problem for collapsed selections)
  if (!outdentResult.GetLeftContent() && !outdentResult.GetRightContent()) {
    return NS_OK;
  }

  if (!SelectionRefPtr()->IsCollapsed()) {
    return NS_OK;
  }

  // Push selection past end of left element of last split indented element.
  if (outdentResult.GetLeftContent()) {
    nsRange* firstRange = SelectionRefPtr()->GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return NS_OK;
    }
    const RangeBoundary& atStartOfSelection = firstRange->StartRef();
    if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
      return NS_ERROR_FAILURE;
    }
    if (atStartOfSelection.Container() == outdentResult.GetLeftContent() ||
        EditorUtils::IsDescendantOf(*atStartOfSelection.Container(),
                                    *outdentResult.GetLeftContent())) {
      // Selection is inside the left node - push it past it.
      EditorRawDOMPoint afterRememberedLeftBQ(outdentResult.GetLeftContent());
      afterRememberedLeftBQ.AdvanceOffset();
      IgnoredErrorResult error;
      SelectionRefPtr()->Collapse(afterRememberedLeftBQ, error);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(
          !error.Failed(),
          "Failed to collapse selection after the left <blockquote>");
    }
  }
  // And pull selection before beginning of right element of last split
  // indented element.
  if (outdentResult.GetRightContent()) {
    nsRange* firstRange = SelectionRefPtr()->GetRangeAt(0);
    const RangeBoundary& atStartOfSelection = firstRange->StartRef();
    if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
      return NS_ERROR_FAILURE;
    }
    if (atStartOfSelection.Container() == outdentResult.GetRightContent() ||
        EditorUtils::IsDescendantOf(*atStartOfSelection.Container(),
                                    *outdentResult.GetRightContent())) {
      // Selection is inside the right element - push it before it.
      EditorRawDOMPoint atRememberedRightBQ(outdentResult.GetRightContent());
      IgnoredErrorResult error;
      SelectionRefPtr()->Collapse(atRememberedRightBQ, error);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(
          !error.Failed(),
          "Failed to collapse selection after the right <blockquote>");
    }
  }
  return NS_OK;
}

SplitRangeOffFromNodeResult HTMLEditRules::OutdentAroundSelection() {
  MOZ_ASSERT(IsEditorDataAvailable());

  AutoSelectionRestorer restoreSelectionLater(HTMLEditorRef());

  bool useCSS = HTMLEditorRef().IsCSSEnabled();

  // Convert the selection ranges into "promoted" selection ranges: this
  // basically just expands the range to include the immediate block parent,
  // and then further expands to include any ancestors whose children are all
  // in the range
  AutoTArray<OwningNonNull<nsINode>, 64> arrayOfNodes;
  nsresult rv =
      MOZ_KnownLive(HTMLEditorRef())
          .SplitInlinesAndCollectEditTargetNodesInExtendedSelectionRanges(
              arrayOfNodes, EditSubAction::eOutdent);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return SplitRangeOffFromNodeResult(rv);
  }

  // Okay, now go through all the nodes and remove a level of blockquoting,
  // or whatever is appropriate.  Wohoo!

  nsCOMPtr<nsIContent> leftContentOfLastOutdented;
  nsCOMPtr<nsIContent> middleContentOfLastOutdented;
  nsCOMPtr<nsIContent> rightContentOfLastOutdented;
  nsCOMPtr<Element> curBlockQuote;
  nsCOMPtr<nsIContent> firstBQChild, lastBQChild;
  bool curBlockQuoteIsIndentedWithCSS = false;
  for (uint32_t i = 0; i < arrayOfNodes.Length(); i++) {
    if (!arrayOfNodes[i]->IsContent()) {
      continue;
    }
    OwningNonNull<nsIContent> curNode = *arrayOfNodes[i]->AsContent();

    // Here's where we actually figure out what to do
    int32_t offset;
    nsCOMPtr<nsINode> curParent = EditorBase::GetNodeLocation(curNode, &offset);
    if (!curParent) {
      continue;
    }

    // Is it a blockquote?
    if (curNode->IsHTMLElement(nsGkAtoms::blockquote)) {
      // If it is a blockquote, remove it.  So we need to finish up dealng
      // with any curBlockQuote first.
      if (curBlockQuote) {
        SplitRangeOffFromNodeResult outdentResult =
            OutdentPartOfBlock(*curBlockQuote, *firstBQChild, *lastBQChild,
                               curBlockQuoteIsIndentedWithCSS);
        if (NS_WARN_IF(outdentResult.Failed())) {
          return outdentResult;
        }
        leftContentOfLastOutdented = outdentResult.GetLeftContent();
        middleContentOfLastOutdented = outdentResult.GetMiddleContent();
        rightContentOfLastOutdented = outdentResult.GetRightContent();
        curBlockQuote = nullptr;
        firstBQChild = nullptr;
        lastBQChild = nullptr;
        curBlockQuoteIsIndentedWithCSS = false;
      }
      rv = MOZ_KnownLive(HTMLEditorRef())
               .RemoveBlockContainerWithTransaction(
                   MOZ_KnownLive(*curNode->AsElement()));
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return SplitRangeOffFromNodeResult(NS_ERROR_EDITOR_DESTROYED);
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return SplitRangeOffFromNodeResult(rv);
      }
      continue;
    }

    // Is it a block with a 'margin' property?
    if (useCSS && HTMLEditor::NodeIsBlockStatic(curNode)) {
      nsAtom& marginProperty = MarginPropertyAtomForIndent(curNode);
      nsAutoString value;
      CSSEditUtils::GetSpecifiedProperty(curNode, marginProperty, value);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return SplitRangeOffFromNodeResult(NS_ERROR_EDITOR_DESTROYED);
      }
      float f;
      RefPtr<nsAtom> unit;
      CSSEditUtils::ParseLength(value, &f, getter_AddRefs(unit));
      if (f > 0) {
        nsresult rv =
            DecreaseMarginToOutdent(MOZ_KnownLive(*curNode->AsElement()));
        if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
          return SplitRangeOffFromNodeResult(NS_ERROR_EDITOR_DESTROYED);
        }
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "Failed to decrease indentation");
        continue;
      }
    }

    // Is it a list item?
    if (HTMLEditUtils::IsListItem(curNode)) {
      // If it is a list item, that means we are not outdenting whole list.
      // So we need to finish up dealing with any curBlockQuote, and then pop
      // this list item.
      if (curBlockQuote) {
        SplitRangeOffFromNodeResult outdentResult =
            OutdentPartOfBlock(*curBlockQuote, *firstBQChild, *lastBQChild,
                               curBlockQuoteIsIndentedWithCSS);
        if (NS_WARN_IF(outdentResult.Failed())) {
          return outdentResult;
        }
        leftContentOfLastOutdented = outdentResult.GetLeftContent();
        middleContentOfLastOutdented = outdentResult.GetMiddleContent();
        rightContentOfLastOutdented = outdentResult.GetRightContent();
        curBlockQuote = nullptr;
        firstBQChild = nullptr;
        lastBQChild = nullptr;
        curBlockQuoteIsIndentedWithCSS = false;
      }
      rv = PopListItem(MOZ_KnownLive(*curNode->AsContent()));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return SplitRangeOffFromNodeResult(rv);
      }
      continue;
    }

    // Do we have a blockquote that we are already committed to removing?
    if (curBlockQuote) {
      // If so, is this node a descendant?
      if (EditorUtils::IsDescendantOf(*curNode, *curBlockQuote)) {
        lastBQChild = curNode;
        // Then we don't need to do anything different for this node
        continue;
      }
      // Otherwise, we have progressed beyond end of curBlockQuote, so
      // let's handle it now.  We need to remove the portion of
      // curBlockQuote that contains [firstBQChild - lastBQChild].
      SplitRangeOffFromNodeResult outdentResult =
          OutdentPartOfBlock(*curBlockQuote, *firstBQChild, *lastBQChild,
                             curBlockQuoteIsIndentedWithCSS);
      if (NS_WARN_IF(outdentResult.Failed())) {
        return outdentResult;
      }
      leftContentOfLastOutdented = outdentResult.GetLeftContent();
      middleContentOfLastOutdented = outdentResult.GetMiddleContent();
      rightContentOfLastOutdented = outdentResult.GetRightContent();
      curBlockQuote = nullptr;
      firstBQChild = nullptr;
      lastBQChild = nullptr;
      curBlockQuoteIsIndentedWithCSS = false;
      // Fall out and handle curNode
    }

    // Are we inside a blockquote?
    OwningNonNull<nsINode> n = curNode;
    curBlockQuoteIsIndentedWithCSS = false;
    // Keep looking up the hierarchy as long as we don't hit the body or the
    // active editing host or a table element (other than an entire table)
    while (!n->IsHTMLElement(nsGkAtoms::body) &&
           HTMLEditorRef().IsDescendantOfEditorRoot(n) &&
           (n->IsHTMLElement(nsGkAtoms::table) ||
            !HTMLEditUtils::IsTableElement(n))) {
      if (!n->GetParentNode()) {
        break;
      }
      n = *n->GetParentNode();
      if (n->IsHTMLElement(nsGkAtoms::blockquote)) {
        // If so, remember it and the first node we are taking out of it.
        curBlockQuote = n->AsElement();
        firstBQChild = curNode;
        lastBQChild = curNode;
        break;
      }

      if (!useCSS) {
        continue;
      }

      nsAtom& marginProperty = MarginPropertyAtomForIndent(curNode);
      nsAutoString value;
      CSSEditUtils::GetSpecifiedProperty(*n, marginProperty, value);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return SplitRangeOffFromNodeResult(NS_ERROR_EDITOR_DESTROYED);
      }
      float f;
      RefPtr<nsAtom> unit;
      CSSEditUtils::ParseLength(value, &f, getter_AddRefs(unit));
      if (f > 0 && !(HTMLEditUtils::IsList(curParent) &&
                     HTMLEditUtils::IsList(curNode))) {
        curBlockQuote = n->AsElement();
        firstBQChild = curNode;
        lastBQChild = curNode;
        curBlockQuoteIsIndentedWithCSS = true;
        break;
      }
    }

    if (curBlockQuote) {
      continue;
    }

    // Couldn't find enclosing blockquote.
    if (HTMLEditUtils::IsList(curParent)) {
      // Move node out of list
      if (HTMLEditUtils::IsList(curNode)) {
        // Just unwrap this sublist
        rv = MOZ_KnownLive(HTMLEditorRef())
                 .RemoveBlockContainerWithTransaction(
                     MOZ_KnownLive(*curNode->AsElement()));
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return SplitRangeOffFromNodeResult(NS_ERROR_EDITOR_DESTROYED);
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return SplitRangeOffFromNodeResult(rv);
        }
      }
      continue;
    }

    if (HTMLEditUtils::IsList(curNode)) {
      // node is a list, but parent is non-list: move list items out
      nsCOMPtr<nsIContent> child = curNode->GetLastChild();
      while (child) {
        if (HTMLEditUtils::IsListItem(child)) {
          rv = PopListItem(*child);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return SplitRangeOffFromNodeResult(rv);
          }
        } else if (HTMLEditUtils::IsList(child)) {
          // We have an embedded list, so move it out from under the parent
          // list. Be sure to put it after the parent list because this
          // loop iterates backwards through the parent's list of children.
          EditorDOMPoint afterCurrentList(curParent, offset + 1);
          rv = MOZ_KnownLive(HTMLEditorRef())
                   .MoveNodeWithTransaction(*child, afterCurrentList);
          if (NS_WARN_IF(!CanHandleEditAction())) {
            return SplitRangeOffFromNodeResult(NS_ERROR_EDITOR_DESTROYED);
          }
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return SplitRangeOffFromNodeResult(rv);
          }
        } else {
          // Delete any non-list items for now
          rv = MOZ_KnownLive(HTMLEditorRef()).DeleteNodeWithTransaction(*child);
          if (NS_WARN_IF(!CanHandleEditAction())) {
            return SplitRangeOffFromNodeResult(NS_ERROR_EDITOR_DESTROYED);
          }
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return SplitRangeOffFromNodeResult(rv);
          }
        }
        child = curNode->GetLastChild();
      }
      // Delete the now-empty list
      rv = MOZ_KnownLive(HTMLEditorRef())
               .RemoveBlockContainerWithTransaction(
                   MOZ_KnownLive(*curNode->AsElement()));
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return SplitRangeOffFromNodeResult(NS_ERROR_EDITOR_DESTROYED);
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return SplitRangeOffFromNodeResult(rv);
      }
      continue;
    }

    if (useCSS) {
      nsCOMPtr<Element> element;
      if (curNode->GetAsText()) {
        // We want to outdent the parent of text nodes
        element = curNode->GetParentElement();
      } else if (curNode->IsElement()) {
        element = curNode->AsElement();
      }
      if (element) {
        nsresult rv = DecreaseMarginToOutdent(*element);
        if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
          return SplitRangeOffFromNodeResult(NS_ERROR_EDITOR_DESTROYED);
        }
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "Failed to decrease indentation");
      }
      continue;
    }
  }

  if (!curBlockQuote) {
    return SplitRangeOffFromNodeResult(leftContentOfLastOutdented,
                                       middleContentOfLastOutdented,
                                       rightContentOfLastOutdented);
  }

  // We have a <blockquote> we haven't finished handling.
  SplitRangeOffFromNodeResult outdentResult =
      OutdentPartOfBlock(*curBlockQuote, *firstBQChild, *lastBQChild,
                         curBlockQuoteIsIndentedWithCSS);
  if (NS_WARN_IF(outdentResult.Failed())) {
    return outdentResult;
  }
  return outdentResult;
}

SplitRangeOffFromNodeResult
HTMLEditRules::SplitRangeOffFromBlockAndRemoveMiddleContainer(
    Element& aBlockElement, nsIContent& aStartOfRange,
    nsIContent& aEndOfRange) {
  MOZ_ASSERT(IsEditorDataAvailable());

  SplitRangeOffFromNodeResult splitResult =
      SplitRangeOffFromBlock(aBlockElement, aStartOfRange, aEndOfRange);
  if (NS_WARN_IF(splitResult.Rv() == NS_ERROR_EDITOR_DESTROYED)) {
    return splitResult;
  }
  NS_WARNING_ASSERTION(splitResult.Succeeded(),
                       "Failed to split the range off from the block element");
  nsresult rv = MOZ_KnownLive(HTMLEditorRef())
                    .RemoveBlockContainerWithTransaction(aBlockElement);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return SplitRangeOffFromNodeResult(NS_ERROR_EDITOR_DESTROYED);
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return SplitRangeOffFromNodeResult(rv);
  }
  return SplitRangeOffFromNodeResult(splitResult.GetLeftContent(), nullptr,
                                     splitResult.GetRightContent());
}

SplitRangeOffFromNodeResult HTMLEditRules::SplitRangeOffFromBlock(
    Element& aBlockElement, nsIContent& aStartOfMiddleElement,
    nsIContent& aEndOfMiddleElement) {
  MOZ_ASSERT(IsEditorDataAvailable());

  // aStartOfMiddleElement and aEndOfMiddleElement must be exclusive
  // descendants of aBlockElement.
  MOZ_ASSERT(EditorUtils::IsDescendantOf(aStartOfMiddleElement, aBlockElement));
  MOZ_ASSERT(EditorUtils::IsDescendantOf(aEndOfMiddleElement, aBlockElement));

  // Split at the start.
  SplitNodeResult splitAtStartResult =
      MOZ_KnownLive(HTMLEditorRef())
          .SplitNodeDeepWithTransaction(
              aBlockElement, EditorDOMPoint(&aStartOfMiddleElement),
              SplitAtEdges::eDoNotCreateEmptyContainer);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return SplitRangeOffFromNodeResult(NS_ERROR_EDITOR_DESTROYED);
  }
  NS_WARNING_ASSERTION(splitAtStartResult.Succeeded(),
                       "Failed to split aBlockElement at start");

  // Split at after the end
  EditorDOMPoint atAfterEnd(&aEndOfMiddleElement);
  DebugOnly<bool> advanced = atAfterEnd.AdvanceOffset();
  NS_WARNING_ASSERTION(advanced, "Failed to advance offset after the end node");
  SplitNodeResult splitAtEndResult =
      MOZ_KnownLive(HTMLEditorRef())
          .SplitNodeDeepWithTransaction(
              aBlockElement, atAfterEnd,
              SplitAtEdges::eDoNotCreateEmptyContainer);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return SplitRangeOffFromNodeResult(NS_ERROR_EDITOR_DESTROYED);
  }
  NS_WARNING_ASSERTION(splitAtEndResult.Succeeded(),
                       "Failed to split aBlockElement at after end");

  return SplitRangeOffFromNodeResult(splitAtStartResult, splitAtEndResult);
}

SplitRangeOffFromNodeResult HTMLEditRules::OutdentPartOfBlock(
    Element& aBlockElement, nsIContent& aStartOfOutdent,
    nsIContent& aEndOfOutdent, bool aIsBlockIndentedWithCSS) {
  MOZ_ASSERT(IsEditorDataAvailable());

  SplitRangeOffFromNodeResult splitResult =
      SplitRangeOffFromBlock(aBlockElement, aStartOfOutdent, aEndOfOutdent);
  if (NS_WARN_IF(splitResult.Rv() == NS_ERROR_EDITOR_DESTROYED)) {
    return SplitRangeOffFromNodeResult(NS_ERROR_EDITOR_DESTROYED);
  }

  if (NS_WARN_IF(!splitResult.GetMiddleContentAsElement())) {
    return SplitRangeOffFromNodeResult(NS_ERROR_FAILURE);
  }

  if (!aIsBlockIndentedWithCSS) {
    nsresult rv = MOZ_KnownLive(HTMLEditorRef())
                      .RemoveBlockContainerWithTransaction(MOZ_KnownLive(
                          *splitResult.GetMiddleContentAsElement()));
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return SplitRangeOffFromNodeResult(NS_ERROR_EDITOR_DESTROYED);
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return SplitRangeOffFromNodeResult(rv);
    }
    return SplitRangeOffFromNodeResult(splitResult.GetLeftContent(), nullptr,
                                       splitResult.GetRightContent());
  }

  if (splitResult.GetMiddleContentAsElement()) {
    nsresult rv = DecreaseMarginToOutdent(
        MOZ_KnownLive(*splitResult.GetMiddleContentAsElement()));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return SplitRangeOffFromNodeResult(rv);
    }
    return splitResult;
  }

  return splitResult;
}

CreateElementResult HTMLEditRules::ConvertListType(Element& aListElement,
                                                   nsAtom& aNewListTag,
                                                   nsAtom& aNewListItemTag) {
  MOZ_ASSERT(IsEditorDataAvailable());

  nsCOMPtr<nsINode> child = aListElement.GetFirstChild();
  while (child) {
    if (child->IsElement()) {
      Element* element = child->AsElement();
      if (HTMLEditUtils::IsListItem(element) &&
          !element->IsHTMLElement(&aNewListItemTag)) {
        child = MOZ_KnownLive(HTMLEditorRef())
                    .ReplaceContainerWithTransaction(MOZ_KnownLive(*element),
                                                     aNewListItemTag);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return CreateElementResult(NS_ERROR_EDITOR_DESTROYED);
        }
        if (NS_WARN_IF(!child)) {
          return CreateElementResult(NS_ERROR_FAILURE);
        }
      } else if (HTMLEditUtils::IsList(element) &&
                 !element->IsHTMLElement(&aNewListTag)) {
        // XXX List elements shouldn't have other list elements as their
        //     child.  Why do we handle such invalid tree?
        CreateElementResult convertListTypeResult = ConvertListType(
            MOZ_KnownLive(*child->AsElement()), aNewListTag, aNewListItemTag);
        if (NS_WARN_IF(convertListTypeResult.Failed())) {
          return convertListTypeResult;
        }
        child = convertListTypeResult.forget();
      }
    }
    child = child->GetNextSibling();
  }

  if (aListElement.IsHTMLElement(&aNewListTag)) {
    return CreateElementResult(&aListElement);
  }

  RefPtr<Element> listElement =
      MOZ_KnownLive(HTMLEditorRef())
          .ReplaceContainerWithTransaction(aListElement, aNewListTag);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return CreateElementResult(NS_ERROR_EDITOR_DESTROYED);
  }
  NS_WARNING_ASSERTION(listElement != nullptr, "Failed to create list element");
  return CreateElementResult(listElement.forget());
}

nsresult HTMLEditor::CreateStyleForInsertText(AbstractRange& aAbstractRange) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(aAbstractRange.IsPositioned());
  MOZ_ASSERT(mTypeInState);

  nsCOMPtr<nsINode> node = aAbstractRange.GetStartContainer();
  int32_t offset = aAbstractRange.StartOffset();

  RefPtr<Element> documentRootElement = GetDocument()->GetRootElement();
  if (NS_WARN_IF(!documentRootElement)) {
    return NS_ERROR_FAILURE;
  }

  // process clearing any styles first
  UniquePtr<PropItem> item = mTypeInState->TakeClearProperty();

  bool weDidSomething = false;
  {
    // Transactions may set selection, but we will set selection if necessary.
    AutoTransactionsConserveSelection dontChangeMySelection(*this);

    while (item && node != documentRootElement) {
      // XXX If we redesign ClearStyle(), we can use EditorDOMPoint in this
      //     method.
      nsresult rv =
          ClearStyle(address_of(node), &offset, MOZ_KnownLive(item->tag),
                     MOZ_KnownLive(item->attr));
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      item = mTypeInState->TakeClearProperty();
      weDidSomething = true;
    }
  }

  // then process setting any styles
  int32_t relFontSize = mTypeInState->TakeRelativeFontSize();
  item = mTypeInState->TakeSetProperty();

  if (item || relFontSize) {
    // we have at least one style to add; make a new text node to insert style
    // nodes above.
    if (RefPtr<Text> text = node->GetAsText()) {
      // if we are in a text node, split it
      SplitNodeResult splitTextNodeResult = SplitNodeDeepWithTransaction(
          *text, EditorDOMPoint(text, offset),
          SplitAtEdges::eAllowToCreateEmptyContainer);
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(splitTextNodeResult.Failed())) {
        return splitTextNodeResult.Rv();
      }
      EditorRawDOMPoint splitPoint(splitTextNodeResult.SplitPoint());
      node = splitPoint.GetContainer();
      offset = splitPoint.Offset();
    }
    if (!IsContainer(node)) {
      return NS_OK;
    }
    RefPtr<Text> newNode = CreateTextNode(EmptyString());
    if (NS_WARN_IF(!newNode)) {
      return NS_ERROR_FAILURE;
    }
    nsresult rv =
        InsertNodeWithTransaction(*newNode, EditorDOMPoint(node, offset));
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    node = newNode;
    offset = 0;
    weDidSomething = true;

    if (relFontSize) {
      // dir indicated bigger versus smaller.  1 = bigger, -1 = smaller
      HTMLEditor::FontSize dir = relFontSize > 0 ? HTMLEditor::FontSize::incr
                                                 : HTMLEditor::FontSize::decr;
      for (int32_t j = 0; j < DeprecatedAbs(relFontSize); j++) {
        rv = RelativeFontChangeOnTextNode(dir, *newNode, 0, -1);
        if (NS_WARN_IF(Destroyed())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
    }

    while (item) {
      rv = SetInlinePropertyOnNode(MOZ_KnownLive(*node->AsContent()),
                                   MOZ_KnownLive(*item->tag),
                                   MOZ_KnownLive(item->attr), item->value);
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      item = mTypeInState->TakeSetProperty();
    }
  }

  if (!weDidSomething) {
    return NS_OK;
  }

  nsresult rv = SelectionRefPtr()->Collapse(node, offset);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Selection::Collapse() failed");
  return rv;
}

bool HTMLEditRules::IsEmptyBlockElement(Element& aElement,
                                        IgnoreSingleBR aIgnoreSingleBR) {
  MOZ_ASSERT(IsEditorDataAvailable());

  if (NS_WARN_IF(!HTMLEditor::NodeIsBlockStatic(aElement))) {
    return false;
  }
  bool isEmpty = true;
  nsresult rv = HTMLEditorRef().IsEmptyNode(
      &aElement, &isEmpty, aIgnoreSingleBR == IgnoreSingleBR::eYes);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }
  return isEmpty;
}

nsresult HTMLEditRules::WillAlign(const nsAString& aAlignType, bool* aCancel,
                                  bool* aHandled) {
  MOZ_ASSERT(IsEditorDataAvailable());
  MOZ_ASSERT(aCancel && aHandled);

  *aCancel = false;
  *aHandled = false;

  // FYI: Ignore cancel result of WillInsert().
  nsresult rv = MOZ_KnownLive(HTMLEditorRef()).WillInsert();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "WillInsert() failed");

  rv = NormalizeSelection();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aHandled = true;
  rv = AlignContentsAtSelection(aAlignType);
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED) ||
      NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult HTMLEditRules::AlignContentsAtSelection(const nsAString& aAlignType) {
  AutoSelectionRestorer restoreSelectionLater(HTMLEditorRef());

  // Convert the selection ranges into "promoted" selection ranges: This
  // basically just expands the range to include the immediate block parent,
  // and then further expands to include any ancestors whose children are all
  // in the range
  AutoTArray<OwningNonNull<nsINode>, 64> nodeArray;
  nsresult rv =
      MOZ_KnownLive(HTMLEditorRef())
          .SplitInlinesAndCollectEditTargetNodesInExtendedSelectionRanges(
              nodeArray, EditSubAction::eSetOrClearAlignment);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // If we don't have any nodes, or we have only a single br, then we are
  // creating an empty alignment div.  We have to do some different things for
  // these.
  bool emptyDiv = nodeArray.IsEmpty();
  if (nodeArray.Length() == 1) {
    OwningNonNull<nsINode> node = nodeArray[0];

    if (HTMLEditUtils::SupportsAlignAttr(*node)) {
      // The node is a table element, an hr, a paragraph, a div or a section
      // header; in HTML 4, it can directly carry the ALIGN attribute and we
      // don't need to make a div! If we are in CSS mode, all the work is done
      // in AlignBlock
      rv = AlignBlock(MOZ_KnownLive(*node->AsElement()), aAlignType,
                      ResetAlignOf::OnlyDescendants);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      return NS_OK;
    }

    if (TextEditUtils::IsBreak(node)) {
      // The special case emptyDiv code (below) that consumes BRs can cause
      // tables to split if the start node of the selection is not in a table
      // cell or caption, for example parent is a <tr>.  Avoid this unnecessary
      // splitting if possible by leaving emptyDiv FALSE so that we fall
      // through to the normal case alignment code.
      //
      // XXX: It seems a little error prone for the emptyDiv special case code
      // to assume that the start node of the selection is the parent of the
      // single node in the nodeArray, as the paragraph above points out. Do we
      // rely on the selection start node because of the fact that nodeArray
      // can be empty?  We should probably revisit this issue. - kin

      nsRange* firstRange = SelectionRefPtr()->GetRangeAt(0);
      if (NS_WARN_IF(!firstRange)) {
        return NS_ERROR_FAILURE;
      }
      const RangeBoundary& atStartOfSelection = firstRange->StartRef();
      if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
        return NS_ERROR_FAILURE;
      }
      nsINode* parent = atStartOfSelection.Container();
      emptyDiv = !HTMLEditUtils::IsTableElement(parent) ||
                 HTMLEditUtils::IsTableCellOrCaption(*parent);
    }
  }
  if (emptyDiv) {
    nsRange* firstRange = SelectionRefPtr()->GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return NS_ERROR_FAILURE;
    }

    EditorDOMPoint atStartOfSelection(firstRange->StartRef());
    if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
      return NS_ERROR_FAILURE;
    }

    SplitNodeResult splitNodeResult =
        MaybeSplitAncestorsForInsertWithTransaction(*nsGkAtoms::div,
                                                    atStartOfSelection);
    if (NS_WARN_IF(splitNodeResult.Failed())) {
      return splitNodeResult.Rv();
    }

    // Consume a trailing br, if any.  This is to keep an alignment from
    // creating extra lines, if possible.
    nsCOMPtr<nsIContent> brContent =
        HTMLEditorRef().GetNextEditableHTMLNodeInBlock(
            splitNodeResult.SplitPoint());
    EditorDOMPoint pointToInsertDiv(splitNodeResult.SplitPoint());
    if (brContent && TextEditUtils::IsBreak(brContent)) {
      // Making use of html structure... if next node after where we are
      // putting our div is not a block, then the br we found is in same block
      // we are, so it's safe to consume it.
      nsCOMPtr<nsIContent> sibling;
      if (pointToInsertDiv.GetChild()) {
        sibling =
            HTMLEditorRef().GetNextHTMLSibling(pointToInsertDiv.GetChild());
      }
      if (sibling && !HTMLEditor::NodeIsBlockStatic(*sibling)) {
        AutoEditorDOMPointChildInvalidator lockOffset(pointToInsertDiv);
        rv = MOZ_KnownLive(HTMLEditorRef())
                 .DeleteNodeWithTransaction(*brContent);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
    }
    RefPtr<Element> div =
        MOZ_KnownLive(HTMLEditorRef())
            .CreateNodeWithTransaction(*nsGkAtoms::div, pointToInsertDiv);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(!div)) {
      return NS_ERROR_FAILURE;
    }
    // Remember our new block for postprocessing
    HTMLEditorRef().TopLevelEditSubActionDataRef().mNewBlockElement = div;
    // Set up the alignment on the div, using HTML or CSS
    rv = AlignBlock(*div, aAlignType, ResetAlignOf::OnlyDescendants);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    // Put in a padding <br> element for empty last line so that it won't get
    // deleted.
    CreateElementResult createPaddingBRResult =
        MOZ_KnownLive(HTMLEditorRef())
            .InsertPaddingBRElementForEmptyLastLineWithTransaction(
                EditorDOMPoint(div, 0));
    if (NS_WARN_IF(createPaddingBRResult.Failed())) {
      return createPaddingBRResult.Rv();
    }
    EditorRawDOMPoint atStartOfDiv(div, 0);
    // Don't restore the selection
    restoreSelectionLater.Abort();
    ErrorResult error;
    SelectionRefPtr()->Collapse(atStartOfDiv, error);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      error.SuppressException();
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
    return NS_OK;
  }

  // Next we detect all the transitions in the array, where a transition
  // means that adjacent nodes in the array don't have the same parent.

  nsTArray<bool> transitionList;
  MakeTransitionList(nodeArray, transitionList);

  // Okay, now go through all the nodes and give them an align attrib or put
  // them in a div, or whatever is appropriate.  Woohoo!

  nsCOMPtr<Element> curDiv;
  bool useCSS = HTMLEditorRef().IsCSSEnabled();
  int32_t indexOfTransitionList = -1;
  for (OwningNonNull<nsINode>& curNode : nodeArray) {
    ++indexOfTransitionList;

    // Ignore all non-editable nodes.  Leave them be.
    if (!HTMLEditorRef().IsEditable(curNode)) {
      continue;
    }

    // The node is a table element, an hr, a paragraph, a div or a section
    // header; in HTML 4, it can directly carry the ALIGN attribute and we
    // don't need to nest it, just set the alignment.  In CSS, assign the
    // corresponding CSS styles in AlignBlock
    if (HTMLEditUtils::SupportsAlignAttr(*curNode)) {
      rv = AlignBlock(MOZ_KnownLive(*curNode->AsElement()), aAlignType,
                      ResetAlignOf::ElementAndDescendants);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      // Clear out curDiv so that we don't put nodes after this one into it
      curDiv = nullptr;
      continue;
    }

    EditorDOMPoint atCurNode(curNode);
    if (NS_WARN_IF(!atCurNode.IsSet())) {
      continue;
    }

    // Skip insignificant formatting text nodes to prevent unnecessary
    // structure splitting!
    bool isEmptyTextNode = false;
    if (curNode->GetAsText() &&
        ((HTMLEditUtils::IsTableElement(atCurNode.GetContainer()) &&
          !HTMLEditUtils::IsTableCellOrCaption(*atCurNode.GetContainer())) ||
         HTMLEditUtils::IsList(atCurNode.GetContainer()) ||
         (NS_SUCCEEDED(
              HTMLEditorRef().IsEmptyNode(curNode, &isEmptyTextNode)) &&
          isEmptyTextNode))) {
      continue;
    }

    // If it's a list item, or a list inside a list, forget any "current" div,
    // and instead put divs inside the appropriate block (td, li, etc.)
    if (HTMLEditUtils::IsListItem(curNode) || HTMLEditUtils::IsList(curNode)) {
      AutoEditorDOMPointOffsetInvalidator lockChild(atCurNode);
      rv = RemoveAlignment(*curNode, aAlignType, true);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      if (useCSS) {
        HTMLEditorRef().mCSSEditUtils->SetCSSEquivalentToHTMLStyle(
            MOZ_KnownLive(curNode->AsElement()), nullptr, nsGkAtoms::align,
            &aAlignType, false);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        curDiv = nullptr;
        continue;
      }
      if (HTMLEditUtils::IsList(atCurNode.GetContainer())) {
        // If we don't use CSS, add a content to list element: they have to
        // be inside another list, i.e., >= second level of nesting.
        // XXX AlignInnerBlocks() handles list item elements and table cells.
        //     Is it intentional to change alignment of nested other type
        //     descendants too?
        rv = AlignInnerBlocks(*curNode, aAlignType);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        curDiv = nullptr;
        continue;
      }
      // Clear out curDiv so that we don't put nodes after this one into it
    }

    // Need to make a div to put things in if we haven't already, or if this
    // node doesn't go in div we used earlier.
    if (!curDiv || transitionList[indexOfTransitionList]) {
      // First, check that our element can contain a div.
      if (!HTMLEditorRef().CanContainTag(*atCurNode.GetContainer(),
                                         *nsGkAtoms::div)) {
        // Cancelled
        return NS_OK;
      }

      SplitNodeResult splitNodeResult =
          MaybeSplitAncestorsForInsertWithTransaction(*nsGkAtoms::div,
                                                      atCurNode);
      if (NS_WARN_IF(splitNodeResult.Failed())) {
        return splitNodeResult.Rv();
      }
      curDiv = MOZ_KnownLive(HTMLEditorRef())
                   .CreateNodeWithTransaction(*nsGkAtoms::div,
                                              splitNodeResult.SplitPoint());
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(!curDiv)) {
        return NS_ERROR_FAILURE;
      }
      // Remember our new block for postprocessing
      HTMLEditorRef().TopLevelEditSubActionDataRef().mNewBlockElement = curDiv;
      // Set up the alignment on the div
      rv = AlignBlock(*curDiv, aAlignType, ResetAlignOf::OnlyDescendants);
      if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to align the <div>");
    }

    // Tuck the node into the end of the active div
    rv = MOZ_KnownLive(HTMLEditorRef())
             .MoveNodeToEndWithTransaction(MOZ_KnownLive(*curNode->AsContent()),
                                           *curDiv);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult HTMLEditRules::AlignInnerBlocks(nsINode& aNode,
                                         const nsAString& aAlignType) {
  MOZ_ASSERT(IsEditorDataAvailable());

  // Gather list of table cells or list items
  nsTArray<OwningNonNull<nsINode>> nodeArray;
  TableCellAndListItemFunctor functor;
  DOMIterator iter(aNode);
  iter.AppendList(functor, nodeArray);

  // Now that we have the list, align their contents as requested
  for (auto& node : nodeArray) {
    nsresult rv = AlignBlockContents(*node, aAlignType);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult HTMLEditRules::AlignBlockContents(nsINode& aNode,
                                           const nsAString& aAlignType) {
  MOZ_ASSERT(IsEditorDataAvailable());

  nsCOMPtr<nsIContent> firstChild =
      HTMLEditorRef().GetFirstEditableChild(aNode);
  if (!firstChild) {
    // this cell has no content, nothing to align
    return NS_OK;
  }

  nsCOMPtr<nsIContent> lastChild = HTMLEditorRef().GetLastEditableChild(aNode);
  if (firstChild == lastChild && firstChild->IsHTMLElement(nsGkAtoms::div)) {
    // the cell already has a div containing all of its content: just
    // act on this div.
    nsresult rv =
        MOZ_KnownLive(HTMLEditorRef())
            .SetAttributeOrEquivalent(MOZ_KnownLive(firstChild->AsElement()),
                                      nsGkAtoms::align, aAlignType, false);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  // else we need to put in a div, set the alignment, and toss in all the
  // children
  RefPtr<Element> divElem = MOZ_KnownLive(HTMLEditorRef())
                                .CreateNodeWithTransaction(
                                    *nsGkAtoms::div, EditorDOMPoint(&aNode, 0));
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(!divElem)) {
    return NS_ERROR_FAILURE;
  }
  // set up the alignment on the div
  nsresult rv = MOZ_KnownLive(HTMLEditorRef())
                    .SetAttributeOrEquivalent(divElem, nsGkAtoms::align,
                                              aAlignType, false);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  // tuck the children into the end of the active div
  while (lastChild && (lastChild != divElem)) {
    nsresult rv =
        MOZ_KnownLive(HTMLEditorRef())
            .MoveNodeWithTransaction(*lastChild, EditorDOMPoint(divElem, 0));
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    lastChild = HTMLEditorRef().GetLastEditableChild(aNode);
  }
  return NS_OK;
}

nsresult HTMLEditRules::MaybeDeleteTopMostEmptyAncestor(
    nsINode& aStartNode, Element& aEditingHostElement,
    nsIEditor::EDirection aAction, bool* aHandled) {
  MOZ_ASSERT(IsEditorDataAvailable());

  // If the editing host is an inline element, bail out early.
  if (HTMLEditor::NodeIsInlineStatic(aEditingHostElement)) {
    return NS_OK;
  }

  // If we are inside an empty block, delete it.  Note: do NOT delete table
  // elements this way.
  RefPtr<Element> block = HTMLEditorRef().GetBlock(aStartNode);
  RefPtr<Element> emptyBlock;
  if (block && block != &aEditingHostElement) {
    // Efficiency hack, avoiding IsEmptyNode() call when in body
    bool isEmptyNode = false;
    nsresult rv = HTMLEditorRef().IsEmptyNode(block, &isEmptyNode, true, false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    while (block && isEmptyNode && !HTMLEditUtils::IsTableElement(block) &&
           block != &aEditingHostElement) {
      emptyBlock = block;
      block = HTMLEditorRef().GetBlockNodeParent(emptyBlock);
      if (block) {
        rv = HTMLEditorRef().IsEmptyNode(block, &isEmptyNode, true, false);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
    }
  }

  if (!emptyBlock || !emptyBlock->IsEditable()) {
    return NS_OK;
  }

  nsCOMPtr<nsINode> blockParent = emptyBlock->GetParentNode();
  if (NS_WARN_IF(!blockParent)) {
    return NS_ERROR_FAILURE;
  }

  if (HTMLEditUtils::IsListItem(emptyBlock)) {
    // If the found empty block is a list item element and its grand parent
    // (i.e., parent of list element) is NOT a list element, insert <br>
    // element before the list element which has the empty list item.
    // XXX Typically, list element shouldn't have another list element.
    //     So, what's the purpose of this block?
    if (HTMLEditorRef().IsFirstEditableChild(emptyBlock)) {
      EditorDOMPoint atBlockParent(blockParent);
      if (NS_WARN_IF(!atBlockParent.IsSet())) {
        return NS_ERROR_FAILURE;
      }
      // If the grand parent IS a list element, we'll adjust Selection in
      // AfterEdit().
      if (!HTMLEditUtils::IsList(atBlockParent.GetContainer())) {
        RefPtr<Element> brElement =
            MOZ_KnownLive(HTMLEditorRef())
                .InsertBRElementWithTransaction(atBlockParent);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(!brElement)) {
          return NS_ERROR_FAILURE;
        }
        ErrorResult error;
        SelectionRefPtr()->Collapse(EditorRawDOMPoint(brElement), error);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          error.SuppressException();
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(error.Failed())) {
          return error.StealNSResult();
        }
      }
    }
  } else {
    switch (aAction) {
      case nsIEditor::eNext:
      case nsIEditor::eNextWord:
      case nsIEditor::eToEndOfLine: {
        // Collapse Selection to next node of after empty block element
        // if there is.  Otherwise, to just after the empty block.
        EditorRawDOMPoint afterEmptyBlock(emptyBlock);
        bool advancedFromEmptyBlock = afterEmptyBlock.AdvanceOffset();
        NS_WARNING_ASSERTION(
            advancedFromEmptyBlock,
            "Failed to set selection to the after the empty block");
        nsCOMPtr<nsIContent> nextNode =
            HTMLEditorRef().GetNextNode(afterEmptyBlock);
        if (nextNode) {
          EditorDOMPoint pt = GetGoodSelPointForNode(*nextNode, aAction);
          ErrorResult error;
          SelectionRefPtr()->Collapse(pt, error);
          if (NS_WARN_IF(!CanHandleEditAction())) {
            error.SuppressException();
            return NS_ERROR_EDITOR_DESTROYED;
          }
          if (NS_WARN_IF(error.Failed())) {
            return error.StealNSResult();
          }
          break;
        }
        if (NS_WARN_IF(!advancedFromEmptyBlock)) {
          return NS_ERROR_FAILURE;
        }
        ErrorResult error;
        SelectionRefPtr()->Collapse(afterEmptyBlock, error);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          error.SuppressException();
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(error.Failed())) {
          return error.StealNSResult();
        }
        break;
      }
      case nsIEditor::ePrevious:
      case nsIEditor::ePreviousWord:
      case nsIEditor::eToBeginningOfLine: {
        // Collapse Selection to previous editable node of the empty block
        // if there is.  Otherwise, to after the empty block.
        EditorRawDOMPoint atEmptyBlock(emptyBlock);
        nsCOMPtr<nsIContent> priorNode =
            HTMLEditorRef().GetPreviousEditableNode(atEmptyBlock);
        if (priorNode) {
          EditorDOMPoint pt = GetGoodSelPointForNode(*priorNode, aAction);
          ErrorResult error;
          SelectionRefPtr()->Collapse(pt, error);
          if (NS_WARN_IF(!CanHandleEditAction())) {
            error.SuppressException();
            return NS_ERROR_EDITOR_DESTROYED;
          }
          if (NS_WARN_IF(error.Failed())) {
            return error.StealNSResult();
          }
          break;
        }
        EditorRawDOMPoint afterEmptyBlock(emptyBlock);
        if (NS_WARN_IF(!afterEmptyBlock.AdvanceOffset())) {
          return NS_ERROR_FAILURE;
        }
        ErrorResult error;
        SelectionRefPtr()->Collapse(afterEmptyBlock, error);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          error.SuppressException();
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(error.Failed())) {
          return error.StealNSResult();
        }
        break;
      }
      case nsIEditor::eNone:
        break;
      default:
        MOZ_CRASH("CheckForEmptyBlock doesn't support this action yet");
    }
  }
  *aHandled = true;
  nsresult rv =
      MOZ_KnownLive(HTMLEditorRef()).DeleteNodeWithTransaction(*emptyBlock);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

Element* HTMLEditRules::CheckForInvisibleBR(Element& aBlock, BRLocation aWhere,
                                            int32_t aOffset) {
  MOZ_ASSERT(IsEditorDataAvailable());

  nsCOMPtr<nsINode> testNode;
  int32_t testOffset = 0;

  if (aWhere == BRLocation::blockEnd) {
    // No block crossing
    nsCOMPtr<nsIContent> rightmostNode =
        HTMLEditorRef().GetRightmostChild(&aBlock, true);

    if (!rightmostNode) {
      return nullptr;
    }

    testNode = rightmostNode->GetParentNode();
    // Since rightmostNode is always the last child, its index is equal to the
    // child count, so instead of ComputeIndexOf() we use the faster
    // GetChildCount(), and assert the equivalence below.
    testOffset = testNode->GetChildCount();

    // Use offset + 1, so last node is included in our evaluation
    MOZ_ASSERT(testNode->ComputeIndexOf(rightmostNode) + 1 == testOffset);
  } else if (aOffset) {
    testNode = &aBlock;
    // We'll check everything to the left of the input position
    testOffset = aOffset;
  } else {
    return nullptr;
  }

  WSRunObject wsTester(&HTMLEditorRef(), testNode, testOffset);
  if (WSType::br == wsTester.mStartReason) {
    return wsTester.mStartReasonNode->AsElement();
  }

  return nullptr;
}

size_t HTMLEditor::CollectChildren(
    nsINode& aNode, nsTArray<OwningNonNull<nsINode>>& aOutArrayOfNodes,
    size_t aIndexToInsertChildren,
    CollectListChildren aCollectListChildren /* = CollectListChildren::Yes */,
    CollectTableChildren
        aCollectTableChildren /* = CollectTableChildren::Yes */) const {
  MOZ_ASSERT(IsEditActionDataAvailable());

  size_t numberOfFoundChildren = 0;
  for (nsIContent* content = GetFirstEditableChild(aNode); content;
       content = content->GetNextSibling()) {
    if ((aCollectListChildren == CollectListChildren::Yes &&
         (HTMLEditUtils::IsList(content) ||
          HTMLEditUtils::IsListItem(content))) ||
        (aCollectTableChildren == CollectTableChildren::Yes &&
         HTMLEditUtils::IsTableElement(content))) {
      numberOfFoundChildren +=
          CollectChildren(*content, aOutArrayOfNodes,
                          aIndexToInsertChildren + numberOfFoundChildren,
                          aCollectListChildren, aCollectTableChildren);
    } else {
      aOutArrayOfNodes.InsertElementAt(
          aIndexToInsertChildren + numberOfFoundChildren++, *content);
    }
  }
  return numberOfFoundChildren;
}

nsresult HTMLEditRules::ExpandSelectionForDeletion() {
  MOZ_ASSERT(IsEditorDataAvailable());

  // Don't need to touch collapsed selections
  if (SelectionRefPtr()->IsCollapsed()) {
    return NS_OK;
  }

  // We don't need to mess with cell selections, and we assume multirange
  // selections are those.
  if (SelectionRefPtr()->RangeCount() != 1) {
    return NS_OK;
  }

  // Find current sel start and end
  nsRange* firstRange = SelectionRefPtr()->GetRangeAt(0);
  if (NS_WARN_IF(!firstRange)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsINode> selStartNode = firstRange->GetStartContainer();
  int32_t selStartOffset = firstRange->StartOffset();
  nsCOMPtr<nsINode> selEndNode = firstRange->GetEndContainer();
  int32_t selEndOffset = firstRange->EndOffset();

  // Find current selection common block parent
  RefPtr<Element> selCommon =
      HTMLEditor::GetBlock(*firstRange->GetCommonAncestor());
  if (NS_WARN_IF(!selCommon)) {
    return NS_ERROR_FAILURE;
  }

  // Set up for loops and cache our root element
  nsCOMPtr<nsINode> firstBRParent;
  int32_t firstBROffset = 0;
  WSType wsType;
  RefPtr<Element> root = HTMLEditorRef().GetActiveEditingHost();
  if (NS_WARN_IF(!root)) {
    return NS_ERROR_FAILURE;
  }

  // Find previous visible things before start of selection
  if (selStartNode != selCommon && selStartNode != root) {
    while (true) {
      WSRunObject wsObj(&HTMLEditorRef(), selStartNode, selStartOffset);
      wsObj.PriorVisibleNode(EditorRawDOMPoint(selStartNode, selStartOffset),
                             &wsType);
      if (wsType != WSType::thisBlock) {
        break;
      }
      // We want to keep looking up.  But stop if we are crossing table
      // element boundaries, or if we hit the root.
      if (HTMLEditUtils::IsTableElement(wsObj.mStartReasonNode) ||
          selCommon == wsObj.mStartReasonNode ||
          root == wsObj.mStartReasonNode) {
        break;
      }
      selStartNode = wsObj.mStartReasonNode->GetParentNode();
      selStartOffset =
          selStartNode ? selStartNode->ComputeIndexOf(wsObj.mStartReasonNode)
                       : -1;
    }
  }

  // Find next visible things after end of selection
  if (selEndNode != selCommon && selEndNode != root) {
    for (;;) {
      WSRunObject wsObj(&HTMLEditorRef(), selEndNode, selEndOffset);
      wsObj.NextVisibleNode(EditorRawDOMPoint(selEndNode, selEndOffset),
                            &wsType);
      if (wsType == WSType::br) {
        if (HTMLEditorRef().IsVisibleBRElement(wsObj.mEndReasonNode)) {
          break;
        }
        if (!firstBRParent) {
          firstBRParent = selEndNode;
          firstBROffset = selEndOffset;
        }
        selEndNode = wsObj.mEndReasonNode->GetParentNode();
        selEndOffset =
            selEndNode ? selEndNode->ComputeIndexOf(wsObj.mEndReasonNode) + 1
                       : 0;
      } else if (wsType == WSType::thisBlock) {
        // We want to keep looking up.  But stop if we are crossing table
        // element boundaries, or if we hit the root.
        if (HTMLEditUtils::IsTableElement(wsObj.mEndReasonNode) ||
            selCommon == wsObj.mEndReasonNode || root == wsObj.mEndReasonNode) {
          break;
        }
        selEndNode = wsObj.mEndReasonNode->GetParentNode();
        selEndOffset = 1 + selEndNode->ComputeIndexOf(wsObj.mEndReasonNode);
      } else {
        break;
      }
    }
  }

  // Expand selection endpoint only if we didn't pass a <br>, or if we really
  // needed to pass that <br> (i.e., its block is now totally selected).
  bool doEndExpansion = true;
  if (firstBRParent) {
    // Find block node containing <br>.
    nsCOMPtr<Element> brBlock = HTMLEditor::GetBlock(*firstBRParent);
    bool nodeBefore = false, nodeAfter = false;

    // Create a range that represents expanded selection
    RefPtr<StaticRange> staticRange = StaticRange::Create(
        selStartNode, selStartOffset, selEndNode, selEndOffset, IgnoreErrors());
    if (NS_WARN_IF(!staticRange)) {
      return NS_ERROR_FAILURE;
    }

    // Check if block is entirely inside range
    if (brBlock) {
      RangeUtils::CompareNodeToRange(brBlock, staticRange, &nodeBefore,
                                     &nodeAfter);
    }

    // If block isn't contained, forgo grabbing the <br> in expanded selection.
    if (nodeBefore || nodeAfter) {
      doEndExpansion = false;
    }
  }

  EditorRawDOMPoint newSelectionStart(selStartNode, selStartOffset);
  EditorRawDOMPoint newSelectionEnd(
      doEndExpansion ? selEndNode : firstBRParent,
      doEndExpansion ? selEndOffset : firstBROffset);
  ErrorResult error;
  MOZ_KnownLive(SelectionRefPtr())
      ->SetStartAndEndInLimiter(newSelectionStart, newSelectionEnd, error);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    error.SuppressException();
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(!error.Failed(), "Failed to set selection for deletion");
  return error.StealNSResult();
}

nsresult HTMLEditRules::NormalizeSelection() {
  MOZ_ASSERT(IsEditorDataAvailable());

  // NormalizeSelection() tweaks non-collapsed selections to be more "natural".
  // Idea here is to adjust selection endpoint so that they do not cross breaks
  // or block boundaries unless something editable beyond that boundary is also
  // selected.  This adjustment makes it much easier for the various block
  // operations to determine what nodes to act on.

  // don't need to touch collapsed selections
  if (SelectionRefPtr()->IsCollapsed()) {
    return NS_OK;
  }

  // We don't need to mess with cell selections, and we assume multirange
  // selections are those.
  // XXX Why?  Even in <input>, user can select 2 or more ranges.
  if (SelectionRefPtr()->RangeCount() != 1) {
    return NS_OK;
  }

  RefPtr<nsRange> range = SelectionRefPtr()->GetRangeAt(0);
  if (NS_WARN_IF(!range)) {
    return NS_ERROR_FAILURE;
  }

  EditorDOMPoint startPoint(range->StartRef());
  if (NS_WARN_IF(!startPoint.IsSet())) {
    return NS_ERROR_FAILURE;
  }
  EditorDOMPoint endPoint(range->EndRef());
  if (NS_WARN_IF(!endPoint.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  // adjusted values default to original values
  EditorDOMPoint newStartPoint(startPoint);
  EditorDOMPoint newEndPoint(endPoint);

  // some locals we need for whitespace code
  WSType wsType;

  // let the whitespace code do the heavy lifting
  WSRunObject wsEndObj(&HTMLEditorRef(), endPoint);
  // Is there any intervening visible whitespace?  If so we can't push
  // selection past that, it would visibly change meaning of users selection.
  wsEndObj.PriorVisibleNode(endPoint, &wsType);
  if (wsType != WSType::text && wsType != WSType::normalWS) {
    // eThisBlock and eOtherBlock conveniently distinguish cases
    // of going "down" into a block and "up" out of a block.
    if (wsEndObj.mStartReason == WSType::otherBlock) {
      // endpoint is just after the close of a block.
      nsINode* child =
          HTMLEditorRef().GetRightmostChild(wsEndObj.mStartReasonNode, true);
      if (child) {
        newEndPoint.SetAfter(child);
      }
      // else block is empty - we can leave selection alone here, i think.
    } else if (wsEndObj.mStartReason == WSType::thisBlock) {
      // endpoint is just after start of this block
      nsINode* child = HTMLEditorRef().GetPreviousEditableHTMLNode(endPoint);
      if (child) {
        newEndPoint.SetAfter(child);
      }
      // else block is empty - we can leave selection alone here, i think.
    } else if (wsEndObj.mStartReason == WSType::br) {
      // endpoint is just after break.  lets adjust it to before it.
      newEndPoint.Set(wsEndObj.mStartReasonNode);
    }
  }

  // similar dealio for start of range
  WSRunObject wsStartObj(&HTMLEditorRef(), startPoint);
  // Is there any intervening visible whitespace?  If so we can't push
  // selection past that, it would visibly change meaning of users selection.
  wsStartObj.NextVisibleNode(startPoint, &wsType);
  if (wsType != WSType::text && wsType != WSType::normalWS) {
    // eThisBlock and eOtherBlock conveniently distinguish cases
    // of going "down" into a block and "up" out of a block.
    if (wsStartObj.mEndReason == WSType::otherBlock) {
      // startpoint is just before the start of a block.
      nsINode* child =
          HTMLEditorRef().GetLeftmostChild(wsStartObj.mEndReasonNode, true);
      if (child) {
        newStartPoint.Set(child);
      }
      // else block is empty - we can leave selection alone here, i think.
    } else if (wsStartObj.mEndReason == WSType::thisBlock) {
      // startpoint is just before end of this block
      nsINode* child = HTMLEditorRef().GetNextEditableHTMLNode(startPoint);
      if (child) {
        newStartPoint.Set(child);
      }
      // else block is empty - we can leave selection alone here, i think.
    } else if (wsStartObj.mEndReason == WSType::br) {
      // startpoint is just before a break.  lets adjust it to after it.
      newStartPoint.SetAfter(wsStartObj.mEndReasonNode);
    }
  }

  // There is a demented possiblity we have to check for.  We might have a very
  // strange selection that is not collapsed and yet does not contain any
  // editable content, and satisfies some of the above conditions that cause
  // tweaking.  In this case we don't want to tweak the selection into a block
  // it was never in, etc.  There are a variety of strategies one might use to
  // try to detect these cases, but I think the most straightforward is to see
  // if the adjusted locations "cross" the old values: i.e., new end before old
  // start, or new start after old end.  If so then just leave things alone.

  int16_t comp;
  comp = nsContentUtils::ComparePoints(startPoint.ToRawRangeBoundary(),
                                       newEndPoint.ToRawRangeBoundary());
  if (comp == 1) {
    return NS_OK;  // New end before old start.
  }
  comp = nsContentUtils::ComparePoints(newStartPoint.ToRawRangeBoundary(),
                                       endPoint.ToRawRangeBoundary());
  if (comp == 1) {
    return NS_OK;  // New start after old end.
  }

  // Otherwise set selection to new values.  Note that end point may be prior
  // to start point.  So, we cannot use Selection::SetStartAndEndInLimit() here.
  ErrorResult error;
  MOZ_KnownLive(SelectionRefPtr())
      ->SetBaseAndExtentInLimiter(newStartPoint, newEndPoint, error);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    error.SuppressException();
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(!error.Failed(), "Failed to set selection");
  return error.StealNSResult();
}

// static
template <typename PT, typename RT>
EditorDOMPoint HTMLEditor::GetWhiteSpaceEndPoint(
    const RangeBoundaryBase<PT, RT>& aPoint, ScanDirection aScanDirection) {
  if (NS_WARN_IF(!aPoint.IsSet()) ||
      NS_WARN_IF(!aPoint.Container()->IsContent())) {
    return EditorDOMPoint();
  }

  bool isSpace = false, isNBSP = false;
  nsIContent* newContent = aPoint.Container()->AsContent();
  int32_t newOffset = aPoint.Offset();
  while (newContent) {
    int32_t offset = -1;
    nsCOMPtr<nsIContent> content;
    if (aScanDirection == ScanDirection::Backward) {
      HTMLEditor::IsPrevCharInNodeWhitespace(newContent, newOffset, &isSpace,
                                             &isNBSP, getter_AddRefs(content),
                                             &offset);
    } else {
      HTMLEditor::IsNextCharInNodeWhitespace(newContent, newOffset, &isSpace,
                                             &isNBSP, getter_AddRefs(content),
                                             &offset);
    }
    if (!isSpace && !isNBSP) {
      break;
    }
    newContent = content;
    newOffset = offset;
  }
  return EditorDOMPoint(newContent, newOffset);
}

template <typename PT, typename RT>
EditorDOMPoint HTMLEditor::GetCurrentHardLineStartPoint(
    const RangeBoundaryBase<PT, RT>& aPoint, EditSubAction aEditSubAction) {
  if (NS_WARN_IF(!aPoint.IsSet())) {
    return EditorDOMPoint();
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
    point.Set(point.GetContainer());
  }

  // Look back through any further inline nodes that aren't across a <br>
  // from us, and that are enclosed in the same block.
  // I.e., looking for start of current hard line.
  for (nsIContent* previousEditableContent =
           GetPreviousEditableHTMLNodeInBlock(point);
       previousEditableContent && previousEditableContent->GetParentNode() &&
       !IsVisibleBRElement(previousEditableContent) &&
       !HTMLEditor::NodeIsBlockStatic(*previousEditableContent);
       previousEditableContent = GetPreviousEditableHTMLNodeInBlock(point)) {
    point.Set(previousEditableContent);
    // XXX Why don't we check \n in pre-formated text node here?
  }

  // Finding the real start for this point unless current line starts after
  // <br> element.  Look up the tree for as long as we are the first node in
  // the container (typically, start of nearest block ancestor), and as long
  // as we haven't hit the body node.
  for (nsIContent* nearContent = GetPreviousEditableHTMLNodeInBlock(point);
       !nearContent && !point.IsContainerHTMLElement(nsGkAtoms::body) &&
       point.GetContainer()->GetParentNode();
       nearContent = GetPreviousEditableHTMLNodeInBlock(point)) {
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
    const RangeBoundaryBase<PT, RT>& aPoint) {
  if (NS_WARN_IF(!aPoint.IsSet())) {
    return EditorDOMPoint();
  }

  EditorDOMPoint point(aPoint);
  // Start scanning from the container node if aPoint is in a text node.
  // XXX Perhaps, IsInDataNode() must be expected.
  if (point.IsInTextNode()) {
    if (!point.GetContainer()->GetParentNode()) {
      // Okay, can't promote any further
      // XXX Why don't we return end of the text node?
      return point;
    }
    // want to be after the text node
    point.Set(point.GetContainer());
    DebugOnly<bool> advanced = point.AdvanceOffset();
    NS_WARNING_ASSERTION(advanced,
                         "Failed to advance offset to after the text node");
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
  for (nsIContent* nextEditableContent = GetNextEditableHTMLNodeInBlock(point);
       nextEditableContent &&
       !HTMLEditor::NodeIsBlockStatic(*nextEditableContent) &&
       nextEditableContent->GetParent();
       nextEditableContent = GetNextEditableHTMLNodeInBlock(point)) {
    point.Set(nextEditableContent);
    if (NS_WARN_IF(!point.AdvanceOffset())) {
      break;
    }
    if (IsVisibleBRElement(nextEditableContent)) {
      break;
    }

    // Check for newlines in pre-formatted text nodes.
    if (EditorBase::IsPreformatted(nextEditableContent) &&
        EditorBase::IsTextNode(nextEditableContent)) {
      nsAutoString textContent;
      nextEditableContent->GetAsText()->GetData(textContent);
      int32_t newlinePos = textContent.FindChar(nsCRT::LF);
      if (newlinePos >= 0) {
        if (static_cast<uint32_t>(newlinePos) + 1 == textContent.Length()) {
          // No need for special processing if the newline is at the end.
          break;
        }
        return EditorDOMPoint(nextEditableContent, newlinePos + 1);
      }
    }
  }

  // Finding the real end for this point unless current line ends with a <br>
  // element.  Look up the tree for as long as we are the last node in the
  // container (typically, block node), and as long as we haven't hit the body
  // node.
  for (nsIContent* nearContent = GetNextEditableHTMLNodeInBlock(point);
       !nearContent && !point.IsContainerHTMLElement(nsGkAtoms::body) &&
       point.GetContainer()->GetParentNode();
       nearContent = GetNextEditableHTMLNodeInBlock(point)) {
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

    point.Set(point.GetContainer());
    if (NS_WARN_IF(!point.AdvanceOffset())) {
      break;
    }
  }
  return point;
}

void HTMLEditor::GetSelectionRangesExtendedToIncludeAdjuscentWhiteSpaces(
    nsTArray<RefPtr<nsRange>>& aOutArrayOfRanges) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(aOutArrayOfRanges.IsEmpty());

  aOutArrayOfRanges.SetCapacity(SelectionRefPtr()->RangeCount());
  for (uint32_t i = 0; i < SelectionRefPtr()->RangeCount(); i++) {
    nsRange* selectionRange = SelectionRefPtr()->GetRangeAt(i);
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

  aOutArrayOfRanges.SetCapacity(SelectionRefPtr()->RangeCount());
  for (uint32_t i = 0; i < SelectionRefPtr()->RangeCount(); i++) {
    // Make a new adjusted range to represent the appropriate block content.
    // The basic idea is to push out the range endpoints to truly enclose the
    // blocks that we will affect.  This call alters opRange.
    nsRange* selectionRange = SelectionRefPtr()->GetRangeAt(i);
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
    RangeBoundaryBase<EPT, ERT>& aEndRef) {
  // MOOSE major hack:
  // The GetWhiteSpaceEndPoint(), GetCurrentHardLineStartPoint() and
  // GetCurrentHardLineEndPoint() don't really do the right thing for
  // collapsed ranges inside block elements that contain nothing but a solo
  // <br>.  It's easier/ to put a workaround here than to revamp them.  :-(
  // XXX This sounds odd in the cases using `GetWhiteSpaceEndPoint()`.  Don't
  //     we hack something in the edit sub-action handlers?
  if (aStartRef != aEndRef) {
    return;
  }

  Element* block = GetBlock(*aStartRef.Container());
  if (!block) {
    return;
  }

  Element* editingHost = GetActiveEditingHost();
  if (NS_WARN_IF(!editingHost)) {
    return;
  }

  // Make sure we don't go higher than our root element in the content tree
  if (editingHost->IsInclusiveDescendantOf(block)) {
    return;
  }

  bool isEmptyNode = false;
  IsEmptyNode(block, &isEmptyNode, true, false);
  if (isEmptyNode) {
    aStartRef.Set(block, 0);
    aEndRef.Set(block, block->Length());
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

  // For text actions, we want to look backwards (or forwards, as
  // appropriate) for additional whitespace or nbsp's.  We may have to act
  // on these later even though they are outside of the initial selection.
  // Even if they are in another node!
  // XXX Although the comment mentioned that this may scan other text nodes,
  //     GetWhiteSpaceEndPoint() scans only in given container node.
  // XXX Looks like that we should make GetWhiteSpaceEndPoint() be a
  //     instance method and stop scanning whitespaces when it reaches
  //     active editing host.
  EditorDOMPoint startPoint = HTMLEditor::GetWhiteSpaceEndPoint(
      startRef, HTMLEditor::ScanDirection::Backward);
  if (!IsDescendantOfEditorRoot(
          EditorBase::GetNodeAtRangeOffsetPoint(startPoint))) {
    return nullptr;
  }
  EditorDOMPoint endPoint = HTMLEditor::GetWhiteSpaceEndPoint(
      endRef, HTMLEditor::ScanDirection::Forward);
  EditorRawDOMPoint lastRawPoint(endPoint);
  lastRawPoint.RewindOffset();
  if (!IsDescendantOfEditorRoot(
          EditorBase::GetNodeAtRangeOffsetPoint(lastRawPoint))) {
    return nullptr;
  }

  RefPtr<nsRange> range = new nsRange(GetDocument());
  nsresult rv = range->SetStartAndEnd(startPoint.ToRawRangeBoundary(),
                                      endPoint.ToRawRangeBoundary());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }
  return range.forget();
}

already_AddRefed<nsRange> HTMLEditor::CreateRangeExtendedToHardLineStartAndEnd(
    const AbstractRange& aAbstractRange, EditSubAction aEditSubAction) {
  if (!aAbstractRange.IsPositioned()) {
    return nullptr;
  }
  return CreateRangeExtendedToHardLineStartAndEnd(
      aAbstractRange.StartRef(), aAbstractRange.EndRef(), aEditSubAction);
}

template <typename SPT, typename SRT, typename EPT, typename ERT>
already_AddRefed<nsRange> HTMLEditor::CreateRangeExtendedToHardLineStartAndEnd(
    const RangeBoundaryBase<SPT, SRT>& aStartRef,
    const RangeBoundaryBase<EPT, ERT>& aEndRef, EditSubAction aEditSubAction) {
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
  if (!IsDescendantOfEditorRoot(
          EditorBase::GetNodeAtRangeOffsetPoint(startPoint))) {
    return nullptr;
  }
  EditorDOMPoint endPoint = GetCurrentHardLineEndPoint(endRef);
  EditorRawDOMPoint lastRawPoint(endPoint);
  lastRawPoint.RewindOffset();
  // XXX GetCurrentHardLineEndPoint() may return point of editing host.
  //     Perhaps, we should change it and stop checking it here since this
  //     check may be expensive.
  if (!IsDescendantOfEditorRoot(
          EditorBase::GetNodeAtRangeOffsetPoint(lastRawPoint))) {
    return nullptr;
  }

  RefPtr<nsRange> range = new nsRange(GetDocument());
  nsresult rv = range->SetStartAndEnd(startPoint.ToRawRangeBoundary(),
                                      endPoint.ToRawRangeBoundary());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }
  return range.forget();
}

class UniqueFunctor final : public BoolDomIterFunctor {
 public:
  explicit UniqueFunctor(nsTArray<OwningNonNull<nsINode>>& aArray)
      : mArray(aArray) {}

  // Used to build list of all nodes iterator covers.
  virtual bool operator()(nsINode* aNode) const override {
    return !mArray.Contains(aNode);
  }

 private:
  nsTArray<OwningNonNull<nsINode>>& mArray;
};

nsresult HTMLEditor::SplitInlinesAndCollectEditTargetNodes(
    nsTArray<RefPtr<nsRange>>& aArrayOfRanges,
    nsTArray<OwningNonNull<nsINode>>& aOutArrayOfNodes,
    EditSubAction aEditSubAction) {
  nsresult rv = SplitTextNodesAtRangeEnd(aArrayOfRanges);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "SplitTextNodesAtRangeEnd() failed");
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = SplitParentInlineElementsAtRangeEdges(aArrayOfRanges);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "SplitParentInlineElementsAtRangeEdges() failed");
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = CollectEditTargetNodes(aArrayOfRanges, aOutArrayOfNodes, aEditSubAction);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "CollectEditTargetNodes() failed");
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = MaybeSplitElementsAtEveryBRElement(aOutArrayOfNodes, aEditSubAction);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "MaybeSplitElementsAtEveryBRElement() failed");
  return rv;
}

nsresult HTMLEditor::SplitTextNodesAtRangeEnd(
    nsTArray<RefPtr<nsRange>>& aArrayOfRanges) {
  // Split text nodes. This is necessary, since given ranges may end in text
  // nodes in case where part of a pre-formatted elements needs to be moved.
  for (RefPtr<nsRange>& range : aArrayOfRanges) {
    EditorDOMPoint atEnd(range->EndRef());
    if (NS_WARN_IF(!atEnd.IsSet()) || !atEnd.IsInTextNode()) {
      continue;
    }

    if (!atEnd.IsStartOfContainer() && !atEnd.IsEndOfContainer()) {
      // Split the text node.
      ErrorResult error;
      nsCOMPtr<nsIContent> newLeftNode = SplitNodeWithTransaction(atEnd, error);
      if (NS_WARN_IF(Destroyed())) {
        error.SuppressException();
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(error.Failed())) {
        return error.StealNSResult();
      }

      // Correct the range.
      // The new end parent becomes the parent node of the text.
      EditorRawDOMPoint atContainerOfSplitNode(atEnd.GetContainer());
      MOZ_ASSERT(!range->IsInSelection());
      range->SetEnd(atContainerOfSplitNode, error);
      if (NS_WARN_IF(error.Failed())) {
        error.SuppressException();
      }
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
    rangeItem->StoreRange(aArrayOfRanges[0]);
    RangeUpdaterRef().RegisterRangeItem(rangeItem);
    aArrayOfRanges.RemoveElementAt(0);
  }
  // Now bust up inlines.
  nsresult rv = NS_OK;
  for (auto& item : Reversed(rangeItemArray)) {
    rv = SplitParentInlineElementsAtRangeEdges(*item);
    if (NS_WARN_IF(NS_FAILED(rv))) {
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
    nsTArray<OwningNonNull<nsINode>>& aOutArrayOfNodes,
    EditSubAction aEditSubAction) const {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // Gather up a list of all the nodes
  for (auto& range : aArrayOfRanges) {
    DOMSubtreeIterator iter;
    nsresult rv = iter.Init(*range);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    if (aOutArrayOfNodes.IsEmpty()) {
      iter.AppendList(TrivialFunctor(), aOutArrayOfNodes);
    } else {
      // We don't want duplicates in aOutArrayOfNodes, so we use an
      // iterator/functor that only return nodes that are not already in
      // aOutArrayOfNodes.
      nsTArray<OwningNonNull<nsINode>> nodes;
      iter.AppendList(UniqueFunctor(aOutArrayOfNodes), nodes);
      aOutArrayOfNodes.AppendElements(nodes);
    }
  }

  switch (aEditSubAction) {
    case EditSubAction::eCreateOrRemoveBlock:
      // Certain operations should not act on li's and td's, but rather inside
      // them.  Alter the list as needed.
      for (int32_t i = aOutArrayOfNodes.Length() - 1; i >= 0; i--) {
        OwningNonNull<nsINode> node = aOutArrayOfNodes[i];
        if (HTMLEditUtils::IsListItem(node)) {
          aOutArrayOfNodes.RemoveElementAt(i);
          CollectChildren(*node, aOutArrayOfNodes, i);
        }
      }
      // Empty text node shouldn't be selected if unnecessary
      for (int32_t i = aOutArrayOfNodes.Length() - 1; i >= 0; i--) {
        if (Text* text = aOutArrayOfNodes[i]->GetAsText()) {
          // Don't select empty text except to empty block
          if (!IsVisibleTextNode(*text)) {
            aOutArrayOfNodes.RemoveElementAt(i);
          }
        }
      }
      break;
    case EditSubAction::eOutdent:
    case EditSubAction::eIndent:
    case EditSubAction::eSetPositionToAbsolute:
      // Indent/outdent already do something special for list items, but we
      // still need to make sure we don't act on table elements
      for (int32_t i = aOutArrayOfNodes.Length() - 1; i >= 0; i--) {
        OwningNonNull<nsINode> node = aOutArrayOfNodes[i];
        if (HTMLEditUtils::IsTableElementButNotTable(node)) {
          aOutArrayOfNodes.RemoveElementAt(i);
          CollectChildren(*node, aOutArrayOfNodes, i);
        }
      }
      break;
    default:
      break;
  }

  // Outdent should look inside of divs.
  if (aEditSubAction == EditSubAction::eOutdent && !IsCSSEnabled()) {
    for (int32_t i = aOutArrayOfNodes.Length() - 1; i >= 0; i--) {
      OwningNonNull<nsINode> node = aOutArrayOfNodes[i];
      if (node->IsHTMLElement(nsGkAtoms::div)) {
        aOutArrayOfNodes.RemoveElementAt(i);
        CollectChildren(*node, aOutArrayOfNodes, i, CollectListChildren::No,
                        CollectTableChildren::No);
      }
    }
  }

  return NS_OK;
}

nsresult HTMLEditor::MaybeSplitElementsAtEveryBRElement(
    nsTArray<OwningNonNull<nsINode>>& aArrayOfNodes,
    EditSubAction aEditSubAction) {
  // Post-process the list to break up inline containers that contain br's, but
  // only for operations that might care, like making lists or paragraphs
  switch (aEditSubAction) {
    case EditSubAction::eCreateOrRemoveBlock:
    case EditSubAction::eCreateOrChangeList:
    case EditSubAction::eSetOrClearAlignment:
    case EditSubAction::eSetPositionToAbsolute:
    case EditSubAction::eIndent:
    case EditSubAction::eOutdent:
      for (int32_t i = aArrayOfNodes.Length() - 1; i >= 0; i--) {
        OwningNonNull<nsINode> node = aArrayOfNodes[i];
        if (HTMLEditor::NodeIsInlineStatic(node) && IsContainer(node) &&
            !EditorBase::IsTextNode(node)) {
          nsTArray<OwningNonNull<nsINode>> arrayOfInlines;
          nsresult rv = SplitElementsAtEveryBRElement(
              MOZ_KnownLive(*node->AsContent()), arrayOfInlines);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }

          // Put these nodes in aArrayOfNodes, replacing the current node
          aArrayOfNodes.RemoveElementAt(i);
          aArrayOfNodes.InsertElementsAt(i, arrayOfInlines);
        }
      }
      return NS_OK;
    default:
      return NS_OK;
  }
}

nsresult HTMLEditRules::GetListActionNodes(
    nsTArray<OwningNonNull<nsINode>>& aOutArrayOfNodes, EntireList aEntireList,
    TouchContent aTouchContent) const {
  MOZ_ASSERT(IsEditorDataAvailable());

  // Added this in so that ui code can ask to change an entire list, even if
  // selection is only in part of it.  used by list item dialog.
  if (aEntireList == EntireList::yes) {
    uint32_t rangeCount = SelectionRefPtr()->RangeCount();
    for (uint32_t rangeIdx = 0; rangeIdx < rangeCount; ++rangeIdx) {
      RefPtr<nsRange> range = SelectionRefPtr()->GetRangeAt(rangeIdx);
      for (nsCOMPtr<nsINode> parent = range->GetCommonAncestor(); parent;
           parent = parent->GetParentNode()) {
        if (HTMLEditUtils::IsList(parent)) {
          aOutArrayOfNodes.AppendElement(*parent);
          break;
        }
      }
    }
    // If we didn't find any nodes this way, then try the normal way.  Perhaps
    // the selection spans multiple lists but with no common list parent.
    if (!aOutArrayOfNodes.IsEmpty()) {
      return NS_OK;
    }
  }

  {
    // We don't like other people messing with our selection!
    AutoTransactionsConserveSelection dontChangeMySelection(HTMLEditorRef());

    // contruct a list of nodes to act on.
    if (aTouchContent == TouchContent::yes) {
      nsresult rv =
          MOZ_KnownLive(HTMLEditorRef())
              .SplitInlinesAndCollectEditTargetNodesInExtendedSelectionRanges(
                  aOutArrayOfNodes, EditSubAction::eCreateOrChangeList);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      nsresult rv =
          HTMLEditorRef().CollectEditTargetNodesInExtendedSelectionRanges(
              aOutArrayOfNodes, EditSubAction::eCreateOrChangeList);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  // Pre-process our list of nodes
  for (int32_t i = aOutArrayOfNodes.Length() - 1; i >= 0; i--) {
    OwningNonNull<nsINode> testNode = aOutArrayOfNodes[i];

    // Remove all non-editable nodes.  Leave them be.
    if (!HTMLEditorRef().IsEditable(testNode)) {
      aOutArrayOfNodes.RemoveElementAt(i);
      continue;
    }

    // Scan for table elements and divs.  If we find table elements other than
    // table, replace it with a list of any editable non-table content.
    if (HTMLEditUtils::IsTableElementButNotTable(testNode)) {
      aOutArrayOfNodes.RemoveElementAt(i);
      HTMLEditorRef().CollectChildren(*testNode, aOutArrayOfNodes, i,
                                      HTMLEditor::CollectListChildren::No);
    }
  }

  // If there is only one node in the array, and it is a list, div, or
  // blockquote, then look inside of it until we find inner list or content.
  LookInsideDivBQandList(aOutArrayOfNodes);

  return NS_OK;
}

void HTMLEditRules::LookInsideDivBQandList(
    nsTArray<OwningNonNull<nsINode>>& aNodeArray) const {
  MOZ_ASSERT(IsEditorDataAvailable());

  // If there is only one node in the array, and it is a list, div, or
  // blockquote, then look inside of it until we find inner list or content.
  if (aNodeArray.Length() != 1) {
    return;
  }

  OwningNonNull<nsINode> curNode = aNodeArray[0];

  while (curNode->IsHTMLElement(nsGkAtoms::div) ||
         HTMLEditUtils::IsList(curNode) ||
         curNode->IsHTMLElement(nsGkAtoms::blockquote)) {
    // Dive as long as there's only one child, and it's a list, div, blockquote
    uint32_t numChildren = HTMLEditorRef().CountEditableChildren(curNode);
    if (numChildren != 1) {
      break;
    }

    // Keep diving!  XXX One would expect to dive into the one editable node.
    nsCOMPtr<nsIContent> child = curNode->GetFirstChild();
    if (!child->IsHTMLElement(nsGkAtoms::div) &&
        !HTMLEditUtils::IsList(child) &&
        !child->IsHTMLElement(nsGkAtoms::blockquote)) {
      break;
    }

    // check editability XXX floppy moose
    curNode = child;
  }

  // We've found innermost list/blockquote/div: replace the one node in the
  // array with these nodes
  aNodeArray.RemoveElementAt(0);
  if (curNode->IsAnyOfHTMLElements(nsGkAtoms::div, nsGkAtoms::blockquote)) {
    HTMLEditorRef().CollectChildren(*curNode, aNodeArray, 0,
                                    HTMLEditor::CollectListChildren::No,
                                    HTMLEditor::CollectTableChildren::No);
    return;
  }

  aNodeArray.AppendElement(*curNode);
}

void HTMLEditRules::GetDefinitionListItemTypes(dom::Element* aElement,
                                               bool* aDT, bool* aDD) const {
  MOZ_ASSERT(aElement);
  MOZ_ASSERT(aElement->IsHTMLElement(nsGkAtoms::dl));
  MOZ_ASSERT(aDT);
  MOZ_ASSERT(aDD);

  *aDT = *aDD = false;
  for (nsIContent* child = aElement->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    if (child->IsHTMLElement(nsGkAtoms::dt)) {
      *aDT = true;
    } else if (child->IsHTMLElement(nsGkAtoms::dd)) {
      *aDD = true;
    }
  }
}

nsresult HTMLEditRules::GetParagraphFormatNodes(
    nsTArray<OwningNonNull<nsINode>>& outArrayOfNodes) {
  MOZ_ASSERT(IsEditorDataAvailable());

  nsresult rv = HTMLEditorRef().CollectEditTargetNodesInExtendedSelectionRanges(
      outArrayOfNodes, EditSubAction::eCreateOrRemoveBlock);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Pre-process our list of nodes
  for (int32_t i = outArrayOfNodes.Length() - 1; i >= 0; i--) {
    OwningNonNull<nsINode> testNode = outArrayOfNodes[i];

    // Remove all non-editable nodes.  Leave them be.
    if (!HTMLEditorRef().IsEditable(testNode)) {
      outArrayOfNodes.RemoveElementAt(i);
      continue;
    }

    // Scan for table elements.  If we find table elements other than table,
    // replace it with a list of any editable non-table content.  Ditto for
    // list elements.
    if (HTMLEditUtils::IsTableElement(testNode) ||
        HTMLEditUtils::IsList(testNode) ||
        HTMLEditUtils::IsListItem(testNode)) {
      outArrayOfNodes.RemoveElementAt(i);
      HTMLEditorRef().CollectChildren(testNode, outArrayOfNodes, i);
    }
  }
  return NS_OK;
}

nsresult HTMLEditor::SplitParentInlineElementsAtRangeEdges(
    RangeItem& aRangeItem) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (!aRangeItem.IsCollapsed()) {
    nsCOMPtr<nsIContent> mostAncestorInlineContentAtEnd =
        GetMostAncestorInlineElement(*aRangeItem.mEndContainer);

    if (mostAncestorInlineContentAtEnd) {
      SplitNodeResult splitEndInlineResult = SplitNodeDeepWithTransaction(
          *mostAncestorInlineContentAtEnd, aRangeItem.EndPoint(),
          SplitAtEdges::eDoNotCreateEmptyContainer);
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(splitEndInlineResult.Failed())) {
        return splitEndInlineResult.Rv();
      }
      EditorRawDOMPoint splitPointAtEnd(splitEndInlineResult.SplitPoint());
      if (NS_WARN_IF(!splitPointAtEnd.IsSet())) {
        return NS_ERROR_FAILURE;
      }
      aRangeItem.mEndContainer = splitPointAtEnd.GetContainer();
      aRangeItem.mEndOffset = splitPointAtEnd.Offset();
    }
  }

  nsCOMPtr<nsIContent> mostAncestorInlineContentAtStart =
      GetMostAncestorInlineElement(*aRangeItem.mStartContainer);

  if (mostAncestorInlineContentAtStart) {
    SplitNodeResult splitStartInlineResult = SplitNodeDeepWithTransaction(
        *mostAncestorInlineContentAtStart, aRangeItem.StartPoint(),
        SplitAtEdges::eDoNotCreateEmptyContainer);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(splitStartInlineResult.Failed())) {
      return splitStartInlineResult.Rv();
    }
    // XXX If we split only here because of collapsed range, we're modifying
    //     only start point of aRangeItem.  Shouldn't we modify end point here
    //     if it's collapsed?
    EditorRawDOMPoint splitPointAtStart(splitStartInlineResult.SplitPoint());
    if (NS_WARN_IF(!splitPointAtStart.IsSet())) {
      return NS_ERROR_FAILURE;
    }
    aRangeItem.mStartContainer = splitPointAtStart.GetContainer();
    aRangeItem.mStartOffset = splitPointAtStart.Offset();
  }

  return NS_OK;
}

nsresult HTMLEditor::SplitElementsAtEveryBRElement(
    nsIContent& aMostAncestorToBeSplit,
    nsTArray<OwningNonNull<nsINode>>& aOutArrayOfNodes) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // First build up a list of all the break nodes inside the inline container.
  nsTArray<OwningNonNull<nsINode>> arrayOfBreaks;
  BRNodeFunctor functor;
  DOMIterator iter(aMostAncestorToBeSplit);
  iter.AppendList(functor, arrayOfBreaks);

  // If there aren't any breaks, just put inNode itself in the array
  if (arrayOfBreaks.IsEmpty()) {
    aOutArrayOfNodes.AppendElement(aMostAncestorToBeSplit);
    return NS_OK;
  }

  // Else we need to bust up aMostAncestorToBeSplit along all the breaks
  nsCOMPtr<nsIContent> nextContent = &aMostAncestorToBeSplit;
  for (OwningNonNull<nsINode>& brNode : arrayOfBreaks) {
    EditorDOMPoint atBrNode(brNode);
    if (NS_WARN_IF(!atBrNode.IsSet())) {
      return NS_ERROR_FAILURE;
    }
    SplitNodeResult splitNodeResult = SplitNodeDeepWithTransaction(
        *nextContent, atBrNode, SplitAtEdges::eAllowToCreateEmptyContainer);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(splitNodeResult.Failed())) {
      return splitNodeResult.Rv();
    }

    // Put previous node at the split point.
    if (splitNodeResult.GetPreviousNode()) {
      // Might not be a left node.  A break might have been at the very
      // beginning of inline container, in which case
      // SplitNodeDeepWithTransaction() would not actually split anything.
      aOutArrayOfNodes.AppendElement(*splitNodeResult.GetPreviousNode());
    }

    // Move break outside of container and also put in node list
    EditorDOMPoint atNextNode(splitNodeResult.GetNextNode());
    nsresult rv = MoveNodeWithTransaction(MOZ_KnownLive(*brNode->AsContent()),
                                          atNextNode);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    aOutArrayOfNodes.AppendElement(*brNode);

    nextContent = splitNodeResult.GetNextNode();
  }

  // Now tack on remaining next node.
  aOutArrayOfNodes.AppendElement(*nextContent);

  return NS_OK;
}

nsIContent* HTMLEditor::GetMostAncestorInlineElement(nsINode& aNode) const {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (!aNode.IsContent() || HTMLEditor::NodeIsBlockStatic(aNode)) {
    return nullptr;
  }

  Element* host = GetActiveEditingHost();
  if (NS_WARN_IF(!host)) {
    return nullptr;
  }

  // If aNode is the editing host itself, there is no modifiable inline
  // parent.
  if (&aNode == host) {
    return nullptr;
  }

  // If aNode is outside of the <body> element, we don't support to edit
  // such elements for now.
  // XXX This should be MOZ_ASSERT after fixing bug 1413131 for avoiding
  //     calling this expensive method.
  if (NS_WARN_IF(!EditorUtils::IsDescendantOf(aNode, *host))) {
    return nullptr;
  }

  // Looks for the highest inline parent in the editing host.
  nsIContent* content = aNode.AsContent();
  for (nsIContent* parent = content->GetParent();
       parent && parent != host && HTMLEditor::NodeIsInlineStatic(*parent);
       parent = parent->GetParent()) {
    content = parent;
  }
  return content;
}

void HTMLEditRules::MakeTransitionList(
    nsTArray<OwningNonNull<nsINode>>& aNodeArray,
    nsTArray<bool>& aTransitionArray) {
  nsCOMPtr<nsINode> prevParent;

  aTransitionArray.EnsureLengthAtLeast(aNodeArray.Length());
  for (uint32_t i = 0; i < aNodeArray.Length(); i++) {
    if (aNodeArray[i]->GetParentNode() != prevParent) {
      // Different parents: transition point
      aTransitionArray[i] = true;
    } else {
      // Same parents: these nodes grew up together
      aTransitionArray[i] = false;
    }
    prevParent = aNodeArray[i]->GetParentNode();
  }
}

Element* HTMLEditRules::IsInListItem(nsINode* aNode) {
  MOZ_ASSERT(IsEditorDataAvailable());

  NS_ENSURE_TRUE(aNode, nullptr);
  if (HTMLEditUtils::IsListItem(aNode)) {
    return aNode->AsElement();
  }

  Element* parent = aNode->GetParentElement();
  while (parent && HTMLEditorRef().IsDescendantOfEditorRoot(parent) &&
         !HTMLEditUtils::IsTableElement(parent)) {
    if (HTMLEditUtils::IsListItem(parent)) {
      return parent;
    }
    parent = parent->GetParentElement();
  }
  return nullptr;
}

nsAtom& HTMLEditRules::DefaultParagraphSeparator() {
  MOZ_ASSERT(IsEditorDataAvailable());
  return ParagraphSeparatorElement(
      HTMLEditorRef().GetDefaultParagraphSeparator());
}

nsresult HTMLEditRules::ReturnInHeader(Element& aHeader, nsINode& aNode,
                                       int32_t aOffset) {
  MOZ_ASSERT(IsEditorDataAvailable());

  // Remember where the header is
  nsCOMPtr<nsINode> headerParent = aHeader.GetParentNode();
  int32_t offset = headerParent ? headerParent->ComputeIndexOf(&aHeader) : -1;

  // Get ws code to adjust any ws
  nsCOMPtr<nsINode> node = &aNode;
  nsresult rv = WSRunObject::PrepareToSplitAcrossBlocks(
      MOZ_KnownLive(&HTMLEditorRef()), address_of(node), &aOffset);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (NS_WARN_IF(!node->IsContent())) {
    return NS_ERROR_FAILURE;
  }

  // Split the header
  SplitNodeResult splitHeaderResult =
      MOZ_KnownLive(HTMLEditorRef())
          .SplitNodeDeepWithTransaction(
              aHeader, EditorDOMPoint(node, aOffset),
              SplitAtEdges::eAllowToCreateEmptyContainer);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(splitHeaderResult.Succeeded(),
                       "Failed to split aHeader");

  // If the previous heading of split point is empty, put a padding <br>
  // element for empty last line into it.
  nsCOMPtr<nsIContent> prevItem = HTMLEditorRef().GetPriorHTMLSibling(&aHeader);
  if (prevItem) {
    MOZ_DIAGNOSTIC_ASSERT(HTMLEditUtils::IsHeader(*prevItem));
    bool isEmptyNode;
    rv = HTMLEditorRef().IsEmptyNode(prevItem, &isEmptyNode);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    if (isEmptyNode) {
      CreateElementResult createPaddingBRResult =
          MOZ_KnownLive(HTMLEditorRef())
              .InsertPaddingBRElementForEmptyLastLineWithTransaction(
                  EditorDOMPoint(prevItem, 0));
      if (NS_WARN_IF(createPaddingBRResult.Failed())) {
        return createPaddingBRResult.Rv();
      }
    }
  }

  // If the new (righthand) header node is empty, delete it
  if (IsEmptyBlockElement(aHeader, IgnoreSingleBR::eYes)) {
    rv = MOZ_KnownLive(HTMLEditorRef()).DeleteNodeWithTransaction(aHeader);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    // Layout tells the caret to blink in a weird place if we don't place a
    // break after the header.
    nsCOMPtr<nsIContent> sibling;
    if (aHeader.GetNextSibling()) {
      sibling = HTMLEditorRef().GetNextHTMLSibling(aHeader.GetNextSibling());
    }
    if (!sibling || !sibling->IsHTMLElement(nsGkAtoms::br)) {
      HTMLEditorRef()
          .TopLevelEditSubActionDataRef()
          .mCachedInlineStyles.Clear();
      HTMLEditorRef().mTypeInState->ClearAllProps();

      // Create a paragraph
      nsAtom& paraAtom = DefaultParagraphSeparator();
      // We want a wrapper element even if we separate with <br>
      EditorDOMPoint nextToHeader(headerParent, offset + 1);
      RefPtr<Element> pNode =
          MOZ_KnownLive(HTMLEditorRef())
              .CreateNodeWithTransaction(&paraAtom == nsGkAtoms::br
                                             ? *nsGkAtoms::p
                                             : MOZ_KnownLive(paraAtom),
                                         nextToHeader);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(!pNode)) {
        return NS_ERROR_FAILURE;
      }

      // Append a <br> to it
      RefPtr<Element> brElement =
          MOZ_KnownLive(HTMLEditorRef())
              .InsertBRElementWithTransaction(EditorDOMPoint(pNode, 0));
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(!brElement)) {
        return NS_ERROR_FAILURE;
      }

      // Set selection to before the break
      ErrorResult error;
      SelectionRefPtr()->Collapse(EditorRawDOMPoint(pNode, 0), error);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        error.SuppressException();
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(error.Failed())) {
        return error.StealNSResult();
      }
    } else {
      EditorRawDOMPoint afterSibling(sibling);
      if (NS_WARN_IF(!afterSibling.AdvanceOffset())) {
        return NS_ERROR_FAILURE;
      }
      // Put selection after break
      ErrorResult error;
      SelectionRefPtr()->Collapse(afterSibling, error);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        error.SuppressException();
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(error.Failed())) {
        return error.StealNSResult();
      }
    }
  } else {
    // Put selection at front of righthand heading
    ErrorResult error;
    SelectionRefPtr()->Collapse(RawRangeBoundary(&aHeader, 0), error);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      error.SuppressException();
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
  }
  return NS_OK;
}

EditActionResult HTMLEditRules::ReturnInParagraph(Element& aParentDivOrP) {
  MOZ_ASSERT(IsEditorDataAvailable());

  nsRange* firstRange = SelectionRefPtr()->GetRangeAt(0);
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
  // XXX We don't support whitespace collapsing in these cases since it needs
  //     some additional work with WSRunObject but it's not usual case.
  //     E.g., |<a href="foo"><b>foo []</b> </a>|
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
  // user types Enter key after a whitespace which is at middle of
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

  bool doesCRCreateNewP =
      HTMLEditorRef().GetReturnInParagraphCreatesNewParagraph();

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
      brContent = HTMLEditorRef().GetPriorHTMLSibling(
          atStartOfSelection.GetContainer());
      if (!brContent || !HTMLEditorRef().IsVisibleBRElement(brContent) ||
          EditorBase::IsPaddingBRElementForEmptyLastLine(*brContent)) {
        pointToInsertBR.Set(atStartOfSelection.GetContainer());
        brContent = nullptr;
      }
    } else if (atStartOfSelection.IsEndOfContainer()) {
      // we're at the end of text node...
      // is there a BR after to it?
      brContent =
          HTMLEditorRef().GetNextHTMLSibling(atStartOfSelection.GetContainer());
      if (!brContent || !HTMLEditorRef().IsVisibleBRElement(brContent) ||
          EditorBase::IsPaddingBRElementForEmptyLastLine(*brContent)) {
        pointToInsertBR.Set(atStartOfSelection.GetContainer());
        DebugOnly<bool> advanced = pointToInsertBR.AdvanceOffset();
        NS_WARNING_ASSERTION(advanced,
                             "Failed to advance offset to after the container "
                             "of selection start");
        brContent = nullptr;
      }
    } else {
      if (doesCRCreateNewP) {
        ErrorResult error;
        nsCOMPtr<nsIContent> newLeftDivOrP =
            MOZ_KnownLive(HTMLEditorRef())
                .SplitNodeWithTransaction(pointToSplitParentDivOrP, error);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          error.SuppressException();
          return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
        }
        if (NS_WARN_IF(error.Failed())) {
          return EditActionResult(error.StealNSResult());
        }
        pointToSplitParentDivOrP.SetToEndOf(newLeftDivOrP);
      }

      // We need to put new <br> after the left node if given node was split
      // above.
      pointToInsertBR.Set(pointToSplitParentDivOrP.GetContainer());
      DebugOnly<bool> advanced = pointToInsertBR.AdvanceOffset();
      NS_WARNING_ASSERTION(advanced,
                           "Failed to advance offset to after the container "
                           "of selection start");
    }
  } else {
    // not in a text node.
    // is there a BR prior to it?
    nsCOMPtr<nsIContent> nearNode;
    nearNode = HTMLEditorRef().GetPreviousEditableHTMLNode(atStartOfSelection);
    if (!nearNode || !HTMLEditorRef().IsVisibleBRElement(nearNode) ||
        EditorBase::IsPaddingBRElementForEmptyLastLine(*nearNode)) {
      // is there a BR after it?
      nearNode = HTMLEditorRef().GetNextEditableHTMLNode(atStartOfSelection);
      if (!nearNode || !HTMLEditorRef().IsVisibleBRElement(nearNode) ||
          EditorBase::IsPaddingBRElementForEmptyLastLine(*nearNode)) {
        pointToInsertBR = atStartOfSelection;
        splitAfterNewBR = true;
      }
    }
    if (!pointToInsertBR.IsSet() && TextEditUtils::IsBreak(nearNode)) {
      brContent = nearNode;
    }
  }
  if (pointToInsertBR.IsSet()) {
    // if CR does not create a new P, default to BR creation
    if (NS_WARN_IF(!doesCRCreateNewP)) {
      return EditActionResult(NS_OK);
    }

    brContent = MOZ_KnownLive(HTMLEditorRef())
                    .InsertBRElementWithTransaction(pointToInsertBR);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(brContent, "Failed to create a <br> element");
    if (splitAfterNewBR) {
      // We split the parent after the br we've just inserted.
      pointToSplitParentDivOrP.Set(brContent);
      DebugOnly<bool> advanced = pointToSplitParentDivOrP.AdvanceOffset();
      NS_WARNING_ASSERTION(advanced,
                           "Failed to advance offset after the new <br>");
    }
  }
  EditActionResult result(
      SplitParagraph(aParentDivOrP, pointToSplitParentDivOrP, brContent));
  result.MarkAsHandled();
  if (NS_WARN_IF(result.Failed())) {
    return result;
  }
  return result;
}

template <typename PT, typename CT>
nsresult HTMLEditRules::SplitParagraph(
    Element& aParentDivOrP, const EditorDOMPointBase<PT, CT>& aStartOfRightNode,
    nsIContent* aNextBRNode) {
  MOZ_ASSERT(IsEditorDataAvailable());

  // split para
  // get ws code to adjust any ws
  nsCOMPtr<nsINode> selNode = aStartOfRightNode.GetContainer();
  int32_t selOffset = aStartOfRightNode.Offset();
  nsresult rv = WSRunObject::PrepareToSplitAcrossBlocks(
      MOZ_KnownLive(&HTMLEditorRef()), address_of(selNode), &selOffset);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (NS_WARN_IF(!selNode->IsContent())) {
    return NS_ERROR_FAILURE;
  }

  // Split the paragraph.
  SplitNodeResult splitDivOrPResult =
      MOZ_KnownLive(HTMLEditorRef())
          .SplitNodeDeepWithTransaction(
              aParentDivOrP, EditorDOMPoint(selNode, selOffset),
              SplitAtEdges::eAllowToCreateEmptyContainer);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(splitDivOrPResult.Failed())) {
    return splitDivOrPResult.Rv();
  }
  if (NS_WARN_IF(!splitDivOrPResult.DidSplit())) {
    return NS_ERROR_FAILURE;
  }

  // Get rid of the break, if it is visible (otherwise it may be needed to
  // prevent an empty p).
  if (aNextBRNode && HTMLEditorRef().IsVisibleBRElement(aNextBRNode)) {
    rv = MOZ_KnownLive(HTMLEditorRef()).DeleteNodeWithTransaction(*aNextBRNode);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // Remove ID attribute on the paragraph from the existing right node.
  rv = MOZ_KnownLive(HTMLEditorRef())
           .RemoveAttributeWithTransaction(aParentDivOrP, *nsGkAtoms::id);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // We need to ensure to both paragraphs visible even if they are empty.
  // However, padding <br> element for empty last line isn't useful in this
  // case because it'll be ignored by PlaintextSerializer.  Additionally,
  // it'll be exposed as <br> with Element.innerHTML.  Therefore, we can use
  // normal <br> elements for placeholder in this case.  Note that Chromium
  // also behaves so.
  rv = InsertBRIfNeeded(MOZ_KnownLive(*splitDivOrPResult.GetPreviousNode()));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  rv = InsertBRIfNeeded(MOZ_KnownLive(*splitDivOrPResult.GetNextNode()));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // selection to beginning of right hand para;
  // look inside any containers that are up front.
  nsIContent* child = HTMLEditorRef().GetLeftmostChild(&aParentDivOrP, true);
  if (EditorBase::IsTextNode(child) || HTMLEditorRef().IsContainer(child)) {
    EditorRawDOMPoint atStartOfChild(child, 0);
    IgnoredErrorResult ignoredError;
    SelectionRefPtr()->Collapse(atStartOfChild, ignoredError);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(
        !ignoredError.Failed(),
        "Failed to collapse selection at the end of the child");
  } else {
    EditorRawDOMPoint atChild(child);
    IgnoredErrorResult ignoredError;
    SelectionRefPtr()->Collapse(atChild, ignoredError);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(!ignoredError.Failed(),
                         "Failed to collapse selection at the child");
  }
  return NS_OK;
}

nsresult HTMLEditRules::ReturnInListItem(Element& aListItem, nsINode& aNode,
                                         int32_t aOffset) {
  MOZ_ASSERT(IsEditorDataAvailable());
  MOZ_ASSERT(HTMLEditUtils::IsListItem(&aListItem));

  // Get the item parent and the active editing host.
  RefPtr<Element> host = HTMLEditorRef().GetActiveEditingHost();

  // If we are in an empty item, then we want to pop up out of the list, but
  // only if prefs say it's okay and if the parent isn't the active editing
  // host.
  if (host != aListItem.GetParentElement() &&
      IsEmptyBlockElement(aListItem, IgnoreSingleBR::eYes)) {
    nsCOMPtr<nsIContent> leftListNode = aListItem.GetParent();
    // Are we the last list item in the list?
    if (!HTMLEditorRef().IsLastEditableChild(&aListItem)) {
      // We need to split the list!
      EditorDOMPoint atListItem(&aListItem);
      ErrorResult error;
      leftListNode = MOZ_KnownLive(HTMLEditorRef())
                         .SplitNodeWithTransaction(atListItem, error);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        error.SuppressException();
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(error.Failed())) {
        return error.StealNSResult();
      }
    }

    // Are we in a sublist?
    EditorDOMPoint atNextSiblingOfLeftList(leftListNode);
    DebugOnly<bool> advanced = atNextSiblingOfLeftList.AdvanceOffset();
    NS_WARNING_ASSERTION(advanced,
                         "Failed to advance offset after the right list node");
    if (HTMLEditUtils::IsList(atNextSiblingOfLeftList.GetContainer())) {
      // If so, move item out of this list and into the grandparent list
      nsresult rv =
          MOZ_KnownLive(HTMLEditorRef())
              .MoveNodeWithTransaction(aListItem, atNextSiblingOfLeftList);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      ErrorResult error;
      SelectionRefPtr()->Collapse(RawRangeBoundary(&aListItem, 0), error);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        error.SuppressException();
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(error.Failed())) {
        return error.StealNSResult();
      }
    } else {
      // Otherwise kill this item
      nsresult rv =
          MOZ_KnownLive(HTMLEditorRef()).DeleteNodeWithTransaction(aListItem);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      // Time to insert a paragraph
      nsAtom& paraAtom = DefaultParagraphSeparator();
      // We want a wrapper even if we separate with <br>
      RefPtr<Element> pNode =
          MOZ_KnownLive(HTMLEditorRef())
              .CreateNodeWithTransaction(&paraAtom == nsGkAtoms::br
                                             ? *nsGkAtoms::p
                                             : MOZ_KnownLive(paraAtom),
                                         atNextSiblingOfLeftList);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(!pNode)) {
        return NS_ERROR_FAILURE;
      }

      // Append a <br> to it
      RefPtr<Element> brElement =
          MOZ_KnownLive(HTMLEditorRef())
              .InsertBRElementWithTransaction(EditorDOMPoint(pNode, 0));
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(!brElement)) {
        return NS_ERROR_FAILURE;
      }

      // Set selection to before the break
      ErrorResult error;
      SelectionRefPtr()->Collapse(EditorRawDOMPoint(pNode, 0), error);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        error.SuppressException();
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(error.Failed())) {
        return error.StealNSResult();
      }
    }
    return NS_OK;
  }

  // Else we want a new list item at the same list level.  Get ws code to
  // adjust any ws.
  nsCOMPtr<nsINode> selNode = &aNode;
  nsresult rv = WSRunObject::PrepareToSplitAcrossBlocks(
      MOZ_KnownLive(&HTMLEditorRef()), address_of(selNode), &aOffset);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (NS_WARN_IF(!selNode->IsContent())) {
    return NS_ERROR_FAILURE;
  }

  // Now split the list item.
  SplitNodeResult splitListItemResult =
      MOZ_KnownLive(HTMLEditorRef())
          .SplitNodeDeepWithTransaction(
              aListItem, EditorDOMPoint(selNode, aOffset),
              SplitAtEdges::eAllowToCreateEmptyContainer);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(splitListItemResult.Succeeded(),
                       "Failed to split the list item");

  // Hack: until I can change the damaged doc range code back to being
  // extra-inclusive, I have to manually detect certain list items that may be
  // left empty.
  nsCOMPtr<nsIContent> prevItem =
      HTMLEditorRef().GetPriorHTMLSibling(&aListItem);
  if (prevItem && HTMLEditUtils::IsListItem(prevItem)) {
    bool isEmptyNode;
    rv = HTMLEditorRef().IsEmptyNode(prevItem, &isEmptyNode);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    if (isEmptyNode) {
      CreateElementResult createPaddingBRResult =
          MOZ_KnownLive(HTMLEditorRef())
              .InsertPaddingBRElementForEmptyLastLineWithTransaction(
                  EditorDOMPoint(prevItem, 0));
      if (NS_WARN_IF(createPaddingBRResult.Failed())) {
        return createPaddingBRResult.Rv();
      }
    } else {
      rv = HTMLEditorRef().IsEmptyNode(&aListItem, &isEmptyNode, true);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      if (isEmptyNode) {
        RefPtr<nsAtom> nodeAtom = aListItem.NodeInfo()->NameAtom();
        if (nodeAtom == nsGkAtoms::dd || nodeAtom == nsGkAtoms::dt) {
          nsCOMPtr<nsINode> list = aListItem.GetParentNode();
          int32_t itemOffset = list ? list->ComputeIndexOf(&aListItem) : -1;

          nsAtom* listAtom =
              nodeAtom == nsGkAtoms::dt ? nsGkAtoms::dd : nsGkAtoms::dt;
          MOZ_DIAGNOSTIC_ASSERT(itemOffset != -1);
          EditorDOMPoint atNextListItem(list, aListItem.GetNextSibling(),
                                        itemOffset + 1);
          RefPtr<Element> newListItem =
              MOZ_KnownLive(HTMLEditorRef())
                  .CreateNodeWithTransaction(MOZ_KnownLive(*listAtom),
                                             atNextListItem);
          if (NS_WARN_IF(!CanHandleEditAction())) {
            return NS_ERROR_EDITOR_DESTROYED;
          }
          if (NS_WARN_IF(!newListItem)) {
            return NS_ERROR_FAILURE;
          }
          rv = MOZ_KnownLive(HTMLEditorRef())
                   .DeleteNodeWithTransaction(aListItem);
          if (NS_WARN_IF(!CanHandleEditAction())) {
            return NS_ERROR_EDITOR_DESTROYED;
          }
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
          ErrorResult error;
          SelectionRefPtr()->Collapse(EditorRawDOMPoint(newListItem, 0), error);
          if (NS_WARN_IF(!CanHandleEditAction())) {
            error.SuppressException();
            return NS_ERROR_EDITOR_DESTROYED;
          }
          if (NS_WARN_IF(error.Failed())) {
            return error.StealNSResult();
          }
          return NS_OK;
        }

        RefPtr<Element> brElement;
        nsresult rv = MOZ_KnownLive(HTMLEditorRef())
                          .CopyLastEditableChildStylesWithTransaction(
                              MOZ_KnownLive(*prevItem->AsElement()), aListItem,
                              address_of(brElement));
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return NS_ERROR_FAILURE;
        }
        if (brElement) {
          EditorRawDOMPoint atBrNode(brElement);
          if (NS_WARN_IF(!atBrNode.IsSetAndValid())) {
            return NS_ERROR_FAILURE;
          }
          ErrorResult error;
          SelectionRefPtr()->Collapse(atBrNode, error);
          if (NS_WARN_IF(!CanHandleEditAction())) {
            error.SuppressException();
            return NS_ERROR_EDITOR_DESTROYED;
          }
          if (NS_WARN_IF(error.Failed())) {
            return error.StealNSResult();
          }
          return NS_OK;
        }
      } else {
        WSRunObject wsObj(&HTMLEditorRef(), &aListItem, 0);
        nsCOMPtr<nsINode> visNode;
        int32_t visOffset = 0;
        WSType wsType;
        wsObj.NextVisibleNode(EditorRawDOMPoint(&aListItem, 0),
                              address_of(visNode), &visOffset, &wsType);
        if (wsType == WSType::special || wsType == WSType::br ||
            visNode->IsHTMLElement(nsGkAtoms::hr)) {
          EditorRawDOMPoint atVisNode(visNode);
          if (NS_WARN_IF(!atVisNode.IsSetAndValid())) {
            return NS_ERROR_FAILURE;
          }
          ErrorResult error;
          SelectionRefPtr()->Collapse(atVisNode, error);
          if (NS_WARN_IF(!CanHandleEditAction())) {
            error.SuppressException();
            return NS_ERROR_EDITOR_DESTROYED;
          }
          if (NS_WARN_IF(error.Failed())) {
            return error.StealNSResult();
          }
          return NS_OK;
        }

        rv = SelectionRefPtr()->Collapse(visNode, visOffset);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        return NS_OK;
      }
    }
  }

  ErrorResult error;
  SelectionRefPtr()->Collapse(EditorRawDOMPoint(&aListItem, 0), error);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    error.SuppressException();
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }
  return NS_OK;
}

nsresult HTMLEditRules::MakeBlockquote(
    nsTArray<OwningNonNull<nsINode>>& aNodeArray) {
  MOZ_ASSERT(IsEditorDataAvailable());

  // The idea here is to put the nodes into a minimal number of blockquotes.
  // When the user blockquotes something, they expect one blockquote.  That
  // may not be possible (for instance, if they have two table cells selected,
  // you need two blockquotes inside the cells).
  RefPtr<Element> curBlock;
  nsCOMPtr<nsINode> prevParent;

  for (auto& curNode : aNodeArray) {
    // Get the node to act on, and its location
    if (NS_WARN_IF(!curNode->IsContent())) {
      return NS_ERROR_FAILURE;
    }

    // If the node is a table element or list item, dive inside
    if (HTMLEditUtils::IsTableElementButNotTable(curNode) ||
        HTMLEditUtils::IsListItem(curNode)) {
      // Forget any previous block
      curBlock = nullptr;
      // Recursion time
      AutoTArray<OwningNonNull<nsINode>, 24> childNodes;
      HTMLEditor::GetChildNodesOf(*curNode, childNodes);
      nsresult rv = MakeBlockquote(childNodes);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    // If the node has different parent than previous node, further nodes in a
    // new parent
    if (prevParent) {
      if (prevParent != curNode->GetParentNode()) {
        // Forget any previous blockquote node we were using
        curBlock = nullptr;
        prevParent = curNode->GetParentNode();
      }
    } else {
      prevParent = curNode->GetParentNode();
    }

    // If no curBlock, make one
    if (!curBlock) {
      SplitNodeResult splitNodeResult =
          MaybeSplitAncestorsForInsertWithTransaction(*nsGkAtoms::blockquote,
                                                      EditorDOMPoint(curNode));
      if (NS_WARN_IF(splitNodeResult.Failed())) {
        return splitNodeResult.Rv();
      }
      curBlock = MOZ_KnownLive(HTMLEditorRef())
                     .CreateNodeWithTransaction(*nsGkAtoms::blockquote,
                                                splitNodeResult.SplitPoint());
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(!curBlock)) {
        return NS_ERROR_FAILURE;
      }
      // remember our new block for postprocessing
      HTMLEditorRef().TopLevelEditSubActionDataRef().mNewBlockElement =
          curBlock;
      // note: doesn't matter if we set mNewBlockElement multiple times.
    }

    nsresult rv = MOZ_KnownLive(HTMLEditorRef())
                      .MoveNodeToEndWithTransaction(
                          MOZ_KnownLive(*curNode->AsContent()), *curBlock);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  return NS_OK;
}

nsresult HTMLEditRules::RemoveBlockStyle(
    nsTArray<OwningNonNull<nsINode>>& aNodeArray) {
  MOZ_ASSERT(IsEditorDataAvailable());

  // Intent of this routine is to be used for converting to/from headers,
  // paragraphs, pre, and address.  Those blocks that pretty much just contain
  // inline things...
  nsCOMPtr<Element> curBlock;
  nsCOMPtr<nsIContent> firstNode, lastNode;
  for (auto& curNode : aNodeArray) {
    // If curNode is an <address>, <p>, <hn>, or <pre>, remove it.
    if (HTMLEditUtils::IsFormatNode(curNode)) {
      // Process any partial progress saved
      if (curBlock) {
        SplitRangeOffFromNodeResult removeMiddleContainerResult =
            SplitRangeOffFromBlockAndRemoveMiddleContainer(
                *curBlock, *firstNode, *lastNode);
        if (NS_WARN_IF(removeMiddleContainerResult.Failed())) {
          return removeMiddleContainerResult.Rv();
        }
        firstNode = lastNode = curBlock = nullptr;
      }
      if (!HTMLEditorRef().IsEditable(curNode)) {
        continue;
      }
      // Remove current block
      nsresult rv = MOZ_KnownLive(HTMLEditorRef())
                        .RemoveBlockContainerWithTransaction(
                            MOZ_KnownLive(*curNode->AsElement()));
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      continue;
    }

    // XXX How about, <th>, <thead>, <tfoot>, <dt>, <dl>?
    if (curNode->IsAnyOfHTMLElements(
            nsGkAtoms::table, nsGkAtoms::tr, nsGkAtoms::tbody, nsGkAtoms::td,
            nsGkAtoms::li, nsGkAtoms::blockquote, nsGkAtoms::div) ||
        HTMLEditUtils::IsList(curNode)) {
      // Process any partial progress saved
      if (curBlock) {
        SplitRangeOffFromNodeResult removeMiddleContainerResult =
            SplitRangeOffFromBlockAndRemoveMiddleContainer(
                *curBlock, *firstNode, *lastNode);
        if (NS_WARN_IF(removeMiddleContainerResult.Failed())) {
          return removeMiddleContainerResult.Rv();
        }
        firstNode = lastNode = curBlock = nullptr;
      }
      if (!HTMLEditorRef().IsEditable(curNode)) {
        continue;
      }
      // Recursion time
      AutoTArray<OwningNonNull<nsINode>, 24> childNodes;
      HTMLEditor::GetChildNodesOf(*curNode, childNodes);
      nsresult rv = RemoveBlockStyle(childNodes);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      continue;
    }

    if (HTMLEditor::NodeIsInlineStatic(curNode)) {
      if (curBlock) {
        // If so, is this node a descendant?
        if (EditorUtils::IsDescendantOf(*curNode, *curBlock)) {
          // Then we don't need to do anything different for this node
          lastNode = curNode->AsContent();
          continue;
        }
        // Otherwise, we have progressed beyond end of curBlock, so let's
        // handle it now.  We need to remove the portion of curBlock that
        // contains [firstNode - lastNode].
        SplitRangeOffFromNodeResult removeMiddleContainerResult =
            SplitRangeOffFromBlockAndRemoveMiddleContainer(
                *curBlock, *firstNode, *lastNode);
        if (NS_WARN_IF(removeMiddleContainerResult.Failed())) {
          return removeMiddleContainerResult.Rv();
        }
        firstNode = lastNode = curBlock = nullptr;
        // Fall out and handle curNode
      }
      curBlock = HTMLEditorRef().GetBlockNodeParent(curNode);
      if (!curBlock || !HTMLEditUtils::IsFormatNode(curBlock) ||
          !HTMLEditorRef().IsEditable(curBlock)) {
        // Not a block kind that we care about.
        curBlock = nullptr;
      } else {
        firstNode = lastNode = curNode->AsContent();
      }
      continue;
    }

    if (curBlock) {
      // Some node that is already sans block style.  Skip over it and process
      // any partial progress saved.
      SplitRangeOffFromNodeResult removeMiddleContainerResult =
          SplitRangeOffFromBlockAndRemoveMiddleContainer(*curBlock, *firstNode,
                                                         *lastNode);
      if (NS_WARN_IF(removeMiddleContainerResult.Failed())) {
        return removeMiddleContainerResult.Rv();
      }
      firstNode = lastNode = curBlock = nullptr;
      continue;
    }
  }
  // Process any partial progress saved
  if (curBlock) {
    SplitRangeOffFromNodeResult removeMiddleContainerResult =
        SplitRangeOffFromBlockAndRemoveMiddleContainer(*curBlock, *firstNode,
                                                       *lastNode);
    if (NS_WARN_IF(removeMiddleContainerResult.Failed())) {
      return removeMiddleContainerResult.Rv();
    }
    firstNode = lastNode = curBlock = nullptr;
  }
  return NS_OK;
}

nsresult HTMLEditRules::ApplyBlockStyle(
    nsTArray<OwningNonNull<nsINode>>& aNodeArray, nsAtom& aBlockTag) {
  // Intent of this routine is to be used for converting to/from headers,
  // paragraphs, pre, and address.  Those blocks that pretty much just contain
  // inline things...
  MOZ_ASSERT(IsEditorDataAvailable());

  nsCOMPtr<Element> newBlock;

  nsCOMPtr<Element> curBlock;
  for (auto& curNode : aNodeArray) {
    if (NS_WARN_IF(!curNode->GetParent())) {
      // If given node has been removed from the document, let's ignore it
      // since the following code may need its parent replace it with new
      // block.
      curBlock = nullptr;
      newBlock = nullptr;
      continue;
    }

    EditorDOMPoint atCurNode(curNode);

    // Is it already the right kind of block, or an uneditable block?
    if (curNode->IsHTMLElement(&aBlockTag) ||
        (!HTMLEditorRef().IsEditable(curNode) &&
         HTMLEditor::NodeIsBlockStatic(curNode))) {
      // Forget any previous block used for previous inline nodes
      curBlock = nullptr;
      // Do nothing to this block
      continue;
    }

    // If curNode is a address, p, header, address, or pre, replace it with a
    // new block of correct type.
    // XXX: pre can't hold everything the others can
    if (HTMLEditUtils::IsMozDiv(curNode) ||
        HTMLEditUtils::IsFormatNode(curNode)) {
      // Forget any previous block used for previous inline nodes
      curBlock = nullptr;
      newBlock = MOZ_KnownLive(HTMLEditorRef())
                     .ReplaceContainerAndCloneAttributesWithTransaction(
                         MOZ_KnownLive(*curNode->AsElement()), aBlockTag);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(!newBlock)) {
        return NS_ERROR_FAILURE;
      }
      // If the new block element was moved to different element or removed by
      // the web app via mutation event listener, we should stop handling this
      // action since we cannot handle each of a lot of edge cases.
      if (NS_WARN_IF(newBlock->GetParentNode() != atCurNode.GetContainer())) {
        return NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE;
      }
      continue;
    }

    if (HTMLEditUtils::IsTable(curNode) || HTMLEditUtils::IsList(curNode) ||
        curNode->IsAnyOfHTMLElements(nsGkAtoms::tbody, nsGkAtoms::tr,
                                     nsGkAtoms::td, nsGkAtoms::li,
                                     nsGkAtoms::blockquote, nsGkAtoms::div)) {
      // Forget any previous block used for previous inline nodes
      curBlock = nullptr;
      // Recursion time
      AutoTArray<OwningNonNull<nsINode>, 24> childNodes;
      HTMLEditor::GetChildNodesOf(*curNode, childNodes);
      if (!childNodes.IsEmpty()) {
        nsresult rv = ApplyBlockStyle(childNodes, aBlockTag);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        continue;
      }

      // Make sure we can put a block here
      SplitNodeResult splitNodeResult =
          MaybeSplitAncestorsForInsertWithTransaction(aBlockTag, atCurNode);
      if (NS_WARN_IF(splitNodeResult.Failed())) {
        return splitNodeResult.Rv();
      }
      // If current handling node has been moved from the container by a
      // mutation event listener when we need to do something more for it,
      // we should stop handling this action since we cannot handle each of
      // a lot of edge cases.
      if (NS_WARN_IF(atCurNode.HasChildMovedFromContainer())) {
        return NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE;
      }
      EditorDOMPoint splitPoint = splitNodeResult.SplitPoint();
      RefPtr<Element> theBlock =
          MOZ_KnownLive(HTMLEditorRef())
              .CreateNodeWithTransaction(aBlockTag, splitPoint);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(!theBlock)) {
        return NS_ERROR_FAILURE;
      }
      // If the new block element was moved to different element or removed by
      // the web app via mutation event listener, we should stop handling this
      // action since we cannot handle each of a lot of edge cases.
      if (NS_WARN_IF(theBlock->GetParentNode() != splitPoint.GetContainer())) {
        return NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE;
      }
      // Remember our new block for postprocessing
      HTMLEditorRef().TopLevelEditSubActionDataRef().mNewBlockElement =
          std::move(theBlock);
      continue;
    }

    if (curNode->IsHTMLElement(nsGkAtoms::br)) {
      // If the node is a break, we honor it by putting further nodes in a new
      // parent
      if (curBlock) {
        // Forget any previous block used for previous inline nodes
        curBlock = nullptr;
        nsresult rv =
            MOZ_KnownLive(HTMLEditorRef()).DeleteNodeWithTransaction(*curNode);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        continue;
      }

      // The break is the first (or even only) node we encountered.  Create a
      // block for it.
      SplitNodeResult splitNodeResult =
          MaybeSplitAncestorsForInsertWithTransaction(aBlockTag, atCurNode);
      if (NS_WARN_IF(splitNodeResult.Failed())) {
        return splitNodeResult.Rv();
      }
      // If current handling node has been moved from the container by a
      // mutation event listener when we need to do something more for it,
      // we should stop handling this action since we cannot handle each of
      // a lot of edge cases.
      if (NS_WARN_IF(atCurNode.HasChildMovedFromContainer())) {
        return NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE;
      }
      EditorDOMPoint splitPoint = splitNodeResult.SplitPoint();
      curBlock = MOZ_KnownLive(HTMLEditorRef())
                     .CreateNodeWithTransaction(aBlockTag, splitPoint);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(!curBlock)) {
        return NS_ERROR_FAILURE;
      }
      // If the new block element was moved to different element or removed by
      // the web app via mutation event listener, we should stop handling this
      // action since we cannot handle each of a lot of edge cases.
      if (NS_WARN_IF(curBlock->GetParentNode() != splitPoint.GetContainer())) {
        return NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE;
      }
      // Remember our new block for postprocessing
      HTMLEditorRef().TopLevelEditSubActionDataRef().mNewBlockElement =
          curBlock;
      // Note: doesn't matter if we set mNewBlockElement multiple times.
      nsresult rv = MOZ_KnownLive(HTMLEditorRef())
                        .MoveNodeToEndWithTransaction(
                            MOZ_KnownLive(*curNode->AsContent()), *curBlock);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      continue;
    }

    if (HTMLEditor::NodeIsInlineStatic(curNode)) {
      // If curNode is inline, pull it into curBlock.  Note: it's assumed that
      // consecutive inline nodes in aNodeArray are actually members of the
      // same block parent.  This happens to be true now as a side effect of
      // how aNodeArray is contructed, but some additional logic should be
      // added here if that should change
      //
      // If curNode is a non editable, drop it if we are going to <pre>.
      if (&aBlockTag == nsGkAtoms::pre &&
          !HTMLEditorRef().IsEditable(curNode)) {
        // Do nothing to this block
        continue;
      }

      // If no curBlock, make one
      if (!curBlock) {
        SplitNodeResult splitNodeResult =
            MaybeSplitAncestorsForInsertWithTransaction(aBlockTag, atCurNode);
        if (NS_WARN_IF(splitNodeResult.Failed())) {
          return splitNodeResult.Rv();
        }
        // If current handling node has been moved from the container by a
        // mutation event listener when we need to do something more for it,
        // we should stop handling this action since we cannot handle each of
        // a lot of edge cases.
        if (NS_WARN_IF(atCurNode.HasChildMovedFromContainer())) {
          return NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE;
        }
        EditorDOMPoint splitPoint = splitNodeResult.SplitPoint();
        curBlock = MOZ_KnownLive(HTMLEditorRef())
                       .CreateNodeWithTransaction(aBlockTag, splitPoint);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(!curBlock)) {
          return NS_ERROR_FAILURE;
        }
        // If the new block element was moved to different element or removed
        // by the web app via mutation event listener, we should stop handling
        // this action since we cannot handle each of a lot of edge cases.
        if (NS_WARN_IF(curBlock->GetParentNode() !=
                       splitPoint.GetContainer())) {
          return NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE;
        }

        // Update container of curNode.
        atCurNode.Set(curNode);

        // Remember our new block for postprocessing
        HTMLEditorRef().TopLevelEditSubActionDataRef().mNewBlockElement =
            curBlock;
        // Note: doesn't matter if we set mNewBlockElement multiple times.
      }

      if (NS_WARN_IF(!atCurNode.IsSet())) {
        // This is possible due to mutation events, let's not assert
        return NS_ERROR_UNEXPECTED;
      }

      // XXX If curNode is a br, replace it with a return if going to <pre>

      // This is a continuation of some inline nodes that belong together in
      // the same block item.  Use curBlock.
      nsresult rv = MOZ_KnownLive(HTMLEditorRef())
                        .MoveNodeToEndWithTransaction(
                            MOZ_KnownLive(*curNode->AsContent()), *curBlock);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }
  return NS_OK;
}

SplitNodeResult HTMLEditRules::MaybeSplitAncestorsForInsertWithTransaction(
    nsAtom& aTag, const EditorDOMPoint& aStartOfDeepestRightNode) {
  MOZ_ASSERT(IsEditorDataAvailable());

  if (NS_WARN_IF(!aStartOfDeepestRightNode.IsSet())) {
    return SplitNodeResult(NS_ERROR_INVALID_ARG);
  }
  MOZ_ASSERT(aStartOfDeepestRightNode.IsSetAndValid());

  RefPtr<Element> host = HTMLEditorRef().GetActiveEditingHost();
  if (NS_WARN_IF(!host)) {
    return SplitNodeResult(NS_ERROR_FAILURE);
  }

  // The point must be descendant of editing host.
  if (NS_WARN_IF(aStartOfDeepestRightNode.GetContainer() != host &&
                 !EditorUtils::IsDescendantOf(
                     *aStartOfDeepestRightNode.GetContainer(), *host))) {
    return SplitNodeResult(NS_ERROR_INVALID_ARG);
  }

  // Look for a node that can legally contain the tag.
  EditorDOMPoint pointToInsert(aStartOfDeepestRightNode);
  for (; pointToInsert.IsSet();
       pointToInsert.Set(pointToInsert.GetContainer())) {
    // We cannot split active editing host and its ancestor.  So, there is
    // no element to contain the specified element.
    if (NS_WARN_IF(pointToInsert.GetChild() == host)) {
      return SplitNodeResult(NS_ERROR_FAILURE);
    }

    if (HTMLEditorRef().CanContainTag(*pointToInsert.GetContainer(), aTag)) {
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

  SplitNodeResult splitNodeResult =
      MOZ_KnownLive(HTMLEditorRef())
          .SplitNodeDeepWithTransaction(
              MOZ_KnownLive(*pointToInsert.GetChild()),
              aStartOfDeepestRightNode,
              SplitAtEdges::eAllowToCreateEmptyContainer);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return SplitNodeResult(NS_ERROR_EDITOR_DESTROYED);
  }
  NS_WARNING_ASSERTION(splitNodeResult.Succeeded(),
                       "Failed to split the node for insert the element");
  return splitNodeResult;
}

nsresult HTMLEditRules::JoinNearestEditableNodesWithTransaction(
    nsIContent& aNodeLeft, nsIContent& aNodeRight,
    EditorDOMPoint* aNewFirstChildOfRightNode) {
  MOZ_ASSERT(IsEditorDataAvailable());
  MOZ_ASSERT(aNewFirstChildOfRightNode);

  // Caller responsible for left and right node being the same type
  nsCOMPtr<nsINode> parent = aNodeLeft.GetParentNode();
  if (NS_WARN_IF(!parent)) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsINode> rightParent = aNodeRight.GetParentNode();

  // If they don't have the same parent, first move the right node to after
  // the left one
  if (parent != rightParent) {
    int32_t parOffset = parent->ComputeIndexOf(&aNodeLeft);
    nsresult rv = MOZ_KnownLive(HTMLEditorRef())
                      .MoveNodeWithTransaction(
                          aNodeRight, EditorDOMPoint(parent, parOffset));
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  EditorDOMPoint ret(&aNodeRight, aNodeLeft.Length());

  // Separate join rules for differing blocks
  if (HTMLEditUtils::IsList(&aNodeLeft) || aNodeLeft.GetAsText()) {
    // For lists, merge shallow (wouldn't want to combine list items)
    nsresult rv = MOZ_KnownLive(HTMLEditorRef())
                      .JoinNodesWithTransaction(aNodeLeft, aNodeRight);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    *aNewFirstChildOfRightNode = std::move(ret);
    return NS_OK;
  }

  // Remember the last left child, and first right child
  nsCOMPtr<nsIContent> lastLeft =
      HTMLEditorRef().GetLastEditableChild(aNodeLeft);
  if (NS_WARN_IF(!lastLeft)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIContent> firstRight =
      HTMLEditorRef().GetFirstEditableChild(aNodeRight);
  if (NS_WARN_IF(!firstRight)) {
    return NS_ERROR_FAILURE;
  }

  // For list items, divs, etc., merge smart
  nsresult rv = MOZ_KnownLive(HTMLEditorRef())
                    .JoinNodesWithTransaction(aNodeLeft, aNodeRight);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (lastLeft && firstRight &&
      HTMLEditorRef().AreNodesSameType(*lastLeft, *firstRight) &&
      (lastLeft->GetAsText() ||
       (lastLeft->IsElement() && firstRight->IsElement() &&
        CSSEditUtils::ElementsSameStyle(lastLeft->AsElement(),
                                        firstRight->AsElement())))) {
    nsresult rv = JoinNearestEditableNodesWithTransaction(
        *lastLeft, *firstRight, aNewFirstChildOfRightNode);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }
  *aNewFirstChildOfRightNode = std::move(ret);
  return NS_OK;
}

Element* HTMLEditor::GetMostAncestorMailCiteElement(nsINode& aNode) const {
  Element* mailCiteElement = nullptr;
  bool isPlaintextEditor = IsPlaintextEditor();
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

nsresult HTMLEditor::CacheInlineStyles(nsINode& aNode) {
  MOZ_ASSERT(IsTopLevelEditSubActionDataAvailable());

  nsresult rv = GetInlineStyles(
      aNode, TopLevelEditSubActionDataRef().mCachedInlineStyles);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "GetInlineStyles() failed");
  return rv;
}

nsresult HTMLEditor::GetInlineStyles(nsINode& aNode,
                                     AutoStyleCacheArray& aStyleCacheArray) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  bool useCSS = IsCSSEnabled();

  for (StyleCache& styleCache : aStyleCacheArray) {
    // If type-in state is set, don't intervene
    bool typeInSet, unused;
    mTypeInState->GetTypingState(typeInSet, unused, styleCache.mTag,
                                 styleCache.mAttr, nullptr);
    if (typeInSet) {
      continue;
    }

    bool isSet = false;
    nsAutoString outValue;
    // Don't use CSS for <font size>, we don't support it usefully (bug
    // 780035)
    if (!useCSS || (styleCache.mTag == nsGkAtoms::font &&
                    styleCache.mAttr == nsGkAtoms::size)) {
      isSet = IsTextPropertySetByContent(&aNode, styleCache.mTag,
                                         styleCache.mAttr, nullptr, &outValue);
    } else {
      isSet = CSSEditUtils::IsCSSEquivalentToHTMLInlineStyleSet(
          &aNode, styleCache.mTag, styleCache.mAttr, outValue,
          CSSEditUtils::eComputed);
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
    }
    if (isSet) {
      styleCache.mPresent = true;
      styleCache.mValue.Assign(outValue);
    }
  }
  return NS_OK;
}

nsresult HTMLEditor::ReapplyCachedStyles() {
  MOZ_ASSERT(IsTopLevelEditSubActionDataAvailable());

  // The idea here is to examine our cached list of styles and see if any have
  // been removed.  If so, add typeinstate for them, so that they will be
  // reinserted when new content is added.

  // remember if we are in css mode
  bool useCSS = IsCSSEnabled();

  if (!SelectionRefPtr()->RangeCount()) {
    // Nothing to do
    return NS_OK;
  }
  const RangeBoundary& atStartOfSelection =
      SelectionRefPtr()->GetRangeAt(0)->StartRef();
  nsCOMPtr<nsIContent> selNode =
      atStartOfSelection.Container() &&
              atStartOfSelection.Container()->IsContent()
          ? atStartOfSelection.Container()->AsContent()
          : nullptr;
  if (!selNode) {
    // Nothing to do
    return NS_OK;
  }

  AutoStyleCacheArray styleCacheArrayAtInsertionPoint;
  nsresult rv = GetInlineStyles(*selNode, styleCacheArrayAtInsertionPoint);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv == NS_ERROR_EDITOR_DESTROYED ? NS_ERROR_EDITOR_DESTROYED : NS_OK;
  }

  for (size_t i = 0; i < styleCacheArrayAtInsertionPoint.Length(); ++i) {
    StyleCache& styleCacheAtInsertionPoint = styleCacheArrayAtInsertionPoint[i];
    StyleCache& styleCacheBeforeEdit =
        TopLevelEditSubActionDataRef().mCachedInlineStyles[i];
    if (styleCacheBeforeEdit.mPresent) {
      bool bFirst, bAny, bAll;
      bFirst = bAny = bAll = false;

      nsAutoString curValue;
      if (useCSS) {
        // check computed style first in css case
        bAny = CSSEditUtils::IsCSSEquivalentToHTMLInlineStyleSet(
            selNode, styleCacheBeforeEdit.mTag, styleCacheBeforeEdit.mAttr,
            curValue, CSSEditUtils::eComputed);
        if (NS_WARN_IF(Destroyed())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
      }
      if (!bAny) {
        // then check typeinstate and html style
        nsresult rv = GetInlinePropertyBase(
            *styleCacheBeforeEdit.mTag, styleCacheBeforeEdit.mAttr,
            &styleCacheBeforeEdit.mValue, &bFirst, &bAny, &bAll, &curValue);
        if (NS_WARN_IF(Destroyed())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
      // This style has disappeared through deletion.  Let's add the styles to
      // mTypeInState when same style isn't applied to the node already.
      if ((!bAny ||
           IsStyleCachePreservingSubAction(GetTopLevelEditSubAction())) &&
          (!styleCacheAtInsertionPoint.mPresent ||
           styleCacheAtInsertionPoint.mValue != styleCacheBeforeEdit.mValue)) {
        mTypeInState->SetProp(styleCacheBeforeEdit.mTag,
                              styleCacheBeforeEdit.mAttr,
                              styleCacheBeforeEdit.mValue);
      }
    }
  }

  return NS_OK;
}

nsresult HTMLEditRules::InsertBRElementToEmptyListItemsAndTableCellsInRange(
    const RawRangeBoundary& aStartRef, const RawRangeBoundary& aEndRef) {
  MOZ_ASSERT(IsEditorDataAvailable());

  // Gather list of empty nodes
  nsTArray<OwningNonNull<nsINode>> nodeArray;
  EmptyEditableFunctor functor(&HTMLEditorRef());
  DOMIterator iter;
  if (NS_WARN_IF(NS_FAILED(iter.Init(aStartRef, aEndRef)))) {
    return NS_ERROR_FAILURE;
  }
  iter.AppendList(functor, nodeArray);

  // Put padding <br> elements for empty <li> and <td>.
  for (auto& node : nodeArray) {
    // Need to put br at END of node.  It may have empty containers in it and
    // still pass the "IsEmptyNode" test, and we want the br's to be after
    // them.  Also, we want the br to be after the selection if the selection
    // is in this node.
    EditorDOMPoint endOfNode;
    endOfNode.SetToEndOf(node);
    CreateElementResult createPaddingBRResult =
        MOZ_KnownLive(HTMLEditorRef())
            .InsertPaddingBRElementForEmptyLastLineWithTransaction(endOfNode);
    if (NS_WARN_IF(createPaddingBRResult.Failed())) {
      return createPaddingBRResult.Rv();
    }
  }
  return NS_OK;
}

nsresult HTMLEditRules::PinSelectionToNewBlock() {
  MOZ_ASSERT(IsEditorDataAvailable());

  if (!SelectionRefPtr()->IsCollapsed()) {
    return NS_OK;
  }

  if (NS_WARN_IF(
          !HTMLEditorRef().TopLevelEditSubActionDataRef().mNewBlockElement)) {
    return NS_ERROR_NULL_POINTER;
  }

  EditorRawDOMPoint selectionStartPoint(
      EditorBase::GetStartPoint(*SelectionRefPtr()));
  if (NS_WARN_IF(!selectionStartPoint.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  // Use ranges and RangeUtils::CompareNodeToRange() to compare selection
  // start to new block.
  RefPtr<StaticRange> staticRange = StaticRange::Create(
      selectionStartPoint.ToRawRangeBoundary(),
      selectionStartPoint.ToRawRangeBoundary(), IgnoreErrors());
  if (NS_WARN_IF(!staticRange)) {
    return NS_ERROR_FAILURE;
  }

  bool nodeBefore, nodeAfter;
  nsresult rv = RangeUtils::CompareNodeToRange(
      HTMLEditorRef().TopLevelEditSubActionDataRef().mNewBlockElement,
      staticRange, &nodeBefore, &nodeAfter);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (nodeBefore && nodeAfter) {
    return NS_OK;  // selection is inside block
  }

  if (nodeBefore) {
    // selection is after block.  put at end of block.
    nsCOMPtr<nsIContent> tmp = HTMLEditorRef().GetLastEditableChild(
        *HTMLEditorRef().TopLevelEditSubActionDataRef().mNewBlockElement);
    if (!tmp) {
      tmp = HTMLEditorRef().TopLevelEditSubActionDataRef().mNewBlockElement;
    }
    EditorRawDOMPoint endPoint;
    if (EditorBase::IsTextNode(tmp) || HTMLEditorRef().IsContainer(tmp)) {
      endPoint.SetToEndOf(tmp);
    } else {
      endPoint.Set(tmp);
      if (NS_WARN_IF(!endPoint.AdvanceOffset())) {
        return NS_ERROR_FAILURE;
      }
    }
    ErrorResult error;
    SelectionRefPtr()->Collapse(endPoint, error);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      error.SuppressException();
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
    return NS_OK;
  }

  // selection is before block.  put at start of block.
  nsCOMPtr<nsIContent> tmp = HTMLEditorRef().GetFirstEditableChild(
      *HTMLEditorRef().TopLevelEditSubActionDataRef().mNewBlockElement);
  if (!tmp) {
    tmp = HTMLEditorRef().TopLevelEditSubActionDataRef().mNewBlockElement;
  }
  EditorRawDOMPoint atStartOfBlock;
  if (EditorBase::IsTextNode(tmp) || HTMLEditorRef().IsContainer(tmp)) {
    atStartOfBlock.Set(tmp);
  } else {
    atStartOfBlock.Set(tmp, 0);
  }
  ErrorResult error;
  SelectionRefPtr()->Collapse(atStartOfBlock, error);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    error.SuppressException();
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }
  return NS_OK;
}

void HTMLEditRules::CheckInterlinePosition() {
  MOZ_ASSERT(IsEditorDataAvailable());

  // If the selection isn't collapsed, do nothing.
  if (!SelectionRefPtr()->IsCollapsed()) {
    return;
  }

  // Get the (collapsed) selection location
  nsRange* firstRange = SelectionRefPtr()->GetRangeAt(0);
  if (NS_WARN_IF(!firstRange)) {
    return;
  }

  EditorDOMPoint atStartOfSelection(firstRange->StartRef());
  if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
    return;
  }
  MOZ_ASSERT(atStartOfSelection.IsSetAndValid());

  // First, let's check to see if we are after a <br>.  We take care of this
  // special-case first so that we don't accidentally fall through into one of
  // the other conditionals.
  nsCOMPtr<nsIContent> node =
      HTMLEditorRef().GetPreviousEditableHTMLNodeInBlock(atStartOfSelection);
  if (node && node->IsHTMLElement(nsGkAtoms::br)) {
    IgnoredErrorResult ignoredError;
    SelectionRefPtr()->SetInterlinePosition(true, ignoredError);
    NS_WARNING_ASSERTION(!ignoredError.Failed(),
                         "Failed to set interline position");
    return;
  }

  // Are we after a block?  If so try set caret to following content
  if (atStartOfSelection.GetChild()) {
    node = HTMLEditorRef().GetPriorHTMLSibling(atStartOfSelection.GetChild());
  } else {
    node = nullptr;
  }
  if (node && HTMLEditor::NodeIsBlockStatic(*node)) {
    IgnoredErrorResult ignoredError;
    SelectionRefPtr()->SetInterlinePosition(true, ignoredError);
    NS_WARNING_ASSERTION(!ignoredError.Failed(),
                         "Failed to set interline position");
    return;
  }

  // Are we before a block?  If so try set caret to prior content
  if (atStartOfSelection.GetChild()) {
    node = HTMLEditorRef().GetNextHTMLSibling(atStartOfSelection.GetChild());
  } else {
    node = nullptr;
  }
  if (node && HTMLEditor::NodeIsBlockStatic(*node)) {
    IgnoredErrorResult ignoredError;
    SelectionRefPtr()->SetInterlinePosition(false, ignoredError);
    NS_WARNING_ASSERTION(!ignoredError.Failed(),
                         "Failed to unset interline position");
  }
}

nsresult HTMLEditRules::AdjustSelection(nsIEditor::EDirection aAction) {
  MOZ_ASSERT(IsEditorDataAvailable());

  // if the selection isn't collapsed, do nothing.
  // moose: one thing to do instead is check for the case of
  // only a single break selected, and collapse it.  Good thing?  Beats me.
  if (!SelectionRefPtr()->IsCollapsed()) {
    return NS_OK;
  }

  // get the (collapsed) selection location
  EditorDOMPoint point(EditorBase::GetStartPoint(*SelectionRefPtr()));
  if (NS_WARN_IF(!point.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  // are we in an editable node?
  while (!HTMLEditorRef().IsEditable(point.GetContainer())) {
    // scan up the tree until we find an editable place to be
    point.Set(point.GetContainer());
    if (NS_WARN_IF(!point.IsSet())) {
      return NS_ERROR_FAILURE;
    }
  }

  // make sure we aren't in an empty block - user will see no cursor.  If this
  // is happening, put a <br> in the block if allowed.
  RefPtr<Element> theblock = HTMLEditorRef().GetBlock(*point.GetContainer());

  if (theblock && HTMLEditorRef().IsEditable(theblock)) {
    bool isEmptyNode;
    nsresult rv =
        HTMLEditorRef().IsEmptyNode(theblock, &isEmptyNode, false, false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    // check if br can go into the destination node
    if (isEmptyNode &&
        HTMLEditorRef().CanContainTag(*point.GetContainer(), *nsGkAtoms::br)) {
      Element* rootElement = HTMLEditorRef().GetRoot();
      if (NS_WARN_IF(!rootElement)) {
        return NS_ERROR_FAILURE;
      }
      if (point.GetContainer() == rootElement) {
        // Our root node is completely empty. Don't add a <br> here.
        // AfterEditInner() will add one for us when it calls
        // TextEditor::MaybeCreatePaddingBRElementForEmptyEditor()!
        return NS_OK;
      }

      // we know we can skip the rest of this routine given the cirumstance
      CreateElementResult createPaddingBRResult =
          MOZ_KnownLive(HTMLEditorRef())
              .InsertPaddingBRElementForEmptyLastLineWithTransaction(point);
      if (NS_WARN_IF(createPaddingBRResult.Failed())) {
        return createPaddingBRResult.Rv();
      }
      return NS_OK;
    }
  }

  // are we in a text node?
  if (point.IsInTextNode()) {
    return NS_OK;  // we LIKE it when we are in a text node.  that RULZ
  }

  // Do we need to insert a padding <br> element for empty last line?  We do
  // if we are:
  // 1) prior node is in same block where selection is AND
  // 2) prior node is a br AND
  // 3) that br is not visible

  nsCOMPtr<nsIContent> nearNode =
      HTMLEditorRef().GetPreviousEditableHTMLNode(point);
  if (nearNode) {
    // is nearNode also a descendant of same block?
    RefPtr<Element> block = HTMLEditorRef().GetBlock(*point.GetContainer());
    RefPtr<Element> nearBlock = HTMLEditorRef().GetBlockNodeParent(nearNode);
    if (block && block == nearBlock) {
      if (nearNode && TextEditUtils::IsBreak(nearNode)) {
        if (!HTMLEditorRef().IsVisibleBRElement(nearNode)) {
          // need to insert special moz BR. Why?  Because if we don't
          // the user will see no new line for the break.  Also, things
          // like table cells won't grow in height.
          CreateElementResult createPaddingBRResult =
              MOZ_KnownLive(HTMLEditorRef())
                  .InsertPaddingBRElementForEmptyLastLineWithTransaction(point);
          if (NS_WARN_IF(createPaddingBRResult.Failed())) {
            return createPaddingBRResult.Rv();
          }
          point.Set(createPaddingBRResult.GetNewNode());
          // Selection stays *before* padding <br> element for empty last
          // line, sticking to it.
          ErrorResult error;
          SelectionRefPtr()->SetInterlinePosition(true, error);
          if (NS_WARN_IF(!CanHandleEditAction())) {
            error.SuppressException();
            return NS_ERROR_EDITOR_DESTROYED;
          }
          NS_WARNING_ASSERTION(!error.Failed(),
                               "Failed to set interline position");
          error = NS_OK;
          SelectionRefPtr()->Collapse(point, error);
          if (NS_WARN_IF(!CanHandleEditAction())) {
            error.SuppressException();
            return NS_ERROR_EDITOR_DESTROYED;
          }
          if (NS_WARN_IF(error.Failed())) {
            return error.StealNSResult();
          }
        } else {
          nsCOMPtr<nsIContent> nextNode =
              HTMLEditorRef().GetNextEditableHTMLNodeInBlock(*nearNode);
          if (nextNode &&
              EditorBase::IsPaddingBRElementForEmptyLastLine(*nextNode)) {
            // Selection between a <br> element and a padding <br> element for
            // empty last line.  Make it stick to the padding <br> element so
            // that it will be on blank line.
            IgnoredErrorResult ignoredError;
            SelectionRefPtr()->SetInterlinePosition(true, ignoredError);
            NS_WARNING_ASSERTION(!ignoredError.Failed(),
                                 "Failed to set interline position");
          }
        }
      }
    }
  }

  // we aren't in a textnode: are we adjacent to text or a break or an image?
  nearNode = HTMLEditorRef().GetPreviousEditableHTMLNodeInBlock(point);
  if (nearNode &&
      (TextEditUtils::IsBreak(nearNode) || EditorBase::IsTextNode(nearNode) ||
       HTMLEditUtils::IsImage(nearNode) ||
       nearNode->IsHTMLElement(nsGkAtoms::hr))) {
    // this is a good place for the caret to be
    return NS_OK;
  }
  nearNode = HTMLEditorRef().GetNextEditableHTMLNodeInBlock(point);
  if (nearNode &&
      (TextEditUtils::IsBreak(nearNode) || EditorBase::IsTextNode(nearNode) ||
       nearNode->IsAnyOfHTMLElements(nsGkAtoms::img, nsGkAtoms::hr))) {
    return NS_OK;  // this is a good place for the caret to be
  }

  // look for a nearby text node.
  // prefer the correct direction.
  nearNode = FindNearEditableNode(point, aAction);
  if (!nearNode) {
    return NS_OK;
  }

  EditorDOMPoint pt = GetGoodSelPointForNode(*nearNode, aAction);
  ErrorResult error;
  SelectionRefPtr()->Collapse(pt, error);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    error.SuppressException();
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }
  return NS_OK;
}

template <typename PT, typename CT>
nsIContent* HTMLEditRules::FindNearEditableNode(
    const EditorDOMPointBase<PT, CT>& aPoint,
    nsIEditor::EDirection aDirection) {
  MOZ_ASSERT(IsEditorDataAvailable());

  if (NS_WARN_IF(!aPoint.IsSet())) {
    return nullptr;
  }
  MOZ_ASSERT(aPoint.IsSetAndValid());

  nsIContent* nearNode = nullptr;
  if (aDirection == nsIEditor::ePrevious) {
    nearNode = HTMLEditorRef().GetPreviousEditableHTMLNode(aPoint);
    if (!nearNode) {
      return nullptr;  // Not illegal.
    }
  } else {
    nearNode = HTMLEditorRef().GetNextEditableHTMLNode(aPoint);
    if (NS_WARN_IF(!nearNode)) {
      // Perhaps, illegal because the node pointed by aPoint isn't editable
      // and nobody of previous nodes is editable.
      return nullptr;
    }
  }

  // scan in the right direction until we find an eligible text node,
  // but don't cross any breaks, images, or table elements.
  // XXX This comment sounds odd.  |nearNode| may have already crossed breaks
  //     and/or images.
  while (nearNode && !(EditorBase::IsTextNode(nearNode) ||
                       TextEditUtils::IsBreak(nearNode) ||
                       HTMLEditUtils::IsImage(nearNode))) {
    if (aDirection == nsIEditor::ePrevious) {
      nearNode = HTMLEditorRef().GetPreviousEditableHTMLNode(*nearNode);
      if (NS_WARN_IF(!nearNode)) {
        return nullptr;
      }
    } else {
      nearNode = HTMLEditorRef().GetNextEditableHTMLNode(*nearNode);
      if (NS_WARN_IF(!nearNode)) {
        return nullptr;
      }
    }
  }

  // don't cross any table elements
  if (InDifferentTableElements(nearNode, aPoint.GetContainer())) {
    return nullptr;
  }

  // otherwise, ok, we have found a good spot to put the selection
  return nearNode;
}

bool HTMLEditRules::InDifferentTableElements(nsINode* aNode1, nsINode* aNode2) {
  MOZ_ASSERT(aNode1 && aNode2);

  while (aNode1 && !HTMLEditUtils::IsTableElement(aNode1)) {
    aNode1 = aNode1->GetParentNode();
  }

  while (aNode2 && !HTMLEditUtils::IsTableElement(aNode2)) {
    aNode2 = aNode2->GetParentNode();
  }

  return aNode1 != aNode2;
}

nsresult HTMLEditRules::RemoveEmptyNodesInChangedRange() {
  MOZ_ASSERT(IsEditorDataAvailable());

  // Some general notes on the algorithm used here: the goal is to examine all
  // the nodes in TopLevelEditSubActionData::mChangedRange, and remove the
  // empty ones.  We do this by using a content iterator to traverse all the
  // nodes in the range, and placing the empty nodes into an array.  After
  // finishing the iteration, we delete the empty nodes in the array.  (They
  // cannot be deleted as we find them because that would invalidate the
  // iterator.)
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
  nsresult rv = postOrderIter.Init(
      HTMLEditorRef().TopLevelEditSubActionDataRef().mChangedRange);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsTArray<OwningNonNull<nsINode>> arrayOfEmptyNodes, arrayOfEmptyCites,
      skipList;

  // Check for empty nodes
  for (; !postOrderIter.IsDone(); postOrderIter.Next()) {
    OwningNonNull<nsINode> node = *postOrderIter.GetCurrentNode();

    nsCOMPtr<nsINode> parent = node->GetParentNode();

    size_t idx = skipList.IndexOf(node);
    if (idx != skipList.NoIndex) {
      // This node is on our skip list.  Skip processing for this node, and
      // replace its value in the skip list with the value of its parent
      if (parent) {
        skipList[idx] = parent;
      }
    } else {
      bool isCandidate = false;
      bool isEmptyNode = false;
      bool isMailCite = false;

      if (node->IsElement()) {
        if (node->IsHTMLElement(nsGkAtoms::body)) {
          // Don't delete the body
        } else if ((isMailCite = HTMLEditUtils::IsMailCite(node)) ||
                   node->IsHTMLElement(nsGkAtoms::a) ||
                   HTMLEditUtils::IsInlineStyle(node) ||
                   HTMLEditUtils::IsList(node) ||
                   node->IsHTMLElement(nsGkAtoms::div)) {
          // Only consider certain nodes to be empty for purposes of removal
          isCandidate = true;
        } else if (HTMLEditUtils::IsFormatNode(node) ||
                   HTMLEditUtils::IsListItem(node) ||
                   node->IsHTMLElement(nsGkAtoms::blockquote)) {
          // These node types are candidates if selection is not in them.  If
          // it is one of these, don't delete if selection inside.  This is so
          // we can create empty headings, etc., for the user to type into.
          bool isSelectionEndInNode;
          rv = SelectionEndpointInNode(node, &isSelectionEndInNode);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
          if (!isSelectionEndInNode) {
            isCandidate = true;
          }
        }
      }

      if (isCandidate) {
        // We delete mailcites even if they have a solo br in them.  Other
        // nodes we require to be empty.
        rv = HTMLEditorRef().IsEmptyNode(node, &isEmptyNode, isMailCite, true);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        if (isEmptyNode) {
          if (isMailCite) {
            // mailcites go on a separate list from other empty nodes
            arrayOfEmptyCites.AppendElement(*node);
          } else {
            arrayOfEmptyNodes.AppendElement(*node);
          }
        }
      }

      if (!isEmptyNode && parent) {
        // put parent on skip list
        skipList.AppendElement(*parent);
      }
    }
  }

  // now delete the empty nodes
  for (OwningNonNull<nsINode>& delNode : arrayOfEmptyNodes) {
    if (HTMLEditorRef().IsModifiableNode(delNode)) {
      rv = MOZ_KnownLive(HTMLEditorRef()).DeleteNodeWithTransaction(*delNode);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  // Now delete the empty mailcites.  This is a separate step because we want
  // to pull out any br's and preserve them.
  for (OwningNonNull<nsINode>& delNode : arrayOfEmptyCites) {
    bool isEmptyNode;
    rv = HTMLEditorRef().IsEmptyNode(delNode, &isEmptyNode, false, true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    if (!isEmptyNode) {
      // We are deleting a cite that has just a br.  We want to delete cite,
      // but preserve br.
      RefPtr<Element> brElement =
          MOZ_KnownLive(HTMLEditorRef())
              .InsertBRElementWithTransaction(EditorDOMPoint(delNode));
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(!brElement)) {
        return NS_ERROR_FAILURE;
      }
    }
    rv = MOZ_KnownLive(HTMLEditorRef()).DeleteNodeWithTransaction(*delNode);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult HTMLEditRules::SelectionEndpointInNode(nsINode* aNode, bool* aResult) {
  MOZ_ASSERT(IsEditorDataAvailable());

  NS_ENSURE_TRUE(aNode && aResult, NS_ERROR_NULL_POINTER);

  *aResult = false;

  uint32_t rangeCount = SelectionRefPtr()->RangeCount();
  for (uint32_t rangeIdx = 0; rangeIdx < rangeCount; ++rangeIdx) {
    RefPtr<nsRange> range = SelectionRefPtr()->GetRangeAt(rangeIdx);
    nsINode* startContainer = range->GetStartContainer();
    if (startContainer) {
      if (aNode == startContainer) {
        *aResult = true;
        return NS_OK;
      }
      if (EditorUtils::IsDescendantOf(*startContainer, *aNode)) {
        *aResult = true;
        return NS_OK;
      }
    }
    nsINode* endContainer = range->GetEndContainer();
    if (startContainer == endContainer) {
      continue;
    }
    if (endContainer) {
      if (aNode == endContainer) {
        *aResult = true;
        return NS_OK;
      }
      if (EditorUtils::IsDescendantOf(*endContainer, *aNode)) {
        *aResult = true;
        return NS_OK;
      }
    }
  }
  return NS_OK;
}

bool HTMLEditRules::IsEmptyInline(nsINode& aNode) {
  MOZ_ASSERT(IsEditorDataAvailable());

  if (HTMLEditor::NodeIsInlineStatic(aNode) &&
      HTMLEditorRef().IsContainer(&aNode)) {
    bool isEmpty = true;
    HTMLEditorRef().IsEmptyNode(&aNode, &isEmpty);
    return isEmpty;
  }
  return false;
}

bool HTMLEditRules::ListIsEmptyLine(
    nsTArray<OwningNonNull<nsINode>>& aArrayOfNodes) {
  MOZ_ASSERT(IsEditorDataAvailable());

  // We have a list of nodes which we are candidates for being moved into a
  // new block.  Determine if it's anything more than a blank line.  Look for
  // editable content above and beyond one single BR.
  if (NS_WARN_IF(!aArrayOfNodes.Length())) {
    return true;
  }

  int32_t brCount = 0;
  for (auto& node : aArrayOfNodes) {
    if (!HTMLEditorRef().IsEditable(node)) {
      continue;
    }
    if (TextEditUtils::IsBreak(node)) {
      // First break doesn't count
      if (brCount) {
        return false;
      }
      brCount++;
    } else if (IsEmptyInline(node)) {
      // Empty inline, keep looking
    } else {
      return false;
    }
  }
  return true;
}

nsresult HTMLEditRules::PopListItem(nsIContent& aListItem, bool* aOutOfList) {
  MOZ_ASSERT(IsEditorDataAvailable());

  if (aOutOfList) {
    *aOutOfList = false;
  }

  if (NS_WARN_IF(!aListItem.GetParent()) ||
      NS_WARN_IF(!aListItem.GetParent()->GetParentNode()) ||
      !HTMLEditUtils::IsListItem(&aListItem)) {
    return NS_ERROR_FAILURE;
  }

  // if it's first or last list item, don't need to split the list
  // otherwise we do.
  bool isFirstListItem = HTMLEditorRef().IsFirstEditableChild(&aListItem);
  bool isLastListItem = HTMLEditorRef().IsLastEditableChild(&aListItem);

  nsCOMPtr<nsIContent> leftListNode = aListItem.GetParent();

  // If it's at middle of parent list element, split the parent list element.
  // Then, aListItem becomes the first list item of the right list element.
  nsCOMPtr<nsIContent> listItem(&aListItem);
  if (!isFirstListItem && !isLastListItem) {
    EditorDOMPoint atListItem(listItem);
    if (NS_WARN_IF(!atListItem.IsSet())) {
      return NS_ERROR_INVALID_ARG;
    }
    MOZ_ASSERT(atListItem.IsSetAndValid());
    ErrorResult error;
    leftListNode = MOZ_KnownLive(HTMLEditorRef())
                       .SplitNodeWithTransaction(atListItem, error);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      error.SuppressException();
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
  }

  // In most cases, insert the list item into the new left list node..
  EditorDOMPoint pointToInsertListItem(leftListNode);
  if (NS_WARN_IF(!pointToInsertListItem.IsSet())) {
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(pointToInsertListItem.IsSetAndValid());

  // But when the list item was the first child of the right list, it should
  // be inserted between the both list elements.  This allows user to hit
  // Enter twice at a list item breaks the parent list node.
  if (!isFirstListItem) {
    DebugOnly<bool> advanced = pointToInsertListItem.AdvanceOffset();
    NS_WARNING_ASSERTION(advanced,
                         "Failed to advance offset to right list node");
  }

  nsresult rv = MOZ_KnownLive(HTMLEditorRef())
                    .MoveNodeWithTransaction(*listItem, pointToInsertListItem);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // unwrap list item contents if they are no longer in a list
  // XXX If the parent list element is a child of another list element
  //     (although invalid tree), the list item element won't be unwrapped.
  //     That makes the parent ancestor element tree valid, but might be
  //     unexpected result.
  // XXX If aListItem is <dl> or <dd> and current parent is <ul> or <ol>,
  //     the list items won't be unwrapped.  If aListItem is <li> and its
  //     current parent is <dl>, there is same issue.
  if (!HTMLEditUtils::IsList(pointToInsertListItem.GetContainer()) &&
      HTMLEditUtils::IsListItem(listItem)) {
    rv = MOZ_KnownLive(HTMLEditorRef())
             .RemoveBlockContainerWithTransaction(
                 MOZ_KnownLive(*listItem->AsElement()));
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    if (aOutOfList) {
      *aOutOfList = true;
    }
  }
  return NS_OK;
}

nsresult HTMLEditRules::RemoveListStructure(Element& aListElement) {
  MOZ_ASSERT(IsEditorDataAvailable());
  MOZ_ASSERT(HTMLEditUtils::IsList(&aListElement));

  while (aListElement.GetFirstChild()) {
    OwningNonNull<nsIContent> child = *aListElement.GetFirstChild();

    if (HTMLEditUtils::IsListItem(child)) {
      bool isOutOfList;
      // Keep popping it out until it's not in a list anymore
      // XXX Using PopuListItem() is too expensive for this purpose.  Looks
      //     like the reason why this method uses it is, only this loop
      //     wants to work with first child of aList.  However, what it
      //     actually does is removing <li> as container.  So, just using
      //     RemoveBlockContainerWithTransaction() is reasonable.
      // XXX This loop means that if aListElement is is a child of another
      //     list element (although it's invalid tree), this moves the
      //     list item to outside of aListElement's parent.  Is that really
      //     intentional behavior?
      do {
        nsresult rv = PopListItem(child, &isOutOfList);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      } while (!isOutOfList);
      continue;
    }

    if (HTMLEditUtils::IsList(child)) {
      nsresult rv = RemoveListStructure(MOZ_KnownLive(*child->AsElement()));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      continue;
    }

    // Delete any non-list items for now
    // XXX This is not HTML5 aware.  HTML5 allows all list elements to have
    //     <script> and <template> and <dl> element to have <div> to group
    //     some <dt> and <dd> elements.  So, this may break valid children.
    nsresult rv =
        MOZ_KnownLive(HTMLEditorRef()).DeleteNodeWithTransaction(*child);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // Delete the now-empty list
  nsresult rv = MOZ_KnownLive(HTMLEditorRef())
                    .RemoveBlockContainerWithTransaction(aListElement);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult HTMLEditRules::ConfirmSelectionInBody() {
  MOZ_ASSERT(IsEditorDataAvailable());

  Element* rootElement = HTMLEditorRef().GetRoot();
  if (NS_WARN_IF(!rootElement)) {
    return NS_ERROR_UNEXPECTED;
  }

  EditorRawDOMPoint selectionStartPoint(
      EditorBase::GetStartPoint(*SelectionRefPtr()));
  if (NS_WARN_IF(!selectionStartPoint.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  // Check that selection start container is inside the <body> element.
  // XXXsmaug this code is insane.
  nsINode* temp = selectionStartPoint.GetContainer();
  while (temp && !temp->IsHTMLElement(nsGkAtoms::body)) {
    temp = temp->GetParentOrShadowHostNode();
  }

  // If we aren't in the <body> element, force the issue.
  if (!temp) {
    IgnoredErrorResult ignoredError;
    SelectionRefPtr()->Collapse(RawRangeBoundary(rootElement, 0), ignoredError);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(
        !ignoredError.Failed(),
        "Failed to collapse selection at start of the root element");
    return NS_OK;
  }

  EditorRawDOMPoint selectionEndPoint(
      EditorBase::GetEndPoint(*SelectionRefPtr()));
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
    IgnoredErrorResult ignoredError;
    SelectionRefPtr()->Collapse(RawRangeBoundary(rootElement, 0), ignoredError);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(
        !ignoredError.Failed(),
        "Failed to collapse selection at start of the root element");
  }

  return NS_OK;
}

nsresult HTMLEditRules::InsertBRIfNeededInternal(nsINode& aNode,
                                                 bool aForPadding) {
  MOZ_ASSERT(IsEditorDataAvailable());

  if (!HTMLEditor::NodeIsBlockStatic(aNode)) {
    return NS_OK;
  }

  bool isEmpty;
  nsresult rv = HTMLEditorRef().IsEmptyNode(&aNode, &isEmpty);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (!isEmpty) {
    return NS_OK;
  }

  if (aForPadding) {
    CreateElementResult createBRResult =
        MOZ_KnownLive(HTMLEditorRef())
            .InsertPaddingBRElementForEmptyLastLineWithTransaction(
                EditorDOMPoint(&aNode, 0));
    NS_WARNING_ASSERTION(createBRResult.Succeeded(),
                         "Failed to create padding <br> element");
    return createBRResult.Rv();
  }

  RefPtr<Element> brElement =
      MOZ_KnownLive(HTMLEditorRef())
          .InsertBRElementWithTransaction(EditorDOMPoint(&aNode, 0));
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(brElement, "Failed to create <br> element");
  return brElement ? NS_OK : NS_ERROR_FAILURE;
}

void HTMLEditRules::DidCreateNode(Element& aNewElement) {
  if (!mInitialized || !mHTMLEditor) {
    return;  // We've been detached from editor, ignore.
  }

  if (NS_WARN_IF(!CanHandleEditAction())) {
    return;
  }

  AutoSafeEditorData setData(*this, *mHTMLEditor);

  if (!HTMLEditorRef().EditSubActionDataRef().mAdjustChangedRangeFromListener) {
    return;  // Temporarily disabled by edit sub-action handler.
  }

  DebugOnly<nsresult> rv =
      HTMLEditorRef().TopLevelEditSubActionDataRef().AddNodeToChangedRange(
          HTMLEditorRef(), aNewElement);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "AddNodeToChangedRange() failed");
}

void HTMLEditRules::DidInsertNode(nsIContent& aContent) {
  if (!mInitialized || !mHTMLEditor) {
    return;  // We've been detached from editor, ignore.
  }

  if (NS_WARN_IF(!CanHandleEditAction())) {
    return;
  }

  AutoSafeEditorData setData(*this, *mHTMLEditor);

  if (!HTMLEditorRef().EditSubActionDataRef().mAdjustChangedRangeFromListener) {
    return;  // Temporarily disabled by edit sub-action handler.
  }

  DebugOnly<nsresult> rv =
      HTMLEditorRef().TopLevelEditSubActionDataRef().AddNodeToChangedRange(
          HTMLEditorRef(), aContent);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "AddNodeToChangedRange() failed");
}

void HTMLEditRules::WillDeleteNode(nsINode& aChild) {
  if (!mInitialized || !mHTMLEditor) {
    return;  // We've been detached from editor, ignore.
  }

  if (NS_WARN_IF(!CanHandleEditAction())) {
    return;
  }

  AutoSafeEditorData setData(*this, *mHTMLEditor);

  if (!HTMLEditorRef().EditSubActionDataRef().mAdjustChangedRangeFromListener) {
    return;  // Temporarily disabled by edit sub-action handler.
  }

  DebugOnly<nsresult> rv =
      HTMLEditorRef().TopLevelEditSubActionDataRef().AddNodeToChangedRange(
          HTMLEditorRef(), aChild);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "AddNodeToChangedRange() failed");
}

void HTMLEditRules::DidSplitNode(nsINode& aExistingRightNode,
                                 nsINode& aNewLeftNode) {
  if (!mInitialized || !mHTMLEditor) {
    return;  // We've been detached from editor, ignore.
  }

  if (NS_WARN_IF(!CanHandleEditAction())) {
    return;
  }

  AutoSafeEditorData setData(*this, *mHTMLEditor);

  if (!HTMLEditorRef().EditSubActionDataRef().mAdjustChangedRangeFromListener) {
    return;  // Temporarily disabled by edit sub-action handler.
  }

  DebugOnly<nsresult> rv =
      HTMLEditorRef().TopLevelEditSubActionDataRef().AddRangeToChangedRange(
          HTMLEditorRef(), EditorRawDOMPoint(&aNewLeftNode, 0),
          EditorRawDOMPoint(&aExistingRightNode, 0));
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "AddRangeToChangedRange() failed");
}

void HTMLEditRules::WillJoinNodes(nsINode& aLeftNode, nsINode& aRightNode) {
  if (!mInitialized || !mHTMLEditor) {
    return;  // We've been detached from editor, ignore.
  }

  if (NS_WARN_IF(!CanHandleEditAction())) {
    return;
  }

  AutoSafeEditorData setData(*this, *mHTMLEditor);

  if (!HTMLEditorRef().EditSubActionDataRef().mAdjustChangedRangeFromListener) {
    return;  // Temporarily disabled by edit sub-action handler.
  }

  // remember split point
  HTMLEditorRef().EditSubActionDataRef().mJoinedLeftNodeLength =
      aLeftNode.Length();
}

void HTMLEditRules::DidJoinNodes(nsINode& aLeftNode, nsINode& aRightNode) {
  if (!mInitialized || !mHTMLEditor) {
    return;  // We've been detached from editor, ignore.
  }

  if (NS_WARN_IF(!CanHandleEditAction())) {
    return;
  }

  AutoSafeEditorData setData(*this, *mHTMLEditor);

  if (!HTMLEditorRef().EditSubActionDataRef().mAdjustChangedRangeFromListener) {
    return;  // Temporarily disabled by edit sub-action handler.
  }

  DebugOnly<nsresult> rv =
      HTMLEditorRef().TopLevelEditSubActionDataRef().AddPointToChangedRange(
          HTMLEditorRef(),
          EditorRawDOMPoint(
              &aRightNode,
              HTMLEditorRef().EditSubActionDataRef().mJoinedLeftNodeLength));
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "AddPointToChangedRange() failed");
}

void HTMLEditRules::DidInsertText(nsINode& aTextNode, int32_t aOffset,
                                  const nsAString& aString) {
  if (!mInitialized || !mHTMLEditor) {
    return;  // We've been detached from editor, ignore.
  }

  if (NS_WARN_IF(!CanHandleEditAction())) {
    return;
  }

  AutoSafeEditorData setData(*this, *mHTMLEditor);

  if (!HTMLEditorRef().EditSubActionDataRef().mAdjustChangedRangeFromListener) {
    return;  // Temporarily disabled by edit sub-action handler.
  }

  DebugOnly<nsresult> rv =
      HTMLEditorRef().TopLevelEditSubActionDataRef().AddRangeToChangedRange(
          HTMLEditorRef(), EditorRawDOMPoint(&aTextNode, aOffset),
          EditorRawDOMPoint(&aTextNode, aOffset + aString.Length()));
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "AddRangeToChangedRange() failed");
}

void HTMLEditRules::DidDeleteText(nsINode& aTextNode, int32_t aOffset,
                                  int32_t aLength) {
  if (!mInitialized || !mHTMLEditor) {
    return;  // We've been detached from editor, ignore.
  }

  if (NS_WARN_IF(!CanHandleEditAction())) {
    return;
  }

  AutoSafeEditorData setData(*this, *mHTMLEditor);

  if (!HTMLEditorRef().EditSubActionDataRef().mAdjustChangedRangeFromListener) {
    return;  // Temporarily disabled by edit sub-action handler.
  }

  DebugOnly<nsresult> rv =
      HTMLEditorRef().TopLevelEditSubActionDataRef().AddPointToChangedRange(
          HTMLEditorRef(), EditorRawDOMPoint(&aTextNode, aOffset));
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "AddPointToChangedRange() failed");
}

void HTMLEditRules::WillDeleteSelection() {
  if (!mInitialized || !mHTMLEditor) {
    return;  // We've been detached from editor, ignore.
  }

  if (NS_WARN_IF(!CanHandleEditAction())) {
    return;
  }

  AutoSafeEditorData setData(*this, *mHTMLEditor);

  if (!HTMLEditorRef().EditSubActionDataRef().mAdjustChangedRangeFromListener) {
    return;  // Temporarily disabled by edit sub-action handler.
  }

  // XXX Looks like that this is wrong.  We delete multiple selection ranges
  //     once, but this adds only first range into the changed range.
  DebugOnly<nsresult> rv =
      HTMLEditorRef().TopLevelEditSubActionDataRef().AddRangeToChangedRange(
          HTMLEditorRef(), EditorBase::GetStartPoint(*SelectionRefPtr()),
          EditorBase::GetStartPoint(*SelectionRefPtr()));
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "AddRangeToChangedRange() failed");
}

nsresult HTMLEditRules::RemoveAlignment(nsINode& aNode,
                                        const nsAString& aAlignType,
                                        bool aDescendantsOnly) {
  MOZ_ASSERT(IsEditorDataAvailable());

  if (EditorBase::IsTextNode(&aNode) || HTMLEditUtils::IsTable(&aNode)) {
    return NS_OK;
  }

  nsCOMPtr<nsINode> child, tmp;
  if (aDescendantsOnly) {
    child = aNode.GetFirstChild();
  } else {
    child = &aNode;
  }

  bool useCSS = HTMLEditorRef().IsCSSEnabled();

  // Let's remove all alignment hints in the children of aNode; it can
  // be an ALIGN attribute (in case we just remove it) or a CENTER
  // element (here we have to remove the container and keep its
  // children). We break on tables and don't look at their children.
  while (child) {
    if (aDescendantsOnly) {
      // get the next sibling right now because we could have to remove child
      tmp = child->GetNextSibling();
    } else {
      tmp = nullptr;
    }

    if (child->IsHTMLElement(nsGkAtoms::center)) {
      // the current node is a CENTER element
      // first remove children's alignment
      nsresult rv = RemoveAlignment(*child, aAlignType, true);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      // we may have to insert BRs in first and last position of element's
      // children if the nodes before/after are not blocks and not BRs
      rv = MakeSureElemStartsAndEndsOnCR(*child);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      // now remove the CENTER container
      rv = MOZ_KnownLive(HTMLEditorRef())
               .RemoveContainerWithTransaction(
                   MOZ_KnownLive(*child->AsElement()));
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else if (HTMLEditor::NodeIsBlockStatic(*child) ||
               child->IsHTMLElement(nsGkAtoms::hr)) {
      // the current node is a block element
      if (HTMLEditUtils::SupportsAlignAttr(*child)) {
        // remove the ALIGN attribute if this element can have it
        nsresult rv =
            MOZ_KnownLive(HTMLEditorRef())
                .RemoveAttributeWithTransaction(
                    MOZ_KnownLive(*child->AsElement()), *nsGkAtoms::align);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
      if (useCSS) {
        if (child->IsAnyOfHTMLElements(nsGkAtoms::table, nsGkAtoms::hr)) {
          nsresult rv = MOZ_KnownLive(HTMLEditorRef())
                            .SetAttributeOrEquivalent(
                                MOZ_KnownLive(child->AsElement()),
                                nsGkAtoms::align, aAlignType, false);
          if (NS_WARN_IF(!CanHandleEditAction())) {
            return NS_ERROR_EDITOR_DESTROYED;
          }
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
        } else {
          nsAutoString dummyCssValue;
          nsresult rv = HTMLEditorRef().mCSSEditUtils->RemoveCSSInlineStyle(
              *child, nsGkAtoms::textAlign, dummyCssValue);
          if (NS_WARN_IF(!CanHandleEditAction())) {
            return NS_ERROR_EDITOR_DESTROYED;
          }
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
        }
      }
      if (!child->IsHTMLElement(nsGkAtoms::table)) {
        // unless this is a table, look at children
        nsresult rv = RemoveAlignment(*child, aAlignType, true);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
    }
    child = tmp;
  }
  return NS_OK;
}

nsresult HTMLEditRules::MakeSureElemStartsOrEndsOnCR(nsINode& aNode,
                                                     bool aStarts) {
  MOZ_ASSERT(IsEditorDataAvailable());

  nsINode* child = aStarts ? HTMLEditorRef().GetFirstEditableChild(aNode)
                           : HTMLEditorRef().GetLastEditableChild(aNode);
  if (NS_WARN_IF(!child)) {
    return NS_OK;
  }

  bool foundCR = false;
  if (HTMLEditor::NodeIsBlockStatic(*child) ||
      child->IsHTMLElement(nsGkAtoms::br)) {
    foundCR = true;
  } else {
    nsINode* sibling = aStarts ? HTMLEditorRef().GetPriorHTMLSibling(&aNode)
                               : HTMLEditorRef().GetNextHTMLSibling(&aNode);
    if (sibling) {
      if (HTMLEditor::NodeIsBlockStatic(*sibling) ||
          sibling->IsHTMLElement(nsGkAtoms::br)) {
        foundCR = true;
      }
    } else {
      foundCR = true;
    }
  }
  if (!foundCR) {
    EditorDOMPoint pointToInsert;
    if (!aStarts) {
      pointToInsert.SetToEndOf(&aNode);
    } else {
      pointToInsert.Set(&aNode, 0);
    }
    RefPtr<Element> brElement =
        MOZ_KnownLive(HTMLEditorRef())
            .InsertBRElementWithTransaction(pointToInsert);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(!brElement)) {
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

nsresult HTMLEditRules::MakeSureElemStartsAndEndsOnCR(nsINode& aNode) {
  MOZ_ASSERT(IsEditorDataAvailable());

  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }

  AutoSafeEditorData setData(*this, *mHTMLEditor);

  nsresult rv = MakeSureElemStartsOrEndsOnCR(aNode, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  rv = MakeSureElemStartsOrEndsOnCR(aNode, true);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult HTMLEditRules::AlignBlock(Element& aElement,
                                   const nsAString& aAlignType,
                                   ResetAlignOf aResetAlignOf) {
  MOZ_ASSERT(IsEditorDataAvailable());

  if (!HTMLEditor::NodeIsBlockStatic(aElement) &&
      !aElement.IsHTMLElement(nsGkAtoms::hr)) {
    // We deal only with blocks; early way out
    return NS_OK;
  }

  nsresult rv = RemoveAlignment(aElement, aAlignType,
                                aResetAlignOf == ResetAlignOf::OnlyDescendants);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (HTMLEditorRef().IsCSSEnabled()) {
    // Let's use CSS alignment; we use margin-left and margin-right for tables
    // and text-align for other block-level elements
    nsresult rv = MOZ_KnownLive(HTMLEditorRef())
                      .SetAttributeOrEquivalent(&aElement, nsGkAtoms::align,
                                                aAlignType, false);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  // HTML case; this code is supposed to be called ONLY if the element
  // supports the align attribute but we'll never know...
  if (NS_WARN_IF(!HTMLEditUtils::SupportsAlignAttr(aElement))) {
    // XXX error?
    return NS_OK;
  }

  rv = MOZ_KnownLive(HTMLEditorRef())
           .SetAttributeOrEquivalent(&aElement, nsGkAtoms::align, aAlignType,
                                     false);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult HTMLEditRules::ChangeMarginStart(Element& aElement, bool aIncrease) {
  MOZ_ASSERT(IsEditorDataAvailable());

  nsAtom& marginProperty = MarginPropertyAtomForIndent(aElement);
  nsAutoString value;
  CSSEditUtils::GetSpecifiedProperty(aElement, MOZ_KnownLive(marginProperty),
                                     value);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  float f;
  RefPtr<nsAtom> unit;
  CSSEditUtils::ParseLength(value, &f, getter_AddRefs(unit));
  if (!f) {
    nsAutoString defaultLengthUnit;
    CSSEditUtils::GetDefaultLengthUnit(defaultLengthUnit);
    unit = NS_Atomize(defaultLengthUnit);
  }
  int8_t multiplier = aIncrease ? +1 : -1;
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
    nsAutoString newValue;
    newValue.AppendFloat(f);
    newValue.Append(nsDependentAtomString(unit));
    HTMLEditorRef().mCSSEditUtils->SetCSSProperty(
        aElement, MOZ_KnownLive(marginProperty), newValue);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    return NS_OK;
  }

  HTMLEditorRef().mCSSEditUtils->RemoveCSSProperty(
      aElement, MOZ_KnownLive(marginProperty), value);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }

  // Remove unnecessary divs
  if (!aElement.IsHTMLElement(nsGkAtoms::div) ||
      &aElement == HTMLEditorRef().GetActiveEditingHost() ||
      !HTMLEditorRef().IsDescendantOfEditorRoot(&aElement) ||
      HTMLEditor::HasAttributes(&aElement)) {
    return NS_OK;
  }

  nsresult rv =
      MOZ_KnownLive(HTMLEditorRef()).RemoveContainerWithTransaction(aElement);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult HTMLEditRules::WillAbsolutePosition(bool* aCancel, bool* aHandled) {
  MOZ_ASSERT(IsEditorDataAvailable());
  MOZ_ASSERT(aCancel && aHandled);

  // FYI: Ignore cancel result of WillInsert().
  nsresult rv = MOZ_KnownLive(HTMLEditorRef()).WillInsert();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "WillInsert() failed");

  *aCancel = false;
  *aHandled = true;

  RefPtr<Element> focusElement = HTMLEditorRef().GetSelectionContainerElement();
  if (focusElement && HTMLEditUtils::IsImage(focusElement)) {
    HTMLEditorRef().TopLevelEditSubActionDataRef().mNewBlockElement =
        std::move(focusElement);
    return NS_OK;
  }

  rv = NormalizeSelection();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = PrepareToMakeElementAbsolutePosition(
      aHandled,
      address_of(
          HTMLEditorRef().TopLevelEditSubActionDataRef().mNewBlockElement));
  // PrepareToMakeElementAbsolutePosition() may restore selection with
  // AutoSelectionRestorer.  Therefore, the editor might have already been
  // destroyed now.
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult HTMLEditRules::PrepareToMakeElementAbsolutePosition(
    bool* aHandled, RefPtr<Element>* aTargetElement) {
  MOZ_ASSERT(IsEditorDataAvailable());

  MOZ_ASSERT(aHandled);
  MOZ_ASSERT(aTargetElement);

  AutoSelectionRestorer restoreSelectionLater(HTMLEditorRef());

  AutoTArray<RefPtr<nsRange>, 4> arrayOfRanges;
  HTMLEditorRef().GetSelectionRangesExtendedToHardLineStartAndEnd(
      arrayOfRanges, EditSubAction::eSetPositionToAbsolute);

  // Use these ranges to contruct a list of nodes to act on.
  nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
  nsresult rv = MOZ_KnownLive(HTMLEditorRef())
                    .SplitInlinesAndCollectEditTargetNodes(
                        arrayOfRanges, arrayOfNodes,
                        EditSubAction::eSetPositionToAbsolute);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // If nothing visible in list, make an empty block
  if (ListIsEmptyLine(arrayOfNodes)) {
    nsRange* firstRange = SelectionRefPtr()->GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return NS_ERROR_FAILURE;
    }

    EditorDOMPoint atStartOfSelection(firstRange->StartRef());
    if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
      return NS_ERROR_FAILURE;
    }

    // Make sure we can put a block here.
    SplitNodeResult splitNodeResult =
        MaybeSplitAncestorsForInsertWithTransaction(*nsGkAtoms::div,
                                                    atStartOfSelection);
    if (NS_WARN_IF(splitNodeResult.Failed())) {
      return splitNodeResult.Rv();
    }
    RefPtr<Element> positionedDiv =
        MOZ_KnownLive(HTMLEditorRef())
            .CreateNodeWithTransaction(*nsGkAtoms::div,
                                       splitNodeResult.SplitPoint());
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(!positionedDiv)) {
      return NS_ERROR_FAILURE;
    }
    // Remember our new block for postprocessing
    *aTargetElement = positionedDiv;
    // Delete anything that was in the list of nodes
    while (!arrayOfNodes.IsEmpty()) {
      OwningNonNull<nsINode> curNode = arrayOfNodes[0];
      rv = MOZ_KnownLive(HTMLEditorRef()).DeleteNodeWithTransaction(*curNode);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      arrayOfNodes.RemoveElementAt(0);
    }
    // Put selection in new block
    *aHandled = true;
    // Don't restore the selection
    restoreSelectionLater.Abort();
    ErrorResult error;
    SelectionRefPtr()->Collapse(RawRangeBoundary(positionedDiv, 0), error);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      error.SuppressException();
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
    return NS_OK;
  }

  // Okay, now go through all the nodes and put them in a blockquote, or
  // whatever is appropriate.  Woohoo!
  nsCOMPtr<Element> curList, curPositionedDiv, indentedLI;
  for (OwningNonNull<nsINode>& curNode : arrayOfNodes) {
    // Here's where we actually figure out what to do.
    EditorDOMPoint atCurNode(curNode);
    if (NS_WARN_IF(!atCurNode.IsSet())) {
      return NS_ERROR_FAILURE;  // XXX not continue??
    }

    // Ignore all non-editable nodes.  Leave them be.
    if (!HTMLEditorRef().IsEditable(curNode)) {
      continue;
    }

    nsCOMPtr<nsIContent> sibling;

    // Some logic for putting list items into nested lists...
    if (HTMLEditUtils::IsList(atCurNode.GetContainer())) {
      // Check to see if curList is still appropriate.  Which it is if curNode
      // is still right after it in the same list.
      if (curList) {
        sibling = HTMLEditorRef().GetPriorHTMLSibling(curNode);
      }

      if (!curList || (sibling && sibling != curList)) {
        nsAtom* containerName =
            atCurNode.GetContainer()->NodeInfo()->NameAtom();
        // Create a new nested list of correct type.
        SplitNodeResult splitNodeResult =
            MaybeSplitAncestorsForInsertWithTransaction(
                MOZ_KnownLive(*containerName), atCurNode);
        if (NS_WARN_IF(splitNodeResult.Failed())) {
          return splitNodeResult.Rv();
        }
        if (!curPositionedDiv) {
          curPositionedDiv =
              MOZ_KnownLive(HTMLEditorRef())
                  .CreateNodeWithTransaction(*nsGkAtoms::div,
                                             splitNodeResult.SplitPoint());
          if (NS_WARN_IF(!CanHandleEditAction())) {
            return NS_ERROR_EDITOR_DESTROYED;
          }
          NS_WARNING_ASSERTION(
              curPositionedDiv,
              "Failed to create current positioned div element");
          *aTargetElement = curPositionedDiv;
        }
        EditorDOMPoint atEndOfCurPositionedDiv;
        atEndOfCurPositionedDiv.SetToEndOf(curPositionedDiv);
        curList = MOZ_KnownLive(HTMLEditorRef())
                      .CreateNodeWithTransaction(MOZ_KnownLive(*containerName),
                                                 atEndOfCurPositionedDiv);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(!curList)) {
          return NS_ERROR_FAILURE;
        }
        // curList is now the correct thing to put curNode in.  Remember our
        // new block for postprocessing.
      }
      // Tuck the node into the end of the active list
      rv = MOZ_KnownLive(HTMLEditorRef())
               .MoveNodeToEndWithTransaction(
                   MOZ_KnownLive(*curNode->AsContent()), *curList);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      continue;
    }

    // Not a list item, use blockquote?  If we are inside a list item, we
    // don't want to blockquote, we want to sublist the list item.  We may
    // have several nodes listed in the array of nodes to act on, that are in
    // the same list item.  Since we only want to indent that li once, we
    // must keep track of the most recent indented list item, and not indent
    // it if we find another node to act on that is still inside the same li.
    RefPtr<Element> listItem = IsInListItem(curNode);
    if (listItem) {
      if (indentedLI == listItem) {
        // Already indented this list item
        continue;
      }
      // Check to see if curList is still appropriate.  Which it is if
      // curNode is still right after it in the same list.
      if (curList) {
        sibling = HTMLEditorRef().GetPriorHTMLSibling(listItem);
      }

      if (!curList || (sibling && sibling != curList)) {
        EditorDOMPoint atListItem(listItem);
        if (NS_WARN_IF(!atListItem.IsSet())) {
          return NS_ERROR_FAILURE;
        }
        nsAtom* containerName =
            atListItem.GetContainer()->NodeInfo()->NameAtom();
        // Create a new nested list of correct type
        SplitNodeResult splitNodeResult =
            MaybeSplitAncestorsForInsertWithTransaction(
                MOZ_KnownLive(*containerName), atListItem);
        if (NS_WARN_IF(splitNodeResult.Failed())) {
          return splitNodeResult.Rv();
        }
        if (!curPositionedDiv) {
          curPositionedDiv = MOZ_KnownLive(HTMLEditorRef())
                                 .CreateNodeWithTransaction(
                                     *nsGkAtoms::div,
                                     EditorDOMPoint(atListItem.GetContainer()));
          if (NS_WARN_IF(!CanHandleEditAction())) {
            return NS_ERROR_EDITOR_DESTROYED;
          }
          NS_WARNING_ASSERTION(
              curPositionedDiv,
              "Failed to create current positioned div element");
          *aTargetElement = curPositionedDiv;
        }
        EditorDOMPoint atEndOfCurPositionedDiv;
        atEndOfCurPositionedDiv.SetToEndOf(curPositionedDiv);
        curList = MOZ_KnownLive(HTMLEditorRef())
                      .CreateNodeWithTransaction(MOZ_KnownLive(*containerName),
                                                 atEndOfCurPositionedDiv);
        if (NS_WARN_IF(!CanHandleEditAction())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(!curList)) {
          return NS_ERROR_FAILURE;
        }
      }
      rv = MOZ_KnownLive(HTMLEditorRef())
               .MoveNodeToEndWithTransaction(*listItem, *curList);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      // Remember we indented this li
      indentedLI = listItem;
      continue;
    }

    // Need to make a div to put things in if we haven't already
    if (!curPositionedDiv) {
      if (curNode->IsHTMLElement(nsGkAtoms::div)) {
        curPositionedDiv = curNode->AsElement();
        *aTargetElement = curPositionedDiv;
        curList = nullptr;
        continue;
      }
      SplitNodeResult splitNodeResult =
          MaybeSplitAncestorsForInsertWithTransaction(*nsGkAtoms::div,
                                                      atCurNode);
      if (NS_WARN_IF(splitNodeResult.Failed())) {
        return splitNodeResult.Rv();
      }
      curPositionedDiv = MOZ_KnownLive(HTMLEditorRef())
                             .CreateNodeWithTransaction(
                                 *nsGkAtoms::div, splitNodeResult.SplitPoint());
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(!curPositionedDiv)) {
        return NS_ERROR_FAILURE;
      }
      // Remember our new block for postprocessing
      *aTargetElement = curPositionedDiv;
      // curPositionedDiv is now the correct thing to put curNode in
    }

    // Tuck the node into the end of the active blockquote
    rv = MOZ_KnownLive(HTMLEditorRef())
             .MoveNodeToEndWithTransaction(MOZ_KnownLive(*curNode->AsContent()),
                                           *curPositionedDiv);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    // Forget curList, if any
    curList = nullptr;
  }
  return NS_OK;
}

nsresult HTMLEditRules::DidAbsolutePosition() {
  MOZ_ASSERT(IsEditorDataAvailable());

  if (!HTMLEditorRef().TopLevelEditSubActionDataRef().mNewBlockElement) {
    return NS_OK;
  }
  OwningNonNull<Element> newBlock(
      *HTMLEditorRef().TopLevelEditSubActionDataRef().mNewBlockElement);
  nsresult rv = MOZ_KnownLive(HTMLEditorRef())
                    .SetPositionToAbsoluteOrStatic(*newBlock, true);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult HTMLEditRules::WillRemoveAbsolutePosition(bool* aCancel,
                                                   bool* aHandled) {
  MOZ_ASSERT(IsEditorDataAvailable());

  if (NS_WARN_IF(!aCancel) || NS_WARN_IF(!aHandled)) {
    return NS_ERROR_INVALID_ARG;
  }

  // FYI: Ignore cancel result of WillInsert().
  nsresult rv = MOZ_KnownLive(HTMLEditorRef()).WillInsert();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "WillInsert() failed");

  *aCancel = false;
  *aHandled = true;

  RefPtr<Element> element =
      HTMLEditorRef().GetAbsolutelyPositionedSelectionContainer();
  if (NS_WARN_IF(!element)) {
    return NS_ERROR_FAILURE;
  }

  {
    AutoSelectionRestorer restoreSelectionLater(HTMLEditorRef());

    nsresult rv = MOZ_KnownLive(HTMLEditorRef())
                      .SetPositionToAbsoluteOrStatic(*element, false);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // Restoring Selection might cause destroying the HTML editor.
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  return NS_OK;
}

nsresult HTMLEditRules::WillRelativeChangeZIndex(int32_t aChange, bool* aCancel,
                                                 bool* aHandled) {
  MOZ_ASSERT(IsEditorDataAvailable());

  if (NS_WARN_IF(!aCancel) || NS_WARN_IF(!aHandled)) {
    return NS_ERROR_INVALID_ARG;
  }

  // FYI: Ignore cancel result of WillInsert().
  nsresult rv = MOZ_KnownLive(HTMLEditorRef()).WillInsert();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "WillInsert() failed");

  *aCancel = false;
  *aHandled = true;

  RefPtr<Element> element =
      HTMLEditorRef().GetAbsolutelyPositionedSelectionContainer();
  if (NS_WARN_IF(!element)) {
    return NS_ERROR_FAILURE;
  }

  {
    AutoSelectionRestorer restoreSelectionLater(HTMLEditorRef());

    int32_t zIndex;
    nsresult rv = MOZ_KnownLive(HTMLEditorRef())
                      .RelativeChangeElementZIndex(*element, aChange, &zIndex);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // Restoring Selection might cause destroying the HTML editor.
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  return NS_OK;
}

nsresult HTMLEditRules::DocumentModified() {
  nsContentUtils::AddScriptRunner(
      NewRunnableMethod("HTMLEditRules::DocumentModifiedWorker", this,
                        &HTMLEditRules::DocumentModifiedWorker));
  // Be aware, if DocumentModifiedWorker() is called synchronously, the
  // editor might have been destroyed here.
  return NS_OK;
}

void HTMLEditRules::DocumentModifiedWorker() {
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return;
  }

  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);
  nsresult rv = htmlEditor->OnModifyDocument();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::OnModifyDocument() failed");
  Unused << rv;
}

}  // namespace mozilla
