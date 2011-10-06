/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Daniel Glazman <glazman@netscape.com>
 *   Mats Palmgren <matspal@gmail.com>
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
#include "nsUnicharUtils.h"

#include "nsHTMLEditor.h"
#include "nsHTMLEditRules.h"
#include "nsTextEditUtils.h"
#include "nsHTMLEditUtils.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMAttr.h"
#include "nsIDOMMouseEvent.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsISelectionController.h"
#include "nsIDocumentObserver.h"
#include "TypeInState.h"

#include "nsIEnumerator.h"
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsAttrName.h"


NS_IMETHODIMP nsHTMLEditor::AddDefaultProperty(nsIAtom *aProperty, 
                                            const nsAString & aAttribute, 
                                            const nsAString & aValue)
{
  nsString outValue;
  PRInt32 index;
  nsString attr(aAttribute);
  if (TypeInState::FindPropInList(aProperty, attr, &outValue, mDefaultStyles, index))
  {
    PropItem *item = mDefaultStyles[index];
    item->value = aValue;
  }
  else
  {
    nsString value(aValue);
    PropItem *propItem = new PropItem(aProperty, attr, value);
    mDefaultStyles.AppendElement(propItem);
  }
  return NS_OK;
}

NS_IMETHODIMP nsHTMLEditor::RemoveDefaultProperty(nsIAtom *aProperty, 
                                   const nsAString & aAttribute, 
                                   const nsAString & aValue)
{
  nsString outValue;
  PRInt32 index;
  nsString attr(aAttribute);
  if (TypeInState::FindPropInList(aProperty, attr, &outValue, mDefaultStyles, index))
  {
    delete mDefaultStyles[index];
    mDefaultStyles.RemoveElementAt(index);
  }
  return NS_OK;
}

NS_IMETHODIMP nsHTMLEditor::RemoveAllDefaultProperties()
{
  PRUint32 j, defcon = mDefaultStyles.Length();
  for (j=0; j<defcon; j++)
  {
    delete mDefaultStyles[j];
  }
  mDefaultStyles.Clear();
  return NS_OK;
}


// Add the CSS style corresponding to the HTML inline style defined
// by aProperty aAttribute and aValue to the selection
NS_IMETHODIMP nsHTMLEditor::SetCSSInlineProperty(nsIAtom *aProperty, 
                            const nsAString & aAttribute, 
                            const nsAString & aValue)
{
  PRBool useCSS;
  GetIsCSSEnabled(&useCSS);
  if (useCSS) {
    return SetInlineProperty(aProperty, aAttribute, aValue);
  }
  return NS_OK;
}

NS_IMETHODIMP nsHTMLEditor::SetInlineProperty(nsIAtom *aProperty, 
                            const nsAString & aAttribute, 
                            const nsAString & aValue)
{
  if (!aProperty) { return NS_ERROR_NULL_POINTER; }
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }
  ForceCompositionEnd();

  nsCOMPtr<nsISelection>selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
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
  NS_ENSURE_SUCCESS(res, res);
  if (!cancel && !handled)
  {
    // get selection range enumerator
    nsCOMPtr<nsIEnumerator> enumerator;
    res = selPriv->GetEnumerator(getter_AddRefs(enumerator));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(enumerator, NS_ERROR_FAILURE);

    // loop thru the ranges in the selection
    enumerator->First(); 
    nsCOMPtr<nsISupports> currentItem;
    while ((NS_ENUMERATOR_FALSE == enumerator->IsDone()))
    {
      res = enumerator->CurrentItem(getter_AddRefs(currentItem));
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_TRUE(currentItem, NS_ERROR_FAILURE);
      
      nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );

      // adjust range to include any ancestors who's children are entirely selected
      res = PromoteInlineRange(range);
      NS_ENSURE_SUCCESS(res, res);
      
      // check for easy case: both range endpoints in same text node
      nsCOMPtr<nsIDOMNode> startNode, endNode;
      res = range->GetStartContainer(getter_AddRefs(startNode));
      NS_ENSURE_SUCCESS(res, res);
      res = range->GetEndContainer(getter_AddRefs(endNode));
      NS_ENSURE_SUCCESS(res, res);
      if ((startNode == endNode) && IsTextNode(startNode))
      {
        PRInt32 startOffset, endOffset;
        range->GetStartOffset(&startOffset);
        range->GetEndOffset(&endOffset);
        nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(startNode);
        res = SetInlinePropertyOnTextNode(nodeAsText, startOffset, endOffset, aProperty, &aAttribute, &aValue);
        NS_ENSURE_SUCCESS(res, res);
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

        nsCOMPtr<nsIContentIterator> iter =
          do_CreateInstance("@mozilla.org/content/subtree-content-iterator;1", &res);
        NS_ENSURE_SUCCESS(res, res);
        NS_ENSURE_TRUE(iter, NS_ERROR_FAILURE);

        nsCOMArray<nsIDOMNode> arrayOfNodes;
        nsCOMPtr<nsIDOMNode> node;
        
        // iterate range and build up array
        res = iter->Init(range);
        // init returns an error if no nodes in range.
        // this can easily happen with the subtree 
        // iterator if the selection doesn't contain
        // any *whole* nodes.
        if (NS_SUCCEEDED(res))
        {
          while (!iter->IsDone())
          {
            node = do_QueryInterface(iter->GetCurrentNode());
            NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

            if (IsEditable(node))
            { 
              arrayOfNodes.AppendObject(node);
            }

            iter->Next();
          }
        }
        // first check the start parent of the range to see if it needs to 
        // be separately handled (it does if it's a text node, due to how the
        // subtree iterator works - it will not have reported it).
        if (IsTextNode(startNode) && IsEditable(startNode))
        {
          nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(startNode);
          PRInt32 startOffset;
          PRUint32 textLen;
          range->GetStartOffset(&startOffset);
          nodeAsText->GetLength(&textLen);
          res = SetInlinePropertyOnTextNode(nodeAsText, startOffset, textLen, aProperty, &aAttribute, &aValue);
          NS_ENSURE_SUCCESS(res, res);
        }
        
        // then loop through the list, set the property on each node
        PRInt32 listCount = arrayOfNodes.Count();
        PRInt32 j;
        for (j = 0; j < listCount; j++)
        {
          node = arrayOfNodes[j];
          res = SetInlinePropertyOnNode(node, aProperty, &aAttribute, &aValue);
          NS_ENSURE_SUCCESS(res, res);
        }
        arrayOfNodes.Clear();
        
        // last check the end parent of the range to see if it needs to 
        // be separately handled (it does if it's a text node, due to how the
        // subtree iterator works - it will not have reported it).
        if (IsTextNode(endNode) && IsEditable(endNode))
        {
          nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(endNode);
          PRInt32 endOffset;
          range->GetEndOffset(&endOffset);
          res = SetInlinePropertyOnTextNode(nodeAsText, 0, endOffset, aProperty, &aAttribute, &aValue);
          NS_ENSURE_SUCCESS(res, res);
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
                                            const nsAString *aAttribute,
                                            const nsAString *aValue)
{
  NS_ENSURE_TRUE(aTextNode, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIDOMNode> parent;
  nsresult res = aTextNode->GetParentNode(getter_AddRefs(parent));
  NS_ENSURE_SUCCESS(res, res);

  nsAutoString tagString;
  aProperty->ToString(tagString);
  if (!CanContainTag(parent, tagString)) return NS_OK;
  
  // don't need to do anything if no characters actually selected
  if (aStartOffset == aEndOffset) return NS_OK;
  
  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aTextNode);
  
  // don't need to do anything if property already set on node
  PRBool bHasProp;
  PRBool useCSS;
  GetIsCSSEnabled(&useCSS);

  if (useCSS &&
      mHTMLCSSUtils->IsCSSEditableProperty(node, aProperty, aAttribute)) {
    // the HTML styles defined by aProperty/aAttribute has a CSS equivalence
    // in this implementation for node; let's check if it carries those css styles
    nsAutoString value;
    if (aValue) value.Assign(*aValue);
    mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(node, aProperty, aAttribute,
                                                       bHasProp, value,
                                                       COMPUTED_STYLE_TYPE);
  }
  else
  {
    nsCOMPtr<nsIDOMNode> styleNode;
    IsTextPropertySetByContent(node, aProperty, aAttribute, aValue, bHasProp, getter_AddRefs(styleNode));
  }

  if (bHasProp) return NS_OK;
  
  // do we need to split the text node?
  PRUint32 textLen;
  aTextNode->GetLength(&textLen);
  
  nsCOMPtr<nsIDOMNode> tmp;
  if ( (PRUint32)aEndOffset != textLen )
  {
    // we need to split off back of text node
    res = SplitNode(node, aEndOffset, getter_AddRefs(tmp));
    NS_ENSURE_SUCCESS(res, res);
    node = tmp;  // remember left node
  }
  if ( aStartOffset )
  {
    // we need to split off front of text node
    res = SplitNode(node, aStartOffset, getter_AddRefs(tmp));
    NS_ENSURE_SUCCESS(res, res);
  }
  
  // look for siblings that are correct type of node
  nsCOMPtr<nsIDOMNode> sibling;
  GetPriorHTMLSibling(node, address_of(sibling));
  if (sibling && NodeIsType(sibling, aProperty) &&         
      HasAttrVal(sibling, aAttribute, aValue) &&
      IsOnlyAttribute(sibling, aAttribute) )
  {
    // previous sib is already right kind of inline node; slide this over into it
    res = MoveNode(node, sibling, -1);
    return res;
  }
  sibling = nsnull;
  GetNextHTMLSibling(node, address_of(sibling));
  if (sibling && NodeIsType(sibling, aProperty) &&         
      HasAttrVal(sibling, aAttribute, aValue) &&
      IsOnlyAttribute(sibling, aAttribute) )
  {
    // following sib is already right kind of inline node; slide this over into it
    res = MoveNode(node, sibling, 0);
    return res;
  }
  
  // reparent the node inside inline node with appropriate {attribute,value}
  return SetInlinePropertyOnNode(node, aProperty, aAttribute, aValue);
}


nsresult
nsHTMLEditor::SetInlinePropertyOnNode( nsIDOMNode *aNode,
                                       nsIAtom *aProperty, 
                                       const nsAString *aAttribute,
                                       const nsAString *aValue)
{
  NS_ENSURE_TRUE(aNode && aProperty, NS_ERROR_NULL_POINTER);

  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> tmp;
  nsAutoString tag;
  aProperty->ToString(tag);
  ToLowerCase(tag);
  
  PRBool useCSS;
  GetIsCSSEnabled(&useCSS);

  if (useCSS)
  {
    // we are in CSS mode
    if (mHTMLCSSUtils->IsCSSEditableProperty(aNode, aProperty, aAttribute))
    {
      // the HTML style defined by aProperty/aAttribute has a CSS equivalence
      // in this implementation for the node aNode
      nsCOMPtr<nsIDOMNode> tmp = aNode;
      if (IsTextNode(tmp))
      {
        // we are working on a text node and need to create a span container
        // that will carry the styles
        InsertContainerAbove( aNode, 
                              address_of(tmp), 
                              NS_LITERAL_STRING("span"),
                              nsnull,
                              nsnull);
      }
      nsCOMPtr<nsIDOMElement>element;
      element = do_QueryInterface(tmp);
      // first we have to remove occurences of the same style hint in the
      // children of the aNode
      res = RemoveStyleInside(tmp, aProperty, aAttribute, PR_TRUE);
      NS_ENSURE_SUCCESS(res, res);
      PRInt32 count;
      // then we add the css styles corresponding to the HTML style request
      res = mHTMLCSSUtils->SetCSSEquivalentToHTMLStyle(element, aProperty, aAttribute, aValue, &count, PR_FALSE);
      NS_ENSURE_SUCCESS(res, res);

      nsCOMPtr<nsIDOMNode> nextSibling, previousSibling;
      GetNextHTMLSibling(tmp, address_of(nextSibling));
      GetPriorHTMLSibling(tmp, address_of(previousSibling));
      if (nextSibling || previousSibling)
      {
        nsCOMPtr<nsIDOMNode> mergeParent;
        res = tmp->GetParentNode(getter_AddRefs(mergeParent));
        NS_ENSURE_SUCCESS(res, res);
        if (previousSibling &&
            nsEditor::NodeIsType(previousSibling, nsEditProperty::span) &&
            NodesSameType(tmp, previousSibling))
        {
          res = JoinNodes(previousSibling, tmp, mergeParent);
          NS_ENSURE_SUCCESS(res, res);
        }
        if (nextSibling &&
            nsEditor::NodeIsType(nextSibling, nsEditProperty::span) &&
            NodesSameType(tmp, nextSibling))
        {
          res = JoinNodes(tmp, nextSibling, mergeParent);
        }
      }
      return res;
    }
  }
  
  // don't need to do anything if property already set on node
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
    NS_ENSURE_SUCCESS(res, res);
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
    NS_ENSURE_SUCCESS(res, res);
    return RemoveStyleInside(aNode, aProperty, aAttribute);
  }
  // none of the above?  then cycle through the children.
  nsCOMPtr<nsIDOMNodeList> childNodes;
  res = aNode->GetChildNodes(getter_AddRefs(childNodes));
  NS_ENSURE_SUCCESS(res, res);
  if (childNodes)
  {
    PRInt32 j;
    PRUint32 childCount;
    childNodes->GetLength(&childCount);
    if (childCount)
    {
      nsCOMArray<nsIDOMNode> arrayOfNodes;
      nsCOMPtr<nsIDOMNode> node;
      
      // populate the list
      for (j=0 ; j < (PRInt32)childCount; j++)
      {
        nsCOMPtr<nsIDOMNode> childNode;
        res = childNodes->Item(j, getter_AddRefs(childNode));
        if ((NS_SUCCEEDED(res)) && (childNode) && IsEditable(childNode))
        {
          arrayOfNodes.AppendObject(childNode);
        }
      }
      
      // then loop through the list, set the property on each node
      PRInt32 listCount = arrayOfNodes.Count();
      for (j = 0; j < listCount; j++)
      {
        node = arrayOfNodes[j];
        res = SetInlinePropertyOnNode(node, aProperty, aAttribute, aValue);
        NS_ENSURE_SUCCESS(res, res);
      }
      arrayOfNodes.Clear();
    }
  }
  return res;
}


nsresult nsHTMLEditor::SplitStyleAboveRange(nsIDOMRange *inRange, 
                                            nsIAtom *aProperty, 
                                            const nsAString *aAttribute)
{
  NS_ENSURE_TRUE(inRange, NS_ERROR_NULL_POINTER);
  nsresult res;
  nsCOMPtr<nsIDOMNode> startNode, endNode, origStartNode;
  PRInt32 startOffset, endOffset, origStartOffset;
  
  res = inRange->GetStartContainer(getter_AddRefs(startNode));
  NS_ENSURE_SUCCESS(res, res);
  res = inRange->GetStartOffset(&startOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = inRange->GetEndContainer(getter_AddRefs(endNode));
  NS_ENSURE_SUCCESS(res, res);
  res = inRange->GetEndOffset(&endOffset);
  NS_ENSURE_SUCCESS(res, res);
  
  origStartNode = startNode;
  origStartOffset = startOffset;
  
  // split any matching style nodes above the start of range
  {
    nsAutoTrackDOMPoint tracker(mRangeUpdater, address_of(endNode), &endOffset);
    res = SplitStyleAbovePoint(address_of(startNode), &startOffset, aProperty, aAttribute);
    NS_ENSURE_SUCCESS(res, res);
  }

  // second verse, same as the first...
  res = SplitStyleAbovePoint(address_of(endNode), &endOffset, aProperty, aAttribute);
  NS_ENSURE_SUCCESS(res, res);
  
  // reset the range
  res = inRange->SetStart(startNode, startOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = inRange->SetEnd(endNode, endOffset);
  return res;
}

nsresult nsHTMLEditor::SplitStyleAbovePoint(nsCOMPtr<nsIDOMNode> *aNode,
                                           PRInt32 *aOffset,
                                           nsIAtom *aProperty,          // null here means we split all properties
                                           const nsAString *aAttribute,
                                           nsCOMPtr<nsIDOMNode> *outLeftNode,
                                           nsCOMPtr<nsIDOMNode> *outRightNode)
{
  NS_ENSURE_TRUE(aNode && *aNode && aOffset, NS_ERROR_NULL_POINTER);
  if (outLeftNode)  *outLeftNode  = nsnull;
  if (outRightNode) *outRightNode = nsnull;
  // split any matching style nodes above the node/offset
  nsCOMPtr<nsIDOMNode> parent, tmp = *aNode;
  PRInt32 offset;

  PRBool useCSS;
  GetIsCSSEnabled(&useCSS);

  PRBool isSet;
  while (tmp && !IsBlockNode(tmp))
  {
    isSet = PR_FALSE;
    if (useCSS && mHTMLCSSUtils->IsCSSEditableProperty(tmp, aProperty, aAttribute)) {
      // the HTML style defined by aProperty/aAttribute has a CSS equivalence
      // in this implementation for the node tmp; let's check if it carries those css styles
      nsAutoString firstValue;
      mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(tmp, aProperty, aAttribute,
                                                         isSet, firstValue,
                                                         SPECIFIED_STYLE_TYPE);
    }
    if ( (aProperty && NodeIsType(tmp, aProperty)) ||   // node is the correct inline prop
         (aProperty == nsEditProperty::href && nsHTMLEditUtils::IsLink(tmp)) ||
                                                        // node is href - test if really <a href=...
         (!aProperty && NodeIsProperty(tmp)) ||         // or node is any prop, and we asked to split them all
         isSet)                                         // or the style is specified in the style attribute
    {
      // found a style node we need to split
      nsresult rv = SplitNodeDeep(tmp, *aNode, *aOffset, &offset, PR_FALSE,
                                  outLeftNode, outRightNode);
      NS_ENSURE_SUCCESS(rv, rv);
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
  NS_ENSURE_TRUE(aNode, PR_FALSE);
  if (!IsContainer(aNode))  return PR_FALSE;
  if (!IsEditable(aNode))   return PR_FALSE;
  if (IsBlockNode(aNode))   return PR_FALSE;
  if (NodeIsType(aNode, nsEditProperty::a)) return PR_FALSE;
  return PR_TRUE;
}

nsresult nsHTMLEditor::ApplyDefaultProperties()
{
  nsresult res = NS_OK;
  PRUint32 j, defcon = mDefaultStyles.Length();
  for (j=0; j<defcon; j++)
  {
    PropItem *propItem = mDefaultStyles[j];
    NS_ENSURE_TRUE(propItem, NS_ERROR_NULL_POINTER);
    res = SetInlineProperty(propItem->tag, propItem->attr, propItem->value);
    NS_ENSURE_SUCCESS(res, res);
  }
  return res;
}

nsresult nsHTMLEditor::RemoveStyleInside(nsIDOMNode *aNode, 
                                   nsIAtom *aProperty,   // null here means remove all properties
                                   const nsAString *aAttribute, 
                                   PRBool aChildrenOnly)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);
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
    NS_ENSURE_SUCCESS(res, res);
    child = tmp;
  }

  // then process the node itself
  if ( !aChildrenOnly && 
        (aProperty && NodeIsType(aNode, aProperty) || // node is prop we asked for
        (aProperty == nsEditProperty::href && nsHTMLEditUtils::IsLink(aNode)) || // but check for link (<a href=...)
        (aProperty == nsEditProperty::name && nsHTMLEditUtils::IsNamedAnchor(aNode))) || // and for named anchors
        (!aProperty && NodeIsProperty(aNode)))  // or node is any prop and we asked for that
  {
    // if we weren't passed an attribute, then we want to 
    // remove any matching inlinestyles entirely
    if (!aAttribute || aAttribute->IsEmpty())
    {
      NS_NAMED_LITERAL_STRING(styleAttr, "style");
      NS_NAMED_LITERAL_STRING(classAttr, "class");
      PRBool hasStyleAttr = HasAttr(aNode, &styleAttr);
      PRBool hasClassAtrr = HasAttr(aNode, &classAttr);
      if (aProperty &&
          (hasStyleAttr || hasClassAtrr)) {
        // aNode carries inline styles or a class attribute so we can't
        // just remove the element... We need to create above the element
        // a span that will carry those styles or class, then we can delete
        // the node.
        nsCOMPtr<nsIDOMNode> spanNode;
        res = InsertContainerAbove(aNode, address_of(spanNode),
                                   NS_LITERAL_STRING("span"));
        NS_ENSURE_SUCCESS(res, res);
        res = CloneAttribute(styleAttr, spanNode, aNode);
        NS_ENSURE_SUCCESS(res, res);
        res = CloneAttribute(classAttr, spanNode, aNode);
        NS_ENSURE_SUCCESS(res, res);
        if (hasStyleAttr)
        {
          // we need to remove the styles property corresponding to
          // aProperty (bug 215406)
          nsAutoString propertyValue;
          mHTMLCSSUtils->RemoveCSSEquivalentToHTMLStyle(spanNode,
                                                        aProperty,
                                                        aAttribute,
                                                        &propertyValue,
                                                        PR_FALSE);
          // remove the span if it's useless
          nsCOMPtr<nsIDOMElement> element = do_QueryInterface(spanNode);
          res = RemoveElementIfNoStyleOrIdOrClass(element, nsEditProperty::span);
        }
      }
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
          NS_ENSURE_TRUE(elem, NS_ERROR_NULL_POINTER);
          res = RemoveAttribute(elem, *aAttribute);
        }
      }
    }
  }
  else {
    PRBool useCSS;
    GetIsCSSEnabled(&useCSS);

    if (!aChildrenOnly
        && useCSS && mHTMLCSSUtils->IsCSSEditableProperty(aNode, aProperty, aAttribute)) {
      // the HTML style defined by aProperty/aAttribute has a CSS equivalence
      // in this implementation for the node aNode; let's check if it carries those css styles
      nsAutoString propertyValue;
      PRBool isSet;
      mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(aNode, aProperty, aAttribute,
                                                         isSet, propertyValue,
                                                         SPECIFIED_STYLE_TYPE);
      if (isSet) {
        // yes, tmp has the corresponding css declarations in its style attribute
        // let's remove them
        mHTMLCSSUtils->RemoveCSSEquivalentToHTMLStyle(aNode,
                                                      aProperty,
                                                      aAttribute,
                                                      &propertyValue,
                                                      PR_FALSE);
        // remove the node if it is a span, if its style attribute is empty or absent,
        // and if it does not have a class nor an id
        nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aNode);
        res = RemoveElementIfNoStyleOrIdOrClass(element, nsEditProperty::span);
      }
    }
  }  
  if ( aProperty == nsEditProperty::font &&    // or node is big or small and we are setting font size
       (nsHTMLEditUtils::IsBig(aNode) || nsHTMLEditUtils::IsSmall(aNode)) &&
       aAttribute && aAttribute->LowerCaseEqualsLiteral("size"))       
  {
    res = RemoveContainer(aNode);  // if we are setting font size, remove any nested bigs and smalls
  }
  return res;
}

PRBool nsHTMLEditor::IsOnlyAttribute(nsIDOMNode *aNode, 
                                     const nsAString *aAttribute)
{
  NS_ENSURE_TRUE(aNode && aAttribute, PR_FALSE);  // ooops
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(content, PR_FALSE);  // ooops
  
  PRUint32 i, attrCount = content->GetAttrCount();
  for (i = 0; i < attrCount; ++i) {
    nsAutoString attrString;
    const nsAttrName* name = content->GetAttrNameAt(i);
    if (!name->NamespaceEquals(kNameSpaceID_None)) {
      return PR_FALSE;
    }
    name->LocalName()->ToString(attrString);
    // if it's the attribute we know about, or a special _moz attribute,
    // keep looking
    if (!attrString.Equals(*aAttribute, nsCaseInsensitiveStringComparator()) &&
        !StringBeginsWith(attrString, NS_LITERAL_STRING("_moz"))) {
      return PR_FALSE;
    }
  }
  // if we made it through all of them without finding a real attribute
  // other than aAttribute, then return PR_TRUE
  return PR_TRUE;
}

PRBool nsHTMLEditor::HasAttr(nsIDOMNode *aNode, 
                             const nsAString *aAttribute)
{
  NS_ENSURE_TRUE(aNode, PR_FALSE);
  if (!aAttribute || aAttribute->IsEmpty()) return PR_TRUE;  // everybody has the 'null' attribute
  
  // get element
  nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(elem, PR_FALSE);
  
  // get attribute node
  nsCOMPtr<nsIDOMAttr> attNode;
  nsresult res = elem->GetAttributeNode(*aAttribute, getter_AddRefs(attNode));
  if ((NS_FAILED(res)) || !attNode) return PR_FALSE;
  return PR_TRUE;
}


PRBool nsHTMLEditor::HasAttrVal(nsIDOMNode *aNode, 
                                const nsAString *aAttribute, 
                                const nsAString *aValue)
{
  NS_ENSURE_TRUE(aNode, PR_FALSE);
  if (!aAttribute || aAttribute->IsEmpty()) return PR_TRUE;  // everybody has the 'null' attribute
  
  // get element
  nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(elem, PR_FALSE);
  
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
  if (attrVal.Equals(*aValue,nsCaseInsensitiveStringComparator())) return PR_TRUE;
  return PR_FALSE;
}

nsresult nsHTMLEditor::PromoteRangeIfStartsOrEndsInNamedAnchor(nsIDOMRange *inRange)
{
  NS_ENSURE_TRUE(inRange, NS_ERROR_NULL_POINTER);
  nsresult res;
  nsCOMPtr<nsIDOMNode> startNode, endNode, parent, tmp;
  PRInt32 startOffset, endOffset, tmpOffset;
  
  res = inRange->GetStartContainer(getter_AddRefs(startNode));
  NS_ENSURE_SUCCESS(res, res);
  res = inRange->GetStartOffset(&startOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = inRange->GetEndContainer(getter_AddRefs(endNode));
  NS_ENSURE_SUCCESS(res, res);
  res = inRange->GetEndOffset(&endOffset);
  NS_ENSURE_SUCCESS(res, res);

  tmp = startNode;
  while ( tmp && 
          !nsTextEditUtils::IsBody(tmp) &&
          !nsHTMLEditUtils::IsNamedAnchor(tmp))
  {
    res = GetNodeLocation(tmp, address_of(parent), &tmpOffset);
    NS_ENSURE_SUCCESS(res, res);
    tmp = parent;
  }
  NS_ENSURE_TRUE(tmp, NS_ERROR_NULL_POINTER);
  if (nsHTMLEditUtils::IsNamedAnchor(tmp))
  {
    res = GetNodeLocation(tmp, address_of(parent), &tmpOffset);
    NS_ENSURE_SUCCESS(res, res);
    startNode = parent;
    startOffset = tmpOffset;
  }

  tmp = endNode;
  while ( tmp && 
          !nsTextEditUtils::IsBody(tmp) &&
          !nsHTMLEditUtils::IsNamedAnchor(tmp))
  {
    res = GetNodeLocation(tmp, address_of(parent), &tmpOffset);
    NS_ENSURE_SUCCESS(res, res);
    tmp = parent;
  }
  NS_ENSURE_TRUE(tmp, NS_ERROR_NULL_POINTER);
  if (nsHTMLEditUtils::IsNamedAnchor(tmp))
  {
    res = GetNodeLocation(tmp, address_of(parent), &tmpOffset);
    NS_ENSURE_SUCCESS(res, res);
    endNode = parent;
    endOffset = tmpOffset + 1;
  }

  res = inRange->SetStart(startNode, startOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = inRange->SetEnd(endNode, endOffset);
  return res;
}

nsresult nsHTMLEditor::PromoteInlineRange(nsIDOMRange *inRange)
{
  NS_ENSURE_TRUE(inRange, NS_ERROR_NULL_POINTER);
  nsresult res;
  nsCOMPtr<nsIDOMNode> startNode, endNode, parent;
  PRInt32 startOffset, endOffset;
  
  res = inRange->GetStartContainer(getter_AddRefs(startNode));
  NS_ENSURE_SUCCESS(res, res);
  res = inRange->GetStartOffset(&startOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = inRange->GetEndContainer(getter_AddRefs(endNode));
  NS_ENSURE_SUCCESS(res, res);
  res = inRange->GetEndOffset(&endOffset);
  NS_ENSURE_SUCCESS(res, res);
  
  while ( startNode && 
          !nsTextEditUtils::IsBody(startNode) && 
          IsEditable(startNode) &&
          IsAtFrontOfNode(startNode, startOffset) )
  {
    res = GetNodeLocation(startNode, address_of(parent), &startOffset);
    NS_ENSURE_SUCCESS(res, res);
    startNode = parent;
  }
  NS_ENSURE_TRUE(startNode, NS_ERROR_NULL_POINTER);
  
  while ( endNode && 
          !nsTextEditUtils::IsBody(endNode) && 
          IsEditable(endNode) &&
          IsAtEndOfNode(endNode, endOffset) )
  {
    res = GetNodeLocation(endNode, address_of(parent), &endOffset);
    NS_ENSURE_SUCCESS(res, res);
    endNode = parent;
    endOffset++;  // we are AFTER this node
  }
  NS_ENSURE_TRUE(endNode, NS_ERROR_NULL_POINTER);
  
  res = inRange->SetStart(startNode, startOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = inRange->SetEnd(endNode, endOffset);
  return res;
}

PRBool nsHTMLEditor::IsAtFrontOfNode(nsIDOMNode *aNode, PRInt32 aOffset)
{
  NS_ENSURE_TRUE(aNode, PR_FALSE);  // oops
  if (!aOffset) {
    return PR_TRUE;
  }

  if (IsTextNode(aNode))
  {
    return PR_FALSE;
  }
  else
  {
    nsCOMPtr<nsIDOMNode> firstNode;
    GetFirstEditableChild(aNode, address_of(firstNode));
    NS_ENSURE_TRUE(firstNode, PR_TRUE); 
    PRInt32 offset;
    nsEditor::GetChildOffset(firstNode, aNode, offset);
    if (offset < aOffset) return PR_FALSE;
    return PR_TRUE;
  }
}

PRBool nsHTMLEditor::IsAtEndOfNode(nsIDOMNode *aNode, PRInt32 aOffset)
{
  NS_ENSURE_TRUE(aNode, PR_FALSE);  // oops
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
    NS_ENSURE_TRUE(lastNode, PR_TRUE); 
    PRInt32 offset;
    nsEditor::GetChildOffset(lastNode, aNode, offset);
    if (offset < aOffset) return PR_TRUE;
    return PR_FALSE;
  }
}


nsresult
nsHTMLEditor::GetInlinePropertyBase(nsIAtom *aProperty, 
                             const nsAString *aAttribute,
                             const nsAString *aValue,
                             PRBool *aFirst, 
                             PRBool *aAny, 
                             PRBool *aAll,
                             nsAString *outValue,
                             PRBool aCheckDefaults)
{
  NS_ENSURE_TRUE(aProperty, NS_ERROR_NULL_POINTER);

  nsresult result;
  *aAny=PR_FALSE;
  *aAll=PR_TRUE;
  *aFirst=PR_FALSE;
  PRBool first=PR_TRUE;

  PRBool useCSS;
  GetIsCSSEnabled(&useCSS);

  nsCOMPtr<nsISelection>selection;
  result = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(result, result);
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));

  PRBool isCollapsed;
  selection->GetIsCollapsed(&isCollapsed);
  nsCOMPtr<nsIDOMNode> collapsedNode;
  nsCOMPtr<nsIEnumerator> enumerator;
  result = selPriv->GetEnumerator(getter_AddRefs(enumerator));
  NS_ENSURE_SUCCESS(result, result);
  NS_ENSURE_TRUE(enumerator, NS_ERROR_NULL_POINTER);

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
      range->GetStartContainer(getter_AddRefs(collapsedNode));
      NS_ENSURE_TRUE(collapsedNode, NS_ERROR_FAILURE);
      PRBool isSet, theSetting;
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
      if (!useCSS) {
        nsCOMPtr<nsIDOMNode> resultNode;
        IsTextPropertySetByContent(collapsedNode, aProperty, aAttribute, aValue,
                                   isSet, getter_AddRefs(resultNode), outValue);
        *aFirst = *aAny = *aAll = isSet;
        
        if (!isSet && aCheckDefaults) 
        {
          // style not set, but if it is a default then it will appear if 
          // content is inserted, so we should report it as set (analogous to TypeInState).
          PRInt32 index;
          if (aAttribute &&
              TypeInState::FindPropInList(aProperty, *aAttribute, outValue, mDefaultStyles, index))
          {
            *aFirst = *aAny = *aAll = PR_TRUE;
            if (outValue)
              outValue->Assign(mDefaultStyles[index]->value);
          }
        }
        return NS_OK;
      }
    }

    // non-collapsed selection
    nsCOMPtr<nsIContentIterator> iter =
            do_CreateInstance("@mozilla.org/content/post-content-iterator;1");
    NS_ENSURE_TRUE(iter, NS_ERROR_NULL_POINTER);

    iter->Init(range);
    nsAutoString firstValue, theValue;

    nsCOMPtr<nsIDOMNode> endNode;
    PRInt32 endOffset;
    result = range->GetEndContainer(getter_AddRefs(endNode));
    NS_ENSURE_SUCCESS(result, result);
    result = range->GetEndOffset(&endOffset);
    NS_ENSURE_SUCCESS(result, result);
    while (!iter->IsDone())
    {
      nsCOMPtr<nsIContent> content = do_QueryInterface(iter->GetCurrentNode());

      nsCOMPtr<nsIDOMNode> node = do_QueryInterface(content);

      if (node && nsTextEditUtils::IsBody(node))
        break;

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
            skipNode = PR_TRUE;
          }
        }
        else if (node == endNode && !endOffset)
        {
          skipNode = PR_TRUE;
        }
      }
      else if (content->IsElement())
      { // handle non-text leaf nodes here
        skipNode = PR_TRUE;
      }
      if (!skipNode)
      {
        if (node)
        {
          PRBool isSet = PR_FALSE;
          nsCOMPtr<nsIDOMNode>resultNode;
          if (first)
          {
            if (useCSS &&
                mHTMLCSSUtils->IsCSSEditableProperty(node, aProperty, aAttribute)) {
              // the HTML styles defined by aProperty/aAttribute has a CSS equivalence
              // in this implementation for node; let's check if it carries those css styles
              if (aValue) firstValue.Assign(*aValue);
              mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(node, aProperty, aAttribute,
                                                                 isSet, firstValue,
                                                                 COMPUTED_STYLE_TYPE);
            }
            else {
              IsTextPropertySetByContent(node, aProperty, aAttribute, aValue, isSet,
                                         getter_AddRefs(resultNode), &firstValue);
            }
            *aFirst = isSet;
            first = PR_FALSE;
            if (outValue) *outValue = firstValue;
          }
          else
          {
            if (useCSS &&
                mHTMLCSSUtils->IsCSSEditableProperty(node, aProperty, aAttribute)) {
              // the HTML styles defined by aProperty/aAttribute has a CSS equivalence
              // in this implementation for node; let's check if it carries those css styles
              if (aValue) theValue.Assign(*aValue);
              mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(node, aProperty, aAttribute,
                                                                 isSet, theValue,
                                                                 COMPUTED_STYLE_TYPE);
            }
            else {
              IsTextPropertySetByContent(node, aProperty, aAttribute, aValue, isSet,
                                         getter_AddRefs(resultNode), &theValue);
            }
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

      iter->Next();
    }
  }
  if (!*aAny) 
  { // make sure that if none of the selection is set, we don't report all is set
    *aAll = PR_FALSE;
  }
  return result;
}


NS_IMETHODIMP nsHTMLEditor::GetInlineProperty(nsIAtom *aProperty, 
                                              const nsAString &aAttribute, 
                                              const nsAString &aValue,
                                              PRBool *aFirst, 
                                              PRBool *aAny, 
                                              PRBool *aAll)
{
  NS_ENSURE_TRUE(aProperty && aFirst && aAny && aAll, NS_ERROR_NULL_POINTER);
  const nsAString *att = nsnull;
  if (!aAttribute.IsEmpty())
    att = &aAttribute;
  const nsAString *val = nsnull;
  if (!aValue.IsEmpty())
    val = &aValue;
  return GetInlinePropertyBase( aProperty, att, val, aFirst, aAny, aAll, nsnull);
}


NS_IMETHODIMP nsHTMLEditor::GetInlinePropertyWithAttrValue(nsIAtom *aProperty, 
                                              const nsAString &aAttribute, 
                                              const nsAString &aValue,
                                              PRBool *aFirst, 
                                              PRBool *aAny, 
                                              PRBool *aAll,
                                              nsAString &outValue)
{
  NS_ENSURE_TRUE(aProperty && aFirst && aAny && aAll, NS_ERROR_NULL_POINTER);
  const nsAString *att = nsnull;
  if (!aAttribute.IsEmpty())
    att = &aAttribute;
  const nsAString *val = nsnull;
  if (!aValue.IsEmpty())
    val = &aValue;
  return GetInlinePropertyBase( aProperty, att, val, aFirst, aAny, aAll, &outValue);
}


NS_IMETHODIMP nsHTMLEditor::RemoveAllInlineProperties()
{
  nsAutoEditBatch batchIt(this);
  nsAutoRules beginRulesSniffing(this, kOpResetTextProperties, nsIEditor::eNext);

  nsresult res = RemoveInlinePropertyImpl(nsnull, nsnull);
  NS_ENSURE_SUCCESS(res, res);
  return ApplyDefaultProperties();
}

NS_IMETHODIMP nsHTMLEditor::RemoveInlineProperty(nsIAtom *aProperty, const nsAString &aAttribute)
{
  return RemoveInlinePropertyImpl(aProperty, &aAttribute);
}

nsresult nsHTMLEditor::RemoveInlinePropertyImpl(nsIAtom *aProperty, const nsAString *aAttribute)
{
  NS_ENSURE_TRUE(mRules, NS_ERROR_NOT_INITIALIZED);
  ForceCompositionEnd();

  nsresult res;
  nsCOMPtr<nsISelection>selection;
  res = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));

  PRBool isCollapsed;
  selection->GetIsCollapsed(&isCollapsed);

  PRBool useCSS;
  GetIsCSSEnabled(&useCSS);

  if (isCollapsed)
  {
    // manipulating text attributes on a collapsed selection only sets state for the next text insertion

    // For links, aProperty uses "href", use "a" instead
    if (aProperty == nsEditProperty::href ||
        aProperty == nsEditProperty::name)
      aProperty = nsEditProperty::a;

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
  NS_ENSURE_SUCCESS(res, res);
  if (!cancel && !handled)
  {
    // get selection range enumerator
    nsCOMPtr<nsIEnumerator> enumerator;
    res = selPriv->GetEnumerator(getter_AddRefs(enumerator));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(enumerator, NS_ERROR_FAILURE);

    // loop thru the ranges in the selection
    enumerator->First(); 
    nsCOMPtr<nsISupports> currentItem;
    while ((NS_ENUMERATOR_FALSE == enumerator->IsDone()))
    {
      res = enumerator->CurrentItem(getter_AddRefs(currentItem));
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_TRUE(currentItem, NS_ERROR_FAILURE);
      
      nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );

      if (aProperty == nsEditProperty::name)
      {
        // promote range if it starts or end in a named anchor and we
        // want to remove named anchors
        res = PromoteRangeIfStartsOrEndsInNamedAnchor(range);
      }
      else {
        // adjust range to include any ancestors who's children are entirely selected
        res = PromoteInlineRange(range);
      }
      NS_ENSURE_SUCCESS(res, res);

      // remove this style from ancestors of our range endpoints, 
      // splitting them as appropriate
      res = SplitStyleAboveRange(range, aProperty, aAttribute);
      NS_ENSURE_SUCCESS(res, res);

      // check for easy case: both range endpoints in same text node
      nsCOMPtr<nsIDOMNode> startNode, endNode;
      res = range->GetStartContainer(getter_AddRefs(startNode));
      NS_ENSURE_SUCCESS(res, res);
      res = range->GetEndContainer(getter_AddRefs(endNode));
      NS_ENSURE_SUCCESS(res, res);
      if ((startNode == endNode) && IsTextNode(startNode))
      {
        // we're done with this range!
        if (useCSS && mHTMLCSSUtils->IsCSSEditableProperty(startNode, aProperty, aAttribute)) {
          // the HTML style defined by aProperty/aAttribute has a CSS equivalence
          // in this implementation for startNode
          nsAutoString cssValue;
          PRBool isSet = PR_FALSE;
          mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(startNode,
                                                    aProperty,
                                                    aAttribute,
                                                    isSet ,
                                                    cssValue,
                                                    COMPUTED_STYLE_TYPE);
          if (isSet) {
            // startNode's computed style indicates the CSS equivalence to the HTML style to
            // remove is applied; but we found no element in the ancestors of startNode
            // carrying specified styles; assume it comes from a rule and let's try to
            // insert a span "inverting" the style
            nsAutoString value; value.AssignLiteral("-moz-editor-invert-value");
            PRInt32 startOffset, endOffset;
            range->GetStartOffset(&startOffset);
            range->GetEndOffset(&endOffset);
            nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(startNode);
            if (mHTMLCSSUtils->IsCSSInvertable(aProperty, aAttribute)) {
              SetInlinePropertyOnTextNode(nodeAsText, startOffset, endOffset, aProperty, aAttribute, &value);
            }
          }
        }
      }
      else
      {
        // not the easy case.  range not contained in single text node. 
        nsCOMPtr<nsIContentIterator> iter =
          do_CreateInstance("@mozilla.org/content/subtree-content-iterator;1", &res);
        NS_ENSURE_SUCCESS(res, res);
        NS_ENSURE_TRUE(iter, NS_ERROR_FAILURE);

        nsCOMArray<nsIDOMNode> arrayOfNodes;
        nsCOMPtr<nsIDOMNode> node;
        
        // iterate range and build up array
        iter->Init(range);
        while (!iter->IsDone())
        {
          node = do_QueryInterface(iter->GetCurrentNode());
          NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

          if (IsEditable(node))
          { 
            arrayOfNodes.AppendObject(node);
          }

          iter->Next();
        }
        
        // loop through the list, remove the property on each node
        PRInt32 listCount = arrayOfNodes.Count();
        PRInt32 j;
        for (j = 0; j < listCount; j++)
        {
          node = arrayOfNodes[j];
          res = RemoveStyleInside(node, aProperty, aAttribute);
          NS_ENSURE_SUCCESS(res, res);
          if (useCSS && mHTMLCSSUtils->IsCSSEditableProperty(node, aProperty, aAttribute)) {
            // the HTML style defined by aProperty/aAttribute has a CSS equivalence
            // in this implementation for node
            nsAutoString cssValue;
            PRBool isSet = PR_FALSE;
            mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(node,
                                                               aProperty,
                                                               aAttribute,
                                                               isSet ,
                                                               cssValue,
                                                               COMPUTED_STYLE_TYPE);
            if (isSet) {
              // startNode's computed style indicates the CSS equivalence to the HTML style to
              // remove is applied; but we found no element in the ancestors of startNode
              // carrying specified styles; assume it comes from a rule and let's try to
              // insert a span "inverting" the style
              if (mHTMLCSSUtils->IsCSSInvertable(aProperty, aAttribute)) {
                nsAutoString value; value.AssignLiteral("-moz-editor-invert-value");
                SetInlinePropertyOnNode(node, aProperty, aAttribute, &value);
              }
            }
          }
        }
        arrayOfNodes.Clear();
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
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));  
  // Is the selection collapsed?
  PRBool bCollapsed;
  res = selection->GetIsCollapsed(&bCollapsed);
  NS_ENSURE_SUCCESS(res, res);
  
  // if it's collapsed set typing state
  if (bCollapsed)
  {
    nsCOMPtr<nsIAtom> atom;
    if (aSizeChange==1) atom = nsEditProperty::big;
    else                atom = nsEditProperty::small;

    // Let's see in what kind of element the selection is
    PRInt32 offset;
    nsCOMPtr<nsIDOMNode> selectedNode;
    res = GetStartNodeAndOffset(selection, getter_AddRefs(selectedNode), &offset);
    if (IsTextNode(selectedNode)) {
      nsCOMPtr<nsIDOMNode> parent;
      res = selectedNode->GetParentNode(getter_AddRefs(parent));
      NS_ENSURE_SUCCESS(res, res);
      selectedNode = parent;
    }
    nsAutoString tag;
    atom->ToString(tag);
    if (!CanContainTag(selectedNode, tag)) return NS_OK;

    // manipulating text attributes on a collapsed selection only sets state for the next text insertion
    return mTypeInState->SetProp(atom, EmptyString(), EmptyString());
  }
  
  // wrap with txn batching, rules sniffing, and selection preservation code
  nsAutoEditBatch batchIt(this);
  nsAutoRules beginRulesSniffing(this, kOpSetTextProperty, nsIEditor::eNext);
  nsAutoSelectionReset selectionResetter(selection, this);
  nsAutoTxnsConserveSelection dontSpazMySelection(this);

  // get selection range enumerator
  nsCOMPtr<nsIEnumerator> enumerator;
  res = selPriv->GetEnumerator(getter_AddRefs(enumerator));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(enumerator, NS_ERROR_FAILURE);

  // loop thru the ranges in the selection
  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  while ((NS_ENUMERATOR_FALSE == enumerator->IsDone()))
  {
    res = enumerator->CurrentItem(getter_AddRefs(currentItem));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(currentItem, NS_ERROR_FAILURE);
    
    nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );

    // adjust range to include any ancestors who's children are entirely selected
    res = PromoteInlineRange(range);
    NS_ENSURE_SUCCESS(res, res);
    
    // check for easy case: both range endpoints in same text node
    nsCOMPtr<nsIDOMNode> startNode, endNode;
    res = range->GetStartContainer(getter_AddRefs(startNode));
    NS_ENSURE_SUCCESS(res, res);
    res = range->GetEndContainer(getter_AddRefs(endNode));
    NS_ENSURE_SUCCESS(res, res);
    if ((startNode == endNode) && IsTextNode(startNode))
    {
      PRInt32 startOffset, endOffset;
      range->GetStartOffset(&startOffset);
      range->GetEndOffset(&endOffset);
      nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(startNode);
      res = RelativeFontChangeOnTextNode(aSizeChange, nodeAsText, startOffset, endOffset);
      NS_ENSURE_SUCCESS(res, res);
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

      nsCOMPtr<nsIContentIterator> iter =
        do_CreateInstance("@mozilla.org/content/subtree-content-iterator;1", &res);
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_TRUE(iter, NS_ERROR_FAILURE);

      nsCOMArray<nsIDOMNode> arrayOfNodes;
      nsCOMPtr<nsIDOMNode> node;
      
      // iterate range and build up array
      res = iter->Init(range);
      if (NS_SUCCEEDED(res))
      {
        while (!iter->IsDone())
        {
          node = do_QueryInterface(iter->GetCurrentNode());
          NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

          if (IsEditable(node))
          { 
            arrayOfNodes.AppendObject(node);
          }

          iter->Next();
        }
        
        // now that we have the list, do the font size change on each node
        PRInt32 listCount = arrayOfNodes.Count();
        PRInt32 j;
        for (j = 0; j < listCount; j++)
        {
          node = arrayOfNodes[j];
          res = RelativeFontChangeOnNode(aSizeChange, node);
          NS_ENSURE_SUCCESS(res, res);
        }
        arrayOfNodes.Clear();
      }
      // now check the start and end parents of the range to see if they need to 
      // be separately handled (they do if they are text nodes, due to how the
      // subtree iterator works - it will not have reported them).
      if (IsTextNode(startNode) && IsEditable(startNode))
      {
        nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(startNode);
        PRInt32 startOffset;
        PRUint32 textLen;
        range->GetStartOffset(&startOffset);
        nodeAsText->GetLength(&textLen);
        res = RelativeFontChangeOnTextNode(aSizeChange, nodeAsText, startOffset, textLen);
        NS_ENSURE_SUCCESS(res, res);
      }
      if (IsTextNode(endNode) && IsEditable(endNode))
      {
        nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(endNode);
        PRInt32 endOffset;
        range->GetEndOffset(&endOffset);
        res = RelativeFontChangeOnTextNode(aSizeChange, nodeAsText, 0, endOffset);
        NS_ENSURE_SUCCESS(res, res);
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
  NS_ENSURE_TRUE(aTextNode, NS_ERROR_NULL_POINTER);
  
  // don't need to do anything if no characters actually selected
  if (aStartOffset == aEndOffset) return NS_OK;
  
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> parent;
  res = aTextNode->GetParentNode(getter_AddRefs(parent));
  NS_ENSURE_SUCCESS(res, res);
  if (!CanContainTag(parent, NS_LITERAL_STRING("big"))) return NS_OK;

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
    NS_ENSURE_SUCCESS(res, res);
    node = tmp;  // remember left node
  }
  if ( aStartOffset )
  {
    // we need to split off front of text node
    res = SplitNode(node, aStartOffset, getter_AddRefs(tmp));
    NS_ENSURE_SUCCESS(res, res);
  }

  NS_NAMED_LITERAL_STRING(bigSize, "big");
  NS_NAMED_LITERAL_STRING(smallSize, "small");
  const nsAString& nodeType = (aSizeChange==1) ? static_cast<const nsAString&>(bigSize) : static_cast<const nsAString&>(smallSize);
  // look for siblings that are correct type of node
  nsCOMPtr<nsIDOMNode> sibling;
  GetPriorHTMLSibling(node, address_of(sibling));
  if (sibling && NodeIsType(sibling, (aSizeChange==1) ? nsEditProperty::big : nsEditProperty::small))
  {
    // previous sib is already right kind of inline node; slide this over into it
    res = MoveNode(node, sibling, -1);
    return res;
  }
  sibling = nsnull;
  GetNextHTMLSibling(node, address_of(sibling));
  if (sibling && NodeIsType(sibling, (aSizeChange==1) ? nsEditProperty::big : nsEditProperty::small))
  {
    // following sib is already right kind of inline node; slide this over into it
    res = MoveNode(node, sibling, 0);
    return res;
  }
  
  // else reparent the node inside font node with appropriate relative size
  res = InsertContainerAbove(node, address_of(tmp), nodeType);
  return res;
}


nsresult
nsHTMLEditor::RelativeFontChangeHelper( PRInt32 aSizeChange, 
                                        nsIDOMNode *aNode)
{
  /*  This routine looks for all the font nodes in the tree rooted by aNode,
      including aNode itself, looking for font nodes that have the size attr
      set.  Any such nodes need to have big or small put inside them, since
      they override any big/small that are above them.
  */
  
  // Can only change font size by + or - 1
  if ( !( (aSizeChange==1) || (aSizeChange==-1) ) )
    return NS_ERROR_ILLEGAL_VALUE;
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);

  nsresult res = NS_OK;
  nsAutoString tag;
  if (aSizeChange == 1) tag.AssignLiteral("big");
  else tag.AssignLiteral("small");
  nsCOMPtr<nsIDOMNodeList> childNodes;
  PRInt32 j;
  PRUint32 childCount;
  nsCOMPtr<nsIDOMNode> childNode;
  
  // if this is a font node with size, put big/small inside it
  NS_NAMED_LITERAL_STRING(attr, "size");
  if (NodeIsType(aNode, nsEditProperty::font) && HasAttr(aNode, &attr))
  {
    // cycle through children and adjust relative font size
    res = aNode->GetChildNodes(getter_AddRefs(childNodes));
    NS_ENSURE_SUCCESS(res, res);
    if (childNodes)
    {
      childNodes->GetLength(&childCount);
      for (j=childCount-1; j>=0; j--)
      {
        res = childNodes->Item(j, getter_AddRefs(childNode));
        if ((NS_SUCCEEDED(res)) && (childNode))
        {
          res = RelativeFontChangeOnNode(aSizeChange, childNode);
          NS_ENSURE_SUCCESS(res, res);
        }
      }
    }
  }

  childNodes = nsnull;
  // now cycle through the children.
  res = aNode->GetChildNodes(getter_AddRefs(childNodes));
  NS_ENSURE_SUCCESS(res, res);
  if (childNodes)
  {
    childNodes->GetLength(&childCount);
    for (j=childCount-1; j>=0; j--)
    {
      res = childNodes->Item(j, getter_AddRefs(childNode));
      if ((NS_SUCCEEDED(res)) && (childNode))
      {
        res = RelativeFontChangeHelper(aSizeChange, childNode);
        NS_ENSURE_SUCCESS(res, res);
      }
    }
  }

  return res;
}


nsresult
nsHTMLEditor::RelativeFontChangeOnNode( PRInt32 aSizeChange, 
                                        nsIDOMNode *aNode)
{
  // Can only change font size by + or - 1
  if ( !( (aSizeChange==1) || (aSizeChange==-1) ) )
    return NS_ERROR_ILLEGAL_VALUE;
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);

  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> tmp;
  nsAutoString tag;
  if (aSizeChange == 1) tag.AssignLiteral("big");
  else tag.AssignLiteral("small");
  
  // is it the opposite of what we want?  
  if ( ((aSizeChange == 1) && nsHTMLEditUtils::IsSmall(aNode)) || 
       ((aSizeChange == -1) &&  nsHTMLEditUtils::IsBig(aNode)) )
  {
    // first populate any nested font tags that have the size attr set
    res = RelativeFontChangeHelper(aSizeChange, aNode);
    NS_ENSURE_SUCCESS(res, res);
    // in that case, just remove this node and pull up the children
    res = RemoveContainer(aNode);
    return res;
  }
  // can it be put inside a "big" or "small"?
  if (TagCanContain(tag, aNode))
  {
    // first populate any nested font tags that have the size attr set
    res = RelativeFontChangeHelper(aSizeChange, aNode);
    NS_ENSURE_SUCCESS(res, res);
    // ok, chuck it in.
    // first look at siblings of aNode for matching bigs or smalls.
    // if we find one, move aNode into it.
    nsCOMPtr<nsIDOMNode> sibling;
    GetPriorHTMLSibling(aNode, address_of(sibling));
    if (sibling && nsEditor::NodeIsType(sibling, (aSizeChange==1 ? nsEditProperty::big : nsEditProperty::small)))
    {
      // previous sib is already right kind of inline node; slide this over into it
      res = MoveNode(aNode, sibling, -1);
      return res;
    }
    sibling = nsnull;
    GetNextHTMLSibling(aNode, address_of(sibling));
    if (sibling && nsEditor::NodeIsType(sibling, (aSizeChange==1 ? nsEditProperty::big : nsEditProperty::small)))
    {
      // following sib is already right kind of inline node; slide this over into it
      res = MoveNode(aNode, sibling, 0);
      return res;
    }
    // else insert it above aNode
    res = InsertContainerAbove(aNode, address_of(tmp), tag);
    return res;
  }
  // none of the above?  then cycle through the children.
  // MOOSE: we should group the children together if possible
  // into a single "big" or "small".  For the moment they are
  // each getting their own.  
  nsCOMPtr<nsIDOMNodeList> childNodes;
  res = aNode->GetChildNodes(getter_AddRefs(childNodes));
  NS_ENSURE_SUCCESS(res, res);
  if (childNodes)
  {
    PRInt32 j;
    PRUint32 childCount;
    childNodes->GetLength(&childCount);
    for (j=childCount-1; j>=0; j--)
    {
      nsCOMPtr<nsIDOMNode> childNode;
      res = childNodes->Item(j, getter_AddRefs(childNode));
      if ((NS_SUCCEEDED(res)) && (childNode))
      {
        res = RelativeFontChangeOnNode(aSizeChange, childNode);
        NS_ENSURE_SUCCESS(res, res);
      }
    }
  }
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::GetFontFaceState(PRBool *aMixed, nsAString &outFace)
{
  NS_ENSURE_TRUE(aMixed, NS_ERROR_FAILURE);
  *aMixed = PR_TRUE;
  outFace.Truncate();

  nsresult res;
  PRBool first, any, all;
  
  NS_NAMED_LITERAL_STRING(attr, "face");
  res = GetInlinePropertyBase(nsEditProperty::font, &attr, nsnull, &first, &any, &all, &outFace);
  NS_ENSURE_SUCCESS(res, res);
  if (any && !all) return res; // mixed
  if (all)
  {
    *aMixed = PR_FALSE;
    return res;
  }
  
  // if there is no font face, check for tt
  res = GetInlinePropertyBase(nsEditProperty::tt, nsnull, nsnull, &first, &any, &all,nsnull);
  NS_ENSURE_SUCCESS(res, res);
  if (any && !all) return res; // mixed
  if (all)
  {
    *aMixed = PR_FALSE;
    nsEditProperty::tt->ToString(outFace);
  }
  
  if (!any)
  {
    // there was no font face attrs of any kind.  We are in normal font.
    outFace.Truncate();
    *aMixed = PR_FALSE;
  }
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::GetFontColorState(PRBool *aMixed, nsAString &aOutColor)
{
  NS_ENSURE_TRUE(aMixed, NS_ERROR_NULL_POINTER);
  *aMixed = PR_TRUE;
  aOutColor.Truncate();
  
  nsresult res;
  NS_NAMED_LITERAL_STRING(colorStr, "color");
  PRBool first, any, all;
  
  res = GetInlinePropertyBase(nsEditProperty::font, &colorStr, nsnull, &first, &any, &all, &aOutColor);
  NS_ENSURE_SUCCESS(res, res);
  if (any && !all) return res; // mixed
  if (all)
  {
    *aMixed = PR_FALSE;
    return res;
  }
  
  if (!any)
  {
    // there was no font color attrs of any kind..
    aOutColor.Truncate();
    *aMixed = PR_FALSE;
  }
  return res;
}

// the return value is true only if the instance of the HTML editor we created
// can handle CSS styles (for instance, Composer can, Messenger can't) and if
// the CSS preference is checked
nsresult
nsHTMLEditor::GetIsCSSEnabled(PRBool *aIsCSSEnabled)
{
  *aIsCSSEnabled = PR_FALSE;
  if (mCSSAware) {
    // TBD later : removal of mCSSAware and use only the presence of mHTMLCSSUtils
    if (mHTMLCSSUtils) {
      *aIsCSSEnabled = mHTMLCSSUtils->IsCSSPrefChecked();
    }
  }
  return NS_OK;
}

nsresult
nsHTMLEditor::HasStyleOrIdOrClass(nsIDOMElement * aElement, PRBool *aHasStyleOrIdOrClass)
{
  NS_ENSURE_TRUE(aElement, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIDOMNode> node  = do_QueryInterface(aElement);


  // remove the node if its style attribute is empty or absent,
  // and if it does not have a class nor an id
  nsAutoString styleVal;
  PRBool isStyleSet;
  *aHasStyleOrIdOrClass = PR_TRUE;
  nsresult res = GetAttributeValue(aElement,  NS_LITERAL_STRING("style"), styleVal, &isStyleSet);
  NS_ENSURE_SUCCESS(res, res);
  if (!isStyleSet || styleVal.IsEmpty()) {
    res = mHTMLCSSUtils->HasClassOrID(aElement, *aHasStyleOrIdOrClass);
    NS_ENSURE_SUCCESS(res, res);
  }
  return res;
}

nsresult
nsHTMLEditor::RemoveElementIfNoStyleOrIdOrClass(nsIDOMElement * aElement, nsIAtom * aTag)
{
  NS_ENSURE_TRUE(aElement, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIDOMNode> node  = do_QueryInterface(aElement);

  // early way out if node is not the right kind of element
  if (!NodeIsType(node, aTag)) {
    return NS_OK;
  }
  PRBool hasStyleOrIdOrClass;
  nsresult res = HasStyleOrIdOrClass(aElement, &hasStyleOrIdOrClass);
  if (!hasStyleOrIdOrClass) {
    res = RemoveContainer(node);
  }
  return res;
}
