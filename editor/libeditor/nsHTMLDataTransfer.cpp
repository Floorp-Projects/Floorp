/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>

#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/OwningNonNull.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Base64.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/Selection.h"
#include "nsAString.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsCRTGlue.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsDependentSubstring.h"
#include "nsEditRules.h"
#include "nsEditor.h"
#include "nsEditorUtils.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsHTMLEditUtils.h"
#include "nsHTMLEditor.h"
#include "nsIClipboard.h"
#include "nsIContent.h"
#include "nsIContentFilter.h"
#include "nsIDOMComment.h"
#include "mozilla/dom/DOMStringList.h"
#include "mozilla/dom/DataTransfer.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIDOMElement.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLEmbedElement.h"
#include "nsIDOMHTMLFrameElement.h"
#include "nsIDOMHTMLIFrameElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLLinkElement.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsIDOMHTMLScriptElement.h"
#include "nsIDOMNode.h"
#include "nsIDocument.h"
#include "nsIEditor.h"
#include "nsIEditorIMESupport.h"
#include "nsIEditorMailSupport.h"
#include "nsIFile.h"
#include "nsIInputStream.h"
#include "nsIMIMEService.h"
#include "nsNameSpaceManager.h"
#include "nsINode.h"
#include "nsIParserUtils.h"
#include "nsIPlaintextEditor.h"
#include "nsISupportsImpl.h"
#include "nsISupportsPrimitives.h"
#include "nsISupportsUtils.h"
#include "nsITransferable.h"
#include "nsIURI.h"
#include "nsIVariant.h"
#include "nsLinebreakConverter.h"
#include "nsLiteralString.h"
#include "nsNetUtil.h"
#include "nsPlaintextEditor.h"
#include "nsRange.h"
#include "nsReadableUtils.h"
#include "nsSelectionState.h"
#include "nsServiceManagerUtils.h"
#include "nsStreamUtils.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsStringIterator.h"
#include "nsSubstringTuple.h"
#include "nsTextEditRules.h"
#include "nsTextEditUtils.h"
#include "nsTreeSanitizer.h"
#include "nsWSRunObject.h"
#include "nsXPCOM.h"
#include "nscore.h"
#include "nsContentUtils.h"

class nsIAtom;
class nsILoadContext;
class nsISupports;

using namespace mozilla;
using namespace mozilla::dom;

#define kInsertCookie  "_moz_Insert Here_moz_"

// some little helpers
static bool FindIntegerAfterString(const char *aLeadingString,
                                     nsCString &aCStr, int32_t &foundNumber);
static nsresult RemoveFragComments(nsCString &theStr);
static void RemoveBodyAndHead(nsINode& aNode);
static nsresult FindTargetNode(nsIDOMNode *aStart, nsCOMPtr<nsIDOMNode> &aResult);

nsresult
nsHTMLEditor::LoadHTML(const nsAString & aInputString)
{
  NS_ENSURE_TRUE(mRules, NS_ERROR_NOT_INITIALIZED);

  // force IME commit; set up rules sniffing and batching
  ForceCompositionEnd();
  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, EditAction::loadHTML, nsIEditor::eNext);

  // Get selection
  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_STATE(selection);

  nsTextRulesInfo ruleInfo(EditAction::loadHTML);
  bool cancel, handled;
  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);
  nsresult rv = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  NS_ENSURE_SUCCESS(rv, rv);
  if (cancel) {
    return NS_OK; // rules canceled the operation
  }

  if (!handled)
  {
    // Delete Selection, but only if it isn't collapsed, see bug #106269
    if (!selection->Collapsed()) {
      rv = DeleteSelection(eNone, eStrip);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // Get the first range in the selection, for context:
    nsRefPtr<nsRange> range = selection->GetRangeAt(0);
    NS_ENSURE_TRUE(range, NS_ERROR_NULL_POINTER);

    // create fragment for pasted html
    nsCOMPtr<nsIDOMDocumentFragment> docfrag;
    {
      rv = range->CreateContextualFragment(aInputString, getter_AddRefs(docfrag));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    // put the fragment into the document
    nsCOMPtr<nsIDOMNode> parent, junk;
    rv = range->GetStartContainer(getter_AddRefs(parent));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);
    int32_t childOffset;
    rv = range->GetStartOffset(&childOffset);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMNode> nodeToInsert;
    docfrag->GetFirstChild(getter_AddRefs(nodeToInsert));
    while (nodeToInsert)
    {
      rv = InsertNode(nodeToInsert, parent, childOffset++);
      NS_ENSURE_SUCCESS(rv, rv);
      docfrag->GetFirstChild(getter_AddRefs(nodeToInsert));
    }
  }

  return mRules->DidDoAction(selection, &ruleInfo, rv);
}


NS_IMETHODIMP nsHTMLEditor::InsertHTML(const nsAString & aInString)
{
  const nsAFlatString& empty = EmptyString();

  return InsertHTMLWithContext(aInString, empty, empty, empty,
                               nullptr,  nullptr, 0, true);
}


nsresult
nsHTMLEditor::InsertHTMLWithContext(const nsAString & aInputString,
                                    const nsAString & aContextStr,
                                    const nsAString & aInfoStr,
                                    const nsAString & aFlavor,
                                    nsIDOMDocument *aSourceDoc,
                                    nsIDOMNode *aDestNode,
                                    int32_t aDestOffset,
                                    bool aDeleteSelection)
{
  return DoInsertHTMLWithContext(aInputString, aContextStr, aInfoStr,
      aFlavor, aSourceDoc, aDestNode, aDestOffset, aDeleteSelection,
      /* trusted input */ true, /* clear style */ false);
}

nsresult
nsHTMLEditor::DoInsertHTMLWithContext(const nsAString & aInputString,
                                      const nsAString & aContextStr,
                                      const nsAString & aInfoStr,
                                      const nsAString & aFlavor,
                                      nsIDOMDocument *aSourceDoc,
                                      nsIDOMNode *aDestNode,
                                      int32_t aDestOffset,
                                      bool aDeleteSelection,
                                      bool aTrustedInput,
                                      bool aClearStyle)
{
  NS_ENSURE_TRUE(mRules, NS_ERROR_NOT_INITIALIZED);

  // Prevent the edit rules object from dying
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);

  // force IME commit; set up rules sniffing and batching
  ForceCompositionEnd();
  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, EditAction::htmlPaste, nsIEditor::eNext);

  // Get selection
  nsRefPtr<Selection> selection = GetSelection();
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

  nsCOMPtr<nsIDOMNode> targetNode, tempNode;
  int32_t targetOffset=0;

  if (!aDestNode)
  {
    // if caller didn't provide the destination/target node,
    // fetch the paste insertion point from our selection
    rv = GetStartNodeAndOffset(selection, getter_AddRefs(targetNode), &targetOffset);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!targetNode || !IsEditable(targetNode)) {
      return NS_ERROR_FAILURE;
    }
  }
  else
  {
    targetNode = aDestNode;
    targetOffset = aDestOffset;
  }

  bool doContinue = true;

  rv = DoContentFilterCallback(aFlavor, aSourceDoc, aDeleteSelection,
                               (nsIDOMNode **)address_of(fragmentAsNode),
                               (nsIDOMNode **)address_of(streamStartParent),
                               &streamStartOffset,
                               (nsIDOMNode **)address_of(streamEndParent),
                               &streamEndOffset,
                               (nsIDOMNode **)address_of(targetNode),
                               &targetOffset, &doContinue);

  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(doContinue, NS_OK);

  // if we have a destination / target node, we want to insert there
  // rather than in place of the selection
  // ignore aDeleteSelection here if no aDestNode since deletion will
  // also occur later; this block is intended to cover the various
  // scenarios where we are dropping in an editor (and may want to delete
  // the selection before collapsing the selection in the new destination)
  if (aDestNode)
  {
    if (aDeleteSelection)
    {
      // Use an auto tracker so that our drop point is correctly
      // positioned after the delete.
      nsAutoTrackDOMPoint tracker(mRangeUpdater, &targetNode, &targetOffset);
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

  if (nodeList.Length() == 0) {
    // We aren't inserting anything, but if aDeleteSelection is set, we do want
    // to delete everything.
    if (aDeleteSelection) {
      return DeleteSelection(eNone, eStrip);
    }
    return NS_OK;
  }

  // Are there any table elements in the list?
  // node and offset for insertion
  nsCOMPtr<nsIDOMNode> parentNode;
  int32_t offsetOfNewNode;

  // check for table cell selection mode
  bool cellSelectionMode = false;
  nsCOMPtr<nsIDOMElement> cell;
  rv = GetFirstSelectedCell(nullptr, getter_AddRefs(cell));
  if (NS_SUCCEEDED(rv) && cell)
  {
    cellSelectionMode = true;
  }

  if (cellSelectionMode)
  {
    // do we have table content to paste?  If so, we want to delete
    // the selected table cells and replace with new table elements;
    // but if not we want to delete _contents_ of cells and replace
    // with non-table elements.  Use cellSelectionMode bool to
    // indicate results.
    if (!nsHTMLEditUtils::IsTableElement(nodeList[0])) {
      cellSelectionMode = false;
    }
  }

  if (!cellSelectionMode)
  {
    rv = DeleteSelectionAndPrepareToCreateNode();
    NS_ENSURE_SUCCESS(rv, rv);

    if (aClearStyle) {
      // pasting does not inherit local inline styles
      nsCOMPtr<nsIDOMNode> tmpNode =
        do_QueryInterface(selection->GetAnchorNode());
      int32_t tmpOffset = static_cast<int32_t>(selection->AnchorOffset());
      rv = ClearStyle(address_of(tmpNode), &tmpOffset, nullptr, nullptr);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  else
  {
    // delete whole cells: we will replace with new table content
    { // Braces for artificial block to scope nsAutoSelectionReset.
      // Save current selection since DeleteTableCell perturbs it
      nsAutoSelectionReset selectionResetter(selection, this);
      rv = DeleteTableCell(1);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    // collapse selection to beginning of deleted table content
    selection->CollapseToStart();
  }

  // give rules a chance to handle or cancel
  nsTextRulesInfo ruleInfo(EditAction::insertElement);
  bool cancel, handled;
  rv = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  NS_ENSURE_SUCCESS(rv, rv);
  if (cancel) {
    return NS_OK; // rules canceled the operation
  }

  if (!handled)
  {
    // The rules code (WillDoAction above) might have changed the selection.
    // refresh our memory...
    rv = GetStartNodeAndOffset(selection, getter_AddRefs(parentNode), &offsetOfNewNode);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(parentNode, NS_ERROR_FAILURE);

    // Adjust position based on the first node we are going to insert.
    NormalizeEOLInsertPosition(GetAsDOMNode(nodeList[0]),
                               address_of(parentNode), &offsetOfNewNode);

    // if there are any invisible br's after our insertion point, remove them.
    // this is because if there is a br at end of what we paste, it will make
    // the invisible br visible.
    nsWSRunObject wsObj(this, parentNode, offsetOfNewNode);
    if (wsObj.mEndReasonNode &&
        nsTextEditUtils::IsBreak(wsObj.mEndReasonNode) &&
        !IsVisBreak(wsObj.mEndReasonNode)) {
      rv = DeleteNode(wsObj.mEndReasonNode);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // Remember if we are in a link.
    bool bStartedInLink = IsInLink(parentNode);

    // Are we in a text node? If so, split it.
    if (IsTextNode(parentNode))
    {
      nsCOMPtr<nsIDOMNode> temp;
      rv = SplitNodeDeep(parentNode, parentNode, offsetOfNewNode, &offsetOfNewNode);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = parentNode->GetParentNode(getter_AddRefs(temp));
      NS_ENSURE_SUCCESS(rv, rv);
      parentNode = temp;
    }

    // build up list of parents of first node in list that are either
    // lists or tables.  First examine front of paste node list.
    nsTArray<OwningNonNull<Element>> startListAndTableArray;
    GetListAndTableParents(StartOrEnd::start, nodeList,
                           startListAndTableArray);

    // remember number of lists and tables above us
    int32_t highWaterMark = -1;
    if (startListAndTableArray.Length() > 0) {
      highWaterMark = DiscoverPartialListsAndTables(nodeList,
                                                    startListAndTableArray);
    }

    // if we have pieces of tables or lists to be inserted, let's force the paste
    // to deal with table elements right away, so that it doesn't orphan some
    // table or list contents outside the table or list.
    if (highWaterMark >= 0)
    {
      ReplaceOrphanedStructure(StartOrEnd::start, nodeList,
                               startListAndTableArray, highWaterMark);
    }

    // Now go through the same process again for the end of the paste node list.
    nsTArray<OwningNonNull<Element>> endListAndTableArray;
    GetListAndTableParents(StartOrEnd::end, nodeList, endListAndTableArray);
    highWaterMark = -1;

    // remember number of lists and tables above us
    if (endListAndTableArray.Length() > 0) {
      highWaterMark = DiscoverPartialListsAndTables(nodeList,
                                                    endListAndTableArray);
    }

    // don't orphan partial list or table structure
    if (highWaterMark >= 0)
    {
      ReplaceOrphanedStructure(StartOrEnd::end, nodeList,
                               endListAndTableArray, highWaterMark);
    }

    // Loop over the node list and paste the nodes:
    nsCOMPtr<nsIDOMNode> parentBlock, lastInsertNode, insertedContextParent;
    int32_t listCount = nodeList.Length();
    int32_t j;
    if (IsBlockNode(parentNode))
      parentBlock = parentNode;
    else
      parentBlock = GetBlockNodeParent(parentNode);

    for (j=0; j<listCount; j++)
    {
      bool bDidInsert = false;
      nsCOMPtr<nsIDOMNode> curNode = nodeList[j]->AsDOMNode();

      NS_ENSURE_TRUE(curNode, NS_ERROR_FAILURE);
      NS_ENSURE_TRUE(curNode != fragmentAsNode, NS_ERROR_FAILURE);
      NS_ENSURE_TRUE(!nsTextEditUtils::IsBody(curNode), NS_ERROR_FAILURE);

      if (insertedContextParent)
      {
        // if we had to insert something higher up in the paste hierarchy, we want to
        // skip any further paste nodes that descend from that.  Else we will paste twice.
        if (nsEditorUtils::IsDescendantOf(curNode, insertedContextParent))
          continue;
      }

      // give the user a hand on table element insertion.  if they have
      // a table or table row on the clipboard, and are trying to insert
      // into a table or table row, insert the appropriate children instead.
      if (  (nsHTMLEditUtils::IsTableRow(curNode) && nsHTMLEditUtils::IsTableRow(parentNode))
         && (nsHTMLEditUtils::IsTable(curNode)    || nsHTMLEditUtils::IsTable(parentNode)) )
      {
        nsCOMPtr<nsIDOMNode> child;
        curNode->GetFirstChild(getter_AddRefs(child));
        while (child)
        {
          rv = InsertNodeAtPoint(child, address_of(parentNode), &offsetOfNewNode, true);
          if (NS_FAILED(rv))
            break;

          bDidInsert = true;
          lastInsertNode = child;
          offsetOfNewNode++;

          curNode->GetFirstChild(getter_AddRefs(child));
        }
      }
      // give the user a hand on list insertion.  if they have
      // a list on the clipboard, and are trying to insert
      // into a list or list item, insert the appropriate children instead,
      // ie, merge the lists instead of pasting in a sublist.
      else if (nsHTMLEditUtils::IsList(curNode) &&
              (nsHTMLEditUtils::IsList(parentNode)  || nsHTMLEditUtils::IsListItem(parentNode)) )
      {
        nsCOMPtr<nsIDOMNode> child, tmp;
        curNode->GetFirstChild(getter_AddRefs(child));
        while (child)
        {
          if (nsHTMLEditUtils::IsListItem(child) || nsHTMLEditUtils::IsList(child))
          {
            // Check if we are pasting into empty list item. If so
            // delete it and paste into parent list instead.
            if (nsHTMLEditUtils::IsListItem(parentNode))
            {
              bool isEmpty;
              rv = IsEmptyNode(parentNode, &isEmpty, true);
              if (NS_SUCCEEDED(rv) && isEmpty)
              {
                int32_t newOffset;
                nsCOMPtr<nsIDOMNode> listNode = GetNodeLocation(parentNode, &newOffset);
                if (listNode)
                {
                  DeleteNode(parentNode);
                  parentNode = listNode;
                  offsetOfNewNode = newOffset;
                }
              }
            }
            rv = InsertNodeAtPoint(child, address_of(parentNode), &offsetOfNewNode, true);
            if (NS_FAILED(rv))
              break;

            bDidInsert = true;
            lastInsertNode = child;
            offsetOfNewNode++;
          }
          else
          {
            curNode->RemoveChild(child, getter_AddRefs(tmp));
          }
          curNode->GetFirstChild(getter_AddRefs(child));
        }

      } else if (parentBlock && nsHTMLEditUtils::IsPre(parentBlock) &&
                 nsHTMLEditUtils::IsPre(curNode)) {
        // Check for pre's going into pre's.
        nsCOMPtr<nsIDOMNode> child, tmp;
        curNode->GetFirstChild(getter_AddRefs(child));
        while (child)
        {
          rv = InsertNodeAtPoint(child, address_of(parentNode), &offsetOfNewNode, true);
          if (NS_FAILED(rv))
            break;

          bDidInsert = true;
          lastInsertNode = child;
          offsetOfNewNode++;

          curNode->GetFirstChild(getter_AddRefs(child));
        }
      }

      if (!bDidInsert || NS_FAILED(rv))
      {
        // try to insert
        rv = InsertNodeAtPoint(curNode, address_of(parentNode), &offsetOfNewNode, true);
        if (NS_SUCCEEDED(rv))
        {
          bDidInsert = true;
          lastInsertNode = curNode;
        }

        // Assume failure means no legal parent in the document hierarchy,
        // try again with the parent of curNode in the paste hierarchy.
        nsCOMPtr<nsIDOMNode> parent;
        while (NS_FAILED(rv) && curNode)
        {
          curNode->GetParentNode(getter_AddRefs(parent));
          if (parent && !nsTextEditUtils::IsBody(parent))
          {
            rv = InsertNodeAtPoint(parent, address_of(parentNode), &offsetOfNewNode, true);
            if (NS_SUCCEEDED(rv))
            {
              bDidInsert = true;
              insertedContextParent = parent;
              lastInsertNode = GetChildAt(parentNode, offsetOfNewNode);
            }
          }
          curNode = parent;
        }
      }
      if (lastInsertNode)
      {
        parentNode = GetNodeLocation(lastInsertNode, &offsetOfNewNode);
        offsetOfNewNode++;
      }
    }

    // Now collapse the selection to the end of what we just inserted:
    if (lastInsertNode)
    {
      // set selection to the end of what we just pasted.
      nsCOMPtr<nsIDOMNode> selNode, tmp, highTable;
      int32_t selOffset;

      // but don't cross tables
      if (!nsHTMLEditUtils::IsTable(lastInsertNode))
      {
        nsCOMPtr<nsINode> lastInsertNode_ = do_QueryInterface(lastInsertNode);
        NS_ENSURE_STATE(lastInsertNode_ || !lastInsertNode);
        selNode = GetAsDOMNode(GetLastEditableLeaf(*lastInsertNode_));
        tmp = selNode;
        while (tmp && (tmp != lastInsertNode))
        {
          if (nsHTMLEditUtils::IsTable(tmp))
            highTable = tmp;
          nsCOMPtr<nsIDOMNode> parent = tmp;
          tmp->GetParentNode(getter_AddRefs(parent));
          tmp = parent;
        }
        if (highTable)
          selNode = highTable;
      }
      if (!selNode)
        selNode = lastInsertNode;
      if (IsTextNode(selNode) || (IsContainer(selNode) && !nsHTMLEditUtils::IsTable(selNode)))
      {
        rv = GetLengthOfDOMNode(selNode, (uint32_t&)selOffset);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      else // we need to find a container for selection.  Look up.
      {
        tmp = selNode;
        selNode = GetNodeLocation(tmp, &selOffset);
        // selNode might be null in case a mutation listener removed
        // the stuff we just inserted from the DOM.
        NS_ENSURE_STATE(selNode);
        ++selOffset;  // want to be *after* last leaf node in paste
      }

      // make sure we don't end up with selection collapsed after an invisible break node
      nsWSRunObject wsRunObj(this, selNode, selOffset);
      nsCOMPtr<nsINode> visNode;
      int32_t outVisOffset=0;
      WSType visType;
      nsCOMPtr<nsINode> selNode_(do_QueryInterface(selNode));
      wsRunObj.PriorVisibleNode(selNode_, selOffset, address_of(visNode),
                                &outVisOffset, &visType);
      if (visType == WSType::br) {
        // we are after a break.  Is it visible?  Despite the name,
        // PriorVisibleNode does not make that determination for breaks.
        // It also may not return the break in visNode.  We have to pull it
        // out of the nsWSRunObject's state.
        if (!IsVisBreak(wsRunObj.mStartReasonNode))
        {
          // don't leave selection past an invisible break;
          // reset {selNode,selOffset} to point before break
          selNode = GetNodeLocation(GetAsDOMNode(wsRunObj.mStartReasonNode), &selOffset);
          // we want to be inside any inline style prior to break
          nsWSRunObject wsRunObj(this, selNode, selOffset);
          selNode_ = do_QueryInterface(selNode);
          wsRunObj.PriorVisibleNode(selNode_, selOffset, address_of(visNode),
                                    &outVisOffset, &visType);
          if (visType == WSType::text || visType == WSType::normalWS) {
            selNode = GetAsDOMNode(visNode);
            selOffset = outVisOffset;  // PriorVisibleNode already set offset to _after_ the text or ws
          } else if (visType == WSType::special) {
            // prior visible thing is an image or some other non-text thingy.
            // We want to be right after it.
            selNode = GetNodeLocation(GetAsDOMNode(wsRunObj.mStartReasonNode), &selOffset);
            ++selOffset;
          }
        }
      }
      selection->Collapse(selNode, selOffset);

      // if we just pasted a link, discontinue link style
      nsCOMPtr<nsIDOMNode> link;
      if (!bStartedInLink && IsInLink(selNode, address_of(link)))
      {
        // so, if we just pasted a link, I split it.  Why do that instead of just
        // nudging selection point beyond it?  Because it might have ended in a BR
        // that is not visible.  If so, the code above just placed selection
        // inside that.  So I split it instead.
        nsCOMPtr<nsIDOMNode> leftLink;
        int32_t linkOffset;
        rv = SplitNodeDeep(link, selNode, selOffset, &linkOffset, true, address_of(leftLink));
        NS_ENSURE_SUCCESS(rv, rv);
        if (leftLink) {
          selNode = GetNodeLocation(leftLink, &selOffset);
          selection->Collapse(selNode, selOffset+1);
        }
      }
    }
  }

  return mRules->DidDoAction(selection, &ruleInfo, rv);
}

NS_IMETHODIMP
nsHTMLEditor::AddInsertionListener(nsIContentFilter *aListener)
{
  NS_ENSURE_TRUE(aListener, NS_ERROR_NULL_POINTER);

  // don't let a listener be added more than once
  if (!mContentFilters.Contains(aListener)) {
    mContentFilters.AppendElement(*aListener);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::RemoveInsertionListener(nsIContentFilter *aListener)
{
  NS_ENSURE_TRUE(aListener, NS_ERROR_FAILURE);

  mContentFilters.RemoveElement(aListener);

  return NS_OK;
}

nsresult
nsHTMLEditor::DoContentFilterCallback(const nsAString &aFlavor,
                                      nsIDOMDocument *sourceDoc,
                                      bool aWillDeleteSelection,
                                      nsIDOMNode **aFragmentAsNode,
                                      nsIDOMNode **aFragStartNode,
                                      int32_t *aFragStartOffset,
                                      nsIDOMNode **aFragEndNode,
                                      int32_t *aFragEndOffset,
                                      nsIDOMNode **aTargetNode,
                                      int32_t *aTargetOffset,
                                      bool *aDoContinue)
{
  *aDoContinue = true;

  for (auto& listener : mContentFilters) {
    if (!*aDoContinue) {
      break;
    }
    listener->NotifyOfInsertion(aFlavor, nullptr, sourceDoc,
                                aWillDeleteSelection, aFragmentAsNode,
                                aFragStartNode, aFragStartOffset,
                                aFragEndNode, aFragEndOffset, aTargetNode,
                                aTargetOffset, aDoContinue);
  }

  return NS_OK;
}

bool
nsHTMLEditor::IsInLink(nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *outLink)
{
  NS_ENSURE_TRUE(aNode, false);
  if (outLink)
    *outLink = nullptr;
  nsCOMPtr<nsIDOMNode> tmp, node = aNode;
  while (node)
  {
    if (nsHTMLEditUtils::IsLink(node))
    {
      if (outLink)
        *outLink = node;
      return true;
    }
    tmp = node;
    tmp->GetParentNode(getter_AddRefs(node));
  }
  return false;
}


nsresult
nsHTMLEditor::StripFormattingNodes(nsIContent& aNode, bool aListOnly)
{
  if (aNode.TextIsOnlyWhitespace()) {
    nsCOMPtr<nsINode> parent = aNode.GetParentNode();
    if (parent) {
      if (!aListOnly || nsHTMLEditUtils::IsList(parent)) {
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

NS_IMETHODIMP nsHTMLEditor::PrepareTransferable(nsITransferable **transferable)
{
  return NS_OK;
}

nsresult
nsHTMLEditor::PrepareHTMLTransferable(nsITransferable **aTransferable,
                                      bool aHavePrivFlavor)
{
  // Create generic Transferable for getting the data
  nsresult rv = CallCreateInstance("@mozilla.org/widget/transferable;1", aTransferable);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the nsITransferable interface for getting the data from the clipboard
  if (aTransferable)
  {
    nsCOMPtr<nsIDocument> destdoc = GetDocument();
    nsILoadContext* loadContext = destdoc ? destdoc->GetLoadContext() : nullptr;
    (*aTransferable)->Init(loadContext);

    // Create the desired DataFlavor for the type of data
    // we want to get out of the transferable
    // This should only happen in html editors, not plaintext
    if (!IsPlaintextEditor())
    {
      if (!aHavePrivFlavor)
      {
        (*aTransferable)->AddDataFlavor(kNativeHTMLMime);
      }
      (*aTransferable)->AddDataFlavor(kHTMLMime);
      (*aTransferable)->AddDataFlavor(kFileMime);

      switch (Preferences::GetInt("clipboard.paste_image_type", 1))
      {
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
FindIntegerAfterString(const char *aLeadingString,
                       nsCString &aCStr, int32_t &foundNumber)
{
  // first obtain offsets from cfhtml str
  int32_t numFront = aCStr.Find(aLeadingString);
  if (numFront == -1)
    return false;
  numFront += strlen(aLeadingString);

  int32_t numBack = aCStr.FindCharInSet(CRLF, numFront);
  if (numBack == -1)
    return false;

  nsAutoCString numStr(Substring(aCStr, numFront, numBack-numFront));
  nsresult errorCode;
  foundNumber = numStr.ToInteger(&errorCode);
  return true;
}

nsresult
RemoveFragComments(nsCString & aStr)
{
  // remove the StartFragment/EndFragment comments from the str, if present
  int32_t startCommentIndx = aStr.Find("<!--StartFragment");
  if (startCommentIndx >= 0)
  {
    int32_t startCommentEnd = aStr.Find("-->", false, startCommentIndx);
    if (startCommentEnd > startCommentIndx)
      aStr.Cut(startCommentIndx, (startCommentEnd+3)-startCommentIndx);
  }
  int32_t endCommentIndx = aStr.Find("<!--EndFragment");
  if (endCommentIndx >= 0)
  {
    int32_t endCommentEnd = aStr.Find("-->", false, endCommentIndx);
    if (endCommentEnd > endCommentIndx)
      aStr.Cut(endCommentIndx, (endCommentEnd+3)-endCommentIndx);
  }
  return NS_OK;
}

nsresult
nsHTMLEditor::ParseCFHTML(nsCString & aCfhtml, char16_t **aStuffToPaste, char16_t **aCfcontext)
{
  // First obtain offsets from cfhtml str.
  int32_t startHTML, endHTML, startFragment, endFragment;
  if (!FindIntegerAfterString("StartHTML:", aCfhtml, startHTML) ||
      startHTML < -1)
    return NS_ERROR_FAILURE;
  if (!FindIntegerAfterString("EndHTML:", aCfhtml, endHTML) ||
      endHTML < -1)
    return NS_ERROR_FAILURE;
  if (!FindIntegerAfterString("StartFragment:", aCfhtml, startFragment) ||
      startFragment < 0)
    return NS_ERROR_FAILURE;
  if (!FindIntegerAfterString("EndFragment:", aCfhtml, endFragment) ||
      startFragment < 0)
    return NS_ERROR_FAILURE;

  // The StartHTML and EndHTML markers are allowed to be -1 to include everything.
  //   See Reference: MSDN doc entitled "HTML Clipboard Format"
  //   http://msdn.microsoft.com/en-us/library/aa767917(VS.85).aspx#unknown_854
  if (startHTML == -1) {
    startHTML = aCfhtml.Find("<!--StartFragment-->");
    if (startHTML == -1)
      return NS_OK;
  }
  if (endHTML == -1) {
    const char endFragmentMarker[] = "<!--EndFragment-->";
    endHTML = aCfhtml.Find(endFragmentMarker);
    if (endHTML == -1)
      return NS_OK;
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
  while (curPos > startHTML)
  {
      if (aCfhtml[curPos] == '>')
      {
          // working backwards, the first thing we see is the end of a tag
          // so StartFragment is good, so do nothing.
          break;
      }
      else if (aCfhtml[curPos] == '<')
      {
          // if we are at the start, then we want to see the '<'
          if (curPos != startFragment)
          {
              // working backwards, the first thing we see is the start of a tag
              // so StartFragment is bad, so we need to update it.
              NS_ERROR("StartFragment byte count in the clipboard looks bad, see bug #228879");
              startFragment = curPos - 1;
          }
          break;
      }
      else
      {
          curPos--;
      }
  }

  // create fragment string
  nsAutoCString fragmentUTF8(Substring(aCfhtml, startFragment, endFragment-startFragment));

  // remove the StartFragment/EndFragment comments from the fragment, if present
  RemoveFragComments(fragmentUTF8);

  // remove the StartFragment/EndFragment comments from the context, if present
  RemoveFragComments(contextUTF8);

  // convert both strings to usc2
  const nsAFlatString& fragUcs2Str = NS_ConvertUTF8toUTF16(fragmentUTF8);
  const nsAFlatString& cntxtUcs2Str = NS_ConvertUTF8toUTF16(contextUTF8);

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

nsresult nsHTMLEditor::InsertObject(const char* aType, nsISupports* aObject, bool aIsSafe,
                                    nsIDOMDocument *aSourceDoc,
                                    nsIDOMNode *aDestinationNode,
                                    int32_t aDestOffset,
                                    bool aDoDeleteSelection)
{
  nsresult rv;

  const char* type = aType;

  // Check to see if we can insert an image file
  bool insertAsImage = false;
  nsCOMPtr<nsIURI> fileURI;
  if (0 == nsCRT::strcmp(type, kFileMime))
  {
    nsCOMPtr<nsIFile> fileObj = do_QueryInterface(aObject);
    if (fileObj)
    {
      rv = NS_NewFileURI(getter_AddRefs(fileURI), fileObj);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIMIMEService> mime = do_GetService("@mozilla.org/mime;1");
      NS_ENSURE_TRUE(mime, NS_ERROR_FAILURE);
      nsAutoCString contentType;
      rv = mime->GetTypeFromFile(fileObj, contentType);
      NS_ENSURE_SUCCESS(rv, rv);

      // Accept any image type fed to us
      if (StringBeginsWith(contentType, NS_LITERAL_CSTRING("image/"))) {
        insertAsImage = true;
        type = contentType.get();
      }
    }
  }

  if (0 == nsCRT::strcmp(type, kJPEGImageMime) ||
      0 == nsCRT::strcmp(type, kJPGImageMime) ||
      0 == nsCRT::strcmp(type, kPNGImageMime) ||
      0 == nsCRT::strcmp(type, kGIFImageMime) ||
      insertAsImage)
  {
    nsCOMPtr<nsIInputStream> imageStream;
    if (insertAsImage) {
      NS_ASSERTION(fileURI, "The file URI should be retrieved earlier");

      nsCOMPtr<nsIChannel> channel;
      rv = NS_NewChannel(getter_AddRefs(channel),
                         fileURI,
                         nsContentUtils::GetSystemPrincipal(),
                         nsILoadInfo::SEC_NORMAL,
                         nsIContentPolicy::TYPE_OTHER);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = channel->Open(getter_AddRefs(imageStream));
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      imageStream = do_QueryInterface(aObject);
      NS_ENSURE_TRUE(imageStream, NS_ERROR_FAILURE);
    }

    nsCString imageData;
    rv = NS_ConsumeStream(imageStream, UINT32_MAX, imageData);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = imageStream->Close();
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString data64;
    rv = Base64Encode(imageData, data64);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString stuffToPaste;
    stuffToPaste.AssignLiteral("<IMG src=\"data:");
    AppendUTF8toUTF16(type, stuffToPaste);
    stuffToPaste.AppendLiteral(";base64,");
    AppendUTF8toUTF16(data64, stuffToPaste);
    stuffToPaste.AppendLiteral("\" alt=\"\" >");
    nsAutoEditBatch beginBatching(this);
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
nsHTMLEditor::InsertFromTransferable(nsITransferable *transferable,
                                     nsIDOMDocument *aSourceDoc,
                                     const nsAString & aContextStr,
                                     const nsAString & aInfoStr,
                                     nsIDOMNode *aDestinationNode,
                                     int32_t aDestOffset,
                                     bool aDoDeleteSelection)
{
  nsresult rv = NS_OK;
  nsXPIDLCString bestFlavor;
  nsCOMPtr<nsISupports> genericDataObj;
  uint32_t len = 0;
  if (NS_SUCCEEDED(transferable->GetAnyTransferData(getter_Copies(bestFlavor), getter_AddRefs(genericDataObj), &len)))
  {
    nsAutoTxnsConserveSelection dontSpazMySelection(this);
    nsAutoString flavor;
    flavor.AssignWithConversion(bestFlavor);
    nsAutoString stuffToPaste;
    bool isSafe = IsSafeToInsertData(aSourceDoc);

    if (0 == nsCRT::strcmp(bestFlavor, kFileMime) ||
        0 == nsCRT::strcmp(bestFlavor, kJPEGImageMime) ||
        0 == nsCRT::strcmp(bestFlavor, kJPGImageMime) ||
        0 == nsCRT::strcmp(bestFlavor, kPNGImageMime) ||
        0 == nsCRT::strcmp(bestFlavor, kGIFImageMime)) {
      rv = InsertObject(bestFlavor, genericDataObj, isSafe,
                        aSourceDoc, aDestinationNode, aDestOffset, aDoDeleteSelection);
    }
    else if (0 == nsCRT::strcmp(bestFlavor, kNativeHTMLMime))
    {
      // note cf_html uses utf8, hence use length = len, not len/2 as in flavors below
      nsCOMPtr<nsISupportsCString> textDataObj = do_QueryInterface(genericDataObj);
      if (textDataObj && len > 0)
      {
        nsAutoCString cfhtml;
        textDataObj->GetData(cfhtml);
        NS_ASSERTION(cfhtml.Length() <= (len), "Invalid length!");
        nsXPIDLString cfcontext, cffragment, cfselection; // cfselection left emtpy for now

        rv = ParseCFHTML(cfhtml, getter_Copies(cffragment), getter_Copies(cfcontext));
        if (NS_SUCCEEDED(rv) && !cffragment.IsEmpty())
        {
          nsAutoEditBatch beginBatching(this);
          rv = DoInsertHTMLWithContext(cffragment,
                                       cfcontext, cfselection, flavor,
                                       aSourceDoc,
                                       aDestinationNode, aDestOffset,
                                       aDoDeleteSelection,
                                       isSafe);
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
    if (0 == nsCRT::strcmp(bestFlavor, kHTMLMime) ||
        0 == nsCRT::strcmp(bestFlavor, kUnicodeMime) ||
        0 == nsCRT::strcmp(bestFlavor, kMozTextInternal)) {
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
        nsAutoEditBatch beginBatching(this);
        if (0 == nsCRT::strcmp(bestFlavor, kHTMLMime)) {
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
  if (NS_SUCCEEDED(rv))
    ScrollSelectionIntoView(false);

  return rv;
}

static void
GetStringFromDataTransfer(nsIDOMDataTransfer *aDataTransfer, const nsAString& aType,
                          int32_t aIndex, nsAString& aOutputString)
{
  nsCOMPtr<nsIVariant> variant;
  aDataTransfer->MozGetDataAt(aType, aIndex, getter_AddRefs(variant));
  if (variant)
    variant->GetAsAString(aOutputString);
}

nsresult nsHTMLEditor::InsertFromDataTransfer(DataTransfer *aDataTransfer,
                                              int32_t aIndex,
                                              nsIDOMDocument *aSourceDoc,
                                              nsIDOMNode *aDestinationNode,
                                              int32_t aDestOffset,
                                              bool aDoDeleteSelection)
{
  ErrorResult rv;
  nsRefPtr<DOMStringList> types = aDataTransfer->MozTypesAt(aIndex, rv);
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
        aDataTransfer->MozGetDataAt(type, aIndex, getter_AddRefs(variant));
        if (variant) {
          nsCOMPtr<nsISupports> object;
          variant->GetAsISupports(getter_AddRefs(object));
          return InsertObject(NS_ConvertUTF16toUTF8(type).get(), object, isSafe,
                              aSourceDoc, aDestinationNode, aDestOffset, aDoDeleteSelection);
        }
      }
      else if (!hasPrivateHTMLFlavor && type.EqualsLiteral(kNativeHTMLMime)) {
        nsAutoString text;
        GetStringFromDataTransfer(aDataTransfer, NS_LITERAL_STRING(kNativeHTMLMime), aIndex, text);
        NS_ConvertUTF16toUTF8 cfhtml(text);

        nsXPIDLString cfcontext, cffragment, cfselection; // cfselection left emtpy for now

        nsresult rv = ParseCFHTML(cfhtml, getter_Copies(cffragment), getter_Copies(cfcontext));
        if (NS_SUCCEEDED(rv) && !cffragment.IsEmpty())
        {
          nsAutoEditBatch beginBatching(this);
          return DoInsertHTMLWithContext(cffragment,
                                         cfcontext, cfselection, type,
                                         aSourceDoc,
                                         aDestinationNode, aDestOffset,
                                         aDoDeleteSelection,
                                         isSafe);
        }
      }
      else if (type.EqualsLiteral(kHTMLMime)) {
        nsAutoString text, contextString, infoString;
        GetStringFromDataTransfer(aDataTransfer, type, aIndex, text);
        GetStringFromDataTransfer(aDataTransfer, NS_LITERAL_STRING(kHTMLContext), aIndex, contextString);
        GetStringFromDataTransfer(aDataTransfer, NS_LITERAL_STRING(kHTMLInfo), aIndex, infoString);

        nsAutoEditBatch beginBatching(this);
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

      nsAutoEditBatch beginBatching(this);
      return InsertTextAt(text, aDestinationNode, aDestOffset, aDoDeleteSelection);
    }
  }

  return NS_OK;
}

bool nsHTMLEditor::HavePrivateHTMLFlavor(nsIClipboard *aClipboard)
{
  // check the clipboard for our special kHTMLContext flavor.  If that is there, we know
  // we have our own internal html format on clipboard.

  NS_ENSURE_TRUE(aClipboard, false);
  bool bHavePrivateHTMLFlavor = false;

  const char* flavArray[] = { kHTMLContext };

  if (NS_SUCCEEDED(aClipboard->HasDataMatchingFlavors(flavArray,
    ArrayLength(flavArray), nsIClipboard::kGlobalClipboard,
    &bHavePrivateHTMLFlavor)))
    return bHavePrivateHTMLFlavor;

  return false;
}


NS_IMETHODIMP nsHTMLEditor::Paste(int32_t aSelectionType)
{
  if (!FireClipboardEvent(NS_PASTE, aSelectionType))
    return NS_OK;

  // Get Clipboard Service
  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard(do_GetService("@mozilla.org/widget/clipboard;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // find out if we have our internal html flavor on the clipboard.  We don't want to mess
  // around with cfhtml if we do.
  bool bHavePrivateHTMLFlavor = HavePrivateHTMLFlavor(clipboard);

  // Get the nsITransferable interface for getting the data from the clipboard
  nsCOMPtr<nsITransferable> trans;
  rv = PrepareHTMLTransferable(getter_AddRefs(trans), bHavePrivateHTMLFlavor);
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

  // also get additional html copy hints, if present
  if (bHavePrivateHTMLFlavor)
  {
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

    if (contextDataObj)
    {
      nsAutoString text;
      textDataObj = do_QueryInterface(contextDataObj);
      textDataObj->GetData(text);
      NS_ASSERTION(text.Length() <= (contextLen/2), "Invalid length!");
      contextStr.Assign(text.get(), contextLen / 2);
    }

    if (infoDataObj)
    {
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
  if (!nsEditorHookUtils::DoInsertionHook(domdoc, nullptr, trans))
    return NS_OK;

  return InsertFromTransferable(trans, nullptr, contextStr, infoStr,
                                nullptr, 0, true);
}

NS_IMETHODIMP nsHTMLEditor::PasteTransferable(nsITransferable *aTransferable)
{
  // Use an invalid value for the clipboard type as data comes from aTransferable
  // and we don't currently implement a way to put that in the data transfer yet.
  if (!FireClipboardEvent(NS_PASTE, nsIClipboard::kGlobalClipboard))
    return NS_OK;

  // handle transferable hooks
  nsCOMPtr<nsIDOMDocument> domdoc = GetDOMDocument();
  if (!nsEditorHookUtils::DoInsertionHook(domdoc, nullptr, aTransferable))
    return NS_OK;

  nsAutoString contextStr, infoStr;
  return InsertFromTransferable(aTransferable, nullptr, contextStr, infoStr,
                                nullptr, 0, true);
}

//
// HTML PasteNoFormatting. Ignore any HTML styles and formating in paste source
//
NS_IMETHODIMP nsHTMLEditor::PasteNoFormatting(int32_t aSelectionType)
{
  if (!FireClipboardEvent(NS_PASTE, aSelectionType))
    return NS_OK;

  ForceCompositionEnd();

  // Get Clipboard Service
  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard(do_GetService("@mozilla.org/widget/clipboard;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the nsITransferable interface for getting the data from the clipboard.
  // use nsPlaintextEditor::PrepareTransferable() to force unicode plaintext data.
  nsCOMPtr<nsITransferable> trans;
  rv = nsPlaintextEditor::PrepareTransferable(getter_AddRefs(trans));
  if (NS_SUCCEEDED(rv) && trans)
  {
    // Get the Data from the clipboard
    if (NS_SUCCEEDED(clipboard->GetData(trans, aSelectionType)) && IsModifiable())
    {
      const nsAFlatString& empty = EmptyString();
      rv = InsertFromTransferable(trans, nullptr, empty, empty, nullptr, 0,
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

NS_IMETHODIMP nsHTMLEditor::CanPaste(int32_t aSelectionType, bool *aCanPaste)
{
  NS_ENSURE_ARG_POINTER(aCanPaste);
  *aCanPaste = false;

  // can't paste if readonly
  if (!IsModifiable()) {
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard(do_GetService("@mozilla.org/widget/clipboard;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  bool haveFlavors;

  // Use the flavors depending on the current editor mask
  if (IsPlaintextEditor())
    rv = clipboard->HasDataMatchingFlavors(textEditorFlavors,
                                           ArrayLength(textEditorFlavors),
                                           aSelectionType, &haveFlavors);
  else
    rv = clipboard->HasDataMatchingFlavors(textHtmlEditorFlavors,
                                           ArrayLength(textHtmlEditorFlavors),
                                           aSelectionType, &haveFlavors);

  NS_ENSURE_SUCCESS(rv, rv);

  *aCanPaste = haveFlavors;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLEditor::CanPasteTransferable(nsITransferable *aTransferable, bool *aCanPaste)
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


//
// HTML PasteAsQuotation: Paste in a blockquote type=cite
//
NS_IMETHODIMP nsHTMLEditor::PasteAsQuotation(int32_t aSelectionType)
{
  if (IsPlaintextEditor())
    return PasteAsPlaintextQuotation(aSelectionType);

  nsAutoString citation;
  return PasteAsCitedQuotation(citation, aSelectionType);
}

NS_IMETHODIMP nsHTMLEditor::PasteAsCitedQuotation(const nsAString & aCitation,
                                                  int32_t aSelectionType)
{
  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, EditAction::insertQuotation, nsIEditor::eNext);

  // get selection
  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  // give rules a chance to handle or cancel
  nsTextRulesInfo ruleInfo(EditAction::insertElement);
  bool cancel, handled;
  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);
  nsresult rv = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
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

//
// Paste a plaintext quotation
//
NS_IMETHODIMP nsHTMLEditor::PasteAsPlaintextQuotation(int32_t aSelectionType)
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
  char* flav = 0;
  rv = trans->GetAnyTransferData(&flav, getter_AddRefs(genericDataObj), &len);
  NS_ENSURE_SUCCESS(rv, rv);

  if (flav && 0 == nsCRT::strcmp(flav, kUnicodeMime))
  {
    nsCOMPtr<nsISupportsString> textDataObj = do_QueryInterface(genericDataObj);
    if (textDataObj && len > 0)
    {
      nsAutoString stuffToPaste;
      textDataObj->GetData(stuffToPaste);
      NS_ASSERTION(stuffToPaste.Length() <= (len/2), "Invalid length!");
      nsAutoEditBatch beginBatching(this);
      rv = InsertAsPlaintextQuotation(stuffToPaste, true, 0);
    }
  }
  free(flav);

  return rv;
}

NS_IMETHODIMP
nsHTMLEditor::InsertTextWithQuotations(const nsAString &aStringToInsert)
{
  if (mWrapToWindow)
    return InsertText(aStringToInsert);

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
  if (FindCharInReadable('\r', dbgStart, strEnd))
    NS_ASSERTION(false,
            "Return characters in DOM! InsertTextWithQuotations may be wrong");
#endif /* DEBUG */

  // Loop over lines:
  nsresult rv = NS_OK;
  nsAString::const_iterator lineStart (hunkStart);
  while (1)   // we will break from inside when we run out of newlines
  {
    // Search for the end of this line (dom newlines, see above):
    bool found = FindCharInReadable('\n', lineStart, strEnd);
    bool quoted = false;
    if (found)
    {
      // if there's another newline, lineStart now points there.
      // Loop over any consecutive newline chars:
      nsAString::const_iterator firstNewline (lineStart);
      while (*lineStart == '\n')
        ++lineStart;
      quoted = (*lineStart == cite);
      if (quoted == curHunkIsQuoted)
        continue;
      // else we're changing state, so we need to insert
      // from curHunk to lineStart then loop around.

      // But if the current hunk is quoted, then we want to make sure
      // that any extra newlines on the end do not get included in
      // the quoted section: blank lines flaking a quoted section
      // should be considered unquoted, so that if the user clicks
      // there and starts typing, the new text will be outside of
      // the quoted block.
      if (curHunkIsQuoted)
        lineStart = firstNewline;
    }

    // If no newline found, lineStart is now strEnd and we can finish up,
    // inserting from curHunk to lineStart then returning.
    const nsAString &curHunk = Substring(hunkStart, lineStart);
    nsCOMPtr<nsIDOMNode> dummyNode;
    if (curHunkIsQuoted)
      rv = InsertAsPlaintextQuotation(curHunk, false,
                                      getter_AddRefs(dummyNode));
    else
      rv = InsertText(curHunk);

    if (!found)
      break;

    curHunkIsQuoted = quoted;
    hunkStart = lineStart;
  }

  EndTransaction();

  return rv;
}

NS_IMETHODIMP nsHTMLEditor::InsertAsQuotation(const nsAString & aQuotedText,
                                              nsIDOMNode **aNodeInserted)
{
  if (IsPlaintextEditor())
    return InsertAsPlaintextQuotation(aQuotedText, true, aNodeInserted);

  nsAutoString citation;
  return InsertAsCitedQuotation(aQuotedText, citation, false,
                                aNodeInserted);
}

// Insert plaintext as a quotation, with cite marks (e.g. "> ").
// This differs from its corresponding method in nsPlaintextEditor
// in that here, quoted material is enclosed in a <pre> tag
// in order to preserve the original line wrapping.
NS_IMETHODIMP
nsHTMLEditor::InsertAsPlaintextQuotation(const nsAString & aQuotedText,
                                         bool aAddCites,
                                         nsIDOMNode **aNodeInserted)
{
  if (mWrapToWindow)
    return nsPlaintextEditor::InsertAsQuotation(aQuotedText, aNodeInserted);

  // get selection
  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, EditAction::insertQuotation, nsIEditor::eNext);

  // give rules a chance to handle or cancel
  nsTextRulesInfo ruleInfo(EditAction::insertElement);
  bool cancel, handled;
  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);
  nsresult rv = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  NS_ENSURE_SUCCESS(rv, rv);
  if (cancel || handled) {
    return NS_OK; // rules canceled the operation
  }

  // Wrap the inserted quote in a <span> so it won't be wrapped:
  nsCOMPtr<Element> newNode =
    DeleteSelectionAndCreateElement(*nsGkAtoms::span);

  // If this succeeded, then set selection inside the pre
  // so the inserted text will end up there.
  // If it failed, we don't care what the return value was,
  // but we'll fall through and try to insert the text anyway.
  if (newNode) {
    // Add an attribute on the pre node so we'll know it's a quotation.
    // Do this after the insertion, so that
    newNode->SetAttr(kNameSpaceID_None, nsGkAtoms::mozquote,
                     NS_LITERAL_STRING("true"), true);
    // turn off wrapping on spans
    newNode->SetAttr(kNameSpaceID_None, nsGkAtoms::style,
                     NS_LITERAL_STRING("white-space: pre;"), true);

    // and set the selection inside it:
    selection->Collapse(newNode, 0);
  }

  if (aAddCites)
    rv = nsPlaintextEditor::InsertAsQuotation(aQuotedText, aNodeInserted);
  else
    rv = nsPlaintextEditor::InsertText(aQuotedText);
  // Note that if !aAddCites, aNodeInserted isn't set.
  // That's okay because the routines that use aAddCites
  // don't need to know the inserted node.

  if (aNodeInserted && NS_SUCCEEDED(rv))
  {
    *aNodeInserted = GetAsDOMNode(newNode);
    NS_IF_ADDREF(*aNodeInserted);
  }

  // Set the selection to just after the inserted node:
  if (NS_SUCCEEDED(rv) && newNode)
  {
    nsCOMPtr<nsINode> parent = newNode->GetParentNode();
    int32_t offset = parent ? parent->IndexOf(newNode) : -1;
    if (parent) {
      selection->Collapse(parent, offset + 1);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLEditor::StripCites()
{
  return nsPlaintextEditor::StripCites();
}

NS_IMETHODIMP
nsHTMLEditor::Rewrap(bool aRespectNewlines)
{
  return nsPlaintextEditor::Rewrap(aRespectNewlines);
}

NS_IMETHODIMP
nsHTMLEditor::InsertAsCitedQuotation(const nsAString & aQuotedText,
                                     const nsAString & aCitation,
                                     bool aInsertHTML,
                                     nsIDOMNode **aNodeInserted)
{
  // Don't let anyone insert html into a "plaintext" editor:
  if (IsPlaintextEditor())
  {
    NS_ASSERTION(!aInsertHTML, "InsertAsCitedQuotation: trying to insert html into plaintext editor");
    return InsertAsPlaintextQuotation(aQuotedText, true, aNodeInserted);
  }

  // get selection
  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, EditAction::insertQuotation, nsIEditor::eNext);

  // give rules a chance to handle or cancel
  nsTextRulesInfo ruleInfo(EditAction::insertElement);
  bool cancel, handled;
  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);
  nsresult rv = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
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

  if (aInsertHTML)
    rv = LoadHTML(aQuotedText);
  else
    rv = InsertText(aQuotedText);  // XXX ignore charset

  if (aNodeInserted && NS_SUCCEEDED(rv))
  {
    *aNodeInserted = GetAsDOMNode(newNode);
    NS_IF_ADDREF(*aNodeInserted);
  }

  // Set the selection to just after the inserted node:
  if (NS_SUCCEEDED(rv) && newNode)
  {
    nsCOMPtr<nsINode> parent = newNode->GetParentNode();
    int32_t offset = parent ? parent->IndexOf(newNode) : -1;
    if (parent) {
      selection->Collapse(parent, offset + 1);
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

  if (!child)
  {
    // If the current result is nullptr, then aStart is a leaf, and is the
    // fallback result.
    if (!aResult)
      aResult = aStart;

    return NS_OK;
  }

  do
  {
    // Is this child the magical cookie?
    nsCOMPtr<nsIDOMComment> comment = do_QueryInterface(child);
    if (comment)
    {
      nsAutoString data;
      rv = comment->GetData(data);
      NS_ENSURE_SUCCESS(rv, rv);

      if (data.EqualsLiteral(kInsertCookie))
      {
        // Yes it is! Return an error so we bubble out and short-circuit the
        // search.
        aResult = aStart;

        // Note: it doesn't matter if this fails.
        aStart->RemoveChild(child, getter_AddRefs(tmp));

        return NS_FOUND_TARGET;
      }
    }

    // Note: Don't use NS_ENSURE_* here since we return a failure result to
    // inicate that we found the magical cookie and we don't want to spam the
    // console.
    rv = FindTargetNode(child, aResult);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = child->GetNextSibling(getter_AddRefs(tmp));
    NS_ENSURE_SUCCESS(rv, rv);

    child = tmp;
  } while (child);

  return NS_OK;
}

nsresult nsHTMLEditor::CreateDOMFragmentFromPaste(const nsAString &aInputString,
                                                  const nsAString & aContextStr,
                                                  const nsAString & aInfoStr,
                                                  nsCOMPtr<nsIDOMNode> *outFragNode,
                                                  nsCOMPtr<nsIDOMNode> *outStartNode,
                                                  nsCOMPtr<nsIDOMNode> *outEndNode,
                                                  int32_t *outStartOffset,
                                                  int32_t *outEndOffset,
                                                  bool aTrustedInput)
{
  NS_ENSURE_TRUE(outFragNode && outStartNode && outEndNode, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDocument> doc = GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  // if we have context info, create a fragment for that
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDOMNode> contextLeaf;
  nsRefPtr<DocumentFragment> contextAsNode;
  if (!aContextStr.IsEmpty()) {
    rv = ParseFragment(aContextStr, nullptr, doc, getter_AddRefs(contextAsNode),
                       aTrustedInput);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(contextAsNode, NS_ERROR_FAILURE);

    rv = StripFormattingNodes(*contextAsNode);
    NS_ENSURE_SUCCESS(rv, rv);

    RemoveBodyAndHead(*contextAsNode);

    rv = FindTargetNode(contextAsNode, contextLeaf);
    if (rv == NS_FOUND_TARGET) {
      rv = NS_OK;
    }
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIContent> contextLeafAsContent = do_QueryInterface(contextLeaf);

  // create fragment for pasted html
  nsIAtom* contextAtom;
  if (contextLeafAsContent) {
    contextAtom = contextLeafAsContent->NodeInfo()->NameAtom();
    if (contextLeafAsContent->IsHTMLElement(nsGkAtoms::html)) {
      contextAtom = nsGkAtoms::body;
    }
  } else {
    contextAtom = nsGkAtoms::body;
  }
  nsRefPtr<DocumentFragment> fragment;
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
    nsCOMPtr<nsIDOMNode> junk;
    contextLeaf->AppendChild(fragment, getter_AddRefs(junk));
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


nsresult nsHTMLEditor::ParseFragment(const nsAString & aFragStr,
                                     nsIAtom* aContextLocalName,
                                     nsIDocument* aTargetDocument,
                                     DocumentFragment** aFragment,
                                     bool aTrustedInput)
{
  nsAutoScriptBlockerSuppressNodeRemoved autoBlocker;

  nsRefPtr<DocumentFragment> fragment =
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
nsHTMLEditor::CreateListOfNodesToPaste(DocumentFragment& aFragment,
                                       nsTArray<OwningNonNull<nsINode>>& outNodeList,
                                       nsINode* aStartNode,
                                       int32_t aStartOffset,
                                       nsINode* aEndNode,
                                       int32_t aEndOffset)
{
  // If no info was provided about the boundary between context and stream,
  // then assume all is stream.
  if (!aStartNode) {
    aStartNode = &aFragment;
    aStartOffset = 0;
    aEndNode = &aFragment;
    aEndOffset = aFragment.Length();
  }

  nsRefPtr<nsRange> docFragRange;
  nsresult rv = nsRange::CreateRange(aStartNode, aStartOffset,
                                     aEndNode, aEndOffset,
                                     getter_AddRefs(docFragRange));
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  NS_ENSURE_SUCCESS(rv, );

  // Now use a subtree iterator over the range to create a list of nodes
  nsTrivialFunctor functor;
  nsDOMSubtreeIterator iter;
  rv = iter.Init(*docFragRange);
  NS_ENSURE_SUCCESS(rv, );
  iter.AppendList(functor, outNodeList);
}

void
nsHTMLEditor::GetListAndTableParents(StartOrEnd aStartOrEnd,
                                     nsTArray<OwningNonNull<nsINode>>& aNodeList,
                                     nsTArray<OwningNonNull<Element>>& outArray)
{
  MOZ_ASSERT(aNodeList.Length());

  // Build up list of parents of first (or last) node in list that are either
  // lists, or tables.
  int32_t idx = aStartOrEnd == StartOrEnd::end ? aNodeList.Length() - 1 : 0;

  for (nsCOMPtr<nsINode> node = aNodeList[idx]; node;
       node = node->GetParentNode()) {
    if (nsHTMLEditUtils::IsList(node) || nsHTMLEditUtils::IsTable(node)) {
      outArray.AppendElement(*node->AsElement());
    }
  }
}

int32_t
nsHTMLEditor::DiscoverPartialListsAndTables(nsTArray<OwningNonNull<nsINode>>& aPasteNodes,
                                            nsTArray<OwningNonNull<Element>>& aListsAndTables)
{
  int32_t ret = -1;
  int32_t listAndTableParents = aListsAndTables.Length();

  // Scan insertion list for table elements (other than table).
  for (auto& curNode : aPasteNodes) {
    if (nsHTMLEditUtils::IsTableElement(curNode) &&
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
    if (nsHTMLEditUtils::IsListItem(curNode)) {
      nsCOMPtr<Element> list = curNode->GetParentElement();
      while (list && !nsHTMLEditUtils::IsList(list)) {
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
nsHTMLEditor::ScanForListAndTableStructure(StartOrEnd aStartOrEnd,
                                           nsTArray<OwningNonNull<nsINode>>& aNodes,
                                           Element& aListOrTable)
{
  // Look upward from first/last paste node for a piece of this list/table
  int32_t idx = aStartOrEnd == StartOrEnd::end ? aNodes.Length() - 1 : 0;
  bool isList = nsHTMLEditUtils::IsList(&aListOrTable);

  for (nsCOMPtr<nsINode> node = aNodes[idx]; node;
       node = node->GetParentNode()) {
    if ((isList && nsHTMLEditUtils::IsListItem(node)) ||
        (!isList && nsHTMLEditUtils::IsTableElement(node) &&
                    !node->IsHTMLElement(nsGkAtoms::table))) {
      nsCOMPtr<Element> structureNode = node->GetParentElement();
      if (isList) {
        while (structureNode && !nsHTMLEditUtils::IsList(structureNode)) {
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
nsHTMLEditor::ReplaceOrphanedStructure(StartOrEnd aStartOrEnd,
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
  while (aNodeArray.Length()) {
    int32_t idx = aStartOrEnd == StartOrEnd::start ? 0
                                                   : aNodeArray.Length() - 1;
    OwningNonNull<nsINode> endpoint = aNodeArray[idx];
    if (!nsEditorUtils::IsDescendantOf(endpoint, replaceNode)) {
      break;
    }
    aNodeArray.RemoveElementAt(idx);
  }

  // Now replace the removed nodes with the structural parent
  if (aStartOrEnd == StartOrEnd::end) {
    aNodeArray.AppendElement(*replaceNode);
  } else {
    aNodeArray.InsertElementAt(0, *replaceNode);
  }
}
