/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "nsHTMLEditor.h"
#include "nsHTMLEditRules.h"
#include "nsTextEditUtils.h"
#include "nsHTMLEditUtils.h"
#include "nsWSRunObject.h"

#include "nsIDOMText.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocument.h"
#include "nsIDOMAttr.h"
#include "nsIDocument.h"
#include "nsIDOMEventTarget.h" 
#include "nsIDOMNSEvent.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMKeyListener.h" 
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMComment.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsISelectionController.h"
#include "nsIFileChannel.h"

#include "nsIDocumentObserver.h"
#include "nsIDocumentStateListener.h"

#include "nsIEnumerator.h"
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsIDOMRange.h"
#include "nsIDOMNSRange.h"
#include "nsCOMArray.h"
#include "nsIFile.h"
#include "nsIURL.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIDocumentEncoder.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsLinebreakConverter.h"
#include "nsIFragmentContentSink.h"
#include "nsIContentSink.h"

// netwerk
#include "nsIURI.h"
#include "nsNetUtil.h"

// Drag & Drop, Clipboard
#include "nsIClipboard.h"
#include "nsITransferable.h"
#include "nsIDragService.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIOutputStream.h"
#include "nsIInputStream.h"
#include "nsDirectoryServiceDefs.h"

// for relativization
#include "nsUnicharUtils.h"
#include "nsIDOMDocumentTraversal.h"
#include "nsIDOMTreeWalker.h"
#include "nsIDOMNodeFilter.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMHTMLLinkElement.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsIDOMHTMLFrameElement.h"
#include "nsIDOMHTMLIFrameElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLScriptElement.h"
#include "nsIDOMHTMLEmbedElement.h"
#include "nsIDOMHTMLTableCellElement.h"
#include "nsIDOMHTMLTableRowElement.h"
#include "nsIDOMHTMLTableElement.h"
#include "nsIDOMHTMLBodyElement.h"

// Misc
#include "TextEditorTest.h"
#include "nsEditorUtils.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIContentFilter.h"
#include "nsEventDispatcher.h"
#include "plbase64.h"
#include "prmem.h"
#include "nsStreamUtils.h"
#include "nsIPrincipal.h"

const PRUnichar nbsp = 160;

static NS_DEFINE_CID(kCParserCID,     NS_PARSER_CID);

// private clipboard data flavors for html copy/paste
#define kHTMLContext   "text/_moz_htmlcontext"
#define kHTMLInfo      "text/_moz_htmlinfo"
#define kInsertCookie  "_moz_Insert Here_moz_"

#define NS_FOUND_TARGET NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_EDITOR, 3)

// some little helpers
static PRInt32 FindPositiveIntegerAfterString(const char *aLeadingString, nsCString &aCStr);
static nsresult RemoveFragComments(nsCString &theStr);
static void RemoveBodyAndHead(nsIDOMNode *aNode);
static nsresult FindTargetNode(nsIDOMNode *aStart, nsCOMPtr<nsIDOMNode> &aResult);

static nsCOMPtr<nsIDOMNode> GetListParent(nsIDOMNode* aNode)
{
  NS_ENSURE_TRUE(aNode, nsnull);
  nsCOMPtr<nsIDOMNode> parent, tmp;
  aNode->GetParentNode(getter_AddRefs(parent));
  while (parent)
  {
    if (nsHTMLEditUtils::IsList(parent)) return parent;
    parent->GetParentNode(getter_AddRefs(tmp));
    parent = tmp;
  }
  return nsnull;
}

static nsCOMPtr<nsIDOMNode> GetTableParent(nsIDOMNode* aNode)
{
  NS_ENSURE_TRUE(aNode, nsnull);
  nsCOMPtr<nsIDOMNode> parent, tmp;
  aNode->GetParentNode(getter_AddRefs(parent));
  while (parent)
  {
    if (nsHTMLEditUtils::IsTable(parent)) return parent;
    parent->GetParentNode(getter_AddRefs(tmp));
    parent = tmp;
  }
  return nsnull;
}


NS_IMETHODIMP nsHTMLEditor::LoadHTML(const nsAString & aInputString)
{
  NS_ENSURE_TRUE(mRules, NS_ERROR_NOT_INITIALIZED);

  // force IME commit; set up rules sniffing and batching
  ForceCompositionEnd();
  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, kOpLoadHTML, nsIEditor::eNext);
  
  // Get selection
  nsCOMPtr<nsISelection>selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  
  nsTextRulesInfo ruleInfo(nsTextEditRules::kLoadHTML);
  PRBool cancel, handled;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  NS_ENSURE_SUCCESS(res, res);
  if (cancel) return NS_OK; // rules canceled the operation
  if (!handled)
  {
    PRBool isCollapsed;
    res = selection->GetIsCollapsed(&isCollapsed);
    NS_ENSURE_SUCCESS(res, res);

    // Delete Selection, but only if it isn't collapsed, see bug #106269
    if (!isCollapsed) 
    {
      res = DeleteSelection(eNone);
      NS_ENSURE_SUCCESS(res, res);
    }

    // Get the first range in the selection, for context:
    nsCOMPtr<nsIDOMRange> range;
    res = selection->GetRangeAt(0, getter_AddRefs(range));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(range, NS_ERROR_NULL_POINTER);
    nsCOMPtr<nsIDOMNSRange> nsrange (do_QueryInterface(range));
    NS_ENSURE_TRUE(nsrange, NS_ERROR_NO_INTERFACE);

    // create fragment for pasted html
    nsCOMPtr<nsIDOMDocumentFragment> docfrag;
    {
      res = nsrange->CreateContextualFragment(aInputString, getter_AddRefs(docfrag));
      NS_ENSURE_SUCCESS(res, res);
    }
    // put the fragment into the document
    nsCOMPtr<nsIDOMNode> parent, junk;
    res = range->GetStartContainer(getter_AddRefs(parent));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);
    PRInt32 childOffset;
    res = range->GetStartOffset(&childOffset);
    NS_ENSURE_SUCCESS(res, res);

    nsCOMPtr<nsIDOMNode> nodeToInsert;
    docfrag->GetFirstChild(getter_AddRefs(nodeToInsert));
    while (nodeToInsert)
    {
      res = InsertNode(nodeToInsert, parent, childOffset++);
      NS_ENSURE_SUCCESS(res, res);
      docfrag->GetFirstChild(getter_AddRefs(nodeToInsert));
    }
  }

  return mRules->DidDoAction(selection, &ruleInfo, res);
}


NS_IMETHODIMP nsHTMLEditor::InsertHTML(const nsAString & aInString)
{
  const nsAFlatString& empty = EmptyString();

  return InsertHTMLWithContext(aInString, empty, empty, empty,
                               nsnull,  nsnull, 0, PR_TRUE);
}


nsresult
nsHTMLEditor::InsertHTMLWithContext(const nsAString & aInputString,
                                    const nsAString & aContextStr,
                                    const nsAString & aInfoStr,
                                    const nsAString & aFlavor,
                                    nsIDOMDocument *aSourceDoc,
                                    nsIDOMNode *aDestNode,
                                    PRInt32 aDestOffset,
                                    PRBool aDeleteSelection)
{
  return DoInsertHTMLWithContext(aInputString, aContextStr, aInfoStr,
      aFlavor, aSourceDoc, aDestNode, aDestOffset, aDeleteSelection,
      PR_TRUE);
}

nsresult
nsHTMLEditor::DoInsertHTMLWithContext(const nsAString & aInputString,
                                      const nsAString & aContextStr,
                                      const nsAString & aInfoStr,
                                      const nsAString & aFlavor,
                                      nsIDOMDocument *aSourceDoc,
                                      nsIDOMNode *aDestNode,
                                      PRInt32 aDestOffset,
                                      PRBool aDeleteSelection,
                                      PRBool aTrustedInput)
{
  NS_ENSURE_TRUE(mRules, NS_ERROR_NOT_INITIALIZED);

  // force IME commit; set up rules sniffing and batching
  ForceCompositionEnd();
  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, kOpHTMLPaste, nsIEditor::eNext);
  
  // Get selection
  nsresult res;
  nsCOMPtr<nsISelection>selection;
  res = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  
  // create a dom document fragment that represents the structure to paste
  nsCOMPtr<nsIDOMNode> fragmentAsNode, streamStartParent, streamEndParent;
  PRInt32 streamStartOffset = 0, streamEndOffset = 0;

  res = CreateDOMFragmentFromPaste(aInputString, aContextStr, aInfoStr, 
                                   address_of(fragmentAsNode),
                                   address_of(streamStartParent),
                                   address_of(streamEndParent),
                                   &streamStartOffset,
                                   &streamEndOffset,
                                   aTrustedInput);
  NS_ENSURE_SUCCESS(res, res);

  nsCOMPtr<nsIDOMNode> targetNode, tempNode;
  PRInt32 targetOffset=0;

  if (!aDestNode)
  {
    // if caller didn't provide the destination/target node,
    // fetch the paste insertion point from our selection
    res = GetStartNodeAndOffset(selection, getter_AddRefs(targetNode), &targetOffset);
    if (!targetNode) res = NS_ERROR_FAILURE;
    NS_ENSURE_SUCCESS(res, res);
  }
  else
  {
    targetNode = aDestNode;
    targetOffset = aDestOffset;
  }

  PRBool doContinue = PR_TRUE;

  res = DoContentFilterCallback(aFlavor, aSourceDoc, aDeleteSelection,
                                (nsIDOMNode **)address_of(fragmentAsNode), 
                                (nsIDOMNode **)address_of(streamStartParent), 
                                &streamStartOffset,
                                (nsIDOMNode **)address_of(streamEndParent),
                                &streamEndOffset, 
                                (nsIDOMNode **)address_of(targetNode), 
                                &targetOffset, &doContinue);

  NS_ENSURE_SUCCESS(res, res);
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
      res = DeleteSelection(eNone);
      NS_ENSURE_SUCCESS(res, res);
    }

    res = selection->Collapse(targetNode, targetOffset);
    NS_ENSURE_SUCCESS(res, res);
  }

  // we need to recalculate various things based on potentially new offsets
  // this is work to be completed at a later date (probably by jfrancis)

  // make a list of what nodes in docFrag we need to move
  nsCOMArray<nsIDOMNode> nodeList;
  res = CreateListOfNodesToPaste(fragmentAsNode, nodeList,
                                 streamStartParent, streamStartOffset,
                                 streamEndParent, streamEndOffset);
  NS_ENSURE_SUCCESS(res, res);

  if (nodeList.Count() == 0)
    return NS_OK;

  // walk list of nodes; perform surgery on nodes (relativize) with _mozattr
  res = RelativizeURIInFragmentList(nodeList, aFlavor, aSourceDoc, targetNode);
  // ignore results from this call, try to paste/insert anyways
 
  // are there any table elements in the list?  
  // node and offset for insertion
  nsCOMPtr<nsIDOMNode> parentNode;
  PRInt32 offsetOfNewNode;
  
  // check for table cell selection mode
  PRBool cellSelectionMode = PR_FALSE;
  nsCOMPtr<nsIDOMElement> cell;
  res = GetFirstSelectedCell(nsnull, getter_AddRefs(cell));
  if (NS_SUCCEEDED(res) && cell)
  {
    cellSelectionMode = PR_TRUE;
  }
  
  if (cellSelectionMode)
  {
    // do we have table content to paste?  If so, we want to delete
    // the selected table cells and replace with new table elements;
    // but if not we want to delete _contents_ of cells and replace
    // with non-table elements.  Use cellSelectionMode bool to 
    // indicate results.
    nsIDOMNode* firstNode = nodeList[0];
    if (!nsHTMLEditUtils::IsTableElement(firstNode))
      cellSelectionMode = PR_FALSE;
  }

  if (!cellSelectionMode)
  {
    res = DeleteSelectionAndPrepareToCreateNode(parentNode, offsetOfNewNode);
    NS_ENSURE_SUCCESS(res, res);

    // pasting does not inherit local inline styles
    res = RemoveAllInlineProperties();
    NS_ENSURE_SUCCESS(res, res);
  }
  else
  {
    // delete whole cells: we will replace with new table content
    if (1)
    {
      // Save current selection since DeleteTableCell perturbs it
      nsAutoSelectionReset selectionResetter(selection, this);
      res = DeleteTableCell(1);
      NS_ENSURE_SUCCESS(res, res);
    }
    // collapse selection to beginning of deleted table content
    selection->CollapseToStart();
  }
  
  // give rules a chance to handle or cancel
  nsTextRulesInfo ruleInfo(nsTextEditRules::kInsertElement);
  PRBool cancel, handled;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  NS_ENSURE_SUCCESS(res, res);
  if (cancel) return NS_OK; // rules canceled the operation
  if (!handled)
  {
    // The rules code (WillDoAction above) might have changed the selection.  
    // refresh our memory...
    res = GetStartNodeAndOffset(selection, getter_AddRefs(parentNode), &offsetOfNewNode);
    if (!parentNode) res = NS_ERROR_FAILURE;
    NS_ENSURE_SUCCESS(res, res);

    // Adjust position based on the first node we are going to insert.
    NormalizeEOLInsertPosition(nodeList[0], address_of(parentNode), &offsetOfNewNode);

    // if there are any invisible br's after our insertion point, remove them.
    // this is because if there is a br at end of what we paste, it will make
    // the invisible br visible.
    nsWSRunObject wsObj(this, parentNode, offsetOfNewNode);
    if (nsTextEditUtils::IsBreak(wsObj.mEndReasonNode) && 
        !IsVisBreak(wsObj.mEndReasonNode) )
    {
      res = DeleteNode(wsObj.mEndReasonNode);
      NS_ENSURE_SUCCESS(res, res);
    }
    
    // remeber if we are in a link.  
    PRBool bStartedInLink = IsInLink(parentNode);
  
    // are we in a text node?  If so, split it.
    if (IsTextNode(parentNode))
    {
      nsCOMPtr<nsIDOMNode> temp;
      res = SplitNodeDeep(parentNode, parentNode, offsetOfNewNode, &offsetOfNewNode);
      NS_ENSURE_SUCCESS(res, res);
      res = parentNode->GetParentNode(getter_AddRefs(temp));
      NS_ENSURE_SUCCESS(res, res);
      parentNode = temp;
    }

    // build up list of parents of first node in list that are either
    // lists or tables.  First examine front of paste node list.
    nsCOMArray<nsIDOMNode> startListAndTableArray;
    res = GetListAndTableParents(PR_FALSE, nodeList, startListAndTableArray);
    NS_ENSURE_SUCCESS(res, res);
    
    // remember number of lists and tables above us
    PRInt32 highWaterMark = -1;
    if (startListAndTableArray.Count() > 0)
    {
      res = DiscoverPartialListsAndTables(nodeList, startListAndTableArray, &highWaterMark);
      NS_ENSURE_SUCCESS(res, res);
    }

    // if we have pieces of tables or lists to be inserted, let's force the paste 
    // to deal with table elements right away, so that it doesn't orphan some 
    // table or list contents outside the table or list.
    if (highWaterMark >= 0)
    {
      res = ReplaceOrphanedStructure(PR_FALSE, nodeList, startListAndTableArray, highWaterMark);
      NS_ENSURE_SUCCESS(res, res);
    }
    
    // Now go through the same process again for the end of the paste node list.
    nsCOMArray<nsIDOMNode> endListAndTableArray;
    res = GetListAndTableParents(PR_TRUE, nodeList, endListAndTableArray);
    NS_ENSURE_SUCCESS(res, res);
    highWaterMark = -1;
   
    // remember number of lists and tables above us
    if (endListAndTableArray.Count() > 0)
    {
      res = DiscoverPartialListsAndTables(nodeList, endListAndTableArray, &highWaterMark);
      NS_ENSURE_SUCCESS(res, res);
    }
    
    // don't orphan partial list or table structure
    if (highWaterMark >= 0)
    {
      res = ReplaceOrphanedStructure(PR_TRUE, nodeList, endListAndTableArray, highWaterMark);
      NS_ENSURE_SUCCESS(res, res);
    }

    // Loop over the node list and paste the nodes:
    nsCOMPtr<nsIDOMNode> parentBlock, lastInsertNode, insertedContextParent;
    PRInt32 listCount = nodeList.Count();
    PRInt32 j;
    if (IsBlockNode(parentNode))
      parentBlock = parentNode;
    else
      parentBlock = GetBlockNodeParent(parentNode);
      
    for (j=0; j<listCount; j++)
    {
      PRBool bDidInsert = PR_FALSE;
      nsCOMPtr<nsIDOMNode> curNode = nodeList[j];

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
          res = InsertNodeAtPoint(child, address_of(parentNode), &offsetOfNewNode, PR_TRUE);
          if (NS_FAILED(res))
            break;

          bDidInsert = PR_TRUE;
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
            // check if we are pasting into empty list item. If so
            // delete it and paste into parent list instead.
            if (nsHTMLEditUtils::IsListItem(parentNode))
            {
              PRBool isEmpty;
              res = IsEmptyNode(parentNode, &isEmpty, PR_TRUE);
              if ((NS_SUCCEEDED(res)) && isEmpty)
              {
                nsCOMPtr<nsIDOMNode> listNode;
                PRInt32 newOffset;
                GetNodeLocation(parentNode, address_of(listNode), &newOffset);
                if (listNode)
                {
                  DeleteNode(parentNode);
                  parentNode = listNode;
                  offsetOfNewNode = newOffset;
                }
              }
            } 
            res = InsertNodeAtPoint(child, address_of(parentNode), &offsetOfNewNode, PR_TRUE);
            if (NS_FAILED(res))
              break;

            bDidInsert = PR_TRUE;
            lastInsertNode = child;
            offsetOfNewNode++;
          }
          else
          {
            curNode->RemoveChild(child, getter_AddRefs(tmp));
          }
          curNode->GetFirstChild(getter_AddRefs(child));
        }
        
      }
      // check for pre's going into pre's.  
      else if (nsHTMLEditUtils::IsPre(parentBlock) && nsHTMLEditUtils::IsPre(curNode))
      {
        nsCOMPtr<nsIDOMNode> child, tmp;
        curNode->GetFirstChild(getter_AddRefs(child));
        while (child)
        {
          res = InsertNodeAtPoint(child, address_of(parentNode), &offsetOfNewNode, PR_TRUE);
          if (NS_FAILED(res))
            break;

          bDidInsert = PR_TRUE;
          lastInsertNode = child;
          offsetOfNewNode++;

          curNode->GetFirstChild(getter_AddRefs(child));
        }
      }

      if (!bDidInsert || NS_FAILED(res))
      {
        // try to insert
        res = InsertNodeAtPoint(curNode, address_of(parentNode), &offsetOfNewNode, PR_TRUE);
        if (NS_SUCCEEDED(res)) 
        {
          bDidInsert = PR_TRUE;
          lastInsertNode = curNode;
        }

        // Assume failure means no legal parent in the document hierarchy,
        // try again with the parent of curNode in the paste hierarchy.
        nsCOMPtr<nsIDOMNode> parent;
        while (NS_FAILED(res) && curNode)
        {
          curNode->GetParentNode(getter_AddRefs(parent));
          if (parent && !nsTextEditUtils::IsBody(parent))
          {
            res = InsertNodeAtPoint(parent, address_of(parentNode), &offsetOfNewNode, PR_TRUE);
            if (NS_SUCCEEDED(res)) 
            {
              bDidInsert = PR_TRUE;
              insertedContextParent = parent;
              lastInsertNode = parent;
            }
          }
          curNode = parent;
        }
      }
      if (lastInsertNode)
      {
        res = GetNodeLocation(lastInsertNode, address_of(parentNode), &offsetOfNewNode);
        NS_ENSURE_SUCCESS(res, res);
        offsetOfNewNode++;
      }
    }

    // Now collapse the selection to the end of what we just inserted:
    if (lastInsertNode) 
    {
      // set selection to the end of what we just pasted.
      nsCOMPtr<nsIDOMNode> selNode, tmp, visNode, highTable;
      PRInt32 selOffset;
      
      // but don't cross tables
      if (!nsHTMLEditUtils::IsTable(lastInsertNode))
      {
        res = GetLastEditableLeaf(lastInsertNode, address_of(selNode));
        NS_ENSURE_SUCCESS(res, res);
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
        res = GetLengthOfDOMNode(selNode, (PRUint32&)selOffset);
        NS_ENSURE_SUCCESS(res, res);
      }
      else // we need to find a container for selection.  Look up.
      {
        tmp = selNode;
        res = GetNodeLocation(tmp, address_of(selNode), &selOffset);
        ++selOffset;  // want to be *after* last leaf node in paste
        NS_ENSURE_SUCCESS(res, res);
      }
      
      // make sure we don't end up with selection collapsed after an invisible break node
      nsWSRunObject wsRunObj(this, selNode, selOffset);
      PRInt32 outVisOffset=0;
      PRInt16 visType=0;
      res = wsRunObj.PriorVisibleNode(selNode, selOffset, address_of(visNode), &outVisOffset, &visType);
      NS_ENSURE_SUCCESS(res, res);
      if (visType == nsWSRunObject::eBreak)
      {
        // we are after a break.  Is it visible?  Despite the name, 
        // PriorVisibleNode does not make that determination for breaks.
        // It also may not return the break in visNode.  We have to pull it
        // out of the nsWSRunObject's state.
        if (!IsVisBreak(wsRunObj.mStartReasonNode))
        {
          // don't leave selection past an invisible break;
          // reset {selNode,selOffset} to point before break
          res = GetNodeLocation(wsRunObj.mStartReasonNode, address_of(selNode), &selOffset);
          // we want to be inside any inline style prior to break
          nsWSRunObject wsRunObj(this, selNode, selOffset);
          res = wsRunObj.PriorVisibleNode(selNode, selOffset, address_of(visNode), &outVisOffset, &visType);
          NS_ENSURE_SUCCESS(res, res);
          if (visType == nsWSRunObject::eText ||
              visType == nsWSRunObject::eNormalWS)
          {
            selNode = visNode;
            selOffset = outVisOffset;  // PriorVisibleNode already set offset to _after_ the text or ws
          }
          else if (visType == nsWSRunObject::eSpecial)
          {
            // prior visible thing is an image or some other non-text thingy.  
            // We want to be right after it.
            res = GetNodeLocation(wsRunObj.mStartReasonNode, address_of(selNode), &selOffset);
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
        PRInt32 linkOffset;
        res = SplitNodeDeep(link, selNode, selOffset, &linkOffset, PR_TRUE, address_of(leftLink));
        NS_ENSURE_SUCCESS(res, res);
        res = GetNodeLocation(leftLink, address_of(selNode), &selOffset);
        NS_ENSURE_SUCCESS(res, res);
        selection->Collapse(selNode, selOffset+1);
      }
    }
  }
  
  res = mRules->DidDoAction(selection, &ruleInfo, res);
  return res;
}

// returns empty string if nothing to modify on node
nsresult
nsHTMLEditor::GetAttributeToModifyOnNode(nsIDOMNode *aNode, nsAString &aAttr)
{
  aAttr.Truncate();

  NS_NAMED_LITERAL_STRING(srcStr, "src");
  nsCOMPtr<nsIDOMHTMLImageElement> nodeAsImage = do_QueryInterface(aNode);
  if (nodeAsImage)
  {
    aAttr = srcStr;
    return NS_OK;
  }

  nsCOMPtr<nsIDOMHTMLAnchorElement> nodeAsAnchor = do_QueryInterface(aNode);
  if (nodeAsAnchor)
  {
    aAttr.AssignLiteral("href");
    return NS_OK;
  }

  NS_NAMED_LITERAL_STRING(bgStr, "background");
  nsCOMPtr<nsIDOMHTMLBodyElement> nodeAsBody = do_QueryInterface(aNode);
  if (nodeAsBody)
  {
    aAttr = bgStr;
    return NS_OK;
  }

  nsCOMPtr<nsIDOMHTMLTableElement> nodeAsTable = do_QueryInterface(aNode);
  if (nodeAsTable)
  {
    aAttr = bgStr;
    return NS_OK;
  }

  nsCOMPtr<nsIDOMHTMLTableRowElement> nodeAsTableRow = do_QueryInterface(aNode);
  if (nodeAsTableRow)
  {
    aAttr = bgStr;
    return NS_OK;
  }

  nsCOMPtr<nsIDOMHTMLTableCellElement> nodeAsTableCell = do_QueryInterface(aNode);
  if (nodeAsTableCell)
  {
    aAttr = bgStr;
    return NS_OK;
  }

  nsCOMPtr<nsIDOMHTMLScriptElement> nodeAsScript = do_QueryInterface(aNode);
  if (nodeAsScript)
  {
    aAttr = srcStr;
    return NS_OK;
  }
  
  nsCOMPtr<nsIDOMHTMLEmbedElement> nodeAsEmbed = do_QueryInterface(aNode);
  if (nodeAsEmbed)
  {
    aAttr = srcStr;
    return NS_OK;
  }
  
  nsCOMPtr<nsIDOMHTMLObjectElement> nodeAsObject = do_QueryInterface(aNode);
  if (nodeAsObject)
  {
    aAttr.AssignLiteral("data");
    return NS_OK;
  }
  
  nsCOMPtr<nsIDOMHTMLLinkElement> nodeAsLink = do_QueryInterface(aNode);
  if (nodeAsLink)
  {
    // Test if the link has a rel value indicating it to be a stylesheet
    nsAutoString linkRel;
    if (NS_SUCCEEDED(nodeAsLink->GetRel(linkRel)) && !linkRel.IsEmpty())
    {
      nsReadingIterator<PRUnichar> start;
      nsReadingIterator<PRUnichar> end;
      nsReadingIterator<PRUnichar> current;

      linkRel.BeginReading(start);
      linkRel.EndReading(end);

      // Walk through space delimited string looking for "stylesheet"
      for (current = start; current != end; ++current)
      {
        // Ignore whitespace
        if (nsCRT::IsAsciiSpace(*current))
          continue;

        // Grab the next space delimited word
        nsReadingIterator<PRUnichar> startWord = current;
        do {
          ++current;
        } while (current != end && !nsCRT::IsAsciiSpace(*current));

        // Store the link for fix up if it says "stylesheet"
        if (Substring(startWord, current).LowerCaseEqualsLiteral("stylesheet"))
        {
          aAttr.AssignLiteral("href");
          return NS_OK;
        }
        if (current == end)
          break;
      }
    }
    return NS_OK;
  }

  nsCOMPtr<nsIDOMHTMLFrameElement> nodeAsFrame = do_QueryInterface(aNode);
  if (nodeAsFrame)
  {
    aAttr = srcStr;
    return NS_OK;
  }

  nsCOMPtr<nsIDOMHTMLIFrameElement> nodeAsIFrame = do_QueryInterface(aNode);
  if (nodeAsIFrame)
  {
    aAttr = srcStr;
    return NS_OK;
  }

  nsCOMPtr<nsIDOMHTMLInputElement> nodeAsInput = do_QueryInterface(aNode);
  if (nodeAsInput)
  {
    aAttr = srcStr;
    return NS_OK;
  }

  return NS_OK;
}

nsresult
nsHTMLEditor::RelativizeURIForNode(nsIDOMNode *aNode, nsIURL *aDestURL)
{
  nsAutoString attributeToModify;
  GetAttributeToModifyOnNode(aNode, attributeToModify);
  if (attributeToModify.IsEmpty())
    return NS_OK;

  nsCOMPtr<nsIDOMNamedNodeMap> attrMap;
  nsresult rv = aNode->GetAttributes(getter_AddRefs(attrMap));
  NS_ENSURE_SUCCESS(rv, NS_OK);
  NS_ENSURE_TRUE(attrMap, NS_OK); // assume errors here shouldn't cancel insertion

  nsCOMPtr<nsIDOMNode> attrNode;
  rv = attrMap->GetNamedItem(attributeToModify, getter_AddRefs(attrNode));
  NS_ENSURE_SUCCESS(rv, NS_OK); // assume errors here shouldn't cancel insertion

  if (attrNode)
  {
    nsAutoString oldValue;
    attrNode->GetNodeValue(oldValue);
    if (!oldValue.IsEmpty())
    {
      NS_ConvertUTF16toUTF8 oldCValue(oldValue);
      nsCOMPtr<nsIURI> currentNodeURI;
      rv = NS_NewURI(getter_AddRefs(currentNodeURI), oldCValue);
      if (NS_SUCCEEDED(rv))
      {
        nsCAutoString newRelativePath;
        aDestURL->GetRelativeSpec(currentNodeURI, newRelativePath);
        if (!newRelativePath.IsEmpty())
        {
          NS_ConvertUTF8toUTF16 newCValue(newRelativePath);
          attrNode->SetNodeValue(newCValue);
        }
      }
    }
  }

  return NS_OK;
}

nsresult
nsHTMLEditor::RelativizeURIInFragmentList(const nsCOMArray<nsIDOMNode> &aNodeList,
                                          const nsAString &aFlavor,
                                          nsIDOMDocument *aSourceDoc,
                                          nsIDOMNode *aTargetNode)
{
  // determine destination URL
  nsCOMPtr<nsIDOMDocument> domDoc;
  GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> destDoc = do_QueryInterface(domDoc);
  NS_ENSURE_TRUE(destDoc, NS_ERROR_FAILURE);

  nsCOMPtr<nsIURL> destURL = do_QueryInterface(destDoc->GetDocBaseURI());
  NS_ENSURE_TRUE(destURL, NS_ERROR_FAILURE);

  nsresult rv;
  nsCOMPtr<nsIDOMDocumentTraversal> trav = do_QueryInterface(domDoc, &rv);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  PRInt32 listCount = aNodeList.Count();
  PRInt32 j;
  for (j = 0; j < listCount; j++)
  {
    nsIDOMNode* somenode = aNodeList[j];

    nsCOMPtr<nsIDOMTreeWalker> walker;
    rv = trav->CreateTreeWalker(somenode, nsIDOMNodeFilter::SHOW_ELEMENT,
                                nsnull, PR_TRUE, getter_AddRefs(walker));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMNode> currentNode;
    walker->GetCurrentNode(getter_AddRefs(currentNode));
    while (currentNode)
    {
      rv = RelativizeURIForNode(currentNode, destURL);
      NS_ENSURE_SUCCESS(rv, rv);

      walker->NextNode(getter_AddRefs(currentNode));
    }
  }

  return NS_OK;
}

nsresult
nsHTMLEditor::AddInsertionListener(nsIContentFilter *aListener)
{
  NS_ENSURE_TRUE(aListener, NS_ERROR_NULL_POINTER);

  // don't let a listener be added more than once
  if (mContentFilters.IndexOfObject(aListener) == -1)
  {
    if (!mContentFilters.AppendObject(aListener))
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}
 
nsresult
nsHTMLEditor::RemoveInsertionListener(nsIContentFilter *aListener)
{
  NS_ENSURE_TRUE(aListener, NS_ERROR_FAILURE);

  if (!mContentFilters.RemoveObject(aListener))
    return NS_ERROR_FAILURE;

  return NS_OK;
}
 
nsresult
nsHTMLEditor::DoContentFilterCallback(const nsAString &aFlavor, 
                                      nsIDOMDocument *sourceDoc,
                                      PRBool aWillDeleteSelection,
                                      nsIDOMNode **aFragmentAsNode, 
                                      nsIDOMNode **aFragStartNode, 
                                      PRInt32 *aFragStartOffset,
                                      nsIDOMNode **aFragEndNode, 
                                      PRInt32 *aFragEndOffset,
                                      nsIDOMNode **aTargetNode,
                                      PRInt32 *aTargetOffset,
                                      PRBool *aDoContinue)
{
  *aDoContinue = PR_TRUE;

  PRInt32 i;
  nsIContentFilter *listener;
  for (i=0; i < mContentFilters.Count() && *aDoContinue; i++)
  {
    listener = (nsIContentFilter *)mContentFilters[i];
    if (listener)
      listener->NotifyOfInsertion(aFlavor, nsnull, sourceDoc,
                                  aWillDeleteSelection, aFragmentAsNode,
                                  aFragStartNode, aFragStartOffset, 
                                  aFragEndNode, aFragEndOffset,
                                  aTargetNode, aTargetOffset, aDoContinue);
  }

  return NS_OK;
}

PRBool
nsHTMLEditor::IsInLink(nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *outLink)
{
  NS_ENSURE_TRUE(aNode, PR_FALSE);
  if (outLink)
    *outLink = nsnull;
  nsCOMPtr<nsIDOMNode> tmp, node = aNode;
  while (node)
  {
    if (nsHTMLEditUtils::IsLink(node)) 
    {
      if (outLink)
        *outLink = node;
      return PR_TRUE;
    }
    tmp = node;
    tmp->GetParentNode(getter_AddRefs(node));
  }
  return PR_FALSE;
}


nsresult
nsHTMLEditor::StripFormattingNodes(nsIDOMNode *aNode, PRBool aListOnly)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);

  nsresult res = NS_OK;
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  if (content->TextIsOnlyWhitespace())
  {
    nsCOMPtr<nsIDOMNode> parent, ignored;
    aNode->GetParentNode(getter_AddRefs(parent));
    if (parent)
    {
      if (!aListOnly || nsHTMLEditUtils::IsList(parent))
        res = parent->RemoveChild(aNode, getter_AddRefs(ignored));
      return res;
    }
  }
  
  if (!nsHTMLEditUtils::IsPre(aNode))
  {
    nsCOMPtr<nsIDOMNode> child;
    aNode->GetLastChild(getter_AddRefs(child));
  
    while (child)
    {
      nsCOMPtr<nsIDOMNode> tmp;
      child->GetPreviousSibling(getter_AddRefs(tmp));
      res = StripFormattingNodes(child, aListOnly);
      NS_ENSURE_SUCCESS(res, res);
      child = tmp;
    }
  }
  return res;
}

NS_IMETHODIMP nsHTMLEditor::PrepareTransferable(nsITransferable **transferable)
{
  return NS_OK;
}

NS_IMETHODIMP nsHTMLEditor::PrepareHTMLTransferable(nsITransferable **aTransferable, 
                                                    PRBool aHavePrivFlavor)
{
  // Create generic Transferable for getting the data
  nsresult rv = CallCreateInstance("@mozilla.org/widget/transferable;1", aTransferable);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the nsITransferable interface for getting the data from the clipboard
  if (aTransferable)
  {
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

      nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
      PRInt32 clipboardPasteOrder = 1; // order of image-encoding preference

      if (prefs)
      {
        prefs->GetIntPref("clipboard.paste_image_type", &clipboardPasteOrder);
        switch (clipboardPasteOrder)
        {
          case 0:  // prefer JPEG over PNG over GIF encoding
            (*aTransferable)->AddDataFlavor(kJPEGImageMime);
            (*aTransferable)->AddDataFlavor(kPNGImageMime);
            (*aTransferable)->AddDataFlavor(kGIFImageMime);
            break;
          case 1:  // prefer PNG over JPEG over GIF encoding (default)
          default:
            (*aTransferable)->AddDataFlavor(kPNGImageMime);
            (*aTransferable)->AddDataFlavor(kJPEGImageMime);
            (*aTransferable)->AddDataFlavor(kGIFImageMime);
            break;
          case 2:  // prefer GIF over JPEG over PNG encoding
            (*aTransferable)->AddDataFlavor(kGIFImageMime);
            (*aTransferable)->AddDataFlavor(kJPEGImageMime);
            (*aTransferable)->AddDataFlavor(kPNGImageMime);
            break;
        }
      }
    }
    (*aTransferable)->AddDataFlavor(kUnicodeMime);
    (*aTransferable)->AddDataFlavor(kMozTextInternal);
  }
  
  return NS_OK;
}

PRInt32
FindPositiveIntegerAfterString(const char *aLeadingString, nsCString &aCStr)
{
  // first obtain offsets from cfhtml str
  PRInt32 numFront = aCStr.Find(aLeadingString);
  if (numFront == -1)
    return -1;
  numFront += strlen(aLeadingString); 
  
  PRInt32 numBack = aCStr.FindCharInSet(CRLF, numFront);
  if (numBack == -1)
    return -1;
   
  nsCAutoString numStr(Substring(aCStr, numFront, numBack-numFront));
  PRInt32 errorCode;
  return numStr.ToInteger(&errorCode);
}

nsresult
RemoveFragComments(nsCString & aStr)
{
  // remove the StartFragment/EndFragment comments from the str, if present
  PRInt32 startCommentIndx = aStr.Find("<!--StartFragment");
  if (startCommentIndx >= 0)
  {
    PRInt32 startCommentEnd = aStr.Find("-->", PR_FALSE, startCommentIndx);
    if (startCommentEnd > startCommentIndx)
      aStr.Cut(startCommentIndx, (startCommentEnd+3)-startCommentIndx);
  }  
  PRInt32 endCommentIndx = aStr.Find("<!--EndFragment");
  if (endCommentIndx >= 0)
  {
    PRInt32 endCommentEnd = aStr.Find("-->", PR_FALSE, endCommentIndx);
    if (endCommentEnd > endCommentIndx)
      aStr.Cut(endCommentIndx, (endCommentEnd+3)-endCommentIndx);
  }  
  return NS_OK;
}

nsresult
nsHTMLEditor::ParseCFHTML(nsCString & aCfhtml, PRUnichar **aStuffToPaste, PRUnichar **aCfcontext)
{
  // first obtain offsets from cfhtml str
  PRInt32 startHTML     = FindPositiveIntegerAfterString("StartHTML:", aCfhtml);
  PRInt32 endHTML       = FindPositiveIntegerAfterString("EndHTML:", aCfhtml);
  PRInt32 startFragment = FindPositiveIntegerAfterString("StartFragment:", aCfhtml);
  PRInt32 endFragment   = FindPositiveIntegerAfterString("EndFragment:", aCfhtml);

  if ((startHTML<0) || (endHTML<0) || (startFragment<0) || (endFragment<0))
    return NS_ERROR_FAILURE;
 
  // create context string
  nsCAutoString contextUTF8(Substring(aCfhtml, startHTML, startFragment - startHTML) +
                            NS_LITERAL_CSTRING("<!--" kInsertCookie "-->") +
                            Substring(aCfhtml, endFragment, endHTML - endFragment));

  // validate startFragment
  // make sure it's not in the middle of a HTML tag
  // see bug #228879 for more details
  PRInt32 curPos = startFragment;
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
  nsCAutoString fragmentUTF8(Substring(aCfhtml, startFragment, endFragment-startFragment));
  
  // remove the StartFragment/EndFragment comments from the fragment, if present
  RemoveFragComments(fragmentUTF8);

  // remove the StartFragment/EndFragment comments from the context, if present
  RemoveFragComments(contextUTF8);

  // convert both strings to usc2
  const nsAFlatString& fragUcs2Str = NS_ConvertUTF8toUTF16(fragmentUTF8);
  const nsAFlatString& cntxtUcs2Str = NS_ConvertUTF8toUTF16(contextUTF8);
  
  // translate platform linebreaks for fragment
  PRInt32 oldLengthInChars = fragUcs2Str.Length() + 1;  // +1 to include null terminator
  PRInt32 newLengthInChars = 0;
  *aStuffToPaste = nsLinebreakConverter::ConvertUnicharLineBreaks(fragUcs2Str.get(),
                                                           nsLinebreakConverter::eLinebreakAny, 
                                                           nsLinebreakConverter::eLinebreakContent, 
                                                           oldLengthInChars, &newLengthInChars);
  if (!aStuffToPaste)
  {
    return NS_ERROR_FAILURE;
  }
  
  // translate platform linebreaks for context
  oldLengthInChars = cntxtUcs2Str.Length() + 1;  // +1 to include null terminator
  newLengthInChars = 0;
  *aCfcontext = nsLinebreakConverter::ConvertUnicharLineBreaks(cntxtUcs2Str.get(),
                                                           nsLinebreakConverter::eLinebreakAny, 
                                                           nsLinebreakConverter::eLinebreakContent, 
                                                           oldLengthInChars, &newLengthInChars);
  // it's ok for context to be empty.  frag might be whole doc and contain all it's context.
  
  // we're done!  
  return NS_OK;
}

NS_IMETHODIMP nsHTMLEditor::InsertFromTransferable(nsITransferable *transferable, 
                                                   nsIDOMDocument *aSourceDoc,
                                                   const nsAString & aContextStr,
                                                   const nsAString & aInfoStr,
                                                   nsIDOMNode *aDestinationNode,
                                                   PRInt32 aDestOffset,
                                                   PRBool aDoDeleteSelection)
{
  nsresult rv = NS_OK;
  nsXPIDLCString bestFlavor;
  nsCOMPtr<nsISupports> genericDataObj;
  PRUint32 len = 0;
  if ( NS_SUCCEEDED(transferable->GetAnyTransferData(getter_Copies(bestFlavor), getter_AddRefs(genericDataObj), &len)) )
  {
    nsAutoTxnsConserveSelection dontSpazMySelection(this);
    nsAutoString flavor;
    flavor.AssignWithConversion(bestFlavor);
    nsAutoString stuffToPaste;
#ifdef DEBUG_clipboard
    printf("Got flavor [%s]\n", bestFlavor.get());
#endif

    // Try to determine whether we should use a sanitizing fragment sink
    PRBool isSafe = PR_FALSE;
    if (aSourceDoc) {
      nsCOMPtr<nsIDOMDocument> destdomdoc;
      rv = GetDocument(getter_AddRefs(destdomdoc));
      NS_ENSURE_SUCCESS(rv, rv);
      nsCOMPtr<nsIDocument> destdoc = do_QueryInterface(destdomdoc);
      NS_ASSERTION(destdoc, "Where is our destination doc?");
      nsCOMPtr<nsIDocument> srcdoc = do_QueryInterface(aSourceDoc);
      NS_ASSERTION(srcdoc, "Where is our source doc?");

      nsIPrincipal* srcPrincipal = srcdoc->NodePrincipal();
      nsIPrincipal* destPrincipal = destdoc->NodePrincipal();
      NS_ASSERTION(srcPrincipal && destPrincipal, "How come we don't have a principal?");
      rv = srcPrincipal->Subsumes(destPrincipal, &isSafe);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (0 == nsCRT::strcmp(bestFlavor, kNativeHTMLMime))
    {
      // note cf_html uses utf8, hence use length = len, not len/2 as in flavors below
      nsCOMPtr<nsISupportsCString> textDataObj(do_QueryInterface(genericDataObj));
      if (textDataObj && len > 0)
      {
        nsCAutoString cfhtml;
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
        }
      }
    }
    else if (0 == nsCRT::strcmp(bestFlavor, kHTMLMime))
    {
      nsCOMPtr<nsISupportsString> textDataObj(do_QueryInterface(genericDataObj));
      if (textDataObj && len > 0)
      {
        nsAutoString text;
        textDataObj->GetData(text);
        NS_ASSERTION(text.Length() <= (len/2), "Invalid length!");
        stuffToPaste.Assign(text.get(), len / 2);
        nsAutoEditBatch beginBatching(this);
        rv = DoInsertHTMLWithContext(stuffToPaste,
                                     aContextStr, aInfoStr, flavor,
                                     aSourceDoc,
                                     aDestinationNode, aDestOffset,
                                     aDoDeleteSelection,
                                     isSafe);
      }
    }
    else if (0 == nsCRT::strcmp(bestFlavor, kUnicodeMime) ||
             0 == nsCRT::strcmp(bestFlavor, kMozTextInternal))
    {
      nsCOMPtr<nsISupportsString> textDataObj(do_QueryInterface(genericDataObj));
      if (textDataObj && len > 0)
      {
        nsAutoString text;
        textDataObj->GetData(text);
        NS_ASSERTION(text.Length() <= (len/2), "Invalid length!");
        stuffToPaste.Assign(text.get(), len / 2);
        nsAutoEditBatch beginBatching(this);
        // need to provide a hook from this point
        rv = InsertTextAt(stuffToPaste, aDestinationNode, aDestOffset, aDoDeleteSelection);
      }
    }
    else if (0 == nsCRT::strcmp(bestFlavor, kFileMime))
    {
      nsCOMPtr<nsIFile> fileObj(do_QueryInterface(genericDataObj));
      if (fileObj && len > 0)
      {
        
        nsCOMPtr<nsIURI> uri;
        rv = NS_NewFileURI(getter_AddRefs(uri), fileObj);
        NS_ENSURE_SUCCESS(rv, rv);
        
        nsCOMPtr<nsIURL> fileURL(do_QueryInterface(uri));
        if (fileURL)
        {
          PRBool insertAsImage = PR_FALSE;
          nsCAutoString fileextension;
          rv = fileURL->GetFileExtension(fileextension);
          if (NS_SUCCEEDED(rv) && !fileextension.IsEmpty())
          {
            if ( (nsCRT::strcasecmp(fileextension.get(), "jpg") == 0 )
              || (nsCRT::strcasecmp(fileextension.get(), "jpeg") == 0 )
              || (nsCRT::strcasecmp(fileextension.get(), "gif") == 0 )
              || (nsCRT::strcasecmp(fileextension.get(), "png") == 0 ) )
            {
              insertAsImage = PR_TRUE;
            }
          }
          
          nsCAutoString urltext;
          rv = fileURL->GetSpec(urltext);
          if (NS_SUCCEEDED(rv) && !urltext.IsEmpty())
          {
            if (insertAsImage)
            {
              stuffToPaste.AssignLiteral("<IMG src=\"");
              AppendUTF8toUTF16(urltext, stuffToPaste);
              stuffToPaste.AppendLiteral("\" alt=\"\" >");
            }
            else /* insert as link */
            {
              stuffToPaste.AssignLiteral("<A href=\"");
              AppendUTF8toUTF16(urltext, stuffToPaste);
              stuffToPaste.AppendLiteral("\">");
              AppendUTF8toUTF16(urltext, stuffToPaste);
              stuffToPaste.AppendLiteral("</A>");
            }
            nsAutoEditBatch beginBatching(this);

            const nsAFlatString& empty = EmptyString();
            rv = DoInsertHTMLWithContext(stuffToPaste,
                                         empty, empty, flavor, 
                                         aSourceDoc,
                                         aDestinationNode, aDestOffset,
                                         aDoDeleteSelection,
                                         isSafe);
          }
        }
      }
    }
    else if (0 == nsCRT::strcmp(bestFlavor, kJPEGImageMime) ||
             0 == nsCRT::strcmp(bestFlavor, kPNGImageMime) ||
             0 == nsCRT::strcmp(bestFlavor, kGIFImageMime))
    {
      nsCOMPtr<nsIInputStream> imageStream(do_QueryInterface(genericDataObj));
      NS_ENSURE_TRUE(imageStream, NS_ERROR_FAILURE);

      nsCString imageData;
      rv = NS_ConsumeStream(imageStream, PR_UINT32_MAX, imageData);
      NS_ENSURE_SUCCESS(rv, rv);

      char * base64 = PL_Base64Encode(imageData.get(), imageData.Length(), nsnull);
      NS_ENSURE_TRUE(base64, NS_ERROR_OUT_OF_MEMORY);

      stuffToPaste.AssignLiteral("<IMG src=\"data:");
      AppendUTF8toUTF16(bestFlavor, stuffToPaste);
      stuffToPaste.AppendLiteral(";base64,");
      AppendUTF8toUTF16(base64, stuffToPaste);
      stuffToPaste.AppendLiteral("\" alt=\"\" >");
      nsAutoEditBatch beginBatching(this);
      rv = DoInsertHTMLWithContext(stuffToPaste, EmptyString(), EmptyString(), 
                                   NS_LITERAL_STRING(kFileMime),
                                   aSourceDoc,
                                   aDestinationNode, aDestOffset,
                                   aDoDeleteSelection,
                                   isSafe);
      PR_Free(base64);
    }
  }
      
  // Try to scroll the selection into view if the paste/drop succeeded

  // After ScrollSelectionIntoView(), the pending notifications might be
  // flushed and PresShell/PresContext/Frames may be dead. See bug 418470.
  if (NS_SUCCEEDED(rv))
    ScrollSelectionIntoView(PR_FALSE);

  return rv;
}

NS_IMETHODIMP nsHTMLEditor::InsertFromDrop(nsIDOMEvent* aDropEvent)
{
  ForceCompositionEnd();
  
  nsresult rv;
  nsCOMPtr<nsIDragService> dragService = 
           do_GetService("@mozilla.org/widget/dragservice;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDragSession> dragSession;
  dragService->GetCurrentSession(getter_AddRefs(dragSession)); 
  NS_ENSURE_TRUE(dragSession, NS_OK);

  // transferable hooks here
  nsCOMPtr<nsIDOMDocument> domdoc;
  GetDocument(getter_AddRefs(domdoc));

  // find out if we have our internal html flavor on the clipboard.  We don't want to mess
  // around with cfhtml if we do.
  PRBool bHavePrivateHTMLFlavor = PR_FALSE;
  rv = dragSession->IsDataFlavorSupported(kHTMLContext, &bHavePrivateHTMLFlavor);
  NS_ENSURE_SUCCESS(rv, rv);
  
  // Get the nsITransferable interface for getting the data from the drop
  nsCOMPtr<nsITransferable> trans;
  rv = PrepareHTMLTransferable(getter_AddRefs(trans), bHavePrivateHTMLFlavor);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(trans, NS_OK);  // NS_ERROR_FAILURE; SHOULD WE FAIL?

  PRUint32 numItems = 0; 
  rv = dragSession->GetNumDropItems(&numItems);
  NS_ENSURE_SUCCESS(rv, rv);

  // Combine any deletion and drop insertion into one transaction
  nsAutoEditBatch beginBatching(this);

  // We never have to delete if selection is already collapsed
  PRBool deleteSelection = PR_FALSE;
  nsCOMPtr<nsIDOMNode> newSelectionParent;
  PRInt32 newSelectionOffset = 0;

  // Source doc is null if source is *not* the current editor document
  nsCOMPtr<nsIDOMDocument> srcdomdoc;
  rv = dragSession->GetSourceDocument(getter_AddRefs(srcdomdoc));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 i; 
  PRBool doPlaceCaret = PR_TRUE;
  for (i = 0; i < numItems; ++i)
  {
    rv = dragSession->GetData(trans, i);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(trans, NS_OK); // NS_ERROR_FAILURE; Should we fail?

    // get additional html copy hints, if present
    nsAutoString contextStr, infoStr;
    nsCOMPtr<nsISupports> contextDataObj, infoDataObj;
    PRUint32 contextLen, infoLen;
    nsCOMPtr<nsISupportsString> textDataObj;
    
    nsCOMPtr<nsITransferable> contextTrans =
                      do_CreateInstance("@mozilla.org/widget/transferable;1");
    NS_ENSURE_TRUE(contextTrans, NS_ERROR_NULL_POINTER);
    contextTrans->AddDataFlavor(kHTMLContext);
    dragSession->GetData(contextTrans, i);
    contextTrans->GetTransferData(kHTMLContext, getter_AddRefs(contextDataObj), &contextLen);

    nsCOMPtr<nsITransferable> infoTrans =
                      do_CreateInstance("@mozilla.org/widget/transferable;1");
    NS_ENSURE_TRUE(infoTrans, NS_ERROR_NULL_POINTER);
    infoTrans->AddDataFlavor(kHTMLInfo);
    dragSession->GetData(infoTrans, i);
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

    if (doPlaceCaret)
    {
      // check if the user pressed the key to force a copy rather than a move
      // if we run into problems here, we'll just assume the user doesn't want a copy
      PRBool userWantsCopy = PR_FALSE;

      nsCOMPtr<nsIDOMNSUIEvent> nsuiEvent(do_QueryInterface(aDropEvent));
      NS_ENSURE_TRUE(nsuiEvent, NS_ERROR_FAILURE);

      nsCOMPtr<nsIDOMMouseEvent> mouseEvent(do_QueryInterface(aDropEvent));
      if (mouseEvent)

#if defined(XP_MAC) || defined(XP_MACOSX)
        mouseEvent->GetAltKey(&userWantsCopy);
#else
        mouseEvent->GetCtrlKey(&userWantsCopy);
#endif

      // Current doc is destination
      nsCOMPtr<nsIDOMDocument>destdomdoc; 
      rv = GetDocument(getter_AddRefs(destdomdoc)); 
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsISelection> selection;
      rv = GetSelection(getter_AddRefs(selection));
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);

      PRBool isCollapsed;
      rv = selection->GetIsCollapsed(&isCollapsed);
      NS_ENSURE_SUCCESS(rv, rv);
      
      // Parent and offset under the mouse cursor
      rv = nsuiEvent->GetRangeParent(getter_AddRefs(newSelectionParent));
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ENSURE_TRUE(newSelectionParent, NS_ERROR_FAILURE);

      rv = nsuiEvent->GetRangeOffset(&newSelectionOffset);
      NS_ENSURE_SUCCESS(rv, rv);

      // XXX: This userSelectNode code is a workaround for bug 195957.
      //
      // Check to see if newSelectionParent is part of a "-moz-user-select: all"
      // subtree. If it is, we need to make sure we don't drop into it!

      nsCOMPtr<nsIDOMNode> userSelectNode = FindUserSelectAllNode(newSelectionParent);

      if (userSelectNode)
      {
        // The drop is happening over a "-moz-user-select: all"
        // subtree so make sure the content we insert goes before
        // the root of the subtree.
        //
        // XXX: Note that inserting before the subtree matches the
        //      current behavior when dropping on top of an image.
        //      The decision for dropping before or after the
        //      subtree should really be done based on coordinates.

        rv = GetNodeLocation(userSelectNode, address_of(newSelectionParent),
                             &newSelectionOffset);

        NS_ENSURE_SUCCESS(rv, rv);
        NS_ENSURE_TRUE(newSelectionParent, NS_ERROR_FAILURE);
      }

      // We never have to delete if selection is already collapsed
      PRBool cursorIsInSelection = PR_FALSE;

      // Check if mouse is in the selection
      if (!isCollapsed)
      {
        PRInt32 rangeCount;
        rv = selection->GetRangeCount(&rangeCount);
        NS_ENSURE_SUCCESS(rv, rv);

        for (PRInt32 j = 0; j < rangeCount; j++)
        {
          nsCOMPtr<nsIDOMRange> range;

          rv = selection->GetRangeAt(j, getter_AddRefs(range));
          if (NS_FAILED(rv) || !range) 
            continue;//don't bail yet, iterate through them all

          nsCOMPtr<nsIDOMNSRange> nsrange(do_QueryInterface(range));
          if (NS_FAILED(rv) || !nsrange) 
            continue;//don't bail yet, iterate through them all

          rv = nsrange->IsPointInRange(newSelectionParent, newSelectionOffset, &cursorIsInSelection);
          if(cursorIsInSelection)
            break;
        }
        if (cursorIsInSelection)
        {
          // Dragging within same doc can't drop on itself -- leave!
          // (We shouldn't get here - drag event shouldn't have started if over selection)
          if (srcdomdoc == destdomdoc)
            return NS_OK;
          
          // Dragging from another window onto a selection
          // XXX Decision made to NOT do this,
          //     note that 4.x does replace if dropped on
          //deleteSelection = PR_TRUE;
        }
        else 
        {
          // We are NOT over the selection
          if (srcdomdoc == destdomdoc)
          {
            // Within the same doc: delete if user doesn't want to copy
            deleteSelection = !userWantsCopy;
          }
          else
          {
            // Different source doc: Don't delete
            deleteSelection = PR_FALSE;
          }
        }
      }

      // We have to figure out whether to delete/relocate caret only once
      doPlaceCaret = PR_FALSE;
    }
    
    // handle transferable hooks
    if (!nsEditorHookUtils::DoInsertionHook(domdoc, aDropEvent, trans))
      return NS_OK;

    // Beware! This may flush notifications via synchronous
    // ScrollSelectionIntoView.
    rv = InsertFromTransferable(trans, srcdomdoc, contextStr, infoStr,
                                newSelectionParent,
                                newSelectionOffset, deleteSelection);
  }

  return rv;
}

NS_IMETHODIMP nsHTMLEditor::CanDrag(nsIDOMEvent *aDragEvent, PRBool *aCanDrag)
{
  return nsPlaintextEditor::CanDrag(aDragEvent, aCanDrag);
}

nsresult
nsHTMLEditor::PutDragDataInTransferable(nsITransferable **aTransferable)
{
  NS_ENSURE_ARG_POINTER(aTransferable);
  *aTransferable = nsnull;
  nsCOMPtr<nsIDocumentEncoder> docEncoder;
  nsresult rv = SetupDocEncoder(getter_AddRefs(docEncoder));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(docEncoder, NS_ERROR_FAILURE);

  // grab a string
  nsAutoString buffer, parents, info;

  // find out if we're a plaintext control or not
  if (!IsPlaintextEditor())
  {
    // encode the selection as html with contextual info
    rv = docEncoder->EncodeToStringWithContext(parents, info, buffer);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else
  {
    // encode the selection
    rv = docEncoder->EncodeToString(buffer);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // if we have an empty string, we're done; otherwise continue
  if ( buffer.IsEmpty() )
    return NS_OK;

  nsCOMPtr<nsISupportsString> dataWrapper, contextWrapper, infoWrapper;

  dataWrapper = do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = dataWrapper->SetData(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  /* create html flavor transferable */
  nsCOMPtr<nsITransferable> trans = do_CreateInstance("@mozilla.org/widget/transferable;1");
  NS_ENSURE_TRUE(trans, NS_ERROR_FAILURE);

  if (IsPlaintextEditor())
  {
    // Add the unicode flavor to the transferable
    rv = trans->AddDataFlavor(kUnicodeMime);
    NS_ENSURE_SUCCESS(rv, rv);

    // QI the data object an |nsISupports| so that when the transferable holds
    // onto it, it will addref the correct interface.
    nsCOMPtr<nsISupports> genericDataObj(do_QueryInterface(dataWrapper));
    rv = trans->SetTransferData(kUnicodeMime, genericDataObj,
                                buffer.Length() * sizeof(PRUnichar));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else
  {
    contextWrapper = do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID);
    NS_ENSURE_TRUE(contextWrapper, NS_ERROR_FAILURE);
    infoWrapper = do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID);
    NS_ENSURE_TRUE(infoWrapper, NS_ERROR_FAILURE);

    contextWrapper->SetData(parents);
    infoWrapper->SetData(info);

    rv = trans->AddDataFlavor(kHTMLMime);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFormatConverter> htmlConverter =
             do_CreateInstance("@mozilla.org/widget/htmlformatconverter;1");
    NS_ENSURE_TRUE(htmlConverter, NS_ERROR_FAILURE);

    rv = trans->SetConverter(htmlConverter);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsISupports> genericDataObj(do_QueryInterface(dataWrapper));
    rv = trans->SetTransferData(kHTMLMime, genericDataObj,
                                buffer.Length() * sizeof(PRUnichar));
    NS_ENSURE_SUCCESS(rv, rv);

    if (!parents.IsEmpty())
    {
      // Add the htmlcontext DataFlavor to the transferable
      trans->AddDataFlavor(kHTMLContext);
      genericDataObj = do_QueryInterface(contextWrapper);
      trans->SetTransferData(kHTMLContext, genericDataObj,
                             parents.Length() * sizeof(PRUnichar));
    }
    if (!info.IsEmpty())
    {
      // Add the htmlinfo DataFlavor to the transferable
      trans->AddDataFlavor(kHTMLInfo);
      genericDataObj = do_QueryInterface(infoWrapper);
      trans->SetTransferData(kHTMLInfo, genericDataObj,
                             info.Length() * sizeof(PRUnichar));
    }
  }

  *aTransferable = trans; 
  NS_ADDREF(*aTransferable);
  return NS_OK;
}

NS_IMETHODIMP nsHTMLEditor::DoDrag(nsIDOMEvent *aDragEvent)
{
  return nsPlaintextEditor::DoDrag(aDragEvent);
}

PRBool nsHTMLEditor::HavePrivateHTMLFlavor(nsIClipboard *aClipboard)
{
  // check the clipboard for our special kHTMLContext flavor.  If that is there, we know
  // we have our own internal html format on clipboard.
  
  NS_ENSURE_TRUE(aClipboard, PR_FALSE);
  PRBool bHavePrivateHTMLFlavor = PR_FALSE;
  
  const char* flavArray[] = { kHTMLContext };
  
  if (NS_SUCCEEDED(aClipboard->HasDataMatchingFlavors(flavArray,
    NS_ARRAY_LENGTH(flavArray), nsIClipboard::kGlobalClipboard,
    &bHavePrivateHTMLFlavor )))
    return bHavePrivateHTMLFlavor;
    
  return PR_FALSE;
}


NS_IMETHODIMP nsHTMLEditor::Paste(PRInt32 aSelectionType)
{
  if (!FireClipboardEvent(NS_PASTE))
    return NS_OK;

  // Get Clipboard Service
  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard(do_GetService("@mozilla.org/widget/clipboard;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  
  // find out if we have our internal html flavor on the clipboard.  We don't want to mess
  // around with cfhtml if we do.
  PRBool bHavePrivateHTMLFlavor = HavePrivateHTMLFlavor(clipboard);

  // Get the nsITransferable interface for getting the data from the clipboard
  nsCOMPtr<nsITransferable> trans;
  rv = PrepareHTMLTransferable(getter_AddRefs(trans), bHavePrivateHTMLFlavor);
  if (NS_SUCCEEDED(rv) && trans)
  {
    // Get the Data from the clipboard  
    if (NS_SUCCEEDED(clipboard->GetData(trans, aSelectionType)) && IsModifiable())
    {
      // also get additional html copy hints, if present
      nsAutoString contextStr, infoStr;

      // also get additional html copy hints, if present
      if (bHavePrivateHTMLFlavor)
      {
        nsCOMPtr<nsISupports> contextDataObj, infoDataObj;
        PRUint32 contextLen, infoLen;
        nsCOMPtr<nsISupportsString> textDataObj;
        
        nsCOMPtr<nsITransferable> contextTrans =
                      do_CreateInstance("@mozilla.org/widget/transferable;1");
        NS_ENSURE_TRUE(contextTrans, NS_ERROR_NULL_POINTER);
        contextTrans->AddDataFlavor(kHTMLContext);
        clipboard->GetData(contextTrans, aSelectionType);
        contextTrans->GetTransferData(kHTMLContext, getter_AddRefs(contextDataObj), &contextLen);

        nsCOMPtr<nsITransferable> infoTrans =
                      do_CreateInstance("@mozilla.org/widget/transferable;1");
        NS_ENSURE_TRUE(infoTrans, NS_ERROR_NULL_POINTER);
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
      if (!nsEditorHookUtils::DoInsertionHook(domdoc, nsnull, trans))
        return NS_OK;

      // Beware! This may flush notifications via synchronous
      // ScrollSelectionIntoView.
      rv = InsertFromTransferable(trans, nsnull, contextStr, infoStr,
                                  nsnull, 0, PR_TRUE);
    }
  }

  return rv;
}

NS_IMETHODIMP nsHTMLEditor::PasteTransferable(nsITransferable *aTransferable)
{
  if (!FireClipboardEvent(NS_PASTE))
    return NS_OK;

  // handle transferable hooks
  nsCOMPtr<nsIDOMDocument> domdoc;
  GetDocument(getter_AddRefs(domdoc));
  if (!nsEditorHookUtils::DoInsertionHook(domdoc, nsnull, aTransferable))
    return NS_OK;

  // Beware! This may flush notifications via synchronous
  // ScrollSelectionIntoView.
  nsAutoString contextStr, infoStr;
  return InsertFromTransferable(aTransferable, nsnull, contextStr, infoStr,
                                nsnull, 0, PR_TRUE);
}

// 
// HTML PasteNoFormatting. Ignore any HTML styles and formating in paste source
//
NS_IMETHODIMP nsHTMLEditor::PasteNoFormatting(PRInt32 aSelectionType)
{
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
      // Beware! This may flush notifications via synchronous
      // ScrollSelectionIntoView.
      rv = InsertFromTransferable(trans, nsnull, empty, empty, nsnull, 0,
                                  PR_TRUE);
    }
  }

  return rv;
}


// The following arrays contain the MIME types that we can paste. The arrays
// are used by CanPaste() and CanPasteTransferable() below.

static const char* textEditorFlavors[] = { kUnicodeMime };
static const char* textHtmlEditorFlavors[] = { kUnicodeMime, kHTMLMime,
                                               kJPEGImageMime, kPNGImageMime,
                                               kGIFImageMime };

NS_IMETHODIMP nsHTMLEditor::CanPaste(PRInt32 aSelectionType, PRBool *aCanPaste)
{
  NS_ENSURE_ARG_POINTER(aCanPaste);
  *aCanPaste = PR_FALSE;

  // can't paste if readonly
  if (!IsModifiable())
    return NS_OK;

  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard(do_GetService("@mozilla.org/widget/clipboard;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool haveFlavors;

  // Use the flavors depending on the current editor mask
  if (IsPlaintextEditor())
    rv = clipboard->HasDataMatchingFlavors(textEditorFlavors,
                                           NS_ARRAY_LENGTH(textEditorFlavors),
                                           aSelectionType, &haveFlavors);
  else
    rv = clipboard->HasDataMatchingFlavors(textHtmlEditorFlavors,
                                           NS_ARRAY_LENGTH(textHtmlEditorFlavors),
                                           aSelectionType, &haveFlavors);
  
  NS_ENSURE_SUCCESS(rv, rv);
  
  *aCanPaste = haveFlavors;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLEditor::CanPasteTransferable(nsITransferable *aTransferable, PRBool *aCanPaste)
{
  NS_ENSURE_ARG_POINTER(aCanPaste);

  // can't paste if readonly
  if (!IsModifiable()) {
    *aCanPaste = PR_FALSE;
    return NS_OK;
  }

  // If |aTransferable| is null, assume that a paste will succeed.
  if (!aTransferable) {
    *aCanPaste = PR_TRUE;
    return NS_OK;
  }

  // Peek in |aTransferable| to see if it contains a supported MIME type.

  // Use the flavors depending on the current editor mask
  const char ** flavors;
  unsigned length;
  if (IsPlaintextEditor()) {
    flavors = textEditorFlavors;
    length = NS_ARRAY_LENGTH(textEditorFlavors);
  } else {
    flavors = textHtmlEditorFlavors;
    length = NS_ARRAY_LENGTH(textHtmlEditorFlavors);
  }

  for (unsigned int i = 0; i < length; i++, flavors++) {
    nsCOMPtr<nsISupports> data;
    PRUint32 dataLen;
    nsresult rv = aTransferable->GetTransferData(*flavors,
                                                 getter_AddRefs(data),
                                                 &dataLen);
    if (NS_SUCCEEDED(rv) && data) {
      *aCanPaste = PR_TRUE;
      return NS_OK;
    }
  }
  
  *aCanPaste = PR_FALSE;
  return NS_OK;
}


// 
// HTML PasteAsQuotation: Paste in a blockquote type=cite
//
NS_IMETHODIMP nsHTMLEditor::PasteAsQuotation(PRInt32 aSelectionType)
{
  if (IsPlaintextEditor())
    return PasteAsPlaintextQuotation(aSelectionType);

  nsAutoString citation;
  return PasteAsCitedQuotation(citation, aSelectionType);
}

NS_IMETHODIMP nsHTMLEditor::PasteAsCitedQuotation(const nsAString & aCitation,
                                                  PRInt32 aSelectionType)
{
  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, kOpInsertQuotation, nsIEditor::eNext);

  // get selection
  nsCOMPtr<nsISelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  // give rules a chance to handle or cancel
  nsTextRulesInfo ruleInfo(nsTextEditRules::kInsertElement);
  PRBool cancel, handled;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  NS_ENSURE_SUCCESS(res, res);
  if (cancel) return NS_OK; // rules canceled the operation
  if (!handled)
  {
    nsCOMPtr<nsIDOMNode> newNode;
    res = DeleteSelectionAndCreateNode(NS_LITERAL_STRING("blockquote"), getter_AddRefs(newNode));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(newNode, NS_ERROR_NULL_POINTER);

    // Try to set type=cite.  Ignore it if this fails.
    nsCOMPtr<nsIDOMElement> newElement (do_QueryInterface(newNode));
    if (newElement)
    {
      newElement->SetAttribute(NS_LITERAL_STRING("type"), NS_LITERAL_STRING("cite"));
    }

    // Set the selection to the underneath the node we just inserted:
    res = selection->Collapse(newNode, 0);
    if (NS_FAILED(res))
    {
#ifdef DEBUG_akkana
      printf("Couldn't collapse");
#endif
      // XXX: error result:  should res be returned here?
    }

    res = Paste(aSelectionType);
  }
  return res;
}

//
// Paste a plaintext quotation
//
NS_IMETHODIMP nsHTMLEditor::PasteAsPlaintextQuotation(PRInt32 aSelectionType)
{
  // Get Clipboard Service
  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard(do_GetService("@mozilla.org/widget/clipboard;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // Create generic Transferable for getting the data
  nsCOMPtr<nsITransferable> trans =
                 do_CreateInstance("@mozilla.org/widget/transferable;1", &rv);
  if (NS_SUCCEEDED(rv) && trans)
  {
    // We only handle plaintext pastes here
    trans->AddDataFlavor(kUnicodeMime);

    // Get the Data from the clipboard
    clipboard->GetData(trans, aSelectionType);

    // Now we ask the transferable for the data
    // it still owns the data, we just have a pointer to it.
    // If it can't support a "text" output of the data the call will fail
    nsCOMPtr<nsISupports> genericDataObj;
    PRUint32 len = 0;
    char* flav = 0;
    rv = trans->GetAnyTransferData(&flav, getter_AddRefs(genericDataObj),
                                   &len);
    if (NS_FAILED(rv))
    {
#ifdef DEBUG_akkana
      printf("PasteAsPlaintextQuotation: GetAnyTransferData failed, %d\n", rv);
#endif
      return rv;
    }

    if (flav && 0 == nsCRT::strcmp((flav), kUnicodeMime))
    {
#ifdef DEBUG_clipboard
    printf("Got flavor [%s]\n", flav);
#endif
      nsCOMPtr<nsISupportsString> textDataObj(do_QueryInterface(genericDataObj));
      if (textDataObj && len > 0)
      {
        nsAutoString stuffToPaste;
        textDataObj->GetData(stuffToPaste);
        NS_ASSERTION(stuffToPaste.Length() <= (len/2), "Invalid length!");
        nsAutoEditBatch beginBatching(this);
        rv = InsertAsPlaintextQuotation(stuffToPaste, PR_TRUE, 0);
      }
    }
    NS_Free(flav);
  }

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

  static const PRUnichar cite('>');
  PRBool curHunkIsQuoted = (aStringToInsert.First() == cite);

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
    NS_ASSERTION(PR_FALSE,
            "Return characters in DOM! InsertTextWithQuotations may be wrong");
#endif /* DEBUG */

  // Loop over lines:
  nsresult rv = NS_OK;
  nsAString::const_iterator lineStart (hunkStart);
  while (1)   // we will break from inside when we run out of newlines
  {
    // Search for the end of this line (dom newlines, see above):
    PRBool found = FindCharInReadable('\n', lineStart, strEnd);
    PRBool quoted = PR_FALSE;
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
#ifdef DEBUG_akkana_verbose
    printf("==== Inserting text as %squoted: ---\n%s---\n",
           curHunkIsQuoted ? "" : "non-",
           NS_LossyConvertUTF16toASCII(curHunk).get());
#endif
    if (curHunkIsQuoted)
      rv = InsertAsPlaintextQuotation(curHunk, PR_FALSE,
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
    return InsertAsPlaintextQuotation(aQuotedText, PR_TRUE, aNodeInserted);

  nsAutoString citation;
  return InsertAsCitedQuotation(aQuotedText, citation, PR_FALSE,
                                aNodeInserted);
}

// Insert plaintext as a quotation, with cite marks (e.g. "> ").
// This differs from its corresponding method in nsPlaintextEditor
// in that here, quoted material is enclosed in a <pre> tag
// in order to preserve the original line wrapping.
NS_IMETHODIMP
nsHTMLEditor::InsertAsPlaintextQuotation(const nsAString & aQuotedText,
                                         PRBool aAddCites,
                                         nsIDOMNode **aNodeInserted)
{
  if (mWrapToWindow)
    return nsPlaintextEditor::InsertAsQuotation(aQuotedText, aNodeInserted);

  nsresult rv;

  // The quotesPreformatted pref is a temporary measure. See bug 69638.
  // Eventually we'll pick one way or the other.
  PRBool quotesInPre = PR_FALSE;
  nsCOMPtr<nsIPrefBranch> prefBranch =
    do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && prefBranch)
    prefBranch->GetBoolPref("editor.quotesPreformatted", &quotesInPre);

  nsCOMPtr<nsIDOMNode> preNode;
  // get selection
  nsCOMPtr<nsISelection> selection;
  rv = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!selection)
  {
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  }
  else
  {
    nsAutoEditBatch beginBatching(this);
    nsAutoRules beginRulesSniffing(this, kOpInsertQuotation, nsIEditor::eNext);

    // give rules a chance to handle or cancel
    nsTextRulesInfo ruleInfo(nsTextEditRules::kInsertElement);
    PRBool cancel, handled;
    rv = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
    NS_ENSURE_SUCCESS(rv, rv);
    if (cancel) return NS_OK; // rules canceled the operation
    if (!handled)
    {
      // Wrap the inserted quote in a <pre> so it won't be wrapped:
      nsAutoString tag;
      if (quotesInPre)
        tag.AssignLiteral("pre");
      else
        tag.AssignLiteral("span");

      rv = DeleteSelectionAndCreateNode(tag, getter_AddRefs(preNode));
      
      // If this succeeded, then set selection inside the pre
      // so the inserted text will end up there.
      // If it failed, we don't care what the return value was,
      // but we'll fall through and try to insert the text anyway.
      if (NS_SUCCEEDED(rv) && preNode)
      {
        // Add an attribute on the pre node so we'll know it's a quotation.
        // Do this after the insertion, so that 
        nsCOMPtr<nsIDOMElement> preElement (do_QueryInterface(preNode));
        if (preElement)
        {
          preElement->SetAttribute(NS_LITERAL_STRING("_moz_quote"),
                                   NS_LITERAL_STRING("true"));
          if (quotesInPre)
          {
            // set style to not have unwanted vertical margins
            preElement->SetAttribute(NS_LITERAL_STRING("style"),
                                     NS_LITERAL_STRING("margin: 0 0 0 0px;"));
          }
          else
          {
            // turn off wrapping on spans
            preElement->SetAttribute(NS_LITERAL_STRING("style"),
                                     NS_LITERAL_STRING("white-space: pre;"));
          }
        }

        // and set the selection inside it:
        selection->Collapse(preNode, 0);
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
        *aNodeInserted = preNode;
        NS_IF_ADDREF(*aNodeInserted);
      }
    }
  }
    
  // Set the selection to just after the inserted node:
  if (NS_SUCCEEDED(rv) && preNode)
  {
    nsCOMPtr<nsIDOMNode> parent;
    PRInt32 offset;
    if (NS_SUCCEEDED(GetNodeLocation(preNode, address_of(parent), &offset)) && parent)
      selection->Collapse(parent, offset+1);
  }
  return rv;
}

NS_IMETHODIMP    
nsHTMLEditor::StripCites()
{
  return nsPlaintextEditor::StripCites();
}

NS_IMETHODIMP    
nsHTMLEditor::Rewrap(PRBool aRespectNewlines)
{
  return nsPlaintextEditor::Rewrap(aRespectNewlines);
}

NS_IMETHODIMP
nsHTMLEditor::InsertAsCitedQuotation(const nsAString & aQuotedText,
                                     const nsAString & aCitation,
                                     PRBool aInsertHTML,
                                     nsIDOMNode **aNodeInserted)
{
  // Don't let anyone insert html into a "plaintext" editor:
  if (IsPlaintextEditor())
  {
    NS_ASSERTION(!aInsertHTML, "InsertAsCitedQuotation: trying to insert html into plaintext editor");
    return InsertAsPlaintextQuotation(aQuotedText, PR_TRUE, aNodeInserted);
  }

  nsCOMPtr<nsIDOMNode> newNode;
  nsresult res = NS_OK;

  // get selection
  nsCOMPtr<nsISelection> selection;
  res = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  if (!selection)
  {
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  }
  else
  {
    nsAutoEditBatch beginBatching(this);
    nsAutoRules beginRulesSniffing(this, kOpInsertQuotation, nsIEditor::eNext);

    // give rules a chance to handle or cancel
    nsTextRulesInfo ruleInfo(nsTextEditRules::kInsertElement);
    PRBool cancel, handled;
    res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
    NS_ENSURE_SUCCESS(res, res);
    if (cancel) return NS_OK; // rules canceled the operation
    if (!handled)
    {
      res = DeleteSelectionAndCreateNode(NS_LITERAL_STRING("blockquote"), getter_AddRefs(newNode));
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_TRUE(newNode, NS_ERROR_NULL_POINTER);

      // Try to set type=cite.  Ignore it if this fails.
      nsCOMPtr<nsIDOMElement> newElement (do_QueryInterface(newNode));
      if (newElement)
      {
        NS_NAMED_LITERAL_STRING(citestr, "cite");
        newElement->SetAttribute(NS_LITERAL_STRING("type"), citestr);

        if (!aCitation.IsEmpty())
          newElement->SetAttribute(citestr, aCitation);

        // Set the selection inside the blockquote so aQuotedText will go there:
        selection->Collapse(newNode, 0);
      }

      if (aInsertHTML)
        res = LoadHTML(aQuotedText);

      else
        res = InsertText(aQuotedText);  // XXX ignore charset

      if (aNodeInserted)
      {
        if (NS_SUCCEEDED(res))
        {
          *aNodeInserted = newNode;
          NS_IF_ADDREF(*aNodeInserted);
        }
      }
    }
  }

  // Set the selection to just after the inserted node:
  if (NS_SUCCEEDED(res) && newNode)
  {
    nsCOMPtr<nsIDOMNode> parent;
    PRInt32 offset;
    if (NS_SUCCEEDED(GetNodeLocation(newNode, address_of(parent), &offset)) && parent)
      selection->Collapse(parent, offset+1);
  }
  return res;
}


void RemoveBodyAndHead(nsIDOMNode *aNode)
{
  if (!aNode) 
    return;
    
  nsCOMPtr<nsIDOMNode> tmp, child, body, head;  
  // find the body and head nodes if any.
  // look only at immediate children of aNode.
  aNode->GetFirstChild(getter_AddRefs(child));
  while (child)
  {
    if (nsTextEditUtils::IsBody(child))
    {
      body = child;
    }
    else if (nsEditor::NodeIsType(child, nsEditProperty::head))
    {
      head = child;
    }
    child->GetNextSibling(getter_AddRefs(tmp));
    child = tmp;
  }
  if (head) 
  {
    aNode->RemoveChild(head, getter_AddRefs(tmp));
  }
  if (body)
  {
    body->GetFirstChild(getter_AddRefs(child));
    while (child)
    {
      aNode->InsertBefore(child, body, getter_AddRefs(tmp));
      body->GetFirstChild(getter_AddRefs(child));
    }
    aNode->RemoveChild(body, getter_AddRefs(tmp));
  }
}

/**
 * This function finds the target node that we will be pasting into. aStart is
 * the context that we're given and aResult will be the target. Initially,
 * *aResult must be NULL.
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
    // If the current result is NULL, then aStart is a leaf, and is the
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
                                                  PRInt32 *outStartOffset,
                                                  PRInt32 *outEndOffset,
                                                  PRBool aTrustedInput)
{
  NS_ENSURE_TRUE(outFragNode && outStartNode && outEndNode, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIDOMDocumentFragment> docfrag;
  nsCOMPtr<nsIDOMNode> contextAsNode, tmp;  
  nsresult res = NS_OK;

  nsCOMPtr<nsIDOMDocument> domDoc;
  GetDocument(getter_AddRefs(domDoc));

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);
  
  // if we have context info, create a fragment for that
  nsAutoTArray<nsString, 32> tagStack;
  nsCOMPtr<nsIDOMDocumentFragment> contextfrag;
  nsCOMPtr<nsIDOMNode> contextLeaf, junk;
  if (!aContextStr.IsEmpty())
  {
    res = ParseFragment(aContextStr, tagStack, doc, address_of(contextAsNode),
                        aTrustedInput);
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(contextAsNode, NS_ERROR_FAILURE);

    res = StripFormattingNodes(contextAsNode);
    NS_ENSURE_SUCCESS(res, res);

    RemoveBodyAndHead(contextAsNode);

    res = FindTargetNode(contextAsNode, contextLeaf);
    if (res == NS_FOUND_TARGET)
      res = NS_OK;
    NS_ENSURE_SUCCESS(res, res);
  }

  // get the tagstack for the context
  res = CreateTagStack(tagStack, contextLeaf);
  NS_ENSURE_SUCCESS(res, res);

  // create fragment for pasted html
  res = ParseFragment(aInputString, tagStack, doc, outFragNode, aTrustedInput);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(*outFragNode, NS_ERROR_FAILURE);

  RemoveBodyAndHead(*outFragNode);

  if (contextAsNode)
  {
    // unite the two trees
    contextLeaf->AppendChild(*outFragNode, getter_AddRefs(junk));
    *outFragNode = contextAsNode;
  }

  res = StripFormattingNodes(*outFragNode, PR_TRUE);
  NS_ENSURE_SUCCESS(res, res);

  // If there was no context, then treat all of the data we did get as the
  // pasted data.
  if (contextLeaf)
    *outEndNode = *outStartNode = contextLeaf;
  else
    *outEndNode = *outStartNode = *outFragNode;

  *outStartOffset = 0;

  // get the infoString contents
  nsAutoString numstr1, numstr2;
  if (!aInfoStr.IsEmpty())
  {
    PRInt32 err, sep, num;
    sep = aInfoStr.FindChar((PRUnichar)',');
    numstr1 = Substring(aInfoStr, 0, sep);
    numstr2 = Substring(aInfoStr, sep+1, aInfoStr.Length() - (sep+1));

    // Move the start and end children.
    num = numstr1.ToInteger(&err);
    while (num--)
    {
      (*outStartNode)->GetFirstChild(getter_AddRefs(tmp));
      NS_ENSURE_TRUE(tmp, NS_ERROR_FAILURE);
      tmp.swap(*outStartNode);
    }

    num = numstr2.ToInteger(&err);
    while (num--)
    {
      (*outEndNode)->GetLastChild(getter_AddRefs(tmp));
      NS_ENSURE_TRUE(tmp, NS_ERROR_FAILURE);
      tmp.swap(*outEndNode);
    }
  }

  GetLengthOfDOMNode(*outEndNode, (PRUint32&)*outEndOffset);
  return res;
}


nsresult nsHTMLEditor::ParseFragment(const nsAString & aFragStr,
                                     nsTArray<nsString> &aTagStack,
                                     nsIDocument* aTargetDocument,
                                     nsCOMPtr<nsIDOMNode> *outNode,
                                     PRBool aTrustedInput)
{
  // figure out if we are parsing full context or not
  PRBool bContext = aTagStack.IsEmpty();

  // create the parser to do the conversion.
  nsresult res;
  nsCOMPtr<nsIParser> parser = do_CreateInstance(kCParserCID, &res);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(parser, NS_ERROR_FAILURE);

  // create the html fragment sink
  nsCOMPtr<nsIContentSink> sink;
  if (aTrustedInput) {
    if (bContext)
      sink = do_CreateInstance(NS_HTMLFRAGMENTSINK2_CONTRACTID);
    else
      sink = do_CreateInstance(NS_HTMLFRAGMENTSINK_CONTRACTID);
  } else {
    if (bContext)
      sink = do_CreateInstance(NS_HTMLPARANOIDFRAGMENTSINK2_CONTRACTID);
    else
      sink = do_CreateInstance(NS_HTMLPARANOIDFRAGMENTSINK_CONTRACTID);

    nsCOMPtr<nsIParanoidFragmentContentSink> paranoidSink(do_QueryInterface(sink));
    NS_ASSERTION(paranoidSink, "Our content sink is paranoid");
    if (bContext) {
      // Allow comments for the context to catch our placeholder cookie
      paranoidSink->AllowComments();
    } else {
      // Allow style elements and attributes for the actual content
      paranoidSink->AllowStyles();
    }
  }

  NS_ENSURE_TRUE(sink, NS_ERROR_FAILURE);
  nsCOMPtr<nsIFragmentContentSink> fragSink(do_QueryInterface(sink));
  NS_ENSURE_TRUE(fragSink, NS_ERROR_FAILURE);

  fragSink->SetTargetDocument(aTargetDocument);

  // parse the fragment
  parser->SetContentSink(sink);
  if (bContext)
    parser->Parse(aFragStr, (void*)0, NS_LITERAL_CSTRING("text/html"), PR_TRUE, eDTDMode_fragment);
  else
    parser->ParseFragment(aFragStr, 0, aTagStack, PR_FALSE, NS_LITERAL_CSTRING("text/html"), eDTDMode_quirks);
  // get the fragment node
  nsCOMPtr<nsIDOMDocumentFragment> contextfrag;
  res = fragSink->GetFragment(PR_TRUE, getter_AddRefs(contextfrag));
  NS_ENSURE_SUCCESS(res, res);
  *outNode = do_QueryInterface(contextfrag);
  
  return res;
}

nsresult nsHTMLEditor::CreateTagStack(nsTArray<nsString> &aTagStack, nsIDOMNode *aNode)
{
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> node= aNode;
  PRBool bSeenBody = PR_FALSE;
  
  while (node) 
  {
    if (nsTextEditUtils::IsBody(node))
      bSeenBody = PR_TRUE;
    nsCOMPtr<nsIDOMNode> temp = node;
    PRUint16 nodeType;
    
    node->GetNodeType(&nodeType);
    if (nsIDOMNode::ELEMENT_NODE == nodeType)
    {
      nsString* tagName = aTagStack.AppendElement();
      NS_ENSURE_TRUE(tagName, NS_ERROR_OUT_OF_MEMORY);

      node->GetNodeName(*tagName);
      // printf("%s\n",NS_LossyConvertUTF16toASCII(tagName).get());
    }

    res = temp->GetParentNode(getter_AddRefs(node));
    NS_ENSURE_SUCCESS(res, res);  
  }
  
  if (!bSeenBody)
  {
      aTagStack.AppendElement(NS_LITERAL_STRING("BODY"));
  }
  return res;
}


nsresult nsHTMLEditor::CreateListOfNodesToPaste(nsIDOMNode  *aFragmentAsNode,
                                                nsCOMArray<nsIDOMNode>& outNodeList,
                                                nsIDOMNode *aStartNode,
                                                PRInt32 aStartOffset,
                                                nsIDOMNode *aEndNode,
                                                PRInt32 aEndOffset)
{
  NS_ENSURE_TRUE(aFragmentAsNode, NS_ERROR_NULL_POINTER);

  nsresult res;

  // if no info was provided about the boundary between context and stream,
  // then assume all is stream.
  if (!aStartNode)
  {
    PRInt32 fragLen;
    res = GetLengthOfDOMNode(aFragmentAsNode, (PRUint32&)fragLen);
    NS_ENSURE_SUCCESS(res, res);

    aStartNode = aFragmentAsNode;
    aStartOffset = 0;
    aEndNode = aFragmentAsNode;
    aEndOffset = fragLen;
  }

  nsCOMPtr<nsIDOMRange> docFragRange =
                          do_CreateInstance("@mozilla.org/content/range;1");
  NS_ENSURE_TRUE(docFragRange, NS_ERROR_OUT_OF_MEMORY);

  res = docFragRange->SetStart(aStartNode, aStartOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = docFragRange->SetEnd(aEndNode, aEndOffset);
  NS_ENSURE_SUCCESS(res, res);

  // now use a subtree iterator over the range to create a list of nodes
  nsTrivialFunctor functor;
  nsDOMSubtreeIterator iter;
  res = iter.Init(docFragRange);
  NS_ENSURE_SUCCESS(res, res);
  res = iter.AppendList(functor, outNodeList);

  return res;
}

nsresult 
nsHTMLEditor::GetListAndTableParents(PRBool aEnd, 
                                     nsCOMArray<nsIDOMNode>& aListOfNodes,
                                     nsCOMArray<nsIDOMNode>& outArray)
{
  PRInt32 listCount = aListOfNodes.Count();
  if (listCount <= 0)
    return NS_ERROR_FAILURE;  // no empty lists, please
    
  // build up list of parents of first (or last) node in list 
  // that are either lists, or tables.  
  PRInt32 idx = 0;
  if (aEnd) idx = listCount-1;
  
  nsCOMPtr<nsIDOMNode>  pNode = aListOfNodes[idx];
  while (pNode)
  {
    if (nsHTMLEditUtils::IsList(pNode) || nsHTMLEditUtils::IsTable(pNode))
    {
      if (!outArray.AppendObject(pNode))
      {
        return NS_ERROR_FAILURE;
      }
    }
    nsCOMPtr<nsIDOMNode> parent;
    pNode->GetParentNode(getter_AddRefs(parent));
    pNode = parent;
  }
  return NS_OK;
}

nsresult
nsHTMLEditor::DiscoverPartialListsAndTables(nsCOMArray<nsIDOMNode>& aPasteNodes,
                                            nsCOMArray<nsIDOMNode>& aListsAndTables,
                                            PRInt32 *outHighWaterMark)
{
  NS_ENSURE_TRUE(outHighWaterMark, NS_ERROR_NULL_POINTER);
  
  *outHighWaterMark = -1;
  PRInt32 listAndTableParents = aListsAndTables.Count();
  
  // scan insertion list for table elements (other than table).
  PRInt32 listCount = aPasteNodes.Count();
  PRInt32 j;  
  for (j=0; j<listCount; j++)
  {
    nsCOMPtr<nsIDOMNode> curNode = aPasteNodes[j];

    NS_ENSURE_TRUE(curNode, NS_ERROR_FAILURE);
    if (nsHTMLEditUtils::IsTableElement(curNode) && !nsHTMLEditUtils::IsTable(curNode))
    {
      nsCOMPtr<nsIDOMNode> theTable = GetTableParent(curNode);
      if (theTable)
      {
        PRInt32 indexT = aListsAndTables.IndexOf(theTable);
        if (indexT >= 0)
        {
          *outHighWaterMark = indexT;
          if (*outHighWaterMark == listAndTableParents-1) break;
        }
        else
        {
          break;
        }
      }
    }
    if (nsHTMLEditUtils::IsListItem(curNode))
    {
      nsCOMPtr<nsIDOMNode> theList = GetListParent(curNode);
      if (theList)
      {
        PRInt32 indexL = aListsAndTables.IndexOf(theList);
        if (indexL >= 0)
        {
          *outHighWaterMark = indexL;
          if (*outHighWaterMark == listAndTableParents-1) break;
        }
        else
        {
          break;
        }
      }
    }
  }
  return NS_OK;
}

nsresult
nsHTMLEditor::ScanForListAndTableStructure( PRBool aEnd,
                                            nsCOMArray<nsIDOMNode>& aNodes,
                                            nsIDOMNode *aListOrTable,
                                            nsCOMPtr<nsIDOMNode> *outReplaceNode)
{
  NS_ENSURE_TRUE(aListOrTable, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(outReplaceNode, NS_ERROR_NULL_POINTER);

  *outReplaceNode = 0;
  
  // look upward from first/last paste node for a piece of this list/table
  PRInt32 listCount = aNodes.Count(), idx = 0;
  if (aEnd) idx = listCount-1;
  PRBool bList = nsHTMLEditUtils::IsList(aListOrTable);
  
  nsCOMPtr<nsIDOMNode>  pNode = aNodes[idx];
  nsCOMPtr<nsIDOMNode>  originalNode = pNode;
  while (pNode)
  {
    if ( (bList && nsHTMLEditUtils::IsListItem(pNode)) ||
         (!bList && (nsHTMLEditUtils::IsTableElement(pNode) && !nsHTMLEditUtils::IsTable(pNode))) )
    {
      nsCOMPtr<nsIDOMNode> structureNode;
      if (bList) structureNode = GetListParent(pNode);
      else structureNode = GetTableParent(pNode);
      if (structureNode == aListOrTable)
      {
        if (bList)
          *outReplaceNode = structureNode;
        else
          *outReplaceNode = pNode;
        break;
      }
    }
    nsCOMPtr<nsIDOMNode> parent;
    pNode->GetParentNode(getter_AddRefs(parent));
    pNode = parent;
  }
  return NS_OK;
}    

nsresult
nsHTMLEditor::ReplaceOrphanedStructure(PRBool aEnd,
                                       nsCOMArray<nsIDOMNode>& aNodeArray,
                                       nsCOMArray<nsIDOMNode>& aListAndTableArray,
                                       PRInt32 aHighWaterMark)
{
  nsCOMPtr<nsIDOMNode> curNode = aListAndTableArray[aHighWaterMark];
  NS_ENSURE_TRUE(curNode, NS_ERROR_NULL_POINTER);
  
  nsCOMPtr<nsIDOMNode> replaceNode, originalNode;
  
  // find substructure of list or table that must be included in paste.
  nsresult res = ScanForListAndTableStructure(aEnd, aNodeArray, 
                                 curNode, address_of(replaceNode));
  NS_ENSURE_SUCCESS(res, res);
  
  // if we found substructure, paste it instead of it's descendants
  if (replaceNode)
  {
    // postprocess list to remove any descendants of this node
    // so that we don't insert them twice.
    nsCOMPtr<nsIDOMNode> endpoint;
    do
    {
      endpoint = GetArrayEndpoint(aEnd, aNodeArray);
      if (!endpoint) break;
      if (nsEditorUtils::IsDescendantOf(endpoint, replaceNode))
        aNodeArray.RemoveObject(endpoint);
      else
        break;
    } while(endpoint);
    
    // now replace the removed nodes with the structural parent
    if (aEnd) aNodeArray.AppendObject(replaceNode);
    else aNodeArray.InsertObjectAt(replaceNode, 0);
  }
  return NS_OK;
}

nsIDOMNode* nsHTMLEditor::GetArrayEndpoint(PRBool aEnd,
                                           nsCOMArray<nsIDOMNode>& aNodeArray)
{
  PRInt32 listCount = aNodeArray.Count();
  if (listCount <= 0) 
    return nsnull;

  if (aEnd)
  {
    return aNodeArray[listCount-1];
  }
  
  return aNodeArray[0];
}
