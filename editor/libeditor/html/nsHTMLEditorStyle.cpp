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
 */
#include "nsICaret.h"

#include "nsHTMLEditor.h"
#include "nsHTMLEditRules.h"
#include "nsTextEditUtils.h"
#include "nsHTMLEditUtils.h"

#include "nsEditorEventListeners.h"

#include "nsIDOMText.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocument.h"
#include "nsIDOMAttr.h"
#include "nsIDocument.h"
#include "nsIDOMEventReceiver.h" 
#include "nsIDOMKeyEvent.h"
#include "nsIDOMKeyListener.h" 
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseEvent.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsISelectionController.h"

#include "nsICSSLoader.h"
#include "nsICSSStyleSheet.h"
#include "nsIHTMLContentContainer.h"
#include "nsIStyleSet.h"
#include "nsIDocumentObserver.h"
#include "nsIDocumentStateListener.h"

#include "nsIStyleContext.h"
#include "TypeInState.h"

#include "nsIEnumerator.h"
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsEditorCID.h"
#include "nsLayoutCID.h"
#include "nsIDOMRange.h"
#include "nsIDOMNSRange.h"
#include "nsISupportsArray.h"
#include "nsVoidArray.h"
#include "nsFileSpec.h"
#include "nsIFile.h"
#include "nsIURL.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsWidgetsCID.h"
#include "nsIDocumentEncoder.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsIImage.h"
#include "nsAOLCiter.h"
#include "nsInternetCiter.h"
#include "nsISupportsPrimitives.h"
#include "InsertTextTxn.h"

// Transactionas
#include "PlaceholderTxn.h"
#include "nsStyleSheetTxns.h"

// Misc
#include "nsEditorUtils.h"

static NS_DEFINE_CID(kHTMLEditorCID,  NS_HTMLEDITOR_CID);
static NS_DEFINE_CID(kCContentIteratorCID, NS_CONTENTITERATOR_CID);
static NS_DEFINE_IID(kSubtreeIteratorCID, NS_SUBTREEITERATOR_CID);
static NS_DEFINE_CID(kCRangeCID,      NS_RANGE_CID);
static NS_DEFINE_CID(kCDOMSelectionCID,      NS_DOMSELECTION_CID);

#if defined(NS_DEBUG) && defined(DEBUG_buster)
static PRBool gNoisy = PR_FALSE;
#else
static const PRBool gNoisy = PR_FALSE;
#endif


NS_IMETHODIMP nsHTMLEditor::SetInlineProperty(nsIAtom *aProperty, 
                            const nsAReadableString & aAttribute, 
                            const nsAReadableString & aValue)
{
  if (!aProperty) { return NS_ERROR_NULL_POINTER; }
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }
  ForceCompositionEnd();

  nsresult res;
  nsCOMPtr<nsISelection>selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));

  PRBool isCollapsed;
  selection->GetIsCollapsed(&isCollapsed);
  if (isCollapsed)
  {
    // manipulating text attributes on a collapsed selection only sets state for the next text insertion
    nsString tAttr(aAttribute);//MJUDGE SCC NEED HELP
    nsString tVal(aValue);//MJUDGE SCC NEED HELP
    return mTypeInState->SetProp(aProperty, tAttr, tVal);
  }
  
  nsAutoEditBatch batchIt(this);
  nsAutoRules beginRulesSniffing(this, kOpInsertElement, nsIEditor::eNext);
  nsAutoSelectionReset selectionResetter(selection, this);
  nsAutoTxnsConserveSelection dontSpazMySelection(this);
  
  PRBool cancel, handled;
  nsTextRulesInfo ruleInfo(nsTextEditRules::kSetTextProperty);
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (NS_FAILED(res)) return res;
  if (!cancel && !handled)
  {
    // get selection range enumerator
    nsCOMPtr<nsIEnumerator> enumerator;
    res = selPriv->GetEnumerator(getter_AddRefs(enumerator));
    if (NS_FAILED(res)) return res;
    if (!enumerator)    return NS_ERROR_FAILURE;

    // loop thru the ranges in the selection
    enumerator->First(); 
    nsCOMPtr<nsISupports> currentItem;
    while ((NS_ENUMERATOR_FALSE == enumerator->IsDone()))
    {
      res = enumerator->CurrentItem(getter_AddRefs(currentItem));
      if (NS_FAILED(res)) return res;
      if (!currentItem)   return NS_ERROR_FAILURE;
      
      nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );

      // adjust range to include any ancestors who's children are entirely selected
      res = PromoteInlineRange(range);
      if (NS_FAILED(res)) return res;
      
      // check for easy case: both range endpoints in same text node
      nsCOMPtr<nsIDOMNode> startNode, endNode;
      res = range->GetStartContainer(getter_AddRefs(startNode));
      if (NS_FAILED(res)) return res;
      res = range->GetEndContainer(getter_AddRefs(endNode));
      if (NS_FAILED(res)) return res;
      if ((startNode == endNode) && IsTextNode(startNode))
      {
        PRInt32 startOffset, endOffset;
        range->GetStartOffset(&startOffset);
        range->GetEndOffset(&endOffset);
        nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(startNode);
        res = SetInlinePropertyOnTextNode(nodeAsText, startOffset, endOffset, aProperty, &aAttribute, &aValue);
        if (NS_FAILED(res)) return res;
      }
      else
      {
        // not the easy case.  range not contained in single text node. 
        // there are up to three phases here.  There are all the nodes
        // reported by the subtree iterator to be processed.  And there
        // are potentially a starting textnode and an ending textnode
        // which are only partially contained by the range.
        
        // lets handle the nodes reported by the iterator.  These nodes
        // are entirely contained in the selection range.  We build up
        // a list of them (since doing operations on the document during
        // iteration would perturb the iterator).

        nsCOMPtr<nsIContentIterator> iter;
        res = nsComponentManager::CreateInstance(kSubtreeIteratorCID, nsnull,
                                                  NS_GET_IID(nsIContentIterator), 
                                                  getter_AddRefs(iter));
        if (NS_FAILED(res)) return res;
        if (!iter)          return NS_ERROR_FAILURE;

        nsCOMPtr<nsISupportsArray> arrayOfNodes;
        nsCOMPtr<nsIContent> content;
        nsCOMPtr<nsIDOMNode> node;
        nsCOMPtr<nsISupports> isupports;
        
        // make a array
        res = NS_NewISupportsArray(getter_AddRefs(arrayOfNodes));
        if (NS_FAILED(res)) return res;
        
        // iterate range and build up array
        res = iter->Init(range);
        // init returns an error if no nodes in range.
        // this can easily happen with the subtree 
        // iterator if the selection doesn't contain
        // any *whole* nodes.
        if (NS_SUCCEEDED(res))
        {
          while (NS_ENUMERATOR_FALSE == iter->IsDone())
          {
            res = iter->CurrentNode(getter_AddRefs(content));
            if (NS_FAILED(res)) return res;
            node = do_QueryInterface(content);
            if (!node) return NS_ERROR_FAILURE;
            if (IsEditable(node))
            { 
              isupports = do_QueryInterface(node);
              arrayOfNodes->AppendElement(isupports);
            }
            res = iter->Next();
            if (NS_FAILED(res)) return res;
          }
        }
        // first check the start parent of the range to see if it needs to 
        // be seperately handled (it does if it's a text node, due to how the
        // subtree iterator works - it will not have reported it).
        if (IsTextNode(startNode) && IsEditable(startNode))
        {
          nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(startNode);
          PRInt32 startOffset;
          PRUint32 textLen;
          range->GetStartOffset(&startOffset);
          nodeAsText->GetLength(&textLen);
          res = SetInlinePropertyOnTextNode(nodeAsText, startOffset, textLen, aProperty, &aAttribute, &aValue);
          if (NS_FAILED(res)) return res;
        }
        
        // then loop through the list, set the property on each node
        PRUint32 listCount;
        PRUint32 j;
        arrayOfNodes->Count(&listCount);
        for (j = 0; j < listCount; j++)
        {
          isupports = dont_AddRef(arrayOfNodes->ElementAt(0));
          node = do_QueryInterface(isupports);
          res = SetInlinePropertyOnNode(node, aProperty, &aAttribute, &aValue);
          if (NS_FAILED(res)) return res;
          arrayOfNodes->RemoveElementAt(0);
        }
        
        // last check the end parent of the range to see if it needs to 
        // be seperately handled (it does if it's a text node, due to how the
        // subtree iterator works - it will not have reported it).
        if (IsTextNode(endNode) && IsEditable(endNode))
        {
          nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(endNode);
          PRInt32 endOffset;
          range->GetEndOffset(&endOffset);
          res = SetInlinePropertyOnTextNode(nodeAsText, 0, endOffset, aProperty, &aAttribute, &aValue);
          if (NS_FAILED(res)) return res;
        }
      }
      enumerator->Next();
    }
  }
  if (!cancel)
  {
    // post-process
    res = mRules->DidDoAction(selection, &ruleInfo, res);
  }
  return res;
}



nsresult
nsHTMLEditor::SetInlinePropertyOnTextNode( nsIDOMCharacterData *aTextNode, 
                                            PRInt32 aStartOffset,
                                            PRInt32 aEndOffset,
                                            nsIAtom *aProperty, 
                                            const nsAReadableString *aAttribute,
                                            const nsAReadableString *aValue)
{
  if (!aTextNode) return NS_ERROR_NULL_POINTER;
  
  // dont need to do anything if no characters actually selected
  if (aStartOffset == aEndOffset) return NS_OK;
  
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> tmp, node = do_QueryInterface(aTextNode);
  
  // dont need to do anything if property already set on node
  PRBool bHasProp;
  nsCOMPtr<nsIDOMNode> styleNode;
  IsTextPropertySetByContent(node, aProperty, aAttribute, aValue, bHasProp, getter_AddRefs(styleNode));
  if (bHasProp) return NS_OK;
  
  // do we need to split the text node?
  PRUint32 textLen;
  aTextNode->GetLength(&textLen);
  
  if ( (PRUint32)aEndOffset != textLen )
  {
    // we need to split off back of text node
    res = SplitNode(node, aEndOffset, getter_AddRefs(tmp));
    if (NS_FAILED(res)) return res;
    node = tmp;  // remember left node
  }
  if ( aStartOffset )
  {
    // we need to split off front of text node
    res = SplitNode(node, aStartOffset, getter_AddRefs(tmp));
    if (NS_FAILED(res)) return res;
  }
  
  // reparent the node inside inline node with appropriate {attribute,value}
  res = SetInlinePropertyOnNode(node, aProperty, aAttribute, aValue);
  return res;
}


nsresult
nsHTMLEditor::SetInlinePropertyOnNode( nsIDOMNode *aNode,
                                       nsIAtom *aProperty, 
                                       const nsAReadableString *aAttribute,
                                       const nsAReadableString *aValue)
{
  if (!aNode || !aProperty) return NS_ERROR_NULL_POINTER;

  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> tmp;
  nsAutoString tag;
  aProperty->ToString(tag);
  tag.ToLowerCase();
  
  // dont need to do anything if property already set on node
  PRBool bHasProp;
  nsCOMPtr<nsIDOMNode> styleNode;
  IsTextPropertySetByContent(aNode, aProperty, aAttribute, aValue, bHasProp, getter_AddRefs(styleNode));
  if (bHasProp) return NS_OK;

  // is it already the right kind of node, but with wrong attribute?
  if (NodeIsType(aNode, aProperty))
  {
    // just set the attribute on it.
    // but first remove any contrary style in it's children.
    res = RemoveStyleInside(aNode, aProperty, aAttribute, PR_TRUE);
    if (NS_FAILED(res)) return res;
    nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(aNode);
    return SetAttribute(elem, *aAttribute, *aValue);
  }
  
  // can it be put inside inline node?
  if (TagCanContain(tag, aNode))
  {
    nsCOMPtr<nsIDOMNode> priorNode, nextNode;
    // is either of it's neighbors the right kind of node?
    GetPriorHTMLSibling(aNode, address_of(priorNode));
    GetNextHTMLSibling(aNode, address_of(nextNode));
    if (priorNode && NodeIsType(priorNode, aProperty) && 
        HasAttrVal(priorNode, aAttribute, aValue)     &&
        IsOnlyAttribute(priorNode, aAttribute) )
    {
      // previous sib is already right kind of inline node; slide this over into it
      res = MoveNode(aNode, priorNode, -1);
    }
    else if (nextNode && NodeIsType(nextNode, aProperty) && 
             HasAttrVal(nextNode, aAttribute, aValue)    &&
             IsOnlyAttribute(priorNode, aAttribute) )
    {
      // following sib is already right kind of inline node; slide this over into it
      res = MoveNode(aNode, nextNode, 0);
    }
    else
    {
      // ok, chuck it in it's very own container
      res = InsertContainerAbove(aNode, address_of(tmp), tag, aAttribute, aValue);
    }
    if (NS_FAILED(res)) return res;
    return RemoveStyleInside(aNode, aProperty, aAttribute);
  }
  // none of the above?  then cycle through the children.
  nsCOMPtr<nsIDOMNodeList> childNodes;
  res = aNode->GetChildNodes(getter_AddRefs(childNodes));
  if (NS_FAILED(res)) return res;
  if (childNodes)
  {
    PRInt32 j;
    PRUint32 childCount;
    childNodes->GetLength(&childCount);
    if (childCount)
    {
      nsCOMPtr<nsISupportsArray> arrayOfNodes;
      nsCOMPtr<nsIDOMNode> node;
      nsCOMPtr<nsISupports> isupports;
      
      // make a array
      res = NS_NewISupportsArray(getter_AddRefs(arrayOfNodes));
      if (NS_FAILED(res)) return res;
      
      // populate the list
      for (j=0 ; j < (PRInt32)childCount; j++)
      {
        nsCOMPtr<nsIDOMNode> childNode;
        res = childNodes->Item(j, getter_AddRefs(childNode));
        if ((NS_SUCCEEDED(res)) && (childNode) && IsEditable(childNode))
        {
          isupports = do_QueryInterface(childNode);
          arrayOfNodes->AppendElement(isupports);
        }
      }
      
      // then loop through the list, set the property on each node
      PRUint32 listCount;
      arrayOfNodes->Count(&listCount);
      for (j = 0; j < (PRInt32)listCount; j++)
      {
        isupports = dont_AddRef(arrayOfNodes->ElementAt(0));
        node = do_QueryInterface(isupports);
        res = SetInlinePropertyOnNode(node, aProperty, aAttribute, aValue);
        if (NS_FAILED(res)) return res;
        arrayOfNodes->RemoveElementAt(0);
      }
    }
  }
  return res;
}


nsresult nsHTMLEditor::SplitStyleAboveRange(nsIDOMRange *inRange, 
                                            nsIAtom *aProperty, 
                                            const nsAReadableString *aAttribute)
{
  if (!inRange) return NS_ERROR_NULL_POINTER;
  nsresult res;
  nsCOMPtr<nsIDOMNode> startNode, endNode, origStartNode;
  PRInt32 startOffset, endOffset, origStartOffset;
  
  res = inRange->GetStartContainer(getter_AddRefs(startNode));
  if (NS_FAILED(res)) return res;
  res = inRange->GetStartOffset(&startOffset);
  if (NS_FAILED(res)) return res;
  res = inRange->GetEndContainer(getter_AddRefs(endNode));
  if (NS_FAILED(res)) return res;
  res = inRange->GetEndOffset(&endOffset);
  if (NS_FAILED(res)) return res;
  
  origStartNode = startNode;
  origStartOffset = startOffset;
  PRBool sameNode = (startNode==endNode);
  
  // split any matching style nodes above the start of range
  res = SplitStyleAbovePoint(address_of(startNode), &startOffset, aProperty, aAttribute);
  if (NS_FAILED(res)) return res;
  
  if (sameNode && (startNode != origStartNode))
  {
    // our startNode got split.  This changes the offset of the end of our range.
    endOffset -= origStartOffset;
  }
  
  // second verse, same as the first...
  res = SplitStyleAbovePoint(address_of(endNode), &endOffset, aProperty, aAttribute);
  if (NS_FAILED(res)) return res;
  
  // reset the range
  res = inRange->SetStart(startNode, startOffset);
  if (NS_FAILED(res)) return res;
  res = inRange->SetEnd(endNode, endOffset);
  return res;
}

nsresult nsHTMLEditor::SplitStyleAbovePoint(nsCOMPtr<nsIDOMNode> *aNode,
                                           PRInt32 *aOffset,
                                           nsIAtom *aProperty,          // null here means we split all properties
                                           const nsAReadableString *aAttribute,
                                           nsCOMPtr<nsIDOMNode> *outLeftNode,
                                           nsCOMPtr<nsIDOMNode> *outRightNode)
{
  if (!aNode || !*aNode || !aOffset) return NS_ERROR_NULL_POINTER;
  if (outLeftNode)  *outLeftNode  = nsnull;
  if (outRightNode) *outRightNode = nsnull;
  // split any matching style nodes above the node/offset
  nsCOMPtr<nsIDOMNode> parent, tmp = *aNode;
  PRInt32 offset;
  
  while (tmp && !IsBlockNode(tmp))
  {
    if ( (aProperty && NodeIsType(tmp, aProperty)) ||   // node is the correct inline prop
         (aProperty == nsIEditProperty::href && nsHTMLEditUtils::IsLink(tmp)) || // node is href - test if really <a href=...
         (!aProperty && NodeIsProperty(tmp)) )         // or node is any prop, and we asked to split them all
    {
      // found a style node we need to split
      SplitNodeDeep(tmp, *aNode, *aOffset, &offset, PR_FALSE, outLeftNode, outRightNode);
      // reset startNode/startOffset
      tmp->GetParentNode(getter_AddRefs(*aNode));
      *aOffset = offset;
    }
    tmp->GetParentNode(getter_AddRefs(parent));
    tmp = parent;
  }
  return NS_OK;
}

PRBool nsHTMLEditor::NodeIsProperty(nsIDOMNode *aNode)
{
  if (!aNode)               return PR_FALSE;
  if (!IsContainer(aNode))  return PR_FALSE;
  if (!IsEditable(aNode))   return PR_FALSE;
  if (IsBlockNode(aNode))   return PR_FALSE;
  if (NodeIsType(aNode, nsIEditProperty::a)) return PR_FALSE;
  return PR_TRUE;
}

nsresult nsHTMLEditor::RemoveStyleInside(nsIDOMNode *aNode, 
                                   nsIAtom *aProperty,   // null here means remove all properties
                                   const nsAReadableString *aAttribute, 
                                   PRBool aChildrenOnly)
{
  if (!aNode) return NS_ERROR_NULL_POINTER;
  if (IsTextNode(aNode)) return NS_OK;
  nsresult res = NS_OK;

  // first process the children
  nsCOMPtr<nsIDOMNode> child, tmp;
  aNode->GetFirstChild(getter_AddRefs(child));
  while (child)
  {
    // cache next sibling since we might remove child
    child->GetNextSibling(getter_AddRefs(tmp));
    res = RemoveStyleInside(child, aProperty, aAttribute);
    if (NS_FAILED(res)) return res;
    child = tmp;
  }

  // then process the node itself
  if ( !aChildrenOnly && 
       ((aProperty && NodeIsType(aNode, aProperty)) || // node is prop we asked for
        (aProperty == nsIEditProperty::href && nsHTMLEditUtils::IsLink(aNode))) || // but check for link (<a href=...)
       (!aProperty && NodeIsProperty(aNode)) )        // or node is any prop and we asked for that
  {
    // if we weren't passed an attribute, then we want to 
    // remove any matching inlinestyles entirely
    if (!aAttribute || aAttribute->IsEmpty())
    {
      res = RemoveContainer(aNode);
    }
    // otherwise we just want to eliminate the attribute
    else
    {
      if (HasAttr(aNode, aAttribute))
      {
        // if this matching attribute is the ONLY one on the node,
        // then remove the whole node.  Otherwise just nix the attribute.
        if (IsOnlyAttribute(aNode, aAttribute))
        {
          res = RemoveContainer(aNode);
        }
        else
        {
          nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(aNode);
          if (!elem) return NS_ERROR_NULL_POINTER;
          res = RemoveAttribute(elem, *aAttribute);
        }
      }
    }
  }
  return res;
}

PRBool nsHTMLEditor::IsOnlyAttribute(nsIDOMNode *aNode, 
                                     const nsAReadableString *aAttribute)
{
  if (!aNode || !aAttribute) return PR_FALSE;  // ooops
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  if (!content) return PR_FALSE;  // ooops
  
  PRInt32 attrCount, i, nameSpaceID;
  nsCOMPtr<nsIAtom> attrName, prefix;
  content->GetAttrCount(attrCount);
  
  for (i=0; i<attrCount; i++)
  {
    content->GetAttrNameAt(i, nameSpaceID, *getter_AddRefs(attrName),
                           *getter_AddRefs(prefix));
    nsAutoString attrString, tmp;
    if (!attrName) continue;  // ooops
    attrName->ToString(attrString);
    // if it's the attribute we know about, keep looking
    if (!Compare(attrString,*aAttribute,nsCaseInsensitiveStringComparator())) continue;
    // if it's a special _moz... attribute, keep looking
    attrString.Left(tmp,4);
    if (!Compare(tmp,NS_LITERAL_STRING("_moz"),nsCaseInsensitiveStringComparator())) continue;
    // otherwise, it's another attribute, so return false
    return PR_FALSE;
  }
  // if we made it through all of them without finding a real attribute
  // other than aAttribute, then return PR_TRUE
  return PR_TRUE;
}

PRBool 
nsHTMLEditor::HasMatchingAttributes(nsIDOMNode *aNode1, 
                                    nsIDOMNode *aNode2)
{
  if (!aNode1  || !aNode2) return PR_FALSE;  // ooops
  nsCOMPtr<nsIContent> content1 = do_QueryInterface(aNode1);
  if (!content1) return PR_FALSE;  // ooops
  nsCOMPtr<nsIContent> content2 = do_QueryInterface(aNode2);
  if (!content2) return PR_FALSE;  // ooops
  
  PRInt32 attrCount, i, nameSpaceID, realCount1=0, realCount2=0;
  nsCOMPtr<nsIAtom> attrName, prefix;
  nsresult res, res2;
  content1->GetAttrCount(attrCount);
  nsAutoString attrString, tmp, attrVal1, attrVal2;
  
  for (i=0; i<attrCount; i++)
  {
    content1->GetAttrNameAt(i, nameSpaceID, *getter_AddRefs(attrName),
                            *getter_AddRefs(prefix));
    if (!attrName) continue;  // ooops
    attrName->ToString(attrString);
    // if it's a special _moz... attribute, keep going
    attrString.Left(tmp,4);
    if (tmp.EqualsWithConversion("_moz")) continue;
    // otherwise, it's another attribute, so count it
    realCount1++;
    // and compare it to element2's attributes
    res = content1->GetAttr(nameSpaceID, attrName, attrVal1);
    res2 = content2->GetAttr(nameSpaceID, attrName, attrVal2);
    if (res != res2) return PR_FALSE;
    if (!attrVal1.EqualsIgnoreCase(attrVal2)) return PR_FALSE;
  }

  content2->GetAttrCount(attrCount);
  for (i=0; i<attrCount; i++)
  {
    content2->GetAttrNameAt(i, nameSpaceID, *getter_AddRefs(attrName),
                            *getter_AddRefs(prefix));
    if (!attrName) continue;  // ooops
    attrName->ToString(attrString);
    // if it's a special _moz... attribute, keep going
    attrString.Left(tmp,4);
    if (tmp.EqualsWithConversion("_moz")) continue;
    // otherwise, it's another attribute, so count it
    realCount2++;
  }

  if (realCount1 != realCount2) return PR_FALSE; 
  // otherwise, attribute counts match, and we already compared them 
  // when going through the first list, so we're done.
  return PR_TRUE;
}

PRBool nsHTMLEditor::HasAttr(nsIDOMNode *aNode, 
                             const nsAReadableString *aAttribute)
{
  if (!aNode) return PR_FALSE;
  if (!aAttribute || aAttribute->IsEmpty()) return PR_TRUE;  // everybody has the 'null' attribute
  
  // get element
  nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(aNode);
  if (!elem) return PR_FALSE;
  
  // get attribute node
  nsCOMPtr<nsIDOMAttr> attNode;
  nsresult res = elem->GetAttributeNode(*aAttribute, getter_AddRefs(attNode));
  if ((NS_FAILED(res)) || !attNode) return PR_FALSE;
  return PR_TRUE;
}


PRBool nsHTMLEditor::HasAttrVal(nsIDOMNode *aNode, 
                                const nsAReadableString *aAttribute, 
                                const nsAReadableString *aValue)
{
  if (!aNode) return PR_FALSE;
  if (!aAttribute || aAttribute->IsEmpty()) return PR_TRUE;  // everybody has the 'null' attribute
  
  // get element
  nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(aNode);
  if (!elem) return PR_FALSE;
  
  // get attribute node
  nsCOMPtr<nsIDOMAttr> attNode;
  nsresult res = elem->GetAttributeNode(*aAttribute, getter_AddRefs(attNode));
  if ((NS_FAILED(res)) || !attNode) return PR_FALSE;
  
  // check if attribute has a value
  PRBool isSet;
  attNode->GetSpecified(&isSet);
  // if no value, and that's what we wanted, then return true
  if (!isSet && (!aValue || aValue->IsEmpty())) return PR_TRUE; 
  
  // get attribute value
  nsAutoString attrVal;
  attNode->GetValue(attrVal);
  
  // do values match?
  if (!Compare(attrVal,*aValue,nsCaseInsensitiveStringComparator())) return PR_TRUE;
  return PR_FALSE;
}


nsresult nsHTMLEditor::PromoteInlineRange(nsIDOMRange *inRange)
{
  if (!inRange) return NS_ERROR_NULL_POINTER;
  nsresult res;
  nsCOMPtr<nsIDOMNode> startNode, endNode, parent;
  PRInt32 startOffset, endOffset;
  
  res = inRange->GetStartContainer(getter_AddRefs(startNode));
  if (NS_FAILED(res)) return res;
  res = inRange->GetStartOffset(&startOffset);
  if (NS_FAILED(res)) return res;
  res = inRange->GetEndContainer(getter_AddRefs(endNode));
  if (NS_FAILED(res)) return res;
  res = inRange->GetEndOffset(&endOffset);
  if (NS_FAILED(res)) return res;
  
  while ( startNode && 
          !nsTextEditUtils::IsBody(startNode) && 
          IsAtFrontOfNode(startNode, startOffset) )
  {
    res = GetNodeLocation(startNode, address_of(parent), &startOffset);
    if (NS_FAILED(res)) return res;
    startNode = parent;
  }
  if (!startNode) return NS_ERROR_NULL_POINTER;
  
  while ( endNode && 
          !nsTextEditUtils::IsBody(endNode) && 
          IsAtEndOfNode(endNode, endOffset) )
  {
    res = GetNodeLocation(endNode, address_of(parent), &endOffset);
    if (NS_FAILED(res)) return res;
    endNode = parent;
    endOffset++;  // we are AFTER this node
  }
  if (!endNode) return NS_ERROR_NULL_POINTER;
  
  res = inRange->SetStart(startNode, startOffset);
  if (NS_FAILED(res)) return res;
  res = inRange->SetEnd(endNode, endOffset);
  return res;
}

PRBool nsHTMLEditor::IsAtFrontOfNode(nsIDOMNode *aNode, PRInt32 aOffset)
{
  if (!aNode) return PR_FALSE;  // oops
  if (!aOffset) return PR_TRUE;
  
  if (IsTextNode(aNode))
  {
    return PR_FALSE;
  }
  else
  {
    nsCOMPtr<nsIDOMNode> firstNode;
    GetFirstEditableChild(aNode, address_of(firstNode));
    if (!firstNode) return PR_TRUE; 
    PRInt32 offset;
    nsEditor::GetChildOffset(firstNode, aNode, offset);
    if (offset < aOffset) return PR_FALSE;
    return PR_TRUE;
  }
}

PRBool nsHTMLEditor::IsAtEndOfNode(nsIDOMNode *aNode, PRInt32 aOffset)
{
  if (!aNode) return PR_FALSE;  // oops
  PRUint32 len;
  GetLengthOfDOMNode(aNode, len);
  if (aOffset == (PRInt32)len) return PR_TRUE;
  
  if (IsTextNode(aNode))
  {
    return PR_FALSE;
  }
  else
  {
    nsCOMPtr<nsIDOMNode> lastNode;
    GetLastEditableChild(aNode, address_of(lastNode));
    if (!lastNode) return PR_TRUE; 
    PRInt32 offset;
    nsEditor::GetChildOffset(lastNode, aNode, offset);
    if (offset < aOffset) return PR_TRUE;
    return PR_FALSE;
  }
}


nsresult
nsHTMLEditor::GetInlinePropertyBase(nsIAtom *aProperty, 
                             const nsAReadableString *aAttribute,
                             const nsAReadableString *aValue,
                             PRBool *aFirst, 
                             PRBool *aAny, 
                             PRBool *aAll,
                             nsAWritableString *outValue)
{
  if (!aProperty)
    return NS_ERROR_NULL_POINTER;
/*
  if (gNoisy) 
  { 
    nsAutoString propString;
    aProperty->ToString(propString);
    char *propCString = propString.ToNewCString();
    if (gNoisy) { printf("nsTextEditor::GetTextProperty %s\n", propCString); }
    nsCRT::free(propCString);
  }
*/
  nsresult result;
  *aAny=PR_FALSE;
  *aAll=PR_TRUE;
  *aFirst=PR_FALSE;
  PRBool first=PR_TRUE;
  nsCOMPtr<nsISelection>selection;
  result = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(result)) return result;
  if (!selection) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));

  PRBool isCollapsed;
  selection->GetIsCollapsed(&isCollapsed);
  nsCOMPtr<nsIDOMNode> collapsedNode;
  nsCOMPtr<nsIEnumerator> enumerator;
  result = selPriv->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(result)) return result;
  if (!enumerator) return NS_ERROR_NULL_POINTER;

  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  result = enumerator->CurrentItem(getter_AddRefs(currentItem));
  // XXX: should be a while loop, to get each separate range
  // XXX: ERROR_HANDLING can currentItem be null?
  if ((NS_SUCCEEDED(result)) && currentItem)
  {
    PRBool firstNodeInRange = PR_TRUE; // for each range, set a flag 
    nsCOMPtr<nsIDOMRange> range(do_QueryInterface(currentItem));

    if (isCollapsed)
    {
      // efficiency hack.  we cache prior results for being collapsed in a given text node.
      // this speeds up typing.  Note that other parts of the editor code have to clear out 
      // this cache after certain actions.
      range->GetStartContainer(getter_AddRefs(collapsedNode));
      if (!collapsedNode) return NS_ERROR_FAILURE;
/*    // refresh the cache if we need to
      if (collapsedNode != mCachedNode) CacheInlineStyles(collapsedNode);
      // cache now current, use it!  But override it with typeInState results if any...
*/    PRBool isSet, theSetting;
      if (aAttribute)
      {
        nsString tString(*aAttribute); //MJUDGE SCC NEED HELP
        nsString tOutString;//MJUDGE SCC NEED HELP
        nsString *tPassString=nsnull;
        if (outValue)
            tPassString = &tOutString;
        mTypeInState->GetTypingState(isSet, theSetting, aProperty, tString, &tOutString);
        if (outValue)
          outValue->Assign(tOutString);
      }
      else
        mTypeInState->GetTypingState(isSet, theSetting, aProperty);
      if (isSet) 
      {
        *aFirst = *aAny = *aAll = theSetting;
        return NS_OK;
      }
      nsCOMPtr<nsIDOMNode> resultNode;
      IsTextPropertySetByContent(collapsedNode, aProperty, 0, 0, isSet, getter_AddRefs(resultNode));
      *aFirst = *aAny = *aAll = isSet;
      return NS_OK;
      /*
      if (aProperty == mBoldAtom.get())
      {
        mTypeInState->GetTypingState(isSet, theSetting, aProperty);
        if (isSet) 
        {
          *aFirst = *aAny = *aAll = theSetting;
        }
        else
        {
          *aFirst = *aAny = *aAll = mCachedBoldStyle;
        }
        return NS_OK;
      }
      else if (aProperty == mItalicAtom.get())
      {
        mTypeInState->GetTypingState(isSet, theSetting, aProperty);
        if (isSet) 
        {
          *aFirst = *aAny = *aAll = theSetting;
        }
        else
        {
          *aFirst = *aAny = *aAll = mCachedItalicStyle;
        }
        return NS_OK;
      }
      else if (aProperty == mUnderlineAtom.get())
      {
        mTypeInState->GetTypingState(isSet, theSetting, aProperty);
        if (isSet) 
        {
          *aFirst = *aAny = *aAll = theSetting;
        }
        else
        {
          *aFirst = *aAny = *aAll = mCachedUnderlineStyle;
        }
        return NS_OK;
      } */
    }

    // either non-collapsed selection or no cached value: do it the hard way
    nsCOMPtr<nsIContentIterator> iter;
    iter = do_CreateInstance(kCContentIteratorCID);
    if (!iter) return NS_ERROR_NULL_POINTER;

    iter->Init(range);
    nsCOMPtr<nsIContent> content;
    nsAutoString firstValue, theValue;
    iter->CurrentNode(getter_AddRefs(content));
    while (NS_ENUMERATOR_FALSE == iter->IsDone())
    {
      nsCOMPtr<nsIDOMNode> node = do_QueryInterface(content);

      if (node && nsTextEditUtils::IsBody(node))
        break;

      //if (gNoisy) { printf("  checking node %p\n", content.get()); }
      nsCOMPtr<nsIDOMCharacterData>text;
      text = do_QueryInterface(content);
      
      PRBool skipNode = PR_FALSE;
      
      // just ignore any non-editable nodes
      if (text && !IsEditable(text))
      {
        skipNode = PR_TRUE;
      }
      else if (text)
      {
        if (!isCollapsed && first && firstNodeInRange)
        {
          firstNodeInRange = PR_FALSE;
          PRInt32 startOffset;
          range->GetStartOffset(&startOffset);
          PRUint32 count;
          text->GetLength(&count);
          if (startOffset==(PRInt32)count) 
          {
            //if (gNoisy) { printf("  skipping node %p\n", content.get()); }
            skipNode = PR_TRUE;
          }
        }
      }
      else
      { // handle non-text leaf nodes here
        PRBool canContainChildren;
        content->CanContainChildren(canContainChildren);
        if (canContainChildren)
        {
          //if (gNoisy) { printf("  skipping non-leaf node %p\n", content.get()); }
          skipNode = PR_TRUE;
        }
        else {
          //if (gNoisy) { printf("  testing non-text leaf node %p\n", content.get()); }
        }
      }
      if (!skipNode)
      {
        if (node)
        {
          PRBool isSet;
          nsCOMPtr<nsIDOMNode>resultNode;
          if (first)
          {
            IsTextPropertySetByContent(node, aProperty, aAttribute, aValue, isSet, getter_AddRefs(resultNode), &firstValue);
            *aFirst = isSet;
            first = PR_FALSE;
            if (outValue) *outValue = firstValue;
          }
          else
          {
            IsTextPropertySetByContent(node, aProperty, aAttribute, aValue, isSet, getter_AddRefs(resultNode), &theValue);
            if (firstValue != theValue)
              *aAll = PR_FALSE;
          }
          
          if (isSet) {
            *aAny = PR_TRUE;
          }
          else {
            *aAll = PR_FALSE;
          }
        }
      }
      result = iter->Next();
      if (NS_FAILED(result))  
        break;
      iter->CurrentNode(getter_AddRefs(content));
    }
  }
  if (!*aAny) 
  { // make sure that if none of the selection is set, we don't report all is set
    *aAll = PR_FALSE;
  }
  //if (gNoisy) { printf("  returning first=%d any=%d all=%d\n", *aFirst, *aAny, *aAll); }
  return result;
}


NS_IMETHODIMP nsHTMLEditor::GetInlineProperty(nsIAtom *aProperty, 
                                              const nsAReadableString &aAttribute, 
                                              const nsAReadableString &aValue,
                                              PRBool *aFirst, 
                                              PRBool *aAny, 
                                              PRBool *aAll)
{
  if (!aProperty || !aFirst || !aAny || !aAll)
    return NS_ERROR_NULL_POINTER;
  const nsAReadableString *att = nsnull;
  if (aAttribute.Length())
    att = &aAttribute;
  const nsAReadableString *val = nsnull;
  if (aValue.Length())
    val = &aValue;
  return GetInlinePropertyBase( aProperty, att, val, aFirst, aAny, aAll, nsnull);
}


NS_IMETHODIMP nsHTMLEditor::GetInlinePropertyWithAttrValue(nsIAtom *aProperty, 
                                              const nsAReadableString &aAttribute, 
                                              const nsAReadableString &aValue,
                                              PRBool *aFirst, 
                                              PRBool *aAny, 
                                              PRBool *aAll,
                                              nsAWritableString &outValue)
{
  if (!aProperty || !aFirst || !aAny || !aAll)
    return NS_ERROR_NULL_POINTER;
  const nsAReadableString *att = nsnull;
  if (aAttribute.Length())
    att = &aAttribute;
  const nsAReadableString *val = nsnull;
  if (aValue.Length())
    val = &aValue;
  return GetInlinePropertyBase( aProperty, att, val, aFirst, aAny, aAll, &outValue);
}


NS_IMETHODIMP nsHTMLEditor::RemoveAllInlineProperties()
{
  return RemoveInlinePropertyImpl(nsnull, nsnull);
}

NS_IMETHODIMP nsHTMLEditor::RemoveInlineProperty(nsIAtom *aProperty, const nsAReadableString &aAttribute)
{
  return RemoveInlinePropertyImpl(aProperty, &aAttribute);
}

nsresult nsHTMLEditor::RemoveInlinePropertyImpl(nsIAtom *aProperty, const nsAReadableString *aAttribute)
{
  if (!mRules)    return NS_ERROR_NOT_INITIALIZED;
  ForceCompositionEnd();

  nsresult res;
  nsCOMPtr<nsISelection>selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));

  PRBool isCollapsed;
  selection->GetIsCollapsed(&isCollapsed);
  if (isCollapsed)
  {
    // manipulating text attributes on a collapsed selection only sets state for the next text insertion

    // For links, aProperty uses "href", use "a" instead
    if (aProperty == nsIEditProperty::href)
      aProperty = nsIEditProperty::a;

    if (aProperty) return mTypeInState->ClearProp(aProperty, nsAutoString(*aAttribute));
    else return mTypeInState->ClearAllProps();
  }
  nsAutoEditBatch batchIt(this);
  nsAutoRules beginRulesSniffing(this, kOpRemoveTextProperty, nsIEditor::eNext);
  nsAutoSelectionReset selectionResetter(selection, this);
  nsAutoTxnsConserveSelection dontSpazMySelection(this);
  
  PRBool cancel, handled;
  nsTextRulesInfo ruleInfo(nsTextEditRules::kRemoveTextProperty);
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (NS_FAILED(res)) return res;
  if (!cancel && !handled)
  {
    // get selection range enumerator
    nsCOMPtr<nsIEnumerator> enumerator;
    res = selPriv->GetEnumerator(getter_AddRefs(enumerator));
    if (NS_FAILED(res)) return res;
    if (!enumerator)    return NS_ERROR_FAILURE;

    // loop thru the ranges in the selection
    enumerator->First(); 
    nsCOMPtr<nsISupports> currentItem;
    while ((NS_ENUMERATOR_FALSE == enumerator->IsDone()))
    {
      res = enumerator->CurrentItem(getter_AddRefs(currentItem));
      if (NS_FAILED(res)) return res;
      if (!currentItem)   return NS_ERROR_FAILURE;
      
      nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );

      // adjust range to include any ancestors who's children are entirely selected
      res = PromoteInlineRange(range);
      if (NS_FAILED(res)) return res;
      
      // remove this style from ancestors of our range endpoints, 
      // splitting them as appropriate
      res = SplitStyleAboveRange(range, aProperty, aAttribute);
      if (NS_FAILED(res)) return res;
      
      // check for easy case: both range endpoints in same text node
      nsCOMPtr<nsIDOMNode> startNode, endNode;
      res = range->GetStartContainer(getter_AddRefs(startNode));
      if (NS_FAILED(res)) return res;
      res = range->GetEndContainer(getter_AddRefs(endNode));
      if (NS_FAILED(res)) return res;
      if ((startNode == endNode) && IsTextNode(startNode))
      {
        // we're done with this range!
      }
      else
      {
        // not the easy case.  range not contained in single text node. 
        nsCOMPtr<nsIContentIterator> iter;
        res = nsComponentManager::CreateInstance(kSubtreeIteratorCID, nsnull,
                                                  NS_GET_IID(nsIContentIterator), 
                                                  getter_AddRefs(iter));
        if (NS_FAILED(res)) return res;
        if (!iter)          return NS_ERROR_FAILURE;

        nsCOMPtr<nsISupportsArray> arrayOfNodes;
        nsCOMPtr<nsIContent> content;
        nsCOMPtr<nsIDOMNode> node;
        nsCOMPtr<nsISupports> isupports;
        
        // make a array
        res = NS_NewISupportsArray(getter_AddRefs(arrayOfNodes));
        if (NS_FAILED(res)) return res;
        
        // iterate range and build up array
        iter->Init(range);
        while (NS_ENUMERATOR_FALSE == iter->IsDone())
        {
          res = iter->CurrentNode(getter_AddRefs(content));
          if (NS_FAILED(res)) return res;
          node = do_QueryInterface(content);
          if (!node) return NS_ERROR_FAILURE;
          if (IsEditable(node))
          { 
            isupports = do_QueryInterface(node);
            arrayOfNodes->AppendElement(isupports);
          }
          res = iter->Next();
          if (NS_FAILED(res)) return res;
        }
        
        // loop through the list, remove the property on each node
        PRUint32 listCount;
        PRUint32 j;
        arrayOfNodes->Count(&listCount);
        for (j = 0; j < listCount; j++)
        {
          isupports = dont_AddRef(arrayOfNodes->ElementAt(0));
          node = do_QueryInterface(isupports);
          res = RemoveStyleInside(node, aProperty, aAttribute);
          if (NS_FAILED(res)) return res;
          arrayOfNodes->RemoveElementAt(0);
        }
      }
      enumerator->Next();
    }
  }
  if (!cancel)
  {
    // post-process 
    res = mRules->DidDoAction(selection, &ruleInfo, res);
  }
  return res;
}

NS_IMETHODIMP nsHTMLEditor::IncreaseFontSize()
{
  return RelativeFontChange(1);
}

NS_IMETHODIMP nsHTMLEditor::DecreaseFontSize()
{
  return RelativeFontChange(-1);
}

nsresult
nsHTMLEditor::RelativeFontChange( PRInt32 aSizeChange)
{
  // Can only change font size by + or - 1
  if ( !( (aSizeChange==1) || (aSizeChange==-1) ) )
    return NS_ERROR_ILLEGAL_VALUE;
  
  ForceCompositionEnd();

  // Get the selection 
  nsCOMPtr<nsISelection>selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_FAILURE;
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));  
  // Is the selection collapsed?
  PRBool bCollapsed;
  res = selection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  
  // if it's collapsed set typing state
  if (bCollapsed)
  {
    nsCOMPtr<nsIAtom> atom;
    if (aSizeChange==1) atom = nsIEditProperty::big;
    else                atom = nsIEditProperty::small;
    // manipulating text attributes on a collapsed selection only sets state for the next text insertion
    return mTypeInState->SetProp(atom, nsAutoString(), nsAutoString());
  }
  
  // wrap with txn batching, rules sniffing, and selection preservation code
  nsAutoEditBatch batchIt(this);
  nsAutoRules beginRulesSniffing(this, kOpSetTextProperty, nsIEditor::eNext);
  nsAutoSelectionReset selectionResetter(selection, this);
  nsAutoTxnsConserveSelection dontSpazMySelection(this);

  // get selection range enumerator
  nsCOMPtr<nsIEnumerator> enumerator;
  res = selPriv->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(res)) return res;
  if (!enumerator)    return NS_ERROR_FAILURE;

  // loop thru the ranges in the selection
  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  while ((NS_ENUMERATOR_FALSE == enumerator->IsDone()))
  {
    res = enumerator->CurrentItem(getter_AddRefs(currentItem));
    if (NS_FAILED(res)) return res;
    if (!currentItem)   return NS_ERROR_FAILURE;
    
    nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );

    // adjust range to include any ancestors who's children are entirely selected
    res = PromoteInlineRange(range);
    if (NS_FAILED(res)) return res;
    
    // check for easy case: both range endpoints in same text node
    nsCOMPtr<nsIDOMNode> startNode, endNode;
    res = range->GetStartContainer(getter_AddRefs(startNode));
    if (NS_FAILED(res)) return res;
    res = range->GetEndContainer(getter_AddRefs(endNode));
    if (NS_FAILED(res)) return res;
    if ((startNode == endNode) && IsTextNode(startNode))
    {
      PRInt32 startOffset, endOffset;
      range->GetStartOffset(&startOffset);
      range->GetEndOffset(&endOffset);
      nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(startNode);
      res = RelativeFontChangeOnTextNode(aSizeChange, nodeAsText, startOffset, endOffset);
      if (NS_FAILED(res)) return res;
    }
    else
    {
      // not the easy case.  range not contained in single text node. 
      // there are up to three phases here.  There are all the nodes
      // reported by the subtree iterator to be processed.  And there
      // are potentially a starting textnode and an ending textnode
      // which are only partially contained by the range.
      
      // lets handle the nodes reported by the iterator.  These nodes
      // are entirely contained in the selection range.  We build up
      // a list of them (since doing operations on the document during
      // iteration would perturb the iterator).

      nsCOMPtr<nsIContentIterator> iter;
      res = nsComponentManager::CreateInstance(kSubtreeIteratorCID, nsnull,
                                                NS_GET_IID(nsIContentIterator), 
                                                getter_AddRefs(iter));
      if (NS_FAILED(res)) return res;
      if (!iter)          return NS_ERROR_FAILURE;

      nsCOMPtr<nsISupportsArray> arrayOfNodes;
      nsCOMPtr<nsIContent> content;
      nsCOMPtr<nsIDOMNode> node;
      nsCOMPtr<nsISupports> isupports;
      
      // make a array
      res = NS_NewISupportsArray(getter_AddRefs(arrayOfNodes));
      if (NS_FAILED(res)) return res;
      
      // iterate range and build up array
      res = iter->Init(range);
      if (NS_SUCCEEDED(res))
      {
        while (NS_ENUMERATOR_FALSE == iter->IsDone())
        {
          res = iter->CurrentNode(getter_AddRefs(content));
          if (NS_FAILED(res)) return res;
          node = do_QueryInterface(content);
          if (!node) return NS_ERROR_FAILURE;
          if (IsEditable(node))
          { 
            isupports = do_QueryInterface(node);
            arrayOfNodes->AppendElement(isupports);
          }
          iter->Next();
        }
        
        // now that we have the list, do the font size change on each node
        PRUint32 listCount;
        PRUint32 j;
        arrayOfNodes->Count(&listCount);
        for (j = 0; j < listCount; j++)
        {
          isupports = dont_AddRef(arrayOfNodes->ElementAt(0));
          node = do_QueryInterface(isupports);
          res = RelativeFontChangeOnNode(aSizeChange, node);
          if (NS_FAILED(res)) return res;
          arrayOfNodes->RemoveElementAt(0);
        }
      }
      // now check the start and end parents of the range to see if they need to 
      // be seperately handled (they do if they are text nodes, due to how the
      // subtree iterator works - it will not have reported them).
      if (IsTextNode(startNode) && IsEditable(startNode))
      {
        nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(startNode);
        PRInt32 startOffset;
        PRUint32 textLen;
        range->GetStartOffset(&startOffset);
        nodeAsText->GetLength(&textLen);
        res = RelativeFontChangeOnTextNode(aSizeChange, nodeAsText, startOffset, textLen);
        if (NS_FAILED(res)) return res;
      }
      if (IsTextNode(endNode) && IsEditable(endNode))
      {
        nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(endNode);
        PRInt32 endOffset;
        range->GetEndOffset(&endOffset);
        res = RelativeFontChangeOnTextNode(aSizeChange, nodeAsText, 0, endOffset);
        if (NS_FAILED(res)) return res;
      }
    }
    enumerator->Next();
  }
  
  return res;  
}

nsresult
nsHTMLEditor::RelativeFontChangeOnTextNode( PRInt32 aSizeChange, 
                                            nsIDOMCharacterData *aTextNode, 
                                            PRInt32 aStartOffset,
                                            PRInt32 aEndOffset)
{
  // Can only change font size by + or - 1
  if ( !( (aSizeChange==1) || (aSizeChange==-1) ) )
    return NS_ERROR_ILLEGAL_VALUE;
  if (!aTextNode) return NS_ERROR_NULL_POINTER;
  
  // dont need to do anything if no characters actually selected
  if (aStartOffset == aEndOffset) return NS_OK;
  
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> tmp, node = do_QueryInterface(aTextNode);
  
  // do we need to split the text node?
  PRUint32 textLen;
  aTextNode->GetLength(&textLen);
  
  // -1 is a magic value meaning to the end of node
  if (aEndOffset == -1) aEndOffset = textLen;
  
  if ( (PRUint32)aEndOffset != textLen )
  {
    // we need to split off back of text node
    res = SplitNode(node, aEndOffset, getter_AddRefs(tmp));
    if (NS_FAILED(res)) return res;
    node = tmp;  // remember left node
  }
  if ( aStartOffset )
  {
    // we need to split off front of text node
    res = SplitNode(node, aStartOffset, getter_AddRefs(tmp));
    if (NS_FAILED(res)) return res;
  }
  
  // reparent the node inside font node with appropriate relative size
  res = InsertContainerAbove(node, address_of(tmp), NS_ConvertASCIItoUCS2(aSizeChange==1 ? "big" : "small"));
  return res;
}


nsresult
nsHTMLEditor::RelativeFontChangeOnNode( PRInt32 aSizeChange, 
                                        nsIDOMNode *aNode)
{
  // Can only change font size by + or - 1
  if ( !( (aSizeChange==1) || (aSizeChange==-1) ) )
    return NS_ERROR_ILLEGAL_VALUE;
  if (!aNode) return NS_ERROR_NULL_POINTER;

  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> tmp;
  nsAutoString tag;
  if (aSizeChange == 1) tag.AssignWithConversion("big");
  else tag.AssignWithConversion("small");
  
  // is this node a text node?
  if (IsTextNode(aNode))
  {
    res = InsertContainerAbove(aNode, address_of(tmp), tag);
    return res;
  }
  // is it the opposite of what we want?  
  if ( ((aSizeChange == 1) && nsHTMLEditUtils::IsSmall(aNode)) || 
       ((aSizeChange == -1) &&  nsHTMLEditUtils::IsBig(aNode)) )
  {
    // in that case, just remove this node and pull up the children
    res = RemoveContainer(aNode);
    return res;
  }
  // can it be put inside a "big" or "small"?
  if (TagCanContain(tag, aNode))
  {
    // ok, chuck it in.
    res = InsertContainerAbove(aNode, address_of(tmp), tag);
    return res;
  }
  // none of the above?  then cycle through the children.
  // MOOSE: we should group the children together if possible
  // into a single "big" or "small".  For the moment they are
  // each getting their own.  
  nsCOMPtr<nsIDOMNodeList> childNodes;
  res = aNode->GetChildNodes(getter_AddRefs(childNodes));
  if (NS_FAILED(res)) return res;
  if (childNodes)
  {
    PRInt32 j;
    PRUint32 childCount;
    childNodes->GetLength(&childCount);
    for (j=0 ; j < (PRInt32)childCount; j++)
    {
      nsCOMPtr<nsIDOMNode> childNode;
      res = childNodes->Item(j, getter_AddRefs(childNode));
      if ((NS_SUCCEEDED(res)) && (childNode))
      {
        res = RelativeFontChangeOnNode(aSizeChange, childNode);
        if (NS_FAILED(res)) return res;
      }
    }
  }
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::GetFontFaceState(PRBool *aMixed, nsAWritableString &outFace)
{
  if (!aMixed)
      return NS_ERROR_FAILURE;
  *aMixed = PR_TRUE;
  //outFace.AssignWithConversion("");
  outFace.SetLength(0);

  nsresult res;
  nsAutoString faceStr;
  faceStr.AssignWithConversion("face");
  PRBool first, any, all;
  
  
  res = GetInlinePropertyBase(nsIEditProperty::font, &faceStr, nsnull, &first, &any, &all, &outFace);
  if (NS_FAILED(res)) return res;
  if (any && !all) return res; // mixed
  if (all)
  {
    *aMixed = PR_FALSE;
    return res;
  }
  
  res = GetInlinePropertyBase(nsIEditProperty::tt, nsnull, nsnull, &first, &any, &all,nsnull);
  if (NS_FAILED(res)) return res;
  if (any && !all) return res; // mixed
  if (all)
  {
    *aMixed = PR_FALSE;
    nsIEditProperty::tt->ToString(outFace);
  }
  
  if (!any)
  {
    // there was no font face attrs of any kind.  We are in normal font.
    outFace.SetLength(0);
    *aMixed = PR_FALSE;
  }
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::GetFontColorState(PRBool *aMixed, nsAWritableString &aOutColor)
{
  if (!aMixed)
      return NS_ERROR_NULL_POINTER;
  *aMixed = PR_TRUE;
  aOutColor.SetLength(0);
  
  nsresult res;
  nsAutoString colorStr; colorStr.AssignWithConversion("color");
  PRBool first, any, all;
  
  res = GetInlinePropertyBase(nsIEditProperty::font, &colorStr, nsnull, &first, &any, &all, &aOutColor);
  if (NS_FAILED(res)) return res;
  if (any && !all) return res; // mixed
  if (all)
  {
    *aMixed = PR_FALSE;
    return res;
  }
  
  if (!any)
  {
    // there was no font color attrs of any kind..
    aOutColor.SetLength(0);
    *aMixed = PR_FALSE;
  }
  return res;
}

