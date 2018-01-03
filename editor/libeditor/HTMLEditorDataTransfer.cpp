/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/HTMLEditor.h"

#include <string.h>

#include "HTMLEditUtils.h"
#include "TextEditUtils.h"
#include "WSRunObject.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/DOMStringList.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Base64.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/EditorDOMPoint.h"
#include "mozilla/EditorUtils.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/Preferences.h"
#include "mozilla/SelectionState.h"
#include "mozilla/TextEditRules.h"
#include "nsAString.h"
#include "nsCOMPtr.h"
#include "nsCRTGlue.h" // for CRLF
#include "nsComponentManagerUtils.h"
#include "nsIScriptError.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsDependentSubstring.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsIClipboard.h"
#include "nsIContent.h"
#include "nsIDOMComment.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIDOMElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMNode.h"
#include "nsIDocument.h"
#include "nsIFile.h"
#include "nsIInputStream.h"
#include "nsIMIMEService.h"
#include "nsNameSpaceManager.h"
#include "nsINode.h"
#include "nsIParserUtils.h"
#include "nsISupportsImpl.h"
#include "nsISupportsPrimitives.h"
#include "nsISupportsUtils.h"
#include "nsITransferable.h"
#include "nsIURI.h"
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
#include "nsSubstringTuple.h"
#include "nsTreeSanitizer.h"
#include "nsXPCOM.h"
#include "nscore.h"
#include "nsContentUtils.h"

class nsAtom;
class nsILoadContext;
class nsISupports;

namespace mozilla {

using namespace dom;

#define kInsertCookie  "_moz_Insert Here_moz_"

// some little helpers
static bool FindIntegerAfterString(const char* aLeadingString,
                                   nsCString& aCStr, int32_t& foundNumber);
static nsresult RemoveFragComments(nsCString& theStr);
static void RemoveBodyAndHead(nsINode& aNode);
static nsresult FindTargetNode(nsIDOMNode* aStart,
                               nsCOMPtr<nsIDOMNode>& aResult);

nsresult
HTMLEditor::LoadHTML(const nsAString& aInputString)
{
  NS_ENSURE_TRUE(mRules, NS_ERROR_NOT_INITIALIZED);

  // force IME commit; set up rules sniffing and batching
  CommitComposition();
  AutoPlaceholderBatch beginBatching(this);
  AutoRules beginRulesSniffing(this, EditAction::loadHTML, nsIEditor::eNext);

  // Get selection
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_STATE(selection);

  RulesInfo ruleInfo(EditAction::loadHTML);
  bool cancel, handled;
  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);
  nsresult rv = rules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  NS_ENSURE_SUCCESS(rv, rv);
  if (cancel) {
    return NS_OK; // rules canceled the operation
  }

  if (!handled) {
    // Delete Selection, but only if it isn't collapsed, see bug #106269
    if (!selection->Collapsed()) {
      rv = DeleteSelection(eNone, eStrip);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // Get the first range in the selection, for context:
    RefPtr<nsRange> range = selection->GetRangeAt(0);
    NS_ENSURE_TRUE(range, NS_ERROR_NULL_POINTER);

    // Create fragment for pasted HTML.
    ErrorResult error;
    RefPtr<DocumentFragment> documentFragment =
      range->CreateContextualFragment(aInputString, error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }

    // Put the fragment into the document at start of selection.
    EditorDOMPoint pointToInsert(range->StartRef());
    // XXX We need to make pointToInsert store offset for keeping traditional
    //     behavior since using only child node to pointing insertion point
    //     changes the behavior when inserted child is moved by mutation
    //     observer.  We need to investigate what we should do here.
    Unused << pointToInsert.Offset();
    for (nsCOMPtr<nsIContent> contentToInsert =
           documentFragment->GetFirstChild();
         contentToInsert;
         contentToInsert = documentFragment->GetFirstChild()) {
      rv = InsertNode(*contentToInsert, pointToInsert.AsRaw());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      // XXX If the inserted node has been moved by mutation observer,
      //     incrementing offset will cause odd result.  Next new node
      //     will be inserted after existing node and the offset will be
      //     overflown from the container node.
      pointToInsert.Set(pointToInsert.GetContainer(),
                        pointToInsert.Offset() + 1);
      if (NS_WARN_IF(!pointToInsert.Offset())) {
        // Append the remaining children to the container if offset is
        // overflown.
        pointToInsert.SetToEndOf(pointToInsert.GetContainer());
      }
    }
  }

  return rules->DidDoAction(selection, &ruleInfo, rv);
}

NS_IMETHODIMP
HTMLEditor::InsertHTML(const nsAString& aInString)
{
  const nsString& empty = EmptyString();

  return InsertHTMLWithContext(aInString, empty, empty, empty,
                               nullptr,  nullptr, 0, true);
}

nsresult
HTMLEditor::InsertHTMLWithContext(const nsAString& aInputString,
                                  const nsAString& aContextStr,
                                  const nsAString& aInfoStr,
                                  const nsAString& aFlavor,
                                  nsIDOMDocument* aSourceDoc,
                                  nsIDOMNode* aDestNode,
                                  int32_t aDestOffset,
                                  bool aDeleteSelection)
{
  return DoInsertHTMLWithContext(aInputString, aContextStr, aInfoStr,
      aFlavor, aSourceDoc, aDestNode, aDestOffset, aDeleteSelection,
      /* trusted input */ true, /* clear style */ false);
}

nsresult
HTMLEditor::DoInsertHTMLWithContext(const nsAString& aInputString,
                                    const nsAString& aContextStr,
                                    const nsAString& aInfoStr,
                                    const nsAString& aFlavor,
                                    nsIDOMDocument* aSourceDoc,
                                    nsIDOMNode* aDestNode,
                                    int32_t aDestOffset,
                                    bool aDeleteSelection,
                                    bool aTrustedInput,
                                    bool aClearStyle)
{
  NS_ENSURE_TRUE(mRules, NS_ERROR_NOT_INITIALIZED);

  // Prevent the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  // force IME commit; set up rules sniffing and batching
  CommitComposition();
  AutoPlaceholderBatch beginBatching(this);
  AutoRules beginRulesSniffing(this, EditAction::htmlPaste, nsIEditor::eNext);

  // Get selection
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_STATE(selection);

  // create a dom document fragment that represents the structure to paste
  nsCOMPtr<nsIDOMNode> fragmentAsNode, streamStartParent, streamEndParent;
  int32_t streamStartOffset = 0, streamEndOffset = 0;

  nsresult rv = CreateDOMFragmentFromPaste(aInputString, aContextStr, aInfoStr,
                                           address_of(fragmentAsNode),
                                           address_of(streamStartParent),
                                           address_of(streamEndParent),
                                           &streamStartOffset,
                                           &streamEndOffset,
                                           aTrustedInput);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> targetNode;
  int32_t targetOffset=0;

  if (!aDestNode) {
    // if caller didn't provide the destination/target node,
    // fetch the paste insertion point from our selection
    rv = GetStartNodeAndOffset(selection, getter_AddRefs(targetNode), &targetOffset);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!targetNode || !IsEditable(targetNode)) {
      return NS_ERROR_FAILURE;
    }
  } else {
    targetNode = aDestNode;
    targetOffset = aDestOffset;
  }

  // if we have a destination / target node, we want to insert there
  // rather than in place of the selection
  // ignore aDeleteSelection here if no aDestNode since deletion will
  // also occur later; this block is intended to cover the various
  // scenarios where we are dropping in an editor (and may want to delete
  // the selection before collapsing the selection in the new destination)
  if (aDestNode) {
    if (aDeleteSelection) {
      // Use an auto tracker so that our drop point is correctly
      // positioned after the delete.
      AutoTrackDOMPoint tracker(mRangeUpdater, &targetNode, &targetOffset);
      rv = DeleteSelection(eNone, eStrip);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    rv = selection->Collapse(targetNode, targetOffset);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // we need to recalculate various things based on potentially new offsets
  // this is work to be completed at a later date (probably by jfrancis)

  // make a list of what nodes in docFrag we need to move
  nsTArray<OwningNonNull<nsINode>> nodeList;
  nsCOMPtr<nsINode> fragmentAsNodeNode = do_QueryInterface(fragmentAsNode);
  NS_ENSURE_STATE(fragmentAsNodeNode || !fragmentAsNode);
  nsCOMPtr<nsINode> streamStartParentNode =
    do_QueryInterface(streamStartParent);
  NS_ENSURE_STATE(streamStartParentNode || !streamStartParent);
  nsCOMPtr<nsINode> streamEndParentNode =
    do_QueryInterface(streamEndParent);
  NS_ENSURE_STATE(streamEndParentNode || !streamEndParent);
  CreateListOfNodesToPaste(*static_cast<DocumentFragment*>(fragmentAsNodeNode.get()),
                           nodeList,
                           streamStartParentNode, streamStartOffset,
                           streamEndParentNode, streamEndOffset);

  if (nodeList.IsEmpty()) {
    // We aren't inserting anything, but if aDeleteSelection is set, we do want
    // to delete everything.
    if (aDeleteSelection) {
      return DeleteSelection(eNone, eStrip);
    }
    return NS_OK;
  }

  // Are there any table elements in the list?
  // check for table cell selection mode
  bool cellSelectionMode = false;
  nsCOMPtr<nsIDOMElement> cell;
  rv = GetFirstSelectedCell(nullptr, getter_AddRefs(cell));
  if (NS_SUCCEEDED(rv) && cell) {
    cellSelectionMode = true;
  }

  if (cellSelectionMode) {
    // do we have table content to paste?  If so, we want to delete
    // the selected table cells and replace with new table elements;
    // but if not we want to delete _contents_ of cells and replace
    // with non-table elements.  Use cellSelectionMode bool to
    // indicate results.
    if (!HTMLEditUtils::IsTableElement(nodeList[0])) {
      cellSelectionMode = false;
    }
  }

  if (!cellSelectionMode) {
    rv = DeleteSelectionAndPrepareToCreateNode();
    NS_ENSURE_SUCCESS(rv, rv);

    if (aClearStyle) {
      // pasting does not inherit local inline styles
      nsCOMPtr<nsINode> tmpNode = selection->GetAnchorNode();
      int32_t tmpOffset = static_cast<int32_t>(selection->AnchorOffset());
      rv = ClearStyle(address_of(tmpNode), &tmpOffset, nullptr, nullptr);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  } else {
    // Delete whole cells: we will replace with new table content.

    // Braces for artificial block to scope AutoSelectionRestorer.
    // Save current selection since DeleteTableCell() perturbs it.
    {
      AutoSelectionRestorer selectionRestorer(selection, this);
      rv = DeleteTableCell(1);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    // collapse selection to beginning of deleted table content
    selection->CollapseToStart();
  }

  // give rules a chance to handle or cancel
  RulesInfo ruleInfo(EditAction::insertElement);
  bool cancel, handled;
  rv = rules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  NS_ENSURE_SUCCESS(rv, rv);
  if (cancel) {
    return NS_OK; // rules canceled the operation
  }

  if (!handled) {
    // Adjust position based on the first node we are going to insert.
    // FYI: WillDoAction() above might have changed the selection.
    EditorDOMPoint pointToInsert =
      GetBetterInsertionPointFor(nodeList[0], GetStartPoint(selection));
    if (NS_WARN_IF(!pointToInsert.IsSet())) {
      return NS_ERROR_FAILURE;
    }

    // if there are any invisible br's after our insertion point, remove them.
    // this is because if there is a br at end of what we paste, it will make
    // the invisible br visible.
    WSRunObject wsObj(this, pointToInsert.GetContainer(),
                      pointToInsert.Offset());
    if (wsObj.mEndReasonNode &&
        TextEditUtils::IsBreak(wsObj.mEndReasonNode) &&
        !IsVisibleBRElement(wsObj.mEndReasonNode)) {
      AutoEditorDOMPointChildInvalidator lockOffset(pointToInsert);
      rv = DeleteNode(wsObj.mEndReasonNode);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // Remember if we are in a link.
    bool bStartedInLink = IsInLink(pointToInsert.GetContainerAsDOMNode());

    // Are we in a text node? If so, split it.
    if (pointToInsert.IsInTextNode()) {
      SplitNodeResult splitNodeResult =
        SplitNodeDeep(*pointToInsert.GetContainerAsContent(),
                      pointToInsert.AsRaw(),
                      SplitAtEdges::eAllowToCreateEmptyContainer);
      if (NS_WARN_IF(splitNodeResult.Failed())) {
        return splitNodeResult.Rv();
      }
      pointToInsert = splitNodeResult.SplitPoint();
      if (NS_WARN_IF(!pointToInsert.IsSet())) {
        return NS_ERROR_FAILURE;
      }
    }

    // build up list of parents of first node in list that are either
    // lists or tables.  First examine front of paste node list.
    nsTArray<OwningNonNull<Element>> startListAndTableArray;
    GetListAndTableParents(StartOrEnd::start, nodeList,
                           startListAndTableArray);

    // remember number of lists and tables above us
    int32_t highWaterMark = -1;
    if (!startListAndTableArray.IsEmpty()) {
      highWaterMark = DiscoverPartialListsAndTables(nodeList,
                                                    startListAndTableArray);
    }

    // if we have pieces of tables or lists to be inserted, let's force the paste
    // to deal with table elements right away, so that it doesn't orphan some
    // table or list contents outside the table or list.
    if (highWaterMark >= 0) {
      ReplaceOrphanedStructure(StartOrEnd::start, nodeList,
                               startListAndTableArray, highWaterMark);
    }

    // Now go through the same process again for the end of the paste node list.
    nsTArray<OwningNonNull<Element>> endListAndTableArray;
    GetListAndTableParents(StartOrEnd::end, nodeList, endListAndTableArray);
    highWaterMark = -1;

    // remember number of lists and tables above us
    if (!endListAndTableArray.IsEmpty()) {
      highWaterMark = DiscoverPartialListsAndTables(nodeList,
                                                    endListAndTableArray);
    }

    // don't orphan partial list or table structure
    if (highWaterMark >= 0) {
      ReplaceOrphanedStructure(StartOrEnd::end, nodeList,
                               endListAndTableArray, highWaterMark);
    }

    MOZ_ASSERT(pointToInsert.GetContainer()->
                               GetChildAt_Deprecated(pointToInsert.Offset()) ==
                 pointToInsert.GetChild());

    // Loop over the node list and paste the nodes:
    nsCOMPtr<nsINode> parentBlock =
      IsBlockNode(pointToInsert.GetContainer()) ?
        pointToInsert.GetContainer() :
        GetBlockNodeParent(pointToInsert.GetContainer());
    nsCOMPtr<nsIContent> lastInsertNode;
    nsCOMPtr<nsINode> insertedContextParent;
    for (OwningNonNull<nsINode>& curNode : nodeList) {
      if (NS_WARN_IF(curNode == fragmentAsNodeNode) ||
          NS_WARN_IF(TextEditUtils::IsBody(curNode))) {
        return NS_ERROR_FAILURE;
      }

      if (insertedContextParent) {
        // If we had to insert something higher up in the paste hierarchy,
        // we want to skip any further paste nodes that descend from that.
        // Else we will paste twice.
        if (EditorUtils::IsDescendantOf(*curNode,
                                        *insertedContextParent)) {
          continue;
        }
      }

      // give the user a hand on table element insertion.  if they have
      // a table or table row on the clipboard, and are trying to insert
      // into a table or table row, insert the appropriate children instead.
      bool bDidInsert = false;
      if (HTMLEditUtils::IsTableRow(curNode) &&
          HTMLEditUtils::IsTableRow(pointToInsert.GetContainer()) &&
          (HTMLEditUtils::IsTable(curNode) ||
           HTMLEditUtils::IsTable(pointToInsert.GetContainer()))) {
        for (nsCOMPtr<nsIContent> firstChild = curNode->GetFirstChild();
             firstChild;
             firstChild = curNode->GetFirstChild()) {
          EditorDOMPoint insertedPoint =
            InsertNodeIntoProperAncestor(
              *firstChild, pointToInsert.AsRaw(),
              SplitAtEdges::eDoNotCreateEmptyContainer);
          if (NS_WARN_IF(!insertedPoint.IsSet())) {
            break;
          }
          bDidInsert = true;
          lastInsertNode = firstChild;
          pointToInsert = insertedPoint;
          DebugOnly<bool> advanced = pointToInsert.AdvanceOffset();
          NS_WARNING_ASSERTION(advanced,
            "Failed to advance offset from inserted point");
        }
      }
      // give the user a hand on list insertion.  if they have
      // a list on the clipboard, and are trying to insert
      // into a list or list item, insert the appropriate children instead,
      // ie, merge the lists instead of pasting in a sublist.
      else if (HTMLEditUtils::IsList(curNode) &&
               (HTMLEditUtils::IsList(pointToInsert.GetContainer()) ||
                HTMLEditUtils::IsListItem(pointToInsert.GetContainer()))) {
        for (nsCOMPtr<nsIContent> firstChild = curNode->GetFirstChild();
             firstChild;
             firstChild = curNode->GetFirstChild()) {
          if (HTMLEditUtils::IsListItem(firstChild) ||
              HTMLEditUtils::IsList(firstChild)) {
            // Check if we are pasting into empty list item. If so
            // delete it and paste into parent list instead.
            if (HTMLEditUtils::IsListItem(pointToInsert.GetContainer())) {
              bool isEmpty;
              rv = IsEmptyNode(pointToInsert.GetContainer(), &isEmpty, true);
              if (NS_SUCCEEDED(rv) && isEmpty) {
                if (NS_WARN_IF(!pointToInsert.GetContainer()->
                                                GetParentNode())) {
                  // Is it an orphan node?
                } else {
                  DeleteNode(pointToInsert.GetContainer());
                  pointToInsert.Set(pointToInsert.GetContainer());
                }
              }
            }
            EditorDOMPoint insertedPoint =
              InsertNodeIntoProperAncestor(
                *firstChild, pointToInsert.AsRaw(),
                SplitAtEdges::eDoNotCreateEmptyContainer);
            if (NS_WARN_IF(!insertedPoint.IsSet())) {
              break;
            }

            bDidInsert = true;
            lastInsertNode = firstChild;
            pointToInsert = insertedPoint;
            DebugOnly<bool> advanced = pointToInsert.AdvanceOffset();
            NS_WARNING_ASSERTION(advanced,
              "Failed to advance offset from inserted point");
          } else {
            AutoEditorDOMPointChildInvalidator lockOffset(pointToInsert);
            ErrorResult error;
            curNode->RemoveChild(*firstChild, error);
            if (NS_WARN_IF(error.Failed())) {
              error.SuppressException();
            }
          }
        }
      } else if (parentBlock && HTMLEditUtils::IsPre(parentBlock) &&
                 HTMLEditUtils::IsPre(curNode)) {
        // Check for pre's going into pre's.
        for (nsCOMPtr<nsIContent> firstChild = curNode->GetFirstChild();
             firstChild;
             firstChild = curNode->GetFirstChild()) {
          EditorDOMPoint insertedPoint =
            InsertNodeIntoProperAncestor(
              *firstChild, pointToInsert.AsRaw(),
              SplitAtEdges::eDoNotCreateEmptyContainer);
          if (NS_WARN_IF(!insertedPoint.IsSet())) {
            break;
          }

          bDidInsert = true;
          lastInsertNode = firstChild;
          pointToInsert = insertedPoint;
          DebugOnly<bool> advanced = pointToInsert.AdvanceOffset();
          NS_WARNING_ASSERTION(advanced,
            "Failed to advance offset from inserted point");
        }
      }

      if (!bDidInsert || NS_FAILED(rv)) {
        // Try to insert.
        EditorDOMPoint insertedPoint =
          InsertNodeIntoProperAncestor(
            *curNode->AsContent(), pointToInsert.AsRaw(),
            SplitAtEdges::eDoNotCreateEmptyContainer);
        if (insertedPoint.IsSet()) {
          lastInsertNode = curNode->AsContent();
          pointToInsert = insertedPoint;
        }

        // Assume failure means no legal parent in the document hierarchy,
        // try again with the parent of curNode in the paste hierarchy.
        for (nsCOMPtr<nsIContent> content =
               curNode->IsContent() ? curNode->AsContent() : nullptr;
             content && !insertedPoint.IsSet();
             content = content->GetParent()) {
          if (NS_WARN_IF(!content->GetParent()) ||
              NS_WARN_IF(TextEditUtils::IsBody(content->GetParent()))) {
            continue;
          }
          nsCOMPtr<nsINode> oldParent = content->GetParentNode();
          insertedPoint =
            InsertNodeIntoProperAncestor(
              *content->GetParent(), pointToInsert.AsRaw(),
              SplitAtEdges::eDoNotCreateEmptyContainer);
          if (insertedPoint.IsSet()) {
            insertedContextParent = oldParent;
            pointToInsert = insertedPoint;
          }
        }
      }
      if (lastInsertNode) {
        pointToInsert.Set(lastInsertNode);
        DebugOnly<bool> advanced = pointToInsert.AdvanceOffset();
        NS_WARNING_ASSERTION(advanced,
          "Failed to advance offset from inserted point");
      }
    }

    // Now collapse the selection to the end of what we just inserted:
    if (lastInsertNode) {
      // set selection to the end of what we just pasted.
      nsCOMPtr<nsINode> selNode;
      int32_t selOffset;

      // but don't cross tables
      if (!HTMLEditUtils::IsTable(lastInsertNode)) {
        selNode = GetLastEditableLeaf(*lastInsertNode);
        nsINode* highTable = nullptr;
        for (nsINode* parent = selNode;
             parent && parent != lastInsertNode;
             parent = parent->GetParentNode()) {
          if (HTMLEditUtils::IsTable(parent)) {
            highTable = parent;
          }
        }
        if (highTable) {
          selNode = highTable;
        }
      }
      if (!selNode) {
        selNode = lastInsertNode;
      }
      if (EditorBase::IsTextNode(selNode) ||
          (IsContainer(selNode) && !HTMLEditUtils::IsTable(selNode))) {
        selOffset = selNode->Length();
      } else {
        // We need to find a container for selection.  Look up.
        EditorRawDOMPoint pointAtContainer(selNode);
        if (NS_WARN_IF(!pointAtContainer.IsSet())) {
          return NS_ERROR_FAILURE;
        }
        // The container might be null in case a mutation listener removed
        // the stuff we just inserted from the DOM.
        selNode = pointAtContainer.GetContainer();
        // Want to be *after* last leaf node in paste.
        selOffset = pointAtContainer.Offset() + 1;
      }

      // make sure we don't end up with selection collapsed after an invisible break node
      WSRunObject wsRunObj(this, selNode, selOffset);
      nsCOMPtr<nsINode> visNode;
      int32_t outVisOffset=0;
      WSType visType;
      wsRunObj.PriorVisibleNode(selNode, selOffset, address_of(visNode),
                                &outVisOffset, &visType);
      if (visType == WSType::br) {
        // we are after a break.  Is it visible?  Despite the name,
        // PriorVisibleNode does not make that determination for breaks.
        // It also may not return the break in visNode.  We have to pull it
        // out of the WSRunObject's state.
        if (!IsVisibleBRElement(wsRunObj.mStartReasonNode)) {
          // don't leave selection past an invisible break;
          // reset {selNode,selOffset} to point before break
          EditorRawDOMPoint atStartReasonNode(wsRunObj.mStartReasonNode);
          selNode = atStartReasonNode.GetContainer();
          selOffset = atStartReasonNode.Offset();
          // we want to be inside any inline style prior to break
          WSRunObject wsRunObj(this, selNode, selOffset);
          wsRunObj.PriorVisibleNode(selNode, selOffset, address_of(visNode),
                                    &outVisOffset, &visType);
          if (visType == WSType::text || visType == WSType::normalWS) {
            selNode = visNode;
            selOffset = outVisOffset;  // PriorVisibleNode already set offset to _after_ the text or ws
          } else if (visType == WSType::special) {
            // prior visible thing is an image or some other non-text thingy.
            // We want to be right after it.
            atStartReasonNode.Set(wsRunObj.mStartReasonNode);
            selNode = atStartReasonNode.GetContainer();
            selOffset = atStartReasonNode.Offset() + 1;
          }
        }
      }
      selection->Collapse(selNode, selOffset);

      // if we just pasted a link, discontinue link style
      nsCOMPtr<nsIDOMNode> link;
      if (!bStartedInLink &&
          IsInLink(GetAsDOMNode(selNode), address_of(link))) {
        // so, if we just pasted a link, I split it.  Why do that instead of just
        // nudging selection point beyond it?  Because it might have ended in a BR
        // that is not visible.  If so, the code above just placed selection
        // inside that.  So I split it instead.
        nsCOMPtr<nsIContent> linkContent = do_QueryInterface(link);
        NS_ENSURE_STATE(linkContent || !link);
        SplitNodeResult splitLinkResult =
          SplitNodeDeep(*linkContent, EditorRawDOMPoint(selNode, selOffset),
                        SplitAtEdges::eDoNotCreateEmptyContainer);
        NS_WARNING_ASSERTION(splitLinkResult.Succeeded(),
          "Failed to split the link");
        if (splitLinkResult.GetPreviousNode()) {
          EditorRawDOMPoint afterLeftLink(splitLinkResult.GetPreviousNode());
          if (afterLeftLink.AdvanceOffset()) {
            selection->Collapse(afterLeftLink);
          }
        }
      }
    }
  }

  return rules->DidDoAction(selection, &ruleInfo, rv);
}

bool
HTMLEditor::IsInLink(nsIDOMNode* aNode,
                     nsCOMPtr<nsIDOMNode>* outLink)
{
  NS_ENSURE_TRUE(aNode, false);
  if (outLink) {
    *outLink = nullptr;
  }
  nsCOMPtr<nsIDOMNode> tmp, node = aNode;
  while (node) {
    if (HTMLEditUtils::IsLink(node)) {
      if (outLink) {
        *outLink = node;
      }
      return true;
    }
    tmp = node;
    tmp->GetParentNode(getter_AddRefs(node));
  }
  return false;
}

nsresult
HTMLEditor::StripFormattingNodes(nsIContent& aNode,
                                 bool aListOnly)
{
  if (aNode.TextIsOnlyWhitespace()) {
    nsCOMPtr<nsINode> parent = aNode.GetParentNode();
    if (parent) {
      if (!aListOnly || HTMLEditUtils::IsList(parent)) {
        ErrorResult rv;
        parent->RemoveChild(aNode, rv);
        return rv.StealNSResult();
      }
      return NS_OK;
    }
  }

  if (!aNode.IsHTMLElement(nsGkAtoms::pre)) {
    nsCOMPtr<nsIContent> child = aNode.GetLastChild();
    while (child) {
      nsCOMPtr<nsIContent> previous = child->GetPreviousSibling();
      nsresult rv = StripFormattingNodes(*child, aListOnly);
      NS_ENSURE_SUCCESS(rv, rv);
      child = previous.forget();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::PrepareTransferable(nsITransferable** aTransferable)
{
  return NS_OK;
}

nsresult
HTMLEditor::PrepareHTMLTransferable(nsITransferable** aTransferable)
{
  // Create generic Transferable for getting the data
  nsresult rv = CallCreateInstance("@mozilla.org/widget/transferable;1", aTransferable);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the nsITransferable interface for getting the data from the clipboard
  if (aTransferable) {
    nsCOMPtr<nsIDocument> destdoc = GetDocument();
    nsILoadContext* loadContext = destdoc ? destdoc->GetLoadContext() : nullptr;
    (*aTransferable)->Init(loadContext);

    // Create the desired DataFlavor for the type of data
    // we want to get out of the transferable
    // This should only happen in html editors, not plaintext
    if (!IsPlaintextEditor()) {
      (*aTransferable)->AddDataFlavor(kNativeHTMLMime);
      (*aTransferable)->AddDataFlavor(kHTMLMime);
      (*aTransferable)->AddDataFlavor(kFileMime);

      switch (Preferences::GetInt("clipboard.paste_image_type", 1)) {
        case 0:  // prefer JPEG over PNG over GIF encoding
          (*aTransferable)->AddDataFlavor(kJPEGImageMime);
          (*aTransferable)->AddDataFlavor(kJPGImageMime);
          (*aTransferable)->AddDataFlavor(kPNGImageMime);
          (*aTransferable)->AddDataFlavor(kGIFImageMime);
          break;
        case 1:  // prefer PNG over JPEG over GIF encoding (default)
        default:
          (*aTransferable)->AddDataFlavor(kPNGImageMime);
          (*aTransferable)->AddDataFlavor(kJPEGImageMime);
          (*aTransferable)->AddDataFlavor(kJPGImageMime);
          (*aTransferable)->AddDataFlavor(kGIFImageMime);
          break;
        case 2:  // prefer GIF over JPEG over PNG encoding
          (*aTransferable)->AddDataFlavor(kGIFImageMime);
          (*aTransferable)->AddDataFlavor(kJPEGImageMime);
          (*aTransferable)->AddDataFlavor(kJPGImageMime);
          (*aTransferable)->AddDataFlavor(kPNGImageMime);
          break;
      }
    }
    (*aTransferable)->AddDataFlavor(kUnicodeMime);
    (*aTransferable)->AddDataFlavor(kMozTextInternal);
  }

  return NS_OK;
}

bool
FindIntegerAfterString(const char* aLeadingString,
                       nsCString& aCStr,
                       int32_t& foundNumber)
{
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

  nsAutoCString numStr(Substring(aCStr, numFront, numBack-numFront));
  nsresult errorCode;
  foundNumber = numStr.ToInteger(&errorCode);
  return true;
}

nsresult
RemoveFragComments(nsCString& aStr)
{
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
  return NS_OK;
}

nsresult
HTMLEditor::ParseCFHTML(nsCString& aCfhtml,
                        char16_t** aStuffToPaste,
                        char16_t** aCfcontext)
{
  // First obtain offsets from cfhtml str.
  int32_t startHTML, endHTML, startFragment, endFragment;
  if (!FindIntegerAfterString("StartHTML:", aCfhtml, startHTML) ||
      startHTML < -1) {
    return NS_ERROR_FAILURE;
  }
  if (!FindIntegerAfterString("EndHTML:", aCfhtml, endHTML) ||
      endHTML < -1) {
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

  // The StartHTML and EndHTML markers are allowed to be -1 to include everything.
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
  nsAutoCString contextUTF8(Substring(aCfhtml, startHTML, startFragment - startHTML) +
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
        NS_ERROR("StartFragment byte count in the clipboard looks bad, see bug #228879");
        startFragment = curPos - 1;
      }
      break;
    }
    curPos--;
  }

  // create fragment string
  nsAutoCString fragmentUTF8(Substring(aCfhtml, startFragment, endFragment-startFragment));

  // remove the StartFragment/EndFragment comments from the fragment, if present
  RemoveFragComments(fragmentUTF8);

  // remove the StartFragment/EndFragment comments from the context, if present
  RemoveFragComments(contextUTF8);

  // convert both strings to usc2
  const nsString& fragUcs2Str = NS_ConvertUTF8toUTF16(fragmentUTF8);
  const nsString& cntxtUcs2Str = NS_ConvertUTF8toUTF16(contextUTF8);

  // translate platform linebreaks for fragment
  int32_t oldLengthInChars = fragUcs2Str.Length() + 1;  // +1 to include null terminator
  int32_t newLengthInChars = 0;
  *aStuffToPaste = nsLinebreakConverter::ConvertUnicharLineBreaks(fragUcs2Str.get(),
                                                           nsLinebreakConverter::eLinebreakAny,
                                                           nsLinebreakConverter::eLinebreakContent,
                                                           oldLengthInChars, &newLengthInChars);
  NS_ENSURE_TRUE(*aStuffToPaste, NS_ERROR_FAILURE);

  // translate platform linebreaks for context
  oldLengthInChars = cntxtUcs2Str.Length() + 1;  // +1 to include null terminator
  newLengthInChars = 0;
  *aCfcontext = nsLinebreakConverter::ConvertUnicharLineBreaks(cntxtUcs2Str.get(),
                                                           nsLinebreakConverter::eLinebreakAny,
                                                           nsLinebreakConverter::eLinebreakContent,
                                                           oldLengthInChars, &newLengthInChars);
  // it's ok for context to be empty.  frag might be whole doc and contain all its context.

  // we're done!
  return NS_OK;
}

static nsresult
ImgFromData(const nsACString& aType, const nsACString& aData, nsString& aOutput)
{
  nsAutoCString data64;
  nsresult rv = Base64Encode(aData, data64);
  NS_ENSURE_SUCCESS(rv, rv);

  aOutput.AssignLiteral("<IMG src=\"data:");
  AppendUTF8toUTF16(aType, aOutput);
  aOutput.AppendLiteral(";base64,");
  if (!AppendASCIItoUTF16(data64, aOutput, fallible_t())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aOutput.AppendLiteral("\" alt=\"\" >");
  return NS_OK;
}

NS_IMPL_ISUPPORTS(HTMLEditor::BlobReader, nsIEditorBlobListener)

HTMLEditor::BlobReader::BlobReader(BlobImpl* aBlob,
                                   HTMLEditor* aHTMLEditor,
                                   bool aIsSafe,
                                   nsIDOMDocument* aSourceDoc,
                                   nsIDOMNode* aDestinationNode,
                                   int32_t aDestOffset,
                                   bool aDoDeleteSelection)
  : mBlob(aBlob)
  , mHTMLEditor(aHTMLEditor)
  , mIsSafe(aIsSafe)
  , mSourceDoc(aSourceDoc)
  , mDestinationNode(aDestinationNode)
  , mDestOffset(aDestOffset)
  , mDoDeleteSelection(aDoDeleteSelection)
{
  MOZ_ASSERT(mBlob);
  MOZ_ASSERT(mHTMLEditor);
  MOZ_ASSERT(mDestinationNode);
}

NS_IMETHODIMP
HTMLEditor::BlobReader::OnResult(const nsACString& aResult)
{
  nsString blobType;
  mBlob->GetType(blobType);

  NS_ConvertUTF16toUTF8 type(blobType);
  nsAutoString stuffToPaste;
  nsresult rv = ImgFromData(type, aResult, stuffToPaste);
  NS_ENSURE_SUCCESS(rv, rv);

  AutoPlaceholderBatch beginBatching(mHTMLEditor);
  rv = mHTMLEditor->DoInsertHTMLWithContext(stuffToPaste, EmptyString(),
                                            EmptyString(),
                                            NS_LITERAL_STRING(kFileMime),
                                            mSourceDoc,
                                            mDestinationNode, mDestOffset,
                                            mDoDeleteSelection,
                                            mIsSafe, false);
  return rv;
}

NS_IMETHODIMP
HTMLEditor::BlobReader::OnError(const nsAString& aError)
{
  nsCOMPtr<nsINode> destNode = do_QueryInterface(mDestinationNode);
  const nsPromiseFlatString& flat = PromiseFlatString(aError);
  const char16_t* error = flat.get();
  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  NS_LITERAL_CSTRING("Editor"),
                                  destNode->OwnerDoc(),
                                  nsContentUtils::eDOM_PROPERTIES,
                                  "EditorFileDropFailed",
                                  &error, 1);
  return NS_OK;
}

nsresult
HTMLEditor::InsertObject(const nsACString& aType,
                         nsISupports* aObject,
                         bool aIsSafe,
                         nsIDOMDocument* aSourceDoc,
                         nsIDOMNode* aDestinationNode,
                         int32_t aDestOffset,
                         bool aDoDeleteSelection)
{
  nsresult rv;

  if (nsCOMPtr<BlobImpl> blob = do_QueryInterface(aObject)) {
    RefPtr<BlobReader> br = new BlobReader(blob, this, aIsSafe, aSourceDoc,
                                           aDestinationNode, aDestOffset,
                                           aDoDeleteSelection);
    nsCOMPtr<nsIEditorUtils> utils =
      do_GetService("@mozilla.org/editor-utils;1");
    NS_ENSURE_TRUE(utils, NS_ERROR_FAILURE);

    nsCOMPtr<nsINode> node = do_QueryInterface(aDestinationNode);
    MOZ_ASSERT(node);

    nsCOMPtr<nsIDOMBlob> domBlob = Blob::Create(node->GetOwnerGlobal(), blob);
    NS_ENSURE_TRUE(domBlob, NS_ERROR_FAILURE);

    return utils->SlurpBlob(domBlob, node->OwnerDoc()->GetWindow(), br);
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

  if (type.EqualsLiteral(kJPEGImageMime) ||
      type.EqualsLiteral(kJPGImageMime) ||
      type.EqualsLiteral(kPNGImageMime) ||
      type.EqualsLiteral(kGIFImageMime) ||
      insertAsImage) {
    nsCString imageData;
    if (insertAsImage) {
      rv = nsContentUtils::SlurpFileToString(fileObj, imageData);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      nsCOMPtr<nsIInputStream> imageStream = do_QueryInterface(aObject);
      NS_ENSURE_TRUE(imageStream, NS_ERROR_FAILURE);

      rv = NS_ConsumeStream(imageStream, UINT32_MAX, imageData);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = imageStream->Close();
      NS_ENSURE_SUCCESS(rv, rv);
    }

    nsAutoString stuffToPaste;
    rv = ImgFromData(type, imageData, stuffToPaste);
    NS_ENSURE_SUCCESS(rv, rv);

    AutoPlaceholderBatch beginBatching(this);
    rv = DoInsertHTMLWithContext(stuffToPaste, EmptyString(), EmptyString(),
                                 NS_LITERAL_STRING(kFileMime),
                                 aSourceDoc,
                                 aDestinationNode, aDestOffset,
                                 aDoDeleteSelection,
                                 aIsSafe, false);
  }

  return NS_OK;
}

nsresult
HTMLEditor::InsertFromTransferable(nsITransferable* transferable,
                                   nsIDOMDocument* aSourceDoc,
                                   const nsAString& aContextStr,
                                   const nsAString& aInfoStr,
                                   bool havePrivateHTMLFlavor,
                                   nsIDOMNode* aDestinationNode,
                                   int32_t aDestOffset,
                                   bool aDoDeleteSelection)
{
  nsresult rv = NS_OK;
  nsAutoCString bestFlavor;
  nsCOMPtr<nsISupports> genericDataObj;
  uint32_t len = 0;
  if (NS_SUCCEEDED(
        transferable->GetAnyTransferData(bestFlavor,
                                         getter_AddRefs(genericDataObj),
                                         &len))) {
    AutoTransactionsConserveSelection dontChangeMySelection(this);
    nsAutoString flavor;
    CopyASCIItoUTF16(bestFlavor, flavor);
    nsAutoString stuffToPaste;
    bool isSafe = IsSafeToInsertData(aSourceDoc);

    if (bestFlavor.EqualsLiteral(kFileMime) ||
        bestFlavor.EqualsLiteral(kJPEGImageMime) ||
        bestFlavor.EqualsLiteral(kJPGImageMime) ||
        bestFlavor.EqualsLiteral(kPNGImageMime) ||
        bestFlavor.EqualsLiteral(kGIFImageMime)) {
      rv = InsertObject(bestFlavor, genericDataObj, isSafe,
                        aSourceDoc, aDestinationNode, aDestOffset, aDoDeleteSelection);
    } else if (bestFlavor.EqualsLiteral(kNativeHTMLMime)) {
      // note cf_html uses utf8, hence use length = len, not len/2 as in flavors below
      nsCOMPtr<nsISupportsCString> textDataObj = do_QueryInterface(genericDataObj);
      if (textDataObj && len > 0) {
        nsAutoCString cfhtml;
        textDataObj->GetData(cfhtml);
        NS_ASSERTION(cfhtml.Length() <= (len), "Invalid length!");
        nsString cfcontext, cffragment, cfselection; // cfselection left emtpy for now

        rv = ParseCFHTML(cfhtml, getter_Copies(cffragment), getter_Copies(cfcontext));
        if (NS_SUCCEEDED(rv) && !cffragment.IsEmpty()) {
          AutoPlaceholderBatch beginBatching(this);
          // If we have our private HTML flavor, we will only use the fragment
          // from the CF_HTML. The rest comes from the clipboard.
          if (havePrivateHTMLFlavor) {
            rv = DoInsertHTMLWithContext(cffragment,
                                         aContextStr, aInfoStr, flavor,
                                         aSourceDoc,
                                         aDestinationNode, aDestOffset,
                                         aDoDeleteSelection,
                                         isSafe);
          } else {
            rv = DoInsertHTMLWithContext(cffragment,
                                         cfcontext, cfselection, flavor,
                                         aSourceDoc,
                                         aDestinationNode, aDestOffset,
                                         aDoDeleteSelection,
                                         isSafe);

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
      nsCOMPtr<nsISupportsString> textDataObj = do_QueryInterface(genericDataObj);
      if (textDataObj && len > 0) {
        nsAutoString text;
        textDataObj->GetData(text);
        NS_ASSERTION(text.Length() <= (len/2), "Invalid length!");
        stuffToPaste.Assign(text.get(), len / 2);
      } else {
        nsCOMPtr<nsISupportsCString> textDataObj(do_QueryInterface(genericDataObj));
        if (textDataObj && len > 0) {
          nsAutoCString text;
          textDataObj->GetData(text);
          NS_ASSERTION(text.Length() <= len, "Invalid length!");
          stuffToPaste.Assign(NS_ConvertUTF8toUTF16(Substring(text, 0, len)));
        }
      }

      if (!stuffToPaste.IsEmpty()) {
        AutoPlaceholderBatch beginBatching(this);
        if (bestFlavor.EqualsLiteral(kHTMLMime)) {
          rv = DoInsertHTMLWithContext(stuffToPaste,
                                       aContextStr, aInfoStr, flavor,
                                       aSourceDoc,
                                       aDestinationNode, aDestOffset,
                                       aDoDeleteSelection,
                                       isSafe);
        } else {
          rv = InsertTextAt(stuffToPaste, aDestinationNode, aDestOffset, aDoDeleteSelection);
        }
      }
    }
  }

  // Try to scroll the selection into view if the paste succeeded
  if (NS_SUCCEEDED(rv)) {
    ScrollSelectionIntoView(false);
  }
  return rv;
}

static void
GetStringFromDataTransfer(nsIDOMDataTransfer* aDataTransfer,
                          const nsAString& aType,
                          int32_t aIndex,
                          nsAString& aOutputString)
{
  nsCOMPtr<nsIVariant> variant;
  DataTransfer::Cast(aDataTransfer)->GetDataAtNoSecurityCheck(aType, aIndex, getter_AddRefs(variant));
  if (variant) {
    variant->GetAsAString(aOutputString);
  }
}

nsresult
HTMLEditor::InsertFromDataTransfer(DataTransfer* aDataTransfer,
                                   int32_t aIndex,
                                   nsIDOMDocument* aSourceDoc,
                                   nsIDOMNode* aDestinationNode,
                                   int32_t aDestOffset,
                                   bool aDoDeleteSelection)
{
  ErrorResult rv;
  RefPtr<DOMStringList> types =
    aDataTransfer->MozTypesAt(aIndex, CallerType::System, rv);
  if (rv.Failed()) {
    return rv.StealNSResult();
  }

  bool hasPrivateHTMLFlavor = types->Contains(NS_LITERAL_STRING(kHTMLContext));

  bool isText = IsPlaintextEditor();
  bool isSafe = IsSafeToInsertData(aSourceDoc);

  uint32_t length = types->Length();
  for (uint32_t t = 0; t < length; t++) {
    nsAutoString type;
    types->Item(t, type);

    if (!isText) {
      if (type.EqualsLiteral(kFileMime) ||
          type.EqualsLiteral(kJPEGImageMime) ||
          type.EqualsLiteral(kJPGImageMime) ||
          type.EqualsLiteral(kPNGImageMime) ||
          type.EqualsLiteral(kGIFImageMime)) {
        nsCOMPtr<nsIVariant> variant;
        DataTransfer::Cast(aDataTransfer)->GetDataAtNoSecurityCheck(type, aIndex, getter_AddRefs(variant));
        if (variant) {
          nsCOMPtr<nsISupports> object;
          variant->GetAsISupports(getter_AddRefs(object));
          return InsertObject(NS_ConvertUTF16toUTF8(type), object, isSafe,
                              aSourceDoc, aDestinationNode, aDestOffset, aDoDeleteSelection);
        }
      } else if (type.EqualsLiteral(kNativeHTMLMime)) {
        // Windows only clipboard parsing.
        nsAutoString text;
        GetStringFromDataTransfer(aDataTransfer, type, aIndex, text);
        NS_ConvertUTF16toUTF8 cfhtml(text);

        nsString cfcontext, cffragment, cfselection; // cfselection left emtpy for now

        nsresult rv = ParseCFHTML(cfhtml, getter_Copies(cffragment), getter_Copies(cfcontext));
        if (NS_SUCCEEDED(rv) && !cffragment.IsEmpty()) {
          AutoPlaceholderBatch beginBatching(this);

          if (hasPrivateHTMLFlavor) {
            // If we have our private HTML flavor, we will only use the fragment
            // from the CF_HTML. The rest comes from the clipboard.
            nsAutoString contextString, infoString;
            GetStringFromDataTransfer(aDataTransfer, NS_LITERAL_STRING(kHTMLContext), aIndex, contextString);
            GetStringFromDataTransfer(aDataTransfer, NS_LITERAL_STRING(kHTMLInfo), aIndex, infoString);
            return DoInsertHTMLWithContext(cffragment,
                                           contextString, infoString, type,
                                           aSourceDoc,
                                           aDestinationNode, aDestOffset,
                                           aDoDeleteSelection,
                                           isSafe);
          } else {
            return DoInsertHTMLWithContext(cffragment,
                                           cfcontext, cfselection, type,
                                           aSourceDoc,
                                           aDestinationNode, aDestOffset,
                                           aDoDeleteSelection,
                                           isSafe);
          }
        }
      } else if (type.EqualsLiteral(kHTMLMime)) {
        nsAutoString text, contextString, infoString;
        GetStringFromDataTransfer(aDataTransfer, type, aIndex, text);
        GetStringFromDataTransfer(aDataTransfer, NS_LITERAL_STRING(kHTMLContext), aIndex, contextString);
        GetStringFromDataTransfer(aDataTransfer, NS_LITERAL_STRING(kHTMLInfo), aIndex, infoString);

        AutoPlaceholderBatch beginBatching(this);
        if (type.EqualsLiteral(kHTMLMime)) {
          return DoInsertHTMLWithContext(text,
                                         contextString, infoString, type,
                                         aSourceDoc,
                                         aDestinationNode, aDestOffset,
                                         aDoDeleteSelection,
                                         isSafe);
        }
      }
    }

    if (type.EqualsLiteral(kTextMime) ||
        type.EqualsLiteral(kMozTextInternal)) {
      nsAutoString text;
      GetStringFromDataTransfer(aDataTransfer, type, aIndex, text);

      AutoPlaceholderBatch beginBatching(this);
      return InsertTextAt(text, aDestinationNode, aDestOffset, aDoDeleteSelection);
    }
  }

  return NS_OK;
}

bool
HTMLEditor::HavePrivateHTMLFlavor(nsIClipboard* aClipboard)
{
  // check the clipboard for our special kHTMLContext flavor.  If that is there, we know
  // we have our own internal html format on clipboard.

  NS_ENSURE_TRUE(aClipboard, false);
  bool bHavePrivateHTMLFlavor = false;

  const char* flavArray[] = { kHTMLContext };

  if (NS_SUCCEEDED(
        aClipboard->HasDataMatchingFlavors(flavArray,
                                           ArrayLength(flavArray),
                                           nsIClipboard::kGlobalClipboard,
                                           &bHavePrivateHTMLFlavor))) {
    return bHavePrivateHTMLFlavor;
  }

  return false;
}


NS_IMETHODIMP
HTMLEditor::Paste(int32_t aSelectionType)
{
  if (!FireClipboardEvent(ePaste, aSelectionType)) {
    return NS_OK;
  }

  // Get Clipboard Service
  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard(do_GetService("@mozilla.org/widget/clipboard;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the nsITransferable interface for getting the data from the clipboard
  nsCOMPtr<nsITransferable> trans;
  rv = PrepareHTMLTransferable(getter_AddRefs(trans));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(trans, NS_ERROR_FAILURE);
  // Get the Data from the clipboard
  rv = clipboard->GetData(trans, aSelectionType);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!IsModifiable()) {
    return NS_OK;
  }

  // also get additional html copy hints, if present
  nsAutoString contextStr, infoStr;

  // If we have our internal html flavor on the clipboard, there is special
  // context to use instead of cfhtml context.
  bool bHavePrivateHTMLFlavor = HavePrivateHTMLFlavor(clipboard);
  if (bHavePrivateHTMLFlavor) {
    nsCOMPtr<nsISupports> contextDataObj, infoDataObj;
    uint32_t contextLen, infoLen;
    nsCOMPtr<nsISupportsString> textDataObj;

    nsCOMPtr<nsITransferable> contextTrans =
                  do_CreateInstance("@mozilla.org/widget/transferable;1");
    NS_ENSURE_TRUE(contextTrans, NS_ERROR_NULL_POINTER);
    contextTrans->Init(nullptr);
    contextTrans->AddDataFlavor(kHTMLContext);
    clipboard->GetData(contextTrans, aSelectionType);
    contextTrans->GetTransferData(kHTMLContext, getter_AddRefs(contextDataObj), &contextLen);

    nsCOMPtr<nsITransferable> infoTrans =
                  do_CreateInstance("@mozilla.org/widget/transferable;1");
    NS_ENSURE_TRUE(infoTrans, NS_ERROR_NULL_POINTER);
    infoTrans->Init(nullptr);
    infoTrans->AddDataFlavor(kHTMLInfo);
    clipboard->GetData(infoTrans, aSelectionType);
    infoTrans->GetTransferData(kHTMLInfo, getter_AddRefs(infoDataObj), &infoLen);

    if (contextDataObj) {
      nsAutoString text;
      textDataObj = do_QueryInterface(contextDataObj);
      textDataObj->GetData(text);
      NS_ASSERTION(text.Length() <= (contextLen/2), "Invalid length!");
      contextStr.Assign(text.get(), contextLen / 2);
    }

    if (infoDataObj) {
      nsAutoString text;
      textDataObj = do_QueryInterface(infoDataObj);
      textDataObj->GetData(text);
      NS_ASSERTION(text.Length() <= (infoLen/2), "Invalid length!");
      infoStr.Assign(text.get(), infoLen / 2);
    }
  }

  // handle transferable hooks
  nsCOMPtr<nsIDOMDocument> domdoc;
  GetDocument(getter_AddRefs(domdoc));
  if (!EditorHookUtils::DoInsertionHook(domdoc, nullptr, trans)) {
    return NS_OK;
  }

  return InsertFromTransferable(trans, nullptr, contextStr, infoStr, bHavePrivateHTMLFlavor,
                                nullptr, 0, true);
}

NS_IMETHODIMP
HTMLEditor::PasteTransferable(nsITransferable* aTransferable)
{
  // Use an invalid value for the clipboard type as data comes from aTransferable
  // and we don't currently implement a way to put that in the data transfer yet.
  if (!FireClipboardEvent(ePaste, nsIClipboard::kGlobalClipboard)) {
    return NS_OK;
  }

  // handle transferable hooks
  nsCOMPtr<nsIDOMDocument> domdoc = GetDOMDocument();
  if (!EditorHookUtils::DoInsertionHook(domdoc, nullptr, aTransferable)) {
    return NS_OK;
  }

  nsAutoString contextStr, infoStr;
  return InsertFromTransferable(aTransferable, nullptr, contextStr, infoStr, false,
                                nullptr, 0, true);
}

/**
 * HTML PasteNoFormatting. Ignore any HTML styles and formating in paste source.
 */
NS_IMETHODIMP
HTMLEditor::PasteNoFormatting(int32_t aSelectionType)
{
  if (!FireClipboardEvent(ePasteNoFormatting, aSelectionType)) {
    return NS_OK;
  }

  CommitComposition();

  // Get Clipboard Service
  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard(do_GetService("@mozilla.org/widget/clipboard;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the nsITransferable interface for getting the data from the clipboard.
  // use TextEditor::PrepareTransferable() to force unicode plaintext data.
  nsCOMPtr<nsITransferable> trans;
  rv = TextEditor::PrepareTransferable(getter_AddRefs(trans));
  if (NS_SUCCEEDED(rv) && trans) {
    // Get the Data from the clipboard
    if (NS_SUCCEEDED(clipboard->GetData(trans, aSelectionType)) &&
        IsModifiable()) {
      const nsString& empty = EmptyString();
      rv = InsertFromTransferable(trans, nullptr, empty, empty, false, nullptr, 0,
                                  true);
    }
  }

  return rv;
}

// The following arrays contain the MIME types that we can paste. The arrays
// are used by CanPaste() and CanPasteTransferable() below.

static const char* textEditorFlavors[] = { kUnicodeMime };
static const char* textHtmlEditorFlavors[] = { kUnicodeMime, kHTMLMime,
                                               kJPEGImageMime, kJPGImageMime,
                                               kPNGImageMime, kGIFImageMime };

NS_IMETHODIMP
HTMLEditor::CanPaste(int32_t aSelectionType,
                     bool* aCanPaste)
{
  NS_ENSURE_ARG_POINTER(aCanPaste);
  *aCanPaste = false;

  // Always enable the paste command when inside of a HTML or XHTML document.
  nsCOMPtr<nsIDocument> doc = GetDocument();
  if (doc && doc->IsHTMLOrXHTML()) {
    *aCanPaste = true;
    return NS_OK;
  }

  // can't paste if readonly
  if (!IsModifiable()) {
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard(do_GetService("@mozilla.org/widget/clipboard;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  bool haveFlavors;

  // Use the flavors depending on the current editor mask
  if (IsPlaintextEditor()) {
    rv = clipboard->HasDataMatchingFlavors(textEditorFlavors,
                                           ArrayLength(textEditorFlavors),
                                           aSelectionType, &haveFlavors);
  } else {
    rv = clipboard->HasDataMatchingFlavors(textHtmlEditorFlavors,
                                           ArrayLength(textHtmlEditorFlavors),
                                           aSelectionType, &haveFlavors);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  *aCanPaste = haveFlavors;
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::CanPasteTransferable(nsITransferable* aTransferable,
                                 bool* aCanPaste)
{
  NS_ENSURE_ARG_POINTER(aCanPaste);

  // can't paste if readonly
  if (!IsModifiable()) {
    *aCanPaste = false;
    return NS_OK;
  }

  // If |aTransferable| is null, assume that a paste will succeed.
  if (!aTransferable) {
    *aCanPaste = true;
    return NS_OK;
  }

  // Peek in |aTransferable| to see if it contains a supported MIME type.

  // Use the flavors depending on the current editor mask
  const char ** flavors;
  unsigned length;
  if (IsPlaintextEditor()) {
    flavors = textEditorFlavors;
    length = ArrayLength(textEditorFlavors);
  } else {
    flavors = textHtmlEditorFlavors;
    length = ArrayLength(textHtmlEditorFlavors);
  }

  for (unsigned int i = 0; i < length; i++, flavors++) {
    nsCOMPtr<nsISupports> data;
    uint32_t dataLen;
    nsresult rv = aTransferable->GetTransferData(*flavors,
                                                 getter_AddRefs(data),
                                                 &dataLen);
    if (NS_SUCCEEDED(rv) && data) {
      *aCanPaste = true;
      return NS_OK;
    }
  }

  *aCanPaste = false;
  return NS_OK;
}

/**
 * HTML PasteAsQuotation: Paste in a blockquote type=cite.
 */
NS_IMETHODIMP
HTMLEditor::PasteAsQuotation(int32_t aSelectionType)
{
  if (IsPlaintextEditor()) {
    return PasteAsPlaintextQuotation(aSelectionType);
  }

  nsAutoString citation;
  return PasteAsCitedQuotation(citation, aSelectionType);
}

NS_IMETHODIMP
HTMLEditor::PasteAsCitedQuotation(const nsAString& aCitation,
                                  int32_t aSelectionType)
{
  AutoPlaceholderBatch beginBatching(this);
  AutoRules beginRulesSniffing(this, EditAction::insertQuotation,
                               nsIEditor::eNext);

  // get selection
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  // give rules a chance to handle or cancel
  RulesInfo ruleInfo(EditAction::insertElement);
  bool cancel, handled;
  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);
  nsresult rv = rules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  NS_ENSURE_SUCCESS(rv, rv);
  if (cancel || handled) {
    return NS_OK; // rules canceled the operation
  }

  nsCOMPtr<Element> newNode =
    DeleteSelectionAndCreateElement(*nsGkAtoms::blockquote);
  NS_ENSURE_TRUE(newNode, NS_ERROR_NULL_POINTER);

  // Try to set type=cite.  Ignore it if this fails.
  newNode->SetAttr(kNameSpaceID_None, nsGkAtoms::type,
                   NS_LITERAL_STRING("cite"), true);

  // Set the selection to the underneath the node we just inserted:
  rv = selection->Collapse(newNode, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  return Paste(aSelectionType);
}

/**
 * Paste a plaintext quotation.
 */
NS_IMETHODIMP
HTMLEditor::PasteAsPlaintextQuotation(int32_t aSelectionType)
{
  // Get Clipboard Service
  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard(do_GetService("@mozilla.org/widget/clipboard;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // Create generic Transferable for getting the data
  nsCOMPtr<nsITransferable> trans =
                 do_CreateInstance("@mozilla.org/widget/transferable;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(trans, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocument> destdoc = GetDocument();
  nsILoadContext* loadContext = destdoc ? destdoc->GetLoadContext() : nullptr;
  trans->Init(loadContext);

  // We only handle plaintext pastes here
  trans->AddDataFlavor(kUnicodeMime);

  // Get the Data from the clipboard
  clipboard->GetData(trans, aSelectionType);

  // Now we ask the transferable for the data
  // it still owns the data, we just have a pointer to it.
  // If it can't support a "text" output of the data the call will fail
  nsCOMPtr<nsISupports> genericDataObj;
  uint32_t len = 0;
  nsAutoCString flav;
  rv = trans->GetAnyTransferData(flav, getter_AddRefs(genericDataObj), &len);
  NS_ENSURE_SUCCESS(rv, rv);

  if (flav.EqualsLiteral(kUnicodeMime)) {
    nsCOMPtr<nsISupportsString> textDataObj = do_QueryInterface(genericDataObj);
    if (textDataObj && len > 0) {
      nsAutoString stuffToPaste;
      textDataObj->GetData(stuffToPaste);
      NS_ASSERTION(stuffToPaste.Length() <= (len/2), "Invalid length!");
      AutoPlaceholderBatch beginBatching(this);
      rv = InsertAsPlaintextQuotation(stuffToPaste, true, 0);
    }
  }

  return rv;
}

NS_IMETHODIMP
HTMLEditor::InsertTextWithQuotations(const nsAString& aStringToInsert)
{
  // The whole operation should be undoable in one transaction:
  BeginTransaction();

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
  nsAString::const_iterator dbgStart (hunkStart);
  if (FindCharInReadable('\r', dbgStart, strEnd)) {
    NS_ASSERTION(false,
            "Return characters in DOM! InsertTextWithQuotations may be wrong");
  }
#endif /* DEBUG */

  // Loop over lines:
  nsresult rv = NS_OK;
  nsAString::const_iterator lineStart (hunkStart);
  // We will break from inside when we run out of newlines.
  for (;;) {
    // Search for the end of this line (dom newlines, see above):
    bool found = FindCharInReadable('\n', lineStart, strEnd);
    bool quoted = false;
    if (found) {
      // if there's another newline, lineStart now points there.
      // Loop over any consecutive newline chars:
      nsAString::const_iterator firstNewline (lineStart);
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
    const nsAString &curHunk = Substring(hunkStart, lineStart);
    nsCOMPtr<nsIDOMNode> dummyNode;
    if (curHunkIsQuoted) {
      rv = InsertAsPlaintextQuotation(curHunk, false,
                                      getter_AddRefs(dummyNode));
    } else {
      rv = InsertText(curHunk);
    }
    if (!found) {
      break;
    }
    curHunkIsQuoted = quoted;
    hunkStart = lineStart;
  }

  EndTransaction();

  return rv;
}

NS_IMETHODIMP
HTMLEditor::InsertAsQuotation(const nsAString& aQuotedText,
                              nsIDOMNode** aNodeInserted)
{
  if (IsPlaintextEditor()) {
    return InsertAsPlaintextQuotation(aQuotedText, true, aNodeInserted);
  }

  nsAutoString citation;
  return InsertAsCitedQuotation(aQuotedText, citation, false,
                                aNodeInserted);
}

// Insert plaintext as a quotation, with cite marks (e.g. "> ").
// This differs from its corresponding method in TextEditor
// in that here, quoted material is enclosed in a <pre> tag
// in order to preserve the original line wrapping.
NS_IMETHODIMP
HTMLEditor::InsertAsPlaintextQuotation(const nsAString& aQuotedText,
                                       bool aAddCites,
                                       nsIDOMNode** aNodeInserted)
{
  // get selection
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  AutoPlaceholderBatch beginBatching(this);
  AutoRules beginRulesSniffing(this, EditAction::insertQuotation,
                               nsIEditor::eNext);

  // give rules a chance to handle or cancel
  RulesInfo ruleInfo(EditAction::insertElement);
  bool cancel, handled;
  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);
  nsresult rv = rules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  NS_ENSURE_SUCCESS(rv, rv);
  if (cancel || handled) {
    return NS_OK; // rules canceled the operation
  }

  // Wrap the inserted quote in a <span> so we can distinguish it. If we're
  // inserting into the <body>, we use a <span> which is displayed as a block
  // and sized to the screen using 98 viewport width units.
  // We could use 100vw, but 98vw avoids a horizontal scroll bar where possible.
  // All this is done to wrap overlong lines to the screen and not to the
  // container element, the width-restricted body.
  nsCOMPtr<Element> newNode =
    DeleteSelectionAndCreateElement(*nsGkAtoms::span);

  // If this succeeded, then set selection inside the pre
  // so the inserted text will end up there.
  // If it failed, we don't care what the return value was,
  // but we'll fall through and try to insert the text anyway.
  if (newNode) {
    // Add an attribute on the pre node so we'll know it's a quotation.
    newNode->SetAttr(kNameSpaceID_None, nsGkAtoms::mozquote,
                     NS_LITERAL_STRING("true"), true);
    // Allow wrapping on spans so long lines get wrapped to the screen.
    nsCOMPtr<nsINode> parent = newNode->GetParentNode();
    if (parent && parent->IsHTMLElement(nsGkAtoms::body)) {
      newNode->SetAttr(kNameSpaceID_None, nsGkAtoms::style,
        NS_LITERAL_STRING("white-space: pre-wrap; display: block; width: 98vw;"),
        true);
    } else {
      newNode->SetAttr(kNameSpaceID_None, nsGkAtoms::style,
        NS_LITERAL_STRING("white-space: pre-wrap;"), true);
    }

    // and set the selection inside it:
    selection->Collapse(newNode, 0);
  }

  if (aAddCites) {
    rv = TextEditor::InsertAsQuotation(aQuotedText, aNodeInserted);
  } else {
    rv = TextEditor::InsertText(aQuotedText);
  }
  // Note that if !aAddCites, aNodeInserted isn't set.
  // That's okay because the routines that use aAddCites
  // don't need to know the inserted node.

  if (aNodeInserted && NS_SUCCEEDED(rv)) {
    *aNodeInserted = GetAsDOMNode(newNode);
    NS_IF_ADDREF(*aNodeInserted);
  }

  // Set the selection to just after the inserted node:
  if (NS_SUCCEEDED(rv) && newNode) {
    EditorRawDOMPoint afterNewNode(newNode);
    if (afterNewNode.AdvanceOffset()) {
      selection->Collapse(afterNewNode);
    }
  }
  return rv;
}

NS_IMETHODIMP
HTMLEditor::StripCites()
{
  return TextEditor::StripCites();
}

NS_IMETHODIMP
HTMLEditor::Rewrap(bool aRespectNewlines)
{
  return TextEditor::Rewrap(aRespectNewlines);
}

NS_IMETHODIMP
HTMLEditor::InsertAsCitedQuotation(const nsAString& aQuotedText,
                                   const nsAString& aCitation,
                                   bool aInsertHTML,
                                   nsIDOMNode** aNodeInserted)
{
  // Don't let anyone insert html into a "plaintext" editor:
  if (IsPlaintextEditor()) {
    NS_ASSERTION(!aInsertHTML, "InsertAsCitedQuotation: trying to insert html into plaintext editor");
    return InsertAsPlaintextQuotation(aQuotedText, true, aNodeInserted);
  }

  // get selection
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  AutoPlaceholderBatch beginBatching(this);
  AutoRules beginRulesSniffing(this, EditAction::insertQuotation,
                               nsIEditor::eNext);

  // give rules a chance to handle or cancel
  RulesInfo ruleInfo(EditAction::insertElement);
  bool cancel, handled;
  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);
  nsresult rv = rules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  NS_ENSURE_SUCCESS(rv, rv);
  if (cancel || handled) {
    return NS_OK; // rules canceled the operation
  }

  nsCOMPtr<Element> newNode =
    DeleteSelectionAndCreateElement(*nsGkAtoms::blockquote);
  NS_ENSURE_TRUE(newNode, NS_ERROR_NULL_POINTER);

  // Try to set type=cite.  Ignore it if this fails.
  newNode->SetAttr(kNameSpaceID_None, nsGkAtoms::type,
                   NS_LITERAL_STRING("cite"), true);

  if (!aCitation.IsEmpty()) {
    newNode->SetAttr(kNameSpaceID_None, nsGkAtoms::cite, aCitation, true);
  }

  // Set the selection inside the blockquote so aQuotedText will go there:
  selection->Collapse(newNode, 0);

  if (aInsertHTML) {
    rv = LoadHTML(aQuotedText);
  } else {
    rv = InsertText(aQuotedText);  // XXX ignore charset
  }

  if (aNodeInserted && NS_SUCCEEDED(rv)) {
    *aNodeInserted = GetAsDOMNode(newNode);
    NS_IF_ADDREF(*aNodeInserted);
  }

  // Set the selection to just after the inserted node:
  if (NS_SUCCEEDED(rv) && newNode) {
    EditorRawDOMPoint afterNewNode(newNode);
    if (afterNewNode.AdvanceOffset()) {
      selection->Collapse(afterNewNode);
    }
  }
  return rv;
}


void RemoveBodyAndHead(nsINode& aNode)
{
  nsCOMPtr<nsIContent> body, head;
  // find the body and head nodes if any.
  // look only at immediate children of aNode.
  for (nsCOMPtr<nsIContent> child = aNode.GetFirstChild();
       child;
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
nsresult FindTargetNode(nsIDOMNode *aStart, nsCOMPtr<nsIDOMNode> &aResult)
{
  NS_ENSURE_TRUE(aStart, NS_OK);

  nsCOMPtr<nsIDOMNode> child, tmp;

  nsresult rv = aStart->GetFirstChild(getter_AddRefs(child));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!child) {
    // If the current result is nullptr, then aStart is a leaf, and is the
    // fallback result.
    if (!aResult) {
      aResult = aStart;
    }
    return NS_OK;
  }

  do {
    // Is this child the magical cookie?
    nsCOMPtr<nsIDOMComment> comment = do_QueryInterface(child);
    if (comment) {
      nsAutoString data;
      rv = comment->GetData(data);
      NS_ENSURE_SUCCESS(rv, rv);

      if (data.EqualsLiteral(kInsertCookie)) {
        // Yes it is! Return an error so we bubble out and short-circuit the
        // search.
        aResult = aStart;

        // Note: it doesn't matter if this fails.
        aStart->RemoveChild(child, getter_AddRefs(tmp));

        return NS_SUCCESS_EDITOR_FOUND_TARGET;
      }
    }

    rv = FindTargetNode(child, aResult);
    NS_ENSURE_SUCCESS(rv, rv);

    if (rv == NS_SUCCESS_EDITOR_FOUND_TARGET) {
      return NS_SUCCESS_EDITOR_FOUND_TARGET;
    }

    rv = child->GetNextSibling(getter_AddRefs(tmp));
    NS_ENSURE_SUCCESS(rv, rv);

    child = tmp;
  } while (child);

  return NS_OK;
}

nsresult
HTMLEditor::CreateDOMFragmentFromPaste(const nsAString& aInputString,
                                       const nsAString& aContextStr,
                                       const nsAString& aInfoStr,
                                       nsCOMPtr<nsIDOMNode>* outFragNode,
                                       nsCOMPtr<nsIDOMNode>* outStartNode,
                                       nsCOMPtr<nsIDOMNode>* outEndNode,
                                       int32_t* outStartOffset,
                                       int32_t* outEndOffset,
                                       bool aTrustedInput)
{
  NS_ENSURE_TRUE(outFragNode && outStartNode && outEndNode, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDocument> doc = GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  // if we have context info, create a fragment for that
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDOMNode> contextLeaf;
  RefPtr<DocumentFragment> contextAsNode;
  if (!aContextStr.IsEmpty()) {
    rv = ParseFragment(aContextStr, nullptr, doc, getter_AddRefs(contextAsNode),
                       aTrustedInput);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(contextAsNode, NS_ERROR_FAILURE);

    rv = StripFormattingNodes(*contextAsNode);
    NS_ENSURE_SUCCESS(rv, rv);

    RemoveBodyAndHead(*contextAsNode);

    rv = FindTargetNode(contextAsNode, contextLeaf);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIContent> contextLeafAsContent = do_QueryInterface(contextLeaf);
  MOZ_ASSERT_IF(contextLeaf, contextLeafAsContent);

  // create fragment for pasted html
  nsAtom* contextAtom;
  if (contextLeafAsContent) {
    contextAtom = contextLeafAsContent->NodeInfo()->NameAtom();
    if (contextLeafAsContent->IsHTMLElement(nsGkAtoms::html)) {
      contextAtom = nsGkAtoms::body;
    }
  } else {
    contextAtom = nsGkAtoms::body;
  }
  RefPtr<DocumentFragment> fragment;
  rv = ParseFragment(aInputString,
                     contextAtom,
                     doc,
                     getter_AddRefs(fragment),
                     aTrustedInput);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(fragment, NS_ERROR_FAILURE);

  RemoveBodyAndHead(*fragment);

  if (contextAsNode) {
    // unite the two trees
    IgnoredErrorResult ignored;
    contextLeafAsContent->AppendChild(*fragment, ignored);
    fragment = contextAsNode;
  }

  rv = StripFormattingNodes(*fragment, true);
  NS_ENSURE_SUCCESS(rv, rv);

  // If there was no context, then treat all of the data we did get as the
  // pasted data.
  if (contextLeaf) {
    *outEndNode = *outStartNode = contextLeaf;
  } else {
    *outEndNode = *outStartNode = fragment;
  }

  *outFragNode = fragment.forget();
  *outStartOffset = 0;

  // get the infoString contents
  if (!aInfoStr.IsEmpty()) {
    int32_t sep = aInfoStr.FindChar((char16_t)',');
    nsAutoString numstr1(Substring(aInfoStr, 0, sep));
    nsAutoString numstr2(Substring(aInfoStr, sep+1, aInfoStr.Length() - (sep+1)));

    // Move the start and end children.
    nsresult err;
    int32_t num = numstr1.ToInteger(&err);

    nsCOMPtr<nsIDOMNode> tmp;
    while (num--) {
      (*outStartNode)->GetFirstChild(getter_AddRefs(tmp));
      NS_ENSURE_TRUE(tmp, NS_ERROR_FAILURE);
      tmp.swap(*outStartNode);
    }

    num = numstr2.ToInteger(&err);
    while (num--) {
      (*outEndNode)->GetLastChild(getter_AddRefs(tmp));
      NS_ENSURE_TRUE(tmp, NS_ERROR_FAILURE);
      tmp.swap(*outEndNode);
    }
  }

  nsCOMPtr<nsINode> node = do_QueryInterface(*outEndNode);
  *outEndOffset = node->Length();
  return NS_OK;
}


nsresult
HTMLEditor::ParseFragment(const nsAString& aFragStr,
                          nsAtom* aContextLocalName,
                          nsIDocument* aTargetDocument,
                          DocumentFragment** aFragment,
                          bool aTrustedInput)
{
  nsAutoScriptBlockerSuppressNodeRemoved autoBlocker;

  RefPtr<DocumentFragment> fragment =
    new DocumentFragment(aTargetDocument->NodeInfoManager());
  nsresult rv = nsContentUtils::ParseFragmentHTML(aFragStr,
                                                  fragment,
                                                  aContextLocalName ?
                                                    aContextLocalName : nsGkAtoms::body,
                                                    kNameSpaceID_XHTML,
                                                  false,
                                                  true);
  if (!aTrustedInput) {
    nsTreeSanitizer sanitizer(aContextLocalName ?
                              nsIParserUtils::SanitizerAllowStyle :
                              nsIParserUtils::SanitizerAllowComments);
    sanitizer.Sanitize(fragment);
  }
  fragment.forget(aFragment);
  return rv;
}

void
HTMLEditor::CreateListOfNodesToPaste(
              DocumentFragment& aFragment,
              nsTArray<OwningNonNull<nsINode>>& outNodeList,
              nsINode* aStartContainer,
              int32_t aStartOffset,
              nsINode* aEndContainer,
              int32_t aEndOffset)
{
  // If no info was provided about the boundary between context and stream,
  // then assume all is stream.
  if (!aStartContainer) {
    aStartContainer = &aFragment;
    aStartOffset = 0;
    aEndContainer = &aFragment;
    aEndOffset = aFragment.Length();
  }

  RefPtr<nsRange> docFragRange;
  nsresult rv = nsRange::CreateRange(aStartContainer, aStartOffset,
                                     aEndContainer, aEndOffset,
                                     getter_AddRefs(docFragRange));
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  NS_ENSURE_SUCCESS(rv, );

  // Now use a subtree iterator over the range to create a list of nodes
  TrivialFunctor functor;
  DOMSubtreeIterator iter;
  rv = iter.Init(*docFragRange);
  NS_ENSURE_SUCCESS(rv, );
  iter.AppendList(functor, outNodeList);
}

void
HTMLEditor::GetListAndTableParents(StartOrEnd aStartOrEnd,
                                   nsTArray<OwningNonNull<nsINode>>& aNodeList,
                                   nsTArray<OwningNonNull<Element>>& outArray)
{
  MOZ_ASSERT(aNodeList.Length());

  // Build up list of parents of first (or last) node in list that are either
  // lists, or tables.
  int32_t idx = aStartOrEnd == StartOrEnd::end ? aNodeList.Length() - 1 : 0;

  for (nsCOMPtr<nsINode> node = aNodeList[idx]; node;
       node = node->GetParentNode()) {
    if (HTMLEditUtils::IsList(node) || HTMLEditUtils::IsTable(node)) {
      outArray.AppendElement(*node->AsElement());
    }
  }
}

int32_t
HTMLEditor::DiscoverPartialListsAndTables(
              nsTArray<OwningNonNull<nsINode>>& aPasteNodes,
              nsTArray<OwningNonNull<Element>>& aListsAndTables)
{
  int32_t ret = -1;
  int32_t listAndTableParents = aListsAndTables.Length();

  // Scan insertion list for table elements (other than table).
  for (auto& curNode : aPasteNodes) {
    if (HTMLEditUtils::IsTableElement(curNode) &&
        !curNode->IsHTMLElement(nsGkAtoms::table)) {
      nsCOMPtr<Element> table = curNode->GetParentElement();
      while (table && !table->IsHTMLElement(nsGkAtoms::table)) {
        table = table->GetParentElement();
      }
      if (table) {
        int32_t idx = aListsAndTables.IndexOf(table);
        if (idx == -1) {
          return ret;
        }
        ret = idx;
        if (ret == listAndTableParents - 1) {
          return ret;
        }
      }
    }
    if (HTMLEditUtils::IsListItem(curNode)) {
      nsCOMPtr<Element> list = curNode->GetParentElement();
      while (list && !HTMLEditUtils::IsList(list)) {
        list = list->GetParentElement();
      }
      if (list) {
        int32_t idx = aListsAndTables.IndexOf(list);
        if (idx == -1) {
          return ret;
        }
        ret = idx;
        if (ret == listAndTableParents - 1) {
          return ret;
        }
      }
    }
  }
  return ret;
}

nsINode*
HTMLEditor::ScanForListAndTableStructure(
              StartOrEnd aStartOrEnd,
              nsTArray<OwningNonNull<nsINode>>& aNodes,
              Element& aListOrTable)
{
  // Look upward from first/last paste node for a piece of this list/table
  int32_t idx = aStartOrEnd == StartOrEnd::end ? aNodes.Length() - 1 : 0;
  bool isList = HTMLEditUtils::IsList(&aListOrTable);

  for (nsCOMPtr<nsINode> node = aNodes[idx]; node;
       node = node->GetParentNode()) {
    if ((isList && HTMLEditUtils::IsListItem(node)) ||
        (!isList && HTMLEditUtils::IsTableElement(node) &&
                    !node->IsHTMLElement(nsGkAtoms::table))) {
      nsCOMPtr<Element> structureNode = node->GetParentElement();
      if (isList) {
        while (structureNode && !HTMLEditUtils::IsList(structureNode)) {
          structureNode = structureNode->GetParentElement();
        }
      } else {
        while (structureNode &&
               !structureNode->IsHTMLElement(nsGkAtoms::table)) {
          structureNode = structureNode->GetParentElement();
        }
      }
      if (structureNode == &aListOrTable) {
        if (isList) {
          return structureNode;
        }
        return node;
      }
    }
  }
  return nullptr;
}

void
HTMLEditor::ReplaceOrphanedStructure(
              StartOrEnd aStartOrEnd,
              nsTArray<OwningNonNull<nsINode>>& aNodeArray,
              nsTArray<OwningNonNull<Element>>& aListAndTableArray,
              int32_t aHighWaterMark)
{
  OwningNonNull<Element> curNode = aListAndTableArray[aHighWaterMark];

  // Find substructure of list or table that must be included in paste.
  nsCOMPtr<nsINode> replaceNode =
    ScanForListAndTableStructure(aStartOrEnd, aNodeArray, curNode);

  if (!replaceNode) {
    return;
  }

  // If we found substructure, paste it instead of its descendants.
  // Postprocess list to remove any descendants of this node so that we don't
  // insert them twice.
  uint32_t removedCount = 0;
  uint32_t originalLength = aNodeArray.Length();
  for (uint32_t i = 0; i < originalLength; i++) {
    uint32_t idx = aStartOrEnd == StartOrEnd::start ?
      (i - removedCount) : (originalLength - i - 1);
    OwningNonNull<nsINode> endpoint = aNodeArray[idx];
    if (endpoint == replaceNode ||
        EditorUtils::IsDescendantOf(*endpoint, *replaceNode)) {
      aNodeArray.RemoveElementAt(idx);
      removedCount++;
    }
  }

  // Now replace the removed nodes with the structural parent
  if (aStartOrEnd == StartOrEnd::end) {
    aNodeArray.AppendElement(*replaceNode);
  } else {
    aNodeArray.InsertElementAt(0, *replaceNode);
  }
}

} // namespace mozilla
