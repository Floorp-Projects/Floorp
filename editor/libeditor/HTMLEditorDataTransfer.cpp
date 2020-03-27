/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/HTMLEditor.h"

#include <string.h>

#include "HTMLEditUtils.h"
#include "InternetCiter.h"
#include "WSRunObject.h"
#include "mozilla/dom/Comment.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/DOMStringList.h"
#include "mozilla/dom/FileReader.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Base64.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/EditAction.h"
#include "mozilla/EditorDOMPoint.h"
#include "mozilla/EditorUtils.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/Preferences.h"
#include "mozilla/SelectionState.h"
#include "nsAString.h"
#include "nsCOMPtr.h"
#include "nsCRTGlue.h"  // for CRLF
#include "nsComponentManagerUtils.h"
#include "nsIScriptError.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsDependentSubstring.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsIClipboard.h"
#include "nsIContent.h"
#include "mozilla/dom/Document.h"
#include "nsIDocumentEncoder.h"
#include "nsIFile.h"
#include "nsIInputStream.h"
#include "nsNameSpaceManager.h"
#include "nsINode.h"
#include "nsIParserUtils.h"
#include "nsIPrincipal.h"
#include "nsISupportsImpl.h"
#include "nsISupportsPrimitives.h"
#include "nsISupportsUtils.h"
#include "nsITransferable.h"
#include "nsIVariant.h"
#include "nsLinebreakConverter.h"
#include "nsLiteralString.h"
#include "nsNetUtil.h"
#include "nsRange.h"
#include "nsReadableUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsStreamUtils.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsStringIterator.h"
#include "nsTreeSanitizer.h"
#include "nsXPCOM.h"
#include "nscore.h"
#include "nsContentUtils.h"
#include "nsQueryObject.h"

class nsAtom;
class nsILoadContext;
class nsISupports;

namespace mozilla {

using namespace dom;

#define kInsertCookie "_moz_Insert Here_moz_"

// some little helpers
static bool FindIntegerAfterString(const char* aLeadingString, nsCString& aCStr,
                                   int32_t& foundNumber);
static void RemoveFragComments(nsCString& aStr);
static void RemoveBodyAndHead(nsINode& aNode);
static bool FindTargetNode(nsINode& aStart, nsCOMPtr<nsINode>& aResult);

nsresult HTMLEditor::LoadHTML(const nsAString& aInputString) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!mInitSucceeded)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // force IME commit; set up rules sniffing and batching
  DebugOnly<nsresult> rvIgnored = CommitComposition();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "EditorBase::CommitComposition() failed, but ignored");
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }

  AutoPlaceholderBatch treatAsOneTransaction(*this);
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eInsertHTMLSource, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  nsresult rv = EnsureNoPaddingBRElementForEmptyEditor();
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::EnsureNoPaddingBRElementForEmptyEditor() failed");
    return rv;
  }

  // Delete Selection, but only if it isn't collapsed, see bug #106269
  if (!SelectionRefPtr()->IsCollapsed()) {
    rv = DeleteSelectionAsSubAction(eNone, eStrip);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "TextEditor::DeleteSelectionAsSubAction(eNone, eStrip) failed");
      return rv;
    }
  }

  // Get the first range in the selection, for context:
  RefPtr<nsRange> range = SelectionRefPtr()->GetRangeAt(0);
  if (NS_WARN_IF(!range)) {
    return NS_ERROR_FAILURE;
  }

  // Create fragment for pasted HTML.
  ErrorResult error;
  RefPtr<DocumentFragment> documentFragment =
      range->CreateContextualFragment(aInputString, error);
  if (error.Failed()) {
    NS_WARNING("nsRange::CreateContextualFragment() failed");
    return error.StealNSResult();
  }

  // Put the fragment into the document at start of selection.
  EditorDOMPoint pointToInsert(range->StartRef());
  // XXX We need to make pointToInsert store offset for keeping traditional
  //     behavior since using only child node to pointing insertion point
  //     changes the behavior when inserted child is moved by mutation
  //     observer.  We need to investigate what we should do here.
  Unused << pointToInsert.Offset();
  for (nsCOMPtr<nsIContent> contentToInsert = documentFragment->GetFirstChild();
       contentToInsert; contentToInsert = documentFragment->GetFirstChild()) {
    rv = InsertNodeWithTransaction(*contentToInsert, pointToInsert);
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::InsertNodeWithTransaction() failed");
      return rv;
    }
    // XXX If the inserted node has been moved by mutation observer,
    //     incrementing offset will cause odd result.  Next new node
    //     will be inserted after existing node and the offset will be
    //     overflown from the container node.
    pointToInsert.Set(pointToInsert.GetContainer(), pointToInsert.Offset() + 1);
    if (NS_WARN_IF(!pointToInsert.Offset())) {
      // Append the remaining children to the container if offset is
      // overflown.
      pointToInsert.SetToEndOf(pointToInsert.GetContainer());
    }
  }

  return NS_OK;
}

NS_IMETHODIMP HTMLEditor::InsertHTML(const nsAString& aInString) {
  nsresult rv = InsertHTMLAsAction(aInString);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::InsertHTMLAsAction() failed");
  return rv;
}

nsresult HTMLEditor::InsertHTMLAsAction(const nsAString& aInString,
                                        nsIPrincipal* aPrincipal) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eInsertHTML,
                                          aPrincipal);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  rv = DoInsertHTMLWithContext(aInString, EmptyString(), EmptyString(),
                               EmptyString(), nullptr, EditorDOMPoint(), true,
                               true, false);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::DoInsertHTMLWithContext() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult HTMLEditor::DoInsertHTMLWithContext(
    const nsAString& aInputString, const nsAString& aContextStr,
    const nsAString& aInfoStr, const nsAString& aFlavor, Document* aSourceDoc,
    const EditorDOMPoint& aPointToInsert, bool aDoDeleteSelection,
    bool aTrustedInput, bool aClearStyle) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!mInitSucceeded)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // force IME commit; set up rules sniffing and batching
  CommitComposition();
  AutoPlaceholderBatch treatAsOneTransaction(*this);
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::ePasteHTMLContent, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");
  ignoredError.SuppressException();

  // create a dom document fragment that represents the structure to paste
  nsCOMPtr<nsINode> fragmentAsNode, streamStartParent, streamEndParent;
  int32_t streamStartOffset = 0, streamEndOffset = 0;

  nsresult rv = CreateDOMFragmentFromPaste(
      aInputString, aContextStr, aInfoStr, address_of(fragmentAsNode),
      address_of(streamStartParent), address_of(streamEndParent),
      &streamStartOffset, &streamEndOffset, aTrustedInput);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::CreateDOMFragmentFromPaste() failed");
    return rv;
  }

  // if we have a destination / target node, we want to insert there
  // rather than in place of the selection
  // ignore aDoDeleteSelection here if aPointToInsert is not set since deletion
  // will also occur later; this block is intended to cover the various
  // scenarios where we are dropping in an editor (and may want to delete
  // the selection before collapsing the selection in the new destination)
  if (aPointToInsert.IsSet()) {
    rv = PrepareToInsertContent(aPointToInsert, aDoDeleteSelection);
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::PrepareToInsertContent() failed");
      return rv;
    }
  }

  // we need to recalculate various things based on potentially new offsets
  // this is work to be completed at a later date (probably by jfrancis)

  AutoTArray<OwningNonNull<nsIContent>, 64> arrayOfTopMostChildContents;
  // If we have stream start point information, lets use it and end point.
  // Otherwise, we should make a range all over the document fragment.
  EditorRawDOMPoint streamStartPoint =
      streamStartParent
          ? EditorRawDOMPoint(streamStartParent, streamStartOffset)
          : EditorRawDOMPoint(fragmentAsNode, 0);
  EditorRawDOMPoint streamEndPoint =
      streamStartParent ? EditorRawDOMPoint(streamEndParent, streamEndOffset)
                        : EditorRawDOMPoint::AtEndOf(fragmentAsNode);

  Unused << streamStartPoint;
  Unused << streamEndPoint;

  HTMLEditor::CollectTopMostChildNodesCompletelyInRange(
      EditorRawDOMPoint(streamStartParent, streamStartOffset),
      EditorRawDOMPoint(streamEndParent, streamEndOffset),
      arrayOfTopMostChildContents);

  if (arrayOfTopMostChildContents.IsEmpty()) {
    // We aren't inserting anything, but if aDoDeleteSelection is set, we do
    // want to delete everything.
    // XXX What will this do? We've already called DeleteSelectionAsSubAtion()
    //     above if insertion point is specified.
    if (aDoDeleteSelection) {
      nsresult rv = DeleteSelectionAsSubAction(eNone, eStrip);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "TextEditor::DeleteSelectionAsSubAction(eNone, eStrip) failed");
        return rv;
      }
    }
    return NS_OK;
  }

  // Are there any table elements in the list?
  // check for table cell selection mode
  bool cellSelectionMode = false;
  RefPtr<Element> cellElement = GetFirstSelectedTableCellElement(ignoredError);
  if (cellElement) {
    cellSelectionMode = true;
  }

  if (cellSelectionMode) {
    // do we have table content to paste?  If so, we want to delete
    // the selected table cells and replace with new table elements;
    // but if not we want to delete _contents_ of cells and replace
    // with non-table elements.  Use cellSelectionMode bool to
    // indicate results.
    if (!HTMLEditUtils::IsTableElement(arrayOfTopMostChildContents[0])) {
      cellSelectionMode = false;
    }
  }

  if (!cellSelectionMode) {
    rv = DeleteSelectionAndPrepareToCreateNode();
    if (NS_FAILED(rv)) {
      NS_WARNING("TextEditor::DeleteSelectionAndPrepareToCreateNode() failed");
      return rv;
    }

    if (aClearStyle) {
      // pasting does not inherit local inline styles
      EditResult result = ClearStyleAt(
          EditorDOMPoint(SelectionRefPtr()->AnchorRef()), nullptr, nullptr);
      if (result.Failed()) {
        NS_WARNING("HTMLEditor::ClearStyleAt() failed");
        return result.Rv();
      }
    }
  } else {
    // Delete whole cells: we will replace with new table content.

    // Braces for artificial block to scope AutoSelectionRestorer.
    // Save current selection since DeleteTableCellWithTransaction() perturbs
    // it.
    {
      AutoSelectionRestorer restoreSelectionLater(*this);
      rv = DeleteTableCellWithTransaction(1);
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::DeleteTableCellWithTransaction(1) failed");
        return rv;
      }
    }
    // collapse selection to beginning of deleted table content
    IgnoredErrorResult ignoredError;
    SelectionRefPtr()->CollapseToStart(ignoredError);
    NS_WARNING_ASSERTION(!ignoredError.Failed(),
                         "Selection::Collapse() failed, but ignored");
  }

  // XXX Why don't we test this first?
  if (IsReadonly()) {
    return NS_OK;
  }

  EditActionResult result = CanHandleHTMLEditSubAction();
  if (result.Failed() || result.Canceled()) {
    NS_WARNING_ASSERTION(result.Succeeded(),
                         "HTMLEditor::CanHandleHTMLEditSubAction() failed");
    return result.Rv();
  }

  UndefineCaretBidiLevel();

  rv = EnsureNoPaddingBRElementForEmptyEditor();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::EnsureNoPaddingBRElementForEmptyEditor() "
                       "failed, but ignored");

  if (NS_SUCCEEDED(rv) && SelectionRefPtr()->IsCollapsed()) {
    nsresult rv = EnsureCaretNotAfterPaddingBRElement();
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::EnsureCaretNotAfterPaddingBRElement() "
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

  // Adjust position based on the first node we are going to insert.
  EditorDOMPoint pointToInsert =
      GetBetterInsertionPointFor(arrayOfTopMostChildContents[0],
                                 EditorBase::GetStartPoint(*SelectionRefPtr()));
  if (!pointToInsert.IsSet()) {
    NS_WARNING("HTMLEditor::GetBetterInsertionPointFor() failed");
    return NS_ERROR_FAILURE;
  }

  // Remove invisible `<br>` element at the point because if there is a `<br>`
  // element at end of what we paste, it will make the existing invisible
  // `<br>` element visible.
  WSRunObject wsObj(*this, pointToInsert);
  if (wsObj.GetEndReasonContent() &&
      wsObj.GetEndReasonContent()->IsHTMLElement(nsGkAtoms::br) &&
      !IsVisibleBRElement(wsObj.GetEndReasonContent())) {
    AutoEditorDOMPointChildInvalidator lockOffset(pointToInsert);
    rv = DeleteNodeWithTransaction(MOZ_KnownLive(*wsObj.GetEndReasonContent()));
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
      return rv;
    }
  }

  bool insertionPointWasInLink = !!GetLinkElement(pointToInsert.GetContainer());

  if (pointToInsert.IsInTextNode()) {
    SplitNodeResult splitNodeResult = SplitNodeDeepWithTransaction(
        MOZ_KnownLive(*pointToInsert.GetContainerAsContent()), pointToInsert,
        SplitAtEdges::eAllowToCreateEmptyContainer);
    if (splitNodeResult.Failed()) {
      NS_WARNING("EditorBase::SplitNodeDeepWithTransaction() failed");
      return splitNodeResult.Rv();
    }
    pointToInsert = splitNodeResult.SplitPoint();
    if (!pointToInsert.IsSet()) {
      NS_WARNING(
          "EditorBase::SplitNodeDeepWithTransaction() didn't return split "
          "point");
      return NS_ERROR_FAILURE;
    }
  }

  {  // Block only for AutoHTMLFragmentBoundariesFixer to hide it from the
     // following code. Note that it may modify arrayOfTopMostChildContents.
    AutoHTMLFragmentBoundariesFixer fixPiecesOfTablesAndLists(
        arrayOfTopMostChildContents);
  }

  MOZ_ASSERT(pointToInsert.GetContainer()->GetChildAt_Deprecated(
                 pointToInsert.Offset()) == pointToInsert.GetChild());

  // Loop over the node list and paste the nodes:
  nsCOMPtr<nsINode> parentBlock =
      IsBlockNode(pointToInsert.GetContainer())
          ? pointToInsert.GetContainer()
          : GetBlockNodeParent(pointToInsert.GetContainer());
  nsCOMPtr<nsIContent> lastInsertedContent;
  nsCOMPtr<nsIContent> insertedContextParentContent;
  for (OwningNonNull<nsIContent>& content : arrayOfTopMostChildContents) {
    if (NS_WARN_IF(content == fragmentAsNode) ||
        NS_WARN_IF(content->IsHTMLElement(nsGkAtoms::body))) {
      return NS_ERROR_FAILURE;
    }

    if (insertedContextParentContent) {
      // If we had to insert something higher up in the paste hierarchy,
      // we want to skip any further paste nodes that descend from that.
      // Else we will paste twice.
      // XXX This check may be really expensive.  Cannot we check whether
      //     the node's `ownerDocument` is the `fragmentAsNode` or not?
      // XXX If content was moved to outside of insertedContextParentContent
      //     by mutation event listeners, we will anyway duplicate it.
      if (EditorUtils::IsDescendantOf(*content,
                                      *insertedContextParentContent)) {
        continue;
      }
    }

    // If a `<table>` or `<tr>` element on the clipboard, and pasting it into
    // a `<table>` or `<tr>` element, insert only the appropriate children
    // instead.
    bool inserted = false;
    if (HTMLEditUtils::IsTableRow(content) &&
        HTMLEditUtils::IsTableRow(pointToInsert.GetContainer()) &&
        (HTMLEditUtils::IsTable(content) ||
         HTMLEditUtils::IsTable(pointToInsert.GetContainer()))) {
      // Move children of current node to the insertion point.
      for (nsCOMPtr<nsIContent> firstChild = content->GetFirstChild();
           firstChild; firstChild = content->GetFirstChild()) {
        EditorDOMPoint insertedPoint =
            InsertNodeIntoProperAncestorWithTransaction(
                *firstChild, pointToInsert,
                SplitAtEdges::eDoNotCreateEmptyContainer);
        if (!insertedPoint.IsSet()) {
          NS_WARNING(
              "HTMLEditor::InsertNodeIntoProperAncestorWithTransaction("
              "SplitAtEdges::eDoNotCreateEmptyContainer) "
              "failed");
          break;
        }
        inserted = true;
        lastInsertedContent = firstChild;
        pointToInsert = insertedPoint;
        DebugOnly<bool> advanced = pointToInsert.AdvanceOffset();
        NS_WARNING_ASSERTION(advanced,
                             "Failed to advance offset from inserted point");
      }
    }
    // If a list element on the clipboard, and pasting it into a list or
    // list item element, insert the appropriate children instead.  I.e.,
    // merge the list elements instead of pasting as a sublist.
    else if (HTMLEditUtils::IsList(content) &&
             (HTMLEditUtils::IsList(pointToInsert.GetContainer()) ||
              HTMLEditUtils::IsListItem(pointToInsert.GetContainer()))) {
      for (nsCOMPtr<nsIContent> firstChild = content->GetFirstChild();
           firstChild; firstChild = content->GetFirstChild()) {
        if (HTMLEditUtils::IsListItem(firstChild) ||
            HTMLEditUtils::IsList(firstChild)) {
          // If we're pasting into empty list item, we should remove it
          // and past current node into the parent list directly.
          // XXX This creates invalid structure if current list item element
          //     is not proper child of the parent element, or current node
          //     is a list element.
          if (HTMLEditUtils::IsListItem(pointToInsert.GetContainer()) &&
              IsEmptyNode(*pointToInsert.GetContainer(), true)) {
            NS_WARNING_ASSERTION(pointToInsert.GetContainerParent(),
                                 "Insertion point is out of the DOM tree");
            if (pointToInsert.GetContainerParent()) {
              pointToInsert.Set(pointToInsert.GetContainer());
              AutoEditorDOMPointChildInvalidator lockOffset(pointToInsert);
              DebugOnly<nsresult> rvIgnored = DeleteNodeWithTransaction(
                  MOZ_KnownLive(*pointToInsert.GetChild()));
              NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                                   "HTMLEditor::DeleteNodeWithTransaction() "
                                   "failed, but ignored");
            }
          }
          EditorDOMPoint insertedPoint =
              InsertNodeIntoProperAncestorWithTransaction(
                  *firstChild, pointToInsert,
                  SplitAtEdges::eDoNotCreateEmptyContainer);
          if (!insertedPoint.IsSet()) {
            NS_WARNING(
                "HTMLEditor::InsertNodeIntoProperAncestorWithTransaction("
                "SplitAtEdges::eDoNotCreateEmptyContainer) failed");
            break;
          }

          inserted = true;
          lastInsertedContent = firstChild;
          pointToInsert = insertedPoint;
          DebugOnly<bool> advanced = pointToInsert.AdvanceOffset();
          NS_WARNING_ASSERTION(advanced,
                               "Failed to advance offset from inserted point");
        }
        // If the child of current node is not list item nor list element,
        // we should remove it from the DOM tree.
        else {
          AutoEditorDOMPointChildInvalidator lockOffset(pointToInsert);
          IgnoredErrorResult ignoredError;
          content->RemoveChild(*firstChild, ignoredError);
          NS_WARNING_ASSERTION(!ignoredError.Failed(),
                               "nsINode::RemoveChild() failed, but ignored");
        }
      }
    }
    // If pasting into a `<pre>` element and current node is a `<pre>` element,
    // move only its children.
    else if (parentBlock && HTMLEditUtils::IsPre(parentBlock) &&
             HTMLEditUtils::IsPre(content)) {
      // Check for pre's going into pre's.
      for (nsCOMPtr<nsIContent> firstChild = content->GetFirstChild();
           firstChild; firstChild = content->GetFirstChild()) {
        EditorDOMPoint insertedPoint =
            InsertNodeIntoProperAncestorWithTransaction(
                *firstChild, pointToInsert,
                SplitAtEdges::eDoNotCreateEmptyContainer);
        if (!insertedPoint.IsSet()) {
          NS_WARNING(
              "HTMLEditor::InsertNodeIntoProperAncestorWithTransaction("
              "SplitAtEdges::eDoNotCreateEmptyContainer) failed");
          break;
        }

        inserted = true;
        lastInsertedContent = firstChild;
        pointToInsert = insertedPoint;
        DebugOnly<bool> advanced = pointToInsert.AdvanceOffset();
        NS_WARNING_ASSERTION(advanced,
                             "Failed to advance offset from inserted point");
      }
    }

    // If we haven't inserted current node nor its children, move current node
    // to the insertion point.
    // XXX Checking `rv` here is odd since it's set only by `IsEmpty()` in
    //     the case of list and list item handling.  And may be it may have
    //     been set not current time.  So, I think that we should ignore it.
    if (!inserted || NS_FAILED(rv)) {
      // MOZ_KnownLive because 'arrayOfTopMostChildContents' is guaranteed to
      // keep it alive.
      EditorDOMPoint insertedPoint =
          InsertNodeIntoProperAncestorWithTransaction(
              MOZ_KnownLive(content), pointToInsert,
              SplitAtEdges::eDoNotCreateEmptyContainer);
      NS_WARNING_ASSERTION(
          insertedPoint.IsSet(),
          "HTMLEditor::InsertNodeIntoProperAncestorWithTransaction("
          "SplitAtEdges::eDoNotCreateEmptyContainer) failed, but ignored");
      if (insertedPoint.IsSet()) {
        lastInsertedContent = content;
        pointToInsert = insertedPoint;
      }

      // Assume failure means no legal parent in the document hierarchy,
      // try again with the parent of content in the paste hierarchy.
      for (nsCOMPtr<nsIContent> parentContent = content;
           parentContent && !insertedPoint.IsSet();
           parentContent = parentContent->GetParent()) {
        if (NS_WARN_IF(!parentContent->GetParent()) ||
            NS_WARN_IF(
                parentContent->GetParent()->IsHTMLElement(nsGkAtoms::body))) {
          continue;
        }
        OwningNonNull<nsIContent> oldParentContent(*parentContent->GetParent());
        insertedPoint = InsertNodeIntoProperAncestorWithTransaction(
            oldParentContent, pointToInsert,
            SplitAtEdges::eDoNotCreateEmptyContainer);
        NS_WARNING_ASSERTION(
            insertedPoint.IsSet(),
            "HTMLEditor::InsertNodeIntoProperAncestorWithTransaction("
            "SplitAtEdges::eDoNotCreateEmptyContainer) failed, but ignored");
        if (insertedPoint.IsSet()) {
          insertedContextParentContent = oldParentContent;
          pointToInsert = insertedPoint;
        }
      }
    }
    if (lastInsertedContent) {
      pointToInsert.Set(lastInsertedContent);
      DebugOnly<bool> advanced = pointToInsert.AdvanceOffset();
      NS_WARNING_ASSERTION(advanced,
                           "Failed to advance offset from inserted point");
    }
  }

  if (!lastInsertedContent) {
    return NS_OK;
  }

  // Now collapse the selection to the end of what we just inserted.
  EditorDOMPoint pointToPutCaret;

  // but don't cross tables
  nsIContent* containerContent = nullptr;
  if (!HTMLEditUtils::IsTable(lastInsertedContent)) {
    containerContent = GetLastEditableLeaf(*lastInsertedContent);
    if (containerContent) {
      Element* mostAncestorTableRelatedElement = nullptr;
      for (Element* maybeTableRelatedElement =
               containerContent->GetAsElementOrParentElement();
           maybeTableRelatedElement &&
           maybeTableRelatedElement != lastInsertedContent;
           maybeTableRelatedElement =
               maybeTableRelatedElement->GetParentElement()) {
        if (HTMLEditUtils::IsTable(maybeTableRelatedElement)) {
          mostAncestorTableRelatedElement = maybeTableRelatedElement;
        }
      }
      // If we're in table elements, we should put caret into the most ancestor
      // table element.
      if (mostAncestorTableRelatedElement) {
        containerContent = mostAncestorTableRelatedElement;
      }
    }
  }
  // If we are not in table elements, we should put caret in the last inserted
  // node.
  if (!containerContent) {
    containerContent = lastInsertedContent;
  }

  // If the container is a text node or a container element except `<table>`
  // element, put caret a end of it.
  if (EditorBase::IsTextNode(containerContent) ||
      (IsContainer(containerContent) &&
       !HTMLEditUtils::IsTable(containerContent))) {
    pointToPutCaret.SetToEndOf(containerContent);
  }
  // Otherwise, i.e., it's an atomic element, `<table>` element or data node,
  // put caret after it.
  else {
    pointToPutCaret.Set(containerContent);
    DebugOnly<bool> advanced = pointToPutCaret.AdvanceOffset();
    NS_WARNING_ASSERTION(advanced, "Failed to advance offset from found node");
  }

  // Make sure we don't end up with selection collapsed after an invisible
  // `<br>` element.
  WSRunScanner wsRunScannerAtCaret(this, pointToPutCaret);
  if (wsRunScannerAtCaret
          .ScanPreviousVisibleNodeOrBlockBoundaryFrom(pointToPutCaret)
          .ReachedBRElement() &&
      !IsVisibleBRElement(wsRunScannerAtCaret.GetStartReasonContent())) {
    WSRunScanner wsRunScannerAtStartReason(
        this, EditorDOMPoint(wsRunScannerAtCaret.GetStartReasonContent()));
    WSScanResult backwardScanFromPointToCaretResult =
        wsRunScannerAtStartReason.ScanPreviousVisibleNodeOrBlockBoundaryFrom(
            pointToPutCaret);
    if (backwardScanFromPointToCaretResult.InNormalWhiteSpacesOrText()) {
      pointToPutCaret = backwardScanFromPointToCaretResult.Point();
    } else if (backwardScanFromPointToCaretResult.ReachedSpecialContent()) {
      // XXX In my understanding, this is odd.  The end reason may not be
      //     same as the reached special content because the equality is
      //     guaranteed only when ReachedCurrentBlockBoundary() returns true.
      //     However, looks like that this code assumes that
      //     GetStartReasonContent() returns the content.
      NS_ASSERTION(wsRunScannerAtStartReason.GetStartReasonContent() ==
                       backwardScanFromPointToCaretResult.GetContent(),
                   "Start reason is not the reached special content");
      pointToPutCaret.SetAfter(
          wsRunScannerAtStartReason.GetStartReasonContent());
    }
  }
  rv = CollapseSelectionTo(pointToPutCaret);
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::CollapseSelectionTo() failed, but ignored");

  // If we didn't start from an `<a href>` element, we should not keep
  // caret in the link to make users type something outside the link.
  if (insertionPointWasInLink) {
    return NS_OK;
  }
  RefPtr<Element> linkElement = GetLinkElement(pointToPutCaret.GetContainer());
  if (!linkElement) {
    return NS_OK;
  }
  // The reason why do that instead of just moving caret after it is, the
  // link might have ended in an invisible `<br>` element.  If so, the code
  // above just placed selection inside that.  So we need to split it instead.
  // XXX Sounds like that it's not really expensive comparing with the reason
  //     to use SplitNodeDeepWithTransaction() here.
  SplitNodeResult splitLinkResult = SplitNodeDeepWithTransaction(
      *linkElement, pointToPutCaret, SplitAtEdges::eDoNotCreateEmptyContainer);
  NS_WARNING_ASSERTION(
      splitLinkResult.Succeeded(),
      "EditorBase::SplitNodeDeepWithTransaction() failed, but ignored");
  if (splitLinkResult.GetPreviousNode()) {
    EditorRawDOMPoint afterLeftLink(splitLinkResult.GetPreviousNode());
    if (afterLeftLink.AdvanceOffset()) {
      nsresult rv = CollapseSelectionTo(afterLeftLink);
      if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "HTMLEditor::CollapseSelectionTo() failed, but ignored");
    }
  }
  return NS_OK;
}

// static
Element* HTMLEditor::GetLinkElement(nsINode* aNode) {
  if (NS_WARN_IF(!aNode)) {
    return nullptr;
  }
  nsINode* node = aNode;
  while (node) {
    if (HTMLEditUtils::IsLink(node)) {
      return node->AsElement();
    }
    node = node->GetParentNode();
  }
  return nullptr;
}

nsresult HTMLEditor::StripFormattingNodes(nsIContent& aNode, bool aListOnly) {
  if (aNode.TextIsOnlyWhitespace()) {
    nsCOMPtr<nsINode> parent = aNode.GetParentNode();
    if (parent) {
      if (!aListOnly || HTMLEditUtils::IsList(parent)) {
        ErrorResult error;
        parent->RemoveChild(aNode, error);
        NS_WARNING_ASSERTION(!error.Failed(), "nsINode::RemoveChild() failed");
        return error.StealNSResult();
      }
      return NS_OK;
    }
  }

  if (!aNode.IsHTMLElement(nsGkAtoms::pre)) {
    nsCOMPtr<nsIContent> child = aNode.GetLastChild();
    while (child) {
      nsCOMPtr<nsIContent> previous = child->GetPreviousSibling();
      nsresult rv = StripFormattingNodes(*child, aListOnly);
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::StripFormattingNodes() failed");
        return rv;
      }
      child = std::move(previous);
    }
  }
  return NS_OK;
}

nsresult HTMLEditor::PrepareTransferable(nsITransferable** aTransferable) {
  return NS_OK;
}

nsresult HTMLEditor::PrepareHTMLTransferable(nsITransferable** aTransferable) {
  MOZ_ASSERT(aTransferable);
  MOZ_ASSERT(!*aTransferable);

  // Create generic Transferable for getting the data
  nsresult rv;
  RefPtr<nsITransferable> transferable =
      do_CreateInstance("@mozilla.org/widget/transferable;1", &rv);
  if (NS_FAILED(rv)) {
    NS_WARNING("do_CreateInstance() failed to create nsITransferable instance");
    return rv;
  }

  if (!transferable) {
    NS_WARNING("do_CreateInstance() returned nullptr, but ignored");
    return NS_OK;
  }

  // Get the nsITransferable interface for getting the data from the clipboard
  RefPtr<Document> destdoc = GetDocument();
  nsILoadContext* loadContext = destdoc ? destdoc->GetLoadContext() : nullptr;
  DebugOnly<nsresult> rvIgnored = transferable->Init(loadContext);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "nsITransferable::Init() failed, but ignored");

  // Create the desired DataFlavor for the type of data
  // we want to get out of the transferable
  // This should only happen in html editors, not plaintext
  if (!IsPlaintextEditor()) {
    rvIgnored = transferable->AddDataFlavor(kNativeHTMLMime);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "nsITransferable::AddDataFlavor(kNativeHTMLMime) failed, but ignored");
    rvIgnored = transferable->AddDataFlavor(kHTMLMime);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "nsITransferable::AddDataFlavor(kHTMLMime) failed, but ignored");
    rvIgnored = transferable->AddDataFlavor(kFileMime);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "nsITransferable::AddDataFlavor(kFileMime) failed, but ignored");

    switch (Preferences::GetInt("clipboard.paste_image_type", 1)) {
      case 0:  // prefer JPEG over PNG over GIF encoding
        rvIgnored = transferable->AddDataFlavor(kJPEGImageMime);
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "nsITransferable::AddDataFlavor(kJPEGImageMime) "
                             "failed, but ignored");
        rvIgnored = transferable->AddDataFlavor(kJPGImageMime);
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "nsITransferable::AddDataFlavor(kJPGImageMime) "
                             "failed, but ignored");
        rvIgnored = transferable->AddDataFlavor(kPNGImageMime);
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "nsITransferable::AddDataFlavor(kPNGImageMime) "
                             "failed, but ignored");
        rvIgnored = transferable->AddDataFlavor(kGIFImageMime);
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "nsITransferable::AddDataFlavor(kGIFImageMime) "
                             "failed, but ignored");
        break;
      case 1:  // prefer PNG over JPEG over GIF encoding (default)
      default:
        rvIgnored = transferable->AddDataFlavor(kPNGImageMime);
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "nsITransferable::AddDataFlavor(kPNGImageMime) "
                             "failed, but ignored");
        rvIgnored = transferable->AddDataFlavor(kJPEGImageMime);
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "nsITransferable::AddDataFlavor(kJPEGImageMime) "
                             "failed, but ignored");
        rvIgnored = transferable->AddDataFlavor(kJPGImageMime);
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "nsITransferable::AddDataFlavor(kJPGImageMime) "
                             "failed, but ignored");
        rvIgnored = transferable->AddDataFlavor(kGIFImageMime);
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "nsITransferable::AddDataFlavor(kGIFImageMime) "
                             "failed, but ignored");
        break;
      case 2:  // prefer GIF over JPEG over PNG encoding
        rvIgnored = transferable->AddDataFlavor(kGIFImageMime);
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "nsITransferable::AddDataFlavor(kGIFImageMime) "
                             "failed, but ignored");
        rvIgnored = transferable->AddDataFlavor(kJPEGImageMime);
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "nsITransferable::AddDataFlavor(kJPEGImageMime) "
                             "failed, but ignored");
        rvIgnored = transferable->AddDataFlavor(kJPGImageMime);
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "nsITransferable::AddDataFlavor(kJPGImageMime) "
                             "failed, but ignored");
        rvIgnored = transferable->AddDataFlavor(kPNGImageMime);
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "nsITransferable::AddDataFlavor(kPNGImageMime) "
                             "failed, but ignored");
        break;
    }
  }
  rvIgnored = transferable->AddDataFlavor(kUnicodeMime);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "nsITransferable::AddDataFlavor(kUnicodeMime) failed, but ignored");
  rvIgnored = transferable->AddDataFlavor(kMozTextInternal);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "nsITransferable::AddDataFlavor(kMozTextInternal) failed, but ignored");

  transferable.forget(aTransferable);

  return NS_OK;
}

bool FindIntegerAfterString(const char* aLeadingString, nsCString& aCStr,
                            int32_t& foundNumber) {
  // first obtain offsets from cfhtml str
  int32_t numFront = aCStr.Find(aLeadingString);
  if (numFront == -1) {
    return false;
  }
  numFront += strlen(aLeadingString);

  int32_t numBack = aCStr.FindCharInSet(CRLF, numFront);
  if (numBack == -1) {
    return false;
  }

  nsAutoCString numStr(Substring(aCStr, numFront, numBack - numFront));
  nsresult errorCode;
  foundNumber = numStr.ToInteger(&errorCode);
  return true;
}

void RemoveFragComments(nsCString& aStr) {
  // remove the StartFragment/EndFragment comments from the str, if present
  int32_t startCommentIndx = aStr.Find("<!--StartFragment");
  if (startCommentIndx >= 0) {
    int32_t startCommentEnd = aStr.Find("-->", false, startCommentIndx);
    if (startCommentEnd > startCommentIndx) {
      aStr.Cut(startCommentIndx, (startCommentEnd + 3) - startCommentIndx);
    }
  }
  int32_t endCommentIndx = aStr.Find("<!--EndFragment");
  if (endCommentIndx >= 0) {
    int32_t endCommentEnd = aStr.Find("-->", false, endCommentIndx);
    if (endCommentEnd > endCommentIndx) {
      aStr.Cut(endCommentIndx, (endCommentEnd + 3) - endCommentIndx);
    }
  }
}

nsresult HTMLEditor::ParseCFHTML(nsCString& aCfhtml, char16_t** aStuffToPaste,
                                 char16_t** aCfcontext) {
  // First obtain offsets from cfhtml str.
  int32_t startHTML, endHTML, startFragment, endFragment;
  if (!FindIntegerAfterString("StartHTML:", aCfhtml, startHTML) ||
      startHTML < -1) {
    return NS_ERROR_FAILURE;
  }
  if (!FindIntegerAfterString("EndHTML:", aCfhtml, endHTML) || endHTML < -1) {
    return NS_ERROR_FAILURE;
  }
  if (!FindIntegerAfterString("StartFragment:", aCfhtml, startFragment) ||
      startFragment < 0) {
    return NS_ERROR_FAILURE;
  }
  if (!FindIntegerAfterString("EndFragment:", aCfhtml, endFragment) ||
      startFragment < 0) {
    return NS_ERROR_FAILURE;
  }

  // The StartHTML and EndHTML markers are allowed to be -1 to include
  // everything.
  //   See Reference: MSDN doc entitled "HTML Clipboard Format"
  //   http://msdn.microsoft.com/en-us/library/aa767917(VS.85).aspx#unknown_854
  if (startHTML == -1) {
    startHTML = aCfhtml.Find("<!--StartFragment-->");
    if (startHTML == -1) {
      return NS_OK;
    }
  }
  if (endHTML == -1) {
    const char endFragmentMarker[] = "<!--EndFragment-->";
    endHTML = aCfhtml.Find(endFragmentMarker);
    if (endHTML == -1) {
      return NS_OK;
    }
    endHTML += ArrayLength(endFragmentMarker) - 1;
  }

  // create context string
  nsAutoCString contextUTF8(
      Substring(aCfhtml, startHTML, startFragment - startHTML) +
      NS_LITERAL_CSTRING("<!--" kInsertCookie "-->") +
      Substring(aCfhtml, endFragment, endHTML - endFragment));

  // validate startFragment
  // make sure it's not in the middle of a HTML tag
  // see bug #228879 for more details
  int32_t curPos = startFragment;
  while (curPos > startHTML) {
    if (aCfhtml[curPos] == '>') {
      // working backwards, the first thing we see is the end of a tag
      // so StartFragment is good, so do nothing.
      break;
    }
    if (aCfhtml[curPos] == '<') {
      // if we are at the start, then we want to see the '<'
      if (curPos != startFragment) {
        // working backwards, the first thing we see is the start of a tag
        // so StartFragment is bad, so we need to update it.
        NS_ERROR(
            "StartFragment byte count in the clipboard looks bad, see bug "
            "#228879");
        startFragment = curPos - 1;
      }
      break;
    }
    curPos--;
  }

  // create fragment string
  nsAutoCString fragmentUTF8(
      Substring(aCfhtml, startFragment, endFragment - startFragment));

  // remove the StartFragment/EndFragment comments from the fragment, if present
  RemoveFragComments(fragmentUTF8);

  // remove the StartFragment/EndFragment comments from the context, if present
  RemoveFragComments(contextUTF8);

  // convert both strings to usc2
  const nsString& fragUcs2Str = NS_ConvertUTF8toUTF16(fragmentUTF8);
  const nsString& cntxtUcs2Str = NS_ConvertUTF8toUTF16(contextUTF8);

  // translate platform linebreaks for fragment
  int32_t oldLengthInChars =
      fragUcs2Str.Length() + 1;  // +1 to include null terminator
  int32_t newLengthInChars = 0;
  *aStuffToPaste = nsLinebreakConverter::ConvertUnicharLineBreaks(
      fragUcs2Str.get(), nsLinebreakConverter::eLinebreakAny,
      nsLinebreakConverter::eLinebreakContent, oldLengthInChars,
      &newLengthInChars);
  if (!*aStuffToPaste) {
    NS_WARNING("nsLinebreakConverter::ConvertUnicharLineBreaks() failed");
    return NS_ERROR_FAILURE;
  }

  // translate platform linebreaks for context
  oldLengthInChars =
      cntxtUcs2Str.Length() + 1;  // +1 to include null terminator
  newLengthInChars = 0;
  *aCfcontext = nsLinebreakConverter::ConvertUnicharLineBreaks(
      cntxtUcs2Str.get(), nsLinebreakConverter::eLinebreakAny,
      nsLinebreakConverter::eLinebreakContent, oldLengthInChars,
      &newLengthInChars);
  // it's ok for context to be empty.  frag might be whole doc and contain all
  // its context.

  // we're done!
  return NS_OK;
}

static nsresult ImgFromData(const nsACString& aType, const nsACString& aData,
                            nsString& aOutput) {
  nsAutoCString data64;
  nsresult rv = Base64Encode(aData, data64);
  if (NS_FAILED(rv)) {
    NS_WARNING("Base64Encode() failed");
    return rv;
  }

  aOutput.AssignLiteral("<IMG src=\"data:");
  AppendUTF8toUTF16(aType, aOutput);
  aOutput.AppendLiteral(";base64,");
  if (!AppendASCIItoUTF16(data64, aOutput, fallible_t())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aOutput.AppendLiteral("\" alt=\"\" >");
  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLEditor::BlobReader)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(HTMLEditor::BlobReader)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBlob)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mHTMLEditor)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSourceDoc)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPointToInsert)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(HTMLEditor::BlobReader)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBlob)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mHTMLEditor)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSourceDoc)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPointToInsert)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(HTMLEditor::BlobReader, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(HTMLEditor::BlobReader, Release)

HTMLEditor::BlobReader::BlobReader(BlobImpl* aBlob, HTMLEditor* aHTMLEditor,
                                   bool aIsSafe, Document* aSourceDoc,
                                   const EditorDOMPoint& aPointToInsert,
                                   bool aDoDeleteSelection)
    : mBlob(aBlob),
      mHTMLEditor(aHTMLEditor),
      // "beforeinput" event should've been dispatched before we read blob,
      // but anyway, we need to clone dataTransfer for "input" event.
      mDataTransfer(mHTMLEditor->GetInputEventDataTransfer()),
      mSourceDoc(aSourceDoc),
      mPointToInsert(aPointToInsert),
      mEditAction(aHTMLEditor->GetEditAction()),
      mIsSafe(aIsSafe),
      mDoDeleteSelection(aDoDeleteSelection),
      mNeedsToDispatchBeforeInputEvent(
          !mHTMLEditor->HasTriedToDispatchBeforeInputEvent()) {
  MOZ_ASSERT(mBlob);
  MOZ_ASSERT(mHTMLEditor);
  MOZ_ASSERT(mHTMLEditor->IsEditActionDataAvailable());
  MOZ_ASSERT(aPointToInsert.IsSet());
  MOZ_ASSERT(mDataTransfer);

  // Take only offset here since it's our traditional behavior.
  AutoEditorDOMPointChildInvalidator storeOnlyWithOffset(mPointToInsert);
}

nsresult HTMLEditor::BlobReader::OnResult(const nsACString& aResult) {
  AutoEditActionDataSetter editActionData(*mHTMLEditor, mEditAction);
  editActionData.InitializeDataTransfer(mDataTransfer);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return EditorBase::ToGenericNSResult(NS_ERROR_FAILURE);
  }

  if (NS_WARN_IF(mNeedsToDispatchBeforeInputEvent)) {
    nsresult rv = editActionData.MaybeDispatchBeforeInputEvent();
    if (NS_FAILED(rv)) {
      NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                           "MaybeDispatchBeforeInputEvent(), failed");
      return EditorBase::ToGenericNSResult(rv);
    }
  } else {
    editActionData.MarkAsBeforeInputHasBeenDispatched();
  }

  nsString blobType;
  mBlob->GetType(blobType);

  // TODO: This does not work well.
  // * If the data is not an image file, this inserts <img> element with odd
  //   data URI (bug 1610220).
  // * If the data is valid image file data, an <img> file is inserted with
  //   data URI, but it's not loaded (bug 1610219).
  NS_ConvertUTF16toUTF8 type(blobType);
  nsAutoString stuffToPaste;
  nsresult rv = ImgFromData(type, aResult, stuffToPaste);
  if (NS_FAILED(rv)) {
    NS_WARNING("ImgFormData() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  AutoPlaceholderBatch treatAsOneTransaction(*mHTMLEditor);
  RefPtr<Document> sourceDocument(mSourceDoc);
  EditorDOMPoint pointToInsert(mPointToInsert);
  rv = MOZ_KnownLive(mHTMLEditor)
           ->DoInsertHTMLWithContext(stuffToPaste, EmptyString(), EmptyString(),
                                     NS_LITERAL_STRING(kFileMime),
                                     sourceDocument, pointToInsert,
                                     mDoDeleteSelection, mIsSafe, false);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::DoInsertHTMLWithContext() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult HTMLEditor::BlobReader::OnError(const nsAString& aError) {
  AutoTArray<nsString, 1> error;
  error.AppendElement(aError);
  nsContentUtils::ReportToConsole(
      nsIScriptError::warningFlag, NS_LITERAL_CSTRING("Editor"),
      mPointToInsert.GetContainer()->OwnerDoc(),
      nsContentUtils::eDOM_PROPERTIES, "EditorFileDropFailed", error);
  return NS_OK;
}

class SlurpBlobEventListener final : public nsIDOMEventListener {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(SlurpBlobEventListener)

  explicit SlurpBlobEventListener(HTMLEditor::BlobReader* aListener)
      : mListener(aListener) {}

  MOZ_CAN_RUN_SCRIPT NS_IMETHOD HandleEvent(Event* aEvent) override;

 private:
  ~SlurpBlobEventListener() = default;

  RefPtr<HTMLEditor::BlobReader> mListener;
};

NS_IMPL_CYCLE_COLLECTION(SlurpBlobEventListener, mListener)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SlurpBlobEventListener)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(SlurpBlobEventListener)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SlurpBlobEventListener)

NS_IMETHODIMP SlurpBlobEventListener::HandleEvent(Event* aEvent) {
  EventTarget* target = aEvent->GetTarget();
  if (!target || !mListener) {
    return NS_OK;
  }

  RefPtr<FileReader> reader = do_QueryObject(target);
  if (!reader) {
    return NS_OK;
  }

  EventMessage message = aEvent->WidgetEventPtr()->mMessage;

  RefPtr<HTMLEditor::BlobReader> listener(mListener);
  if (message == eLoad) {
    MOZ_ASSERT(reader->DataFormat() == FileReader::FILE_AS_BINARY);

    // The original data has been converted from Latin1 to UTF-16, this just
    // undoes that conversion.
    DebugOnly<nsresult> rvIgnored =
        listener->OnResult(NS_LossyConvertUTF16toASCII(reader->Result()));
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "HTMLEditor::BlobReader::OnResult() failed, but ignored");
    return NS_OK;
  }

  if (message == eLoadError) {
    nsAutoString errorMessage;
    reader->GetError()->GetErrorMessage(errorMessage);
    DebugOnly<nsresult> rvIgnored = listener->OnError(errorMessage);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "HTMLEditor::BlobReader::OnError() failed, but ignored");
    return NS_OK;
  }

  return NS_OK;
}

// static
nsresult HTMLEditor::SlurpBlob(Blob* aBlob, nsPIDOMWindowOuter* aWindow,
                               BlobReader* aBlobReader) {
  MOZ_ASSERT(aBlob);
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aBlobReader);

  nsCOMPtr<nsPIDOMWindowInner> inner = aWindow->GetCurrentInnerWindow();
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(inner);
  RefPtr<WeakWorkerRef> workerRef;
  RefPtr<FileReader> reader = new FileReader(global, workerRef);

  RefPtr<SlurpBlobEventListener> eventListener =
      new SlurpBlobEventListener(aBlobReader);

  nsresult rv =
      reader->AddEventListener(NS_LITERAL_STRING("load"), eventListener, false);
  if (NS_FAILED(rv)) {
    NS_WARNING("FileReader::AddEventListener(load) failed");
    return rv;
  }

  rv = reader->AddEventListener(NS_LITERAL_STRING("error"), eventListener,
                                false);
  if (NS_FAILED(rv)) {
    NS_WARNING("FileReader::AddEventListener(error) failed");
    return rv;
  }

  ErrorResult error;
  reader->ReadAsBinaryString(*aBlob, error);
  NS_WARNING_ASSERTION(!error.Failed(),
                       "FileReader::ReadAsBinaryString() failed");
  return error.StealNSResult();
}

nsresult HTMLEditor::InsertObject(const nsACString& aType, nsISupports* aObject,
                                  bool aIsSafe, Document* aSourceDoc,
                                  const EditorDOMPoint& aPointToInsert,
                                  bool aDoDeleteSelection) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (nsCOMPtr<BlobImpl> blob = do_QueryInterface(aObject)) {
    RefPtr<BlobReader> br = new BlobReader(blob, this, aIsSafe, aSourceDoc,
                                           aPointToInsert, aDoDeleteSelection);
    // XXX This is not guaranteed.
    MOZ_ASSERT(aPointToInsert.IsSet());

    RefPtr<Blob> domBlob =
        Blob::Create(aPointToInsert.GetContainer()->GetOwnerGlobal(), blob);
    if (!domBlob) {
      NS_WARNING("Blob::Create() failed");
      return NS_ERROR_FAILURE;
    }

    nsresult rv = SlurpBlob(
        domBlob, aPointToInsert.GetContainer()->OwnerDoc()->GetWindow(), br);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "HTMLEditor::::SlurpBlob() failed");
    return rv;
  }

  nsAutoCString type(aType);

  // Check to see if we can insert an image file
  bool insertAsImage = false;
  nsCOMPtr<nsIFile> fileObj;
  if (type.EqualsLiteral(kFileMime)) {
    fileObj = do_QueryInterface(aObject);
    if (fileObj) {
      // Accept any image type fed to us
      if (nsContentUtils::IsFileImage(fileObj, type)) {
        insertAsImage = true;
      } else {
        // Reset type.
        type.AssignLiteral(kFileMime);
      }
    }
  }

  if (type.EqualsLiteral(kJPEGImageMime) || type.EqualsLiteral(kJPGImageMime) ||
      type.EqualsLiteral(kPNGImageMime) || type.EqualsLiteral(kGIFImageMime) ||
      insertAsImage) {
    nsCString imageData;
    if (insertAsImage) {
      nsresult rv = nsContentUtils::SlurpFileToString(fileObj, imageData);
      if (NS_FAILED(rv)) {
        NS_WARNING("nsContentUtils::SlurpFileToString() failed");
        return rv;
      }
    } else {
      nsCOMPtr<nsIInputStream> imageStream = do_QueryInterface(aObject);
      if (NS_WARN_IF(!imageStream)) {
        return NS_ERROR_FAILURE;
      }

      nsresult rv = NS_ConsumeStream(imageStream, UINT32_MAX, imageData);
      if (NS_FAILED(rv)) {
        NS_WARNING("NS_ConsumeStream() failed");
        return rv;
      }

      rv = imageStream->Close();
      if (NS_FAILED(rv)) {
        NS_WARNING("nsIInputStream::Close() failed");
        return rv;
      }
    }

    nsAutoString stuffToPaste;
    nsresult rv = ImgFromData(type, imageData, stuffToPaste);
    if (NS_FAILED(rv)) {
      NS_WARNING("ImgFromData() failed");
      return rv;
    }

    AutoPlaceholderBatch treatAsOneTransaction(*this);
    rv = DoInsertHTMLWithContext(stuffToPaste, EmptyString(), EmptyString(),
                                 NS_LITERAL_STRING(kFileMime), aSourceDoc,
                                 aPointToInsert, aDoDeleteSelection, aIsSafe,
                                 false);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::DoInsertHTMLWithContext() failed, but ignored");
  }

  return NS_OK;
}

static bool GetString(nsISupports* aData, nsAString& aText) {
  if (nsCOMPtr<nsISupportsString> str = do_QueryInterface(aData)) {
    DebugOnly<nsresult> rvIgnored = str->GetData(aText);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "nsISupportsString::GetData() failed, but ignored");
    return !aText.IsEmpty();
  }

  return false;
}

static bool GetCString(nsISupports* aData, nsACString& aText) {
  if (nsCOMPtr<nsISupportsCString> str = do_QueryInterface(aData)) {
    DebugOnly<nsresult> rvIgnored = str->GetData(aText);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "nsISupportsString::GetData() failed, but ignored");
    return !aText.IsEmpty();
  }

  return false;
}

nsresult HTMLEditor::InsertFromTransferable(nsITransferable* aTransferable,
                                            Document* aSourceDoc,
                                            const nsAString& aContextStr,
                                            const nsAString& aInfoStr,
                                            bool aHavePrivateHTMLFlavor,
                                            bool aDoDeleteSelection) {
  nsAutoCString bestFlavor;
  nsCOMPtr<nsISupports> genericDataObj;
  nsresult rv = aTransferable->GetAnyTransferData(
      bestFlavor, getter_AddRefs(genericDataObj));
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "nsITransferable::GetAnyTransferData() failed, but ignored");
  if (NS_SUCCEEDED(rv)) {
    AutoTransactionsConserveSelection dontChangeMySelection(*this);
    nsAutoString flavor;
    CopyASCIItoUTF16(bestFlavor, flavor);
    bool isSafe = IsSafeToInsertData(aSourceDoc);

    if (bestFlavor.EqualsLiteral(kFileMime) ||
        bestFlavor.EqualsLiteral(kJPEGImageMime) ||
        bestFlavor.EqualsLiteral(kJPGImageMime) ||
        bestFlavor.EqualsLiteral(kPNGImageMime) ||
        bestFlavor.EqualsLiteral(kGIFImageMime)) {
      nsresult rv = InsertObject(bestFlavor, genericDataObj, isSafe, aSourceDoc,
                                 EditorDOMPoint(), aDoDeleteSelection);
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::InsertObject() failed");
        return rv;
      }
    } else if (bestFlavor.EqualsLiteral(kNativeHTMLMime)) {
      // note cf_html uses utf8
      nsAutoCString cfhtml;
      if (GetCString(genericDataObj, cfhtml)) {
        // cfselection left emtpy for now.
        nsString cfcontext, cffragment, cfselection;
        nsresult rv = ParseCFHTML(cfhtml, getter_Copies(cffragment),
                                  getter_Copies(cfcontext));
        if (NS_SUCCEEDED(rv) && !cffragment.IsEmpty()) {
          AutoPlaceholderBatch treatAsOneTransaction(*this);
          // If we have our private HTML flavor, we will only use the fragment
          // from the CF_HTML. The rest comes from the clipboard.
          if (aHavePrivateHTMLFlavor) {
            rv = DoInsertHTMLWithContext(cffragment, aContextStr, aInfoStr,
                                         flavor, aSourceDoc, EditorDOMPoint(),
                                         aDoDeleteSelection, isSafe);
            if (NS_FAILED(rv)) {
              NS_WARNING("HTMLEditor::DoInsertHTMLWithContext() failed");
              return rv;
            }
          } else {
            rv = DoInsertHTMLWithContext(cffragment, cfcontext, cfselection,
                                         flavor, aSourceDoc, EditorDOMPoint(),
                                         aDoDeleteSelection, isSafe);
            if (NS_FAILED(rv)) {
              NS_WARNING("HTMLEditor::DoInsertHTMLWithContext() failed");
              return rv;
            }
          }
        } else {
          // In some platforms (like Linux), the clipboard might return data
          // requested for unknown flavors (for example:
          // application/x-moz-nativehtml).  In this case, treat the data
          // to be pasted as mere HTML to get the best chance of pasting it
          // correctly.
          bestFlavor.AssignLiteral(kHTMLMime);
          // Fall through the next case
        }
      }
    }
    if (bestFlavor.EqualsLiteral(kHTMLMime) ||
        bestFlavor.EqualsLiteral(kUnicodeMime) ||
        bestFlavor.EqualsLiteral(kMozTextInternal)) {
      nsAutoString stuffToPaste;
      if (!GetString(genericDataObj, stuffToPaste)) {
        nsAutoCString text;
        if (GetCString(genericDataObj, text)) {
          CopyUTF8toUTF16(text, stuffToPaste);
        }
      }

      if (!stuffToPaste.IsEmpty()) {
        AutoPlaceholderBatch treatAsOneTransaction(*this);
        if (bestFlavor.EqualsLiteral(kHTMLMime)) {
          nsresult rv = DoInsertHTMLWithContext(
              stuffToPaste, aContextStr, aInfoStr, flavor, aSourceDoc,
              EditorDOMPoint(), aDoDeleteSelection, isSafe);
          if (NS_FAILED(rv)) {
            NS_WARNING("HTMLEditor::DoInsertHTMLWithContext() failed");
            return rv;
          }
        } else {
          nsresult rv = InsertTextAsSubAction(stuffToPaste);
          if (NS_FAILED(rv)) {
            NS_WARNING("EditorBase::InsertTextAsSubAction() failed");
            return rv;
          }
        }
      }
    }
  }

  // Try to scroll the selection into view if the paste succeeded
  DebugOnly<nsresult> rvIgnored = ScrollSelectionFocusIntoView();
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "EditorBase::ScrollSelectionFocusIntoView() failed, but ignored");
  return NS_OK;
}

static void GetStringFromDataTransfer(DataTransfer* aDataTransfer,
                                      const nsAString& aType, int32_t aIndex,
                                      nsString& aOutputString) {
  nsCOMPtr<nsIVariant> variant;
  DebugOnly<nsresult> rvIgnored = aDataTransfer->GetDataAtNoSecurityCheck(
      aType, aIndex, getter_AddRefs(variant));
  if (!variant) {
    MOZ_ASSERT(aOutputString.IsEmpty());
    return;
  }
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "DataTransfer::GetDataAtNoSecurityCheck() failed, but ignored");
  variant->GetAsAString(aOutputString);
  nsContentUtils::PlatformToDOMLineBreaks(aOutputString);
}

nsresult HTMLEditor::InsertFromDataTransfer(DataTransfer* aDataTransfer,
                                            int32_t aIndex,
                                            Document* aSourceDoc,
                                            const EditorDOMPoint& aDroppedAt,
                                            bool aDoDeleteSelection) {
  MOZ_ASSERT(GetEditAction() == EditAction::eDrop);
  MOZ_ASSERT(
      mPlaceholderBatch,
      "TextEditor::InsertFromDataTransfer() should be called only by OnDrop() "
      "and there should've already been placeholder transaction");
  MOZ_ASSERT(aDroppedAt.IsSet());

  ErrorResult error;
  RefPtr<DOMStringList> types =
      aDataTransfer->MozTypesAt(aIndex, CallerType::System, error);
  if (error.Failed()) {
    NS_WARNING("DataTransfer::MozTypesAt() failed");
    return error.StealNSResult();
  }

  bool hasPrivateHTMLFlavor = types->Contains(NS_LITERAL_STRING(kHTMLContext));

  bool isPlaintextEditor = IsPlaintextEditor();
  bool isSafe = IsSafeToInsertData(aSourceDoc);

  uint32_t length = types->Length();
  for (uint32_t i = 0; i < length; i++) {
    nsAutoString type;
    types->Item(i, type);

    if (!isPlaintextEditor) {
      if (type.EqualsLiteral(kFileMime) || type.EqualsLiteral(kJPEGImageMime) ||
          type.EqualsLiteral(kJPGImageMime) ||
          type.EqualsLiteral(kPNGImageMime) ||
          type.EqualsLiteral(kGIFImageMime)) {
        nsCOMPtr<nsIVariant> variant;
        DebugOnly<nsresult> rvIgnored = aDataTransfer->GetDataAtNoSecurityCheck(
            type, aIndex, getter_AddRefs(variant));
        if (variant) {
          NS_WARNING_ASSERTION(
              NS_SUCCEEDED(rvIgnored),
              "DataTransfer::GetDataAtNoSecurityCheck() failed, but ignored");
          nsCOMPtr<nsISupports> object;
          rvIgnored = variant->GetAsISupports(getter_AddRefs(object));
          NS_WARNING_ASSERTION(
              NS_SUCCEEDED(rvIgnored),
              "nsIVariant::GetAsISupports() failed, but ignored");
          nsresult rv =
              InsertObject(NS_ConvertUTF16toUTF8(type), object, isSafe,
                           aSourceDoc, aDroppedAt, aDoDeleteSelection);
          NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                               "HTMLEditor::InsertObject() failed");
          return rv;
        }
      } else if (type.EqualsLiteral(kNativeHTMLMime)) {
        // Windows only clipboard parsing.
        nsAutoString text;
        GetStringFromDataTransfer(aDataTransfer, type, aIndex, text);
        NS_ConvertUTF16toUTF8 cfhtml(text);

        nsString cfcontext, cffragment,
            cfselection;  // cfselection left emtpy for now

        nsresult rv = ParseCFHTML(cfhtml, getter_Copies(cffragment),
                                  getter_Copies(cfcontext));
        if (NS_SUCCEEDED(rv) && !cffragment.IsEmpty()) {
          if (hasPrivateHTMLFlavor) {
            // If we have our private HTML flavor, we will only use the fragment
            // from the CF_HTML. The rest comes from the clipboard.
            nsAutoString contextString, infoString;
            GetStringFromDataTransfer(aDataTransfer,
                                      NS_LITERAL_STRING(kHTMLContext), aIndex,
                                      contextString);
            GetStringFromDataTransfer(aDataTransfer,
                                      NS_LITERAL_STRING(kHTMLInfo), aIndex,
                                      infoString);
            nsresult rv = DoInsertHTMLWithContext(
                cffragment, contextString, infoString, type, aSourceDoc,
                aDroppedAt, aDoDeleteSelection, isSafe);
            NS_WARNING_ASSERTION(
                NS_SUCCEEDED(rv),
                "HTMLEditor::DoInsertHTMLWithContext() failed");
            return rv;
          }
          nsresult rv = DoInsertHTMLWithContext(
              cffragment, cfcontext, cfselection, type, aSourceDoc, aDroppedAt,
              aDoDeleteSelection, isSafe);
          NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                               "HTMLEditor::DoInsertHTMLWithContext() failed");
          return rv;
        }
      } else if (type.EqualsLiteral(kHTMLMime)) {
        nsAutoString text, contextString, infoString;
        GetStringFromDataTransfer(aDataTransfer, type, aIndex, text);
        GetStringFromDataTransfer(aDataTransfer,
                                  NS_LITERAL_STRING(kHTMLContext), aIndex,
                                  contextString);
        GetStringFromDataTransfer(aDataTransfer, NS_LITERAL_STRING(kHTMLInfo),
                                  aIndex, infoString);
        if (type.EqualsLiteral(kHTMLMime)) {
          nsresult rv = DoInsertHTMLWithContext(text, contextString, infoString,
                                                type, aSourceDoc, aDroppedAt,
                                                aDoDeleteSelection, isSafe);
          NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                               "HTMLEditor::DoInsertHTMLWithContext() failed");
          return rv;
        }
      }
    }

    if (type.EqualsLiteral(kTextMime) || type.EqualsLiteral(kMozTextInternal)) {
      nsAutoString text;
      GetStringFromDataTransfer(aDataTransfer, type, aIndex, text);
      nsresult rv = InsertTextAt(text, aDroppedAt, aDoDeleteSelection);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "TextEditor::InsertTextAt() failed");
      return rv;
    }
  }

  return NS_OK;
}

bool HTMLEditor::HavePrivateHTMLFlavor(nsIClipboard* aClipboard) {
  if (NS_WARN_IF(!aClipboard)) {
    return false;
  }

  // check the clipboard for our special kHTMLContext flavor.  If that is there,
  // we know we have our own internal html format on clipboard.
  bool bHavePrivateHTMLFlavor = false;
  AutoTArray<nsCString, 1> flavArray = {nsDependentCString(kHTMLContext)};
  nsresult rv = aClipboard->HasDataMatchingFlavors(
      flavArray, nsIClipboard::kGlobalClipboard, &bHavePrivateHTMLFlavor);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "nsIClipboard::HasDataMatchingFlavors(nsIClipboard::"
                       "kGlobalClipboard) failed");
  return NS_SUCCEEDED(rv) && bHavePrivateHTMLFlavor;
}

nsresult HTMLEditor::PasteInternal(int32_t aClipboardType) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // Get Clipboard Service
  nsresult rv = NS_OK;
  nsCOMPtr<nsIClipboard> clipboard =
      do_GetService("@mozilla.org/widget/clipboard;1", &rv);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to get nsIClipboard service");
    return rv;
  }

  // Get the nsITransferable interface for getting the data from the clipboard
  nsCOMPtr<nsITransferable> transferable;
  rv = PrepareHTMLTransferable(getter_AddRefs(transferable));
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::PrepareHTMLTransferable() failed");
    return rv;
  }
  if (!transferable) {
    NS_WARNING("HTMLEditor::PrepareHTMLTransferable() returned nullptr");
    return NS_ERROR_FAILURE;
  }
  // Get the Data from the clipboard
  rv = clipboard->GetData(transferable, aClipboardType);
  if (NS_FAILED(rv)) {
    NS_WARNING("nsIClipboard::GetData() failed");
    return rv;
  }

  // XXX Why don't you check this first?
  if (!IsModifiable()) {
    return NS_OK;
  }

  // also get additional html copy hints, if present
  nsAutoString contextStr, infoStr;

  // If we have our internal html flavor on the clipboard, there is special
  // context to use instead of cfhtml context.
  bool hasPrivateHTMLFlavor = HavePrivateHTMLFlavor(clipboard);
  if (hasPrivateHTMLFlavor) {
    nsCOMPtr<nsITransferable> contextTransferable =
        do_CreateInstance("@mozilla.org/widget/transferable;1");
    if (!contextTransferable) {
      NS_WARNING(
          "do_CreateInstance() failed to create nsITransferable instance");
      return NS_ERROR_FAILURE;
    }
    DebugOnly<nsresult> rvIgnored = contextTransferable->Init(nullptr);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "nsITransferable::Init() failed, but ignored");
    contextTransferable->SetIsPrivateData(transferable->GetIsPrivateData());
    rvIgnored = contextTransferable->AddDataFlavor(kHTMLContext);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "nsITransferable::AddDataFlavor(kHTMLContext) failed, but ignored");
    rvIgnored = clipboard->GetData(contextTransferable, aClipboardType);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "nsIClipboard::GetData() failed, but ignored");
    nsCOMPtr<nsISupports> contextDataObj;
    rv = contextTransferable->GetTransferData(kHTMLContext,
                                              getter_AddRefs(contextDataObj));
    if (NS_SUCCEEDED(rv) && contextDataObj) {
      if (nsCOMPtr<nsISupportsString> str = do_QueryInterface(contextDataObj)) {
        DebugOnly<nsresult> rvIgnored = str->GetData(contextStr);
        NS_WARNING_ASSERTION(
            NS_SUCCEEDED(rvIgnored),
            "nsISupportsString::GetData() failed, but ignored");
      }
    }

    nsCOMPtr<nsITransferable> infoTransferable =
        do_CreateInstance("@mozilla.org/widget/transferable;1");
    if (!infoTransferable) {
      NS_WARNING(
          "do_CreateInstance() failed to create nsITransferable instance");
      return NS_ERROR_FAILURE;
    }
    rvIgnored = infoTransferable->Init(nullptr);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "nsITransferable::Init() failed, but ignored");
    contextTransferable->SetIsPrivateData(transferable->GetIsPrivateData());
    rvIgnored = infoTransferable->AddDataFlavor(kHTMLInfo);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "nsITransferable::AddDataFlavor(kHTMLInfo) failed, but ignored");
    clipboard->GetData(infoTransferable, aClipboardType);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "nsIClipboard::GetData() failed, but ignored");
    nsCOMPtr<nsISupports> infoDataObj;
    rv = infoTransferable->GetTransferData(kHTMLInfo,
                                           getter_AddRefs(infoDataObj));
    if (NS_SUCCEEDED(rv) && infoDataObj) {
      if (nsCOMPtr<nsISupportsString> str = do_QueryInterface(infoDataObj)) {
        DebugOnly<nsresult> rvIgnored = str->GetData(infoStr);
        NS_WARNING_ASSERTION(
            NS_SUCCEEDED(rvIgnored),
            "nsISupportsString::GetData() failed, but ignored");
      }
    }
  }

  rv = InsertFromTransferable(transferable, nullptr, contextStr, infoStr,
                              hasPrivateHTMLFlavor, true);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::InsertFromTransferable() failed");
  return rv;
}

nsresult HTMLEditor::PasteTransferableAsAction(nsITransferable* aTransferable,
                                               nsIPrincipal* aPrincipal) {
  AutoEditActionDataSetter editActionData(*this, EditAction::ePaste,
                                          aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  editActionData.InitializeDataTransfer(aTransferable);

  // Use an invalid value for the clipboard type as data comes from
  // aTransferable and we don't currently implement a way to put that in the
  // data transfer yet.
  if (!FireClipboardEvent(ePaste, nsIClipboard::kGlobalClipboard)) {
    return EditorBase::ToGenericNSResult(NS_ERROR_EDITOR_ACTION_CANCELED);
  }

  // Dispatch "beforeinput" event after "paste" event.
  nsresult rv = editActionData.MaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "MaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  nsAutoString contextStr, infoStr;
  rv = InsertFromTransferable(aTransferable, nullptr, contextStr, infoStr,
                              false, true);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::InsertFromTransferable() failed");
  return EditorBase::ToGenericNSResult(rv);
}

/**
 * HTML PasteNoFormatting. Ignore any HTML styles and formating in paste source.
 */
NS_IMETHODIMP HTMLEditor::PasteNoFormatting(int32_t aSelectionType) {
  nsresult rv = PasteNoFormattingAsAction(aSelectionType);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::PasteNoFormattingAsAction() failed");
  return rv;
}

nsresult HTMLEditor::PasteNoFormattingAsAction(int32_t aSelectionType,
                                               nsIPrincipal* aPrincipal) {
  AutoEditActionDataSetter editActionData(*this, EditAction::ePaste,
                                          aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  editActionData.InitializeDataTransferWithClipboard(
      SettingDataTransfer::eWithoutFormat, aSelectionType);

  if (!FireClipboardEvent(ePasteNoFormatting, aSelectionType)) {
    return EditorBase::ToGenericNSResult(NS_ERROR_EDITOR_ACTION_CANCELED);
  }

  // Dispatch "beforeinput" event after "paste" event.  And perhaps, before
  // committing composition because if pasting is canceled, we don't need to
  // commit the active composition.
  nsresult rv = editActionData.MaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "MaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  DebugOnly<nsresult> rvIgnored = CommitComposition();
  if (NS_WARN_IF(Destroyed())) {
    return EditorBase::ToGenericNSResult(NS_ERROR_EDITOR_DESTROYED);
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::CommitComposition() failed, but ignored");

  // Get Clipboard Service
  nsCOMPtr<nsIClipboard> clipboard(
      do_GetService("@mozilla.org/widget/clipboard;1", &rv));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to get nsIClipboard service");
    return rv;
  }

  // Get the nsITransferable interface for getting the data from the clipboard.
  // use TextEditor::PrepareTransferable() to force unicode plaintext data.
  nsCOMPtr<nsITransferable> transferable;
  rv = TextEditor::PrepareTransferable(getter_AddRefs(transferable));
  if (NS_FAILED(rv)) {
    NS_WARNING("TextEditor::PrepareTransferable() failed");
    return EditorBase::ToGenericNSResult(rv);
  }
  if (!transferable) {
    NS_WARNING(
        "TextEditor::PrepareTransferable() returned nullptr, but ignored");
    return NS_OK;
  }

  if (!IsModifiable()) {
    return NS_OK;
  }

  // Get the Data from the clipboard
  rv = clipboard->GetData(transferable, aSelectionType);
  if (NS_FAILED(rv)) {
    NS_WARNING("nsIClipboard::GetData() failed");
    return rv;
  }

  const nsString& empty = EmptyString();
  rv = InsertFromTransferable(transferable, nullptr, empty, empty, false, true);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::InsertFromTransferable() failed");
  return EditorBase::ToGenericNSResult(rv);
}

// The following arrays contain the MIME types that we can paste. The arrays
// are used by CanPaste() and CanPasteTransferable() below.

static const char* textEditorFlavors[] = {kUnicodeMime};
static const char* textHtmlEditorFlavors[] = {kUnicodeMime,   kHTMLMime,
                                              kJPEGImageMime, kJPGImageMime,
                                              kPNGImageMime,  kGIFImageMime};

bool HTMLEditor::CanPaste(int32_t aClipboardType) const {
  // Always enable the paste command when inside of a HTML or XHTML document,
  // but if the document is chrome, let it control it.
  Document* document = GetDocument();
  if (document && document->IsHTMLOrXHTML() &&
      !nsContentUtils::IsChromeDoc(document)) {
    return true;
  }

  // can't paste if readonly
  if (!IsModifiable()) {
    return false;
  }

  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard(
      do_GetService("@mozilla.org/widget/clipboard;1", &rv));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to get nsIClipboard service");
    return false;
  }

  // Use the flavors depending on the current editor mask
  if (IsPlaintextEditor()) {
    AutoTArray<nsCString, ArrayLength(textEditorFlavors)> flavors;
    flavors.AppendElements<const char*>(Span<const char*>(textEditorFlavors));
    bool haveFlavors;
    nsresult rv = clipboard->HasDataMatchingFlavors(flavors, aClipboardType,
                                                    &haveFlavors);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "nsIClipboard::HasDataMatchingFlavors() failed");
    return NS_SUCCEEDED(rv) && haveFlavors;
  }

  AutoTArray<nsCString, ArrayLength(textHtmlEditorFlavors)> flavors;
  flavors.AppendElements<const char*>(Span<const char*>(textHtmlEditorFlavors));
  bool haveFlavors;
  rv = clipboard->HasDataMatchingFlavors(flavors, aClipboardType, &haveFlavors);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "nsIClipboard::HasDataMatchingFlavors() failed");
  return NS_SUCCEEDED(rv) && haveFlavors;
}

bool HTMLEditor::CanPasteTransferable(nsITransferable* aTransferable) {
  // can't paste if readonly
  if (!IsModifiable()) {
    return false;
  }

  // If |aTransferable| is null, assume that a paste will succeed.
  if (!aTransferable) {
    return true;
  }

  // Peek in |aTransferable| to see if it contains a supported MIME type.

  // Use the flavors depending on the current editor mask
  const char** flavors;
  size_t length;
  if (IsPlaintextEditor()) {
    flavors = textEditorFlavors;
    length = ArrayLength(textEditorFlavors);
  } else {
    flavors = textHtmlEditorFlavors;
    length = ArrayLength(textHtmlEditorFlavors);
  }

  for (size_t i = 0; i < length; i++, flavors++) {
    nsCOMPtr<nsISupports> data;
    nsresult rv =
        aTransferable->GetTransferData(*flavors, getter_AddRefs(data));
    if (NS_SUCCEEDED(rv) && data) {
      return true;
    }
  }

  return false;
}

nsresult HTMLEditor::PasteAsQuotationAsAction(int32_t aClipboardType,
                                              bool aDispatchPasteEvent,
                                              nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(aClipboardType == nsIClipboard::kGlobalClipboard ||
             aClipboardType == nsIClipboard::kSelectionClipboard);

  if (IsReadonly()) {
    return NS_OK;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::ePasteAsQuotation,
                                          aPrincipal);
  editActionData.InitializeDataTransferWithClipboard(
      SettingDataTransfer::eWithFormat, aClipboardType);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (aDispatchPasteEvent && !FireClipboardEvent(ePaste, aClipboardType)) {
    return EditorBase::ToGenericNSResult(NS_ERROR_EDITOR_ACTION_CANCELED);
  }

  nsresult rv = editActionData.MaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "MaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  if (IsPlaintextEditor()) {
    nsresult rv = PasteAsPlaintextQuotation(aClipboardType);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::PasteAsPlaintextQuotation() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  // If it's not in plain text edit mode, paste text into new
  // <blockquote type="cite"> element after removing selection.

  // XXX Why don't we test these first?
  EditActionResult result = CanHandleHTMLEditSubAction();
  if (result.Failed() || result.Canceled()) {
    NS_WARNING_ASSERTION(result.Succeeded(),
                         "HTMLEditor::CanHandleHTMLEditSubAction() failed");
    return EditorBase::ToGenericNSResult(result.Rv());
  }

  UndefineCaretBidiLevel();

  AutoPlaceholderBatch treatAsOneTransaction(*this);
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eInsertQuotation, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  rv = EnsureNoPaddingBRElementForEmptyEditor();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return EditorBase::ToGenericNSResult(NS_ERROR_EDITOR_DESTROYED);
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::EnsureNoPaddingBRElementForEmptyEditor() "
                       "failed, but ignored");

  if (NS_SUCCEEDED(rv) && SelectionRefPtr()->IsCollapsed()) {
    nsresult rv = EnsureCaretNotAfterPaddingBRElement();
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return EditorBase::ToGenericNSResult(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::EnsureCaretNotAfterPaddingBRElement() "
                         "failed, but ignored");
    if (NS_SUCCEEDED(rv)) {
      nsresult rv = PrepareInlineStylesForCaret();
      if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
        return EditorBase::ToGenericNSResult(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "HTMLEditor::PrepareInlineStylesForCaret() failed, but ignored");
    }
  }

  // Remove Selection and create `<blockquote type="cite">` now.
  // XXX Why don't we insert the `<blockquote>` into the DOM tree after
  //     pasting the content in clipboard into it?
  RefPtr<Element> newBlockquoteElement =
      DeleteSelectionAndCreateElement(*nsGkAtoms::blockquote);
  if (!newBlockquoteElement) {
    NS_WARNING(
        "TextEditor::DeleteSelectionAndCreateElement(nsGkAtoms::blockquote) "
        "failed");
    return NS_ERROR_FAILURE;
  }
  DebugOnly<nsresult> rvIgnored = newBlockquoteElement->SetAttr(
      kNameSpaceID_None, nsGkAtoms::type, NS_LITERAL_STRING("cite"), true);
  if (NS_WARN_IF(Destroyed())) {
    return EditorBase::ToGenericNSResult(NS_ERROR_EDITOR_DESTROYED);
  }
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "Element::SetAttr(nsGkAtoms::type, cite) failed, but ignored");

  // Collapse Selection in the new `<blockquote>` element.
  rv = CollapseSelectionToStartOf(*newBlockquoteElement);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::CollapseSelectionToStartOf() failed");
    return rv;
  }

  rv = PasteInternal(aClipboardType);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "HTMLEditor::PasteInternal() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult HTMLEditor::PasteAsPlaintextQuotation(int32_t aSelectionType) {
  // Get Clipboard Service
  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard =
      do_GetService("@mozilla.org/widget/clipboard;1", &rv);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to get nsIClipboard service");
    return rv;
  }

  // Create generic Transferable for getting the data
  nsCOMPtr<nsITransferable> transferable =
      do_CreateInstance("@mozilla.org/widget/transferable;1", &rv);
  if (NS_FAILED(rv)) {
    NS_WARNING("do_CreateInstance() failed to create nsITransferable instance");
    return rv;
  }
  if (!transferable) {
    NS_WARNING("do_CreateInstance() returned nullptr");
    return NS_ERROR_FAILURE;
  }

  RefPtr<Document> destdoc = GetDocument();
  nsILoadContext* loadContext = destdoc ? destdoc->GetLoadContext() : nullptr;
  DebugOnly<nsresult> rvIgnored = transferable->Init(loadContext);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "nsITransferable::Init() failed, but ignored");

  // We only handle plaintext pastes here
  rvIgnored = transferable->AddDataFlavor(kUnicodeMime);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "nsITransferable::AddDataFlavor(kUnicodeMime) failed, but ignored");

  // Get the Data from the clipboard
  rvIgnored = clipboard->GetData(transferable, aSelectionType);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "nsIClipboard::GetData() failed, but ignored");

  // Now we ask the transferable for the data
  // it still owns the data, we just have a pointer to it.
  // If it can't support a "text" output of the data the call will fail
  nsCOMPtr<nsISupports> genericDataObj;
  nsAutoCString flavor;
  rv = transferable->GetAnyTransferData(flavor, getter_AddRefs(genericDataObj));
  if (NS_FAILED(rv)) {
    NS_WARNING("nsITransferable::GetAnyTransferData() failed");
    return rv;
  }

  if (!flavor.EqualsLiteral(kUnicodeMime)) {
    return NS_OK;
  }

  nsAutoString stuffToPaste;
  if (!GetString(genericDataObj, stuffToPaste)) {
    return NS_OK;
  }

  AutoPlaceholderBatch treatAsOneTransaction(*this);
  rv = InsertAsPlaintextQuotation(stuffToPaste, true, 0);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::InsertAsPlaintextQuotation() failed");
  return rv;
}

nsresult HTMLEditor::InsertWithQuotationsAsSubAction(
    const nsAString& aQuotedText) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (IsReadonly()) {
    return NS_OK;
  }

  EditActionResult result = CanHandleHTMLEditSubAction();
  if (result.Failed() || result.Canceled()) {
    NS_WARNING_ASSERTION(result.Succeeded(),
                         "HTMLEditor::CanHandleHTMLEditSubAction() failed");
    return result.Rv();
  }

  UndefineCaretBidiLevel();

  // Let the citer quote it for us:
  nsString quotedStuff;
  nsresult rv = InternetCiter::GetCiteString(aQuotedText, quotedStuff);
  if (NS_FAILED(rv)) {
    NS_WARNING("InternetCiter::GetCiteString() failed");
    return rv;
  }

  // It's best to put a blank line after the quoted text so that mails
  // written without thinking won't be so ugly.
  if (!aQuotedText.IsEmpty() && (aQuotedText.Last() != char16_t('\n'))) {
    quotedStuff.Append(char16_t('\n'));
  }

  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eInsertText, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  rv = EnsureNoPaddingBRElementForEmptyEditor();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::EnsureNoPaddingBRElementForEmptyEditor() "
                       "failed, but ignored");

  if (NS_SUCCEEDED(rv) && SelectionRefPtr()->IsCollapsed()) {
    nsresult rv = EnsureCaretNotAfterPaddingBRElement();
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::EnsureCaretNotAfterPaddingBRElement() "
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

  rv = InsertTextAsSubAction(quotedStuff);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::InsertTextAsSubAction() failed");
  return rv;
}

nsresult HTMLEditor::InsertTextWithQuotations(
    const nsAString& aStringToInsert) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eInsertText);
  MOZ_ASSERT(!aStringToInsert.IsVoid());
  editActionData.SetData(aStringToInsert);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  // The whole operation should be undoable in one transaction:
  // XXX Why isn't enough to use only AutoPlaceholderBatch here?
  AutoTransactionBatch bundleAllTransactions(*this);
  AutoPlaceholderBatch treatAsOneTransaction(*this);

  rv = InsertTextWithQuotationsInternal(aStringToInsert);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::InsertTextWithQuotationsInternal() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult HTMLEditor::InsertTextWithQuotationsInternal(
    const nsAString& aStringToInsert) {
  // We're going to loop over the string, collecting up a "hunk"
  // that's all the same type (quoted or not),
  // Whenever the quotedness changes (or we reach the string's end)
  // we will insert the hunk all at once, quoted or non.

  static const char16_t cite('>');
  bool curHunkIsQuoted = (aStringToInsert.First() == cite);

  nsAString::const_iterator hunkStart, strEnd;
  aStringToInsert.BeginReading(hunkStart);
  aStringToInsert.EndReading(strEnd);

  // In the loop below, we only look for DOM newlines (\n),
  // because we don't have a FindChars method that can look
  // for both \r and \n.  \r is illegal in the dom anyway,
  // but in debug builds, let's take the time to verify that
  // there aren't any there:
#ifdef DEBUG
  nsAString::const_iterator dbgStart(hunkStart);
  if (FindCharInReadable('\r', dbgStart, strEnd)) {
    NS_ASSERTION(
        false,
        "Return characters in DOM! InsertTextWithQuotations may be wrong");
  }
#endif /* DEBUG */

  // Loop over lines:
  nsresult rv = NS_OK;
  nsAString::const_iterator lineStart(hunkStart);
  // We will break from inside when we run out of newlines.
  for (;;) {
    // Search for the end of this line (dom newlines, see above):
    bool found = FindCharInReadable('\n', lineStart, strEnd);
    bool quoted = false;
    if (found) {
      // if there's another newline, lineStart now points there.
      // Loop over any consecutive newline chars:
      nsAString::const_iterator firstNewline(lineStart);
      while (*lineStart == '\n') {
        ++lineStart;
      }
      quoted = (*lineStart == cite);
      if (quoted == curHunkIsQuoted) {
        continue;
      }
      // else we're changing state, so we need to insert
      // from curHunk to lineStart then loop around.

      // But if the current hunk is quoted, then we want to make sure
      // that any extra newlines on the end do not get included in
      // the quoted section: blank lines flaking a quoted section
      // should be considered unquoted, so that if the user clicks
      // there and starts typing, the new text will be outside of
      // the quoted block.
      if (curHunkIsQuoted) {
        lineStart = firstNewline;

        // 'firstNewline' points to the first '\n'. We want to
        // ensure that this first newline goes into the hunk
        // since quoted hunks can be displayed as blocks
        // (and the newline should become invisible in this case).
        // So the next line needs to start at the next character.
        lineStart++;
      }
    }

    // If no newline found, lineStart is now strEnd and we can finish up,
    // inserting from curHunk to lineStart then returning.
    const nsAString& curHunk = Substring(hunkStart, lineStart);
    nsCOMPtr<nsINode> dummyNode;
    if (curHunkIsQuoted) {
      rv =
          InsertAsPlaintextQuotation(curHunk, false, getter_AddRefs(dummyNode));
      if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "HTMLEditor::InsertAsPlaintextQuotation() failed, "
                           "but might be ignored");
    } else {
      rv = InsertTextAsSubAction(curHunk);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "EditorBase::InsertTextAsSubAction() failed, but might be ignored");
    }
    if (!found) {
      break;
    }
    curHunkIsQuoted = quoted;
    hunkStart = lineStart;
  }

  // XXX This returns the last result of InsertAsPlaintextQuotation() or
  //     InsertTextAsSubAction() in the loop.  This must be a bug.
  return rv;
}

nsresult HTMLEditor::InsertAsQuotation(const nsAString& aQuotedText,
                                       nsINode** aNodeInserted) {
  if (IsPlaintextEditor()) {
    AutoEditActionDataSetter editActionData(*this, EditAction::eInsertText);
    MOZ_ASSERT(!aQuotedText.IsVoid());
    editActionData.SetData(aQuotedText);
    nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
    if (NS_FAILED(rv)) {
      NS_WARNING_ASSERTION(
          rv == NS_ERROR_EDITOR_ACTION_CANCELED,
          "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
      return EditorBase::ToGenericNSResult(rv);
    }
    AutoPlaceholderBatch treatAsOneTransaction(*this);
    rv = InsertAsPlaintextQuotation(aQuotedText, true, aNodeInserted);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::InsertAsPlaintextQuotation() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  AutoEditActionDataSetter editActionData(*this,
                                          EditAction::eInsertBlockquoteElement);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  AutoPlaceholderBatch treatAsOneTransaction(*this);
  nsAutoString citation;
  rv = InsertAsCitedQuotationInternal(aQuotedText, citation, false,
                                      aNodeInserted);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::InsertAsCitedQuotationInternal() failed");
  return EditorBase::ToGenericNSResult(rv);
}

// Insert plaintext as a quotation, with cite marks (e.g. "> ").
// This differs from its corresponding method in TextEditor
// in that here, quoted material is enclosed in a <pre> tag
// in order to preserve the original line wrapping.
nsresult HTMLEditor::InsertAsPlaintextQuotation(const nsAString& aQuotedText,
                                                bool aAddCites,
                                                nsINode** aNodeInserted) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (aNodeInserted) {
    *aNodeInserted = nullptr;
  }

  if (IsReadonly()) {
    return NS_OK;
  }

  EditActionResult result = CanHandleHTMLEditSubAction();
  if (result.Failed() || result.Canceled()) {
    NS_WARNING_ASSERTION(result.Succeeded(),
                         "HTMLEditor::CanHandleHTMLEditSubAction() failed");
    return result.Rv();
  }

  UndefineCaretBidiLevel();

  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eInsertQuotation, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  nsresult rv = EnsureNoPaddingBRElementForEmptyEditor();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::EnsureNoPaddingBRElementForEmptyEditor() "
                       "failed, but ignored");

  if (NS_SUCCEEDED(rv) && SelectionRefPtr()->IsCollapsed()) {
    nsresult rv = EnsureCaretNotAfterPaddingBRElement();
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::EnsureCaretNotAfterPaddingBRElement() "
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

  // Wrap the inserted quote in a <span> so we can distinguish it. If we're
  // inserting into the <body>, we use a <span> which is displayed as a block
  // and sized to the screen using 98 viewport width units.
  // We could use 100vw, but 98vw avoids a horizontal scroll bar where possible.
  // All this is done to wrap overlong lines to the screen and not to the
  // container element, the width-restricted body.
  RefPtr<Element> newSpanElement =
      DeleteSelectionAndCreateElement(*nsGkAtoms::span);
  NS_WARNING_ASSERTION(
      newSpanElement,
      "TextEditor::DeleteSelectionAndCreateElement() failed, but ignored");

  // If this succeeded, then set selection inside the pre
  // so the inserted text will end up there.
  // If it failed, we don't care what the return value was,
  // but we'll fall through and try to insert the text anyway.
  if (newSpanElement) {
    // Add an attribute on the pre node so we'll know it's a quotation.
    DebugOnly<nsresult> rvIgnored =
        newSpanElement->SetAttr(kNameSpaceID_None, nsGkAtoms::mozquote,
                                NS_LITERAL_STRING("true"), true);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "Element::SetAttr(nsGkAtoms::mozquote, true) failed");
    // Allow wrapping on spans so long lines get wrapped to the screen.
    nsCOMPtr<nsINode> parentNode = newSpanElement->GetParentNode();
    if (parentNode && parentNode->IsHTMLElement(nsGkAtoms::body)) {
      DebugOnly<nsresult> rvIgnored = newSpanElement->SetAttr(
          kNameSpaceID_None, nsGkAtoms::style,
          NS_LITERAL_STRING(
              "white-space: pre-wrap; display: block; width: 98vw;"),
          true);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "Element::SetAttr(nsGkAtoms::style) failed, but ignored");
    } else {
      DebugOnly<nsresult> rvIgnored = newSpanElement->SetAttr(
          kNameSpaceID_None, nsGkAtoms::style,
          NS_LITERAL_STRING("white-space: pre-wrap;"), true);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "Element::SetAttr(nsGkAtoms::style) failed, but ignored");
    }

    // and set the selection inside it:
    rv = CollapseSelectionToStartOf(*newSpanElement);
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::CollapseSelectionToStartOf() failed, but ignored");
  }

  if (aAddCites) {
    rv = InsertWithQuotationsAsSubAction(aQuotedText);
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::InsertWithQuotationsAsSubAction() failed");
      return rv;
    }
  } else {
    rv = InsertTextAsSubAction(aQuotedText);
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::InsertTextAsSubAction() failed");
      return rv;
    }
  }

  // XXX Why don't we check this before inserting the quoted text?
  if (!newSpanElement) {
    return NS_OK;
  }

  // Set the selection to just after the inserted node:
  EditorRawDOMPoint afterNewSpanElement(
      EditorRawDOMPoint::After(*newSpanElement));
  NS_WARNING_ASSERTION(
      afterNewSpanElement.IsSet(),
      "Failed to set after the new <span> element, but ignored");
  if (afterNewSpanElement.IsSet()) {
    nsresult rv = CollapseSelectionTo(afterNewSpanElement);
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::CollapseSelectionTo() failed, but ignored");
  }

  // Note that if !aAddCites, aNodeInserted isn't set.
  // That's okay because the routines that use aAddCites
  // don't need to know the inserted node.
  if (aNodeInserted) {
    newSpanElement.forget(aNodeInserted);
  }

  return NS_OK;
}

NS_IMETHODIMP HTMLEditor::Rewrap(bool aRespectNewlines) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eRewrap);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  // Rewrap makes no sense if there's no wrap column; default to 72.
  int32_t wrapWidth = WrapWidth();
  if (wrapWidth <= 0) {
    wrapWidth = 72;
  }

  nsAutoString current;
  bool isCollapsed;
  rv = SharedOutputString(nsIDocumentEncoder::OutputFormatted |
                              nsIDocumentEncoder::OutputLFLineBreak,
                          &isCollapsed, current);
  if (NS_FAILED(rv)) {
    NS_WARNING("TextEditor::SharedOutputString() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  nsString wrapped;
  uint32_t firstLineOffset = 0;  // XXX need to reset this if there is a
                                 //     selection
  rv = InternetCiter::Rewrap(current, wrapWidth, firstLineOffset,
                             aRespectNewlines, wrapped);
  if (NS_FAILED(rv)) {
    NS_WARNING("InternetCiter::Rewrap() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  if (isCollapsed) {
    DebugOnly<nsresult> rvIgnored = SelectAllInternal();
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "HTMLEditor::SelectAllInternal() failed");
  }

  // The whole operation in InsertTextWithQuotationsInternal() should be
  // undoable in one transaction.
  // XXX Why isn't enough to use only AutoPlaceholderBatch here?
  AutoTransactionBatch bundleAllTransactions(*this);
  AutoPlaceholderBatch treatAsOneTransaction(*this);
  rv = InsertTextWithQuotationsInternal(wrapped);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::InsertTextWithQuotationsInternal() failed");
  return EditorBase::ToGenericNSResult(rv);
}

NS_IMETHODIMP HTMLEditor::InsertAsCitedQuotation(const nsAString& aQuotedText,
                                                 const nsAString& aCitation,
                                                 bool aInsertHTML,
                                                 nsINode** aNodeInserted) {
  // Don't let anyone insert HTML when we're in plaintext mode.
  if (IsPlaintextEditor()) {
    NS_ASSERTION(
        !aInsertHTML,
        "InsertAsCitedQuotation: trying to insert html into plaintext editor");

    AutoEditActionDataSetter editActionData(*this, EditAction::eInsertText);
    MOZ_ASSERT(!aQuotedText.IsVoid());
    editActionData.SetData(aQuotedText);
    nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
    if (NS_FAILED(rv)) {
      NS_WARNING_ASSERTION(
          rv == NS_ERROR_EDITOR_ACTION_CANCELED,
          "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
      return EditorBase::ToGenericNSResult(rv);
    }

    AutoPlaceholderBatch treatAsOneTransaction(*this);
    rv = InsertAsPlaintextQuotation(aQuotedText, true, aNodeInserted);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::InsertAsPlaintextQuotation() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  AutoEditActionDataSetter editActionData(*this,
                                          EditAction::eInsertBlockquoteElement);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  AutoPlaceholderBatch treatAsOneTransaction(*this);
  rv = InsertAsCitedQuotationInternal(aQuotedText, aCitation, aInsertHTML,
                                      aNodeInserted);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::InsertAsCitedQuotationInternal() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult HTMLEditor::InsertAsCitedQuotationInternal(
    const nsAString& aQuotedText, const nsAString& aCitation, bool aInsertHTML,
    nsINode** aNodeInserted) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!IsPlaintextEditor());

  if (IsReadonly()) {
    return NS_OK;
  }

  EditActionResult result = CanHandleHTMLEditSubAction();
  if (result.Failed() || result.Canceled()) {
    NS_WARNING_ASSERTION(result.Succeeded(),
                         "HTMLEditor::CanHandleHTMLEditSubAction() failed");
    return result.Rv();
  }

  UndefineCaretBidiLevel();

  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eInsertQuotation, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  nsresult rv = EnsureNoPaddingBRElementForEmptyEditor();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::EnsureNoPaddingBRElementForEmptyEditor() "
                       "failed, but ignored");

  if (NS_SUCCEEDED(rv) && SelectionRefPtr()->IsCollapsed()) {
    nsresult rv = EnsureCaretNotAfterPaddingBRElement();
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::EnsureCaretNotAfterPaddingBRElement() "
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

  RefPtr<Element> newBlockquoteElement =
      DeleteSelectionAndCreateElement(*nsGkAtoms::blockquote);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (!newBlockquoteElement) {
    NS_WARNING(
        "TextEditor::DeleteSelectionAndCreateElement(nsGkAtoms::blockquote) "
        "failed");
    return NS_ERROR_FAILURE;
  }

  // Try to set type=cite.  Ignore it if this fails.
  DebugOnly<nsresult> rvIgnored = newBlockquoteElement->SetAttr(
      kNameSpaceID_None, nsGkAtoms::type, NS_LITERAL_STRING("cite"), true);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "Element::SetAttr(nsGkAtoms::type, cite) failed, but ignored");

  if (!aCitation.IsEmpty()) {
    DebugOnly<nsresult> rvIgnored = newBlockquoteElement->SetAttr(
        kNameSpaceID_None, nsGkAtoms::cite, aCitation, true);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "Element::SetAttr(nsGkAtoms::cite) failed, but ignored");
  }

  // Set the selection inside the blockquote so aQuotedText will go there:
  rv = CollapseSelectionTo(EditorRawDOMPoint(newBlockquoteElement, 0));
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::CollapseSelectionTo() failed, but ignored");

  if (aInsertHTML) {
    rv = LoadHTML(aQuotedText);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::LoadHTML() failed");
      return rv;
    }
  } else {
    rv = InsertTextAsSubAction(aQuotedText);  // XXX ignore charset
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::LoadHTML() failed");
      return rv;
    }
  }

  // XXX Why don't we check this before inserting aQuotedText?
  if (!newBlockquoteElement) {
    return NS_OK;
  }

  // Set the selection to just after the inserted node:
  EditorRawDOMPoint afterNewBlockquoteElement(
      EditorRawDOMPoint::After(newBlockquoteElement));
  NS_WARNING_ASSERTION(
      afterNewBlockquoteElement.IsSet(),
      "Failed to set after new <blockquote> element, but ignored");
  if (afterNewBlockquoteElement.IsSet()) {
    nsresult rv = CollapseSelectionTo(afterNewBlockquoteElement);
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::CollapseSelectionTo() failed, but ignored");
  }

  if (aNodeInserted) {
    newBlockquoteElement.forget(aNodeInserted);
  }

  return NS_OK;
}

void RemoveBodyAndHead(nsINode& aNode) {
  nsCOMPtr<nsIContent> body, head;
  // find the body and head nodes if any.
  // look only at immediate children of aNode.
  for (nsCOMPtr<nsIContent> child = aNode.GetFirstChild(); child;
       child = child->GetNextSibling()) {
    if (child->IsHTMLElement(nsGkAtoms::body)) {
      body = child;
    } else if (child->IsHTMLElement(nsGkAtoms::head)) {
      head = child;
    }
  }
  if (head) {
    ErrorResult ignored;
    aNode.RemoveChild(*head, ignored);
  }
  if (body) {
    nsCOMPtr<nsIContent> child = body->GetFirstChild();
    while (child) {
      ErrorResult ignored;
      aNode.InsertBefore(*child, body, ignored);
      child = body->GetFirstChild();
    }

    ErrorResult ignored;
    aNode.RemoveChild(*body, ignored);
  }
}

/**
 * This function finds the target node that we will be pasting into. aStart is
 * the context that we're given and aResult will be the target. Initially,
 * *aResult must be nullptr.
 *
 * The target for a paste is found by either finding the node that contains
 * the magical comment node containing kInsertCookie or, failing that, the
 * firstChild of the firstChild (until we reach a leaf).
 */
bool FindTargetNode(nsINode& aStart, nsCOMPtr<nsINode>& aResult) {
  nsIContent* firstChild = aStart.GetFirstChild();
  if (!firstChild) {
    // If the current result is nullptr, then aStart is a leaf, and is the
    // fallback result.
    if (!aResult) {
      aResult = &aStart;
    }
    return false;
  }

  for (nsCOMPtr<nsIContent> child = firstChild; child;
       child = child->GetNextSibling()) {
    // Is this child the magical cookie?
    if (auto* comment = Comment::FromNode(child)) {
      nsAutoString data;
      comment->GetData(data);

      if (data.EqualsLiteral(kInsertCookie)) {
        // Yes it is! Return an error so we bubble out and short-circuit the
        // search.
        aResult = &aStart;

        child->Remove();

        return true;
      }
    }

    if (FindTargetNode(*child, aResult)) {
      return true;
    }
  }

  return false;
}

nsresult HTMLEditor::CreateDOMFragmentFromPaste(
    const nsAString& aInputString, const nsAString& aContextStr,
    const nsAString& aInfoStr, nsCOMPtr<nsINode>* aOutFragNode,
    nsCOMPtr<nsINode>* aOutStartNode, nsCOMPtr<nsINode>* aOutEndNode,
    int32_t* aOutStartOffset, int32_t* aOutEndOffset, bool aTrustedInput) {
  if (NS_WARN_IF(!aOutFragNode) || NS_WARN_IF(!aOutStartNode) ||
      NS_WARN_IF(!aOutEndNode) || NS_WARN_IF(!aOutStartOffset) ||
      NS_WARN_IF(!aOutEndOffset)) {
    return NS_ERROR_INVALID_ARG;
  }

  RefPtr<Document> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return NS_ERROR_FAILURE;
  }

  // if we have context info, create a fragment for that
  nsCOMPtr<nsINode> targetNode;
  RefPtr<DocumentFragment> documentFragmentForContext;
  if (!aContextStr.IsEmpty()) {
    nsresult rv = ParseFragment(aContextStr, nullptr, document,
                                getter_AddRefs(documentFragmentForContext),
                                aTrustedInput);
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::ParseFragment(aContextStr) failed");
      return rv;
    }
    if (!documentFragmentForContext) {
      NS_WARNING("HTMLEditor::ParseFragment(aContextStr) returned nullptr");
      return NS_ERROR_FAILURE;
    }

    rv = StripFormattingNodes(*documentFragmentForContext);
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::StripFormattingNodes() failed");
      return rv;
    }

    RemoveBodyAndHead(*documentFragmentForContext);

    FindTargetNode(*documentFragmentForContext, targetNode);
  }

  nsCOMPtr<nsIContent> targetContent = nsIContent::FromNodeOrNull(targetNode);
  MOZ_ASSERT_IF(targetNode, targetContent);

  // create fragment for pasted html
  nsAtom* contextLocalNameAtom;
  if (targetContent) {
    contextLocalNameAtom = targetContent->NodeInfo()->NameAtom();
    if (targetContent->IsHTMLElement(nsGkAtoms::html)) {
      contextLocalNameAtom = nsGkAtoms::body;
    }
  } else {
    contextLocalNameAtom = nsGkAtoms::body;
  }
  RefPtr<DocumentFragment> documentFragmentToInsert;
  nsresult rv =
      ParseFragment(aInputString, contextLocalNameAtom, document,
                    getter_AddRefs(documentFragmentToInsert), aTrustedInput);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::ParseFragment(aInputString) failed");
    return rv;
  }
  if (!documentFragmentToInsert) {
    NS_WARNING("HTMLEditor::ParseFragment(aInputString) returned nullptr");
    return NS_ERROR_FAILURE;
  }

  RemoveBodyAndHead(*documentFragmentToInsert);

  if (targetNode) {
    // unite the two trees
    IgnoredErrorResult ignoredError;
    targetContent->AppendChild(*documentFragmentToInsert, ignoredError);
    NS_WARNING_ASSERTION(!ignoredError.Failed(),
                         "nsINode::AppendChild() failed, but ignored");
    documentFragmentToInsert = documentFragmentForContext;
  }

  rv = StripFormattingNodes(*documentFragmentToInsert, true);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::StripFormattingNodes() failed");
    return rv;
  }

  // If there was no context, then treat all of the data we did get as the
  // pasted data.
  if (targetNode) {
    *aOutEndNode = *aOutStartNode = targetNode;
  } else {
    *aOutEndNode = *aOutStartNode = documentFragmentToInsert;
  }

  *aOutFragNode = std::move(documentFragmentToInsert);
  *aOutStartOffset = 0;

  // get the infoString contents
  if (!aInfoStr.IsEmpty()) {
    int32_t sep = aInfoStr.FindChar((char16_t)',');
    nsAutoString numstr1(Substring(aInfoStr, 0, sep));
    nsAutoString numstr2(
        Substring(aInfoStr, sep + 1, aInfoStr.Length() - (sep + 1)));

    // Move the start and end children.
    nsresult rvIgnored;
    int32_t num = numstr1.ToInteger(&rvIgnored);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "nsAString::ToInteger() failed, but ignored");
    while (num--) {
      nsINode* tmp = (*aOutStartNode)->GetFirstChild();
      if (!tmp) {
        NS_WARNING("aOutStartNode did not have children");
        return NS_ERROR_FAILURE;
      }
      *aOutStartNode = tmp;
    }

    num = numstr2.ToInteger(&rvIgnored);
    while (num--) {
      nsINode* tmp = (*aOutEndNode)->GetLastChild();
      if (!tmp) {
        NS_WARNING("aOutEndNode did not have children");
        return NS_ERROR_FAILURE;
      }
      *aOutEndNode = tmp;
    }
  }

  *aOutEndOffset = (*aOutEndNode)->Length();
  return NS_OK;
}

nsresult HTMLEditor::ParseFragment(const nsAString& aFragStr,
                                   nsAtom* aContextLocalName,
                                   Document* aTargetDocument,
                                   DocumentFragment** aFragment,
                                   bool aTrustedInput) {
  nsAutoScriptBlockerSuppressNodeRemoved autoBlocker;

  RefPtr<DocumentFragment> fragment = new (aTargetDocument->NodeInfoManager())
      DocumentFragment(aTargetDocument->NodeInfoManager());
  nsresult rv = nsContentUtils::ParseFragmentHTML(
      aFragStr, fragment,
      aContextLocalName ? aContextLocalName : nsGkAtoms::body,
      kNameSpaceID_XHTML, false, true);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "nsContentUtils::ParseFragmentHTML() failed");
  if (!aTrustedInput) {
    nsTreeSanitizer sanitizer(aContextLocalName
                                  ? nsIParserUtils::SanitizerAllowStyle
                                  : nsIParserUtils::SanitizerAllowComments);
    sanitizer.Sanitize(fragment);
  }
  fragment.forget(aFragment);
  return rv;
}

// static
void HTMLEditor::CollectTopMostChildNodesCompletelyInRange(
    const EditorRawDOMPoint& aStartPoint, const EditorRawDOMPoint& aEndPoint,
    nsTArray<OwningNonNull<nsIContent>>& aOutArrayOfContents) {
  MOZ_ASSERT(aStartPoint.IsSetAndValid());
  MOZ_ASSERT(aEndPoint.IsSetAndValid());

  RefPtr<nsRange> range =
      nsRange::Create(aStartPoint.ToRawRangeBoundary(),
                      aEndPoint.ToRawRangeBoundary(), IgnoreErrors());
  if (!range) {
    NS_WARNING("nsRange::Create() failed");
    return;
  }
  DOMSubtreeIterator iter;
  if (NS_FAILED(iter.Init(*range))) {
    NS_WARNING("DOMSubtreeIterator::Init() failed, but ignored");
    return;
  }

  iter.AppendAllNodesToArray(aOutArrayOfContents);
}

/******************************************************************************
 * HTMLEditor::AutoHTMLFragmentBoundariesFixer
 ******************************************************************************/

HTMLEditor::AutoHTMLFragmentBoundariesFixer::AutoHTMLFragmentBoundariesFixer(
    nsTArray<OwningNonNull<nsIContent>>& aArrayOfTopMostChildContents) {
  EnsureBeginsOrEndsWithValidContent(StartOrEnd::start,
                                     aArrayOfTopMostChildContents);
  EnsureBeginsOrEndsWithValidContent(StartOrEnd::end,
                                     aArrayOfTopMostChildContents);
}

void HTMLEditor::AutoHTMLFragmentBoundariesFixer::
    CollectListAndTableRelatedElementsAt(
        nsIContent& aContent,
        nsTArray<OwningNonNull<Element>>& aOutArrayOfListAndTableElements)
        const {
  for (Element* element = aContent.GetAsElementOrParentElement(); element;
       element = element->GetParentElement()) {
    if (HTMLEditUtils::IsList(element) || HTMLEditUtils::IsTable(element)) {
      aOutArrayOfListAndTableElements.AppendElement(*element);
    }
  }
}

Element*
HTMLEditor::AutoHTMLFragmentBoundariesFixer::GetMostAncestorListOrTableElement(
    const nsTArray<OwningNonNull<nsIContent>>& aArrayOfTopMostChildContents,
    const nsTArray<OwningNonNull<Element>>& aArrayOfListAndTableRelatedElements)
    const {
  Element* lastFoundAncestorListOrTableElement = nullptr;
  for (auto& content : aArrayOfTopMostChildContents) {
    if (HTMLEditUtils::IsTableElement(content) &&
        !content->IsHTMLElement(nsGkAtoms::table)) {
      Element* tableElement = nullptr;
      for (Element* maybeTableElement = content->GetParentElement();
           maybeTableElement;
           maybeTableElement = maybeTableElement->GetParentElement()) {
        if (maybeTableElement->IsHTMLElement(nsGkAtoms::table)) {
          tableElement = maybeTableElement;
          break;
        }
      }
      if (!tableElement) {
        continue;
      }
      // If we find a `<table>` element which is an ancestor of a table
      // related element and is not an acestor of first nor last of
      // aArrayOfNodes, return the last found list or `<table>` element.
      // XXX Is that really expected that this returns a list element in this
      //     case?
      if (!aArrayOfListAndTableRelatedElements.Contains(tableElement)) {
        return lastFoundAncestorListOrTableElement;
      }
      // If we find a `<table>` element which is topmost list or `<table>`
      // element at first or last of aArrayOfNodes, return it.
      if (aArrayOfListAndTableRelatedElements.LastElement().get() ==
          tableElement) {
        return tableElement;
      }
      // Otherwise, store the `<table>` element which is an ancestor but
      // not topmost ancestor of first or last of aArrayOfNodes.
      lastFoundAncestorListOrTableElement = tableElement;
      continue;
    }

    if (!HTMLEditUtils::IsListItem(content)) {
      continue;
    }
    Element* listElement = nullptr;
    for (Element* maybeListElement = content->GetParentElement();
         maybeListElement;
         maybeListElement = maybeListElement->GetParentElement()) {
      if (HTMLEditUtils::IsList(maybeListElement)) {
        listElement = maybeListElement;
        break;
      }
    }
    if (!listElement) {
      continue;
    }
    // If we find a list element which is ancestor of a list item element and
    // is not an acestor of first nor last of aArrayOfNodes, return the last
    // found list or `<table>` element.
    // XXX Is that really expected that this returns a `<table>` element in
    //     this case?
    if (!aArrayOfListAndTableRelatedElements.Contains(listElement)) {
      return lastFoundAncestorListOrTableElement;
    }
    // If we find a list element which is topmost list or `<table>` element at
    // first or last of aArrayOfNodes, return it.
    if (aArrayOfListAndTableRelatedElements.LastElement().get() ==
        listElement) {
      return listElement;
    }
    // Otherwise, store the list element which is an ancestor but not topmost
    // ancestor of first or last of aArrayOfNodes.
    lastFoundAncestorListOrTableElement = listElement;
  }

  // If we find only non-topmost list or `<table>` element, returns the last
  // found one (meaning bottommost one).  Otherwise, nullptr.
  return lastFoundAncestorListOrTableElement;
}

Element*
HTMLEditor::AutoHTMLFragmentBoundariesFixer::FindReplaceableTableElement(
    Element& aTableElement, nsIContent& aContentMaybeInTableElement) const {
  MOZ_ASSERT(aTableElement.IsHTMLElement(nsGkAtoms::table));
  // Perhaps, this is designed for climbing up the DOM tree from
  // aContentMaybeInTableElement to aTableElement and making sure that
  // aContentMaybeInTableElement itself or its ancestor is a `<td>`, `<th>`,
  // `<tr>`, `<thead>`, `<tbody>`, `<tfoot>` or `<caption>`.
  // But this looks really buggy because this loop may skip aTableElement
  // as the following NS_ASSERTION.  We should write automated tests and
  // check right behavior.
  for (Element* element =
           aContentMaybeInTableElement.GetAsElementOrParentElement();
       element; element = element->GetParentElement()) {
    if (!HTMLEditUtils::IsTableElement(element) ||
        element->IsHTMLElement(nsGkAtoms::table)) {
      // XXX Perhaps, the original developer of this method assumed that
      //     aTableElement won't be skipped because if it's assumed, we can
      //     stop climbing up the tree in that case.
      NS_ASSERTION(element != &aTableElement,
                   "The table element which is looking for is ignored");
      continue;
    }
    Element* tableElement = nullptr;
    for (Element* maybeTableElement = element->GetParentElement();
         maybeTableElement;
         maybeTableElement = maybeTableElement->GetParentElement()) {
      if (maybeTableElement->IsHTMLElement(nsGkAtoms::table)) {
        tableElement = maybeTableElement;
        break;
      }
    }
    if (tableElement == &aTableElement) {
      return element;
    }
    // XXX If we find another `<table>` element, why don't we keep searching
    //     from its parent?
  }
  return nullptr;
}

bool HTMLEditor::AutoHTMLFragmentBoundariesFixer::IsReplaceableListElement(
    Element& aListElement, nsIContent& aContentMaybeInListElement) const {
  MOZ_ASSERT(HTMLEditUtils::IsList(&aListElement));
  // Perhaps, this is designed for climbing up the DOM tree from
  // aContentMaybeInListElement to aListElement and making sure that
  // aContentMaybeInListElement itself or its ancestor is an list item.
  // But this looks really buggy because this loop may skip aListElement
  // as the following NS_ASSERTION.  We should write automated tests and
  // check right behavior.
  for (Element* element =
           aContentMaybeInListElement.GetAsElementOrParentElement();
       element; element = element->GetParentElement()) {
    if (!HTMLEditUtils::IsListItem(element)) {
      // XXX Perhaps, the original developer of this method assumed that
      //     aListElement won't be skipped because if it's assumed, we can
      //     stop climbing up the tree in that case.
      NS_ASSERTION(element != &aListElement,
                   "The list element which is looking for is ignored");
      continue;
    }
    Element* listElement = nullptr;
    for (Element* maybeListElement = element->GetParentElement();
         maybeListElement;
         maybeListElement = maybeListElement->GetParentElement()) {
      if (HTMLEditUtils::IsList(maybeListElement)) {
        listElement = maybeListElement;
        break;
      }
    }
    if (listElement == &aListElement) {
      return true;
    }
    // XXX If we find another list element, why don't we keep searching
    //     from its parent?
  }
  return false;
}

void HTMLEditor::AutoHTMLFragmentBoundariesFixer::
    EnsureBeginsOrEndsWithValidContent(StartOrEnd aStartOrEnd,
                                       nsTArray<OwningNonNull<nsIContent>>&
                                           aArrayOfTopMostChildContents) const {
  MOZ_ASSERT(!aArrayOfTopMostChildContents.IsEmpty());

  // Collect list elements and table related elements at first or last node
  // in aArrayOfTopMostChildContents.
  AutoTArray<OwningNonNull<Element>, 4>
      arrayOfListAndTableRelatedElementsAtEdge;
  CollectListAndTableRelatedElementsAt(
      aStartOrEnd == StartOrEnd::end
          ? aArrayOfTopMostChildContents.LastElement()
          : aArrayOfTopMostChildContents[0],
      arrayOfListAndTableRelatedElementsAtEdge);
  if (arrayOfListAndTableRelatedElementsAtEdge.IsEmpty()) {
    return;
  }

  // Get most ancestor list or `<table>` element in
  // arrayOfListAndTableRelatedElementsAtEdge which contains earlier
  // node in aArrayOfTopMostChildContents as far as possible.
  // XXX With arrayOfListAndTableRelatedElementsAtEdge, this returns a
  //     list or `<table>` element which contains first or last node of
  //     aArrayOfTopMostChildContents.  However, this seems slow when
  //     aStartOrEnd is StartOrEnd::end and only the last node is in
  //     different list or `<table>`.  But I'm not sure whether it's
  //     possible case or not.  We need to add tests to
  //     test_content_iterator_subtree.html for checking how
  //     SubtreeContentIterator works.
  Element* listOrTableElement = GetMostAncestorListOrTableElement(
      aArrayOfTopMostChildContents, arrayOfListAndTableRelatedElementsAtEdge);
  if (!listOrTableElement) {
    return;
  }

  // If we have pieces of tables or lists to be inserted, let's force the
  // insertion to deal with table elements right away, so that it doesn't
  // orphan some table or list contents outside the table or list.

  OwningNonNull<nsIContent>& firstOrLastChildContent =
      aStartOrEnd == StartOrEnd::end
          ? aArrayOfTopMostChildContents.LastElement()
          : aArrayOfTopMostChildContents[0];

  // Find substructure of list or table that must be included in paste.
  Element* replaceElement;
  if (HTMLEditUtils::IsList(listOrTableElement)) {
    if (!IsReplaceableListElement(*listOrTableElement,
                                  firstOrLastChildContent)) {
      return;
    }
    replaceElement = listOrTableElement;
  } else {
    MOZ_ASSERT(listOrTableElement->IsHTMLElement(nsGkAtoms::table));
    replaceElement = FindReplaceableTableElement(*listOrTableElement,
                                                 firstOrLastChildContent);
    if (!replaceElement) {
      return;
    }
  }

  // If we can replace the given list element or found a table related element
  // in the `<table>` element, insert it into aArrayOfTopMostChildContents which
  // is tompost children to be inserted instead of descendants of them in
  // aArrayOfTopMostChildContents.
  for (size_t i = 0; i < aArrayOfTopMostChildContents.Length();) {
    OwningNonNull<nsIContent>& content = aArrayOfTopMostChildContents[i];
    if (content == replaceElement) {
      // If the element is n aArrayOfTopMostChildContents, its descendants must
      // not be in the array.  Therefore, we don't need to optimize this case.
      // XXX Perhaps, we can break this loop right now.
      aArrayOfTopMostChildContents.RemoveElementAt(i);
      continue;
    }
    if (!EditorUtils::IsDescendantOf(content, *replaceElement)) {
      i++;
      continue;
    }
    // For saving number of calls of EditorUtils::IsDescendantOf(), we should
    // remove its siblings in the array.
    nsIContent* parent = content->GetParent();
    aArrayOfTopMostChildContents.RemoveElementAt(i);
    while (i < aArrayOfTopMostChildContents.Length() &&
           aArrayOfTopMostChildContents[i]->GetParent() == parent) {
      aArrayOfTopMostChildContents.RemoveElementAt(i);
    }
  }

  // Now replace the removed nodes with the structural parent
  if (aStartOrEnd == StartOrEnd::end) {
    aArrayOfTopMostChildContents.AppendElement(*replaceElement);
  } else {
    aArrayOfTopMostChildContents.InsertElementAt(0, *replaceElement);
  }
}

}  // namespace mozilla
