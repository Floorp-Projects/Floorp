/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL") you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsHTMLEditRules.h"
#include "nsEditor.h"
#include "PlaceholderTxn.h"
#include "InsertTextTxn.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMSelection.h"
#include "nsIDOMRange.h"
#include "nsIDOMCharacterData.h"
#include "nsIEnumerator.h"

const static char* kMOZEditorBogusNodeAttr="MOZ_EDITOR_BOGUS_NODE";
const static char* kMOZEditorBogusNodeValue="TRUE";

static NS_DEFINE_IID(kPlaceholderTxnIID,  PLACEHOLDER_TXN_IID);

nsIAtom *nsHTMLEditRules::sAAtom;
nsIAtom *nsHTMLEditRules::sAddressAtom;
nsIAtom *nsHTMLEditRules::sBigAtom;
nsIAtom *nsHTMLEditRules::sBlinkAtom;
nsIAtom *nsHTMLEditRules::sBAtom;
nsIAtom *nsHTMLEditRules::sCiteAtom;
nsIAtom *nsHTMLEditRules::sCodeAtom;
nsIAtom *nsHTMLEditRules::sDfnAtom;
nsIAtom *nsHTMLEditRules::sEmAtom;
nsIAtom *nsHTMLEditRules::sFontAtom;
nsIAtom *nsHTMLEditRules::sIAtom;
nsIAtom *nsHTMLEditRules::sKbdAtom;
nsIAtom *nsHTMLEditRules::sKeygenAtom;
nsIAtom *nsHTMLEditRules::sNobrAtom;
nsIAtom *nsHTMLEditRules::sSAtom;
nsIAtom *nsHTMLEditRules::sSampAtom;
nsIAtom *nsHTMLEditRules::sSmallAtom;
nsIAtom *nsHTMLEditRules::sSpacerAtom;
nsIAtom *nsHTMLEditRules::sSpanAtom;      
nsIAtom *nsHTMLEditRules::sStrikeAtom;
nsIAtom *nsHTMLEditRules::sStrongAtom;
nsIAtom *nsHTMLEditRules::sSubAtom;
nsIAtom *nsHTMLEditRules::sSupAtom;
nsIAtom *nsHTMLEditRules::sTtAtom;
nsIAtom *nsHTMLEditRules::sUAtom;
nsIAtom *nsHTMLEditRules::sVarAtom;
nsIAtom *nsHTMLEditRules::sWbrAtom;

PRInt32 nsHTMLEditRules::sInstanceCount;

/********************************************************
 *  Constructor/Destructor 
 ********************************************************/

nsHTMLEditRules::nsHTMLEditRules()
{
  if (sInstanceCount <= 0)
  {
    sAAtom = NS_NewAtom("a");
    sAddressAtom = NS_NewAtom("address");
    sBigAtom = NS_NewAtom("big");
    sBlinkAtom = NS_NewAtom("blink");
    sBAtom = NS_NewAtom("b");
    sCiteAtom = NS_NewAtom("cite");
    sCodeAtom = NS_NewAtom("code");
    sDfnAtom = NS_NewAtom("dfn");
    sEmAtom = NS_NewAtom("em");
    sFontAtom = NS_NewAtom("font");
    sIAtom = NS_NewAtom("i");
    sKbdAtom = NS_NewAtom("kbd");
    sKeygenAtom = NS_NewAtom("keygen");
    sNobrAtom = NS_NewAtom("nobr");
    sSAtom = NS_NewAtom("s");
    sSampAtom = NS_NewAtom("samp");
    sSmallAtom = NS_NewAtom("small");
    sSpacerAtom = NS_NewAtom("spacer");
    sSpanAtom = NS_NewAtom("span");      
    sStrikeAtom = NS_NewAtom("strike");
    sStrongAtom = NS_NewAtom("strong");
    sSubAtom = NS_NewAtom("sub");
    sSupAtom = NS_NewAtom("sup");
    sTtAtom = NS_NewAtom("tt");
    sUAtom = NS_NewAtom("u");
    sVarAtom = NS_NewAtom("var");
    sWbrAtom = NS_NewAtom("wbr");
  }

  ++sInstanceCount;
}

nsHTMLEditRules::~nsHTMLEditRules()
{
  if (sInstanceCount <= 1)
  {
    NS_IF_RELEASE(sAAtom);
    NS_IF_RELEASE(sAddressAtom);
    NS_IF_RELEASE(sBigAtom);
    NS_IF_RELEASE(sBlinkAtom);
    NS_IF_RELEASE(sBAtom);
    NS_IF_RELEASE(sCiteAtom);
    NS_IF_RELEASE(sCodeAtom);
    NS_IF_RELEASE(sDfnAtom);
    NS_IF_RELEASE(sEmAtom);
    NS_IF_RELEASE(sFontAtom);
    NS_IF_RELEASE(sIAtom);
    NS_IF_RELEASE(sKbdAtom);
    NS_IF_RELEASE(sKeygenAtom);
    NS_IF_RELEASE(sNobrAtom);
    NS_IF_RELEASE(sSAtom);
    NS_IF_RELEASE(sSampAtom);
    NS_IF_RELEASE(sSmallAtom);
    NS_IF_RELEASE(sSpacerAtom);
    NS_IF_RELEASE(sSpanAtom);      
    NS_IF_RELEASE(sStrikeAtom);
    NS_IF_RELEASE(sStrongAtom);
    NS_IF_RELEASE(sSubAtom);
    NS_IF_RELEASE(sSupAtom);
    NS_IF_RELEASE(sTtAtom);
    NS_IF_RELEASE(sUAtom);
    NS_IF_RELEASE(sVarAtom);
    NS_IF_RELEASE(sWbrAtom);
  }

  --sInstanceCount;
}


/********************************************************
 *  Public methods 
 ********************************************************/

NS_IMETHODIMP 
nsHTMLEditRules::WillDoAction(nsIDOMSelection *aSelection, 
                              nsRulesInfo *aInfo, PRBool *aCancel)
{
  if (!aSelection || !aInfo) 
    return NS_ERROR_NULL_POINTER;
    
  // my kingdom for dynamic cast
  nsTextRulesInfo *info = NS_STATIC_CAST(nsTextRulesInfo*, aInfo);
    
  switch (info->action)
  {
    case kInsertText:
      return WillInsertText(aSelection, 
                            aCancel, 
                            info->placeTxn, 
                            info->inString,
                            info->outString,
                            info->typeInState);
    case kInsertBreak:
      return WillInsertBreak(aSelection, aCancel);
  }
  return nsTextEditRules::WillDoAction(aSelection, aInfo, aCancel);
}
  
NS_IMETHODIMP 
nsHTMLEditRules::DidDoAction(nsIDOMSelection *aSelection,
                             nsRulesInfo *aInfo, nsresult aResult)
{
  if (!aSelection || !aInfo) 
    return NS_ERROR_NULL_POINTER;
    
  // my kingdom for dynamic cast
  nsTextRulesInfo *info = NS_STATIC_CAST(nsTextRulesInfo*, aInfo);
    
  switch (info->action)
  {
    case kInsertText:
      return DidInsertText(aSelection, aResult);
    case kInsertBreak:
      return DidInsertBreak(aSelection, aResult);
  }
  return nsTextEditRules::DidDoAction(aSelection, aInfo, aResult);
}
  

/********************************************************
 *  Protected rules methods 
 ********************************************************/
 
nsresult
nsHTMLEditRules::WillInsertText(nsIDOMSelection  *aSelection, 
                                PRBool          *aCancel,
                                PlaceholderTxn **aTxn,
                                const nsString *inString,
                                nsString       *outString,
                                TypeInState    typeInState)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;

  // XXX - need to handle strings of length >1 with embedded tabs or spaces
  
  // is it a tab?
  if (*inString == "\t" )
    return InsertTab(aSelection,aCancel,aTxn,outString);
  // is it a space?
  if (*inString == " ")
    return InsertSpace(aSelection,aCancel,aTxn,outString);
  
  // otherwise, return nsTextEditRules version
  return nsTextEditRules::WillInsertText(aSelection, 
                                         aCancel, 
                                         aTxn,
                                         inString,
                                         outString,
                                         typeInState);
}

nsresult
nsHTMLEditRules::DidInsertText(nsIDOMSelection *aSelection, 
                               nsresult aResult)
{
  // for now, return nsTextEditRules version
  return nsTextEditRules::DidInsertText(aSelection, aResult);
}

nsresult
nsHTMLEditRules::WillInsertBreak(nsIDOMSelection *aSelection, PRBool *aCancel)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  return WillInsert(aSelection, aCancel);
}

// XXX: this code is all experimental, and has no effect on the content model yet
//      the point here is to collapse adjacent BR's into P's
nsresult
nsHTMLEditRules::DidInsertBreak(nsIDOMSelection *aSelection, nsresult aResult)
{
  nsresult result = aResult;  // if aResult is an error, we return it.
  if (!aSelection) { return NS_ERROR_NULL_POINTER; }
  PRBool isCollapsed;
  aSelection->IsCollapsed(&isCollapsed);
  NS_ASSERTION(PR_TRUE==isCollapsed, "selection not collapsed after insert break.");
  // if the insert break resulted in consecutive BR tags, 
  // collapse the two BR tags into a single P
  if (NS_SUCCEEDED(result)) 
  {
    nsCOMPtr<nsIEnumerator> enumerator;
    enumerator = do_QueryInterface(aSelection,&result);
    if (enumerator)
    {
      enumerator->First(); 
      nsISupports *currentItem;
      result = enumerator->CurrentItem(&currentItem);
      if ((NS_SUCCEEDED(result)) && currentItem)
      {
        result = NS_ERROR_UNEXPECTED; 
        nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
        if (range)
        {
          nsIAtom *brTag = NS_NewAtom("BR");
          nsCOMPtr<nsIDOMNode> startNode;
          result = range->GetStartParent(getter_AddRefs(startNode));
          if ((NS_SUCCEEDED(result)) && startNode)
          {
            PRInt32 offset;
            range->GetStartOffset(&offset);
            nsCOMPtr<nsIDOMNodeList>startNodeChildren;
            result = startNode->GetChildNodes(getter_AddRefs(startNodeChildren));
            if ((NS_SUCCEEDED(result)) && startNodeChildren)
            {              
              nsCOMPtr<nsIDOMNode> selectedNode;
              result = startNodeChildren->Item(offset, getter_AddRefs(selectedNode));
              if ((NS_SUCCEEDED(result)) && selectedNode)
              {
                nsCOMPtr<nsIDOMNode> prevNode;
                result = selectedNode->GetPreviousSibling(getter_AddRefs(prevNode));
                if ((NS_SUCCEEDED(result)) && prevNode)
                {
                  if (PR_TRUE==NodeIsType(prevNode, brTag))
                  { // the previous node is a BR, check it's siblings
                    nsCOMPtr<nsIDOMNode> leftNode;
                    result = prevNode->GetPreviousSibling(getter_AddRefs(leftNode));
                    if ((NS_SUCCEEDED(result)) && leftNode)
                    {
                      if (PR_TRUE==NodeIsType(leftNode, brTag))
                      { // left sibling is also a BR, collapse
                        printf("1\n");
                      }
                      else
                      {
                        if (PR_TRUE==NodeIsType(selectedNode, brTag))
                        { // right sibling is also a BR, collapse
                          printf("2\n");
                        }
                      }
                    }
                  }
                }
                // now check the next node from selectedNode
                nsCOMPtr<nsIDOMNode> nextNode;
                result = selectedNode->GetNextSibling(getter_AddRefs(nextNode));
                if ((NS_SUCCEEDED(result)) && nextNode)
                {
                  if (PR_TRUE==NodeIsType(nextNode, brTag))
                  { // the previous node is a BR, check it's siblings
                    nsCOMPtr<nsIDOMNode> rightNode;
                    result = nextNode->GetNextSibling(getter_AddRefs(rightNode));
                    if ((NS_SUCCEEDED(result)) && rightNode)
                    {
                      if (PR_TRUE==NodeIsType(rightNode, brTag))
                      { // right sibling is also a BR, collapse
                        printf("3\n");
                      }
                      else
                      {
                        if (PR_TRUE==NodeIsType(selectedNode, brTag))
                        { // left sibling is also a BR, collapse
                          printf("4\n");
                        }
                      }
                    }
                  }
                }
              }
            }
          }
          NS_RELEASE(brTag);
        }
      }
    }
  }
  return result;
}



/********************************************************
 *  helper methods 
 ********************************************************/
 
PRBool
nsHTMLEditRules::IsBlockNode(nsCOMPtr <nsIContent> aContent)
{
  nsIAtom* atom = nsnull;
  PRBool result;
  
  aContent->GetTag(atom);

  if (!atom)
    return PR_TRUE;

  if (sAAtom != atom &&
          sAddressAtom != atom &&
          sBigAtom != atom &&
          sBlinkAtom != atom &&
          sBAtom != atom &&
          sCiteAtom != atom &&
          sCodeAtom != atom &&
          sDfnAtom != atom &&
          sEmAtom != atom &&
          sFontAtom != atom &&
          sIAtom != atom &&
          sKbdAtom != atom &&
          sKeygenAtom != atom &&
          sNobrAtom != atom &&
          sSAtom != atom &&
          sSampAtom != atom &&
          sSmallAtom != atom &&
          sSpacerAtom != atom &&
          sSpanAtom != atom &&
          sStrikeAtom != atom &&
          sStrongAtom != atom &&
          sSubAtom != atom &&
          sSupAtom != atom &&
          sTtAtom != atom &&
          sUAtom != atom &&
          sVarAtom != atom &&
          sWbrAtom != atom)
   {
     result = PR_TRUE;
   }
   else
   {
     result = PR_FALSE;
   }
   NS_RELEASE(atom);
   return result;
}

nsCOMPtr<nsIContent>
nsHTMLEditRules::GetBlockNodeParent(nsCOMPtr<nsIContent> aContent)
{
  nsCOMPtr<nsIContent> p;

  if (NS_FAILED(aContent->GetParent(*getter_AddRefs(p))))  // no parent, ran off top of tree
    return aContent;

  nsCOMPtr<nsIContent> tmp;

  while (p && !IsBlockNode(p))
  {
    if (NS_FAILED(p->GetParent(*getter_AddRefs(tmp)))) // no parent, ran off top of tree
      return p;

    p = tmp;
  }
  return p;
}

///////////////////////////////////////////////////////////////////////////
// GetStartNode: returns whatever the start parent is of the first range
//               in the selection.
nsCOMPtr<nsIDOMNode> 
nsHTMLEditRules::GetStartNode(nsIDOMSelection *aSelection)
{
  nsCOMPtr<nsIDOMNode> startNode;
  nsCOMPtr<nsIEnumerator> enumerator;
  enumerator = do_QueryInterface(aSelection);
  if (!enumerator) 
    return startNode;
  enumerator->First(); 
  nsISupports *currentItem;
  if ((NS_FAILED(enumerator->CurrentItem(&currentItem))) || !currentItem)
    return startNode;

  nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
  if (!range)
    return startNode;
  range->GetStartParent(getter_AddRefs(startNode));
  return startNode;
}


///////////////////////////////////////////////////////////////////////////
// IsPreformatted: checks the style info for the node for the preformatted
//                 text style.
nsresult 
nsHTMLEditRules::IsPreformatted(nsCOMPtr<nsIDOMNode> aNode, PRBool *aResult)
{
  return PR_TRUE;
}


nsresult 
nsHTMLEditRules::InsertTab(nsIDOMSelection *aSelection, 
                           PRBool *aCancel, 
                           PlaceholderTxn **aTxn,
                           nsString *outString)
{
  nsCOMPtr<nsIDOMNode> theNode;
  PRBool isPRE;
  theNode = GetStartNode(aSelection);
  if (!theNode)
    return NS_ERROR_UNEXPECTED;
  nsresult result = IsPreformatted(theNode,&isPRE);
  if (NS_FAILED(result))
    return result;
  if (isPRE)
  {
    outString += '\t';
    // we're done - let everything fall through to the InsertText code 
    // in nsTextEditor which will insert the tab as is.
  }
  else
  {
    
  }
  return NS_OK;
}


nsresult 
nsHTMLEditRules::InsertSpace(nsIDOMSelection *aSelection, 
                             PRBool *aCancel, 
                             PlaceholderTxn **aTxn,
                             nsString *outString)
{
  return NS_OK;
}




