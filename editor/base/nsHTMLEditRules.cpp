/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Daniel Glazman <glazman@netscape.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* build on macs with low memory */
#if defined(XP_MAC) && defined(MOZ_MAC_LOWMEM)
#pragma optimization_level 1
#endif

#include "nsHTMLEditRules.h"

#include "nsEditor.h"
#include "nsTextEditUtils.h"
#include "nsHTMLEditUtils.h"
#include "nsHTMLEditor.h"
#include "TypeInState.h"

#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsIDOMNode.h"
#include "nsIDOMText.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsISelectionController.h"
#include "nsIDOMRange.h"
#include "nsIDOMNSRange.h"
#include "nsIDOMCharacterData.h"
#include "nsIEnumerator.h"
#include "nsIStyleContext.h"
#include "nsIPresShell.h"
#include "nsLayoutCID.h"
#include "nsIPref.h"

#include "nsEditorUtils.h"
#include "nsWSRunObject.h"

#include "InsertTextTxn.h"
#include "DeleteTextTxn.h"
#include "nsReadableUtils.h"


//const static char* kMOZEditorBogusNodeAttr="MOZ_EDITOR_BOGUS_NODE";
//const static char* kMOZEditorBogusNodeValue="TRUE";
const static PRUnichar nbsp = 160;

static NS_DEFINE_IID(kContentIteratorCID, NS_CONTENTITERATOR_CID);
static NS_DEFINE_IID(kRangeCID, NS_RANGE_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

enum
{
  kLonely = 0,
  kPrevSib = 1,
  kNextSib = 2,
  kBothSibs = 3
};

/********************************************************
 *  first some helpful funcotrs we will use
 ********************************************************/

static PRBool IsBlockNode(nsIDOMNode* node)
{
  PRBool isBlock (PR_FALSE);
  nsHTMLEditor::NodeIsBlockStatic(node, &isBlock);
  return isBlock;
}

static PRBool IsInlineNode(nsIDOMNode* node)
{
  return !IsBlockNode(node);
}
 
class nsTableCellAndListItemFunctor : public nsBoolDomIterFunctor
{
  public:
    virtual PRBool operator()(nsIDOMNode* aNode)  // used to build list of all li's, td's & th's iterator covers
    {
      if (nsHTMLEditUtils::IsTableCell(aNode)) return PR_TRUE;
      if (nsHTMLEditUtils::IsListItem(aNode)) return PR_TRUE;
      return PR_FALSE;
    }
};

class nsBRNodeFunctor : public nsBoolDomIterFunctor
{
  public:
    virtual PRBool operator()(nsIDOMNode* aNode)  // used to build list of all td's & th's iterator covers
    {
      if (nsTextEditUtils::IsBreak(aNode)) return PR_TRUE;
      return PR_FALSE;
    }
};

class nsEmptyFunctor : public nsBoolDomIterFunctor
{
  public:
    nsEmptyFunctor(nsHTMLEditor* editor) : mHTMLEditor(editor) {}
    virtual PRBool operator()(nsIDOMNode* aNode)  // used to build list of empty li's and td's
    {
      PRBool bIsEmptyNode;
      nsresult res = mHTMLEditor->IsEmptyNode(aNode, &bIsEmptyNode, PR_FALSE, PR_FALSE);
      if (NS_FAILED(res)) return PR_FALSE;
      if (bIsEmptyNode
        && (nsHTMLEditUtils::IsListItem(aNode) || nsHTMLEditUtils::IsTableCellOrCaption(aNode)))
      {
        return PR_TRUE;
      }
      return PR_FALSE;
    }
  protected:
    nsHTMLEditor* mHTMLEditor;
};

class nsEditableTextFunctor : public nsBoolDomIterFunctor
{
  public:
    nsEditableTextFunctor(nsHTMLEditor* editor) : mHTMLEditor(editor) {}
    virtual PRBool operator()(nsIDOMNode* aNode)  // used to build list of empty li's and td's
    {
      if (nsEditor::IsTextNode(aNode) && mHTMLEditor->IsEditable(aNode)) 
      {
        return PR_TRUE;
      }
      return PR_FALSE;
    }
  protected:
    nsHTMLEditor* mHTMLEditor;
};


/********************************************************
 *  routine for making new rules instance
 ********************************************************/

nsresult
NS_NewHTMLEditRules(nsIEditRules** aInstancePtrResult)
{
  nsHTMLEditRules * rules = new nsHTMLEditRules();
  if (rules)
    return rules->QueryInterface(NS_GET_IID(nsIEditRules), (void**) aInstancePtrResult);
  return NS_ERROR_OUT_OF_MEMORY;
}

/********************************************************
 *  Constructor/Destructor 
 ********************************************************/

nsHTMLEditRules::nsHTMLEditRules() : 
mDocChangeRange(nsnull)
,mListenerEnabled(PR_TRUE)
,mReturnInEmptyLIKillsList(PR_TRUE)
,mUtilRange(nsnull)
,mJoinOffset(0)
{
}

nsHTMLEditRules::~nsHTMLEditRules()
{
  // remove ourselves as a listener to edit actions
  // In the normal case, we have already been removed by 
  // ~nsHTMLEditor, in which case we will get an error here
  // which we ignore.  But this allows us to add the ability to
  // switch rule sets on the fly if we want.
  mHTMLEditor->RemoveEditActionListener(this);
}

/********************************************************
 *  XPCOM Cruft
 ********************************************************/

NS_IMPL_ADDREF_INHERITED(nsHTMLEditRules, nsTextEditRules)
NS_IMPL_RELEASE_INHERITED(nsHTMLEditRules, nsTextEditRules)
NS_IMPL_QUERY_INTERFACE3(nsHTMLEditRules, nsIHTMLEditRules, nsIEditRules, nsIEditActionListener)


/********************************************************
 *  Public methods 
 ********************************************************/

NS_IMETHODIMP
nsHTMLEditRules::Init(nsPlaintextEditor *aEditor, PRUint32 aFlags)
{
  mHTMLEditor = NS_STATIC_CAST(nsHTMLEditor*, aEditor);
  nsresult res;
  
  // call through to base class Init 
  res = nsTextEditRules::Init(aEditor, aFlags);
  if (NS_FAILED(res)) return res;

  // cache any prefs we care about
  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID, &res));
  if (NS_FAILED(res)) return res;

  char *returnInEmptyLIKillsList = 0;
  res = prefs->CopyCharPref("editor.html.typing.returnInEmptyListItemClosesList",
                               &returnInEmptyLIKillsList);
                          
  if (NS_SUCCEEDED(res) && returnInEmptyLIKillsList)
  {
    if (!strncmp(returnInEmptyLIKillsList, "false", 5))
      mReturnInEmptyLIKillsList = PR_FALSE; 
    else
      mReturnInEmptyLIKillsList = PR_TRUE; 
  }
  else
  {
    mReturnInEmptyLIKillsList = PR_TRUE; 
  }
  
  // make a utility range for use by the listenter
  mUtilRange = do_CreateInstance(kRangeCID);
  if (!mUtilRange) return NS_ERROR_NULL_POINTER;
   
  // set up mDocChangeRange to be whole doc
  nsCOMPtr<nsIDOMElement> bodyElem;
  nsCOMPtr<nsIDOMNode> bodyNode;
  mHTMLEditor->GetRootElement(getter_AddRefs(bodyElem));
  bodyNode = do_QueryInterface(bodyElem);
  if (bodyNode)
  {
    // temporarily turn off rules sniffing
    nsAutoLockRulesSniffing lockIt((nsTextEditRules*)this);
    if (!mDocChangeRange)
    {
      mDocChangeRange = do_CreateInstance(kRangeCID);
      if (!mDocChangeRange) return NS_ERROR_NULL_POINTER;
    }
    mDocChangeRange->SelectNode(bodyNode);
    res = AdjustSpecialBreaks();
    if (NS_FAILED(res)) return res;
  }

  // add ourselves as a listener to edit actions
  res = mHTMLEditor->AddEditActionListener(this);

  return res;
}


NS_IMETHODIMP
nsHTMLEditRules::BeforeEdit(PRInt32 action, nsIEditor::EDirection aDirection)
{
  if (mLockRulesSniffing) return NS_OK;
  
  nsAutoLockRulesSniffing lockIt((nsTextEditRules*)this);
  
  if (!mActionNesting)
  {
    nsCOMPtr<nsIDOMNSRange> nsrange;
    if(mDocChangeRange)
    {
      nsrange = do_QueryInterface(mDocChangeRange);
      if (!nsrange)
        return NS_ERROR_FAILURE;
      nsrange->NSDetach();  // clear out our accounting of what changed
    }
    if(mUtilRange)
    {
      nsrange = do_QueryInterface(mUtilRange);
      if (!nsrange)
        return NS_ERROR_FAILURE;
      nsrange->NSDetach();  // ditto for mUtilRange.  
    }
    // turn off caret
    nsCOMPtr<nsISelectionController> selCon;
    mHTMLEditor->GetSelectionController(getter_AddRefs(selCon));
    if (selCon) selCon->SetCaretEnabled(PR_FALSE);
    // check that selection is in subtree defined by body node
    ConfirmSelectionInBody();
    // let rules remember the top level action
    mTheAction = action;
  }
  mActionNesting++;
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLEditRules::AfterEdit(PRInt32 action, nsIEditor::EDirection aDirection)
{
  if (mLockRulesSniffing) return NS_OK;

  nsAutoLockRulesSniffing lockIt(this);

  NS_PRECONDITION(mActionNesting>0, "bad action nesting!");
  nsresult res = NS_OK;
  if (!--mActionNesting)
  {
    // do all the tricky stuff
    res = AfterEditInner(action, aDirection);
    // turn on caret
    nsCOMPtr<nsISelectionController> selCon;
    mHTMLEditor->GetSelectionController(getter_AddRefs(selCon));
    if (selCon) selCon->SetCaretEnabled(PR_TRUE);
#ifdef IBMBIDI
    /* After inserting text the cursor Bidi level must be set to the level of the inserted text.
     * This is difficult, because we cannot know what the level is until after the Bidi algorithm
     * is applied to the whole paragraph.
     *
     * So we set the cursor Bidi level to UNDEFINED here, and the caret code will set it correctly later
     */
    if (action == nsEditor::kOpInsertText) {
      nsCOMPtr<nsIPresShell> shell;
      mEditor->GetPresShell(getter_AddRefs(shell));
      if (shell) {
        shell->UndefineCursorBidiLevel();
      }
    }
#endif
  }

  return res;
}


nsresult
nsHTMLEditRules::AfterEditInner(PRInt32 action, nsIEditor::EDirection aDirection)
{
  ConfirmSelectionInBody();
  if (action == nsEditor::kOpIgnore) return NS_OK;
  
  nsCOMPtr<nsISelection>selection;
  nsresult res = mHTMLEditor->GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  
  // do we have a real range to act on?
  PRBool bDamagedRange = PR_FALSE;  
  if (mDocChangeRange)
  {  
    nsCOMPtr<nsIDOMNode> rangeStartParent, rangeEndParent;
    mDocChangeRange->GetStartContainer(getter_AddRefs(rangeStartParent));
    mDocChangeRange->GetEndContainer(getter_AddRefs(rangeEndParent));
    if (rangeStartParent && rangeEndParent) 
      bDamagedRange = PR_TRUE; 
  }
  
  if (bDamagedRange && !((action == nsEditor::kOpUndo) || (action == nsEditor::kOpRedo)))
  {
    // dont let any txns in here move the selection around behind our back.
    // Note that this won't prevent explicit selection setting from working.
    nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
   
    // expand the "changed doc range" as needed
    res = PromoteRange(mDocChangeRange, action);
    if (NS_FAILED(res)) return res;
    
    // add in any needed <br>s, and remove any unneeded ones.
    res = AdjustSpecialBreaks();
    if (NS_FAILED(res)) return res;
    
    // merge any adjacent text nodes
    if ( (action != nsEditor::kOpInsertText &&
         action != nsEditor::kOpInsertIMEText) )
    {
      res = mHTMLEditor->CollapseAdjacentTextNodes(mDocChangeRange);
      if (NS_FAILED(res)) return res;
    }
    
    // replace newlines with breaks.
    // MOOSE:  This is buttUgly.  A better way to 
    // organize the action enum is in order.
    if (// (action == nsEditor::kOpInsertText) || 
        // (action == nsEditor::kOpInsertIMEText) ||
        (action == nsHTMLEditor::kOpInsertElement) ||
        (action == nsHTMLEditor::kOpInsertQuotation) ||
        (action == nsEditor::kOpInsertNode))
    {
      res = ReplaceNewlines(mDocChangeRange);
    }
    
    // clean up any empty nodes in the selection
    res = RemoveEmptyNodes();
    if (NS_FAILED(res)) return res;

    // attempt to transform any uneeded nbsp's into spaces after doing deletions
    if (action == nsEditor::kOpDeleteSelection)
    {
      res = AdjustWhitespace(selection);
      if (NS_FAILED(res)) return res;
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
        (action == nsHTMLEditor::kOpHTMLPaste))
    {
      res = AdjustSelection(selection, aDirection);
      if (NS_FAILED(res)) return res;
    }
    
  }

  // detect empty doc
  res = CreateBogusNodeIfNeeded(selection);
  
  // adjust selection HINT if needed
  if (NS_FAILED(res)) return res;
  res = CheckInterlinePosition(selection);
  return res;
}


NS_IMETHODIMP 
nsHTMLEditRules::WillDoAction(nsISelection *aSelection, 
                              nsRulesInfo *aInfo, 
                              PRBool *aCancel, 
                              PRBool *aHandled)
{
  if (!aInfo || !aCancel || !aHandled) 
    return NS_ERROR_NULL_POINTER;
#if defined(DEBUG_ftang)
  printf("nsHTMLEditRules::WillDoAction action = %d\n", aInfo->action);
#endif

  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;
    
  // my kingdom for dynamic cast
  nsTextRulesInfo *info = NS_STATIC_CAST(nsTextRulesInfo*, aInfo);
    
  switch (info->action)
  {
    case kInsertText:
    case kInsertTextIME:
      return WillInsertText(info->action,
                            aSelection, 
                            aCancel, 
                            aHandled,
                            info->inString,
                            info->outString,
                            info->maxLength);
    case kInsertBreak:
      return WillInsertBreak(aSelection, aCancel, aHandled);
    case kDeleteSelection:
      return WillDeleteSelection(aSelection, info->collapsedAction, aCancel, aHandled);
    case kMakeList:
      return WillMakeList(aSelection, info->blockType, info->entireList, aCancel, aHandled);
    case kIndent:
      return WillIndent(aSelection, aCancel, aHandled);
    case kOutdent:
      return WillOutdent(aSelection, aCancel, aHandled);
    case kAlign:
      return WillAlign(aSelection, info->alignType, aCancel, aHandled);
    case kMakeBasicBlock:
      return WillMakeBasicBlock(aSelection, info->blockType, aCancel, aHandled);
    case kRemoveList:
      return WillRemoveList(aSelection, info->bOrdered, aCancel, aHandled);
    case kMakeDefListItem:
      return WillMakeDefListItem(aSelection, info->blockType, info->entireList, aCancel, aHandled);
    case kInsertElement:
      return WillInsert(aSelection, aCancel);
  }
  return nsTextEditRules::WillDoAction(aSelection, aInfo, aCancel, aHandled);
}
  
  
NS_IMETHODIMP 
nsHTMLEditRules::DidDoAction(nsISelection *aSelection,
                             nsRulesInfo *aInfo, nsresult aResult)
{
  nsTextRulesInfo *info = NS_STATIC_CAST(nsTextRulesInfo*, aInfo);
  switch (info->action)
  {
    case kInsertBreak:
      return DidInsertBreak(aSelection, aResult);
    case kMakeBasicBlock:
    case kIndent:
    case kOutdent:
    case kAlign:
      return DidMakeBasicBlock(aSelection, aInfo, aResult);
  }
  
  // default: pass thru to nsTextEditRules
  return nsTextEditRules::DidDoAction(aSelection, aInfo, aResult);
}
  
/********************************************************
 *  nsIHTMLEditRules methods
 ********************************************************/

NS_IMETHODIMP 
nsHTMLEditRules::GetListState(PRBool *aMixed, PRBool *aOL, PRBool *aUL, PRBool *aDL)
{
  if (!aMixed || !aOL || !aUL || !aDL)
    return NS_ERROR_NULL_POINTER;
  *aMixed = PR_FALSE;
  *aOL = PR_FALSE;
  *aUL = PR_FALSE;
  *aDL = PR_FALSE;
  PRBool bNonList = PR_FALSE;
  
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  nsresult res = GetListActionNodes(address_of(arrayOfNodes), PR_FALSE, PR_TRUE);
  if (NS_FAILED(res)) return res;

  // examine list type for nodes in selection
  PRUint32 listCount;
  PRInt32 i;
  arrayOfNodes->Count(&listCount);
  for (i=(PRInt32)listCount-1; i>=0; i--)
  {
    nsCOMPtr<nsISupports> isupports = dont_AddRef(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports) );
    
    if (mHTMLEditor->NodeIsType(curNode,nsIEditProperty::ul))
      *aUL = PR_TRUE;
    else if (mHTMLEditor->NodeIsType(curNode,nsIEditProperty::ol))
      *aOL = PR_TRUE;
    else if (mHTMLEditor->NodeIsType(curNode,nsIEditProperty::li))
    {
      nsCOMPtr<nsIDOMNode> parent;
      PRInt32 offset;
      res = nsEditor::GetNodeLocation(curNode, address_of(parent), &offset);
      if (NS_FAILED(res)) return res;
      if (nsHTMLEditUtils::IsUnorderedList(parent))
        *aUL = PR_TRUE;
      else if (nsHTMLEditUtils::IsOrderedList(parent))
        *aOL = PR_TRUE;
    }
    else if (mHTMLEditor->NodeIsType(curNode,nsIEditProperty::dl) ||
             mHTMLEditor->NodeIsType(curNode,nsIEditProperty::dt) ||
             mHTMLEditor->NodeIsType(curNode,nsIEditProperty::dd) )
    {
      *aDL = PR_TRUE;
    }
    else bNonList = PR_TRUE;
  }  
  
  // hokey arithmetic with booleans
  if ( (*aUL + *aOL + *aDL + bNonList) > 1) *aMixed = PR_TRUE;
  
  return res;
}

NS_IMETHODIMP 
nsHTMLEditRules::GetListItemState(PRBool *aMixed, PRBool *aLI, PRBool *aDT, PRBool *aDD)
{
  if (!aMixed || !aLI || !aDT || !aDD)
    return NS_ERROR_NULL_POINTER;
  *aMixed = PR_FALSE;
  *aLI = PR_FALSE;
  *aDT = PR_FALSE;
  *aDD = PR_FALSE;
  PRBool bNonList = PR_FALSE;
  
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  nsresult res = GetListActionNodes(address_of(arrayOfNodes), PR_FALSE, PR_TRUE);
  if (NS_FAILED(res)) return res;

  // examine list type for nodes in selection
  PRUint32 listCount;
  PRInt32 i;
  arrayOfNodes->Count(&listCount);
  for (i=(PRInt32)listCount-1; i>=0; i--)
  {
    nsCOMPtr<nsISupports> isupports = dont_AddRef(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports) );
    
    if (mHTMLEditor->NodeIsType(curNode,nsIEditProperty::ul) ||
        mHTMLEditor->NodeIsType(curNode,nsIEditProperty::ol) ||
        mHTMLEditor->NodeIsType(curNode,nsIEditProperty::li) )
    {
      *aLI = PR_TRUE;
    }
    else if (mHTMLEditor->NodeIsType(curNode,nsIEditProperty::dt))
    {
      *aDT = PR_TRUE;
    }
    else if (mHTMLEditor->NodeIsType(curNode,nsIEditProperty::dd))
    {
      *aDD = PR_TRUE;
    }
    else if (mHTMLEditor->NodeIsType(curNode,nsIEditProperty::dl))
    {
      // need to look inside dl and see which types of items it has
      PRBool bDT, bDD;
      res = GetDefinitionListItemTypes(curNode, bDT, bDD);
      if (NS_FAILED(res)) return res;
      *aDT |= bDT;
      *aDD |= bDD;
    }
    else bNonList = PR_TRUE;
  }  
  
  // hokey arithmetic with booleans
  if ( (*aDT + *aDD + bNonList) > 1) *aMixed = PR_TRUE;
  
  return res;
}

NS_IMETHODIMP 
nsHTMLEditRules::GetAlignment(PRBool *aMixed, nsIHTMLEditor::EAlignment *aAlign)
{
  // for now, just return first alignment.  we'll lie about
  // if it's mixed.  This is for efficiency
  // given that our current ui doesn't care if it's mixed.
  // cmanske: NOT TRUE! We would like to pay attention to mixed state
  //  in Format | Align submenu!

  // this routine assumes that alignment is done ONLY via divs
  
  // default alignment is left
  if (!aMixed || !aAlign)
    return NS_ERROR_NULL_POINTER;
  *aMixed = PR_FALSE;
  *aAlign = nsIHTMLEditor::eLeft;
  
  // get selection
  nsCOMPtr<nsISelection>selection;
  nsresult res = mHTMLEditor->GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;

  // get selection location
  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset;
  res = mHTMLEditor->GetStartNodeAndOffset(selection, address_of(parent), &offset);
  if (NS_FAILED(res)) return res;
  
  // is the selection collapsed?
  PRBool bCollapsed;
  res = selection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  nsCOMPtr<nsIDOMNode> nodeToExamine;
  if (bCollapsed)
  {
    // if it is, we want to look at 'parent' and it's ancestors
    // for divs with alignment on them
    nodeToExamine = parent;
  }
  else if (mHTMLEditor->IsTextNode(parent)) 
  {
    // if we are in a text node, then that is the node of interest
    nodeToExamine = parent;
  }
  else
  {
    // otherwise we want to look at the first editable node after
    // {parent,offset} and it's ancestors for divs with alignment on them
    mHTMLEditor->GetNextNode(parent, offset, PR_TRUE, address_of(nodeToExamine));
  }
  
  if (!nodeToExamine) return NS_ERROR_NULL_POINTER;
  
  // check up the ladder for divs with alignment
  nsCOMPtr<nsIDOMNode> temp = nodeToExamine;
  PRBool isFirstNodeToExamine = PR_TRUE;
  while (nodeToExamine)
  {
    if (nsHTMLEditUtils::IsDiv(nodeToExamine))
    {
      // check for alignment
      nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(nodeToExamine);
      if (elem)
      {
        nsAutoString typeAttrName; typeAttrName.AssignWithConversion("align");
        nsAutoString typeAttrVal;
        res = elem->GetAttribute(typeAttrName, typeAttrVal);
        typeAttrVal.ToLowerCase();
        if (NS_SUCCEEDED(res) && typeAttrVal.Length())
        {
          if (typeAttrVal.EqualsWithConversion("center"))
            *aAlign = nsIHTMLEditor::eCenter;
          else if (typeAttrVal.EqualsWithConversion("right"))
            *aAlign = nsIHTMLEditor::eRight;
          else if (typeAttrVal.EqualsWithConversion("justify"))
            *aAlign = nsIHTMLEditor::eJustify;
          else
            *aAlign = nsIHTMLEditor::eLeft;
          return res;
        }
      }
    }
    if (!isFirstNodeToExamine && nsHTMLEditUtils::IsTable(nodeToExamine)) {
      // the node to examine is a table and this is not the first node
      // we examine; let's break here to materialize the 'inline-block'
      // behaviour of html tables regarding to text alignment
      return NS_OK;
    }
    isFirstNodeToExamine = PR_FALSE;
    res = nodeToExamine->GetParentNode(getter_AddRefs(temp));
    if (NS_FAILED(res)) temp = nsnull;
    nodeToExamine = temp; 
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLEditRules::GetIndentState(PRBool *aCanIndent, PRBool *aCanOutdent)
{
  if (!aCanIndent || !aCanOutdent)
      return NS_ERROR_FAILURE;
  *aCanIndent = PR_TRUE;    
  *aCanOutdent = PR_FALSE;
  
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  nsresult res = GetListActionNodes(address_of(arrayOfNodes), PR_FALSE, PR_TRUE);
  if (NS_FAILED(res)) return res;

  // examine nodes in selection for blockquotes or list elements;
  // these we can outdent.  Note that we return true for canOutdent
  // if *any* of the selection is outdentable, rather than all of it.
  PRUint32 listCount;
  PRInt32 i;
  arrayOfNodes->Count(&listCount);
  for (i=(PRInt32)listCount-1; i>=0; i--)
  {
    nsCOMPtr<nsISupports> isupports = dont_AddRef(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports) );
    
    if (nsHTMLEditUtils::IsList(curNode)     || 
        nsHTMLEditUtils::IsListItem(curNode) ||
        nsHTMLEditUtils::IsBlockquote(curNode))
    {
      *aCanOutdent = PR_TRUE;
      break;
    }
  }  
  
  if (!*aCanOutdent)
  {
    // if we haven't found something to outdent yet, also check the parents
    // of selection endpoints.  We might have a blockquote or list item 
    // in the parent heirarchy.
    
    // gather up info we need for test
    nsCOMPtr<nsIDOMNode> parent, tmp, root;
    nsCOMPtr<nsIDOMElement> rootElem;
    nsCOMPtr<nsISelection> selection;
    PRInt32 selOffset;
    res = mHTMLEditor->GetRootElement(getter_AddRefs(rootElem));
    if (NS_FAILED(res)) return res;
    if (!rootElem) return NS_ERROR_NULL_POINTER;
    root = do_QueryInterface(rootElem);
    if (!root) return NS_ERROR_NO_INTERFACE;
    res = mHTMLEditor->GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(res)) return res;
    if (!selection) return NS_ERROR_NULL_POINTER;
    
    // test start parent heirachy
    res = mHTMLEditor->GetStartNodeAndOffset(selection, address_of(parent), &selOffset);
    if (NS_FAILED(res)) return res;
    while (parent && (parent!=root))
    {
      if (nsHTMLEditUtils::IsList(parent)     || 
          nsHTMLEditUtils::IsListItem(parent) ||
          nsHTMLEditUtils::IsBlockquote(parent))
      {
        *aCanOutdent = PR_TRUE;
        break;
      }
      tmp=parent;
      tmp->GetParentNode(getter_AddRefs(parent));
    }

    // test end parent heirachy
    res = mHTMLEditor->GetEndNodeAndOffset(selection, address_of(parent), &selOffset);
    if (NS_FAILED(res)) return res;
    while (parent && (parent!=root))
    {
      if (nsHTMLEditUtils::IsList(parent)     || 
          nsHTMLEditUtils::IsListItem(parent) ||
          nsHTMLEditUtils::IsBlockquote(parent))
      {
        *aCanOutdent = PR_TRUE;
        break;
      }
      tmp=parent;
      tmp->GetParentNode(getter_AddRefs(parent));
    }
  }
  return res;
}


NS_IMETHODIMP 
nsHTMLEditRules::GetParagraphState(PRBool *aMixed, nsAWritableString &outFormat)
{
  // This routine is *heavily* tied to our ui choices in the paragraph
  // style popup.  I cant see a way around that.
  if (!aMixed)
    return NS_ERROR_NULL_POINTER;
  *aMixed = PR_TRUE;
  outFormat.Truncate(0);
  
  PRBool bMixed = PR_FALSE;
  nsAutoString formatStr; 
  // using "x" as an uninitialized value, since "" is meaningful
  formatStr.Assign(NS_LITERAL_STRING("x"));
  
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  nsresult res = GetParagraphFormatNodes(address_of(arrayOfNodes), PR_TRUE);
  if (NS_FAILED(res)) return res;

  // we might have an empty node list.  if so, find selection parent
  // and put that on the list
  PRUint32 listCount;
  arrayOfNodes->Count(&listCount);
  nsCOMPtr<nsISupports> isupports;
  if (!listCount)
  {
    nsCOMPtr<nsIDOMNode> selNode;
    PRInt32 selOffset;
    nsCOMPtr<nsISelection>selection;
    res = mHTMLEditor->GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(res)) return res;
    res = mHTMLEditor->GetStartNodeAndOffset(selection, address_of(selNode), &selOffset);
    if (NS_FAILED(res)) return res;
    isupports = do_QueryInterface(selNode);
    if (!isupports) return NS_ERROR_NULL_POINTER;
    arrayOfNodes->AppendElement(isupports);
    listCount = 1;
  }

  // loop through the nodes in selection and examine their paragraph format
  PRInt32 i;
  for (i=(PRInt32)listCount-1; i>=0; i--)
  {
    isupports = dont_AddRef(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports) );

    nsAutoString format;
    nsCOMPtr<nsIAtom> atom = mHTMLEditor->GetTag(curNode);
    
    if (IsInlineNode(curNode))
    {
      nsCOMPtr<nsIDOMNode> block = mHTMLEditor->GetBlockNodeParent(curNode);
      if (block)
      {
        nsCOMPtr<nsIAtom> blockAtom = mHTMLEditor->GetTag(block);
        if ( nsIEditProperty::p == blockAtom.get()           ||
             nsIEditProperty::blockquote == blockAtom.get()  ||
             nsIEditProperty::address == blockAtom.get()     ||
             nsIEditProperty::pre == blockAtom.get()           )
        {
          blockAtom->ToString(format);
        }
        else if (nsHTMLEditUtils::IsHeader(block))
        {
          nsAutoString tag;
          nsEditor::GetTagString(block,tag);
          tag.ToLowerCase();
          format = tag;
        }
        else
        {
          format.Truncate(0);
        }
      }
      else
      {
        format.Truncate(0);
      }  
    }    
    else if (nsHTMLEditUtils::IsHeader(curNode))
    {
      nsAutoString tag;
      nsEditor::GetTagString(curNode,tag);
      tag.ToLowerCase();
      format = tag;
    }
    else if (nsIEditProperty::p == atom.get()           ||
             nsIEditProperty::blockquote == atom.get()  ||
             nsIEditProperty::address == atom.get()     ||
             nsIEditProperty::pre == atom.get()           )
    {
      atom->ToString(format);
    }
    
    // if this is the first node, we've found, remember it as the format
    if (formatStr.EqualsWithConversion("x"))
      formatStr = format;
    // else make sure it matches previously found format
    else if (format != formatStr) 
    {
      bMixed = PR_TRUE;
      break; 
    }
  }  
  
  *aMixed = bMixed;
  outFormat = formatStr;
  return res;
}


/********************************************************
 *  Protected rules methods 
 ********************************************************/

nsresult
nsHTMLEditRules::WillInsert(nsISelection *aSelection, PRBool *aCancel)
{
  nsresult res = nsTextEditRules::WillInsert(aSelection, aCancel);
  if (NS_FAILED(res)) return res; 
  
  // Adjust selection to prevent insertion after a moz-BR.
  // this next only works for collapsed selections right now,
  // because selection is a pain to work with when not collapsed.
  // (no good way to extend start or end of selection)
  PRBool bCollapsed;
  res = aSelection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  if (!bCollapsed) return NS_OK;

  // if we are after a mozBR in the same block, then move selection
  // to be before it
  nsCOMPtr<nsIDOMNode> selNode, priorNode;
  PRInt32 selOffset;
  // get the (collapsed) selection location
  res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(selNode),
                                           &selOffset);
  if (NS_FAILED(res)) return res;
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
      res = nsEditor::GetNodeLocation(priorNode, address_of(selNode), &selOffset);
      if (NS_FAILED(res)) return res;
      res = aSelection->Collapse(selNode,selOffset);
      if (NS_FAILED(res)) return res;
    }
  }

  // we need to get the doc
  nsCOMPtr<nsIDOMDocument>doc;
  res = mHTMLEditor->GetDocument(getter_AddRefs(doc));
  if (NS_FAILED(res)) return res;
  if (!doc) return NS_ERROR_NULL_POINTER;
    
  // for every property that is set, insert a new inline style node
  return CreateStyleForInsertText(aSelection, doc);
}    

nsresult
nsHTMLEditRules::DidInsert(nsISelection *aSelection, nsresult aResult)
{
  return nsTextEditRules::DidInsert(aSelection, aResult);
}

nsresult
nsHTMLEditRules::WillInsertText(PRInt32          aAction,
                                nsISelection *aSelection, 
                                PRBool          *aCancel,
                                PRBool          *aHandled,
                                const nsAReadableString  *inString,
                                nsAWritableString        *outString,
                                PRInt32          aMaxLength)
{  
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }

  if (inString->IsEmpty() && (aAction != kInsertTextIME))
  {
    // HACK: this is a fix for bug 19395
    // I can't outlaw all empty insertions
    // because IME transaction depend on them
    // There is more work to do to make the 
    // world safe for IME.
    *aCancel = PR_TRUE;
    *aHandled = PR_FALSE;
    return NS_OK;
  }
  
  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_TRUE;
  nsresult res;
  nsCOMPtr<nsIDOMNode> selNode;
  PRInt32 selOffset;

  PRBool bPlaintext = mFlags & nsIPlaintextEditor::eEditorPlaintextMask;

  // if the selection isn't collapsed, delete it.
  PRBool bCollapsed;
  res = aSelection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  if (!bCollapsed)
  {
    res = mHTMLEditor->DeleteSelection(nsIEditor::eNone);
    if (NS_FAILED(res)) return res;
  }

  res = WillInsert(aSelection, aCancel);
  if (NS_FAILED(res)) return res;
  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = PR_FALSE;
  
  // get the (collapsed) selection location
  res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(selNode), &selOffset);
  if (NS_FAILED(res)) return res;

  // dont put text in places that cant have it
  nsAutoString textTag; textTag.AssignWithConversion("__moz_text");
  if (!mHTMLEditor->IsTextNode(selNode) && !mHTMLEditor->CanContainTag(selNode, textTag))
    return NS_ERROR_FAILURE;

  // we need to get the doc
  nsCOMPtr<nsIDOMDocument>doc;
  res = mHTMLEditor->GetDocument(getter_AddRefs(doc));
  if (NS_FAILED(res)) return res;
  if (!doc) return NS_ERROR_NULL_POINTER;
    
  if (aAction == kInsertTextIME) 
  { 
    res = mHTMLEditor->InsertTextImpl(*inString, address_of(selNode), &selOffset, doc);
    if (NS_FAILED(res)) return res;
  }
  else // aAction == kInsertText
  {
    // find where we are
    nsCOMPtr<nsIDOMNode> curNode = selNode;
    PRInt32 curOffset = selOffset;
    
    // is our text going to be PREformatted?  
    // We remember this so that we know how to handle tabs.
    PRBool isPRE;
    res = mHTMLEditor->IsPreformatted(selNode, &isPRE);
    if (NS_FAILED(res)) return res;    
    
    // turn off the edit listener: we know how to
    // build the "doc changed range" ourselves, and it's
    // must faster to do it once here than to track all
    // the changes one at a time.
    nsAutoLockListener lockit(&mListenerEnabled); 
    
    // dont spaz my selection in subtransactions
    nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
    nsAutoString tString(*inString);
    const PRUnichar *unicodeBuf = tString.get();
    nsCOMPtr<nsIDOMNode> unused;
    PRInt32 pos = 0;
        
    // for efficiency, break out the pre case seperately.  This is because
    // its a lot cheaper to search the input string for only newlines than
    // it is to search for both tabs and newlines.
    if (isPRE || bPlaintext)
    {
      NS_NAMED_LITERAL_STRING(newlineStr, "\n");
      char newlineChar = '\n';
      while (unicodeBuf && (pos != -1) && (pos < (PRInt32)(*inString).Length()))
      {
        PRInt32 oldPos = pos;
        PRInt32 subStrLen;
        pos = tString.FindChar(newlineChar, PR_FALSE, oldPos);

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
        if (NS_FAILED(res)) return res;
      }
    }
    else
    {
      NS_NAMED_LITERAL_STRING(tabStr, "\t");
      NS_NAMED_LITERAL_STRING(newlineStr, "\n");
      char specialChars[] = {'\t','\n',0};
      nsAutoString tabString; tabString.AssignWithConversion("    ");
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
//        res = mHTMLEditor->InsertTextImpl(tabString, address_of(curNode), &curOffset, doc);
          res = wsObj.InsertText(tabString, address_of(curNode), &curOffset, doc);
          if (NS_FAILED(res)) return res;
          pos++;
        }
        // is it a return?
        else if (subStr.Equals(newlineStr))
        {
//        res = mHTMLEditor->CreateBRImpl(address_of(curNode), &curOffset, address_of(unused), nsIEditor::eNone);
          res = wsObj.InsertBreak(address_of(curNode), &curOffset, address_of(unused), nsIEditor::eNone);
          if (NS_FAILED(res)) return res;
          pos++;
        }
        else
        {
//        res = mHTMLEditor->InsertTextImpl(subStr, address_of(curNode), &curOffset, doc);
          res = wsObj.InsertText(subStr, address_of(curNode), &curOffset, doc);
          if (NS_FAILED(res)) return res;
        }
        if (NS_FAILED(res)) return res;
      }
    }
#ifdef IBMBIDI
    nsCOMPtr<nsISelection> selection(aSelection);
    nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));
    selPriv->SetInterlinePosition(PR_FALSE);
#endif
    if (curNode) aSelection->Collapse(curNode, curOffset);
    // manually update the doc changed range so that AfterEdit will clean up
    // the correct portion of the document.
    if (!mDocChangeRange)
    {
      mDocChangeRange = do_CreateInstance(kRangeCID);
      if (!mDocChangeRange) return NS_ERROR_NULL_POINTER;
    }
    res = mDocChangeRange->SetStart(selNode, selOffset);
    if (NS_FAILED(res)) return res;
    if (curNode)
      res = mDocChangeRange->SetEnd(curNode, curOffset);
    else
      res = mDocChangeRange->SetEnd(selNode, selOffset);
    if (NS_FAILED(res)) return res;
  }
  return res;
}

nsresult
nsHTMLEditRules::WillInsertBreak(nsISelection *aSelection, PRBool *aCancel, PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  nsCOMPtr<nsISelection> selection(aSelection);
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));
  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;
  
  PRBool bPlaintext = mFlags & nsIPlaintextEditor::eEditorPlaintextMask;

  // if the selection isn't collapsed, delete it.
  PRBool bCollapsed;
  nsresult res = aSelection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  if (!bCollapsed)
  {
    res = mHTMLEditor->DeleteSelection(nsIEditor::eNone);
    if (NS_FAILED(res)) return res;
  }
  
  res = WillInsert(aSelection, aCancel);
  if (NS_FAILED(res)) return res;
  
  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = PR_FALSE;
  
  // split any mailcites in the way.
  // should we abort this if we encounter table cell boundaries?
  if (mFlags & nsIPlaintextEditor::eEditorMailMask)
  {
    nsCOMPtr<nsIDOMNode> citeNode, selNode;
    PRInt32 selOffset, newOffset;
    res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(selNode), &selOffset);
    if (NS_FAILED(res)) return res;
    res = GetTopEnclosingMailCite(selNode, address_of(citeNode), bPlaintext);
    if (NS_FAILED(res)) return res;
    if (citeNode)
    {
      nsCOMPtr<nsIDOMNode> brNode;
      res = mHTMLEditor->SplitNodeDeep(citeNode, selNode, selOffset, &newOffset);
      if (NS_FAILED(res)) return res;
      res = citeNode->GetParentNode(getter_AddRefs(selNode));
      if (NS_FAILED(res)) return res;
      res = mHTMLEditor->CreateBR(selNode, newOffset, address_of(brNode));
      if (NS_FAILED(res)) return res;
      // want selection before the break, and on same line
      selPriv->SetInterlinePosition(PR_TRUE);
      res = aSelection->Collapse(selNode, newOffset);
      if (NS_FAILED(res)) return res;
      *aHandled = PR_TRUE;
      return NS_OK;
    }
  }

  // smart splitting rules
  nsCOMPtr<nsIDOMNode> node;
  PRInt32 offset;
  
  res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(node), &offset);
  if (NS_FAILED(res)) return res;
  if (!node) return NS_ERROR_FAILURE;
    
  // identify the block
  nsCOMPtr<nsIDOMNode> blockParent;
  
  if (IsBlockNode(node)) 
    blockParent = node;
  else 
    blockParent = mHTMLEditor->GetBlockNodeParent(node);
    
  if (!blockParent) return NS_ERROR_FAILURE;
  
  // headers: close (or split) header
  else if (nsHTMLEditUtils::IsHeader(blockParent))
  {
    res = ReturnInHeader(aSelection, blockParent, node, offset);
    *aHandled = PR_TRUE;
    return NS_OK;
  }
  
  // paragraphs: special rules to look for <br>s
  else if (nsHTMLEditUtils::IsParagraph(blockParent))
  {
    res = ReturnInParagraph(aSelection, blockParent, node, offset, aCancel, aHandled);
    return NS_OK;
  }
  
  // list items: special rules to make new list items
  else if (nsHTMLEditUtils::IsListItem(blockParent))
  {
    res = ReturnInListItem(aSelection, blockParent, node, offset);
    *aHandled = PR_TRUE;
    return NS_OK;
  }
  // its something else (body, div, td, ...): insert a normal br
  else
  {
    nsCOMPtr<nsIDOMNode> brNode;
    if (bPlaintext)
    {
      res = mHTMLEditor->CreateBR(node, offset, address_of(brNode));
    }
    else
    {
      nsWSRunObject wsObj(mHTMLEditor, node, offset);
      res = wsObj.InsertBreak(address_of(node), &offset, address_of(brNode), nsIEditor::eNone);
    }
    if (NS_FAILED(res)) return res;
    res = nsEditor::GetNodeLocation(brNode, address_of(node), &offset);
    if (NS_FAILED(res)) return res;
    // SetInterlinePosition(PR_TRUE) means we want the caret to stick to the content on the "right".
    // We want the caret to stick to whatever is past the break.  This is
    // because the break is on the same line we were on, but the next content
    // will be on the following line.
    
    // An exception to this is if the break has a next sibling that is a block node.
    // Then we stick to the left to aviod an uber caret.
    nsCOMPtr<nsIDOMNode> siblingNode;
    brNode->GetNextSibling(getter_AddRefs(siblingNode));
    if (siblingNode && IsBlockNode(siblingNode))
      selPriv->SetInterlinePosition(PR_FALSE);
    else 
      selPriv->SetInterlinePosition(PR_TRUE);
    res = aSelection->Collapse(node, offset+1);
    if (NS_FAILED(res)) return res;
    *aHandled = PR_TRUE;
  }
  
  return res;
}


nsresult
nsHTMLEditRules::DidInsertBreak(nsISelection *aSelection, nsresult aResult)
{
  return NS_OK;
}


nsresult
nsHTMLEditRules::WillDeleteSelection(nsISelection *aSelection, 
                                     nsIEditor::EDirection aAction, 
                                     PRBool *aCancel,
                                     PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;
  
  // if there is only bogus content, cancel the operation
  if (mBogusNode) 
  {
    *aCancel = PR_TRUE;
    return NS_OK;
  }

  nsresult res = NS_OK;
  PRBool bPlaintext = mFlags & nsIPlaintextEditor::eEditorPlaintextMask;
  
  PRBool bCollapsed;
  res = aSelection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  
  nsCOMPtr<nsIDOMNode> startNode, selNode;
  PRInt32 startOffset, selOffset;
  
  // first check for table selection mode.  If so,
  // hand off to table editor.
  {
    nsCOMPtr<nsIDOMElement> cell;
    res = mHTMLEditor->GetFirstSelectedCell(getter_AddRefs(cell), nsnull);
    if (NS_SUCCEEDED(res) && cell)
    {
      res = mHTMLEditor->DeleteTableCellContents();
      *aHandled = PR_TRUE;
      return res;
    }
  }
  
  res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(startNode), &startOffset);
  if (NS_FAILED(res)) return res;
  if (!startNode) return NS_ERROR_FAILURE;
    
  // get the root element  
  nsCOMPtr<nsIDOMNode> bodyNode; 
  {
    nsCOMPtr<nsIDOMElement> bodyElement;   
    res = mHTMLEditor->GetRootElement(getter_AddRefs(bodyElement));
    if (NS_FAILED(res)) return res;
    if (!bodyElement) return NS_ERROR_UNEXPECTED;
    bodyNode = do_QueryInterface(bodyElement);
  }

  if (bCollapsed)
  {
    // if we are inside an empty block, delete it.
    res = CheckForEmptyBlock(startNode, bodyNode, aSelection, aHandled);
    if (NS_FAILED(res)) return res;
    if (*aHandled) return NS_OK;
        
#ifdef IBMBIDI
    // Test for distance between caret and text that will be deleted
    res = CheckBidiLevelForDeletion(startNode, startOffset, aAction, aCancel);
    if (NS_FAILED(res)) return res;
    if (*aCancel) return NS_OK;
#endif // IBMBIDI

    if (!bPlaintext)
    {
      // Check for needed whitespace adjustments.  If we need to delete
      // just whitespace, that is handled here.  
      // CheckForWhitespaceDeletion also adjusts it's first two args to
      // skip over invisible ws.  So we need to check that we didn't cross a table
      // element boundary afterwards.
      nsCOMPtr<nsIDOMNode> visNode = startNode;
      PRInt32 visOffset = startOffset;
      res = CheckForWhitespaceDeletion(address_of(visNode), &visOffset, 
                                       aAction, aHandled);
      if (NS_FAILED(res)) return res;
      if (*aHandled) return NS_OK;
      // dont cross any table elements
      PRBool bInDifTblElems;
      res = InDifferentTableElements(visNode, startNode, &bInDifTblElems);
      if (NS_FAILED(res)) return res;
      if (bInDifTblElems) 
      {
        *aCancel = PR_TRUE;
        return NS_OK;
      }
      // else reset startNode/startOffset
      startNode = visNode;  startOffset = visOffset;
    }
    // in a text node:
    if (mHTMLEditor->IsTextNode(startNode))
    {
      nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(startNode);
      PRUint32 strLength;
      res = textNode->GetLength(&strLength);
      if (NS_FAILED(res)) return res;

      // at beginning of text node and backspaced?
      if (!startOffset && (aAction == nsIEditor::ePrevious))
      {
        nsCOMPtr<nsIDOMNode> priorNode;
        res = mHTMLEditor->GetPriorHTMLNode(startNode, address_of(priorNode));
        if (NS_FAILED(res)) return res;
        
        // if there is no prior node then cancel the deletion
        if (!priorNode || !nsTextEditUtils::InBody(priorNode, mHTMLEditor))
        {
          *aCancel = PR_TRUE;
          return res;
        }
        
        // block parents the same?  
        if (mHTMLEditor->HasSameBlockNodeParent(startNode, priorNode)) 
        {
          // is prior node a text node?
          if ( mHTMLEditor->IsTextNode(priorNode) )
          {
            // delete last character
            PRUint32 offset;
            nsCOMPtr<nsIDOMCharacterData>nodeAsText;
            nodeAsText = do_QueryInterface(priorNode);
            nodeAsText->GetLength((PRUint32*)&offset);
            NS_ENSURE_TRUE(offset, NS_ERROR_FAILURE);
            res = aSelection->Collapse(priorNode,offset);
            if (NS_FAILED(res)) return res;
            if (!bPlaintext)
            {
              PRInt32 so = offset-1;
              PRInt32 eo = offset;
              res = nsWSRunObject::PrepareToDeleteRange(mHTMLEditor, 
                                                      address_of(priorNode), &so, 
                                                      address_of(priorNode), &eo);
            }
            // just return without setting handled to true.
            // default code will take care of actual deletion
            return res;
          }
          // is prior node not a container?  (ie, a br, hr, image...)
          else if (!mHTMLEditor->IsContainer(priorNode))   // MOOSE: anchors not handled
          {
            if (!bPlaintext)
            {
              res = nsWSRunObject::PrepareToDeleteNode(mHTMLEditor, priorNode);
              if (NS_FAILED(res)) return res;
            }
            // remember prior sibling to prior node, if any
            nsCOMPtr<nsIDOMNode> sibling, stepbrother;  
            mHTMLEditor->GetPriorHTMLSibling(priorNode, address_of(sibling));
            // delete the break, and join like nodes if appropriate
            res = mHTMLEditor->DeleteNode(priorNode);
            if (NS_FAILED(res)) return res;
            // we did something, so lets say so.
            *aHandled = PR_TRUE;
            // is there a prior node and are they siblings?
            if (sibling)
              sibling->GetNextSibling(getter_AddRefs(stepbrother));
            if (startNode == stepbrother) 
            {
              // are they same type?
              if (mHTMLEditor->IsTextNode(stepbrother))
              {
                // if so, join them!
                res = JoinNodesSmart(sibling, startNode, address_of(selNode), &selOffset);
                if (NS_FAILED(res)) return res;
                // fix up selection
                res = aSelection->Collapse(selNode, selOffset);
              }
            }
            return res;
          }
          else if ( IsInlineNode(priorNode) )
          {
            if (!bPlaintext)
            {
              res = nsWSRunObject::PrepareToDeleteNode(mHTMLEditor, priorNode);
              if (NS_FAILED(res)) return res;
            }
            // remember where we are
            PRInt32 offset;
            nsCOMPtr<nsIDOMNode> node;
            res = mHTMLEditor->GetNodeLocation(priorNode, address_of(node), &offset);
            if (NS_FAILED(res)) return res;
            // delete it
            res = mHTMLEditor->DeleteNode(priorNode);
            if (NS_FAILED(res)) return res;
            // we did something, so lets say so.
            *aHandled = PR_TRUE;
            // fix up selection
            res = aSelection->Collapse(node,offset);
            return res;
          }
          else return NS_OK; // punt to default
        }
        
        // ---- deleting across blocks ---------------------------------------------
        
        // dont cross any table elements
        PRBool bInDifTblElems;
        res = InDifferentTableElements(startNode, priorNode, &bInDifTblElems);
        if (NS_FAILED(res)) return res;
        if (bInDifTblElems) 
        {
          *aCancel = PR_TRUE;
          return NS_OK;
        }

        nsCOMPtr<nsIDOMNode> leftParent;
        nsCOMPtr<nsIDOMNode> rightParent;
        if (IsBlockNode(priorNode))
          leftParent = priorNode;
        else
          leftParent = mHTMLEditor->GetBlockNodeParent(priorNode);
        if (IsBlockNode(startNode))
          rightParent = startNode;
        else
          rightParent = mHTMLEditor->GetBlockNodeParent(startNode);
        
        // if leftParent or rightParent is null, it's because the
        // corresponding selection endpoint is in the body node.
        if (!leftParent || !rightParent)
          return NS_OK;  // bail to default
          
        // are the blocks of same type?
        if (mHTMLEditor->NodesSameType(leftParent, rightParent))
        {
          if (!bPlaintext)
          {
            // adjust whitespace at block boundaries
            res = nsWSRunObject::PrepareToJoinBlocks(mHTMLEditor,leftParent,rightParent);
            if (NS_FAILED(res)) return res;
          }
          // join the nodes
          *aHandled = PR_TRUE;
          res = JoinNodesSmart(leftParent,rightParent,address_of(selNode),&selOffset);
          if (NS_FAILED(res)) return res;
          // fix up selection
          res = aSelection->Collapse(selNode,selOffset);
          return res;
        }
        
        // else blocks not same type, bail to default
        return NS_OK;
        
      }
    
      // at end of text node and deleted?
      if ((startOffset == (PRInt32)strLength)
          && (aAction == nsIEditor::eNext))
      {
        nsCOMPtr<nsIDOMNode> nextNode;
        res = mHTMLEditor->GetNextHTMLNode(startNode, address_of(nextNode));
        if (NS_FAILED(res)) return res;
         
        // if there is no next node, or it's not in the body, then cancel the deletion
        if (!nextNode || !nsTextEditUtils::InBody(nextNode, mHTMLEditor))
        {
          *aCancel = PR_TRUE;
          return res;
        }
       
        // block parents the same?  
        if (mHTMLEditor->HasSameBlockNodeParent(startNode, nextNode)) 
        {
          // is next node a text node?
          if ( mHTMLEditor->IsTextNode(nextNode) )
          {
            // delete first character
            nsCOMPtr<nsIDOMCharacterData>nodeAsText;
            nodeAsText = do_QueryInterface(nextNode);
            res = aSelection->Collapse(nextNode,0);
            if (NS_FAILED(res)) return res;
            if (!bPlaintext)
            {
              PRInt32 so = 0;
              PRInt32 eo = 1;
              res = nsWSRunObject::PrepareToDeleteRange(mHTMLEditor, 
                                                      address_of(nextNode), &so, 
                                                      address_of(nextNode), &eo);
            }
            // just return without setting handled to true.
            // default code will take care of actual deletion
            return res;
          }
          // is next node not a container?  (ie, a br, hr, image...)
          else if (!mHTMLEditor->IsContainer(nextNode))  // MOOSE: anchors not handled
          {
            if (!bPlaintext)
            {
              res = nsWSRunObject::PrepareToDeleteNode(mHTMLEditor, nextNode);
              if (NS_FAILED(res)) return res;
            }
            // remember prior sibling to prior node, if any
            nsCOMPtr<nsIDOMNode> sibling, stepbrother;  
            mHTMLEditor->GetNextHTMLSibling(nextNode, address_of(sibling));
            // delete the break, and join like nodes if appropriate
            res = mHTMLEditor->DeleteNode(nextNode);
            if (NS_FAILED(res)) return res;
            // we did something, so lets say so.
            *aHandled = PR_TRUE;
            // is there a prior node and are they siblings?
            if (sibling)
              sibling->GetPreviousSibling(getter_AddRefs(stepbrother));
            if (startNode == stepbrother) 
            {
              // are they same type?
              if (mHTMLEditor->IsTextNode(stepbrother))
              {
                // if so, join them!
                res = JoinNodesSmart(startNode, sibling, address_of(selNode), &selOffset);
                if (NS_FAILED(res)) return res;
                // fix up selection
                res = aSelection->Collapse(selNode, selOffset);
              }
            }
            return res;
          }
          else if ( IsInlineNode(nextNode) )
          {
            if (!bPlaintext)
            {
              res = nsWSRunObject::PrepareToDeleteNode(mHTMLEditor, nextNode);
              if (NS_FAILED(res)) return res;
            }
            // remember where we are
            PRInt32 offset;
            nsCOMPtr<nsIDOMNode> node;
            res = mHTMLEditor->GetNodeLocation(nextNode, address_of(node), &offset);
            if (NS_FAILED(res)) return res;
            // delete it
            res = mHTMLEditor->DeleteNode(nextNode);
            if (NS_FAILED(res)) return res;
            // we did something, so lets say so.
            *aHandled = PR_TRUE;
            // fix up selection
            res = aSelection->Collapse(node,offset);
            return res;
          }
          else return NS_OK; // punt to default
        }
                
        // ---- deleting across blocks ---------------------------------------------
        
        // dont cross any table elements
        PRBool bInDifTblElems;
        res = InDifferentTableElements(startNode, nextNode, &bInDifTblElems);
        if (NS_FAILED(res)) return res;
        if (bInDifTblElems) 
        {
          *aCancel = PR_TRUE;
          return NS_OK;
        }

        nsCOMPtr<nsIDOMNode> leftParent;
        nsCOMPtr<nsIDOMNode> rightParent;
        if (IsBlockNode(startNode))
          leftParent = startNode;
        else
          leftParent = mHTMLEditor->GetBlockNodeParent(startNode);
        if (IsBlockNode(nextNode))
          rightParent = nextNode;
        else
          rightParent = mHTMLEditor->GetBlockNodeParent(nextNode);
    
        // if leftParent or rightParent is null, it's because the
        // corresponding selection endpoint is in the body node.
        if (!leftParent || !rightParent)
          return NS_OK;  // bail to default
          
        // are the blocks of same type?
        if (mHTMLEditor->NodesSameType(leftParent, rightParent))
        {
          if (!bPlaintext)
          {
            // adjust whitespace at block boundaries
            res = nsWSRunObject::PrepareToJoinBlocks(mHTMLEditor,leftParent,rightParent);
            if (NS_FAILED(res)) return res;
          }
          // join the nodes
          *aHandled = PR_TRUE;
          res = JoinNodesSmart(leftParent,rightParent,address_of(selNode),&selOffset);
          if (NS_FAILED(res)) return res;
          // fix up selection
          res = aSelection->Collapse(selNode,selOffset);
          return res;
        }
        
        // else blocks not same type, bail to default
        return NS_OK;
      }
      // else in middle of text node.  default will do right thing.
      nsCOMPtr<nsIDOMCharacterData>nodeAsText;
      nodeAsText = do_QueryInterface(startNode);
      if (!nodeAsText) return NS_ERROR_NULL_POINTER;
      res = aSelection->Collapse(startNode,startOffset);
      if (NS_FAILED(res)) return res;
      if (!bPlaintext)
      {
        PRInt32 so = startOffset;
        PRInt32 eo = startOffset;
        if (aAction == nsIEditor::ePrevious)
          so--;  // we know so not zero - that case handled above
        else
          eo++;  // we know eo not at end of text node - that case handled above
        res = nsWSRunObject::PrepareToDeleteRange(mHTMLEditor, 
                                                address_of(startNode), &so, 
                                                address_of(startNode), &eo);
      }
      // just return without setting handled to true.
      // default code will take care of actual deletion
      return res;
    }
    // else not in text node; we need to find right place to act on
    else
    {
      nsCOMPtr<nsIDOMNode> nodeToDelete;
      
      // first note that the right node to delete might be the one we
      // are in.  For example, if a list item is deleted one character at a time,
      // eventually it will be empty (except for a moz-br).  If the user hits 
      // backspace again, they expect the item itself to go away.  Check to
      // see if we are in an "empty" node.
      // Note: do NOT delete table elements this way.
      PRBool bIsEmptyNode;
      res = mHTMLEditor->IsEmptyNode(startNode, &bIsEmptyNode, PR_TRUE, PR_FALSE);
      if (bIsEmptyNode && !nsHTMLEditUtils::IsTableElement(startNode))
        nodeToDelete = startNode;
      else 
      {
        // see if we are on empty line and need to delete it.  This is true
        // when a break is after a block and we are deleting backwards; or
        // when a break is before a block and we are delting forwards.  In
        // these cases, we want to delete the break when we are between it
        // and the block element, even though the break is on the "wrong"
        // side of us.
        nsCOMPtr<nsIDOMNode> maybeBreak;
        nsCOMPtr<nsIDOMNode> maybeBlock;
        
        if (aAction == nsIEditor::ePrevious)
        {
          res = mHTMLEditor->GetPriorHTMLSibling(startNode, startOffset, address_of(maybeBlock));
          if (NS_FAILED(res)) return res;
          res = mHTMLEditor->GetNextHTMLSibling(startNode, startOffset, address_of(maybeBreak));
          if (NS_FAILED(res)) return res;
        }
        else if (aAction == nsIEditor::eNext)
        {
          res = mHTMLEditor->GetPriorHTMLSibling(startNode, startOffset, address_of(maybeBreak));
          if (NS_FAILED(res)) return res;
          res = mHTMLEditor->GetNextHTMLSibling(startNode, startOffset, address_of(maybeBlock));
          if (NS_FAILED(res)) return res;
        }
        
        if (maybeBreak && maybeBlock &&
            nsTextEditUtils::IsBreak(maybeBreak) && IsBlockNode(maybeBlock))
          nodeToDelete = maybeBreak;
        else if (aAction == nsIEditor::ePrevious)
          res = mHTMLEditor->GetPriorHTMLNode(startNode, startOffset, address_of(nodeToDelete));
        else if (aAction == nsIEditor::eNext)
          res = mHTMLEditor->GetNextHTMLNode(startNode, startOffset, address_of(nodeToDelete));
        else
          return NS_OK;
      } 
      
      // don't delete the root element!
      if (NS_FAILED(res)) return res;
      if (!nodeToDelete) return NS_ERROR_NULL_POINTER;
      if (mBody == nodeToDelete) 
      {
        *aCancel = PR_TRUE;
        return res;
      }
      
      // dont cross any table elements
      PRBool bInDifTblElems;
      res = InDifferentTableElements(startNode, nodeToDelete, &bInDifTblElems);
      if (NS_FAILED(res)) return res;
      if (bInDifTblElems) 
      {
        *aCancel = PR_TRUE;
        return NS_OK;
      }
      
      // if this node is text node, adjust selection
      if (nsEditor::IsTextNode(nodeToDelete))
      {
        PRUint32 selPoint = 0;
        nsCOMPtr<nsIDOMCharacterData>nodeAsText;
        nodeAsText = do_QueryInterface(nodeToDelete);
        if (aAction == nsIEditor::ePrevious)
          nodeAsText->GetLength(&selPoint);
        res = aSelection->Collapse(nodeToDelete,selPoint);
        return res;
      }
      else
      {
        // editable leaf node is not text; delete it.
        // that's the default behavior
        // EXCEPTION: if it's a mozBR, we have to check and see if
        // there is a br in front of it. If so, we must delete both.
        // else you get this: deletion code deletes mozBR, then selection
        // adjusting code puts it back in.  doh
        if (nsTextEditUtils::IsMozBR(nodeToDelete))
        {
          nsCOMPtr<nsIDOMNode> brNode;
          res = mHTMLEditor->GetPriorHTMLNode(nodeToDelete, address_of(brNode));
          if (brNode && nsTextEditUtils::IsBreak(brNode))
          {
            // is brNode also a descendant of same block?
            nsCOMPtr<nsIDOMNode> block, brBlock;
            block = mHTMLEditor->GetBlockNodeParent(nodeToDelete);
            brBlock = mHTMLEditor->GetBlockNodeParent(brNode);
            if (block == brBlock)
            {
              // delete both breaks
              if (!bPlaintext)
              {
                res = nsWSRunObject::PrepareToDeleteNode(mHTMLEditor, brNode);
                if (NS_FAILED(res)) return res;
              }
              res = mHTMLEditor->DeleteNode(brNode);
              if (NS_FAILED(res)) return res;
              // fall through to delete other br
            }
            // else fall through
          }
          // else fall through
        }
        if (!bPlaintext)
        {
          res = nsWSRunObject::PrepareToDeleteNode(mHTMLEditor, nodeToDelete);
          if (NS_FAILED(res)) return res;
        }
        PRInt32 offset;
        nsCOMPtr<nsIDOMNode> node;
        res = nsEditor::GetNodeLocation(nodeToDelete, address_of(node), &offset);
        if (NS_FAILED(res)) return res;
        // adjust selection to be right after it
        res = aSelection->Collapse(node, offset+1);
        if (NS_FAILED(res)) return res;
        res = mHTMLEditor->DeleteNode(nodeToDelete);
        *aHandled = PR_TRUE;
        return res;
      }
    }
    
    return NS_OK;
  }
  
  // else we have a non collapsed selection
  // figure out if the enpoints are in nodes that can be merged
  nsCOMPtr<nsIDOMNode> endNode;
  PRInt32 endOffset;
  res = mHTMLEditor->GetEndNodeAndOffset(aSelection, address_of(endNode), &endOffset);
  if (NS_FAILED(res)) return res; 
  
  // adjust surrounding whitespace in preperation to delete selection
  if (!bPlaintext)
  {
    nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
    res = nsWSRunObject::PrepareToDeleteRange(mHTMLEditor,
                                            address_of(startNode), &startOffset, 
                                            address_of(endNode), &endOffset);
    if (NS_FAILED(res)) return res; 
  }
  if (endNode.get() != startNode.get())
  {
    {
      // track end location of where we are deleting
      nsAutoTrackDOMPoint tracker(mHTMLEditor->mRangeUpdater, address_of(endNode), &endOffset);
    
      // block parents the same?  use default deletion
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
      if (leftParent.get() == rightParent.get()) return NS_OK;
      
      // deleting across blocks
      // are the blocks of same type?
      
      // are the blocks siblings?
      nsCOMPtr<nsIDOMNode> leftBlockParent;
      nsCOMPtr<nsIDOMNode> rightBlockParent;
      leftParent->GetParentNode(getter_AddRefs(leftBlockParent));
      rightParent->GetParentNode(getter_AddRefs(rightBlockParent));

      // MOOSE: this could conceivably screw up a table.. fix me.
      if (   (leftBlockParent.get() == rightBlockParent.get())
          && (mHTMLEditor->NodesSameType(leftParent, rightParent))  )
      {
        if (nsHTMLEditUtils::IsParagraph(leftParent))
        {
          // first delete the selection
          *aHandled = PR_TRUE;
          res = mHTMLEditor->DeleteSelectionImpl(aAction);
          if (NS_FAILED(res)) return res;
          // then join para's, insert break
          res = mHTMLEditor->JoinNodeDeep(leftParent,rightParent,address_of(selNode),&selOffset);
          if (NS_FAILED(res)) return res;
          // fix up selection
          res = aSelection->Collapse(selNode,selOffset);
          return res;
        }
        if (nsHTMLEditUtils::IsListItem(leftParent)
            || nsHTMLEditUtils::IsHeader(leftParent))
        {
          // first delete the selection
          *aHandled = PR_TRUE;
          res = mHTMLEditor->DeleteSelectionImpl(aAction);
          if (NS_FAILED(res)) return res;
          // join blocks
          res = mHTMLEditor->JoinNodeDeep(leftParent,rightParent,address_of(selNode),&selOffset);
          if (NS_FAILED(res)) return res;
          // fix up selection
          res = aSelection->Collapse(selNode,selOffset);
          return res;
        }
      }
      
      // else blocks not same type, or not siblings.  Delete everything except
      // table elements.
      *aHandled = PR_TRUE;
      nsCOMPtr<nsIEnumerator> enumerator;
      nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(aSelection));
      res = selPriv->GetEnumerator(getter_AddRefs(enumerator));
      if (NS_FAILED(res)) return res;
      if (!enumerator) return NS_ERROR_UNEXPECTED;

      for (enumerator->First(); NS_OK!=enumerator->IsDone(); enumerator->Next())
      {
        nsCOMPtr<nsISupports> currentItem;
        res = enumerator->CurrentItem(getter_AddRefs(currentItem));
        if (NS_FAILED(res)) return res;
        if (!currentItem) return NS_ERROR_UNEXPECTED;

        // build a list of nodes in the range
        nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
        nsCOMPtr<nsISupportsArray> arrayOfNodes;
        nsTrivialFunctor functor;
        nsDOMSubtreeIterator iter;
        res = iter.Init(range);
        if (NS_FAILED(res)) return res;
        res = iter.MakeList(functor, address_of(arrayOfNodes));
        if (NS_FAILED(res)) return res;
    
        // now that we have the list, delete non table elements
        PRUint32 listCount;
        PRUint32 j;
        nsCOMPtr<nsIDOMNode> somenode;
        nsCOMPtr<nsISupports> isupports;

        arrayOfNodes->Count(&listCount);
        for (j = 0; j < listCount; j++)
        {
          isupports = dont_AddRef(arrayOfNodes->ElementAt(0));
          somenode = do_QueryInterface(isupports);
          res = DeleteNonTableElements(somenode);
          arrayOfNodes->RemoveElementAt(0);
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
          if (NS_FAILED(res)) return res;
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
          if (NS_FAILED(res)) return res;
        }
      }
    }
    if (aAction == nsIEditor::eNext)
    {
      res = aSelection->Collapse(endNode,endOffset);
    }
    else
    {
      res = aSelection->Collapse(startNode,startOffset);
    }
    return NS_OK;
  }

  return res;
}  

nsresult
nsHTMLEditRules::DeleteNonTableElements(nsIDOMNode *aNode)
{
  if (!aNode) return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;
  if (nsHTMLEditUtils::IsTableElement(aNode) && !nsHTMLEditUtils::IsTable(aNode))
  {
    nsCOMPtr<nsIDOMNodeList> children;
    aNode->GetChildNodes(getter_AddRefs(children));
    if (children)
    {
      PRUint32 len;
      children->GetLength(&len);
      if (!len) return NS_OK;
      PRInt32 j;
      for (j=len-1; j>=0; j--)
      {
        nsCOMPtr<nsIDOMNode> node;
        children->Item(j,getter_AddRefs(node));
        res = DeleteNonTableElements(node);
        if (NS_FAILED(res)) return res;
      }
    }
  }
  else
  {
    res = mHTMLEditor->DeleteNode(aNode);
    if (NS_FAILED(res)) return res;
  }
  return res;
}

nsresult
nsHTMLEditRules::WillMakeList(nsISelection *aSelection, 
                              const nsAReadableString *aListType, 
                              PRBool aEntireList,
                              PRBool *aCancel,
                              PRBool *aHandled,
                              const nsAReadableString *aItemType)
{
  if (!aSelection || !aListType || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }

  nsresult res = WillInsert(aSelection, aCancel);
  if (NS_FAILED(res)) return res;

  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;

  // deduce what tag to use for list items
  nsAutoString itemType;
  if (aItemType) 
    itemType = *aItemType;
  else if (!Compare(*aListType,NS_LITERAL_STRING("dl"),nsCaseInsensitiveStringComparator()))
    itemType.AssignWithConversion("dd");
  else
    itemType.AssignWithConversion("li");
    
  // convert the selection ranges into "promoted" selection ranges:
  // this basically just expands the range to include the immediate
  // block parent, and then further expands to include any ancestors
  // whose children are all in the range
  
  *aHandled = PR_TRUE;

  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);

  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  res = GetListActionNodes(address_of(arrayOfNodes), aEntireList);
  if (NS_FAILED(res)) return res;
  
  PRUint32 listCount;
  arrayOfNodes->Count(&listCount);
  
  // check if all our nodes are <br>s, or empty inlines
  PRBool bOnlyBreaks = PR_TRUE;
  PRInt32 j;
  for (j=0; j<(PRInt32)listCount; j++)
  {
    nsCOMPtr<nsISupports> isupports = dont_AddRef(arrayOfNodes->ElementAt(j));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports ) );
    // if curNode is not a Break or empty inline, we're done
    if ( (!nsTextEditUtils::IsBreak(curNode)) && (!IsEmptyInline(curNode)) )
    {
      bOnlyBreaks = PR_FALSE;
      break;
    }
  }
  
  // if no nodes, we make empty list.  Ditto if the user tried to make a list of some # of breaks.
  if (!listCount || bOnlyBreaks) 
  {
    nsCOMPtr<nsIDOMNode> parent, theList, theListItem;
    PRInt32 offset;

    // if only breaks, delete them
    if (bOnlyBreaks)
    {
      for (j=0; j<(PRInt32)listCount; j++)
      {
        nsCOMPtr<nsISupports> isupports = dont_AddRef(arrayOfNodes->ElementAt(j));
        nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports ) );
        res = mHTMLEditor->DeleteNode(curNode);
        if (NS_FAILED(res)) return res;
      }
    }
    
    // get selection location
    res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(parent), &offset);
    if (NS_FAILED(res)) return res;
    
    // make sure we can put a list here
    res = SplitAsNeeded(aListType, address_of(parent), &offset);
    if (NS_FAILED(res)) return res;
    res = mHTMLEditor->CreateNode(*aListType, parent, offset, getter_AddRefs(theList));
    if (NS_FAILED(res)) return res;
    res = mHTMLEditor->CreateNode(itemType, theList, 0, getter_AddRefs(theListItem));
    if (NS_FAILED(res)) return res;
    // remember our new block for postprocessing
    mNewBlock = theListItem;
    // put selection in new list item
    res = aSelection->Collapse(theListItem,0);
    selectionResetter.Abort();  // to prevent selection reseter from overriding us.
    *aHandled = PR_TRUE;
    return res;
  }

  // if there is only one node in the array, and it is a list, div, or blockquote,
  // then look inside of it until we find inner list or content.

  res = LookInsideDivBQandList(arrayOfNodes);
  if (NS_FAILED(res)) return res;                                 

  // Next we detect all the transitions in the array, where a transition
  // means that adjacent nodes in the array don't have the same parent.
  
  nsVoidArray transitionList;
  res = MakeTransitionList(arrayOfNodes, &transitionList);
  if (NS_FAILED(res)) return res;                                 

  // Ok, now go through all the nodes and put then in the list, 
  // or whatever is approriate.  Wohoo!

  arrayOfNodes->Count(&listCount);
  nsCOMPtr<nsIDOMNode> curParent;
  nsCOMPtr<nsIDOMNode> curList;
  nsCOMPtr<nsIDOMNode> prevListItem;
  
  PRInt32 i;
  for (i=0; i<(PRInt32)listCount; i++)
  {
    // here's where we actually figure out what to do
    nsCOMPtr<nsIDOMNode> newBlock;
    nsCOMPtr<nsISupports> isupports = dont_AddRef(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports ) );
    PRInt32 offset;
    res = nsEditor::GetNodeLocation(curNode, address_of(curParent), &offset);
    if (NS_FAILED(res)) return res;
  
    // if curNode is a Break, delete it, and quit remembering prev list item
    if (nsTextEditUtils::IsBreak(curNode)) 
    {
      res = mHTMLEditor->DeleteNode(curNode);
      if (NS_FAILED(res)) return res;
      prevListItem = 0;
      continue;
    }
    // if curNode is an empty inline container, delete it
    else if (IsEmptyInline(curNode)) 
    {
      res = mHTMLEditor->DeleteNode(curNode);
      if (NS_FAILED(res)) return res;
      continue;
    }
    
    if (nsHTMLEditUtils::IsList(curNode))
    {
      nsAutoString existingListStr;
      res = mHTMLEditor->GetTagString(curNode, existingListStr);
      existingListStr.ToLowerCase();
      // do we have a curList already?
      if (curList && !nsHTMLEditUtils::IsDescendantOf(curNode, curList))
      {
        // move all of our children into curList.
        // cheezy way to do it: move whole list and then
        // RemoveContainer() on the list.
        // ConvertListType first: that routine
        // handles converting the list item types, if needed
        res = mHTMLEditor->MoveNode(curNode, curList, -1);
        if (NS_FAILED(res)) return res;
        res = ConvertListType(curNode, address_of(newBlock), *aListType, itemType);
        if (NS_FAILED(res)) return res;
        res = mHTMLEditor->RemoveBlockContainer(newBlock);
        if (NS_FAILED(res)) return res;
      }
      else
      {
        // replace list with new list type
        res = ConvertListType(curNode, address_of(newBlock), *aListType, itemType);
        if (NS_FAILED(res)) return res;
        curList = newBlock;
      }
      prevListItem = 0;
      continue;
    }

    if (nsHTMLEditUtils::IsListItem(curNode))
    {
      nsAutoString existingListStr;
      res = mHTMLEditor->GetTagString(curParent, existingListStr);
      existingListStr.ToLowerCase();
      if ( existingListStr != *aListType )
      {
        // list item is in wrong type of list.  
        // if we dont have a curList, split the old list
        // and make a new list of correct type.
        if (!curList || nsHTMLEditUtils::IsDescendantOf(curNode, curList))
        {
          res = mHTMLEditor->SplitNode(curParent, offset, getter_AddRefs(newBlock));
          if (NS_FAILED(res)) return res;
          nsCOMPtr<nsIDOMNode> p;
          PRInt32 o;
          res = nsEditor::GetNodeLocation(curParent, address_of(p), &o);
          if (NS_FAILED(res)) return res;
          res = mHTMLEditor->CreateNode(*aListType, p, o, getter_AddRefs(curList));
          if (NS_FAILED(res)) return res;
        }
        // move list item to new list
        res = mHTMLEditor->MoveNode(curNode, curList, -1);
        if (NS_FAILED(res)) return res;
        // convert list item type if needed
        if (!mHTMLEditor->NodeIsType(curNode,itemType))
        {
          res = mHTMLEditor->ReplaceContainer(curNode, address_of(newBlock), itemType);
          if (NS_FAILED(res)) return res;
        }
      }
      else
      {
        // item is in right type of list.  But we might still have to move it.
        // and we might need to convert list item types.
        if (!curList)
          curList = curParent;
        else
        {
          if (curParent != curList)
          {
            // move list item to new list
            res = mHTMLEditor->MoveNode(curNode, curList, -1);
            if (NS_FAILED(res)) return res;
          }
        }
        if (!mHTMLEditor->NodeIsType(curNode,itemType))
        {
          res = mHTMLEditor->ReplaceContainer(curNode, address_of(newBlock), itemType);
          if (NS_FAILED(res)) return res;
        }
      }
      continue;
    }
    
    // need to make a list to put things in if we haven't already,
    // or if this node doesn't go in list we used earlier.
    if (!curList) // || transitionList[i])
    {
      res = SplitAsNeeded(aListType, address_of(curParent), &offset);
      if (NS_FAILED(res)) return res;
      res = mHTMLEditor->CreateNode(*aListType, curParent, offset, getter_AddRefs(curList));
      if (NS_FAILED(res)) return res;
      // remember our new block for postprocessing
      mNewBlock = curList;
      // curList is now the correct thing to put curNode in
      prevListItem = 0;
    }
  
    // if curNode isn't a list item, we must wrap it in one
    nsCOMPtr<nsIDOMNode> listItem;
    if (!nsHTMLEditUtils::IsListItem(curNode))
    {
      if (IsInlineNode(curNode) && prevListItem)
      {
        // this is a continuation of some inline nodes that belong together in
        // the same list item.  use prevListItem
        PRUint32 listItemLen;
        res = mHTMLEditor->GetLengthOfDOMNode(prevListItem, listItemLen);
        if (NS_FAILED(res)) return res;
        res = mHTMLEditor->MoveNode(curNode, prevListItem, listItemLen);
        if (NS_FAILED(res)) return res;
      }
      else
      {
        // don't wrap li around a paragraph.  instead replace paragraph with li
        if (nsHTMLEditUtils::IsParagraph(curNode))
        {
          res = mHTMLEditor->ReplaceContainer(curNode, address_of(listItem), itemType);
        }
        else
        {
          res = mHTMLEditor->InsertContainerAbove(curNode, address_of(listItem), itemType);
        }
        if (NS_FAILED(res)) return res;
        if (IsInlineNode(curNode)) 
          prevListItem = listItem;
      }
    }
    else
    {
      listItem = curNode;
    }
  
    if (listItem)  // if we made a new list item, deal with it
    {
      // tuck the listItem into the end of the active list
      PRUint32 listLen;
      res = mHTMLEditor->GetLengthOfDOMNode(curList, listLen);
      if (NS_FAILED(res)) return res;
      res = mHTMLEditor->MoveNode(listItem, curList, listLen);
      if (NS_FAILED(res)) return res;
    }
  }

  return res;
}


nsresult
nsHTMLEditRules::WillRemoveList(nsISelection *aSelection, 
                                PRBool aOrdered, 
                                PRBool *aCancel,
                                PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_TRUE;
  
  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);
  
  nsCOMPtr<nsISupportsArray> arrayOfRanges;
  nsresult res = GetPromotedRanges(aSelection, address_of(arrayOfRanges), kMakeList);
  if (NS_FAILED(res)) return res;
  
  // use these ranges to contruct a list of nodes to act on.
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  res = GetListActionNodes(address_of(arrayOfNodes), PR_FALSE);
  if (NS_FAILED(res)) return res;                                 
                                     
  // Remove all non-editable nodes.  Leave them be.
  
  PRUint32 listCount;
  PRInt32 i;
  arrayOfNodes->Count(&listCount);
  for (i=listCount-1; i>=0; i--)
  {
    nsCOMPtr<nsISupports> isupports = dont_AddRef(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> testNode( do_QueryInterface(isupports ) );
    if (!mHTMLEditor->IsEditable(testNode))
    {
      arrayOfNodes->RemoveElementAt(i);
    }
  }
  
  // Only act on lists or list items in the array
  nsCOMPtr<nsIDOMNode> curParent;
  for (i=0; i<(PRInt32)listCount; i++)
  {
    // here's where we actually figure out what to do
    nsCOMPtr<nsISupports> isupports = dont_AddRef(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports ) );
    PRInt32 offset;
    res = nsEditor::GetNodeLocation(curNode, address_of(curParent), &offset);
    if (NS_FAILED(res)) return res;
    
    if (nsHTMLEditUtils::IsListItem(curNode))  // unlist this listitem
    {
      PRBool bOutOfList;
      do
      {
        res = PopListItem(curNode, &bOutOfList);
        if (NS_FAILED(res)) return res;
      } while (!bOutOfList); // keep popping it out until it's not in a list anymore
    }
    else if (nsHTMLEditUtils::IsList(curNode)) // node is a list, move list items out
    {
      res = RemoveListStructure(curNode);
      if (NS_FAILED(res)) return res;
    }
  }
  return res;
}


nsresult
nsHTMLEditRules::WillMakeDefListItem(nsISelection *aSelection, 
                                     const nsAReadableString *aItemType, 
                                     PRBool aEntireList, 
                                     PRBool *aCancel,
                                     PRBool *aHandled)
{
  // for now we let WillMakeList handle this
  nsAutoString listType; 
  listType.AssignWithConversion("dl");
  return WillMakeList(aSelection, &listType, aEntireList, aCancel, aHandled, aItemType);
}

nsresult
nsHTMLEditRules::WillMakeBasicBlock(nsISelection *aSelection, 
                                    const nsAReadableString *aBlockType, 
                                    PRBool *aCancel,
                                    PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;
  
  nsresult res = WillInsert(aSelection, aCancel);
  if (NS_FAILED(res)) return res;
  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = PR_FALSE;
  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);
  nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
  *aHandled = PR_TRUE;

  nsCOMPtr<nsISupportsArray> arrayOfRanges;
  res = GetPromotedRanges(aSelection, address_of(arrayOfRanges), kMakeBasicBlock);
  if (NS_FAILED(res)) return res;
  
  // use these ranges to contruct a list of nodes to act on.
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  res = GetNodesForOperation(arrayOfRanges, address_of(arrayOfNodes), kMakeBasicBlock);
  if (NS_FAILED(res)) return res;                                 
  
  nsString tString(*aBlockType);

  // if nothing visible in list, make an empty block
  if (ListIsEmptyLine(arrayOfNodes))
  {
    nsCOMPtr<nsIDOMNode> parent, theBlock;
    PRInt32 offset;
    
    // get selection location
    res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(parent), &offset);
    if (NS_FAILED(res)) return res;
    if (tString.EqualsWithConversion("normal") ||
             tString.IsEmpty() ) // we are removing blocks (going to "body text")
    {
      nsCOMPtr<nsIDOMNode> curBlock = parent;
      if (!IsBlockNode(curBlock))
        curBlock = mHTMLEditor->GetBlockNodeParent(parent);
      nsCOMPtr<nsIDOMNode> curBlockPar;
      if (!curBlock) return NS_ERROR_NULL_POINTER;
      curBlock->GetParentNode(getter_AddRefs(curBlockPar));
      nsAutoString curBlockTag;
      nsEditor::GetTagString(curBlock, curBlockTag);
      curBlockTag.ToLowerCase();
      if ((curBlockTag.EqualsWithConversion("pre")) || 
          (curBlockTag.EqualsWithConversion("p"))   ||
          (curBlockTag.EqualsWithConversion("h1"))  ||
          (curBlockTag.EqualsWithConversion("h2"))  ||
          (curBlockTag.EqualsWithConversion("h3"))  ||
          (curBlockTag.EqualsWithConversion("h4"))  ||
          (curBlockTag.EqualsWithConversion("h5"))  ||
          (curBlockTag.EqualsWithConversion("h6"))  ||
          (curBlockTag.EqualsWithConversion("address")))
      {
        // if the first editable node after selection is a br, consume it.  Otherwise
        // it gets pushed into a following block after the split, which is visually bad.
        nsCOMPtr<nsIDOMNode> brNode;
        res = mHTMLEditor->GetNextHTMLNode(parent, offset, address_of(brNode));
        if (NS_FAILED(res)) return res;        
        if (brNode && nsTextEditUtils::IsBreak(brNode))
        {
          res = mHTMLEditor->DeleteNode(brNode);
          if (NS_FAILED(res)) return res; 
        }
        // do the splits!
        res = mHTMLEditor->SplitNodeDeep(curBlock, parent, offset, &offset, PR_TRUE);
        if (NS_FAILED(res)) return res;
        // put a br at the split point
        res = mHTMLEditor->CreateBR(curBlockPar, offset, address_of(brNode));
        if (NS_FAILED(res)) return res;
        // put selection at the split point
        res = aSelection->Collapse(curBlockPar, offset);
        selectionResetter.Abort();  // to prevent selection reseter from overriding us.
        *aHandled = PR_TRUE;
      }
      // else nothing to do!
    }
    else  // we are making a block
    {   
      // consume a br, if needed
      nsCOMPtr<nsIDOMNode> brNode;
      res = mHTMLEditor->GetNextHTMLNode(parent, offset, address_of(brNode), PR_TRUE);
      if (NS_FAILED(res)) return res;
      if (brNode && nsTextEditUtils::IsBreak(brNode))
      {
        res = mHTMLEditor->DeleteNode(brNode);
        if (NS_FAILED(res)) return res;
      }
      // make sure we can put a block here
      res = SplitAsNeeded(aBlockType, address_of(parent), &offset);
      if (NS_FAILED(res)) return res;
      res = mHTMLEditor->CreateNode(*aBlockType, parent, offset, getter_AddRefs(theBlock));
      if (NS_FAILED(res)) return res;
      // remember our new block for postprocessing
      mNewBlock = theBlock;
      // delete anything that was in the list of nodes
      nsCOMPtr<nsISupports> isupports = dont_AddRef(arrayOfNodes->ElementAt(0));
      nsCOMPtr<nsIDOMNode> curNode;
      while (isupports)
      {
        curNode = do_QueryInterface(isupports);
        res = mHTMLEditor->DeleteNode(curNode);
        if (NS_FAILED(res)) return res;
        res = arrayOfNodes->RemoveElementAt(0);
        if (NS_FAILED(res)) return res;
        isupports = dont_AddRef(arrayOfNodes->ElementAt(0));
      }
      // put selection in new block
      res = aSelection->Collapse(theBlock,0);
      selectionResetter.Abort();  // to prevent selection reseter from overriding us.
      *aHandled = PR_TRUE;
    }
    return res;    
  }
  else
  {
    // Ok, now go through all the nodes and make the right kind of blocks, 
    // or whatever is approriate.  Wohoo! 
    // Note: blockquote is handled a little differently
    if (tString.EqualsWithConversion("blockquote"))
      res = MakeBlockquote(arrayOfNodes);
    else if (tString.EqualsWithConversion("normal") ||
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
  if (!aSelection) return NS_ERROR_NULL_POINTER;
  // check for empty block.  if so, put a moz br in it.
  PRBool isCollapsed;
  nsresult res = aSelection->GetIsCollapsed(&isCollapsed);
  if (NS_FAILED(res)) return res;
  if (!isCollapsed) return NS_OK;

  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset;
  res = nsEditor::GetStartNodeAndOffset(aSelection, address_of(parent), &offset);
  if (NS_FAILED(res)) return res;
  res = InsertMozBRIfNeeded(parent);
  return res;
}

nsresult
nsHTMLEditRules::WillIndent(nsISelection *aSelection, PRBool *aCancel, PRBool * aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  
  nsresult res = WillInsert(aSelection, aCancel);
  if (NS_FAILED(res)) return res;

  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = PR_FALSE;
  *aHandled = PR_TRUE;

  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);
  nsCOMPtr<nsISupportsArray> arrayOfRanges;
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  
  // short circuit: detect case of collapsed selection inside an <li>.
  // just sublist that <li>.  This prevents bug 97797.
  
  PRBool bCollapsed;
  nsCOMPtr<nsIDOMNode> liNode;
  res = aSelection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  if (bCollapsed) 
  {
    nsCOMPtr<nsIDOMNode> node, block;
    PRInt32 offset;
    nsresult res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(node), &offset);
    if (NS_FAILED(res)) return res;
    if (IsBlockNode(node)) 
      block = node;
    else
      block = mHTMLEditor->GetBlockNodeParent(node);
    if (block && nsHTMLEditUtils::IsListItem(block))
      liNode = block;
  }
  
  if (liNode)
  {
    res = NS_NewISupportsArray(getter_AddRefs(arrayOfNodes));
    if (NS_FAILED(res)) return res;
    nsCOMPtr<nsISupports> isupports = do_QueryInterface(liNode);
    arrayOfNodes->AppendElement(isupports);
  }
  else
  {
    // convert the selection ranges into "promoted" selection ranges:
    // this basically just expands the range to include the immediate
    // block parent, and then further expands to include any ancestors
    // whose children are all in the range
    
    res = GetPromotedRanges(aSelection, address_of(arrayOfRanges), kIndent);
    if (NS_FAILED(res)) return res;
    
    // use these ranges to contruct a list of nodes to act on.
    res = GetNodesForOperation(arrayOfRanges, address_of(arrayOfNodes), kIndent);
    if (NS_FAILED(res)) return res;                                 
  }
  
  // if nothing visible in list, make an empty block
  if (ListIsEmptyLine(arrayOfNodes))
  {
    nsCOMPtr<nsIDOMNode> parent, theBlock;
    PRInt32 offset;
    nsAutoString quoteType; quoteType.AssignWithConversion("blockquote");
    
    // get selection location
    res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(parent), &offset);
    if (NS_FAILED(res)) return res;
    // make sure we can put a block here
    res = SplitAsNeeded(&quoteType, address_of(parent), &offset);
    if (NS_FAILED(res)) return res;
    res = mHTMLEditor->CreateNode(quoteType, parent, offset, getter_AddRefs(theBlock));
    if (NS_FAILED(res)) return res;
    // remember our new block for postprocessing
    mNewBlock = theBlock;
    // delete anything that was in the list of nodes
    nsCOMPtr<nsISupports> isupports = dont_AddRef(arrayOfNodes->ElementAt(0));
    nsCOMPtr<nsIDOMNode> curNode;
    while (isupports)
    {
      curNode = do_QueryInterface(isupports);
      res = mHTMLEditor->DeleteNode(curNode);
      if (NS_FAILED(res)) return res;
      res = arrayOfNodes->RemoveElementAt(0);
      if (NS_FAILED(res)) return res;
      isupports = dont_AddRef(arrayOfNodes->ElementAt(0));
    }
    // put selection in new block
    res = aSelection->Collapse(theBlock,0);
    selectionResetter.Abort();  // to prevent selection reseter from overriding us.
    *aHandled = PR_TRUE;
    return res;
  }

  // Next we detect all the transitions in the array, where a transition
  // means that adjacent nodes in the array don't have the same parent.
  
  nsVoidArray transitionList;
  res = MakeTransitionList(arrayOfNodes, &transitionList);
  if (NS_FAILED(res)) return res;                                 
  
  // Ok, now go through all the nodes and put them in a blockquote, 
  // or whatever is appropriate.  Wohoo!
  PRInt32 i;
  nsCOMPtr<nsIDOMNode> curParent;
  nsCOMPtr<nsIDOMNode> curQuote;
  nsCOMPtr<nsIDOMNode> curList;
  PRUint32 listCount;
  arrayOfNodes->Count(&listCount);
  for (i=0; i<(PRInt32)listCount; i++)
  {
    // here's where we actually figure out what to do
    nsCOMPtr<nsISupports> isupports = dont_AddRef(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports ) );
    PRInt32 offset;
    res = nsEditor::GetNodeLocation(curNode, address_of(curParent), &offset);
    if (NS_FAILED(res)) return res;
    
    // some logic for putting list items into nested lists...
    if (nsHTMLEditUtils::IsList(curParent))
    {
      if (!curList || transitionList[i])
      {
        nsAutoString listTag;
        nsEditor::GetTagString(curParent,listTag);
        listTag.ToLowerCase();
        // create a new nested list of correct type
        res = SplitAsNeeded(&listTag, address_of(curParent), &offset);
        if (NS_FAILED(res)) return res;
        res = mHTMLEditor->CreateNode(listTag, curParent, offset, getter_AddRefs(curList));
        if (NS_FAILED(res)) return res;
        // curList is now the correct thing to put curNode in
        // remember our new block for postprocessing
        mNewBlock = curList;
      }
      // tuck the node into the end of the active list
      PRUint32 listLen;
      res = mHTMLEditor->GetLengthOfDOMNode(curList, listLen);
      if (NS_FAILED(res)) return res;
      res = mHTMLEditor->MoveNode(curNode, curList, listLen);
      if (NS_FAILED(res)) return res;
    }
    
    else // not a list item, use blockquote
    {
      // need to make a blockquote to put things in if we haven't already,
      // or if this node doesn't go in blockquote we used earlier.
      if (!curQuote) // || transitionList[i])
      {
        nsAutoString quoteType; quoteType.AssignWithConversion("blockquote");
        res = SplitAsNeeded(&quoteType, address_of(curParent), &offset);
        if (NS_FAILED(res)) return res;
        res = mHTMLEditor->CreateNode(quoteType, curParent, offset, getter_AddRefs(curQuote));
        if (NS_FAILED(res)) return res;
        // remember our new block for postprocessing
        mNewBlock = curQuote;
        
/* !!!!!!!!!!!!!!!  TURNED OFF PER BUG 33213 !!!!!!!!!!!!!!!!!!!!
        // set style to not have unwanted vertical margins
        nsCOMPtr<nsIDOMElement> quoteElem = do_QueryInterface(curQuote);
        res = mHTMLEditor->SetAttribute(quoteElem, NS_ConvertASCIItoUCS2("style"), NS_ConvertASCIItoUCS2("margin: 0 0 0 40px;"));
        if (NS_FAILED(res)) return res;
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/

        // curQuote is now the correct thing to put curNode in
      }
        
      // tuck the node into the end of the active blockquote
      PRUint32 quoteLen;
      res = mHTMLEditor->GetLengthOfDOMNode(curQuote, quoteLen);
      if (NS_FAILED(res)) return res;
      res = mHTMLEditor->MoveNode(curNode, curQuote, quoteLen);
      if (NS_FAILED(res)) return res;
    }
  }
  return res;
}


nsresult
nsHTMLEditRules::WillOutdent(nsISelection *aSelection, PRBool *aCancel, PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_TRUE;
  
  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);
  nsresult res = NS_OK;
  
  // convert the selection ranges into "promoted" selection ranges:
  // this basically just expands the range to include the immediate
  // block parent, and then further expands to include any ancestors
  // whose children are all in the range
  
  nsCOMPtr<nsISupportsArray> arrayOfRanges;
  res = GetPromotedRanges(aSelection, address_of(arrayOfRanges), kOutdent);
  if (NS_FAILED(res)) return res;
  
  // use these ranges to contruct a list of nodes to act on.

  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  res = GetNodesForOperation(arrayOfRanges, address_of(arrayOfNodes), kOutdent);
  if (NS_FAILED(res)) return res;                                 
                                     
  // Ok, now go through all the nodes and remove a level of blockquoting, 
  // or whatever is appropriate.  Wohoo!

  nsCOMPtr<nsIDOMNode> curBlockQuote, firstBQChild, lastBQChild;
  PRUint32 listCount;
  PRInt32 i;
  arrayOfNodes->Count(&listCount);
  nsCOMPtr<nsIDOMNode> curParent;
  for (i=0; i<(PRInt32)listCount; i++)
  {
    // here's where we actually figure out what to do
    nsCOMPtr<nsISupports> isupports = dont_AddRef(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports ) );
    PRInt32 offset;
    res = nsEditor::GetNodeLocation(curNode, address_of(curParent), &offset);
    if (NS_FAILED(res)) return res;
    
    // is it a blockquote?
    if (nsHTMLEditUtils::IsBlockquote(curNode)) 
    {
      // if it is a blockquote, remove it.
      // So we need to finish up dealng with any curBlockQuote first.
      if (curBlockQuote)
      {
        res = RemovePartOfBlock(curBlockQuote, firstBQChild, lastBQChild);
        if (NS_FAILED(res)) return res;
        curBlockQuote = 0;  firstBQChild = 0;  lastBQChild = 0;
      }
      res = mHTMLEditor->RemoveBlockContainer(curNode);
      if (NS_FAILED(res)) return res;
      continue;
    }
    // is it a list item?
    if (nsHTMLEditUtils::IsListItem(curNode)) 
    {
      // if it is a list item, that means we are not outdenting whole list.
      // So we need to finish up dealng with any curBlockQuote, and then
      // pop this list item.
      if (curBlockQuote)
      {
        res = RemovePartOfBlock(curBlockQuote, firstBQChild, lastBQChild);
        if (NS_FAILED(res)) return res;
        curBlockQuote = 0;  firstBQChild = 0;  lastBQChild = 0;
      }
      PRBool bOutOfList;
      res = PopListItem(curNode, &bOutOfList);
      if (NS_FAILED(res)) return res;
      continue;
    }
    // do we have a blockquote that we are already committed to removing?
    if (curBlockQuote)
    {
      // if so, is this node a descendant?
      if (nsHTMLEditUtils::IsDescendantOf(curNode, curBlockQuote))
      {
        lastBQChild = curNode;
        continue;  // then we dont need to do anything different for this node
      }
      else
      {
        // otherwise, we have progressed beyond end of curBlockQuote,
        // so lets handle it now.  We need to remove the portion of 
        // curBlockQuote that contains [firstBQChild - lastBQChild].
        res = RemovePartOfBlock(curBlockQuote, firstBQChild, lastBQChild);
        if (NS_FAILED(res)) return res;
        curBlockQuote = 0;  firstBQChild = 0;  lastBQChild = 0;
        // fall out and handle curNode
      }
    }
    
    // are we inside a blockquote?
    nsCOMPtr<nsIDOMNode> n = curNode;
    nsCOMPtr<nsIDOMNode> tmp;
    while (!nsTextEditUtils::IsBody(n))
    {
      n->GetParentNode(getter_AddRefs(tmp));
      n = tmp;
      if (nsHTMLEditUtils::IsBlockquote(n))
      {
        // if so, remember it, and remember first node we are taking out of it.
        curBlockQuote = n;
        firstBQChild  = curNode;
        lastBQChild   = curNode;
        break;
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
          if (NS_FAILED(res)) return res;
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
            PRBool bOutOfList;
            res = PopListItem(child, &bOutOfList);
            if (NS_FAILED(res)) return res;
          }
          else if (nsHTMLEditUtils::IsList(child))
          {
            // We have an embedded list, so move it out from under the
            // parent list. Be sure to put it after the parent list
            // because this loop iterates backwards through the parent's
            // list of children.

            res = mHTMLEditor->MoveNode(child, curParent, offset + 1);
            if (NS_FAILED(res)) return res;
          }
          else
          {
            // delete any non- list items for now
            res = mHTMLEditor->DeleteNode(child);
            if (NS_FAILED(res)) return res;
          }
          curNode->GetLastChild(getter_AddRefs(child));
        }
        // delete the now-empty list
        res = mHTMLEditor->RemoveBlockContainer(curNode);
        if (NS_FAILED(res)) return res;
      }
    }
  }
  if (curBlockQuote)
  {
    // we have a blockquote we haven't finished handling
    res = RemovePartOfBlock(curBlockQuote, firstBQChild, lastBQChild);
    if (NS_FAILED(res)) return res;
  }

  return res;
}


///////////////////////////////////////////////////////////////////////////
// ConvertListType:  convert list type and list item type.
//                
//                  
nsresult 
nsHTMLEditRules::RemovePartOfBlock(nsIDOMNode *aBlock, 
                                   nsIDOMNode *aStartChild, 
                                   nsIDOMNode *aEndChild)
{
  if (!aBlock || !aStartChild || !aEndChild)
    return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIDOMNode> startParent, endParent, leftNode, rightNode;
  PRInt32 startOffset, endOffset, offset;
  nsresult res;

  // get split point location
  res = nsEditor::GetNodeLocation(aStartChild, address_of(startParent), &startOffset);
  if (NS_FAILED(res)) return res;
  
  // do the splits!
  res = mHTMLEditor->SplitNodeDeep(aBlock, startParent, startOffset, &offset, 
                                   PR_TRUE, address_of(leftNode), address_of(rightNode));
  if (NS_FAILED(res)) return res;
  if (rightNode)  aBlock = rightNode;

  // get split point location
  res = nsEditor::GetNodeLocation(aEndChild, address_of(endParent), &endOffset);
  if (NS_FAILED(res)) return res;
  endOffset++;  // want to be after lastBQChild

  // do the splits!
  res = mHTMLEditor->SplitNodeDeep(aBlock, endParent, endOffset, &offset, 
                                   PR_TRUE, address_of(leftNode), address_of(rightNode));
  if (NS_FAILED(res)) return res;
  if (leftNode)  aBlock = leftNode;
  
  // get rid of part of blockquote we are outdenting
  res = mHTMLEditor->RemoveBlockContainer(aBlock);
  return res;
}

///////////////////////////////////////////////////////////////////////////
// ConvertListType:  convert list type and list item type.
//                
//                  
nsresult 
nsHTMLEditRules::ConvertListType(nsIDOMNode *aList, 
                                 nsCOMPtr<nsIDOMNode> *outList,
                                 const nsAReadableString& aListType, 
                                 const nsAReadableString& aItemType) 
{
  if (!aList || !outList) return NS_ERROR_NULL_POINTER;
  *outList = aList;  // we migvht not need to change the list
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> child, temp;
  aList->GetFirstChild(getter_AddRefs(child));
  while (child)
  {
    if (nsHTMLEditUtils::IsListItem(child) && !mHTMLEditor->NodeIsType(child, aItemType))
    {
      res = mHTMLEditor->ReplaceContainer(child, address_of(temp), aItemType);
      if (NS_FAILED(res)) return res;
      child = temp;
    }
    else if (nsHTMLEditUtils::IsList(child) && !mHTMLEditor->NodeIsType(child, aListType))
    {
      res = ConvertListType(child, address_of(temp), aListType, aItemType);
      if (NS_FAILED(res)) return res;
      child = temp;
    }
    child->GetNextSibling(getter_AddRefs(temp));
    child = temp;
  }
  if (!mHTMLEditor->NodeIsType(aList, aListType))
  {
    res = mHTMLEditor->ReplaceContainer(aList, outList, aListType);
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// CreateStyleForInsertText:  take care of clearing and setting appropriate
//                            style nodes for text insertion.
//                
//                  
nsresult 
nsHTMLEditRules::CreateStyleForInsertText(nsISelection *aSelection, nsIDOMDocument *aDoc) 
{
  if (!aSelection || !aDoc) return NS_ERROR_NULL_POINTER;
  if (!mHTMLEditor->mTypeInState) return NS_ERROR_NULL_POINTER;
  
  PRBool weDidSometing = PR_FALSE;
  nsCOMPtr<nsIDOMNode> node, tmp;
  PRInt32 offset;
  nsresult res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(node), &offset);
  if (NS_FAILED(res)) return res;
  PropItem *item = nsnull;
  
  // process clearing any styles first
  mHTMLEditor->mTypeInState->TakeClearProperty(&item);
  while (item)
  {
    nsCOMPtr<nsIDOMNode> leftNode, rightNode, secondSplitParent, newSelParent, savedBR;
    res = mHTMLEditor->SplitStyleAbovePoint(address_of(node), &offset, item->tag, &item->attr, address_of(leftNode), address_of(rightNode));
    if (NS_FAILED(res)) return res;
    PRBool bIsEmptyNode;
    if (leftNode)
    {
      mHTMLEditor->IsEmptyNode(leftNode, &bIsEmptyNode, PR_FALSE, PR_TRUE);
      if (bIsEmptyNode)
      {
        // delete leftNode if it became empty
        res = mEditor->DeleteNode(leftNode);
        if (NS_FAILED(res)) return res;
      }
    }
    if (rightNode)
    {
      secondSplitParent = mHTMLEditor->GetLeftmostChild(rightNode);
      // don't try to split br's...
      // note: probably should only split containers, but being more conservative in changes for now.
      if (!secondSplitParent) secondSplitParent = rightNode;
      if (nsTextEditUtils::IsBreak(secondSplitParent))
      {
        savedBR = secondSplitParent;
        savedBR->GetParentNode(getter_AddRefs(tmp));
        secondSplitParent = tmp;
      }
      offset = 0;
      res = mHTMLEditor->SplitStyleAbovePoint(address_of(secondSplitParent), &offset, item->tag, &(item->attr), address_of(leftNode), address_of(rightNode));
      if (NS_FAILED(res)) return res;
      // should be impossible to not get a new leftnode here
      if (!leftNode) return NS_ERROR_FAILURE;
      newSelParent = mHTMLEditor->GetLeftmostChild(leftNode);
      if (!newSelParent) newSelParent = leftNode;
      // if rightNode starts with a br, suck it out of right node and into leftNode.
      // This is so we you don't revert back to the previous style if you happen to click at the end of a line.
      if (savedBR)
      {
        res = mEditor->MoveNode(savedBR, newSelParent, 0);
        if (NS_FAILED(res)) return res;
      }
      mHTMLEditor->IsEmptyNode(rightNode, &bIsEmptyNode, PR_FALSE, PR_TRUE);
      if (bIsEmptyNode)
      {
        // delete rightNode if it became empty
        res = mEditor->DeleteNode(rightNode);
        if (NS_FAILED(res)) return res;
      }
      // remove the style on this new heirarchy
      PRInt32 newSelOffset = 0;
      {
        // track the point at the new heirarchy.
        // This is so we can know where to put the selection after we call
        // RemoveStyleInside().  RemoveStyleInside() could remove any and all of those nodes,
        // so I have to use the range tracking system to find the right spot to put selection.
        nsAutoTrackDOMPoint tracker(mHTMLEditor->mRangeUpdater, address_of(newSelParent), &newSelOffset);
        res = mHTMLEditor->RemoveStyleInside(leftNode, item->tag, &(item->attr));
        if (NS_FAILED(res)) return res;
      }
      // reset our node offset values to the resulting new sel point
      node = newSelParent;
      offset = newSelOffset;
    }
    // we own item now (TakeClearProperty hands ownership to us)
    delete item;
    mHTMLEditor->mTypeInState->TakeClearProperty(&item);
    weDidSometing = PR_TRUE;
  }
  
  // then process setting any styles
  PRInt32 relFontSize;
  
  res = mHTMLEditor->mTypeInState->TakeRelativeFontSize(&relFontSize);
  if (NS_FAILED(res)) return res;
  res = mHTMLEditor->mTypeInState->TakeSetProperty(&item);
  if (NS_FAILED(res)) return res;
  
  if (item || relFontSize) // we have at least one style to add; make a
  {                        // new text node to insert style nodes above.
    if (mHTMLEditor->IsTextNode(node))
    {
      // if we are in a text node, split it
      res = mHTMLEditor->SplitNodeDeep(node, node, offset, &offset);
      if (NS_FAILED(res)) return res;
      node->GetParentNode(getter_AddRefs(tmp));
      node = tmp;
    }
    nsCOMPtr<nsIDOMNode> newNode;
    nsCOMPtr<nsIDOMText> nodeAsText;
    res = aDoc->CreateTextNode(nsAutoString(), getter_AddRefs(nodeAsText));
    if (NS_FAILED(res)) return res;
    if (!nodeAsText) return NS_ERROR_NULL_POINTER;
    newNode = do_QueryInterface(nodeAsText);
    res = mHTMLEditor->InsertNode(newNode, node, offset);
    if (NS_FAILED(res)) return res;
    node = newNode;
    offset = 0;
    weDidSometing = PR_TRUE;

    if (relFontSize)
    {
      PRInt32 j, dir;
      // dir indicated bigger versus smaller.  1 = bigger, -1 = smaller
      if (relFontSize > 0) dir=1;
      else dir = -1;
      for (j=0; j<abs(relFontSize); j++)
      {
        res = mHTMLEditor->RelativeFontChangeOnTextNode(dir, nodeAsText, 0, -1);
        if (NS_FAILED(res)) return res;
      }
    }
    
    while (item)
    {
      res = mHTMLEditor->SetInlinePropertyOnNode(node, item->tag, &item->attr, &item->value);
      if (NS_FAILED(res)) return res;
      // we own item now (TakeSetProperty hands ownership to us)
      delete item;
      mHTMLEditor->mTypeInState->TakeSetProperty(&item);
    }
  }
  if (weDidSometing)
    return aSelection->Collapse(node, offset);
    
  return res;
}


///////////////////////////////////////////////////////////////////////////
// IsEmptyBlock: figure out if aNode is (or is inside) an empty block.
//               A block can have children and still be considered empty,
//               if the children are empty or non-editable.
//                  
nsresult 
nsHTMLEditRules::IsEmptyBlock(nsIDOMNode *aNode, 
                              PRBool *outIsEmptyBlock, 
                              PRBool aMozBRDoesntCount,
                              PRBool aListItemsNotEmpty) 
{
  if (!aNode || !outIsEmptyBlock) return NS_ERROR_NULL_POINTER;
  *outIsEmptyBlock = PR_TRUE;
  
//  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> nodeToTest;
  if (IsBlockNode(aNode)) nodeToTest = do_QueryInterface(aNode);
//  else nsCOMPtr<nsIDOMElement> block;
//  looks like I forgot to finish this.  Wonder what I was going to do?

  if (!nodeToTest) return NS_ERROR_NULL_POINTER;
  return mHTMLEditor->IsEmptyNode(nodeToTest, outIsEmptyBlock,
                     aMozBRDoesntCount, aListItemsNotEmpty);
}


nsresult
nsHTMLEditRules::WillAlign(nsISelection *aSelection, 
                           const nsAReadableString *alignType, 
                           PRBool *aCancel,
                           PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  
  nsresult res = WillInsert(aSelection, aCancel);
  if (NS_FAILED(res)) return res;

  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;
  
  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);

  // convert the selection ranges into "promoted" selection ranges:
  // this basically just expands the range to include the immediate
  // block parent, and then further expands to include any ancestors
  // whose children are all in the range
  *aHandled = PR_TRUE;
  
  nsCOMPtr<nsISupportsArray> arrayOfRanges;
  res = GetPromotedRanges(aSelection, address_of(arrayOfRanges), kAlign);
  if (NS_FAILED(res)) return res;
  
  // use these ranges to contruct a list of nodes to act on.
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  res = GetNodesForOperation(arrayOfRanges, address_of(arrayOfNodes), kAlign);
  if (NS_FAILED(res)) return res;                                 
                                     
  // if we don't have any nodes, or we have only a single br, then we are
  // creating an empty alignment div.  We have to do some different things for these.
  PRBool emptyDiv = PR_FALSE;
  PRUint32 listCount;
  arrayOfNodes->Count(&listCount);
  if (!listCount) emptyDiv = PR_TRUE;
  if (listCount == 1)
  {
    nsCOMPtr<nsISupports> isupports = dont_AddRef(arrayOfNodes->ElementAt(0));
    nsCOMPtr<nsIDOMNode> theNode( do_QueryInterface(isupports ) );
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
      res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(parent), &offset);

      if (!nsHTMLEditUtils::IsTableElement(parent) || nsHTMLEditUtils::IsTableCellOrCaption(parent))
        emptyDiv = PR_TRUE;
    }
  }
  if (emptyDiv)
  {
    PRInt32 offset;
    nsCOMPtr<nsIDOMNode> brNode, parent, theDiv, sib;
    nsAutoString divType; divType.AssignWithConversion("div");
    res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(parent), &offset);
    if (NS_FAILED(res)) return res;
    res = SplitAsNeeded(&divType, address_of(parent), &offset);
    if (NS_FAILED(res)) return res;
    // consume a trailing br, if any.  This is to keep an alignment from
    // creating extra lines, if possible.
    res = mHTMLEditor->GetNextHTMLNode(parent, offset, address_of(brNode));
    if (NS_FAILED(res)) return res;
    if (brNode && nsTextEditUtils::IsBreak(brNode))
    {
      // making use of html structure... if next node after where
      // we are putting our div is not a block, then the br we 
      // found is in same block we are, so its safe to consume it.
      res = mHTMLEditor->GetNextHTMLSibling(parent, offset, address_of(sib));
      if (NS_FAILED(res)) return res;
      if (!IsBlockNode(sib))
      {
        res = mHTMLEditor->DeleteNode(brNode);
        if (NS_FAILED(res)) return res;
      }
    }
    res = mHTMLEditor->CreateNode(divType, parent, offset, getter_AddRefs(theDiv));
    if (NS_FAILED(res)) return res;
    // remember our new block for postprocessing
    mNewBlock = theDiv;
    // set up the alignment on the div
    nsCOMPtr<nsIDOMElement> divElem = do_QueryInterface(theDiv);
    nsAutoString attr; attr.AssignWithConversion("align");
    res = mHTMLEditor->SetAttribute(divElem, attr, *alignType);
    if (NS_FAILED(res)) return res;
    *aHandled = PR_TRUE;
    // put in a moz-br so that it won't get deleted
    res = CreateMozBR(theDiv, 0, address_of(brNode));
    if (NS_FAILED(res)) return res;
    res = aSelection->Collapse(theDiv, 0);
    selectionResetter.Abort();  // dont reset our selection in this case.
    return res;
  }

  // Next we detect all the transitions in the array, where a transition
  // means that adjacent nodes in the array don't have the same parent.
  
  nsVoidArray transitionList;
  res = MakeTransitionList(arrayOfNodes, &transitionList);
  if (NS_FAILED(res)) return res;                                 

  // Ok, now go through all the nodes and give them an align attrib or put them in a div, 
  // or whatever is appropriate.  Wohoo!
  
  PRInt32 i;
  nsCOMPtr<nsIDOMNode> curParent;
  nsCOMPtr<nsIDOMNode> curDiv;
  for (i=0; i<(PRInt32)listCount; i++)
  {
    // here's where we actually figure out what to do
    nsCOMPtr<nsISupports> isupports = dont_AddRef(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports ) );
    PRInt32 offset;
    res = nsEditor::GetNodeLocation(curNode, address_of(curParent), &offset);
    if (NS_FAILED(res)) return res;
    
    // if it's a div, don't nest it, just set the alignment
    if (nsHTMLEditUtils::IsDiv(curNode))
    {
      nsCOMPtr<nsIDOMElement> divElem = do_QueryInterface(curNode);
      nsAutoString attr; attr.AssignWithConversion("align");
      res = mHTMLEditor->SetAttribute(divElem, attr, *alignType);
      if (NS_FAILED(res)) return res;
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

    // if it's a table element (but not a table) or a list item, or a list
    // inside a list, forget any "current" div, and instead put divs inside
    // the appropriate block (td, li, etc)
    if ( (nsHTMLEditUtils::IsTableElement(curNode) && !nsHTMLEditUtils::IsTable(curNode))
         || nsHTMLEditUtils::IsListItem(curNode)
         || (nsHTMLEditUtils::IsList(curNode) && nsHTMLEditUtils::IsList(curParent)))
    {
      res = AlignInnerBlocks(curNode, alignType);
      if (NS_FAILED(res)) return res;
      // clear out curDiv so that we don't put nodes after this one into it
      curDiv = 0;
      continue;
    }      
    
    // need to make a div to put things in if we haven't already,
    // or if this node doesn't go in div we used earlier.
    if (!curDiv || transitionList[i])
    {
      nsAutoString divType; divType.AssignWithConversion("div");
      res = SplitAsNeeded(&divType, address_of(curParent), &offset);
      if (NS_FAILED(res)) return res;
      res = mHTMLEditor->CreateNode(divType, curParent, offset, getter_AddRefs(curDiv));
      if (NS_FAILED(res)) return res;
      // remember our new block for postprocessing
      mNewBlock = curDiv;
      // set up the alignment on the div
      nsCOMPtr<nsIDOMElement> divElem = do_QueryInterface(curDiv);
      nsAutoString attr; attr.AssignWithConversion("align");
      res = mHTMLEditor->SetAttribute(divElem, attr, *alignType);
      if (NS_FAILED(res)) return res;
      // curDiv is now the correct thing to put curNode in
    }
        
    // tuck the node into the end of the active div
    PRUint32 listLen;
    res = mHTMLEditor->GetLengthOfDOMNode(curDiv, listLen);
    if (NS_FAILED(res)) return res;
    res = mHTMLEditor->MoveNode(curNode, curDiv, listLen);
    if (NS_FAILED(res)) return res;
  }

  return res;
}


///////////////////////////////////////////////////////////////////////////
// AlignInnerBlocks: align inside table cells or list items
//       
nsresult
nsHTMLEditRules::AlignInnerBlocks(nsIDOMNode *aNode, const nsAReadableString *alignType)
{
  if (!aNode || !alignType) return NS_ERROR_NULL_POINTER;
  nsresult res;
  
  // gather list of table cells or list items
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  nsTableCellAndListItemFunctor functor;
  nsDOMIterator iter;
  res = iter.Init(aNode);
  if (NS_FAILED(res)) return res;
  res = iter.MakeList(functor, address_of(arrayOfNodes));
  if (NS_FAILED(res)) return res;
  
  // now that we have the list, align their contents as requested
  PRUint32 listCount;
  PRUint32 j;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsISupports> isupports;

  arrayOfNodes->Count(&listCount);
  for (j = 0; j < listCount; j++)
  {
    isupports = dont_AddRef(arrayOfNodes->ElementAt(0));
    node = do_QueryInterface(isupports);
    res = AlignBlockContents(node, alignType);
    if (NS_FAILED(res)) return res;
    arrayOfNodes->RemoveElementAt(0);
  }

  return res;  
}


///////////////////////////////////////////////////////////////////////////
// AlignBlockContents: align contents of a block element
//                  
nsresult
nsHTMLEditRules::AlignBlockContents(nsIDOMNode *aNode, const nsAReadableString *alignType)
{
  if (!aNode || !alignType) return NS_ERROR_NULL_POINTER;
  nsresult res;
  nsCOMPtr <nsIDOMNode> firstChild, lastChild, divNode;
  
  res = mHTMLEditor->GetFirstEditableChild(aNode, address_of(firstChild));
  if (NS_FAILED(res)) return res;
  res = mHTMLEditor->GetLastEditableChild(aNode, address_of(lastChild));
  if (NS_FAILED(res)) return res;
  if (!firstChild)
  {
    // this cell has no content, nothing to align
  }
  else if ((firstChild==lastChild) && nsHTMLEditUtils::IsDiv(firstChild))
  {
    // the cell already has a div containing all of it's content: just
    // act on this div.
    nsCOMPtr<nsIDOMElement> divElem = do_QueryInterface(firstChild);
    nsAutoString attr; attr.AssignWithConversion("align");
    res = mHTMLEditor->SetAttribute(divElem, attr, *alignType);
    if (NS_FAILED(res)) return res;
  }
  else
  {
    // else we need to put in a div, set the alignment, and toss in all the children
    nsAutoString divType; divType.AssignWithConversion("div");
    res = mHTMLEditor->CreateNode(divType, aNode, 0, getter_AddRefs(divNode));
    if (NS_FAILED(res)) return res;
    // set up the alignment on the div
    nsCOMPtr<nsIDOMElement> divElem = do_QueryInterface(divNode);
    nsAutoString attr; attr.AssignWithConversion("align");
    res = mHTMLEditor->SetAttribute(divElem, attr, *alignType);
    if (NS_FAILED(res)) return res;
    // tuck the children into the end of the active div
    while (lastChild && (lastChild != divNode))
    {
      res = mHTMLEditor->MoveNode(lastChild, divNode, 0);
      if (NS_FAILED(res)) return res;
      res = mHTMLEditor->GetLastEditableChild(aNode, address_of(lastChild));
      if (NS_FAILED(res)) return res;
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
                                    PRBool *aHandled)
{
  // if we are inside an empty block, delete it.
  // Note: do NOT delete table elements this way.
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> block;
  if (IsBlockNode(aStartNode)) 
    block = aStartNode;
  else
    block = mHTMLEditor->GetBlockNodeParent(aStartNode);
  PRBool bIsEmptyNode;
  if (block != aBodyNode)
  {
    res = mHTMLEditor->IsEmptyNode(block, &bIsEmptyNode, PR_TRUE, PR_FALSE);
    if (NS_FAILED(res)) return res;
    if (bIsEmptyNode && !nsHTMLEditUtils::IsTableElement(aStartNode))
    {
      // adjust selection to be right after it
      nsCOMPtr<nsIDOMNode> blockParent;
      PRInt32 offset;
      res = nsEditor::GetNodeLocation(block, address_of(blockParent), &offset);
      if (NS_FAILED(res)) return res;
      if (!blockParent || offset < 0) return NS_ERROR_FAILURE;
      res = aSelection->Collapse(blockParent, offset+1);
      if (NS_FAILED(res)) return res;
      res = mHTMLEditor->DeleteNode(block);
      *aHandled = PR_TRUE;
    }
  }
  return res;
}

///////////////////////////////////////////////////////////////////////////
// CheckForWhitespaceDeletion: Called by WillDeleteSelection to detect and handle
//                     case of deleting whitespace.  If we need to skip over
//                     whitespace, visNode and visOffset are updated to new
//                     location to handle deletion.
//                  
nsresult
nsHTMLEditRules::CheckForWhitespaceDeletion(nsCOMPtr<nsIDOMNode> *ioStartNode,
                                            PRInt32 *ioStartOffset,
                                            PRInt32 aAction,
                                            PRBool *aHandled)
{
  nsresult res = NS_OK;
  // gather up ws data here.  We may be next to non-significant ws.
  nsWSRunObject wsObj(mHTMLEditor, *ioStartNode, *ioStartOffset);
  nsCOMPtr<nsIDOMNode> visNode;
  PRInt32 visOffset;
  PRInt16 wsType;
  if (aAction == nsIEditor::ePrevious)
  {
    res = wsObj.PriorVisibleNode(*ioStartNode, *ioStartOffset, address_of(visNode), &visOffset, &wsType);
    // note that visOffset is _after_ what we are about to delete.
  }
  else if (aAction == nsIEditor::eNext)
  {
    res = wsObj.NextVisibleNode(*ioStartNode, *ioStartOffset, address_of(visNode), &visOffset, &wsType);
    // note that visOffset is _before_ what we are about to delete.
  }
  if (NS_SUCCEEDED(res))
  {
    if (wsType==nsWSRunObject::eNormalWS)
    {
      // we found some visible ws to delete.  Let ws code handle it.
      if (aAction == nsIEditor::ePrevious)
        res = wsObj.DeleteWSBackward();
      else if (aAction == nsIEditor::eNext)
        res = wsObj.DeleteWSForward();
      *aHandled = PR_TRUE;
      return res;
    } 
    else if (visNode)
    {
      // reposition startNode and startOffset so that we skip over any non-significant ws
      *ioStartNode = visNode;
      *ioStartOffset = visOffset;
    }
  }
  return res;
}

///////////////////////////////////////////////////////////////////////////
// GetInnerContent: aList and aTbl allow the caller to specify what kind 
//                  of content to "look inside".  If aTbl is true, look inside
//                  any table content, and append the inner content to the
//                  supplied issupportsarray.  Similarly with aList and list content.
//                  
nsresult
nsHTMLEditRules::GetInnerContent(nsIDOMNode *aNode, nsISupportsArray *outArrayOfNodes, PRBool aList, PRBool aTbl)
{
  if (!aNode || !outArrayOfNodes) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsISupports> isupports;
  
  nsresult res = mHTMLEditor->GetFirstEditableChild(aNode, address_of(node));
  while (NS_SUCCEEDED(res) && node)
  {
    if (  ( aList && (nsHTMLEditUtils::IsList(node)     || 
                      nsHTMLEditUtils::IsListItem(node) ) )
       || ( aTbl && nsHTMLEditUtils::IsTableElement(node) )  )
    {
      res = GetInnerContent(node, outArrayOfNodes, aList, aTbl);
      if (NS_FAILED(res)) return res;
    }
    else
    {
      isupports = do_QueryInterface(node);
      outArrayOfNodes->AppendElement(isupports);
    }
    nsCOMPtr<nsIDOMNode> tmp;
    res = node->GetNextSibling(getter_AddRefs(tmp));
    node = tmp;
  }

  return res;
}

///////////////////////////////////////////////////////////////////////////
// IsFirstNode: Are we the first edittable node in our parent?
//                  
PRBool
nsHTMLEditRules::IsFirstNode(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset, j=0;
  nsresult res = nsEditor::GetNodeLocation(aNode, address_of(parent), &offset);
  if (NS_FAILED(res)) 
  {
    NS_NOTREACHED("failure in nsHTMLEditRules::IsFirstNode");
    return PR_FALSE;
  }
  if (!offset)  // easy case, we are first dom child
    return PR_TRUE;
  if (!parent)  
    return PR_TRUE;
  
  // ok, so there are earlier children.  But are they editable???
  nsCOMPtr<nsIDOMNodeList> childList;
  nsCOMPtr<nsIDOMNode> child;

  res = parent->GetChildNodes(getter_AddRefs(childList));
  if (NS_FAILED(res) || !childList) 
  {
    NS_NOTREACHED("failure in nsHTMLEditUtils::IsFirstNode");
    return PR_TRUE;
  }
  while (j < offset)
  {
    childList->Item(j, getter_AddRefs(child));
    if (mHTMLEditor->IsEditable(child)) 
      return PR_FALSE;
    j++;
  }
  return PR_TRUE;
}


///////////////////////////////////////////////////////////////////////////
// IsLastNode: Are we the first edittable node in our parent?
//                  
PRBool
nsHTMLEditRules::IsLastNode(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset, j;
  PRUint32 numChildren;
  nsresult res = nsEditor::GetNodeLocation(aNode, address_of(parent), &offset);
  if (NS_FAILED(res)) 
  {
    NS_NOTREACHED("failure in nsHTMLEditUtils::IsLastNode");
    return PR_FALSE;
  }
  nsEditor::GetLengthOfDOMNode(parent, numChildren); 
  if (offset+1 == (PRInt32)numChildren) // easy case, we are last dom child
    return PR_TRUE;
  if (!parent)
    return PR_TRUE;
    
  // ok, so there are later children.  But are they editable???
  j = offset+1;
  nsCOMPtr<nsIDOMNodeList>childList;
  nsCOMPtr<nsIDOMNode> child;
  res = parent->GetChildNodes(getter_AddRefs(childList));
  if (NS_FAILED(res) || !childList) 
  {
    NS_NOTREACHED("failure in nsHTMLEditRules::IsLastNode");
    return PR_TRUE;
  }
  while (j < (PRInt32)numChildren)
  {
    childList->Item(j, getter_AddRefs(child));
    if (mHTMLEditor->IsEditable(child)) 
      return PR_FALSE;
    j++;
  }
  return PR_TRUE;
}


///////////////////////////////////////////////////////////////////////////
// AtStartOfBlock: is node/offset at the start of the editable material in this block?
//                  
PRBool
nsHTMLEditRules::AtStartOfBlock(nsIDOMNode *aNode, PRInt32 aOffset, nsIDOMNode *aBlock)
{
  nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(aNode);
  if (nodeAsText && aOffset) return PR_FALSE;  // there are chars in front of us
  
  nsCOMPtr<nsIDOMNode> priorNode;
  nsresult  res = mHTMLEditor->GetPriorHTMLNode(aNode, aOffset, address_of(priorNode));
  if (NS_FAILED(res)) return PR_TRUE;
  if (!priorNode) return PR_TRUE;
  nsCOMPtr<nsIDOMNode> blockParent = mHTMLEditor->GetBlockNodeParent(priorNode);
  if (blockParent && (blockParent.get() == aBlock)) return PR_FALSE;
  return PR_TRUE;
}


///////////////////////////////////////////////////////////////////////////
// AtEndOfBlock: is node/offset at the end of the editable material in this block?
//                  
PRBool
nsHTMLEditRules::AtEndOfBlock(nsIDOMNode *aNode, PRInt32 aOffset, nsIDOMNode *aBlock)
{
  nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(aNode);
  if (nodeAsText)   
  {
    PRUint32 strLength;
    nodeAsText->GetLength(&strLength);
    if ((PRInt32)strLength > aOffset) return PR_FALSE;  // there are chars in after us
  }
  nsCOMPtr<nsIDOMNode> nextNode;
  nsresult  res = mHTMLEditor->GetNextHTMLNode(aNode, aOffset, address_of(nextNode));
  if (NS_FAILED(res)) return PR_TRUE;
  if (!nextNode) return PR_TRUE;
  nsCOMPtr<nsIDOMNode> blockParent = mHTMLEditor->GetBlockNodeParent(nextNode);
  if (blockParent && (blockParent.get() == aBlock)) return PR_FALSE;
  return PR_TRUE;
}


// not needed at moment - leaving around in case we go back to it.
#if 0
///////////////////////////////////////////////////////////////////////////
// CreateMozDiv: makes a div with type = _moz
//                       
nsresult
nsHTMLEditRules::CreateMozDiv(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outDiv)
{
  if (!inParent || !outDiv) return NS_ERROR_NULL_POINTER;
  nsAutoString divType= "div";
  *outDiv = nsnull;
  nsresult res = mHTMLEditor->CreateNode(divType, inParent, inOffset, getter_AddRefs(*outDiv));
  if (NS_FAILED(res)) return res;
  // give it special moz attr
  nsCOMPtr<nsIDOMElement> mozDivElem = do_QueryInterface(*outDiv);
  res = mHTMLEditor->SetAttribute(mozDivElem, "type", "_moz");
  if (NS_FAILED(res)) return res;
  res = AddTrailerBR(*outDiv);
  return res;
}
#endif    


///////////////////////////////////////////////////////////////////////////
// GetPromotedPoint: figure out where a start or end point for a block
//                   operation really is
nsresult
nsHTMLEditRules::GetPromotedPoint(RulesEndpoint aWhere, nsIDOMNode *aNode, PRInt32 aOffset, 
                                  PRInt32 actionID, nsCOMPtr<nsIDOMNode> *outNode, PRInt32 *outOffset)
{
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> node = aNode;
  nsCOMPtr<nsIDOMNode> parent = aNode;
  PRInt32 offset = aOffset;
  
  // defualt values
  *outNode = node;
  *outOffset = offset;

  // we do one thing for InsertText actions, something else entirely for other actions
  if (actionID == kInsertText)
  {
    PRBool isSpace, isNBSP; 
    nsCOMPtr<nsIDOMNode> temp;   
    // for insert text or delete actions, we want to look backwards (or forwards, as appropriate)
    // for additional whitespace or nbsp's.  We may have to act on these later even though
    // they are outside of the initial selection.  Even if they are in another node!
    if (aWhere == kStart)
    {
      do
      {
        res = mHTMLEditor->IsPrevCharWhitespace(node, offset, &isSpace, &isNBSP, address_of(temp), &offset);
        if (NS_FAILED(res)) return res;
        if (isSpace || isNBSP) node = temp;
        else break;
      } while (node);
  
      *outNode = node;
      *outOffset = offset;
    }
    else if (aWhere == kEnd)
    {
      do
      {
        res = mHTMLEditor->IsNextCharWhitespace(node, offset, &isSpace, &isNBSP, address_of(temp), &offset);
        if (NS_FAILED(res)) return res;
        if (isSpace || isNBSP) node = temp;
        else break;
      } while (node);
  
      *outNode = node;
      *outOffset = offset;
    }
    return res;
  }
  
  // else not kInsertText.  In this case we want to see if we should
  // grab any adjacent inline nodes and/or parents and other ancestors
  if (aWhere == kStart)
  {
    // some special casing for text nodes
    if (nsEditor::IsTextNode(aNode))  
    {
      res = nsEditor::GetNodeLocation(aNode, address_of(parent), &offset);
      if (NS_FAILED(res)) return res;
    }
    else
    {
      node = nsEditor::GetChildAt(parent,offset);
    }
    if (!node) node = parent;
    
    
    // if this is an inline node,
    // back up through any prior inline nodes that
    // aren't across a <br> from us.
    
    if (!IsBlockNode(node))
    {
      nsCOMPtr<nsIDOMNode> block = nsHTMLEditor::GetBlockNodeParent(node);
      nsCOMPtr<nsIDOMNode> prevNode, prevNodeBlock;
      res = mHTMLEditor->GetPriorHTMLNode(node, address_of(prevNode));
      
      while (prevNode && NS_SUCCEEDED(res))
      {
        prevNodeBlock = nsHTMLEditor::GetBlockNodeParent(prevNode);
        if (prevNodeBlock != block)
          break;
        if (nsTextEditUtils::IsBreak(prevNode))
          break;
        if (IsBlockNode(prevNode))
          break;
        node = prevNode;
        res = mHTMLEditor->GetPriorHTMLNode(node, address_of(prevNode));
      }
    }
    
    // finding the real start for this point.  look up the tree for as long as we are the 
    // first node in the container, and as long as we haven't hit the body node.
    if (!nsTextEditUtils::IsBody(node))
    {
      res = nsEditor::GetNodeLocation(node, address_of(parent), &offset);
      if (NS_FAILED(res)) return res;
      while ((IsFirstNode(node)) && (!nsTextEditUtils::IsBody(parent)))
      {
        // some cutoffs are here: we don't need to also include them in the aWhere == kEnd case.
        // as long as they are in one or the other it will work.
        
        // dont cross table cell boundaries
        if (nsHTMLEditUtils::IsTableCell(parent)) break;
        // special case for outdent: don't keep looking up 
        // if we have found a blockquote element to act on
        if ((actionID == kOutdent) && nsHTMLEditUtils::IsBlockquote(parent))
          break;
        node = parent;
        res = nsEditor::GetNodeLocation(node, address_of(parent), &offset);
        if (NS_FAILED(res)) return res;
      } 
      *outNode = parent;
      *outOffset = offset;
      return res;
    }
  }
  
  if (aWhere == kEnd)
  {
    // some special casing for text nodes
    if (nsEditor::IsTextNode(aNode))  
    {
      res = nsEditor::GetNodeLocation(aNode, address_of(parent), &offset);
      if (NS_FAILED(res)) return res;
    }
    else
    {
      if (offset) offset--; // we want node _before_ offset
      else
      {  // XXX HACK MOOSE
        // The logic in this routine, while it works almost all the time, is fundamentally flawed.
        // I need to rewrite it using something like nsWSRunObject::GetPreviousWSNode().  Til then 
        // I need this hack to correct the most commmon error here.
        
        // we couldn't get a prior node in this parent.  If next node is a br, just skip
        // the promotion altogether.  We have a br after us.  If we fell through to code below,
        // it would think br was before us and would proceed merrily along.
        node = nsEditor::GetChildAt(parent,offset);
        if (node && nsTextEditUtils::IsBreak(node))
          return NS_OK;  // default values used.
      }
      node = nsEditor::GetChildAt(parent,offset);
    }
    if (!node) node = parent;
    
    // if this is an inline node, 
    // look ahead through any further inline nodes that
    // aren't across a <br> from us, and that are enclosed in the same block.
    
    if (!IsBlockNode(node))
    {
      nsCOMPtr<nsIDOMNode> block = nsHTMLEditor::GetBlockNodeParent(node);
      nsCOMPtr<nsIDOMNode> nextNode, nextNodeBlock;
      res = mHTMLEditor->GetNextHTMLNode(node, address_of(nextNode));
      
      while (nextNode && NS_SUCCEEDED(res))
      {
        nextNodeBlock = nsHTMLEditor::GetBlockNodeParent(nextNode);
        if (nextNodeBlock != block)
          break;
        if (nsTextEditUtils::IsBreak(nextNode))
        {
          node = nextNode;
          break;
        }
        if (IsBlockNode(nextNode))
          break;
        node = nextNode;
        res = mHTMLEditor->GetNextHTMLNode(node, address_of(nextNode));
      }
    }
    
    // finding the real end for this point.  look up the tree for as long as we are the 
    // last node in the container, and as long as we haven't hit the body node.
    if (!nsTextEditUtils::IsBody(node))
    {
      res = nsEditor::GetNodeLocation(node, address_of(parent), &offset);
      if (NS_FAILED(res)) return res;
      while ((IsLastNode(node)) && (!nsTextEditUtils::IsBody(parent)))
      {
        node = parent;
        res = nsEditor::GetNodeLocation(node, address_of(parent), &offset);
        if (NS_FAILED(res)) return res;
      } 
      *outNode = parent;
      offset++;  // add one since this in an endpoint - want to be AFTER node.
      *outOffset = offset;
      return res;
    }
  }
  
  return res;
}


///////////////////////////////////////////////////////////////////////////
// GetPromotedRanges: run all the selection range endpoint through 
//                    GetPromotedPoint()
//                       
nsresult 
nsHTMLEditRules::GetPromotedRanges(nsISelection *inSelection, 
                                   nsCOMPtr<nsISupportsArray> *outArrayOfRanges, 
                                   PRInt32 inOperationType)
{
  if (!inSelection || !outArrayOfRanges) return NS_ERROR_NULL_POINTER;

  nsresult res = NS_NewISupportsArray(getter_AddRefs(*outArrayOfRanges));
  if (NS_FAILED(res)) return res;
  
  PRInt32 rangeCount;
  res = inSelection->GetRangeCount(&rangeCount);
  if (NS_FAILED(res)) return res;
  
  PRInt32 i;
  nsCOMPtr<nsIDOMRange> selectionRange;
  nsCOMPtr<nsIDOMRange> opRange;

  for (i = 0; i < rangeCount; i++)
  {
    res = inSelection->GetRangeAt(i, getter_AddRefs(selectionRange));
    if (NS_FAILED(res)) return res;

    // clone range so we dont muck with actual selection ranges
    res = selectionRange->CloneRange(getter_AddRefs(opRange));
    if (NS_FAILED(res)) return res;

    // make a new adjusted range to represent the appropriate block content.
    // The basic idea is to push out the range endpoints
    // to truly enclose the blocks that we will affect.
    // This call alters opRange.
    res = PromoteRange(opRange, inOperationType);
    if (NS_FAILED(res)) return res;
      
    // stuff new opRange into nsISupportsArray
    nsCOMPtr<nsISupports> isupports = do_QueryInterface(opRange);
    (*outArrayOfRanges)->AppendElement(isupports);
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// PromoteRange: expand a range to include any parents for which all
//               editable children are already in range. 
//                       
nsresult 
nsHTMLEditRules::PromoteRange(nsIDOMRange *inRange, 
                              PRInt32 inOperationType)
{
  if (!inRange) return NS_ERROR_NULL_POINTER;
  nsresult res;
  nsCOMPtr<nsIDOMNode> startNode, endNode;
  PRInt32 startOffset, endOffset;
  
  res = inRange->GetStartContainer(getter_AddRefs(startNode));
  if (NS_FAILED(res)) return res;
  res = inRange->GetStartOffset(&startOffset);
  if (NS_FAILED(res)) return res;
  res = inRange->GetEndContainer(getter_AddRefs(endNode));
  if (NS_FAILED(res)) return res;
  res = inRange->GetEndOffset(&endOffset);
  if (NS_FAILED(res)) return res;
  
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
      PRBool bIsEmptyNode = PR_FALSE;
      // check for body
      nsCOMPtr<nsIDOMElement> bodyElement;
      nsCOMPtr<nsIDOMNode> bodyNode;
      res = mHTMLEditor->GetRootElement(getter_AddRefs(bodyElement));
      if (NS_FAILED(res)) return res;
      if (!bodyElement) return NS_ERROR_UNEXPECTED;
      bodyNode = do_QueryInterface(bodyElement);
      if (block != bodyNode)
      {
        // ok, not body, check if empty
        res = mHTMLEditor->IsEmptyNode(block, &bIsEmptyNode, PR_TRUE, PR_FALSE);
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
  
  res = GetPromotedPoint( kStart, startNode, startOffset, inOperationType, address_of(opStartNode), &opStartOffset);
  if (NS_FAILED(res)) return res;
  res = GetPromotedPoint( kEnd, endNode, endOffset, inOperationType, address_of(opEndNode), &opEndOffset);
  if (NS_FAILED(res)) return res;
  res = inRange->SetStart(opStartNode, opStartOffset);
  if (NS_FAILED(res)) return res;
  res = inRange->SetEnd(opEndNode, opEndOffset);
  return res;
} 

///////////////////////////////////////////////////////////////////////////
// GetNodesForOperation: run through the ranges in the array and construct 
//                       a new array of nodes to be acted on.
//                       
nsresult 
nsHTMLEditRules::GetNodesForOperation(nsISupportsArray *inArrayOfRanges, 
                                   nsCOMPtr<nsISupportsArray> *outArrayOfNodes, 
                                   PRInt32 inOperationType,
                                   PRBool aDontTouchContent)
{
  if (!inArrayOfRanges || !outArrayOfNodes) return NS_ERROR_NULL_POINTER;

  // make a array
  nsresult res = NS_NewISupportsArray(getter_AddRefs(*outArrayOfNodes));
  if (NS_FAILED(res)) return res;
  
  PRUint32 rangeCount;
  res = inArrayOfRanges->Count(&rangeCount);
  if (NS_FAILED(res)) return res;
  
  PRInt32 i;
  nsCOMPtr<nsIDOMRange> opRange;
  nsCOMPtr<nsISupports> isupports;

  // bust up any inlines that cross our range endpoints,
  // but only if we are allowed to touch content.
  
  if (!aDontTouchContent)
  {
    nsVoidArray rangeItemArray;
    // first register ranges for special editor gravity
    for (i = 0; i < (PRInt32)rangeCount; i++)
    {
      isupports = dont_AddRef(inArrayOfRanges->ElementAt(0));
      opRange = do_QueryInterface(isupports);
      nsRangeStore *item = new nsRangeStore();
      if (!item) return NS_ERROR_NULL_POINTER;
      item->StoreRange(opRange);
      mHTMLEditor->mRangeUpdater.RegisterRangeItem(item);
      rangeItemArray.AppendElement((void*)item);
      inArrayOfRanges->RemoveElementAt(0);
    }    
    // now bust up inlines
    for (i = (PRInt32)rangeCount-1; i >= 0; i--)
    {
      nsRangeStore *item = (nsRangeStore*)rangeItemArray.ElementAt(i);
      res = BustUpInlinesAtRangeEndpoints(*item);
      if (NS_FAILED(res)) return res;    
    } 
    // then unregister the ranges
    for (i = 0; i < (PRInt32)rangeCount; i++)
    {
      nsRangeStore *item = (nsRangeStore*)rangeItemArray.ElementAt(0);
      if (!item) return NS_ERROR_NULL_POINTER;
      rangeItemArray.RemoveElementAt(0);
      mHTMLEditor->mRangeUpdater.DropRangeItem(item);
      res = item->GetRange(address_of(opRange));
      if (NS_FAILED(res)) return res;
      delete item;
      isupports = do_QueryInterface(opRange);
      inArrayOfRanges->AppendElement(isupports);
    }    
  }
  // gather up a list of all the nodes
  for (i = 0; i < (PRInt32)rangeCount; i++)
  {
    isupports = dont_AddRef(inArrayOfRanges->ElementAt(i));
    opRange = do_QueryInterface(isupports);
    
    nsTrivialFunctor functor;
    nsDOMSubtreeIterator iter;
    res = iter.Init(opRange);
    if (NS_FAILED(res)) return res;
    res = iter.AppendList(functor, *outArrayOfNodes);
    if (NS_FAILED(res)) return res;    
  }    

  // certain operations should not act on li's and td's, but rather inside 
  // them.  alter the list as needed
  if ( (inOperationType == kMakeBasicBlock)  ||
       (inOperationType == kAlign) )
  {
    PRUint32 listCount;
    (*outArrayOfNodes)->Count(&listCount);
    for (i=(PRInt32)listCount-1; i>=0; i--)
    {
      isupports = dont_AddRef((*outArrayOfNodes)->ElementAt(i));
      nsCOMPtr<nsIDOMNode> node( do_QueryInterface(isupports) );
      if ( (nsHTMLEditUtils::IsTableElement(node) && !nsHTMLEditUtils::IsTable(node))
          || (nsHTMLEditUtils::IsListItem(node)))
      {
        (*outArrayOfNodes)->RemoveElementAt(i);
        res = GetInnerContent(node, *outArrayOfNodes);
        if (NS_FAILED(res)) return res;
      }
    }
  }
  // indent/outdent already do something special for list items, but
  // we still need to make sure we dont act on table elements
  else if ( (inOperationType == kOutdent)  ||
            (inOperationType == kIndent) )
  {
    PRUint32 listCount;
    (*outArrayOfNodes)->Count(&listCount);
    for (i=(PRInt32)listCount-1; i>=0; i--)
    {
      isupports = dont_AddRef((*outArrayOfNodes)->ElementAt(i));
      nsCOMPtr<nsIDOMNode> node( do_QueryInterface(isupports) );
      if ( (nsHTMLEditUtils::IsTableElement(node) && !nsHTMLEditUtils::IsTable(node)) )
      {
        (*outArrayOfNodes)->RemoveElementAt(i);
        res = GetInnerContent(node, *outArrayOfNodes);
        if (NS_FAILED(res)) return res;
      }
    }
  }
  // outdent should look inside of divs.
  if (inOperationType == kOutdent) 
  {
    PRUint32 listCount;
    (*outArrayOfNodes)->Count(&listCount);
    for (i=(PRInt32)listCount-1; i>=0; i--)
    {
      isupports = dont_AddRef((*outArrayOfNodes)->ElementAt(i));
      nsCOMPtr<nsIDOMNode> node( do_QueryInterface(isupports) );
      if (nsHTMLEditUtils::IsDiv(node))
      {
        (*outArrayOfNodes)->RemoveElementAt(i);
        res = GetInnerContent(node, *outArrayOfNodes, PR_FALSE, PR_FALSE);
        if (NS_FAILED(res)) return res;
      }
    }
  }


  // post process the list to break up inline containers that contain br's.
  // but only for operations that might care, like making lists or para's...
  if ( (inOperationType == kMakeBasicBlock)  ||
       (inOperationType == kMakeList)        ||
       (inOperationType == kAlign)           ||
       (inOperationType == kIndent)          ||
       (inOperationType == kOutdent) )
  {
    PRUint32 listCount;
    (*outArrayOfNodes)->Count(&listCount);
    for (i=(PRInt32)listCount-1; i>=0; i--)
    {
      isupports = dont_AddRef((*outArrayOfNodes)->ElementAt(i));
      nsCOMPtr<nsIDOMNode> node( do_QueryInterface(isupports) );
      if (!aDontTouchContent && IsInlineNode(node) 
           && mHTMLEditor->IsContainer(node) && !mHTMLEditor->IsTextNode(node))
      {
        nsCOMPtr<nsISupportsArray> arrayOfInlines;
        res = BustUpInlinesAtBRs(node, address_of(arrayOfInlines));
        if (NS_FAILED(res)) return res;
        // put these nodes in outArrayOfNodes, replacing the current node
        (*outArrayOfNodes)->RemoveElementAt((PRUint32)i);
        // i sure wish nsISupportsArray had an IsEmpty() and DeleteLastElement()
        // calls.  For that matter, if I could just insert one nsISupportsArray
        // into another at a given position, that would do everything I need here.
        PRUint32 iCount;
        arrayOfInlines->Count(&iCount);
        while (iCount)
        {
          iCount--;
          isupports = dont_AddRef(arrayOfInlines->ElementAt(iCount));
          (*outArrayOfNodes)->InsertElementAt(isupports, i);
        }
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
                                   nsCOMPtr<nsISupportsArray> *outArrayOfNodes)
{
  if (!inNode || !outArrayOfNodes) return NS_ERROR_NULL_POINTER;

  nsresult res = NS_NewISupportsArray(getter_AddRefs(*outArrayOfNodes));
  if (NS_FAILED(res)) return res;
  
  nsCOMPtr<nsIDOMNodeList> childNodes;
  res = inNode->GetChildNodes(getter_AddRefs(childNodes));
  if (NS_FAILED(res)) return res;
  if (!childNodes) return NS_ERROR_NULL_POINTER;
  PRUint32 childCount;
  res = childNodes->GetLength(&childCount);
  if (NS_FAILED(res)) return res;
  
  PRUint32 i;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsISupports> isupports;
  for (i = 0; i < childCount; i++)
  {
    res = childNodes->Item( i, getter_AddRefs(node));
    if (!node) return NS_ERROR_FAILURE;
    isupports = do_QueryInterface(node);
    (*outArrayOfNodes)->AppendElement(isupports);
    if (NS_FAILED(res)) return res;
  }
  return res;
}



///////////////////////////////////////////////////////////////////////////
// GetListActionNodes: 
//                       
nsresult 
nsHTMLEditRules::GetListActionNodes(nsCOMPtr<nsISupportsArray> *outArrayOfNodes, 
                                    PRBool aEntireList,
                                    PRBool aDontTouchContent)
{
  if (!outArrayOfNodes) return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;
  
  nsCOMPtr<nsISelection>selection;
  res = mHTMLEditor->GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));
  if (!selPriv)
    return NS_ERROR_FAILURE;
  // added this in so that ui code can ask to change an entire list, even if selection
  // is only in part of it.  used by list item dialog.
  if (aEntireList)
  {       
    res = NS_NewISupportsArray(getter_AddRefs(*outArrayOfNodes));
    if (NS_FAILED(res)) return res;
    nsCOMPtr<nsIEnumerator> enumerator;
    res = selPriv->GetEnumerator(getter_AddRefs(enumerator));
    if (NS_FAILED(res)) return res;
    if (!enumerator) return NS_ERROR_UNEXPECTED;

    for (enumerator->First(); NS_OK!=enumerator->IsDone(); enumerator->Next())
    {
      nsCOMPtr<nsISupports> currentItem;
      res = enumerator->CurrentItem(getter_AddRefs(currentItem));
      if (NS_FAILED(res)) return res;
      if (!currentItem) return NS_ERROR_UNEXPECTED;

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
            nsCOMPtr<nsISupports> isupports = do_QueryInterface(parent);
            (*outArrayOfNodes)->AppendElement(isupports);
            break;
          }
          parent->GetParentNode(getter_AddRefs(tmp));
          parent = tmp;
        }
      }
    }
    // if we didn't find any nodes this way, then try the normal way.  perhaps the
    // selection spans multiple lists but with no common list parent.
    if (*outArrayOfNodes)
    {
      PRUint32 nodeCount;
      (*outArrayOfNodes)->Count(&nodeCount);
      if (nodeCount) return NS_OK;
    }
  }
  
  nsCOMPtr<nsISupportsArray> arrayOfRanges;
  res = GetPromotedRanges(selection, address_of(arrayOfRanges), kMakeList);
  if (NS_FAILED(res)) return res;
  
  // use these ranges to contruct a list of nodes to act on.
  res = GetNodesForOperation(arrayOfRanges, outArrayOfNodes, kMakeList, aDontTouchContent);
  if (NS_FAILED(res)) return res;                                 
               
  // pre process our list of nodes...                      
  PRUint32 listCount;
  PRInt32 i;
  (*outArrayOfNodes)->Count(&listCount);
  for (i=(PRInt32)listCount-1; i>=0; i--)
  {
    nsCOMPtr<nsISupports> isupports = dont_AddRef((*outArrayOfNodes)->ElementAt(i));
    nsCOMPtr<nsIDOMNode> testNode( do_QueryInterface(isupports ) );

    // Remove all non-editable nodes.  Leave them be.
    if (!mHTMLEditor->IsEditable(testNode))
    {
      (*outArrayOfNodes)->RemoveElementAt(i);
    }
    
    // scan for table elements and divs.  If we find table elements other than table,
    // replace it with a list of any editable non-table content.
    if ( (nsHTMLEditUtils::IsTableElement(testNode) && !nsHTMLEditUtils::IsTable(testNode))
         || nsHTMLEditUtils::IsDiv(testNode) )
    {
      (*outArrayOfNodes)->RemoveElementAt(i);
      res = GetInnerContent(testNode, *outArrayOfNodes, PR_FALSE);
      if (NS_FAILED(res)) return res;
    }
  }

  // if there is only one node in the array, and it is a list, div, or blockquote,
  // then look inside of it until we find inner list or content.
  res = LookInsideDivBQandList(*outArrayOfNodes);
  return res;
}


///////////////////////////////////////////////////////////////////////////
// LookInsideDivBQandList: 
//                       
nsresult 
nsHTMLEditRules::LookInsideDivBQandList(nsISupportsArray *aNodeArray)
{
  // if there is only one node in the array, and it is a list, div, or blockquote,
  // then look inside of it until we find inner list or content.
  nsresult res = NS_OK;
  PRUint32 listCount;
  aNodeArray->Count(&listCount);
  if (listCount == 1)
  {
    nsCOMPtr<nsISupports> isupports = dont_AddRef(aNodeArray->ElementAt(0));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports) );
    
    while (nsHTMLEditUtils::IsDiv(curNode)
           || nsHTMLEditUtils::IsList(curNode)
           || nsHTMLEditUtils::IsBlockquote(curNode))
    {
      // dive as long as there is only one child, and it is a list, div, blockquote
      PRUint32 numChildren;
      res = mHTMLEditor->CountEditableChildren(curNode, numChildren);
      if (NS_FAILED(res)) return res;
      
      if (numChildren == 1)
      {
        // keep diving
        nsCOMPtr <nsIDOMNode> tmpNode = nsEditor::GetChildAt(curNode, 0);
        if (nsHTMLEditUtils::IsDiv(tmpNode)
            || nsHTMLEditUtils::IsList(tmpNode)
            || nsHTMLEditUtils::IsBlockquote(tmpNode))
        {
          // check editablility XXX floppy moose
          curNode = tmpNode;
        }
        else break;
      }
      else break;
    }
    // we've found innermost list/blockquote/div: 
    // replace the one node in the array with these nodes
    aNodeArray->RemoveElementAt(0);
    if ((nsHTMLEditUtils::IsDiv(curNode) || nsHTMLEditUtils::IsBlockquote(curNode)))
    {
      res = GetInnerContent(curNode, aNodeArray, PR_FALSE, PR_FALSE);
    }
    else
    {
      nsCOMPtr<nsISupports> isuports (do_QueryInterface(curNode));
      res = aNodeArray->AppendElement(isuports);
    }
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// GetDefinitionListItemTypes: 
//                       
nsresult 
nsHTMLEditRules::GetDefinitionListItemTypes(nsIDOMNode *aNode, PRBool &aDT, PRBool &aDD)
{
  if (!aNode) return NS_ERROR_NULL_POINTER;
  aDT = aDD = PR_FALSE;
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> child, temp;
  res = aNode->GetFirstChild(getter_AddRefs(child));
  while (child && NS_SUCCEEDED(res))
  {
    if (mHTMLEditor->NodeIsType(child,nsIEditProperty::dt)) aDT = PR_TRUE;
    else if (mHTMLEditor->NodeIsType(child,nsIEditProperty::dd)) aDD = PR_TRUE;
    res = child->GetNextSibling(getter_AddRefs(temp));
    child = temp;
  }
  return res;
}

///////////////////////////////////////////////////////////////////////////
// GetParagraphFormatNodes: 
//                       
nsresult 
nsHTMLEditRules::GetParagraphFormatNodes(nsCOMPtr<nsISupportsArray> *outArrayOfNodes,
                                         PRBool aDontTouchContent)
{
  if (!outArrayOfNodes) return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsISelection>selection;
  nsresult res = mHTMLEditor->GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsISupportsArray> arrayOfRanges;
  res = GetPromotedRanges(selection, address_of(arrayOfRanges), kMakeList);
  if (NS_FAILED(res)) return res;
  
  // use these ranges to contruct a list of nodes to act on.
  res = GetNodesForOperation(arrayOfRanges, outArrayOfNodes, kMakeBasicBlock, aDontTouchContent);
  if (NS_FAILED(res)) return res;                                 
               
  // pre process our list of nodes...                      
  PRUint32 listCount;
  PRInt32 i;
  (*outArrayOfNodes)->Count(&listCount);
  for (i=(PRInt32)listCount-1; i>=0; i--)
  {
    nsCOMPtr<nsISupports> isupports = dont_AddRef((*outArrayOfNodes)->ElementAt(i));
    nsCOMPtr<nsIDOMNode> testNode( do_QueryInterface(isupports ) );

    // Remove all non-editable nodes.  Leave them be.
    if (!mHTMLEditor->IsEditable(testNode))
    {
      (*outArrayOfNodes)->RemoveElementAt(i);
    }
    
    // scan for table elements.  If we find table elements other than table,
    // replace it with a list of any editable non-table content.  Ditto for list elements.
    if (nsHTMLEditUtils::IsTableElement(testNode) ||
        nsHTMLEditUtils::IsList(testNode) || 
        nsHTMLEditUtils::IsListItem(testNode) )
    {
      (*outArrayOfNodes)->RemoveElementAt(i);
      res = GetInnerContent(testNode, *outArrayOfNodes);
      if (NS_FAILED(res)) return res;
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
  PRBool isCollapsed = ((item.startNode == item.endNode) && (item.startOffset == item.endOffset));

  nsCOMPtr<nsIDOMNode> endInline = GetHighestInlineParent(item.endNode);
  
  // if we have inline parents above range endpoints, split them
  if (endInline && !isCollapsed)
  {
    nsCOMPtr<nsIDOMNode> resultEndNode;
    PRInt32 resultEndOffset;
    item.endNode->GetParentNode(getter_AddRefs(resultEndNode));
    res = mHTMLEditor->SplitNodeDeep(endInline, item.endNode, item.endOffset,
                          &resultEndOffset, PR_TRUE);
    if (NS_FAILED(res)) return res;
    // reset range
    item.endNode = resultEndNode; item.endOffset = resultEndOffset;
  }

  nsCOMPtr<nsIDOMNode> startInline = GetHighestInlineParent(item.startNode);

  if (startInline)
  {
    nsCOMPtr<nsIDOMNode> resultStartNode;
    PRInt32 resultStartOffset;
    item.startNode->GetParentNode(getter_AddRefs(resultStartNode));
    res = mHTMLEditor->SplitNodeDeep(startInline, item.startNode, item.startOffset,
                          &resultStartOffset, PR_TRUE);
    if (NS_FAILED(res)) return res;
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
                                    nsCOMPtr<nsISupportsArray> *outArrayOfNodes)
{
  if (!inNode || !outArrayOfNodes) return NS_ERROR_NULL_POINTER;

  nsresult res = NS_NewISupportsArray(getter_AddRefs(*outArrayOfNodes));
  if (NS_FAILED(res)) return res;
  
  // first step is to build up a list of all the break nodes inside 
  // the inline container.
  nsCOMPtr<nsISupportsArray> arrayOfBreaks;
  nsBRNodeFunctor functor;
  nsDOMIterator iter;
  res = iter.Init(inNode);
  if (NS_FAILED(res)) return res;
  res = iter.MakeList(functor, address_of(arrayOfBreaks));
  if (NS_FAILED(res)) return res;
  
  // if there aren't any breaks, just put inNode itself in the array
  nsCOMPtr<nsISupports> isupports;
  PRUint32 listCount;
  arrayOfBreaks->Count(&listCount);
  if (!listCount)
  {
    isupports = do_QueryInterface(inNode);
    (*outArrayOfNodes)->AppendElement(isupports);
    if (NS_FAILED(res)) return res;
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
    
    for (i=0; i< (PRInt32)listCount; i++)
    {
      isupports = dont_AddRef(arrayOfBreaks->ElementAt(i));
      breakNode = do_QueryInterface(isupports);
      if (!breakNode) return NS_ERROR_NULL_POINTER;
      if (!splitDeepNode) return NS_ERROR_NULL_POINTER;
      res = nsEditor::GetNodeLocation(breakNode, address_of(splitParentNode), &splitOffset);
      if (NS_FAILED(res)) return res;
      res = mHTMLEditor->SplitNodeDeep(splitDeepNode, splitParentNode, splitOffset,
                          &resultOffset, PR_FALSE, address_of(leftNode), address_of(rightNode));
      if (NS_FAILED(res)) return res;
      // put left node in node list
      if (leftNode)
      {
        // might not be a left node.  a break might have been at the very
        // beginning of inline container, in which case splitnodedeep
        // would not actually split anything
        isupports = do_QueryInterface(leftNode);
        (*outArrayOfNodes)->AppendElement(isupports);
        if (NS_FAILED(res)) return res;
      }
      // move break outside of container and also put in node list
      res = mHTMLEditor->MoveNode(breakNode, inlineParentNode, resultOffset);
      if (NS_FAILED(res)) return res;
      isupports = do_QueryInterface(breakNode);
      (*outArrayOfNodes)->AppendElement(isupports);
      if (NS_FAILED(res)) return res;
      // now rightNode becomes the new node to split
      splitDeepNode = rightNode;
    }
    // now tack on remaining rightNode, if any, to the list
    if (rightNode)
    {
      isupports = do_QueryInterface(rightNode);
      (*outArrayOfNodes)->AppendElement(isupports);
      if (NS_FAILED(res)) return res;
    }
  }
  return res;
}


nsCOMPtr<nsIDOMNode> 
nsHTMLEditRules::GetHighestInlineParent(nsIDOMNode* aNode)
{
  if (!aNode) return nsnull;
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
// MakeTransitionList: detect all the transitions in the array, where a 
//                     transition means that adjacent nodes in the array 
//                     don't have the same parent.
//                       
nsresult 
nsHTMLEditRules::MakeTransitionList(nsISupportsArray *inArrayOfNodes, 
                                   nsVoidArray *inTransitionArray)
{
  if (!inArrayOfNodes || !inTransitionArray) return NS_ERROR_NULL_POINTER;

  PRUint32 listCount;
  PRUint32 i;
  inArrayOfNodes->Count(&listCount);
  nsVoidArray transitionList;
  nsCOMPtr<nsIDOMNode> prevElementParent;
  nsCOMPtr<nsIDOMNode> curElementParent;
  
  for (i=0; i<listCount; i++)
  {
    nsCOMPtr<nsISupports> isupports  = dont_AddRef(inArrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> transNode( do_QueryInterface(isupports ) );
    transNode->GetParentNode(getter_AddRefs(curElementParent));
    if (curElementParent != prevElementParent)
    {
      // different parents, or seperated by <br>: transition point
      inTransitionArray->InsertElementAt((void*)PR_TRUE,i);  
    }
    else
    {
      // same parents: these nodes grew up together
      inTransitionArray->InsertElementAt((void*)PR_FALSE,i); 
    }
    prevElementParent = curElementParent;
  }
  return NS_OK;
}



/********************************************************
 *  main implementation methods 
 ********************************************************/
 
///////////////////////////////////////////////////////////////////////////
// InsertTab: top level logic for determining how to insert a tab
//                       
nsresult 
nsHTMLEditRules::InsertTab(nsISelection *aSelection, 
                           nsAWritableString *outString)
{
  nsCOMPtr<nsIDOMNode> parentNode;
  PRInt32 offset;
  PRBool isPRE;
  
  nsresult res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(parentNode), &offset);
  if (NS_FAILED(res)) return res;
  
  if (!parentNode) return NS_ERROR_FAILURE;
    
  res = mHTMLEditor->IsPreformatted(parentNode, &isPRE);
  if (NS_FAILED(res)) return res;
    
  if (isPRE)
  {
    outString->Assign(PRUnichar('\t'));
  }
  else
  {
    // number of spaces should be a pref?
    // note that we dont play around with nbsps here anymore.  
    // let the AfterEdit whitespace cleanup code handle it.
    outString->Assign(NS_LITERAL_STRING("    "));
  }
  
  return NS_OK;
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
  if (!aSelection || !aHeader || !aNode) return NS_ERROR_NULL_POINTER;  
  
  // remeber where the header is
  nsCOMPtr<nsIDOMNode> headerParent;
  PRInt32 offset;
  nsresult res = nsEditor::GetNodeLocation(aHeader, address_of(headerParent), &offset);
  if (NS_FAILED(res)) return res;

  // get ws code to adjust any ws
  nsCOMPtr<nsIDOMNode> selNode = aNode;
  res = nsWSRunObject::PrepareToSplitAcrossBlocks(mHTMLEditor, address_of(selNode), &aOffset);
  if (NS_FAILED(res)) return res;

  // split the header
  PRInt32 newOffset;
  res = mHTMLEditor->SplitNodeDeep( aHeader, selNode, aOffset, &newOffset);
  if (NS_FAILED(res)) return res;

  // if the leftand heading is empty, put a mozbr in it
  nsCOMPtr<nsIDOMNode> prevItem;
  mHTMLEditor->GetPriorHTMLSibling(aHeader, address_of(prevItem));
  if (prevItem && nsHTMLEditUtils::IsHeader(prevItem))
  {
    PRBool bIsEmptyNode;
    res = mHTMLEditor->IsEmptyNode(prevItem, &bIsEmptyNode);
    if (NS_FAILED(res)) return res;
    if (bIsEmptyNode)
    {
      nsCOMPtr<nsIDOMNode> brNode;
      res = CreateMozBR(prevItem, 0, address_of(brNode));
      if (NS_FAILED(res)) return res;
    }
  }
  
  // if the new (righthand) header node is empty, delete it
  PRBool isEmpty;
  res = IsEmptyBlock(aHeader, &isEmpty, PR_TRUE);
  if (NS_FAILED(res)) return res;
  if (isEmpty)
  {
    res = mHTMLEditor->DeleteNode(aHeader);
    if (NS_FAILED(res)) return res;
    // layout tells the caret to blink in a weird place
    // if we dont place a break after the header.
    nsCOMPtr<nsIDOMNode> sibling;
    res = mHTMLEditor->GetNextHTMLSibling(headerParent, offset+1, address_of(sibling));
    if (NS_FAILED(res)) return res;
    if (!sibling || !nsTextEditUtils::IsBreak(sibling))
    {
      res = CreateMozBR(headerParent, offset+1, address_of(sibling));
      if (NS_FAILED(res)) return res;
    }
    res = nsEditor::GetNodeLocation(sibling, address_of(headerParent), &offset);
    if (NS_FAILED(res)) return res;
    // put selection after break
    res = aSelection->Collapse(headerParent,offset+1);
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
nsHTMLEditRules::ReturnInParagraph(nsISelection *aSelection, 
                                   nsIDOMNode *aPara, 
                                   nsIDOMNode *aNode, 
                                   PRInt32 aOffset,
                                   PRBool *aCancel,
                                   PRBool *aHandled)
{
  if (!aSelection || !aPara || !aNode || !aCancel || !aHandled) 
    { return NS_ERROR_NULL_POINTER; }
  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;

  nsCOMPtr<nsIDOMNode> sibling;
  nsresult res = NS_OK;

  // easy case, in a text node:
  if (mHTMLEditor->IsTextNode(aNode))
  {
    nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(aNode);
    PRUint32 strLength;
    res = textNode->GetLength(&strLength);
    if (NS_FAILED(res)) return res;
    
    // at beginning of text node?
    if (!aOffset)
    {
      // is there a BR prior to it?
      mHTMLEditor->GetPriorHTMLSibling(aNode, address_of(sibling));
      if (!sibling) 
      {
        // no previous sib, so
        // just fall out to default of inserting a BR
        return res;
      }
      if (nsTextEditUtils::IsBreak(sibling)
          && !nsTextEditUtils::HasMozAttr(sibling))
      {
        PRInt32 newOffset;
        *aCancel = PR_TRUE;
        // get ws code to adjust any ws
        nsCOMPtr<nsIDOMNode> selNode = aNode;
        res = nsWSRunObject::PrepareToSplitAcrossBlocks(mHTMLEditor, address_of(selNode), &aOffset);
        if (NS_FAILED(res)) return res;
        // split the paragraph
        res = mHTMLEditor->SplitNodeDeep( aPara, selNode, aOffset, &newOffset);
        if (NS_FAILED(res)) return res;
        // get rid of the break
        res = mHTMLEditor->DeleteNode(sibling);  
        if (NS_FAILED(res)) return res;
        // check both halves of para to see if we need mozBR
        res = InsertMozBRIfNeeded(aPara);
        if (NS_FAILED(res)) return res;
        res = mHTMLEditor->GetPriorHTMLSibling(aPara, address_of(sibling));
        if (NS_FAILED(res)) return res;
        if (sibling && nsHTMLEditUtils::IsParagraph(sibling))
        {
          res = InsertMozBRIfNeeded(sibling);
          if (NS_FAILED(res)) return res;
        }
        // position selection inside right hand para
        res = aSelection->Collapse(aPara,0);
      }
      // else just fall out to default of inserting a BR
      return res;
    }
    // at end of text node?
    if (aOffset == (PRInt32)strLength)
    {
      // is there a BR after to it?
      res = mHTMLEditor->GetNextHTMLSibling(aNode, address_of(sibling));
      if (!sibling) 
      {
        // no next sib, so
        // just fall out to default of inserting a BR
        return res;
      }
      if (nsTextEditUtils::IsBreak(sibling)
          && !nsTextEditUtils::HasMozAttr(sibling))
      {
        PRInt32 newOffset;
        *aCancel = PR_TRUE;
        // get ws code to adjust any ws
        nsCOMPtr<nsIDOMNode> selNode = aNode;
        res = nsWSRunObject::PrepareToSplitAcrossBlocks(mHTMLEditor, address_of(selNode), &aOffset);
        if (NS_FAILED(res)) return res;
        // split the paragraph
        res = mHTMLEditor->SplitNodeDeep(aPara, selNode, aOffset, &newOffset);
        if (NS_FAILED(res)) return res;
        // get rid of the break
        res = mHTMLEditor->DeleteNode(sibling);  
        if (NS_FAILED(res)) return res;
        // position selection inside right hand para
        res = aSelection->Collapse(aPara,0);
      }
      // else just fall out to default of inserting a BR
      return res;
    }
    // inside text node
    // just fall out to default of inserting a BR
    return res;
  }
  else
  {
    // not in a text node.  
    // is there a BR prior to it?
    nsCOMPtr<nsIDOMNode> nearNode;
    res = mHTMLEditor->GetPriorHTMLNode(aNode, aOffset, address_of(nearNode));
    if (NS_FAILED(res)) return res;
    if (!nearNode || !nsTextEditUtils::IsBreak(nearNode)
        || nsTextEditUtils::HasMozAttr(nearNode)) 
    {
      // is there a BR after to it?
      res = mHTMLEditor->GetNextHTMLNode(aNode, aOffset, address_of(nearNode));
      if (NS_FAILED(res)) return res;
      if (!nearNode || !nsTextEditUtils::IsBreak(nearNode)
          || nsTextEditUtils::HasMozAttr(nearNode)) 
      {
        // just fall out to default of inserting a BR
        return res;
      }
    }
    // else remove sibling br and split para
    PRInt32 newOffset;
    *aCancel = PR_TRUE;
    // get ws code to adjust any ws
    nsCOMPtr<nsIDOMNode> selNode = aNode;
    res = nsWSRunObject::PrepareToSplitAcrossBlocks(mHTMLEditor, address_of(selNode), &aOffset);
    if (NS_FAILED(res)) return res;
    // split the paragraph
    res = mHTMLEditor->SplitNodeDeep( aPara, selNode, aOffset, &newOffset);
    if (NS_FAILED(res)) return res;
    // get rid of the break
    res = mHTMLEditor->DeleteNode(nearNode);  
    if (NS_FAILED(res)) return res;
    // selection to beginning of right hand para
    aSelection->Collapse(aPara,0);
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
  if (!aSelection || !aListItem || !aNode) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsISelection> selection(aSelection);
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));
  nsresult res = NS_OK;
  
  nsCOMPtr<nsIDOMNode> listitem;
  
  // sanity check
  NS_PRECONDITION(PR_TRUE == nsHTMLEditUtils::IsListItem(aListItem),
                  "expected a list item and didnt get one");
  
  // if we are in an empty listitem, then we want to pop up out of the list
  PRBool isEmpty;
  res = IsEmptyBlock(aListItem, &isEmpty, PR_TRUE, PR_FALSE);
  if (NS_FAILED(res)) return res;
  if (isEmpty && mReturnInEmptyLIKillsList)   // but only if prefs says it's ok
  {
    nsCOMPtr<nsIDOMNode> list, listparent;
    PRInt32 offset, itemOffset;
    res = nsEditor::GetNodeLocation(aListItem, address_of(list), &itemOffset);
    if (NS_FAILED(res)) return res;
    res = nsEditor::GetNodeLocation(list, address_of(listparent), &offset);
    if (NS_FAILED(res)) return res;
    
    // are we the last list item in the list?
    PRBool bIsLast;
    res = mHTMLEditor->IsLastEditableChild(aListItem, &bIsLast);
    if (NS_FAILED(res)) return res;
    if (!bIsLast)
    {
      // we need to split the list!
      nsCOMPtr<nsIDOMNode> tempNode;
      res = mHTMLEditor->SplitNode(list, itemOffset, getter_AddRefs(tempNode));
      if (NS_FAILED(res)) return res;
    }
    // are we in a sublist?
    if (nsHTMLEditUtils::IsList(listparent))  //in a sublist
    {
      // if so, move this list item out of this list and into the grandparent list
      res = mHTMLEditor->MoveNode(aListItem,listparent,offset+1);
      if (NS_FAILED(res)) return res;
      res = aSelection->Collapse(aListItem,0);
    }
    else
    {
      // otherwise kill this listitem
      res = mHTMLEditor->DeleteNode(aListItem);
      if (NS_FAILED(res)) return res;
      
      // time to insert a break
      nsCOMPtr<nsIDOMNode> brNode;
      res = CreateMozBR(listparent, offset+1, address_of(brNode));
      if (NS_FAILED(res)) return res;
      
      // set selection to before the moz br
      selPriv->SetInterlinePosition(PR_TRUE);
      res = aSelection->Collapse(listparent,offset+1);
    }
    return res;
  }
  
  // else we want a new list item at the same list level.
  // get ws code to adjust any ws
  nsCOMPtr<nsIDOMNode> selNode = aNode;
  res = nsWSRunObject::PrepareToSplitAcrossBlocks(mHTMLEditor, address_of(selNode), &aOffset);
  if (NS_FAILED(res)) return res;
  // now split list item
  PRInt32 newOffset;
  res = mHTMLEditor->SplitNodeDeep( aListItem, selNode, aOffset, &newOffset);
  if (NS_FAILED(res)) return res;
  // hack: until I can change the damaged doc range code back to being
  // extra inclusive, I have to manually detect certain list items that
  // may be left empty.
  nsCOMPtr<nsIDOMNode> prevItem;
  mHTMLEditor->GetPriorHTMLSibling(aListItem, address_of(prevItem));
  if (prevItem && nsHTMLEditUtils::IsListItem(prevItem))
  {
    PRBool bIsEmptyNode;
    res = mHTMLEditor->IsEmptyNode(prevItem, &bIsEmptyNode);
    if (NS_FAILED(res)) return res;
    if (bIsEmptyNode)
    {
      nsCOMPtr<nsIDOMNode> brNode;
      res = CreateMozBR(prevItem, 0, address_of(brNode));
      if (NS_FAILED(res)) return res;
    }
  }
  res = aSelection->Collapse(aListItem,0);
  return res;
}


///////////////////////////////////////////////////////////////////////////
// MakeBlockquote:  put the list of nodes into one or more blockquotes.  
//                       
nsresult 
nsHTMLEditRules::MakeBlockquote(nsISupportsArray *arrayOfNodes)
{
  // the idea here is to put the nodes into a minimal number of 
  // blockquotes.  When the user blockquotes something, they expect
  // one blockquote.  That may not be possible (for instance, if they
  // have two table cells selected, you need two blockquotes inside the cells).
  
  if (!arrayOfNodes) return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;
  
  nsCOMPtr<nsIDOMNode> curNode, curParent, curBlock, newBlock;
  PRInt32 offset;
  PRUint32 listCount;
  
  arrayOfNodes->Count(&listCount);
  nsCOMPtr<nsIDOMNode> prevParent;
  
  PRUint32 i;
  for (i=0; i<listCount; i++)
  {
    // get the node to act on, and it's location
    nsCOMPtr<nsISupports> isupports  = dont_AddRef(arrayOfNodes->ElementAt(i));
    curNode = do_QueryInterface(isupports);
    res = nsEditor::GetNodeLocation(curNode, address_of(curParent), &offset);
    if (NS_FAILED(res)) return res;

    // if the node is a table element or list item, dive inside
    if ( (nsHTMLEditUtils::IsTableElement(curNode) && !(nsHTMLEditUtils::IsTable(curNode))) || 
         nsHTMLEditUtils::IsListItem(curNode) )
    {
      curBlock = 0;  // forget any previous block
      // recursion time
      nsCOMPtr<nsISupportsArray> childArray;
      res = GetChildNodesForOperation(curNode, address_of(childArray));
      if (NS_FAILED(res)) return res;
      res = MakeBlockquote(childArray);
      if (NS_FAILED(res)) return res;
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
      nsAutoString quoteType; quoteType.AssignWithConversion("blockquote");
      res = SplitAsNeeded(&quoteType, address_of(curParent), &offset);
      if (NS_FAILED(res)) return res;
      res = mHTMLEditor->CreateNode(quoteType, curParent, offset, getter_AddRefs(curBlock));
      if (NS_FAILED(res)) return res;
      // remember our new block for postprocessing
      mNewBlock = curBlock;
      // note: doesn't matter if we set mNewBlock multiple times.
    }
      
    PRUint32 blockLen;
    res = mHTMLEditor->GetLengthOfDOMNode(curBlock, blockLen);
    if (NS_FAILED(res)) return res;
    res = mHTMLEditor->MoveNode(curNode, curBlock, blockLen);
    if (NS_FAILED(res)) return res;
  }
  return res;
}



///////////////////////////////////////////////////////////////////////////
// RemoveBlockStyle:  make the list nodes have no special block type.  
//                       
nsresult 
nsHTMLEditRules::RemoveBlockStyle(nsISupportsArray *arrayOfNodes)
{
  // intent of this routine is to be used for converting to/from
  // headers, paragraphs, pre, and address.  Those blocks
  // that pretty much just contain inline things...
  
  if (!arrayOfNodes) return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;
  
  nsCOMPtr<nsIDOMNode> curNode, curParent, curBlock, firstNode, lastNode;
  PRInt32 offset;
  PRUint32 listCount;
    
  arrayOfNodes->Count(&listCount);
  
  PRUint32 i;
  for (i=0; i<listCount; i++)
  {
    // get the node to act on, and it's location
    nsCOMPtr<nsISupports> isupports  = dont_AddRef(arrayOfNodes->ElementAt(i));
    curNode = do_QueryInterface(isupports);
    res = nsEditor::GetNodeLocation(curNode, address_of(curParent), &offset);
    if (NS_FAILED(res)) return res;
    nsAutoString curNodeTag, curBlockTag;
    nsEditor::GetTagString(curNode, curNodeTag);
    curNodeTag.ToLowerCase();
 
    // if curNode is a address, p, header, address, or pre, remove it 
    if ((curNodeTag.EqualsWithConversion("pre")) || 
        (curNodeTag.EqualsWithConversion("p"))   ||
        (curNodeTag.EqualsWithConversion("h1"))  ||
        (curNodeTag.EqualsWithConversion("h2"))  ||
        (curNodeTag.EqualsWithConversion("h3"))  ||
        (curNodeTag.EqualsWithConversion("h4"))  ||
        (curNodeTag.EqualsWithConversion("h5"))  ||
        (curNodeTag.EqualsWithConversion("h6"))  ||
        (curNodeTag.EqualsWithConversion("address")))
    {
      // process any partial progress saved
      if (curBlock)
      {
        res = RemovePartOfBlock(curBlock, firstNode, lastNode);
        if (NS_FAILED(res)) return res;
        curBlock = 0;  firstNode = 0;  lastNode = 0;
      }
      // remove curent block
      res = mHTMLEditor->RemoveBlockContainer(curNode); 
      if (NS_FAILED(res)) return res;
    }
    else if ((curNodeTag.EqualsWithConversion("table"))      || 
             (curNodeTag.EqualsWithConversion("tbody"))      ||
             (curNodeTag.EqualsWithConversion("tr"))         ||
             (curNodeTag.EqualsWithConversion("td"))         ||
             (curNodeTag.EqualsWithConversion("ol"))         ||
             (curNodeTag.EqualsWithConversion("ul"))         ||
             (curNodeTag.EqualsWithConversion("dl"))         ||
             (curNodeTag.EqualsWithConversion("li"))         ||
             (curNodeTag.EqualsWithConversion("blockquote")) ||
             (curNodeTag.EqualsWithConversion("div"))) 
    {
      // process any partial progress saved
      if (curBlock)
      {
        res = RemovePartOfBlock(curBlock, firstNode, lastNode);
        if (NS_FAILED(res)) return res;
        curBlock = 0;  firstNode = 0;  lastNode = 0;
      }
      // recursion time
      nsCOMPtr<nsISupportsArray> childArray;
      res = GetChildNodesForOperation(curNode, address_of(childArray));
      if (NS_FAILED(res)) return res;
      res = RemoveBlockStyle(childArray);
      if (NS_FAILED(res)) return res;
    }
    else if (IsInlineNode(curNode))
    {
      if (curBlock)
      {
        // if so, is this node a descendant?
        if (nsHTMLEditUtils::IsDescendantOf(curNode, curBlock))
        {
          lastNode = curNode;
          continue;  // then we dont need to do anything different for this node
        }
        else
        {
          // otherwise, we have progressed beyond end of curBlock,
          // so lets handle it now.  We need to remove the portion of 
          // curBlock that contains [firstNode - lastNode].
          res = RemovePartOfBlock(curBlock, firstNode, lastNode);
          if (NS_FAILED(res)) return res;
          curBlock = 0;  firstNode = 0;  lastNode = 0;
          // fall out and handle curNode
        }
      }
      curBlock = mHTMLEditor->GetBlockNodeParent(curNode);
      nsEditor::GetTagString(curBlock, curBlockTag);
      curBlockTag.ToLowerCase();
      if ((curBlockTag.EqualsWithConversion("pre")) || 
          (curBlockTag.EqualsWithConversion("p"))   ||
          (curBlockTag.EqualsWithConversion("h1"))  ||
          (curBlockTag.EqualsWithConversion("h2"))  ||
          (curBlockTag.EqualsWithConversion("h3"))  ||
          (curBlockTag.EqualsWithConversion("h4"))  ||
          (curBlockTag.EqualsWithConversion("h5"))  ||
          (curBlockTag.EqualsWithConversion("h6"))  ||
          (curBlockTag.EqualsWithConversion("address")))
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
        if (NS_FAILED(res)) return res;
        curBlock = 0;  firstNode = 0;  lastNode = 0;
      }
    }
  }
  // process any partial progress saved
  if (curBlock)
  {
    res = RemovePartOfBlock(curBlock, firstNode, lastNode);
    if (NS_FAILED(res)) return res;
    curBlock = 0;  firstNode = 0;  lastNode = 0;
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// ApplyBlockStyle:  do whatever it takes to make the list of nodes into 
//                   one or more blocks of type blockTag.  
//                       
nsresult 
nsHTMLEditRules::ApplyBlockStyle(nsISupportsArray *arrayOfNodes, const nsAReadableString *aBlockTag)
{
  // intent of this routine is to be used for converting to/from
  // headers, paragraphs, pre, and address.  Those blocks
  // that pretty much just contain inline things...
  
  if (!arrayOfNodes || !aBlockTag) return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;
  
  nsCOMPtr<nsIDOMNode> curNode, curParent, curBlock, newBlock;
  PRInt32 offset;
  PRUint32 listCount;
  nsString tString(*aBlockTag);////MJUDGE SCC NEED HELP

  arrayOfNodes->Count(&listCount);
  
  PRUint32 i;
  for (i=0; i<listCount; i++)
  {
    // get the node to act on, and it's location
    nsCOMPtr<nsISupports> isupports  = dont_AddRef(arrayOfNodes->ElementAt(i));
    curNode = do_QueryInterface(isupports);
    res = nsEditor::GetNodeLocation(curNode, address_of(curParent), &offset);
    if (NS_FAILED(res)) return res;
    nsAutoString curNodeTag;
    nsEditor::GetTagString(curNode, curNodeTag);
    curNodeTag.ToLowerCase();
 
    // is it already the right kind of block?
    if (curNodeTag == *aBlockTag)
    {
      curBlock = 0;  // forget any previous block used for previous inline nodes
      continue;  // do nothing to this block
    }
        
    // if curNode is a address, p, header, address, or pre, replace 
    // it with a new block of correct type.
    // xxx floppy moose: pre cant hold everything the others can
    if (nsHTMLEditUtils::IsMozDiv(curNode)     ||
        (curNodeTag.EqualsWithConversion("pre")) || 
        (curNodeTag.EqualsWithConversion("p"))   ||
        (curNodeTag.EqualsWithConversion("h1"))  ||
        (curNodeTag.EqualsWithConversion("h2"))  ||
        (curNodeTag.EqualsWithConversion("h3"))  ||
        (curNodeTag.EqualsWithConversion("h4"))  ||
        (curNodeTag.EqualsWithConversion("h5"))  ||
        (curNodeTag.EqualsWithConversion("h6"))  ||
        (curNodeTag.EqualsWithConversion("address")))
    {
      curBlock = 0;  // forget any previous block used for previous inline nodes
      res = mHTMLEditor->ReplaceContainer(curNode, address_of(newBlock), *aBlockTag);
      if (NS_FAILED(res)) return res;
    }
    else if ((curNodeTag.EqualsWithConversion("table"))      || 
             (curNodeTag.EqualsWithConversion("tbody"))      ||
             (curNodeTag.EqualsWithConversion("tr"))         ||
             (curNodeTag.EqualsWithConversion("td"))         ||
             (curNodeTag.EqualsWithConversion("ol"))         ||
             (curNodeTag.EqualsWithConversion("ul"))         ||
             (curNodeTag.EqualsWithConversion("dl"))         ||
             (curNodeTag.EqualsWithConversion("li"))         ||
             (curNodeTag.EqualsWithConversion("blockquote")) ||
             (curNodeTag.EqualsWithConversion("div"))) 
    {
      curBlock = 0;  // forget any previous block used for previous inline nodes
      // recursion time
      nsCOMPtr<nsISupportsArray> childArray;
      res = GetChildNodesForOperation(curNode, address_of(childArray));
      if (NS_FAILED(res)) return res;
      res = ApplyBlockStyle(childArray, aBlockTag);
      if (NS_FAILED(res)) return res;
    }
    
    // if the node is a break, we honor it by putting further nodes in a new parent
    else if (curNodeTag.EqualsWithConversion("br"))
    {
      curBlock = 0;  // forget any previous block used for previous inline nodes
      res = mHTMLEditor->DeleteNode(curNode);
      if (NS_FAILED(res)) return res;
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
      if (!Compare(tString,NS_LITERAL_STRING("pre"),nsCaseInsensitiveStringComparator()) 
        && (!mHTMLEditor->IsEditable(curNode)))
        continue; // do nothing to this block
      
      // if no curBlock, make one
      if (!curBlock)
      {
        res = SplitAsNeeded(aBlockTag, address_of(curParent), &offset);
        if (NS_FAILED(res)) return res;
        res = mHTMLEditor->CreateNode(*aBlockTag, curParent, offset, getter_AddRefs(curBlock));
        if (NS_FAILED(res)) return res;
        // remember our new block for postprocessing
        mNewBlock = curBlock;
        // note: doesn't matter if we set mNewBlock multiple times.
      }
      
      // if curNode is a Break, replace it with a return if we are going to <pre>
      // xxx floppy moose
 
      // this is a continuation of some inline nodes that belong together in
      // the same block item.  use curBlock
      PRUint32 blockLen;
      res = mHTMLEditor->GetLengthOfDOMNode(curBlock, blockLen);
      if (NS_FAILED(res)) return res;
      res = mHTMLEditor->MoveNode(curNode, curBlock, blockLen);
      if (NS_FAILED(res)) return res;
    }
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// SplitAsNeeded:  given a tag name, split inOutParent up to the point   
//                 where we can insert the tag.  Adjust inOutParent and
//                 inOutOffset to pint to new location for tag.
nsresult 
nsHTMLEditRules::SplitAsNeeded(const nsAReadableString *aTag, 
                               nsCOMPtr<nsIDOMNode> *inOutParent,
                               PRInt32 *inOutOffset)
{
  if (!aTag || !inOutParent || !inOutOffset) return NS_ERROR_NULL_POINTER;
  if (!*inOutParent) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIDOMNode> tagParent, temp, splitNode, parent = *inOutParent;
  nsresult res = NS_OK;
   
  // check that we have a place that can legally contain the tag
  while (!tagParent)
  {
    // sniffing up the parent tree until we find 
    // a legal place for the block
    if (!parent) break;
    if (mHTMLEditor->CanContainTag(parent, *aTag))
    {
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
    if (NS_FAILED(res)) return res;
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
  if (!aNodeLeft ||  
      !aNodeRight || 
      !aOutMergeParent ||
      !aOutMergeOffset) 
    return NS_ERROR_NULL_POINTER;
  
  nsresult res = NS_OK;
  // caller responsible for:
  //   left & right node are same type
  PRInt32 parOffset;
  nsCOMPtr<nsIDOMNode> parent, rightParent;
  res = nsEditor::GetNodeLocation(aNodeLeft, address_of(parent), &parOffset);
  if (NS_FAILED(res)) return res;
  aNodeRight->GetParentNode(getter_AddRefs(rightParent));

  // if they don't have the same parent, first move the 'right' node 
  // to after the 'left' one
  if (parent != rightParent)
  {
    res = mHTMLEditor->MoveNode(aNodeRight, parent, parOffset);
    if (NS_FAILED(res)) return res;
  }
  
  // defaults for outParams
  *aOutMergeParent = aNodeRight;
  res = mHTMLEditor->GetLengthOfDOMNode(aNodeLeft, *((PRUint32*)aOutMergeOffset));
  if (NS_FAILED(res)) return res;

  // seperate join rules for differing blocks
  if (nsHTMLEditUtils::IsParagraph(aNodeLeft))
  {
    // for para's, merge deep & add a <br> after merging
    res = mHTMLEditor->JoinNodeDeep(aNodeLeft, aNodeRight, aOutMergeParent, aOutMergeOffset);
    if (NS_FAILED(res)) return res;
    // now we need to insert a br.  
    nsCOMPtr<nsIDOMNode> brNode;
    res = mHTMLEditor->CreateBR(*aOutMergeParent, *aOutMergeOffset, address_of(brNode));
    if (NS_FAILED(res)) return res;
    res = nsEditor::GetNodeLocation(brNode, aOutMergeParent, aOutMergeOffset);
    if (NS_FAILED(res)) return res;
    (*aOutMergeOffset)++;
    return res;
  }
  else if (nsHTMLEditUtils::IsList(aNodeLeft)
           || mHTMLEditor->IsTextNode(aNodeLeft))
  {
    // for list's, merge shallow (wouldn't want to combine list items)
    res = mHTMLEditor->JoinNodes(aNodeLeft, aNodeRight, parent);
    if (NS_FAILED(res)) return res;
    return res;
  }
  else
  {
    // remember the last left child, and firt right child
    nsCOMPtr<nsIDOMNode> lastLeft, firstRight;
    res = mHTMLEditor->GetLastEditableChild(aNodeLeft, address_of(lastLeft));
    if (NS_FAILED(res)) return res;
    res = mHTMLEditor->GetFirstEditableChild(aNodeRight, address_of(firstRight));
    if (NS_FAILED(res)) return res;

    // for list items, divs, etc, merge smart
    res = mHTMLEditor->JoinNodes(aNodeLeft, aNodeRight, parent);
    if (NS_FAILED(res)) return res;

    if (lastLeft && firstRight && mHTMLEditor->NodesSameType(lastLeft, firstRight))
    {
      return JoinNodesSmart(lastLeft, firstRight, aOutMergeParent, aOutMergeOffset);
    }
  }
  return res;
}


nsresult 
nsHTMLEditRules::GetTopEnclosingMailCite(nsIDOMNode *aNode, 
                                         nsCOMPtr<nsIDOMNode> *aOutCiteNode,
                                         PRBool aPlainText)
{
  // check parms
  if (!aNode || !aOutCiteNode) 
    return NS_ERROR_NULL_POINTER;
  
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> node, parentNode;
  node = do_QueryInterface(aNode);
  
  while (node)
  {
    if ( (aPlainText && nsHTMLEditUtils::IsPre(node)) ||
         (!aPlainText && nsHTMLEditUtils::IsMailCite(node)) )
      *aOutCiteNode = node;
    if (nsTextEditUtils::IsBody(node)) break;
    
    res = node->GetParentNode(getter_AddRefs(parentNode));
    if (NS_FAILED(res)) return res;
    node = parentNode;
  }

  return res;
}


nsresult 
nsHTMLEditRules::AdjustSpecialBreaks(PRBool aSafeToAskFrames)
{
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  nsCOMPtr<nsISupports> isupports;
  PRUint32 nodeCount,j;
  
  // gather list of empty nodes
  nsEmptyFunctor functor(mHTMLEditor);
  nsDOMIterator iter;
  nsresult res = iter.Init(mDocChangeRange);
  if (NS_FAILED(res)) return res;
  res = iter.MakeList(functor, address_of(arrayOfNodes));
  if (NS_FAILED(res)) return res;

  // put moz-br's into these empty li's and td's
  res = arrayOfNodes->Count(&nodeCount);
  if (NS_FAILED(res)) return res;
  for (j = 0; j < nodeCount; j++)
  {
    isupports = dont_AddRef(arrayOfNodes->ElementAt(0));
    nsCOMPtr<nsIDOMNode> brNode, theNode( do_QueryInterface(isupports ) );
    arrayOfNodes->RemoveElementAt(0);
    res = CreateMozBR(theNode, 0, address_of(brNode));
    if (NS_FAILED(res)) return res;
  }
  
  return res;
}

nsresult 
nsHTMLEditRules::AdjustWhitespace(nsISelection *aSelection)
{
  // get selection point
  nsCOMPtr<nsIDOMNode> selNode;
  PRInt32 selOffset;
  nsresult res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(selNode), &selOffset);
  if (NS_FAILED(res)) return res;
  
  // ask whitespace object to tweak nbsp's
  return nsWSRunObject(mHTMLEditor, selNode, selOffset).AdjustWhitespace();
}

nsresult 
nsHTMLEditRules::PinSelectionToNewBlock(nsISelection *aSelection)
{
  if (!aSelection) return NS_ERROR_NULL_POINTER;
  PRBool bCollapsed;
  nsresult res = aSelection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  if (!bCollapsed) return res;

  // get the (collapsed) selection location
  nsCOMPtr<nsIDOMNode> selNode, temp;
  PRInt32 selOffset;
  res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(selNode), &selOffset);
  if (NS_FAILED(res)) return res;
  temp = selNode;
  
  // use ranges and mRangeHelper to compare sel point to new block
  nsCOMPtr<nsIDOMRange> range = do_CreateInstance(kRangeCID);
  res = range->SetStart(selNode, selOffset);
  if (NS_FAILED(res)) return res;
  res = range->SetEnd(selNode, selOffset);
  if (NS_FAILED(res)) return res;
  nsCOMPtr<nsIContent> block (do_QueryInterface(mNewBlock));
  if (!block) return NS_ERROR_NO_INTERFACE;
  PRBool nodeBefore, nodeAfter;
  res = mHTMLEditor->mRangeHelper->CompareNodeToRange(block, range, &nodeBefore, &nodeAfter);
  if (NS_FAILED(res)) return res;
  
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
      if (NS_FAILED(res)) return res;
    }
    else
    {
      nsCOMPtr<nsIDOMNode> tmp2;
      res = nsEditor::GetNodeLocation(tmp, address_of(tmp2), (PRInt32*)&endPoint);
      if (NS_FAILED(res)) return res;
      tmp = tmp2;
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
      nsCOMPtr<nsIDOMNode> tmp2;
      res = nsEditor::GetNodeLocation(tmp, address_of(tmp2), &offset);
      if (NS_FAILED(res)) return res;
      tmp = tmp2;
    }
    return aSelection->Collapse(tmp, 0);
  }
}

nsresult 
nsHTMLEditRules::CheckInterlinePosition(nsISelection *aSelection)
{
  if (!aSelection) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsISelection> selection(aSelection);
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));

  // if the selection isn't collapsed, do nothing.
  PRBool bCollapsed;
  nsresult res = aSelection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  if (!bCollapsed) return res;

  // get the (collapsed) selection location
  nsCOMPtr<nsIDOMNode> selNode, node;
  PRInt32 selOffset;
  res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(selNode), &selOffset);
  if (NS_FAILED(res)) return res;
  
  // are we after a block?  If so try set caret to following content
  mHTMLEditor->GetPriorHTMLSibling(selNode, selOffset, address_of(node));
  if (node && IsBlockNode(node))
  {
    selPriv->SetInterlinePosition(PR_TRUE);
    return NS_OK;
  }

  // are we before a block?  If so try set caret to prior content
  mHTMLEditor->GetNextHTMLSibling(selNode, selOffset, address_of(node));
  if (node && IsBlockNode(node))
  {
    selPriv->SetInterlinePosition(PR_FALSE);
    return NS_OK;
  }
  return NS_OK;
}

nsresult 
nsHTMLEditRules::AdjustSelection(nsISelection *aSelection, nsIEditor::EDirection aAction)
{
  if (!aSelection) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsISelection> selection(aSelection);
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));
 
  // if the selection isn't collapsed, do nothing.
  // moose: one thing to do instead is check for the case of
  // only a single break selected, and collapse it.  Good thing?  Beats me.
  PRBool bCollapsed;
  nsresult res = aSelection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  if (!bCollapsed) return res;

  // get the (collapsed) selection location
  nsCOMPtr<nsIDOMNode> selNode, temp;
  PRInt32 selOffset;
  res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(selNode), &selOffset);
  if (NS_FAILED(res)) return res;
  temp = selNode;
  
  // are we in an editable node?
  while (!mHTMLEditor->IsEditable(selNode))
  {
    // scan up the tree until we find an editable place to be
    res = nsEditor::GetNodeLocation(temp, address_of(selNode), &selOffset);
    if (NS_FAILED(res)) return res;
    if (!selNode) return NS_ERROR_FAILURE;
    temp = selNode;
  }
  
  // are we in a text node? 
  nsCOMPtr<nsIDOMCharacterData> textNode = do_QueryInterface(selNode);
  if (textNode) 
    return NS_OK; // we LIKE it when we are in a text node.  that RULZ
  
  // do we need to insert a special mozBR?  We do if we are:
  // 1) that block is same block where selection is AND
  // 2) in a collapsed selection AND
  // 3) after a normal (non-moz) br AND
  // 4) that br is the last editable node in it's block 

  nsCOMPtr<nsIDOMNode> nearNode;
  res = mHTMLEditor->GetPriorHTMLNode(selNode, selOffset, address_of(nearNode));
  if (NS_FAILED(res)) return res;
  if (!nearNode) return res;
  
  // is nearNode also a descendant of same block?
  nsCOMPtr<nsIDOMNode> block, nearBlock;
  if (IsBlockNode(selNode)) block = selNode;
  else block = mHTMLEditor->GetBlockNodeParent(selNode);
  nearBlock = mHTMLEditor->GetBlockNodeParent(nearNode);
  if (block == nearBlock)
  {
    if (nearNode && nsTextEditUtils::IsBreak(nearNode)
        && !nsTextEditUtils::IsMozBR(nearNode))
    {
      PRBool bIsLast;
      res = mHTMLEditor->IsLastEditableChild(nearNode, &bIsLast);
      if (NS_FAILED(res)) return res;
      if (bIsLast)
      {
        // need to insert special moz BR. Why?  Because if we don't
        // the user will see no new line for the break.  Also, things
        // like table cells won't grow in height.
        nsCOMPtr<nsIDOMNode> brNode;
        res = CreateMozBR(selNode, selOffset, address_of(brNode));
        if (NS_FAILED(res)) return res;
        res = nsEditor::GetNodeLocation(brNode, address_of(selNode), &selOffset);
        if (NS_FAILED(res)) return res;
        // selection stays *before* moz-br, sticking to it
        selPriv->SetInterlinePosition(PR_TRUE);
        res = aSelection->Collapse(selNode,selOffset);
        if (NS_FAILED(res)) return res;
      }
      else
      {
        // ok, the br inst the last child.  
        // the br might be right in front of a new block (ie,:
        // <body> text<br> <ol><li>list item</li></ol></body>  )
        // in this case we also need moz-br.
        nsCOMPtr<nsIDOMNode> nextNode;
        res = mHTMLEditor->GetNextHTMLNode(nearNode, address_of(nextNode));
        if (NS_FAILED(res)) return res;
        res = mHTMLEditor->GetNextHTMLSibling(nearNode, address_of(nextNode));
        if (NS_FAILED(res)) return res;
        if (nextNode && IsBlockNode(nextNode))
        {
          // need to insert special moz BR. Why?  Because if we don't
          // the user will see no new line for the break.  
          nsCOMPtr<nsIDOMNode> brNode;
          res = CreateMozBR(selNode, selOffset, address_of(brNode));
          if (NS_FAILED(res)) return res;
          res = nsEditor::GetNodeLocation(brNode, address_of(selNode), &selOffset);
          if (NS_FAILED(res)) return res;
          // selection stays *before* moz-br, sticking to it
          selPriv->SetInterlinePosition(PR_TRUE);
          res = aSelection->Collapse(selNode,selOffset);
          if (NS_FAILED(res)) return res;
        }
        else if (nextNode && nsTextEditUtils::IsMozBR(nextNode))
        {
          // selection between br and mozbr.  make it stick to mozbr
          // so that it will be on blank line.   
          selPriv->SetInterlinePosition(PR_TRUE);
        }
      }
    }
  }

  // we aren't in a textnode: are we adjacent to a break or an image?
  res = mHTMLEditor->GetPriorHTMLSibling(selNode, selOffset, address_of(nearNode));
  if (NS_FAILED(res)) return res;
  if (nearNode && (nsTextEditUtils::IsBreak(nearNode)
                   || nsHTMLEditUtils::IsImage(nearNode)))
    return NS_OK; // this is a good place for the caret to be
  res = mHTMLEditor->GetNextHTMLSibling(selNode, selOffset, address_of(nearNode));
  if (NS_FAILED(res)) return res;
  if (nearNode && (nsTextEditUtils::IsBreak(nearNode)
                   || nsHTMLEditUtils::IsImage(nearNode)))
    return NS_OK; // this is a good place for the caret to be

  // look for a nearby text node.
  // prefer the correct direction.
  res = FindNearSelectableNode(selNode, selOffset, aAction, address_of(nearNode));
  if (NS_FAILED(res)) return res;

  if (!nearNode) return NS_OK; // couldn't find a near text node

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
    res = nsEditor::GetNodeLocation(nearNode, address_of(selNode), &selOffset);
    if (NS_FAILED(res)) return res;
    if (aAction == nsIEditor::ePrevious) selOffset++;  // want to be beyond it if we backed up to it
    res = aSelection->Collapse(selNode, selOffset);
  }

  return res;
}


PRBool
nsHTMLEditRules::IsVisBreak(nsIDOMNode *aNode)
{
  if (!aNode) 
    return PR_FALSE;
  if (!nsTextEditUtils::IsBreak(aNode)) 
    return PR_FALSE;
  // check if there is a later node in block after br
  nsCOMPtr<nsIDOMNode> nextNode;
  mHTMLEditor->GetNextHTMLNode(aNode, address_of(nextNode), PR_TRUE); 
  if (!nextNode) 
    return PR_FALSE;  // this break is trailer in block, it's not visible
  return PR_TRUE;
}


nsresult
nsHTMLEditRules::FindNearSelectableNode(nsIDOMNode *aSelNode, 
                                        PRInt32 aSelOffset, 
                                        nsIEditor::EDirection &aDirection,
                                        nsCOMPtr<nsIDOMNode> *outSelectableNode)
{
  if (!aSelNode || !outSelectableNode) return NS_ERROR_NULL_POINTER;
  *outSelectableNode = nsnull;
  nsresult res = NS_OK;
  
  nsCOMPtr<nsIDOMNode> nearNode, curNode;
  if (aDirection == nsIEditor::ePrevious)
    res = mHTMLEditor->GetPriorHTMLNode(aSelNode, aSelOffset, address_of(nearNode));
  else
    res = mHTMLEditor->GetNextHTMLNode(aSelNode, aSelOffset, address_of(nearNode));
  if (NS_FAILED(res)) return res;
  
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
    if (NS_FAILED(res)) return res;
  }
  
  // scan in the right direction until we find an eligible text node,
  // but dont cross any breaks, images, or table elements.
  while (nearNode && !(mHTMLEditor->IsTextNode(nearNode)
                       || IsVisBreak(nearNode)
                       || nsHTMLEditUtils::IsImage(nearNode)))
  {
    curNode = nearNode;
    if (aDirection == nsIEditor::ePrevious)
      res = mHTMLEditor->GetPriorHTMLNode(curNode, address_of(nearNode));
    else
      res = mHTMLEditor->GetNextHTMLNode(curNode, address_of(nearNode));
    if (NS_FAILED(res)) return res;
  }
  
  if (nearNode)
  {
    // dont cross any table elements
    PRBool bInDifTblElems;
    res = InDifferentTableElements(nearNode, aSelNode, &bInDifTblElems);
    if (NS_FAILED(res)) return res;
    if (bInDifTblElems) return NS_OK;  
    
    // otherwise, ok, we have found a good spot to put the selection
    *outSelectableNode = do_QueryInterface(nearNode);
  }
  return res;
}


nsresult
nsHTMLEditRules::InDifferentTableElements(nsIDOMNode *aNode1, nsIDOMNode *aNode2, PRBool *aResult)
{
  NS_ASSERTION(aNode1 && aNode2 && aResult, "null args");
  if (!aNode1 || !aNode2 || !aResult) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMNode> tn1, tn2, node = aNode1, temp;
  
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
  nsCOMPtr<nsIContentIterator> iter;
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  nsCOMPtr<nsISupports> isupports;
  PRUint32 nodeCount,j;
  
  // make an isupportsArray to hold a list of nodes
  nsresult res = NS_NewISupportsArray(getter_AddRefs(arrayOfNodes));
  if (NS_FAILED(res)) return res;

  // need an iterator
  iter = do_CreateInstance(kContentIteratorCID);
  if (!iter) return NS_ERROR_NULL_POINTER;
  
  // loop over iter and create list of empty containers
  do
  {
    res = iter->Init(mDocChangeRange);
    if (NS_FAILED(res)) return res;
    
    // gather up a list of empty nodes
    while (NS_ENUMERATOR_FALSE == iter->IsDone())
    {
      nsCOMPtr<nsIDOMNode> node;
      nsCOMPtr<nsIContent> content;
      res = iter->CurrentNode(getter_AddRefs(content));
      if (NS_FAILED(res)) return res;
      node = do_QueryInterface(content);
      if (!node) return NS_ERROR_FAILURE;
      
      PRBool bIsEmptyNode;
      res = mHTMLEditor->IsEmptyNode(node, &bIsEmptyNode, PR_FALSE, PR_TRUE);
      if (NS_FAILED(res)) return res;
      
      // only consider certain nodes to be empty for purposes of removal
      if (!(
            nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("a"))      || 
            nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("b"))      || 
            nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("i"))      || 
            nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("u"))      || 
            nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("tt"))     || 
            nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("s"))      || 
            nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("strike")) || 
            nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("big"))    || 
            nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("small"))  || 
            nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("blink"))  || 
            nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("sub"))    || 
            nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("sup"))    || 
            nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("font"))   || 
            nsHTMLEditUtils::IsList(node)      || 
            nsHTMLEditUtils::IsParagraph(node) ||
            nsHTMLEditUtils::IsHeader(node)    ||
            nsHTMLEditUtils::IsListItem(node)  ||
            nsHTMLEditUtils::IsBlockquote(node)||
            nsHTMLEditUtils::IsDiv(node)       ||
            nsHTMLEditUtils::IsPre(node)       ||
            nsHTMLEditUtils::IsAddress(node) ) )
      {
        bIsEmptyNode = PR_FALSE;
      }
      
      if (bIsEmptyNode && !nsTextEditUtils::IsBody(node))
      {
        if (nsHTMLEditUtils::IsParagraph(node) ||
            nsHTMLEditUtils::IsHeader(node)    ||
            nsHTMLEditUtils::IsListItem(node)  ||
            nsHTMLEditUtils::IsBlockquote(node)||
            nsHTMLEditUtils::IsPre(node)       ||
            nsHTMLEditUtils::IsAddress(node) )
        {
          // if it is one of these, dont delete if sel inside.
          // this is so we can create empty headings, etc, for the
          // user to type into.
          PRBool bIsSelInNode;
          res = SelectionEndpointInNode(node, &bIsSelInNode);
          if (NS_FAILED(res)) return res;
          if (!bIsSelInNode)
          {
            isupports = do_QueryInterface(node);
            arrayOfNodes->AppendElement(isupports);
          }
        }
        else
        {
          // if it's not such an element, delete it even if sel is inside
          isupports = do_QueryInterface(node);
          arrayOfNodes->AppendElement(isupports);
        }
      }
      
      res = iter->Next();
      if (NS_FAILED(res)) return res;
    }
    
    // now delete the empty nodes
    res = arrayOfNodes->Count(&nodeCount);
    if (NS_FAILED(res)) return res;
    for (j = 0; j < nodeCount; j++)
    {
      isupports = dont_AddRef(arrayOfNodes->ElementAt(0));
      nsCOMPtr<nsIDOMNode> delNode( do_QueryInterface(isupports ) );
      arrayOfNodes->RemoveElementAt(0);
      res = mHTMLEditor->DeleteNode(delNode);
      if (NS_FAILED(res)) return res;
    }
    
  } while (nodeCount);  // if we deleted any, loop again
                        // deleting some nodes may make some parents now empty
  return res;
}

nsresult
nsHTMLEditRules::SelectionEndpointInNode(nsIDOMNode *aNode, PRBool *aResult)
{
  if (!aNode || !aResult) return NS_ERROR_NULL_POINTER;
  
  *aResult = PR_FALSE;
  
  nsCOMPtr<nsISelection>selection;
  nsresult res = mHTMLEditor->GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  nsCOMPtr<nsISelectionPrivate>selPriv(do_QueryInterface(selection));
  
  nsCOMPtr<nsIEnumerator> enumerator;
  res = selPriv->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(res)) return res;
  if (!enumerator) return NS_ERROR_UNEXPECTED;

  for (enumerator->First(); NS_OK!=enumerator->IsDone(); enumerator->Next())
  {
    nsCOMPtr<nsISupports> currentItem;
    res = enumerator->CurrentItem(getter_AddRefs(currentItem));
    if (NS_FAILED(res)) return res;
    if (!currentItem) return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
    nsCOMPtr<nsIDOMNode> startParent, endParent;
    range->GetStartContainer(getter_AddRefs(startParent));
    if (startParent)
    {
      if (aNode == startParent.get())
      {
        *aResult = PR_TRUE;
        return NS_OK;
      }
      if (nsHTMLEditUtils::IsDescendantOf(startParent, aNode)) 
      {
        *aResult = PR_TRUE;
        return NS_OK;
      }
    }
    range->GetEndContainer(getter_AddRefs(endParent));
    if (startParent == endParent) continue;
    if (endParent)
    {
      if (aNode == endParent.get()) 
      {
        *aResult = PR_TRUE;
        return NS_OK;
      }
      if (nsHTMLEditUtils::IsDescendantOf(endParent, aNode))
      {
        *aResult = PR_TRUE;
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
PRBool 
nsHTMLEditRules::IsEmptyInline(nsIDOMNode *aNode)
{
  if (aNode && IsInlineNode(aNode) && mHTMLEditor->IsContainer(aNode)) 
  {
    PRBool bEmpty;
    mHTMLEditor->IsEmptyNode(aNode, &bEmpty);
    return bEmpty;
  }
  return PR_FALSE;
}


PRBool 
nsHTMLEditRules::ListIsEmptyLine(nsISupportsArray *arrayOfNodes)
{
  // we have a list of nodes which we are candidates for being moved
  // into a new block.  Determine if it's anything more than a blank line.
  // Look for editable content above and beyond one single BR.
  if (!arrayOfNodes) return PR_TRUE;
  PRUint32 listCount;
  arrayOfNodes->Count(&listCount);
  if (!listCount) return PR_TRUE;
  nsCOMPtr<nsIDOMNode> somenode;
  nsCOMPtr<nsISupports> isupports;
  PRInt32 j, brCount=0;
  arrayOfNodes->Count(&listCount);
  for (j = 0; j < listCount; j++)
  {
    isupports = dont_AddRef(arrayOfNodes->ElementAt(j));
    somenode = do_QueryInterface(isupports);
    if (somenode && mHTMLEditor->IsEditable(somenode))
    {
      if (nsTextEditUtils::IsBreak(somenode))
      {
        // first break doesn't count
        if (brCount) return PR_FALSE;
        brCount++;
      }
      else if (IsEmptyInline(somenode)) 
      {
        // empty inline, keep looking
      }
      else return PR_FALSE;
    }
  }
  return PR_TRUE;
}


nsresult 
nsHTMLEditRules::DoTextNodeWhitespace(nsIDOMCharacterData *aTextNode, PRInt32 aStart, PRInt32 aEnd)
{
  // check parms
  if (!aTextNode) return NS_ERROR_NULL_POINTER;
  if (aStart == -1)  // -1 means do the whole darn node please
  {
    aStart = 0;
    aTextNode->GetLength((PRUint32*)&aEnd);
  }
  if (aStart == aEnd) return NS_OK;
  
  nsresult res = NS_OK;
  PRBool isPRE;
  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aTextNode);
  
  res = mHTMLEditor->IsPreformatted(node,&isPRE);
  if (NS_FAILED(res)) return res;
    
  if (isPRE)
  {
    // text node has a Preformatted style. All we need to do is strip out any nbsp's 
    // we just put in and replace them with spaces.
  
    // moose: write me!
    return res;
  }
  
  // else we are not preformatted.  Need to convert any adjacent spaces to alterating
  // space/nbsp pairs; need to convert stranded nbsp's to spaces; need to convert
  // tabs to whitespace; need to convert returns to whitespace.
  PRInt32 j = 0;
  nsAutoString tempString;

  aTextNode->SubstringData(aStart, aEnd, tempString);
  
  // identify runs of whitespace
  PRInt32 runStart = -1, runEnd = -1;
  do {
    PRUnichar c = tempString[j];
    PRBool isSpace = nsCRT::IsAsciiSpace(c);
    if (isSpace || c==nbsp)
    {
      if (runStart<0) runStart = j;
      runEnd = j+1;
    }
    // translation of below line:
    // if we have a whitespace run, AND
    // either we are at the end of it, or the end of the whole string,
    // THEN process it
    if (runStart>=0 && (!(isSpace || c==nbsp)  ||  (j==aEnd-1)) )
    {
      // current char is non whitespace, but we have identified an earlier
      // run of whitespace.  convert it if needed.
      NS_PRECONDITION(runEnd>runStart, "this is what happens when integers turn bad!");
      // runStart to runEnd is a run of whitespace
      nsAutoString runStr, newStr;
      tempString.Mid(runStr, runStart, runEnd-runStart);
      
      res = ConvertWhitespace(runStr, newStr);
      if (NS_FAILED(res)) return res;
      
      if (runStr != newStr)
      {
        // delete the original whitespace run
        EditTxn *txn;
        // note 1: we are not telling edit listeners about these because they don't care
        // note 2: we are not wrapping these in a placeholder because we know they already are
        res = mHTMLEditor->CreateTxnForDeleteText(aTextNode, aStart+runStart, runEnd-runStart, (DeleteTextTxn**)&txn);
        if (NS_FAILED(res))  return res; 
        if (!txn)  return NS_ERROR_OUT_OF_MEMORY;
        res = mHTMLEditor->Do(txn); 
        if (NS_FAILED(res))  return res; 
        // The transaction system (if any) has taken ownwership of txn
        NS_IF_RELEASE(txn);
        
        // insert the new run
        res = mHTMLEditor->CreateTxnForInsertText(newStr, aTextNode, aStart+runStart, (InsertTextTxn**)&txn);
        if (NS_FAILED(res))  return res; 
        if (!txn)  return NS_ERROR_OUT_OF_MEMORY;
        res = mHTMLEditor->Do(txn);
        // The transaction system (if any) has taken ownwership of txns.
        NS_IF_RELEASE(txn);
      } 
      runStart = -1; // reset our run     
    }
    j++; // next char please!
  } while ((PRUint32)j < tempString.Length());

  return res;
}


nsresult 
nsHTMLEditRules::ConvertWhitespace(const nsAReadableString & inString, nsAWritableString & outString)
{
  PRUint32 j,len = inString.Length();
  nsReadingIterator <PRUnichar> iter;
  inString.BeginReading(iter);
  switch (len)
  {
    case 0:
      outString.SetLength(0);
      return NS_OK;
    case 1:
      if (*iter == '\n')   // a bit of a hack: don't convert single newlines that 
        outString.Assign(NS_LITERAL_STRING("\n"));     // dont have whitespace adjacent.  This is to preserve 
      else                    // html source formatting to some degree.
        outString.Assign(NS_LITERAL_STRING(" "));
      return NS_OK;
    case 2:
      outString.Assign(PRUnichar(nbsp));
      outString.Append(NS_LITERAL_STRING(" "));
      return NS_OK;
    case 3:
      outString.Assign(NS_LITERAL_STRING(" "));
      outString += PRUnichar(nbsp);
      outString.Append(NS_LITERAL_STRING(" "));
      return NS_OK;
  }
  if (len%2)  // length is odd
  {
    for (j=0;j<len;j++)
    {
      if (!(j & 0x01)) outString.Append(NS_LITERAL_STRING(" "));      // even char
      else outString += PRUnichar(nbsp); // odd char
    }
  }
  else
  {
    outString.Assign(NS_LITERAL_STRING(" "));
    outString += PRUnichar(nbsp);
    outString += PRUnichar(nbsp);
    for (j=0;j<len-3;j++)
    {
      if (!(j%2)) outString.Append(NS_LITERAL_STRING(" "));      // even char
      else outString += PRUnichar(nbsp); // odd char
    }
  }  
  return NS_OK;
}


nsresult 
nsHTMLEditRules::PopListItem(nsIDOMNode *aListItem, PRBool *aOutOfList)
{
  // check parms
  if (!aListItem || !aOutOfList) 
    return NS_ERROR_NULL_POINTER;
  
  // init out params
  *aOutOfList = PR_FALSE;
  
  nsCOMPtr<nsIDOMNode> curParent;
  nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(aListItem));
  PRInt32 offset;
  nsresult res = nsEditor::GetNodeLocation(curNode, address_of(curParent), &offset);
  if (NS_FAILED(res)) return res;
    
  if (!nsHTMLEditUtils::IsListItem(curNode))
    return NS_ERROR_FAILURE;
    
  // if it's first or last list item, dont need to split the list
  // otherwise we do.
  nsCOMPtr<nsIDOMNode> curParPar;
  PRInt32 parOffset;
  res = nsEditor::GetNodeLocation(curParent, address_of(curParPar), &parOffset);
  if (NS_FAILED(res)) return res;
  
  PRBool bIsFirstListItem;
  res = mHTMLEditor->IsFirstEditableChild(curNode, &bIsFirstListItem);
  if (NS_FAILED(res)) return res;

  PRBool bIsLastListItem;
  res = mHTMLEditor->IsLastEditableChild(curNode, &bIsLastListItem);
  if (NS_FAILED(res)) return res;
    
  if (!bIsFirstListItem && !bIsLastListItem)
  {
    // split the list
    nsCOMPtr<nsIDOMNode> newBlock;
    res = mHTMLEditor->SplitNode(curParent, offset, getter_AddRefs(newBlock));
    if (NS_FAILED(res)) return res;
  }
  
  if (!bIsFirstListItem) parOffset++;
  
  res = mHTMLEditor->MoveNode(curNode, curParPar, parOffset);
  if (NS_FAILED(res)) return res;
    
  // unwrap list item contents if they are no longer in a list
  if (!nsHTMLEditUtils::IsList(curParPar)
      && nsHTMLEditUtils::IsListItem(curNode)) 
  {
    res = mHTMLEditor->RemoveBlockContainer(curNode);
    if (NS_FAILED(res)) return res;
    *aOutOfList = PR_TRUE;
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
      PRBool bOutOfList;
      do
      {
        res = PopListItem(child, &bOutOfList);
        if (NS_FAILED(res)) return res;
      } while (!bOutOfList);   // keep popping it out until it's not in a list anymore
    }
    else if (nsHTMLEditUtils::IsList(child))
    {
      res = RemoveListStructure(child);
      if (NS_FAILED(res)) return res;
    }
    else
    {
      // delete any non- list items for now
      res = mHTMLEditor->DeleteNode(child);
      if (NS_FAILED(res)) return res;
    }
    aList->GetFirstChild(getter_AddRefs(child));
  }
  // delete the now-empty list
  res = mHTMLEditor->RemoveBlockContainer(aList);
  if (NS_FAILED(res)) return res;

  return res;
}


nsresult 
nsHTMLEditRules::ConfirmSelectionInBody()
{
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMElement> bodyElement;   
  nsCOMPtr<nsIDOMNode> bodyNode; 
  
  // get the body  
  res = mHTMLEditor->GetRootElement(getter_AddRefs(bodyElement));
  if (NS_FAILED(res)) return res;
  if (!bodyElement) return NS_ERROR_UNEXPECTED;
  bodyNode = do_QueryInterface(bodyElement);

  // get the selection
  nsCOMPtr<nsISelection>selection;
  res = mHTMLEditor->GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  
  // get the selection start location
  nsCOMPtr<nsIDOMNode> selNode, temp, parent;
  PRInt32 selOffset;
  res = mHTMLEditor->GetStartNodeAndOffset(selection, address_of(selNode), &selOffset);
  if (NS_FAILED(res)) return res;
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
    selection->Collapse(bodyNode,0);
  }
  
  // get the selection end location
  res = mHTMLEditor->GetEndNodeAndOffset(selection, address_of(selNode), &selOffset);
  if (NS_FAILED(res)) return res;
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
    selection->Collapse(bodyNode,0);
  }
  
  return res;
}


nsresult 
nsHTMLEditRules::UpdateDocChangeRange(nsIDOMRange *aRange)
{
  nsresult res = NS_OK;
  
  if (!mDocChangeRange)
  {
    // clone aRange.  
    res = aRange->CloneRange(getter_AddRefs(mDocChangeRange));
    return res;
  }
  else
  {
    PRInt32 result;
    
    // compare starts of ranges
    res = mDocChangeRange->CompareBoundaryPoints(nsIDOMRange::START_TO_START, aRange, &result);
    if (NS_FAILED(res)) return res;
    if (result > 0)  // positive result means mDocChangeRange start is after aRange start
    {
      nsCOMPtr<nsIDOMNode> startNode;
      PRInt32 startOffset;
      res = aRange->GetStartContainer(getter_AddRefs(startNode));
      if (NS_FAILED(res)) return res;
      res = aRange->GetStartOffset(&startOffset);
      if (NS_FAILED(res)) return res;
      res = mDocChangeRange->SetStart(startNode, startOffset);
      if (NS_FAILED(res)) return res;
    }
    
    // compare ends of ranges
    res = mDocChangeRange->CompareBoundaryPoints(nsIDOMRange::END_TO_END, aRange, &result);
    if (NS_FAILED(res)) return res;
    if (result < 0)  // negative result means mDocChangeRange end is before aRange end
    {
      nsCOMPtr<nsIDOMNode> endNode;
      PRInt32 endOffset;
      res = aRange->GetEndContainer(getter_AddRefs(endNode));
      if (NS_FAILED(res)) return res;
      res = aRange->GetEndOffset(&endOffset);
      if (NS_FAILED(res)) return res;
      res = mDocChangeRange->SetEnd(endNode, endOffset);
      if (NS_FAILED(res)) return res;
    }
  }
  return res;
}

nsresult 
nsHTMLEditRules::InsertMozBRIfNeeded(nsIDOMNode *aNode)
{
  if (!aNode) return NS_ERROR_NULL_POINTER;
  if (!IsBlockNode(aNode)) return NS_OK;
  
  PRBool isEmpty;
  nsCOMPtr<nsIDOMNode> brNode;
  nsresult res = mHTMLEditor->IsEmptyNode(aNode, &isEmpty);
  if (NS_FAILED(res)) return res;
  if (isEmpty)
  {
    res = CreateMozBR(aNode, 0, address_of(brNode));
  }
  return res;
}

#ifdef XP_MAC
#pragma mark -
#pragma mark  nsIEditActionListener methods 
#pragma mark -
#endif

NS_IMETHODIMP 
nsHTMLEditRules::WillCreateNode(const nsAReadableString& aTag, nsIDOMNode *aParent, PRInt32 aPosition)
{
  return NS_OK;  
}

NS_IMETHODIMP 
nsHTMLEditRules::DidCreateNode(const nsAReadableString& aTag, 
                               nsIDOMNode *aNode, 
                               nsIDOMNode *aParent, 
                               PRInt32 aPosition, 
                               nsresult aResult)
{
  if (!mListenerEnabled) return NS_OK;
  // assumption that Join keeps the righthand node
  nsresult res = mUtilRange->SelectNode(aNode);
  if (NS_FAILED(res)) return res;
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
  if (!mListenerEnabled) return NS_OK;
  nsresult res = mUtilRange->SelectNode(aNode);
  if (NS_FAILED(res)) return res;
  res = UpdateDocChangeRange(mUtilRange);
  return res;  
}


NS_IMETHODIMP 
nsHTMLEditRules::WillDeleteNode(nsIDOMNode *aChild)
{
  if (!mListenerEnabled) return NS_OK;
  nsresult res = mUtilRange->SelectNode(aChild);
  if (NS_FAILED(res)) return res;
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
  if (!mListenerEnabled) return NS_OK;
  nsresult res = mUtilRange->SetStart(aNewLeftNode, 0);
  if (NS_FAILED(res)) return res;
  res = mUtilRange->SetEnd(aExistingRightNode, 0);
  if (NS_FAILED(res)) return res;
  res = UpdateDocChangeRange(mUtilRange);
  return res;  
}


NS_IMETHODIMP 
nsHTMLEditRules::WillJoinNodes(nsIDOMNode *aLeftNode, nsIDOMNode *aRightNode, nsIDOMNode *aParent)
{
  if (!mListenerEnabled) return NS_OK;
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
  if (!mListenerEnabled) return NS_OK;
  // assumption that Join keeps the righthand node
  nsresult res = mUtilRange->SetStart(aRightNode, mJoinOffset);
  if (NS_FAILED(res)) return res;
  res = mUtilRange->SetEnd(aRightNode, mJoinOffset);
  if (NS_FAILED(res)) return res;
  res = UpdateDocChangeRange(mUtilRange);
  return res;  
}


NS_IMETHODIMP 
nsHTMLEditRules::WillInsertText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, const nsAReadableString &aString)
{
  return NS_OK;  
}


NS_IMETHODIMP 
nsHTMLEditRules::DidInsertText(nsIDOMCharacterData *aTextNode, 
                                  PRInt32 aOffset, 
                                  const nsAReadableString &aString, 
                                  nsresult aResult)
{
  if (!mListenerEnabled) return NS_OK;
  PRInt32 length = aString.Length();
  nsCOMPtr<nsIDOMNode> theNode = do_QueryInterface(aTextNode);
  nsresult res = mUtilRange->SetStart(theNode, aOffset);
  if (NS_FAILED(res)) return res;
  res = mUtilRange->SetEnd(theNode, aOffset+length);
  if (NS_FAILED(res)) return res;
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
  if (!mListenerEnabled) return NS_OK;
  nsCOMPtr<nsIDOMNode> theNode = do_QueryInterface(aTextNode);
  nsresult res = mUtilRange->SetStart(theNode, aOffset);
  if (NS_FAILED(res)) return res;
  res = mUtilRange->SetEnd(theNode, aOffset);
  if (NS_FAILED(res)) return res;
  res = UpdateDocChangeRange(mUtilRange);
  return res;  
}

NS_IMETHODIMP
nsHTMLEditRules::WillDeleteRange(nsIDOMRange *aRange)
{
  if (!mListenerEnabled) return NS_OK;
  // get the (collapsed) selection location
  return UpdateDocChangeRange(aRange);
}

NS_IMETHODIMP
nsHTMLEditRules::DidDeleteRange(nsIDOMRange *aRange)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditRules::WillDeleteSelection(nsISelection *aSelection)
{
  if (!mListenerEnabled) return NS_OK;
  // get the (collapsed) selection location
  nsCOMPtr<nsIDOMNode> selNode;
  PRInt32 selOffset;

  nsresult res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(selNode), &selOffset);
  if (NS_FAILED(res)) return res;
  res = mUtilRange->SetStart(selNode, selOffset);
  if (NS_FAILED(res)) return res;
  res = mHTMLEditor->GetEndNodeAndOffset(aSelection, address_of(selNode), &selOffset);
  if (NS_FAILED(res)) return res;
  res = mUtilRange->SetEnd(selNode, selOffset);
  if (NS_FAILED(res)) return res;
  res = UpdateDocChangeRange(mUtilRange);
  return res;  
}

NS_IMETHODIMP
nsHTMLEditRules::DidDeleteSelection(nsISelection *aSelection)
{
  return NS_OK;
}
