/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsHTMLEditRules.h"

#include "nsEditor.h"
#include "nsHTMLEditUtils.h"
#include "nsHTMLEditor.h"

#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsIDOMNode.h"
#include "nsIDOMText.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMSelection.h"
#include "nsIDOMRange.h"
#include "nsIDOMCharacterData.h"
#include "nsIEnumerator.h"
#include "nsIStyleContext.h"
#include "nsIPresShell.h"
#include "nsLayoutCID.h"
#include "nsIPref.h"

#include "nsEditorUtils.h"

#include "InsertTextTxn.h"
#include "DeleteTextTxn.h"

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
 
 
class nsTrivialFunctor : public nsBoolDomIterFunctor
{
  public:
    virtual PRBool operator()(nsIDOMNode* aNode)  // used to build list of all nodes iterator covers
    {
      return PR_TRUE;
    }
};

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
      if (nsHTMLEditUtils::IsBreak(aNode)) return PR_TRUE;
      return PR_FALSE;
    }
};

class nsEmptyFunctor : public nsBoolDomIterFunctor
{
  public:
    nsEmptyFunctor(nsHTMLEditor* editor) : mEditor(editor) {}
    virtual PRBool operator()(nsIDOMNode* aNode)  // used to build list of empty li's and td's
    {
      PRBool bIsEmptyNode;
      nsresult res = mEditor->IsEmptyNode(aNode, &bIsEmptyNode, PR_FALSE, PR_FALSE);
      if (NS_FAILED(res)) return PR_FALSE;
      if (bIsEmptyNode
        && (nsHTMLEditUtils::IsListItem(aNode) || nsHTMLEditUtils::IsTableCellOrCaption(aNode)))
      {
        return PR_TRUE;
      }
      return PR_FALSE;
    }
  protected:
    nsHTMLEditor* mEditor;
};

class nsEditableTextFunctor : public nsBoolDomIterFunctor
{
  public:
    nsEditableTextFunctor(nsHTMLEditor* editor) : mEditor(editor) {}
    virtual PRBool operator()(nsIDOMNode* aNode)  // used to build list of empty li's and td's
    {
      if (nsEditor::IsTextNode(aNode) && mEditor->IsEditable(aNode)) 
      {
        return PR_TRUE;
      }
      return PR_FALSE;
    }
  protected:
    nsHTMLEditor* mEditor;
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
  mEditor->RemoveEditActionListener(this);
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
nsHTMLEditRules::Init(nsHTMLEditor *aEditor, PRUint32 aFlags)
{
  // call through to base class Init first
  nsresult res = nsTextEditRules::Init(aEditor, aFlags);
  if (NS_FAILED(res)) return res;

  // cache any prefs we care about
  NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &res);
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
  res = nsComponentManager::CreateInstance(kRangeCID, nsnull, NS_GET_IID(nsIDOMRange),
                                                    getter_AddRefs(mUtilRange));
  if (NS_FAILED(res)) return res;
  
  // pass over document and add any needed mozBRs
  // first turn off undo
  mEditor->EnableUndo(PR_FALSE);
  
  // set up mDocChangeRange to be whole doc
  nsCOMPtr<nsIDOMElement> bodyElem;
  nsCOMPtr<nsIDOMNode> bodyNode;
  mEditor->GetRootElement(getter_AddRefs(bodyElem));
  bodyNode = do_QueryInterface(bodyElem);
  if (bodyNode)
  {
    // temporarily turn off rules sniffing
    nsAutoLockRulesSniffing lockIt((nsTextEditRules*)this);
    res = nsComponentManager::CreateInstance(kRangeCID, nsnull, NS_GET_IID(nsIDOMRange),
                                                    getter_AddRefs(mDocChangeRange));
    if (NS_FAILED(res)) return res;
    if (!mDocChangeRange) return NS_ERROR_NULL_POINTER;
    mDocChangeRange->SelectNode(bodyNode);
    res = ReplaceNewlines(mDocChangeRange);
    if (NS_FAILED(res)) return res;
    res = AdjustSpecialBreaks();
    if (NS_FAILED(res)) return res;
  }
  
  // turn on undo
  mEditor->EnableUndo(PR_TRUE);
  
  // add ourselves as a listener to edit actions
  res = mEditor->AddEditActionListener(this);

  return res;
}


NS_IMETHODIMP
nsHTMLEditRules::BeforeEdit(PRInt32 action, nsIEditor::EDirection aDirection)
{
  if (mLockRulesSniffing) return NS_OK;
  
  nsAutoLockRulesSniffing lockIt((nsTextEditRules*)this);
  
  if (!mActionNesting)
  {
    mDocChangeRange = nsnull;  // clear out our accounting of what changed
    // total hack to clear mUtilRange.  This is because there is the
    // unposition call is not available thru nsIDOMRange.idl
    nsComponentManager::CreateInstance(kRangeCID, nsnull, NS_GET_IID(nsIDOMRange),
                                                    getter_AddRefs(mUtilRange));
    // turn off caret
    nsCOMPtr<nsISelectionController> selCon;
    mEditor->GetSelectionController(getter_AddRefs(selCon));
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
    mEditor->GetSelectionController(getter_AddRefs(selCon));
    if (selCon) selCon->SetCaretEnabled(PR_TRUE);
  }

  return res;
}


nsresult
nsHTMLEditRules::AfterEditInner(PRInt32 action, nsIEditor::EDirection aDirection)
{
  ConfirmSelectionInBody();
  if (action == nsEditor::kOpIgnore) return NS_OK;
  
  nsCOMPtr<nsIDOMSelection>selection;
  nsresult res = mEditor->GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  
  if (mDocChangeRange && !((action == nsEditor::kOpUndo) || (action == nsEditor::kOpRedo)))
  {
    // dont let any txns in here move the selection around behind our back.
    // Note that this won't prevent explicit selection setting from working.
    nsAutoTxnsConserveSelection dontSpazMySelection(mEditor);
   
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
      res = mEditor->CollapseAdjacentTextNodes(mDocChangeRange);
      if (NS_FAILED(res)) return res;
    }
    
    // adjust whitespace for insert text and delete actions
    if ((action == nsEditor::kOpInsertText) || 
        (action == nsEditor::kOpInsertIMEText) ||
        (action == nsEditor::kOpDeleteSelection))
    {
      res = AdjustWhitespace(selection);
      if (NS_FAILED(res)) return res;
    }
    
    // replace newlines that are preformatted
    // MOOSE:  This is buttUgly.  A better way to 
    // organize the action enum is in order.
    if ((action == nsEditor::kOpInsertText) || 
        (action == nsEditor::kOpInsertIMEText) ||
        (action == nsHTMLEditor::kOpInsertElement) ||
        (action == nsHTMLEditor::kOpInsertQuotation) ||
        (action == nsEditor::kOpInsertNode))
    {
      res = ReplaceNewlines(mDocChangeRange);
    }
    
    // clean up any empty nodes in the selection
    res = RemoveEmptyNodes();
    if (NS_FAILED(res)) return res;
    
    // adjust selection for insert text and delete actions
    if ((action == nsEditor::kOpInsertText) || 
        (action == nsEditor::kOpInsertIMEText) ||
        (action == nsEditor::kOpDeleteSelection))
    {
      res = AdjustSelection(selection, aDirection);
      if (NS_FAILED(res)) return res;
    }
  }

  // detect empty doc
  res = CreateBogusNodeIfNeeded(selection);
  return res;
}


NS_IMETHODIMP 
nsHTMLEditRules::WillDoAction(nsIDOMSelection *aSelection, 
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
      return WillMakeList(aSelection, info->blockType, aCancel, aHandled);
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
      return WillMakeDefListItem(aSelection, info->blockType, aCancel, aHandled);
    case kInsertElement:
      return WillInsert(aSelection, aCancel);
  }
  return nsTextEditRules::WillDoAction(aSelection, aInfo, aCancel, aHandled);
}
  
  
NS_IMETHODIMP 
nsHTMLEditRules::DidDoAction(nsIDOMSelection *aSelection,
                             nsRulesInfo *aInfo, nsresult aResult)
{
  nsTextRulesInfo *info = NS_STATIC_CAST(nsTextRulesInfo*, aInfo);
  switch (info->action)
  {
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
nsHTMLEditRules::GetListState(PRBool &aMixed, PRBool &aOL, PRBool &aUL, PRBool &aDL)
{
  aMixed = PR_FALSE;
  aOL = PR_FALSE;
  aUL = PR_FALSE;
  aDL = PR_FALSE;
  PRBool bNonList = PR_FALSE;
  
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  nsresult res = GetListActionNodes(&arrayOfNodes, PR_TRUE);
  if (NS_FAILED(res)) return res;

  // examine list type for nodes in selection
  PRUint32 listCount;
  PRInt32 i;
  arrayOfNodes->Count(&listCount);
  for (i=(PRInt32)listCount-1; i>=0; i--)
  {
    nsCOMPtr<nsISupports> isupports = (dont_AddRef)(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports) );
    
    if (mEditor->NodeIsType(curNode,nsIEditProperty::ul))
      aUL = PR_TRUE;
    else if (mEditor->NodeIsType(curNode,nsIEditProperty::ol))
      aOL = PR_TRUE;
    else if (mEditor->NodeIsType(curNode,nsIEditProperty::li))
    {
      nsCOMPtr<nsIDOMNode> parent;
      PRInt32 offset;
      res = nsEditor::GetNodeLocation(curNode, &parent, &offset);
      if (NS_FAILED(res)) return res;
      if (nsHTMLEditUtils::IsUnorderedList(parent))
        aUL = PR_TRUE;
      else if (nsHTMLEditUtils::IsOrderedList(parent))
        aOL = PR_TRUE;
    }
    else if (mEditor->NodeIsType(curNode,nsIEditProperty::dl) ||
             mEditor->NodeIsType(curNode,nsIEditProperty::dt) ||
             mEditor->NodeIsType(curNode,nsIEditProperty::dd) )
    {
      aDL = PR_TRUE;
    }
    else bNonList = PR_TRUE;
  }  
  
  // hokey arithmetic with booleans
  if ( (aUL + aOL + aDL + bNonList) > 1) aMixed = PR_TRUE;
  
  return res;
}

NS_IMETHODIMP 
nsHTMLEditRules::GetListItemState(PRBool &aMixed, PRBool &aLI, PRBool &aDT, PRBool &aDD)
{
  aMixed = PR_FALSE;
  aLI = PR_FALSE;
  aDT = PR_FALSE;
  aDD = PR_FALSE;
  PRBool bNonList = PR_FALSE;
  
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  nsresult res = GetListActionNodes(&arrayOfNodes, PR_TRUE);
  if (NS_FAILED(res)) return res;

  // examine list type for nodes in selection
  PRUint32 listCount;
  PRInt32 i;
  arrayOfNodes->Count(&listCount);
  for (i=(PRInt32)listCount-1; i>=0; i--)
  {
    nsCOMPtr<nsISupports> isupports = (dont_AddRef)(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports) );
    
    if (mEditor->NodeIsType(curNode,nsIEditProperty::ul) ||
        mEditor->NodeIsType(curNode,nsIEditProperty::ol) ||
        mEditor->NodeIsType(curNode,nsIEditProperty::li) )
    {
      aLI = PR_TRUE;
    }
    else if (mEditor->NodeIsType(curNode,nsIEditProperty::dt))
    {
      aDT = PR_TRUE;
    }
    else if (mEditor->NodeIsType(curNode,nsIEditProperty::dd))
    {
      aDD = PR_TRUE;
    }
    else if (mEditor->NodeIsType(curNode,nsIEditProperty::dl))
    {
      // need to look inside dl and see which types of items it has
      PRBool bDT, bDD;
      res = GetDefinitionListItemTypes(curNode, bDT, bDD);
      if (NS_FAILED(res)) return res;
      aDT |= bDT;
      aDD |= bDD;
    }
    else bNonList = PR_TRUE;
  }  
  
  // hokey arithmetic with booleans
  if ( (aDT + aDD + bNonList) > 1) aMixed = PR_TRUE;
  
  return res;
}

NS_IMETHODIMP 
nsHTMLEditRules::GetAlignment(PRBool &aMixed, nsIHTMLEditor::EAlignment &aAlign)
{
  // for now, just return first alignment.  we'll lie about
  // if it's mixed.  This is for efficiency, given that our
  // current ui doesn't care if it's mixed.

  // this routine assumes that alignment is done ONLY via divs
  
  // default alignment is left
  aMixed = PR_FALSE;
  aAlign = nsIHTMLEditor::eLeft;
  
  // get selection
  nsCOMPtr<nsIDOMSelection>selection;
  nsresult res = mEditor->GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;

  // get selection location
  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset;
  res = mEditor->GetStartNodeAndOffset(selection, &parent, &offset);
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
  else if (mEditor->IsTextNode(parent)) 
  {
    // if we are in a text node, then that is the node of interest
    nodeToExamine = parent;
  }
  else
  {
    // otherwise we want to look at the first editable node after
    // {parent,offset} and it's ancestors for divs with alignment on them
    mEditor->GetNextNode(parent, offset, PR_TRUE, getter_AddRefs(nodeToExamine));
  }
  
  if (!nodeToExamine) return NS_ERROR_NULL_POINTER;
  
  // check up the ladder for divs with alignment
  nsCOMPtr<nsIDOMNode> temp = nodeToExamine;
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
        nsresult res = elem->GetAttribute(typeAttrName, typeAttrVal);
        typeAttrVal.ToLowerCase();
        if (NS_SUCCEEDED(res) && typeAttrVal.Length())
        {
          if (typeAttrVal.EqualsWithConversion("center"))
            aAlign = nsIHTMLEditor::eCenter;
          else if (typeAttrVal.EqualsWithConversion("right"))
            aAlign = nsIHTMLEditor::eRight;
          else if (typeAttrVal.EqualsWithConversion("justify"))
            aAlign = nsIHTMLEditor::eJustify;
          else
            aAlign = nsIHTMLEditor::eLeft;
          return res;
        }
      }
    }
    res = nodeToExamine->GetParentNode(getter_AddRefs(temp));
    if (NS_FAILED(res)) temp = nsnull;
    nodeToExamine = temp; 
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLEditRules::GetIndentState(PRBool &aCanIndent, PRBool &aCanOutdent)
{
  aCanIndent = PR_TRUE;    
  aCanOutdent = PR_FALSE;
  
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  nsresult res = GetListActionNodes(&arrayOfNodes, PR_TRUE);
  if (NS_FAILED(res)) return res;

  // examine nodes in selection for blockquotes or list elements;
  // these we can outdent.  Note that we return true for canOutdent
  // if *any* of the selection is outdentable, rather than all of it.
  PRUint32 listCount;
  PRInt32 i;
  arrayOfNodes->Count(&listCount);
  for (i=(PRInt32)listCount-1; i>=0; i--)
  {
    nsCOMPtr<nsISupports> isupports = (dont_AddRef)(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports) );
    
    if (nsHTMLEditUtils::IsList(curNode)     || 
        nsHTMLEditUtils::IsListItem(curNode) ||
        nsHTMLEditUtils::IsBlockquote(curNode))
    {
      aCanOutdent = PR_TRUE;
      break;
    }
  }  
  
  return res;
}


NS_IMETHODIMP 
nsHTMLEditRules::GetParagraphState(PRBool &aMixed, nsString &outFormat)
{
  // This routine is *heavily* tied to our ui choices in the paragraph
  // style popup.  I cant see a way around that.
  aMixed = PR_TRUE;
  outFormat.AssignWithConversion("");
  
  PRBool bMixed = PR_FALSE;
  nsAutoString formatStr; 
  // using "x" as an uninitialized value, since "" is meaningful
  formatStr.AssignWithConversion("x");
  
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  nsresult res = GetParagraphFormatNodes(&arrayOfNodes, PR_TRUE);
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
    nsCOMPtr<nsIDOMSelection>selection;
    nsresult res = mEditor->GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(res)) return res;
    res = mEditor->GetStartNodeAndOffset(selection, &selNode, &selOffset);
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
    isupports = (dont_AddRef)(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports) );

    nsAutoString format;
    nsCOMPtr<nsIAtom> atom = mEditor->GetTag(curNode);
    
    if (mEditor->IsInlineNode(curNode))
    {
      nsCOMPtr<nsIDOMNode> block = mEditor->GetBlockNodeParent(curNode);
      if (block)
      {
        nsCOMPtr<nsIAtom> blockAtom = mEditor->GetTag(block);
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
          format = tag;
        }
        else
        {
          format.AssignWithConversion("");
        }
      }
      else
      {
        format.AssignWithConversion("");
      }  
    }    
    else if (nsHTMLEditUtils::IsHeader(curNode))
    {
      nsAutoString tag;
      nsEditor::GetTagString(curNode,tag);
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
  
  aMixed = bMixed;
  outFormat = formatStr;
  return res;
}


/********************************************************
 *  Protected rules methods 
 ********************************************************/

nsresult
nsHTMLEditRules::WillInsert(nsIDOMSelection *aSelection, PRBool *aCancel)
{
  nsresult res = nsTextEditRules::WillInsert(aSelection, aCancel);
  if (NS_FAILED(res)) return res; 
  
  // we need to get the doc
  nsCOMPtr<nsIDOMDocument>doc;
  res = mEditor->GetDocument(getter_AddRefs(doc));
  if (NS_FAILED(res)) return res;
  if (!doc) return NS_ERROR_NULL_POINTER;
    
  // for every property that is set, insert a new inline style node
  res = CreateStyleForInsertText(aSelection, doc);
  return res; 
}    

nsresult
nsHTMLEditRules::DidInsert(nsIDOMSelection *aSelection, nsresult aResult)
{
  return nsTextEditRules::DidInsert(aSelection, aResult);
}

nsresult
nsHTMLEditRules::WillInsertText(PRInt32          aAction,
                                nsIDOMSelection *aSelection, 
                                PRBool          *aCancel,
                                PRBool          *aHandled,
                                const nsString  *inString,
                                nsString        *outString,
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

  // if the selection isn't collapsed, delete it.
  PRBool bCollapsed;
  res = aSelection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  if (!bCollapsed)
  {
    res = mEditor->DeleteSelection(nsIEditor::eNone);
    if (NS_FAILED(res)) return res;
  }

  res = WillInsert(aSelection, aCancel);
  if (NS_FAILED(res)) return res;
  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = PR_FALSE;
  
  // get the (collapsed) selection location
  res = mEditor->GetStartNodeAndOffset(aSelection, &selNode, &selOffset);
  if (NS_FAILED(res)) return res;

  // dont put text in places that cant have it
  nsAutoString textTag; textTag.AssignWithConversion("__moz_text");
  if (!mEditor->IsTextNode(selNode) && !mEditor->CanContainTag(selNode, textTag))
    return NS_ERROR_FAILURE;

  // we need to get the doc
  nsCOMPtr<nsIDOMDocument>doc;
  res = mEditor->GetDocument(getter_AddRefs(doc));
  if (NS_FAILED(res)) return res;
  if (!doc) return NS_ERROR_NULL_POINTER;
    
  if (aAction == kInsertTextIME) 
  { 
    res = mEditor->InsertTextImpl(*inString, &selNode, &selOffset, doc);
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
    res = mEditor->IsPreformatted(selNode, &isPRE);
    if (NS_FAILED(res)) return res;    
    
    // turn off the edit listener: we know how to
    // build the "doc changed range" ourselves, and it's
    // must faster to do it once here than to track all
    // the changes one at a time.
    nsAutoLockListener lockit(&mListenerEnabled); 
    
    // dont spaz my selection in subtransactions
    nsAutoTxnsConserveSelection dontSpazMySelection(mEditor);
    nsSubsumeStr subStr;
    const PRUnichar *unicodeBuf = inString->GetUnicode();
    nsCOMPtr<nsIDOMNode> unused;
    PRInt32 pos = 0;
        
    // for efficiency, break out the pre case seperately.  This is because
    // its a lot cheaper to search the input string for only newlines than
    // it is to search for both tabs and newlines.
    if (isPRE)
    {
      char newlineChar = '\n';
      while (unicodeBuf && (pos != -1) && (pos < (PRInt32)inString->Length()))
      {
        PRInt32 oldPos = pos;
        PRInt32 subStrLen;
        pos = inString->FindChar(newlineChar, PR_FALSE, oldPos);
        
        if (pos != -1) 
        {
          subStrLen = pos - oldPos;
          // if first char is newline, then use just it
          if (subStrLen == 0)
            subStrLen = 1;
        }
        else
        {
          subStrLen = inString->Length() - oldPos;
          pos = inString->Length();
        }

        subStr.Subsume((PRUnichar*)&unicodeBuf[oldPos], PR_FALSE, subStrLen);
        
        // is it a return?
        if (subStr.EqualsWithConversion("\n"))
        {
          res = mEditor->CreateBRImpl(&curNode, &curOffset, &unused, nsIEditor::eNone);
          pos++;
        }
        else
        {
          res = mEditor->InsertTextImpl(subStr, &curNode, &curOffset, doc);
        }
        if (NS_FAILED(res)) return res;
      }
    }
    else
    {
      char specialChars[] = {'\t','\n',0};
      nsAutoString tabString; tabString.AssignWithConversion("    ");
      while (unicodeBuf && (pos != -1) && (pos < (PRInt32)inString->Length()))
      {
        PRInt32 oldPos = pos;
        PRInt32 subStrLen;
        pos = inString->FindCharInSet(specialChars, oldPos);
        
        if (pos != -1) 
        {
          subStrLen = pos - oldPos;
          // if first char is newline, then use just it
          if (subStrLen == 0)
            subStrLen = 1;
        }
        else
        {
          subStrLen = inString->Length() - oldPos;
          pos = inString->Length();
        }

        subStr.Subsume((PRUnichar*)&unicodeBuf[oldPos], PR_FALSE, subStrLen);
        
        // is it a tab?
        if (subStr.EqualsWithConversion("\t"))
        {
          res = mEditor->InsertTextImpl(tabString, &curNode, &curOffset, doc);
          pos++;
        }
        // is it a return?
        else if (subStr.EqualsWithConversion("\n"))
        {
          res = mEditor->CreateBRImpl(&curNode, &curOffset, &unused, nsIEditor::eNone);
          pos++;
        }
        else
        {
          res = mEditor->InsertTextImpl(subStr, &curNode, &curOffset, doc);
        }
        if (NS_FAILED(res)) return res;
      }
    }
    if (curNode) aSelection->Collapse(curNode, curOffset);
    // manually update the doc changed range so that AfterEdit will clean up
    // the correct portion of the document.
    if (!mDocChangeRange)
    {
      res = nsComponentManager::CreateInstance(kRangeCID, nsnull, NS_GET_IID(nsIDOMRange),
                                                    getter_AddRefs(mDocChangeRange));
      if (NS_FAILED(res)) return res;
    		if (!mDocChangeRange) return NS_ERROR_NULL_POINTER;
    }
    res = mDocChangeRange->SetStart(selNode, selOffset);
    if (NS_FAILED(res)) return res;
    res = mDocChangeRange->SetEnd(curNode, curOffset);
    if (NS_FAILED(res)) return res;
  }
  return res;
}

nsresult
nsHTMLEditRules::WillInsertBreak(nsIDOMSelection *aSelection, PRBool *aCancel, PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;
  

  // if the selection isn't collapsed, delete it.
  PRBool bCollapsed;
  nsresult res = aSelection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  if (!bCollapsed)
  {
    res = mEditor->DeleteSelection(nsIEditor::eNone);
    if (NS_FAILED(res)) return res;
  }
  
  res = WillInsert(aSelection, aCancel);
  if (NS_FAILED(res)) return res;
  
  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = PR_FALSE;


  // split any mailcites in the way
  if (mFlags & nsIHTMLEditor::eEditorMailMask)
  {
    nsCOMPtr<nsIDOMNode> citeNode, selNode;
    PRInt32 selOffset, newOffset;
    res = mEditor->GetStartNodeAndOffset(aSelection, &selNode, &selOffset);
    if (NS_FAILED(res)) return res;
    res = GetTopEnclosingMailCite(selNode, &citeNode);
    if (NS_FAILED(res)) return res;
    
    if (citeNode)
    {
      nsCOMPtr<nsIDOMNode> brNode;
      res = mEditor->SplitNodeDeep(citeNode, selNode, selOffset, &newOffset);
      if (NS_FAILED(res)) return res;
      res = citeNode->GetParentNode(getter_AddRefs(selNode));
      if (NS_FAILED(res)) return res;
      res = mEditor->CreateBR(selNode, newOffset, &brNode);
      if (NS_FAILED(res)) return res;
      // want selection before the break, and on same line
      aSelection->SetHint(PR_TRUE);
      res = aSelection->Collapse(selNode, newOffset);
      if (NS_FAILED(res)) return res;
      *aHandled = PR_TRUE;
      return NS_OK;
    }
  }

  // smart splitting rules
  nsCOMPtr<nsIDOMNode> node;
  PRInt32 offset;
  
  res = mEditor->GetStartNodeAndOffset(aSelection, &node, &offset);
  if (NS_FAILED(res)) return res;
  if (!node) return NS_ERROR_FAILURE;
    
  // identify the block
  nsCOMPtr<nsIDOMNode> blockParent;
  
  if (nsEditor::IsBlockNode(node)) 
    blockParent = node;
  else 
    blockParent = mEditor->GetBlockNodeParent(node);
    
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
    res = mEditor->CreateBR(node, offset, &brNode);  
    if (NS_FAILED(res)) return res;
    res = nsEditor::GetNodeLocation(brNode, &node, &offset);
    if (NS_FAILED(res)) return res;
    // SetHint(PR_TRUE) means we want the caret to stick to the content on the "right".
    // We want the caret to stick to whatever is past the break.  This is
    // because the break is on the same line we were on, but the next content
    // will be on the following line.
    
    // An exception to this is if the break has a next sibling that is a block node.
    // Then we stick to the left to aviod an uber caret.
    nsCOMPtr<nsIDOMNode> siblingNode;
    brNode->GetNextSibling(getter_AddRefs(siblingNode));
    if (siblingNode && mEditor->IsBlockNode(siblingNode))
      aSelection->SetHint(PR_FALSE);
    else 
      aSelection->SetHint(PR_TRUE);
    res = aSelection->Collapse(node, offset+1);
    if (NS_FAILED(res)) return res;
    *aHandled = PR_TRUE;
  }
  
  return res;
}



nsresult
nsHTMLEditRules::WillDeleteSelection(nsIDOMSelection *aSelection, 
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
  
  PRBool bCollapsed;
  res = aSelection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  
  nsCOMPtr<nsIDOMNode> node, selNode;
  PRInt32 offset, selOffset;
  
  res = mEditor->GetStartNodeAndOffset(aSelection, &node, &offset);
  if (NS_FAILED(res)) return res;
  if (!node) return NS_ERROR_FAILURE;
    
  if (bCollapsed)
  {
    // easy case, in a text node:
    if (mEditor->IsTextNode(node))
    {
      nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(node);
      PRUint32 strLength;
      res = textNode->GetLength(&strLength);
      if (NS_FAILED(res)) return res;
    
      // at beginning of text node and backspaced?
      if (!offset && (aAction == nsIEditor::ePrevious))
      {
        nsCOMPtr<nsIDOMNode> priorNode;
        res = mEditor->GetPriorHTMLNode(node, &priorNode);
        if (NS_FAILED(res)) return res;
        
        // if there is no prior node then cancel the deletion
        if (!priorNode)
        {
          *aCancel = PR_TRUE;
          return res;
        }
        
        // block parents the same?  
        if (mEditor->HasSameBlockNodeParent(node, priorNode)) 
        {
          // is prior node not a container?  (ie, a br, hr, image...)
          if (!mEditor->IsContainer(priorNode))   // MOOSE: anchors not handled
          {
            // delete the break, and join like nodes if appropriate
            res = mEditor->DeleteNode(priorNode);
            if (NS_FAILED(res)) return res;
            // we did something, so lets say so.
            *aHandled = PR_TRUE;
            // get new prior node
            res = mEditor->GetPriorHTMLNode(node, &priorNode);
            if (NS_FAILED(res)) return res;
            // are they in same block?
            if (mEditor->HasSameBlockNodeParent(node, priorNode)) 
            {
              // are they same type?
              if (mEditor->IsTextNode(priorNode))
              {
                // if so, join them!
                nsCOMPtr<nsIDOMNode> topParent;
                priorNode->GetParentNode(getter_AddRefs(topParent));
                res = JoinNodesSmart(priorNode,node,&selNode,&selOffset);
                if (NS_FAILED(res)) return res;
                // fix up selection
                res = aSelection->Collapse(selNode,selOffset);
              }
            }
            return res;
          }
          // is prior node a text node?
          else if ( mEditor->IsTextNode(priorNode) )
          {
            // delete last character
            nsCOMPtr<nsIDOMCharacterData>nodeAsText;
            nodeAsText = do_QueryInterface(priorNode);
            nodeAsText->GetLength((PRUint32*)&offset);
            res = aSelection->Collapse(priorNode,offset);
            // just return without setting handled to true.
            // default code will take care of actual deletion
            return res;
          }
          else if ( mEditor->IsInlineNode(priorNode) )
          {
            // remember where we are
            res = mEditor->GetNodeLocation(priorNode, &node, &offset);
            if (NS_FAILED(res)) return res;
            // delete it
            res = mEditor->DeleteNode(priorNode);
            if (NS_FAILED(res)) return res;
            // we did something, so lets say so.
            *aHandled = PR_TRUE;
            // fix up selection
            res = aSelection->Collapse(node,offset);
            return res;
          }
          else return NS_OK; // punt to default
        }
        
        // deleting across blocks
        nsCOMPtr<nsIDOMNode> leftParent = mEditor->GetBlockNodeParent(priorNode);
        nsCOMPtr<nsIDOMNode> rightParent = mEditor->GetBlockNodeParent(node);
        
        // if leftParent or rightParent is null, it's because the
        // corresponding selection endpoint is in the body node.
        if (!leftParent || !rightParent)
          return NS_OK;  // bail to default
          
        // do not delete across table structures
        if (nsHTMLEditUtils::IsTableElement(leftParent) || nsHTMLEditUtils::IsTableElement(rightParent))
        {
          *aCancel = PR_TRUE;
          return NS_OK;
        }
                
        // are the blocks of same type?
        if (mEditor->NodesSameType(leftParent, rightParent))
        {
          nsCOMPtr<nsIDOMNode> topParent;
          leftParent->GetParentNode(getter_AddRefs(topParent));
          
          *aHandled = PR_TRUE;
          res = JoinNodesSmart(leftParent,rightParent,&selNode,&selOffset);
          if (NS_FAILED(res)) return res;
          // fix up selection
          res = aSelection->Collapse(selNode,selOffset);
          return res;
        }
        
        // else blocks not same type, bail to default
        return NS_OK;
        
      }
    
      // at end of text node and deleted?
      if ((offset == (PRInt32)strLength)
          && (aAction == nsIEditor::eNext))
      {
        nsCOMPtr<nsIDOMNode> nextNode;
        res = mEditor->GetNextHTMLNode(node, &nextNode);
        if (NS_FAILED(res)) return res;
         
        // if there is no next node, or it's not in the body, then cancel the deletion
        if (!nextNode || !nsHTMLEditUtils::InBody(nextNode, mEditor))
        {
          *aCancel = PR_TRUE;
          return res;
        }
       
        // block parents the same?  
        if (mEditor->HasSameBlockNodeParent(node, nextNode)) 
        {
          // is next node not a container?  (ie, a br, hr, image...)
          if (!mEditor->IsContainer(nextNode))  // MOOSE: anchors not handled
          {
            // delete the break, and join like nodes if appropriate
            res = mEditor->DeleteNode(nextNode);
            if (NS_FAILED(res)) return res;
            // we did something, so lets say so.
            *aHandled = PR_TRUE;
            // get new next node
            res = mEditor->GetNextHTMLNode(node, &nextNode);
            if (NS_FAILED(res)) return res;
            // are they in same block?
            if (mEditor->HasSameBlockNodeParent(node, nextNode)) 
            {
              // are they same type?
              if ( mEditor->IsTextNode(nextNode) )
              {
                // if so, join them!
                nsCOMPtr<nsIDOMNode> topParent;
                nextNode->GetParentNode(getter_AddRefs(topParent));
                res = JoinNodesSmart(node,nextNode,&selNode,&selOffset);
                if (NS_FAILED(res)) return res;
                // fix up selection
                res = aSelection->Collapse(selNode,selOffset);
              }
            }
            return res;
          }
          // is next node a text node?
          else if ( mEditor->IsTextNode(nextNode) )
          {
            // delete first character
            nsCOMPtr<nsIDOMCharacterData>nodeAsText;
            nodeAsText = do_QueryInterface(nextNode);
            res = aSelection->Collapse(nextNode,0);
            // just return without setting handled to true.
            // default code will take care of actual deletion
            return res;
          }
          else if ( mEditor->IsInlineNode(nextNode) )
          {
            // remember where we are
            res = mEditor->GetNodeLocation(nextNode, &node, &offset);
            if (NS_FAILED(res)) return res;
            // delete it
            res = mEditor->DeleteNode(nextNode);
            if (NS_FAILED(res)) return res;
            // we did something, so lets say so.
            *aHandled = PR_TRUE;
            // fix up selection
            res = aSelection->Collapse(node,offset);
            return res;
          }
          else return NS_OK; // punt to default
        }
                
        // deleting across blocks
        nsCOMPtr<nsIDOMNode> leftParent = mEditor->GetBlockNodeParent(node);
        nsCOMPtr<nsIDOMNode> rightParent = mEditor->GetBlockNodeParent(nextNode);

        // if leftParent or rightParent is null, it's because the
        // corresponding selection endpoint is in the body node.
        if (!leftParent || !rightParent)
          return NS_OK;  // bail to default
          
        // do not delete across table structures
        if (nsHTMLEditUtils::IsTableElement(leftParent) || nsHTMLEditUtils::IsTableElement(rightParent))
        {
          *aCancel = PR_TRUE;
          return NS_OK;
        }
                
        // are the blocks of same type?
        if (mEditor->NodesSameType(leftParent, rightParent))
        {
          nsCOMPtr<nsIDOMNode> topParent;
          leftParent->GetParentNode(getter_AddRefs(topParent));
          
          *aHandled = PR_TRUE;
          res = JoinNodesSmart(leftParent,rightParent,&selNode,&selOffset);
          if (NS_FAILED(res)) return res;
          // fix up selection
          res = aSelection->Collapse(selNode,selOffset);
          return res;
        }
        
        // else blocks not same type, bail to default
        return NS_OK;
        
      }
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
      res = mEditor->IsEmptyNode(node, &bIsEmptyNode, PR_TRUE, PR_FALSE);
      if (bIsEmptyNode && !nsHTMLEditUtils::IsTableElement(node))
        nodeToDelete = node;
      else 
      {
        // see if we are on empty lineand need to delete it.  This is true
        // when a break is after a block and we are deleting backwards; or
        // when a break is before a block and we are delting forwards.  In
        // these cases, we want to delete the break when we are between it
        // and the block element, even though the break is on the "wrong"
        // side of us.
        nsCOMPtr<nsIDOMNode> maybeBreak;
        nsCOMPtr<nsIDOMNode> maybeBlock;
        
        if (aAction == nsIEditor::ePrevious)
        {
          res = mEditor->GetPriorHTMLSibling(node, offset, &maybeBlock);
          if (NS_FAILED(res)) return res;
          res = mEditor->GetNextHTMLSibling(node, offset, &maybeBreak);
          if (NS_FAILED(res)) return res;
        }
        else if (aAction == nsIEditor::eNext)
        {
          res = mEditor->GetPriorHTMLSibling(node, offset, &maybeBreak);
          if (NS_FAILED(res)) return res;
          res = mEditor->GetNextHTMLSibling(node, offset, &maybeBlock);
          if (NS_FAILED(res)) return res;
        }
        
        if (maybeBreak && maybeBlock &&
            nsHTMLEditUtils::IsBreak(maybeBreak) && nsEditor::IsBlockNode(maybeBlock))
          nodeToDelete = maybeBreak;
        else if (aAction == nsIEditor::ePrevious)
          res = mEditor->GetPriorHTMLNode(node, offset, &nodeToDelete);
        else if (aAction == nsIEditor::eNext)
          res = mEditor->GetNextHTMLNode(node, offset, &nodeToDelete);
        else
          return NS_OK;
      } 
      
      if (NS_FAILED(res)) return res;
      if (!nodeToDelete) return NS_ERROR_NULL_POINTER;
      if (mBody == nodeToDelete) 
      {
        *aCancel = PR_TRUE;
        return res;
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
        res = nsEditor::GetNodeLocation(nodeToDelete, &node, &offset);
        if (NS_FAILED(res)) return res;
        // EXCEPTION: if it's a mozBR, we have to check and see if
        // there is a br in front of it. If so, we must delete both.
        // else you get this: deletion code deletes mozBR, then selection
        // adjusting code puts it back in.  doh
        if (nsHTMLEditUtils::IsMozBR(nodeToDelete))
        {
          nsCOMPtr<nsIDOMNode> brNode;
          res = mEditor->GetPriorHTMLNode(nodeToDelete, &brNode);
          if (nsHTMLEditUtils::IsBreak(brNode))
          {
            // is brNode also a descendant of same block?
            nsCOMPtr<nsIDOMNode> block, brBlock;
            block = mEditor->GetBlockNodeParent(nodeToDelete);
            brBlock = mEditor->GetBlockNodeParent(brNode);
            if (block == brBlock)
            {
              // delete both breaks
              res = mEditor->DeleteNode(brNode);
              if (NS_FAILED(res)) return res;
              res = mEditor->DeleteNode(nodeToDelete);
              *aHandled = PR_TRUE;
              return res;
            }
            // else fall through
          }
          // else fall through
        }
        // adjust selection to be right after it
        res = aSelection->Collapse(node, offset+1);
        if (NS_FAILED(res)) return res;
        res = mEditor->DeleteNode(nodeToDelete);
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
  res = mEditor->GetEndNodeAndOffset(aSelection, &endNode, &endOffset);
  if (NS_FAILED(res)) 
  { 
    return res; 
  }
  if (endNode.get() != node.get())
  {
    // block parents the same?  use default deletion
    if (mEditor->HasSameBlockNodeParent(node, endNode)) return NS_OK;
    
    // deleting across blocks
    // are the blocks of same type?
    nsCOMPtr<nsIDOMNode> leftParent;
    nsCOMPtr<nsIDOMNode> rightParent;

    // XXX: Fix for bug #10815: Crash deleting selected text and table.
    //      Make sure leftParent and rightParent are never NULL. This
    //      can happen if we call GetBlockNodeParent() and the node we
    //      pass in is a body node.
    //
    //      Should we be calling IsBlockNode() instead of IsBody() here?

    if (nsHTMLEditUtils::IsBody(node))
      leftParent = node;
    else
      leftParent = mEditor->GetBlockNodeParent(node);

    if (nsHTMLEditUtils::IsBody(endNode))
      rightParent = endNode;
    else
      rightParent = mEditor->GetBlockNodeParent(endNode);
    
    // do not delete across table structures
    if (nsHTMLEditUtils::IsTableElement(leftParent) || nsHTMLEditUtils::IsTableElement(rightParent))
    {
      *aCancel = PR_TRUE;
      return NS_OK;
    }
                
    // are the blocks siblings?
    nsCOMPtr<nsIDOMNode> leftBlockParent;
    nsCOMPtr<nsIDOMNode> rightBlockParent;
    leftParent->GetParentNode(getter_AddRefs(leftBlockParent));
    rightParent->GetParentNode(getter_AddRefs(rightBlockParent));
    // bail to default if blocks aren't siblings
    if (leftBlockParent.get() != rightBlockParent.get()) return NS_OK;

    if (mEditor->NodesSameType(leftParent, rightParent))
    {
      nsCOMPtr<nsIDOMNode> topParent;
      leftParent->GetParentNode(getter_AddRefs(topParent));
      
      if (nsHTMLEditUtils::IsParagraph(leftParent))
      {
        // first delete the selection
        *aHandled = PR_TRUE;
        res = mEditor->DeleteSelectionImpl(aAction);
        if (NS_FAILED(res)) return res;
        // then join para's, insert break
        res = mEditor->JoinNodeDeep(leftParent,rightParent,&selNode,&selOffset);
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
        res = mEditor->DeleteSelectionImpl(aAction);
        if (NS_FAILED(res)) return res;
        // join blocks
        res = mEditor->JoinNodeDeep(leftParent,rightParent,&selNode,&selOffset);
        if (NS_FAILED(res)) return res;
        // fix up selection
        res = aSelection->Collapse(selNode,selOffset);
        return res;
      }
    }
    
    // else blocks not same type, bail to default
    return NS_OK;
  }

  return res;
}  


nsresult
nsHTMLEditRules::WillMakeList(nsIDOMSelection *aSelection, 
                              const nsString *aListType, 
                              PRBool *aCancel,
                              PRBool *aHandled,
                              const nsString *aItemType)
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
  else if (aListType->EqualsWithConversion("dl"))
    itemType.AssignWithConversion("dd");
  else
    itemType.AssignWithConversion("li");
    
  // ok, we aren't creating a new empty list.  Instead we are converting
  // the set of blocks implied by the selection into a list.
  
  // convert the selection ranges into "promoted" selection ranges:
  // this basically just expands the range to include the immediate
  // block parent, and then further expands to include any ancestors
  // whose children are all in the range
  
  *aHandled = PR_TRUE;

  nsAutoSelectionReset selectionResetter(aSelection, mEditor);

  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  res = GetListActionNodes(&arrayOfNodes);
  if (NS_FAILED(res)) return res;
  
  PRUint32 listCount;
  arrayOfNodes->Count(&listCount);
  
  // if no nodes, we make empty list.
  if (!listCount) 
  {
    nsCOMPtr<nsIDOMNode> parent, theList, theListItem;
    PRInt32 offset;
    
    // get selection location
    res = mEditor->GetStartNodeAndOffset(aSelection, &parent, &offset);
    if (NS_FAILED(res)) return res;
    
    // make sure we can put a list here
    res = SplitAsNeeded(aListType, &parent, &offset);
    if (NS_FAILED(res)) return res;
    res = mEditor->CreateNode(*aListType, parent, offset, getter_AddRefs(theList));
    if (NS_FAILED(res)) return res;
    res = mEditor->CreateNode(itemType, theList, 0, getter_AddRefs(theListItem));
    if (NS_FAILED(res)) return res;
    // put selection in new list item
    res = aSelection->Collapse(theListItem,0);
    selectionResetter.Abort();  // to prevent selection reseter from overriding us.
    *aHandled = PR_TRUE;
    return res;
  }

  // if there is only one node in the array, and it is a list, div, or blockquote,
  // then look inside of it until we find what we want to make a list out of.
  if (listCount == 1)
  {
    nsCOMPtr<nsISupports> isupports = (dont_AddRef)(arrayOfNodes->ElementAt(0));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports) );
    
    while (nsHTMLEditUtils::IsDiv(curNode)
           || nsHTMLEditUtils::IsList(curNode)
           || nsHTMLEditUtils::IsBlockquote(curNode))
    {
      // dive as long as there is only one child, and it is a list, div, blockquote
      PRUint32 numChildren;
      res = mEditor->CountEditableChildren(curNode, numChildren);
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
    // replace the one node in the array with this node
    isupports = do_QueryInterface(curNode);
    arrayOfNodes->ReplaceElementAt(isupports, 0);
  }

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
    nsCOMPtr<nsISupports> isupports  = (dont_AddRef)(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports ) );
    PRInt32 offset;
    res = nsEditor::GetNodeLocation(curNode, &curParent, &offset);
    if (NS_FAILED(res)) return res;
  
    // if curNode is a Break, delete it, and quit remembering prev list item
    if (nsHTMLEditUtils::IsBreak(curNode)) 
    {
      res = mEditor->DeleteNode(curNode);
      if (NS_FAILED(res)) return res;
      prevListItem = 0;
      continue;
    }
    // if curNode is an empty inline container, delete it
    else if (mEditor->IsInlineNode(curNode) && mEditor->IsContainer(curNode)) 
    {
      PRBool bEmpty;
      res = mEditor->IsEmptyNode(curNode, &bEmpty);
      if (NS_FAILED(res)) return res;
      if (bEmpty)
      {
        res = mEditor->DeleteNode(curNode);
        if (NS_FAILED(res)) return res;
        continue;
      }
    }
    
    if (nsHTMLEditUtils::IsList(curNode))
    {
      nsAutoString existingListStr;
      res = mEditor->GetTagString(curNode, existingListStr);
      // do we have a curList already?
      if (curList && !nsHTMLEditUtils::IsDescendantOf(curNode, curList))
      {
        // move all of our children into curList.
        // cheezy way to do it: move whole list and then
        // RemoveContainer() on the list.
        // ConvertListType first: that routine
        // handles converting the list item types, if needed
        res = mEditor->MoveNode(curNode, curList, -1);
        if (NS_FAILED(res)) return res;
        res = ConvertListType(curNode, &newBlock, *aListType, itemType);
        if (NS_FAILED(res)) return res;
        res = mEditor->RemoveContainer(newBlock);
        if (NS_FAILED(res)) return res;
      }
      else
      {
        // replace list with new list type
        res = ConvertListType(curNode, &newBlock, *aListType, itemType);
        if (NS_FAILED(res)) return res;
        curList = newBlock;
      }
      continue;
    }

    if (nsHTMLEditUtils::IsListItem(curNode))
    {
      nsAutoString existingListStr;
      res = mEditor->GetTagString(curParent, existingListStr);
      if ( existingListStr != *aListType )
      {
        // list item is in wrong type of list.  
        // if we dont have a curList, split the old list
        // and make a new list of correct type.
        if (!curList || nsHTMLEditUtils::IsDescendantOf(curNode, curList))
        {
          res = mEditor->SplitNode(curParent, offset, getter_AddRefs(newBlock));
          if (NS_FAILED(res)) return res;
          nsCOMPtr<nsIDOMNode> p;
          PRInt32 o;
          res = nsEditor::GetNodeLocation(curParent, &p, &o);
          if (NS_FAILED(res)) return res;
          res = mEditor->CreateNode(*aListType, p, o, getter_AddRefs(curList));
          if (NS_FAILED(res)) return res;
        }
        // move list item to new list
        res = mEditor->MoveNode(curNode, curList, -1);
        if (NS_FAILED(res)) return res;
        // convert list item type if needed
        if (!mEditor->NodeIsType(curNode,itemType))
        {
          res = mEditor->ReplaceContainer(curNode, &newBlock, itemType);
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
            res = mEditor->MoveNode(curNode, curList, -1);
            if (NS_FAILED(res)) return res;
          }
        }
        if (!mEditor->NodeIsType(curNode,itemType))
        {
          res = mEditor->ReplaceContainer(curNode, &newBlock, itemType);
          if (NS_FAILED(res)) return res;
        }
      }
      continue;
    }
    
    // need to make a list to put things in if we haven't already,
    // or if this node doesn't go in list we used earlier.
    if (!curList) // || transitionList[i])
    {
      res = SplitAsNeeded(aListType, &curParent, &offset);
      if (NS_FAILED(res)) return res;
      res = mEditor->CreateNode(*aListType, curParent, offset, getter_AddRefs(curList));
      if (NS_FAILED(res)) return res;
      // curList is now the correct thing to put curNode in
      prevListItem = 0;
    }
  
    // if curNode isn't a list item, we must wrap it in one
    nsCOMPtr<nsIDOMNode> listItem;
    if (!nsHTMLEditUtils::IsListItem(curNode))
    {
      if (nsEditor::IsInlineNode(curNode) && prevListItem)
      {
        // this is a continuation of some inline nodes that belong together in
        // the same list item.  use prevListItem
        PRUint32 listItemLen;
        res = mEditor->GetLengthOfDOMNode(prevListItem, listItemLen);
        if (NS_FAILED(res)) return res;
        res = mEditor->MoveNode(curNode, prevListItem, listItemLen);
        if (NS_FAILED(res)) return res;
      }
      else
      {
        // don't wrap li around a paragraph.  instead replace paragraph with li
        if (nsHTMLEditUtils::IsParagraph(curNode))
        {
          res = mEditor->ReplaceContainer(curNode, &listItem, itemType);
        }
        else
        {
          res = mEditor->InsertContainerAbove(curNode, &listItem, itemType);
        }
        if (NS_FAILED(res)) return res;
        if (nsEditor::IsInlineNode(curNode)) 
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
      res = mEditor->GetLengthOfDOMNode(curList, listLen);
      if (NS_FAILED(res)) return res;
      res = mEditor->MoveNode(listItem, curList, listLen);
      if (NS_FAILED(res)) return res;
    }
  }

  return res;
}


nsresult
nsHTMLEditRules::WillRemoveList(nsIDOMSelection *aSelection, 
                                PRBool aOrdered, 
                                PRBool *aCancel,
                                PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_TRUE;
  
  nsAutoSelectionReset selectionResetter(aSelection, mEditor);
  
  nsCOMPtr<nsISupportsArray> arrayOfRanges;
  nsresult res = GetPromotedRanges(aSelection, &arrayOfRanges, kMakeList);
  if (NS_FAILED(res)) return res;
  
  // use these ranges to contruct a list of nodes to act on.
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  res = GetNodesForOperation(arrayOfRanges, &arrayOfNodes, kMakeList);
  if (NS_FAILED(res)) return res;                                 
                                     
  // Remove all non-editable nodes.  Leave them be.
  
  PRUint32 listCount;
  PRInt32 i;
  arrayOfNodes->Count(&listCount);
  for (i=listCount-1; i>=0; i--)
  {
    nsCOMPtr<nsISupports> isupports = (dont_AddRef)(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> testNode( do_QueryInterface(isupports ) );
    if (!mEditor->IsEditable(testNode))
    {
      arrayOfNodes->RemoveElementAt(i);
    }
  }
  
  // Only act on lists or list items in the array
  nsCOMPtr<nsIDOMNode> curParent;
  for (i=0; i<(PRInt32)listCount; i++)
  {
    // here's where we actually figure out what to do
    nsCOMPtr<nsISupports> isupports  = (dont_AddRef)(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports ) );
    PRInt32 offset;
    res = nsEditor::GetNodeLocation(curNode, &curParent, &offset);
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
      nsCOMPtr<nsIDOMNode> child;
      curNode->GetLastChild(getter_AddRefs(child));

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
        else
        {
          // delete any non- list items for now
          res = mEditor->DeleteNode(child);
          if (NS_FAILED(res)) return res;
        }
        curNode->GetLastChild(getter_AddRefs(child));
      }
      // delete the now-empty list
      res = mEditor->DeleteNode(curNode);
      if (NS_FAILED(res)) return res;
    }
  }
  return res;
}


nsresult
nsHTMLEditRules::WillMakeDefListItem(nsIDOMSelection *aSelection, 
                                     const nsString *aItemType, 
                                     PRBool *aCancel,
                                     PRBool *aHandled)
{
  // for now we let WillMakeList handle this
  nsAutoString listType; 
  listType.AssignWithConversion("dl");
  return WillMakeList(aSelection, &listType, aCancel, aHandled, aItemType);
}

nsresult
nsHTMLEditRules::WillMakeBasicBlock(nsIDOMSelection *aSelection, 
                                    const nsString *aBlockType, 
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
  nsAutoSelectionReset selectionResetter(aSelection, mEditor);
  nsAutoTxnsConserveSelection dontSpazMySelection(mEditor);
  *aHandled = PR_TRUE;

  nsCOMPtr<nsISupportsArray> arrayOfRanges;
  res = GetPromotedRanges(aSelection, &arrayOfRanges, kMakeBasicBlock);
  if (NS_FAILED(res)) return res;
  
  // use these ranges to contruct a list of nodes to act on.
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  res = GetNodesForOperation(arrayOfRanges, &arrayOfNodes, kMakeBasicBlock);
  if (NS_FAILED(res)) return res;                                 
                                     
  // if no nodes, we make empty block.
  PRUint32 listCount;
  arrayOfNodes->Count(&listCount);
  if (!listCount) 
  {
    nsCOMPtr<nsIDOMNode> parent, theBlock;
    PRInt32 offset;
    
    // get selection location
    res = mEditor->GetStartNodeAndOffset(aSelection, &parent, &offset);
    if (NS_FAILED(res)) return res;
    
    // make sure we can put a block here
    res = SplitAsNeeded(aBlockType, &parent, &offset);
    if (NS_FAILED(res)) return res;
    res = mEditor->CreateNode(*aBlockType, parent, offset, getter_AddRefs(theBlock));
    if (NS_FAILED(res)) return res;
    // put selection in new block
    res = aSelection->Collapse(theBlock,0);
    selectionResetter.Abort();  // to prevent selection reseter from overriding us.
    *aHandled = PR_TRUE;
    return res;
  }
  else
  {
    // Ok, now go through all the nodes and make the right kind of blocks, 
    // or whatever is approriate.  Wohoo! 
    // Note: blockquote is handled a little differently
    if (aBlockType->EqualsWithConversion("blockquote"))
      res = MakeBlockquote(arrayOfNodes);
    else
      res = ApplyBlockStyle(arrayOfNodes, aBlockType);
    return res;
  }
  return res;
}

nsresult 
nsHTMLEditRules::DidMakeBasicBlock(nsIDOMSelection *aSelection,
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
  res = nsEditor::GetStartNodeAndOffset(aSelection, &parent, &offset);
  if (NS_FAILED(res)) return res;
  res = InsertMozBRIfNeeded(parent);
  return res;
}

nsresult
nsHTMLEditRules::WillIndent(nsIDOMSelection *aSelection, PRBool *aCancel, PRBool * aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  
  nsresult res = WillInsert(aSelection, aCancel);
  if (NS_FAILED(res)) return res;

  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = PR_FALSE;
  *aHandled = PR_TRUE;

  nsAutoSelectionReset selectionResetter(aSelection, mEditor);
  
  // convert the selection ranges into "promoted" selection ranges:
  // this basically just expands the range to include the immediate
  // block parent, and then further expands to include any ancestors
  // whose children are all in the range
  
  nsCOMPtr<nsISupportsArray> arrayOfRanges;
  res = GetPromotedRanges(aSelection, &arrayOfRanges, kIndent);
  if (NS_FAILED(res)) return res;
  
  // use these ranges to contruct a list of nodes to act on.
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  res = GetNodesForOperation(arrayOfRanges, &arrayOfNodes, kIndent);
  if (NS_FAILED(res)) return res;                                 
                                     
  // if no nodes, we make empty block.
  PRUint32 listCount;
  arrayOfNodes->Count(&listCount);
  if (!listCount) 
  {
    nsCOMPtr<nsIDOMNode> parent, theBlock;
    PRInt32 offset;
    nsAutoString quoteType; quoteType.AssignWithConversion("blockquote");
    
    // get selection location
    res = mEditor->GetStartNodeAndOffset(aSelection, &parent, &offset);
    if (NS_FAILED(res)) return res;
    
    // make sure we can put a block here
    res = SplitAsNeeded(&quoteType, &parent, &offset);
    if (NS_FAILED(res)) return res;
    res = mEditor->CreateNode(quoteType, parent, offset, getter_AddRefs(theBlock));
    if (NS_FAILED(res)) return res;
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
  for (i=0; i<(PRInt32)listCount; i++)
  {
    // here's where we actually figure out what to do
    nsCOMPtr<nsISupports> isupports  = (dont_AddRef)(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports ) );
    PRInt32 offset;
    res = nsEditor::GetNodeLocation(curNode, &curParent, &offset);
    if (NS_FAILED(res)) return res;
    
    // some logic for putting list items into nested lists...
    if (nsHTMLEditUtils::IsList(curParent))
    {
      if (!curList || transitionList[i])
      {
        nsAutoString listTag;
        nsEditor::GetTagString(curParent,listTag);
        // create a new nested list of correct type
        res = SplitAsNeeded(&listTag, &curParent, &offset);
        if (NS_FAILED(res)) return res;
        res = mEditor->CreateNode(listTag, curParent, offset, getter_AddRefs(curList));
        if (NS_FAILED(res)) return res;
        // curList is now the correct thing to put curNode in
      }
      // tuck the node into the end of the active list
      PRUint32 listLen;
      res = mEditor->GetLengthOfDOMNode(curList, listLen);
      if (NS_FAILED(res)) return res;
      res = mEditor->MoveNode(curNode, curList, listLen);
      if (NS_FAILED(res)) return res;
    }
    
    else // not a list item, use blockquote
    {
      // need to make a blockquote to put things in if we haven't already,
      // or if this node doesn't go in blockquote we used earlier.
      if (!curQuote) // || transitionList[i])
      {
        nsAutoString quoteType; quoteType.AssignWithConversion("blockquote");
        res = SplitAsNeeded(&quoteType, &curParent, &offset);
        if (NS_FAILED(res)) return res;
        res = mEditor->CreateNode(quoteType, curParent, offset, getter_AddRefs(curQuote));
        if (NS_FAILED(res)) return res;
        
/* !!!!!!!!!!!!!!!  TURNED OFF PER BUG 33213 !!!!!!!!!!!!!!!!!!!!
        // set style to not have unwanted vertical margins
        nsCOMPtr<nsIDOMElement> quoteElem = do_QueryInterface(curQuote);
        res = mEditor->SetAttribute(quoteElem, NS_ConvertASCIItoUCS2("style"), NS_ConvertASCIItoUCS2("margin: 0 0 0 40px;"));
        if (NS_FAILED(res)) return res;
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/

        // curQuote is now the correct thing to put curNode in
      }
        
      // tuck the node into the end of the active blockquote
      PRUint32 quoteLen;
      res = mEditor->GetLengthOfDOMNode(curQuote, quoteLen);
      if (NS_FAILED(res)) return res;
      res = mEditor->MoveNode(curNode, curQuote, quoteLen);
      if (NS_FAILED(res)) return res;
    }
  }
  return res;
}


nsresult
nsHTMLEditRules::WillOutdent(nsIDOMSelection *aSelection, PRBool *aCancel, PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_TRUE;
  
  nsAutoSelectionReset selectionResetter(aSelection, mEditor);
  nsresult res = NS_OK;
  
  // convert the selection ranges into "promoted" selection ranges:
  // this basically just expands the range to include the immediate
  // block parent, and then further expands to include any ancestors
  // whose children are all in the range
  
  nsCOMPtr<nsISupportsArray> arrayOfRanges;
  res = GetPromotedRanges(aSelection, &arrayOfRanges, kOutdent);
  if (NS_FAILED(res)) return res;
  
  // use these ranges to contruct a list of nodes to act on.
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  res = GetNodesForOperation(arrayOfRanges, &arrayOfNodes, kOutdent);
  if (NS_FAILED(res)) return res;                                 
                                     
  // Next we detect all the transitions in the array, where a transition
  // means that adjacent nodes in the array don't have the same parent.
  
  nsVoidArray transitionList;
  res = MakeTransitionList(arrayOfNodes, &transitionList);
  if (NS_FAILED(res)) return res;                                 
  
  // Ok, now go through all the nodes and remove a level of blockquoting, 
  // or whatever is appropriate.  Wohoo!

  PRUint32 listCount;
  PRInt32 i;
  arrayOfNodes->Count(&listCount);
  nsCOMPtr<nsIDOMNode> curParent;
  for (i=0; i<(PRInt32)listCount; i++)
  {
    // here's where we actually figure out what to do
    nsCOMPtr<nsISupports> isupports  = (dont_AddRef)(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports ) );
    PRInt32 offset;
    res = nsEditor::GetNodeLocation(curNode, &curParent, &offset);
    if (NS_FAILED(res)) return res;
    
    if (nsHTMLEditUtils::IsList(curParent))  // move node out of list
    {
      if (nsHTMLEditUtils::IsList(curNode))  // just unwrap this sublist
      {
        res = mEditor->RemoveContainer(curNode);
        if (NS_FAILED(res)) return res;
      }
      else  // we are moving a list item, but not whole list
      {
        PRBool bOutOfList;
        res = PopListItem(curNode, &bOutOfList);
        if (NS_FAILED(res)) return res;
      }
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
        else
        {
          // delete any non- list items for now
          res = mEditor->DeleteNode(child);
          if (NS_FAILED(res)) return res;
        }
        curNode->GetLastChild(getter_AddRefs(child));
      }
      // delete the now-empty list
      res = mEditor->DeleteNode(curNode);
      if (NS_FAILED(res)) return res;
    }
    else if (transitionList[i])  // not list related - look for enclosing blockquotes and remove
    {
      // look for a blockquote somewhere above us and remove it.
      // this is a hack until i think about outdent for real.
      nsCOMPtr<nsIDOMNode> n = curNode;
      nsCOMPtr<nsIDOMNode> tmp;
      while (!nsHTMLEditUtils::IsBody(n))
      {
        if (nsHTMLEditUtils::IsBlockquote(n))
        {
          res = AddTerminatingBR(n);
          if (NS_FAILED(res)) return res;
          mEditor->RemoveContainer(n);
          break;
        }
        n->GetParentNode(getter_AddRefs(tmp));
        n = tmp;
      }
    }
  }

  return res;
}


///////////////////////////////////////////////////////////////////////////
// ConvertListType:  convert list type and list item type.
//                
//                  
nsresult 
nsHTMLEditRules::ConvertListType(nsIDOMNode *aList, 
                                 nsCOMPtr<nsIDOMNode> *outList,
                                 const nsString& aListType, 
                                 const nsString& aItemType) 
{
  if (!aList || !outList) return NS_ERROR_NULL_POINTER;
  *outList = aList;  // we migvht not need to change the list
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> child, temp;
  aList->GetFirstChild(getter_AddRefs(child));
  while (child)
  {
    if (!mEditor->NodeIsType(child, aItemType))
    {
      res = mEditor->ReplaceContainer(child, &temp, aItemType);
      if (NS_FAILED(res)) return res;
      child = temp;
    }
    child->GetNextSibling(getter_AddRefs(temp));
    child = temp;
  }
  if (!mEditor->NodeIsType(aList, aListType))
  {
    res = mEditor->ReplaceContainer(aList, outList, aListType);
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// CreateStyleForInsertText:  take care of clearing and setting appropriate
//                            style nodes for text insertion.
//                
//                  
nsresult 
nsHTMLEditRules::CreateStyleForInsertText(nsIDOMSelection *aSelection, nsIDOMDocument *aDoc) 
{
  if (!aSelection || !aDoc) return NS_ERROR_NULL_POINTER;
  if (!mEditor->mTypeInState) return NS_ERROR_NULL_POINTER;
  
  PRBool weDidSometing = PR_FALSE;
  nsCOMPtr<nsIDOMNode> node;
  PRInt32 offset;
  nsresult res = mEditor->GetStartNodeAndOffset(aSelection, &node, &offset);
  if (NS_FAILED(res)) return res;
  PropItem *item = nsnull;
  
  // process clearing any styles first
  mEditor->mTypeInState->TakeClearProperty(&item);
  while (item)
  {
    res = mEditor->SplitStyleAbovePoint(&node, &offset, item->tag, &item->attr);
    if (NS_FAILED(res)) return res;
    // we own item now (TakeClearProperty hands ownership to us)
    delete item;
    mEditor->mTypeInState->TakeClearProperty(&item);
    weDidSometing = PR_TRUE;
  }
  
  // then process setting any styles
  PRInt32 relFontSize;
  
  res = mEditor->mTypeInState->TakeRelativeFontSize(&relFontSize);
  if (NS_FAILED(res)) return res;
  res = mEditor->mTypeInState->TakeSetProperty(&item);
  if (NS_FAILED(res)) return res;
  
  if (item || relFontSize) // we have at least one style to add; make a
  {                        // new text node to insert style nodes above.
    if (mEditor->IsTextNode(node))
    {
      // if we are in a text node, split it
      res = mEditor->SplitNodeDeep(node, node, offset, &offset);
      if (NS_FAILED(res)) return res;
      nsCOMPtr<nsIDOMNode> tmp;
      node->GetParentNode(getter_AddRefs(tmp));
      node = tmp;
    }
    nsCOMPtr<nsIDOMNode> newNode;
    nsCOMPtr<nsIDOMText> nodeAsText;
    res = aDoc->CreateTextNode(nsAutoString(), getter_AddRefs(nodeAsText));
    if (NS_FAILED(res)) return res;
    if (!nodeAsText) return NS_ERROR_NULL_POINTER;
    newNode = do_QueryInterface(nodeAsText);
    res = mEditor->InsertNode(newNode, node, offset);
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
        res = mEditor->RelativeFontChangeOnTextNode(dir, nodeAsText, 0, -1);
        if (NS_FAILED(res)) return res;
      }
    }
    
    while (item)
    {
      res = mEditor->SetInlinePropertyOnNode(node, item->tag, &item->attr, &item->value);
      if (NS_FAILED(res)) return res;
      // we own item now (TakeSetProperty hands ownership to us)
      delete item;
      mEditor->mTypeInState->TakeSetProperty(&item);
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
  if (nsEditor::IsBlockNode(aNode)) nodeToTest = do_QueryInterface(aNode);
//  else nsCOMPtr<nsIDOMElement> block;
//  looks like I forgot to finish this.  Wonder what I was going to do?

  if (!nodeToTest) return NS_ERROR_NULL_POINTER;
  return mEditor->IsEmptyNode(nodeToTest, outIsEmptyBlock,
                     aMozBRDoesntCount, aListItemsNotEmpty);
}


nsresult
nsHTMLEditRules::WillAlign(nsIDOMSelection *aSelection, 
                           const nsString *alignType, 
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
  
  nsAutoSelectionReset selectionResetter(aSelection, mEditor);

  // convert the selection ranges into "promoted" selection ranges:
  // this basically just expands the range to include the immediate
  // block parent, and then further expands to include any ancestors
  // whose children are all in the range
  *aHandled = PR_TRUE;
  
  nsCOMPtr<nsISupportsArray> arrayOfRanges;
  res = GetPromotedRanges(aSelection, &arrayOfRanges, kAlign);
  if (NS_FAILED(res)) return res;
  
  // use these ranges to contruct a list of nodes to act on.
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  res = GetNodesForOperation(arrayOfRanges, &arrayOfNodes, kAlign);
  if (NS_FAILED(res)) return res;                                 
                                     
  PRUint32 listCount;
  arrayOfNodes->Count(&listCount);
  if (!listCount) 
  {
    PRInt32 offset;
    nsCOMPtr<nsIDOMNode> brNode, parent, theDiv, sib;
    nsAutoString divType; divType.AssignWithConversion("div");
    res = mEditor->GetStartNodeAndOffset(aSelection, &parent, &offset);
    if (NS_FAILED(res)) return res;
    res = SplitAsNeeded(&divType, &parent, &offset);
    if (NS_FAILED(res)) return res;
    // consume a trailing br, if any.  This is to keep an alignment from
    // creating extra lines, if possible.
    res = mEditor->GetNextHTMLNode(parent, offset, &brNode);
    if (NS_FAILED(res)) return res;
    if (brNode && nsHTMLEditUtils::IsBreak(brNode))
    {
      // making use of html structure... if next node after where
      // we are putting our div is not a block, then the br we 
      // found is in same block we are, so its safe to consume it.
      res = mEditor->GetNextHTMLSibling(parent, offset, &sib);
      if (NS_FAILED(res)) return res;
      if (!nsEditor::IsBlockNode(sib))
      {
        res = mEditor->DeleteNode(brNode);
        if (NS_FAILED(res)) return res;
      }
    }
    res = mEditor->CreateNode(divType, parent, offset, getter_AddRefs(theDiv));
    if (NS_FAILED(res)) return res;
    // set up the alignment on the div
    nsCOMPtr<nsIDOMElement> divElem = do_QueryInterface(theDiv);
    nsAutoString attr; attr.AssignWithConversion("align");
    res = mEditor->SetAttribute(divElem, attr, *alignType);
    if (NS_FAILED(res)) return res;
    *aHandled = PR_TRUE;
    // put in a moz-br so that it won't get deleted
    res = CreateMozBR(theDiv, 0, &brNode);
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
    nsCOMPtr<nsISupports> isupports  = (dont_AddRef)(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports ) );
    PRInt32 offset;
    res = nsEditor::GetNodeLocation(curNode, &curParent, &offset);
    if (NS_FAILED(res)) return res;
    
    // if it's a div, don't nest it, just set the alignment
    if (nsHTMLEditUtils::IsDiv(curNode))
    {
      nsCOMPtr<nsIDOMElement> divElem = do_QueryInterface(curNode);
      nsAutoString attr; attr.AssignWithConversion("align");
      res = mEditor->SetAttribute(divElem, attr, *alignType);
      if (NS_FAILED(res)) return res;
      // clear out curDiv so that we don't put nodes after this one into it
      curDiv = 0;
      continue;
    }
    
    // if it's a table element (but not a table) or a list item, forget any "current" div, and 
    // instead put divs inside the appropriate block (td, li, etc)
    if ( (nsHTMLEditUtils::IsTableElement(curNode) && !nsHTMLEditUtils::IsTable(curNode))
         || nsHTMLEditUtils::IsListItem(curNode) )
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
      res = SplitAsNeeded(&divType, &curParent, &offset);
      if (NS_FAILED(res)) return res;
      res = mEditor->CreateNode(divType, curParent, offset, getter_AddRefs(curDiv));
      if (NS_FAILED(res)) return res;
      // set up the alignment on the div
      nsCOMPtr<nsIDOMElement> divElem = do_QueryInterface(curDiv);
      nsAutoString attr; attr.AssignWithConversion("align");
      res = mEditor->SetAttribute(divElem, attr, *alignType);
      if (NS_FAILED(res)) return res;
      // curDiv is now the correct thing to put curNode in
    }
        
    // tuck the node into the end of the active div
    PRUint32 listLen;
    res = mEditor->GetLengthOfDOMNode(curDiv, listLen);
    if (NS_FAILED(res)) return res;
    res = mEditor->MoveNode(curNode, curDiv, listLen);
    if (NS_FAILED(res)) return res;
  }

  return res;
}


///////////////////////////////////////////////////////////////////////////
// AlignInnerBlocks: align inside table cells or list items
//       
nsresult
nsHTMLEditRules::AlignInnerBlocks(nsIDOMNode *aNode, const nsString *alignType)
{
  if (!aNode || !alignType) return NS_ERROR_NULL_POINTER;
  nsresult res;
  
  // gather list of table cells or list items
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  nsTableCellAndListItemFunctor functor;
  nsDOMIterator iter;
  res = iter.Init(aNode);
  if (NS_FAILED(res)) return res;
  res = iter.MakeList(functor, &arrayOfNodes);
  if (NS_FAILED(res)) return res;
  
  // now that we have the list, align their contents as requested
  PRUint32 listCount;
  PRUint32 j;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsISupports> isupports;

  arrayOfNodes->Count(&listCount);
  for (j = 0; j < listCount; j++)
  {
    isupports = (dont_AddRef)(arrayOfNodes->ElementAt(0));
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
nsHTMLEditRules::AlignBlockContents(nsIDOMNode *aNode, const nsString *alignType)
{
  if (!aNode || !alignType) return NS_ERROR_NULL_POINTER;
  nsresult res;
  nsCOMPtr <nsIDOMNode> firstChild, lastChild, divNode;
  
  res = mEditor->GetFirstEditableChild(aNode, &firstChild);
  if (NS_FAILED(res)) return res;
  res = mEditor->GetLastEditableChild(aNode, &lastChild);
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
    res = mEditor->SetAttribute(divElem, attr, *alignType);
    if (NS_FAILED(res)) return res;
  }
  else
  {
    // else we need to put in a div, set the alignment, and toss in all the children
    nsAutoString divType; divType.AssignWithConversion("div");
    res = mEditor->CreateNode(divType, aNode, 0, getter_AddRefs(divNode));
    if (NS_FAILED(res)) return res;
    // set up the alignment on the div
    nsCOMPtr<nsIDOMElement> divElem = do_QueryInterface(divNode);
    nsAutoString attr; attr.AssignWithConversion("align");
    res = mEditor->SetAttribute(divElem, attr, *alignType);
    if (NS_FAILED(res)) return res;
    // tuck the children into the end of the active div
    while (lastChild && (lastChild != divNode))
    {
      res = mEditor->MoveNode(lastChild, divNode, 0);
      if (NS_FAILED(res)) return res;
      res = mEditor->GetLastEditableChild(aNode, &lastChild);
      if (NS_FAILED(res)) return res;
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
  
  nsresult res = mEditor->GetFirstEditableChild(aNode, &node);
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
  nsresult res = nsEditor::GetNodeLocation(aNode, &parent, &offset);
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
    if (mEditor->IsEditable(child)) 
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
  nsresult res = nsEditor::GetNodeLocation(aNode, &parent, &offset);
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
    if (mEditor->IsEditable(child)) 
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
  nsresult  res = mEditor->GetPriorHTMLNode(aNode, aOffset, &priorNode);
  if (NS_FAILED(res)) return PR_TRUE;
  if (!priorNode) return PR_TRUE;
  nsCOMPtr<nsIDOMNode> blockParent = mEditor->GetBlockNodeParent(priorNode);
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
  nsresult  res = mEditor->GetNextHTMLNode(aNode, aOffset, &nextNode);
  if (NS_FAILED(res)) return PR_TRUE;
  if (!nextNode) return PR_TRUE;
  nsCOMPtr<nsIDOMNode> blockParent = mEditor->GetBlockNodeParent(nextNode);
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
  nsresult res = mEditor->CreateNode(divType, inParent, inOffset, getter_AddRefs(*outDiv));
  if (NS_FAILED(res)) return res;
  // give it special moz attr
  nsCOMPtr<nsIDOMElement> mozDivElem = do_QueryInterface(*outDiv);
  res = mEditor->SetAttribute(mozDivElem, "type", "_moz");
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
        res = mEditor->IsPrevCharWhitespace(node, offset, &isSpace, &isNBSP, &temp, &offset);
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
        res = mEditor->IsNextCharWhitespace(node, offset, &isSpace, &isNBSP, &temp, &offset);
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
      res = nsEditor::GetNodeLocation(aNode, &parent, &offset);
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
    
    if (!nsEditor::IsBlockNode(node))
    {
      nsCOMPtr<nsIDOMNode> block = nsEditor::GetBlockNodeParent(node);
      nsCOMPtr<nsIDOMNode> prevNode, prevNodeBlock;
      res = mEditor->GetPriorHTMLNode(node, &prevNode);
      
      while (prevNode && NS_SUCCEEDED(res))
      {
        prevNodeBlock = nsEditor::GetBlockNodeParent(prevNode);
        if (prevNodeBlock != block)
          break;
        if (nsHTMLEditUtils::IsBreak(prevNode))
          break;
        if (nsEditor::IsBlockNode(prevNode))
          break;
        node = prevNode;
        res = mEditor->GetPriorHTMLNode(node, &prevNode);
      }
    }
    
    // finding the real start for this point.  look up the tree for as long as we are the 
    // first node in the container, and as long as we haven't hit the body node.
    if (!nsHTMLEditUtils::IsBody(node))
    {
      res = nsEditor::GetNodeLocation(node, &parent, &offset);
      if (NS_FAILED(res)) return res;
      while ((IsFirstNode(node)) && (!nsHTMLEditUtils::IsBody(parent)))
      {
        // special case for outdent: don't keep looking up 
        // if we have found a blockquote element to act on
        if ((actionID == kOutdent) && nsHTMLEditUtils::IsBlockquote(parent))
          break;
        node = parent;
        res = nsEditor::GetNodeLocation(node, &parent, &offset);
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
      res = nsEditor::GetNodeLocation(aNode, &parent, &offset);
      if (NS_FAILED(res)) return res;
    }
    else
    {
      if (offset) offset--; // we want node _before_ offset
      node = nsEditor::GetChildAt(parent,offset);
    }
    if (!node) node = parent;
    
    // if this is an inline node, 
    // look ahead through any further inline nodes that
    // aren't across a <br> from us, and that are enclosed in the same block.
    
    if (!nsEditor::IsBlockNode(node))
    {
      nsCOMPtr<nsIDOMNode> block = nsEditor::GetBlockNodeParent(node);
      nsCOMPtr<nsIDOMNode> nextNode, nextNodeBlock;
      res = mEditor->GetNextHTMLNode(node, &nextNode);
      
      while (nextNode && NS_SUCCEEDED(res))
      {
        nextNodeBlock = nsEditor::GetBlockNodeParent(nextNode);
        if (nextNodeBlock != block)
          break;
        if (nsHTMLEditUtils::IsBreak(nextNode))
        {
          node = nextNode;
          break;
        }
        if (nsEditor::IsBlockNode(nextNode))
          break;
        node = nextNode;
        res = mEditor->GetNextHTMLNode(node, &nextNode);
      }
    }
    
    // finding the real end for this point.  look up the tree for as long as we are the 
    // last node in the container, and as long as we haven't hit the body node.
    if (!nsHTMLEditUtils::IsBody(node))
    {
      res = nsEditor::GetNodeLocation(node, &parent, &offset);
      if (NS_FAILED(res)) return res;
      while ((IsLastNode(node)) && (!nsHTMLEditUtils::IsBody(parent)))
      {
        node = parent;
        res = nsEditor::GetNodeLocation(node, &parent, &offset);
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
nsHTMLEditRules::GetPromotedRanges(nsIDOMSelection *inSelection, 
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
  
  // make a new adjusted range to represent the appropriate block content.
  // this is tricky.  the basic idea is to push out the range endpoints
  // to truly enclose the blocks that we will affect
  
  nsCOMPtr<nsIDOMNode> opStartNode;
  nsCOMPtr<nsIDOMNode> opEndNode;
  PRInt32 opStartOffset, opEndOffset;
  nsCOMPtr<nsIDOMRange> opRange;
  
  res = GetPromotedPoint( kStart, startNode, startOffset, inOperationType, &opStartNode, &opStartOffset);
  if (NS_FAILED(res)) return res;
  res = GetPromotedPoint( kEnd, endNode, endOffset, inOperationType, &opEndNode, &opEndOffset);
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
      isupports = (dont_AddRef)(inArrayOfRanges->ElementAt(i));
      opRange = do_QueryInterface(isupports);
      nsRangeStore *item = new nsRangeStore();
      if (!item) return NS_ERROR_NULL_POINTER;
      item->StoreRange(opRange);
      mEditor->mRangeUpdater.RegisterRangeItem(item);
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
      mEditor->mRangeUpdater.DropRangeItem(item);
      res = item->GetRange(&opRange);
      if (NS_FAILED(res)) return res;
      nsCOMPtr<nsISupports> isupports = do_QueryInterface(opRange);
      inArrayOfRanges->AppendElement(isupports);
    }    
  }
  // gather up a list of all the nodes
  for (i = 0; i < (PRInt32)rangeCount; i++)
  {
    isupports = (dont_AddRef)(inArrayOfRanges->ElementAt(i));
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
      nsCOMPtr<nsISupports> isupports = (dont_AddRef)((*outArrayOfNodes)->ElementAt(i));
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
      nsCOMPtr<nsISupports> isupports = (dont_AddRef)((*outArrayOfNodes)->ElementAt(i));
      nsCOMPtr<nsIDOMNode> node( do_QueryInterface(isupports) );
      if ( (nsHTMLEditUtils::IsTableElement(node) && !nsHTMLEditUtils::IsTable(node)) )
      {
        (*outArrayOfNodes)->RemoveElementAt(i);
        res = GetInnerContent(node, *outArrayOfNodes);
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
      nsCOMPtr<nsISupports> isupports = (dont_AddRef)((*outArrayOfNodes)->ElementAt(i));
      nsCOMPtr<nsIDOMNode> node( do_QueryInterface(isupports) );
      if (!aDontTouchContent && mEditor->IsInlineNode(node) 
           && mEditor->IsContainer(node) && !mEditor->IsTextNode(node))
      {
        nsCOMPtr<nsISupportsArray> arrayOfInlines;
        res = BustUpInlinesAtBRs(node, &arrayOfInlines);
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
          isupports = (dont_AddRef)(arrayOfInlines->ElementAt(iCount));
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
                                    PRBool aDontTouchContent)
{
  if (!outArrayOfNodes) return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIDOMSelection>selection;
  nsresult res = mEditor->GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsISupportsArray> arrayOfRanges;
  res = GetPromotedRanges(selection, &arrayOfRanges, kMakeList);
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
    nsCOMPtr<nsISupports> isupports = (dont_AddRef)((*outArrayOfNodes)->ElementAt(i));
    nsCOMPtr<nsIDOMNode> testNode( do_QueryInterface(isupports ) );

    // Remove all non-editable nodes.  Leave them be.
    if (!mEditor->IsEditable(testNode))
    {
      (*outArrayOfNodes)->RemoveElementAt(i);
    }
    
    // scan for table elements.  If we find table elements other than table,
    // replace it with a list of any editable non-table content.
    if (nsHTMLEditUtils::IsTableElement(testNode) && !nsHTMLEditUtils::IsTable(testNode))
    {
      (*outArrayOfNodes)->RemoveElementAt(i);
      res = GetInnerContent(testNode, *outArrayOfNodes, PR_FALSE);
      if (NS_FAILED(res)) return res;
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
    if (mEditor->NodeIsType(child,nsIEditProperty::dt)) aDT = PR_TRUE;
    else if (mEditor->NodeIsType(child,nsIEditProperty::dd)) aDD = PR_TRUE;
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
  
  nsCOMPtr<nsIDOMSelection>selection;
  nsresult res = mEditor->GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsISupportsArray> arrayOfRanges;
  res = GetPromotedRanges(selection, &arrayOfRanges, kMakeList);
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
    nsCOMPtr<nsISupports> isupports = (dont_AddRef)((*outArrayOfNodes)->ElementAt(i));
    nsCOMPtr<nsIDOMNode> testNode( do_QueryInterface(isupports ) );

    // Remove all non-editable nodes.  Leave them be.
    if (!mEditor->IsEditable(testNode))
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
    res = mEditor->SplitNodeDeep(endInline, item.endNode, item.endOffset,
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
    res = mEditor->SplitNodeDeep(startInline, item.startNode, item.startOffset,
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
  res = iter.MakeList(functor, &arrayOfBreaks);
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
      isupports = (dont_AddRef)(arrayOfBreaks->ElementAt(i));
      breakNode = do_QueryInterface(isupports);
      if (!breakNode) return NS_ERROR_NULL_POINTER;
      if (!splitDeepNode) return NS_ERROR_NULL_POINTER;
      res = nsEditor::GetNodeLocation(breakNode, &splitParentNode, &splitOffset);
      if (NS_FAILED(res)) return res;
      res = mEditor->SplitNodeDeep(splitDeepNode, splitParentNode, splitOffset,
                          &resultOffset, PR_FALSE, &leftNode, &rightNode);
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
      res = mEditor->MoveNode(breakNode, inlineParentNode, resultOffset);
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
  if (nsEditor::IsBlockNode(aNode)) return nsnull;
  nsCOMPtr<nsIDOMNode> inlineNode, node=aNode;

  while (node && mEditor->IsInlineNode(node))
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
    nsCOMPtr<nsISupports> isupports  = (dont_AddRef)(inArrayOfNodes->ElementAt(i));
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
nsHTMLEditRules::InsertTab(nsIDOMSelection *aSelection, 
                           nsString *outString)
{
  nsCOMPtr<nsIDOMNode> parentNode;
  PRInt32 offset;
  PRBool isPRE;
  
  nsresult res = mEditor->GetStartNodeAndOffset(aSelection, &parentNode, &offset);
  if (NS_FAILED(res)) return res;
  
  if (!parentNode) return NS_ERROR_FAILURE;
    
  res = mEditor->IsPreformatted(parentNode, &isPRE);
  if (NS_FAILED(res)) return res;
    
  if (isPRE)
  {
    outString->AssignWithConversion('\t');
  }
  else
  {
    // number of spaces should be a pref?
    // note that we dont play around with nbsps here anymore.  
    // let the AfterEdit whitespace cleanup code handle it.
    outString->AssignWithConversion("    ");
  }
  
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// ReturnInHeader: do the right thing for returns pressed in headers
//                       
nsresult 
nsHTMLEditRules::ReturnInHeader(nsIDOMSelection *aSelection, 
                                nsIDOMNode *aHeader, 
                                nsIDOMNode *aNode, 
                                PRInt32 aOffset)
{
  if (!aSelection || !aHeader || !aNode) return NS_ERROR_NULL_POINTER;  
  
  // remeber where the header is
  nsCOMPtr<nsIDOMNode> headerParent;
  PRInt32 offset;
  nsresult res = nsEditor::GetNodeLocation(aHeader, &headerParent, &offset);
  if (NS_FAILED(res)) return res;

  // split the header
  PRInt32 newOffset;
  res = mEditor->SplitNodeDeep( aHeader, aNode, aOffset, &newOffset);
  if (NS_FAILED(res)) return res;

  // if the leftand heading is empty, put a mozbr in it
  nsCOMPtr<nsIDOMNode> prevItem;
  mEditor->GetPriorHTMLSibling(aHeader, &prevItem);
  if (prevItem && nsHTMLEditUtils::IsHeader(prevItem))
  {
    PRBool bIsEmptyNode;
    res = mEditor->IsEmptyNode(prevItem, &bIsEmptyNode);
    if (NS_FAILED(res)) return res;
    if (bIsEmptyNode)
    {
      nsCOMPtr<nsIDOMNode> brNode;
      res = CreateMozBR(prevItem, 0, &brNode);
      if (NS_FAILED(res)) return res;
    }
  }
  
  // if the new (righthand) header node is empty, delete it
  PRBool isEmpty;
  res = IsEmptyBlock(aHeader, &isEmpty, PR_TRUE);
  if (NS_FAILED(res)) return res;
  if (isEmpty)
  {
    res = mEditor->DeleteNode(aHeader);
    if (NS_FAILED(res)) return res;
    // layout tells the caret to blink in a weird place
    // if we dont place a break after the header.
    nsCOMPtr<nsIDOMNode> sibling;
    res = mEditor->GetNextHTMLSibling(headerParent, offset+1, &sibling);
    if (NS_FAILED(res)) return res;
    if (!sibling || !nsHTMLEditUtils::IsBreak(sibling))
    {
      res = CreateMozBR(headerParent, offset+1, &sibling);
      if (NS_FAILED(res)) return res;
    }
    res = nsEditor::GetNodeLocation(sibling, &headerParent, &offset);
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
nsHTMLEditRules::ReturnInParagraph(nsIDOMSelection *aSelection, 
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
  if (mEditor->IsTextNode(aNode))
  {
    nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(aNode);
    PRUint32 strLength;
    res = textNode->GetLength(&strLength);
    if (NS_FAILED(res)) return res;
    
    // at beginning of text node?
    if (!aOffset)
    {
      // is there a BR prior to it?
      mEditor->GetPriorHTMLSibling(aNode, &sibling);
      if (!sibling) 
      {
        // no previous sib, so
        // just fall out to default of inserting a BR
        return res;
      }
      if (nsHTMLEditUtils::IsBreak(sibling)
          && !nsHTMLEditUtils::HasMozAttr(sibling))
      {
        PRInt32 newOffset;
        *aCancel = PR_TRUE;
        // split the paragraph
        res = mEditor->SplitNodeDeep( aPara, aNode, aOffset, &newOffset);
        if (NS_FAILED(res)) return res;
        // get rid of the break
        res = mEditor->DeleteNode(sibling);  
        if (NS_FAILED(res)) return res;
        // check both halves of para to see if we need mozBR
        res = InsertMozBRIfNeeded(aPara);
        if (NS_FAILED(res)) return res;
        res = mEditor->GetPriorHTMLSibling(aPara, &sibling);
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
      res = mEditor->GetNextHTMLSibling(aNode, &sibling);
      if (!sibling) 
      {
        // no next sib, so
        // just fall out to default of inserting a BR
        return res;
      }
      if (nsHTMLEditUtils::IsBreak(sibling)
          && !nsHTMLEditUtils::HasMozAttr(sibling))
      {
        PRInt32 newOffset;
        *aCancel = PR_TRUE;
        // split the paragraph
        res = mEditor->SplitNodeDeep(aPara, aNode, aOffset, &newOffset);
        if (NS_FAILED(res)) return res;
        // get rid of the break
        res = mEditor->DeleteNode(sibling);  
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
    res = mEditor->GetPriorHTMLNode(aNode, aOffset, &nearNode);
    if (NS_FAILED(res)) return res;
    if (!nearNode || !nsHTMLEditUtils::IsBreak(nearNode)
        || nsHTMLEditUtils::HasMozAttr(nearNode)) 
    {
      // is there a BR after to it?
      res = mEditor->GetNextHTMLNode(aNode, aOffset, &nearNode);
      if (NS_FAILED(res)) return res;
      if (!nearNode || !nsHTMLEditUtils::IsBreak(nearNode)
          || nsHTMLEditUtils::HasMozAttr(nearNode)) 
      {
        // just fall out to default of inserting a BR
        return res;
      }
    }
    // else remove sibling br and split para
    PRInt32 newOffset;
    *aCancel = PR_TRUE;
    // split the paragraph
    res = mEditor->SplitNodeDeep( aPara, aNode, aOffset, &newOffset);
    if (NS_FAILED(res)) return res;
    // get rid of the break
    res = mEditor->DeleteNode(nearNode);  
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
nsHTMLEditRules::ReturnInListItem(nsIDOMSelection *aSelection, 
                                  nsIDOMNode *aListItem, 
                                  nsIDOMNode *aNode, 
                                  PRInt32 aOffset)
{
  if (!aSelection || !aListItem || !aNode) return NS_ERROR_NULL_POINTER;
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
    res = nsEditor::GetNodeLocation(aListItem, &list, &itemOffset);
    if (NS_FAILED(res)) return res;
    res = nsEditor::GetNodeLocation(list, &listparent, &offset);
    if (NS_FAILED(res)) return res;
    
    // are we the last list item in the list?
    PRBool bIsLast;
    res = mEditor->IsLastEditableChild(aListItem, &bIsLast);
    if (NS_FAILED(res)) return res;
    if (!bIsLast)
    {
      // we need to split the list!
      nsCOMPtr<nsIDOMNode> tempNode;
      res = mEditor->SplitNode(list, itemOffset, getter_AddRefs(tempNode));
      if (NS_FAILED(res)) return res;
    }
    // are we in a sublist?
    if (nsHTMLEditUtils::IsList(listparent))  //in a sublist
    {
      // if so, move this list item out of this list and into the grandparent list
      res = mEditor->MoveNode(aListItem,listparent,offset+1);
      if (NS_FAILED(res)) return res;
      res = aSelection->Collapse(aListItem,0);
    }
    else
    {
      // otherwise kill this listitem
      res = mEditor->DeleteNode(aListItem);
      if (NS_FAILED(res)) return res;
      
      // time to insert a break
      nsCOMPtr<nsIDOMNode> brNode;
      res = CreateMozBR(listparent, offset+1, &brNode);
      if (NS_FAILED(res)) return res;
      
      // set selection to before the moz br
      aSelection->SetHint(PR_TRUE);
      res = aSelection->Collapse(listparent,offset+1);
    }
    return res;
  }
  
  // else we want a new list item at the same list level
  PRInt32 newOffset;
  res = mEditor->SplitNodeDeep( aListItem, aNode, aOffset, &newOffset);
  if (NS_FAILED(res)) return res;
  // hack: until I can change the damaged doc range code back to being
  // extra inclusive, I have to manually detect certain list items that
  // may be left empty.
  nsCOMPtr<nsIDOMNode> prevItem;
  mEditor->GetPriorHTMLSibling(aListItem, &prevItem);
  if (prevItem && nsHTMLEditUtils::IsListItem(prevItem))
  {
    PRBool bIsEmptyNode;
    res = mEditor->IsEmptyNode(prevItem, &bIsEmptyNode);
    if (NS_FAILED(res)) return res;
    if (bIsEmptyNode)
    {
      nsCOMPtr<nsIDOMNode> brNode;
      res = CreateMozBR(prevItem, 0, &brNode);
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
    nsCOMPtr<nsISupports> isupports  = (dont_AddRef)(arrayOfNodes->ElementAt(i));
    curNode = do_QueryInterface(isupports);
    res = nsEditor::GetNodeLocation(curNode, &curParent, &offset);
    if (NS_FAILED(res)) return res;

    // if the node is a table element or list item, dive inside
    if ( (nsHTMLEditUtils::IsTableElement(curNode) && !(nsHTMLEditUtils::IsTable(curNode))) || 
         nsHTMLEditUtils::IsListItem(curNode) )
    {
      curBlock = 0;  // forget any previous block
      // recursion time
      nsCOMPtr<nsISupportsArray> childArray;
      res = GetChildNodesForOperation(curNode, &childArray);
      if (NS_FAILED(res)) return res;
      res = MakeBlockquote(childArray);
      if (NS_FAILED(res)) return res;
    }
    
    // if the node has different parent than previous node,
    // further nodes in a new parent
    if (prevParent)
    {
      nsCOMPtr<nsIDOMNode> curParent;
      curNode->GetParentNode(getter_AddRefs(curParent));
      if (curParent != prevParent)
      {
        curBlock = 0;  // forget any previous blockquote node we were using
        prevParent = curParent;
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
      res = SplitAsNeeded(&quoteType, &curParent, &offset);
      if (NS_FAILED(res)) return res;
      res = mEditor->CreateNode(quoteType, curParent, offset, getter_AddRefs(curBlock));
      if (NS_FAILED(res)) return res;
    }
      
    PRUint32 blockLen;
    res = mEditor->GetLengthOfDOMNode(curBlock, blockLen);
    if (NS_FAILED(res)) return res;
    res = mEditor->MoveNode(curNode, curBlock, blockLen);
    if (NS_FAILED(res)) return res;
  }
  return res;
}



///////////////////////////////////////////////////////////////////////////
// ApplyBlockStyle:  do whatever it takes to make the list of nodes into 
//                   one or more blocks of type blockTag.  
//                       
nsresult 
nsHTMLEditRules::ApplyBlockStyle(nsISupportsArray *arrayOfNodes, const nsString *aBlockTag)
{
  // intent of this routine is to be used for converting to/from
  // headers, paragraphs, pre, and address.  Those blocks
  // that pretty much just contain inline things...
  
  if (!arrayOfNodes || !aBlockTag) return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;
  
  nsCOMPtr<nsIDOMNode> curNode, curParent, curBlock, newBlock;
  PRInt32 offset;
  PRUint32 listCount;
  PRBool bNoParent = PR_FALSE;
  
  // we special case an empty tag name to mean "remove block parents".
  // This is used for the "normal" paragraph style in mail-compose
  if (aBlockTag->IsEmpty() || aBlockTag->EqualsWithConversion("normal")) bNoParent = PR_TRUE;
  
  arrayOfNodes->Count(&listCount);
  
  PRUint32 i;
  for (i=0; i<listCount; i++)
  {
    // get the node to act on, and it's location
    nsCOMPtr<nsISupports> isupports  = (dont_AddRef)(arrayOfNodes->ElementAt(i));
    curNode = do_QueryInterface(isupports);
    res = nsEditor::GetNodeLocation(curNode, &curParent, &offset);
    if (NS_FAILED(res)) return res;
    nsAutoString curNodeTag;
    nsEditor::GetTagString(curNode, curNodeTag);
        
 
    // is it already the right kind of block?
    if (!bNoParent && curNodeTag == *aBlockTag)
    {
      curBlock = 0;  // forget any previous block used for previous inline nodes
      continue;  // do nothing to this block
    }
        
    // if curNode is a mozdiv, p, header, address, or pre, replace 
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
      if (bNoParent)
      {
        // make sure we have a normal br at end of block
        res = AddTerminatingBR(curNode);
        if (NS_FAILED(res)) return res;
        res = mEditor->RemoveContainer(curNode); 
      }
      else
      {
        res = mEditor->ReplaceContainer(curNode, &newBlock, *aBlockTag);
      }
      if (NS_FAILED(res)) return res;
    }
    else if ((curNodeTag.EqualsWithConversion("table"))      || 
             (curNodeTag.EqualsWithConversion("tbody"))      ||
             (curNodeTag.EqualsWithConversion("tr"))         ||
             (curNodeTag.EqualsWithConversion("td"))         ||
             (curNodeTag.EqualsWithConversion("ol"))         ||
             (curNodeTag.EqualsWithConversion("ul"))         ||
             (curNodeTag.EqualsWithConversion("li"))         ||
             (curNodeTag.EqualsWithConversion("blockquote")) ||
             (curNodeTag.EqualsWithConversion("div")))  // div's other than mozdivs
    {
      curBlock = 0;  // forget any previous block used for previous inline nodes
      // recursion time
      nsCOMPtr<nsISupportsArray> childArray;
      res = GetChildNodesForOperation(curNode, &childArray);
      if (NS_FAILED(res)) return res;
      res = ApplyBlockStyle(childArray, aBlockTag);
      if (NS_FAILED(res)) return res;
    }
    
    // if the node is a break, we honor it by putting further nodes in a new parent
    else if (curNodeTag.EqualsWithConversion("br"))
    {
      curBlock = 0;  // forget any previous block used for previous inline nodes
      if (!bNoParent)
      {
        res = mEditor->DeleteNode(curNode);
        if (NS_FAILED(res)) return res;
      }
    }
        
    
    // if curNode is inline, pull it into curBlock
    // note: it's assumed that consecutive inline nodes in the 
    // arrayOfNodes are actually members of the same block parent.
    // this happens to be true now as a side effect of how
    // arrayOfNodes is contructed, but some additional logic should
    // be added here if that should change
    
    else if (nsEditor::IsInlineNode(curNode) && !bNoParent)
    {
      // if curNode is a non editable, drop it if we are going to <pre>
      if ((aBlockTag->EqualsWithConversion("pre")) && (!mEditor->IsEditable(curNode)))
        continue; // do nothing to this block
      
      // if no curBlock, make one
      if (!curBlock)
      {
        res = SplitAsNeeded(aBlockTag, &curParent, &offset);
        if (NS_FAILED(res)) return res;
        res = mEditor->CreateNode(*aBlockTag, curParent, offset, getter_AddRefs(curBlock));
        if (NS_FAILED(res)) return res;
      }
      
      // if curNode is a Break, replace it with a return if we are going to <pre>
      // xxx floppy moose
 
      // this is a continuation of some inline nodes that belong together in
      // the same block item.  use curBlock
      PRUint32 blockLen;
      res = mEditor->GetLengthOfDOMNode(curBlock, blockLen);
      if (NS_FAILED(res)) return res;
      res = mEditor->MoveNode(curNode, curBlock, blockLen);
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
nsHTMLEditRules::SplitAsNeeded(const nsString *aTag, 
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
    if (mEditor->CanContainTag(parent, *aTag))
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
    res = mEditor->SplitNodeDeep(splitNode, *inOutParent, *inOutOffset, inOutOffset);
    if (NS_FAILED(res)) return res;
    *inOutParent = tagParent;
  }
  return res;
}      

///////////////////////////////////////////////////////////////////////////
// AddTerminatingBR:  place an ordinary br node at the end of aBlock,   
//                  if it doens't already have one.  If it has a moz-BR,
//                  simply convertit to a normal br.  
nsresult 
nsHTMLEditRules::AddTerminatingBR(nsIDOMNode *aBlock)
{
  if (!aBlock) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIDOMNode> last;
  nsresult res = mEditor->GetLastEditableLeaf(aBlock, &last);
  if (last && nsHTMLEditUtils::IsBreak(last))
  {
    if (nsHTMLEditUtils::IsMozBR(last))
    {
      // need to convert a br
      nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(last);
      res = mEditor->RemoveAttribute(elem, NS_ConvertASCIItoUCS2("type")); 
      if (NS_FAILED(res)) return res;
    }
    else // we have what we want, we're done
    {
      return res;
    }
  }
  else // need to add a br
  {
    PRUint32 len;
    nsCOMPtr<nsIDOMNode> brNode;
    res = mEditor->GetLengthOfDOMNode(aBlock, len);
    if (NS_FAILED(res)) return res;
    res = mEditor->CreateBR(aBlock, len, &brNode);
    if (NS_FAILED(res)) return res;
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
  res = nsEditor::GetNodeLocation(aNodeLeft, &parent, &parOffset);
  if (NS_FAILED(res)) return res;
  aNodeRight->GetParentNode(getter_AddRefs(rightParent));

  // if they don't have the same parent, first move the 'right' node 
  // to after the 'left' one
  if (parent != rightParent)
  {
    res = mEditor->MoveNode(aNodeRight, parent, parOffset);
    if (NS_FAILED(res)) return res;
  }
  
  // defaults for outParams
  *aOutMergeParent = aNodeRight;
  res = mEditor->GetLengthOfDOMNode(aNodeLeft, *((PRUint32*)aOutMergeOffset));
  if (NS_FAILED(res)) return res;

  // seperate join rules for differing blocks
  if (nsHTMLEditUtils::IsParagraph(aNodeLeft))
  {
    // for para's, merge deep & add a <br> after merging
    res = mEditor->JoinNodeDeep(aNodeLeft, aNodeRight, aOutMergeParent, aOutMergeOffset);
    if (NS_FAILED(res)) return res;
    // now we need to insert a br.  
    nsCOMPtr<nsIDOMNode> brNode;
    res = mEditor->CreateBR(*aOutMergeParent, *aOutMergeOffset, &brNode);
    if (NS_FAILED(res)) return res;
    res = nsEditor::GetNodeLocation(brNode, aOutMergeParent, aOutMergeOffset);
    if (NS_FAILED(res)) return res;
    (*aOutMergeOffset)++;
    return res;
  }
  else if (nsHTMLEditUtils::IsList(aNodeLeft)
           || mEditor->IsTextNode(aNodeLeft))
  {
    // for list's, merge shallow (wouldn't want to combine list items)
    res = mEditor->JoinNodes(aNodeLeft, aNodeRight, parent);
    if (NS_FAILED(res)) return res;
    return res;
  }
  else
  {
    // remember the last left child, and firt right child
    nsCOMPtr<nsIDOMNode> lastLeft, firstRight;
    res = mEditor->GetLastEditableChild(aNodeLeft, &lastLeft);
    if (NS_FAILED(res)) return res;
    res = mEditor->GetFirstEditableChild(aNodeRight, &firstRight);
    if (NS_FAILED(res)) return res;

    // for list items, divs, etc, merge smart
    res = mEditor->JoinNodes(aNodeLeft, aNodeRight, parent);
    if (NS_FAILED(res)) return res;

    if (lastLeft && firstRight && mEditor->NodesSameType(lastLeft, firstRight))
    {
      return JoinNodesSmart(lastLeft, firstRight, aOutMergeParent, aOutMergeOffset);
    }
  }
  return res;
}


nsresult 
nsHTMLEditRules::GetTopEnclosingMailCite(nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *aOutCiteNode)
{
  // check parms
  if (!aNode || !aOutCiteNode) 
    return NS_ERROR_NULL_POINTER;
  
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> node, parentNode;
  node = do_QueryInterface(aNode);
  
  while (node)
  {
    if (nsHTMLEditUtils::IsMailCite(node)) *aOutCiteNode = node;
    if (nsHTMLEditUtils::IsBody(node)) break;
    
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
  nsEmptyFunctor functor(mEditor);
  nsDOMIterator iter;
  nsresult res = iter.Init(mDocChangeRange);
  if (NS_FAILED(res)) return res;
  res = iter.MakeList(functor, &arrayOfNodes);
  if (NS_FAILED(res)) return res;

  // put moz-br's into these empty li's and td's
  res = arrayOfNodes->Count(&nodeCount);
  if (NS_FAILED(res)) return res;
  for (j = 0; j < nodeCount; j++)
  {
    isupports = (dont_AddRef)(arrayOfNodes->ElementAt(0));
    nsCOMPtr<nsIDOMNode> brNode, theNode( do_QueryInterface(isupports ) );
    arrayOfNodes->RemoveElementAt(0);
    res = CreateMozBR(theNode, 0, &brNode);
    if (NS_FAILED(res)) return res;
  }
  
  return res;
}


nsresult 
nsHTMLEditRules::AdjustWhitespace(nsIDOMSelection *aSelection)
{
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  nsCOMPtr<nsISupports> isupports;
  PRUint32 nodeCount,j;
  nsresult res;

  nsAutoSelectionReset selectionResetter(aSelection, mEditor);
  
  // special case for mDocChangeRange entirely in one text node.
  // This is an efficiency hack for normal typing in the editor.
  nsCOMPtr<nsIDOMNode> startNode, endNode;
  PRInt32 startOffset, endOffset;
  res = mDocChangeRange->GetStartContainer(getter_AddRefs(startNode));
  if (NS_FAILED(res)) return res;
  res = mDocChangeRange->GetStartOffset(&startOffset);
  if (NS_FAILED(res)) return res;
  res = mDocChangeRange->GetEndContainer(getter_AddRefs(endNode));
  if (NS_FAILED(res)) return res;
  res = mDocChangeRange->GetEndOffset(&endOffset);
  if (NS_FAILED(res)) return res;

  if (startNode == endNode)
  {
    nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(startNode);
    if (nodeAsText)
    {
      res = DoTextNodeWhitespace(nodeAsText, startOffset, endOffset);
      return res;
    }
  }
  
  // gather up a list of text nodes
  nsEditableTextFunctor functor(mEditor);
  nsDOMIterator iter;
  res = iter.Init(mDocChangeRange);
  if (NS_FAILED(res)) return res;
  res = iter.MakeList(functor, &arrayOfNodes);
  if (NS_FAILED(res)) return res;
    
  // now adjust whitespace on node we found
  res = arrayOfNodes->Count(&nodeCount);
  if (NS_FAILED(res)) return res;
  for (j = 0; j < nodeCount; j++)
  {
    isupports = (dont_AddRef)(arrayOfNodes->ElementAt(0));
    nsCOMPtr<nsIDOMCharacterData> textNode( do_QueryInterface(isupports ) );
    arrayOfNodes->RemoveElementAt(0);
    res = DoTextNodeWhitespace(textNode, -1, -1);
    if (NS_FAILED(res)) return res;
  }
  
  return res;
}


nsresult 
nsHTMLEditRules::AdjustSelection(nsIDOMSelection *aSelection, nsIEditor::EDirection aAction)
{
  if (!aSelection) return NS_ERROR_NULL_POINTER;
  
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
  res = mEditor->GetStartNodeAndOffset(aSelection, &selNode, &selOffset);
  if (NS_FAILED(res)) return res;
  temp = selNode;
  
  // are we in an editable node?
  while (!mEditor->IsEditable(selNode))
  {
    // scan up the tree until we find an editable place to be
    res = nsEditor::GetNodeLocation(temp, &selNode, &selOffset);
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
  res = mEditor->GetPriorHTMLNode(selNode, selOffset, &nearNode);
  if (NS_FAILED(res)) return res;
  if (!nearNode) return res;
  
  // is nearNode also a descendant of same block?
  nsCOMPtr<nsIDOMNode> block, nearBlock;
  if (mEditor->IsBlockNode(selNode)) block = selNode;
  else block = mEditor->GetBlockNodeParent(selNode);
  nearBlock = mEditor->GetBlockNodeParent(nearNode);
  if (block == nearBlock)
  {
    if (nearNode && nsHTMLEditUtils::IsBreak(nearNode)
        && !nsHTMLEditUtils::IsMozBR(nearNode))
    {
      PRBool bIsLast;
      res = mEditor->IsLastEditableChild(nearNode, &bIsLast);
      if (NS_FAILED(res)) return res;
      if (bIsLast)
      {
        // need to insert special moz BR. Why?  Because if we don't
        // the user will see no new line for the break.  Also, things
        // like table cells won't grow in height.
        nsCOMPtr<nsIDOMNode> brNode;
        res = CreateMozBR(selNode, selOffset, &brNode);
        if (NS_FAILED(res)) return res;
        res = nsEditor::GetNodeLocation(brNode, &selNode, &selOffset);
        if (NS_FAILED(res)) return res;
        // selection stays *before* moz-br, sticking to it
        aSelection->SetHint(PR_TRUE);
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
        res = mEditor->GetNextHTMLNode(nearNode, &nextNode);
        if (NS_FAILED(res)) return res;
        res = mEditor->GetNextHTMLSibling(nearNode, &nextNode);
        if (NS_FAILED(res)) return res;
        if (nextNode && mEditor->IsBlockNode(nextNode))
        {
          // need to insert special moz BR. Why?  Because if we don't
          // the user will see no new line for the break.  
          nsCOMPtr<nsIDOMNode> brNode;
          res = CreateMozBR(selNode, selOffset, &brNode);
          if (NS_FAILED(res)) return res;
          res = nsEditor::GetNodeLocation(brNode, &selNode, &selOffset);
          if (NS_FAILED(res)) return res;
          // selection stays *before* moz-br, sticking to it
          aSelection->SetHint(PR_TRUE);
          res = aSelection->Collapse(selNode,selOffset);
          if (NS_FAILED(res)) return res;
        }
      }
    }
  }

  // we aren't in a textnode: are we adjacent to a break or an image?
  res = mEditor->GetPriorHTMLSibling(selNode, selOffset, &nearNode);
  if (NS_FAILED(res)) return res;
  if (nearNode && (nsHTMLEditUtils::IsBreak(nearNode)
                   || nsHTMLEditUtils::IsImage(nearNode)))
    return NS_OK; // this is a good place for the caret to be
  res = mEditor->GetNextHTMLSibling(selNode, selOffset, &nearNode);
  if (NS_FAILED(res)) return res;
  if (nearNode && (nsHTMLEditUtils::IsBreak(nearNode)
                   || nsHTMLEditUtils::IsImage(nearNode)))
    return NS_OK; // this is a good place for the caret to be

  // look for a nearby text node.
  // prefer the correct direction.
  res = FindNearSelectableNode(selNode, selOffset, aAction, &nearNode);
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
    res = nsEditor::GetNodeLocation(nearNode, &selNode, &selOffset);
    if (NS_FAILED(res)) return res;
    if (aAction == nsIEditor::ePrevious) selOffset++;  // want to be beyond it if we backed up to it
    res = aSelection->Collapse(selNode, selOffset);
  }

  return res;
}

nsresult
nsHTMLEditRules::FindNearSelectableNode(nsIDOMNode *aSelNode, 
                                        PRInt32 aSelOffset, 
                                        nsIEditor::EDirection aDirection,
                                        nsCOMPtr<nsIDOMNode> *outSelectableNode)
{
  if (!aSelNode || !outSelectableNode) return NS_ERROR_NULL_POINTER;
  *outSelectableNode = nsnull;
  nsresult res = NS_OK;
  
  nsCOMPtr<nsIDOMNode> nearNode, curNode;
  if (aDirection == nsIEditor::ePrevious)
    res = mEditor->GetPriorHTMLNode(aSelNode, aSelOffset, &nearNode);
  else
    res = mEditor->GetNextHTMLNode(aSelNode, aSelOffset, &nearNode);
  if (NS_FAILED(res)) return res;
  
  // scan in the right direction until we find an eligible text node,
  // but dont cross any breaks, images, or table elements.
  while (nearNode && !(mEditor->IsTextNode(nearNode)
                       || nsHTMLEditUtils::IsBreak(nearNode)
                       || nsHTMLEditUtils::IsImage(nearNode)))
  {
    curNode = nearNode;
    if (aDirection == nsIEditor::ePrevious)
      res = mEditor->GetPriorHTMLNode(curNode, &nearNode);
    else
      res = mEditor->GetNextHTMLNode(curNode, &nearNode);
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
  if (!aNode1 || !aNode2 || !aResult) NS_ERROR_NULL_POINTER;

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
  res = nsComponentManager::CreateInstance(kContentIteratorCID,
                                        nsnull,
                                        NS_GET_IID(nsIContentIterator), 
                                        getter_AddRefs(iter));
  if (NS_FAILED(res)) return res;
  
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
      res = mEditor->IsEmptyNode(node, &bIsEmptyNode, PR_FALSE, PR_TRUE);
      if (NS_FAILED(res)) return res;
      if (bIsEmptyNode && !nsHTMLEditUtils::IsBody(node))
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
      isupports = (dont_AddRef)(arrayOfNodes->ElementAt(0));
      nsCOMPtr<nsIDOMNode> delNode( do_QueryInterface(isupports ) );
      arrayOfNodes->RemoveElementAt(0);
      res = mEditor->DeleteNode(delNode);
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
  
  nsCOMPtr<nsIDOMSelection>selection;
  nsresult res = mEditor->GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  
  nsCOMPtr<nsIEnumerator> enumerator;
  res = selection->GetEnumerator(getter_AddRefs(enumerator));
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
  
  res = mEditor->IsPreformatted(node,&isPRE);
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
        res = mEditor->CreateTxnForDeleteText(aTextNode, aStart+runStart, runEnd-runStart, (DeleteTextTxn**)&txn);
        if (NS_FAILED(res))  return res; 
        if (!txn)  return NS_ERROR_OUT_OF_MEMORY;
        res = mEditor->Do(txn); 
        if (NS_FAILED(res))  return res; 
        // The transaction system (if any) has taken ownwership of txn
        NS_IF_RELEASE(txn);
        
        // insert the new run
        res = mEditor->CreateTxnForInsertText(newStr, aTextNode, aStart+runStart, (InsertTextTxn**)&txn);
        if (NS_FAILED(res))  return res; 
        if (!txn)  return NS_ERROR_OUT_OF_MEMORY;
        res = mEditor->Do(txn);
        // The transaction system (if any) has taken ownwership of txns.
        NS_IF_RELEASE(txn);
      } 
      runStart = -1; // reset our run     
    }
    j++; // next char please!
  } while (j<tempString.Length());

  return res;
}


nsresult 
nsHTMLEditRules::ConvertWhitespace(const nsString & inString, nsString & outString)
{
  PRUint32 j,len = inString.Length();
  switch (len)
  {
    case 0:
      outString.SetLength(0);
      return NS_OK;
    case 1:
      if (inString.EqualsWithConversion("\n"))   // a bit of a hack: don't convert single newlines that 
        outString.AssignWithConversion("\n");     // dont have whitespace adjacent.  This is to preserve 
      else                    // html source formatting to some degree.
        outString.AssignWithConversion(" ");
      return NS_OK;
    case 2:
      outString.Assign((PRUnichar)nbsp);
      outString.AppendWithConversion(" ");
      return NS_OK;
    case 3:
      outString.AssignWithConversion(" ");
      outString += (PRUnichar)nbsp;
      outString.AppendWithConversion(" ");
      return NS_OK;
  }
  if (len%2)  // length is odd
  {
    for (j=0;j<len;j++)
    {
      if (!(j & 0x01)) outString.AppendWithConversion(" ");      // even char
      else outString += (PRUnichar)nbsp; // odd char
    }
  }
  else
  {
    outString.AssignWithConversion(" ");
    outString += (PRUnichar)nbsp;
    outString += (PRUnichar)nbsp;
    for (j=0;j<len-3;j++)
    {
      if (!(j%2)) outString.AppendWithConversion(" ");      // even char
      else outString += (PRUnichar)nbsp; // odd char
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
  nsresult res = nsEditor::GetNodeLocation(curNode, &curParent, &offset);
  if (NS_FAILED(res)) return res;
    
  if (!nsHTMLEditUtils::IsListItem(curNode))
    return NS_ERROR_FAILURE;
    
  // if it's first or last list item, dont need to split the list
  // otherwise we do.
  nsCOMPtr<nsIDOMNode> curParPar;
  PRInt32 parOffset;
  res = nsEditor::GetNodeLocation(curParent, &curParPar, &parOffset);
  if (NS_FAILED(res)) return res;
  
  PRBool bIsFirstListItem;
  res = mEditor->IsFirstEditableChild(curNode, &bIsFirstListItem);
  if (NS_FAILED(res)) return res;

  PRBool bIsLastListItem;
  res = mEditor->IsLastEditableChild(curNode, &bIsLastListItem);
  if (NS_FAILED(res)) return res;
    
  if (!bIsFirstListItem && !bIsLastListItem)
  {
    // split the list
    nsCOMPtr<nsIDOMNode> newBlock;
    res = mEditor->SplitNode(curParent, offset, getter_AddRefs(newBlock));
    if (NS_FAILED(res)) return res;
  }
  
  if (!bIsFirstListItem) parOffset++;
  
  res = mEditor->MoveNode(curNode, curParPar, parOffset);
  if (NS_FAILED(res)) return res;
    
  // unwrap list item contents if they are no longer in a list
  if (!nsHTMLEditUtils::IsList(curParPar)
      && nsHTMLEditUtils::IsListItem(curNode)) 
  {
    res = AddTerminatingBR(curNode);
    if (NS_FAILED(res)) return res;
    res = mEditor->RemoveContainer(curNode);
    if (NS_FAILED(res)) return res;
    *aOutOfList = PR_TRUE;
  }
  return res;
}


nsresult 
nsHTMLEditRules::ConfirmSelectionInBody()
{
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMElement> bodyElement;   
  nsCOMPtr<nsIDOMNode> bodyNode; 
  
  // get the body  
  res = mEditor->GetRootElement(getter_AddRefs(bodyElement));
  if (NS_FAILED(res)) return res;
  if (!bodyElement) return NS_ERROR_UNEXPECTED;
  bodyNode = do_QueryInterface(bodyElement);

  // get the selection
  nsCOMPtr<nsIDOMSelection>selection;
  res = mEditor->GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  
  // get the selection start location
  nsCOMPtr<nsIDOMNode> selNode, temp, parent;
  PRInt32 selOffset;
  res = mEditor->GetStartNodeAndOffset(selection, &selNode, &selOffset);
  if (NS_FAILED(res)) return res;
  temp = selNode;
  
  // check that selNode is inside body
  while (temp && !nsHTMLEditUtils::IsBody(temp))
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
  res = mEditor->GetEndNodeAndOffset(selection, &selNode, &selOffset);
  if (NS_FAILED(res)) return res;
  temp = selNode;
  
  // check that selNode is inside body
  while (temp && !nsHTMLEditUtils::IsBody(temp))
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
    if (result < 0)  // negative result means aRange start is before mDocChangeRange start
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
    if (result > 0)  // positive result means aRange end is after mDocChangeRange end
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
  if (!mEditor->IsBlockNode(aNode)) return NS_OK;
  
  PRBool isEmpty;
  nsCOMPtr<nsIDOMNode> brNode;
  nsresult res = mEditor->IsEmptyNode(aNode, &isEmpty);
  if (NS_FAILED(res)) return res;
  if (isEmpty)
  {
    res = CreateMozBR(aNode, 0, &brNode);
  }
  return res;
}

#ifdef XP_MAC
#pragma mark -
#pragma mark  nsIEditActionListener methods 
#pragma mark -
#endif

NS_IMETHODIMP 
nsHTMLEditRules::WillCreateNode(const nsString& aTag, nsIDOMNode *aParent, PRInt32 aPosition)
{
  return NS_OK;  
}

NS_IMETHODIMP 
nsHTMLEditRules::DidCreateNode(const nsString& aTag, 
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
nsHTMLEditRules::WillInsertText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, const nsString &aString)
{
  return NS_OK;  
}


NS_IMETHODIMP 
nsHTMLEditRules::DidInsertText(nsIDOMCharacterData *aTextNode, 
                                  PRInt32 aOffset, 
                                  const nsString &aString, 
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
nsHTMLEditRules::WillDeleteSelection(nsIDOMSelection *aSelection)
{
  if (!mListenerEnabled) return NS_OK;
  // get the (collapsed) selection location
  nsCOMPtr<nsIDOMNode> selNode;
  PRInt32 selOffset;

  nsresult res = mEditor->GetStartNodeAndOffset(aSelection, &selNode, &selOffset);
  if (NS_FAILED(res)) return res;
  res = mUtilRange->SetStart(selNode, selOffset);
  if (NS_FAILED(res)) return res;
  res = mEditor->GetEndNodeAndOffset(aSelection, &selNode, &selOffset);
  if (NS_FAILED(res)) return res;
  res = mUtilRange->SetEnd(selNode, selOffset);
  if (NS_FAILED(res)) return res;
  res = UpdateDocChangeRange(mUtilRange);
  return res;  
}

NS_IMETHODIMP
nsHTMLEditRules::DidDeleteSelection(nsIDOMSelection *aSelection)
{
  return NS_OK;
}









