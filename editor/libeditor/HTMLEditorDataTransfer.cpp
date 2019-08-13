/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/HTMLEditor.h"

#include <string.h>

#include "HTMLEditUtils.h"
#include "InternetCiter.h"
#include "TextEditUtils.h"
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
#include "mozilla/TextEditRules.h"
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
#include "nsIFile.h"
#include "nsIInputStream.h"
#include "nsIMIMEService.h"
#include "nsNameSpaceManager.h"
#include "nsINode.h"
#include "nsIParserUtils.h"
#include "nsIPrincipal.h"
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
static nsresult RemoveFragComments(nsCString& theStr);
static void RemoveBodyAndHead(nsINode& aNode);
static nsresult FindTargetNode(nsINode* aStart, nsCOMPtr<nsINode>& aResult);

nsresult HTMLEditor::LoadHTML(const nsAString& aInputString) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!mRules)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // force IME commit; set up rules sniffing and batching
  CommitComposition();
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }

  AutoPlaceholderBatch treatAsOneTransaction(*this);
  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eInsertHTMLSource, nsIEditor::eNext);

  nsresult rv = EnsureNoPaddingBRElementForEmptyEditor();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Delete Selection, but only if it isn't collapsed, see bug #106269
  if (!SelectionRefPtr()->IsCollapsed()) {
    rv = DeleteSelectionAsSubAction(eNone, eStrip);
    if (NS_WARN_IF(NS_FAILED(rv))) {
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
  for (nsCOMPtr<nsIContent> contentToInsert = documentFragment->GetFirstChild();
       contentToInsert; contentToInsert = documentFragment->GetFirstChild()) {
    rv = InsertNodeWithTransaction(*contentToInsert, pointToInsert);
    if (NS_WARN_IF(NS_FAILED(rv))) {
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

NS_IMETHODIMP
HTMLEditor::InsertHTML(const nsAString& aInString) {
  nsresult rv = InsertHTMLAsAction(aInString);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to insert HTML");
  return rv;
}

nsresult HTMLEditor::InsertHTMLAsAction(const nsAString& aInString,
                                        nsIPrincipal* aPrincipal) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eInsertHTML,
                                          aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = DoInsertHTMLWithContext(aInString, EmptyString(), EmptyString(),
                                        EmptyString(), nullptr,
                                        EditorDOMPoint(), true, true, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

nsresult HTMLEditor::DoInsertHTMLWithContext(
    const nsAString& aInputString, const nsAString& aContextStr,
    const nsAString& aInfoStr, const nsAString& aFlavor, Document* aSourceDoc,
    const EditorDOMPoint& aPointToInsert, bool aDoDeleteSelection,
    bool aTrustedInput, bool aClearStyle) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!mRules)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Prevent the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  // force IME commit; set up rules sniffing and batching
  CommitComposition();
  AutoPlaceholderBatch treatAsOneTransaction(*this);
  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::ePasteHTMLContent, nsIEditor::eNext);

  // create a dom document fragment that represents the structure to paste
  nsCOMPtr<nsINode> fragmentAsNode, streamStartParent, streamEndParent;
  int32_t streamStartOffset = 0, streamEndOffset = 0;

  nsresult rv = CreateDOMFragmentFromPaste(
      aInputString, aContextStr, aInfoStr, address_of(fragmentAsNode),
      address_of(streamStartParent), address_of(streamEndParent),
      &streamStartOffset, &streamEndOffset, aTrustedInput);
  if (NS_WARN_IF(NS_FAILED(rv))) {
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
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // we need to recalculate various things based on potentially new offsets
  // this is work to be completed at a later date (probably by jfrancis)

  // make a list of what nodes in docFrag we need to move
  nsTArray<OwningNonNull<nsINode>> nodeList;
  CreateListOfNodesToPaste(*fragmentAsNode->AsDocumentFragment(), nodeList,
                           streamStartParent, streamStartOffset,
                           streamEndParent, streamEndOffset);

  if (nodeList.IsEmpty()) {
    // We aren't inserting anything, but if aDoDeleteSelection is set, we do
    // want to delete everything.
    // XXX What will this do? We've already called DeleteSelectionAsSubAtion()
    //     above if insertion point is specified.
    if (aDoDeleteSelection) {
      nsresult rv = DeleteSelectionAsSubAction(eNone, eStrip);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
    return NS_OK;
  }

  // Are there any table elements in the list?
  // check for table cell selection mode
  bool cellSelectionMode = false;
  IgnoredErrorResult ignoredError;
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
    if (!HTMLEditUtils::IsTableElement(nodeList[0])) {
      cellSelectionMode = false;
    }
  }

  if (!cellSelectionMode) {
    rv = DeleteSelectionAndPrepareToCreateNode();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (aClearStyle) {
      // pasting does not inherit local inline styles
      nsCOMPtr<nsINode> tmpNode = SelectionRefPtr()->GetAnchorNode();
      int32_t tmpOffset =
          static_cast<int32_t>(SelectionRefPtr()->AnchorOffset());
      rv = ClearStyle(address_of(tmpNode), &tmpOffset, nullptr, nullptr);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
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
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
    // collapse selection to beginning of deleted table content
    IgnoredErrorResult ignoredError;
    SelectionRefPtr()->CollapseToStart(ignoredError);
    NS_WARNING_ASSERTION(!ignoredError.Failed(),
                         "Failed to collapse Selection to start");
  }

  // give rules a chance to handle or cancel
  EditSubActionInfo subActionInfo(EditSubAction::eInsertElement);
  bool cancel, handled;
  rv = rules->WillDoAction(subActionInfo, &cancel, &handled);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (cancel) {
    return NS_OK;  // rules canceled the operation
  }

  if (!handled) {
    // Adjust position based on the first node we are going to insert.
    // FYI: WillDoAction() above might have changed the selection.
    EditorDOMPoint pointToInsert = GetBetterInsertionPointFor(
        nodeList[0], EditorBase::GetStartPoint(*SelectionRefPtr()));
    if (NS_WARN_IF(!pointToInsert.IsSet())) {
      return NS_ERROR_FAILURE;
    }

    // if there are any invisible br's after our insertion point, remove them.
    // this is because if there is a br at end of what we paste, it will make
    // the invisible br visible.
    WSRunObject wsObj(this, pointToInsert);
    if (wsObj.mEndReasonNode && TextEditUtils::IsBreak(wsObj.mEndReasonNode) &&
        !IsVisibleBRElement(wsObj.mEndReasonNode)) {
      AutoEditorDOMPointChildInvalidator lockOffset(pointToInsert);
      rv = DeleteNodeWithTransaction(MOZ_KnownLive(*wsObj.mEndReasonNode));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    // Remember if we are in a link.
    bool bStartedInLink = !!GetLinkElement(pointToInsert.GetContainer());

    // Are we in a text node? If so, split it.
    if (pointToInsert.IsInTextNode()) {
      SplitNodeResult splitNodeResult = SplitNodeDeepWithTransaction(
          MOZ_KnownLive(*pointToInsert.GetContainerAsContent()), pointToInsert,
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
    GetListAndTableParents(StartOrEnd::start, nodeList, startListAndTableArray);

    // remember number of lists and tables above us
    int32_t highWaterMark = -1;
    if (!startListAndTableArray.IsEmpty()) {
      highWaterMark =
          DiscoverPartialListsAndTables(nodeList, startListAndTableArray);
    }

    // if we have pieces of tables or lists to be inserted, let's force the
    // paste to deal with table elements right away, so that it doesn't orphan
    // some table or list contents outside the table or list.
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
      highWaterMark =
          DiscoverPartialListsAndTables(nodeList, endListAndTableArray);
    }

    // don't orphan partial list or table structure
    if (highWaterMark >= 0) {
      ReplaceOrphanedStructure(StartOrEnd::end, nodeList, endListAndTableArray,
                               highWaterMark);
    }

    MOZ_ASSERT(pointToInsert.GetContainer()->GetChildAt_Deprecated(
                   pointToInsert.Offset()) == pointToInsert.GetChild());

    // Loop over the node list and paste the nodes:
    nsCOMPtr<nsINode> parentBlock =
        IsBlockNode(pointToInsert.GetContainer())
            ? pointToInsert.GetContainer()
            : GetBlockNodeParent(pointToInsert.GetContainer());
    nsCOMPtr<nsIContent> lastInsertNode;
    nsCOMPtr<nsINode> insertedContextParent;
    for (OwningNonNull<nsINode>& curNode : nodeList) {
      if (NS_WARN_IF(curNode == fragmentAsNode) ||
          NS_WARN_IF(TextEditUtils::IsBody(curNode))) {
        return NS_ERROR_FAILURE;
      }

      if (insertedContextParent) {
        // If we had to insert something higher up in the paste hierarchy,
        // we want to skip any further paste nodes that descend from that.
        // Else we will paste twice.
        if (EditorUtils::IsDescendantOf(*curNode, *insertedContextParent)) {
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
             firstChild; firstChild = curNode->GetFirstChild()) {
          EditorDOMPoint insertedPoint =
              InsertNodeIntoProperAncestorWithTransaction(
                  *firstChild, pointToInsert,
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
             firstChild; firstChild = curNode->GetFirstChild()) {
          if (HTMLEditUtils::IsListItem(firstChild) ||
              HTMLEditUtils::IsList(firstChild)) {
            // Check if we are pasting into empty list item. If so
            // delete it and paste into parent list instead.
            if (HTMLEditUtils::IsListItem(pointToInsert.GetContainer())) {
              bool isEmpty;
              rv = IsEmptyNode(pointToInsert.GetContainer(), &isEmpty, true);
              if (NS_SUCCEEDED(rv) && isEmpty) {
                if (NS_WARN_IF(
                        !pointToInsert.GetContainer()->GetParentNode())) {
                  // Is it an orphan node?
                } else {
                  DeleteNodeWithTransaction(
                      MOZ_KnownLive(*pointToInsert.GetContainer()));
                  pointToInsert.Set(pointToInsert.GetContainer());
                }
              }
            }
            EditorDOMPoint insertedPoint =
                InsertNodeIntoProperAncestorWithTransaction(
                    *firstChild, pointToInsert,
                    SplitAtEdges::eDoNotCreateEmptyContainer);
            if (NS_WARN_IF(!insertedPoint.IsSet())) {
              break;
            }

            bDidInsert = true;
            lastInsertNode = firstChild;
            pointToInsert = insertedPoint;
            DebugOnly<bool> advanced = pointToInsert.AdvanceOffset();
            NS_WARNING_ASSERTION(
                advanced, "Failed to advance offset from inserted point");
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
             firstChild; firstChild = curNode->GetFirstChild()) {
          EditorDOMPoint insertedPoint =
              InsertNodeIntoProperAncestorWithTransaction(
                  *firstChild, pointToInsert,
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
            InsertNodeIntoProperAncestorWithTransaction(
                MOZ_KnownLive(*curNode->AsContent()), pointToInsert,
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
          insertedPoint = InsertNodeIntoProperAncestorWithTransaction(
              MOZ_KnownLive(*content->GetParent()), pointToInsert,
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
        for (nsINode* parent = selNode; parent && parent != lastInsertNode;
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

      // make sure we don't end up with selection collapsed after an invisible
      // break node
      WSRunObject wsRunObj(this, selNode, selOffset);
      WSType visType;
      wsRunObj.PriorVisibleNode(EditorRawDOMPoint(selNode, selOffset),
                                &visType);
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
          nsCOMPtr<nsINode> visNode;
          int32_t outVisOffset;
          wsRunObj.PriorVisibleNode(EditorRawDOMPoint(selNode, selOffset),
                                    address_of(visNode), &outVisOffset,
                                    &visType);
          if (visType == WSType::text || visType == WSType::normalWS) {
            selNode = visNode;
            selOffset = outVisOffset;  // PriorVisibleNode already set offset to
                                       // _after_ the text or ws
          } else if (visType == WSType::special) {
            // prior visible thing is an image or some other non-text thingy.
            // We want to be right after it.
            atStartReasonNode.Set(wsRunObj.mStartReasonNode);
            selNode = atStartReasonNode.GetContainer();
            selOffset = atStartReasonNode.Offset() + 1;
          }
        }
      }
      DebugOnly<nsresult> rv = SelectionRefPtr()->Collapse(selNode, selOffset);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to collapse Selection");

      // if we just pasted a link, discontinue link style
      nsCOMPtr<nsIContent> linkContent;
      if (!bStartedInLink && (linkContent = GetLinkElement(selNode))) {
        // so, if we just pasted a link, I split it.  Why do that instead of
        // just nudging selection point beyond it?  Because it might have ended
        // in a BR that is not visible.  If so, the code above just placed
        // selection inside that.  So I split it instead.
        SplitNodeResult splitLinkResult = SplitNodeDeepWithTransaction(
            *linkContent, EditorDOMPoint(selNode, selOffset),
            SplitAtEdges::eDoNotCreateEmptyContainer);
        NS_WARNING_ASSERTION(splitLinkResult.Succeeded(),
                             "Failed to split the link");
        if (splitLinkResult.GetPreviousNode()) {
          EditorRawDOMPoint afterLeftLink(splitLinkResult.GetPreviousNode());
          if (afterLeftLink.AdvanceOffset()) {
            DebugOnly<nsresult> rv = SelectionRefPtr()->Collapse(afterLeftLink);
            NS_WARNING_ASSERTION(
                NS_SUCCEEDED(rv),
                "Failed to collapse Selection after the left link");
          }
        }
      }
    }
  }

  rv = rules->DidDoAction(subActionInfo, rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
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

nsresult HTMLEditor::PrepareTransferable(nsITransferable** aTransferable) {
  return NS_OK;
}

nsresult HTMLEditor::PrepareHTMLTransferable(nsITransferable** aTransferable) {
  // Create generic Transferable for getting the data
  nsresult rv =
      CallCreateInstance("@mozilla.org/widget/transferable;1", aTransferable);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the nsITransferable interface for getting the data from the clipboard
  if (aTransferable) {
    RefPtr<Document> destdoc = GetDocument();
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

nsresult RemoveFragComments(nsCString& aStr) {
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
  NS_ENSURE_TRUE(*aStuffToPaste, NS_ERROR_FAILURE);

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
      mSourceDoc(aSourceDoc),
      mPointToInsert(aPointToInsert),
      mEditAction(aHTMLEditor->GetEditAction()),
      mIsSafe(aIsSafe),
      mDoDeleteSelection(aDoDeleteSelection) {
  MOZ_ASSERT(mBlob);
  MOZ_ASSERT(mHTMLEditor);
  MOZ_ASSERT(mHTMLEditor->IsEditActionDataAvailable());
  MOZ_ASSERT(aPointToInsert.IsSet());

  // Take only offset here since it's our traditional behavior.
  AutoEditorDOMPointChildInvalidator storeOnlyWithOffset(mPointToInsert);
}

nsresult HTMLEditor::BlobReader::OnResult(const nsACString& aResult) {
  AutoEditActionDataSetter editActionData(*mHTMLEditor, mEditAction);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsString blobType;
  mBlob->GetType(blobType);

  NS_ConvertUTF16toUTF8 type(blobType);
  nsAutoString stuffToPaste;
  nsresult rv = ImgFromData(type, aResult, stuffToPaste);
  if (NS_WARN_IF(NS_FAILED(rv))) {
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
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
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

  MOZ_CAN_RUN_SCRIPT
  NS_IMETHOD HandleEvent(Event* aEvent) override;

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

NS_IMETHODIMP
SlurpBlobEventListener::HandleEvent(Event* aEvent) {
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
    listener->OnResult(NS_LossyConvertUTF16toASCII(reader->Result()));
  } else if (message == eLoadError) {
    nsAutoString errorMessage;
    reader->GetError()->GetErrorMessage(errorMessage);
    listener->OnError(errorMessage);
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
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = reader->AddEventListener(NS_LITERAL_STRING("error"), eventListener,
                                false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  ErrorResult result;
  reader->ReadAsBinaryString(*aBlob, result);
  if (result.Failed()) {
    return result.StealNSResult();
  }
  return NS_OK;
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
    if (NS_WARN_IF(!domBlob)) {
      return NS_ERROR_FAILURE;
    }

    nsresult rv = SlurpBlob(
        domBlob, aPointToInsert.GetContainer()->OwnerDoc()->GetWindow(), br);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
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
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      nsCOMPtr<nsIInputStream> imageStream = do_QueryInterface(aObject);
      if (NS_WARN_IF(!imageStream)) {
        return NS_ERROR_FAILURE;
      }

      nsresult rv = NS_ConsumeStream(imageStream, UINT32_MAX, imageData);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = imageStream->Close();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    nsAutoString stuffToPaste;
    nsresult rv = ImgFromData(type, imageData, stuffToPaste);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    AutoPlaceholderBatch treatAsOneTransaction(*this);
    rv = DoInsertHTMLWithContext(stuffToPaste, EmptyString(), EmptyString(),
                                 NS_LITERAL_STRING(kFileMime), aSourceDoc,
                                 aPointToInsert, aDoDeleteSelection, aIsSafe,
                                 false);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to insert the image");
  }

  return NS_OK;
}

static bool GetString(nsISupports* aData, nsAString& aText) {
  if (nsCOMPtr<nsISupportsString> str = do_QueryInterface(aData)) {
    str->GetData(aText);
    return !aText.IsEmpty();
  }

  return false;
}

static bool GetCString(nsISupports* aData, nsACString& aText) {
  if (nsCOMPtr<nsISupportsCString> str = do_QueryInterface(aData)) {
    str->GetData(aText);
    return !aText.IsEmpty();
  }

  return false;
}

nsresult HTMLEditor::InsertFromTransferable(nsITransferable* transferable,
                                            Document* aSourceDoc,
                                            const nsAString& aContextStr,
                                            const nsAString& aInfoStr,
                                            bool havePrivateHTMLFlavor,
                                            bool aDoDeleteSelection) {
  nsAutoCString bestFlavor;
  nsCOMPtr<nsISupports> genericDataObj;
  if (NS_SUCCEEDED(transferable->GetAnyTransferData(
          bestFlavor, getter_AddRefs(genericDataObj)))) {
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
      if (NS_WARN_IF(NS_FAILED(rv))) {
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
          if (havePrivateHTMLFlavor) {
            rv = DoInsertHTMLWithContext(cffragment, aContextStr, aInfoStr,
                                         flavor, aSourceDoc, EditorDOMPoint(),
                                         aDoDeleteSelection, isSafe);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              return rv;
            }
          } else {
            rv = DoInsertHTMLWithContext(cffragment, cfcontext, cfselection,
                                         flavor, aSourceDoc, EditorDOMPoint(),
                                         aDoDeleteSelection, isSafe);
            if (NS_WARN_IF(NS_FAILED(rv))) {
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
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
        } else {
          nsresult rv = InsertTextAsSubAction(stuffToPaste);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
        }
      }
    }
  }

  // Try to scroll the selection into view if the paste succeeded
  ScrollSelectionIntoView(false);
  return NS_OK;
}

static void GetStringFromDataTransfer(DataTransfer* aDataTransfer,
                                      const nsAString& aType, int32_t aIndex,
                                      nsAString& aOutputString) {
  nsCOMPtr<nsIVariant> variant;
  aDataTransfer->GetDataAtNoSecurityCheck(aType, aIndex,
                                          getter_AddRefs(variant));
  if (variant) {
    variant->GetAsAString(aOutputString);
  }
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
      if (type.EqualsLiteral(kFileMime) || type.EqualsLiteral(kJPEGImageMime) ||
          type.EqualsLiteral(kJPGImageMime) ||
          type.EqualsLiteral(kPNGImageMime) ||
          type.EqualsLiteral(kGIFImageMime)) {
        nsCOMPtr<nsIVariant> variant;
        aDataTransfer->GetDataAtNoSecurityCheck(type, aIndex,
                                                getter_AddRefs(variant));
        if (variant) {
          nsCOMPtr<nsISupports> object;
          variant->GetAsISupports(getter_AddRefs(object));
          nsresult rv =
              InsertObject(NS_ConvertUTF16toUTF8(type), object, isSafe,
                           aSourceDoc, aDroppedAt, aDoDeleteSelection);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
          return NS_OK;
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
            rv = DoInsertHTMLWithContext(cffragment, contextString, infoString,
                                         type, aSourceDoc, aDroppedAt,
                                         aDoDeleteSelection, isSafe);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              return rv;
            }
            return NS_OK;
          }
          rv = DoInsertHTMLWithContext(cffragment, cfcontext, cfselection, type,
                                       aSourceDoc, aDroppedAt,
                                       aDoDeleteSelection, isSafe);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
          return NS_OK;
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
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
          return NS_OK;
        }
      }
    }

    if (type.EqualsLiteral(kTextMime) || type.EqualsLiteral(kMozTextInternal)) {
      nsAutoString text;
      GetStringFromDataTransfer(aDataTransfer, type, aIndex, text);
      nsresult rv = InsertTextAt(text, aDroppedAt, aDoDeleteSelection);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      return NS_OK;
    }
  }

  return NS_OK;
}

bool HTMLEditor::HavePrivateHTMLFlavor(nsIClipboard* aClipboard) {
  // check the clipboard for our special kHTMLContext flavor.  If that is there,
  // we know we have our own internal html format on clipboard.

  NS_ENSURE_TRUE(aClipboard, false);
  bool bHavePrivateHTMLFlavor = false;

  AutoTArray<nsCString, 1> flavArray = {nsDependentCString(kHTMLContext)};

  if (NS_SUCCEEDED(aClipboard->HasDataMatchingFlavors(
          flavArray, nsIClipboard::kGlobalClipboard,
          &bHavePrivateHTMLFlavor))) {
    return bHavePrivateHTMLFlavor;
  }

  return false;
}

nsresult HTMLEditor::PasteInternal(int32_t aClipboardType,
                                   bool aDispatchPasteEvent) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (aDispatchPasteEvent && !FireClipboardEvent(ePaste, aClipboardType)) {
    return NS_ERROR_EDITOR_ACTION_CANCELED;
  }

  // Get Clipboard Service
  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard =
      do_GetService("@mozilla.org/widget/clipboard;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Get the nsITransferable interface for getting the data from the clipboard
  nsCOMPtr<nsITransferable> transferable;
  rv = PrepareHTMLTransferable(getter_AddRefs(transferable));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (NS_WARN_IF(!transferable)) {
    return NS_ERROR_FAILURE;
  }
  // Get the Data from the clipboard
  rv = clipboard->GetData(transferable, aClipboardType);
  if (NS_WARN_IF(NS_FAILED(rv))) {
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
  bool bHavePrivateHTMLFlavor = HavePrivateHTMLFlavor(clipboard);
  if (bHavePrivateHTMLFlavor) {
    nsCOMPtr<nsITransferable> contextTransferable =
        do_CreateInstance("@mozilla.org/widget/transferable;1");
    if (NS_WARN_IF(!contextTransferable)) {
      return NS_ERROR_FAILURE;
    }
    contextTransferable->Init(nullptr);
    contextTransferable->SetIsPrivateData(transferable->GetIsPrivateData());
    contextTransferable->AddDataFlavor(kHTMLContext);
    clipboard->GetData(contextTransferable, aClipboardType);
    nsCOMPtr<nsISupports> contextDataObj;
    rv = contextTransferable->GetTransferData(kHTMLContext,
                                              getter_AddRefs(contextDataObj));
    if (NS_SUCCEEDED(rv) && contextDataObj) {
      if (nsCOMPtr<nsISupportsString> str = do_QueryInterface(contextDataObj)) {
        str->GetData(contextStr);
      }
    }

    nsCOMPtr<nsITransferable> infoTransferable =
        do_CreateInstance("@mozilla.org/widget/transferable;1");
    if (NS_WARN_IF(!infoTransferable)) {
      return NS_ERROR_FAILURE;
    }
    infoTransferable->Init(nullptr);
    contextTransferable->SetIsPrivateData(transferable->GetIsPrivateData());
    infoTransferable->AddDataFlavor(kHTMLInfo);
    clipboard->GetData(infoTransferable, aClipboardType);
    nsCOMPtr<nsISupports> infoDataObj;
    rv = infoTransferable->GetTransferData(kHTMLInfo,
                                           getter_AddRefs(infoDataObj));
    if (NS_SUCCEEDED(rv) && infoDataObj) {
      if (nsCOMPtr<nsISupportsString> str = do_QueryInterface(infoDataObj)) {
        str->GetData(infoStr);
      }
    }
  }

  rv = InsertFromTransferable(transferable, nullptr, contextStr, infoStr,
                              bHavePrivateHTMLFlavor, true);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
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

  nsAutoString contextStr, infoStr;
  nsresult rv = InsertFromTransferable(aTransferable, nullptr, contextStr,
                                       infoStr, false, true);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

/**
 * HTML PasteNoFormatting. Ignore any HTML styles and formating in paste source.
 */
NS_IMETHODIMP
HTMLEditor::PasteNoFormatting(int32_t aSelectionType) {
  nsresult rv = PasteNoFormattingAsAction(aSelectionType);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to paste without format");
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

  CommitComposition();

  // Get Clipboard Service
  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard(
      do_GetService("@mozilla.org/widget/clipboard;1", &rv));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Get the nsITransferable interface for getting the data from the clipboard.
  // use TextEditor::PrepareTransferable() to force unicode plaintext data.
  nsCOMPtr<nsITransferable> trans;
  rv = TextEditor::PrepareTransferable(getter_AddRefs(trans));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  if (!trans) {
    return NS_OK;
  }

  if (!IsModifiable()) {
    return NS_OK;
  }

  // Get the Data from the clipboard
  rv = clipboard->GetData(trans, aSelectionType);
  if (NS_FAILED(rv)) {
    return rv;
  }

  const nsString& empty = EmptyString();
  rv = InsertFromTransferable(trans, nullptr, empty, empty, false, true);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
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
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  // Use the flavors depending on the current editor mask
  if (IsPlaintextEditor()) {
    AutoTArray<nsCString, ArrayLength(textEditorFlavors)> flavors;
    flavors.AppendElements<const char*>(Span<const char*>(textEditorFlavors));
    bool haveFlavors;
    rv = clipboard->HasDataMatchingFlavors(flavors, aClipboardType,
                                           &haveFlavors);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }
    return haveFlavors;
  }

  AutoTArray<nsCString, ArrayLength(textHtmlEditorFlavors)> flavors;
  flavors.AppendElements<const char*>(Span<const char*>(textHtmlEditorFlavors));
  bool haveFlavors;
  rv = clipboard->HasDataMatchingFlavors(flavors, aClipboardType, &haveFlavors);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }
  return haveFlavors;
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

  AutoEditActionDataSetter editActionData(*this, EditAction::ePasteAsQuotation,
                                          aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  editActionData.InitializeDataTransferWithClipboard(
      SettingDataTransfer::eWithFormat, aClipboardType);

  if (IsPlaintextEditor()) {
    // XXX In this case, we don't dispatch ePaste event.  Why?
    nsresult rv = PasteAsPlaintextQuotation(aClipboardType);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return EditorBase::ToGenericNSResult(rv);
    }
    return NS_OK;
  }

  // If it's not in plain text edit mode, paste text into new
  // <blockquote type="cite"> element after removing selection.

  AutoPlaceholderBatch treatAsOneTransaction(*this);
  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eInsertQuotation, nsIEditor::eNext);

  // Adjust Selection and clear cached style before inserting <blockquote>.
  EditSubActionInfo subActionInfo(EditSubAction::eInsertElement);
  bool cancel, handled;
  RefPtr<TextEditRules> rules(mRules);
  nsresult rv = rules->WillDoAction(subActionInfo, &cancel, &handled);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  if (cancel || handled) {
    return NS_OK;
  }

  // Then, remove Selection and create <blockquote type="cite"> now.
  // XXX Why don't we insert the <blockquote> into the DOM tree after
  //     pasting the content in clipboard into it?
  nsCOMPtr<Element> newNode =
      DeleteSelectionAndCreateElement(*nsGkAtoms::blockquote);
  if (NS_WARN_IF(!newNode)) {
    return NS_ERROR_FAILURE;
  }
  newNode->SetAttr(kNameSpaceID_None, nsGkAtoms::type,
                   NS_LITERAL_STRING("cite"), true);

  // Collapse Selection in the new <blockquote> element.
  rv = SelectionRefPtr()->Collapse(newNode, 0);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // XXX Why don't we call HTMLEditRules::DidDoAction() after Paste()?
  // XXX If ePaste event has not been dispatched yet but selected content
  //     has already been removed and created a <blockquote> element.
  //     So, web apps cannot prevent the default of ePaste event which
  //     will be dispatched by PasteInternal().
  rv = PasteInternal(aClipboardType, aDispatchPasteEvent);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

/**
 * Paste a plaintext quotation.
 */
nsresult HTMLEditor::PasteAsPlaintextQuotation(int32_t aSelectionType) {
  // Get Clipboard Service
  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard(
      do_GetService("@mozilla.org/widget/clipboard;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // Create generic Transferable for getting the data
  nsCOMPtr<nsITransferable> trans =
      do_CreateInstance("@mozilla.org/widget/transferable;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(trans, NS_ERROR_FAILURE);

  RefPtr<Document> destdoc = GetDocument();
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
  nsAutoCString flav;
  rv = trans->GetAnyTransferData(flav, getter_AddRefs(genericDataObj));
  NS_ENSURE_SUCCESS(rv, rv);

  if (flav.EqualsLiteral(kUnicodeMime)) {
    nsAutoString stuffToPaste;
    if (GetString(genericDataObj, stuffToPaste)) {
      AutoPlaceholderBatch treatAsOneTransaction(*this);
      rv = InsertAsPlaintextQuotation(stuffToPaste, true, 0);
    }
  }

  return rv;
}

nsresult HTMLEditor::InsertTextWithQuotations(
    const nsAString& aStringToInsert) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eInsertText);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  MOZ_ASSERT(!aStringToInsert.IsVoid());
  editActionData.SetData(aStringToInsert);

  // The whole operation should be undoable in one transaction:
  // XXX Why isn't enough to use only AutoPlaceholderBatch here?
  AutoTransactionBatch bundleAllTransactions(*this);
  AutoPlaceholderBatch treatAsOneTransaction(*this);

  nsresult rv = InsertTextWithQuotationsInternal(aStringToInsert);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
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
    } else {
      rv = InsertTextAsSubAction(curHunk);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "Failed to insert a line of the quoted text");
    }
    if (!found) {
      break;
    }
    curHunkIsQuoted = quoted;
    hunkStart = lineStart;
  }

  return rv;
}

nsresult HTMLEditor::InsertAsQuotation(const nsAString& aQuotedText,
                                       nsINode** aNodeInserted) {
  if (IsPlaintextEditor()) {
    AutoEditActionDataSetter editActionData(*this, EditAction::eInsertText);
    if (NS_WARN_IF(!editActionData.CanHandle())) {
      return NS_ERROR_NOT_INITIALIZED;
    }
    MOZ_ASSERT(!aQuotedText.IsVoid());
    editActionData.SetData(aQuotedText);
    AutoPlaceholderBatch treatAsOneTransaction(*this);
    nsresult rv = InsertAsPlaintextQuotation(aQuotedText, true, aNodeInserted);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return EditorBase::ToGenericNSResult(rv);
    }
    return NS_OK;
  }

  AutoEditActionDataSetter editActionData(*this,
                                          EditAction::eInsertBlockquoteElement);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  AutoPlaceholderBatch treatAsOneTransaction(*this);
  nsAutoString citation;
  nsresult rv = InsertAsCitedQuotationInternal(aQuotedText, citation, false,
                                               aNodeInserted);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
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

  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eInsertQuotation, nsIEditor::eNext);

  // give rules a chance to handle or cancel
  EditSubActionInfo subActionInfo(EditSubAction::eInsertElement);
  bool cancel, handled;
  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);
  nsresult rv = rules->WillDoAction(subActionInfo, &cancel, &handled);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (cancel || handled) {
    return NS_OK;  // rules canceled the operation
  }

  // Wrap the inserted quote in a <span> so we can distinguish it. If we're
  // inserting into the <body>, we use a <span> which is displayed as a block
  // and sized to the screen using 98 viewport width units.
  // We could use 100vw, but 98vw avoids a horizontal scroll bar where possible.
  // All this is done to wrap overlong lines to the screen and not to the
  // container element, the width-restricted body.
  RefPtr<Element> newNode = DeleteSelectionAndCreateElement(*nsGkAtoms::span);

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
      newNode->SetAttr(
          kNameSpaceID_None, nsGkAtoms::style,
          NS_LITERAL_STRING(
              "white-space: pre-wrap; display: block; width: 98vw;"),
          true);
    } else {
      newNode->SetAttr(kNameSpaceID_None, nsGkAtoms::style,
                       NS_LITERAL_STRING("white-space: pre-wrap;"), true);
    }

    // and set the selection inside it:
    DebugOnly<nsresult> rv = SelectionRefPtr()->Collapse(newNode, 0);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "Failed to collapse selection into the new node");
  }

  if (aAddCites) {
    rv = InsertWithQuotationsAsSubAction(aQuotedText);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    rv = InsertTextAsSubAction(aQuotedText);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (!newNode) {
    return NS_OK;
  }

  // Set the selection to just after the inserted node:
  EditorRawDOMPoint afterNewNode(newNode);
  bool advanced = afterNewNode.AdvanceOffset();
  NS_WARNING_ASSERTION(
      advanced, "Failed to advance offset to after the new <span> element");
  if (advanced) {
    DebugOnly<nsresult> rvIgnored = SelectionRefPtr()->Collapse(afterNewNode);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "Failed to collapse after the new node");
  }

  // Note that if !aAddCites, aNodeInserted isn't set.
  // That's okay because the routines that use aAddCites
  // don't need to know the inserted node.
  if (aNodeInserted) {
    newNode.forget(aNodeInserted);
  }

  // XXX Why don't we call HTMLEditRules::DidDoAction() here?
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::Rewrap(bool aRespectNewlines) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eRewrap);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Rewrap makes no sense if there's no wrap column; default to 72.
  int32_t wrapWidth = WrapWidth();
  if (wrapWidth <= 0) {
    wrapWidth = 72;
  }

  nsAutoString current;
  bool isCollapsed;
  nsresult rv = SharedOutputString(nsIDocumentEncoder::OutputFormatted |
                                       nsIDocumentEncoder::OutputLFLineBreak,
                                   &isCollapsed, current);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }

  nsString wrapped;
  uint32_t firstLineOffset = 0;  // XXX need to reset this if there is a
                                 //     selection
  rv = InternetCiter::Rewrap(current, wrapWidth, firstLineOffset,
                             aRespectNewlines, wrapped);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }

  if (isCollapsed) {
    DebugOnly<nsresult> rv = SelectAllInternal();
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to select all text");
  }

  // The whole operation in InsertTextWithQuotationsInternal() should be
  // undoable in one transaction.
  // XXX Why isn't enough to use only AutoPlaceholderBatch here?
  AutoTransactionBatch bundleAllTransactions(*this);
  AutoPlaceholderBatch treatAsOneTransaction(*this);
  rv = InsertTextWithQuotationsInternal(wrapped);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::InsertAsCitedQuotation(const nsAString& aQuotedText,
                                   const nsAString& aCitation, bool aInsertHTML,
                                   nsINode** aNodeInserted) {
  // Don't let anyone insert HTML when we're in plaintext mode.
  if (IsPlaintextEditor()) {
    NS_ASSERTION(
        !aInsertHTML,
        "InsertAsCitedQuotation: trying to insert html into plaintext editor");

    AutoEditActionDataSetter editActionData(*this, EditAction::eInsertText);
    if (NS_WARN_IF(!editActionData.CanHandle())) {
      return NS_ERROR_NOT_INITIALIZED;
    }
    MOZ_ASSERT(!aQuotedText.IsVoid());
    editActionData.SetData(aQuotedText);

    AutoPlaceholderBatch treatAsOneTransaction(*this);
    nsresult rv = InsertAsPlaintextQuotation(aQuotedText, true, aNodeInserted);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return EditorBase::ToGenericNSResult(rv);
    }
    return NS_OK;
  }

  AutoEditActionDataSetter editActionData(*this,
                                          EditAction::eInsertBlockquoteElement);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  AutoPlaceholderBatch treatAsOneTransaction(*this);
  nsresult rv = InsertAsCitedQuotationInternal(aQuotedText, aCitation,
                                               aInsertHTML, aNodeInserted);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

nsresult HTMLEditor::InsertAsCitedQuotationInternal(
    const nsAString& aQuotedText, const nsAString& aCitation, bool aInsertHTML,
    nsINode** aNodeInserted) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!IsPlaintextEditor());

  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eInsertQuotation, nsIEditor::eNext);

  // give rules a chance to handle or cancel
  EditSubActionInfo subActionInfo(EditSubAction::eInsertElement);
  bool cancel, handled;
  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);
  nsresult rv = rules->WillDoAction(subActionInfo, &cancel, &handled);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (cancel || handled) {
    return NS_OK;  // rules canceled the operation
  }

  RefPtr<Element> newNode =
      DeleteSelectionAndCreateElement(*nsGkAtoms::blockquote);
  if (NS_WARN_IF(!newNode)) {
    return NS_ERROR_FAILURE;
  }

  // Try to set type=cite.  Ignore it if this fails.
  newNode->SetAttr(kNameSpaceID_None, nsGkAtoms::type,
                   NS_LITERAL_STRING("cite"), true);

  if (!aCitation.IsEmpty()) {
    newNode->SetAttr(kNameSpaceID_None, nsGkAtoms::cite, aCitation, true);
  }

  // Set the selection inside the blockquote so aQuotedText will go there:
  DebugOnly<nsresult> rvIgnored = SelectionRefPtr()->Collapse(newNode, 0);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "Failed to collapse Selection in the new <blockquote> element");

  if (aInsertHTML) {
    rv = LoadHTML(aQuotedText);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    rv = InsertTextAsSubAction(aQuotedText);  // XXX ignore charset
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (!newNode) {
    return NS_OK;
  }

  // Set the selection to just after the inserted node:
  EditorRawDOMPoint afterNewNode(newNode);
  bool advanced = afterNewNode.AdvanceOffset();
  NS_WARNING_ASSERTION(
      advanced, "Failed advance offset to after the new <blockquote> element");
  if (advanced) {
    DebugOnly<nsresult> rvIgnored = SelectionRefPtr()->Collapse(afterNewNode);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "Failed to collapse after the new node");
  }

  if (aNodeInserted) {
    newNode.forget(aNodeInserted);
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
nsresult FindTargetNode(nsINode* aStart, nsCOMPtr<nsINode>& aResult) {
  NS_ENSURE_TRUE(aStart, NS_OK);

  nsCOMPtr<nsINode> child = aStart->GetFirstChild();

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
    if (auto* comment = Comment::FromNode(child)) {
      nsAutoString data;
      comment->GetData(data);

      if (data.EqualsLiteral(kInsertCookie)) {
        // Yes it is! Return an error so we bubble out and short-circuit the
        // search.
        aResult = aStart;

        child->Remove();

        return NS_SUCCESS_EDITOR_FOUND_TARGET;
      }
    }

    nsresult rv = FindTargetNode(child, aResult);
    NS_ENSURE_SUCCESS(rv, rv);

    if (rv == NS_SUCCESS_EDITOR_FOUND_TARGET) {
      return NS_SUCCESS_EDITOR_FOUND_TARGET;
    }

    child = child->GetNextSibling();
  } while (child);

  return NS_OK;
}

nsresult HTMLEditor::CreateDOMFragmentFromPaste(
    const nsAString& aInputString, const nsAString& aContextStr,
    const nsAString& aInfoStr, nsCOMPtr<nsINode>* outFragNode,
    nsCOMPtr<nsINode>* outStartNode, nsCOMPtr<nsINode>* outEndNode,
    int32_t* outStartOffset, int32_t* outEndOffset, bool aTrustedInput) {
  NS_ENSURE_TRUE(outFragNode && outStartNode && outEndNode,
                 NS_ERROR_NULL_POINTER);

  RefPtr<Document> doc = GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  // if we have context info, create a fragment for that
  nsresult rv = NS_OK;
  nsCOMPtr<nsINode> contextLeaf;
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
  rv = ParseFragment(aInputString, contextAtom, doc, getter_AddRefs(fragment),
                     aTrustedInput);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(fragment, NS_ERROR_FAILURE);

  RemoveBodyAndHead(*fragment);

  if (contextAsNode) {
    // unite the two trees
    contextLeafAsContent->AppendChild(*fragment, IgnoreErrors());
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
    nsAutoString numstr2(
        Substring(aInfoStr, sep + 1, aInfoStr.Length() - (sep + 1)));

    // Move the start and end children.
    nsresult err;
    int32_t num = numstr1.ToInteger(&err);
    while (num--) {
      nsINode* tmp = (*outStartNode)->GetFirstChild();
      NS_ENSURE_TRUE(tmp, NS_ERROR_FAILURE);
      *outStartNode = tmp;
    }

    num = numstr2.ToInteger(&err);
    while (num--) {
      nsINode* tmp = (*outEndNode)->GetLastChild();
      NS_ENSURE_TRUE(tmp, NS_ERROR_FAILURE);
      *outEndNode = tmp;
    }
  }

  *outEndOffset = (*outEndNode)->Length();
  return NS_OK;
}

nsresult HTMLEditor::ParseFragment(const nsAString& aFragStr,
                                   nsAtom* aContextLocalName,
                                   Document* aTargetDocument,
                                   DocumentFragment** aFragment,
                                   bool aTrustedInput) {
  nsAutoScriptBlockerSuppressNodeRemoved autoBlocker;

  RefPtr<DocumentFragment> fragment =
      new DocumentFragment(aTargetDocument->NodeInfoManager());
  nsresult rv = nsContentUtils::ParseFragmentHTML(
      aFragStr, fragment,
      aContextLocalName ? aContextLocalName : nsGkAtoms::body,
      kNameSpaceID_XHTML, false, true);
  if (!aTrustedInput) {
    nsTreeSanitizer sanitizer(aContextLocalName
                                  ? nsIParserUtils::SanitizerAllowStyle
                                  : nsIParserUtils::SanitizerAllowComments);
    sanitizer.Sanitize(fragment);
  }
  fragment.forget(aFragment);
  return rv;
}

void HTMLEditor::CreateListOfNodesToPaste(
    DocumentFragment& aFragment, nsTArray<OwningNonNull<nsINode>>& outNodeList,
    nsINode* aStartContainer, int32_t aStartOffset, nsINode* aEndContainer,
    int32_t aEndOffset) {
  // If no info was provided about the boundary between context and stream,
  // then assume all is stream.
  if (!aStartContainer) {
    aStartContainer = &aFragment;
    aStartOffset = 0;
    aEndContainer = &aFragment;
    aEndOffset = aFragment.Length();
  }

  RefPtr<nsRange> docFragRange = nsRange::Create(
      aStartContainer, aStartOffset, aEndContainer, aEndOffset, IgnoreErrors());
  if (NS_WARN_IF(!docFragRange)) {
    MOZ_ASSERT(docFragRange);
    return;
  }

  // Now use a subtree iterator over the range to create a list of nodes
  TrivialFunctor functor;
  DOMSubtreeIterator iter;
  if (NS_WARN_IF(NS_FAILED(iter.Init(*docFragRange)))) {
    return;
  }
  iter.AppendList(functor, outNodeList);
}

void HTMLEditor::GetListAndTableParents(
    StartOrEnd aStartOrEnd, nsTArray<OwningNonNull<nsINode>>& aNodeList,
    nsTArray<OwningNonNull<Element>>& outArray) {
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

int32_t HTMLEditor::DiscoverPartialListsAndTables(
    nsTArray<OwningNonNull<nsINode>>& aPasteNodes,
    nsTArray<OwningNonNull<Element>>& aListsAndTables) {
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

nsINode* HTMLEditor::ScanForListAndTableStructure(
    StartOrEnd aStartOrEnd, nsTArray<OwningNonNull<nsINode>>& aNodes,
    Element& aListOrTable) {
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

void HTMLEditor::ReplaceOrphanedStructure(
    StartOrEnd aStartOrEnd, nsTArray<OwningNonNull<nsINode>>& aNodeArray,
    nsTArray<OwningNonNull<Element>>& aListAndTableArray,
    int32_t aHighWaterMark) {
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
    uint32_t idx = aStartOrEnd == StartOrEnd::start ? (i - removedCount)
                                                    : (originalLength - i - 1);
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

}  // namespace mozilla
