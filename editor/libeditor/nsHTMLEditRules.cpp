/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHTMLEditRules.h"

#include <stdlib.h>

#include "mozilla/Assertions.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/Selection.h"
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
#include "nsEditor.h"
#include "nsEditorUtils.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsHTMLCSSUtils.h"
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
#include "nsIDOMText.h"
#include "nsIHTMLAbsPosEditor.h"
#include "nsIHTMLDocument.h"
#include "nsINode.h"
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
#include <algorithm>

class nsISupports;
class nsRulesInfo;

using namespace mozilla;
using namespace mozilla::dom;

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
IsStyleCachePreservingAction(EditAction action)
{
  return action == EditAction::deleteSelection ||
         action == EditAction::insertBreak ||
         action == EditAction::makeList ||
         action == EditAction::indent ||
         action == EditAction::outdent ||
         action == EditAction::align ||
         action == EditAction::makeBasicBlock ||
         action == EditAction::removeList ||
         action == EditAction::makeDefListItem ||
         action == EditAction::insertElement ||
         action == EditAction::insertQuotation;
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
    explicit nsEmptyEditableFunctor(nsHTMLEditor* editor) : mHTMLEditor(editor) {}
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
    explicit nsEditableTextFunctor(nsHTMLEditor* editor) : mHTMLEditor(editor) {}
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

nsHTMLEditRules::nsHTMLEditRules()
{
  InitFields();
}

void
nsHTMLEditRules::InitFields()
{
  mHTMLEditor = nullptr;
  mDocChangeRange = nullptr;
  mListenerEnabled = true;
  mReturnInEmptyLIKillsList = true;
  mDidDeleteSelection = false;
  mDidRangedDelete = false;
  mRestoreContentEditableCount = false;
  mUtilRange = nullptr;
  mJoinOffset = 0;
  mNewBlock = nullptr;
  mRangeItem = new nsRangeStore();
  // populate mCachedStyles
  mCachedStyles[0] = StyleCache(nsGkAtoms::b, EmptyString(), EmptyString());
  mCachedStyles[1] = StyleCache(nsGkAtoms::i, EmptyString(), EmptyString());
  mCachedStyles[2] = StyleCache(nsGkAtoms::u, EmptyString(), EmptyString());
  mCachedStyles[3] = StyleCache(nsGkAtoms::font, NS_LITERAL_STRING("face"), EmptyString());
  mCachedStyles[4] = StyleCache(nsGkAtoms::font, NS_LITERAL_STRING("size"), EmptyString());
  mCachedStyles[5] = StyleCache(nsGkAtoms::font, NS_LITERAL_STRING("color"), EmptyString());
  mCachedStyles[6] = StyleCache(nsGkAtoms::tt, EmptyString(), EmptyString());
  mCachedStyles[7] = StyleCache(nsGkAtoms::em, EmptyString(), EmptyString());
  mCachedStyles[8] = StyleCache(nsGkAtoms::strong, EmptyString(), EmptyString());
  mCachedStyles[9] = StyleCache(nsGkAtoms::dfn, EmptyString(), EmptyString());
  mCachedStyles[10] = StyleCache(nsGkAtoms::code, EmptyString(), EmptyString());
  mCachedStyles[11] = StyleCache(nsGkAtoms::samp, EmptyString(), EmptyString());
  mCachedStyles[12] = StyleCache(nsGkAtoms::var, EmptyString(), EmptyString());
  mCachedStyles[13] = StyleCache(nsGkAtoms::cite, EmptyString(), EmptyString());
  mCachedStyles[14] = StyleCache(nsGkAtoms::abbr, EmptyString(), EmptyString());
  mCachedStyles[15] = StyleCache(nsGkAtoms::acronym, EmptyString(), EmptyString());
  mCachedStyles[16] = StyleCache(nsGkAtoms::backgroundColor, EmptyString(), EmptyString());
  mCachedStyles[17] = StyleCache(nsGkAtoms::sub, EmptyString(), EmptyString());
  mCachedStyles[18] = StyleCache(nsGkAtoms::sup, EmptyString(), EmptyString());
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
NS_IMPL_QUERY_INTERFACE_INHERITED(nsHTMLEditRules, nsTextEditRules, nsIEditActionListener)


/********************************************************
 *  Public methods 
 ********************************************************/

NS_IMETHODIMP
nsHTMLEditRules::Init(nsPlaintextEditor *aEditor)
{
  InitFields();

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
  nsCOMPtr<nsINode> node = mHTMLEditor->GetRoot();
  if (!node) {
    node = mHTMLEditor->GetDocument();
  }

  NS_ENSURE_STATE(node);

  mUtilRange = new nsRange(node);
   
  // set up mDocChangeRange to be whole doc
  // temporarily turn off rules sniffing
  nsAutoLockRulesSniffing lockIt((nsTextEditRules*)this);
  if (!mDocChangeRange) {
    mDocChangeRange = new nsRange(node);
  }

  if (node->IsElement()) {
    ErrorResult rv;
    mDocChangeRange->SelectNode(*node, rv);
    res = AdjustSpecialBreaks(node);
    NS_ENSURE_SUCCESS(res, res);
  }

  // add ourselves as a listener to edit actions
  res = mHTMLEditor->AddEditActionListener(this);

  return res;
}

NS_IMETHODIMP
nsHTMLEditRules::DetachEditor()
{
  if (mHTMLEditor) {
    mHTMLEditor->RemoveEditActionListener(this);
  }
  mHTMLEditor = nullptr;
  return nsTextEditRules::DetachEditor();
}

NS_IMETHODIMP
nsHTMLEditRules::BeforeEdit(EditAction action,
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
    NS_ENSURE_STATE(mHTMLEditor);
    nsRefPtr<Selection> selection = mHTMLEditor->GetSelection();

    // get the selection location
    NS_ENSURE_STATE(selection->RangeCount());
    mRangeItem->startNode = selection->GetRangeAt(0)->GetStartParent();
    mRangeItem->startOffset = selection->GetRangeAt(0)->StartOffset();
    mRangeItem->endNode = selection->GetRangeAt(0)->GetEndParent();
    mRangeItem->endOffset = selection->GetRangeAt(0)->EndOffset();
    nsCOMPtr<nsIDOMNode> selStartNode = GetAsDOMNode(mRangeItem->startNode);
    nsCOMPtr<nsIDOMNode> selEndNode = GetAsDOMNode(mRangeItem->endNode);

    // register this range with range updater to track this as we perturb the doc
    NS_ENSURE_STATE(mHTMLEditor);
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
    if (action == EditAction::insertText ||
        action == EditAction::insertIMEText ||
        action == EditAction::deleteSelection ||
        IsStyleCachePreservingAction(action)) {
      nsCOMPtr<nsIDOMNode> selNode = selStartNode;
      if (aDirection == nsIEditor::eNext)
        selNode = selEndNode;
      nsresult res = CacheInlineStyles(selNode);
      NS_ENSURE_SUCCESS(res, res);
    }

    // Stabilize the document against contenteditable count changes
    NS_ENSURE_STATE(mHTMLEditor);
    nsCOMPtr<nsIDOMDocument> doc = mHTMLEditor->GetDOMDocument();
    NS_ENSURE_TRUE(doc, NS_ERROR_NOT_INITIALIZED);
    nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(doc);
    NS_ENSURE_TRUE(htmlDoc, NS_ERROR_FAILURE);
    if (htmlDoc->GetEditingState() == nsIHTMLDocument::eContentEditable) {
      htmlDoc->ChangeContentEditableCount(nullptr, +1);
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
nsHTMLEditRules::AfterEdit(EditAction action,
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
    NS_ENSURE_STATE(mHTMLEditor);
    (mHTMLEditor->mRangeUpdater).DropRangeItem(mRangeItem);

    // Reset the contenteditable count to its previous value
    if (mRestoreContentEditableCount) {
      NS_ENSURE_STATE(mHTMLEditor);
      nsCOMPtr<nsIDOMDocument> doc = mHTMLEditor->GetDOMDocument();
      NS_ENSURE_TRUE(doc, NS_ERROR_NOT_INITIALIZED);
      nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(doc);
      NS_ENSURE_TRUE(htmlDoc, NS_ERROR_FAILURE);
      if (htmlDoc->GetEditingState() == nsIHTMLDocument::eContentEditable) {
        htmlDoc->ChangeContentEditableCount(nullptr, -1);
      }
      mRestoreContentEditableCount = false;
    }
  }

  return res;
}


nsresult
nsHTMLEditRules::AfterEditInner(EditAction action,
                                nsIEditor::EDirection aDirection)
{
  ConfirmSelectionInBody();
  if (action == EditAction::ignore) return NS_OK;
  
  NS_ENSURE_STATE(mHTMLEditor);
  nsRefPtr<Selection> selection = mHTMLEditor->GetSelection();
  NS_ENSURE_STATE(selection);
  
  nsCOMPtr<nsIDOMNode> rangeStartParent, rangeEndParent;
  int32_t rangeStartOffset = 0, rangeEndOffset = 0;
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
  
  nsresult res;
  if (bDamagedRange && !((action == EditAction::undo) || (action == EditAction::redo)))
  {
    // don't let any txns in here move the selection around behind our back.
    // Note that this won't prevent explicit selection setting from working.
    NS_ENSURE_STATE(mHTMLEditor);
    nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
   
    // expand the "changed doc range" as needed
    res = PromoteRange(mDocChangeRange, action);
    NS_ENSURE_SUCCESS(res, res);

    // if we did a ranged deletion or handling backspace key, make sure we have
    // a place to put caret.
    // Note we only want to do this if the overall operation was deletion,
    // not if deletion was done along the way for EditAction::loadHTML, EditAction::insertText, etc.
    // That's why this is here rather than DidDeleteSelection().
    if ((action == EditAction::deleteSelection) && mDidRangedDelete)
    {
      res = InsertBRIfNeeded(selection);
      NS_ENSURE_SUCCESS(res, res);
    }  
    
    // add in any needed <br>s, and remove any unneeded ones.
    res = AdjustSpecialBreaks();
    NS_ENSURE_SUCCESS(res, res);
    
    // merge any adjacent text nodes
    if ( (action != EditAction::insertText &&
         action != EditAction::insertIMEText) )
    {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->CollapseAdjacentTextNodes(mDocChangeRange);
      NS_ENSURE_SUCCESS(res, res);
    }

    // clean up any empty nodes in the selection
    res = RemoveEmptyNodes();
    NS_ENSURE_SUCCESS(res, res);

    // attempt to transform any unneeded nbsp's into spaces after doing various operations
    if ((action == EditAction::insertText) || 
        (action == EditAction::insertIMEText) ||
        (action == EditAction::deleteSelection) ||
        (action == EditAction::insertBreak) || 
        (action == EditAction::htmlPaste ||
        (action == EditAction::loadHTML)))
    {
      res = AdjustWhitespace(selection);
      NS_ENSURE_SUCCESS(res, res);
      
      // also do this for original selection endpoints. 
      NS_ENSURE_STATE(mHTMLEditor);
      nsWSRunObject(mHTMLEditor, mRangeItem->startNode,
                    mRangeItem->startOffset).AdjustWhitespace();
      // we only need to handle old selection endpoint if it was different from start
      if (mRangeItem->startNode != mRangeItem->endNode ||
          mRangeItem->startOffset != mRangeItem->endOffset) {
        NS_ENSURE_STATE(mHTMLEditor);
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
    if ((action == EditAction::insertText) || 
        (action == EditAction::insertIMEText) ||
        (action == EditAction::deleteSelection) ||
        (action == EditAction::insertBreak) || 
        (action == EditAction::htmlPaste ||
        (action == EditAction::loadHTML)))
    {
      res = AdjustSelection(selection, aDirection);
      NS_ENSURE_SUCCESS(res, res);
    }

    // check for any styles which were removed inappropriately
    if (action == EditAction::insertText ||
        action == EditAction::insertIMEText ||
        action == EditAction::deleteSelection ||
        IsStyleCachePreservingAction(action)) {
      NS_ENSURE_STATE(mHTMLEditor);
      mHTMLEditor->mTypeInState->UpdateSelState(selection);
      res = ReapplyCachedStyles();
      NS_ENSURE_SUCCESS(res, res);
      ClearCachedStyles();
    }    
  }

  NS_ENSURE_STATE(mHTMLEditor);
  
  res = mHTMLEditor->HandleInlineSpellCheck(action, selection, 
                                            GetAsDOMNode(mRangeItem->startNode),
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
  if (info->action == EditAction::outputText ||
      info->action == EditAction::undo ||
      info->action == EditAction::redo) {
    return nsTextEditRules::WillDoAction(aSelection, aInfo, aCancel, aHandled);
  }

  // Nothing to do if there's no selection to act on
  if (!aSelection) {
    return NS_OK;
  }
  NS_ENSURE_TRUE(aSelection->RangeCount(), NS_OK);

  nsRefPtr<nsRange> range = aSelection->GetRangeAt(0);
  nsCOMPtr<nsINode> selStartNode = range->GetStartParent();

  NS_ENSURE_STATE(mHTMLEditor);
  if (!mHTMLEditor->IsModifiableNode(selStartNode)) {
    *aCancel = true;
    return NS_OK;
  }

  nsCOMPtr<nsINode> selEndNode = range->GetEndParent();

  if (selStartNode != selEndNode) {
    NS_ENSURE_STATE(mHTMLEditor);
    if (!mHTMLEditor->IsModifiableNode(selEndNode)) {
      *aCancel = true;
      return NS_OK;
    }

    NS_ENSURE_STATE(mHTMLEditor);
    if (!mHTMLEditor->IsModifiableNode(range->GetCommonAncestor())) {
      *aCancel = true;
      return NS_OK;
    }
  }

  switch (info->action) {
    case EditAction::insertText:
    case EditAction::insertIMEText:
      return WillInsertText(info->action, aSelection, aCancel, aHandled,
                            info->inString, info->outString, info->maxLength);
    case EditAction::loadHTML:
      return WillLoadHTML(aSelection, aCancel);
    case EditAction::insertBreak:
      return WillInsertBreak(aSelection, aCancel, aHandled);
    case EditAction::deleteSelection:
      return WillDeleteSelection(aSelection, info->collapsedAction,
                                 info->stripWrappers, aCancel, aHandled);
    case EditAction::makeList:
      return WillMakeList(aSelection, info->blockType, info->entireList,
                          info->bulletType, aCancel, aHandled);
    case EditAction::indent:
      return WillIndent(aSelection, aCancel, aHandled);
    case EditAction::outdent:
      return WillOutdent(aSelection, aCancel, aHandled);
    case EditAction::setAbsolutePosition:
      return WillAbsolutePosition(aSelection, aCancel, aHandled);
    case EditAction::removeAbsolutePosition:
      return WillRemoveAbsolutePosition(aSelection, aCancel, aHandled);
    case EditAction::align:
      return WillAlign(aSelection, info->alignType, aCancel, aHandled);
    case EditAction::makeBasicBlock:
      return WillMakeBasicBlock(aSelection, info->blockType, aCancel, aHandled);
    case EditAction::removeList:
      return WillRemoveList(aSelection, info->bOrdered, aCancel, aHandled);
    case EditAction::makeDefListItem:
      return WillMakeDefListItem(aSelection, info->blockType, info->entireList,
                                 aCancel, aHandled);
    case EditAction::insertElement:
      return WillInsert(aSelection, aCancel);
    case EditAction::decreaseZIndex:
      return WillRelativeChangeZIndex(aSelection, -1, aCancel, aHandled);
    case EditAction::increaseZIndex:
      return WillRelativeChangeZIndex(aSelection, 1, aCancel, aHandled);
    default:
      return nsTextEditRules::WillDoAction(aSelection, aInfo,
                                           aCancel, aHandled);
  }
}


NS_IMETHODIMP
nsHTMLEditRules::DidDoAction(Selection* aSelection, nsRulesInfo* aInfo,
                             nsresult aResult)
{
  nsTextRulesInfo *info = static_cast<nsTextRulesInfo*>(aInfo);
  switch (info->action)
  {
    case EditAction::insertBreak:
      return DidInsertBreak(aSelection, aResult);
    case EditAction::deleteSelection:
      return DidDeleteSelection(aSelection, info->collapsedAction, aResult);
    case EditAction::makeBasicBlock:
    case EditAction::indent:
    case EditAction::outdent:
    case EditAction::align:
      return DidMakeBasicBlock(aSelection, aInfo, aResult);
    case EditAction::setAbsolutePosition: {
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
  int32_t listCount = arrayOfNodes.Count();
  for (int32_t i = listCount - 1; i >= 0; --i) {
    nsIDOMNode* curDOMNode = arrayOfNodes[i];
    nsCOMPtr<dom::Element> curElement = do_QueryInterface(curDOMNode);

    if (!curElement) {
      bNonList = true;
    } else if (curElement->IsHTMLElement(nsGkAtoms::ul)) {
      *aUL = true;
    } else if (curElement->IsHTMLElement(nsGkAtoms::ol)) {
      *aOL = true;
    } else if (curElement->IsHTMLElement(nsGkAtoms::li)) {
      if (dom::Element* parent = curElement->GetParentElement()) {
        if (parent->IsHTMLElement(nsGkAtoms::ul)) {
          *aUL = true;
        } else if (parent->IsHTMLElement(nsGkAtoms::ol)) {
          *aOL = true;
        }
      }
    } else if (curElement->IsAnyOfHTMLElements(nsGkAtoms::dl,
                                               nsGkAtoms::dt,
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
  int32_t listCount = arrayOfNodes.Count();
  for (int32_t i = listCount - 1; i >= 0; --i) {
    nsIDOMNode* curNode = arrayOfNodes[i];
    nsCOMPtr<dom::Element> element = do_QueryInterface(curNode);
    if (!element) {
      bNonList = true;
    } else if (element->IsAnyOfHTMLElements(nsGkAtoms::ul,
                                            nsGkAtoms::ol,
                                            nsGkAtoms::li)) {
      *aLI = true;
    } else if (element->IsHTMLElement(nsGkAtoms::dt)) {
      *aDT = true;
    } else if (element->IsHTMLElement(nsGkAtoms::dd)) {
      *aDD = true;
    } else if (element->IsHTMLElement(nsGkAtoms::dl)) {
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
  NS_ENSURE_STATE(mHTMLEditor);  
  nsRefPtr<Selection> selection = mHTMLEditor->GetSelection();
  NS_ENSURE_STATE(selection);

  // get selection location
  NS_ENSURE_STATE(mHTMLEditor);  
  nsCOMPtr<Element> rootElem = mHTMLEditor->GetRoot();
  NS_ENSURE_TRUE(rootElem, NS_ERROR_FAILURE);

  int32_t offset, rootOffset;
  nsCOMPtr<nsINode> parent = nsEditor::GetNodeLocation(rootElem, &rootOffset);
  NS_ENSURE_STATE(mHTMLEditor);
  nsresult res = mHTMLEditor->GetStartNodeAndOffset(selection,
                                                    getter_AddRefs(parent),
                                                    &offset);
  NS_ENSURE_SUCCESS(res, res);

  // is the selection collapsed?
  nsCOMPtr<nsIDOMNode> nodeToExamine;
  if (selection->Collapsed()) {
    // if it is, we want to look at 'parent' and its ancestors
    // for divs with alignment on them
    nodeToExamine = GetAsDOMNode(parent);
  }
  else if (!mHTMLEditor) {
    return NS_ERROR_UNEXPECTED;
  }
  else if (mHTMLEditor->IsTextNode(parent)) 
  {
    // if we are in a text node, then that is the node of interest
    nodeToExamine = GetAsDOMNode(parent);
  } else if (parent->IsHTMLElement(nsGkAtoms::html) && offset == rootOffset) {
    // if we have selected the body, let's look at the first editable node
    NS_ENSURE_STATE(mHTMLEditor);
    nodeToExamine =
      GetAsDOMNode(mHTMLEditor->GetNextNode(parent, offset, true));
  }
  else
  {
    nsTArray<nsRefPtr<nsRange>> arrayOfRanges;
    res = GetPromotedRanges(selection, arrayOfRanges, EditAction::align);
    NS_ENSURE_SUCCESS(res, res);

    // use these ranges to construct a list of nodes to act on.
    nsCOMArray<nsIDOMNode> arrayOfNodes;
    res = GetNodesForOperation(arrayOfRanges, arrayOfNodes,
                               EditAction::align, true);
    NS_ENSURE_SUCCESS(res, res);                                 
    nodeToExamine = arrayOfNodes.SafeObjectAt(0);
  }

  NS_ENSURE_TRUE(nodeToExamine, NS_ERROR_NULL_POINTER);

  NS_NAMED_LITERAL_STRING(typeAttrName, "align");
  nsIAtom  *dummyProperty = nullptr;
  nsCOMPtr<nsIDOMNode> blockParent;
  NS_ENSURE_STATE(mHTMLEditor);
  if (mHTMLEditor->IsBlockNode(nodeToExamine))
    blockParent = nodeToExamine;
  else {
    NS_ENSURE_STATE(mHTMLEditor);
    blockParent = mHTMLEditor->GetBlockNodeParent(nodeToExamine);
  }

  NS_ENSURE_TRUE(blockParent, NS_ERROR_FAILURE);

  NS_ENSURE_STATE(mHTMLEditor);
  if (mHTMLEditor->IsCSSEnabled())
  {
    nsCOMPtr<nsIContent> blockParentContent = do_QueryInterface(blockParent);
    NS_ENSURE_STATE(mHTMLEditor);
    if (blockParentContent && 
        mHTMLEditor->mHTMLCSSUtils->IsCSSEditableProperty(blockParentContent, dummyProperty, &typeAttrName))
    {
      // we are in CSS mode and we know how to align this element with CSS
      nsAutoString value;
      // let's get the value(s) of text-align or margin-left/margin-right
      NS_ENSURE_STATE(mHTMLEditor);
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
    if (NS_FAILED(res)) temp = nullptr;
    nodeToExamine = temp; 
  }
  return NS_OK;
}

static nsIAtom* MarginPropertyAtomForIndent(nsHTMLCSSUtils* aHTMLCSSUtils,
                                            nsIDOMNode* aNode) {
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node || !aNode, nsGkAtoms::marginLeft);
  nsAutoString direction;
  aHTMLCSSUtils->GetComputedProperty(*node, *nsGkAtoms::direction, direction);
  return direction.EqualsLiteral("rtl") ?
    nsGkAtoms::marginRight : nsGkAtoms::marginLeft;
}

nsresult 
nsHTMLEditRules::GetIndentState(bool *aCanIndent, bool *aCanOutdent)
{
  NS_ENSURE_TRUE(aCanIndent && aCanOutdent, NS_ERROR_FAILURE);
  *aCanIndent = true;    
  *aCanOutdent = false;

  // get selection
  NS_ENSURE_STATE(mHTMLEditor && mHTMLEditor->GetSelection());
  OwningNonNull<Selection> selection = *mHTMLEditor->GetSelection();

  // contruct a list of nodes to act on.
  nsCOMArray<nsIDOMNode> arrayOfNodes;
  nsresult res = GetNodesFromSelection(selection, EditAction::indent,
                                       arrayOfNodes, true);
  NS_ENSURE_SUCCESS(res, res);

  // examine nodes in selection for blockquotes or list elements;
  // these we can outdent.  Note that we return true for canOutdent
  // if *any* of the selection is outdentable, rather than all of it.
  int32_t listCount = arrayOfNodes.Count();
  int32_t i;
  NS_ENSURE_STATE(mHTMLEditor);
  bool useCSS = mHTMLEditor->IsCSSEnabled();
  for (i=listCount-1; i>=0; i--)
  {
    nsCOMPtr<nsINode> curNode = do_QueryInterface(arrayOfNodes[i]);
    NS_ENSURE_STATE(curNode || !arrayOfNodes[i]);
    
    if (nsHTMLEditUtils::IsNodeThatCanOutdent(GetAsDOMNode(curNode))) {
      *aCanOutdent = true;
      break;
    }
    else if (useCSS) {
      // we are in CSS mode, indentation is done using the margin-left (or margin-right) property
      NS_ENSURE_STATE(mHTMLEditor);
      nsIAtom* marginProperty =
        MarginPropertyAtomForIndent(mHTMLEditor->mHTMLCSSUtils,
                                    GetAsDOMNode(curNode));
      nsAutoString value;
      // retrieve its specified value
      NS_ENSURE_STATE(mHTMLEditor);
      mHTMLEditor->mHTMLCSSUtils->GetSpecifiedProperty(*curNode,
                                                       *marginProperty, value);
      float f;
      nsCOMPtr<nsIAtom> unit;
      // get its number part and its unit
      NS_ENSURE_STATE(mHTMLEditor);
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
    NS_ENSURE_STATE(mHTMLEditor);
    nsCOMPtr<nsIDOMNode> parent, tmp, root = do_QueryInterface(mHTMLEditor->GetRoot());
    NS_ENSURE_TRUE(root, NS_ERROR_NULL_POINTER);
    int32_t selOffset;
    NS_ENSURE_STATE(mHTMLEditor);
    nsRefPtr<Selection> selection = mHTMLEditor->GetSelection();
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
    
    // test start parent hierarchy
    NS_ENSURE_STATE(mHTMLEditor);
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
    NS_ENSURE_STATE(mHTMLEditor);
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
  int32_t listCount = arrayOfNodes.Count();
  int32_t i;
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
    int32_t selOffset;
    NS_ENSURE_STATE(mHTMLEditor);
    nsRefPtr<Selection> selection = mHTMLEditor->GetSelection();
    NS_ENSURE_STATE(selection);
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->GetStartNodeAndOffset(selection, getter_AddRefs(selNode), &selOffset);
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(selNode, NS_ERROR_NULL_POINTER);
    arrayOfNodes.AppendObject(selNode);
    listCount = 1;
  }

  // remember root node
  NS_ENSURE_STATE(mHTMLEditor);
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
    bool isFormat = nsHTMLEditUtils::IsFormatNode(child);
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
nsHTMLEditRules::WillInsert(Selection* aSelection, bool* aCancel)
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
  int32_t selOffset;
  // get the (collapsed) selection location
  NS_ENSURE_STATE(mHTMLEditor);
  res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(selNode),
                                           &selOffset);
  NS_ENSURE_SUCCESS(res, res);
  // get prior node
  NS_ENSURE_STATE(mHTMLEditor);
  res = mHTMLEditor->GetPriorHTMLNode(selNode, selOffset,
                                      address_of(priorNode));
  if (NS_SUCCEEDED(res) && priorNode && nsTextEditUtils::IsMozBR(priorNode))
  {
    nsCOMPtr<nsIDOMNode> block1, block2;
    if (IsBlockNode(selNode)) {
      block1 = selNode;
    }
    else {
      NS_ENSURE_STATE(mHTMLEditor);
      block1 = mHTMLEditor->GetBlockNodeParent(selNode);
    }
    NS_ENSURE_STATE(mHTMLEditor);
    block2 = mHTMLEditor->GetBlockNodeParent(priorNode);
  
    if (block1 && block1 == block2) {
      // if we are here then the selection is right after a mozBR
      // that is in the same block as the selection.  We need to move
      // the selection start to be before the mozBR.
      selNode = nsEditor::GetNodeLocation(priorNode, &selOffset);
      res = aSelection->Collapse(selNode,selOffset);
      NS_ENSURE_SUCCESS(res, res);
    }
  }

  if (mDidDeleteSelection &&
      (mTheAction == EditAction::insertText ||
       mTheAction == EditAction::insertIMEText ||
       mTheAction == EditAction::deleteSelection)) {
    res = ReapplyCachedStyles();
    NS_ENSURE_SUCCESS(res, res);
  }
  // For most actions we want to clear the cached styles, but there are
  // exceptions
  if (!IsStyleCachePreservingAction(mTheAction)) {
    ClearCachedStyles();
  }

  return NS_OK;
}    

nsresult
nsHTMLEditRules::WillInsertText(EditAction aAction,
                                Selection*       aSelection,
                                bool            *aCancel,
                                bool            *aHandled,
                                const nsAString *inString,
                                nsAString       *outString,
                                int32_t          aMaxLength)
{  
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }

  if (inString->IsEmpty() && aAction != EditAction::insertIMEText) {
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
  // If the selection isn't collapsed, delete it.  Don't delete existing inline
  // tags, because we're hopefully going to insert text (bug 787432).
  if (!aSelection->Collapsed()) {
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->DeleteSelection(nsIEditor::eNone, nsIEditor::eNoStrip);
    NS_ENSURE_SUCCESS(res, res);
  }

  res = WillInsert(aSelection, aCancel);
  NS_ENSURE_SUCCESS(res, res);
  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = false;

  // we need to get the doc
  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIDocument> doc = mHTMLEditor->GetDocument();
  nsCOMPtr<nsIDOMDocument> domDoc = mHTMLEditor->GetDOMDocument();
  NS_ENSURE_TRUE(doc && domDoc, NS_ERROR_NOT_INITIALIZED);

  // for every property that is set, insert a new inline style node
  res = CreateStyleForInsertText(aSelection, domDoc);
  NS_ENSURE_SUCCESS(res, res);
  
  // get the (collapsed) selection location
  NS_ENSURE_STATE(mHTMLEditor);
  NS_ENSURE_STATE(aSelection->GetRangeAt(0));
  nsCOMPtr<nsINode> selNode = aSelection->GetRangeAt(0)->GetStartParent();
  int32_t selOffset = aSelection->GetRangeAt(0)->StartOffset();
  NS_ENSURE_STATE(selNode);

  // dont put text in places that can't have it
  NS_ENSURE_STATE(mHTMLEditor);
  if (!mHTMLEditor->IsTextNode(selNode) &&
      (!mHTMLEditor || !mHTMLEditor->CanContainTag(*selNode,
                                                   *nsGkAtoms::textTagName))) {
    return NS_ERROR_FAILURE;
  }
    
  if (aAction == EditAction::insertIMEText) {
    // Right now the nsWSRunObject code bails on empty strings, but IME needs 
    // the InsertTextImpl() call to still happen since empty strings are meaningful there.
    if (inString->IsEmpty())
    {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->InsertTextImpl(*inString, address_of(selNode),
                                        &selOffset, doc);
    }
    else
    {
      NS_ENSURE_STATE(mHTMLEditor);
      nsWSRunObject wsObj(mHTMLEditor, selNode, selOffset);
      res = wsObj.InsertText(*inString, address_of(selNode), &selOffset, doc);
    }
    NS_ENSURE_SUCCESS(res, res);
  }
  else // aAction == kInsertText
  {
    // find where we are
    nsCOMPtr<nsINode> curNode = selNode;
    int32_t curOffset = selOffset;
    
    // is our text going to be PREformatted?  
    // We remember this so that we know how to handle tabs.
    bool isPRE;
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->IsPreformatted(GetAsDOMNode(selNode), &isPRE);
    NS_ENSURE_SUCCESS(res, res);    
    
    // turn off the edit listener: we know how to
    // build the "doc changed range" ourselves, and it's
    // must faster to do it once here than to track all
    // the changes one at a time.
    nsAutoLockListener lockit(&mListenerEnabled); 
    
    // don't spaz my selection in subtransactions
    NS_ENSURE_STATE(mHTMLEditor);
    nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
    nsAutoString tString(*inString);
    const char16_t *unicodeBuf = tString.get();
    int32_t pos = 0;
    NS_NAMED_LITERAL_STRING(newlineStr, LFSTR);
        
    // for efficiency, break out the pre case separately.  This is because
    // its a lot cheaper to search the input string for only newlines than
    // it is to search for both tabs and newlines.
    if (isPRE || IsPlaintextEditor())
    {
      while (unicodeBuf && (pos != -1) && (pos < (int32_t)(*inString).Length()))
      {
        int32_t oldPos = pos;
        int32_t subStrLen;
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
          NS_ENSURE_STATE(mHTMLEditor);
          nsCOMPtr<Element> br =
            mHTMLEditor->CreateBRImpl(address_of(curNode), &curOffset,
                                      nsIEditor::eNone);
          NS_ENSURE_STATE(br);
          pos++;
        }
        else
        {
          NS_ENSURE_STATE(mHTMLEditor);
          res = mHTMLEditor->InsertTextImpl(subStr, address_of(curNode),
                                            &curOffset, doc);
          NS_ENSURE_SUCCESS(res, res);
        }
      }
    }
    else
    {
      NS_NAMED_LITERAL_STRING(tabStr, "\t");
      NS_NAMED_LITERAL_STRING(spacesStr, "    ");
      char specialChars[] = {TAB, nsCRT::LF, 0};
      while (unicodeBuf && (pos != -1) && (pos < (int32_t)inString->Length()))
      {
        int32_t oldPos = pos;
        int32_t subStrLen;
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
        NS_ENSURE_STATE(mHTMLEditor);
        nsWSRunObject wsObj(mHTMLEditor, curNode, curOffset);

        // is it a tab?
        if (subStr.Equals(tabStr))
        {
          res =
            wsObj.InsertText(spacesStr, address_of(curNode), &curOffset, doc);
          NS_ENSURE_SUCCESS(res, res);
          pos++;
        }
        // is it a return?
        else if (subStr.Equals(newlineStr))
        {
          nsCOMPtr<Element> br = wsObj.InsertBreak(address_of(curNode),
                                                   &curOffset,
                                                   nsIEditor::eNone);
          NS_ENSURE_TRUE(br, NS_ERROR_FAILURE);
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
    aSelection->SetInterlinePosition(false);
    if (curNode) aSelection->Collapse(curNode, curOffset);
    // manually update the doc changed range so that AfterEdit will clean up
    // the correct portion of the document.
    if (!mDocChangeRange)
    {
      mDocChangeRange = new nsRange(selNode);
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
nsHTMLEditRules::WillLoadHTML(Selection* aSelection, bool* aCancel)
{
  NS_ENSURE_TRUE(aSelection && aCancel, NS_ERROR_NULL_POINTER);

  *aCancel = false;

  // Delete mBogusNode if it exists. If we really need one,
  // it will be added during post-processing in AfterEditInner().

  if (mBogusNode)
  {
    mEditor->DeleteNode(mBogusNode);
    mBogusNode = nullptr;
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
    NS_ENSURE_STATE(mHTMLEditor);
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
    res = SplitMailCites(aSelection, aHandled);
    NS_ENSURE_SUCCESS(res, res);
    if (*aHandled) {
      return NS_OK;
    }
  }

  // smart splitting rules
  nsCOMPtr<nsIDOMNode> node;
  int32_t offset;

  NS_ENSURE_STATE(mHTMLEditor);
  res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(node),
                                           &offset);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

  // do nothing if the node is read-only
  NS_ENSURE_STATE(mHTMLEditor);
  if (!mHTMLEditor->IsModifiableNode(node)) {
    *aCancel = true;
    return NS_OK;
  }

  // identify the block
  nsCOMPtr<nsIDOMNode> blockParent;
  if (IsBlockNode(node)) {
    blockParent = node;
  } else {
    NS_ENSURE_STATE(mHTMLEditor);
    blockParent = mHTMLEditor->GetBlockNodeParent(node);
  }
  NS_ENSURE_TRUE(blockParent, NS_ERROR_FAILURE);

  // if the active editing host is an inline element, or if the active editing
  // host is the block parent itself, just append a br.
  NS_ENSURE_STATE(mHTMLEditor);
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
    uint32_t blockLen;
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->GetLengthOfDOMNode(blockParent, blockLen);
    NS_ENSURE_SUCCESS(res, res);
    nsCOMPtr<nsIDOMNode> brNode;
    NS_ENSURE_STATE(mHTMLEditor);
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
nsHTMLEditRules::StandardBreakImpl(nsIDOMNode* aNode, int32_t aOffset,
                                   Selection* aSelection)
{
  nsCOMPtr<nsIDOMNode> brNode;
  bool bAfterBlock = false;
  bool bBeforeBlock = false;
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> node(aNode);

  if (IsPlaintextEditor()) {
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->CreateBR(node, aOffset, address_of(brNode));
  } else {
    NS_ENSURE_STATE(mHTMLEditor);
    nsWSRunObject wsObj(mHTMLEditor, node, aOffset);
    int32_t visOffset = 0, newOffset;
    WSType wsType;
    nsCOMPtr<nsINode> node_(do_QueryInterface(node)), visNode;
    wsObj.PriorVisibleNode(node_, aOffset, address_of(visNode),
                           &visOffset, &wsType);
    if (wsType & WSType::block) {
      bAfterBlock = true;
    }
    wsObj.NextVisibleNode(node_, aOffset, address_of(visNode),
                          &visOffset, &wsType);
    if (wsType & WSType::block) {
      bBeforeBlock = true;
    }
    NS_ENSURE_STATE(mHTMLEditor);
    nsCOMPtr<nsIDOMNode> linkNode;
    if (mHTMLEditor->IsInLink(node, address_of(linkNode))) {
      // split the link
      nsCOMPtr<nsIDOMNode> linkParent;
      res = linkNode->GetParentNode(getter_AddRefs(linkParent));
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->SplitNodeDeep(linkNode, node, aOffset,
                                       &newOffset, true);
      NS_ENSURE_SUCCESS(res, res);
      // reset {node,aOffset} to the point where link was split
      node = linkParent;
      aOffset = newOffset;
    }
    node_ = do_QueryInterface(node);
    nsCOMPtr<Element> br =
      wsObj.InsertBreak(address_of(node_), &aOffset, nsIEditor::eNone);
    node = GetAsDOMNode(node_);
    brNode = GetAsDOMNode(br);
    NS_ENSURE_TRUE(brNode, NS_ERROR_FAILURE);
  }
  NS_ENSURE_SUCCESS(res, res);
  node = nsEditor::GetNodeLocation(brNode, &aOffset);
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);
  if (bAfterBlock && bBeforeBlock) {
    // we just placed a br between block boundaries.  This is the one case
    // where we want the selection to be before the br we just placed, as the
    // br will be on a new line, rather than at end of prior line.
    aSelection->SetInterlinePosition(true);
    res = aSelection->Collapse(node, aOffset);
  } else {
    NS_ENSURE_STATE(mHTMLEditor);
    nsWSRunObject wsObj(mHTMLEditor, node, aOffset+1);
    nsCOMPtr<nsINode> secondBR;
    int32_t visOffset = 0;
    WSType wsType;
    nsCOMPtr<nsINode> node_(do_QueryInterface(node));
    wsObj.NextVisibleNode(node_, aOffset+1, address_of(secondBR),
                          &visOffset, &wsType);
    if (wsType == WSType::br) {
      // the next thing after the break we inserted is another break.  Move
      // the 2nd break to be the first breaks sibling.  This will prevent them
      // from being in different inline nodes, which would break
      // SetInterlinePosition().  It will also assure that if the user clicks
      // away and then clicks back on their new blank line, they will still
      // get the style from the line above.
      int32_t brOffset;
      nsCOMPtr<nsIDOMNode> brParent = nsEditor::GetNodeLocation(GetAsDOMNode(secondBR), &brOffset);
      if (brParent != node || brOffset != aOffset + 1) {
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->MoveNode(secondBR->AsContent(), node_, aOffset + 1);
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
      aSelection->SetInterlinePosition(false);
    } else {
      aSelection->SetInterlinePosition(true);
    }
    res = aSelection->Collapse(node, aOffset+1);
  }
  return res;
}

nsresult
nsHTMLEditRules::DidInsertBreak(Selection* aSelection, nsresult aResult)
{
  return NS_OK;
}


nsresult
nsHTMLEditRules::SplitMailCites(Selection* aSelection, bool* aHandled)
{
  NS_ENSURE_TRUE(aSelection && aHandled, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIDOMNode> leftCite, rightCite;
  nsCOMPtr<nsINode> selNode;
  nsCOMPtr<Element> citeNode;
  int32_t selOffset, newOffset;
  NS_ENSURE_STATE(mHTMLEditor);
  nsresult res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(selNode), &selOffset);
  NS_ENSURE_SUCCESS(res, res);
  citeNode = GetTopEnclosingMailCite(*selNode);
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
    NS_ENSURE_STATE(mHTMLEditor);
    nsWSRunObject wsObj(mHTMLEditor, selNode, selOffset);
    nsCOMPtr<nsINode> visNode;
    int32_t visOffset=0;
    WSType wsType;
    wsObj.NextVisibleNode(selNode, selOffset, address_of(visNode),
                          &visOffset, &wsType);
    if (wsType == WSType::br) {
      // ok, we are just before a break.  is it inside the mailquote?
      if (visNode != citeNode && citeNode->Contains(visNode)) {
        // it is.  so lets reset our selection to be just after it.
        NS_ENSURE_STATE(mHTMLEditor);
        selNode = mHTMLEditor->GetNodeLocation(visNode, &selOffset);
        ++selOffset;
      }
    }
     
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->SplitNodeDeep(GetAsDOMNode(citeNode),
                                     GetAsDOMNode(selNode), selOffset,
                                     &newOffset, true, address_of(leftCite),
                                     address_of(rightCite));
    NS_ENSURE_SUCCESS(res, res);
    selNode = citeNode->GetParentNode();
    NS_ENSURE_STATE(mHTMLEditor);
    nsCOMPtr<Element> brNode = mHTMLEditor->CreateBR(selNode, newOffset);
    NS_ENSURE_STATE(brNode);
    // want selection before the break, and on same line
    aSelection->SetInterlinePosition(true);
    res = aSelection->Collapse(selNode, newOffset);
    NS_ENSURE_SUCCESS(res, res);
    // if citeNode wasn't a block, we might also want another break before it.
    // We need to examine the content both before the br we just added and also
    // just after it.  If we don't have another br or block boundary adjacent,
    // then we will need a 2nd br added to achieve blank line that user expects.
    if (IsInlineNode(GetAsDOMNode(citeNode))) {
      NS_ENSURE_STATE(mHTMLEditor);
      nsWSRunObject wsObj(mHTMLEditor, selNode, newOffset);
      nsCOMPtr<nsINode> visNode;
      int32_t visOffset=0;
      WSType wsType;
      wsObj.PriorVisibleNode(selNode, newOffset, address_of(visNode),
                             &visOffset, &wsType);
      if (wsType == WSType::normalWS || wsType == WSType::text ||
          wsType == WSType::special) {
        NS_ENSURE_STATE(mHTMLEditor);
        nsWSRunObject wsObjAfterBR(mHTMLEditor, selNode, newOffset+1);
        wsObjAfterBR.NextVisibleNode(selNode, newOffset + 1,
                                     address_of(visNode), &visOffset, &wsType);
        if (wsType == WSType::normalWS || wsType == WSType::text ||
            wsType == WSType::special) {
          NS_ENSURE_STATE(mHTMLEditor);
          brNode = mHTMLEditor->CreateBR(selNode, newOffset);
          NS_ENSURE_STATE(brNode);
        }
      }
    }
    // delete any empty cites
    bool bEmptyCite = false;
    if (leftCite)
    {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->IsEmptyNode(leftCite, &bEmptyCite, true, false);
      if (NS_SUCCEEDED(res) && bEmptyCite) {
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->DeleteNode(leftCite);
      }
      NS_ENSURE_SUCCESS(res, res);
    }
    if (rightCite)
    {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->IsEmptyNode(rightCite, &bEmptyCite, true, false);
      if (NS_SUCCEEDED(res) && bEmptyCite) {
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->DeleteNode(rightCite);
      }
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

  if (!aSelection || !aCancel || !aHandled) {
    return NS_ERROR_NULL_POINTER;
  }
  // Initialize out params
  *aCancel = false;
  *aHandled = false;

  // Remember that we did a selection deletion.  Used by CreateStyleForInsertText()
  mDidDeleteSelection = true;

  // If there is only bogus content, cancel the operation
  if (mBogusNode) {
    *aCancel = true;
    return NS_OK;
  }

  // First check for table selection mode.  If so, hand off to table editor.
  nsCOMPtr<nsIDOMElement> cell;
  NS_ENSURE_STATE(mHTMLEditor);
  nsresult res = mHTMLEditor->GetFirstSelectedCell(nullptr, getter_AddRefs(cell));
  if (NS_SUCCEEDED(res) && cell) {
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->DeleteTableCellContents();
    *aHandled = true;
    return res;
  }
  cell = nullptr;

  // origCollapsed is used later to determine whether we should join blocks. We
  // don't really care about bCollapsed because it will be modified by
  // ExtendSelectionForDelete later. JoinBlocks should happen if the original
  // selection is collapsed and the cursor is at the end of a block element, in
  // which case ExtendSelectionForDelete would always make the selection not
  // collapsed.
  bool bCollapsed = aSelection->Collapsed();
  bool join = false;
  bool origCollapsed = bCollapsed;

  nsCOMPtr<nsINode> selNode;
  int32_t selOffset;

  NS_ENSURE_STATE(aSelection->GetRangeAt(0));
  nsCOMPtr<nsINode> startNode = aSelection->GetRangeAt(0)->GetStartParent();
  int32_t startOffset = aSelection->GetRangeAt(0)->StartOffset();
  NS_ENSURE_TRUE(startNode, NS_ERROR_FAILURE);


  if (bCollapsed) {
    // If we are inside an empty block, delete it.
    NS_ENSURE_STATE(mHTMLEditor);
    nsCOMPtr<Element> host = mHTMLEditor->GetActiveEditingHost();
    NS_ENSURE_TRUE(host, NS_ERROR_FAILURE);
    res = CheckForEmptyBlock(startNode, host, aSelection, aHandled);
    NS_ENSURE_SUCCESS(res, res);
    if (*aHandled) {
      return NS_OK;
    }

    // Test for distance between caret and text that will be deleted
    res = CheckBidiLevelForDeletion(aSelection, GetAsDOMNode(startNode),
                                    startOffset, aAction, aCancel);
    NS_ENSURE_SUCCESS(res, res);
    if (*aCancel) {
      return NS_OK;
    }

    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->ExtendSelectionForDelete(aSelection, &aAction);
    NS_ENSURE_SUCCESS(res, res);

    // We should delete nothing.
    if (aAction == nsIEditor::eNone) {
      return NS_OK;
    }

    // ExtendSelectionForDelete() may have changed the selection, update it
    NS_ENSURE_STATE(aSelection->GetRangeAt(0));
    startNode = aSelection->GetRangeAt(0)->GetStartParent();
    startOffset = aSelection->GetRangeAt(0)->StartOffset();
    NS_ENSURE_TRUE(startNode, NS_ERROR_FAILURE);

    bCollapsed = aSelection->Collapsed();
  }

  if (bCollapsed) {
    // What's in the direction we are deleting?
    NS_ENSURE_STATE(mHTMLEditor);
    nsWSRunObject wsObj(mHTMLEditor, startNode, startOffset);
    nsCOMPtr<nsINode> visNode;
    int32_t visOffset;
    WSType wsType;

    // Find next visible node
    if (aAction == nsIEditor::eNext) {
      wsObj.NextVisibleNode(startNode, startOffset, address_of(visNode),
                            &visOffset, &wsType);
    } else {
      wsObj.PriorVisibleNode(startNode, startOffset, address_of(visNode),
                             &visOffset, &wsType);
    }

    if (!visNode) {
      // Can't find anything to delete!
      *aCancel = true;
      return res;
    }

    if (wsType == WSType::normalWS) {
      // We found some visible ws to delete.  Let ws code handle it.
      if (aAction == nsIEditor::eNext) {
        res = wsObj.DeleteWSForward();
      } else {
        res = wsObj.DeleteWSBackward();
      }
      *aHandled = true;
      NS_ENSURE_SUCCESS(res, res);
      res = InsertBRIfNeeded(aSelection);
      return res;
    }
    
    if (wsType == WSType::text) {
      // Found normal text to delete.
      OwningNonNull<Text> nodeAsText = *visNode->GetAsText();
      int32_t so = visOffset;
      int32_t eo = visOffset + 1;
      if (aAction == nsIEditor::ePrevious) {
        if (so == 0) {
          return NS_ERROR_UNEXPECTED;
        }
        so--;
        eo--;
        // Bug 1068979: delete both codepoints if surrogate pair
        if (so > 0) {
          const nsTextFragment *text = nodeAsText->GetText();
          if (NS_IS_LOW_SURROGATE(text->CharAt(so)) &&
              NS_IS_HIGH_SURROGATE(text->CharAt(so - 1))) {
            so--;
          }
        }
      } else {
        nsRefPtr<nsRange> range = aSelection->GetRangeAt(0);
        NS_ENSURE_STATE(range);

        NS_ASSERTION(range->GetStartParent() == visNode,
                     "selection start not in visNode");
        NS_ASSERTION(range->GetEndParent() == visNode,
                     "selection end not in visNode");

        so = range->StartOffset();
        eo = range->EndOffset();
      }
      NS_ENSURE_STATE(mHTMLEditor);
      res = nsWSRunObject::PrepareToDeleteRange(mHTMLEditor,
          address_of(visNode), &so, address_of(visNode), &eo);
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->DeleteText(nodeAsText, std::min(so, eo),
                                    DeprecatedAbs(eo - so));
      *aHandled = true;
      NS_ENSURE_SUCCESS(res, res);
      res = InsertBRIfNeeded(aSelection);
      NS_ENSURE_SUCCESS(res, res);

      // Remember that we did a ranged delete for the benefit of
      // AfterEditInner().
      mDidRangedDelete = true;

      return NS_OK;
    }
    
    if (wsType == WSType::special || wsType == WSType::br ||
        visNode->IsHTMLElement(nsGkAtoms::hr)) {
      // Short circuit for invisible breaks.  delete them and recurse.
      if (visNode->IsHTMLElement(nsGkAtoms::br) &&
          (!mHTMLEditor || !mHTMLEditor->IsVisBreak(visNode))) {
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->DeleteNode(visNode);
        NS_ENSURE_SUCCESS(res, res);
        return WillDeleteSelection(aSelection, aAction, aStripWrappers,
                                   aCancel, aHandled);
      }

      // Special handling for backspace when positioned after <hr>
      if (aAction == nsIEditor::ePrevious &&
          visNode->IsHTMLElement(nsGkAtoms::hr)) {
        // Only if the caret is positioned at the end-of-hr-line position, we
        // want to delete the <hr>.
        //
        // In other words, we only want to delete, if our selection position
        // (indicated by startNode and startOffset) is the position directly
        // after the <hr>, on the same line as the <hr>.
        //
        // To detect this case we check:
        // startNode == parentOfVisNode
        // and
        // startOffset -1 == visNodeOffsetToVisNodeParent
        // and
        // interline position is false (left)
        //
        // In any other case we set the position to startnode -1 and
        // interlineposition to false, only moving the caret to the
        // end-of-hr-line position.
        bool moveOnly = true;

        selNode = visNode->GetParentNode();
        selOffset = selNode ? selNode->IndexOf(visNode) : -1;

        bool interLineIsRight;
        res = aSelection->GetInterlinePosition(&interLineIsRight);
        NS_ENSURE_SUCCESS(res, res);

        if (startNode == selNode && startOffset - 1 == selOffset &&
            !interLineIsRight) {
          moveOnly = false;
        }

        if (moveOnly) {
          // Go to the position after the <hr>, but to the end of the <hr> line
          // by setting the interline position to left.
          ++selOffset;
          aSelection->Collapse(selNode, selOffset);
          aSelection->SetInterlinePosition(false);
          mDidExplicitlySetInterline = true;
          *aHandled = true;

          // There is one exception to the move only case.  If the <hr> is
          // followed by a <br> we want to delete the <br>.

          WSType otherWSType;
          nsCOMPtr<nsINode> otherNode;
          int32_t otherOffset;

          wsObj.NextVisibleNode(startNode, startOffset, address_of(otherNode),
                                &otherOffset, &otherWSType);

          if (otherWSType == WSType::br) {
            // Delete the <br>

            NS_ENSURE_STATE(mHTMLEditor);
            nsCOMPtr<nsIContent> otherContent(do_QueryInterface(otherNode));
            res = nsWSRunObject::PrepareToDeleteNode(mHTMLEditor, otherContent);
            NS_ENSURE_SUCCESS(res, res);
            NS_ENSURE_STATE(mHTMLEditor);
            res = mHTMLEditor->DeleteNode(otherNode);
            NS_ENSURE_SUCCESS(res, res);
          }

          return NS_OK;
        }
        // Else continue with normal delete code
      }

      // Found break or image, or hr.
      NS_ENSURE_STATE(mHTMLEditor);
      NS_ENSURE_STATE(visNode->IsContent());
      res = nsWSRunObject::PrepareToDeleteNode(mHTMLEditor,
                                               visNode->AsContent());
      NS_ENSURE_SUCCESS(res, res);
      // Remember sibling to visnode, if any
      NS_ENSURE_STATE(mHTMLEditor);
      nsCOMPtr<nsIContent> sibling = mHTMLEditor->GetPriorHTMLSibling(visNode);
      // Delete the node, and join like nodes if appropriate
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->DeleteNode(visNode);
      NS_ENSURE_SUCCESS(res, res);
      // We did something, so let's say so.
      *aHandled = true;
      // Is there a prior node and are they siblings?
      nsCOMPtr<nsINode> stepbrother;
      if (sibling) {
        NS_ENSURE_STATE(mHTMLEditor);
        stepbrother = mHTMLEditor->GetNextHTMLSibling(sibling);
      }
      // Are they both text nodes?  If so, join them!
      if (startNode == stepbrother && startNode->GetAsText() &&
          sibling->GetAsText()) {
        ::DOMPoint pt = JoinNodesSmart(*sibling, *startNode->AsContent());
        NS_ENSURE_STATE(pt.node);
        // Fix up selection
        res = aSelection->Collapse(pt.node, pt.offset);
        NS_ENSURE_SUCCESS(res, res);
      }
      res = InsertBRIfNeeded(aSelection);
      NS_ENSURE_SUCCESS(res, res);
      return NS_OK;
    }
    
    if (wsType == WSType::otherBlock) {
      // Make sure it's not a table element.  If so, cancel the operation
      // (translation: users cannot backspace or delete across table cells)
      if (nsHTMLEditUtils::IsTableElement(visNode)) {
        *aCancel = true;
        return NS_OK;
      }

      // Next to a block.  See if we are between a block and a br.  If so, we
      // really want to delete the br.  Else join content at selection to the
      // block.
      bool bDeletedBR = false;
      WSType otherWSType;
      nsCOMPtr<nsINode> otherNode;
      int32_t otherOffset;

      // Find node in other direction
      if (aAction == nsIEditor::eNext) {
        wsObj.PriorVisibleNode(startNode, startOffset, address_of(otherNode),
                               &otherOffset, &otherWSType);
      } else {
        wsObj.NextVisibleNode(startNode, startOffset, address_of(otherNode),
                              &otherOffset, &otherWSType);
      }

      // First find the adjacent node in the block
      nsCOMPtr<nsIContent> leafNode;
      nsCOMPtr<nsINode> leftNode, rightNode;
      if (aAction == nsIEditor::ePrevious) {
        NS_ENSURE_STATE(mHTMLEditor);
        leafNode = mHTMLEditor->GetLastEditableLeaf(*visNode);
        leftNode = leafNode;
        rightNode = startNode;
      } else {
        NS_ENSURE_STATE(mHTMLEditor);
        leafNode = mHTMLEditor->GetFirstEditableLeaf(*visNode);
        leftNode = startNode;
        rightNode = leafNode;
      }

      if (otherNode->IsHTMLElement(nsGkAtoms::br)) {
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->DeleteNode(otherNode);
        NS_ENSURE_SUCCESS(res, res);
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
        NS_ENSURE_STATE(leafNode);
        ::DOMPoint newSel = GetGoodSelPointForNode(*leafNode, aAction);
        NS_ENSURE_STATE(newSel.node);
        aSelection->Collapse(newSel.node, newSel.offset);
        return NS_OK;
      }

      // Else we are joining content to block

      nsCOMPtr<nsINode> selPointNode = startNode;
      int32_t selPointOffset = startOffset;
      {
        NS_ENSURE_STATE(mHTMLEditor);
        nsAutoTrackDOMPoint tracker(mHTMLEditor->mRangeUpdater,
                                    address_of(selPointNode), &selPointOffset);
        res = JoinBlocks(GetAsDOMNode(leftNode), GetAsDOMNode(rightNode),
                         aCancel);
        *aHandled = true;
        NS_ENSURE_SUCCESS(res, res);
      }
      aSelection->Collapse(selPointNode, selPointOffset);
      return NS_OK;
    }
    
    if (wsType == WSType::thisBlock) {
      // At edge of our block.  Look beside it and see if we can join to an
      // adjacent block

      // Make sure it's not a table element.  If so, cancel the operation
      // (translation: users cannot backspace or delete across table cells)
      if (nsHTMLEditUtils::IsTableElement(visNode)) {
        *aCancel = true;
        return NS_OK;
      }

      // First find the relevant nodes
      nsCOMPtr<nsINode> leftNode, rightNode;
      if (aAction == nsIEditor::ePrevious) {
        NS_ENSURE_STATE(mHTMLEditor);
        leftNode = mHTMLEditor->GetPriorHTMLNode(visNode);
        rightNode = startNode;
      } else {
        NS_ENSURE_STATE(mHTMLEditor);
        rightNode = mHTMLEditor->GetNextHTMLNode(visNode);
        leftNode = startNode;
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

      nsCOMPtr<nsINode> selPointNode = startNode;
      int32_t selPointOffset = startOffset;
      {
        NS_ENSURE_STATE(mHTMLEditor);
        nsAutoTrackDOMPoint tracker(mHTMLEditor->mRangeUpdater,
                                    address_of(selPointNode), &selPointOffset);
        res = JoinBlocks(GetAsDOMNode(leftNode), GetAsDOMNode(rightNode),
                         aCancel);
        *aHandled = true;
        NS_ENSURE_SUCCESS(res, res);
      }
      aSelection->Collapse(selPointNode, selPointOffset);
      return NS_OK;
    }
  }


  // Else we have a non-collapsed selection.  First adjust the selection.
  res = ExpandSelectionForDeletion(aSelection);
  NS_ENSURE_SUCCESS(res, res);

  // Remember that we did a ranged delete for the benefit of AfterEditInner().
  mDidRangedDelete = true;

  // Refresh start and end points
  NS_ENSURE_STATE(aSelection->GetRangeAt(0));
  startNode = aSelection->GetRangeAt(0)->GetStartParent();
  startOffset = aSelection->GetRangeAt(0)->StartOffset();
  NS_ENSURE_TRUE(startNode, NS_ERROR_FAILURE);
  nsCOMPtr<nsINode> endNode = aSelection->GetRangeAt(0)->GetEndParent();
  int32_t endOffset = aSelection->GetRangeAt(0)->EndOffset();
  NS_ENSURE_TRUE(endNode, NS_ERROR_FAILURE);

  // Figure out if the endpoints are in nodes that can be merged.  Adjust
  // surrounding whitespace in preparation to delete selection.
  if (!IsPlaintextEditor()) {
    NS_ENSURE_STATE(mHTMLEditor);
    nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
    res = nsWSRunObject::PrepareToDeleteRange(mHTMLEditor,
                                           address_of(startNode), &startOffset,
                                           address_of(endNode), &endOffset);
    NS_ENSURE_SUCCESS(res, res);
  }

  {
    // Track location of where we are deleting
    NS_ENSURE_STATE(mHTMLEditor);
    nsAutoTrackDOMPoint startTracker(mHTMLEditor->mRangeUpdater,
                                     address_of(startNode), &startOffset);
    nsAutoTrackDOMPoint endTracker(mHTMLEditor->mRangeUpdater,
                                   address_of(endNode), &endOffset);
    // We are handling all ranged deletions directly now.
    *aHandled = true;

    if (endNode == startNode) {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->DeleteSelectionImpl(aAction, aStripWrappers);
      NS_ENSURE_SUCCESS(res, res);
    } else {
      // Figure out mailcite ancestors
      nsCOMPtr<Element> startCiteNode = GetTopEnclosingMailCite(*startNode);
      nsCOMPtr<Element> endCiteNode = GetTopEnclosingMailCite(*endNode);

      // If we only have a mailcite at one of the two endpoints, set the
      // directionality of the deletion so that the selection will end up
      // outside the mailcite.
      if (startCiteNode && !endCiteNode) {
        aAction = nsIEditor::eNext;
      } else if (!startCiteNode && endCiteNode) {
        aAction = nsIEditor::ePrevious;
      }

      // Figure out block parents
      nsCOMPtr<Element> leftParent;
      if (IsBlockNode(GetAsDOMNode(startNode))) {
        leftParent = startNode->AsElement();
      } else {
        NS_ENSURE_STATE(mHTMLEditor);
        leftParent = mHTMLEditor->GetBlockNodeParent(startNode);
      }

      nsCOMPtr<Element> rightParent;
      if (IsBlockNode(GetAsDOMNode(endNode))) {
        rightParent = endNode->AsElement();
      } else {
        NS_ENSURE_STATE(mHTMLEditor);
        rightParent = mHTMLEditor->GetBlockNodeParent(endNode);
      }

      // Are endpoint block parents the same?  Use default deletion
      if (leftParent && leftParent == rightParent) {
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->DeleteSelectionImpl(aAction, aStripWrappers);
      } else {
        // Deleting across blocks.  Are the blocks of same type?
        NS_ENSURE_STATE(leftParent && rightParent);

        // Are the blocks siblings?
        nsCOMPtr<nsINode> leftBlockParent = leftParent->GetParentNode();
        nsCOMPtr<nsINode> rightBlockParent = rightParent->GetParentNode();

        // MOOSE: this could conceivably screw up a table.. fix me.
        NS_ENSURE_STATE(mHTMLEditor);
        if (leftBlockParent == rightBlockParent &&
            mHTMLEditor->NodesSameType(GetAsDOMNode(leftParent),
                                       GetAsDOMNode(rightParent))) {
          if (leftParent->IsHTMLElement(nsGkAtoms::p)) {
            // First delete the selection
            NS_ENSURE_STATE(mHTMLEditor);
            res = mHTMLEditor->DeleteSelectionImpl(aAction, aStripWrappers);
            NS_ENSURE_SUCCESS(res, res);
            // Then join paragraphs, insert break
            NS_ENSURE_STATE(mHTMLEditor);
            ::DOMPoint pt = mHTMLEditor->JoinNodeDeep(*leftParent, *rightParent);
            NS_ENSURE_STATE(pt.node);
            // Fix up selection
            res = aSelection->Collapse(pt.node, pt.offset);
            NS_ENSURE_SUCCESS(res, res);
            return NS_OK;
          }
          if (nsHTMLEditUtils::IsListItem(leftParent) ||
              nsHTMLEditUtils::IsHeader(*leftParent)) {
            // First delete the selection
            NS_ENSURE_STATE(mHTMLEditor);
            res = mHTMLEditor->DeleteSelectionImpl(aAction, aStripWrappers);
            NS_ENSURE_SUCCESS(res, res);
            // Join blocks
            NS_ENSURE_STATE(mHTMLEditor);
            ::DOMPoint pt = mHTMLEditor->JoinNodeDeep(*leftParent, *rightParent);
            NS_ENSURE_STATE(pt.node);
            // Fix up selection
            res = aSelection->Collapse(pt.node, pt.offset);
            NS_ENSURE_SUCCESS(res, res);
            return NS_OK;
          }
        }

        // Else blocks not same type, or not siblings.  Delete everything
        // except table elements.
        join = true;

        uint32_t rangeCount = aSelection->RangeCount();
        for (uint32_t rangeIdx = 0; rangeIdx < rangeCount; ++rangeIdx) {
          OwningNonNull<nsRange> range = *aSelection->GetRangeAt(rangeIdx);

          // Build a list of nodes in the range
          nsTArray<nsCOMPtr<nsINode>> arrayOfNodes;
          nsTrivialFunctor functor;
          nsDOMSubtreeIterator iter;
          res = iter.Init(range);
          NS_ENSURE_SUCCESS(res, res);
          res = iter.AppendList(functor, arrayOfNodes);
          NS_ENSURE_SUCCESS(res, res);

          // Now that we have the list, delete non-table elements
          int32_t listCount = arrayOfNodes.Length();
          for (int32_t j = 0; j < listCount; j++) {
            nsCOMPtr<nsINode> somenode = do_QueryInterface(arrayOfNodes[0]);
            NS_ENSURE_STATE(somenode);
            DeleteNonTableElements(somenode);
            arrayOfNodes.RemoveElementAt(0);
            // If something visible is deleted, no need to join.  Visible means
            // all nodes except non-visible textnodes and breaks.
            if (join && origCollapsed) {
              if (!somenode->IsContent()) {
                join = false;
                continue;
              }
              nsCOMPtr<nsIContent> content = somenode->AsContent();
              if (content->NodeType() == nsIDOMNode::TEXT_NODE) {
                NS_ENSURE_STATE(mHTMLEditor);
                mHTMLEditor->IsVisTextNode(content, &join, true);
              } else {
                NS_ENSURE_STATE(mHTMLEditor);
                join = content->IsHTMLElement(nsGkAtoms::br) &&
                       !mHTMLEditor->IsVisBreak(somenode);
              }
            }
          }
        }

        // Check endpoints for possible text deletion.  We can assume that if
        // text node is found, we can delete to end or to begining as
        // appropriate, since the case where both sel endpoints in same text
        // node was already handled (we wouldn't be here)
        if (startNode->GetAsText() &&
            startNode->Length() > uint32_t(startOffset)) {
          // Delete to last character
          OwningNonNull<nsGenericDOMDataNode> dataNode =
            *static_cast<nsGenericDOMDataNode*>(startNode.get());
          NS_ENSURE_STATE(mHTMLEditor);
          res = mHTMLEditor->DeleteText(dataNode, startOffset,
              startNode->Length() - startOffset);
          NS_ENSURE_SUCCESS(res, res);
        }
        if (endNode->GetAsText() && endOffset) {
          // Delete to first character
          NS_ENSURE_STATE(mHTMLEditor);
          OwningNonNull<nsGenericDOMDataNode> dataNode =
            *static_cast<nsGenericDOMDataNode*>(endNode.get());
          res = mHTMLEditor->DeleteText(dataNode, 0, endOffset);
          NS_ENSURE_SUCCESS(res, res);
        }

        if (join) {
          res = JoinBlocks(GetAsDOMNode(leftParent), GetAsDOMNode(rightParent),
                           aCancel);
          NS_ENSURE_SUCCESS(res, res);
        }
      }
    }
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
    res = aSelection->Collapse(endNode, endOffset);
  } else {
    res = aSelection->Collapse(startNode, startOffset);
  }
  NS_ENSURE_SUCCESS(res, res);
  return NS_OK;
}


/*****************************************************************************************************
*    InsertBRIfNeeded: determines if a br is needed for current selection to not be spastic.
*    If so, it inserts one.  Callers responsibility to only call with collapsed selection.
*         Selection* aSelection      the collapsed selection 
*/
nsresult
nsHTMLEditRules::InsertBRIfNeeded(Selection* aSelection)
{
  NS_ENSURE_TRUE(aSelection, NS_ERROR_NULL_POINTER);
  
  // get selection  
  nsCOMPtr<nsINode> node;
  int32_t offset;
  nsresult res = mEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(node), &offset);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

  // inline elements don't need any br
  if (!IsBlockNode(GetAsDOMNode(node))) {
    return NS_OK;
  }

  // examine selection
  NS_ENSURE_STATE(mHTMLEditor);
  nsWSRunObject wsObj(mHTMLEditor, node, offset);
  if (((wsObj.mStartReason & WSType::block) ||
       (wsObj.mStartReason & WSType::br)) &&
      (wsObj.mEndReason & WSType::block)) {
    // if we are tucked between block boundaries then insert a br
    // first check that we are allowed to
    NS_ENSURE_STATE(mHTMLEditor);
    if (mHTMLEditor->CanContainTag(*node, *nsGkAtoms::br)) {
      NS_ENSURE_STATE(mHTMLEditor);
      nsCOMPtr<Element> br =
        mHTMLEditor->CreateBR(node, offset, nsIEditor::ePrevious);
      return br ? NS_OK : NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

/**
 *  GetGoodSelPointForNode: Finds where at a node you would want to set the
 *  selection if you were trying to have a caret next to it.
 *       nsINode& aNode                   the node
 *       nsIEditor::EDirection aAction    which edge to find: eNext indicates
 *                                        beginning, ePrevious ending
 */
::DOMPoint
nsHTMLEditRules::GetGoodSelPointForNode(nsINode& aNode,
                                        nsIEditor::EDirection aAction)
{
  NS_ENSURE_TRUE(mHTMLEditor, ::DOMPoint());
  if (aNode.GetAsText() || mHTMLEditor->IsContainer(&aNode)) {
    return ::DOMPoint(&aNode,
                      aAction == nsIEditor::ePrevious ? aNode.Length() : 0);
  }

  ::DOMPoint ret;
  ret.node = aNode.GetParentNode();
  ret.offset = ret.node ? ret.node->IndexOf(&aNode) : -1;
  NS_ENSURE_TRUE(mHTMLEditor, ::DOMPoint());
  if ((!aNode.IsHTMLElement(nsGkAtoms::br) ||
       mHTMLEditor->IsVisBreak(&aNode)) &&
      aAction == nsIEditor::ePrevious) {
    ret.offset++;
  }
  return ret;
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
nsHTMLEditRules::JoinBlocks(nsIDOMNode *aLeftNode,
                            nsIDOMNode *aRightNode,
                            bool *aCanceled)
{
  NS_ENSURE_ARG_POINTER(aLeftNode && aRightNode);

  nsCOMPtr<nsIDOMNode> aLeftBlock, aRightBlock;

  if (IsBlockNode(aLeftNode)) {
    aLeftBlock = aLeftNode;
  } else if (aLeftNode) {
    NS_ENSURE_STATE(mHTMLEditor);
    aLeftBlock = mHTMLEditor->GetBlockNodeParent(aLeftNode);
  }

  if (IsBlockNode(aRightNode)) {
    aRightBlock = aRightNode;
  } else if (aRightNode) {
    NS_ENSURE_STATE(mHTMLEditor);
    aRightBlock = mHTMLEditor->GetBlockNodeParent(aRightNode);
  }

  // sanity checks
  NS_ENSURE_TRUE(aLeftBlock && aRightBlock, NS_ERROR_NULL_POINTER);
  NS_ENSURE_STATE(aLeftBlock != aRightBlock);

  if (nsHTMLEditUtils::IsTableElement(aLeftBlock) ||
      nsHTMLEditUtils::IsTableElement(aRightBlock)) {
    // do not try to merge table elements
    *aCanceled = true;
    return NS_OK;
  }

  // make sure we don't try to move thing's into HR's, which look like blocks but aren't containers
  if (nsHTMLEditUtils::IsHR(aLeftBlock)) {
    NS_ENSURE_STATE(mHTMLEditor);
    nsCOMPtr<nsIDOMNode> realLeft = mHTMLEditor->GetBlockNodeParent(aLeftBlock);
    aLeftBlock = realLeft;
  }
  if (nsHTMLEditUtils::IsHR(aRightBlock)) {
    NS_ENSURE_STATE(mHTMLEditor);
    nsCOMPtr<nsIDOMNode> realRight = mHTMLEditor->GetBlockNodeParent(aRightBlock);
    aRightBlock = realRight;
  }
  NS_ENSURE_STATE(aLeftBlock && aRightBlock);

  // bail if both blocks the same
  if (aLeftBlock == aRightBlock) {
    *aCanceled = true;
    return NS_OK;
  }
  
  // Joining a list item to its parent is a NOP.
  if (nsHTMLEditUtils::IsList(aLeftBlock) &&
      nsHTMLEditUtils::IsListItem(aRightBlock)) {
    nsCOMPtr<nsIDOMNode> rightParent;
    aRightBlock->GetParentNode(getter_AddRefs(rightParent));
    if (rightParent == aLeftBlock) {
      return NS_OK;
    }
  }

  // special rule here: if we are trying to join list items, and they are in different lists,
  // join the lists instead.
  bool bMergeLists = false;
  nsIAtom* existingList = nsGkAtoms::_empty;
  int32_t theOffset;
  nsCOMPtr<nsIDOMNode> leftList, rightList;
  if (nsHTMLEditUtils::IsListItem(aLeftBlock) &&
      nsHTMLEditUtils::IsListItem(aRightBlock)) {
    aLeftBlock->GetParentNode(getter_AddRefs(leftList));
    aRightBlock->GetParentNode(getter_AddRefs(rightList));
    if (leftList && rightList && (leftList!=rightList))
    {
      // there are some special complications if the lists are descendants of
      // the other lists' items.  Note that it is ok for them to be descendants
      // of the other lists themselves, which is the usual case for sublists
      // in our impllementation.
      if (!nsEditorUtils::IsDescendantOf(leftList, aRightBlock, &theOffset) &&
          !nsEditorUtils::IsDescendantOf(rightList, aLeftBlock, &theOffset))
      {
        aLeftBlock = leftList;
        aRightBlock = rightList;
        bMergeLists = true;
        NS_ENSURE_STATE(mHTMLEditor);
        existingList = mHTMLEditor->GetTag(leftList);
      }
    }
  }
  
  NS_ENSURE_STATE(mHTMLEditor);
  nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
  
  nsresult res = NS_OK;
  int32_t  rightOffset = 0;
  int32_t  leftOffset  = -1;

  // theOffset below is where you find yourself in aRightBlock when you traverse upwards
  // from aLeftBlock
  if (nsEditorUtils::IsDescendantOf(aLeftBlock, aRightBlock, &rightOffset)) {
    // tricky case.  left block is inside right block.
    // Do ws adjustment.  This just destroys non-visible ws at boundaries we will be joining.
    rightOffset++;
    NS_ENSURE_STATE(mHTMLEditor);
    nsCOMPtr<nsINode> leftBlock(do_QueryInterface(aLeftBlock));
    res = nsWSRunObject::ScrubBlockBoundary(mHTMLEditor,
                                            nsWSRunObject::kBlockEnd,
                                            leftBlock);
    NS_ENSURE_SUCCESS(res, res);

    {
      nsAutoTrackDOMPoint tracker(mHTMLEditor->mRangeUpdater,
                                  address_of(aRightBlock), &rightOffset);
      nsCOMPtr<nsINode> rightBlock(do_QueryInterface(aRightBlock));
      res = nsWSRunObject::ScrubBlockBoundary(mHTMLEditor,
                                              nsWSRunObject::kAfterBlock,
                                              rightBlock, rightOffset);
      NS_ENSURE_SUCCESS(res, res);
    }
    // Do br adjustment.
    nsCOMPtr<nsIDOMNode> brNode;
    res = CheckForInvisibleBR(aLeftBlock, kBlockEnd, address_of(brNode));
    NS_ENSURE_SUCCESS(res, res);
    if (bMergeLists)
    {
      // idea here is to take all children in  rightList that are past
      // theOffset, and pull them into leftlist.
      nsCOMPtr<nsIContent> parent(do_QueryInterface(rightList));
      NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);

      nsIContent *child = parent->GetChildAt(theOffset);
      nsCOMPtr<nsINode> leftList_ = do_QueryInterface(leftList);
      NS_ENSURE_STATE(leftList_);
      while (child)
      {
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->MoveNode(child, leftList_, -1);
        NS_ENSURE_SUCCESS(res, res);

        child = parent->GetChildAt(rightOffset);
      }
    }
    else
    {
      res = MoveBlock(aLeftBlock, aRightBlock, leftOffset, rightOffset);
    }
    NS_ENSURE_STATE(mHTMLEditor);
    if (brNode) mHTMLEditor->DeleteNode(brNode);
  // theOffset below is where you find yourself in aLeftBlock when you traverse upwards
  // from aRightBlock
  } else if (nsEditorUtils::IsDescendantOf(aRightBlock, aLeftBlock, &leftOffset)) {
    // tricky case.  right block is inside left block.
    // Do ws adjustment.  This just destroys non-visible ws at boundaries we will be joining.
    NS_ENSURE_STATE(mHTMLEditor);
    nsCOMPtr<nsINode> rightBlock(do_QueryInterface(aRightBlock));
    res = nsWSRunObject::ScrubBlockBoundary(mHTMLEditor,
                                            nsWSRunObject::kBlockStart,
                                            rightBlock);
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_STATE(mHTMLEditor);
    {
      nsAutoTrackDOMPoint tracker(mHTMLEditor->mRangeUpdater,
                                  address_of(aLeftBlock), &leftOffset);
      nsCOMPtr<nsINode> leftBlock(do_QueryInterface(aLeftBlock));
      res = nsWSRunObject::ScrubBlockBoundary(mHTMLEditor,
                                              nsWSRunObject::kBeforeBlock,
                                              leftBlock, leftOffset);
      NS_ENSURE_SUCCESS(res, res);
    }
    // Do br adjustment.
    nsCOMPtr<nsIDOMNode> brNode;
    res = CheckForInvisibleBR(aLeftBlock, kBeforeBlock, address_of(brNode),
                              leftOffset);
    NS_ENSURE_SUCCESS(res, res);
    if (bMergeLists)
    {
      res = MoveContents(rightList, leftList, &leftOffset);
    }
    else
    {
      // Left block is a parent of right block, and the parent of the previous
      // visible content.  Right block is a child and contains the contents we
      // want to move.

      int32_t previousContentOffset;
      nsCOMPtr<nsIDOMNode> previousContentParent;

      if (aLeftNode == aLeftBlock) {
        // We are working with valid HTML, aLeftNode is a block node, and is
        // therefore allowed to contain aRightBlock.  This is the simple case,
        // we will simply move the content in aRightBlock out of its block.
        previousContentParent = aLeftBlock;
        previousContentOffset = leftOffset;
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

        previousContentParent =
          nsEditor::GetNodeLocation(aLeftNode, &previousContentOffset);

        // We want to move our content just after the previous visible node.
        previousContentOffset++;
      }

      // Because we don't want the moving content to receive the style of the
      // previous content, we split the previous content's style.

      NS_ENSURE_STATE(mHTMLEditor);
      nsCOMPtr<nsINode> editorRoot = mHTMLEditor->GetEditorRoot();
      if (!editorRoot || aLeftNode != editorRoot->AsDOMNode()) {
        nsCOMPtr<nsIDOMNode> splittedPreviousContent;
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->SplitStyleAbovePoint(address_of(previousContentParent),
                                                &previousContentOffset,
                                                nullptr, nullptr, nullptr,
                                                address_of(splittedPreviousContent));
        NS_ENSURE_SUCCESS(res, res);

        if (splittedPreviousContent) {
          previousContentParent =
            nsEditor::GetNodeLocation(splittedPreviousContent,
                                      &previousContentOffset);
        }
      }

      res = MoveBlock(previousContentParent, aRightBlock,
                      previousContentOffset, rightOffset);
    }
    NS_ENSURE_STATE(mHTMLEditor);
    if (brNode) mHTMLEditor->DeleteNode(brNode);
  }
  else
  {
    // normal case.  blocks are siblings, or at least close enough to siblings.  An example
    // of the latter is a <p>paragraph</p><ul><li>one<li>two<li>three</ul>.  The first
    // li and the p are not true siblings, but we still want to join them if you backspace
    // from li into p.
    
    // adjust whitespace at block boundaries
    NS_ENSURE_STATE(mHTMLEditor);
    nsCOMPtr<Element> leftBlock(do_QueryInterface(aLeftBlock));
    nsCOMPtr<Element> rightBlock(do_QueryInterface(aRightBlock));
    res = nsWSRunObject::PrepareToJoinBlocks(mHTMLEditor, leftBlock, rightBlock);
    NS_ENSURE_SUCCESS(res, res);
    // Do br adjustment.
    nsCOMPtr<nsIDOMNode> brNode;
    res = CheckForInvisibleBR(aLeftBlock, kBlockEnd, address_of(brNode));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_STATE(mHTMLEditor);
    if (bMergeLists || mHTMLEditor->NodesSameType(aLeftBlock, aRightBlock)) {
      // nodes are same type.  merge them.
      ::DOMPoint pt = JoinNodesSmart(*leftBlock, *rightBlock);
      if (pt.node && bMergeLists) {
        nsCOMPtr<nsIDOMNode> newBlock;
        res = ConvertListType(aRightBlock, address_of(newBlock),
                              existingList, nsGkAtoms::li);
      }
    }
    else
    {
      // nodes are disimilar types. 
      res = MoveBlock(aLeftBlock, aRightBlock, leftOffset, rightOffset);
    }
    if (NS_SUCCEEDED(res) && brNode)
    {
      NS_ENSURE_STATE(mHTMLEditor);
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
*         int32_t aLeftOffset            offset in aLeftBlock to move content to
*         int32_t aRightOffset           offset in aRightBlock to move content from
*/
nsresult
nsHTMLEditRules::MoveBlock(nsIDOMNode *aLeftBlock, nsIDOMNode *aRightBlock, int32_t aLeftOffset, int32_t aRightOffset)
{
  nsCOMArray<nsIDOMNode> arrayOfNodes;
  nsCOMPtr<nsISupports> isupports;
  // GetNodesFromPoint is the workhorse that figures out what we wnat to move.
  nsresult res = GetNodesFromPoint(::DOMPoint(aRightBlock,aRightOffset),
                                   EditAction::makeList, arrayOfNodes, true);
  NS_ENSURE_SUCCESS(res, res);
  int32_t listCount = arrayOfNodes.Count();
  int32_t i;
  for (i=0; i<listCount; i++)
  {
    // get the node to act on
    nsIDOMNode* curNode = arrayOfNodes[i];
    if (IsBlockNode(curNode))
    {
      // For block nodes, move their contents only, then delete block.
      res = MoveContents(curNode, aLeftBlock, &aLeftOffset); 
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_STATE(mHTMLEditor);
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
*         int32_t *aOffset          offset in aDest to move content to
*/
nsresult
nsHTMLEditRules::MoveNodeSmart(nsIDOMNode *aSource, nsIDOMNode *aDest, int32_t *aOffset)
{
  nsCOMPtr<nsIContent> source = do_QueryInterface(aSource);
  nsCOMPtr<nsINode> dest = do_QueryInterface(aDest);
  NS_ENSURE_TRUE(source && dest && aOffset, NS_ERROR_NULL_POINTER);

  nsresult res;
  // check if this node can go into the destination node
  NS_ENSURE_STATE(mHTMLEditor);
  if (mHTMLEditor->CanContain(*dest, *source)) {
    // if it can, move it there
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->MoveNode(source, dest, *aOffset);
    NS_ENSURE_SUCCESS(res, res);
    if (*aOffset != -1) ++(*aOffset);
  }
  else
  {
    // if it can't, move its children, and then delete it.
    res = MoveContents(aSource, aDest, aOffset);
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_STATE(mHTMLEditor);
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
*         int32_t *aOffset          offset in aDest to move content to
*/
nsresult
nsHTMLEditRules::MoveContents(nsIDOMNode *aSource, nsIDOMNode *aDest, int32_t *aOffset)
{
  NS_ENSURE_TRUE(aSource && aDest && aOffset, NS_ERROR_NULL_POINTER);
  if (aSource == aDest) return NS_ERROR_ILLEGAL_VALUE;
  NS_ENSURE_STATE(mHTMLEditor);
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
  if (!nsHTMLEditUtils::IsTableElementButNotTable(aNode)) {
    NS_ENSURE_STATE(mHTMLEditor);
    return mHTMLEditor->DeleteNode(aNode->AsDOMNode());
  }

  for (int32_t i = aNode->GetChildCount() - 1; i >= 0; --i) {
    nsresult rv = DeleteNonTableElements(aNode->GetChildAt(i));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult
nsHTMLEditRules::DidDeleteSelection(Selection* aSelection, 
                                    nsIEditor::EDirection aDir, 
                                    nsresult aResult)
{
  if (!aSelection) { return NS_ERROR_NULL_POINTER; }
  
  // find where we are
  nsCOMPtr<nsINode> startNode;
  int32_t startOffset;
  nsresult res = mEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(startNode), &startOffset);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(startNode, NS_ERROR_FAILURE);
  
  // find any enclosing mailcite
  nsCOMPtr<Element> citeNode = GetTopEnclosingMailCite(*startNode);
  if (citeNode) {
    bool isEmpty = true, seenBR = false;
    NS_ENSURE_STATE(mHTMLEditor);
    mHTMLEditor->IsEmptyNodeImpl(citeNode, &isEmpty, true, true, false,
                                 &seenBR);
    if (isEmpty)
    {
      int32_t offset;
      nsCOMPtr<nsINode> parent = nsEditor::GetNodeLocation(citeNode, &offset);
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->DeleteNode(citeNode);
      NS_ENSURE_SUCCESS(res, res);
      if (parent && seenBR)
      {
        NS_ENSURE_STATE(mHTMLEditor);
        nsCOMPtr<Element> brNode = mHTMLEditor->CreateBR(parent, offset);
        NS_ENSURE_STATE(brNode);
        aSelection->Collapse(parent, offset);
      }
    }
  }
  
  // call through to base class
  return nsTextEditRules::DidDeleteSelection(aSelection, aDir, aResult);
}

nsresult
nsHTMLEditRules::WillMakeList(Selection* aSelection,
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
  OwningNonNull<nsIAtom> listType = do_GetAtom(*aListType);

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

  res = NormalizeSelection(aSelection);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_STATE(mHTMLEditor);
  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);

  nsCOMArray<nsIDOMNode> arrayOfNodes;
  res = GetListActionNodes(arrayOfNodes, aEntireList);
  NS_ENSURE_SUCCESS(res, res);

  int32_t listCount = arrayOfNodes.Count();

  // check if all our nodes are <br>s, or empty inlines
  bool bOnlyBreaks = true;
  for (int32_t j = 0; j < listCount; j++) {
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
    // if only breaks, delete them
    if (bOnlyBreaks) {
      for (int32_t j = 0; j < (int32_t)listCount; j++) {
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->DeleteNode(arrayOfNodes[j]);
        NS_ENSURE_SUCCESS(res, res);
      }
    }

    // get selection location
    NS_ENSURE_STATE(aSelection->RangeCount());
    nsCOMPtr<nsINode> parent = aSelection->GetRangeAt(0)->GetStartParent();
    int32_t offset = aSelection->GetRangeAt(0)->StartOffset();
    NS_ENSURE_STATE(parent);

    // make sure we can put a list here
    NS_ENSURE_STATE(mHTMLEditor);
    if (!mHTMLEditor->CanContainTag(*parent, listType)) {
      *aCancel = true;
      return NS_OK;
    }
    res = SplitAsNeeded(listType, parent, offset);
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_STATE(mHTMLEditor);
    nsCOMPtr<Element> theList =
      mHTMLEditor->CreateNode(listType, parent, offset);
    NS_ENSURE_STATE(theList);

    NS_ENSURE_STATE(mHTMLEditor);
    nsCOMPtr<Element> theListItem =
      mHTMLEditor->CreateNode(itemType, theList, 0);
    NS_ENSURE_STATE(theListItem);

    // remember our new block for postprocessing
    mNewBlock = GetAsDOMNode(theListItem);
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
  nsCOMPtr<nsINode> curParent;
  nsCOMPtr<Element> curList, prevListItem;

  for (int32_t i = 0; i < listCount; i++) {
    // here's where we actually figure out what to do
    nsCOMPtr<nsIDOMNode> newBlock;
    nsCOMPtr<nsIContent> curNode = do_QueryInterface(arrayOfNodes[i]);
    NS_ENSURE_STATE(curNode);
    int32_t offset;
    curParent = nsEditor::GetNodeLocation(curNode, &offset);

    // make sure we don't assemble content that is in different table cells
    // into the same list.  respect table cell boundaries when listifying.
    if (curList && InDifferentTableElements(curList, curNode)) {
      curList = nullptr;
    }

    // if curNode is a Break, delete it, and quit remembering prev list item
    if (nsTextEditUtils::IsBreak(curNode)) {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->DeleteNode(curNode);
      NS_ENSURE_SUCCESS(res, res);
      prevListItem = 0;
      continue;
    } else if (IsEmptyInline(GetAsDOMNode(curNode))) {
      // if curNode is an empty inline container, delete it
      NS_ENSURE_STATE(mHTMLEditor);
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
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->MoveNode(curNode, curList, -1);
        NS_ENSURE_SUCCESS(res, res);
        res = ConvertListType(GetAsDOMNode(curNode), address_of(newBlock),
                              listType, itemType);
        NS_ENSURE_SUCCESS(res, res);
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->RemoveBlockContainer(newBlock);
        NS_ENSURE_SUCCESS(res, res);
      } else {
        // replace list with new list type
        res = ConvertListType(GetAsDOMNode(curNode), address_of(newBlock),
                              listType, itemType);
        NS_ENSURE_SUCCESS(res, res);
        curList = do_QueryInterface(newBlock);
      }
      prevListItem = 0;
      continue;
    }

    if (nsHTMLEditUtils::IsListItem(curNode)) {
      NS_ENSURE_STATE(mHTMLEditor);
      if (!curParent->IsHTMLElement(listType)) {
        // list item is in wrong type of list. if we don't have a curList,
        // split the old list and make a new list of correct type.
        if (!curList || nsEditorUtils::IsDescendantOf(curNode, curList)) {
          NS_ENSURE_STATE(mHTMLEditor);
          res = mHTMLEditor->SplitNode(curParent->AsDOMNode(), offset,
                                       getter_AddRefs(newBlock));
          NS_ENSURE_SUCCESS(res, res);
          int32_t offset;
          nsCOMPtr<nsINode> parent = nsEditor::GetNodeLocation(curParent,
                                                               &offset);
          NS_ENSURE_STATE(mHTMLEditor);
          curList = mHTMLEditor->CreateNode(listType, parent, offset);
          NS_ENSURE_STATE(curList);
        }
        // move list item to new list
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->MoveNode(curNode, curList, -1);
        NS_ENSURE_SUCCESS(res, res);
        // convert list item type if needed
        NS_ENSURE_STATE(mHTMLEditor);
        if (!curNode->IsHTMLElement(itemType)) {
          NS_ENSURE_STATE(mHTMLEditor);
          newBlock = dont_AddRef(GetAsDOMNode(
            mHTMLEditor->ReplaceContainer(curNode->AsElement(),
                                          itemType).take()));
          NS_ENSURE_STATE(newBlock);
        }
      } else {
        // item is in right type of list.  But we might still have to move it.
        // and we might need to convert list item types.
        if (!curList) {
          curList = curParent->AsElement();
        } else if (curParent != curList) {
          // move list item to new list
          NS_ENSURE_STATE(mHTMLEditor);
          res = mHTMLEditor->MoveNode(curNode, curList, -1);
          NS_ENSURE_SUCCESS(res, res);
        }
        NS_ENSURE_STATE(mHTMLEditor);
        if (!curNode->IsHTMLElement(itemType)) {
          NS_ENSURE_STATE(mHTMLEditor);
          newBlock = dont_AddRef(GetAsDOMNode(
            mHTMLEditor->ReplaceContainer(curNode->AsElement(),
                                          itemType).take()));
          NS_ENSURE_STATE(newBlock);
        }
      }
      nsCOMPtr<nsIDOMElement> curElement = do_QueryInterface(curNode);
      NS_NAMED_LITERAL_STRING(typestr, "type");
      if (aBulletType && !aBulletType->IsEmpty()) {
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->SetAttribute(curElement, typestr, *aBulletType);
      } else {
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->RemoveAttribute(curElement, typestr);
      }
      NS_ENSURE_SUCCESS(res, res);
      continue;
    }

    // if we hit a div clear our prevListItem, insert divs contents
    // into our node array, and remove the div
    if (curNode->IsHTMLElement(nsGkAtoms::div)) {
      prevListItem = nullptr;
      int32_t j = i + 1;
      res = GetInnerContent(curNode->AsDOMNode(), arrayOfNodes, &j);
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->RemoveContainer(curNode);
      NS_ENSURE_SUCCESS(res, res);
      listCount = arrayOfNodes.Count();
      continue;
    }

    // need to make a list to put things in if we haven't already,
    if (!curList) {
      res = SplitAsNeeded(listType, curParent, offset);
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_STATE(mHTMLEditor);
      curList = mHTMLEditor->CreateNode(listType, curParent, offset);
      NS_ENSURE_SUCCESS(res, res);
      // remember our new block for postprocessing
      mNewBlock = GetAsDOMNode(curList);
      // curList is now the correct thing to put curNode in
      prevListItem = 0;
    }

    // if curNode isn't a list item, we must wrap it in one
    nsCOMPtr<Element> listItem;
    if (!nsHTMLEditUtils::IsListItem(curNode)) {
      if (IsInlineNode(GetAsDOMNode(curNode)) && prevListItem) {
        // this is a continuation of some inline nodes that belong together in
        // the same list item.  use prevListItem
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->MoveNode(curNode, prevListItem, -1);
        NS_ENSURE_SUCCESS(res, res);
      } else {
        // don't wrap li around a paragraph.  instead replace paragraph with li
        if (curNode->IsHTMLElement(nsGkAtoms::p)) {
          NS_ENSURE_STATE(mHTMLEditor);
          listItem = mHTMLEditor->ReplaceContainer(curNode->AsElement(),
                                                   itemType);
          NS_ENSURE_STATE(listItem);
        } else {
          NS_ENSURE_STATE(mHTMLEditor);
          listItem = mHTMLEditor->InsertContainerAbove(curNode, itemType);
          NS_ENSURE_STATE(listItem);
        }
        if (IsInlineNode(GetAsDOMNode(curNode))) {
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
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->MoveNode(listItem, curList, -1);
      NS_ENSURE_SUCCESS(res, res);
    }
  }

  return res;
}


nsresult
nsHTMLEditRules::WillRemoveList(Selection* aSelection,
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
  NS_ENSURE_STATE(mHTMLEditor);
  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);
  
  nsTArray<nsRefPtr<nsRange>> arrayOfRanges;
  res = GetPromotedRanges(aSelection, arrayOfRanges, EditAction::makeList);
  NS_ENSURE_SUCCESS(res, res);
  
  // use these ranges to contruct a list of nodes to act on.
  nsCOMArray<nsIDOMNode> arrayOfNodes;
  res = GetListActionNodes(arrayOfNodes, false);
  NS_ENSURE_SUCCESS(res, res);                                 
                                     
  // Remove all non-editable nodes.  Leave them be.
  int32_t listCount = arrayOfNodes.Count();
  int32_t i;
  for (i=listCount-1; i>=0; i--)
  {
    nsIDOMNode* testNode = arrayOfNodes[i];
    NS_ENSURE_STATE(mHTMLEditor);
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
    int32_t offset;
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
nsHTMLEditRules::WillMakeDefListItem(Selection* aSelection,
                                     const nsAString *aItemType, 
                                     bool aEntireList, 
                                     bool *aCancel,
                                     bool *aHandled)
{
  // for now we let WillMakeList handle this
  NS_NAMED_LITERAL_STRING(listType, "dl");
  return WillMakeList(aSelection, &listType, aEntireList, nullptr, aCancel, aHandled, aItemType);
}

nsresult
nsHTMLEditRules::WillMakeBasicBlock(Selection* aSelection,
                                    const nsAString *aBlockType, 
                                    bool *aCancel,
                                    bool *aHandled)
{
  OwningNonNull<nsIAtom> blockType = do_GetAtom(*aBlockType);
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
  NS_ENSURE_STATE(mHTMLEditor);
  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);
  NS_ENSURE_STATE(mHTMLEditor);
  nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
  *aHandled = true;
  nsString tString(*aBlockType);

  // contruct a list of nodes to act on.
  nsCOMArray<nsIDOMNode> arrayOfNodes;
  res = GetNodesFromSelection(aSelection, EditAction::makeBasicBlock,
                              arrayOfNodes);
  NS_ENSURE_SUCCESS(res, res);

  // Remove all non-editable nodes.  Leave them be.
  int32_t listCount = arrayOfNodes.Count();
  int32_t i;
  for (i=listCount-1; i>=0; i--)
  {
    NS_ENSURE_STATE(mHTMLEditor);
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
    nsCOMPtr<nsIDOMNode> theBlock;
    
    // get selection location
    NS_ENSURE_STATE(aSelection->RangeCount());
    nsCOMPtr<nsINode> parent = aSelection->GetRangeAt(0)->GetStartParent();
    int32_t offset = aSelection->GetRangeAt(0)->StartOffset();
    NS_ENSURE_STATE(parent);
    if (tString.EqualsLiteral("normal") ||
        tString.IsEmpty() ) // we are removing blocks (going to "body text")
    {
      nsCOMPtr<nsIDOMNode> curBlock = parent->AsDOMNode();
      if (!IsBlockNode(curBlock)) {
        NS_ENSURE_STATE(mHTMLEditor);
        curBlock = dont_AddRef(GetAsDOMNode(
          mHTMLEditor->GetBlockNodeParent(parent).take()));
      }
      nsCOMPtr<nsIDOMNode> curBlockPar;
      NS_ENSURE_TRUE(curBlock, NS_ERROR_NULL_POINTER);
      curBlock->GetParentNode(getter_AddRefs(curBlockPar));
      if (nsHTMLEditUtils::IsFormatNode(curBlock))
      {
        // if the first editable node after selection is a br, consume it.  Otherwise
        // it gets pushed into a following block after the split, which is visually bad.
        nsCOMPtr<nsIDOMNode> brNode;
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->GetNextHTMLNode(parent->AsDOMNode(), offset,
                                           address_of(brNode));
        NS_ENSURE_SUCCESS(res, res);        
        if (brNode && nsTextEditUtils::IsBreak(brNode))
        {
          NS_ENSURE_STATE(mHTMLEditor);
          res = mHTMLEditor->DeleteNode(brNode);
          NS_ENSURE_SUCCESS(res, res); 
        }
        // do the splits!
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->SplitNodeDeep(curBlock, parent->AsDOMNode(), offset,
                                         &offset, true);
        NS_ENSURE_SUCCESS(res, res);
        // put a br at the split point
        NS_ENSURE_STATE(mHTMLEditor);
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
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->GetNextHTMLNode(parent->AsDOMNode(), offset,
                                         address_of(brNode), true);
      NS_ENSURE_SUCCESS(res, res);
      if (brNode && nsTextEditUtils::IsBreak(brNode))
      {
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->DeleteNode(brNode);
        NS_ENSURE_SUCCESS(res, res);
        // we don't need to act on this node any more
        arrayOfNodes.RemoveObject(brNode);
      }
      // make sure we can put a block here
      res = SplitAsNeeded(blockType, parent, offset);
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_STATE(mHTMLEditor);
      theBlock = dont_AddRef(GetAsDOMNode(
        mHTMLEditor->CreateNode(blockType, parent, offset).take()));
      NS_ENSURE_STATE(theBlock);
      // remember our new block for postprocessing
      mNewBlock = theBlock;
      // delete anything that was in the list of nodes
      for (int32_t j = arrayOfNodes.Count() - 1; j >= 0; --j) 
      {
        nsCOMPtr<nsIDOMNode> curNode = arrayOfNodes[0];
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->DeleteNode(curNode);
        NS_ENSURE_SUCCESS(res, res);
        arrayOfNodes.RemoveObjectAt(0);
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
nsHTMLEditRules::DidMakeBasicBlock(Selection* aSelection,
                                   nsRulesInfo *aInfo, nsresult aResult)
{
  NS_ENSURE_TRUE(aSelection, NS_ERROR_NULL_POINTER);
  // check for empty block.  if so, put a moz br in it.
  if (!aSelection->Collapsed()) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMNode> parent;
  int32_t offset;
  nsresult res = nsEditor::GetStartNodeAndOffset(aSelection, getter_AddRefs(parent), &offset);
  NS_ENSURE_SUCCESS(res, res);
  res = InsertMozBRIfNeeded(parent);
  return res;
}

nsresult
nsHTMLEditRules::WillIndent(Selection* aSelection,
                            bool* aCancel, bool* aHandled)
{
  nsresult res;
  NS_ENSURE_STATE(mHTMLEditor);
  if (mHTMLEditor->IsCSSEnabled()) {
    res = WillCSSIndent(aSelection, aCancel, aHandled);
  }
  else {
    res = WillHTMLIndent(aSelection, aCancel, aHandled);
  }
  return res;
}

nsresult
nsHTMLEditRules::WillCSSIndent(Selection* aSelection,
                               bool* aCancel, bool* aHandled)
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
  NS_ENSURE_STATE(mHTMLEditor);
  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);
  nsTArray<nsRefPtr<nsRange>> arrayOfRanges;
  nsCOMArray<nsIDOMNode> arrayOfNodes;
  
  // short circuit: detect case of collapsed selection inside an <li>.
  // just sublist that <li>.  This prevents bug 97797.
  
  nsCOMPtr<nsIDOMNode> liNode;
  if (aSelection->Collapsed()) {
    nsCOMPtr<nsIDOMNode> node, block;
    int32_t offset;
    NS_ENSURE_STATE(mHTMLEditor);
    nsresult res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(node), &offset);
    NS_ENSURE_SUCCESS(res, res);
    if (IsBlockNode(node)) {
      block = node;
    } else {
      NS_ENSURE_STATE(mHTMLEditor);
      block = mHTMLEditor->GetBlockNodeParent(node);
    }
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
    res = GetNodesFromSelection(aSelection, EditAction::indent, arrayOfNodes);
    NS_ENSURE_SUCCESS(res, res);
  }
  
  // if nothing visible in list, make an empty block
  if (ListIsEmptyLine(arrayOfNodes))
  {
    // get selection location
    NS_ENSURE_STATE(aSelection->RangeCount());
    nsCOMPtr<nsINode> parent = aSelection->GetRangeAt(0)->GetStartParent();
    int32_t offset = aSelection->GetRangeAt(0)->StartOffset();
    NS_ENSURE_STATE(parent);

    // make sure we can put a block here
    res = SplitAsNeeded(*nsGkAtoms::div, parent, offset);
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_STATE(mHTMLEditor);
    nsCOMPtr<Element> theBlock = mHTMLEditor->CreateNode(nsGkAtoms::div,
                                                         parent, offset);
    NS_ENSURE_STATE(theBlock);
    // remember our new block for postprocessing
    mNewBlock = theBlock->AsDOMNode();
    RelativeChangeIndentationOfElementNode(theBlock->AsDOMNode(), +1);
    // delete anything that was in the list of nodes
    for (int32_t j = arrayOfNodes.Count() - 1; j >= 0; --j) 
    {
      nsCOMPtr<nsIDOMNode> curNode = arrayOfNodes[0];
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->DeleteNode(curNode);
      NS_ENSURE_SUCCESS(res, res);
      arrayOfNodes.RemoveObjectAt(0);
    }
    // put selection in new block
    res = aSelection->Collapse(theBlock,0);
    selectionResetter.Abort();  // to prevent selection reseter from overriding us.
    *aHandled = true;
    return res;
  }
  
  // Ok, now go through all the nodes and put them in a blockquote, 
  // or whatever is appropriate.  Wohoo!
  int32_t i;
  nsCOMPtr<nsINode> curParent;
  nsCOMPtr<Element> curList, curQuote;
  nsCOMPtr<nsIContent> sibling;
  int32_t listCount = arrayOfNodes.Count();
  for (i=0; i<listCount; i++)
  {
    // here's where we actually figure out what to do
    nsCOMPtr<nsIContent> curNode = do_QueryInterface(arrayOfNodes[i]);
    NS_ENSURE_STATE(!arrayOfNodes[i] || curNode);

    // Ignore all non-editable nodes.  Leave them be.
    NS_ENSURE_STATE(mHTMLEditor);
    if (!mHTMLEditor->IsEditable(curNode)) continue;

    curParent = curNode->GetParentNode();
    int32_t offset = curParent ? curParent->IndexOf(curNode) : -1;
    
    // some logic for putting list items into nested lists...
    if (nsHTMLEditUtils::IsList(curParent))
    {
      sibling = nullptr;

      // Check for whether we should join a list that follows curNode.
      // We do this if the next element is a list, and the list is of the
      // same type (li/ol) as curNode was a part it.
      NS_ENSURE_STATE(mHTMLEditor);
      sibling = mHTMLEditor->GetNextHTMLSibling(curNode);
      if (sibling && nsHTMLEditUtils::IsList(sibling))
      {
        if (curParent->NodeInfo()->NameAtom() == sibling->NodeInfo()->NameAtom() &&
            curParent->NodeInfo()->NamespaceID() == sibling->NodeInfo()->NamespaceID()) {
          NS_ENSURE_STATE(mHTMLEditor);
          res = mHTMLEditor->MoveNode(curNode, sibling, 0);
          NS_ENSURE_SUCCESS(res, res);
          continue;
        }
      }
      // Check for whether we should join a list that preceeds curNode.
      // We do this if the previous element is a list, and the list is of
      // the same type (li/ol) as curNode was a part of.
      NS_ENSURE_STATE(mHTMLEditor);
      sibling = mHTMLEditor->GetPriorHTMLSibling(curNode);
      if (sibling && nsHTMLEditUtils::IsList(sibling))
      {
        if (curParent->NodeInfo()->NameAtom() == sibling->NodeInfo()->NameAtom() &&
            curParent->NodeInfo()->NamespaceID() == sibling->NodeInfo()->NamespaceID()) {
          NS_ENSURE_STATE(mHTMLEditor);
          res = mHTMLEditor->MoveNode(curNode, sibling, -1);
          NS_ENSURE_SUCCESS(res, res);
          continue;
        }
      }
      sibling = nullptr;
      
      // check to see if curList is still appropriate.  Which it is if
      // curNode is still right after it in the same list.
      if (curList) {
        NS_ENSURE_STATE(mHTMLEditor);
        sibling = mHTMLEditor->GetPriorHTMLSibling(curNode);
      }

      if (!curList || (sibling && sibling != curList)) {
        // create a new nested list of correct type
        res = SplitAsNeeded(*curParent->NodeInfo()->NameAtom(), curParent,
                            offset);
        NS_ENSURE_SUCCESS(res, res);
        NS_ENSURE_STATE(mHTMLEditor);
        curList = mHTMLEditor->CreateNode(curParent->NodeInfo()->NameAtom(),
                                          curParent, offset);
        NS_ENSURE_STATE(curList);
        // curList is now the correct thing to put curNode in
        // remember our new block for postprocessing
        mNewBlock = curList->AsDOMNode();
      }
      // tuck the node into the end of the active list
      uint32_t listLen = curList->Length();
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->MoveNode(curNode, curList, listLen);
      NS_ENSURE_SUCCESS(res, res);
    }
    
    else // not a list item
    {
      if (IsBlockNode(curNode->AsDOMNode())) {
        RelativeChangeIndentationOfElementNode(curNode->AsDOMNode(), +1);
        curQuote = nullptr;
      }
      else {
        if (!curQuote)
        {
          // First, check that our element can contain a div.
          if (!mEditor->CanContainTag(*curParent, *nsGkAtoms::div)) {
            return NS_OK; // cancelled
          }

          res = SplitAsNeeded(*nsGkAtoms::div, curParent, offset);
          NS_ENSURE_SUCCESS(res, res);
          NS_ENSURE_STATE(mHTMLEditor);
          curQuote = mHTMLEditor->CreateNode(nsGkAtoms::div, curParent,
                                             offset);
          NS_ENSURE_STATE(curQuote);
          RelativeChangeIndentationOfElementNode(curQuote->AsDOMNode(), +1);
          // remember our new block for postprocessing
          mNewBlock = curQuote->AsDOMNode();
          // curQuote is now the correct thing to put curNode in
        }
        
        // tuck the node into the end of the active blockquote
        uint32_t quoteLen = curQuote->Length();
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->MoveNode(curNode, curQuote, quoteLen);
        NS_ENSURE_SUCCESS(res, res);
      }
    }
  }
  return res;
}

nsresult
nsHTMLEditRules::WillHTMLIndent(Selection* aSelection,
                                bool* aCancel, bool* aHandled)
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
  NS_ENSURE_STATE(mHTMLEditor);
  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);
  
  // convert the selection ranges into "promoted" selection ranges:
  // this basically just expands the range to include the immediate
  // block parent, and then further expands to include any ancestors
  // whose children are all in the range
  
  nsTArray<nsRefPtr<nsRange>> arrayOfRanges;
  res = GetPromotedRanges(aSelection, arrayOfRanges, EditAction::indent);
  NS_ENSURE_SUCCESS(res, res);
  
  // use these ranges to contruct a list of nodes to act on.
  nsCOMArray<nsIDOMNode> arrayOfNodes;
  res = GetNodesForOperation(arrayOfRanges, arrayOfNodes, EditAction::indent);
  NS_ENSURE_SUCCESS(res, res);                                 
                                     
  // if nothing visible in list, make an empty block
  if (ListIsEmptyLine(arrayOfNodes))
  {
    // get selection location
    NS_ENSURE_STATE(aSelection->RangeCount());
    nsCOMPtr<nsINode> parent = aSelection->GetRangeAt(0)->GetStartParent();
    int32_t offset = aSelection->GetRangeAt(0)->StartOffset();
    NS_ENSURE_STATE(parent);

    // make sure we can put a block here
    res = SplitAsNeeded(*nsGkAtoms::blockquote, parent, offset);
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_STATE(mHTMLEditor);
    nsCOMPtr<Element> theBlock = mHTMLEditor->CreateNode(nsGkAtoms::blockquote,
                                                         parent, offset);
    NS_ENSURE_STATE(theBlock);
    // remember our new block for postprocessing
    mNewBlock = theBlock->AsDOMNode();
    // delete anything that was in the list of nodes
    for (int32_t j = arrayOfNodes.Count() - 1; j >= 0; --j) 
    {
      nsCOMPtr<nsIDOMNode> curNode = arrayOfNodes[0];
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->DeleteNode(curNode);
      NS_ENSURE_SUCCESS(res, res);
      arrayOfNodes.RemoveObjectAt(0);
    }
    // put selection in new block
    res = aSelection->Collapse(theBlock,0);
    selectionResetter.Abort();  // to prevent selection reseter from overriding us.
    *aHandled = true;
    return res;
  }

  // Ok, now go through all the nodes and put them in a blockquote, 
  // or whatever is appropriate.  Wohoo!
  int32_t i;
  nsCOMPtr<nsINode> curParent;
  nsCOMPtr<nsIContent> sibling;
  nsCOMPtr<Element> curList, curQuote, indentedLI;
  int32_t listCount = arrayOfNodes.Count();
  for (i=0; i<listCount; i++)
  {
    // here's where we actually figure out what to do
    nsCOMPtr<nsIContent> curNode = do_QueryInterface(arrayOfNodes[i]);
    NS_ENSURE_STATE(!arrayOfNodes[i] || curNode);

    // Ignore all non-editable nodes.  Leave them be.
    NS_ENSURE_STATE(mHTMLEditor);
    if (!mHTMLEditor->IsEditable(curNode)) continue;

    curParent = curNode->GetParentNode();
    int32_t offset = curParent ? curParent->IndexOf(curNode) : -1;
     
    // some logic for putting list items into nested lists...
    if (nsHTMLEditUtils::IsList(curParent))
    {
      sibling = nullptr;

      // Check for whether we should join a list that follows curNode.
      // We do this if the next element is a list, and the list is of the
      // same type (li/ol) as curNode was a part it.
      NS_ENSURE_STATE(mHTMLEditor);
      sibling = mHTMLEditor->GetNextHTMLSibling(curNode);
      if (sibling && nsHTMLEditUtils::IsList(sibling) &&
          curParent->NodeInfo()->NameAtom() == sibling->NodeInfo()->NameAtom() &&
          curParent->NodeInfo()->NamespaceID() == sibling->NodeInfo()->NamespaceID()) {
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->MoveNode(curNode, sibling, 0);
        NS_ENSURE_SUCCESS(res, res);
        continue;
      }

      // Check for whether we should join a list that preceeds curNode.
      // We do this if the previous element is a list, and the list is of
      // the same type (li/ol) as curNode was a part of.
      NS_ENSURE_STATE(mHTMLEditor);
      sibling = mHTMLEditor->GetPriorHTMLSibling(curNode);
      if (sibling && nsHTMLEditUtils::IsList(sibling) &&
          curParent->NodeInfo()->NameAtom() == sibling->NodeInfo()->NameAtom() &&
          curParent->NodeInfo()->NamespaceID() == sibling->NodeInfo()->NamespaceID()) {
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->MoveNode(curNode, sibling, -1);
        NS_ENSURE_SUCCESS(res, res);
        continue;
      }

      sibling = nullptr;

      // check to see if curList is still appropriate.  Which it is if
      // curNode is still right after it in the same list.
      if (curList) {
        NS_ENSURE_STATE(mHTMLEditor);
        sibling = mHTMLEditor->GetPriorHTMLSibling(curNode);
      }

      if (!curList || (sibling && sibling != curList) )
      {
        // create a new nested list of correct type
        res = SplitAsNeeded(*curParent->NodeInfo()->NameAtom(), curParent,
                            offset);
        NS_ENSURE_SUCCESS(res, res);
        NS_ENSURE_STATE(mHTMLEditor);
        curList = mHTMLEditor->CreateNode(curParent->NodeInfo()->NameAtom(),
                                          curParent, offset);
        NS_ENSURE_STATE(curList);
        // curList is now the correct thing to put curNode in
        // remember our new block for postprocessing
        mNewBlock = curList->AsDOMNode();
      }
      // tuck the node into the end of the active list
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->MoveNode(curNode, curList, -1);
      NS_ENSURE_SUCCESS(res, res);
      // forget curQuote, if any
      curQuote = nullptr;
    }
    
    else // not a list item, use blockquote?
    {
      // if we are inside a list item, we don't want to blockquote, we want
      // to sublist the list item.  We may have several nodes listed in the
      // array of nodes to act on, that are in the same list item.  Since
      // we only want to indent that li once, we must keep track of the most
      // recent indented list item, and not indent it if we find another node
      // to act on that is still inside the same li.
      nsCOMPtr<Element> listItem = IsInListItem(curNode);
      if (listItem) {
        if (indentedLI == listItem) {
          // already indented this list item
          continue;
        }
        curParent = listItem->GetParentNode();
        offset = curParent ? curParent->IndexOf(listItem) : -1;
        // check to see if curList is still appropriate.  Which it is if
        // curNode is still right after it in the same list.
        if (curList)
        {
          sibling = nullptr;
          NS_ENSURE_STATE(mHTMLEditor);
          sibling = mHTMLEditor->GetPriorHTMLSibling(curNode);
        }
         
        if (!curList || (sibling && sibling != curList) )
        {
          // create a new nested list of correct type
          res = SplitAsNeeded(*curParent->NodeInfo()->NameAtom(), curParent,
                              offset);
          NS_ENSURE_SUCCESS(res, res);
          NS_ENSURE_STATE(mHTMLEditor);
          curList = mHTMLEditor->CreateNode(curParent->NodeInfo()->NameAtom(),
                                            curParent, offset);
          NS_ENSURE_STATE(curList);
        }
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->MoveNode(listItem, curList, -1);
        NS_ENSURE_SUCCESS(res, res);
        // remember we indented this li
        indentedLI = listItem;
      }
      
      else
      {
        // need to make a blockquote to put things in if we haven't already,
        // or if this node doesn't go in blockquote we used earlier.
        // One reason it might not go in prio blockquote is if we are now
        // in a different table cell. 
        if (curQuote && InDifferentTableElements(curQuote, curNode)) {
          curQuote = nullptr;
        }
        
        if (!curQuote) 
        {
          // First, check that our element can contain a blockquote.
          if (!mEditor->CanContainTag(*curParent, *nsGkAtoms::blockquote)) {
            return NS_OK; // cancelled
          }

          res = SplitAsNeeded(*nsGkAtoms::blockquote, curParent, offset);
          NS_ENSURE_SUCCESS(res, res);
          NS_ENSURE_STATE(mHTMLEditor);
          curQuote = mHTMLEditor->CreateNode(nsGkAtoms::blockquote, curParent,
                                             offset);
          NS_ENSURE_STATE(curQuote);
          // remember our new block for postprocessing
          mNewBlock = curQuote->AsDOMNode();
          // curQuote is now the correct thing to put curNode in
        }
          
        // tuck the node into the end of the active blockquote
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->MoveNode(curNode, curQuote, -1);
        NS_ENSURE_SUCCESS(res, res);
        // forget curList, if any
        curList = nullptr;
      }
    }
  }
  return res;
}


nsresult
nsHTMLEditRules::WillOutdent(Selection* aSelection,
                             bool* aCancel, bool* aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = false;
  *aHandled = true;
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> rememberedLeftBQ, rememberedRightBQ;
  NS_ENSURE_STATE(mHTMLEditor);
  bool useCSS = mHTMLEditor->IsCSSEnabled();

  res = NormalizeSelection(aSelection);
  NS_ENSURE_SUCCESS(res, res);
  // some scoping for selection resetting - we may need to tweak it
  {
    NS_ENSURE_STATE(mHTMLEditor);
    nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);
    
    // convert the selection ranges into "promoted" selection ranges:
    // this basically just expands the range to include the immediate
    // block parent, and then further expands to include any ancestors
    // whose children are all in the range
    nsCOMArray<nsIDOMNode> arrayOfNodes;
    res = GetNodesFromSelection(aSelection, EditAction::outdent,
                                arrayOfNodes);
    NS_ENSURE_SUCCESS(res, res);

    // Ok, now go through all the nodes and remove a level of blockquoting, 
    // or whatever is appropriate.  Wohoo!

    nsCOMPtr<nsIDOMNode> curBlockQuote, firstBQChild, lastBQChild;
    bool curBlockQuoteIsIndentedWithCSS = false;
    int32_t listCount = arrayOfNodes.Count();
    int32_t i;
    for (i=0; i<listCount; i++)
    {
      // here's where we actually figure out what to do
      nsCOMPtr<nsIDOMNode> curNode = arrayOfNodes[i];
      nsCOMPtr<nsINode> curNode_ = do_QueryInterface(curNode);
      NS_ENSURE_STATE(curNode_);
      nsCOMPtr<nsINode> curParent = curNode_->GetParentNode();
      int32_t offset = curParent ? curParent->IndexOf(curNode_) : -1;
      
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
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->RemoveBlockContainer(curNode);
        NS_ENSURE_SUCCESS(res, res);
        continue;
      }
      // is it a block with a 'margin' property?
      if (useCSS && IsBlockNode(curNode))
      {
        NS_ENSURE_STATE(mHTMLEditor);
        nsIAtom* marginProperty = MarginPropertyAtomForIndent(mHTMLEditor->mHTMLCSSUtils, curNode);
        nsAutoString value;
        NS_ENSURE_STATE(mHTMLEditor);
        mHTMLEditor->mHTMLCSSUtils->GetSpecifiedProperty(*curNode_,
                                                         *marginProperty,
                                                         value);
        float f;
        nsCOMPtr<nsIAtom> unit;
        NS_ENSURE_STATE(mHTMLEditor);
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
      nsCOMPtr<nsINode> n = curNode_;
      curBlockQuoteIsIndentedWithCSS = false;
      // keep looking up the hierarchy as long as we don't hit the body or the
      // active editing host or a table element (other than an entire table)
      while (!n->IsHTMLElement(nsGkAtoms::body) && mHTMLEditor &&
             mHTMLEditor->IsDescendantOfEditorRoot(n) &&
             (n->IsHTMLElement(nsGkAtoms::table) ||
              !nsHTMLEditUtils::IsTableElement(n))) {
        if (!n->GetParentNode()) {
          break;
        }
        n = n->GetParentNode();
        if (n->IsHTMLElement(nsGkAtoms::blockquote)) {
          // if so, remember it, and remember first node we are taking out of it.
          curBlockQuote = GetAsDOMNode(n);
          firstBQChild  = curNode;
          lastBQChild   = curNode;
          break;
        }
        else if (useCSS)
        {
          NS_ENSURE_STATE(mHTMLEditor);
          nsIAtom* marginProperty = MarginPropertyAtomForIndent(mHTMLEditor->mHTMLCSSUtils, curNode);
          nsAutoString value;
          NS_ENSURE_STATE(mHTMLEditor);
          mHTMLEditor->mHTMLCSSUtils->GetSpecifiedProperty(*n, *marginProperty,
                                                           value);
          float f;
          nsCOMPtr<nsIAtom> unit;
          NS_ENSURE_STATE(mHTMLEditor);
          mHTMLEditor->mHTMLCSSUtils->ParseLength(value, &f, getter_AddRefs(unit));
          if (f > 0 && !(nsHTMLEditUtils::IsList(curParent) && nsHTMLEditUtils::IsList(curNode)))
          {
            curBlockQuote = GetAsDOMNode(n);
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
            NS_ENSURE_STATE(mHTMLEditor);
            res = mHTMLEditor->RemoveBlockContainer(curNode);
            NS_ENSURE_SUCCESS(res, res);
          }
          // handled list item case above
        }
        else if (nsHTMLEditUtils::IsList(curNode)) // node is a list, but parent is non-list: move list items out
        {
          nsCOMPtr<nsIDOMNode> childDOM;
          curNode->GetLastChild(getter_AddRefs(childDOM));
          nsCOMPtr<nsIContent> child = do_QueryInterface(childDOM);
          NS_ENSURE_STATE(!childDOM || child);
          while (child)
          {
            if (nsHTMLEditUtils::IsListItem(child))
            {
              bool bOutOfList;
              res = PopListItem(GetAsDOMNode(child), &bOutOfList);
              NS_ENSURE_SUCCESS(res, res);
            }
            else if (nsHTMLEditUtils::IsList(child))
            {
              // We have an embedded list, so move it out from under the
              // parent list. Be sure to put it after the parent list
              // because this loop iterates backwards through the parent's
              // list of children.

              NS_ENSURE_STATE(mHTMLEditor);
              res = mHTMLEditor->MoveNode(child, curParent, offset + 1);
              NS_ENSURE_SUCCESS(res, res);
            }
            else
            {
              // delete any non- list items for now
              NS_ENSURE_STATE(mHTMLEditor);
              res = mHTMLEditor->DeleteNode(child);
              NS_ENSURE_SUCCESS(res, res);
            }
            curNode->GetLastChild(getter_AddRefs(childDOM));
            child = do_QueryInterface(childDOM);
            NS_ENSURE_STATE(!childDOM || child);
          }
          // delete the now-empty list
          NS_ENSURE_STATE(mHTMLEditor);
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
      int32_t sOffset;
      NS_ENSURE_STATE(mHTMLEditor);
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
      NS_ENSURE_STATE(mHTMLEditor);
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

  NS_ENSURE_STATE(mHTMLEditor);
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
  int32_t startOffset, endOffset, offset;
  nsresult res;

  // get split point location
  nsCOMPtr<nsIDOMNode> startParent = nsEditor::GetNodeLocation(aStartChild, &startOffset);
  
  // do the splits!
  NS_ENSURE_STATE(mHTMLEditor);
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
  NS_ENSURE_STATE(mHTMLEditor);
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
  if (aIsBlockIndentedWithCSS) {
    res = RelativeChangeIndentationOfElementNode(middleNode, -1);
  } else {
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->RemoveBlockContainer(middleNode);
  }
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
  nsCOMPtr<Element> list = do_QueryInterface(aList);
  NS_ENSURE_STATE(list);

  nsCOMPtr<dom::Element> outNode;
  nsresult rv = ConvertListType(list, getter_AddRefs(outNode), aListType, aItemType);
  *outList = outNode ? outNode->AsDOMNode() : nullptr;
  return rv;
}

nsresult
nsHTMLEditRules::ConvertListType(Element* aList,
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
      if (nsHTMLEditUtils::IsListItem(element) &&
          !element->IsHTMLElement(aItemType)) {
        child = mHTMLEditor->ReplaceContainer(element, aItemType);
        NS_ENSURE_STATE(child);
      } else if (nsHTMLEditUtils::IsList(element) &&
                 !element->IsHTMLElement(aListType)) {
        nsCOMPtr<dom::Element> temp;
        nsresult rv = ConvertListType(child->AsElement(), getter_AddRefs(temp),
                                      aListType, aItemType);
        NS_ENSURE_SUCCESS(rv, rv);
        child = temp.forget();
      }
    }
    child = child->GetNextSibling();
  }

  if (aList->IsHTMLElement(aListType)) {
    nsCOMPtr<dom::Element> list = aList->AsElement();
    list.forget(aOutList);
    return NS_OK;
  }

  *aOutList = mHTMLEditor->ReplaceContainer(aList, aListType).take();
  NS_ENSURE_STATE(aOutList);

  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// CreateStyleForInsertText:  take care of clearing and setting appropriate
//                            style nodes for text insertion.
//
//
nsresult
nsHTMLEditRules::CreateStyleForInsertText(Selection* aSelection,
                                          nsIDOMDocument *aDoc)
{
  MOZ_ASSERT(aSelection && aDoc && mHTMLEditor->mTypeInState);

  bool weDidSomething = false;
  nsCOMPtr<nsIDOMNode> node, tmp;
  int32_t offset;
  NS_ENSURE_STATE(mHTMLEditor);
  nsresult res = mHTMLEditor->GetStartNodeAndOffset(aSelection,
                                                    getter_AddRefs(node),
                                                    &offset);
  NS_ENSURE_SUCCESS(res, res);

  // next examine our present style and make sure default styles are either
  // present or explicitly overridden.  If neither, add the default style to
  // the TypeInState
  int32_t length = mHTMLEditor->mDefaultStyles.Length();
  for (int32_t j = 0; j < length; j++) {
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
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->GetInlinePropertyBase(*propItem->tag, &propItem->attr,
                                             nullptr, &bFirst, &bAny, &bAll,
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
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->ClearStyle(address_of(node), &offset,
                                  item->tag, &item->attr);
    NS_ENSURE_SUCCESS(res, res);
    item = mHTMLEditor->mTypeInState->TakeClearProperty();
    weDidSomething = true;
  }

  // then process setting any styles
  int32_t relFontSize = mHTMLEditor->mTypeInState->TakeRelativeFontSize();
  item = mHTMLEditor->mTypeInState->TakeSetProperty();

  if (item || relFontSize) {
    // we have at least one style to add; make a new text node to insert style
    // nodes above.
    if (mHTMLEditor->IsTextNode(node)) {
      // if we are in a text node, split it
      NS_ENSURE_STATE(mHTMLEditor);
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
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->InsertNode(newNode, node, offset);
    NS_ENSURE_SUCCESS(res, res);
    node = newNode;
    offset = 0;
    weDidSomething = true;

    if (relFontSize) {
      // dir indicated bigger versus smaller.  1 = bigger, -1 = smaller
      int32_t dir = relFontSize > 0 ? 1 : -1;
      for (int32_t j = 0; j < DeprecatedAbs(relFontSize); j++) {
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->RelativeFontChangeOnTextNode(dir, nodeAsText,
                                                        0, -1);
        NS_ENSURE_SUCCESS(res, res);
      }
    }

    while (item) {
      NS_ENSURE_STATE(mHTMLEditor);
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
nsHTMLEditRules::WillAlign(Selection* aSelection,
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
  res = GetNodesFromSelection(aSelection, EditAction::align, arrayOfNodes);
  NS_ENSURE_SUCCESS(res, res);

  // if we don't have any nodes, or we have only a single br, then we are
  // creating an empty alignment div.  We have to do some different things for these.
  bool emptyDiv = false;
  int32_t listCount = arrayOfNodes.Count();
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
      int32_t offset;
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(parent), &offset);

      if (!nsHTMLEditUtils::IsTableElement(parent) || nsHTMLEditUtils::IsTableCellOrCaption(parent))
        emptyDiv = true;
    }
  }
  if (emptyDiv)
  {
    nsCOMPtr<nsIDOMNode> brNode, sib;
    NS_NAMED_LITERAL_STRING(divType, "div");

    NS_ENSURE_STATE(aSelection->GetRangeAt(0));
    nsCOMPtr<nsINode> parent = aSelection->GetRangeAt(0)->GetStartParent();
    int32_t offset = aSelection->GetRangeAt(0)->StartOffset();
    NS_ENSURE_STATE(parent);

    res = SplitAsNeeded(*nsGkAtoms::div, parent, offset);
    NS_ENSURE_SUCCESS(res, res);
    // consume a trailing br, if any.  This is to keep an alignment from
    // creating extra lines, if possible.
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->GetNextHTMLNode(parent->AsDOMNode(), offset,
                                       address_of(brNode));
    NS_ENSURE_SUCCESS(res, res);
    if (brNode && nsTextEditUtils::IsBreak(brNode))
    {
      // making use of html structure... if next node after where
      // we are putting our div is not a block, then the br we 
      // found is in same block we are, so its safe to consume it.
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->GetNextHTMLSibling(parent->AsDOMNode(), offset,
                                            address_of(sib));
      NS_ENSURE_SUCCESS(res, res);
      if (!IsBlockNode(sib))
      {
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->DeleteNode(brNode);
        NS_ENSURE_SUCCESS(res, res);
      }
    }
    NS_ENSURE_STATE(mHTMLEditor);
    nsCOMPtr<Element> theDiv = mHTMLEditor->CreateNode(nsGkAtoms::div, parent,
                                                       offset);
    NS_ENSURE_STATE(theDiv);
    // remember our new block for postprocessing
    mNewBlock = theDiv->AsDOMNode();
    // set up the alignment on the div, using HTML or CSS
    nsCOMPtr<nsIDOMElement> divElem = do_QueryInterface(theDiv);
    res = AlignBlock(divElem, alignType, true);
    NS_ENSURE_SUCCESS(res, res);
    *aHandled = true;
    // put in a moz-br so that it won't get deleted
    res = CreateMozBR(theDiv->AsDOMNode(), 0);
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

  nsCOMPtr<nsINode> curParent;
  nsCOMPtr<Element> curDiv;
  bool useCSS = mHTMLEditor->IsCSSEnabled();
  for (int32_t i = 0; i < listCount; ++i) {
    // here's where we actually figure out what to do
    nsCOMPtr<nsIDOMNode> curNode = arrayOfNodes[i];
    nsCOMPtr<nsIContent> curContent = do_QueryInterface(curNode);
    NS_ENSURE_STATE(curContent);

    // Ignore all non-editable nodes.  Leave them be.
    if (!mHTMLEditor->IsEditable(curNode)) continue;

    curParent = curContent->GetParentNode();
    int32_t offset = curParent ? curParent->IndexOf(curContent) : -1;

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
    bool isEmptyTextNode = false;
    if (nsEditor::IsTextNode(curNode) &&
       ((nsHTMLEditUtils::IsTableElement(curParent) &&
         !nsHTMLEditUtils::IsTableCellOrCaption(GetAsDOMNode(curParent))) ||
        nsHTMLEditUtils::IsList(curParent) ||
        (NS_SUCCEEDED(mHTMLEditor->IsEmptyNode(curNode, &isEmptyTextNode)) && isEmptyTextNode)))
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
        int32_t count;
        mHTMLEditor->mHTMLCSSUtils->SetCSSEquivalentToHTMLStyle(curNode, nullptr,
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
      if (!mEditor->CanContainTag(*curParent, *nsGkAtoms::div)) {
        return NS_OK; // cancelled
      }

      res = SplitAsNeeded(*nsGkAtoms::div, curParent, offset);
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_STATE(mHTMLEditor);
      curDiv = mHTMLEditor->CreateNode(nsGkAtoms::div, curParent, offset);
      NS_ENSURE_STATE(curDiv);
      // remember our new block for postprocessing
      mNewBlock = curDiv->AsDOMNode();
      // set up the alignment on the div
      nsCOMPtr<nsIDOMElement> divElem = do_QueryInterface(curDiv);
      res = AlignBlock(divElem, alignType, true);
      //nsAutoString attr(NS_LITERAL_STRING("align"));
      //res = mHTMLEditor->SetAttribute(divElem, attr, *alignType);
      //NS_ENSURE_SUCCESS(res, res);
      // curDiv is now the correct thing to put curNode in
    }

    // tuck the node into the end of the active div
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->MoveNode(curContent, curDiv, -1);
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
  int32_t listCount = arrayOfNodes.Count();
  int32_t j;

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
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node && alignType, NS_ERROR_NULL_POINTER);
  nsresult res = NS_OK;
  nsCOMPtr<nsIContent> firstChild, lastChild;
  nsCOMPtr<Element> divNode;
  
  bool useCSS = mHTMLEditor->IsCSSEnabled();

  NS_ENSURE_STATE(mHTMLEditor);
  firstChild = mHTMLEditor->GetFirstEditableChild(*node);
  NS_ENSURE_STATE(mHTMLEditor);
  lastChild = mHTMLEditor->GetLastEditableChild(*node);
  NS_NAMED_LITERAL_STRING(attr, "align");
  if (!firstChild)
  {
    // this cell has no content, nothing to align
  } else if (firstChild == lastChild &&
             firstChild->IsHTMLElement(nsGkAtoms::div)) {
    // the cell already has a div containing all of its content: just
    // act on this div.
    nsCOMPtr<nsIDOMElement> divElem = do_QueryInterface(firstChild);
    if (useCSS) {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->SetAttributeOrEquivalent(divElem, attr, *alignType, false); 
    }
    else {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->SetAttribute(divElem, attr, *alignType);
    }
    NS_ENSURE_SUCCESS(res, res);
  }
  else
  {
    // else we need to put in a div, set the alignment, and toss in all the children
    NS_ENSURE_STATE(mHTMLEditor);
    divNode = mHTMLEditor->CreateNode(nsGkAtoms::div, node, 0);
    NS_ENSURE_STATE(divNode);
    // set up the alignment on the div
    nsCOMPtr<nsIDOMElement> divElem = do_QueryInterface(divNode);
    if (useCSS) {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->SetAttributeOrEquivalent(divElem, attr, *alignType, false); 
    }
    else {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->SetAttribute(divElem, attr, *alignType);
    }
    NS_ENSURE_SUCCESS(res, res);
    // tuck the children into the end of the active div
    while (lastChild && (lastChild != divNode))
    {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->MoveNode(lastChild, divNode, 0);
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_STATE(mHTMLEditor);
      lastChild = mHTMLEditor->GetLastEditableChild(*node);
    }
  }
  return res;
}

///////////////////////////////////////////////////////////////////////////
// CheckForEmptyBlock: Called by WillDeleteSelection to detect and handle
//                     case of deleting from inside an empty block.
//
nsresult
nsHTMLEditRules::CheckForEmptyBlock(nsINode* aStartNode,
                                    Element* aBodyNode,
                                    Selection* aSelection,
                                    bool* aHandled)
{
  // If the editing host is an inline element, bail out early.
  if (IsInlineNode(GetAsDOMNode(aBodyNode))) {
    return NS_OK;
  }
  // If we are inside an empty block, delete it.  Note: do NOT delete table
  // elements this way.
  nsCOMPtr<Element> block;
  if (IsBlockNode(GetAsDOMNode(aStartNode))) {
    block = aStartNode->AsElement();
  } else {
    block = mHTMLEditor->GetBlockNodeParent(aStartNode);
  }
  bool bIsEmptyNode;
  nsCOMPtr<Element> emptyBlock;
  nsresult res;
  if (block && block != aBodyNode) {
    // Efficiency hack, avoiding IsEmptyNode() call when in body
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->IsEmptyNode(block, &bIsEmptyNode, true, false);
    NS_ENSURE_SUCCESS(res, res);
    while (block && bIsEmptyNode && !nsHTMLEditUtils::IsTableElement(block) &&
           block != aBodyNode) {
      emptyBlock = block;
      block = mHTMLEditor->GetBlockNodeParent(emptyBlock);
      NS_ENSURE_STATE(mHTMLEditor);
      if (block) {
        res = mHTMLEditor->IsEmptyNode(block, &bIsEmptyNode, true, false);
        NS_ENSURE_SUCCESS(res, res);
      }
    }
  }

  if (emptyBlock && emptyBlock->IsEditable()) {
    nsCOMPtr<nsINode> blockParent = emptyBlock->GetParentNode();
    NS_ENSURE_TRUE(blockParent, NS_ERROR_FAILURE);
    int32_t offset = blockParent->IndexOf(emptyBlock);

    if (nsHTMLEditUtils::IsListItem(emptyBlock)) {
      // Are we the first list item in the list?
      bool bIsFirst;
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->IsFirstEditableChild(GetAsDOMNode(emptyBlock),
                                              &bIsFirst);
      NS_ENSURE_SUCCESS(res, res);
      if (bIsFirst) {
        nsCOMPtr<nsINode> listParent = blockParent->GetParentNode();
        NS_ENSURE_TRUE(listParent, NS_ERROR_FAILURE);
        int32_t listOffset = listParent->IndexOf(blockParent);
        // If we are a sublist, skip the br creation
        if (!nsHTMLEditUtils::IsList(listParent)) {
          // Create a br before list
          NS_ENSURE_STATE(mHTMLEditor);
          nsCOMPtr<Element> br =
            mHTMLEditor->CreateBR(listParent, listOffset);
          NS_ENSURE_STATE(br);
          // Adjust selection to be right before it
          res = aSelection->Collapse(listParent, listOffset);
          NS_ENSURE_SUCCESS(res, res);
        }
        // Else just let selection percolate up.  We'll adjust it in
        // AfterEdit()
      }
    } else {
      // Adjust selection to be right after it
      res = aSelection->Collapse(blockParent, offset + 1);
      NS_ENSURE_SUCCESS(res, res);
    }
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->DeleteNode(emptyBlock);
    *aHandled = true;
    NS_ENSURE_SUCCESS(res, res);
  }
  return NS_OK;
}

nsresult
nsHTMLEditRules::CheckForInvisibleBR(nsIDOMNode *aBlock, 
                                     BRLocation aWhere, 
                                     nsCOMPtr<nsIDOMNode> *outBRNode,
                                     int32_t aOffset)
{
  nsCOMPtr<nsINode> block = do_QueryInterface(aBlock);
  NS_ENSURE_TRUE(block && outBRNode, NS_ERROR_NULL_POINTER);
  *outBRNode = nullptr;

  nsCOMPtr<nsIDOMNode> testNode;
  int32_t testOffset = 0;
  bool runTest = false;

  if (aWhere == kBlockEnd)
  {
    nsCOMPtr<nsIDOMNode> rightmostNode =
      // no block crossing
      GetAsDOMNode(mHTMLEditor->GetRightmostChild(block, true));

    if (rightmostNode)
    {
      int32_t nodeOffset;
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
    if (WSType::br == wsTester.mStartReason) {
      *outBRNode = GetAsDOMNode(wsTester.mStartReasonNode);
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
                                 int32_t *aIndex, bool aList, bool aTbl)
{
  nsCOMPtr<nsINode> aNode_ = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(aNode_ && aIndex, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMNode> node =
    GetAsDOMNode(mHTMLEditor->GetFirstEditableChild(*aNode_));
  nsresult res = NS_OK;
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
nsHTMLEditRules::ExpandSelectionForDeletion(Selection* aSelection)
{
  NS_ENSURE_TRUE(aSelection, NS_ERROR_NULL_POINTER);
  
  // don't need to touch collapsed selections
  if (aSelection->Collapsed()) {
    return NS_OK;
  }

  int32_t rangeCount;
  nsresult res = aSelection->GetRangeCount(&rangeCount);
  NS_ENSURE_SUCCESS(res, res);
  
  // we don't need to mess with cell selections, and we assume multirange selections are those.
  if (rangeCount != 1) return NS_OK;
  
  // find current sel start and end
  nsRefPtr<nsRange> range = aSelection->GetRangeAt(0);
  NS_ENSURE_TRUE(range, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIDOMNode> selStartNode, selEndNode, selCommon;
  int32_t selStartOffset, selEndOffset;
  
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
  NS_ENSURE_STATE(selCommon);

  // set up for loops and cache our root element
  bool stillLooking = true;
  nsCOMPtr<nsIDOMNode> firstBRParent;
  nsCOMPtr<nsINode> unused;
  int32_t visOffset=0, firstBROffset=0;
  WSType wsType;
  nsCOMPtr<nsIContent> rootContent = mHTMLEditor->GetActiveEditingHost();
  nsCOMPtr<nsIDOMNode> rootElement = do_QueryInterface(rootContent);
  NS_ENSURE_TRUE(rootElement, NS_ERROR_FAILURE);

  // find previous visible thingy before start of selection
  if ((selStartNode!=selCommon) && (selStartNode!=rootElement))
  {
    while (stillLooking)
    {
      nsWSRunObject wsObj(mHTMLEditor, selStartNode, selStartOffset);
      nsCOMPtr<nsINode> selStartNode_(do_QueryInterface(selStartNode));
      wsObj.PriorVisibleNode(selStartNode_, selStartOffset, address_of(unused),
                             &visOffset, &wsType);
      if (wsType == WSType::thisBlock) {
        // we want to keep looking up.  But stop if we are crossing table element
        // boundaries, or if we hit the root.
        if (nsHTMLEditUtils::IsTableElement(wsObj.mStartReasonNode) ||
            selCommon == GetAsDOMNode(wsObj.mStartReasonNode) ||
            rootElement == GetAsDOMNode(wsObj.mStartReasonNode)) {
          stillLooking = false;
        }
        else
        { 
          selStartNode = nsEditor::GetNodeLocation(GetAsDOMNode(wsObj.mStartReasonNode),
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
      nsCOMPtr<nsINode> selEndNode_(do_QueryInterface(selEndNode));
      wsObj.NextVisibleNode(selEndNode_, selEndOffset, address_of(unused),
                            &visOffset, &wsType);
      if (wsType == WSType::br) {
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
          selEndNode = nsEditor::GetNodeLocation(GetAsDOMNode(wsObj.mEndReasonNode), &selEndOffset);
          ++selEndOffset;
        }
      } else if (wsType == WSType::thisBlock) {
        // we want to keep looking up.  But stop if we are crossing table element
        // boundaries, or if we hit the root.
        if (nsHTMLEditUtils::IsTableElement(wsObj.mEndReasonNode) ||
            selCommon == GetAsDOMNode(wsObj.mEndReasonNode) ||
            rootElement == GetAsDOMNode(wsObj.mEndReasonNode)) {
          stillLooking = false;
        }
        else
        { 
          selEndNode = nsEditor::GetNodeLocation(GetAsDOMNode(wsObj.mEndReasonNode), &selEndOffset);
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
    nsCOMPtr<nsINode> node = do_QueryInterface(selStartNode);
    NS_ENSURE_STATE(node);
    nsRefPtr<nsRange> range = new nsRange(node);
    res = range->SetStart(selStartNode, selStartOffset);
    NS_ENSURE_SUCCESS(res, res);
    res = range->SetEnd(selEndNode, selEndOffset);
    NS_ENSURE_SUCCESS(res, res);
    
    // check if block is entirely inside range
    nsCOMPtr<nsIContent> brContentBlock = do_QueryInterface(brBlock);
    if (brContentBlock) {
      res = nsRange::CompareNodeToRange(brContentBlock, range, &nodeBefore,
                                        &nodeAfter);
    }
    
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
nsHTMLEditRules::NormalizeSelection(Selection* inSelection)
{
  NS_ENSURE_TRUE(inSelection, NS_ERROR_NULL_POINTER);

  // don't need to touch collapsed selections
  if (inSelection->Collapsed()) {
    return NS_OK;
  }

  int32_t rangeCount;
  nsresult res = inSelection->GetRangeCount(&rangeCount);
  NS_ENSURE_SUCCESS(res, res);
  
  // we don't need to mess with cell selections, and we assume multirange selections are those.
  if (rangeCount != 1) return NS_OK;
  
  nsRefPtr<nsRange> range = inSelection->GetRangeAt(0);
  NS_ENSURE_TRUE(range, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIDOMNode> startNode, endNode;
  int32_t startOffset, endOffset;
  nsCOMPtr<nsIDOMNode> newStartNode, newEndNode;
  int32_t newStartOffset, newEndOffset;
  
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
  nsCOMPtr<nsINode> unused;
  int32_t offset;
  WSType wsType;

  // let the whitespace code do the heavy lifting
  nsWSRunObject wsEndObj(mHTMLEditor, endNode, endOffset);
  // is there any intervening visible whitespace?  if so we can't push selection past that,
  // it would visibly change maening of users selection
  nsCOMPtr<nsINode> endNode_(do_QueryInterface(endNode));
  wsEndObj.PriorVisibleNode(endNode_, endOffset, address_of(unused),
                            &offset, &wsType);
  if (wsType != WSType::text && wsType != WSType::normalWS) {
    // eThisBlock and eOtherBlock conveniently distinquish cases
    // of going "down" into a block and "up" out of a block.
    if (wsEndObj.mStartReason == WSType::otherBlock) {
      // endpoint is just after the close of a block.
      nsCOMPtr<nsIDOMNode> child =
        GetAsDOMNode(mHTMLEditor->GetRightmostChild(wsEndObj.mStartReasonNode,
                                                    true));
      if (child)
      {
        newEndNode = nsEditor::GetNodeLocation(child, &newEndOffset);
        ++newEndOffset; // offset *after* child
      }
      // else block is empty - we can leave selection alone here, i think.
    } else if (wsEndObj.mStartReason == WSType::thisBlock) {
      // endpoint is just after start of this block
      nsCOMPtr<nsIDOMNode> child;
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->GetPriorHTMLNode(endNode, endOffset, address_of(child));
      if (child)
      {
        newEndNode = nsEditor::GetNodeLocation(child, &newEndOffset);
        ++newEndOffset; // offset *after* child
      }
      // else block is empty - we can leave selection alone here, i think.
    } else if (wsEndObj.mStartReason == WSType::br) {
      // endpoint is just after break.  lets adjust it to before it.
      newEndNode = nsEditor::GetNodeLocation(GetAsDOMNode(wsEndObj.mStartReasonNode),
                                             &newEndOffset);
    }
  }
  
  
  // similar dealio for start of range
  nsWSRunObject wsStartObj(mHTMLEditor, startNode, startOffset);
  // is there any intervening visible whitespace?  if so we can't push selection past that,
  // it would visibly change maening of users selection
  nsCOMPtr<nsINode> startNode_(do_QueryInterface(startNode));
  wsStartObj.NextVisibleNode(startNode_, startOffset, address_of(unused),
                             &offset, &wsType);
  if (wsType != WSType::text && wsType != WSType::normalWS) {
    // eThisBlock and eOtherBlock conveniently distinquish cases
    // of going "down" into a block and "up" out of a block.
    if (wsStartObj.mEndReason == WSType::otherBlock) {
      // startpoint is just before the start of a block.
      nsCOMPtr<nsIDOMNode> child =
        GetAsDOMNode(mHTMLEditor->GetLeftmostChild(wsStartObj.mEndReasonNode,
                                                   true));
      if (child)
      {
        newStartNode = nsEditor::GetNodeLocation(child, &newStartOffset);
      }
      // else block is empty - we can leave selection alone here, i think.
    } else if (wsStartObj.mEndReason == WSType::thisBlock) {
      // startpoint is just before end of this block
      nsCOMPtr<nsIDOMNode> child;
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->GetNextHTMLNode(startNode, startOffset, address_of(child));
      if (child)
      {
        newStartNode = nsEditor::GetNodeLocation(child, &newStartOffset);
      }
      // else block is empty - we can leave selection alone here, i think.
    } else if (wsStartObj.mEndReason == WSType::br) {
      // startpoint is just before a break.  lets adjust it to after it.
      newStartNode = nsEditor::GetNodeLocation(GetAsDOMNode(wsStartObj.mEndReasonNode),
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
  
  int16_t comp;
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
                                  int32_t aOffset,
                                  EditAction actionID,
                                  nsCOMPtr<nsIDOMNode>* outNode,
                                  int32_t* outOffset)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  MOZ_ASSERT(node && outNode && outOffset);

  // default values
  *outNode = node->AsDOMNode();
  *outOffset = aOffset;

  // we do one thing for text actions, something else entirely for other
  // actions
  if (actionID == EditAction::insertText ||
      actionID == EditAction::insertIMEText ||
      actionID == EditAction::insertBreak ||
      actionID == EditAction::deleteText) {
    bool isSpace, isNBSP;
    nsCOMPtr<nsIContent> content = do_QueryInterface(node), temp;
    // for text actions, we want to look backwards (or forwards, as
    // appropriate) for additional whitespace or nbsp's.  We may have to act on
    // these later even though they are outside of the initial selection.  Even
    // if they are in another node!
    while (content) {
      int32_t offset;
      if (aWhere == kStart) {
        NS_ENSURE_TRUE(mHTMLEditor, /* void */);
        mHTMLEditor->IsPrevCharInNodeWhitespace(content, *outOffset,
                                                &isSpace, &isNBSP,
                                                getter_AddRefs(temp), &offset);
      } else {
        NS_ENSURE_TRUE(mHTMLEditor, /* void */);
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

  int32_t offset = aOffset;

  // else not a text section.  In this case we want to see if we should grab
  // any adjacent inline nodes and/or parents and other ancestors
  if (aWhere == kStart) {
    // some special casing for text nodes
    if (node->IsNodeOfType(nsINode::eTEXT)) {
      if (!node->GetParentNode()) {
        // Okay, can't promote any further
        return;
      }
      offset = node->GetParentNode()->IndexOf(node);
      node = node->GetParentNode();
    }

    // look back through any further inline nodes that aren't across a <br>
    // from us, and that are enclosed in the same block.
    NS_ENSURE_TRUE(mHTMLEditor, /* void */);
    nsCOMPtr<nsINode> priorNode =
      mHTMLEditor->GetPriorHTMLNode(node, offset, true);

    while (priorNode && priorNode->GetParentNode() &&
           mHTMLEditor && !mHTMLEditor->IsVisBreak(priorNode->AsDOMNode()) &&
           !IsBlockNode(priorNode->AsDOMNode())) {
      offset = priorNode->GetParentNode()->IndexOf(priorNode);
      node = priorNode->GetParentNode();
      NS_ENSURE_TRUE(mHTMLEditor, /* void */);
      priorNode = mHTMLEditor->GetPriorHTMLNode(node, offset, true);
    }

    // finding the real start for this point.  look up the tree for as long as
    // we are the first node in the container, and as long as we haven't hit
    // the body node.
    NS_ENSURE_TRUE(mHTMLEditor, /* void */);
    nsCOMPtr<nsIContent> nearNode =
      mHTMLEditor->GetPriorHTMLNode(node, offset, true);
    while (!nearNode && !node->IsHTMLElement(nsGkAtoms::body) &&
           node->GetParentNode()) {
      // some cutoffs are here: we don't need to also include them in the
      // aWhere == kEnd case.  as long as they are in one or the other it will
      // work.  special case for outdent: don't keep looking up if we have
      // found a blockquote element to act on
      if (actionID == EditAction::outdent &&
          node->IsHTMLElement(nsGkAtoms::blockquote)) {
        break;
      }

      int32_t parentOffset = node->GetParentNode()->IndexOf(node);
      nsCOMPtr<nsINode> parent = node->GetParentNode();

      // Don't walk past the editable section. Note that we need to check
      // before walking up to a parent because we need to return the parent
      // object, so the parent itself might not be in the editable area, but
      // it's OK if we're not performing a block-level action.
      bool blockLevelAction = actionID == EditAction::indent ||
                              actionID == EditAction::outdent ||
                              actionID == EditAction::align ||
                              actionID == EditAction::makeBasicBlock;
      NS_ENSURE_TRUE(mHTMLEditor, /* void */);
      if (!mHTMLEditor->IsDescendantOfEditorRoot(parent) &&
          (blockLevelAction || !mHTMLEditor ||
           !mHTMLEditor->IsDescendantOfEditorRoot(node))) {
        NS_ENSURE_TRUE(mHTMLEditor, /* void */);
        break;
      }

      node = parent;
      offset = parentOffset;
      NS_ENSURE_TRUE(mHTMLEditor, /* void */);
      nearNode = mHTMLEditor->GetPriorHTMLNode(node, offset, true);
    }
    *outNode = node->AsDOMNode();
    *outOffset = offset;
    return;
  }

  // aWhere == kEnd
  // some special casing for text nodes
  if (node->IsNodeOfType(nsINode::eTEXT)) {
    if (!node->GetParentNode()) {
      // Okay, can't promote any further
      return;
    }
    // want to be after the text node
    offset = 1 + node->GetParentNode()->IndexOf(node);
    node = node->GetParentNode();
  }

  // look ahead through any further inline nodes that aren't across a <br> from
  // us, and that are enclosed in the same block.
  NS_ENSURE_TRUE(mHTMLEditor, /* void */);
  nsCOMPtr<nsIContent> nextNode =
    mHTMLEditor->GetNextHTMLNode(node, offset, true);

  while (nextNode && !IsBlockNode(nextNode->AsDOMNode()) &&
         nextNode->GetParentNode()) {
    offset = 1 + nextNode->GetParentNode()->IndexOf(nextNode);
    node = nextNode->GetParentNode();
    NS_ENSURE_TRUE(mHTMLEditor, /* void */);
    if (mHTMLEditor->IsVisBreak(nextNode->AsDOMNode())) {
      break;
    }
    NS_ENSURE_TRUE(mHTMLEditor, /* void */);
    nextNode = mHTMLEditor->GetNextHTMLNode(node, offset, true);
  }

  // finding the real end for this point.  look up the tree for as long as we
  // are the last node in the container, and as long as we haven't hit the body
  // node.
  NS_ENSURE_TRUE(mHTMLEditor, /* void */);
  nsCOMPtr<nsIContent> nearNode =
    mHTMLEditor->GetNextHTMLNode(node, offset, true);
  while (!nearNode && !node->IsHTMLElement(nsGkAtoms::body) &&
         node->GetParentNode()) {
    int32_t parentOffset = node->GetParentNode()->IndexOf(node);
    nsCOMPtr<nsINode> parent = node->GetParentNode();

    // Don't walk past the editable section. Note that we need to check before
    // walking up to a parent because we need to return the parent object, so
    // the parent itself might not be in the editable area, but it's OK.
    if ((!mHTMLEditor || !mHTMLEditor->IsDescendantOfEditorRoot(node)) &&
        (!mHTMLEditor || !mHTMLEditor->IsDescendantOfEditorRoot(parent))) {
      NS_ENSURE_TRUE(mHTMLEditor, /* void */);
      break;
    }

    node = parent;
    // we want to be AFTER nearNode
    offset = parentOffset + 1;
    NS_ENSURE_TRUE(mHTMLEditor, /* void */);
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
nsHTMLEditRules::GetPromotedRanges(Selection* inSelection, 
                                   nsTArray<nsRefPtr<nsRange>>& outArrayOfRanges, 
                                   EditAction inOperationType)
{
  NS_ENSURE_TRUE(inSelection, NS_ERROR_NULL_POINTER);

  int32_t rangeCount;
  nsresult res = inSelection->GetRangeCount(&rangeCount);
  NS_ENSURE_SUCCESS(res, res);
  
  int32_t i;
  nsRefPtr<nsRange> selectionRange;
  nsRefPtr<nsRange> opRange;

  for (i = 0; i < rangeCount; i++)
  {
    selectionRange = inSelection->GetRangeAt(i);
    NS_ENSURE_STATE(selectionRange);

    // clone range so we don't muck with actual selection ranges
    opRange = selectionRange->CloneRange();

    // make a new adjusted range to represent the appropriate block content.
    // The basic idea is to push out the range endpoints
    // to truly enclose the blocks that we will affect.
    // This call alters opRange.
    res = PromoteRange(opRange, inOperationType);
    NS_ENSURE_SUCCESS(res, res);
      
    // stuff new opRange into array
    outArrayOfRanges.AppendElement(opRange);
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// PromoteRange: expand a range to include any parents for which all
//               editable children are already in range. 
//                       
nsresult 
nsHTMLEditRules::PromoteRange(nsRange* inRange, EditAction inOperationType)
{
  NS_ENSURE_TRUE(inRange, NS_ERROR_NULL_POINTER);
  nsresult res;
  nsCOMPtr<nsIDOMNode> startNode, endNode;
  int32_t startOffset, endOffset;
  
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
    if (IsBlockNode(startNode)) {
      block = startNode;
    } else {
      NS_ENSURE_STATE(mHTMLEditor);
      block = mHTMLEditor->GetBlockNodeParent(startNode);
    }
    if (block)
    {
      bool bIsEmptyNode = false;
      // check for the editing host
      NS_ENSURE_STATE(mHTMLEditor);
      nsIContent *rootContent = mHTMLEditor->GetActiveEditingHost();
      nsCOMPtr<nsINode> rootNode = do_QueryInterface(rootContent);
      nsCOMPtr<nsINode> blockNode = do_QueryInterface(block);
      NS_ENSURE_TRUE(rootNode && blockNode, NS_ERROR_UNEXPECTED);
      // Make sure we don't go higher than our root element in the content tree
      if (!nsContentUtils::ContentIsDescendantOf(rootNode, blockNode))
      {
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->IsEmptyNode(block, &bIsEmptyNode, true, false);
      }
      if (bIsEmptyNode)
      {
        uint32_t numChildren;
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
  int32_t opStartOffset, opEndOffset;
  nsRefPtr<nsRange> opRange;
  
  GetPromotedPoint(kStart, startNode, startOffset, inOperationType,
                   address_of(opStartNode), &opStartOffset);
  GetPromotedPoint(kEnd, endNode, endOffset, inOperationType,
                   address_of(opEndNode), &opEndOffset);

  // Make sure that the new range ends up to be in the editable section.
  NS_ENSURE_STATE(mHTMLEditor);
  if (!mHTMLEditor->IsDescendantOfEditorRoot(nsEditor::GetNodeAtRangeOffsetPoint(opStartNode, opStartOffset)) ||
      !mHTMLEditor || // Check again, since it may have gone away
      !mHTMLEditor->IsDescendantOfEditorRoot(nsEditor::GetNodeAtRangeOffsetPoint(opEndNode, opEndOffset - 1))) {
    NS_ENSURE_STATE(mHTMLEditor);
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
  explicit nsUniqueFunctor(nsCOMArray<nsIDOMNode> &aArray) : mArray(aArray)
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
nsHTMLEditRules::GetNodesForOperation(nsTArray<nsRefPtr<nsRange>>& inArrayOfRanges, 
                                      nsCOMArray<nsIDOMNode>& outArrayOfNodes, 
                                      EditAction inOperationType,
                                      bool aDontTouchContent)
{
  int32_t rangeCount = inArrayOfRanges.Length();
  
  int32_t i;
  nsRefPtr<nsRange> opRange;

  nsresult res = NS_OK;
  
  // bust up any inlines that cross our range endpoints,
  // but only if we are allowed to touch content.
  
  if (!aDontTouchContent)
  {
    nsTArray<nsRefPtr<nsRangeStore>> rangeItemArray;
    rangeItemArray.AppendElements(rangeCount);

    NS_ASSERTION(static_cast<uint32_t>(rangeCount) == rangeItemArray.Length(),
                 "How did that happen?");

    // first register ranges for special editor gravity
    for (i = 0; i < rangeCount; i++)
    {
      opRange = inArrayOfRanges[0];
      rangeItemArray[i] = new nsRangeStore();
      rangeItemArray[i]->StoreRange(opRange);
      NS_ENSURE_STATE(mHTMLEditor);
      mHTMLEditor->mRangeUpdater.RegisterRangeItem(rangeItemArray[i]);
      inArrayOfRanges.RemoveElementAt(0);
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
      NS_ENSURE_STATE(mHTMLEditor);
      mHTMLEditor->mRangeUpdater.DropRangeItem(item);
      opRange = item->GetRange();
      inArrayOfRanges.AppendElement(opRange);
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
  if (inOperationType == EditAction::makeBasicBlock) {
    int32_t listCount = outArrayOfNodes.Count();
    for (i=listCount-1; i>=0; i--)
    {
      nsCOMPtr<nsIDOMNode> node = outArrayOfNodes[i];
      if (nsHTMLEditUtils::IsListItem(node))
      {
        int32_t j=i;
        outArrayOfNodes.RemoveObjectAt(i);
        res = GetInnerContent(node, outArrayOfNodes, &j);
        NS_ENSURE_SUCCESS(res, res);
      }
    }
  }
  // indent/outdent already do something special for list items, but
  // we still need to make sure we don't act on table elements
  else if (inOperationType == EditAction::outdent ||
           inOperationType == EditAction::indent ||
           inOperationType == EditAction::setAbsolutePosition) {
    int32_t listCount = outArrayOfNodes.Count();
    for (i=listCount-1; i>=0; i--)
    {
      nsCOMPtr<nsIDOMNode> node = outArrayOfNodes[i];
      if (nsHTMLEditUtils::IsTableElementButNotTable(node))
      {
        int32_t j=i;
        outArrayOfNodes.RemoveObjectAt(i);
        res = GetInnerContent(node, outArrayOfNodes, &j);
        NS_ENSURE_SUCCESS(res, res);
      }
    }
  }
  // outdent should look inside of divs.
  if (inOperationType == EditAction::outdent &&
      (!mHTMLEditor || !mHTMLEditor->IsCSSEnabled())) {
    NS_ENSURE_STATE(mHTMLEditor);
    int32_t listCount = outArrayOfNodes.Count();
    for (i=listCount-1; i>=0; i--)
    {
      nsCOMPtr<nsIDOMNode> node = outArrayOfNodes[i];
      if (nsHTMLEditUtils::IsDiv(node))
      {
        int32_t j=i;
        outArrayOfNodes.RemoveObjectAt(i);
        res = GetInnerContent(node, outArrayOfNodes, &j, false, false);
        NS_ENSURE_SUCCESS(res, res);
      }
    }
  }


  // post process the list to break up inline containers that contain br's.
  // but only for operations that might care, like making lists or para's...
  if (inOperationType == EditAction::makeBasicBlock ||
      inOperationType == EditAction::makeList ||
      inOperationType == EditAction::align ||
      inOperationType == EditAction::setAbsolutePosition ||
      inOperationType == EditAction::indent ||
      inOperationType == EditAction::outdent) {
    int32_t listCount = outArrayOfNodes.Count();
    for (i=listCount-1; i>=0; i--)
    {
      nsCOMPtr<nsIDOMNode> node = outArrayOfNodes[i];
      if (!aDontTouchContent && IsInlineNode(node) &&
          (!mHTMLEditor || mHTMLEditor->IsContainer(node)) &&
          (!mHTMLEditor || !mHTMLEditor->IsTextNode(node)))
      {
        NS_ENSURE_STATE(mHTMLEditor);
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
  
  NS_ENSURE_STATE(mHTMLEditor);
  nsRefPtr<Selection> selection = mHTMLEditor->GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);
  // added this in so that ui code can ask to change an entire list, even if selection
  // is only in part of it.  used by list item dialog.
  if (aEntireList)
  {       
    uint32_t rangeCount = selection->RangeCount();
    for (uint32_t rangeIdx = 0; rangeIdx < rangeCount; ++rangeIdx) {
      nsRefPtr<nsRange> range = selection->GetRangeAt(rangeIdx);
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
    NS_ENSURE_STATE(mHTMLEditor);
    nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);

    // contruct a list of nodes to act on.
    res = GetNodesFromSelection(selection, EditAction::makeList,
                                outArrayOfNodes, aDontTouchContent);
    NS_ENSURE_SUCCESS(res, res);
  }
               
  // pre process our list of nodes...                      
  int32_t listCount = outArrayOfNodes.Count();
  int32_t i;
  for (i=listCount-1; i>=0; i--)
  {
    nsCOMPtr<nsIDOMNode> testNode = outArrayOfNodes[i];

    // Remove all non-editable nodes.  Leave them be.
    NS_ENSURE_STATE(mHTMLEditor);
    if (!mHTMLEditor->IsEditable(testNode))
    {
      outArrayOfNodes.RemoveObjectAt(i);
    }
    
    // scan for table elements and divs.  If we find table elements other than table,
    // replace it with a list of any editable non-table content.
    if (nsHTMLEditUtils::IsTableElementButNotTable(testNode))
    {
      int32_t j=i;
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
  int32_t listCount = aNodeArray.Count();
  if (listCount != 1) {
    return NS_OK;
  }

  nsCOMPtr<nsINode> curNode = do_QueryInterface(aNodeArray[0]);
  NS_ENSURE_STATE(curNode);

  while (curNode->IsElement() &&
         (curNode->IsHTMLElement(nsGkAtoms::div) ||
          nsHTMLEditUtils::IsList(curNode) ||
          curNode->IsHTMLElement(nsGkAtoms::blockquote))) {
    // dive as long as there is only one child, and it is a list, div, blockquote
    NS_ENSURE_STATE(mHTMLEditor);
    uint32_t numChildren = mHTMLEditor->CountEditableChildren(curNode);
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
    if (!element->IsHTMLElement(nsGkAtoms::div) &&
        !nsHTMLEditUtils::IsList(element) &&
        !element->IsHTMLElement(nsGkAtoms::blockquote)) {
      break;
    }

    // check editablility XXX floppy moose
    curNode = tmp;
  }

  // we've found innermost list/blockquote/div: 
  // replace the one node in the array with these nodes
  aNodeArray.RemoveObjectAt(0);
  if (curNode->IsAnyOfHTMLElements(nsGkAtoms::div,
                                   nsGkAtoms::blockquote)) {
    int32_t j = 0;
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
  MOZ_ASSERT(aElement->IsHTMLElement(nsGkAtoms::dl));
  MOZ_ASSERT(aDT);
  MOZ_ASSERT(aDD);

  *aDT = *aDD = false;
  for (nsIContent* child = aElement->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (child->IsHTMLElement(nsGkAtoms::dt)) {
      *aDT = true;
    } else if (child->IsHTMLElement(nsGkAtoms::dd)) {
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
  NS_ENSURE_STATE(mHTMLEditor);
  nsRefPtr<Selection> selection = mHTMLEditor->GetSelection();
  NS_ENSURE_STATE(selection);

  // contruct a list of nodes to act on.
  nsresult res = GetNodesFromSelection(selection, EditAction::makeBasicBlock,
                                       outArrayOfNodes, aDontTouchContent);
  NS_ENSURE_SUCCESS(res, res);

  // pre process our list of nodes...                      
  int32_t listCount = outArrayOfNodes.Count();
  int32_t i;
  for (i=listCount-1; i>=0; i--)
  {
    nsCOMPtr<nsIDOMNode> testNode = outArrayOfNodes[i];

    // Remove all non-editable nodes.  Leave them be.
    NS_ENSURE_STATE(mHTMLEditor);
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
      int32_t j=i;
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

  nsCOMPtr<nsIDOMNode> endInline =
    GetHighestInlineParent(GetAsDOMNode(item.endNode));
  
  // if we have inline parents above range endpoints, split them
  if (endInline && !isCollapsed)
  {
    nsCOMPtr<nsIDOMNode> resultEndNode;
    int32_t resultEndOffset;
    endInline->GetParentNode(getter_AddRefs(resultEndNode));
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->SplitNodeDeep(endInline, GetAsDOMNode(item.endNode),
                                     item.endOffset, &resultEndOffset, true);
    NS_ENSURE_SUCCESS(res, res);
    // reset range
    item.endNode = do_QueryInterface(resultEndNode);
    item.endOffset = resultEndOffset;
  }

  nsCOMPtr<nsIDOMNode> startInline =
    GetHighestInlineParent(GetAsDOMNode(item.startNode));

  if (startInline)
  {
    nsCOMPtr<nsIDOMNode> resultStartNode;
    int32_t resultStartOffset;
    startInline->GetParentNode(getter_AddRefs(resultStartNode));
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->SplitNodeDeep(startInline, GetAsDOMNode(item.startNode),
                                     item.startOffset, &resultStartOffset,
                                     true);
    NS_ENSURE_SUCCESS(res, res);
    // reset range
    item.startNode = do_QueryInterface(resultStartNode);
    item.startOffset = resultStartOffset;
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
  nsCOMPtr<nsINode> node = do_QueryInterface(inNode);
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);

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
  int32_t listCount = arrayOfBreaks.Count();
  if (!listCount)
  {
    if (!outArrayOfNodes.AppendObject(inNode))
      return NS_ERROR_FAILURE;
  }
  else
  {
    // else we need to bust up inNode along all the breaks
    nsCOMPtr<Element> breakNode;
    nsCOMPtr<nsINode> inlineParentNode = node->GetParentNode();
    nsCOMPtr<nsIDOMNode> leftNode;
    nsCOMPtr<nsIDOMNode> rightNode;
    nsCOMPtr<nsIDOMNode> splitDeepNode = inNode;
    nsCOMPtr<nsIDOMNode> splitParentNode;
    int32_t splitOffset, resultOffset, i;
    
    for (i=0; i< listCount; i++)
    {
      breakNode = do_QueryInterface(arrayOfBreaks[i]);
      NS_ENSURE_TRUE(breakNode, NS_ERROR_NULL_POINTER);
      NS_ENSURE_TRUE(splitDeepNode, NS_ERROR_NULL_POINTER);
      splitParentNode = GetAsDOMNode(nsEditor::GetNodeLocation(breakNode,
                                                               &splitOffset));
      NS_ENSURE_STATE(mHTMLEditor);
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
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->MoveNode(breakNode, inlineParentNode, resultOffset);
      NS_ENSURE_SUCCESS(res, res);
      if (!outArrayOfNodes.AppendObject(GetAsDOMNode(breakNode)))
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
  NS_ENSURE_TRUE(aNode, nullptr);
  if (IsBlockNode(aNode)) return nullptr;
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
nsHTMLEditRules::GetNodesFromPoint(::DOMPoint point,
                                   EditAction operation,
                                   nsCOMArray<nsIDOMNode> &arrayOfNodes,
                                   bool dontTouchContent)
{
  NS_ENSURE_STATE(point.node);
  nsRefPtr<nsRange> range = new nsRange(point.node);
  nsresult res = range->SetStart(point.node, point.offset);
  NS_ENSURE_SUCCESS(res, res);
  
  // expand the range to include adjacent inlines
  res = PromoteRange(range, operation);
  NS_ENSURE_SUCCESS(res, res);
      
  // make array of ranges
  nsTArray<nsRefPtr<nsRange>> arrayOfRanges;
  
  // stuff new opRange into array
  arrayOfRanges.AppendElement(range);
  
  // use these ranges to contruct a list of nodes to act on.
  res = GetNodesForOperation(arrayOfRanges, arrayOfNodes, operation, dontTouchContent); 
  return res;
}


///////////////////////////////////////////////////////////////////////////
// GetNodesFromSelection: given a particular operation, construct a list  
//                     of nodes from the selection that will be operated on. 
//                       
nsresult 
nsHTMLEditRules::GetNodesFromSelection(Selection* selection,
                                       EditAction operation,
                                       nsCOMArray<nsIDOMNode>& arrayOfNodes,
                                       bool dontTouchContent)
{
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  nsresult res;
  
  // promote selection ranges
  nsTArray<nsRefPtr<nsRange>> arrayOfRanges;
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
  uint32_t listCount = inArrayOfNodes.Count();
  inTransitionArray.EnsureLengthAtLeast(listCount);
  uint32_t i;
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

Element*
nsHTMLEditRules::IsInListItem(nsINode* aNode)
{
  NS_ENSURE_TRUE(aNode, nullptr);
  if (nsHTMLEditUtils::IsListItem(aNode)) {
    return aNode->AsElement();
  }

  Element* parent = aNode->GetParentElement();
  while (parent && mHTMLEditor && mHTMLEditor->IsDescendantOfEditorRoot(parent) &&
         !nsHTMLEditUtils::IsTableElement(parent)) {
    if (nsHTMLEditUtils::IsListItem(parent)) {
      return parent;
    }
    parent = parent->GetParentElement();
  }
  return nullptr;
}


///////////////////////////////////////////////////////////////////////////
// ReturnInHeader: do the right thing for returns pressed in headers
//                       
nsresult 
nsHTMLEditRules::ReturnInHeader(Selection* aSelection, 
                                nsIDOMNode *aHeader, 
                                nsIDOMNode *aNode, 
                                int32_t aOffset)
{
  NS_ENSURE_TRUE(aSelection && aHeader && aNode, NS_ERROR_NULL_POINTER);  
  
  // remeber where the header is
  int32_t offset;
  nsCOMPtr<nsIDOMNode> headerParent = nsEditor::GetNodeLocation(aHeader, &offset);

  // get ws code to adjust any ws
  nsCOMPtr<nsINode> selNode(do_QueryInterface(aNode));
  NS_ENSURE_STATE(mHTMLEditor);
  nsresult res = nsWSRunObject::PrepareToSplitAcrossBlocks(mHTMLEditor,
                                                           address_of(selNode),
                                                           &aOffset);
  NS_ENSURE_SUCCESS(res, res);

  // split the header
  int32_t newOffset;
  NS_ENSURE_STATE(mHTMLEditor);
  res = mHTMLEditor->SplitNodeDeep(aHeader, GetAsDOMNode(selNode), aOffset, &newOffset);
  NS_ENSURE_SUCCESS(res, res);

  // if the leftand heading is empty, put a mozbr in it
  nsCOMPtr<nsIDOMNode> prevItem;
  NS_ENSURE_STATE(mHTMLEditor);
  mHTMLEditor->GetPriorHTMLSibling(aHeader, address_of(prevItem));
  if (prevItem && nsHTMLEditUtils::IsHeader(prevItem))
  {
    bool bIsEmptyNode;
    NS_ENSURE_STATE(mHTMLEditor);
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
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->DeleteNode(aHeader);
    NS_ENSURE_SUCCESS(res, res);
    // layout tells the caret to blink in a weird place
    // if we don't place a break after the header.
    nsCOMPtr<nsIDOMNode> sibling;
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->GetNextHTMLSibling(headerParent, offset+1, address_of(sibling));
    NS_ENSURE_SUCCESS(res, res);
    if (!sibling || !nsTextEditUtils::IsBreak(sibling))
    {
      ClearCachedStyles();
      NS_ENSURE_STATE(mHTMLEditor);
      mHTMLEditor->mTypeInState->ClearAllProps();

      // create a paragraph
      NS_NAMED_LITERAL_STRING(pType, "p");
      nsCOMPtr<nsIDOMNode> pNode;
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->CreateNode(pType, headerParent, offset+1, getter_AddRefs(pNode));
      NS_ENSURE_SUCCESS(res, res);

      // append a <br> to it
      nsCOMPtr<nsIDOMNode> brNode;
      NS_ENSURE_STATE(mHTMLEditor);
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
nsHTMLEditRules::ReturnInParagraph(Selection* aSelection,
                                   nsIDOMNode* aPara,
                                   nsIDOMNode* aNode,
                                   int32_t aOffset,
                                   bool* aCancel,
                                   bool* aHandled)
{
  if (!aSelection || !aPara || !aNode || !aCancel || !aHandled) {
    return NS_ERROR_NULL_POINTER;
  }
  *aCancel = false;
  *aHandled = false;
  nsresult res;

  int32_t offset;
  nsCOMPtr<nsIDOMNode> parent = nsEditor::GetNodeLocation(aNode, &offset);

  NS_ENSURE_STATE(mHTMLEditor);
  bool doesCRCreateNewP = mHTMLEditor->GetReturnInParagraphCreatesNewParagraph();

  bool newBRneeded = false;
  nsCOMPtr<nsIDOMNode> sibling;

  NS_ENSURE_STATE(mHTMLEditor);
  if (aNode == aPara && doesCRCreateNewP) {
    // we are at the edges of the block, newBRneeded not needed!
    sibling = aNode;
  } else if (mHTMLEditor->IsTextNode(aNode)) {
    nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(aNode);
    uint32_t strLength;
    res = textNode->GetLength(&strLength);
    NS_ENSURE_SUCCESS(res, res);

    // at beginning of text node?
    if (!aOffset) {
      // is there a BR prior to it?
      NS_ENSURE_STATE(mHTMLEditor);
      mHTMLEditor->GetPriorHTMLSibling(aNode, address_of(sibling));
      if (!sibling || !mHTMLEditor || !mHTMLEditor->IsVisBreak(sibling) ||
          nsTextEditUtils::HasMozAttr(sibling)) {
        NS_ENSURE_STATE(mHTMLEditor);
        newBRneeded = true;
      }
    } else if (aOffset == (int32_t)strLength) {
      // we're at the end of text node...
      // is there a BR after to it?
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->GetNextHTMLSibling(aNode, address_of(sibling));
      if (!sibling || !mHTMLEditor || !mHTMLEditor->IsVisBreak(sibling) ||
          nsTextEditUtils::HasMozAttr(sibling)) {
        NS_ENSURE_STATE(mHTMLEditor);
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
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->GetPriorHTMLNode(aNode, aOffset, address_of(nearNode));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_STATE(mHTMLEditor);
    if (!nearNode || !mHTMLEditor->IsVisBreak(nearNode) ||
        nsTextEditUtils::HasMozAttr(nearNode)) {
      // is there a BR after it?
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->GetNextHTMLNode(aNode, aOffset, address_of(nearNode));
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_STATE(mHTMLEditor);
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
    NS_ENSURE_STATE(mHTMLEditor);
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
                                Selection* aSelection,
                                nsCOMPtr<nsIDOMNode> *aSelNode, 
                                int32_t *aOffset)
{
  NS_ENSURE_TRUE(aPara && aBRNode && aSelNode && *aSelNode && aOffset && aSelection, NS_ERROR_NULL_POINTER);
  nsresult res = NS_OK;
  
  // split para
  int32_t newOffset;
  // get ws code to adjust any ws
  nsCOMPtr<nsIDOMNode> leftPara, rightPara;
  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsINode> selNode(do_QueryInterface(*aSelNode));
  res = nsWSRunObject::PrepareToSplitAcrossBlocks(mHTMLEditor, address_of(selNode), aOffset);
  *aSelNode = GetAsDOMNode(selNode);
  NS_ENSURE_SUCCESS(res, res);
  // split the paragraph
  NS_ENSURE_STATE(mHTMLEditor);
  res = mHTMLEditor->SplitNodeDeep(aPara, *aSelNode, *aOffset, &newOffset, false,
                                   address_of(leftPara), address_of(rightPara));
  NS_ENSURE_SUCCESS(res, res);
  // get rid of the break, if it is visible (otherwise it may be needed to prevent an empty p)
  NS_ENSURE_STATE(mHTMLEditor);
  if (mHTMLEditor->IsVisBreak(aBRNode))
  {
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->DeleteNode(aBRNode);  
    NS_ENSURE_SUCCESS(res, res);
  }

  // remove ID attribute on the paragraph we just created
  nsCOMPtr<nsIDOMElement> rightElt = do_QueryInterface(rightPara);
  NS_ENSURE_STATE(mHTMLEditor);
  res = mHTMLEditor->RemoveAttribute(rightElt, NS_LITERAL_STRING("id"));
  NS_ENSURE_SUCCESS(res, res);

  // check both halves of para to see if we need mozBR
  res = InsertMozBRIfNeeded(leftPara);
  NS_ENSURE_SUCCESS(res, res);
  res = InsertMozBRIfNeeded(rightPara);
  NS_ENSURE_SUCCESS(res, res);

  // selection to beginning of right hand para;
  // look inside any containers that are up front.
  nsCOMPtr<nsINode> rightParaNode = do_QueryInterface(rightPara);
  NS_ENSURE_STATE(mHTMLEditor && rightParaNode);
  nsCOMPtr<nsIDOMNode> child =
    GetAsDOMNode(mHTMLEditor->GetLeftmostChild(rightParaNode, true));
  NS_ENSURE_STATE(mHTMLEditor);
  if (mHTMLEditor->IsTextNode(child) || !mHTMLEditor ||
      mHTMLEditor->IsContainer(child))
  {
    NS_ENSURE_STATE(mHTMLEditor);
    aSelection->Collapse(child,0);
  }
  else
  {
    int32_t offset;
    nsCOMPtr<nsIDOMNode> parent = nsEditor::GetNodeLocation(child, &offset);
    aSelection->Collapse(parent,offset);
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// ReturnInListItem: do the right thing for returns pressed in list items
//                       
nsresult 
nsHTMLEditRules::ReturnInListItem(Selection* aSelection, 
                                  nsIDOMNode *aListItem, 
                                  nsIDOMNode *aNode, 
                                  int32_t aOffset)
{
  nsCOMPtr<Element> listItem = do_QueryInterface(aListItem);
  NS_ENSURE_TRUE(aSelection && listItem && aNode, NS_ERROR_NULL_POINTER);
  nsresult res = NS_OK;
  
  nsCOMPtr<nsIDOMNode> listitem;
  
  // sanity check
  NS_PRECONDITION(true == nsHTMLEditUtils::IsListItem(aListItem),
                  "expected a list item and didn't get one");
  
  // get the listitem parent and the active editing host.
  NS_ENSURE_STATE(mHTMLEditor);
  nsIContent* rootContent = mHTMLEditor->GetActiveEditingHost();
  nsCOMPtr<nsIDOMNode> rootNode = do_QueryInterface(rootContent);
  nsCOMPtr<nsINode> list = listItem->GetParentNode();
  int32_t itemOffset = list ? list->IndexOf(listItem) : -1;

  // if we are in an empty listitem, then we want to pop up out of the list
  // but only if prefs says it's ok and if the parent isn't the active editing host.
  bool isEmpty;
  res = IsEmptyBlock(aListItem, &isEmpty, true, false);
  NS_ENSURE_SUCCESS(res, res);
  if (isEmpty && (rootNode != GetAsDOMNode(list)) &&
      mReturnInEmptyLIKillsList) {
    // get the list offset now -- before we might eventually split the list
    nsCOMPtr<nsINode> listParent = list->GetParentNode();
    int32_t offset = listParent ? listParent->IndexOf(list) : -1;

    // are we the last list item in the list?
    bool bIsLast;
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->IsLastEditableChild(aListItem, &bIsLast);
    NS_ENSURE_SUCCESS(res, res);
    if (!bIsLast)
    {
      // we need to split the list!
      nsCOMPtr<nsIDOMNode> tempNode;
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->SplitNode(GetAsDOMNode(list), itemOffset,
                                   getter_AddRefs(tempNode));
      NS_ENSURE_SUCCESS(res, res);
    }

    // are we in a sublist?
    if (nsHTMLEditUtils::IsList(listParent)) {
      // if so, move this list item out of this list and into the grandparent list
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->MoveNode(listItem, listParent, offset + 1);
      NS_ENSURE_SUCCESS(res, res);
      res = aSelection->Collapse(aListItem,0);
    }
    else
    {
      // otherwise kill this listitem
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->DeleteNode(aListItem);
      NS_ENSURE_SUCCESS(res, res);

      // time to insert a paragraph
      NS_NAMED_LITERAL_STRING(pType, "p");
      nsCOMPtr<nsIDOMNode> pNode;
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->CreateNode(pType, GetAsDOMNode(listParent),
                                    offset + 1, getter_AddRefs(pNode));
      NS_ENSURE_SUCCESS(res, res);

      // append a <br> to it
      nsCOMPtr<nsIDOMNode> brNode;
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->CreateBR(pNode, 0, address_of(brNode));
      NS_ENSURE_SUCCESS(res, res);

      // set selection to before the break
      res = aSelection->Collapse(pNode, 0);
    }
    return res;
  }
  
  // else we want a new list item at the same list level.
  // get ws code to adjust any ws
  nsCOMPtr<nsINode> selNode(do_QueryInterface(aNode));
  NS_ENSURE_STATE(mHTMLEditor);
  res = nsWSRunObject::PrepareToSplitAcrossBlocks(mHTMLEditor, address_of(selNode), &aOffset);
  NS_ENSURE_SUCCESS(res, res);
  // now split list item
  int32_t newOffset;
  NS_ENSURE_STATE(mHTMLEditor);
  res = mHTMLEditor->SplitNodeDeep(aListItem, GetAsDOMNode(selNode), aOffset, &newOffset, false);
  NS_ENSURE_SUCCESS(res, res);
  // hack: until I can change the damaged doc range code back to being
  // extra inclusive, I have to manually detect certain list items that
  // may be left empty.
  nsCOMPtr<nsIDOMNode> prevItem;
  NS_ENSURE_STATE(mHTMLEditor);
  mHTMLEditor->GetPriorHTMLSibling(aListItem, address_of(prevItem));

  if (prevItem && nsHTMLEditUtils::IsListItem(prevItem))
  {
    bool bIsEmptyNode;
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->IsEmptyNode(prevItem, &bIsEmptyNode);
    NS_ENSURE_SUCCESS(res, res);
    if (bIsEmptyNode) {
      res = CreateMozBR(prevItem, 0);
      NS_ENSURE_SUCCESS(res, res);
    } else {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->IsEmptyNode(aListItem, &bIsEmptyNode, true);
      NS_ENSURE_SUCCESS(res, res);
      if (bIsEmptyNode) 
      {
        nsCOMPtr<nsIAtom> nodeAtom = nsEditor::GetTag(aListItem);
        if (nodeAtom == nsGkAtoms::dd || nodeAtom == nsGkAtoms::dt) {
          int32_t itemOffset;
          nsCOMPtr<nsIDOMNode> list = nsEditor::GetNodeLocation(aListItem, &itemOffset);

          nsAutoString listTag(nodeAtom == nsGkAtoms::dt
            ? NS_LITERAL_STRING("dd") : NS_LITERAL_STRING("dt"));
          nsCOMPtr<nsIDOMNode> newListItem;
          NS_ENSURE_STATE(mHTMLEditor);
          res = mHTMLEditor->CreateNode(listTag, list, itemOffset+1, getter_AddRefs(newListItem));
          NS_ENSURE_SUCCESS(res, res);
          res = mEditor->DeleteNode(aListItem);
          NS_ENSURE_SUCCESS(res, res);
          return aSelection->Collapse(newListItem, 0);
        }

        nsCOMPtr<nsIDOMNode> brNode;
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->CopyLastEditableChildStyles(prevItem, aListItem, getter_AddRefs(brNode));
        NS_ENSURE_SUCCESS(res, res);
        if (brNode) 
        {
          int32_t offset;
          nsCOMPtr<nsIDOMNode> brParent = nsEditor::GetNodeLocation(brNode, &offset);
          return aSelection->Collapse(brParent, offset);
        }
      }
      else
      {
        NS_ENSURE_STATE(mHTMLEditor);
        nsWSRunObject wsObj(mHTMLEditor, aListItem, 0);
        nsCOMPtr<nsINode> visNode_;
        int32_t visOffset = 0;
        WSType wsType;
        nsCOMPtr<nsINode> aListItem_(do_QueryInterface(aListItem));
        wsObj.NextVisibleNode(aListItem_, 0, address_of(visNode_),
                              &visOffset, &wsType);
        nsCOMPtr<nsIDOMNode> visNode(GetAsDOMNode(visNode_));
        if (wsType == WSType::special || wsType == WSType::br ||
            nsHTMLEditUtils::IsHR(visNode)) {
          int32_t offset;
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
  
  nsCOMPtr<nsIDOMNode> curNode, newBlock;
  nsCOMPtr<nsINode> curParent;
  nsCOMPtr<Element> curBlock;
  int32_t offset;
  int32_t listCount = arrayOfNodes.Count();
  
  nsCOMPtr<nsIDOMNode> prevParent;
  
  int32_t i;
  for (i=0; i<listCount; i++)
  {
    // get the node to act on, and its location
    curNode = arrayOfNodes[i];
    nsCOMPtr<nsIContent> curContent = do_QueryInterface(curNode);
    NS_ENSURE_STATE(curContent);
    curParent = curContent->GetParentNode();
    offset = curParent ? curParent->IndexOf(curContent) : -1;

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
      res = SplitAsNeeded(*nsGkAtoms::blockquote, curParent, offset);
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_STATE(mHTMLEditor);
      curBlock = mHTMLEditor->CreateNode(nsGkAtoms::blockquote, curParent,
                                         offset);
      NS_ENSURE_STATE(curBlock);
      // remember our new block for postprocessing
      mNewBlock = curBlock->AsDOMNode();
      // note: doesn't matter if we set mNewBlock multiple times.
    }
      
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->MoveNode(curContent, curBlock, -1);
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
  int32_t listCount = arrayOfNodes.Count();
  for (int32_t i = 0; i < listCount; ++i) {
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
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->RemoveBlockContainer(curNode); 
      NS_ENSURE_SUCCESS(res, res);
    } else if (curElement &&
               (curElement->IsAnyOfHTMLElements(nsGkAtoms::table,
                                                nsGkAtoms::tr,
                                                nsGkAtoms::tbody,
                                                nsGkAtoms::td,
                                                nsGkAtoms::li,
                                                nsGkAtoms::blockquote,
                                                nsGkAtoms::div) ||
                nsHTMLEditUtils::IsList(curElement))) {
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
      NS_ENSURE_STATE(mHTMLEditor);
      curBlock = mHTMLEditor->GetBlockNodeParent(curNode);
      if (curBlock && nsHTMLEditUtils::IsFormatNode(curBlock)) {
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
  nsCOMPtr<nsIAtom> blockTag = do_GetAtom(*aBlockTag);
  nsresult res = NS_OK;
  
  nsCOMPtr<nsINode> curParent;
  nsCOMPtr<nsIDOMNode> newBlock;
  int32_t offset;
  int32_t listCount = arrayOfNodes.Count();
  nsString tString(*aBlockTag);////MJUDGE SCC NEED HELP

  // Remove all non-editable nodes.  Leave them be.
  int32_t j;
  for (j=listCount-1; j>=0; j--)
  {
    NS_ENSURE_STATE(mHTMLEditor);
    if (!mHTMLEditor->IsEditable(arrayOfNodes[j]))
    {
      arrayOfNodes.RemoveObjectAt(j);
    }
  }
  
  // reset list count
  listCount = arrayOfNodes.Count();
  
  nsCOMPtr<Element> curBlock;
  int32_t i;
  for (i=0; i<listCount; i++)
  {
    // get the node to act on, and its location
    nsCOMPtr<nsIContent> curNode = do_QueryInterface(arrayOfNodes[i]);
    NS_ENSURE_STATE(curNode);
    curParent = curNode->GetParentNode();
    offset = curParent ? curParent->IndexOf(curNode) : -1;
    nsAutoString curNodeTag;
    curNode->NodeInfo()->NameAtom()->ToString(curNodeTag);
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
      NS_ENSURE_STATE(mHTMLEditor);
      nsCOMPtr<Element> element = curNode->AsElement();
      newBlock = dont_AddRef(GetAsDOMNode(
        mHTMLEditor->ReplaceContainer(element, blockTag, nullptr, nullptr,
                                      nsEditor::eCloneAttributes).take()));
      NS_ENSURE_STATE(newBlock);
    }
    else if (nsHTMLEditUtils::IsTable(curNode)                    || 
             (curNodeTag.EqualsLiteral("tbody"))      ||
             (curNodeTag.EqualsLiteral("tr"))         ||
             (curNodeTag.EqualsLiteral("td"))         ||
             nsHTMLEditUtils::IsList(curNode)                     ||
             (curNodeTag.EqualsLiteral("li"))         ||
             curNode->IsAnyOfHTMLElements(nsGkAtoms::blockquote,
                                          nsGkAtoms::div)) {
      curBlock = 0;  // forget any previous block used for previous inline nodes
      // recursion time
      nsCOMArray<nsIDOMNode> childArray;
      res = GetChildNodesForOperation(GetAsDOMNode(curNode), childArray);
      NS_ENSURE_SUCCESS(res, res);
      int32_t childCount = childArray.Count();
      if (childCount)
      {
        res = ApplyBlockStyle(childArray, aBlockTag);
        NS_ENSURE_SUCCESS(res, res);
      }
      else
      {
        // make sure we can put a block here
        res = SplitAsNeeded(*blockTag, curParent, offset);
        NS_ENSURE_SUCCESS(res, res);
        NS_ENSURE_STATE(mHTMLEditor);
        nsCOMPtr<Element> theBlock =
          mHTMLEditor->CreateNode(blockTag, curParent, offset);
        NS_ENSURE_STATE(theBlock);
        // remember our new block for postprocessing
        mNewBlock = theBlock->AsDOMNode();
      }
    }
    
    // if the node is a break, we honor it by putting further nodes in a new parent
    else if (curNodeTag.EqualsLiteral("br"))
    {
      if (curBlock)
      {
        curBlock = 0;  // forget any previous block used for previous inline nodes
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->DeleteNode(curNode);
        NS_ENSURE_SUCCESS(res, res);
      }
      else
      {
        // the break is the first (or even only) node we encountered.  Create a
        // block for it.
        res = SplitAsNeeded(*blockTag, curParent, offset);
        NS_ENSURE_SUCCESS(res, res);
        NS_ENSURE_STATE(mHTMLEditor);
        curBlock = mHTMLEditor->CreateNode(blockTag, curParent, offset);
        NS_ENSURE_STATE(curBlock);
        // remember our new block for postprocessing
        mNewBlock = curBlock->AsDOMNode();
        // note: doesn't matter if we set mNewBlock multiple times.
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->MoveNode(curNode, curBlock, -1);
        NS_ENSURE_SUCCESS(res, res);
      }
        
    
    // if curNode is inline, pull it into curBlock
    // note: it's assumed that consecutive inline nodes in the 
    // arrayOfNodes are actually members of the same block parent.
    // this happens to be true now as a side effect of how
    // arrayOfNodes is contructed, but some additional logic should
    // be added here if that should change
    
    } else if (IsInlineNode(GetAsDOMNode(curNode))) {
      // if curNode is a non editable, drop it if we are going to <pre>
      NS_ENSURE_STATE(mHTMLEditor);
      if (tString.LowerCaseEqualsLiteral("pre") 
        && (!mHTMLEditor->IsEditable(curNode)))
        continue; // do nothing to this block
      
      // if no curBlock, make one
      if (!curBlock)
      {
        res = SplitAsNeeded(*blockTag, curParent, offset);
        NS_ENSURE_SUCCESS(res, res);
        NS_ENSURE_STATE(mHTMLEditor);
        curBlock = mHTMLEditor->CreateNode(blockTag, curParent, offset);
        NS_ENSURE_STATE(curBlock);
        // remember our new block for postprocessing
        mNewBlock = curBlock->AsDOMNode();
        // note: doesn't matter if we set mNewBlock multiple times.
      }
      
      // if curNode is a Break, replace it with a return if we are going to <pre>
      // xxx floppy moose
 
      // this is a continuation of some inline nodes that belong together in
      // the same block item.  use curBlock
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->MoveNode(curNode, curBlock, -1);
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////////
// SplitAsNeeded: Given a tag name, split inOutParent up to the point where we
//                can insert the tag.  Adjust inOutParent and inOutOffset to
//                point to new location for tag.
nsresult
nsHTMLEditRules::SplitAsNeeded(nsIAtom& aTag,
                               nsCOMPtr<nsINode>& inOutParent,
                               int32_t& inOutOffset)
{
  NS_ENSURE_TRUE(inOutParent, NS_ERROR_NULL_POINTER);

  // Check that we have a place that can legally contain the tag
  nsCOMPtr<nsINode> tagParent, splitNode;
  for (nsCOMPtr<nsINode> parent = inOutParent; parent;
       parent = parent->GetParentNode()) {
    // Sniffing up the parent tree until we find a legal place for the block

    // Don't leave the active editing host
    NS_ENSURE_STATE(mHTMLEditor);
    if (!mHTMLEditor->IsDescendantOfEditorRoot(parent)) {
      NS_ENSURE_STATE(mHTMLEditor);
      if (parent != mHTMLEditor->GetActiveEditingHost()) {
        return NS_ERROR_FAILURE;
      }
    }

    NS_ENSURE_STATE(mHTMLEditor);
    if (mHTMLEditor->CanContainTag(*parent, aTag)) {
      // Success
      tagParent = parent;
      break;
    }

    splitNode = parent;
  }
  if (!tagParent) {
    // Could not find a place to build tag!
    return NS_ERROR_FAILURE;
  }
  if (splitNode) {
    // We found a place for block, but above inOutParent. We need to split.
    NS_ENSURE_STATE(mHTMLEditor);
    nsresult res = mHTMLEditor->SplitNodeDeep(splitNode->AsDOMNode(),
                                              inOutParent->AsDOMNode(),
                                              inOutOffset, &inOutOffset);
    NS_ENSURE_SUCCESS(res, res);
    inOutParent = tagParent;
  }
  return NS_OK;
}

/**
 * JoinNodesSmart: Join two nodes, doing whatever makes sense for their
 * children (which often means joining them, too).  aNodeLeft & aNodeRight must
 * be same type of node.
 *
 * Returns the point where they're merged, or (nullptr, -1) on failure.
 */
::DOMPoint
nsHTMLEditRules::JoinNodesSmart(nsIContent& aNodeLeft, nsIContent& aNodeRight)
{
  // Caller responsible for left and right node being the same type
  nsCOMPtr<nsINode> parent = aNodeLeft.GetParentNode();
  NS_ENSURE_TRUE(parent, ::DOMPoint());
  int32_t parOffset = parent->IndexOf(&aNodeLeft);
  nsCOMPtr<nsINode> rightParent = aNodeRight.GetParentNode();

  // If they don't have the same parent, first move the right node to after the
  // left one
  nsresult res;
  if (parent != rightParent) {
    NS_ENSURE_TRUE(mHTMLEditor, ::DOMPoint());
    res = mHTMLEditor->MoveNode(&aNodeRight, parent, parOffset);
    NS_ENSURE_SUCCESS(res, ::DOMPoint());
  }

  ::DOMPoint ret(&aNodeRight, aNodeLeft.Length());

  // Separate join rules for differing blocks
  if (nsHTMLEditUtils::IsList(&aNodeLeft) || aNodeLeft.GetAsText()) {
    // For lists, merge shallow (wouldn't want to combine list items)
    res = mHTMLEditor->JoinNodes(aNodeLeft, aNodeRight);
    NS_ENSURE_SUCCESS(res, ::DOMPoint());
    return ret;
  }

  // Remember the last left child, and first right child
  NS_ENSURE_TRUE(mHTMLEditor, ::DOMPoint());
  nsCOMPtr<nsIContent> lastLeft = mHTMLEditor->GetLastEditableChild(aNodeLeft);
  NS_ENSURE_TRUE(lastLeft, ::DOMPoint());

  NS_ENSURE_TRUE(mHTMLEditor, ::DOMPoint());
  nsCOMPtr<nsIContent> firstRight = mHTMLEditor->GetFirstEditableChild(aNodeRight);
  NS_ENSURE_TRUE(firstRight, ::DOMPoint());

  // For list items, divs, etc., merge smart
  NS_ENSURE_TRUE(mHTMLEditor, ::DOMPoint());
  res = mHTMLEditor->JoinNodes(aNodeLeft, aNodeRight);
  NS_ENSURE_SUCCESS(res, ::DOMPoint());

  if (lastLeft && firstRight && mHTMLEditor &&
      mHTMLEditor->AreNodesSameType(lastLeft, firstRight) &&
      (lastLeft->GetAsText() || !mHTMLEditor ||
       (lastLeft->IsElement() && firstRight->IsElement() &&
        mHTMLEditor->mHTMLCSSUtils->ElementsSameStyle(lastLeft->AsElement(),
                                                  firstRight->AsElement())))) {
    NS_ENSURE_TRUE(mHTMLEditor, ::DOMPoint());
    return JoinNodesSmart(*lastLeft, *firstRight);
  }
  return ret;
}


Element*
nsHTMLEditRules::GetTopEnclosingMailCite(nsINode& aNode)
{
  nsCOMPtr<Element> ret;

  for (nsCOMPtr<nsINode> node = &aNode; node; node = node->GetParentNode()) {
    if ((IsPlaintextEditor() && node->IsHTMLElement(nsGkAtoms::pre)) ||
        nsHTMLEditUtils::IsMailCite(node)) {
      ret = node->AsElement();
    }
    if (node->IsHTMLElement(nsGkAtoms::body)) {
      break;
    }
  }

  return ret;
}


nsresult 
nsHTMLEditRules::CacheInlineStyles(nsIDOMNode *aNode)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);

  NS_ENSURE_STATE(mHTMLEditor);
  bool useCSS = mHTMLEditor->IsCSSEnabled();

  for (int32_t j = 0; j < SIZE_STYLE_TABLE; ++j)
  {
    bool isSet = false;
    nsAutoString outValue;
    // Don't use CSS for <font size>, we don't support it usefully (bug 780035)
    if (!useCSS || (mCachedStyles[j].tag == nsGkAtoms::font &&
                    mCachedStyles[j].attr.EqualsLiteral("size"))) {
      NS_ENSURE_STATE(mHTMLEditor);
      mHTMLEditor->IsTextPropertySetByContent(aNode, mCachedStyles[j].tag,
                                              &(mCachedStyles[j].attr), nullptr,
                                              isSet, &outValue);
    }
    else
    {
      NS_ENSURE_STATE(mHTMLEditor);
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
  NS_ENSURE_STATE(mHTMLEditor);
  bool useCSS = mHTMLEditor->IsCSSEnabled();

  // get selection point; if it doesn't exist, we have nothing to do
  NS_ENSURE_STATE(mHTMLEditor);
  nsRefPtr<Selection> selection = mHTMLEditor->GetSelection();
  MOZ_ASSERT(selection);
  if (!selection->RangeCount()) {
    // Nothing to do
    return NS_OK;
  }
  nsCOMPtr<nsIContent> selNode =
    do_QueryInterface(selection->GetRangeAt(0)->GetStartParent());
  if (!selNode) {
    // Nothing to do
    return NS_OK;
  }

  for (int32_t i = 0; i < SIZE_STYLE_TABLE; ++i) {
    if (mCachedStyles[i].mPresent) {
      bool bFirst, bAny, bAll;
      bFirst = bAny = bAll = false;

      nsAutoString curValue;
      if (useCSS) {
        // check computed style first in css case
        NS_ENSURE_STATE(mHTMLEditor);
        bAny = mHTMLEditor->mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(
          selNode, mCachedStyles[i].tag, &(mCachedStyles[i].attr), curValue,
          nsHTMLCSSUtils::eComputed);
      }
      if (!bAny) {
        // then check typeinstate and html style
        NS_ENSURE_STATE(mHTMLEditor);
        nsresult res = mHTMLEditor->GetInlinePropertyBase(
                                                     *mCachedStyles[i].tag,
                                                     &(mCachedStyles[i].attr),
                                                     &(mCachedStyles[i].value),
                                                     &bFirst, &bAny, &bAll,
                                                     &curValue, false);
        NS_ENSURE_SUCCESS(res, res);
      }
      // this style has disappeared through deletion.  Add to our typeinstate:
      if (!bAny || IsStyleCachePreservingAction(mTheAction)) {
        NS_ENSURE_STATE(mHTMLEditor);
        mHTMLEditor->mTypeInState->SetProp(mCachedStyles[i].tag,
                                           mCachedStyles[i].attr,
                                           mCachedStyles[i].value);
      }
    }
  }

  return NS_OK;
}


void
nsHTMLEditRules::ClearCachedStyles()
{
  // clear the mPresent bits in mCachedStyles array
  for (uint32_t j = 0; j < SIZE_STYLE_TABLE; j++) {
    mCachedStyles[j].mPresent = false;
    mCachedStyles[j].value.Truncate();
  }
}


nsresult 
nsHTMLEditRules::AdjustSpecialBreaks(bool aSafeToAskFrames)
{
  nsCOMArray<nsIDOMNode> arrayOfNodes;
  nsCOMPtr<nsISupports> isupports;
  int32_t nodeCount,j;
  
  // gather list of empty nodes
  NS_ENSURE_STATE(mHTMLEditor);
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
    uint32_t len;
    nsCOMPtr<nsIDOMNode> theNode = arrayOfNodes[0];
    arrayOfNodes.RemoveObjectAt(0);
    res = nsEditor::GetLengthOfDOMNode(theNode, len);
    NS_ENSURE_SUCCESS(res, res);
    res = CreateMozBR(theNode, (int32_t)len);
    NS_ENSURE_SUCCESS(res, res);
  }
  
  return res;
}

nsresult 
nsHTMLEditRules::AdjustWhitespace(Selection* aSelection)
{
  // get selection point
  nsCOMPtr<nsIDOMNode> selNode;
  int32_t selOffset;
  NS_ENSURE_STATE(mHTMLEditor);
  nsresult res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(selNode), &selOffset);
  NS_ENSURE_SUCCESS(res, res);
  
  // ask whitespace object to tweak nbsp's
  NS_ENSURE_STATE(mHTMLEditor);
  return nsWSRunObject(mHTMLEditor, selNode, selOffset).AdjustWhitespace();
}

nsresult 
nsHTMLEditRules::PinSelectionToNewBlock(Selection* aSelection)
{
  NS_ENSURE_TRUE(aSelection, NS_ERROR_NULL_POINTER);
  if (!aSelection->Collapsed()) {
    return NS_OK;
  }

  // get the (collapsed) selection location
  nsCOMPtr<nsIDOMNode> selNode, temp;
  int32_t selOffset;
  NS_ENSURE_STATE(mHTMLEditor);
  nsresult res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(selNode), &selOffset);
  NS_ENSURE_SUCCESS(res, res);
  temp = selNode;
  
  // use ranges and sRangeHelper to compare sel point to new block
  nsCOMPtr<nsINode> node = do_QueryInterface(selNode);
  NS_ENSURE_STATE(node);
  nsRefPtr<nsRange> range = new nsRange(node);
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
    NS_ENSURE_STATE(mHTMLEditor);
    tmp = GetAsDOMNode(mHTMLEditor->GetLastEditableChild(*block));
    uint32_t endPoint;
    NS_ENSURE_STATE(mHTMLEditor);
    if (mHTMLEditor->IsTextNode(tmp) || !mHTMLEditor ||
        mHTMLEditor->IsContainer(tmp))
    {
      NS_ENSURE_STATE(mHTMLEditor);
      res = nsEditor::GetLengthOfDOMNode(tmp, endPoint);
      NS_ENSURE_SUCCESS(res, res);
    }
    else
    {
      tmp = nsEditor::GetNodeLocation(tmp, (int32_t*)&endPoint);
      endPoint++;  // want to be after this node
    }
    return aSelection->Collapse(tmp, (int32_t)endPoint);
  }
  else
  {
    // selection is before block.  put at start of block.
    nsCOMPtr<nsIDOMNode> tmp = mNewBlock;
    NS_ENSURE_STATE(mHTMLEditor);
    tmp = GetAsDOMNode(mHTMLEditor->GetFirstEditableChild(*block));
    int32_t offset;
    if (!(mHTMLEditor->IsTextNode(tmp) || !mHTMLEditor ||
          mHTMLEditor->IsContainer(tmp)))
    {
      tmp = nsEditor::GetNodeLocation(tmp, &offset);
    }
    NS_ENSURE_STATE(mHTMLEditor);
    return aSelection->Collapse(tmp, 0);
  }
}

nsresult 
nsHTMLEditRules::CheckInterlinePosition(Selection* aSelection)
{
  NS_ENSURE_TRUE(aSelection, NS_ERROR_NULL_POINTER);

  // if the selection isn't collapsed, do nothing.
  if (!aSelection->Collapsed()) {
    return NS_OK;
  }

  // get the (collapsed) selection location
  nsCOMPtr<nsIDOMNode> selNode, node;
  int32_t selOffset;
  NS_ENSURE_STATE(mHTMLEditor);
  nsresult res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(selNode), &selOffset);
  NS_ENSURE_SUCCESS(res, res);

  // First, let's check to see if we are after a <br>.  We take care of this
  // special-case first so that we don't accidentally fall through into one
  // of the other conditionals.
  NS_ENSURE_STATE(mHTMLEditor);
  mHTMLEditor->GetPriorHTMLNode(selNode, selOffset, address_of(node), true);
  if (node && nsTextEditUtils::IsBreak(node))
  {
    aSelection->SetInterlinePosition(true);
    return NS_OK;
  }

  // are we after a block?  If so try set caret to following content
  NS_ENSURE_STATE(mHTMLEditor);
  mHTMLEditor->GetPriorHTMLSibling(selNode, selOffset, address_of(node));
  if (node && IsBlockNode(node))
  {
    aSelection->SetInterlinePosition(true);
    return NS_OK;
  }

  // are we before a block?  If so try set caret to prior content
  NS_ENSURE_STATE(mHTMLEditor);
  mHTMLEditor->GetNextHTMLSibling(selNode, selOffset, address_of(node));
  if (node && IsBlockNode(node))
    aSelection->SetInterlinePosition(false);
  return NS_OK;
}

nsresult 
nsHTMLEditRules::AdjustSelection(Selection* aSelection,
                                 nsIEditor::EDirection aAction)
{
  NS_ENSURE_TRUE(aSelection, NS_ERROR_NULL_POINTER);
 
  // if the selection isn't collapsed, do nothing.
  // moose: one thing to do instead is check for the case of
  // only a single break selected, and collapse it.  Good thing?  Beats me.
  if (!aSelection->Collapsed()) {
    return NS_OK;
  }

  // get the (collapsed) selection location
  nsCOMPtr<nsINode> selNode, temp;
  int32_t selOffset;
  NS_ENSURE_STATE(mHTMLEditor);
  nsresult res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(selNode), &selOffset);
  NS_ENSURE_SUCCESS(res, res);
  temp = selNode;
  
  // are we in an editable node?
  NS_ENSURE_STATE(mHTMLEditor);
  while (!mHTMLEditor->IsEditable(selNode))
  {
    // scan up the tree until we find an editable place to be
    selNode = nsEditor::GetNodeLocation(temp, &selOffset);
    NS_ENSURE_TRUE(selNode, NS_ERROR_FAILURE);
    temp = selNode;
    NS_ENSURE_STATE(mHTMLEditor);
  }
  
  // make sure we aren't in an empty block - user will see no cursor.  If this
  // is happening, put a <br> in the block if allowed.
  nsCOMPtr<nsINode> theblock;
  if (IsBlockNode(GetAsDOMNode(selNode))) {
    theblock = selNode;
  } else {
    NS_ENSURE_STATE(mHTMLEditor);
    theblock = mHTMLEditor->GetBlockNodeParent(selNode);
  }
  NS_ENSURE_STATE(mHTMLEditor);
  if (theblock && mHTMLEditor->IsEditable(theblock)) {
    bool bIsEmptyNode;
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->IsEmptyNode(theblock, &bIsEmptyNode, false, false);
    NS_ENSURE_SUCCESS(res, res);
    // check if br can go into the destination node
    NS_ENSURE_STATE(mHTMLEditor);
    if (bIsEmptyNode && mHTMLEditor->CanContainTag(*selNode, *nsGkAtoms::br)) {
      NS_ENSURE_STATE(mHTMLEditor);
      nsCOMPtr<Element> rootNode = mHTMLEditor->GetRoot();
      NS_ENSURE_TRUE(rootNode, NS_ERROR_FAILURE);
      if (selNode == rootNode)
      {
        // Our root node is completely empty. Don't add a <br> here.
        // AfterEditInner() will add one for us when it calls
        // CreateBogusNodeIfNeeded()!
        return NS_OK;
      }

      // we know we can skip the rest of this routine given the cirumstance
      return CreateMozBR(GetAsDOMNode(selNode), selOffset);
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

  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIContent> nearNode =
    mHTMLEditor->GetPriorHTMLNode(selNode, selOffset);
  if (nearNode) 
  {
    // is nearNode also a descendant of same block?
    nsCOMPtr<nsINode> block, nearBlock;
    if (IsBlockNode(GetAsDOMNode(selNode))) {
      block = selNode;
    } else {
      NS_ENSURE_STATE(mHTMLEditor);
      block = mHTMLEditor->GetBlockNodeParent(selNode);
    }
    NS_ENSURE_STATE(mHTMLEditor);
    nearBlock = mHTMLEditor->GetBlockNodeParent(nearNode);
    if (block && block == nearBlock) {
      if (nearNode && nsTextEditUtils::IsBreak(nearNode) )
      {   
        NS_ENSURE_STATE(mHTMLEditor);
        if (!mHTMLEditor->IsVisBreak(nearNode))
        {
          // need to insert special moz BR. Why?  Because if we don't
          // the user will see no new line for the break.  Also, things
          // like table cells won't grow in height.
          nsCOMPtr<nsIDOMNode> brNode;
          res = CreateMozBR(GetAsDOMNode(selNode), selOffset,
                            getter_AddRefs(brNode));
          NS_ENSURE_SUCCESS(res, res);
          nsCOMPtr<nsIDOMNode> brParent =
            nsEditor::GetNodeLocation(brNode, &selOffset);
          // selection stays *before* moz-br, sticking to it
          aSelection->SetInterlinePosition(true);
          res = aSelection->Collapse(brParent, selOffset);
          NS_ENSURE_SUCCESS(res, res);
        }
        else
        {
          NS_ENSURE_STATE(mHTMLEditor);
          nsCOMPtr<nsIContent> nextNode =
            mHTMLEditor->GetNextHTMLNode(nearNode, true);
          if (nextNode && nsTextEditUtils::IsMozBR(nextNode))
          {
            // selection between br and mozbr.  make it stick to mozbr
            // so that it will be on blank line.   
            aSelection->SetInterlinePosition(true);
          }
        }
      }
    }
  }

  // we aren't in a textnode: are we adjacent to text or a break or an image?
  NS_ENSURE_STATE(mHTMLEditor);
  nearNode = mHTMLEditor->GetPriorHTMLNode(selNode, selOffset, true);
  if (nearNode && (nsTextEditUtils::IsBreak(nearNode) ||
                   nsEditor::IsTextNode(nearNode) ||
                   nsHTMLEditUtils::IsImage(nearNode) ||
                   nearNode->IsHTMLElement(nsGkAtoms::hr))) {
    // this is a good place for the caret to be
    return NS_OK;
  }
  NS_ENSURE_STATE(mHTMLEditor);
  nearNode = mHTMLEditor->GetNextHTMLNode(selNode, selOffset, true);
  if (nearNode && (nsTextEditUtils::IsBreak(nearNode) ||
                   nsEditor::IsTextNode(nearNode) ||
                   nearNode->IsAnyOfHTMLElements(nsGkAtoms::img,
                                                 nsGkAtoms::hr))) {
    return NS_OK; // this is a good place for the caret to be
  }

  // look for a nearby text node.
  // prefer the correct direction.
  nsCOMPtr<nsIDOMNode> nearNodeDOM = GetAsDOMNode(nearNode);
  res = FindNearSelectableNode(GetAsDOMNode(selNode), selOffset, aAction,
                               address_of(nearNodeDOM));
  NS_ENSURE_SUCCESS(res, res);
  nearNode = do_QueryInterface(nearNodeDOM);

  if (nearNode)
  {
    // is the nearnode a text node?
    textNode = do_QueryInterface(nearNode);
    if (textNode)
    {
      int32_t offset = 0;
      // put selection in right place:
      if (aAction == nsIEditor::ePrevious)
        textNode->GetLength((uint32_t*)&offset);
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
                                        int32_t aSelOffset, 
                                        nsIEditor::EDirection &aDirection,
                                        nsCOMPtr<nsIDOMNode> *outSelectableNode)
{
  NS_ENSURE_TRUE(aSelNode && outSelectableNode, NS_ERROR_NULL_POINTER);
  *outSelectableNode = nullptr;
  nsresult res = NS_OK;
  
  nsCOMPtr<nsIDOMNode> nearNode, curNode;
  if (aDirection == nsIEditor::ePrevious) {
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->GetPriorHTMLNode(aSelNode, aSelOffset, address_of(nearNode));
  } else {
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->GetNextHTMLNode(aSelNode, aSelOffset, address_of(nearNode));
  }
  NS_ENSURE_SUCCESS(res, res);
  
  if (!nearNode) // try the other direction then
  {
    if (aDirection == nsIEditor::ePrevious)
      aDirection = nsIEditor::eNext;
    else
      aDirection = nsIEditor::ePrevious;
    
    if (aDirection == nsIEditor::ePrevious) {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->GetPriorHTMLNode(aSelNode, aSelOffset, address_of(nearNode));
    } else {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->GetNextHTMLNode(aSelNode, aSelOffset, address_of(nearNode));
    }
    NS_ENSURE_SUCCESS(res, res);
  }
  
  // scan in the right direction until we find an eligible text node,
  // but don't cross any breaks, images, or table elements.
  NS_ENSURE_STATE(mHTMLEditor);
  while (nearNode && !(mHTMLEditor->IsTextNode(nearNode)
                       || nsTextEditUtils::IsBreak(nearNode)
                       || nsHTMLEditUtils::IsImage(nearNode)))
  {
    curNode = nearNode;
    if (aDirection == nsIEditor::ePrevious) {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->GetPriorHTMLNode(curNode, address_of(nearNode));
    } else {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->GetNextHTMLNode(curNode, address_of(nearNode));
    }
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_STATE(mHTMLEditor);
  }
  
  if (nearNode)
  {
    // don't cross any table elements
    if (InDifferentTableElements(nearNode, aSelNode)) {
      return NS_OK;
    }
    
    // otherwise, ok, we have found a good spot to put the selection
    *outSelectableNode = do_QueryInterface(nearNode);
  }
  return res;
}


bool nsHTMLEditRules::InDifferentTableElements(nsIDOMNode* aNode1,
                                               nsIDOMNode* aNode2)
{
  nsCOMPtr<nsINode> node1 = do_QueryInterface(aNode1);
  nsCOMPtr<nsINode> node2 = do_QueryInterface(aNode2);
  return InDifferentTableElements(node1, node2);
}

bool
nsHTMLEditRules::InDifferentTableElements(nsINode* aNode1, nsINode* aNode2)
{
  MOZ_ASSERT(aNode1 && aNode2);

  while (aNode1 && !nsHTMLEditUtils::IsTableElement(aNode1)) {
    aNode1 = aNode1->GetParentNode();
  }

  while (aNode2 && !nsHTMLEditUtils::IsTableElement(aNode2)) {
    aNode2 = aNode2->GetParentNode();
  }

  return aNode1 != aNode2;
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
  nsTArray<nsCOMPtr<nsINode> > skipList;

  // check for empty nodes
  while (!iter->IsDone()) {
    nsINode* node = iter->GetCurrentNode();
    NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

    nsINode* parent = node->GetParentNode();

    size_t idx = skipList.IndexOf(node);
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
        if (element->IsHTMLElement(nsGkAtoms::body)) {
          // don't delete the body
        } else if ((bIsMailCite = nsHTMLEditUtils::IsMailCite(element))  ||
                   element->IsHTMLElement(nsGkAtoms::a)                  ||
                   nsHTMLEditUtils::IsInlineStyle(element)               ||
                   nsHTMLEditUtils::IsList(element)                      ||
                   element->IsHTMLElement(nsGkAtoms::div)) {
          // only consider certain nodes to be empty for purposes of removal
          bIsCandidate = true;
        } else if (nsHTMLEditUtils::IsFormatNode(element) ||
                   nsHTMLEditUtils::IsListItem(element)   ||
                   element->IsHTMLElement(nsGkAtoms::blockquote)) {
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
        NS_ENSURE_STATE(mHTMLEditor);
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
  int32_t nodeCount = arrayOfEmptyNodes.Count();
  for (int32_t j = 0; j < nodeCount; j++) {
    nsCOMPtr<nsIDOMNode> delNode = arrayOfEmptyNodes[0]->AsDOMNode();
    arrayOfEmptyNodes.RemoveObjectAt(0);
    NS_ENSURE_STATE(mHTMLEditor);
    if (mHTMLEditor->IsModifiableNode(delNode)) {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->DeleteNode(delNode);
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  
  // now delete the empty mailcites
  // this is a separate step because we want to pull out any br's and preserve them.
  nodeCount = arrayOfEmptyCites.Count();
  for (int32_t j = 0; j < nodeCount; j++) {
    nsCOMPtr<nsIDOMNode> delNode = arrayOfEmptyCites[0]->AsDOMNode();
    arrayOfEmptyCites.RemoveObjectAt(0);
    bool bIsEmptyNode;
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->IsEmptyNode(delNode, &bIsEmptyNode, false, true);
    NS_ENSURE_SUCCESS(res, res);
    if (!bIsEmptyNode)
    {
      // we are deleting a cite that has just a br.  We want to delete cite, 
      // but preserve br.
      nsCOMPtr<nsIDOMNode> parent, brNode;
      int32_t offset;
      parent = nsEditor::GetNodeLocation(delNode, &offset);
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->CreateBR(parent, offset, address_of(brNode));
      NS_ENSURE_SUCCESS(res, res);
    }
    NS_ENSURE_STATE(mHTMLEditor);
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
  
  NS_ENSURE_STATE(mHTMLEditor);
  nsRefPtr<Selection> selection = mHTMLEditor->GetSelection();
  NS_ENSURE_STATE(selection);
  
  uint32_t rangeCount = selection->RangeCount();
  for (uint32_t rangeIdx = 0; rangeIdx < rangeCount; ++rangeIdx) {
    nsRefPtr<nsRange> range = selection->GetRangeAt(rangeIdx);
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
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////
// IsEmptyInline:  return true if aNode is an empty inline container
//                
//                  
bool 
nsHTMLEditRules::IsEmptyInline(nsIDOMNode *aNode)
{
  if (aNode && IsInlineNode(aNode) && mHTMLEditor &&
      mHTMLEditor->IsContainer(aNode)) 
  {
    bool bEmpty;
    NS_ENSURE_TRUE(mHTMLEditor, false);
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
  int32_t listCount = arrayOfNodes.Count();
  NS_ENSURE_TRUE(listCount, true);
  nsCOMPtr<nsIDOMNode> somenode;
  int32_t j, brCount=0;
  for (j = 0; j < listCount; j++)
  {
    somenode = arrayOfNodes[j];
    NS_ENSURE_TRUE(mHTMLEditor, false);
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
  nsCOMPtr<Element> listItem = do_QueryInterface(aListItem);
  // check parms
  NS_ENSURE_TRUE(listItem && aOutOfList, NS_ERROR_NULL_POINTER);
  
  // init out params
  *aOutOfList = false;
  
  nsCOMPtr<nsINode> curParent = listItem->GetParentNode();
  int32_t offset = curParent ? curParent->IndexOf(listItem) : -1;
    
  if (!nsHTMLEditUtils::IsListItem(listItem)) {
    return NS_ERROR_FAILURE;
  }
    
  // if it's first or last list item, don't need to split the list
  // otherwise we do.
  nsCOMPtr<nsINode> curParPar = curParent->GetParentNode();
  int32_t parOffset = curParPar ? curParPar->IndexOf(curParent) : -1;
  
  bool bIsFirstListItem;
  NS_ENSURE_STATE(mHTMLEditor);
  nsresult res = mHTMLEditor->IsFirstEditableChild(aListItem,
                                                   &bIsFirstListItem);
  NS_ENSURE_SUCCESS(res, res);

  bool bIsLastListItem;
  NS_ENSURE_STATE(mHTMLEditor);
  res = mHTMLEditor->IsLastEditableChild(aListItem, &bIsLastListItem);
  NS_ENSURE_SUCCESS(res, res);
    
  if (!bIsFirstListItem && !bIsLastListItem)
  {
    // split the list
    nsCOMPtr<nsIDOMNode> newBlock;
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->SplitNode(GetAsDOMNode(curParent), offset,
                                 getter_AddRefs(newBlock));
    NS_ENSURE_SUCCESS(res, res);
  }
  
  if (!bIsFirstListItem) parOffset++;
  
  NS_ENSURE_STATE(mHTMLEditor);
  res = mHTMLEditor->MoveNode(listItem, curParPar, parOffset);
  NS_ENSURE_SUCCESS(res, res);
    
  // unwrap list item contents if they are no longer in a list
  if (!nsHTMLEditUtils::IsList(curParPar) &&
      nsHTMLEditUtils::IsListItem(listItem)) {
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->RemoveBlockContainer(GetAsDOMNode(listItem));
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
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->DeleteNode(child);
      NS_ENSURE_SUCCESS(res, res);
    }
    aList->GetFirstChild(getter_AddRefs(child));
  }
  // delete the now-empty list
  NS_ENSURE_STATE(mHTMLEditor);
  res = mHTMLEditor->RemoveBlockContainer(aList);
  NS_ENSURE_SUCCESS(res, res);

  return res;
}


nsresult 
nsHTMLEditRules::ConfirmSelectionInBody()
{
  // get the body  
  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIDOMElement> rootElement = do_QueryInterface(mHTMLEditor->GetRoot());
  NS_ENSURE_TRUE(rootElement, NS_ERROR_UNEXPECTED);

  // get the selection
  NS_ENSURE_STATE(mHTMLEditor);
  nsRefPtr<Selection> selection = mHTMLEditor->GetSelection();
  NS_ENSURE_STATE(selection);
  
  // get the selection start location
  nsCOMPtr<nsIDOMNode> selNode, temp, parent;
  int32_t selOffset;
  NS_ENSURE_STATE(mHTMLEditor);
  nsresult res = mHTMLEditor->GetStartNodeAndOffset(selection,
                                                    getter_AddRefs(selNode),
                                                    &selOffset);
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
  NS_ENSURE_STATE(mHTMLEditor);
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
nsHTMLEditRules::UpdateDocChangeRange(nsRange* aRange)
{
  nsresult res = NS_OK;

  // first make sure aRange is in the document.  It might not be if
  // portions of our editting action involved manipulating nodes
  // prior to placing them in the document (e.g., populating a list item
  // before placing it in its list)
  nsCOMPtr<nsIDOMNode> startNode;
  res = aRange->GetStartContainer(getter_AddRefs(startNode));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_STATE(mHTMLEditor);
  if (!mHTMLEditor->IsDescendantOfRoot(startNode)) {
    // just return - we don't need to adjust mDocChangeRange in this case
    return NS_OK;
  }
  
  if (!mDocChangeRange)
  {
    // clone aRange.
    mDocChangeRange = aRange->CloneRange();
  }
  else
  {
    int16_t result;
    
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
      int32_t startOffset;
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
      int32_t endOffset;
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
  NS_ENSURE_STATE(mHTMLEditor);
  nsresult res = mHTMLEditor->IsEmptyNode(aNode, &isEmpty);
  NS_ENSURE_SUCCESS(res, res);
  if (!isEmpty) {
    return NS_OK;
  }

  return CreateMozBR(aNode, 0);
}

NS_IMETHODIMP 
nsHTMLEditRules::WillCreateNode(const nsAString& aTag, nsIDOMNode *aParent, int32_t aPosition)
{
  return NS_OK;  
}

NS_IMETHODIMP 
nsHTMLEditRules::DidCreateNode(const nsAString& aTag, 
                               nsIDOMNode *aNode, 
                               nsIDOMNode *aParent, 
                               int32_t aPosition, 
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
nsHTMLEditRules::WillInsertNode(nsIDOMNode *aNode, nsIDOMNode *aParent, int32_t aPosition)
{
  return NS_OK;  
}


NS_IMETHODIMP 
nsHTMLEditRules::DidInsertNode(nsIDOMNode *aNode, 
                               nsIDOMNode *aParent, 
                               int32_t aPosition, 
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
nsHTMLEditRules::WillSplitNode(nsIDOMNode *aExistingRightNode, int32_t aOffset)
{
  return NS_OK;  
}


NS_IMETHODIMP 
nsHTMLEditRules::DidSplitNode(nsIDOMNode *aExistingRightNode, 
                              int32_t aOffset, 
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
nsHTMLEditRules::WillInsertText(nsIDOMCharacterData *aTextNode, int32_t aOffset, const nsAString &aString)
{
  return NS_OK;  
}


NS_IMETHODIMP 
nsHTMLEditRules::DidInsertText(nsIDOMCharacterData *aTextNode, 
                                  int32_t aOffset, 
                                  const nsAString &aString, 
                                  nsresult aResult)
{
  if (!mListenerEnabled) {
    return NS_OK;
  }
  int32_t length = aString.Length();
  nsCOMPtr<nsIDOMNode> theNode = do_QueryInterface(aTextNode);
  nsresult res = mUtilRange->SetStart(theNode, aOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = mUtilRange->SetEnd(theNode, aOffset+length);
  NS_ENSURE_SUCCESS(res, res);
  res = UpdateDocChangeRange(mUtilRange);
  return res;  
}


NS_IMETHODIMP 
nsHTMLEditRules::WillDeleteText(nsIDOMCharacterData *aTextNode, int32_t aOffset, int32_t aLength)
{
  return NS_OK;  
}


NS_IMETHODIMP 
nsHTMLEditRules::DidDeleteText(nsIDOMCharacterData *aTextNode, 
                                  int32_t aOffset, 
                                  int32_t aLength, 
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
nsHTMLEditRules::WillDeleteSelection(nsISelection* aSelection)
{
  if (!mListenerEnabled) {
    return NS_OK;
  }
  nsRefPtr<Selection> selection = static_cast<Selection*>(aSelection);
  // get the (collapsed) selection location
  nsCOMPtr<nsIDOMNode> selNode;
  int32_t selOffset;

  NS_ENSURE_STATE(mHTMLEditor);
  nsresult res = mHTMLEditor->GetStartNodeAndOffset(selection,
                                                    getter_AddRefs(selNode),
                                                    &selOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = mUtilRange->SetStart(selNode, selOffset);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_STATE(mHTMLEditor);
  res = mHTMLEditor->GetEndNodeAndOffset(selection, getter_AddRefs(selNode),
                                         &selOffset);
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

  NS_ENSURE_STATE(mHTMLEditor);
  if (mHTMLEditor->IsTextNode(aNode) || nsHTMLEditUtils::IsTable(aNode)) return NS_OK;
  nsresult res = NS_OK;

  nsCOMPtr<nsIDOMNode> child = aNode,tmp;
  if (aChildrenOnly)
  {
    aNode->GetFirstChild(getter_AddRefs(child));
  }
  NS_ENSURE_STATE(mHTMLEditor);
  bool useCSS = mHTMLEditor->IsCSSEnabled();

  while (child)
  {
    if (aChildrenOnly) {
      // get the next sibling right now because we could have to remove child
      child->GetNextSibling(getter_AddRefs(tmp));
    }
    else
    {
      tmp = nullptr;
    }
    bool isBlock;
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->NodeIsBlockStatic(child, &isBlock);
    NS_ENSURE_SUCCESS(res, res);

    if (nsEditor::NodeIsType(child, nsGkAtoms::center)) {
      // the current node is a CENTER element
      // first remove children's alignment
      res = RemoveAlignment(child, aAlignType, true);
      NS_ENSURE_SUCCESS(res, res);

      // we may have to insert BRs in first and last position of element's children
      // if the nodes before/after are not blocks and not BRs
      res = MakeSureElemStartsOrEndsOnCR(child);
      NS_ENSURE_SUCCESS(res, res);

      // now remove the CENTER container
      NS_ENSURE_STATE(mHTMLEditor);
      nsCOMPtr<Element> childAsElement = do_QueryInterface(child);
      NS_ENSURE_STATE(childAsElement);
      res = mHTMLEditor->RemoveContainer(childAsElement);
      NS_ENSURE_SUCCESS(res, res);
    }
    else if (isBlock || nsHTMLEditUtils::IsHR(child))
    {
      // the current node is a block element
      nsCOMPtr<nsIDOMElement> curElem = do_QueryInterface(child);
      if (nsHTMLEditUtils::SupportsAlignAttr(child))
      {
        // remove the ALIGN attribute if this element can have it
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->RemoveAttribute(curElem, NS_LITERAL_STRING("align"));
        NS_ENSURE_SUCCESS(res, res);
      }
      if (useCSS)
      {
        if (nsHTMLEditUtils::IsTable(child) || nsHTMLEditUtils::IsHR(child))
        {
          NS_ENSURE_STATE(mHTMLEditor);
          res = mHTMLEditor->SetAttributeOrEquivalent(curElem, NS_LITERAL_STRING("align"), aAlignType, false); 
        }
        else
        {
          nsAutoString dummyCssValue;
          NS_ENSURE_STATE(mHTMLEditor);
          res = mHTMLEditor->mHTMLCSSUtils->RemoveCSSInlineStyle(child,
            nsGkAtoms::textAlign, dummyCssValue);
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
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMNode> child;
  nsresult res;
  if (aStarts)
  {
    NS_ENSURE_STATE(mHTMLEditor);
    child = GetAsDOMNode(mHTMLEditor->GetFirstEditableChild(*node));
  }
  else
  {
    NS_ENSURE_STATE(mHTMLEditor);
    child = GetAsDOMNode(mHTMLEditor->GetLastEditableChild(*node));
  }
  NS_ENSURE_TRUE(child, NS_OK);
  bool isChildBlock;
  NS_ENSURE_STATE(mHTMLEditor);
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
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->GetPriorHTMLSibling(aNode, address_of(sibling));
    }
    else
    {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->GetNextHTMLSibling(aNode, address_of(sibling));
    }
    NS_ENSURE_SUCCESS(res, res);
    if (sibling)
    {
      bool isBlock;
      NS_ENSURE_STATE(mHTMLEditor);
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
    int32_t offset = 0;
    if (!aStarts) {
      nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
      NS_ENSURE_STATE(node);
      offset = node->GetChildCount();
    }
    nsCOMPtr<nsIDOMNode> brNode;
    NS_ENSURE_STATE(mHTMLEditor);
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
  NS_ENSURE_STATE(mHTMLEditor);
  if (mHTMLEditor->IsCSSEnabled()) {
    // let's use CSS alignment; we use margin-left and margin-right for tables
    // and text-align for other block-level elements
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->SetAttributeOrEquivalent(aElement, attr, *aAlignType, false); 
    NS_ENSURE_SUCCESS(res, res);
  }
  else {
    // HTML case; this code is supposed to be called ONLY if the element
    // supports the align attribute but we'll never know...
    if (nsHTMLEditUtils::SupportsAlignAttr(node)) {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->SetAttribute(aElement, attr, *aAlignType);
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  return NS_OK;
}

nsresult
nsHTMLEditRules::RelativeChangeIndentationOfElementNode(nsIDOMNode *aNode, int8_t aRelativeChange)
{
  NS_ENSURE_ARG_POINTER(aNode);

  if (aRelativeChange != 1 && aRelativeChange != -1) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsCOMPtr<Element> element = do_QueryInterface(aNode);
  if (!element) {
    return NS_OK;
  }

  NS_ENSURE_STATE(mHTMLEditor);
  nsIAtom* marginProperty =
    MarginPropertyAtomForIndent(mHTMLEditor->mHTMLCSSUtils,
                                GetAsDOMNode(element));
  nsAutoString value;
  NS_ENSURE_STATE(mHTMLEditor);
  mHTMLEditor->mHTMLCSSUtils->GetSpecifiedProperty(*element, *marginProperty,
                                                   value);
  float f;
  nsCOMPtr<nsIAtom> unit;
  NS_ENSURE_STATE(mHTMLEditor);
  mHTMLEditor->mHTMLCSSUtils->ParseLength(value, &f, getter_AddRefs(unit));
  if (0 == f) {
    nsAutoString defaultLengthUnit;
    NS_ENSURE_STATE(mHTMLEditor);
    mHTMLEditor->mHTMLCSSUtils->GetDefaultLengthUnit(defaultLengthUnit);
    unit = do_GetAtom(defaultLengthUnit);
  }
  if        (nsGkAtoms::in == unit) {
            f += NS_EDITOR_INDENT_INCREMENT_IN * aRelativeChange;
  } else if (nsGkAtoms::cm == unit) {
            f += NS_EDITOR_INDENT_INCREMENT_CM * aRelativeChange;
  } else if (nsGkAtoms::mm == unit) {
            f += NS_EDITOR_INDENT_INCREMENT_MM * aRelativeChange;
  } else if (nsGkAtoms::pt == unit) {
            f += NS_EDITOR_INDENT_INCREMENT_PT * aRelativeChange;
  } else if (nsGkAtoms::pc == unit) {
            f += NS_EDITOR_INDENT_INCREMENT_PC * aRelativeChange;
  } else if (nsGkAtoms::em == unit) {
            f += NS_EDITOR_INDENT_INCREMENT_EM * aRelativeChange;
  } else if (nsGkAtoms::ex == unit) {
            f += NS_EDITOR_INDENT_INCREMENT_EX * aRelativeChange;
  } else if (nsGkAtoms::px == unit) {
            f += NS_EDITOR_INDENT_INCREMENT_PX * aRelativeChange;
  } else if (nsGkAtoms::percentage == unit) {
            f += NS_EDITOR_INDENT_INCREMENT_PERCENT * aRelativeChange;    
  }

  if (0 < f) {
    nsAutoString newValue;
    newValue.AppendFloat(f);
    newValue.Append(nsDependentAtomString(unit));
    NS_ENSURE_STATE(mHTMLEditor);
    mHTMLEditor->mHTMLCSSUtils->SetCSSProperty(*element, *marginProperty,
                                               newValue);
    return NS_OK;
  }

  NS_ENSURE_STATE(mHTMLEditor);
  mHTMLEditor->mHTMLCSSUtils->RemoveCSSProperty(*element, *marginProperty,
                                                value);

  // remove unnecessary DIV blocks:
  // we could skip this section but that would cause a FAIL in
  // editor/libeditor/tests/browserscope/richtext.html, which expects
  // to unapply a CSS "indent" (<div style="margin-left: 40px;">) by
  // removing the DIV container instead of just removing the CSS property.
  nsCOMPtr<dom::Element> node = do_QueryInterface(aNode);
  if (!node || !node->IsHTMLElement(nsGkAtoms::div) ||
      !mHTMLEditor ||
      node == mHTMLEditor->GetActiveEditingHost() ||
      !mHTMLEditor->IsDescendantOfEditorRoot(node) ||
      nsHTMLEditor::HasAttributes(node)) {
    NS_ENSURE_STATE(mHTMLEditor);
    return NS_OK;
  }

  NS_ENSURE_STATE(mHTMLEditor);
  return mHTMLEditor->RemoveContainer(node);
}

//
// Support for Absolute Positioning
//

nsresult
nsHTMLEditRules::WillAbsolutePosition(Selection* aSelection,
                                      bool* aCancel, bool* aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  nsresult res = WillInsert(aSelection, aCancel);
  NS_ENSURE_SUCCESS(res, res);

  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = false;
  *aHandled = true;
  
  nsCOMPtr<nsIDOMElement> focusElement;
  NS_ENSURE_STATE(mHTMLEditor);
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
  NS_ENSURE_STATE(mHTMLEditor);
  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);
  
  // convert the selection ranges into "promoted" selection ranges:
  // this basically just expands the range to include the immediate
  // block parent, and then further expands to include any ancestors
  // whose children are all in the range
  
  nsTArray<nsRefPtr<nsRange>> arrayOfRanges;
  res = GetPromotedRanges(aSelection, arrayOfRanges,
                          EditAction::setAbsolutePosition);
  NS_ENSURE_SUCCESS(res, res);
  
  // use these ranges to contruct a list of nodes to act on.
  nsCOMArray<nsIDOMNode> arrayOfNodes;
  res = GetNodesForOperation(arrayOfRanges, arrayOfNodes,
                             EditAction::setAbsolutePosition);
  NS_ENSURE_SUCCESS(res, res);                                 
                                     
  // if nothing visible in list, make an empty block
  if (ListIsEmptyLine(arrayOfNodes))
  {
    // get selection location
    NS_ENSURE_STATE(aSelection->RangeCount());
    nsCOMPtr<nsINode> parent = aSelection->GetRangeAt(0)->GetStartParent();
    int32_t offset = aSelection->GetRangeAt(0)->StartOffset();
    NS_ENSURE_STATE(parent);

    // make sure we can put a block here
    res = SplitAsNeeded(*nsGkAtoms::div, parent, offset);
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_STATE(mHTMLEditor);
    nsCOMPtr<Element> thePositionedDiv =
      mHTMLEditor->CreateNode(nsGkAtoms::div, parent, offset);
    NS_ENSURE_STATE(thePositionedDiv);
    // remember our new block for postprocessing
    mNewBlock = thePositionedDiv->AsDOMNode();
    // delete anything that was in the list of nodes
    for (int32_t j = arrayOfNodes.Count() - 1; j >= 0; --j) 
    {
      nsCOMPtr<nsIDOMNode> curNode = arrayOfNodes[0];
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->DeleteNode(curNode);
      NS_ENSURE_SUCCESS(res, res);
      arrayOfNodes.RemoveObjectAt(0);
    }
    // put selection in new block
    res = aSelection->Collapse(thePositionedDiv,0);
    selectionResetter.Abort();  // to prevent selection reseter from overriding us.
    *aHandled = true;
    return res;
  }

  // Ok, now go through all the nodes and put them in a blockquote, 
  // or whatever is appropriate.  Wohoo!
  int32_t i;
  nsCOMPtr<nsINode> curParent;
  nsCOMPtr<nsIDOMNode> indentedLI, sibling;
  nsCOMPtr<Element> curList, curPositionedDiv;
  int32_t listCount = arrayOfNodes.Count();
  for (i=0; i<listCount; i++)
  {
    // here's where we actually figure out what to do
    nsCOMPtr<nsIContent> curNode = do_QueryInterface(arrayOfNodes[i]);
    NS_ENSURE_STATE(curNode);

    // Ignore all non-editable nodes.  Leave them be.
    NS_ENSURE_STATE(mHTMLEditor);
    if (!mHTMLEditor->IsEditable(curNode)) continue;

    curParent = curNode->GetParentNode();
    int32_t offset = curParent ? curParent->IndexOf(curNode) : -1;
     
    // some logic for putting list items into nested lists...
    if (nsHTMLEditUtils::IsList(curParent))
    {
      // check to see if curList is still appropriate.  Which it is if
      // curNode is still right after it in the same list.
      if (curList)
      {
        NS_ENSURE_STATE(mHTMLEditor);
        sibling = GetAsDOMNode(mHTMLEditor->GetPriorHTMLSibling(curNode));
      }
      
      if (!curList || (sibling && sibling != GetAsDOMNode(curList))) {
        // create a new nested list of correct type
        res = SplitAsNeeded(*curParent->NodeInfo()->NameAtom(), curParent,
                            offset);
        NS_ENSURE_SUCCESS(res, res);
        if (!curPositionedDiv) {
          nsCOMPtr<nsINode> curParentParent = curParent->GetParentNode();
          int32_t parentOffset = curParentParent
            ? curParentParent->IndexOf(curParent) : -1;
          NS_ENSURE_STATE(mHTMLEditor);
          curPositionedDiv = mHTMLEditor->CreateNode(nsGkAtoms::div, curParentParent,
                                                     parentOffset);
          mNewBlock = GetAsDOMNode(curPositionedDiv);
        }
        NS_ENSURE_STATE(mHTMLEditor);
        curList = mHTMLEditor->CreateNode(curParent->NodeInfo()->NameAtom(),
                                          curPositionedDiv, -1);
        NS_ENSURE_STATE(curList);
        // curList is now the correct thing to put curNode in
        // remember our new block for postprocessing
        // mNewBlock = curList;
      }
      // tuck the node into the end of the active list
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->MoveNode(curNode, curList, -1);
      NS_ENSURE_SUCCESS(res, res);
      // forget curPositionedDiv, if any
      // curPositionedDiv = nullptr;
    }
    
    else // not a list item, use blockquote?
    {
      // if we are inside a list item, we don't want to blockquote, we want
      // to sublist the list item.  We may have several nodes listed in the
      // array of nodes to act on, that are in the same list item.  Since
      // we only want to indent that li once, we must keep track of the most
      // recent indented list item, and not indent it if we find another node
      // to act on that is still inside the same li.
      nsCOMPtr<Element> listItem = IsInListItem(curNode);
      if (listItem) {
        if (indentedLI == GetAsDOMNode(listItem)) {
          // already indented this list item
          continue;
        }
        curParent = listItem->GetParentNode();
        offset = curParent ? curParent->IndexOf(listItem) : -1;
        // check to see if curList is still appropriate.  Which it is if
        // curNode is still right after it in the same list.
        if (curList)
        {
          NS_ENSURE_STATE(mHTMLEditor);
          sibling = GetAsDOMNode(mHTMLEditor->GetPriorHTMLSibling(curNode));
        }
         
        if (!curList || (sibling && sibling != GetAsDOMNode(curList))) {
          // create a new nested list of correct type
          res = SplitAsNeeded(*curParent->NodeInfo()->NameAtom(), curParent,
                              offset);
          NS_ENSURE_SUCCESS(res, res);
          if (!curPositionedDiv) {
            nsCOMPtr<nsINode> curParentParent = curParent->GetParentNode();
            int32_t parentOffset = curParentParent ?
              curParentParent->IndexOf(curParent) : -1;
            NS_ENSURE_STATE(mHTMLEditor);
            curPositionedDiv = mHTMLEditor->CreateNode(nsGkAtoms::div,
                                                       curParentParent,
                                                       parentOffset);
            mNewBlock = GetAsDOMNode(curPositionedDiv);
          }
          NS_ENSURE_STATE(mHTMLEditor);
          curList = mHTMLEditor->CreateNode(curParent->NodeInfo()->NameAtom(),
                                            curPositionedDiv, -1);
          NS_ENSURE_STATE(curList);
        }
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->MoveNode(listItem, curList, -1);
        NS_ENSURE_SUCCESS(res, res);
        // remember we indented this li
        indentedLI = GetAsDOMNode(listItem);
      }
      
      else
      {
        // need to make a div to put things in if we haven't already

        if (!curPositionedDiv) 
        {
          if (curNode->IsHTMLElement(nsGkAtoms::div)) {
            curPositionedDiv = curNode->AsElement();
            mNewBlock = GetAsDOMNode(curPositionedDiv);
            curList = nullptr;
            continue;
          }
          res = SplitAsNeeded(*nsGkAtoms::div, curParent, offset);
          NS_ENSURE_SUCCESS(res, res);
          NS_ENSURE_STATE(mHTMLEditor);
          curPositionedDiv = mHTMLEditor->CreateNode(nsGkAtoms::div, curParent,
                                                     offset);
          NS_ENSURE_STATE(curPositionedDiv);
          // remember our new block for postprocessing
          mNewBlock = GetAsDOMNode(curPositionedDiv);
          // curPositionedDiv is now the correct thing to put curNode in
        }
          
        // tuck the node into the end of the active blockquote
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->MoveNode(curNode, curPositionedDiv, -1);
        NS_ENSURE_SUCCESS(res, res);
        // forget curList, if any
        curList = nullptr;
      }
    }
  }
  return res;
}

nsresult
nsHTMLEditRules::DidAbsolutePosition()
{
  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIHTMLAbsPosEditor> absPosHTMLEditor = mHTMLEditor;
  nsCOMPtr<nsIDOMElement> elt = do_QueryInterface(mNewBlock);
  return absPosHTMLEditor->AbsolutelyPositionElement(elt, true);
}

nsresult
nsHTMLEditRules::WillRemoveAbsolutePosition(Selection* aSelection,
                                            bool* aCancel, bool* aHandled) {
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  nsresult res = WillInsert(aSelection, aCancel);
  NS_ENSURE_SUCCESS(res, res);

  // initialize out param
  // we want to ignore aCancel from WillInsert()
  *aCancel = false;
  *aHandled = true;

  nsCOMPtr<nsIDOMElement>  elt;
  NS_ENSURE_STATE(mHTMLEditor);
  res = mHTMLEditor->GetAbsolutelyPositionedSelectionContainer(getter_AddRefs(elt));
  NS_ENSURE_SUCCESS(res, res);

  NS_ENSURE_STATE(mHTMLEditor);
  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);

  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIHTMLAbsPosEditor> absPosHTMLEditor = mHTMLEditor;
  return absPosHTMLEditor->AbsolutelyPositionElement(elt, false);
}

nsresult
nsHTMLEditRules::WillRelativeChangeZIndex(Selection* aSelection,
                                          int32_t aChange,
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
  NS_ENSURE_STATE(mHTMLEditor);
  res = mHTMLEditor->GetAbsolutelyPositionedSelectionContainer(getter_AddRefs(elt));
  NS_ENSURE_SUCCESS(res, res);

  NS_ENSURE_STATE(mHTMLEditor);
  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);

  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIHTMLAbsPosEditor> absPosHTMLEditor = mHTMLEditor;
  int32_t zIndex;
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
  nsRefPtr<Selection> selection = mHTMLEditor->GetSelection();
  if (!selection) {
    return;
  }

  // Delete our bogus node, if we have one, since the document might not be
  // empty any more.
  if (mBogusNode) {
    mEditor->DeleteNode(mBogusNode);
    mBogusNode = nullptr;
  }

  // Try to recreate the bogus node if needed.
  CreateBogusNodeIfNeeded(selection);
}
