/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>

#include "mozilla/Assertions.h"
#include "mozilla/Preferences.h"
#include "mozilla/Selection.h"
#include "mozilla/dom/Element.h"
#include "mozilla/mozalloc.h"
#include "nsAString.h"
#include "nsAlgorithm.h"
#include "nsCOMArray.h"
#include "nsCRT.h"
#include "nsCRTGlue.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsEditProperty.h"
#include "nsEditor.h"
#include "nsEditorUtils.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsHTMLCSSUtils.h"
#include "nsHTMLEditRules.h"
#include "nsHTMLEditUtils.h"
#include "nsHTMLEditor.h"
#include "nsIAtom.h"
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsID.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsIDOMRange.h"
#include "nsIDOMText.h"
#include "nsIEnumerator.h"
#include "nsIHTMLAbsPosEditor.h"
#include "nsIHTMLDocument.h"
#include "nsINode.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsLiteralString.h"
#include "nsPlaintextEditor.h"
#include "nsRange.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "nsTextEditUtils.h"
#include "nsThreadUtils.h"
#include "nsUnicharUtils.h"
#include "nsWSRunObject.h"

class nsISupports;
class nsRulesInfo;

using namespace mozilla;

//const static char* kMOZEditorBogusNodeAttr="MOZ_EDITOR_BOGUS_NODE";
//const static char* kMOZEditorBogusNodeValue="TRUE";

enum
{
  kLonely = 0,
  kPrevSib = 1,
  kNextSib = 2,
  kBothSibs = 3
};

/********************************************************
 *  first some helpful functors we will use
 ********************************************************/

static bool IsBlockNode(nsIDOMNode* node)
{
  bool isBlock (false);
  nsHTMLEditor::NodeIsBlockStatic(node, &isBlock);
  return isBlock;
}

static bool IsInlineNode(nsIDOMNode* node)
{
  return !IsBlockNode(node);
}

static bool
IsStyleCachePreservingAction(nsEditor::OperationID action)
{
  return action == nsEditor::kOpDeleteSelection ||
         action == nsEditor::kOpInsertBreak ||
         action == nsEditor::kOpMakeList ||
         action == nsEditor::kOpIndent ||
         action == nsEditor::kOpOutdent ||
         action == nsEditor::kOpAlign ||
         action == nsEditor::kOpMakeBasicBlock ||
         action == nsEditor::kOpRemoveList ||
         action == nsEditor::kOpMakeDefListItem ||
         action == nsEditor::kOpInsertElement ||
         action == nsEditor::kOpInsertQuotation;
}
 
class nsTableCellAndListItemFunctor : public nsBoolDomIterFunctor
{
  public:
    virtual bool operator()(nsIDOMNode* aNode)  // used to build list of all li's, td's & th's iterator covers
    {
      if (nsHTMLEditUtils::IsTableCell(aNode)) return true;
      if (nsHTMLEditUtils::IsListItem(aNode)) return true;
      return false;
    }
};

class nsBRNodeFunctor : public nsBoolDomIterFunctor
{
  public:
    virtual bool operator()(nsIDOMNode* aNode)  
    {
      if (nsTextEditUtils::IsBreak(aNode)) return true;
      return false;
    }
};

class nsEmptyEditableFunctor : public nsBoolDomIterFunctor
{
  public:
    nsEmptyEditableFunctor(nsHTMLEditor* editor) : mHTMLEditor(editor) {}
    virtual bool operator()(nsIDOMNode* aNode)  
    {
      if (mHTMLEditor->IsEditable(aNode) &&
        (nsHTMLEditUtils::IsListItem(aNode) ||
        nsHTMLEditUtils::IsTableCellOrCaption(aNode)))
      {
        bool bIsEmptyNode;
        nsresult res = mHTMLEditor->IsEmptyNode(aNode, &bIsEmptyNode, false, false);
        NS_ENSURE_SUCCESS(res, false);
        if (bIsEmptyNode)
          return true;
      }
      return false;
    }
  protected:
    nsHTMLEditor* mHTMLEditor;
};

class nsEditableTextFunctor : public nsBoolDomIterFunctor
{
  public:
    nsEditableTextFunctor(nsHTMLEditor* editor) : mHTMLEditor(editor) {}
    virtual bool operator()(nsIDOMNode* aNode)  
    {
      if (nsEditor::IsTextNode(aNode) && mHTMLEditor->IsEditable(aNode)) 
      {
        return true;
      }
      return false;
    }
  protected:
    nsHTMLEditor* mHTMLEditor;
};


/********************************************************
 *  Constructor/Destructor 
 ********************************************************/

nsHTMLEditRules::nsHTMLEditRules() : 
mDocChangeRange(nsnull)
,mListenerEnabled(true)
,mReturnInEmptyLIKillsList(true)
,mDidDeleteSelection(false)
,mDidRangedDelete(false)
,mRestoreContentEditableCount(false)
,mUtilRange(nsnull)
,mJoinOffset(0)
{
  // populate mCachedStyles
  mCachedStyles[0] = StyleCache(nsEditProperty::b, EmptyString(), EmptyString());
  mCachedStyles[1] = StyleCache(nsEditProperty::i, EmptyString(), EmptyString());
  mCachedStyles[2] = StyleCache(nsEditProperty::u, EmptyString(), EmptyString());
  mCachedStyles[3] = StyleCache(nsEditProperty::font, NS_LITERAL_STRING("face"), EmptyString());
  mCachedStyles[4] = StyleCache(nsEditProperty::font, NS_LITERAL_STRING("size"), EmptyString());
  mCachedStyles[5] = StyleCache(nsEditProperty::font, NS_LITERAL_STRING("color"), EmptyString());
  mCachedStyles[6] = StyleCache(nsEditProperty::tt, EmptyString(), EmptyString());
  mCachedStyles[7] = StyleCache(nsEditProperty::em, EmptyString(), EmptyString());
  mCachedStyles[8] = StyleCache(nsEditProperty::strong, EmptyString(), EmptyString());
  mCachedStyles[9] = StyleCache(nsEditProperty::dfn, EmptyString(), EmptyString());
  mCachedStyles[10] = StyleCache(nsEditProperty::code, EmptyString(), EmptyString());
  mCachedStyles[11] = StyleCache(nsEditProperty::samp, EmptyString(), EmptyString());
  mCachedStyles[12] = StyleCache(nsEditProperty::var, EmptyString(), EmptyString());
  mCachedStyles[13] = StyleCache(nsEditProperty::cite, EmptyString(), EmptyString());
  mCachedStyles[14] = StyleCache(nsEditProperty::abbr, EmptyString(), EmptyString());
  mCachedStyles[15] = StyleCache(nsEditProperty::acronym, EmptyString(), EmptyString());
  mCachedStyles[16] = StyleCache(nsEditProperty::cssBackgroundColor, EmptyString(), EmptyString());
  mCachedStyles[17] = StyleCache(nsEditProperty::sub, EmptyString(), EmptyString());
  mCachedStyles[18] = StyleCache(nsEditProperty::sup, EmptyString(), EmptyString());
  mRangeItem = new nsRangeStore();
}

nsHTMLEditRules::~nsHTMLEditRules()
{
  // remove ourselves as a listener to edit actions
  // In some cases, we have already been removed by 
  // ~nsHTMLEditor, in which case we will get a null pointer here
  // which we ignore.  But this allows us to add the ability to
  // switch rule sets on the fly if we want.
  if (mHTMLEditor)
    mHTMLEditor->RemoveEditActionListener(this);
}

/********************************************************
 *  XPCOM Cruft
 ********************************************************/

NS_IMPL_ADDREF_INHERITED(nsHTMLEditRules, nsTextEditRules)
NS_IMPL_RELEASE_INHERITED(nsHTMLEditRules, nsTextEditRules)
NS_IMPL_QUERY_INTERFACE_INHERITED1(nsHTMLEditRules, nsTextEditRules, nsIEditActionListener)


/********************************************************
 *  Public methods 
 ********************************************************/

NS_IMETHODIMP
nsHTMLEditRules::Init(nsPlaintextEditor *aEditor)
{
  mHTMLEditor = static_cast<nsHTMLEditor*>(aEditor);
  nsresult res;
  
  // call through to base class Init 
  res = nsTextEditRules::Init(aEditor);
  NS_ENSURE_SUCCESS(res, res);

  // cache any prefs we care about
  static const char kPrefName[] =
    "editor.html.typing.returnInEmptyListItemClosesList";
  nsAdoptingCString returnInEmptyLIKillsList =
    Preferences::GetCString(kPrefName);

  // only when "false", becomes FALSE.  Otherwise (including empty), TRUE.
  // XXX Why was this pref designed as a string and not bool?
  mReturnInEmptyLIKillsList = !returnInEmptyLIKillsList.EqualsLiteral("false");

  // make a utility range for use by the listenter
  mUtilRange = new nsRange();
   
  // set up mDocChangeRange to be whole doc
  nsCOMPtr<nsIDOMElement> rootElem = do_QueryInterface(mHTMLEditor->GetRoot());
  if (rootElem)
  {
    // temporarily turn off rules sniffing
    nsAutoLockRulesSniffing lockIt((nsTextEditRules*)this);
    if (!mDocChangeRange)
    {
      mDocChangeRange = new nsRange();
    }
    mDocChangeRange->SelectNode(rootElem);
    res = AdjustSpecialBreaks();
    NS_ENSURE_SUCCESS(res, res);
  }

  // add ourselves as a listener to edit actions
  res = mHTMLEditor->AddEditActionListener(this);

  return res;
}

NS_IMETHODIMP
nsHTMLEditRules::DetachEditor()
{
  mHTMLEditor = nsnull;
  return nsTextEditRules::DetachEditor();
}

NS_IMETHODIMP
nsHTMLEditRules::BeforeEdit(nsEditor::OperationID action,
                            nsIEditor::EDirection aDirection)
{
  if (mLockRulesSniffing) return NS_OK;
  
  nsAutoLockRulesSniffing lockIt((nsTextEditRules*)this);
  mDidExplicitlySetInterline = false;

  if (!mActionNesting++)
  {
    // clear our flag about if just deleted a range
    mDidRangedDelete = false;
    
    // remember where our selection was before edit action took place:
    
    // get selection
    nsCOMPtr<nsISelection> selection;
    nsresult res = mHTMLEditor->GetSelection(getter_AddRefs(selection));
    NS_ENSURE_SUCCESS(res, res);
  
    // get the selection start location
    nsCOMPtr<nsIDOMNode> selStartNode, selEndNode;
    PRInt32 selOffset;
    res = mHTMLEditor->GetStartNodeAndOffset(selection, getter_AddRefs(selStartNode), &selOffset);
    NS_ENSURE_SUCCESS(res, res);
    mRangeItem->startNode = selStartNode;
    mRangeItem->startOffset = selOffset;

    // get the selection end location
    res = mHTMLEditor->GetEndNodeAndOffset(selection, getter_AddRefs(selEndNode), &selOffset);
    NS_ENSURE_SUCCESS(res, res);
    mRangeItem->endNode = selEndNode;
    mRangeItem->endOffset = selOffset;

    // register this range with range updater to track this as we perturb the doc
    (mHTMLEditor->mRangeUpdater).RegisterRangeItem(mRangeItem);

    // clear deletion state bool
    mDidDeleteSelection = false;
    
    // clear out mDocChangeRange and mUtilRange
    if(mDocChangeRange)
    {
      // clear out our accounting of what changed
      mDocChangeRange->Reset(); 
    }
    if(mUtilRange)
    {
      // ditto for mUtilRange.
      mUtilRange->Reset(); 
    }

    // remember current inline styles for deletion and normal insertion operations
    if (action == nsEditor::kOpInsertText ||
        action == nsEditor::kOpInsertIMEText ||
        action == nsEditor::kOpDeleteSelection ||
        IsStyleCachePreservingAction(action)) {
      nsCOMPtr<nsIDOMNode> selNode = selStartNode;
      if (aDirection == nsIEditor::eNext)
        selNode = selEndNode;
      res = CacheInlineStyles(selNode);
      NS_ENSURE_SUCCESS(res, res);
    }

    // Stabilize the document against contenteditable count changes
    nsCOMPtr<nsIDOMDocument> doc = mHTMLEditor->GetDOMDocument();
    NS_ENSURE_TRUE(doc, NS_ERROR_NOT_INITIALIZED);
    nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(doc);
    NS_ENSURE_TRUE(htmlDoc, NS_ERROR_FAILURE);
    if (htmlDoc->GetEditingState() == nsIHTMLDocument::eContentEditable) {
      htmlDoc->ChangeContentEditableCount(nsnull, +1);
      mRestoreContentEditableCount = true;
    }

    // check that selection is in subtree defined by body node
    ConfirmSelectionInBody();
    // let rules remember the top level action
    mTheAction = action;
  }
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLEditRules::AfterEdit(nsEditor::OperationID action,
                           nsIEditor::EDirection aDirection)
{
  if (mLockRulesSniffing) return NS_OK;

  nsAutoLockRulesSniffing lockIt(this);

  NS_PRECONDITION(mActionNesting>0, "bad action nesting!");
  nsresult res = NS_OK;
  if (!--mActionNesting)
  {
    // do all the tricky stuff
    res = AfterEditInner(action, aDirection);

    // free up selectionState range item
    (mHTMLEditor->mRangeUpdater).DropRangeItem(mRangeItem);

    // Reset the contenteditable count to its previous value
    if (mRestoreContentEditableCount) {
      nsCOMPtr<nsIDOMDocument> doc = mHTMLEditor->GetDOMDocument();
      NS_ENSURE_TRUE(doc, NS_ERROR_NOT_INITIALIZED);
      nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(doc);
      NS_ENSURE_TRUE(htmlDoc, NS_ERROR_FAILURE);
      if (htmlDoc->GetEditingState() == nsIHTMLDocument::eContentEditable) {
        htmlDoc->ChangeContentEditableCount(nsnull, -1);
      }
      mRestoreContentEditableCount = false;
    }
  }

  return res;
}


nsresult
nsHTMLEditRules::AfterEditInner(nsEditor::OperationID action,
                                nsIEditor::EDirection aDirection)
{
  ConfirmSelectionInBody();
  if (action == nsEditor::kOpIgnore) return NS_OK;
  
  nsCOMPtr<nsISelection>selection;
  nsresult res = mHTMLEditor->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  
  nsCOMPtr<nsIDOMNode> rangeStartParent, rangeEndParent;
  PRInt32 rangeStartOffset = 0, rangeEndOffset = 0;
  // do we have a real range to act on?
  bool bDamagedRange = false;  
  if (mDocChangeRange)
  {  
    mDocChangeRange->GetStartContainer(getter_AddRefs(rangeStartParent));
    mDocChangeRange->GetEndContainer(getter_AddRefs(rangeEndParent));
    mDocChangeRange->GetStartOffset(&rangeStartOffset);
    mDocChangeRange->GetEndOffset(&rangeEndOffset);
    if (rangeStartParent && rangeEndParent) 
      bDamagedRange = true; 
  }
  
  if (bDamagedRange && !((action == nsEditor::kOpUndo) || (action == nsEditor::kOpRedo)))
  {
    // don't let any txns in here move the selection around behind our back.
    // Note that this won't prevent explicit selection setting from working.
    nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
   
    // expand the "changed doc range" as needed
    res = PromoteRange(mDocChangeRange, action);
    NS_ENSURE_SUCCESS(res, res);

    // if we did a ranged deletion, make sure we have a place to put caret.
    // Note we only want to do this if the overall operation was deletion,
    // not if deletion was done along the way for kOpLoadHTML, kOpInsertText, etc.
    // That's why this is here rather than DidDeleteSelection().
    if ((action == nsEditor::kOpDeleteSelection) && mDidRangedDelete)
    {
      res = InsertBRIfNeeded(selection);
      NS_ENSURE_SUCCESS(res, res);
    }  
    
    // add in any needed <br>s, and remove any unneeded ones.
    res = AdjustSpecialBreaks();
    NS_ENSURE_SUCCESS(res, res);
    
    // merge any adjacent text nodes
    if ( (action != nsEditor::kOpInsertText &&
         action != nsEditor::kOpInsertIMEText) )
    {
      res = mHTMLEditor->CollapseAdjacentTextNodes(mDocChangeRange);
      NS_ENSURE_SUCCESS(res, res);
    }

    // clean up any empty nodes in the selection
    res = RemoveEmptyNodes();
    NS_ENSURE_SUCCESS(res, res);

    // attempt to transform any unneeded nbsp's into spaces after doing various operations
    if ((action == nsEditor::kOpInsertText) || 
        (action == nsEditor::kOpInsertIMEText) ||
        (action == nsEditor::kOpDeleteSelection) ||
        (action == nsEditor::kOpInsertBreak) || 
        (action == nsHTMLEditor::kOpHTMLPaste ||
        (action == nsHTMLEditor::kOpLoadHTML)))
    {
      res = AdjustWhitespace(selection);
      NS_ENSURE_SUCCESS(res, res);
      
      // also do this for original selection endpoints. 
      nsWSRunObject(mHTMLEditor, mRangeItem->startNode,
                    mRangeItem->startOffset).AdjustWhitespace();
      // we only need to handle old selection endpoint if it was different from start
      if (mRangeItem->startNode != mRangeItem->endNode ||
          mRangeItem->startOffset != mRangeItem->endOffset) {
        nsWSRunObject(mHTMLEditor, mRangeItem->endNode,
                      mRangeItem->endOffset).AdjustWhitespace();
      }
    }
    
    // if we created a new block, make sure selection lands in it
    if (mNewBlock)
    {
      res = PinSelectionToNewBlock(selection);
      mNewBlock = 0;
    }

    // adjust selection for insert text, html paste, and delete actions
    if ((action == nsEditor::kOpInsertText) || 
        (action == nsEditor::kOpInsertIMEText) ||
        (action == nsEditor::kOpDeleteSelection) ||
        (action == nsEditor::kOpInsertBreak) || 
        (action == nsHTMLEditor::kOpHTMLPaste ||
        (action == nsHTMLEditor::kOpLoadHTML)))
    {
      res = AdjustSelection(selection, aDirection);
      NS_ENSURE_SUCCESS(res, res);
    }

    // check for any styles which were removed inappropriately
    if (action == nsEditor::kOpInsertText ||
        action == nsEditor::kOpInsertIMEText ||
        action == nsEditor::kOpDeleteSelection ||
        IsStyleCachePreservingAction(action)) {
      mHTMLEditor->mTypeInState->UpdateSelState(selection);
      res = ReapplyCachedStyles();
      NS_ENSURE_SUCCESS(res, res);
      res = ClearCachedStyles();
      NS_ENSURE_SUCCESS(res, res);
    }    
  }

  res = mHTMLEditor->HandleInlineSpellCheck(action, selection, 
                                            mRangeItem->startNode,
                                            mRangeItem->startOffset,
                                            rangeStartParent, rangeStartOffset,
                                            rangeEndParent, rangeEndOffset);
  NS_ENSURE_SUCCESS(res, res);

  // detect empty doc
  res = CreateBogusNodeIfNeeded(selection);
  
  // adjust selection HINT if needed
  NS_ENSURE_SUCCESS(res, res);
  
  if (!mDidExplicitlySetInterline)
  {
    res = CheckInterlinePosition(selection);
  }
  
  return res;
}


NS_IMETHODIMP
nsHTMLEditRules::WillDoAction(Selection* aSelection,
                              nsRulesInfo* aInfo,
                              bool* aCancel,
                              bool* aHandled)
{
  MOZ_ASSERT(aInfo && aCancel && aHandled);

  *aCancel = false;
  *aHandled = false;

  // my kingdom for dynamic cast
  nsTextRulesInfo *info = static_cast<nsTextRulesInfo*>(aInfo);

  // Deal with actions for which we don't need to check whether the selection is
  // editable.
  if (info->action == nsEditor::kOpOutputText ||
      info->action == nsEditor::kOpUndo ||
      info->action == nsEditor::kOpRedo) {
    return nsTextEditRules::WillDoAction(aSelection, aInfo, aCancel, aHandled);
  }

  // Nothing to do if there's no selection to act on
  if (!aSelection) {
    return NS_OK;
  }
  NS_ENSURE_TRUE(aSelection->GetRangeCount(), NS_OK);

  nsRefPtr<nsRange> range = aSelection->GetRangeAt(0);
  nsCOMPtr<nsINode> selStartNode = range->GetStartParent();

  if (!mHTMLEditor->IsModifiableNode(selStartNode)) {
    *aCancel = true;
    return NS_OK;
  }

  nsCOMPtr<nsINode> selEndNode = range->GetEndParent();

  if (selStartNode != selEndNode) {
    if (!mHTMLEditor->IsModifiableNode(selEndNode)) {
      *aCancel = true;
      return NS_OK;
    }

    if (!mHTMLEditor->IsModifiableNode(range->GetCommonAncestor())) {
      *aCancel = true;
      return NS_OK;
    }
  }

  switch (info->action) {
    case nsEditor::kOpInsertText:
    case nsEditor::kOpInsertIMEText:
      return WillInsertText(info->action, aSelection, aCancel, aHandled,
                            info->inString, info->outString, info->maxLength);
    case nsEditor::kOpLoadHTML:
      return WillLoadHTML(aSelection, aCancel);
    case nsEditor::kOpInsertBreak:
      return WillInsertBreak(aSelection, aCancel, aHandled);
    case nsEditor::kOpDeleteSelection:
      return WillDeleteSelection(aSelection, info->collapsedAction,
                                 info->stripWrappers, aCancel, aHandled);
    case nsEditor::kOpMakeList:
      return WillMakeList(aSelection, info->blockType, info->entireList,
                          info->bulletType, aCancel, aHandled);
    case nsEditor::kOpIndent:
      return WillIndent(aSelection, aCancel, aHandled);
    case nsEditor::kOpOutdent:
      return WillOutdent(aSelection, aCancel, aHandled);
    case nsEditor::kOpSetAbsolutePosition:
      return WillAbsolutePosition(aSelection, aCancel, aHandled);
    case nsEditor::kOpRemoveAbsolutePosition:
      return WillRemoveAbsolutePosition(aSelection, aCancel, aHandled);
    case nsEditor::kOpAlign:
      return WillAlign(aSelection, info->alignType, aCancel, aHandled);
    case nsEditor::kOpMakeBasicBlock:
      return WillMakeBasicBlock(aSelection, info->blockType, aCancel, aHandled);
    case nsEditor::kOpRemoveList:
      return WillRemoveList(aSelection, info->bOrdered, aCancel, aHandled);
    case nsEditor::kOpMakeDefListItem:
      return WillMakeDefListItem(aSelection, info->blockType, info->entireList,
                                 aCancel, aHandled);
    case nsEditor::kOpInsertElement:
      return WillInsert(aSelection, aCancel);
    case nsEditor::kOpDecreaseZIndex:
      return WillRelativeChangeZIndex(aSelection, -1, aCancel, aHandled);
    case nsEditor::kOpIncreaseZIndex:
      return WillRelativeChangeZIndex(aSelection, 1, aCancel, aHandled);
    default:
      return nsTextEditRules::WillDoAction(aSelection, aInfo,
                                           aCancel, aHandled);
  }
}


NS_IMETHODIMP 
nsHTMLEditRules::DidDoAction(nsISelection *aSelection,
                             nsRulesInfo *aInfo, nsresult aResult)
{
  nsTextRulesInfo *info = static_cast<nsTextRulesInfo*>(aInfo);
  switch (info->action)
  {
    case nsEditor::kOpInsertBreak:
      return DidInsertBreak(aSelection, aResult);
    case nsEditor::kOpDeleteSelection:
      return DidDeleteSelection(aSelection, info->collapsedAction, aResult);
    case nsEditor::kOpMakeBasicBlock:
    case nsEditor::kOpIndent:
    case nsEditor::kOpOutdent:
    case nsEditor::kOpAlign:
      return DidMakeBasicBlock(aSelection, aInfo, aResult);
    case nsEditor::kOpSetAbsolutePosition: {
      nsresult rv = DidMakeBasicBlock(aSelection, aInfo, aResult);
      NS_ENSURE_SUCCESS(rv, rv);
      return DidAbsolutePosition();
    }
    default:
      // pass thru to nsTextEditRules
      return nsTextEditRules::DidDoAction(aSelection, aInfo, aResult);
  }
}
  
nsresult
nsHTMLEditRules::GetListState(bool *aMixed, bool *aOL, bool *aUL, bool *aDL)
{
  NS_ENSURE_TRUE(aMixed && aOL && aUL && aDL, NS_ERROR_NULL_POINTER);
  *aMixed = false;
  *aOL = false;
  *aUL = false;
  *aDL = false;
  bool bNonList = false;
  
  nsCOMArray<nsIDOMNode> arrayOfNodes;
  nsresult res = GetListActionNodes(arrayOfNodes, false, true);
  NS_ENSURE_SUCCESS(res, res);

  // Examine list type for nodes in selection.
  PRInt32 listCount = arrayOfNodes.Count();
  for (PRInt32 i = listCount - 1; i >= 0; --i) {
    nsIDOMNode* curDOMNode = arrayOfNodes[i];
    nsCOMPtr<dom::Element> curElement = do_QueryInterface(curDOMNode);

    if (!curElement) {
      bNonList = true;
    } else if (curElement->IsHTML(nsGkAtoms::ul)) {
      *aUL = true;
    } else if (curElement->IsHTML(nsGkAtoms::ol)) {
      *aOL = true;
    } else if (curElement->IsHTML(nsGkAtoms::li)) {
      if (dom::Element* parent = curElement->GetElementParent()) {
        if (parent->IsHTML(nsGkAtoms::ul)) {
          *aUL = true;
        } else if (parent->IsHTML(nsGkAtoms::ol)) {
          *aOL = true;
        }
      }
    } else if (curElement->IsHTML(nsGkAtoms::dl) ||
               curElement->IsHTML(nsGkAtoms::dt) ||
               curElement->IsHTML(nsGkAtoms::dd)) {
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

nsresult 
nsHTMLEditRules::GetListItemState(bool *aMixed, bool *aLI, bool *aDT, bool *aDD)
{
  NS_ENSURE_TRUE(aMixed && aLI && aDT && aDD, NS_ERROR_NULL_POINTER);
  *aMixed = false;
  *aLI = false;
  *aDT = false;
  *aDD = false;
  bool bNonList = false;
  
  nsCOMArray<nsIDOMNode> arrayOfNodes;
  nsresult res = GetListActionNodes(arrayOfNodes, false, true);
  NS_ENSURE_SUCCESS(res, res);

  // examine list type for nodes in selection
  PRInt32 listCount = arrayOfNodes.Count();
  for (PRInt32 i = listCount - 1; i >= 0; --i) {
    nsIDOMNode* curNode = arrayOfNodes[i];
    nsCOMPtr<dom::Element> element = do_QueryInterface(curNode);
    if (!element) {
      bNonList = true;
    } else if (element->IsHTML(nsGkAtoms::ul) ||
               element->IsHTML(nsGkAtoms::ol) ||
               element->IsHTML(nsGkAtoms::li)) {
      *aLI = true;
    } else if (element->IsHTML(nsGkAtoms::dt)) {
      *aDT = true;
    } else if (element->IsHTML(nsGkAtoms::dd)) {
      *aDD = true;
    } else if (element->IsHTML(nsGkAtoms::dl)) {
      // need to look inside dl and see which types of items it has
      bool bDT, bDD;
      GetDefinitionListItemTypes(element, &bDT, &bDD);
      *aDT |= bDT;
      *aDD |= bDD;
    } else {
      bNonList = true;
    }
  }  
  
  // hokey arithmetic with booleans
  if ( (*aDT + *aDD + bNonList) > 1) *aMixed = true;
  
  return NS_OK;
}

nsresult 
nsHTMLEditRules::GetAlignment(bool *aMixed, nsIHTMLEditor::EAlignment *aAlign)
{
  // for now, just return first alignment.  we'll lie about
  // if it's mixed.  This is for efficiency
  // given that our current ui doesn't care if it's mixed.
  // cmanske: NOT TRUE! We would like to pay attention to mixed state
  //  in Format | Align submenu!

  // this routine assumes that alignment is done ONLY via divs

  // default alignment is left
  NS_ENSURE_TRUE(aMixed && aAlign, NS_ERROR_NULL_POINTER);
  *aMixed = false;
  *aAlign = nsIHTMLEditor::eLeft;

  // get selection
  nsCOMPtr<nsISelection>selection;
  nsresult res = mHTMLEditor->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);

  // get selection location
  nsCOMPtr<nsIDOMElement> rootElem = do_QueryInterface(mHTMLEditor->GetRoot());
  NS_ENSURE_TRUE(rootElem, NS_ERROR_FAILURE);

  PRInt32 offset, rootOffset;
  nsCOMPtr<nsIDOMNode> parent = nsEditor::GetNodeLocation(rootElem, &rootOffset);
  res = mHTMLEditor->GetStartNodeAndOffset(selection, getter_AddRefs(parent), &offset);
  NS_ENSURE_SUCCESS(res, res);

  // is the selection collapsed?
  nsCOMPtr<nsIDOMNode> nodeToExamine;
  if (selection->Collapsed()) {
    // if it is, we want to look at 'parent' and its ancestors
    // for divs with alignment on them
    nodeToExamine = parent;
  }
  else if (mHTMLEditor->IsTextNode(parent)) 
  {
    // if we are in a text node, then that is the node of interest
    nodeToExamine = parent;
  }
  else if (nsEditor::NodeIsType(parent, nsEditProperty::html) &&
           offset == rootOffset)
  {
    // if we have selected the body, let's look at the first editable node
    mHTMLEditor->GetNextNode(parent, offset, true, address_of(nodeToExamine));
  }
  else
  {
    nsCOMArray<nsIDOMRange> arrayOfRanges;
    res = GetPromotedRanges(selection, arrayOfRanges, nsEditor::kOpAlign);
    NS_ENSURE_SUCCESS(res, res);

    // use these ranges to construct a list of nodes to act on.
    nsCOMArray<nsIDOMNode> arrayOfNodes;
    res = GetNodesForOperation(arrayOfRanges, arrayOfNodes,
                               nsEditor::kOpAlign, true);
    NS_ENSURE_SUCCESS(res, res);                                 
    nodeToExamine = arrayOfNodes.SafeObjectAt(0);
  }

  NS_ENSURE_TRUE(nodeToExamine, NS_ERROR_NULL_POINTER);

  NS_NAMED_LITERAL_STRING(typeAttrName, "align");
  nsIAtom  *dummyProperty = nsnull;
  nsCOMPtr<nsIDOMNode> blockParent;
  if (mHTMLEditor->IsBlockNode(nodeToExamine))
    blockParent = nodeToExamine;
  else
    blockParent = mHTMLEditor->GetBlockNodeParent(nodeToExamine);

  NS_ENSURE_TRUE(blockParent, NS_ERROR_FAILURE);

  if (mHTMLEditor->IsCSSEnabled())
  {
    nsCOMPtr<nsIContent> blockParentContent = do_QueryInterface(blockParent);
    if (blockParentContent && 
        mHTMLEditor->mHTMLCSSUtils->IsCSSEditableProperty(blockParentContent, dummyProperty, &typeAttrName))
    {
      // we are in CSS mode and we know how to align this element with CSS
      nsAutoString value;
      // let's get the value(s) of text-align or margin-left/margin-right
      mHTMLEditor->mHTMLCSSUtils->GetCSSEquivalentToHTMLInlineStyleSet(
        blockParentContent, dummyProperty, &typeAttrName, value,
        nsHTMLCSSUtils::eComputed);
      if (value.EqualsLiteral("center") ||
          value.EqualsLiteral("-moz-center") ||
          value.EqualsLiteral("auto auto"))
      {
        *aAlign = nsIHTMLEditor::eCenter;
        return NS_OK;
      }
      if (value.EqualsLiteral("right") ||
          value.EqualsLiteral("-moz-right") ||
          value.EqualsLiteral("auto 0px"))
      {
        *aAlign = nsIHTMLEditor::eRight;
        return NS_OK;
      }
      if (value.EqualsLiteral("justify"))
      {
        *aAlign = nsIHTMLEditor::eJustify;
        return NS_OK;
      }
      *aAlign = nsIHTMLEditor::eLeft;
      return NS_OK;
    }
  }

  // check up the ladder for divs with alignment
  nsCOMPtr<nsIDOMNode> temp = nodeToExamine;
  bool isFirstNodeToExamine = true;
  while (nodeToExamine)
  {
    if (!isFirstNodeToExamine && nsHTMLEditUtils::IsTable(nodeToExamine))
    {
      // the node to examine is a table and this is not the first node
      // we examine; let's break here to materialize the 'inline-block'
      // behaviour of html tables regarding to text alignment
      return NS_OK;
    }
    if (nsHTMLEditUtils::SupportsAlignAttr(nodeToExamine))
    {
      // check for alignment
      nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(nodeToExamine);
      if (elem)
      {
        nsAutoString typeAttrVal;
        res = elem->GetAttribute(NS_LITERAL_STRING("align"), typeAttrVal);
        ToLowerCase(typeAttrVal);
        if (NS_SUCCEEDED(res) && typeAttrVal.Length())
        {
          if (typeAttrVal.EqualsLiteral("center"))
            *aAlign = nsIHTMLEditor::eCenter;
          else if (typeAttrVal.EqualsLiteral("right"))
            *aAlign = nsIHTMLEditor::eRight;
          else if (typeAttrVal.EqualsLiteral("justify"))
            *aAlign = nsIHTMLEditor::eJustify;
          else
            *aAlign = nsIHTMLEditor::eLeft;
          return res;
        }
      }
    }
    isFirstNodeToExamine = false;
    res = nodeToExamine->GetParentNode(getter_AddRefs(temp));
    if (NS_FAILED(res)) temp = nsnull;
    nodeToExamine = temp; 
  }
  return NS_OK;
}

nsIAtom* MarginPropertyAtomForIndent(nsHTMLCSSUtils* aHTMLCSSUtils, nsIDOMNode* aNode) {
  nsAutoString direction;
  aHTMLCSSUtils->GetComputedProperty(aNode, nsEditProperty::cssDirection, direction);
  return direction.EqualsLiteral("rtl") ?
    nsEditProperty::cssMarginRight : nsEditProperty::cssMarginLeft;
}

nsresult 
nsHTMLEditRules::GetIndentState(bool *aCanIndent, bool *aCanOutdent)
{
  NS_ENSURE_TRUE(aCanIndent && aCanOutdent, NS_ERROR_FAILURE);
  *aCanIndent = true;    
  *aCanOutdent = false;

  // get selection
  nsCOMPtr<nsISelection>selection;
  nsresult res = mHTMLEditor->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));
  NS_ENSURE_TRUE(selPriv, NS_ERROR_FAILURE);

  // contruct a list of nodes to act on.
  nsCOMArray<nsIDOMNode> arrayOfNodes;
  res = GetNodesFromSelection(selection, nsEditor::kOpIndent,
                              arrayOfNodes, true);
  NS_ENSURE_SUCCESS(res, res);

  // examine nodes in selection for blockquotes or list elements;
  // these we can outdent.  Note that we return true for canOutdent
  // if *any* of the selection is outdentable, rather than all of it.
  PRInt32 listCount = arrayOfNodes.Count();
  PRInt32 i;
  bool useCSS = mHTMLEditor->IsCSSEnabled();
  for (i=listCount-1; i>=0; i--)
  {
    nsCOMPtr<nsIDOMNode> curNode = arrayOfNodes[i];
    
    if (nsHTMLEditUtils::IsNodeThatCanOutdent(curNode))
    {
      *aCanOutdent = true;
      break;
    }
    else if (useCSS) {
      // we are in CSS mode, indentation is done using the margin-left (or margin-right) property
      nsIAtom* marginProperty = MarginPropertyAtomForIndent(mHTMLEditor->mHTMLCSSUtils, curNode);
      nsAutoString value;
      // retrieve its specified value
      mHTMLEditor->mHTMLCSSUtils->GetSpecifiedProperty(curNode, marginProperty, value);
      float f;
      nsCOMPtr<nsIAtom> unit;
      // get its number part and its unit
      mHTMLEditor->mHTMLCSSUtils->ParseLength(value, &f, getter_AddRefs(unit));
      // if the number part is strictly positive, outdent is possible
      if (0 < f) {
        *aCanOutdent = true;
        break;
      }
    }
  }  
  
  if (!*aCanOutdent)
  {
    // if we haven't found something to outdent yet, also check the parents
    // of selection endpoints.  We might have a blockquote or list item 
    // in the parent hierarchy.
    
    // gather up info we need for test
    nsCOMPtr<nsIDOMNode> parent, tmp, root = do_QueryInterface(mHTMLEditor->GetRoot());
    NS_ENSURE_TRUE(root, NS_ERROR_NULL_POINTER);
    nsCOMPtr<nsISelection> selection;
    PRInt32 selOffset;
    res = mHTMLEditor->GetSelection(getter_AddRefs(selection));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
    
    // test start parent hierarchy
    res = mHTMLEditor->GetStartNodeAndOffset(selection, getter_AddRefs(parent), &selOffset);
    NS_ENSURE_SUCCESS(res, res);
    while (parent && (parent!=root))
    {
      if (nsHTMLEditUtils::IsNodeThatCanOutdent(parent))
      {
        *aCanOutdent = true;
        break;
      }
      tmp=parent;
      tmp->GetParentNode(getter_AddRefs(parent));
    }

    // test end parent hierarchy
    res = mHTMLEditor->GetEndNodeAndOffset(selection, getter_AddRefs(parent), &selOffset);
    NS_ENSURE_SUCCESS(res, res);
    while (parent && (parent!=root))
    {
      if (nsHTMLEditUtils::IsNodeThatCanOutdent(parent))
      {
        *aCanOutdent = true;
        break;
      }
      tmp=parent;
      tmp->GetParentNode(getter_AddRefs(parent));
    }
  }
  return res;
}


nsresult 
nsHTMLEditRules::GetParagraphState(bool *aMixed, nsAString &outFormat)
{
  // This routine is *heavily* tied to our ui choices in the paragraph
  // style popup.  I can't see a way around that.
  NS_ENSURE_TRUE(aMixed, NS_ERROR_NULL_POINTER);
  *aMixed = true;
  outFormat.Truncate(0);
  
  bool bMixed = false;
  // using "x" as an uninitialized value, since "" is meaningful
  nsAutoString formatStr(NS_LITERAL_STRING("x")); 
  
  nsCOMArray<nsIDOMNode> arrayOfNodes;
  nsresult res = GetParagraphFormatNodes(arrayOfNodes, true);
  NS_ENSURE_SUCCESS(res, res);

  // post process list.  We need to replace any block nodes that are not format
  // nodes with their content.  This is so we only have to look "up" the hierarchy
  // to find format nodes, instead of both up and down.
  PRInt32 listCount = arrayOfNodes.Count();
  PRInt32 i;
  for (i=listCount-1; i>=0; i--)
  {
    nsCOMPtr<nsIDOMNode> curNode = arrayOfNodes[i];
    nsAutoString format;
    // if it is a known format node we have it easy
    if (IsBlockNode(curNode) && !nsHTMLEditUtils::IsFormatNode(curNode))
    {
      // arrayOfNodes.RemoveObject(curNode);
      res = AppendInnerFormatNodes(arrayOfNodes, curNode);
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  
  // we might have an empty node list.  if so, find selection parent
  // and put that on the list
  listCount = arrayOfNodes.Count();
  if (!listCount)
  {
    nsCOMPtr<nsIDOMNode> selNode;
    PRInt32 selOffset;
    nsCOMPtr<nsISelection>selection;
    res = mHTMLEditor->GetSelection(getter_AddRefs(selection));
    NS_ENSURE_SUCCESS(res, res);
    res = mHTMLEditor->GetStartNodeAndOffset(selection, getter_AddRefs(selNode), &selOffset);
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(selNode, NS_ERROR_NULL_POINTER);
    arrayOfNodes.AppendObject(selNode);
    listCount = 1;
  }

  // remember root node
  nsCOMPtr<nsIDOMElement> rootElem = do_QueryInterface(mHTMLEditor->GetRoot());
  NS_ENSURE_TRUE(rootElem, NS_ERROR_NULL_POINTER);

  // loop through the nodes in selection and examine their paragraph format
  for (i=listCount-1; i>=0; i--)
  {
    nsCOMPtr<nsIDOMNode> curNode = arrayOfNodes[i];
    nsAutoString format;
    // if it is a known format node we have it easy
    if (nsHTMLEditUtils::IsFormatNode(curNode))
      GetFormatString(curNode, format);
    else if (IsBlockNode(curNode))
    {
      // this is a div or some other non-format block.
      // we should ignore it.  Its children were appended to this list
      // by AppendInnerFormatNodes() call above.  We will get needed
      // info when we examine them instead.
      continue;
    }
    else
    {
      nsCOMPtr<nsIDOMNode> node, tmp = curNode;
      tmp->GetParentNode(getter_AddRefs(node));
      while (node)
      {
        if (node == rootElem)
        {
          format.Truncate(0);
          break;
        }
        else if (nsHTMLEditUtils::IsFormatNode(node))
        {
          GetFormatString(node, format);
          break;
        }
        // else keep looking up
        tmp = node;
        tmp->GetParentNode(getter_AddRefs(node));
      }
    }
    
    // if this is the first node, we've found, remember it as the format
    if (formatStr.EqualsLiteral("x"))
      formatStr = format;
    // else make sure it matches previously found format
    else if (format != formatStr) 
    {
      bMixed = true;
      break; 
    }
  }  
  
  *aMixed = bMixed;
  outFormat = formatStr;
  return res;
}

nsresult
nsHTMLEditRules::AppendInnerFormatNodes(nsCOMArray<nsIDOMNode>& aArray,
                                        nsIDOMNode *aNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);

  return AppendInnerFormatNodes(aArray, node);
}

nsresult
nsHTMLEditRules::AppendInnerFormatNodes(nsCOMArray<nsIDOMNode>& aArray,
                                        nsINode* aNode)
{
  MOZ_ASSERT(aNode);

  // we only need to place any one inline inside this node onto 
  // the list.  They are all the same for purposes of determining
  // paragraph style.  We use foundInline to track this as we are 
  // going through the children in the loop below.
  bool foundInline = false;
  for (nsIContent* child = aNode->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    bool isBlock = IsBlockNode(child->AsDOMNode());
    bool isFormat = child->IsElement() &&
                    nsHTMLEditUtils::IsFormatNode(child->AsElement());
    if (isBlock && !isFormat) {
      // if it's a div, etc, recurse
      AppendInnerFormatNodes(aArray, child);
    } else if (isFormat) {
      aArray.AppendObject(child->AsDOMNode());
    } else if (!foundInline) {
      // if this is the first inline we've found, use it
      foundInline = true;      
      aArray.AppendObject(child->AsDOMNode());
    }
  }
  return NS_OK;
}

nsresult 
nsHTMLEditRules::GetFormatString(nsIDOMNode *aNode, nsAString &outFormat)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);

  if (nsHTMLEditUtils::IsFormatNode(aNode))
  {
    nsCOMPtr<nsIAtom> atom = nsEditor::GetTag(aNode);
    atom->ToString(outFormat);
  }
  else
    outFormat.Truncate();

  return NS_OK;
}    

/********************************************************
 *  Protected rules methods 
 ********************************************************/

nsresult
nsHTMLEditRules::WillInsert(nsISelection *aSelection, bool *aCancel)
{
  nsresult res = nsTextEditRules::WillInsert(aSelection, aCancel);
  NS_ENSURE_SUCCESS(res, res); 
  
  // Adjust selection to prevent insertion after a moz-BR.
  // this next only works for collapsed selections right now,
  // because selection is a pain to work with when not collapsed.
  // (no good way to extend start or end of selection), so we ignore
  // those types of selections.
  if (!aSelection->Collapsed()) {
    return NS_OK;
  }

  // if we are after a mozBR in the same block, then move selection
  // to be before it
  nsCOMPtr<nsIDOMNode> selNode, priorNode;
  PRInt32 selOffset;
  // get the (collapsed) selection location
  res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(selNode),
                                           &selOffset);
  NS_ENSURE_SUCCESS(res, res);
  // get prior node
  res = mHTMLEditor->GetPriorHTMLNode(selNode, selOffset,
                                      address_of(priorNode));
  if (NS_SUCCEEDED(res) && priorNode && nsTextEditUtils::IsMozBR(priorNode))
  {
    nsCOMPtr<nsIDOMNode> block1, block2;
    if (IsBlockNode(selNode)) block1 = selNode;
    else block1 = mHTMLEditor->GetBlockNodeParent(selNode);
    block2 = mHTMLEditor->GetBlockNodeParent(priorNode);
  
    if (block1 == block2)
    {
      // if we are here then the selection is right after a mozBR
      // that is in the same block as the selection.  We need to move
      // the selection start to be before the mozBR.
      selNode = nsEditor::GetNodeLocation(priorNode, &selOffset);
      res = aSelection->Collapse(selNode,selOffset);
      NS_ENSURE_SUCCESS(res, res);
    }
  }

  if (mDidDeleteSelection &&
      (mTheAction == nsEditor::kOpInsertText ||
       mTheAction == nsEditor::kOpInsertIMEText ||
       mTheAction == nsEditor::kOpDeleteSelection)) {
    res = ReapplyCachedStyles();
    NS_ENSURE_SUCCESS(res, res);
  }
  // For most actions we want to clear the cached styles, but there are
  // exceptions
  if (!IsStyleCachePreservingAction(mTheAction)) {
    res = ClearCachedStyles();
    NS_ENSURE_SUCCESS(res, res);
  }

  return NS_OK;
}    

nsresult
nsHTMLEditRules::WillInsertText(nsEditor::OperationID aAction,
                                Selection*       aSelection,
                                bool            *aCancel,
                                bool            *aHandled,
                                const nsAString *inString,
                                nsAString       *outString,
                                PRInt32          aMaxLength)
{  
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }

  if (inString->IsEmpty() && aAction != nsEditor::kOpInsertIMEText) {
    // HACK: this is a fix for bug 19395
    // I can't outlaw all empty insertions
    // because IME transaction depend on them
    // There is more work to do to make the 
    // world safe for IME.
    *aCancel = true;
    *aHandled = false;
    return NS_OK;
  }
  
  // initialize out param
  *aCancel = false;
  *aHandled = true;
  nsresult res;
  nsCOMPtr<nsIDOMNode> selNode;
  PRInt32 selOffset;

  // if the selection isn't collapsed, delete it.
  if (!aSelection->Collapsed()) {
    res = mHTMLEditor->DeleteSelection(nsIEditor::eNone, nsIEditor::eStrip);
    NS_ENSURE_SUCCESS(res, res);
  }

  res = WillInsert(aSelection, aCancel);
  NS_ENSURE_SUCCESS(res, res);
  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = false;

  // we need to get the doc
  nsCOMPtr<nsIDOMDocument> doc = mHTMLEditor->GetDOMDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_NOT_INITIALIZED);

  // for every property that is set, insert a new inline style node
  res = CreateStyleForInsertText(aSelection, doc);
  NS_ENSURE_SUCCESS(res, res);
  
  // get the (collapsed) selection location
  res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(selNode), &selOffset);
  NS_ENSURE_SUCCESS(res, res);

  // dont put text in places that can't have it
  if (!mHTMLEditor->IsTextNode(selNode) &&
      !mHTMLEditor->CanContainTag(selNode, nsGkAtoms::textTagName)) {
    return NS_ERROR_FAILURE;
  }
    
  if (aAction == nsEditor::kOpInsertIMEText) {
    // Right now the nsWSRunObject code bails on empty strings, but IME needs 
    // the InsertTextImpl() call to still happen since empty strings are meaningful there.
    if (inString->IsEmpty())
    {
      res = mHTMLEditor->InsertTextImpl(*inString, address_of(selNode), &selOffset, doc);
    }
    else
    {
      nsWSRunObject wsObj(mHTMLEditor, selNode, selOffset);
      res = wsObj.InsertText(*inString, address_of(selNode), &selOffset, doc);
    }
    NS_ENSURE_SUCCESS(res, res);
  }
  else // aAction == kInsertText
  {
    // find where we are
    nsCOMPtr<nsIDOMNode> curNode = selNode;
    PRInt32 curOffset = selOffset;
    
    // is our text going to be PREformatted?  
    // We remember this so that we know how to handle tabs.
    bool isPRE;
    res = mHTMLEditor->IsPreformatted(selNode, &isPRE);
    NS_ENSURE_SUCCESS(res, res);    
    
    // turn off the edit listener: we know how to
    // build the "doc changed range" ourselves, and it's
    // must faster to do it once here than to track all
    // the changes one at a time.
    nsAutoLockListener lockit(&mListenerEnabled); 
    
    // don't spaz my selection in subtransactions
    nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
    nsAutoString tString(*inString);
    const PRUnichar *unicodeBuf = tString.get();
    nsCOMPtr<nsIDOMNode> unused;
    PRInt32 pos = 0;
    NS_NAMED_LITERAL_STRING(newlineStr, LFSTR);
        
    // for efficiency, break out the pre case separately.  This is because
    // its a lot cheaper to search the input string for only newlines than
    // it is to search for both tabs and newlines.
    if (isPRE || IsPlaintextEditor())
    {
      while (unicodeBuf && (pos != -1) && (pos < (PRInt32)(*inString).Length()))
      {
        PRInt32 oldPos = pos;
        PRInt32 subStrLen;
        pos = tString.FindChar(nsCRT::LF, oldPos);

        if (pos != -1) 
        {
          subStrLen = pos - oldPos;
          // if first char is newline, then use just it
          if (subStrLen == 0)
            subStrLen = 1;
        }
        else
        {
          subStrLen = tString.Length() - oldPos;
          pos = tString.Length();
        }

        nsDependentSubstring subStr(tString, oldPos, subStrLen);
        
        // is it a return?
        if (subStr.Equals(newlineStr))
        {
          res = mHTMLEditor->CreateBRImpl(address_of(curNode), &curOffset, address_of(unused), nsIEditor::eNone);
          pos++;
        }
        else
        {
          res = mHTMLEditor->InsertTextImpl(subStr, address_of(curNode), &curOffset, doc);
        }
        NS_ENSURE_SUCCESS(res, res);
      }
    }
    else
    {
      NS_NAMED_LITERAL_STRING(tabStr, "\t");
      NS_NAMED_LITERAL_STRING(spacesStr, "    ");
      char specialChars[] = {TAB, nsCRT::LF, 0};
      while (unicodeBuf && (pos != -1) && (pos < (PRInt32)inString->Length()))
      {
        PRInt32 oldPos = pos;
        PRInt32 subStrLen;
        pos = tString.FindCharInSet(specialChars, oldPos);
        
        if (pos != -1) 
        {
          subStrLen = pos - oldPos;
          // if first char is newline, then use just it
          if (subStrLen == 0)
            subStrLen = 1;
        }
        else
        {
          subStrLen = tString.Length() - oldPos;
          pos = tString.Length();
        }

        nsDependentSubstring subStr(tString, oldPos, subStrLen);
        nsWSRunObject wsObj(mHTMLEditor, curNode, curOffset);

        // is it a tab?
        if (subStr.Equals(tabStr))
        {
          res = wsObj.InsertText(spacesStr, address_of(curNode), &curOffset, doc);
          NS_ENSURE_SUCCESS(res, res);
          pos++;
        }
        // is it a return?
        else if (subStr.Equals(newlineStr))
        {
          res = wsObj.InsertBreak(address_of(curNode), &curOffset, address_of(unused), nsIEditor::eNone);
          NS_ENSURE_SUCCESS(res, res);
          pos++;
        }
        else
        {
          res = wsObj.InsertText(subStr, address_of(curNode), &curOffset, doc);
          NS_ENSURE_SUCCESS(res, res);
        }
        NS_ENSURE_SUCCESS(res, res);
      }
    }
    nsCOMPtr<nsISelection> selection(aSelection);
    nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));
    selPriv->SetInterlinePosition(false);
    if (curNode) aSelection->Collapse(curNode, curOffset);
    // manually update the doc changed range so that AfterEdit will clean up
    // the correct portion of the document.
    if (!mDocChangeRange)
    {
      mDocChangeRange = new nsRange();
    }
    res = mDocChangeRange->SetStart(selNode, selOffset);
    NS_ENSURE_SUCCESS(res, res);
    if (curNode)
      res = mDocChangeRange->SetEnd(curNode, curOffset);
    else
      res = mDocChangeRange->SetEnd(selNode, selOffset);
    NS_ENSURE_SUCCESS(res, res);
  }
  return res;
}

nsresult
nsHTMLEditRules::WillLoadHTML(nsISelection *aSelection, bool *aCancel)
{
  NS_ENSURE_TRUE(aSelection && aCancel, NS_ERROR_NULL_POINTER);

  *aCancel = false;

  // Delete mBogusNode if it exists. If we really need one,
  // it will be added during post-processing in AfterEditInner().

  if (mBogusNode)
  {
    mEditor->DeleteNode(mBogusNode);
    mBogusNode = nsnull;
  }

  return NS_OK;
}

nsresult
nsHTMLEditRules::WillInsertBreak(Selection* aSelection,
                                 bool* aCancel, bool* aHandled)
{
  if (!aSelection || !aCancel || !aHandled) {
    return NS_ERROR_NULL_POINTER;
  }
  // initialize out params
  *aCancel = false;
  *aHandled = false;

  // if the selection isn't collapsed, delete it.
  nsresult res = NS_OK;
  if (!aSelection->Collapsed()) {
    res = mHTMLEditor->DeleteSelection(nsIEditor::eNone, nsIEditor::eStrip);
    NS_ENSURE_SUCCESS(res, res);
  }

  res = WillInsert(aSelection, aCancel);
  NS_ENSURE_SUCCESS(res, res);

  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = false;

  // split any mailcites in the way.
  // should we abort this if we encounter table cell boundaries?
  if (IsMailEditor()) {
    res = SplitMailCites(aSelection, IsPlaintextEditor(), aHandled);
    NS_ENSURE_SUCCESS(res, res);
    if (*aHandled) {
      return NS_OK;
    }
  }

  // smart splitting rules
  nsCOMPtr<nsIDOMNode> node;
  PRInt32 offset;

  res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(node),
                                           &offset);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

  // do nothing if the node is read-only
  if (!mHTMLEditor->IsModifiableNode(node)) {
    *aCancel = true;
    return NS_OK;
  }

  // identify the block
  nsCOMPtr<nsIDOMNode> blockParent;
  if (IsBlockNode(node)) {
    blockParent = node;
  } else {
    blockParent = mHTMLEditor->GetBlockNodeParent(node);
  }
  NS_ENSURE_TRUE(blockParent, NS_ERROR_FAILURE);

  // if the active editing host is an inline element, or if the active editing
  // host is the block parent itself, just append a br.
  nsCOMPtr<nsIContent> hostContent = mHTMLEditor->GetActiveEditingHost();
  nsCOMPtr<nsIDOMNode> hostNode = do_QueryInterface(hostContent);
  if (!nsEditorUtils::IsDescendantOf(blockParent, hostNode)) {
    res = StandardBreakImpl(node, offset, aSelection);
    NS_ENSURE_SUCCESS(res, res);
    *aHandled = true;
    return NS_OK;
  }

  // if block is empty, populate with br.  (for example, imagine a div that
  // contains the word "text".  the user selects "text" and types return.
  // "text" is deleted leaving an empty block.  we want to put in one br to
  // make block have a line.  then code further below will put in a second br.)
  bool isEmpty;
  IsEmptyBlock(blockParent, &isEmpty);
  if (isEmpty) {
    PRUint32 blockLen;
    res = mHTMLEditor->GetLengthOfDOMNode(blockParent, blockLen);
    NS_ENSURE_SUCCESS(res, res);
    nsCOMPtr<nsIDOMNode> brNode;
    res = mHTMLEditor->CreateBR(blockParent, blockLen, address_of(brNode));
    NS_ENSURE_SUCCESS(res, res);
  }

  nsCOMPtr<nsIDOMNode> listItem = IsInListItem(blockParent);
  if (listItem && listItem != hostNode) {
    ReturnInListItem(aSelection, listItem, node, offset);
    *aHandled = true;
    return NS_OK;
  } else if (nsHTMLEditUtils::IsHeader(blockParent)) {
    // headers: close (or split) header
    ReturnInHeader(aSelection, blockParent, node, offset);
    *aHandled = true;
    return NS_OK;
  } else if (nsHTMLEditUtils::IsParagraph(blockParent)) {
    // paragraphs: special rules to look for <br>s
    res = ReturnInParagraph(aSelection, blockParent, node, offset,
                            aCancel, aHandled);
    NS_ENSURE_SUCCESS(res, res);
    // fall through, we may not have handled it in ReturnInParagraph()
  }

  // if not already handled then do the standard thing
  if (!(*aHandled)) {
    *aHandled = true;
    return StandardBreakImpl(node, offset, aSelection);
  }
  return NS_OK;
}

nsresult
nsHTMLEditRules::StandardBreakImpl(nsIDOMNode* aNode, PRInt32 aOffset,
                                   nsISelection* aSelection)
{
  nsCOMPtr<nsIDOMNode> brNode;
  bool bAfterBlock = false;
  bool bBeforeBlock = false;
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> node(aNode);
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(aSelection));

  if (IsPlaintextEditor()) {
    res = mHTMLEditor->CreateBR(node, aOffset, address_of(brNode));
  } else {
    nsWSRunObject wsObj(mHTMLEditor, node, aOffset);
    nsCOMPtr<nsIDOMNode> visNode, linkNode;
    PRInt32 visOffset = 0, newOffset;
    PRInt16 wsType;
    wsObj.PriorVisibleNode(node, aOffset, address_of(visNode),
                           &visOffset, &wsType);
    if (wsType & nsWSRunObject::eBlock) {
      bAfterBlock = true;
    }
    wsObj.NextVisibleNode(node, aOffset, address_of(visNode),
                          &visOffset, &wsType);
    if (wsType & nsWSRunObject::eBlock) {
      bBeforeBlock = true;
    }
    if (mHTMLEditor->IsInLink(node, address_of(linkNode))) {
      // split the link
      nsCOMPtr<nsIDOMNode> linkParent;
      res = linkNode->GetParentNode(getter_AddRefs(linkParent));
      NS_ENSURE_SUCCESS(res, res);
      res = mHTMLEditor->SplitNodeDeep(linkNode, node, aOffset,
                                       &newOffset, true);
      NS_ENSURE_SUCCESS(res, res);
      // reset {node,aOffset} to the point where link was split
      node = linkParent;
      aOffset = newOffset;
    }
    res = wsObj.InsertBreak(address_of(node), &aOffset,
                            address_of(brNode), nsIEditor::eNone);
  }
  NS_ENSURE_SUCCESS(res, res);
  node = nsEditor::GetNodeLocation(brNode, &aOffset);
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);
  if (bAfterBlock && bBeforeBlock) {
    // we just placed a br between block boundaries.  This is the one case
    // where we want the selection to be before the br we just placed, as the
    // br will be on a new line, rather than at end of prior line.
    selPriv->SetInterlinePosition(true);
    res = aSelection->Collapse(node, aOffset);
  } else {
    nsWSRunObject wsObj(mHTMLEditor, node, aOffset+1);
    nsCOMPtr<nsIDOMNode> secondBR;
    PRInt32 visOffset = 0;
    PRInt16 wsType;
    wsObj.NextVisibleNode(node, aOffset+1, address_of(secondBR),
                          &visOffset, &wsType);
    if (wsType == nsWSRunObject::eBreak) {
      // the next thing after the break we inserted is another break.  Move
      // the 2nd break to be the first breaks sibling.  This will prevent them
      // from being in different inline nodes, which would break
      // SetInterlinePosition().  It will also assure that if the user clicks
      // away and then clicks back on their new blank line, they will still
      // get the style from the line above.
      PRInt32 brOffset;
      nsCOMPtr<nsIDOMNode> brParent = nsEditor::GetNodeLocation(secondBR, &brOffset);
      if (brParent != node || brOffset != aOffset + 1) {
        res = mHTMLEditor->MoveNode(secondBR, node, aOffset+1);
        NS_ENSURE_SUCCESS(res, res);
      }
    }
    // SetInterlinePosition(true) means we want the caret to stick to the
    // content on the "right".  We want the caret to stick to whatever is past
    // the break.  This is because the break is on the same line we were on,
    // but the next content will be on the following line.

    // An exception to this is if the break has a next sibling that is a block
    // node.  Then we stick to the left to avoid an uber caret.
    nsCOMPtr<nsIDOMNode> siblingNode;
    brNode->GetNextSibling(getter_AddRefs(siblingNode));
    if (siblingNode && IsBlockNode(siblingNode)) {
      selPriv->SetInterlinePosition(false);
    } else {
      selPriv->SetInterlinePosition(true);
    }
    res = aSelection->Collapse(node, aOffset+1);
  }
  return res;
}

nsresult
nsHTMLEditRules::DidInsertBreak(nsISelection *aSelection, nsresult aResult)
{
  return NS_OK;
}


nsresult
nsHTMLEditRules::SplitMailCites(nsISelection *aSelection, bool aPlaintext, bool *aHandled)
{
  NS_ENSURE_TRUE(aSelection && aHandled, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(aSelection));
  nsCOMPtr<nsIDOMNode> citeNode, selNode, leftCite, rightCite;
  PRInt32 selOffset, newOffset;
  nsresult res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(selNode), &selOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = GetTopEnclosingMailCite(selNode, address_of(citeNode), aPlaintext);
  NS_ENSURE_SUCCESS(res, res);
  if (citeNode)
  {
    // If our selection is just before a break, nudge it to be
    // just after it.  This does two things for us.  It saves us the trouble of having to add
    // a break here ourselves to preserve the "blockness" of the inline span mailquote
    // (in the inline case), and :
    // it means the break won't end up making an empty line that happens to be inside a
    // mailquote (in either inline or block case).  
    // The latter can confuse a user if they click there and start typing,
    // because being in the mailquote may affect wrapping behavior, or font color, etc.
    nsWSRunObject wsObj(mHTMLEditor, selNode, selOffset);
    nsCOMPtr<nsIDOMNode> visNode;
    PRInt32 visOffset=0;
    PRInt16 wsType;
    wsObj.NextVisibleNode(selNode, selOffset, address_of(visNode),
                          &visOffset, &wsType);
    if (wsType==nsWSRunObject::eBreak)
    {
      // ok, we are just before a break.  is it inside the mailquote?
      PRInt32 unused;
      if (nsEditorUtils::IsDescendantOf(visNode, citeNode, &unused))
      {
        // it is.  so lets reset our selection to be just after it.
        selNode = mHTMLEditor->GetNodeLocation(visNode, &selOffset);
        ++selOffset;
      }
    }
     
    nsCOMPtr<nsIDOMNode> brNode;
    res = mHTMLEditor->SplitNodeDeep(citeNode, selNode, selOffset, &newOffset, 
                       true, address_of(leftCite), address_of(rightCite));
    NS_ENSURE_SUCCESS(res, res);
    res = citeNode->GetParentNode(getter_AddRefs(selNode));
    NS_ENSURE_SUCCESS(res, res);
    res = mHTMLEditor->CreateBR(selNode, newOffset, address_of(brNode));
    NS_ENSURE_SUCCESS(res, res);
    // want selection before the break, and on same line
    selPriv->SetInterlinePosition(true);
    res = aSelection->Collapse(selNode, newOffset);
    NS_ENSURE_SUCCESS(res, res);
    // if citeNode wasn't a block, we might also want another break before it.
    // We need to examine the content both before the br we just added and also
    // just after it.  If we don't have another br or block boundary adjacent,
    // then we will need a 2nd br added to achieve blank line that user expects.
    if (IsInlineNode(citeNode))
    {
      nsWSRunObject wsObj(mHTMLEditor, selNode, newOffset);
      nsCOMPtr<nsIDOMNode> visNode;
      PRInt32 visOffset=0;
      PRInt16 wsType;
      wsObj.PriorVisibleNode(selNode, newOffset, address_of(visNode),
                             &visOffset, &wsType);
      if ((wsType==nsWSRunObject::eNormalWS) || 
          (wsType==nsWSRunObject::eText)     ||
          (wsType==nsWSRunObject::eSpecial))
      {
        nsWSRunObject wsObjAfterBR(mHTMLEditor, selNode, newOffset+1);
        wsObjAfterBR.NextVisibleNode(selNode, newOffset+1, address_of(visNode),
                                     &visOffset, &wsType);
        if ((wsType==nsWSRunObject::eNormalWS) || 
            (wsType==nsWSRunObject::eText)     ||
            (wsType==nsWSRunObject::eSpecial))
        {
          res = mHTMLEditor->CreateBR(selNode, newOffset, address_of(brNode));
          NS_ENSURE_SUCCESS(res, res);
        }
      }
    }
    // delete any empty cites
    bool bEmptyCite = false;
    if (leftCite)
    {
      res = mHTMLEditor->IsEmptyNode(leftCite, &bEmptyCite, true, false);
      if (NS_SUCCEEDED(res) && bEmptyCite)
        res = mHTMLEditor->DeleteNode(leftCite);
      NS_ENSURE_SUCCESS(res, res);
    }
    if (rightCite)
    {
      res = mHTMLEditor->IsEmptyNode(rightCite, &bEmptyCite, true, false);
      if (NS_SUCCEEDED(res) && bEmptyCite)
        res = mHTMLEditor->DeleteNode(rightCite);
      NS_ENSURE_SUCCESS(res, res);
    }
    *aHandled = true;
  }
  return NS_OK;
}


nsresult
nsHTMLEditRules::WillDeleteSelection(Selection* aSelection,
                                     nsIEditor::EDirection aAction,
                                     nsIEditor::EStripWrappers aStripWrappers,
                                     bool* aCancel,
                                     bool* aHandled)
{
  MOZ_ASSERT(aStripWrappers == nsIEditor::eStrip ||
             aStripWrappers == nsIEditor::eNoStrip);

  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = false;
  *aHandled = false;

  // remember that we did a selection deletion.  Used by CreateStyleForInsertText()
  mDidDeleteSelection = true;
  
  // if there is only bogus content, cancel the operation
  if (mBogusNode) 
  {
    *aCancel = true;
    return NS_OK;
  }

  bool bCollapsed = aSelection->Collapsed(), join = false;

  // origCollapsed is used later to determine whether we should join 
  // blocks. We don't really care about bCollapsed because it will be 
  // modified by ExtendSelectionForDelete later. JoinBlocks should 
  // happen if the original selection is collapsed and the cursor is 
  // at the end of a block element, in which case ExtendSelectionForDelete 
  // would always make the selection not collapsed.
  bool origCollapsed = bCollapsed;
  nsCOMPtr<nsIDOMNode> startNode, selNode;
  PRInt32 startOffset, selOffset;
  
  // first check for table selection mode.  If so,
  // hand off to table editor.
  nsCOMPtr<nsIDOMElement> cell;
  nsresult res = mHTMLEditor->GetFirstSelectedCell(nsnull, getter_AddRefs(cell));
  if (NS_SUCCEEDED(res) && cell) {
    res = mHTMLEditor->DeleteTableCellContents();
    *aHandled = true;
    return res;
  }
  cell = NULL;

  res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(startNode), &startOffset);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(startNode, NS_ERROR_FAILURE);

  if (bCollapsed)
  {
    // if we are inside an empty block, delete it.
    nsCOMPtr<nsIContent> hostContent = mHTMLEditor->GetActiveEditingHost();
    nsCOMPtr<nsIDOMNode> hostNode = do_QueryInterface(hostContent);
    NS_ENSURE_TRUE(hostNode, NS_ERROR_FAILURE);
    res = CheckForEmptyBlock(startNode, hostNode, aSelection, aHandled);
    NS_ENSURE_SUCCESS(res, res);
    if (*aHandled) return NS_OK;

    // Test for distance between caret and text that will be deleted
    res = CheckBidiLevelForDeletion(aSelection, startNode, startOffset, aAction, aCancel);
    NS_ENSURE_SUCCESS(res, res);
    if (*aCancel) return NS_OK;

    res = mHTMLEditor->ExtendSelectionForDelete(aSelection, &aAction);
    NS_ENSURE_SUCCESS(res, res);

    // We should delete nothing.
    if (aAction == nsIEditor::eNone)
      return NS_OK;

    // ExtendSelectionForDelete() may have changed the selection, update it
    res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(startNode), &startOffset);
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(startNode, NS_ERROR_FAILURE);
    
    bCollapsed = aSelection->Collapsed();
  }

  if (bCollapsed)
  {
    // what's in the direction we are deleting?
    nsWSRunObject wsObj(mHTMLEditor, startNode, startOffset);
    nsCOMPtr<nsIDOMNode> visNode;
    PRInt32 visOffset;
    PRInt16 wsType;

    // find next visible node
    if (aAction == nsIEditor::eNext)
      wsObj.NextVisibleNode(startNode, startOffset, address_of(visNode),
                            &visOffset, &wsType);
    else
      wsObj.PriorVisibleNode(startNode, startOffset, address_of(visNode),
                             &visOffset, &wsType);
    
    if (!visNode) // can't find anything to delete!
    {
      *aCancel = true;
      return res;
    }
    
    if (wsType==nsWSRunObject::eNormalWS)
    {
      // we found some visible ws to delete.  Let ws code handle it.
      if (aAction == nsIEditor::eNext)
        res = wsObj.DeleteWSForward();
      else
        res = wsObj.DeleteWSBackward();
      *aHandled = true;
      NS_ENSURE_SUCCESS(res, res);
      res = InsertBRIfNeeded(aSelection);
      return res;
    } 
    else if (wsType==nsWSRunObject::eText)
    {
      // found normal text to delete.  
      PRInt32 so = visOffset;
      PRInt32 eo = visOffset+1;
      if (aAction == nsIEditor::ePrevious) 
      { 
        if (so == 0) return NS_ERROR_UNEXPECTED;
        so--; 
        eo--; 
      }
      else
      {
        nsCOMPtr<nsIDOMRange> range;
        res = aSelection->GetRangeAt(0, getter_AddRefs(range));
        NS_ENSURE_SUCCESS(res, res);

#ifdef DEBUG
        nsIDOMNode *container;

        res = range->GetStartContainer(&container);
        NS_ENSURE_SUCCESS(res, res);
        NS_ASSERTION(container == visNode, "selection start not in visNode");

        res = range->GetEndContainer(&container);
        NS_ENSURE_SUCCESS(res, res);
        NS_ASSERTION(container == visNode, "selection end not in visNode");
#endif

        res = range->GetStartOffset(&so);
        NS_ENSURE_SUCCESS(res, res);
        res = range->GetEndOffset(&eo);
        NS_ENSURE_SUCCESS(res, res);
      }
      res = nsWSRunObject::PrepareToDeleteRange(mHTMLEditor, address_of(visNode), &so, address_of(visNode), &eo);
      NS_ENSURE_SUCCESS(res, res);
      nsCOMPtr<nsIDOMCharacterData> nodeAsText(do_QueryInterface(visNode));
      res = mHTMLEditor->DeleteText(nodeAsText, NS_MIN(so, eo), NS_ABS(eo - so));
      *aHandled = true;
      NS_ENSURE_SUCCESS(res, res);    
      res = InsertBRIfNeeded(aSelection);
      return res;
    }
    else if ( (wsType==nsWSRunObject::eSpecial)  || 
              (wsType==nsWSRunObject::eBreak)    ||
              nsHTMLEditUtils::IsHR(visNode) ) 
    {
      // short circuit for invisible breaks.  delete them and recurse.
      if (nsTextEditUtils::IsBreak(visNode) && !mHTMLEditor->IsVisBreak(visNode))
      {
        res = mHTMLEditor->DeleteNode(visNode);
        NS_ENSURE_SUCCESS(res, res);
        return WillDeleteSelection(aSelection, aAction, aStripWrappers,
                                   aCancel, aHandled);
      }
      
      // special handling for backspace when positioned after <hr>
      if (aAction == nsIEditor::ePrevious && nsHTMLEditUtils::IsHR(visNode))
      {
        /*
          Only if the caret is positioned at the end-of-hr-line position,
          we want to delete the <hr>.
          
          In other words, we only want to delete, if
          our selection position (indicated by startNode and startOffset)
          is the position directly after the <hr>,
          on the same line as the <hr>.

          To detect this case we check:
          startNode == parentOfVisNode
          and
          startOffset -1 == visNodeOffsetToVisNodeParent
          and
          interline position is false (left)

          In any other case we set the position to 
          startnode -1 and interlineposition to false,
          only moving the caret to the end-of-hr-line position.
        */

        bool moveOnly = true;

        selNode = nsEditor::GetNodeLocation(visNode, &selOffset);

        bool interLineIsRight;
        res = aSelection->GetInterlinePosition(&interLineIsRight);
        NS_ENSURE_SUCCESS(res, res);

        if (startNode == selNode &&
            startOffset -1 == selOffset &&
            !interLineIsRight)
        {
          moveOnly = false;
        }
        
        if (moveOnly)
        {
          // Go to the position after the <hr>, but to the end of the <hr> line
          // by setting the interline position to left.
          ++selOffset;
          res = aSelection->Collapse(selNode, selOffset);
          aSelection->SetInterlinePosition(false);
          mDidExplicitlySetInterline = true;
          *aHandled = true;

          // There is one exception to the move only case.
          // If the <hr> is followed by a <br> we want to delete the <br>.

          PRInt16 otherWSType;
          nsCOMPtr<nsIDOMNode> otherNode;
          PRInt32 otherOffset;

          wsObj.NextVisibleNode(startNode, startOffset, address_of(otherNode),
                                &otherOffset, &otherWSType);

          if (otherWSType == nsWSRunObject::eBreak)
          {
            // Delete the <br>

            res = nsWSRunObject::PrepareToDeleteNode(mHTMLEditor, otherNode);
            NS_ENSURE_SUCCESS(res, res);
            res = mHTMLEditor->DeleteNode(otherNode);
            NS_ENSURE_SUCCESS(res, res);
          }

          return NS_OK;
        }
        // else continue with normal delete code
      }

      // found break or image, or hr.  
      res = nsWSRunObject::PrepareToDeleteNode(mHTMLEditor, visNode);
      NS_ENSURE_SUCCESS(res, res);
      // remember sibling to visnode, if any
      nsCOMPtr<nsIDOMNode> sibling, stepbrother;
      mHTMLEditor->GetPriorHTMLSibling(visNode, address_of(sibling));
      // delete the node, and join like nodes if appropriate
      res = mHTMLEditor->DeleteNode(visNode);
      NS_ENSURE_SUCCESS(res, res);
      // we did something, so lets say so.
      *aHandled = true;
      // is there a prior node and are they siblings?
      if (sibling)
         mHTMLEditor->GetNextHTMLSibling(sibling, address_of(stepbrother));
      if (startNode == stepbrother) 
      {
        // are they both text nodes?
        if (mHTMLEditor->IsTextNode(startNode) && mHTMLEditor->IsTextNode(sibling))
        {
          // if so, join them!
          res = JoinNodesSmart(sibling, startNode, address_of(selNode), &selOffset);
          NS_ENSURE_SUCCESS(res, res);
          // fix up selection
          res = aSelection->Collapse(selNode, selOffset);
        }
      }
      NS_ENSURE_SUCCESS(res, res);    
      res = InsertBRIfNeeded(aSelection);
      return res;
    }
    else if (wsType==nsWSRunObject::eOtherBlock)
    {
      // make sure it's not a table element.  If so, cancel the operation 
      // (translation: users cannot backspace or delete across table cells)
      if (nsHTMLEditUtils::IsTableElement(visNode))
      {
        *aCancel = true;
        return NS_OK;
      }
      
      // next to a block.  See if we are between a block and a br.  If so, we really
      // want to delete the br.  Else join content at selection to the block.
      
      bool bDeletedBR = false;
      PRInt16 otherWSType;
      nsCOMPtr<nsIDOMNode> otherNode;
      PRInt32 otherOffset;
      
      // find node in other direction
      if (aAction == nsIEditor::eNext)
        wsObj.PriorVisibleNode(startNode, startOffset, address_of(otherNode),
                               &otherOffset, &otherWSType);
      else
        wsObj.NextVisibleNode(startNode, startOffset, address_of(otherNode),
                              &otherOffset, &otherWSType);
      
      // first find the adjacent node in the block
      nsCOMPtr<nsIDOMNode> leafNode, leftNode, rightNode, leftParent, rightParent;
      if (aAction == nsIEditor::ePrevious) 
      {
        res = mHTMLEditor->GetLastEditableLeaf( visNode, address_of(leafNode));
        NS_ENSURE_SUCCESS(res, res);
        leftNode = leafNode;
        rightNode = startNode;
      }
      else
      {
        res = mHTMLEditor->GetFirstEditableLeaf( visNode, address_of(leafNode));
        NS_ENSURE_SUCCESS(res, res);
        leftNode = startNode;
        rightNode = leafNode;
      }
      
      if (nsTextEditUtils::IsBreak(otherNode))
      {
        res = mHTMLEditor->DeleteNode(otherNode);
        NS_ENSURE_SUCCESS(res, res);
        *aHandled = true;
        bDeletedBR = true;
      }
      
      // don't cross table boundaries
      if (leftNode && rightNode)
      {
        bool bInDifTblElems;
        res = InDifferentTableElements(leftNode, rightNode, &bInDifTblElems);
        if (NS_FAILED(res) || bInDifTblElems) return res;
      }
      
      if (bDeletedBR)
      {
        // put selection at edge of block and we are done.
        nsCOMPtr<nsIDOMNode> newSelNode;
        PRInt32 newSelOffset;
        res = GetGoodSelPointForNode(leafNode, aAction, address_of(newSelNode), &newSelOffset);
        NS_ENSURE_SUCCESS(res, res);
        aSelection->Collapse(newSelNode, newSelOffset);
        return res;
      }
      
      // else we are joining content to block
      
      // find the relavent blocks
      if (IsBlockNode(leftNode))
        leftParent = leftNode;
      else if (leftNode)
        leftParent = mHTMLEditor->GetBlockNodeParent(leftNode);
      if (IsBlockNode(rightNode))
        rightParent = rightNode;
      else
        rightParent = mHTMLEditor->GetBlockNodeParent(rightNode);
      
      // sanity checks
      NS_ENSURE_TRUE(leftParent && rightParent, NS_ERROR_NULL_POINTER);  
      if (leftParent == rightParent)
        return NS_ERROR_UNEXPECTED;  
      
      // now join them
      nsCOMPtr<nsIDOMNode> selPointNode = startNode;
      PRInt32 selPointOffset = startOffset;
      {
        nsAutoTrackDOMPoint tracker(mHTMLEditor->mRangeUpdater, address_of(selPointNode), &selPointOffset);
        res = JoinBlocks(address_of(leftParent), address_of(rightParent), aCancel);
        *aHandled = true;
      }
      aSelection->Collapse(selPointNode, selPointOffset);
      return res;
    }
    else if (wsType==nsWSRunObject::eThisBlock)
    {
      // at edge of our block.  Look beside it and see if we can join to an adjacent block
      
      // make sure it's not a table element.  If so, cancel the operation 
      // (translation: users cannot backspace or delete across table cells)
      if (nsHTMLEditUtils::IsTableElement(visNode))
      {
        *aCancel = true;
        return NS_OK;
      }
      
      // first find the relavent nodes
      nsCOMPtr<nsIDOMNode> leftNode, rightNode, leftParent, rightParent;
      if (aAction == nsIEditor::ePrevious) 
      {
        res = mHTMLEditor->GetPriorHTMLNode(visNode, address_of(leftNode));
        NS_ENSURE_SUCCESS(res, res);
        rightNode = startNode;
      }
      else
      {
        res = mHTMLEditor->GetNextHTMLNode( visNode, address_of(rightNode));
        NS_ENSURE_SUCCESS(res, res);
        leftNode = startNode;
      }

      // nothing to join
      if (!leftNode || !rightNode)
      {
        *aCancel = true;
        return NS_OK;
      }

      // don't cross table boundaries
      bool bInDifTblElems;
      res = InDifferentTableElements(leftNode, rightNode, &bInDifTblElems);
      if (NS_FAILED(res) || bInDifTblElems) return res;

      // find the relavent blocks
      if (IsBlockNode(leftNode))
        leftParent = leftNode;
      else
        leftParent = mHTMLEditor->GetBlockNodeParent(leftNode);
      if (IsBlockNode(rightNode))
        rightParent = rightNode;
      else
        rightParent = mHTMLEditor->GetBlockNodeParent(rightNode);
      
      // sanity checks
      NS_ENSURE_TRUE(leftParent && rightParent, NS_ERROR_NULL_POINTER);  
      if (leftParent == rightParent)
        return NS_ERROR_UNEXPECTED;  
      
      // now join them
      nsCOMPtr<nsIDOMNode> selPointNode = startNode;
      PRInt32 selPointOffset = startOffset;
      {
        nsAutoTrackDOMPoint tracker(mHTMLEditor->mRangeUpdater, address_of(selPointNode), &selPointOffset);
        res = JoinBlocks(address_of(leftParent), address_of(rightParent), aCancel);
        *aHandled = true;
      }
      aSelection->Collapse(selPointNode, selPointOffset);
      return res;
    }
  }

  
  // else we have a non collapsed selection
  // first adjust the selection
  res = ExpandSelectionForDeletion(aSelection);
  NS_ENSURE_SUCCESS(res, res);
  
  // remember that we did a ranged delete for the benefit of AfterEditInner().
  mDidRangedDelete = true;
  
  // refresh start and end points
  res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(startNode), &startOffset);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(startNode, NS_ERROR_FAILURE);
  nsCOMPtr<nsIDOMNode> endNode;
  PRInt32 endOffset;
  res = mHTMLEditor->GetEndNodeAndOffset(aSelection, getter_AddRefs(endNode), &endOffset);
  NS_ENSURE_SUCCESS(res, res); 
  NS_ENSURE_TRUE(endNode, NS_ERROR_FAILURE);

  // figure out if the endpoints are in nodes that can be merged  
  // adjust surrounding whitespace in preperation to delete selection
  if (!IsPlaintextEditor())
  {
    nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
    res = nsWSRunObject::PrepareToDeleteRange(mHTMLEditor,
                                            address_of(startNode), &startOffset, 
                                            address_of(endNode), &endOffset);
    NS_ENSURE_SUCCESS(res, res); 
  }
  
  {
    // track location of where we are deleting
    nsAutoTrackDOMPoint startTracker(mHTMLEditor->mRangeUpdater,
                                     address_of(startNode), &startOffset);
    nsAutoTrackDOMPoint endTracker(mHTMLEditor->mRangeUpdater,
                                   address_of(endNode), &endOffset);
    // we are handling all ranged deletions directly now.
    *aHandled = true;
    
    if (endNode == startNode)
    {
      res = mHTMLEditor->DeleteSelectionImpl(aAction, aStripWrappers);
      NS_ENSURE_SUCCESS(res, res); 
    }
    else
    {
      // figure out mailcite ancestors
      nsCOMPtr<nsIDOMNode> endCiteNode, startCiteNode;
      res = GetTopEnclosingMailCite(startNode, address_of(startCiteNode), 
                                    IsPlaintextEditor());
      NS_ENSURE_SUCCESS(res, res); 
      res = GetTopEnclosingMailCite(endNode, address_of(endCiteNode), 
                                    IsPlaintextEditor());
      NS_ENSURE_SUCCESS(res, res); 
      
      // if we only have a mailcite at one of the two endpoints, set the directionality
      // of the deletion so that the selection will end up outside the mailcite.
      if (startCiteNode && !endCiteNode)
      {
        aAction = nsIEditor::eNext;
      }
      else if (!startCiteNode && endCiteNode)
      {
        aAction = nsIEditor::ePrevious;
      }
      
      // figure out block parents
      nsCOMPtr<nsIDOMNode> leftParent;
      nsCOMPtr<nsIDOMNode> rightParent;
      if (IsBlockNode(startNode))
        leftParent = startNode;
      else
        leftParent = mHTMLEditor->GetBlockNodeParent(startNode);
      if (IsBlockNode(endNode))
        rightParent = endNode;
      else
        rightParent = mHTMLEditor->GetBlockNodeParent(endNode);
        
      // are endpoint block parents the same?  use default deletion
      if (leftParent == rightParent) 
      {
        res = mHTMLEditor->DeleteSelectionImpl(aAction, aStripWrappers);
      }
      else
      {
        // deleting across blocks
        // are the blocks of same type?
        NS_ENSURE_STATE(leftParent && rightParent);
        
        // are the blocks siblings?
        nsCOMPtr<nsIDOMNode> leftBlockParent;
        nsCOMPtr<nsIDOMNode> rightBlockParent;
        leftParent->GetParentNode(getter_AddRefs(leftBlockParent));
        rightParent->GetParentNode(getter_AddRefs(rightBlockParent));

        // MOOSE: this could conceivably screw up a table.. fix me.
        if (   (leftBlockParent == rightBlockParent)
            && (mHTMLEditor->NodesSameType(leftParent, rightParent))  )
        {
          if (nsHTMLEditUtils::IsParagraph(leftParent))
          {
            // first delete the selection
            res = mHTMLEditor->DeleteSelectionImpl(aAction, aStripWrappers);
            NS_ENSURE_SUCCESS(res, res);
            // then join para's, insert break
            res = mHTMLEditor->JoinNodeDeep(leftParent,rightParent,address_of(selNode),&selOffset);
            NS_ENSURE_SUCCESS(res, res);
            // fix up selection
            res = aSelection->Collapse(selNode,selOffset);
            return res;
          }
          if (nsHTMLEditUtils::IsListItem(leftParent)
              || nsHTMLEditUtils::IsHeader(leftParent))
          {
            // first delete the selection
            res = mHTMLEditor->DeleteSelectionImpl(aAction, aStripWrappers);
            NS_ENSURE_SUCCESS(res, res);
            // join blocks
            res = mHTMLEditor->JoinNodeDeep(leftParent,rightParent,address_of(selNode),&selOffset);
            NS_ENSURE_SUCCESS(res, res);
            // fix up selection
            res = aSelection->Collapse(selNode,selOffset);
            return res;
          }
        }
        
        // else blocks not same type, or not siblings.  Delete everything except
        // table elements.
        nsCOMPtr<nsIEnumerator> enumerator;
        res = aSelection->GetEnumerator(getter_AddRefs(enumerator));
        NS_ENSURE_SUCCESS(res, res);
        NS_ENSURE_TRUE(enumerator, NS_ERROR_UNEXPECTED);

        join = true;

        for (enumerator->First(); NS_OK!=enumerator->IsDone(); enumerator->Next())
        {
          nsCOMPtr<nsISupports> currentItem;
          res = enumerator->CurrentItem(getter_AddRefs(currentItem));
          NS_ENSURE_SUCCESS(res, res);
          NS_ENSURE_TRUE(currentItem, NS_ERROR_UNEXPECTED);

          // build a list of nodes in the range
          nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
          nsCOMArray<nsIDOMNode> arrayOfNodes;
          nsTrivialFunctor functor;
          nsDOMSubtreeIterator iter;
          res = iter.Init(range);
          NS_ENSURE_SUCCESS(res, res);
          res = iter.AppendList(functor, arrayOfNodes);
          NS_ENSURE_SUCCESS(res, res);
      
          // now that we have the list, delete non table elements
          PRInt32 listCount = arrayOfNodes.Count();
          for (PRInt32 j = 0; j < listCount; j++) {
            nsCOMPtr<nsINode> somenode = do_QueryInterface(arrayOfNodes[0]);
            NS_ENSURE_STATE(somenode);
            DeleteNonTableElements(somenode);
            arrayOfNodes.RemoveObjectAt(0);
            // If something visible is deleted, no need to join.
            // Visible means all nodes except non-visible textnodes and breaks.
            if (join && origCollapsed) {
              if (!somenode->IsContent()) {
                join = false;
                continue;
              }
              nsCOMPtr<nsIContent> content = somenode->AsContent();
              if (content->NodeType() == nsIDOMNode::TEXT_NODE) {
                mHTMLEditor->IsVisTextNode(content, &join, true);
              } else {
                join = content->IsHTML(nsGkAtoms::br) &&
                       !mHTMLEditor->IsVisBreak(somenode->AsDOMNode());
              }
            }
          }
        }
        
        // check endopints for possible text deletion.
        // we can assume that if text node is found, we can
        // delete to end or to begining as appropriate,
        // since the case where both sel endpoints in same
        // text node was already handled (we wouldn't be here)
        if ( mHTMLEditor->IsTextNode(startNode) )
        {
          // delete to last character
          nsCOMPtr<nsIDOMCharacterData>nodeAsText;
          PRUint32 len;
          nodeAsText = do_QueryInterface(startNode);
          nodeAsText->GetLength(&len);
          if (len > (PRUint32)startOffset)
          {
            res = mHTMLEditor->DeleteText(nodeAsText,startOffset,len-startOffset);
            NS_ENSURE_SUCCESS(res, res);
          }
        }
        if ( mHTMLEditor->IsTextNode(endNode) )
        {
          // delete to first character
          nsCOMPtr<nsIDOMCharacterData>nodeAsText;
          nodeAsText = do_QueryInterface(endNode);
          if (endOffset)
          {
            res = mHTMLEditor->DeleteText(nodeAsText,0,endOffset);
            NS_ENSURE_SUCCESS(res, res);
          }
        }

        if (join) {
          res = JoinBlocks(address_of(leftParent), address_of(rightParent),
                           aCancel);
          NS_ENSURE_SUCCESS(res, res);
        }
      }
    }
  }
  //If we're joining blocks: if deleting forward the selection should be 
  //collapsed to the end of the selection, if deleting backward the selection 
  //should be collapsed to the beginning of the selection. But if we're not 
  //joining then the selection should collapse to the beginning of the 
  //selection if we'redeleting forward, because the end of the selection will 
  //still be in the next block. And same thing for deleting backwards 
  //(selection should collapse to the end, because the beginning will still 
  //be in the first block). See Bug 507936
  if (join ? aAction == nsIEditor::eNext : aAction == nsIEditor::ePrevious)
  {
    res = aSelection->Collapse(endNode,endOffset);
  }
  else
  {
    res = aSelection->Collapse(startNode,startOffset);
  }
  return res;
}  


/*****************************************************************************************************
*    InsertBRIfNeeded: determines if a br is needed for current selection to not be spastic.
*    If so, it inserts one.  Callers responsibility to only call with collapsed selection.
*         nsISelection *aSelection      the collapsed selection 
*/
nsresult
nsHTMLEditRules::InsertBRIfNeeded(nsISelection *aSelection)
{
  NS_ENSURE_TRUE(aSelection, NS_ERROR_NULL_POINTER);
  
  // get selection  
  nsCOMPtr<nsIDOMNode> node;
  PRInt32 offset;
  nsresult res = mEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(node), &offset);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

  // inline elements don't need any br
  if (!IsBlockNode(node))
    return res;

  // examine selection
  nsWSRunObject wsObj(mHTMLEditor, node, offset);
  if (((wsObj.mStartReason & nsWSRunObject::eBlock) || (wsObj.mStartReason & nsWSRunObject::eBreak))
      && (wsObj.mEndReason & nsWSRunObject::eBlock))
  {
    // if we are tucked between block boundaries then insert a br
    // first check that we are allowed to
    if (mHTMLEditor->CanContainTag(node, nsGkAtoms::br)) {
      nsCOMPtr<nsIDOMNode> brNode;
      res = mHTMLEditor->CreateBR(node, offset, address_of(brNode), nsIEditor::ePrevious);
    }
  }
  return res;
}

/*****************************************************************************************************
*    GetGoodSelPointForNode: Finds where at a node you would want to set the selection if you were
*    trying to have a caret next to it.
*         nsIDOMNode *aNode                  the node 
*         nsIEditor::EDirection aAction      which edge to find: eNext indicates beginning, ePrevious ending
*         nsCOMPtr<nsIDOMNode> *outSelNode   desired sel node
*         PRInt32 *outSelOffset              desired sel offset
*/
nsresult
nsHTMLEditRules::GetGoodSelPointForNode(nsIDOMNode *aNode, nsIEditor::EDirection aAction, 
                                        nsCOMPtr<nsIDOMNode> *outSelNode, PRInt32 *outSelOffset)
{
  NS_ENSURE_TRUE(aNode && outSelNode && outSelOffset, NS_ERROR_NULL_POINTER);
  
  nsresult res = NS_OK;
  
  // default values
  *outSelNode = aNode;
  *outSelOffset = 0;
  
  if (mHTMLEditor->IsTextNode(aNode) || mHTMLEditor->IsContainer(aNode))
  {
    if (aAction == nsIEditor::ePrevious)
    {
      PRUint32 len;
      res = mHTMLEditor->GetLengthOfDOMNode(aNode, len);
      *outSelOffset = PRInt32(len);
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  else 
  {
    *outSelNode = nsEditor::GetNodeLocation(aNode, outSelOffset);
    if (!nsTextEditUtils::IsBreak(aNode) || mHTMLEditor->IsVisBreak(aNode))
    {
      if (aAction == nsIEditor::ePrevious)
        (*outSelOffset)++;
    }
  }
  return res;
}


/*****************************************************************************************************
*    JoinBlocks: this method is used to join two block elements.  The right element is always joined
*    to the left element.  If the elements are the same type and not nested within each other, 
*    JoinNodesSmart is called (example, joining two list items together into one).  If the elements
*    are not the same type, or one is a descendant of the other, we instead destroy the right block
*    placing its children into leftblock.  DTD containment rules are followed throughout.
*         nsCOMPtr<nsIDOMNode> *aLeftBlock         pointer to the left block
*         nsCOMPtr<nsIDOMNode> *aRightBlock        pointer to the right block; will have contents moved to left block
*         bool *aCanceled                        return TRUE if we had to cancel operation
*/
nsresult
nsHTMLEditRules::JoinBlocks(nsCOMPtr<nsIDOMNode> *aLeftBlock, 
                            nsCOMPtr<nsIDOMNode> *aRightBlock, 
                            bool *aCanceled)
{
  NS_ENSURE_TRUE(aLeftBlock && aRightBlock && *aLeftBlock  && *aRightBlock, NS_ERROR_NULL_POINTER);
  if (nsHTMLEditUtils::IsTableElement(*aLeftBlock) || nsHTMLEditUtils::IsTableElement(*aRightBlock))
  {
    // do not try to merge table elements
    *aCanceled = true;
    return NS_OK;
  }

  // make sure we don't try to move thing's into HR's, which look like blocks but aren't containers
  if (nsHTMLEditUtils::IsHR(*aLeftBlock))
  {
    nsCOMPtr<nsIDOMNode> realLeft = mHTMLEditor->GetBlockNodeParent(*aLeftBlock);
    *aLeftBlock = realLeft;
  }
  if (nsHTMLEditUtils::IsHR(*aRightBlock))
  {
    nsCOMPtr<nsIDOMNode> realRight = mHTMLEditor->GetBlockNodeParent(*aRightBlock);
    *aRightBlock = realRight;
  }

  // bail if both blocks the same
  if (*aLeftBlock == *aRightBlock)
  {
    *aCanceled = true;
    return NS_OK;
  }
  
  // Joining a list item to its parent is a NOP.
  if (nsHTMLEditUtils::IsList(*aLeftBlock) && nsHTMLEditUtils::IsListItem(*aRightBlock))
  {
    nsCOMPtr<nsIDOMNode> rightParent;
    (*aRightBlock)->GetParentNode(getter_AddRefs(rightParent));
    if (rightParent == *aLeftBlock)
      return NS_OK;
  }

  // special rule here: if we are trying to join list items, and they are in different lists,
  // join the lists instead.
  bool bMergeLists = false;
  nsIAtom* existingList = nsGkAtoms::_empty;
  PRInt32 theOffset;
  nsCOMPtr<nsIDOMNode> leftList, rightList;
  if (nsHTMLEditUtils::IsListItem(*aLeftBlock) && nsHTMLEditUtils::IsListItem(*aRightBlock))
  {
    (*aLeftBlock)->GetParentNode(getter_AddRefs(leftList));
    (*aRightBlock)->GetParentNode(getter_AddRefs(rightList));
    if (leftList && rightList && (leftList!=rightList))
    {
      // there are some special complications if the lists are descendants of
      // the other lists' items.  Note that it is ok for them to be descendants
      // of the other lists themselves, which is the usual case for sublists
      // in our impllementation.
      if (!nsEditorUtils::IsDescendantOf(leftList, *aRightBlock, &theOffset) &&
          !nsEditorUtils::IsDescendantOf(rightList, *aLeftBlock, &theOffset))
      {
        *aLeftBlock = leftList;
        *aRightBlock = rightList;
        bMergeLists = true;
        existingList = mHTMLEditor->GetTag(leftList);
      }
    }
  }
  
  nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
  
  nsresult res = NS_OK;
  PRInt32  rightOffset = 0;
  PRInt32  leftOffset  = -1;

  // theOffset below is where you find yourself in aRightBlock when you traverse upwards
  // from aLeftBlock
  if (nsEditorUtils::IsDescendantOf(*aLeftBlock, *aRightBlock, &rightOffset))
  {
    // tricky case.  left block is inside right block.
    // Do ws adjustment.  This just destroys non-visible ws at boundaries we will be joining.
    rightOffset++;
    res = nsWSRunObject::ScrubBlockBoundary(mHTMLEditor, aLeftBlock, nsWSRunObject::kBlockEnd);
    NS_ENSURE_SUCCESS(res, res);
    res = nsWSRunObject::ScrubBlockBoundary(mHTMLEditor, aRightBlock, nsWSRunObject::kAfterBlock, &rightOffset);
    NS_ENSURE_SUCCESS(res, res);
    // Do br adjustment.
    nsCOMPtr<nsIDOMNode> brNode;
    res = CheckForInvisibleBR(*aLeftBlock, kBlockEnd, address_of(brNode));
    NS_ENSURE_SUCCESS(res, res);
    if (bMergeLists)
    {
      // idea here is to take all children in  rightList that are past
      // theOffset, and pull them into leftlist.
      nsCOMPtr<nsIDOMNode> childToMove;
      nsCOMPtr<nsIContent> parent(do_QueryInterface(rightList));
      NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);

      nsIContent *child = parent->GetChildAt(theOffset);
      while (child)
      {
        childToMove = do_QueryInterface(child);
        res = mHTMLEditor->MoveNode(childToMove, leftList, -1);
        NS_ENSURE_SUCCESS(res, res);

        child = parent->GetChildAt(rightOffset);
      }
    }
    else
    {
      res = MoveBlock(*aLeftBlock, *aRightBlock, leftOffset, rightOffset);
    }
    if (brNode) mHTMLEditor->DeleteNode(brNode);
  }
  // theOffset below is where you find yourself in aLeftBlock when you traverse upwards
  // from aRightBlock
  else if (nsEditorUtils::IsDescendantOf(*aRightBlock, *aLeftBlock, &leftOffset))
  {
    // tricky case.  right block is inside left block.
    // Do ws adjustment.  This just destroys non-visible ws at boundaries we will be joining.
    res = nsWSRunObject::ScrubBlockBoundary(mHTMLEditor, aRightBlock, nsWSRunObject::kBlockStart);
    NS_ENSURE_SUCCESS(res, res);
    res = nsWSRunObject::ScrubBlockBoundary(mHTMLEditor, aLeftBlock, nsWSRunObject::kBeforeBlock, &leftOffset);
    NS_ENSURE_SUCCESS(res, res);
    // Do br adjustment.
    nsCOMPtr<nsIDOMNode> brNode;
    res = CheckForInvisibleBR(*aLeftBlock, kBeforeBlock, address_of(brNode), leftOffset);
    NS_ENSURE_SUCCESS(res, res);
    if (bMergeLists)
    {
      res = MoveContents(rightList, leftList, &leftOffset);
    }
    else
    {
      res = MoveBlock(*aLeftBlock, *aRightBlock, leftOffset, rightOffset);
    }
    if (brNode) mHTMLEditor->DeleteNode(brNode);
  }
  else
  {
    // normal case.  blocks are siblings, or at least close enough to siblings.  An example
    // of the latter is a <p>paragraph</p><ul><li>one<li>two<li>three</ul>.  The first
    // li and the p are not true siblings, but we still want to join them if you backspace
    // from li into p.
    
    // adjust whitespace at block boundaries
    res = nsWSRunObject::PrepareToJoinBlocks(mHTMLEditor, *aLeftBlock, *aRightBlock);
    NS_ENSURE_SUCCESS(res, res);
    // Do br adjustment.
    nsCOMPtr<nsIDOMNode> brNode;
    res = CheckForInvisibleBR(*aLeftBlock, kBlockEnd, address_of(brNode));
    NS_ENSURE_SUCCESS(res, res);
    if (bMergeLists || mHTMLEditor->NodesSameType(*aLeftBlock, *aRightBlock))
    {
      // nodes are same type.  merge them.
      nsCOMPtr<nsIDOMNode> parent;
      PRInt32 offset;
      res = JoinNodesSmart(*aLeftBlock, *aRightBlock, address_of(parent), &offset);
      if (NS_SUCCEEDED(res) && bMergeLists)
      {
        nsCOMPtr<nsIDOMNode> newBlock;
        res = ConvertListType(*aRightBlock, address_of(newBlock),
                              existingList, nsGkAtoms::li);
      }
    }
    else
    {
      // nodes are disimilar types. 
      res = MoveBlock(*aLeftBlock, *aRightBlock, leftOffset, rightOffset);
    }
    if (NS_SUCCEEDED(res) && brNode)
    {
      res = mHTMLEditor->DeleteNode(brNode);
    }
  }
  return res;
}


/*****************************************************************************************************
*    MoveBlock: this method is used to move the content from rightBlock into leftBlock
*    Note that the "block" might merely be inline nodes between <br>s, or between blocks, etc.
*    DTD containment rules are followed throughout.
*         nsIDOMNode *aLeftBlock         parent to receive moved content
*         nsIDOMNode *aRightBlock        parent to provide moved content
*         PRInt32 aLeftOffset            offset in aLeftBlock to move content to
*         PRInt32 aRightOffset           offset in aRightBlock to move content from
*/
nsresult
nsHTMLEditRules::MoveBlock(nsIDOMNode *aLeftBlock, nsIDOMNode *aRightBlock, PRInt32 aLeftOffset, PRInt32 aRightOffset)
{
  nsCOMArray<nsIDOMNode> arrayOfNodes;
  nsCOMPtr<nsISupports> isupports;
  // GetNodesFromPoint is the workhorse that figures out what we wnat to move.
  nsresult res = GetNodesFromPoint(DOMPoint(aRightBlock,aRightOffset),
                                   nsEditor::kOpMakeList, arrayOfNodes, true);
  NS_ENSURE_SUCCESS(res, res);
  PRInt32 listCount = arrayOfNodes.Count();
  PRInt32 i;
  for (i=0; i<listCount; i++)
  {
    // get the node to act on
    nsIDOMNode* curNode = arrayOfNodes[i];
    if (IsBlockNode(curNode))
    {
      // For block nodes, move their contents only, then delete block.
      res = MoveContents(curNode, aLeftBlock, &aLeftOffset); 
      NS_ENSURE_SUCCESS(res, res);
      res = mHTMLEditor->DeleteNode(curNode);
    }
    else
    {
      // otherwise move the content as is, checking against the dtd.
      res = MoveNodeSmart(curNode, aLeftBlock, &aLeftOffset);
    }
  }
  return res;
}

/*****************************************************************************************************
*    MoveNodeSmart: this method is used to move node aSource to (aDest,aOffset).
*    DTD containment rules are followed throughout.  aOffset is updated to point _after_
*    inserted content.
*         nsIDOMNode *aSource       the selection.  
*         nsIDOMNode *aDest         parent to receive moved content
*         PRInt32 *aOffset          offset in aDest to move content to
*/
nsresult
nsHTMLEditRules::MoveNodeSmart(nsIDOMNode *aSource, nsIDOMNode *aDest, PRInt32 *aOffset)
{
  NS_ENSURE_TRUE(aSource && aDest && aOffset, NS_ERROR_NULL_POINTER);

  nsresult res;
  // check if this node can go into the destination node
  if (mHTMLEditor->CanContain(aDest, aSource)) {
    // if it can, move it there
    res = mHTMLEditor->MoveNode(aSource, aDest, *aOffset);
    NS_ENSURE_SUCCESS(res, res);
    if (*aOffset != -1) ++(*aOffset);
  }
  else
  {
    // if it can't, move its children, and then delete it.
    res = MoveContents(aSource, aDest, aOffset);
    NS_ENSURE_SUCCESS(res, res);
    res = mHTMLEditor->DeleteNode(aSource);
    NS_ENSURE_SUCCESS(res, res);
  }
  return NS_OK;
}

/*****************************************************************************************************
*    MoveContents: this method is used to move node the _contents_ of aSource to (aDest,aOffset).
*    DTD containment rules are followed throughout.  aOffset is updated to point _after_
*    inserted content.  aSource is deleted.
*         nsIDOMNode *aSource       the selection.  
*         nsIDOMNode *aDest         parent to receive moved content
*         PRInt32 *aOffset          offset in aDest to move content to
*/
nsresult
nsHTMLEditRules::MoveContents(nsIDOMNode *aSource, nsIDOMNode *aDest, PRInt32 *aOffset)
{
  NS_ENSURE_TRUE(aSource && aDest && aOffset, NS_ERROR_NULL_POINTER);
  if (aSource == aDest) return NS_ERROR_ILLEGAL_VALUE;
  NS_ASSERTION(!mHTMLEditor->IsTextNode(aSource), "#text does not have contents");
  
  nsCOMPtr<nsIDOMNode> child;
  nsAutoString tag;
  nsresult res;
  aSource->GetFirstChild(getter_AddRefs(child));
  while (child)
  {
    res = MoveNodeSmart(child, aDest, aOffset);
    NS_ENSURE_SUCCESS(res, res);
    aSource->GetFirstChild(getter_AddRefs(child));
  }
  return NS_OK;
}


nsresult
nsHTMLEditRules::DeleteNonTableElements(nsINode* aNode)
{
  MOZ_ASSERT(aNode);
  if (!aNode->IsElement() ||
      !nsHTMLEditUtils::IsTableElementButNotTable(aNode->AsElement())) {
    return mHTMLEditor->DeleteNode(aNode->AsDOMNode());
  }

  for (nsCOMPtr<nsIContent> child = aNode->GetLastChild();
       child;
       child = child->GetPreviousSibling()) {
    nsresult rv = DeleteNonTableElements(child);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult
nsHTMLEditRules::DidDeleteSelection(nsISelection *aSelection, 
                                    nsIEditor::EDirection aDir, 
                                    nsresult aResult)
{
  if (!aSelection) { return NS_ERROR_NULL_POINTER; }
  
  // find where we are
  nsCOMPtr<nsIDOMNode> startNode;
  PRInt32 startOffset;
  nsresult res = mEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(startNode), &startOffset);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(startNode, NS_ERROR_FAILURE);
  
  // find any enclosing mailcite
  nsCOMPtr<nsIDOMNode> citeNode;
  res = GetTopEnclosingMailCite(startNode, address_of(citeNode), 
                                IsPlaintextEditor());
  NS_ENSURE_SUCCESS(res, res);
  if (citeNode) {
    nsCOMPtr<nsINode> cite = do_QueryInterface(citeNode);
    bool isEmpty = true, seenBR = false;
    mHTMLEditor->IsEmptyNodeImpl(cite, &isEmpty, true, true, false, &seenBR);
    if (isEmpty)
    {
      nsCOMPtr<nsIDOMNode> brNode;
      PRInt32 offset;
      nsCOMPtr<nsIDOMNode> parent = nsEditor::GetNodeLocation(citeNode, &offset);
      res = mHTMLEditor->DeleteNode(citeNode);
      NS_ENSURE_SUCCESS(res, res);
      if (parent && seenBR)
      {
        res = mHTMLEditor->CreateBR(parent, offset, address_of(brNode));
        NS_ENSURE_SUCCESS(res, res);
        aSelection->Collapse(parent, offset);
      }
    }
  }
  
  // call through to base class
  return nsTextEditRules::DidDeleteSelection(aSelection, aDir, aResult);
}

nsresult
nsHTMLEditRules::WillMakeList(nsISelection* aSelection,
                              const nsAString* aListType,
                              bool aEntireList,
                              const nsAString* aBulletType,
                              bool* aCancel,
                              bool* aHandled,
                              const nsAString* aItemType)
{
  if (!aSelection || !aListType || !aCancel || !aHandled) {
    return NS_ERROR_NULL_POINTER;
  }
  nsCOMPtr<nsIAtom> listTypeAtom = do_GetAtom(*aListType);
  NS_ENSURE_TRUE(listTypeAtom, NS_ERROR_OUT_OF_MEMORY);

  nsresult res = WillInsert(aSelection, aCancel);
  NS_ENSURE_SUCCESS(res, res);

  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = false;
  *aHandled = false;

  // deduce what tag to use for list items
  nsCOMPtr<nsIAtom> itemType;
  if (aItemType) { 
    itemType = do_GetAtom(*aItemType);
    NS_ENSURE_TRUE(itemType, NS_ERROR_OUT_OF_MEMORY);
  } else if (listTypeAtom == nsGkAtoms::dl) {
    itemType = nsGkAtoms::dd;
  } else {
    itemType = nsGkAtoms::li;
  }

  // convert the selection ranges into "promoted" selection ranges:
  // this basically just expands the range to include the immediate
  // block parent, and then further expands to include any ancestors
  // whose children are all in the range

  *aHandled = true;

  res = NormalizeSelection(aSelection);
  NS_ENSURE_SUCCESS(res, res);
  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);

  nsCOMArray<nsIDOMNode> arrayOfNodes;
  res = GetListActionNodes(arrayOfNodes, aEntireList);
  NS_ENSURE_SUCCESS(res, res);

  PRInt32 listCount = arrayOfNodes.Count();

  // check if all our nodes are <br>s, or empty inlines
  bool bOnlyBreaks = true;
  for (PRInt32 j = 0; j < listCount; j++) {
    nsIDOMNode* curNode = arrayOfNodes[j];
    // if curNode is not a Break or empty inline, we're done
    if (!nsTextEditUtils::IsBreak(curNode) && !IsEmptyInline(curNode)) {
      bOnlyBreaks = false;
      break;
    }
  }

  // if no nodes, we make empty list.  Ditto if the user tried to make a list
  // of some # of breaks.
  if (!listCount || bOnlyBreaks) {
    nsCOMPtr<nsIDOMNode> parent, theList, theListItem;
    PRInt32 offset;

    // if only breaks, delete them
    if (bOnlyBreaks) {
      for (PRInt32 j = 0; j < (PRInt32)listCount; j++) {
        res = mHTMLEditor->DeleteNode(arrayOfNodes[j]);
        NS_ENSURE_SUCCESS(res, res);
      }
    }

    // get selection location
    res = mHTMLEditor->GetStartNodeAndOffset(aSelection,
                                             getter_AddRefs(parent), &offset);
    NS_ENSURE_SUCCESS(res, res);

    // make sure we can put a list here
    if (!mHTMLEditor->CanContainTag(parent, listTypeAtom)) {
      *aCancel = true;
      return NS_OK;
    }
    res = SplitAsNeeded(aListType, address_of(parent), &offset);
    NS_ENSURE_SUCCESS(res, res);
    res = mHTMLEditor->CreateNode(*aListType, parent, offset,
                                  getter_AddRefs(theList));
    NS_ENSURE_SUCCESS(res, res);
    res = mHTMLEditor->CreateNode(nsDependentAtomString(itemType), theList, 0,
                                  getter_AddRefs(theListItem));
    NS_ENSURE_SUCCESS(res, res);
    // remember our new block for postprocessing
    mNewBlock = theListItem;
    // put selection in new list item
    res = aSelection->Collapse(theListItem, 0);
    // to prevent selection resetter from overriding us
    selectionResetter.Abort();
    *aHandled = true;
    return res;
  }

  // if there is only one node in the array, and it is a list, div, or
  // blockquote, then look inside of it until we find inner list or content.

  res = LookInsideDivBQandList(arrayOfNodes);
  NS_ENSURE_SUCCESS(res, res);

  // Ok, now go through all the nodes and put then in the list,
  // or whatever is approriate.  Wohoo!

  listCount = arrayOfNodes.Count();
  nsCOMPtr<nsIDOMNode> curParent;
  nsCOMPtr<nsIDOMNode> curList;
  nsCOMPtr<nsIDOMNode> prevListItem;

  for (PRInt32 i = 0; i < listCount; i++) {
    // here's where we actually figure out what to do
    nsCOMPtr<nsIDOMNode> newBlock;
    nsCOMPtr<nsIDOMNode> curNode = arrayOfNodes[i];
    PRInt32 offset;
    curParent = nsEditor::GetNodeLocation(curNode, &offset);

    // make sure we don't assemble content that is in different table cells
    // into the same list.  respect table cell boundaries when listifying.
    if (curList) {
      bool bInDifTblElems;
      res = InDifferentTableElements(curList, curNode, &bInDifTblElems);
      NS_ENSURE_SUCCESS(res, res);
      if (bInDifTblElems) {
        curList = nsnull;
      }
    }

    // if curNode is a Break, delete it, and quit remembering prev list item
    if (nsTextEditUtils::IsBreak(curNode)) {
      res = mHTMLEditor->DeleteNode(curNode);
      NS_ENSURE_SUCCESS(res, res);
      prevListItem = 0;
      continue;
    } else if (IsEmptyInline(curNode)) {
      // if curNode is an empty inline container, delete it
      res = mHTMLEditor->DeleteNode(curNode);
      NS_ENSURE_SUCCESS(res, res);
      continue;
    }

    if (nsHTMLEditUtils::IsList(curNode)) {
      // do we have a curList already?
      if (curList && !nsEditorUtils::IsDescendantOf(curNode, curList)) {
        // move all of our children into curList.  cheezy way to do it: move
        // whole list and then RemoveContainer() on the list.  ConvertListType
        // first: that routine handles converting the list item types, if
        // needed
        res = mHTMLEditor->MoveNode(curNode, curList, -1);
        NS_ENSURE_SUCCESS(res, res);
        res = ConvertListType(curNode, address_of(newBlock), listTypeAtom,
                              itemType);
        NS_ENSURE_SUCCESS(res, res);
        res = mHTMLEditor->RemoveBlockContainer(newBlock);
        NS_ENSURE_SUCCESS(res, res);
      } else {
        // replace list with new list type
        res = ConvertListType(curNode, address_of(newBlock), listTypeAtom,
                              itemType);
        NS_ENSURE_SUCCESS(res, res);
        curList = newBlock;
      }
      prevListItem = 0;
      continue;
    }

    if (nsHTMLEditUtils::IsListItem(curNode)) {
      if (mHTMLEditor->GetTag(curParent) != listTypeAtom) {
        // list item is in wrong type of list. if we don't have a curList,
        // split the old list and make a new list of correct type.
        if (!curList || nsEditorUtils::IsDescendantOf(curNode, curList)) {
          res = mHTMLEditor->SplitNode(curParent, offset,
                                       getter_AddRefs(newBlock));
          NS_ENSURE_SUCCESS(res, res);
          PRInt32 offset;
          nsCOMPtr<nsIDOMNode> parent = nsEditor::GetNodeLocation(curParent, &offset);
          res = mHTMLEditor->CreateNode(*aListType, parent, offset,
                                        getter_AddRefs(curList));
          NS_ENSURE_SUCCESS(res, res);
        }
        // move list item to new list
        res = mHTMLEditor->MoveNode(curNode, curList, -1);
        NS_ENSURE_SUCCESS(res, res);
        // convert list item type if needed
        if (!mHTMLEditor->NodeIsType(curNode, itemType)) {
          res = mHTMLEditor->ReplaceContainer(curNode, address_of(newBlock),
                                              nsDependentAtomString(itemType));
          NS_ENSURE_SUCCESS(res, res);
        }
      } else {
        // item is in right type of list.  But we might still have to move it.
        // and we might need to convert list item types.
        if (!curList) {
          curList = curParent;
        } else if (curParent != curList) {
          // move list item to new list
          res = mHTMLEditor->MoveNode(curNode, curList, -1);
          NS_ENSURE_SUCCESS(res, res);
        }
        if (!mHTMLEditor->NodeIsType(curNode, itemType)) {
          res = mHTMLEditor->ReplaceContainer(curNode, address_of(newBlock),
                                              nsDependentAtomString(itemType));
          NS_ENSURE_SUCCESS(res, res);
        }
      }
      nsCOMPtr<nsIDOMElement> curElement = do_QueryInterface(curNode);
      NS_NAMED_LITERAL_STRING(typestr, "type");
      if (aBulletType && !aBulletType->IsEmpty()) {
        res = mHTMLEditor->SetAttribute(curElement, typestr, *aBulletType);
      } else {
        res = mHTMLEditor->RemoveAttribute(curElement, typestr);
      }
      NS_ENSURE_SUCCESS(res, res);
      continue;
    }

    // if we hit a div clear our prevListItem, insert divs contents
    // into our node array, and remove the div
    if (nsHTMLEditUtils::IsDiv(curNode)) {
      prevListItem = nsnull;
      PRInt32 j = i + 1;
      res = GetInnerContent(curNode, arrayOfNodes, &j);
      NS_ENSURE_SUCCESS(res, res);
      res = mHTMLEditor->RemoveContainer(curNode);
      NS_ENSURE_SUCCESS(res, res);
      listCount = arrayOfNodes.Count();
      continue;
    }

    // need to make a list to put things in if we haven't already,
    if (!curList) {
      res = SplitAsNeeded(aListType, address_of(curParent), &offset);
      NS_ENSURE_SUCCESS(res, res);
      res = mHTMLEditor->CreateNode(*aListType, curParent, offset,
                                    getter_AddRefs(curList));
      NS_ENSURE_SUCCESS(res, res);
      // remember our new block for postprocessing
      mNewBlock = curList;
      // curList is now the correct thing to put curNode in
      prevListItem = 0;
    }

    // if curNode isn't a list item, we must wrap it in one
    nsCOMPtr<nsIDOMNode> listItem;
    if (!nsHTMLEditUtils::IsListItem(curNode)) {
      if (IsInlineNode(curNode) && prevListItem) {
        // this is a continuation of some inline nodes that belong together in
        // the same list item.  use prevListItem
        res = mHTMLEditor->MoveNode(curNode, prevListItem, -1);
        NS_ENSURE_SUCCESS(res, res);
      } else {
        // don't wrap li around a paragraph.  instead replace paragraph with li
        if (nsHTMLEditUtils::IsParagraph(curNode)) {
          res = mHTMLEditor->ReplaceContainer(curNode, address_of(listItem),
                                              nsDependentAtomString(itemType));
        } else {
          res = mHTMLEditor->InsertContainerAbove(curNode, address_of(listItem),
                                                  nsDependentAtomString(itemType));
        }
        NS_ENSURE_SUCCESS(res, res);
        if (IsInlineNode(curNode)) {
          prevListItem = listItem;
        } else {
          prevListItem = nsnull;
        }
      }
    } else {
      listItem = curNode;
    }

    if (listItem) {
      // if we made a new list item, deal with it: tuck the listItem into the
      // end of the active list
      res = mHTMLEditor->MoveNode(listItem, curList, -1);
      NS_ENSURE_SUCCESS(res, res);
    }
  }

  return res;
}


nsresult
nsHTMLEditRules::WillRemoveList(nsISelection *aSelection, 
                                bool aOrdered, 
                                bool *aCancel,
                                bool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = false;
  *aHandled = true;
  
  nsresult res = NormalizeSelection(aSelection);
  NS_ENSURE_SUCCESS(res, res);
  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);
  
  nsCOMArray<nsIDOMRange> arrayOfRanges;
  res = GetPromotedRanges(aSelection, arrayOfRanges, nsEditor::kOpMakeList);
  NS_ENSURE_SUCCESS(res, res);
  
  // use these ranges to contruct a list of nodes to act on.
  nsCOMArray<nsIDOMNode> arrayOfNodes;
  res = GetListActionNodes(arrayOfNodes, false);
  NS_ENSURE_SUCCESS(res, res);                                 
                                     
  // Remove all non-editable nodes.  Leave them be.
  PRInt32 listCount = arrayOfNodes.Count();
  PRInt32 i;
  for (i=listCount-1; i>=0; i--)
  {
    nsIDOMNode* testNode = arrayOfNodes[i];
    if (!mHTMLEditor->IsEditable(testNode))
    {
      arrayOfNodes.RemoveObjectAt(i);
    }
  }
  
  // reset list count
  listCount = arrayOfNodes.Count();
  
  // Only act on lists or list items in the array
  nsCOMPtr<nsIDOMNode> curParent;
  for (i=0; i<listCount; i++)
  {
    // here's where we actually figure out what to do
    nsIDOMNode* curNode = arrayOfNodes[i];
    PRInt32 offset;
    curParent = nsEditor::GetNodeLocation(curNode, &offset);
    
    if (nsHTMLEditUtils::IsListItem(curNode))  // unlist this listitem
    {
      bool bOutOfList;
      do
      {
        res = PopListItem(curNode, &bOutOfList);
        NS_ENSURE_SUCCESS(res, res);
      } while (!bOutOfList); // keep popping it out until it's not in a list anymore
    }
    else if (nsHTMLEditUtils::IsList(curNode)) // node is a list, move list items out
    {
      res = RemoveListStructure(curNode);
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  return res;
}


nsresult
nsHTMLEditRules::WillMakeDefListItem(nsISelection *aSelection, 
                                     const nsAString *aItemType, 
                                     bool aEntireList, 
                                     bool *aCancel,
                                     bool *aHandled)
{
  // for now we let WillMakeList handle this
  NS_NAMED_LITERAL_STRING(listType, "dl");
  return WillMakeList(aSelection, &listType, aEntireList, nsnull, aCancel, aHandled, aItemType);
}

nsresult
nsHTMLEditRules::WillMakeBasicBlock(nsISelection *aSelection, 
                                    const nsAString *aBlockType, 
                                    bool *aCancel,
                                    bool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = false;
  *aHandled = false;
  
  nsresult res = WillInsert(aSelection, aCancel);
  NS_ENSURE_SUCCESS(res, res);
  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = false;
  res = NormalizeSelection(aSelection);
  NS_ENSURE_SUCCESS(res, res);
  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);
  nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
  *aHandled = true;
  nsString tString(*aBlockType);

  // contruct a list of nodes to act on.
  nsCOMArray<nsIDOMNode> arrayOfNodes;
  res = GetNodesFromSelection(aSelection, nsEditor::kOpMakeBasicBlock,
                              arrayOfNodes);
  NS_ENSURE_SUCCESS(res, res);

  // Remove all non-editable nodes.  Leave them be.
  PRInt32 listCount = arrayOfNodes.Count();
  PRInt32 i;
  for (i=listCount-1; i>=0; i--)
  {
    if (!mHTMLEditor->IsEditable(arrayOfNodes[i]))
    {
      arrayOfNodes.RemoveObjectAt(i);
    }
  }
  
  // reset list count
  listCount = arrayOfNodes.Count();
  
  // if nothing visible in list, make an empty block
  if (ListIsEmptyLine(arrayOfNodes))
  {
    nsCOMPtr<nsIDOMNode> parent, theBlock;
    PRInt32 offset;
    
    // get selection location
    res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(parent), &offset);
    NS_ENSURE_SUCCESS(res, res);
    if (tString.EqualsLiteral("normal") ||
        tString.IsEmpty() ) // we are removing blocks (going to "body text")
    {
      nsCOMPtr<nsIDOMNode> curBlock = parent;
      if (!IsBlockNode(curBlock))
        curBlock = mHTMLEditor->GetBlockNodeParent(parent);
      nsCOMPtr<nsIDOMNode> curBlockPar;
      NS_ENSURE_TRUE(curBlock, NS_ERROR_NULL_POINTER);
      curBlock->GetParentNode(getter_AddRefs(curBlockPar));
      if (nsHTMLEditUtils::IsFormatNode(curBlock))
      {
        // if the first editable node after selection is a br, consume it.  Otherwise
        // it gets pushed into a following block after the split, which is visually bad.
        nsCOMPtr<nsIDOMNode> brNode;
        res = mHTMLEditor->GetNextHTMLNode(parent, offset, address_of(brNode));
        NS_ENSURE_SUCCESS(res, res);        
        if (brNode && nsTextEditUtils::IsBreak(brNode))
        {
          res = mHTMLEditor->DeleteNode(brNode);
          NS_ENSURE_SUCCESS(res, res); 
        }
        // do the splits!
        res = mHTMLEditor->SplitNodeDeep(curBlock, parent, offset, &offset, true);
        NS_ENSURE_SUCCESS(res, res);
        // put a br at the split point
        res = mHTMLEditor->CreateBR(curBlockPar, offset, address_of(brNode));
        NS_ENSURE_SUCCESS(res, res);
        // put selection at the split point
        res = aSelection->Collapse(curBlockPar, offset);
        selectionResetter.Abort();  // to prevent selection reseter from overriding us.
        *aHandled = true;
      }
      // else nothing to do!
    }
    else  // we are making a block
    {   
      // consume a br, if needed
      nsCOMPtr<nsIDOMNode> brNode;
      res = mHTMLEditor->GetNextHTMLNode(parent, offset, address_of(brNode), true);
      NS_ENSURE_SUCCESS(res, res);
      if (brNode && nsTextEditUtils::IsBreak(brNode))
      {
        res = mHTMLEditor->DeleteNode(brNode);
        NS_ENSURE_SUCCESS(res, res);
        // we don't need to act on this node any more
        arrayOfNodes.RemoveObject(brNode);
      }
      // make sure we can put a block here
      res = SplitAsNeeded(aBlockType, address_of(parent), &offset);
      NS_ENSURE_SUCCESS(res, res);
      res = mHTMLEditor->CreateNode(*aBlockType, parent, offset, getter_AddRefs(theBlock));
      NS_ENSURE_SUCCESS(res, res);
      // remember our new block for postprocessing
      mNewBlock = theBlock;
      // delete anything that was in the list of nodes
      for (PRInt32 j = arrayOfNodes.Count() - 1; j >= 0; --j) 
      {
        nsCOMPtr<nsIDOMNode> curNode = arrayOfNodes[0];
        res = mHTMLEditor->DeleteNode(curNode);
        NS_ENSURE_SUCCESS(res, res);
        res = arrayOfNodes.RemoveObjectAt(0);
        NS_ENSURE_SUCCESS(res, res);
      }
      // put selection in new block
      res = aSelection->Collapse(theBlock,0);
      selectionResetter.Abort();  // to prevent selection reseter from overriding us.
      *aHandled = true;
    }
    return res;    
  }
  else
  {
    // Ok, now go through all the nodes and make the right kind of blocks, 
    // or whatever is approriate.  Wohoo! 
    // Note: blockquote is handled a little differently
    if (tString.EqualsLiteral("blockquote"))
      res = MakeBlockquote(arrayOfNodes);
    else if (tString.EqualsLiteral("normal") ||
             tString.IsEmpty() )
      res = RemoveBlockStyle(arrayOfNodes);
    else
      res = ApplyBlockStyle(arrayOfNodes, aBlockType);
    return res;
  }
  return res;
}

nsresult 
nsHTMLEditRules::DidMakeBasicBlock(nsISelection *aSelection,
                                   nsRulesInfo *aInfo, nsresult aResult)
{
  NS_ENSURE_TRUE(aSelection, NS_ERROR_NULL_POINTER);
  // check for empty block.  if so, put a moz br in it.
  if (!aSelection->Collapsed()) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset;
  nsresult res = nsEditor::GetStartNodeAndOffset(aSelection, getter_AddRefs(parent), &offset);
  NS_ENSURE_SUCCESS(res, res);
  res = InsertMozBRIfNeeded(parent);
  return res;
}

nsresult
nsHTMLEditRules::WillIndent(nsISelection *aSelection, bool *aCancel, bool * aHandled)
{
  nsresult res;
  if (mHTMLEditor->IsCSSEnabled()) {
    res = WillCSSIndent(aSelection, aCancel, aHandled);
  }
  else {
    res = WillHTMLIndent(aSelection, aCancel, aHandled);
  }
  return res;
}

nsresult
nsHTMLEditRules::WillCSSIndent(nsISelection *aSelection, bool *aCancel, bool * aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  
  nsresult res = WillInsert(aSelection, aCancel);
  NS_ENSURE_SUCCESS(res, res);

  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = false;
  *aHandled = true;

  res = NormalizeSelection(aSelection);
  NS_ENSURE_SUCCESS(res, res);
  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);
  nsCOMArray<nsIDOMRange>  arrayOfRanges;
  nsCOMArray<nsIDOMNode> arrayOfNodes;
  
  // short circuit: detect case of collapsed selection inside an <li>.
  // just sublist that <li>.  This prevents bug 97797.
  
  nsCOMPtr<nsIDOMNode> liNode;
  if (aSelection->Collapsed()) {
    nsCOMPtr<nsIDOMNode> node, block;
    PRInt32 offset;
    nsresult res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(node), &offset);
    NS_ENSURE_SUCCESS(res, res);
    if (IsBlockNode(node)) 
      block = node;
    else
      block = mHTMLEditor->GetBlockNodeParent(node);
    if (block && nsHTMLEditUtils::IsListItem(block))
      liNode = block;
  }
  
  if (liNode)
  {
    arrayOfNodes.AppendObject(liNode);
  }
  else
  {
    // convert the selection ranges into "promoted" selection ranges:
    // this basically just expands the range to include the immediate
    // block parent, and then further expands to include any ancestors
    // whose children are all in the range
    res = GetNodesFromSelection(aSelection, nsEditor::kOpIndent, arrayOfNodes);
    NS_ENSURE_SUCCESS(res, res);
  }
  
  NS_NAMED_LITERAL_STRING(quoteType, "blockquote");
  // if nothing visible in list, make an empty block
  if (ListIsEmptyLine(arrayOfNodes))
  {
    nsCOMPtr<nsIDOMNode> parent, theBlock;
    PRInt32 offset;
    nsAutoString quoteType(NS_LITERAL_STRING("div"));
    // get selection location
    res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(parent), &offset);
    NS_ENSURE_SUCCESS(res, res);
    // make sure we can put a block here
    res = SplitAsNeeded(&quoteType, address_of(parent), &offset);
    NS_ENSURE_SUCCESS(res, res);
    res = mHTMLEditor->CreateNode(quoteType, parent, offset, getter_AddRefs(theBlock));
    NS_ENSURE_SUCCESS(res, res);
    // remember our new block for postprocessing
    mNewBlock = theBlock;
    RelativeChangeIndentationOfElementNode(theBlock, +1);
    // delete anything that was in the list of nodes
    for (PRInt32 j = arrayOfNodes.Count() - 1; j >= 0; --j) 
    {
      nsCOMPtr<nsIDOMNode> curNode = arrayOfNodes[0];
      res = mHTMLEditor->DeleteNode(curNode);
      NS_ENSURE_SUCCESS(res, res);
      res = arrayOfNodes.RemoveObjectAt(0);
      NS_ENSURE_SUCCESS(res, res);
    }
    // put selection in new block
    res = aSelection->Collapse(theBlock,0);
    selectionResetter.Abort();  // to prevent selection reseter from overriding us.
    *aHandled = true;
    return res;
  }
  
  // Ok, now go through all the nodes and put them in a blockquote, 
  // or whatever is appropriate.  Wohoo!
  PRInt32 i;
  nsCOMPtr<nsIDOMNode> curParent;
  nsCOMPtr<nsIDOMNode> curQuote;
  nsCOMPtr<nsIDOMNode> curList;
  nsCOMPtr<nsIDOMNode> sibling;
  PRInt32 listCount = arrayOfNodes.Count();
  for (i=0; i<listCount; i++)
  {
    // here's where we actually figure out what to do
    nsCOMPtr<nsIDOMNode> curNode = arrayOfNodes[i];

    // Ignore all non-editable nodes.  Leave them be.
    if (!mHTMLEditor->IsEditable(curNode)) continue;

    PRInt32 offset;
    curParent = nsEditor::GetNodeLocation(curNode, &offset);
    
    // some logic for putting list items into nested lists...
    if (nsHTMLEditUtils::IsList(curParent))
    {
      sibling = nsnull;

      // Check for whether we should join a list that follows curNode.
      // We do this if the next element is a list, and the list is of the
      // same type (li/ol) as curNode was a part it.
      mHTMLEditor->GetNextHTMLSibling(curNode, address_of(sibling));
      if (sibling && nsHTMLEditUtils::IsList(sibling))
      {
        nsAutoString curListTag, siblingListTag;
        nsEditor::GetTagString(curParent, curListTag);
        nsEditor::GetTagString(sibling, siblingListTag);
        if (curListTag == siblingListTag)
        {
          res = mHTMLEditor->MoveNode(curNode, sibling, 0);
          NS_ENSURE_SUCCESS(res, res);
          continue;
        }
      }
      // Check for whether we should join a list that preceeds curNode.
      // We do this if the previous element is a list, and the list is of
      // the same type (li/ol) as curNode was a part of.
      mHTMLEditor->GetPriorHTMLSibling(curNode, address_of(sibling));
      if (sibling && nsHTMLEditUtils::IsList(sibling))
      {
        nsAutoString curListTag, siblingListTag;
        nsEditor::GetTagString(curParent, curListTag);
        nsEditor::GetTagString(sibling, siblingListTag);
        if (curListTag == siblingListTag)
        {
          res = mHTMLEditor->MoveNode(curNode, sibling, -1);
          NS_ENSURE_SUCCESS(res, res);
          continue;
        }
      }
      sibling = nsnull;
      
      // check to see if curList is still appropriate.  Which it is if
      // curNode is still right after it in the same list.
      if (curList)
        mHTMLEditor->GetPriorHTMLSibling(curNode, address_of(sibling));

      if (!curList || (sibling && sibling != curList))
      {
        nsAutoString listTag;
        nsEditor::GetTagString(curParent,listTag);
        ToLowerCase(listTag);
        // create a new nested list of correct type
        res = SplitAsNeeded(&listTag, address_of(curParent), &offset);
        NS_ENSURE_SUCCESS(res, res);
        res = mHTMLEditor->CreateNode(listTag, curParent, offset, getter_AddRefs(curList));
        NS_ENSURE_SUCCESS(res, res);
        // curList is now the correct thing to put curNode in
        // remember our new block for postprocessing
        mNewBlock = curList;
      }
      // tuck the node into the end of the active list
      PRUint32 listLen;
      res = mHTMLEditor->GetLengthOfDOMNode(curList, listLen);
      NS_ENSURE_SUCCESS(res, res);
      res = mHTMLEditor->MoveNode(curNode, curList, listLen);
      NS_ENSURE_SUCCESS(res, res);
    }
    
    else // not a list item
    {
      if (IsBlockNode(curNode)) {
        RelativeChangeIndentationOfElementNode(curNode, +1);
        curQuote = nsnull;
      }
      else {
        if (!curQuote)
        {
          // First, check that our element can contain a div.
          if (!mEditor->CanContainTag(curParent, nsGkAtoms::div)) {
            return NS_OK; // cancelled
          }

          NS_NAMED_LITERAL_STRING(divquoteType, "div");
          res = SplitAsNeeded(&divquoteType, address_of(curParent), &offset);
          NS_ENSURE_SUCCESS(res, res);
          res = mHTMLEditor->CreateNode(divquoteType, curParent, offset, getter_AddRefs(curQuote));
          NS_ENSURE_SUCCESS(res, res);
          RelativeChangeIndentationOfElementNode(curQuote, +1);
          // remember our new block for postprocessing
          mNewBlock = curQuote;
          // curQuote is now the correct thing to put curNode in
        }
        
        // tuck the node into the end of the active blockquote
        PRUint32 quoteLen;
        res = mHTMLEditor->GetLengthOfDOMNode(curQuote, quoteLen);
        NS_ENSURE_SUCCESS(res, res);
        res = mHTMLEditor->MoveNode(curNode, curQuote, quoteLen);
        NS_ENSURE_SUCCESS(res, res);
      }
    }
  }
  return res;
}

nsresult
nsHTMLEditRules::WillHTMLIndent(nsISelection *aSelection, bool *aCancel, bool * aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  nsresult res = WillInsert(aSelection, aCancel);
  NS_ENSURE_SUCCESS(res, res);

  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = false;
  *aHandled = true;

  res = NormalizeSelection(aSelection);
  NS_ENSURE_SUCCESS(res, res);
  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);
  
  // convert the selection ranges into "promoted" selection ranges:
  // this basically just expands the range to include the immediate
  // block parent, and then further expands to include any ancestors
  // whose children are all in the range
  
  nsCOMArray<nsIDOMRange> arrayOfRanges;
  res = GetPromotedRanges(aSelection, arrayOfRanges, nsEditor::kOpIndent);
  NS_ENSURE_SUCCESS(res, res);
  
  // use these ranges to contruct a list of nodes to act on.
  nsCOMArray<nsIDOMNode> arrayOfNodes;
  res = GetNodesForOperation(arrayOfRanges, arrayOfNodes, nsEditor::kOpIndent);
  NS_ENSURE_SUCCESS(res, res);                                 
                                     
  NS_NAMED_LITERAL_STRING(quoteType, "blockquote");

  // if nothing visible in list, make an empty block
  if (ListIsEmptyLine(arrayOfNodes))
  {
    nsCOMPtr<nsIDOMNode> parent, theBlock;
    PRInt32 offset;
    
    // get selection location
    res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(parent), &offset);
    NS_ENSURE_SUCCESS(res, res);
    // make sure we can put a block here
    res = SplitAsNeeded(&quoteType, address_of(parent), &offset);
    NS_ENSURE_SUCCESS(res, res);
    res = mHTMLEditor->CreateNode(quoteType, parent, offset, getter_AddRefs(theBlock));
    NS_ENSURE_SUCCESS(res, res);
    // remember our new block for postprocessing
    mNewBlock = theBlock;
    // delete anything that was in the list of nodes
    for (PRInt32 j = arrayOfNodes.Count() - 1; j >= 0; --j) 
    {
      nsCOMPtr<nsIDOMNode> curNode = arrayOfNodes[0];
      res = mHTMLEditor->DeleteNode(curNode);
      NS_ENSURE_SUCCESS(res, res);
      res = arrayOfNodes.RemoveObjectAt(0);
      NS_ENSURE_SUCCESS(res, res);
    }
    // put selection in new block
    res = aSelection->Collapse(theBlock,0);
    selectionResetter.Abort();  // to prevent selection reseter from overriding us.
    *aHandled = true;
    return res;
  }

  // Ok, now go through all the nodes and put them in a blockquote, 
  // or whatever is appropriate.  Wohoo!
  PRInt32 i;
  nsCOMPtr<nsIDOMNode> curParent, curQuote, curList, indentedLI, sibling;
  PRInt32 listCount = arrayOfNodes.Count();
  for (i=0; i<listCount; i++)
  {
    // here's where we actually figure out what to do
    nsCOMPtr<nsIDOMNode> curNode = arrayOfNodes[i];

    // Ignore all non-editable nodes.  Leave them be.
    if (!mHTMLEditor->IsEditable(curNode)) continue;

    PRInt32 offset;
    curParent = nsEditor::GetNodeLocation(curNode, &offset);
     
    // some logic for putting list items into nested lists...
    if (nsHTMLEditUtils::IsList(curParent))
    {
      sibling = nsnull;

      // Check for whether we should join a list that follows curNode.
      // We do this if the next element is a list, and the list is of the
      // same type (li/ol) as curNode was a part it.
      mHTMLEditor->GetNextHTMLSibling(curNode, address_of(sibling));
      if (sibling && nsHTMLEditUtils::IsList(sibling))
      {
        nsAutoString curListTag, siblingListTag;
        nsEditor::GetTagString(curParent, curListTag);
        nsEditor::GetTagString(sibling, siblingListTag);
        if (curListTag == siblingListTag)
        {
          res = mHTMLEditor->MoveNode(curNode, sibling, 0);
          NS_ENSURE_SUCCESS(res, res);
          continue;
        }
      }

      // Check for whether we should join a list that preceeds curNode.
      // We do this if the previous element is a list, and the list is of
      // the same type (li/ol) as curNode was a part of.
      mHTMLEditor->GetPriorHTMLSibling(curNode, address_of(sibling));
      if (sibling && nsHTMLEditUtils::IsList(sibling))
      {
        nsAutoString curListTag, siblingListTag;
        nsEditor::GetTagString(curParent, curListTag);
        nsEditor::GetTagString(sibling, siblingListTag);
        if (curListTag == siblingListTag)
        {
          res = mHTMLEditor->MoveNode(curNode, sibling, -1);
          NS_ENSURE_SUCCESS(res, res);
          continue;
        }
      }

      sibling = nsnull;

      // check to see if curList is still appropriate.  Which it is if
      // curNode is still right after it in the same list.
      if (curList)
        mHTMLEditor->GetPriorHTMLSibling(curNode, address_of(sibling));

      if (!curList || (sibling && sibling != curList) )
      {
        nsAutoString listTag;
        nsEditor::GetTagString(curParent,listTag);
        ToLowerCase(listTag);
        // create a new nested list of correct type
        res = SplitAsNeeded(&listTag, address_of(curParent), &offset);
        NS_ENSURE_SUCCESS(res, res);
        res = mHTMLEditor->CreateNode(listTag, curParent, offset, getter_AddRefs(curList));
        NS_ENSURE_SUCCESS(res, res);
        // curList is now the correct thing to put curNode in
        // remember our new block for postprocessing
        mNewBlock = curList;
      }
      // tuck the node into the end of the active list
      res = mHTMLEditor->MoveNode(curNode, curList, -1);
      NS_ENSURE_SUCCESS(res, res);
      // forget curQuote, if any
      curQuote = nsnull;
    }
    
    else // not a list item, use blockquote?
    {
      // if we are inside a list item, we don't want to blockquote, we want
      // to sublist the list item.  We may have several nodes listed in the
      // array of nodes to act on, that are in the same list item.  Since
      // we only want to indent that li once, we must keep track of the most
      // recent indented list item, and not indent it if we find another node
      // to act on that is still inside the same li.
      nsCOMPtr<nsIDOMNode> listitem=IsInListItem(curNode);
      if (listitem)
      {
        if (indentedLI == listitem) continue;  // already indented this list item
        curParent = nsEditor::GetNodeLocation(listitem, &offset);
        // check to see if curList is still appropriate.  Which it is if
        // curNode is still right after it in the same list.
        if (curList)
        {
          sibling = nsnull;
          mHTMLEditor->GetPriorHTMLSibling(curNode, address_of(sibling));
        }
         
        if (!curList || (sibling && sibling != curList) )
        {
          nsAutoString listTag;
          nsEditor::GetTagString(curParent,listTag);
          ToLowerCase(listTag);
          // create a new nested list of correct type
          res = SplitAsNeeded(&listTag, address_of(curParent), &offset);
          NS_ENSURE_SUCCESS(res, res);
          res = mHTMLEditor->CreateNode(listTag, curParent, offset, getter_AddRefs(curList));
          NS_ENSURE_SUCCESS(res, res);
        }
        res = mHTMLEditor->MoveNode(listitem, curList, -1);
        NS_ENSURE_SUCCESS(res, res);
        // remember we indented this li
        indentedLI = listitem;
      }
      
      else
      {
        // need to make a blockquote to put things in if we haven't already,
        // or if this node doesn't go in blockquote we used earlier.
        // One reason it might not go in prio blockquote is if we are now
        // in a different table cell. 
        if (curQuote)
        {
          bool bInDifTblElems;
          res = InDifferentTableElements(curQuote, curNode, &bInDifTblElems);
          NS_ENSURE_SUCCESS(res, res);
          if (bInDifTblElems)
            curQuote = nsnull;
        }
        
        if (!curQuote) 
        {
          // First, check that our element can contain a blockquote.
          if (!mEditor->CanContainTag(curParent, nsGkAtoms::blockquote)) {
            return NS_OK; // cancelled
          }

          res = SplitAsNeeded(&quoteType, address_of(curParent), &offset);
          NS_ENSURE_SUCCESS(res, res);
          res = mHTMLEditor->CreateNode(quoteType, curParent, offset, getter_AddRefs(curQuote));
          NS_ENSURE_SUCCESS(res, res);
          // remember our new block for postprocessing
          mNewBlock = curQuote;
          // curQuote is now the correct thing to put curNode in
        }
          
        // tuck the node into the end of the active blockquote
        res = mHTMLEditor->MoveNode(curNode, curQuote, -1);
        NS_ENSURE_SUCCESS(res, res);
        // forget curList, if any
        curList = nsnull;
      }
    }
  }
  return res;
}


nsresult
nsHTMLEditRules::WillOutdent(nsISelection *aSelection, bool *aCancel, bool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = false;
  *aHandled = true;
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> rememberedLeftBQ, rememberedRightBQ;
  bool useCSS = mHTMLEditor->IsCSSEnabled();

  res = NormalizeSelection(aSelection);
  NS_ENSURE_SUCCESS(res, res);
  // some scoping for selection resetting - we may need to tweak it
  {
    nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);
    
    // convert the selection ranges into "promoted" selection ranges:
    // this basically just expands the range to include the immediate
    // block parent, and then further expands to include any ancestors
    // whose children are all in the range
    nsCOMArray<nsIDOMNode> arrayOfNodes;
    res = GetNodesFromSelection(aSelection, nsEditor::kOpOutdent,
                                arrayOfNodes);
    NS_ENSURE_SUCCESS(res, res);

    // Ok, now go through all the nodes and remove a level of blockquoting, 
    // or whatever is appropriate.  Wohoo!

    nsCOMPtr<nsIDOMNode> curBlockQuote, firstBQChild, lastBQChild;
    bool curBlockQuoteIsIndentedWithCSS = false;
    PRInt32 listCount = arrayOfNodes.Count();
    PRInt32 i;
    nsCOMPtr<nsIDOMNode> curParent;
    for (i=0; i<listCount; i++)
    {
      // here's where we actually figure out what to do
      nsCOMPtr<nsIDOMNode> curNode = arrayOfNodes[i];
      PRInt32 offset;
      curParent = nsEditor::GetNodeLocation(curNode, &offset);
      
      // is it a blockquote?
      if (nsHTMLEditUtils::IsBlockquote(curNode)) 
      {
        // if it is a blockquote, remove it.
        // So we need to finish up dealng with any curBlockQuote first.
        if (curBlockQuote)
        {
          res = OutdentPartOfBlock(curBlockQuote, firstBQChild, lastBQChild,
                                   curBlockQuoteIsIndentedWithCSS,
                                   address_of(rememberedLeftBQ),
                                   address_of(rememberedRightBQ));
          NS_ENSURE_SUCCESS(res, res);
          curBlockQuote = 0;  firstBQChild = 0;  lastBQChild = 0;
          curBlockQuoteIsIndentedWithCSS = false;
        }
        res = mHTMLEditor->RemoveBlockContainer(curNode);
        NS_ENSURE_SUCCESS(res, res);
        continue;
      }
      // is it a block with a 'margin' property?
      if (useCSS && IsBlockNode(curNode))
      {
        nsIAtom* marginProperty = MarginPropertyAtomForIndent(mHTMLEditor->mHTMLCSSUtils, curNode);
        nsAutoString value;
        mHTMLEditor->mHTMLCSSUtils->GetSpecifiedProperty(curNode, marginProperty, value);
        float f;
        nsCOMPtr<nsIAtom> unit;
        mHTMLEditor->mHTMLCSSUtils->ParseLength(value, &f, getter_AddRefs(unit));
        if (f > 0)
        {
          RelativeChangeIndentationOfElementNode(curNode, -1);
          continue;
        }
      }
      // is it a list item?
      if (nsHTMLEditUtils::IsListItem(curNode)) 
      {
        // if it is a list item, that means we are not outdenting whole list.
        // So we need to finish up dealing with any curBlockQuote, and then
        // pop this list item.
        if (curBlockQuote)
        {
          res = OutdentPartOfBlock(curBlockQuote, firstBQChild, lastBQChild,
                                   curBlockQuoteIsIndentedWithCSS,
                                   address_of(rememberedLeftBQ),
                                   address_of(rememberedRightBQ));
          NS_ENSURE_SUCCESS(res, res);
          curBlockQuote = 0;  firstBQChild = 0;  lastBQChild = 0;
          curBlockQuoteIsIndentedWithCSS = false;
        }
        bool bOutOfList;
        res = PopListItem(curNode, &bOutOfList);
        NS_ENSURE_SUCCESS(res, res);
        continue;
      }
      // do we have a blockquote that we are already committed to removing?
      if (curBlockQuote)
      {
        // if so, is this node a descendant?
        if (nsEditorUtils::IsDescendantOf(curNode, curBlockQuote))
        {
          lastBQChild = curNode;
          continue;  // then we don't need to do anything different for this node
        }
        else
        {
          // otherwise, we have progressed beyond end of curBlockQuote,
          // so lets handle it now.  We need to remove the portion of 
          // curBlockQuote that contains [firstBQChild - lastBQChild].
          res = OutdentPartOfBlock(curBlockQuote, firstBQChild, lastBQChild,
                                   curBlockQuoteIsIndentedWithCSS,
                                   address_of(rememberedLeftBQ),
                                   address_of(rememberedRightBQ));
          NS_ENSURE_SUCCESS(res, res);
          curBlockQuote = 0;  firstBQChild = 0;  lastBQChild = 0;
          curBlockQuoteIsIndentedWithCSS = false;
          // fall out and handle curNode
        }
      }
      
      // are we inside a blockquote?
      nsCOMPtr<nsIDOMNode> n = curNode;
      nsCOMPtr<nsIDOMNode> tmp;
      curBlockQuoteIsIndentedWithCSS = false;
      // keep looking up the hierarchy as long as we don't hit the body or the
      // active editing host or a table element (other than an entire table)
      while (!nsTextEditUtils::IsBody(n) && mHTMLEditor->IsDescendantOfEditorRoot(n)
          && (nsHTMLEditUtils::IsTable(n) || !nsHTMLEditUtils::IsTableElement(n)))
      {
        n->GetParentNode(getter_AddRefs(tmp));
        if (!tmp) {
          break;
        }
        n = tmp;
        if (nsHTMLEditUtils::IsBlockquote(n))
        {
          // if so, remember it, and remember first node we are taking out of it.
          curBlockQuote = n;
          firstBQChild  = curNode;
          lastBQChild   = curNode;
          break;
        }
        else if (useCSS)
        {
          nsIAtom* marginProperty = MarginPropertyAtomForIndent(mHTMLEditor->mHTMLCSSUtils, curNode);
          nsAutoString value;
          mHTMLEditor->mHTMLCSSUtils->GetSpecifiedProperty(n, marginProperty, value);
          float f;
          nsCOMPtr<nsIAtom> unit;
          mHTMLEditor->mHTMLCSSUtils->ParseLength(value, &f, getter_AddRefs(unit));
          if (f > 0 && !(nsHTMLEditUtils::IsList(curParent) && nsHTMLEditUtils::IsList(curNode)))
          {
            curBlockQuote = n;
            firstBQChild  = curNode;
            lastBQChild   = curNode;
            curBlockQuoteIsIndentedWithCSS = true;
            break;
          }
        }
      }

      if (!curBlockQuote)
      {
        // could not find an enclosing blockquote for this node.  handle list cases.
        if (nsHTMLEditUtils::IsList(curParent))  // move node out of list
        {
          if (nsHTMLEditUtils::IsList(curNode))  // just unwrap this sublist
          {
            res = mHTMLEditor->RemoveBlockContainer(curNode);
            NS_ENSURE_SUCCESS(res, res);
          }
          // handled list item case above
        }
        else if (nsHTMLEditUtils::IsList(curNode)) // node is a list, but parent is non-list: move list items out
        {
          nsCOMPtr<nsIDOMNode> child;
          curNode->GetLastChild(getter_AddRefs(child));
          while (child)
          {
            if (nsHTMLEditUtils::IsListItem(child))
            {
              bool bOutOfList;
              res = PopListItem(child, &bOutOfList);
              NS_ENSURE_SUCCESS(res, res);
            }
            else if (nsHTMLEditUtils::IsList(child))
            {
              // We have an embedded list, so move it out from under the
              // parent list. Be sure to put it after the parent list
              // because this loop iterates backwards through the parent's
              // list of children.

              res = mHTMLEditor->MoveNode(child, curParent, offset + 1);
              NS_ENSURE_SUCCESS(res, res);
            }
            else
            {
              // delete any non- list items for now
              res = mHTMLEditor->DeleteNode(child);
              NS_ENSURE_SUCCESS(res, res);
            }
            curNode->GetLastChild(getter_AddRefs(child));
          }
          // delete the now-empty list
          res = mHTMLEditor->RemoveBlockContainer(curNode);
          NS_ENSURE_SUCCESS(res, res);
        }
        else if (useCSS) {
          nsCOMPtr<nsIDOMElement> element;
          nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(curNode);
          if (textNode) {
            // We want to outdent the parent of text nodes
            nsCOMPtr<nsIDOMNode> parent;
            textNode->GetParentNode(getter_AddRefs(parent));
            element = do_QueryInterface(parent);
          } else {
            element = do_QueryInterface(curNode);
          }
          if (element) {
            RelativeChangeIndentationOfElementNode(element, -1);
          }
        }
      }
    }
    if (curBlockQuote)
    {
      // we have a blockquote we haven't finished handling
      res = OutdentPartOfBlock(curBlockQuote, firstBQChild, lastBQChild,
                               curBlockQuoteIsIndentedWithCSS,
                               address_of(rememberedLeftBQ),
                               address_of(rememberedRightBQ));
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  // make sure selection didn't stick to last piece of content in old bq
  // (only a problem for collapsed selections)
  if (rememberedLeftBQ || rememberedRightBQ) {
    if (aSelection->Collapsed()) {
      // push selection past end of rememberedLeftBQ
      nsCOMPtr<nsIDOMNode> sNode;
      PRInt32 sOffset;
      mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(sNode), &sOffset);
      if (rememberedLeftBQ &&
          ((sNode == rememberedLeftBQ) || nsEditorUtils::IsDescendantOf(sNode, rememberedLeftBQ)))
      {
        // selection is inside rememberedLeftBQ - push it past it.
        sNode = nsEditor::GetNodeLocation(rememberedLeftBQ, &sOffset);
        sOffset++;
        aSelection->Collapse(sNode, sOffset);
      }
      // and pull selection before beginning of rememberedRightBQ
      mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(sNode), &sOffset);
      if (rememberedRightBQ &&
          ((sNode == rememberedRightBQ) || nsEditorUtils::IsDescendantOf(sNode, rememberedRightBQ)))
      {
        // selection is inside rememberedRightBQ - push it before it.
        sNode = nsEditor::GetNodeLocation(rememberedRightBQ, &sOffset);
        aSelection->Collapse(sNode, sOffset);
      }
    }
    return NS_OK;
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// RemovePartOfBlock:  split aBlock and move aStartChild to aEndChild out
//                     of aBlock.  return left side of block (if any) in
//                     aLeftNode.  return right side of block (if any) in
//                     aRightNode.  
//                  
nsresult 
nsHTMLEditRules::RemovePartOfBlock(nsIDOMNode *aBlock, 
                                   nsIDOMNode *aStartChild, 
                                   nsIDOMNode *aEndChild,
                                   nsCOMPtr<nsIDOMNode> *aLeftNode,
                                   nsCOMPtr<nsIDOMNode> *aRightNode)
{
  nsCOMPtr<nsIDOMNode> middleNode;
  nsresult res = SplitBlock(aBlock, aStartChild, aEndChild,
                            aLeftNode, aRightNode,
                            address_of(middleNode));
  NS_ENSURE_SUCCESS(res, res);
  // get rid of part of blockquote we are outdenting

  return mHTMLEditor->RemoveBlockContainer(aBlock);
}

nsresult 
nsHTMLEditRules::SplitBlock(nsIDOMNode *aBlock, 
                            nsIDOMNode *aStartChild, 
                            nsIDOMNode *aEndChild,
                            nsCOMPtr<nsIDOMNode> *aLeftNode,
                            nsCOMPtr<nsIDOMNode> *aRightNode,
                            nsCOMPtr<nsIDOMNode> *aMiddleNode)
{
  NS_ENSURE_TRUE(aBlock && aStartChild && aEndChild, NS_ERROR_NULL_POINTER);
  
  nsCOMPtr<nsIDOMNode> leftNode, rightNode;
  PRInt32 startOffset, endOffset, offset;
  nsresult res;

  // get split point location
  nsCOMPtr<nsIDOMNode> startParent = nsEditor::GetNodeLocation(aStartChild, &startOffset);
  
  // do the splits!
  res = mHTMLEditor->SplitNodeDeep(aBlock, startParent, startOffset, &offset, 
                                   true, address_of(leftNode), address_of(rightNode));
  NS_ENSURE_SUCCESS(res, res);
  if (rightNode)  aBlock = rightNode;

  // remember left portion of block if caller requested
  if (aLeftNode) 
    *aLeftNode = leftNode;

  // get split point location
  nsCOMPtr<nsIDOMNode> endParent = nsEditor::GetNodeLocation(aEndChild, &endOffset);
  endOffset++;  // want to be after lastBQChild

  // do the splits!
  res = mHTMLEditor->SplitNodeDeep(aBlock, endParent, endOffset, &offset, 
                                   true, address_of(leftNode), address_of(rightNode));
  NS_ENSURE_SUCCESS(res, res);
  if (leftNode)  aBlock = leftNode;
  
  // remember right portion of block if caller requested
  if (aRightNode) 
    *aRightNode = rightNode;

  if (aMiddleNode)
    *aMiddleNode = aBlock;

  return NS_OK;
}

nsresult
nsHTMLEditRules::OutdentPartOfBlock(nsIDOMNode *aBlock, 
                                    nsIDOMNode *aStartChild, 
                                    nsIDOMNode *aEndChild,
                                    bool aIsBlockIndentedWithCSS,
                                    nsCOMPtr<nsIDOMNode> *aLeftNode,
                                    nsCOMPtr<nsIDOMNode> *aRightNode)
{
  nsCOMPtr<nsIDOMNode> middleNode;
  nsresult res = SplitBlock(aBlock, aStartChild, aEndChild, 
                            aLeftNode,
                            aRightNode,
                            address_of(middleNode));
  NS_ENSURE_SUCCESS(res, res);
  if (aIsBlockIndentedWithCSS)
    res = RelativeChangeIndentationOfElementNode(middleNode, -1);
  else
    res = mHTMLEditor->RemoveBlockContainer(middleNode);
  return res;
}

///////////////////////////////////////////////////////////////////////////
// ConvertListType:  convert list type and list item type.
//                
//                  
nsresult
nsHTMLEditRules::ConvertListType(nsIDOMNode* aList,
                                 nsCOMPtr<nsIDOMNode>* outList,
                                 nsIAtom* aListType,
                                 nsIAtom* aItemType)
{
  MOZ_ASSERT(aListType);
  MOZ_ASSERT(aItemType);

  NS_ENSURE_TRUE(aList && outList, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsINode> list = do_QueryInterface(aList);
  NS_ENSURE_STATE(list);

  nsCOMPtr<dom::Element> outNode;
  nsresult rv = ConvertListType(list, getter_AddRefs(outNode), aListType, aItemType);
  *outList = outNode ? outNode->AsDOMNode() : nsnull;
  return rv;
}

nsresult
nsHTMLEditRules::ConvertListType(nsINode* aList,
                                 dom::Element** aOutList,
                                 nsIAtom* aListType,
                                 nsIAtom* aItemType)
{
  MOZ_ASSERT(aList);
  MOZ_ASSERT(aOutList);
  MOZ_ASSERT(aListType);
  MOZ_ASSERT(aItemType);

  nsCOMPtr<nsINode> child = aList->GetFirstChild();
  while (child)
  {
    if (child->IsElement()) {
      dom::Element* element = child->AsElement();
      if (nsHTMLEditUtils::IsListItem(element) && !element->IsHTML(aItemType)) {
        nsCOMPtr<dom::Element> temp;
        nsresult rv =
          mHTMLEditor->ReplaceContainer(child, getter_AddRefs(temp),
                                        nsDependentAtomString(aItemType));
        NS_ENSURE_SUCCESS(rv, rv);
        child = temp.forget();
      } else if (nsHTMLEditUtils::IsList(element) &&
                 !element->IsHTML(aListType)) {
        nsCOMPtr<dom::Element> temp;
        nsresult rv =
          ConvertListType(child, getter_AddRefs(temp), aListType, aItemType);
        NS_ENSURE_SUCCESS(rv, rv);
        child = temp.forget();
      }
    }
    child = child->GetNextSibling();
  }

  if (aList->IsElement() && aList->AsElement()->IsHTML(aListType)) {
    nsCOMPtr<dom::Element> list = aList->AsElement();
    list.forget(aOutList);
    return NS_OK;
  }

  return mHTMLEditor->ReplaceContainer(aList, aOutList,
                                       nsDependentAtomString(aListType));
}


///////////////////////////////////////////////////////////////////////////
// CreateStyleForInsertText:  take care of clearing and setting appropriate
//                            style nodes for text insertion.
//
//
nsresult
nsHTMLEditRules::CreateStyleForInsertText(nsISelection *aSelection,
                                          nsIDOMDocument *aDoc)
{
  MOZ_ASSERT(aSelection && aDoc && mHTMLEditor->mTypeInState);

  bool weDidSomething = false;
  nsCOMPtr<nsIDOMNode> node, tmp;
  PRInt32 offset;
  nsresult res = mHTMLEditor->GetStartNodeAndOffset(aSelection,
                                                    getter_AddRefs(node),
                                                    &offset);
  NS_ENSURE_SUCCESS(res, res);

  // next examine our present style and make sure default styles are either
  // present or explicitly overridden.  If neither, add the default style to
  // the TypeInState
  PRInt32 length = mHTMLEditor->mDefaultStyles.Length();
  for (PRInt32 j = 0; j < length; j++) {
    PropItem* propItem = mHTMLEditor->mDefaultStyles[j];
    MOZ_ASSERT(propItem);
    bool bFirst, bAny, bAll;

    // GetInlineProperty also examine TypeInState.  The only gotcha here is
    // that a cleared property looks like an unset property.  For now I'm
    // assuming that's not a problem: that default styles will always be
    // multivalue styles (like font face or size) where clearing the style
    // means we want to go back to the default.  If we ever wanted a "toggle"
    // style like bold for a default, though, I'll have to add code to detect
    // the difference between unset and explicitly cleared, else user would
    // never be able to unbold, for instance.
    nsAutoString curValue;
    res = mHTMLEditor->GetInlinePropertyBase(propItem->tag, &propItem->attr,
                                             nsnull, &bFirst, &bAny, &bAll,
                                             &curValue, false);
    NS_ENSURE_SUCCESS(res, res);

    if (!bAny) {
      // no style set for this prop/attr
      mHTMLEditor->mTypeInState->SetProp(propItem->tag, propItem->attr,
                                         propItem->value);
    }
  }

  nsCOMPtr<nsIDOMElement> rootElement;
  res = aDoc->GetDocumentElement(getter_AddRefs(rootElement));
  NS_ENSURE_SUCCESS(res, res);

  // process clearing any styles first
  nsAutoPtr<PropItem> item(mHTMLEditor->mTypeInState->TakeClearProperty());
  while (item && node != rootElement) {
    res = mHTMLEditor->ClearStyle(address_of(node), &offset,
                                  item->tag, &item->attr);
    NS_ENSURE_SUCCESS(res, res);
    item = mHTMLEditor->mTypeInState->TakeClearProperty();
    weDidSomething = true;
  }

  // then process setting any styles
  PRInt32 relFontSize = mHTMLEditor->mTypeInState->TakeRelativeFontSize();
  item = mHTMLEditor->mTypeInState->TakeSetProperty();

  if (item || relFontSize) {
    // we have at least one style to add; make a new text node to insert style
    // nodes above.
    if (mHTMLEditor->IsTextNode(node)) {
      // if we are in a text node, split it
      res = mHTMLEditor->SplitNodeDeep(node, node, offset, &offset);
      NS_ENSURE_SUCCESS(res, res);
      node->GetParentNode(getter_AddRefs(tmp));
      node = tmp;
    }
    if (!mHTMLEditor->IsContainer(node)) {
      return NS_OK;
    }
    nsCOMPtr<nsIDOMNode> newNode;
    nsCOMPtr<nsIDOMText> nodeAsText;
    res = aDoc->CreateTextNode(EmptyString(), getter_AddRefs(nodeAsText));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(nodeAsText, NS_ERROR_NULL_POINTER);
    newNode = do_QueryInterface(nodeAsText);
    res = mHTMLEditor->InsertNode(newNode, node, offset);
    NS_ENSURE_SUCCESS(res, res);
    node = newNode;
    offset = 0;
    weDidSomething = true;

    if (relFontSize) {
      // dir indicated bigger versus smaller.  1 = bigger, -1 = smaller
      PRInt32 dir = relFontSize > 0 ? 1 : -1;
      for (PRInt32 j = 0; j < abs(relFontSize); j++) {
        res = mHTMLEditor->RelativeFontChangeOnTextNode(dir, nodeAsText,
                                                        0, -1);
        NS_ENSURE_SUCCESS(res, res);
      }
    }

    while (item) {
      res = mHTMLEditor->SetInlinePropertyOnNode(node, item->tag, &item->attr,
                                                 &item->value);
      NS_ENSURE_SUCCESS(res, res);
      item = mHTMLEditor->mTypeInState->TakeSetProperty();
    }
  }
  if (weDidSomething) {
    return aSelection->Collapse(node, offset);
  }

  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// IsEmptyBlock: figure out if aNode is (or is inside) an empty block.
//               A block can have children and still be considered empty,
//               if the children are empty or non-editable.
//                  
nsresult 
nsHTMLEditRules::IsEmptyBlock(nsIDOMNode *aNode, 
                              bool *outIsEmptyBlock, 
                              bool aMozBRDoesntCount,
                              bool aListItemsNotEmpty) 
{
  NS_ENSURE_TRUE(aNode && outIsEmptyBlock, NS_ERROR_NULL_POINTER);
  *outIsEmptyBlock = true;
  
//  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> nodeToTest;
  if (IsBlockNode(aNode)) nodeToTest = do_QueryInterface(aNode);
//  else nsCOMPtr<nsIDOMElement> block;
//  looks like I forgot to finish this.  Wonder what I was going to do?

  NS_ENSURE_TRUE(nodeToTest, NS_ERROR_NULL_POINTER);
  return mHTMLEditor->IsEmptyNode(nodeToTest, outIsEmptyBlock,
                     aMozBRDoesntCount, aListItemsNotEmpty);
}


nsresult
nsHTMLEditRules::WillAlign(nsISelection *aSelection, 
                           const nsAString *alignType, 
                           bool *aCancel,
                           bool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }

  nsresult res = WillInsert(aSelection, aCancel);
  NS_ENSURE_SUCCESS(res, res);

  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = false;
  *aHandled = false;

  res = NormalizeSelection(aSelection);
  NS_ENSURE_SUCCESS(res, res);
  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);

  // convert the selection ranges into "promoted" selection ranges:
  // this basically just expands the range to include the immediate
  // block parent, and then further expands to include any ancestors
  // whose children are all in the range
  *aHandled = true;
  nsCOMArray<nsIDOMNode> arrayOfNodes;
  res = GetNodesFromSelection(aSelection, nsEditor::kOpAlign, arrayOfNodes);
  NS_ENSURE_SUCCESS(res, res);

  // if we don't have any nodes, or we have only a single br, then we are
  // creating an empty alignment div.  We have to do some different things for these.
  bool emptyDiv = false;
  PRInt32 listCount = arrayOfNodes.Count();
  if (!listCount) emptyDiv = true;
  if (listCount == 1)
  {
    nsCOMPtr<nsIDOMNode> theNode = arrayOfNodes[0];

    if (nsHTMLEditUtils::SupportsAlignAttr(theNode))
    {
      // the node is a table element, an horiz rule, a paragraph, a div
      // or a section header; in HTML 4, it can directly carry the ALIGN
      // attribute and we don't need to make a div! If we are in CSS mode,
      // all the work is done in AlignBlock
      nsCOMPtr<nsIDOMElement> theElem = do_QueryInterface(theNode);
      res = AlignBlock(theElem, alignType, true);
      NS_ENSURE_SUCCESS(res, res);
      return NS_OK;
    }

    if (nsTextEditUtils::IsBreak(theNode))
    {
      // The special case emptyDiv code (below) that consumes BRs can
      // cause tables to split if the start node of the selection is
      // not in a table cell or caption, for example parent is a <tr>.
      // Avoid this unnecessary splitting if possible by leaving emptyDiv
      // FALSE so that we fall through to the normal case alignment code.
      //
      // XXX: It seems a little error prone for the emptyDiv special
      //      case code to assume that the start node of the selection
      //      is the parent of the single node in the arrayOfNodes, as
      //      the paragraph above points out. Do we rely on the selection
      //      start node because of the fact that arrayOfNodes can be empty?
      //      We should probably revisit this issue. - kin

      nsCOMPtr<nsIDOMNode> parent;
      PRInt32 offset;
      res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(parent), &offset);

      if (!nsHTMLEditUtils::IsTableElement(parent) || nsHTMLEditUtils::IsTableCellOrCaption(parent))
        emptyDiv = true;
    }
  }
  if (emptyDiv)
  {
    PRInt32 offset;
    nsCOMPtr<nsIDOMNode> brNode, parent, theDiv, sib;
    NS_NAMED_LITERAL_STRING(divType, "div");
    res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(parent), &offset);
    NS_ENSURE_SUCCESS(res, res);
    res = SplitAsNeeded(&divType, address_of(parent), &offset);
    NS_ENSURE_SUCCESS(res, res);
    // consume a trailing br, if any.  This is to keep an alignment from
    // creating extra lines, if possible.
    res = mHTMLEditor->GetNextHTMLNode(parent, offset, address_of(brNode));
    NS_ENSURE_SUCCESS(res, res);
    if (brNode && nsTextEditUtils::IsBreak(brNode))
    {
      // making use of html structure... if next node after where
      // we are putting our div is not a block, then the br we 
      // found is in same block we are, so its safe to consume it.
      res = mHTMLEditor->GetNextHTMLSibling(parent, offset, address_of(sib));
      NS_ENSURE_SUCCESS(res, res);
      if (!IsBlockNode(sib))
      {
        res = mHTMLEditor->DeleteNode(brNode);
        NS_ENSURE_SUCCESS(res, res);
      }
    }
    res = mHTMLEditor->CreateNode(divType, parent, offset, getter_AddRefs(theDiv));
    NS_ENSURE_SUCCESS(res, res);
    // remember our new block for postprocessing
    mNewBlock = theDiv;
    // set up the alignment on the div, using HTML or CSS
    nsCOMPtr<nsIDOMElement> divElem = do_QueryInterface(theDiv);
    res = AlignBlock(divElem, alignType, true);
    NS_ENSURE_SUCCESS(res, res);
    *aHandled = true;
    // put in a moz-br so that it won't get deleted
    res = CreateMozBR(theDiv, 0);
    NS_ENSURE_SUCCESS(res, res);
    res = aSelection->Collapse(theDiv, 0);
    selectionResetter.Abort();  // don't reset our selection in this case.
    return res;
  }

  // Next we detect all the transitions in the array, where a transition
  // means that adjacent nodes in the array don't have the same parent.

  nsTArray<bool> transitionList;
  res = MakeTransitionList(arrayOfNodes, transitionList);
  NS_ENSURE_SUCCESS(res, res);                                 

  // Ok, now go through all the nodes and give them an align attrib or put them in a div, 
  // or whatever is appropriate.  Wohoo!

  nsCOMPtr<nsIDOMNode> curParent;
  nsCOMPtr<nsIDOMNode> curDiv;
  bool useCSS = mHTMLEditor->IsCSSEnabled();
  for (PRInt32 i = 0; i < listCount; ++i) {
    // here's where we actually figure out what to do
    nsCOMPtr<nsIDOMNode> curNode = arrayOfNodes[i];

    // Ignore all non-editable nodes.  Leave them be.
    if (!mHTMLEditor->IsEditable(curNode)) continue;

    PRInt32 offset;
    curParent = nsEditor::GetNodeLocation(curNode, &offset);

    // the node is a table element, an horiz rule, a paragraph, a div
    // or a section header; in HTML 4, it can directly carry the ALIGN
    // attribute and we don't need to nest it, just set the alignment.
    // In CSS, assign the corresponding CSS styles in AlignBlock
    if (nsHTMLEditUtils::SupportsAlignAttr(curNode))
    {
      nsCOMPtr<nsIDOMElement> curElem = do_QueryInterface(curNode);
      res = AlignBlock(curElem, alignType, false);
      NS_ENSURE_SUCCESS(res, res);
      // clear out curDiv so that we don't put nodes after this one into it
      curDiv = 0;
      continue;
    }

    // Skip insignificant formatting text nodes to prevent
    // unnecessary structure splitting!
    if (nsEditor::IsTextNode(curNode) &&
       ((nsHTMLEditUtils::IsTableElement(curParent) && !nsHTMLEditUtils::IsTableCellOrCaption(curParent)) ||
        nsHTMLEditUtils::IsList(curParent)))
      continue;

    // if it's a list item, or a list
    // inside a list, forget any "current" div, and instead put divs inside
    // the appropriate block (td, li, etc)
    if ( nsHTMLEditUtils::IsListItem(curNode)
         || nsHTMLEditUtils::IsList(curNode))
    {
      res = RemoveAlignment(curNode, *alignType, true);
      NS_ENSURE_SUCCESS(res, res);
      if (useCSS) {
        nsCOMPtr<nsIDOMElement> curElem = do_QueryInterface(curNode);
        NS_NAMED_LITERAL_STRING(attrName, "align");
        PRInt32 count;
        mHTMLEditor->mHTMLCSSUtils->SetCSSEquivalentToHTMLStyle(curNode, nsnull,
                                                                &attrName, alignType,
                                                                &count, false);
        curDiv = 0;
        continue;
      }
      else if (nsHTMLEditUtils::IsList(curParent)) {
        // if we don't use CSS, add a contraint to list element : they have
        // to be inside another list, ie >= second level of nesting
        res = AlignInnerBlocks(curNode, alignType);
        NS_ENSURE_SUCCESS(res, res);
        curDiv = 0;
        continue;
      }
      // clear out curDiv so that we don't put nodes after this one into it
    }      

    // need to make a div to put things in if we haven't already,
    // or if this node doesn't go in div we used earlier.
    if (!curDiv || transitionList[i])
    {
      // First, check that our element can contain a div.
      NS_NAMED_LITERAL_STRING(divType, "div");
      if (!mEditor->CanContainTag(curParent, nsGkAtoms::div)) {
        return NS_OK; // cancelled
      }

      res = SplitAsNeeded(&divType, address_of(curParent), &offset);
      NS_ENSURE_SUCCESS(res, res);
      res = mHTMLEditor->CreateNode(divType, curParent, offset, getter_AddRefs(curDiv));
      NS_ENSURE_SUCCESS(res, res);
      // remember our new block for postprocessing
      mNewBlock = curDiv;
      // set up the alignment on the div
      nsCOMPtr<nsIDOMElement> divElem = do_QueryInterface(curDiv);
      res = AlignBlock(divElem, alignType, true);
      //nsAutoString attr(NS_LITERAL_STRING("align"));
      //res = mHTMLEditor->SetAttribute(divElem, attr, *alignType);
      //NS_ENSURE_SUCCESS(res, res);
      // curDiv is now the correct thing to put curNode in
    }

    // tuck the node into the end of the active div
    res = mHTMLEditor->MoveNode(curNode, curDiv, -1);
    NS_ENSURE_SUCCESS(res, res);
  }

  return res;
}


///////////////////////////////////////////////////////////////////////////
// AlignInnerBlocks: align inside table cells or list items
//       
nsresult
nsHTMLEditRules::AlignInnerBlocks(nsIDOMNode *aNode, const nsAString *alignType)
{
  NS_ENSURE_TRUE(aNode && alignType, NS_ERROR_NULL_POINTER);
  nsresult res;
  
  // gather list of table cells or list items
  nsCOMArray<nsIDOMNode> arrayOfNodes;
  nsTableCellAndListItemFunctor functor;
  nsDOMIterator iter;
  res = iter.Init(aNode);
  NS_ENSURE_SUCCESS(res, res);
  res = iter.AppendList(functor, arrayOfNodes);
  NS_ENSURE_SUCCESS(res, res);
  
  // now that we have the list, align their contents as requested
  PRInt32 listCount = arrayOfNodes.Count();
  PRInt32 j;

  for (j = 0; j < listCount; j++)
  {
    nsIDOMNode* node = arrayOfNodes[0];
    res = AlignBlockContents(node, alignType);
    NS_ENSURE_SUCCESS(res, res);
    arrayOfNodes.RemoveObjectAt(0);
  }

  return res;  
}


///////////////////////////////////////////////////////////////////////////
// AlignBlockContents: align contents of a block element
//                  
nsresult
nsHTMLEditRules::AlignBlockContents(nsIDOMNode *aNode, const nsAString *alignType)
{
  NS_ENSURE_TRUE(aNode && alignType, NS_ERROR_NULL_POINTER);
  nsresult res;
  nsCOMPtr <nsIDOMNode> firstChild, lastChild, divNode;
  
  bool useCSS = mHTMLEditor->IsCSSEnabled();

  res = mHTMLEditor->GetFirstEditableChild(aNode, address_of(firstChild));
  NS_ENSURE_SUCCESS(res, res);
  res = mHTMLEditor->GetLastEditableChild(aNode, address_of(lastChild));
  NS_ENSURE_SUCCESS(res, res);
  NS_NAMED_LITERAL_STRING(attr, "align");
  if (!firstChild)
  {
    // this cell has no content, nothing to align
  }
  else if ((firstChild==lastChild) && nsHTMLEditUtils::IsDiv(firstChild))
  {
    // the cell already has a div containing all of its content: just
    // act on this div.
    nsCOMPtr<nsIDOMElement> divElem = do_QueryInterface(firstChild);
    if (useCSS) {
      res = mHTMLEditor->SetAttributeOrEquivalent(divElem, attr, *alignType, false); 
    }
    else {
      res = mHTMLEditor->SetAttribute(divElem, attr, *alignType);
    }
    NS_ENSURE_SUCCESS(res, res);
  }
  else
  {
    // else we need to put in a div, set the alignment, and toss in all the children
    res = mHTMLEditor->CreateNode(NS_LITERAL_STRING("div"), aNode, 0, getter_AddRefs(divNode));
    NS_ENSURE_SUCCESS(res, res);
    // set up the alignment on the div
    nsCOMPtr<nsIDOMElement> divElem = do_QueryInterface(divNode);
    if (useCSS) {
      res = mHTMLEditor->SetAttributeOrEquivalent(divElem, attr, *alignType, false); 
    }
    else {
      res = mHTMLEditor->SetAttribute(divElem, attr, *alignType);
    }
    NS_ENSURE_SUCCESS(res, res);
    // tuck the children into the end of the active div
    while (lastChild && (lastChild != divNode))
    {
      res = mHTMLEditor->MoveNode(lastChild, divNode, 0);
      NS_ENSURE_SUCCESS(res, res);
      res = mHTMLEditor->GetLastEditableChild(aNode, address_of(lastChild));
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  return res;
}

///////////////////////////////////////////////////////////////////////////
// CheckForEmptyBlock: Called by WillDeleteSelection to detect and handle
//                     case of deleting from inside an empty block.
//                  
nsresult
nsHTMLEditRules::CheckForEmptyBlock(nsIDOMNode *aStartNode, 
                                    nsIDOMNode *aBodyNode,
                                    nsISelection *aSelection,
                                    bool *aHandled)
{
  // If the editing host is an inline element, bail out early.
  if (IsInlineNode(aBodyNode)) {
    return NS_OK;
  }
  // if we are inside an empty block, delete it.
  // Note: do NOT delete table elements this way.
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> block, emptyBlock;
  if (IsBlockNode(aStartNode)) 
    block = aStartNode;
  else
    block = mHTMLEditor->GetBlockNodeParent(aStartNode);
  bool bIsEmptyNode;
  if (block != aBodyNode)  // efficiency hack. avoiding IsEmptyNode() call when in body
  {
    res = mHTMLEditor->IsEmptyNode(block, &bIsEmptyNode, true, false);
    NS_ENSURE_SUCCESS(res, res);
    while (bIsEmptyNode && !nsHTMLEditUtils::IsTableElement(block) && (block != aBodyNode))
    {
      emptyBlock = block;
      block = mHTMLEditor->GetBlockNodeParent(emptyBlock);
      res = mHTMLEditor->IsEmptyNode(block, &bIsEmptyNode, true, false);
      NS_ENSURE_SUCCESS(res, res);
    }
  }

  nsCOMPtr<nsIContent> emptyContent = do_QueryInterface(emptyBlock);
  if (emptyBlock && emptyContent->IsEditable())
  {
    PRInt32 offset;
    nsCOMPtr<nsIDOMNode> blockParent = nsEditor::GetNodeLocation(emptyBlock, &offset);
    NS_ENSURE_TRUE(blockParent && offset >= 0, NS_ERROR_FAILURE);

    if (nsHTMLEditUtils::IsListItem(emptyBlock))
    {
      // are we the first list item in the list?
      bool bIsFirst;
      res = mHTMLEditor->IsFirstEditableChild(emptyBlock, &bIsFirst);
      NS_ENSURE_SUCCESS(res, res);
      if (bIsFirst)
      {
        PRInt32 listOffset;
        nsCOMPtr<nsIDOMNode> listParent = nsEditor::GetNodeLocation(blockParent,
                                                                    &listOffset);
        NS_ENSURE_TRUE(listParent && listOffset >= 0, NS_ERROR_FAILURE);
        // if we are a sublist, skip the br creation
        if (!nsHTMLEditUtils::IsList(listParent))
        {
          // create a br before list
          nsCOMPtr<nsIDOMNode> brNode;
          res = mHTMLEditor->CreateBR(listParent, listOffset, address_of(brNode));
          NS_ENSURE_SUCCESS(res, res);
          // adjust selection to be right before it
          res = aSelection->Collapse(listParent, listOffset);
          NS_ENSURE_SUCCESS(res, res);
        }
        // else just let selection perculate up.  We'll adjust it in AfterEdit()
      }
    }
    else
    {
      // adjust selection to be right after it
      res = aSelection->Collapse(blockParent, offset+1);
      NS_ENSURE_SUCCESS(res, res);
    }
    res = mHTMLEditor->DeleteNode(emptyBlock);
    *aHandled = true;
  }
  return res;
}

nsresult
nsHTMLEditRules::CheckForInvisibleBR(nsIDOMNode *aBlock, 
                                     BRLocation aWhere, 
                                     nsCOMPtr<nsIDOMNode> *outBRNode,
                                     PRInt32 aOffset)
{
  NS_ENSURE_TRUE(aBlock && outBRNode, NS_ERROR_NULL_POINTER);
  *outBRNode = nsnull;

  nsCOMPtr<nsIDOMNode> testNode;
  PRInt32 testOffset = 0;
  bool runTest = false;

  if (aWhere == kBlockEnd)
  {
    nsCOMPtr<nsIDOMNode> rightmostNode =
      mHTMLEditor->GetRightmostChild(aBlock, true); // no block crossing

    if (rightmostNode)
    {
      PRInt32 nodeOffset;
      nsCOMPtr<nsIDOMNode> nodeParent = nsEditor::GetNodeLocation(rightmostNode,
                                                                  &nodeOffset);
      runTest = true;
      testNode = nodeParent;
      // use offset + 1, because we want the last node included in our
      // evaluation
      testOffset = nodeOffset + 1;
    }
  }
  else if (aOffset)
  {
    runTest = true;
    testNode = aBlock;
    // we'll check everything to the left of the input position
    testOffset = aOffset;
  }

  if (runTest)
  {
    nsWSRunObject wsTester(mHTMLEditor, testNode, testOffset);
    if (nsWSRunObject::eBreak == wsTester.mStartReason)
    {
      *outBRNode = wsTester.mStartReasonNode;
    }
  }

  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// GetInnerContent: aList and aTbl allow the caller to specify what kind 
//                  of content to "look inside".  If aTbl is true, look inside
//                  any table content, and insert the inner content into the
//                  supplied issupportsarray at offset aIndex.  
//                  Similarly with aList and list content.
//                  aIndex is updated to point past inserted elements.
//                  
nsresult
nsHTMLEditRules::GetInnerContent(nsIDOMNode *aNode, nsCOMArray<nsIDOMNode> &outArrayOfNodes, 
                                 PRInt32 *aIndex, bool aList, bool aTbl)
{
  NS_ENSURE_TRUE(aNode && aIndex, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMNode> node;
  
  nsresult res = mHTMLEditor->GetFirstEditableChild(aNode, address_of(node));
  while (NS_SUCCEEDED(res) && node)
  {
    if (  ( aList && (nsHTMLEditUtils::IsList(node)     || 
                      nsHTMLEditUtils::IsListItem(node) ) )
       || ( aTbl && nsHTMLEditUtils::IsTableElement(node) )  )
    {
      res = GetInnerContent(node, outArrayOfNodes, aIndex, aList, aTbl);
      NS_ENSURE_SUCCESS(res, res);
    }
    else
    {
      outArrayOfNodes.InsertObjectAt(node, *aIndex);
      (*aIndex)++;
    }
    nsCOMPtr<nsIDOMNode> tmp;
    res = node->GetNextSibling(getter_AddRefs(tmp));
    node = tmp;
  }

  return res;
}

///////////////////////////////////////////////////////////////////////////
// ExpandSelectionForDeletion: this promotes our selection to include blocks
// that have all their children selected.
//                  
nsresult
nsHTMLEditRules::ExpandSelectionForDeletion(nsISelection *aSelection)
{
  NS_ENSURE_TRUE(aSelection, NS_ERROR_NULL_POINTER);
  
  // don't need to touch collapsed selections
  if (aSelection->Collapsed()) {
    return NS_OK;
  }

  PRInt32 rangeCount;
  nsresult res = aSelection->GetRangeCount(&rangeCount);
  NS_ENSURE_SUCCESS(res, res);
  
  // we don't need to mess with cell selections, and we assume multirange selections are those.
  if (rangeCount != 1) return NS_OK;
  
  // find current sel start and end
  nsCOMPtr<nsIDOMRange> range;
  res = aSelection->GetRangeAt(0, getter_AddRefs(range));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(range, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIDOMNode> selStartNode, selEndNode, selCommon;
  PRInt32 selStartOffset, selEndOffset;
  
  res = range->GetStartContainer(getter_AddRefs(selStartNode));
  NS_ENSURE_SUCCESS(res, res);
  res = range->GetStartOffset(&selStartOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = range->GetEndContainer(getter_AddRefs(selEndNode));
  NS_ENSURE_SUCCESS(res, res);
  res = range->GetEndOffset(&selEndOffset);
  NS_ENSURE_SUCCESS(res, res);

  // find current selection common block parent
  res = range->GetCommonAncestorContainer(getter_AddRefs(selCommon));
  NS_ENSURE_SUCCESS(res, res);
  if (!IsBlockNode(selCommon))
    selCommon = nsHTMLEditor::GetBlockNodeParent(selCommon);

  // set up for loops and cache our root element
  bool stillLooking = true;
  nsCOMPtr<nsIDOMNode> visNode, firstBRParent;
  PRInt32 visOffset=0, firstBROffset=0;
  PRInt16 wsType;
  nsCOMPtr<nsIContent> rootContent = mHTMLEditor->GetActiveEditingHost();
  nsCOMPtr<nsIDOMNode> rootElement = do_QueryInterface(rootContent);
  NS_ENSURE_TRUE(rootElement, NS_ERROR_FAILURE);

  // find previous visible thingy before start of selection
  if ((selStartNode!=selCommon) && (selStartNode!=rootElement))
  {
    while (stillLooking)
    {
      nsWSRunObject wsObj(mHTMLEditor, selStartNode, selStartOffset);
      wsObj.PriorVisibleNode(selStartNode, selStartOffset, address_of(visNode),
                             &visOffset, &wsType);
      if (wsType == nsWSRunObject::eThisBlock)
      {
        // we want to keep looking up.  But stop if we are crossing table element
        // boundaries, or if we hit the root.
        if ( nsHTMLEditUtils::IsTableElement(wsObj.mStartReasonNode) ||
            (selCommon == wsObj.mStartReasonNode)                    ||
            (rootElement == wsObj.mStartReasonNode) )
        {
          stillLooking = false;
        }
        else
        { 
          selStartNode = nsEditor::GetNodeLocation(wsObj.mStartReasonNode,
                                                   &selStartOffset);
        }
      }
      else
      {
        stillLooking = false;
      }
    }
  }
  
  stillLooking = true;
  // find next visible thingy after end of selection
  if ((selEndNode!=selCommon) && (selEndNode!=rootElement))
  {
    while (stillLooking)
    {
      nsWSRunObject wsObj(mHTMLEditor, selEndNode, selEndOffset);
      wsObj.NextVisibleNode(selEndNode, selEndOffset, address_of(visNode),
                            &visOffset, &wsType);
      if (wsType == nsWSRunObject::eBreak)
      {
        if (mHTMLEditor->IsVisBreak(wsObj.mEndReasonNode))
        {
          stillLooking = false;
        }
        else
        { 
          if (!firstBRParent)
          {
            firstBRParent = selEndNode;
            firstBROffset = selEndOffset;
          }
          selEndNode = nsEditor::GetNodeLocation(wsObj.mEndReasonNode, &selEndOffset);
          ++selEndOffset;
        }
      }
      else if (wsType == nsWSRunObject::eThisBlock)
      {
        // we want to keep looking up.  But stop if we are crossing table element
        // boundaries, or if we hit the root.
        if ( nsHTMLEditUtils::IsTableElement(wsObj.mEndReasonNode) ||
            (selCommon == wsObj.mEndReasonNode)                    ||
            (rootElement == wsObj.mEndReasonNode) )
        {
          stillLooking = false;
        }
        else
        { 
          selEndNode = nsEditor::GetNodeLocation(wsObj.mEndReasonNode, &selEndOffset);
          ++selEndOffset;
        }
       }
      else
      {
        stillLooking = false;
      }
    }
  }
  // now set the selection to the new range
  aSelection->Collapse(selStartNode, selStartOffset);
  
  // expand selection endpoint only if we didnt pass a br,
  // or if we really needed to pass that br (ie, its block is now 
  // totally selected)
  bool doEndExpansion = true;
  if (firstBRParent)
  {
    // find block node containing br
    nsCOMPtr<nsIDOMNode> brBlock = firstBRParent;
    if (!IsBlockNode(brBlock))
      brBlock = nsHTMLEditor::GetBlockNodeParent(brBlock);
    bool nodeBefore=false, nodeAfter=false;
    
    // create a range that represents expanded selection
    nsRefPtr<nsRange> range = new nsRange();
    res = range->SetStart(selStartNode, selStartOffset);
    NS_ENSURE_SUCCESS(res, res);
    res = range->SetEnd(selEndNode, selEndOffset);
    NS_ENSURE_SUCCESS(res, res);
    
    // check if block is entirely inside range
    nsCOMPtr<nsIContent> brContentBlock = do_QueryInterface(brBlock);
    res = nsRange::CompareNodeToRange(brContentBlock, range, &nodeBefore, &nodeAfter);
    
    // if block isn't contained, forgo grabbing the br in the expanded selection
    if (nodeBefore || nodeAfter)
      doEndExpansion = false;
  }
  if (doEndExpansion)
  {
    res = aSelection->Extend(selEndNode, selEndOffset);
  }
  else
  {
    // only expand to just before br
    res = aSelection->Extend(firstBRParent, firstBROffset);
  }
  
  return res;
}


///////////////////////////////////////////////////////////////////////////
// NormalizeSelection:  tweak non-collapsed selections to be more "natural".
//    Idea here is to adjust selection endpoint so that they do not cross
//    breaks or block boundaries unless something editable beyond that boundary
//    is also selected.  This adjustment makes it much easier for the various
//    block operations to determine what nodes to act on.
//                       
nsresult 
nsHTMLEditRules::NormalizeSelection(nsISelection *inSelection)
{
  NS_ENSURE_TRUE(inSelection, NS_ERROR_NULL_POINTER);

  // don't need to touch collapsed selections
  if (inSelection->Collapsed()) {
    return NS_OK;
  }

  PRInt32 rangeCount;
  nsresult res = inSelection->GetRangeCount(&rangeCount);
  NS_ENSURE_SUCCESS(res, res);
  
  // we don't need to mess with cell selections, and we assume multirange selections are those.
  if (rangeCount != 1) return NS_OK;
  
  nsCOMPtr<nsIDOMRange> range;
  res = inSelection->GetRangeAt(0, getter_AddRefs(range));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(range, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIDOMNode> startNode, endNode;
  PRInt32 startOffset, endOffset;
  nsCOMPtr<nsIDOMNode> newStartNode, newEndNode;
  PRInt32 newStartOffset, newEndOffset;
  
  res = range->GetStartContainer(getter_AddRefs(startNode));
  NS_ENSURE_SUCCESS(res, res);
  res = range->GetStartOffset(&startOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = range->GetEndContainer(getter_AddRefs(endNode));
  NS_ENSURE_SUCCESS(res, res);
  res = range->GetEndOffset(&endOffset);
  NS_ENSURE_SUCCESS(res, res);
  
  // adjusted values default to original values
  newStartNode = startNode; 
  newStartOffset = startOffset;
  newEndNode = endNode; 
  newEndOffset = endOffset;
  
  // some locals we need for whitespace code
  nsCOMPtr<nsIDOMNode> someNode;
  PRInt32 offset;
  PRInt16 wsType;

  // let the whitespace code do the heavy lifting
  nsWSRunObject wsEndObj(mHTMLEditor, endNode, endOffset);
  // is there any intervening visible whitespace?  if so we can't push selection past that,
  // it would visibly change maening of users selection
  wsEndObj.PriorVisibleNode(endNode, endOffset, address_of(someNode),
                            &offset, &wsType);
  if ((wsType != nsWSRunObject::eText) && (wsType != nsWSRunObject::eNormalWS))
  {
    // eThisBlock and eOtherBlock conveniently distinquish cases
    // of going "down" into a block and "up" out of a block.
    if (wsEndObj.mStartReason == nsWSRunObject::eOtherBlock) 
    {
      // endpoint is just after the close of a block.
      nsCOMPtr<nsIDOMNode> child = mHTMLEditor->GetRightmostChild(wsEndObj.mStartReasonNode, true);
      if (child)
      {
        newEndNode = nsEditor::GetNodeLocation(child, &newEndOffset);
        ++newEndOffset; // offset *after* child
      }
      // else block is empty - we can leave selection alone here, i think.
    }
    else if (wsEndObj.mStartReason == nsWSRunObject::eThisBlock)
    {
      // endpoint is just after start of this block
      nsCOMPtr<nsIDOMNode> child;
      res = mHTMLEditor->GetPriorHTMLNode(endNode, endOffset, address_of(child));
      if (child)
      {
        newEndNode = nsEditor::GetNodeLocation(child, &newEndOffset);
        ++newEndOffset; // offset *after* child
      }
      // else block is empty - we can leave selection alone here, i think.
    }
    else if (wsEndObj.mStartReason == nsWSRunObject::eBreak)
    {                                       
      // endpoint is just after break.  lets adjust it to before it.
      newEndNode = nsEditor::GetNodeLocation(wsEndObj.mStartReasonNode,
                                             &newEndOffset);
    }
  }
  
  
  // similar dealio for start of range
  nsWSRunObject wsStartObj(mHTMLEditor, startNode, startOffset);
  // is there any intervening visible whitespace?  if so we can't push selection past that,
  // it would visibly change maening of users selection
  wsStartObj.NextVisibleNode(startNode, startOffset, address_of(someNode),
                             &offset, &wsType);
  if ((wsType != nsWSRunObject::eText) && (wsType != nsWSRunObject::eNormalWS))
  {
    // eThisBlock and eOtherBlock conveniently distinquish cases
    // of going "down" into a block and "up" out of a block.
    if (wsStartObj.mEndReason == nsWSRunObject::eOtherBlock) 
    {
      // startpoint is just before the start of a block.
      nsCOMPtr<nsIDOMNode> child = mHTMLEditor->GetLeftmostChild(wsStartObj.mEndReasonNode, true);
      if (child)
      {
        newStartNode = nsEditor::GetNodeLocation(child, &newStartOffset);
      }
      // else block is empty - we can leave selection alone here, i think.
    }
    else if (wsStartObj.mEndReason == nsWSRunObject::eThisBlock)
    {
      // startpoint is just before end of this block
      nsCOMPtr<nsIDOMNode> child;
      res = mHTMLEditor->GetNextHTMLNode(startNode, startOffset, address_of(child));
      if (child)
      {
        newStartNode = nsEditor::GetNodeLocation(child, &newStartOffset);
      }
      // else block is empty - we can leave selection alone here, i think.
    }
    else if (wsStartObj.mEndReason == nsWSRunObject::eBreak)
    {                                       
      // startpoint is just before a break.  lets adjust it to after it.
      newStartNode = nsEditor::GetNodeLocation(wsStartObj.mEndReasonNode,
                                               &newStartOffset);
      ++newStartOffset; // offset *after* break
    }
  }
  
  // there is a demented possiblity we have to check for.  We might have a very strange selection
  // that is not collapsed and yet does not contain any editable content, and satisfies some of the
  // above conditions that cause tweaking.  In this case we don't want to tweak the selection into
  // a block it was never in, etc.  There are a variety of strategies one might use to try to
  // detect these cases, but I think the most straightforward is to see if the adjusted locations
  // "cross" the old values: ie, new end before old start, or new start after old end.  If so 
  // then just leave things alone.
  
  PRInt16 comp;
  comp = nsContentUtils::ComparePoints(startNode, startOffset,
                                       newEndNode, newEndOffset);
  if (comp == 1) return NS_OK;  // new end before old start
  comp = nsContentUtils::ComparePoints(newStartNode, newStartOffset,
                                       endNode, endOffset);
  if (comp == 1) return NS_OK;  // new start after old end
  
  // otherwise set selection to new values.  
  inSelection->Collapse(newStartNode, newStartOffset);
  inSelection->Extend(newEndNode, newEndOffset);
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// GetPromotedPoint: figure out where a start or end point for a block
//                   operation really is
void
nsHTMLEditRules::GetPromotedPoint(RulesEndpoint aWhere, nsIDOMNode* aNode,
                                  PRInt32 aOffset,
                                  nsEditor::OperationID actionID,
                                  nsCOMPtr<nsIDOMNode>* outNode,
                                  PRInt32* outOffset)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  MOZ_ASSERT(node && outNode && outOffset);

  // default values
  *outNode = node->AsDOMNode();
  *outOffset = aOffset;

  // we do one thing for text actions, something else entirely for other
  // actions
  if (actionID == nsEditor::kOpInsertText ||
      actionID == nsEditor::kOpInsertIMEText ||
      actionID == nsEditor::kOpInsertBreak ||
      actionID == nsEditor::kOpDeleteText) {
    bool isSpace, isNBSP;
    nsCOMPtr<nsIContent> content = do_QueryInterface(node), temp;
    // for text actions, we want to look backwards (or forwards, as
    // appropriate) for additional whitespace or nbsp's.  We may have to act on
    // these later even though they are outside of the initial selection.  Even
    // if they are in another node!
    while (content) {
      PRInt32 offset;
      if (aWhere == kStart) {
        mHTMLEditor->IsPrevCharInNodeWhitespace(content, *outOffset,
                                                &isSpace, &isNBSP,
                                                getter_AddRefs(temp), &offset);
      } else {
        mHTMLEditor->IsNextCharInNodeWhitespace(content, *outOffset,
                                                &isSpace, &isNBSP,
                                                getter_AddRefs(temp), &offset);
      }
      if (isSpace || isNBSP) {
        content = temp;
        *outOffset = offset;
      } else {
        break;
      }
    }

    *outNode = content->AsDOMNode();
    return;
  }

  PRInt32 offset = aOffset;

  // else not a text section.  In this case we want to see if we should grab
  // any adjacent inline nodes and/or parents and other ancestors
  if (aWhere == kStart) {
    // some special casing for text nodes
    if (node->IsNodeOfType(nsINode::eTEXT)) {
      if (!node->GetNodeParent()) {
        // Okay, can't promote any further
        return;
      }
      offset = node->GetNodeParent()->IndexOf(node);
      node = node->GetNodeParent();
    }

    // look back through any further inline nodes that aren't across a <br>
    // from us, and that are enclosed in the same block.
    nsCOMPtr<nsINode> priorNode =
      mHTMLEditor->GetPriorHTMLNode(node, offset, true);

    while (priorNode && priorNode->GetNodeParent() &&
           !mHTMLEditor->IsVisBreak(priorNode->AsDOMNode()) &&
           !IsBlockNode(priorNode->AsDOMNode())) {
      offset = priorNode->GetNodeParent()->IndexOf(priorNode);
      node = priorNode->GetNodeParent();
      priorNode = mHTMLEditor->GetPriorHTMLNode(node, offset, true);
    }

    // finding the real start for this point.  look up the tree for as long as
    // we are the first node in the container, and as long as we haven't hit
    // the body node.
    nsCOMPtr<nsIContent> nearNode =
      mHTMLEditor->GetPriorHTMLNode(node, offset, true);
    while (!nearNode && node->Tag() != nsGkAtoms::body &&
           node->GetNodeParent()) {
      // some cutoffs are here: we don't need to also include them in the
      // aWhere == kEnd case.  as long as they are in one or the other it will
      // work.  special case for outdent: don't keep looking up if we have
      // found a blockquote element to act on
      if (actionID == nsHTMLEditor::kOpOutdent &&
          node->Tag() == nsGkAtoms::blockquote) {
        break;
      }

      PRInt32 parentOffset = node->GetNodeParent()->IndexOf(node);
      nsCOMPtr<nsINode> parent = node->GetNodeParent();

      // Don't walk past the editable section. Note that we need to check
      // before walking up to a parent because we need to return the parent
      // object, so the parent itself might not be in the editable area, but
      // it's OK if we're not performing a block-level action.
      bool blockLevelAction = actionID == nsHTMLEditor::kOpIndent ||
                              actionID == nsHTMLEditor::kOpOutdent ||
                              actionID == nsHTMLEditor::kOpAlign ||
                              actionID == nsHTMLEditor::kOpMakeBasicBlock;
      if (!mHTMLEditor->IsDescendantOfEditorRoot(parent) &&
          (blockLevelAction || !mHTMLEditor->IsDescendantOfEditorRoot(node))) {
        break;
      }

      node = parent;
      offset = parentOffset;
      nearNode = mHTMLEditor->GetPriorHTMLNode(node, offset, true);
    }
    *outNode = node->AsDOMNode();
    *outOffset = offset;
    return;
  }

  // aWhere == kEnd
  // some special casing for text nodes
  if (node->IsNodeOfType(nsINode::eTEXT)) {
    if (!node->GetNodeParent()) {
      // Okay, can't promote any further
      return;
    }
    // want to be after the text node
    offset = 1 + node->GetNodeParent()->IndexOf(node);
    node = node->GetNodeParent();
  }

  // look ahead through any further inline nodes that aren't across a <br> from
  // us, and that are enclosed in the same block.
  nsCOMPtr<nsIContent> nextNode =
    mHTMLEditor->GetNextHTMLNode(node, offset, true);

  while (nextNode && !IsBlockNode(nextNode->AsDOMNode()) &&
         nextNode->GetNodeParent()) {
    offset = 1 + nextNode->GetNodeParent()->IndexOf(nextNode);
    node = nextNode->GetNodeParent();
    if (mHTMLEditor->IsVisBreak(nextNode->AsDOMNode())) {
      break;
    }
    nextNode = mHTMLEditor->GetNextHTMLNode(node, offset, true);
  }

  // finding the real end for this point.  look up the tree for as long as we
  // are the last node in the container, and as long as we haven't hit the body
  // node.
  nsCOMPtr<nsIContent> nearNode =
    mHTMLEditor->GetNextHTMLNode(node, offset, true);
  while (!nearNode && node->Tag() != nsGkAtoms::body &&
         node->GetNodeParent()) {
    PRInt32 parentOffset = node->GetNodeParent()->IndexOf(node);
    nsCOMPtr<nsINode> parent = node->GetNodeParent();

    // Don't walk past the editable section. Note that we need to check before
    // walking up to a parent because we need to return the parent object, so
    // the parent itself might not be in the editable area, but it's OK.
    if (!mHTMLEditor->IsDescendantOfEditorRoot(node) &&
        !mHTMLEditor->IsDescendantOfEditorRoot(parent)) {
      break;
    }

    node = parent;
    // we want to be AFTER nearNode
    offset = parentOffset + 1;
    nearNode = mHTMLEditor->GetNextHTMLNode(node, offset, true);
  }
  *outNode = node->AsDOMNode();
  *outOffset = offset;
}


///////////////////////////////////////////////////////////////////////////
// GetPromotedRanges: run all the selection range endpoint through 
//                    GetPromotedPoint()
//                       
nsresult 
nsHTMLEditRules::GetPromotedRanges(nsISelection *inSelection, 
                                   nsCOMArray<nsIDOMRange> &outArrayOfRanges, 
                                   nsEditor::OperationID inOperationType)
{
  NS_ENSURE_TRUE(inSelection, NS_ERROR_NULL_POINTER);

  PRInt32 rangeCount;
  nsresult res = inSelection->GetRangeCount(&rangeCount);
  NS_ENSURE_SUCCESS(res, res);
  
  PRInt32 i;
  nsCOMPtr<nsIDOMRange> selectionRange;
  nsCOMPtr<nsIDOMRange> opRange;

  for (i = 0; i < rangeCount; i++)
  {
    res = inSelection->GetRangeAt(i, getter_AddRefs(selectionRange));
    NS_ENSURE_SUCCESS(res, res);

    // clone range so we don't muck with actual selection ranges
    res = selectionRange->CloneRange(getter_AddRefs(opRange));
    NS_ENSURE_SUCCESS(res, res);

    // make a new adjusted range to represent the appropriate block content.
    // The basic idea is to push out the range endpoints
    // to truly enclose the blocks that we will affect.
    // This call alters opRange.
    res = PromoteRange(opRange, inOperationType);
    NS_ENSURE_SUCCESS(res, res);
      
    // stuff new opRange into array
    outArrayOfRanges.AppendObject(opRange);
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// PromoteRange: expand a range to include any parents for which all
//               editable children are already in range. 
//                       
nsresult 
nsHTMLEditRules::PromoteRange(nsIDOMRange *inRange, 
                              nsEditor::OperationID inOperationType)
{
  NS_ENSURE_TRUE(inRange, NS_ERROR_NULL_POINTER);
  nsresult res;
  nsCOMPtr<nsIDOMNode> startNode, endNode;
  PRInt32 startOffset, endOffset;
  
  res = inRange->GetStartContainer(getter_AddRefs(startNode));
  NS_ENSURE_SUCCESS(res, res);
  res = inRange->GetStartOffset(&startOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = inRange->GetEndContainer(getter_AddRefs(endNode));
  NS_ENSURE_SUCCESS(res, res);
  res = inRange->GetEndOffset(&endOffset);
  NS_ENSURE_SUCCESS(res, res);
  
  // MOOSE major hack:
  // GetPromotedPoint doesn't really do the right thing for collapsed ranges
  // inside block elements that contain nothing but a solo <br>.  It's easier
  // to put a workaround here than to revamp GetPromotedPoint.  :-(
  if ( (startNode == endNode) && (startOffset == endOffset))
  {
    nsCOMPtr<nsIDOMNode> block;
    if (IsBlockNode(startNode)) 
      block = startNode;
    else
      block = mHTMLEditor->GetBlockNodeParent(startNode);
    if (block)
    {
      bool bIsEmptyNode = false;
      // check for the editing host
      nsIContent *rootContent = mHTMLEditor->GetActiveEditingHost();
      nsCOMPtr<nsINode> rootNode = do_QueryInterface(rootContent);
      nsCOMPtr<nsINode> blockNode = do_QueryInterface(block);
      NS_ENSURE_TRUE(rootNode && blockNode, NS_ERROR_UNEXPECTED);
      // Make sure we don't go higher than our root element in the content tree
      if (!nsContentUtils::ContentIsDescendantOf(rootNode, blockNode))
      {
        res = mHTMLEditor->IsEmptyNode(block, &bIsEmptyNode, true, false);
      }
      if (bIsEmptyNode)
      {
        PRUint32 numChildren;
        nsEditor::GetLengthOfDOMNode(block, numChildren); 
        startNode = block;
        endNode = block;
        startOffset = 0;
        endOffset = numChildren;
      }
    }
  }

  // make a new adjusted range to represent the appropriate block content.
  // this is tricky.  the basic idea is to push out the range endpoints
  // to truly enclose the blocks that we will affect
  
  nsCOMPtr<nsIDOMNode> opStartNode;
  nsCOMPtr<nsIDOMNode> opEndNode;
  PRInt32 opStartOffset, opEndOffset;
  nsCOMPtr<nsIDOMRange> opRange;
  
  GetPromotedPoint(kStart, startNode, startOffset, inOperationType,
                   address_of(opStartNode), &opStartOffset);
  GetPromotedPoint(kEnd, endNode, endOffset, inOperationType,
                   address_of(opEndNode), &opEndOffset);

  // Make sure that the new range ends up to be in the editable section.
  if (!mHTMLEditor->IsDescendantOfEditorRoot(nsEditor::GetNodeAtRangeOffsetPoint(opStartNode, opStartOffset)) ||
      !mHTMLEditor->IsDescendantOfEditorRoot(nsEditor::GetNodeAtRangeOffsetPoint(opEndNode, opEndOffset - 1))) {
    return NS_OK;
  }

  res = inRange->SetStart(opStartNode, opStartOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = inRange->SetEnd(opEndNode, opEndOffset);
  return res;
} 

class nsUniqueFunctor : public nsBoolDomIterFunctor
{
public:
  nsUniqueFunctor(nsCOMArray<nsIDOMNode> &aArray) : mArray(aArray)
  {
  }
  virtual bool operator()(nsIDOMNode* aNode)  // used to build list of all nodes iterator covers
  {
    return mArray.IndexOf(aNode) < 0;
  }

private:
  nsCOMArray<nsIDOMNode> &mArray;
};

///////////////////////////////////////////////////////////////////////////
// GetNodesForOperation: run through the ranges in the array and construct 
//                       a new array of nodes to be acted on.
//                       
nsresult 
nsHTMLEditRules::GetNodesForOperation(nsCOMArray<nsIDOMRange>& inArrayOfRanges, 
                                      nsCOMArray<nsIDOMNode>& outArrayOfNodes, 
                                      nsEditor::OperationID inOperationType,
                                      bool aDontTouchContent)
{
  PRInt32 rangeCount = inArrayOfRanges.Count();
  
  PRInt32 i;
  nsCOMPtr<nsIDOMRange> opRange;

  nsresult res = NS_OK;
  
  // bust up any inlines that cross our range endpoints,
  // but only if we are allowed to touch content.
  
  if (!aDontTouchContent)
  {
    nsTArray<nsRefPtr<nsRangeStore> > rangeItemArray;
    if (!rangeItemArray.AppendElements(rangeCount)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ASSERTION(static_cast<PRUint32>(rangeCount) == rangeItemArray.Length(),
                 "How did that happen?");

    // first register ranges for special editor gravity
    for (i = 0; i < rangeCount; i++)
    {
      opRange = inArrayOfRanges[0];
      rangeItemArray[i] = new nsRangeStore();
      rangeItemArray[i]->StoreRange(opRange);
      mHTMLEditor->mRangeUpdater.RegisterRangeItem(rangeItemArray[i]);
      inArrayOfRanges.RemoveObjectAt(0);
    }    
    // now bust up inlines.  Safe to start at rangeCount-1, since we
    // asserted we have enough items above.
    for (i = rangeCount-1; i >= 0 && NS_SUCCEEDED(res); i--)
    {
      res = BustUpInlinesAtRangeEndpoints(*rangeItemArray[i]);
    } 
    // then unregister the ranges
    for (i = 0; i < rangeCount; i++)
    {
      nsRangeStore* item = rangeItemArray[i];
      mHTMLEditor->mRangeUpdater.DropRangeItem(item);
      nsRefPtr<nsRange> range;
      nsresult res2 = item->GetRange(getter_AddRefs(range));
      opRange = range;
      if (NS_FAILED(res2) && NS_SUCCEEDED(res)) {
        // Remember the failure, but keep going so we make sure to unregister
        // all our range items.
        res = res2;
      }
      inArrayOfRanges.AppendObject(opRange);
    }
    NS_ENSURE_SUCCESS(res, res);
  }
  // gather up a list of all the nodes
  for (i = 0; i < rangeCount; i++)
  {
    opRange = inArrayOfRanges[i];
    
    nsDOMSubtreeIterator iter;
    res = iter.Init(opRange);
    NS_ENSURE_SUCCESS(res, res);
    if (outArrayOfNodes.Count() == 0) {
      nsTrivialFunctor functor;
      res = iter.AppendList(functor, outArrayOfNodes);
      NS_ENSURE_SUCCESS(res, res);    
    }
    else {
      // We don't want duplicates in outArrayOfNodes, so we use an
      // iterator/functor that only return nodes that are not already in
      // outArrayOfNodes.
      nsCOMArray<nsIDOMNode> nodes;
      nsUniqueFunctor functor(outArrayOfNodes);
      res = iter.AppendList(functor, nodes);
      NS_ENSURE_SUCCESS(res, res);
      if (!outArrayOfNodes.AppendObjects(nodes))
        return NS_ERROR_OUT_OF_MEMORY;
    }
  }    

  // certain operations should not act on li's and td's, but rather inside 
  // them.  alter the list as needed
  if (inOperationType == nsEditor::kOpMakeBasicBlock) {
    PRInt32 listCount = outArrayOfNodes.Count();
    for (i=listCount-1; i>=0; i--)
    {
      nsCOMPtr<nsIDOMNode> node = outArrayOfNodes[i];
      if (nsHTMLEditUtils::IsListItem(node))
      {
        PRInt32 j=i;
        outArrayOfNodes.RemoveObjectAt(i);
        res = GetInnerContent(node, outArrayOfNodes, &j);
        NS_ENSURE_SUCCESS(res, res);
      }
    }
  }
  // indent/outdent already do something special for list items, but
  // we still need to make sure we don't act on table elements
  else if (inOperationType == nsEditor::kOpOutdent ||
           inOperationType == nsEditor::kOpIndent ||
           inOperationType == nsEditor::kOpSetAbsolutePosition) {
    PRInt32 listCount = outArrayOfNodes.Count();
    for (i=listCount-1; i>=0; i--)
    {
      nsCOMPtr<nsIDOMNode> node = outArrayOfNodes[i];
      if (nsHTMLEditUtils::IsTableElementButNotTable(node))
      {
        PRInt32 j=i;
        outArrayOfNodes.RemoveObjectAt(i);
        res = GetInnerContent(node, outArrayOfNodes, &j);
        NS_ENSURE_SUCCESS(res, res);
      }
    }
  }
  // outdent should look inside of divs.
  if (inOperationType == nsEditor::kOpOutdent &&
      !mHTMLEditor->IsCSSEnabled()) {
    PRInt32 listCount = outArrayOfNodes.Count();
    for (i=listCount-1; i>=0; i--)
    {
      nsCOMPtr<nsIDOMNode> node = outArrayOfNodes[i];
      if (nsHTMLEditUtils::IsDiv(node))
      {
        PRInt32 j=i;
        outArrayOfNodes.RemoveObjectAt(i);
        res = GetInnerContent(node, outArrayOfNodes, &j, false, false);
        NS_ENSURE_SUCCESS(res, res);
      }
    }
  }


  // post process the list to break up inline containers that contain br's.
  // but only for operations that might care, like making lists or para's...
  if (inOperationType == nsEditor::kOpMakeBasicBlock ||
      inOperationType == nsEditor::kOpMakeList ||
      inOperationType == nsEditor::kOpAlign ||
      inOperationType == nsEditor::kOpSetAbsolutePosition ||
      inOperationType == nsEditor::kOpIndent ||
      inOperationType == nsEditor::kOpOutdent) {
    PRInt32 listCount = outArrayOfNodes.Count();
    for (i=listCount-1; i>=0; i--)
    {
      nsCOMPtr<nsIDOMNode> node = outArrayOfNodes[i];
      if (!aDontTouchContent && IsInlineNode(node) 
           && mHTMLEditor->IsContainer(node) && !mHTMLEditor->IsTextNode(node))
      {
        nsCOMArray<nsIDOMNode> arrayOfInlines;
        res = BustUpInlinesAtBRs(node, arrayOfInlines);
        NS_ENSURE_SUCCESS(res, res);
        // put these nodes in outArrayOfNodes, replacing the current node
        outArrayOfNodes.RemoveObjectAt(i);
        outArrayOfNodes.InsertObjectsAt(arrayOfInlines, i);
      }
    }
  }
  return res;
}



///////////////////////////////////////////////////////////////////////////
// GetChildNodesForOperation: 
//                       
nsresult 
nsHTMLEditRules::GetChildNodesForOperation(nsIDOMNode *inNode, 
                                           nsCOMArray<nsIDOMNode>& outArrayOfNodes)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(inNode);
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);

  for (nsIContent* child = node->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    nsIDOMNode* childNode = child->AsDOMNode();
    if (!outArrayOfNodes.AppendObject(childNode)) {
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}



///////////////////////////////////////////////////////////////////////////
// GetListActionNodes: 
//                       
nsresult 
nsHTMLEditRules::GetListActionNodes(nsCOMArray<nsIDOMNode> &outArrayOfNodes, 
                                    bool aEntireList,
                                    bool aDontTouchContent)
{
  nsresult res = NS_OK;
  
  nsCOMPtr<nsISelection>selection;
  res = mHTMLEditor->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));
  NS_ENSURE_TRUE(selPriv, NS_ERROR_FAILURE);
  // added this in so that ui code can ask to change an entire list, even if selection
  // is only in part of it.  used by list item dialog.
  if (aEntireList)
  {       
    nsCOMPtr<nsIEnumerator> enumerator;
    res = selPriv->GetEnumerator(getter_AddRefs(enumerator));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(enumerator, NS_ERROR_UNEXPECTED);

    for (enumerator->First(); NS_OK!=enumerator->IsDone(); enumerator->Next())
    {
      nsCOMPtr<nsISupports> currentItem;
      res = enumerator->CurrentItem(getter_AddRefs(currentItem));
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_TRUE(currentItem, NS_ERROR_UNEXPECTED);

      nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
      nsCOMPtr<nsIDOMNode> commonParent, parent, tmp;
      range->GetCommonAncestorContainer(getter_AddRefs(commonParent));
      if (commonParent)
      {
        parent = commonParent;
        while (parent)
        {
          if (nsHTMLEditUtils::IsList(parent))
          {
            outArrayOfNodes.AppendObject(parent);
            break;
          }
          parent->GetParentNode(getter_AddRefs(tmp));
          parent = tmp;
        }
      }
    }
    // if we didn't find any nodes this way, then try the normal way.  perhaps the
    // selection spans multiple lists but with no common list parent.
    if (outArrayOfNodes.Count()) return NS_OK;
  }

  {
    // We don't like other people messing with our selection!
    nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);

    // contruct a list of nodes to act on.
    res = GetNodesFromSelection(selection, nsEditor::kOpMakeList,
                                outArrayOfNodes, aDontTouchContent);
    NS_ENSURE_SUCCESS(res, res);
  }
               
  // pre process our list of nodes...                      
  PRInt32 listCount = outArrayOfNodes.Count();
  PRInt32 i;
  for (i=listCount-1; i>=0; i--)
  {
    nsCOMPtr<nsIDOMNode> testNode = outArrayOfNodes[i];

    // Remove all non-editable nodes.  Leave them be.
    if (!mHTMLEditor->IsEditable(testNode))
    {
      outArrayOfNodes.RemoveObjectAt(i);
    }
    
    // scan for table elements and divs.  If we find table elements other than table,
    // replace it with a list of any editable non-table content.
    if (nsHTMLEditUtils::IsTableElementButNotTable(testNode))
    {
      PRInt32 j=i;
      outArrayOfNodes.RemoveObjectAt(i);
      res = GetInnerContent(testNode, outArrayOfNodes, &j, false);
      NS_ENSURE_SUCCESS(res, res);
    }
  }

  // if there is only one node in the array, and it is a list, div, or blockquote,
  // then look inside of it until we find inner list or content.
  res = LookInsideDivBQandList(outArrayOfNodes);
  return res;
}


///////////////////////////////////////////////////////////////////////////
// LookInsideDivBQandList: 
//                       
nsresult 
nsHTMLEditRules::LookInsideDivBQandList(nsCOMArray<nsIDOMNode>& aNodeArray)
{
  // if there is only one node in the array, and it is a list, div, or blockquote,
  // then look inside of it until we find inner list or content.
  PRInt32 listCount = aNodeArray.Count();
  if (listCount != 1) {
    return NS_OK;
  }

  nsCOMPtr<nsINode> curNode = do_QueryInterface(aNodeArray[0]);
  NS_ENSURE_STATE(curNode);

  while (curNode->IsElement() &&
         (curNode->AsElement()->IsHTML(nsGkAtoms::div) ||
          nsHTMLEditUtils::IsList(curNode->AsElement()) ||
          curNode->AsElement()->IsHTML(nsGkAtoms::blockquote))) {
    // dive as long as there is only one child, and it is a list, div, blockquote
    PRUint32 numChildren = mHTMLEditor->CountEditableChildren(curNode);
    if (numChildren != 1) {
      break;
    }

    // keep diving
    // XXX One would expect to dive into the one editable node.
    nsIContent* tmp = curNode->GetFirstChild();
    if (!tmp->IsElement()) {
      break;
    }

    dom::Element* element = tmp->AsElement();
    if (!element->IsHTML(nsGkAtoms::div) &&
        !nsHTMLEditUtils::IsList(element) &&
        !element->IsHTML(nsGkAtoms::blockquote)) {
      break;
    }

    // check editablility XXX floppy moose
    curNode = tmp;
  }

  // we've found innermost list/blockquote/div: 
  // replace the one node in the array with these nodes
  aNodeArray.RemoveObjectAt(0);
  if (curNode->IsElement() &&
      (curNode->AsElement()->IsHTML(nsGkAtoms::div) ||
       curNode->AsElement()->IsHTML(nsGkAtoms::blockquote))) {
    PRInt32 j = 0;
    return GetInnerContent(curNode->AsDOMNode(), aNodeArray, &j, false, false);
  }

  aNodeArray.AppendObject(curNode->AsDOMNode());
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// GetDefinitionListItemTypes: 
//                       
void
nsHTMLEditRules::GetDefinitionListItemTypes(dom::Element* aElement, bool* aDT, bool* aDD)
{
  MOZ_ASSERT(aElement);
  MOZ_ASSERT(aElement->IsHTML(nsGkAtoms::dl));
  MOZ_ASSERT(aDT);
  MOZ_ASSERT(aDD);

  *aDT = *aDD = false;
  for (nsIContent* child = aElement->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (child->IsHTML(nsGkAtoms::dt)) {
      *aDT = true;
    } else if (child->IsHTML(nsGkAtoms::dd)) {
      *aDD = true;
    }
  }
}

///////////////////////////////////////////////////////////////////////////
// GetParagraphFormatNodes: 
//                       
nsresult 
nsHTMLEditRules::GetParagraphFormatNodes(nsCOMArray<nsIDOMNode>& outArrayOfNodes,
                                         bool aDontTouchContent)
{  
  nsCOMPtr<nsISelection>selection;
  nsresult res = mHTMLEditor->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);

  // contruct a list of nodes to act on.
  res = GetNodesFromSelection(selection, nsEditor::kOpMakeBasicBlock,
                              outArrayOfNodes, aDontTouchContent);
  NS_ENSURE_SUCCESS(res, res);

  // pre process our list of nodes...                      
  PRInt32 listCount = outArrayOfNodes.Count();
  PRInt32 i;
  for (i=listCount-1; i>=0; i--)
  {
    nsCOMPtr<nsIDOMNode> testNode = outArrayOfNodes[i];

    // Remove all non-editable nodes.  Leave them be.
    if (!mHTMLEditor->IsEditable(testNode))
    {
      outArrayOfNodes.RemoveObjectAt(i);
    }
    
    // scan for table elements.  If we find table elements other than table,
    // replace it with a list of any editable non-table content.  Ditto for list elements.
    if (nsHTMLEditUtils::IsTableElement(testNode) ||
        nsHTMLEditUtils::IsList(testNode) || 
        nsHTMLEditUtils::IsListItem(testNode) )
    {
      PRInt32 j=i;
      outArrayOfNodes.RemoveObjectAt(i);
      res = GetInnerContent(testNode, outArrayOfNodes, &j);
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// BustUpInlinesAtRangeEndpoints: 
//                       
nsresult 
nsHTMLEditRules::BustUpInlinesAtRangeEndpoints(nsRangeStore &item)
{
  nsresult res = NS_OK;
  bool isCollapsed = ((item.startNode == item.endNode) && (item.startOffset == item.endOffset));

  nsCOMPtr<nsIDOMNode> endInline = GetHighestInlineParent(item.endNode);
  
  // if we have inline parents above range endpoints, split them
  if (endInline && !isCollapsed)
  {
    nsCOMPtr<nsIDOMNode> resultEndNode;
    PRInt32 resultEndOffset;
    endInline->GetParentNode(getter_AddRefs(resultEndNode));
    res = mHTMLEditor->SplitNodeDeep(endInline, item.endNode, item.endOffset,
                          &resultEndOffset, true);
    NS_ENSURE_SUCCESS(res, res);
    // reset range
    item.endNode = resultEndNode; item.endOffset = resultEndOffset;
  }

  nsCOMPtr<nsIDOMNode> startInline = GetHighestInlineParent(item.startNode);

  if (startInline)
  {
    nsCOMPtr<nsIDOMNode> resultStartNode;
    PRInt32 resultStartOffset;
    startInline->GetParentNode(getter_AddRefs(resultStartNode));
    res = mHTMLEditor->SplitNodeDeep(startInline, item.startNode, item.startOffset,
                          &resultStartOffset, true);
    NS_ENSURE_SUCCESS(res, res);
    // reset range
    item.startNode = resultStartNode; item.startOffset = resultStartOffset;
  }
  
  return res;
}



///////////////////////////////////////////////////////////////////////////
// BustUpInlinesAtBRs: 
//                       
nsresult 
nsHTMLEditRules::BustUpInlinesAtBRs(nsIDOMNode *inNode, 
                                    nsCOMArray<nsIDOMNode>& outArrayOfNodes)
{
  NS_ENSURE_TRUE(inNode, NS_ERROR_NULL_POINTER);

  // first step is to build up a list of all the break nodes inside 
  // the inline container.
  nsCOMArray<nsIDOMNode> arrayOfBreaks;
  nsBRNodeFunctor functor;
  nsDOMIterator iter;
  nsresult res = iter.Init(inNode);
  NS_ENSURE_SUCCESS(res, res);
  res = iter.AppendList(functor, arrayOfBreaks);
  NS_ENSURE_SUCCESS(res, res);
  
  // if there aren't any breaks, just put inNode itself in the array
  PRInt32 listCount = arrayOfBreaks.Count();
  if (!listCount)
  {
    if (!outArrayOfNodes.AppendObject(inNode))
      return NS_ERROR_FAILURE;
  }
  else
  {
    // else we need to bust up inNode along all the breaks
    nsCOMPtr<nsIDOMNode> breakNode;
    nsCOMPtr<nsIDOMNode> inlineParentNode;
    nsCOMPtr<nsIDOMNode> leftNode;
    nsCOMPtr<nsIDOMNode> rightNode;
    nsCOMPtr<nsIDOMNode> splitDeepNode = inNode;
    nsCOMPtr<nsIDOMNode> splitParentNode;
    PRInt32 splitOffset, resultOffset, i;
    inNode->GetParentNode(getter_AddRefs(inlineParentNode));
    
    for (i=0; i< listCount; i++)
    {
      breakNode = arrayOfBreaks[i];
      NS_ENSURE_TRUE(breakNode, NS_ERROR_NULL_POINTER);
      NS_ENSURE_TRUE(splitDeepNode, NS_ERROR_NULL_POINTER);
      splitParentNode = nsEditor::GetNodeLocation(breakNode, &splitOffset);
      res = mHTMLEditor->SplitNodeDeep(splitDeepNode, splitParentNode, splitOffset,
                          &resultOffset, false, address_of(leftNode), address_of(rightNode));
      NS_ENSURE_SUCCESS(res, res);
      // put left node in node list
      if (leftNode)
      {
        // might not be a left node.  a break might have been at the very
        // beginning of inline container, in which case splitnodedeep
        // would not actually split anything
        if (!outArrayOfNodes.AppendObject(leftNode))
          return NS_ERROR_FAILURE;
      }
      // move break outside of container and also put in node list
      res = mHTMLEditor->MoveNode(breakNode, inlineParentNode, resultOffset);
      NS_ENSURE_SUCCESS(res, res);
      if (!outArrayOfNodes.AppendObject(breakNode))
        return  NS_ERROR_FAILURE;
      // now rightNode becomes the new node to split
      splitDeepNode = rightNode;
    }
    // now tack on remaining rightNode, if any, to the list
    if (rightNode)
    {
      if (!outArrayOfNodes.AppendObject(rightNode))
        return NS_ERROR_FAILURE;
    }
  }
  return res;
}


nsCOMPtr<nsIDOMNode> 
nsHTMLEditRules::GetHighestInlineParent(nsIDOMNode* aNode)
{
  NS_ENSURE_TRUE(aNode, nsnull);
  if (IsBlockNode(aNode)) return nsnull;
  nsCOMPtr<nsIDOMNode> inlineNode, node=aNode;

  while (node && IsInlineNode(node))
  {
    inlineNode = node;
    inlineNode->GetParentNode(getter_AddRefs(node));
  }
  return inlineNode;
}


///////////////////////////////////////////////////////////////////////////
// GetNodesFromPoint: given a particular operation, construct a list  
//                     of nodes from a point that will be operated on. 
//                       
nsresult 
nsHTMLEditRules::GetNodesFromPoint(DOMPoint point,
                                   nsEditor::OperationID operation,
                                   nsCOMArray<nsIDOMNode> &arrayOfNodes,
                                   bool dontTouchContent)
{
  nsresult res;

  // get our point
  nsCOMPtr<nsIDOMNode> node;
  PRInt32 offset;
  point.GetPoint(node, offset);
  
  // use it to make a range
  nsRefPtr<nsRange> range = new nsRange();
  res = range->SetStart(node, offset);
  NS_ENSURE_SUCCESS(res, res);
  /* SetStart() will also set the end for this new range
  res = range->SetEnd(node, offset);
  NS_ENSURE_SUCCESS(res, res); */
  
  // expand the range to include adjacent inlines
  res = PromoteRange(range, operation);
  NS_ENSURE_SUCCESS(res, res);
      
  // make array of ranges
  nsCOMArray<nsIDOMRange> arrayOfRanges;
  
  // stuff new opRange into array
  arrayOfRanges.AppendObject(range);
  
  // use these ranges to contruct a list of nodes to act on.
  res = GetNodesForOperation(arrayOfRanges, arrayOfNodes, operation, dontTouchContent); 
  return res;
}


///////////////////////////////////////////////////////////////////////////
// GetNodesFromSelection: given a particular operation, construct a list  
//                     of nodes from the selection that will be operated on. 
//                       
nsresult 
nsHTMLEditRules::GetNodesFromSelection(nsISelection *selection,
                                       nsEditor::OperationID operation,
                                       nsCOMArray<nsIDOMNode>& arrayOfNodes,
                                       bool dontTouchContent)
{
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  nsresult res;
  
  // promote selection ranges
  nsCOMArray<nsIDOMRange> arrayOfRanges;
  res = GetPromotedRanges(selection, arrayOfRanges, operation);
  NS_ENSURE_SUCCESS(res, res);
  
  // use these ranges to contruct a list of nodes to act on.
  res = GetNodesForOperation(arrayOfRanges, arrayOfNodes, operation, dontTouchContent); 
  return res;
}


///////////////////////////////////////////////////////////////////////////
// MakeTransitionList: detect all the transitions in the array, where a 
//                     transition means that adjacent nodes in the array 
//                     don't have the same parent.
//                       
nsresult 
nsHTMLEditRules::MakeTransitionList(nsCOMArray<nsIDOMNode>& inArrayOfNodes, 
                                    nsTArray<bool> &inTransitionArray)
{
  PRUint32 listCount = inArrayOfNodes.Count();
  inTransitionArray.EnsureLengthAtLeast(listCount);
  PRUint32 i;
  nsCOMPtr<nsIDOMNode> prevElementParent;
  nsCOMPtr<nsIDOMNode> curElementParent;
  
  for (i=0; i<listCount; i++)
  {
    nsIDOMNode* transNode = inArrayOfNodes[i];
    transNode->GetParentNode(getter_AddRefs(curElementParent));
    if (curElementParent != prevElementParent)
    {
      // different parents, or separated by <br>: transition point
      inTransitionArray[i] = true;
    }
    else
    {
      // same parents: these nodes grew up together
      inTransitionArray[i] = false;
    }
    prevElementParent = curElementParent;
  }
  return NS_OK;
}



/********************************************************
 *  main implementation methods 
 ********************************************************/
 
///////////////////////////////////////////////////////////////////////////
// IsInListItem: if aNode is the descendant of a listitem, return that li.
//               But table element boundaries are stoppers on the search.
//               Also stops on the active editor host (contenteditable).
//               Also test if aNode is an li itself.
//                       
already_AddRefed<nsIDOMNode>
nsHTMLEditRules::IsInListItem(nsIDOMNode* aNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  nsCOMPtr<nsIDOMNode> retval = do_QueryInterface(IsInListItem(node));
  return retval.forget();
}

nsINode*
nsHTMLEditRules::IsInListItem(nsINode* aNode)
{
  NS_ENSURE_TRUE(aNode, nsnull);
  if (aNode->IsElement() && nsHTMLEditUtils::IsListItem(aNode->AsElement())) {
    return aNode;
  }

  nsINode* parent = aNode->GetNodeParent();
  while (parent && mHTMLEditor->IsDescendantOfEditorRoot(parent) &&
         !(parent->IsElement() &&
           nsHTMLEditUtils::IsTableElement(parent->AsElement()))) {
    if (nsHTMLEditUtils::IsListItem(parent->AsElement())) {
      return parent;
    }
    parent = parent->GetNodeParent();
  }
  return nsnull;
}


///////////////////////////////////////////////////////////////////////////
// ReturnInHeader: do the right thing for returns pressed in headers
//                       
nsresult 
nsHTMLEditRules::ReturnInHeader(nsISelection *aSelection, 
                                nsIDOMNode *aHeader, 
                                nsIDOMNode *aNode, 
                                PRInt32 aOffset)
{
  NS_ENSURE_TRUE(aSelection && aHeader && aNode, NS_ERROR_NULL_POINTER);  
  
  // remeber where the header is
  PRInt32 offset;
  nsCOMPtr<nsIDOMNode> headerParent = nsEditor::GetNodeLocation(aHeader, &offset);

  // get ws code to adjust any ws
  nsCOMPtr<nsIDOMNode> selNode = aNode;
  nsresult res = nsWSRunObject::PrepareToSplitAcrossBlocks(mHTMLEditor,
                                                           address_of(selNode),
                                                           &aOffset);
  NS_ENSURE_SUCCESS(res, res);

  // split the header
  PRInt32 newOffset;
  res = mHTMLEditor->SplitNodeDeep( aHeader, selNode, aOffset, &newOffset);
  NS_ENSURE_SUCCESS(res, res);

  // if the leftand heading is empty, put a mozbr in it
  nsCOMPtr<nsIDOMNode> prevItem;
  mHTMLEditor->GetPriorHTMLSibling(aHeader, address_of(prevItem));
  if (prevItem && nsHTMLEditUtils::IsHeader(prevItem))
  {
    bool bIsEmptyNode;
    res = mHTMLEditor->IsEmptyNode(prevItem, &bIsEmptyNode);
    NS_ENSURE_SUCCESS(res, res);
    if (bIsEmptyNode) {
      res = CreateMozBR(prevItem, 0);
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  
  // if the new (righthand) header node is empty, delete it
  bool isEmpty;
  res = IsEmptyBlock(aHeader, &isEmpty, true);
  NS_ENSURE_SUCCESS(res, res);
  if (isEmpty)
  {
    res = mHTMLEditor->DeleteNode(aHeader);
    NS_ENSURE_SUCCESS(res, res);
    // layout tells the caret to blink in a weird place
    // if we don't place a break after the header.
    nsCOMPtr<nsIDOMNode> sibling;
    res = mHTMLEditor->GetNextHTMLSibling(headerParent, offset+1, address_of(sibling));
    NS_ENSURE_SUCCESS(res, res);
    if (!sibling || !nsTextEditUtils::IsBreak(sibling))
    {
      // create a paragraph
      NS_NAMED_LITERAL_STRING(pType, "p");
      nsCOMPtr<nsIDOMNode> pNode;
      res = mHTMLEditor->CreateNode(pType, headerParent, offset+1, getter_AddRefs(pNode));
      NS_ENSURE_SUCCESS(res, res);

      // append a <br> to it
      nsCOMPtr<nsIDOMNode> brNode;
      res = mHTMLEditor->CreateBR(pNode, 0, address_of(brNode));
      NS_ENSURE_SUCCESS(res, res);

      // set selection to before the break
      res = aSelection->Collapse(pNode, 0);
    }
    else
    {
      headerParent = nsEditor::GetNodeLocation(sibling, &offset);
      // put selection after break
      res = aSelection->Collapse(headerParent,offset+1);
    }
  }
  else
  {
    // put selection at front of righthand heading
    res = aSelection->Collapse(aHeader,0);
  }
  return res;
}

///////////////////////////////////////////////////////////////////////////
// ReturnInParagraph: do the right thing for returns pressed in paragraphs
//
nsresult
nsHTMLEditRules::ReturnInParagraph(nsISelection* aSelection,
                                   nsIDOMNode* aPara,
                                   nsIDOMNode* aNode,
                                   PRInt32 aOffset,
                                   bool* aCancel,
                                   bool* aHandled)
{
  if (!aSelection || !aPara || !aNode || !aCancel || !aHandled) {
    return NS_ERROR_NULL_POINTER;
  }
  *aCancel = false;
  *aHandled = false;
  nsresult res;

  PRInt32 offset;
  nsCOMPtr<nsIDOMNode> parent = nsEditor::GetNodeLocation(aNode, &offset);

  bool doesCRCreateNewP = mHTMLEditor->GetReturnInParagraphCreatesNewParagraph();

  bool newBRneeded = false;
  nsCOMPtr<nsIDOMNode> sibling;

  if (aNode == aPara && doesCRCreateNewP) {
    // we are at the edges of the block, newBRneeded not needed!
    sibling = aNode;
  } else if (mHTMLEditor->IsTextNode(aNode)) {
    nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(aNode);
    PRUint32 strLength;
    res = textNode->GetLength(&strLength);
    NS_ENSURE_SUCCESS(res, res);

    // at beginning of text node?
    if (!aOffset) {
      // is there a BR prior to it?
      mHTMLEditor->GetPriorHTMLSibling(aNode, address_of(sibling));
      if (!sibling || !mHTMLEditor->IsVisBreak(sibling) ||
          nsTextEditUtils::HasMozAttr(sibling)) {
        newBRneeded = true;
      }
    } else if (aOffset == (PRInt32)strLength) {
      // we're at the end of text node...
      // is there a BR after to it?
      res = mHTMLEditor->GetNextHTMLSibling(aNode, address_of(sibling));
      if (!sibling || !mHTMLEditor->IsVisBreak(sibling) ||
          nsTextEditUtils::HasMozAttr(sibling)) {
        newBRneeded = true;
        offset++;
      }
    } else {
      if (doesCRCreateNewP) {
        nsCOMPtr<nsIDOMNode> tmp;
        res = mEditor->SplitNode(aNode, aOffset, getter_AddRefs(tmp));
        NS_ENSURE_SUCCESS(res, res);
        aNode = tmp;
      }

      newBRneeded = true;
      offset++;
    }
  } else {
    // not in a text node.
    // is there a BR prior to it?
    nsCOMPtr<nsIDOMNode> nearNode, selNode = aNode;
    res = mHTMLEditor->GetPriorHTMLNode(aNode, aOffset, address_of(nearNode));
    NS_ENSURE_SUCCESS(res, res);
    if (!nearNode || !mHTMLEditor->IsVisBreak(nearNode) ||
        nsTextEditUtils::HasMozAttr(nearNode)) {
      // is there a BR after it?
      res = mHTMLEditor->GetNextHTMLNode(aNode, aOffset, address_of(nearNode));
      NS_ENSURE_SUCCESS(res, res);
      if (!nearNode || !mHTMLEditor->IsVisBreak(nearNode) ||
          nsTextEditUtils::HasMozAttr(nearNode)) {
        newBRneeded = true;
      }
    }
    if (!newBRneeded) {
      sibling = nearNode;
    }
  }
  if (newBRneeded) {
    // if CR does not create a new P, default to BR creation
    NS_ENSURE_TRUE(doesCRCreateNewP, NS_OK);

    nsCOMPtr<nsIDOMNode> brNode;
    res =  mHTMLEditor->CreateBR(parent, offset, address_of(brNode));
    sibling = brNode;
  }
  nsCOMPtr<nsIDOMNode> selNode = aNode;
  *aHandled = true;
  return SplitParagraph(aPara, sibling, aSelection, address_of(selNode), &aOffset);
}

///////////////////////////////////////////////////////////////////////////
// SplitParagraph: split a paragraph at selection point, possibly deleting a br
//                       
nsresult 
nsHTMLEditRules::SplitParagraph(nsIDOMNode *aPara,
                                nsIDOMNode *aBRNode, 
                                nsISelection *aSelection,
                                nsCOMPtr<nsIDOMNode> *aSelNode, 
                                PRInt32 *aOffset)
{
  NS_ENSURE_TRUE(aPara && aBRNode && aSelNode && *aSelNode && aOffset && aSelection, NS_ERROR_NULL_POINTER);
  nsresult res = NS_OK;
  
  // split para
  PRInt32 newOffset;
  // get ws code to adjust any ws
  nsCOMPtr<nsIDOMNode> leftPara, rightPara;
  res = nsWSRunObject::PrepareToSplitAcrossBlocks(mHTMLEditor, aSelNode, aOffset);
  NS_ENSURE_SUCCESS(res, res);
  // split the paragraph
  res = mHTMLEditor->SplitNodeDeep(aPara, *aSelNode, *aOffset, &newOffset, false,
                                   address_of(leftPara), address_of(rightPara));
  NS_ENSURE_SUCCESS(res, res);
  // get rid of the break, if it is visible (otherwise it may be needed to prevent an empty p)
  if (mHTMLEditor->IsVisBreak(aBRNode))
  {
    res = mHTMLEditor->DeleteNode(aBRNode);  
    NS_ENSURE_SUCCESS(res, res);
  }

  // remove ID attribute on the paragraph we just created
  nsCOMPtr<nsIDOMElement> rightElt = do_QueryInterface(rightPara);
  res = mHTMLEditor->RemoveAttribute(rightElt, NS_LITERAL_STRING("id"));
  NS_ENSURE_SUCCESS(res, res);

  // check both halves of para to see if we need mozBR
  res = InsertMozBRIfNeeded(leftPara);
  NS_ENSURE_SUCCESS(res, res);
  res = InsertMozBRIfNeeded(rightPara);
  NS_ENSURE_SUCCESS(res, res);

  // selection to beginning of right hand para;
  // look inside any containers that are up front.
  nsCOMPtr<nsIDOMNode> child = mHTMLEditor->GetLeftmostChild(rightPara, true);
  if (mHTMLEditor->IsTextNode(child) || mHTMLEditor->IsContainer(child))
  {
    aSelection->Collapse(child,0);
  }
  else
  {
    PRInt32 offset;
    nsCOMPtr<nsIDOMNode> parent = nsEditor::GetNodeLocation(child, &offset);
    aSelection->Collapse(parent,offset);
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// ReturnInListItem: do the right thing for returns pressed in list items
//                       
nsresult 
nsHTMLEditRules::ReturnInListItem(nsISelection *aSelection, 
                                  nsIDOMNode *aListItem, 
                                  nsIDOMNode *aNode, 
                                  PRInt32 aOffset)
{
  NS_ENSURE_TRUE(aSelection && aListItem && aNode, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsISelection> selection(aSelection);
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));
  nsresult res = NS_OK;
  
  nsCOMPtr<nsIDOMNode> listitem;
  
  // sanity check
  NS_PRECONDITION(true == nsHTMLEditUtils::IsListItem(aListItem),
                  "expected a list item and didn't get one");
  
  // get the listitem parent and the active editing host.
  nsIContent* rootContent = mHTMLEditor->GetActiveEditingHost();
  nsCOMPtr<nsIDOMNode> rootNode = do_QueryInterface(rootContent);
  PRInt32 itemOffset;
  nsCOMPtr<nsIDOMNode> list = nsEditor::GetNodeLocation(aListItem, &itemOffset);

  // if we are in an empty listitem, then we want to pop up out of the list
  // but only if prefs says it's ok and if the parent isn't the active editing host.
  bool isEmpty;
  res = IsEmptyBlock(aListItem, &isEmpty, true, false);
  NS_ENSURE_SUCCESS(res, res);
  if (isEmpty && (rootNode != list) && mReturnInEmptyLIKillsList)
  {
    // get the list offset now -- before we might eventually split the list
    PRInt32 offset;
    nsCOMPtr<nsIDOMNode> listparent = nsEditor::GetNodeLocation(list, &offset);

    // are we the last list item in the list?
    bool bIsLast;
    res = mHTMLEditor->IsLastEditableChild(aListItem, &bIsLast);
    NS_ENSURE_SUCCESS(res, res);
    if (!bIsLast)
    {
      // we need to split the list!
      nsCOMPtr<nsIDOMNode> tempNode;
      res = mHTMLEditor->SplitNode(list, itemOffset, getter_AddRefs(tempNode));
      NS_ENSURE_SUCCESS(res, res);
    }

    // are we in a sublist?
    if (nsHTMLEditUtils::IsList(listparent))  //in a sublist
    {
      // if so, move this list item out of this list and into the grandparent list
      res = mHTMLEditor->MoveNode(aListItem,listparent,offset+1);
      NS_ENSURE_SUCCESS(res, res);
      res = aSelection->Collapse(aListItem,0);
    }
    else
    {
      // otherwise kill this listitem
      res = mHTMLEditor->DeleteNode(aListItem);
      NS_ENSURE_SUCCESS(res, res);
      
      // time to insert a paragraph
      NS_NAMED_LITERAL_STRING(pType, "p");
      nsCOMPtr<nsIDOMNode> pNode;
      res = mHTMLEditor->CreateNode(pType, listparent, offset+1, getter_AddRefs(pNode));
      NS_ENSURE_SUCCESS(res, res);

      // append a <br> to it
      nsCOMPtr<nsIDOMNode> brNode;
      res = mHTMLEditor->CreateBR(pNode, 0, address_of(brNode));
      NS_ENSURE_SUCCESS(res, res);

      // set selection to before the break
      res = aSelection->Collapse(pNode, 0);
    }
    return res;
  }
  
  // else we want a new list item at the same list level.
  // get ws code to adjust any ws
  nsCOMPtr<nsIDOMNode> selNode = aNode;
  res = nsWSRunObject::PrepareToSplitAcrossBlocks(mHTMLEditor, address_of(selNode), &aOffset);
  NS_ENSURE_SUCCESS(res, res);
  // now split list item
  PRInt32 newOffset;
  res = mHTMLEditor->SplitNodeDeep( aListItem, selNode, aOffset, &newOffset, false);
  NS_ENSURE_SUCCESS(res, res);
  // hack: until I can change the damaged doc range code back to being
  // extra inclusive, I have to manually detect certain list items that
  // may be left empty.
  nsCOMPtr<nsIDOMNode> prevItem;
  mHTMLEditor->GetPriorHTMLSibling(aListItem, address_of(prevItem));

  if (prevItem && nsHTMLEditUtils::IsListItem(prevItem))
  {
    bool bIsEmptyNode;
    res = mHTMLEditor->IsEmptyNode(prevItem, &bIsEmptyNode);
    NS_ENSURE_SUCCESS(res, res);
    if (bIsEmptyNode) {
      res = CreateMozBR(prevItem, 0);
      NS_ENSURE_SUCCESS(res, res);
    } else {
      res = mHTMLEditor->IsEmptyNode(aListItem, &bIsEmptyNode, true);
      NS_ENSURE_SUCCESS(res, res);
      if (bIsEmptyNode) 
      {
        nsCOMPtr<nsIAtom> nodeAtom = nsEditor::GetTag(aListItem);
        if (nodeAtom == nsEditProperty::dd || nodeAtom == nsEditProperty::dt)
        {
          PRInt32 itemOffset;
          nsCOMPtr<nsIDOMNode> list = nsEditor::GetNodeLocation(aListItem, &itemOffset);

          nsAutoString listTag((nodeAtom == nsEditProperty::dt) ? NS_LITERAL_STRING("dd") : NS_LITERAL_STRING("dt"));
          nsCOMPtr<nsIDOMNode> newListItem;
          res = mHTMLEditor->CreateNode(listTag, list, itemOffset+1, getter_AddRefs(newListItem));
          NS_ENSURE_SUCCESS(res, res);
          res = mEditor->DeleteNode(aListItem);
          NS_ENSURE_SUCCESS(res, res);
          return aSelection->Collapse(newListItem, 0);
        }

        nsCOMPtr<nsIDOMNode> brNode;
        res = mHTMLEditor->CopyLastEditableChildStyles(prevItem, aListItem, getter_AddRefs(brNode));
        NS_ENSURE_SUCCESS(res, res);
        if (brNode) 
        {
          PRInt32 offset;
          nsCOMPtr<nsIDOMNode> brParent = nsEditor::GetNodeLocation(brNode, &offset);
          return aSelection->Collapse(brParent, offset);
        }
      }
      else
      {
        nsWSRunObject wsObj(mHTMLEditor, aListItem, 0);
        nsCOMPtr<nsIDOMNode> visNode;
        PRInt32 visOffset = 0;
        PRInt16 wsType;
        wsObj.NextVisibleNode(aListItem, 0, address_of(visNode),
                              &visOffset, &wsType);
        if ( (wsType==nsWSRunObject::eSpecial)  || 
             (wsType==nsWSRunObject::eBreak)    ||
             nsHTMLEditUtils::IsHR(visNode) ) 
        {
          PRInt32 offset;
          nsCOMPtr<nsIDOMNode> parent = nsEditor::GetNodeLocation(visNode, &offset);
          return aSelection->Collapse(parent, offset);
        }
        else
        {
          return aSelection->Collapse(visNode, visOffset);
        }
      }
    }
  }
  res = aSelection->Collapse(aListItem,0);
  return res;
}


///////////////////////////////////////////////////////////////////////////
// MakeBlockquote:  put the list of nodes into one or more blockquotes.  
//                       
nsresult 
nsHTMLEditRules::MakeBlockquote(nsCOMArray<nsIDOMNode>& arrayOfNodes)
{
  // the idea here is to put the nodes into a minimal number of 
  // blockquotes.  When the user blockquotes something, they expect
  // one blockquote.  That may not be possible (for instance, if they
  // have two table cells selected, you need two blockquotes inside the cells).
  
  nsresult res = NS_OK;
  
  nsCOMPtr<nsIDOMNode> curNode, curParent, curBlock, newBlock;
  PRInt32 offset;
  PRInt32 listCount = arrayOfNodes.Count();
  
  nsCOMPtr<nsIDOMNode> prevParent;
  
  PRInt32 i;
  for (i=0; i<listCount; i++)
  {
    // get the node to act on, and its location
    curNode = arrayOfNodes[i];
    curParent = nsEditor::GetNodeLocation(curNode, &offset);

    // if the node is a table element or list item, dive inside
    if (nsHTMLEditUtils::IsTableElementButNotTable(curNode) || 
        nsHTMLEditUtils::IsListItem(curNode))
    {
      curBlock = 0;  // forget any previous block
      // recursion time
      nsCOMArray<nsIDOMNode> childArray;
      res = GetChildNodesForOperation(curNode, childArray);
      NS_ENSURE_SUCCESS(res, res);
      res = MakeBlockquote(childArray);
      NS_ENSURE_SUCCESS(res, res);
    }
    
    // if the node has different parent than previous node,
    // further nodes in a new parent
    if (prevParent)
    {
      nsCOMPtr<nsIDOMNode> temp;
      curNode->GetParentNode(getter_AddRefs(temp));
      if (temp != prevParent)
      {
        curBlock = 0;  // forget any previous blockquote node we were using
        prevParent = temp;
      }
    }
    else     

    {
      curNode->GetParentNode(getter_AddRefs(prevParent));
    }
    
    // if no curBlock, make one
    if (!curBlock)
    {
      NS_NAMED_LITERAL_STRING(quoteType, "blockquote");
      res = SplitAsNeeded(&quoteType, address_of(curParent), &offset);
      NS_ENSURE_SUCCESS(res, res);
      res = mHTMLEditor->CreateNode(quoteType, curParent, offset, getter_AddRefs(curBlock));
      NS_ENSURE_SUCCESS(res, res);
      // remember our new block for postprocessing
      mNewBlock = curBlock;
      // note: doesn't matter if we set mNewBlock multiple times.
    }
      
    res = mHTMLEditor->MoveNode(curNode, curBlock, -1);
    NS_ENSURE_SUCCESS(res, res);
  }
  return res;
}



///////////////////////////////////////////////////////////////////////////
// RemoveBlockStyle:  make the nodes have no special block type.  
//                       
nsresult 
nsHTMLEditRules::RemoveBlockStyle(nsCOMArray<nsIDOMNode>& arrayOfNodes)
{
  // intent of this routine is to be used for converting to/from
  // headers, paragraphs, pre, and address.  Those blocks
  // that pretty much just contain inline things...
  
  nsresult res = NS_OK;
  
  nsCOMPtr<nsIDOMNode> curBlock, firstNode, lastNode;
  PRInt32 listCount = arrayOfNodes.Count();
  for (PRInt32 i = 0; i < listCount; ++i) {
    // get the node to act on, and its location
    nsCOMPtr<nsIDOMNode> curNode = arrayOfNodes[i];

    nsCOMPtr<dom::Element> curElement = do_QueryInterface(curNode);

    // if curNode is a address, p, header, address, or pre, remove it 
    if (curElement && nsHTMLEditUtils::IsFormatNode(curElement)) {
      // process any partial progress saved
      if (curBlock)
      {
        res = RemovePartOfBlock(curBlock, firstNode, lastNode);
        NS_ENSURE_SUCCESS(res, res);
        curBlock = 0;  firstNode = 0;  lastNode = 0;
      }
      // remove curent block
      res = mHTMLEditor->RemoveBlockContainer(curNode); 
      NS_ENSURE_SUCCESS(res, res);
    } else if (curElement &&
               (curElement->IsHTML(nsGkAtoms::table)      ||
                curElement->IsHTML(nsGkAtoms::tr)         ||
                curElement->IsHTML(nsGkAtoms::tbody)      ||
                curElement->IsHTML(nsGkAtoms::td)         ||
                nsHTMLEditUtils::IsList(curElement)       ||
                curElement->IsHTML(nsGkAtoms::li)         ||
                curElement->IsHTML(nsGkAtoms::blockquote) ||
                curElement->IsHTML(nsGkAtoms::div))) {
      // process any partial progress saved
      if (curBlock)
      {
        res = RemovePartOfBlock(curBlock, firstNode, lastNode);
        NS_ENSURE_SUCCESS(res, res);
        curBlock = 0;  firstNode = 0;  lastNode = 0;
      }
      // recursion time
      nsCOMArray<nsIDOMNode> childArray;
      res = GetChildNodesForOperation(curNode, childArray);
      NS_ENSURE_SUCCESS(res, res);
      res = RemoveBlockStyle(childArray);
      NS_ENSURE_SUCCESS(res, res);
    }
    else if (IsInlineNode(curNode))
    {
      if (curBlock)
      {
        // if so, is this node a descendant?
        if (nsEditorUtils::IsDescendantOf(curNode, curBlock))
        {
          lastNode = curNode;
          continue;  // then we don't need to do anything different for this node
        }
        else
        {
          // otherwise, we have progressed beyond end of curBlock,
          // so lets handle it now.  We need to remove the portion of 
          // curBlock that contains [firstNode - lastNode].
          res = RemovePartOfBlock(curBlock, firstNode, lastNode);
          NS_ENSURE_SUCCESS(res, res);
          curBlock = 0;  firstNode = 0;  lastNode = 0;
          // fall out and handle curNode
        }
      }
      curBlock = mHTMLEditor->GetBlockNodeParent(curNode);
      if (nsHTMLEditUtils::IsFormatNode(curBlock))
      {
        firstNode = curNode;  
        lastNode = curNode;
      }
      else
        curBlock = 0;  // not a block kind that we care about.
    }
    else
    { // some node that is already sans block style.  skip over it and
      // process any partial progress saved
      if (curBlock)
      {
        res = RemovePartOfBlock(curBlock, firstNode, lastNode);
        NS_ENSURE_SUCCESS(res, res);
        curBlock = 0;  firstNode = 0;  lastNode = 0;
      }
    }
  }
  // process any partial progress saved
  if (curBlock)
  {
    res = RemovePartOfBlock(curBlock, firstNode, lastNode);
    NS_ENSURE_SUCCESS(res, res);
    curBlock = 0;  firstNode = 0;  lastNode = 0;
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// ApplyBlockStyle:  do whatever it takes to make the list of nodes into 
//                   one or more blocks of type blockTag.  
//                       
nsresult 
nsHTMLEditRules::ApplyBlockStyle(nsCOMArray<nsIDOMNode>& arrayOfNodes, const nsAString *aBlockTag)
{
  // intent of this routine is to be used for converting to/from
  // headers, paragraphs, pre, and address.  Those blocks
  // that pretty much just contain inline things...
  
  NS_ENSURE_TRUE(aBlockTag, NS_ERROR_NULL_POINTER);
  nsresult res = NS_OK;
  
  nsCOMPtr<nsIDOMNode> curNode, curParent, curBlock, newBlock;
  PRInt32 offset;
  PRInt32 listCount = arrayOfNodes.Count();
  nsString tString(*aBlockTag);////MJUDGE SCC NEED HELP

  // Remove all non-editable nodes.  Leave them be.
  PRInt32 j;
  for (j=listCount-1; j>=0; j--)
  {
    if (!mHTMLEditor->IsEditable(arrayOfNodes[j]))
    {
      arrayOfNodes.RemoveObjectAt(j);
    }
  }
  
  // reset list count
  listCount = arrayOfNodes.Count();
  
  PRInt32 i;
  for (i=0; i<listCount; i++)
  {
    // get the node to act on, and its location
    curNode = arrayOfNodes[i];
    curParent = nsEditor::GetNodeLocation(curNode, &offset);
    nsAutoString curNodeTag;
    nsEditor::GetTagString(curNode, curNodeTag);
    ToLowerCase(curNodeTag);
 
    // is it already the right kind of block?
    if (curNodeTag == *aBlockTag)
    {
      curBlock = 0;  // forget any previous block used for previous inline nodes
      continue;  // do nothing to this block
    }
        
    // if curNode is a address, p, header, address, or pre, replace 
    // it with a new block of correct type.
    // xxx floppy moose: pre can't hold everything the others can
    if (nsHTMLEditUtils::IsMozDiv(curNode)     ||
        nsHTMLEditUtils::IsFormatNode(curNode))
    {
      curBlock = 0;  // forget any previous block used for previous inline nodes
      res = mHTMLEditor->ReplaceContainer(curNode, address_of(newBlock), *aBlockTag,
                                          nsnull, nsnull, true);
      NS_ENSURE_SUCCESS(res, res);
    }
    else if (nsHTMLEditUtils::IsTable(curNode)                    || 
             (curNodeTag.EqualsLiteral("tbody"))      ||
             (curNodeTag.EqualsLiteral("tr"))         ||
             (curNodeTag.EqualsLiteral("td"))         ||
             nsHTMLEditUtils::IsList(curNode)                     ||
             (curNodeTag.EqualsLiteral("li"))         ||
             nsHTMLEditUtils::IsBlockquote(curNode)               ||
             nsHTMLEditUtils::IsDiv(curNode))
    {
      curBlock = 0;  // forget any previous block used for previous inline nodes
      // recursion time
      nsCOMArray<nsIDOMNode> childArray;
      res = GetChildNodesForOperation(curNode, childArray);
      NS_ENSURE_SUCCESS(res, res);
      PRInt32 childCount = childArray.Count();
      if (childCount)
      {
        res = ApplyBlockStyle(childArray, aBlockTag);
        NS_ENSURE_SUCCESS(res, res);
      }
      else
      {
        // make sure we can put a block here
        res = SplitAsNeeded(aBlockTag, address_of(curParent), &offset);
        NS_ENSURE_SUCCESS(res, res);
        nsCOMPtr<nsIDOMNode> theBlock;
        res = mHTMLEditor->CreateNode(*aBlockTag, curParent, offset, getter_AddRefs(theBlock));
        NS_ENSURE_SUCCESS(res, res);
        // remember our new block for postprocessing
        mNewBlock = theBlock;
      }
    }
    
    // if the node is a break, we honor it by putting further nodes in a new parent
    else if (curNodeTag.EqualsLiteral("br"))
    {
      if (curBlock)
      {
        curBlock = 0;  // forget any previous block used for previous inline nodes
        res = mHTMLEditor->DeleteNode(curNode);
        NS_ENSURE_SUCCESS(res, res);
      }
      else
      {
        // the break is the first (or even only) node we encountered.  Create a
        // block for it.
        res = SplitAsNeeded(aBlockTag, address_of(curParent), &offset);
        NS_ENSURE_SUCCESS(res, res);
        res = mHTMLEditor->CreateNode(*aBlockTag, curParent, offset, getter_AddRefs(curBlock));
        NS_ENSURE_SUCCESS(res, res);
        // remember our new block for postprocessing
        mNewBlock = curBlock;
        // note: doesn't matter if we set mNewBlock multiple times.
        res = mHTMLEditor->MoveNode(curNode, curBlock, -1);
        NS_ENSURE_SUCCESS(res, res);
      }
    }
        
    
    // if curNode is inline, pull it into curBlock
    // note: it's assumed that consecutive inline nodes in the 
    // arrayOfNodes are actually members of the same block parent.
    // this happens to be true now as a side effect of how
    // arrayOfNodes is contructed, but some additional logic should
    // be added here if that should change
    
    else if (IsInlineNode(curNode))
    {
      // if curNode is a non editable, drop it if we are going to <pre>
      if (tString.LowerCaseEqualsLiteral("pre") 
        && (!mHTMLEditor->IsEditable(curNode)))
        continue; // do nothing to this block
      
      // if no curBlock, make one
      if (!curBlock)
      {
        res = SplitAsNeeded(aBlockTag, address_of(curParent), &offset);
        NS_ENSURE_SUCCESS(res, res);
        res = mHTMLEditor->CreateNode(*aBlockTag, curParent, offset, getter_AddRefs(curBlock));
        NS_ENSURE_SUCCESS(res, res);
        // remember our new block for postprocessing
        mNewBlock = curBlock;
        // note: doesn't matter if we set mNewBlock multiple times.
      }
      
      // if curNode is a Break, replace it with a return if we are going to <pre>
      // xxx floppy moose
 
      // this is a continuation of some inline nodes that belong together in
      // the same block item.  use curBlock
      res = mHTMLEditor->MoveNode(curNode, curBlock, -1);
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// SplitAsNeeded:  given a tag name, split inOutParent up to the point   
//                 where we can insert the tag.  Adjust inOutParent and
//                 inOutOffset to pint to new location for tag.
nsresult 
nsHTMLEditRules::SplitAsNeeded(const nsAString *aTag, 
                               nsCOMPtr<nsIDOMNode> *inOutParent,
                               PRInt32 *inOutOffset)
{
  NS_ENSURE_TRUE(aTag && inOutParent && inOutOffset, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(*inOutParent, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIDOMNode> tagParent, temp, splitNode, parent = *inOutParent;
  nsresult res = NS_OK;
  nsCOMPtr<nsIAtom> tagAtom = do_GetAtom(*aTag);
   
  // check that we have a place that can legally contain the tag
  while (!tagParent)
  {
    // sniffing up the parent tree until we find 
    // a legal place for the block
    if (!parent) break;
    // Don't leave the active editing host
    if (!mHTMLEditor->IsDescendantOfEditorRoot(parent)) {
      nsCOMPtr<nsIContent> parentContent = do_QueryInterface(parent);
      if (parentContent != mHTMLEditor->GetActiveEditingHost()) {
        break;
      }
    }
    if (mHTMLEditor->CanContainTag(parent, tagAtom)) {
      tagParent = parent;
      break;
    }
    splitNode = parent;
    parent->GetParentNode(getter_AddRefs(temp));
    parent = temp;
  }
  if (!tagParent)
  {
    // could not find a place to build tag!
    return NS_ERROR_FAILURE;
  }
  if (splitNode)
  {
    // we found a place for block, but above inOutParent.  We need to split nodes.
    res = mHTMLEditor->SplitNodeDeep(splitNode, *inOutParent, *inOutOffset, inOutOffset);
    NS_ENSURE_SUCCESS(res, res);
    *inOutParent = tagParent;
  }
  return res;
}      

///////////////////////////////////////////////////////////////////////////
// JoinNodesSmart:  join two nodes, doing whatever makes sense for their  
//                  children (which often means joining them, too).
//                  aNodeLeft & aNodeRight must be same type of node.
nsresult 
nsHTMLEditRules::JoinNodesSmart( nsIDOMNode *aNodeLeft, 
                                 nsIDOMNode *aNodeRight, 
                                 nsCOMPtr<nsIDOMNode> *aOutMergeParent, 
                                 PRInt32 *aOutMergeOffset)
{
  // check parms
  NS_ENSURE_TRUE(aNodeLeft &&  
      aNodeRight && 
      aOutMergeParent &&
      aOutMergeOffset, NS_ERROR_NULL_POINTER);
  
  nsresult res = NS_OK;
  // caller responsible for:
  //   left & right node are same type
  PRInt32 parOffset;
  nsCOMPtr<nsIDOMNode> rightParent;
  nsCOMPtr<nsIDOMNode> parent = nsEditor::GetNodeLocation(aNodeLeft, &parOffset);
  aNodeRight->GetParentNode(getter_AddRefs(rightParent));

  // if they don't have the same parent, first move the 'right' node 
  // to after the 'left' one
  if (parent != rightParent)
  {
    res = mHTMLEditor->MoveNode(aNodeRight, parent, parOffset);
    NS_ENSURE_SUCCESS(res, res);
  }
  
  // defaults for outParams
  *aOutMergeParent = aNodeRight;
  res = mHTMLEditor->GetLengthOfDOMNode(aNodeLeft, *((PRUint32*)aOutMergeOffset));
  NS_ENSURE_SUCCESS(res, res);

  // separate join rules for differing blocks
  if (nsHTMLEditUtils::IsList(aNodeLeft) ||
      mHTMLEditor->IsTextNode(aNodeLeft))
  {
    // for list's, merge shallow (wouldn't want to combine list items)
    res = mHTMLEditor->JoinNodes(aNodeLeft, aNodeRight, parent);
    NS_ENSURE_SUCCESS(res, res);
    return res;
  }
  else
  {
    // remember the last left child, and firt right child
    nsCOMPtr<nsIDOMNode> lastLeft, firstRight;
    res = mHTMLEditor->GetLastEditableChild(aNodeLeft, address_of(lastLeft));
    NS_ENSURE_SUCCESS(res, res);
    res = mHTMLEditor->GetFirstEditableChild(aNodeRight, address_of(firstRight));
    NS_ENSURE_SUCCESS(res, res);

    // for list items, divs, etc, merge smart
    res = mHTMLEditor->JoinNodes(aNodeLeft, aNodeRight, parent);
    NS_ENSURE_SUCCESS(res, res);

    if (lastLeft && firstRight &&
        mHTMLEditor->NodesSameType(lastLeft, firstRight) &&
        (nsEditor::IsTextNode(lastLeft) ||
         mHTMLEditor->mHTMLCSSUtils->ElementsSameStyle(lastLeft, firstRight)))
      return JoinNodesSmart(lastLeft, firstRight, aOutMergeParent, aOutMergeOffset);
  }
  return res;
}


nsresult 
nsHTMLEditRules::GetTopEnclosingMailCite(nsIDOMNode *aNode, 
                                         nsCOMPtr<nsIDOMNode> *aOutCiteNode,
                                         bool aPlainText)
{
  // check parms
  NS_ENSURE_TRUE(aNode && aOutCiteNode, NS_ERROR_NULL_POINTER);
  
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> node, parentNode;
  node = do_QueryInterface(aNode);
  
  while (node)
  {
    if ( (aPlainText && nsHTMLEditUtils::IsPre(node)) ||
         nsHTMLEditUtils::IsMailCite(node) )
      *aOutCiteNode = node;
    if (nsTextEditUtils::IsBody(node)) break;
    
    res = node->GetParentNode(getter_AddRefs(parentNode));
    NS_ENSURE_SUCCESS(res, res);
    node = parentNode;
  }

  return res;
}


nsresult 
nsHTMLEditRules::CacheInlineStyles(nsIDOMNode *aNode)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);

  bool useCSS = mHTMLEditor->IsCSSEnabled();

  for (PRInt32 j = 0; j < SIZE_STYLE_TABLE; ++j)
  {
    bool isSet = false;
    nsAutoString outValue;
    if (!useCSS)
    {
      mHTMLEditor->IsTextPropertySetByContent(aNode, mCachedStyles[j].tag,
                                              &(mCachedStyles[j].attr), nsnull,
                                              isSet, &outValue);
    }
    else
    {
      mHTMLEditor->mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(aNode,
        mCachedStyles[j].tag, &(mCachedStyles[j].attr), isSet, outValue,
        nsHTMLCSSUtils::eComputed);
    }
    if (isSet)
    {
      mCachedStyles[j].mPresent = true;
      mCachedStyles[j].value.Assign(outValue);
    }
  }
  return NS_OK;
}


nsresult
nsHTMLEditRules::ReapplyCachedStyles()
{
  // The idea here is to examine our cached list of styles and see if any have
  // been removed.  If so, add typeinstate for them, so that they will be
  // reinserted when new content is added.

  // remember if we are in css mode
  bool useCSS = mHTMLEditor->IsCSSEnabled();

  // get selection point; if it doesn't exist, we have nothing to do
  nsRefPtr<Selection> selection = mHTMLEditor->GetSelection();
  MOZ_ASSERT(selection);
  if (!selection->GetRangeCount()) {
    // Nothing to do
    return NS_OK;
  }
  nsCOMPtr<nsIContent> selNode =
    do_QueryInterface(selection->GetRangeAt(0)->GetStartParent());
  if (!selNode) {
    // Nothing to do
    return NS_OK;
  }

  for (PRInt32 i = 0; i < SIZE_STYLE_TABLE; ++i) {
    if (mCachedStyles[i].mPresent) {
      bool bFirst, bAny, bAll;
      bFirst = bAny = bAll = false;

      nsAutoString curValue;
      if (useCSS) {
        // check computed style first in css case
        bAny = mHTMLEditor->mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(
          selNode, mCachedStyles[i].tag, &(mCachedStyles[i].attr), curValue,
          nsHTMLCSSUtils::eComputed);
      }
      if (!bAny) {
        // then check typeinstate and html style
        nsresult res = mHTMLEditor->GetInlinePropertyBase(mCachedStyles[i].tag,
                                                     &(mCachedStyles[i].attr),
                                                     &(mCachedStyles[i].value),
                                                     &bFirst, &bAny, &bAll,
                                                     &curValue, false);
        NS_ENSURE_SUCCESS(res, res);
      }
      // this style has disappeared through deletion.  Add to our typeinstate:
      if (!bAny || IsStyleCachePreservingAction(mTheAction)) {
        mHTMLEditor->mTypeInState->SetProp(mCachedStyles[i].tag,
                                           mCachedStyles[i].attr,
                                           mCachedStyles[i].value);
      }
    }
  }

  return NS_OK;
}


nsresult
nsHTMLEditRules::ClearCachedStyles()
{
  // clear the mPresent bits in mCachedStyles array
  
  PRInt32 j;
  for (j=0; j<SIZE_STYLE_TABLE; j++)
  {
    mCachedStyles[j].mPresent = false;
    mCachedStyles[j].value.Truncate(0);
  }
  return NS_OK;
}


nsresult 
nsHTMLEditRules::AdjustSpecialBreaks(bool aSafeToAskFrames)
{
  nsCOMArray<nsIDOMNode> arrayOfNodes;
  nsCOMPtr<nsISupports> isupports;
  PRInt32 nodeCount,j;
  
  // gather list of empty nodes
  nsEmptyEditableFunctor functor(mHTMLEditor);
  nsDOMIterator iter;
  nsresult res = iter.Init(mDocChangeRange);
  NS_ENSURE_SUCCESS(res, res);
  res = iter.AppendList(functor, arrayOfNodes);
  NS_ENSURE_SUCCESS(res, res);

  // put moz-br's into these empty li's and td's
  nodeCount = arrayOfNodes.Count();
  for (j = 0; j < nodeCount; j++)
  {
    // need to put br at END of node.  It may have
    // empty containers in it and still pass the "IsEmptynode" test,
    // and we want the br's to be after them.  Also, we want the br
    // to be after the selection if the selection is in this node.
    PRUint32 len;
    nsCOMPtr<nsIDOMNode> theNode = arrayOfNodes[0];
    arrayOfNodes.RemoveObjectAt(0);
    res = nsEditor::GetLengthOfDOMNode(theNode, len);
    NS_ENSURE_SUCCESS(res, res);
    res = CreateMozBR(theNode, (PRInt32)len);
    NS_ENSURE_SUCCESS(res, res);
  }
  
  return res;
}

nsresult 
nsHTMLEditRules::AdjustWhitespace(nsISelection *aSelection)
{
  // get selection point
  nsCOMPtr<nsIDOMNode> selNode;
  PRInt32 selOffset;
  nsresult res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(selNode), &selOffset);
  NS_ENSURE_SUCCESS(res, res);
  
  // ask whitespace object to tweak nbsp's
  return nsWSRunObject(mHTMLEditor, selNode, selOffset).AdjustWhitespace();
}

nsresult 
nsHTMLEditRules::PinSelectionToNewBlock(nsISelection *aSelection)
{
  NS_ENSURE_TRUE(aSelection, NS_ERROR_NULL_POINTER);
  if (!aSelection->Collapsed()) {
    return NS_OK;
  }

  // get the (collapsed) selection location
  nsCOMPtr<nsIDOMNode> selNode, temp;
  PRInt32 selOffset;
  nsresult res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(selNode), &selOffset);
  NS_ENSURE_SUCCESS(res, res);
  temp = selNode;
  
  // use ranges and sRangeHelper to compare sel point to new block
  nsRefPtr<nsRange> range = new nsRange();
  res = range->SetStart(selNode, selOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = range->SetEnd(selNode, selOffset);
  NS_ENSURE_SUCCESS(res, res);
  nsCOMPtr<nsIContent> block (do_QueryInterface(mNewBlock));
  NS_ENSURE_TRUE(block, NS_ERROR_NO_INTERFACE);
  bool nodeBefore, nodeAfter;
  res = nsRange::CompareNodeToRange(block, range, &nodeBefore, &nodeAfter);
  NS_ENSURE_SUCCESS(res, res);
  
  if (nodeBefore && nodeAfter)
    return NS_OK;  // selection is inside block
  else if (nodeBefore)
  {
    // selection is after block.  put at end of block.
    nsCOMPtr<nsIDOMNode> tmp = mNewBlock;
    mHTMLEditor->GetLastEditableChild(mNewBlock, address_of(tmp));
    PRUint32 endPoint;
    if (mHTMLEditor->IsTextNode(tmp) || mHTMLEditor->IsContainer(tmp))
    {
      res = nsEditor::GetLengthOfDOMNode(tmp, endPoint);
      NS_ENSURE_SUCCESS(res, res);
    }
    else
    {
      tmp = nsEditor::GetNodeLocation(tmp, (PRInt32*)&endPoint);
      endPoint++;  // want to be after this node
    }
    return aSelection->Collapse(tmp, (PRInt32)endPoint);
  }
  else
  {
    // selection is before block.  put at start of block.
    nsCOMPtr<nsIDOMNode> tmp = mNewBlock;
    mHTMLEditor->GetFirstEditableChild(mNewBlock, address_of(tmp));
    PRInt32 offset;
    if (!(mHTMLEditor->IsTextNode(tmp) || mHTMLEditor->IsContainer(tmp)))
    {
      tmp = nsEditor::GetNodeLocation(tmp, &offset);
    }
    return aSelection->Collapse(tmp, 0);
  }
}

nsresult 
nsHTMLEditRules::CheckInterlinePosition(nsISelection *aSelection)
{
  NS_ENSURE_TRUE(aSelection, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsISelection> selection(aSelection);
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));

  // if the selection isn't collapsed, do nothing.
  if (!aSelection->Collapsed()) {
    return NS_OK;
  }

  // get the (collapsed) selection location
  nsCOMPtr<nsIDOMNode> selNode, node;
  PRInt32 selOffset;
  nsresult res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(selNode), &selOffset);
  NS_ENSURE_SUCCESS(res, res);

  // First, let's check to see if we are after a <br>.  We take care of this
  // special-case first so that we don't accidentally fall through into one
  // of the other conditionals.
  mHTMLEditor->GetPriorHTMLNode(selNode, selOffset, address_of(node), true);
  if (node && nsTextEditUtils::IsBreak(node))
  {
    selPriv->SetInterlinePosition(true);
    return NS_OK;
  }

  // are we after a block?  If so try set caret to following content
  mHTMLEditor->GetPriorHTMLSibling(selNode, selOffset, address_of(node));
  if (node && IsBlockNode(node))
  {
    selPriv->SetInterlinePosition(true);
    return NS_OK;
  }

  // are we before a block?  If so try set caret to prior content
  mHTMLEditor->GetNextHTMLSibling(selNode, selOffset, address_of(node));
  if (node && IsBlockNode(node))
    selPriv->SetInterlinePosition(false);
  return NS_OK;
}

nsresult 
nsHTMLEditRules::AdjustSelection(nsISelection *aSelection, nsIEditor::EDirection aAction)
{
  NS_ENSURE_TRUE(aSelection, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsISelection> selection(aSelection);
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));
 
  // if the selection isn't collapsed, do nothing.
  // moose: one thing to do instead is check for the case of
  // only a single break selected, and collapse it.  Good thing?  Beats me.
  if (!aSelection->Collapsed()) {
    return NS_OK;
  }

  // get the (collapsed) selection location
  nsCOMPtr<nsIDOMNode> selNode, temp;
  PRInt32 selOffset;
  nsresult res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(selNode), &selOffset);
  NS_ENSURE_SUCCESS(res, res);
  temp = selNode;
  
  // are we in an editable node?
  while (!mHTMLEditor->IsEditable(selNode))
  {
    // scan up the tree until we find an editable place to be
    selNode = nsEditor::GetNodeLocation(temp, &selOffset);
    NS_ENSURE_TRUE(selNode, NS_ERROR_FAILURE);
    temp = selNode;
  }
  
  // make sure we aren't in an empty block - user will see no cursor.  If this
  // is happening, put a <br> in the block if allowed.
  nsCOMPtr<nsIDOMNode> theblock;
  if (IsBlockNode(selNode)) theblock = selNode;
  else theblock = mHTMLEditor->GetBlockNodeParent(selNode);
  if (theblock && mHTMLEditor->IsEditable(theblock)) {
    bool bIsEmptyNode;
    res = mHTMLEditor->IsEmptyNode(theblock, &bIsEmptyNode, false, false);
    NS_ENSURE_SUCCESS(res, res);
    // check if br can go into the destination node
    if (bIsEmptyNode && mHTMLEditor->CanContainTag(selNode, nsGkAtoms::br)) {
      nsCOMPtr<nsIDOMNode> rootNode = do_QueryInterface(mHTMLEditor->GetRoot());
      NS_ENSURE_TRUE(rootNode, NS_ERROR_FAILURE);
      if (selNode == rootNode)
      {
        // Our root node is completely empty. Don't add a <br> here.
        // AfterEditInner() will add one for us when it calls
        // CreateBogusNodeIfNeeded()!
        return NS_OK;
      }

      // we know we can skip the rest of this routine given the cirumstance
      return CreateMozBR(selNode, selOffset);
    }
  }
  
  // are we in a text node? 
  nsCOMPtr<nsIDOMCharacterData> textNode = do_QueryInterface(selNode);
  if (textNode)
    return NS_OK; // we LIKE it when we are in a text node.  that RULZ
  
  // do we need to insert a special mozBR?  We do if we are:
  // 1) prior node is in same block where selection is AND
  // 2) prior node is a br AND
  // 3) that br is not visible

  nsCOMPtr<nsIDOMNode> nearNode;
  res = mHTMLEditor->GetPriorHTMLNode(selNode, selOffset, address_of(nearNode));
  NS_ENSURE_SUCCESS(res, res);
  if (nearNode) 
  {
    // is nearNode also a descendant of same block?
    nsCOMPtr<nsIDOMNode> block, nearBlock;
    if (IsBlockNode(selNode)) block = selNode;
    else block = mHTMLEditor->GetBlockNodeParent(selNode);
    nearBlock = mHTMLEditor->GetBlockNodeParent(nearNode);
    if (block == nearBlock)
    {
      if (nearNode && nsTextEditUtils::IsBreak(nearNode) )
      {   
        if (!mHTMLEditor->IsVisBreak(nearNode))
        {
          // need to insert special moz BR. Why?  Because if we don't
          // the user will see no new line for the break.  Also, things
          // like table cells won't grow in height.
          nsCOMPtr<nsIDOMNode> brNode;
          res = CreateMozBR(selNode, selOffset, getter_AddRefs(brNode));
          NS_ENSURE_SUCCESS(res, res);
          selNode = nsEditor::GetNodeLocation(brNode, &selOffset);
          // selection stays *before* moz-br, sticking to it
          selPriv->SetInterlinePosition(true);
          res = aSelection->Collapse(selNode,selOffset);
          NS_ENSURE_SUCCESS(res, res);
        }
        else
        {
          nsCOMPtr<nsIDOMNode> nextNode;
          mHTMLEditor->GetNextHTMLNode(nearNode, address_of(nextNode), true);
          if (nextNode && nsTextEditUtils::IsMozBR(nextNode))
          {
            // selection between br and mozbr.  make it stick to mozbr
            // so that it will be on blank line.   
            selPriv->SetInterlinePosition(true);
          }
        }
      }
    }
  }

  // we aren't in a textnode: are we adjacent to text or a break or an image?
  res = mHTMLEditor->GetPriorHTMLNode(selNode, selOffset, address_of(nearNode), true);
  NS_ENSURE_SUCCESS(res, res);
  if (nearNode && (nsTextEditUtils::IsBreak(nearNode)
                   || nsEditor::IsTextNode(nearNode)
                   || nsHTMLEditUtils::IsImage(nearNode)
                   || nsHTMLEditUtils::IsHR(nearNode)))
    return NS_OK; // this is a good place for the caret to be
  res = mHTMLEditor->GetNextHTMLNode(selNode, selOffset, address_of(nearNode), true);
  NS_ENSURE_SUCCESS(res, res);
  if (nearNode && (nsTextEditUtils::IsBreak(nearNode)
                   || nsEditor::IsTextNode(nearNode)
                   || nsHTMLEditUtils::IsImage(nearNode)
                   || nsHTMLEditUtils::IsHR(nearNode)))
    return NS_OK; // this is a good place for the caret to be

  // look for a nearby text node.
  // prefer the correct direction.
  res = FindNearSelectableNode(selNode, selOffset, aAction, address_of(nearNode));
  NS_ENSURE_SUCCESS(res, res);

  if (nearNode)
  {
    // is the nearnode a text node?
    textNode = do_QueryInterface(nearNode);
    if (textNode)
    {
      PRInt32 offset = 0;
      // put selection in right place:
      if (aAction == nsIEditor::ePrevious)
        textNode->GetLength((PRUint32*)&offset);
      res = aSelection->Collapse(nearNode,offset);
    }
    else  // must be break or image
    {
      selNode = nsEditor::GetNodeLocation(nearNode, &selOffset);
      if (aAction == nsIEditor::ePrevious) selOffset++;  // want to be beyond it if we backed up to it
      res = aSelection->Collapse(selNode, selOffset);
    }
  }
  return res;
}


nsresult
nsHTMLEditRules::FindNearSelectableNode(nsIDOMNode *aSelNode, 
                                        PRInt32 aSelOffset, 
                                        nsIEditor::EDirection &aDirection,
                                        nsCOMPtr<nsIDOMNode> *outSelectableNode)
{
  NS_ENSURE_TRUE(aSelNode && outSelectableNode, NS_ERROR_NULL_POINTER);
  *outSelectableNode = nsnull;
  nsresult res = NS_OK;
  
  nsCOMPtr<nsIDOMNode> nearNode, curNode;
  if (aDirection == nsIEditor::ePrevious)
    res = mHTMLEditor->GetPriorHTMLNode(aSelNode, aSelOffset, address_of(nearNode));
  else
    res = mHTMLEditor->GetNextHTMLNode(aSelNode, aSelOffset, address_of(nearNode));
  NS_ENSURE_SUCCESS(res, res);
  
  if (!nearNode) // try the other direction then
  {
    if (aDirection == nsIEditor::ePrevious)
      aDirection = nsIEditor::eNext;
    else
      aDirection = nsIEditor::ePrevious;
    
    if (aDirection == nsIEditor::ePrevious)
      res = mHTMLEditor->GetPriorHTMLNode(aSelNode, aSelOffset, address_of(nearNode));
    else
      res = mHTMLEditor->GetNextHTMLNode(aSelNode, aSelOffset, address_of(nearNode));
    NS_ENSURE_SUCCESS(res, res);
  }
  
  // scan in the right direction until we find an eligible text node,
  // but don't cross any breaks, images, or table elements.
  while (nearNode && !(mHTMLEditor->IsTextNode(nearNode)
                       || nsTextEditUtils::IsBreak(nearNode)
                       || nsHTMLEditUtils::IsImage(nearNode)))
  {
    curNode = nearNode;
    if (aDirection == nsIEditor::ePrevious)
      res = mHTMLEditor->GetPriorHTMLNode(curNode, address_of(nearNode));
    else
      res = mHTMLEditor->GetNextHTMLNode(curNode, address_of(nearNode));
    NS_ENSURE_SUCCESS(res, res);
  }
  
  if (nearNode)
  {
    // don't cross any table elements
    bool bInDifTblElems;
    res = InDifferentTableElements(nearNode, aSelNode, &bInDifTblElems);
    NS_ENSURE_SUCCESS(res, res);
    if (bInDifTblElems) return NS_OK;  
    
    // otherwise, ok, we have found a good spot to put the selection
    *outSelectableNode = do_QueryInterface(nearNode);
  }
  return res;
}


nsresult
nsHTMLEditRules::InDifferentTableElements(nsIDOMNode *aNode1, nsIDOMNode *aNode2, bool *aResult)
{
  NS_ASSERTION(aNode1 && aNode2 && aResult, "null args");
  NS_ENSURE_TRUE(aNode1 && aNode2 && aResult, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMNode> tn1, tn2, node = aNode1, temp;
  *aResult = false;
  
  while (node && !nsHTMLEditUtils::IsTableElement(node))
  {
    node->GetParentNode(getter_AddRefs(temp));
    node = temp;
  }
  tn1 = node;
  
  node = aNode2;
  while (node && !nsHTMLEditUtils::IsTableElement(node))
  {
    node->GetParentNode(getter_AddRefs(temp));
    node = temp;
  }
  tn2 = node;
  
  *aResult = (tn1 != tn2);
  
  return NS_OK;
}


nsresult 
nsHTMLEditRules::RemoveEmptyNodes()
{
  // some general notes on the algorithm used here: the goal is to examine all the
  // nodes in mDocChangeRange, and remove the empty ones.  We do this by using a
  // content iterator to traverse all the nodes in the range, and placing the empty
  // nodes into an array.  After finishing the iteration, we delete the empty nodes
  // in the array.  (they cannot be deleted as we find them becasue that would 
  // invalidate the iterator.)  
  // Since checking to see if a node is empty can be costly for nodes with many
  // descendants, there are some optimizations made.  I rely on the fact that the
  // iterator is post-order: it will visit children of a node before visiting the 
  // parent node.  So if I find that a child node is not empty, I know that its
  // parent is not empty without even checking.  So I put the parent on a "skipList"
  // which is just a voidArray of nodes I can skip the empty check on.  If I 
  // encounter a node on the skiplist, i skip the processing for that node and replace
  // its slot in the skiplist with that node's parent.
  // An interseting idea is to go ahead and regard parent nodes that are NOT on the
  // skiplist as being empty (without even doing the IsEmptyNode check) on the theory
  // that if they weren't empty, we would have encountered a non-empty child earlier
  // and thus put this parent node on the skiplist.
  // Unfortunately I can't use that strategy here, because the range may include 
  // some children of a node while excluding others.  Thus I could find all the 
  // _examined_ children empty, but still not have an empty parent.
  
  // need an iterator
  nsCOMPtr<nsIContentIterator> iter =
                  do_CreateInstance("@mozilla.org/content/post-content-iterator;1");
  NS_ENSURE_TRUE(iter, NS_ERROR_NULL_POINTER);
  
  nsresult res = iter->Init(mDocChangeRange);
  NS_ENSURE_SUCCESS(res, res);
  
  nsCOMArray<nsINode> arrayOfEmptyNodes, arrayOfEmptyCites;
  nsTArray<nsINode*> skipList;

  // check for empty nodes
  while (!iter->IsDone()) {
    nsINode* node = iter->GetCurrentNode();
    NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

    nsINode* parent = node->GetNodeParent();
    
    PRUint32 idx = skipList.IndexOf(node);
    if (idx != skipList.NoIndex) {
      // this node is on our skip list.  Skip processing for this node, 
      // and replace its value in the skip list with the value of its parent
      skipList[idx] = parent;
    } else {
      bool bIsCandidate = false;
      bool bIsEmptyNode = false;
      bool bIsMailCite = false;

      if (node->IsElement()) {
        dom::Element* element = node->AsElement();
        if (element->IsHTML(nsGkAtoms::body)) {
          // don't delete the body
        } else if ((bIsMailCite = nsHTMLEditUtils::IsMailCite(element))  ||
                   element->IsHTML(nsGkAtoms::a)                         ||
                   nsHTMLEditUtils::IsInlineStyle(element)               ||
                   nsHTMLEditUtils::IsList(element)                      ||
                   element->IsHTML(nsGkAtoms::div)) {
          // only consider certain nodes to be empty for purposes of removal
          bIsCandidate = true;
        } else if (nsHTMLEditUtils::IsFormatNode(element) ||
                   nsHTMLEditUtils::IsListItem(element)   ||
                   element->IsHTML(nsGkAtoms::blockquote)) {
          // these node types are candidates if selection is not in them
          // if it is one of these, don't delete if selection inside.
          // this is so we can create empty headings, etc, for the
          // user to type into.
          bool bIsSelInNode;
          res = SelectionEndpointInNode(node, &bIsSelInNode);
          NS_ENSURE_SUCCESS(res, res);
          if (!bIsSelInNode)
          {
            bIsCandidate = true;
          }
        }
      }
      
      if (bIsCandidate) {
        // we delete mailcites even if they have a solo br in them
        // other nodes we require to be empty
        res = mHTMLEditor->IsEmptyNode(node->AsDOMNode(), &bIsEmptyNode,
                                       bIsMailCite, true);
        NS_ENSURE_SUCCESS(res, res);
        if (bIsEmptyNode) {
          if (bIsMailCite) {
            // mailcites go on a separate list from other empty nodes
            arrayOfEmptyCites.AppendObject(node);
          } else {
            arrayOfEmptyNodes.AppendObject(node);
          }
        }
      }
      
      if (!bIsEmptyNode) {
        // put parent on skip list
        skipList.AppendElement(parent);
      }
    }

    iter->Next();
  }
  
  // now delete the empty nodes
  PRInt32 nodeCount = arrayOfEmptyNodes.Count();
  for (PRInt32 j = 0; j < nodeCount; j++) {
    nsCOMPtr<nsIDOMNode> delNode = arrayOfEmptyNodes[0]->AsDOMNode();
    arrayOfEmptyNodes.RemoveObjectAt(0);
    if (mHTMLEditor->IsModifiableNode(delNode)) {
      res = mHTMLEditor->DeleteNode(delNode);
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  
  // now delete the empty mailcites
  // this is a separate step because we want to pull out any br's and preserve them.
  nodeCount = arrayOfEmptyCites.Count();
  for (PRInt32 j = 0; j < nodeCount; j++) {
    nsCOMPtr<nsIDOMNode> delNode = arrayOfEmptyCites[0]->AsDOMNode();
    arrayOfEmptyCites.RemoveObjectAt(0);
    bool bIsEmptyNode;
    res = mHTMLEditor->IsEmptyNode(delNode, &bIsEmptyNode, false, true);
    NS_ENSURE_SUCCESS(res, res);
    if (!bIsEmptyNode)
    {
      // we are deleting a cite that has just a br.  We want to delete cite, 
      // but preserve br.
      nsCOMPtr<nsIDOMNode> parent, brNode;
      PRInt32 offset;
      parent = nsEditor::GetNodeLocation(delNode, &offset);
      res = mHTMLEditor->CreateBR(parent, offset, address_of(brNode));
      NS_ENSURE_SUCCESS(res, res);
    }
    res = mHTMLEditor->DeleteNode(delNode);
    NS_ENSURE_SUCCESS(res, res);
  }
  
  return res;
}

nsresult
nsHTMLEditRules::SelectionEndpointInNode(nsINode* aNode, bool* aResult)
{
  NS_ENSURE_TRUE(aNode && aResult, NS_ERROR_NULL_POINTER);

  nsIDOMNode* node = aNode->AsDOMNode();
  
  *aResult = false;
  
  nsCOMPtr<nsISelection>selection;
  nsresult res = mHTMLEditor->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  nsCOMPtr<nsISelectionPrivate>selPriv(do_QueryInterface(selection));
  
  nsCOMPtr<nsIEnumerator> enumerator;
  res = selPriv->GetEnumerator(getter_AddRefs(enumerator));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(enumerator, NS_ERROR_UNEXPECTED);

  for (enumerator->First(); NS_OK!=enumerator->IsDone(); enumerator->Next())
  {
    nsCOMPtr<nsISupports> currentItem;
    res = enumerator->CurrentItem(getter_AddRefs(currentItem));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(currentItem, NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
    nsCOMPtr<nsIDOMNode> startParent, endParent;
    range->GetStartContainer(getter_AddRefs(startParent));
    if (startParent)
    {
      if (node == startParent) {
        *aResult = true;
        return NS_OK;
      }
      if (nsEditorUtils::IsDescendantOf(startParent, node)) {
        *aResult = true;
        return NS_OK;
      }
    }
    range->GetEndContainer(getter_AddRefs(endParent));
    if (startParent == endParent) continue;
    if (endParent)
    {
      if (node == endParent) {
        *aResult = true;
        return NS_OK;
      }
      if (nsEditorUtils::IsDescendantOf(endParent, node)) {
        *aResult = true;
        return NS_OK;
      }
    }
  }
  return res;
}

///////////////////////////////////////////////////////////////////////////
// IsEmptyInline:  return true if aNode is an empty inline container
//                
//                  
bool 
nsHTMLEditRules::IsEmptyInline(nsIDOMNode *aNode)
{
  if (aNode && IsInlineNode(aNode) && mHTMLEditor->IsContainer(aNode)) 
  {
    bool bEmpty;
    mHTMLEditor->IsEmptyNode(aNode, &bEmpty);
    return bEmpty;
  }
  return false;
}


bool 
nsHTMLEditRules::ListIsEmptyLine(nsCOMArray<nsIDOMNode> &arrayOfNodes)
{
  // we have a list of nodes which we are candidates for being moved
  // into a new block.  Determine if it's anything more than a blank line.
  // Look for editable content above and beyond one single BR.
  PRInt32 listCount = arrayOfNodes.Count();
  NS_ENSURE_TRUE(listCount, true);
  nsCOMPtr<nsIDOMNode> somenode;
  PRInt32 j, brCount=0;
  for (j = 0; j < listCount; j++)
  {
    somenode = arrayOfNodes[j];
    if (somenode && mHTMLEditor->IsEditable(somenode))
    {
      if (nsTextEditUtils::IsBreak(somenode))
      {
        // first break doesn't count
        if (brCount) return false;
        brCount++;
      }
      else if (IsEmptyInline(somenode)) 
      {
        // empty inline, keep looking
      }
      else return false;
    }
  }
  return true;
}


nsresult 
nsHTMLEditRules::PopListItem(nsIDOMNode *aListItem, bool *aOutOfList)
{
  // check parms
  NS_ENSURE_TRUE(aListItem && aOutOfList, NS_ERROR_NULL_POINTER);
  
  // init out params
  *aOutOfList = false;
  
  nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(aListItem));
  PRInt32 offset;
  nsCOMPtr<nsIDOMNode> curParent = nsEditor::GetNodeLocation(curNode, &offset);
    
  if (!nsHTMLEditUtils::IsListItem(curNode))
    return NS_ERROR_FAILURE;
    
  // if it's first or last list item, don't need to split the list
  // otherwise we do.
  PRInt32 parOffset;
  nsCOMPtr<nsIDOMNode> curParPar = nsEditor::GetNodeLocation(curParent, &parOffset);
  
  bool bIsFirstListItem;
  nsresult res = mHTMLEditor->IsFirstEditableChild(curNode, &bIsFirstListItem);
  NS_ENSURE_SUCCESS(res, res);

  bool bIsLastListItem;
  res = mHTMLEditor->IsLastEditableChild(curNode, &bIsLastListItem);
  NS_ENSURE_SUCCESS(res, res);
    
  if (!bIsFirstListItem && !bIsLastListItem)
  {
    // split the list
    nsCOMPtr<nsIDOMNode> newBlock;
    res = mHTMLEditor->SplitNode(curParent, offset, getter_AddRefs(newBlock));
    NS_ENSURE_SUCCESS(res, res);
  }
  
  if (!bIsFirstListItem) parOffset++;
  
  res = mHTMLEditor->MoveNode(curNode, curParPar, parOffset);
  NS_ENSURE_SUCCESS(res, res);
    
  // unwrap list item contents if they are no longer in a list
  if (!nsHTMLEditUtils::IsList(curParPar)
      && nsHTMLEditUtils::IsListItem(curNode)) 
  {
    res = mHTMLEditor->RemoveBlockContainer(curNode);
    NS_ENSURE_SUCCESS(res, res);
    *aOutOfList = true;
  }
  return res;
}

nsresult
nsHTMLEditRules::RemoveListStructure(nsIDOMNode *aList)
{
  NS_ENSURE_ARG_POINTER(aList);

  nsresult res;

  nsCOMPtr<nsIDOMNode> child;
  aList->GetFirstChild(getter_AddRefs(child));

  while (child)
  {
    if (nsHTMLEditUtils::IsListItem(child))
    {
      bool bOutOfList;
      do
      {
        res = PopListItem(child, &bOutOfList);
        NS_ENSURE_SUCCESS(res, res);
      } while (!bOutOfList);   // keep popping it out until it's not in a list anymore
    }
    else if (nsHTMLEditUtils::IsList(child))
    {
      res = RemoveListStructure(child);
      NS_ENSURE_SUCCESS(res, res);
    }
    else
    {
      // delete any non- list items for now
      res = mHTMLEditor->DeleteNode(child);
      NS_ENSURE_SUCCESS(res, res);
    }
    aList->GetFirstChild(getter_AddRefs(child));
  }
  // delete the now-empty list
  res = mHTMLEditor->RemoveBlockContainer(aList);
  NS_ENSURE_SUCCESS(res, res);

  return res;
}


nsresult 
nsHTMLEditRules::ConfirmSelectionInBody()
{
  nsresult res = NS_OK;

  // get the body  
  nsCOMPtr<nsIDOMElement> rootElement = do_QueryInterface(mHTMLEditor->GetRoot());
  NS_ENSURE_TRUE(rootElement, NS_ERROR_UNEXPECTED);

  // get the selection
  nsCOMPtr<nsISelection>selection;
  res = mHTMLEditor->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  
  // get the selection start location
  nsCOMPtr<nsIDOMNode> selNode, temp, parent;
  PRInt32 selOffset;
  res = mHTMLEditor->GetStartNodeAndOffset(selection, getter_AddRefs(selNode), &selOffset);
  NS_ENSURE_SUCCESS(res, res);
  temp = selNode;
  
  // check that selNode is inside body
  while (temp && !nsTextEditUtils::IsBody(temp))
  {
    res = temp->GetParentNode(getter_AddRefs(parent));
    temp = parent;
  }
  
  // if we aren't in the body, force the issue
  if (!temp) 
  {
//    uncomment this to see when we get bad selections
//    NS_NOTREACHED("selection not in body");
    selection->Collapse(rootElement, 0);
  }
  
  // get the selection end location
  res = mHTMLEditor->GetEndNodeAndOffset(selection, getter_AddRefs(selNode), &selOffset);
  NS_ENSURE_SUCCESS(res, res);
  temp = selNode;
  
  // check that selNode is inside body
  while (temp && !nsTextEditUtils::IsBody(temp))
  {
    res = temp->GetParentNode(getter_AddRefs(parent));
    temp = parent;
  }
  
  // if we aren't in the body, force the issue
  if (!temp) 
  {
//    uncomment this to see when we get bad selections
//    NS_NOTREACHED("selection not in body");
    selection->Collapse(rootElement, 0);
  }
  
  return res;
}


nsresult 
nsHTMLEditRules::UpdateDocChangeRange(nsIDOMRange *aRange)
{
  nsresult res = NS_OK;

  // first make sure aRange is in the document.  It might not be if
  // portions of our editting action involved manipulating nodes
  // prior to placing them in the document (e.g., populating a list item
  // before placing it in its list)
  nsCOMPtr<nsIDOMNode> startNode;
  res = aRange->GetStartContainer(getter_AddRefs(startNode));
  NS_ENSURE_SUCCESS(res, res);
  if (!mHTMLEditor->IsDescendantOfRoot(startNode)) {
    // just return - we don't need to adjust mDocChangeRange in this case
    return NS_OK;
  }
  
  if (!mDocChangeRange)
  {
    // clone aRange.
    nsCOMPtr<nsIDOMRange> range;
    res = aRange->CloneRange(getter_AddRefs(range));
    mDocChangeRange = static_cast<nsRange*>(range.get());
  }
  else
  {
    PRInt16 result;
    
    // compare starts of ranges
    res = mDocChangeRange->CompareBoundaryPoints(nsIDOMRange::START_TO_START, aRange, &result);
    if (res == NS_ERROR_NOT_INITIALIZED) {
      // This will happen is mDocChangeRange is non-null, but the range is
      // uninitialized. In this case we'll set the start to aRange start.
      // The same test won't be needed further down since after we've set
      // the start the range will be collapsed to that point.
      result = 1;
      res = NS_OK;
    }
    NS_ENSURE_SUCCESS(res, res);
    if (result > 0)  // positive result means mDocChangeRange start is after aRange start
    {
      PRInt32 startOffset;
      res = aRange->GetStartOffset(&startOffset);
      NS_ENSURE_SUCCESS(res, res);
      res = mDocChangeRange->SetStart(startNode, startOffset);
      NS_ENSURE_SUCCESS(res, res);
    }
    
    // compare ends of ranges
    res = mDocChangeRange->CompareBoundaryPoints(nsIDOMRange::END_TO_END, aRange, &result);
    NS_ENSURE_SUCCESS(res, res);
    if (result < 0)  // negative result means mDocChangeRange end is before aRange end
    {
      nsCOMPtr<nsIDOMNode> endNode;
      PRInt32 endOffset;
      res = aRange->GetEndContainer(getter_AddRefs(endNode));
      NS_ENSURE_SUCCESS(res, res);
      res = aRange->GetEndOffset(&endOffset);
      NS_ENSURE_SUCCESS(res, res);
      res = mDocChangeRange->SetEnd(endNode, endOffset);
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  return res;
}

nsresult 
nsHTMLEditRules::InsertMozBRIfNeeded(nsIDOMNode *aNode)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);
  if (!IsBlockNode(aNode)) return NS_OK;
  
  bool isEmpty;
  nsresult res = mHTMLEditor->IsEmptyNode(aNode, &isEmpty);
  NS_ENSURE_SUCCESS(res, res);
  if (!isEmpty) {
    return NS_OK;
  }

  return CreateMozBR(aNode, 0);
}

NS_IMETHODIMP 
nsHTMLEditRules::WillCreateNode(const nsAString& aTag, nsIDOMNode *aParent, PRInt32 aPosition)
{
  return NS_OK;  
}

NS_IMETHODIMP 
nsHTMLEditRules::DidCreateNode(const nsAString& aTag, 
                               nsIDOMNode *aNode, 
                               nsIDOMNode *aParent, 
                               PRInt32 aPosition, 
                               nsresult aResult)
{
  if (!mListenerEnabled) {
    return NS_OK;
  }
  // assumption that Join keeps the righthand node
  nsresult res = mUtilRange->SelectNode(aNode);
  NS_ENSURE_SUCCESS(res, res);
  res = UpdateDocChangeRange(mUtilRange);
  return res;  
}


NS_IMETHODIMP 
nsHTMLEditRules::WillInsertNode(nsIDOMNode *aNode, nsIDOMNode *aParent, PRInt32 aPosition)
{
  return NS_OK;  
}


NS_IMETHODIMP 
nsHTMLEditRules::DidInsertNode(nsIDOMNode *aNode, 
                               nsIDOMNode *aParent, 
                               PRInt32 aPosition, 
                               nsresult aResult)
{
  if (!mListenerEnabled) {
    return NS_OK;
  }
  nsresult res = mUtilRange->SelectNode(aNode);
  NS_ENSURE_SUCCESS(res, res);
  res = UpdateDocChangeRange(mUtilRange);
  return res;  
}


NS_IMETHODIMP 
nsHTMLEditRules::WillDeleteNode(nsIDOMNode *aChild)
{
  if (!mListenerEnabled) {
    return NS_OK;
  }
  nsresult res = mUtilRange->SelectNode(aChild);
  NS_ENSURE_SUCCESS(res, res);
  res = UpdateDocChangeRange(mUtilRange);
  return res;  
}


NS_IMETHODIMP 
nsHTMLEditRules::DidDeleteNode(nsIDOMNode *aChild, nsresult aResult)
{
  return NS_OK;  
}


NS_IMETHODIMP 
nsHTMLEditRules::WillSplitNode(nsIDOMNode *aExistingRightNode, PRInt32 aOffset)
{
  return NS_OK;  
}


NS_IMETHODIMP 
nsHTMLEditRules::DidSplitNode(nsIDOMNode *aExistingRightNode, 
                              PRInt32 aOffset, 
                              nsIDOMNode *aNewLeftNode, 
                              nsresult aResult)
{
  if (!mListenerEnabled) {
    return NS_OK;
  }
  nsresult res = mUtilRange->SetStart(aNewLeftNode, 0);
  NS_ENSURE_SUCCESS(res, res);
  res = mUtilRange->SetEnd(aExistingRightNode, 0);
  NS_ENSURE_SUCCESS(res, res);
  res = UpdateDocChangeRange(mUtilRange);
  return res;  
}


NS_IMETHODIMP 
nsHTMLEditRules::WillJoinNodes(nsIDOMNode *aLeftNode, nsIDOMNode *aRightNode, nsIDOMNode *aParent)
{
  if (!mListenerEnabled) {
    return NS_OK;
  }
  // remember split point
  nsresult res = nsEditor::GetLengthOfDOMNode(aLeftNode, mJoinOffset);
  return res;  
}


NS_IMETHODIMP 
nsHTMLEditRules::DidJoinNodes(nsIDOMNode  *aLeftNode, 
                              nsIDOMNode *aRightNode, 
                              nsIDOMNode *aParent, 
                              nsresult aResult)
{
  if (!mListenerEnabled) {
    return NS_OK;
  }
  // assumption that Join keeps the righthand node
  nsresult res = mUtilRange->SetStart(aRightNode, mJoinOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = mUtilRange->SetEnd(aRightNode, mJoinOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = UpdateDocChangeRange(mUtilRange);
  return res;  
}


NS_IMETHODIMP 
nsHTMLEditRules::WillInsertText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, const nsAString &aString)
{
  return NS_OK;  
}


NS_IMETHODIMP 
nsHTMLEditRules::DidInsertText(nsIDOMCharacterData *aTextNode, 
                                  PRInt32 aOffset, 
                                  const nsAString &aString, 
                                  nsresult aResult)
{
  if (!mListenerEnabled) {
    return NS_OK;
  }
  PRInt32 length = aString.Length();
  nsCOMPtr<nsIDOMNode> theNode = do_QueryInterface(aTextNode);
  nsresult res = mUtilRange->SetStart(theNode, aOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = mUtilRange->SetEnd(theNode, aOffset+length);
  NS_ENSURE_SUCCESS(res, res);
  res = UpdateDocChangeRange(mUtilRange);
  return res;  
}


NS_IMETHODIMP 
nsHTMLEditRules::WillDeleteText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, PRInt32 aLength)
{
  return NS_OK;  
}


NS_IMETHODIMP 
nsHTMLEditRules::DidDeleteText(nsIDOMCharacterData *aTextNode, 
                                  PRInt32 aOffset, 
                                  PRInt32 aLength, 
                                  nsresult aResult)
{
  if (!mListenerEnabled) {
    return NS_OK;
  }
  nsCOMPtr<nsIDOMNode> theNode = do_QueryInterface(aTextNode);
  nsresult res = mUtilRange->SetStart(theNode, aOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = mUtilRange->SetEnd(theNode, aOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = UpdateDocChangeRange(mUtilRange);
  return res;  
}

NS_IMETHODIMP
nsHTMLEditRules::WillDeleteSelection(nsISelection *aSelection)
{
  if (!mListenerEnabled) {
    return NS_OK;
  }
  // get the (collapsed) selection location
  nsCOMPtr<nsIDOMNode> selNode;
  PRInt32 selOffset;

  nsresult res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(selNode), &selOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = mUtilRange->SetStart(selNode, selOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = mHTMLEditor->GetEndNodeAndOffset(aSelection, getter_AddRefs(selNode), &selOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = mUtilRange->SetEnd(selNode, selOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = UpdateDocChangeRange(mUtilRange);
  return res;  
}

NS_IMETHODIMP
nsHTMLEditRules::DidDeleteSelection(nsISelection *aSelection)
{
  return NS_OK;
}

// Let's remove all alignment hints in the children of aNode; it can
// be an ALIGN attribute (in case we just remove it) or a CENTER
// element (here we have to remove the container and keep its
// children). We break on tables and don't look at their children.
nsresult
nsHTMLEditRules::RemoveAlignment(nsIDOMNode * aNode, const nsAString & aAlignType, bool aChildrenOnly)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);

  if (mHTMLEditor->IsTextNode(aNode) || nsHTMLEditUtils::IsTable(aNode)) return NS_OK;
  nsresult res = NS_OK;

  nsCOMPtr<nsIDOMNode> child = aNode,tmp;
  if (aChildrenOnly)
  {
    aNode->GetFirstChild(getter_AddRefs(child));
  }
  bool useCSS = mHTMLEditor->IsCSSEnabled();

  while (child)
  {
    if (aChildrenOnly) {
      // get the next sibling right now because we could have to remove child
      child->GetNextSibling(getter_AddRefs(tmp));
    }
    else
    {
      tmp = nsnull;
    }
    bool isBlock;
    res = mHTMLEditor->NodeIsBlockStatic(child, &isBlock);
    NS_ENSURE_SUCCESS(res, res);

    if (nsEditor::NodeIsType(child, nsEditProperty::center))
    {
      // the current node is a CENTER element
      // first remove children's alignment
      res = RemoveAlignment(child, aAlignType, true);
      NS_ENSURE_SUCCESS(res, res);

      // we may have to insert BRs in first and last position of element's children
      // if the nodes before/after are not blocks and not BRs
      res = MakeSureElemStartsOrEndsOnCR(child);
      NS_ENSURE_SUCCESS(res, res);

      // now remove the CENTER container
      res = mHTMLEditor->RemoveContainer(child);
      NS_ENSURE_SUCCESS(res, res);
    }
    else if (isBlock || nsHTMLEditUtils::IsHR(child))
    {
      // the current node is a block element
      nsCOMPtr<nsIDOMElement> curElem = do_QueryInterface(child);
      if (nsHTMLEditUtils::SupportsAlignAttr(child))
      {
        // remove the ALIGN attribute if this element can have it
        res = mHTMLEditor->RemoveAttribute(curElem, NS_LITERAL_STRING("align"));
        NS_ENSURE_SUCCESS(res, res);
      }
      if (useCSS)
      {
        if (nsHTMLEditUtils::IsTable(child) || nsHTMLEditUtils::IsHR(child))
        {
          res = mHTMLEditor->SetAttributeOrEquivalent(curElem, NS_LITERAL_STRING("align"), aAlignType, false); 
        }
        else
        {
          nsAutoString dummyCssValue;
          res = mHTMLEditor->mHTMLCSSUtils->RemoveCSSInlineStyle(child, nsEditProperty::cssTextAlign, dummyCssValue);
        }
        NS_ENSURE_SUCCESS(res, res);
      }
      if (!nsHTMLEditUtils::IsTable(child))
      {
        // unless this is a table, look at children
        res = RemoveAlignment(child, aAlignType, true);
        NS_ENSURE_SUCCESS(res, res);
      }
    }
    child = tmp;
  }
  return NS_OK;
}

// Let's insert a BR as first (resp. last) child of aNode if its
// first (resp. last) child is not a block nor a BR, and if the
// previous (resp. next) sibling is not a block nor a BR
nsresult
nsHTMLEditRules::MakeSureElemStartsOrEndsOnCR(nsIDOMNode *aNode, bool aStarts)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMNode> child;
  nsresult res;
  if (aStarts)
  {
    res = mHTMLEditor->GetFirstEditableChild(aNode, address_of(child));
  }
  else
  {
    res = mHTMLEditor->GetLastEditableChild(aNode, address_of(child));
  }
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(child, NS_OK);
  bool isChildBlock;
  res = mHTMLEditor->NodeIsBlockStatic(child, &isChildBlock);
  NS_ENSURE_SUCCESS(res, res);
  bool foundCR = false;
  if (isChildBlock || nsTextEditUtils::IsBreak(child))
  {
    foundCR = true;
  }
  else
  {
    nsCOMPtr<nsIDOMNode> sibling;
    if (aStarts)
    {
      res = mHTMLEditor->GetPriorHTMLSibling(aNode, address_of(sibling));
    }
    else
    {
      res = mHTMLEditor->GetNextHTMLSibling(aNode, address_of(sibling));
    }
    NS_ENSURE_SUCCESS(res, res);
    if (sibling)
    {
      bool isBlock;
      res = mHTMLEditor->NodeIsBlockStatic(sibling, &isBlock);
      NS_ENSURE_SUCCESS(res, res);
      if (isBlock || nsTextEditUtils::IsBreak(sibling))
      {
        foundCR = true;
      }
    }
    else
    {
      foundCR = true;
    }
  }
  if (!foundCR)
  {
    PRInt32 offset = 0;
    if (!aStarts) {
      nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
      NS_ENSURE_STATE(node);
      offset = node->GetChildCount();
    }
    nsCOMPtr<nsIDOMNode> brNode;
    res = mHTMLEditor->CreateBR(aNode, offset, address_of(brNode));
    NS_ENSURE_SUCCESS(res, res);
  }
  return NS_OK;
}

nsresult
nsHTMLEditRules::MakeSureElemStartsOrEndsOnCR(nsIDOMNode *aNode)
{
  nsresult res = MakeSureElemStartsOrEndsOnCR(aNode, false);
  NS_ENSURE_SUCCESS(res, res);
  res = MakeSureElemStartsOrEndsOnCR(aNode, true);
  return res;
}

nsresult
nsHTMLEditRules::AlignBlock(nsIDOMElement * aElement, const nsAString * aAlignType, bool aContentsOnly)
{
  NS_ENSURE_TRUE(aElement, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aElement);
  bool isBlock = IsBlockNode(node);
  if (!isBlock && !nsHTMLEditUtils::IsHR(node)) {
    // we deal only with blocks; early way out
    return NS_OK;
  }

  nsresult res = RemoveAlignment(node, *aAlignType, aContentsOnly);
  NS_ENSURE_SUCCESS(res, res);
  NS_NAMED_LITERAL_STRING(attr, "align");
  if (mHTMLEditor->IsCSSEnabled()) {
    // let's use CSS alignment; we use margin-left and margin-right for tables
    // and text-align for other block-level elements
    res = mHTMLEditor->SetAttributeOrEquivalent(aElement, attr, *aAlignType, false); 
    NS_ENSURE_SUCCESS(res, res);
  }
  else {
    // HTML case; this code is supposed to be called ONLY if the element
    // supports the align attribute but we'll never know...
    if (nsHTMLEditUtils::SupportsAlignAttr(node)) {
      res = mHTMLEditor->SetAttribute(aElement, attr, *aAlignType);
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  return NS_OK;
}

nsresult
nsHTMLEditRules::RelativeChangeIndentationOfElementNode(nsIDOMNode *aNode, PRInt8 aRelativeChange)
{
  NS_ENSURE_ARG_POINTER(aNode);

  if (aRelativeChange != 1 && aRelativeChange != -1) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aNode);
  if (!element) {
    return NS_OK;
  }

  nsIAtom* marginProperty = MarginPropertyAtomForIndent(mHTMLEditor->mHTMLCSSUtils, element);    
  nsAutoString value;
  mHTMLEditor->mHTMLCSSUtils->GetSpecifiedProperty(aNode, marginProperty, value);
  float f;
  nsCOMPtr<nsIAtom> unit;
  mHTMLEditor->mHTMLCSSUtils->ParseLength(value, &f, getter_AddRefs(unit));
  if (0 == f) {
    nsAutoString defaultLengthUnit;
    mHTMLEditor->mHTMLCSSUtils->GetDefaultLengthUnit(defaultLengthUnit);
    unit = do_GetAtom(defaultLengthUnit);
  }
  if      (nsEditProperty::cssInUnit == unit)
            f += NS_EDITOR_INDENT_INCREMENT_IN * aRelativeChange;
  else if (nsEditProperty::cssCmUnit == unit)
            f += NS_EDITOR_INDENT_INCREMENT_CM * aRelativeChange;
  else if (nsEditProperty::cssMmUnit == unit)
            f += NS_EDITOR_INDENT_INCREMENT_MM * aRelativeChange;
  else if (nsEditProperty::cssPtUnit == unit)
            f += NS_EDITOR_INDENT_INCREMENT_PT * aRelativeChange;
  else if (nsEditProperty::cssPcUnit == unit)
            f += NS_EDITOR_INDENT_INCREMENT_PC * aRelativeChange;
  else if (nsEditProperty::cssEmUnit == unit)
            f += NS_EDITOR_INDENT_INCREMENT_EM * aRelativeChange;
  else if (nsEditProperty::cssExUnit == unit)
            f += NS_EDITOR_INDENT_INCREMENT_EX * aRelativeChange;
  else if (nsEditProperty::cssPxUnit == unit)
            f += NS_EDITOR_INDENT_INCREMENT_PX * aRelativeChange;
  else if (nsEditProperty::cssPercentUnit == unit)
            f += NS_EDITOR_INDENT_INCREMENT_PERCENT * aRelativeChange;    

  if (0 < f) {
    nsAutoString newValue;
    newValue.AppendFloat(f);
    newValue.Append(nsDependentAtomString(unit));
    mHTMLEditor->mHTMLCSSUtils->SetCSSProperty(element, marginProperty, newValue, false);
    return NS_OK;
  }

  mHTMLEditor->mHTMLCSSUtils->RemoveCSSProperty(element, marginProperty, value, false);

  // remove unnecessary DIV blocks:
  // we could skip this section but that would cause a FAIL in
  // editor/libeditor/html/tests/browserscope/richtext.html, which expects
  // to unapply a CSS "indent" (<div style="margin-left: 40px;">) by
  // removing the DIV container instead of just removing the CSS property.
  nsCOMPtr<dom::Element> node = do_QueryInterface(aNode);
  if (!node || !node->IsHTML(nsGkAtoms::div) ||
      node == mHTMLEditor->GetActiveEditingHost() ||
      !mHTMLEditor->IsDescendantOfEditorRoot(node) ||
      nsHTMLEditor::HasAttributes(node)) {
    return NS_OK;
  }

  return mHTMLEditor->RemoveContainer(element);
}

//
// Support for Absolute Positioning
//

nsresult
nsHTMLEditRules::WillAbsolutePosition(nsISelection *aSelection, bool *aCancel, bool * aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  nsresult res = WillInsert(aSelection, aCancel);
  NS_ENSURE_SUCCESS(res, res);

  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = false;
  *aHandled = true;
  
  nsCOMPtr<nsIDOMElement> focusElement;
  res = mHTMLEditor->GetSelectionContainer(getter_AddRefs(focusElement));
  if (focusElement) {
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(focusElement);
    if (nsHTMLEditUtils::IsImage(node)) {
      mNewBlock = node;
      return NS_OK;
    }
  }

  res = NormalizeSelection(aSelection);
  NS_ENSURE_SUCCESS(res, res);
  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);
  
  // convert the selection ranges into "promoted" selection ranges:
  // this basically just expands the range to include the immediate
  // block parent, and then further expands to include any ancestors
  // whose children are all in the range
  
  nsCOMArray<nsIDOMRange> arrayOfRanges;
  res = GetPromotedRanges(aSelection, arrayOfRanges,
                          nsEditor::kOpSetAbsolutePosition);
  NS_ENSURE_SUCCESS(res, res);
  
  // use these ranges to contruct a list of nodes to act on.
  nsCOMArray<nsIDOMNode> arrayOfNodes;
  res = GetNodesForOperation(arrayOfRanges, arrayOfNodes,
                             nsEditor::kOpSetAbsolutePosition);
  NS_ENSURE_SUCCESS(res, res);                                 
                                     
  NS_NAMED_LITERAL_STRING(divType, "div");


  // if nothing visible in list, make an empty block
  if (ListIsEmptyLine(arrayOfNodes))
  {
    nsCOMPtr<nsIDOMNode> parent, thePositionedDiv;
    PRInt32 offset;
    
    // get selection location
    res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(parent), &offset);
    NS_ENSURE_SUCCESS(res, res);
    // make sure we can put a block here
    res = SplitAsNeeded(&divType, address_of(parent), &offset);
    NS_ENSURE_SUCCESS(res, res);
    res = mHTMLEditor->CreateNode(divType, parent, offset, getter_AddRefs(thePositionedDiv));
    NS_ENSURE_SUCCESS(res, res);
    // remember our new block for postprocessing
    mNewBlock = thePositionedDiv;
    // delete anything that was in the list of nodes
    for (PRInt32 j = arrayOfNodes.Count() - 1; j >= 0; --j) 
    {
      nsCOMPtr<nsIDOMNode> curNode = arrayOfNodes[0];
      res = mHTMLEditor->DeleteNode(curNode);
      NS_ENSURE_SUCCESS(res, res);
      res = arrayOfNodes.RemoveObjectAt(0);
      NS_ENSURE_SUCCESS(res, res);
    }
    // put selection in new block
    res = aSelection->Collapse(thePositionedDiv,0);
    selectionResetter.Abort();  // to prevent selection reseter from overriding us.
    *aHandled = true;
    return res;
  }

  // Ok, now go through all the nodes and put them in a blockquote, 
  // or whatever is appropriate.  Wohoo!
  PRInt32 i;
  nsCOMPtr<nsIDOMNode> curParent, curPositionedDiv, curList, indentedLI, sibling;
  PRInt32 listCount = arrayOfNodes.Count();
  for (i=0; i<listCount; i++)
  {
    // here's where we actually figure out what to do
    nsCOMPtr<nsIDOMNode> curNode = arrayOfNodes[i];

    // Ignore all non-editable nodes.  Leave them be.
    if (!mHTMLEditor->IsEditable(curNode)) continue;

    PRInt32 offset;
    curParent = nsEditor::GetNodeLocation(curNode, &offset);
     
    // some logic for putting list items into nested lists...
    if (nsHTMLEditUtils::IsList(curParent))
    {
      // check to see if curList is still appropriate.  Which it is if
      // curNode is still right after it in the same list.
      if (curList)
      {
        sibling = nsnull;
        mHTMLEditor->GetPriorHTMLSibling(curNode, address_of(sibling));
      }
      
      if (!curList || (sibling && sibling != curList) )
      {
        nsAutoString listTag;
        nsEditor::GetTagString(curParent,listTag);
        ToLowerCase(listTag);
        // create a new nested list of correct type
        res = SplitAsNeeded(&listTag, address_of(curParent), &offset);
        NS_ENSURE_SUCCESS(res, res);
        if (!curPositionedDiv) {
          PRInt32 parentOffset;
          nsCOMPtr<nsIDOMNode> curParentParent = nsEditor::GetNodeLocation(curParent, &parentOffset);
          res = mHTMLEditor->CreateNode(divType, curParentParent, parentOffset, getter_AddRefs(curPositionedDiv));
          mNewBlock = curPositionedDiv;
        }
        res = mHTMLEditor->CreateNode(listTag, curPositionedDiv, -1, getter_AddRefs(curList));
        NS_ENSURE_SUCCESS(res, res);
        // curList is now the correct thing to put curNode in
        // remember our new block for postprocessing
        // mNewBlock = curList;
      }
      // tuck the node into the end of the active list
      res = mHTMLEditor->MoveNode(curNode, curList, -1);
      NS_ENSURE_SUCCESS(res, res);
      // forget curPositionedDiv, if any
      // curPositionedDiv = nsnull;
    }
    
    else // not a list item, use blockquote?
    {
      // if we are inside a list item, we don't want to blockquote, we want
      // to sublist the list item.  We may have several nodes listed in the
      // array of nodes to act on, that are in the same list item.  Since
      // we only want to indent that li once, we must keep track of the most
      // recent indented list item, and not indent it if we find another node
      // to act on that is still inside the same li.
      nsCOMPtr<nsIDOMNode> listitem=IsInListItem(curNode);
      if (listitem)
      {
        if (indentedLI == listitem) continue;  // already indented this list item
        curParent = nsEditor::GetNodeLocation(listitem, &offset);
        // check to see if curList is still appropriate.  Which it is if
        // curNode is still right after it in the same list.
        if (curList)
        {
          sibling = nsnull;
          mHTMLEditor->GetPriorHTMLSibling(curNode, address_of(sibling));
        }
         
        if (!curList || (sibling && sibling != curList) )
        {
          nsAutoString listTag;
          nsEditor::GetTagString(curParent,listTag);
          ToLowerCase(listTag);
          // create a new nested list of correct type
          res = SplitAsNeeded(&listTag, address_of(curParent), &offset);
          NS_ENSURE_SUCCESS(res, res);
          if (!curPositionedDiv) {
          PRInt32 parentOffset;
          nsCOMPtr<nsIDOMNode> curParentParent = nsEditor::GetNodeLocation(curParent, &parentOffset);
          res = mHTMLEditor->CreateNode(divType, curParentParent, parentOffset, getter_AddRefs(curPositionedDiv));
            mNewBlock = curPositionedDiv;
          }
          res = mHTMLEditor->CreateNode(listTag, curPositionedDiv, -1, getter_AddRefs(curList));
          NS_ENSURE_SUCCESS(res, res);
        }
        res = mHTMLEditor->MoveNode(listitem, curList, -1);
        NS_ENSURE_SUCCESS(res, res);
        // remember we indented this li
        indentedLI = listitem;
      }
      
      else
      {
        // need to make a div to put things in if we haven't already

        if (!curPositionedDiv) 
        {
          if (nsHTMLEditUtils::IsDiv(curNode))
          {
            curPositionedDiv = curNode;
            mNewBlock = curPositionedDiv;
            curList = nsnull;
            continue;
          }
          res = SplitAsNeeded(&divType, address_of(curParent), &offset);
          NS_ENSURE_SUCCESS(res, res);
          res = mHTMLEditor->CreateNode(divType, curParent, offset, getter_AddRefs(curPositionedDiv));
          NS_ENSURE_SUCCESS(res, res);
          // remember our new block for postprocessing
          mNewBlock = curPositionedDiv;
          // curPositionedDiv is now the correct thing to put curNode in
        }
          
        // tuck the node into the end of the active blockquote
        res = mHTMLEditor->MoveNode(curNode, curPositionedDiv, -1);
        NS_ENSURE_SUCCESS(res, res);
        // forget curList, if any
        curList = nsnull;
      }
    }
  }
  return res;
}

nsresult
nsHTMLEditRules::DidAbsolutePosition()
{
  nsCOMPtr<nsIHTMLAbsPosEditor> absPosHTMLEditor = mHTMLEditor;
  nsCOMPtr<nsIDOMElement> elt = do_QueryInterface(mNewBlock);
  return absPosHTMLEditor->AbsolutelyPositionElement(elt, true);
}

nsresult
nsHTMLEditRules::WillRemoveAbsolutePosition(nsISelection *aSelection, bool *aCancel, bool * aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  nsresult res = WillInsert(aSelection, aCancel);
  NS_ENSURE_SUCCESS(res, res);

  // initialize out param
  // we want to ignore aCancel from WillInsert()
  *aCancel = false;
  *aHandled = true;

  nsCOMPtr<nsIDOMElement>  elt;
  res = mHTMLEditor->GetAbsolutelyPositionedSelectionContainer(getter_AddRefs(elt));
  NS_ENSURE_SUCCESS(res, res);

  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);

  nsCOMPtr<nsIHTMLAbsPosEditor> absPosHTMLEditor = mHTMLEditor;
  return absPosHTMLEditor->AbsolutelyPositionElement(elt, false);
}

nsresult
nsHTMLEditRules::WillRelativeChangeZIndex(nsISelection *aSelection,
                                          PRInt32 aChange,
                                          bool *aCancel,
                                          bool * aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  nsresult res = WillInsert(aSelection, aCancel);
  NS_ENSURE_SUCCESS(res, res);

  // initialize out param
  // we want to ignore aCancel from WillInsert()
  *aCancel = false;
  *aHandled = true;

  nsCOMPtr<nsIDOMElement>  elt;
  res = mHTMLEditor->GetAbsolutelyPositionedSelectionContainer(getter_AddRefs(elt));
  NS_ENSURE_SUCCESS(res, res);

  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);

  nsCOMPtr<nsIHTMLAbsPosEditor> absPosHTMLEditor = mHTMLEditor;
  PRInt32 zIndex;
  return absPosHTMLEditor->RelativeChangeElementZIndex(elt, aChange, &zIndex);
}

NS_IMETHODIMP
nsHTMLEditRules::DocumentModified()
{
  nsContentUtils::AddScriptRunner(NS_NewRunnableMethod(this, &nsHTMLEditRules::DocumentModifiedWorker));
  return NS_OK;
}

void
nsHTMLEditRules::DocumentModifiedWorker()
{
  if (!mHTMLEditor) {
    return;
  }

  // DeleteNode below may cause a flush, which could destroy the editor
  nsAutoScriptBlockerSuppressNodeRemoved scriptBlocker;

  nsCOMPtr<nsIHTMLEditor> kungFuDeathGrip(mHTMLEditor);
  nsCOMPtr<nsISelection> selection;
  nsresult rv = mHTMLEditor->GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(rv)) {
    return;
  }

  // Delete our bogus node, if we have one, since the document might not be
  // empty any more.
  if (mBogusNode) {
    mEditor->DeleteNode(mBogusNode);
    mBogusNode = nsnull;
  }

  // Try to recreate the bogus node if needed.
  CreateBogusNodeIfNeeded(selection);
}
