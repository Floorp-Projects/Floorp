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
#include "nsCOMPtr.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMSelection.h"
#include "nsIDOMRange.h"
#include "nsIDOMCharacterData.h"
#include "nsIEnumerator.h"
#include "nsIContent.h"

const static char* kMOZEditorBogusNodeAttr="MOZ_EDITOR_BOGUS_NODE";
const static char* kMOZEditorBogusNodeValue="TRUE";

static NS_DEFINE_IID(kPlaceholderTxnIID,  PLACEHOLDER_TXN_IID);

/********************************************************
 *  Constructor/Destructor 
 ********************************************************/

nsHTMLEditRules::nsHTMLEditRules()
{
}

nsHTMLEditRules::~nsHTMLEditRules()
{
}


/********************************************************
 *  Public methods 
 ********************************************************/

NS_IMETHODIMP 
nsHTMLEditRules::WillDoAction(int aAction, nsIDOMSelection *aSelection, 
                              void **aOtherInfo, PRBool *aCancel)
{
  if (!aSelection) 
    return NS_ERROR_NULL_POINTER;
    
  switch (aAction)
  {
    case kInsertBreak:
      return WillInsertBreak(aSelection, aCancel);
  }
  return nsTextEditRules::WillDoAction(aAction, aSelection, aOtherInfo, aCancel);
}
  
NS_IMETHODIMP 
nsHTMLEditRules::DidDoAction(int aAction, nsIDOMSelection *aSelection,
                             void **aOtherInfo, nsresult aResult)
{
  if (!aSelection) 
    return NS_ERROR_NULL_POINTER;
    
  switch (aAction)
  {
    case kInsertBreak:
      return DidInsertBreak(aSelection, aResult);
  }
  return nsTextEditRules::DidDoAction(aAction, aSelection, aOtherInfo, aResult);
}
  

/********************************************************
 *  Protected methods 
 ********************************************************/
NS_IMETHODIMP
nsHTMLEditRules::WillInsertBreak(nsIDOMSelection *aSelection, PRBool *aCancel)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  return WillInsert(aSelection, aCancel);
}

// XXX: this code is all experimental, and has no effect on the content model yet
//      the point here is to collapse adjacent BR's into P's
NS_IMETHODIMP
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
