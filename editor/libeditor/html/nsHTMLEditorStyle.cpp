/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
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

#include "mozilla/dom/Element.h"

using namespace mozilla;

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


NS_IMETHODIMP
nsHTMLEditor::SetInlineProperty(nsIAtom *aProperty,
                                const nsAString& aAttribute,
                                const nsAString& aValue)
{
  if (!aProperty) {
    return NS_ERROR_NULL_POINTER;
  }
  if (!mRules) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  ForceCompositionEnd();

  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  if (selection->Collapsed()) {
    // manipulating text attributes on a collapsed selection only sets state
    // for the next text insertion
    mTypeInState->SetProp(aProperty, aAttribute, aValue);
    return NS_OK;
  }

  nsAutoEditBatch batchIt(this);
  nsAutoRules beginRulesSniffing(this, kOpInsertElement, nsIEditor::eNext);
  nsAutoSelectionReset selectionResetter(selection, this);
  nsAutoTxnsConserveSelection dontSpazMySelection(this);

  bool cancel, handled;
  nsTextRulesInfo ruleInfo(kOpSetTextProperty);
  nsresult res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  NS_ENSURE_SUCCESS(res, res);
  if (!cancel && !handled) {
    // get selection range enumerator
    nsCOMPtr<nsIEnumerator> enumerator;
    res = selection->GetEnumerator(getter_AddRefs(enumerator));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(enumerator, NS_ERROR_FAILURE);

    // loop thru the ranges in the selection
    nsCOMPtr<nsISupports> currentItem;
    for (enumerator->First(); NS_ENUMERATOR_FALSE == enumerator->IsDone();
         enumerator->Next()) {
      res = enumerator->CurrentItem(getter_AddRefs(currentItem));
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_TRUE(currentItem, NS_ERROR_FAILURE);

      nsCOMPtr<nsIDOMRange> range(do_QueryInterface(currentItem));

      // adjust range to include any ancestors whose children are entirely
      // selected
      res = PromoteInlineRange(range);
      NS_ENSURE_SUCCESS(res, res);

      // check for easy case: both range endpoints in same text node
      nsCOMPtr<nsIDOMNode> startNode, endNode;
      res = range->GetStartContainer(getter_AddRefs(startNode));
      NS_ENSURE_SUCCESS(res, res);
      res = range->GetEndContainer(getter_AddRefs(endNode));
      NS_ENSURE_SUCCESS(res, res);
      if (startNode == endNode && IsTextNode(startNode)) {
        PRInt32 startOffset, endOffset;
        range->GetStartOffset(&startOffset);
        range->GetEndOffset(&endOffset);
        nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(startNode);
        res = SetInlinePropertyOnTextNode(nodeAsText, startOffset, endOffset,
                                          aProperty, &aAttribute, &aValue);
        NS_ENSURE_SUCCESS(res, res);
        continue;
      }

      // Not the easy case.  Range not contained in single text node.  There
      // are up to three phases here.  There are all the nodes reported by the
      // subtree iterator to be processed.  And there are potentially a
      // starting textnode and an ending textnode which are only partially
      // contained by the range.

      // Let's handle the nodes reported by the iterator.  These nodes are
      // entirely contained in the selection range.  We build up a list of them
      // (since doing operations on the document during iteration would perturb
      // the iterator).

      nsCOMPtr<nsIContentIterator> iter =
        do_CreateInstance("@mozilla.org/content/subtree-content-iterator;1", &res);
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_TRUE(iter, NS_ERROR_FAILURE);

      nsCOMArray<nsIDOMNode> arrayOfNodes;

      // iterate range and build up array
      res = iter->Init(range);
      // Init returns an error if there are no nodes in range.  This can easily
      // happen with the subtree iterator if the selection doesn't contain any
      // *whole* nodes.
      if (NS_SUCCEEDED(res)) {
        nsCOMPtr<nsIDOMNode> node;
        for (; !iter->IsDone(); iter->Next()) {
          node = do_QueryInterface(iter->GetCurrentNode());
          NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

          if (IsEditable(node)) {
            arrayOfNodes.AppendObject(node);
          }
        }
      }
      // first check the start parent of the range to see if it needs to
      // be separately handled (it does if it's a text node, due to how the
      // subtree iterator works - it will not have reported it).
      if (IsTextNode(startNode) && IsEditable(startNode)) {
        nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(startNode);
        PRInt32 startOffset;
        PRUint32 textLen;
        range->GetStartOffset(&startOffset);
        nodeAsText->GetLength(&textLen);
        res = SetInlinePropertyOnTextNode(nodeAsText, startOffset, textLen,
                                          aProperty, &aAttribute, &aValue);
        NS_ENSURE_SUCCESS(res, res);
      }

      // then loop through the list, set the property on each node
      PRInt32 listCount = arrayOfNodes.Count();
      PRInt32 j;
      for (j = 0; j < listCount; j++) {
        res = SetInlinePropertyOnNode(arrayOfNodes[j], aProperty,
                                      &aAttribute, &aValue);
        NS_ENSURE_SUCCESS(res, res);
      }

      // last check the end parent of the range to see if it needs to
      // be separately handled (it does if it's a text node, due to how the
      // subtree iterator works - it will not have reported it).
      if (IsTextNode(endNode) && IsEditable(endNode)) {
        nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(endNode);
        PRInt32 endOffset;
        range->GetEndOffset(&endOffset);
        res = SetInlinePropertyOnTextNode(nodeAsText, 0, endOffset,
                                          aProperty, &aAttribute, &aValue);
        NS_ENSURE_SUCCESS(res, res);
      }
    }
  }
  if (!cancel) {
    // post-process
    return mRules->DidDoAction(selection, &ruleInfo, res);
  }
  return NS_OK;
}



// Helper function for SetInlinePropertyOn*: is aNode a simple old <b>, <font>,
// <span style="">, etc. that we can reuse instead of creating a new one?
bool
nsHTMLEditor::IsSimpleModifiableNode(nsIContent* aContent,
                                     nsIAtom* aProperty,
                                     const nsAString* aAttribute,
                                     const nsAString* aValue)
{
  // aContent can be null, in which case we'll return false in a few lines
  MOZ_ASSERT(aProperty);
  MOZ_ASSERT_IF(aAttribute, aValue);

  nsCOMPtr<dom::Element> element = do_QueryInterface(aContent);
  if (!element) {
    return false;
  }

  // First check for <b>, <i>, etc.
  if (element->IsHTML(aProperty) && !element->GetAttrCount() &&
      (!aAttribute || aAttribute->IsEmpty())) {
    return true;
  }

  // Special cases for various equivalencies: <strong>, <em>, <s>
  if (!element->GetAttrCount() &&
      ((aProperty == nsGkAtoms::b && element->IsHTML(nsGkAtoms::strong)) ||
       (aProperty == nsGkAtoms::i && element->IsHTML(nsGkAtoms::em)) ||
       (aProperty == nsGkAtoms::strike && element->IsHTML(nsGkAtoms::s)))) {
    return true;
  }

  // Now look for things like <font>
  if (aAttribute && !aAttribute->IsEmpty()) {
    nsCOMPtr<nsIAtom> atom = do_GetAtom(*aAttribute);
    MOZ_ASSERT(atom);

    if (element->IsHTML(aProperty) && IsOnlyAttribute(element, *aAttribute) &&
        element->AttrValueIs(kNameSpaceID_None, atom, *aValue, eIgnoreCase)) {
      // This is not quite correct, because it excludes cases like
      // <font face=000> being the same as <font face=#000000>.
      // Property-specific handling is needed (bug 760211).
      return true;
    }
  }

  // No luck so far.  Now we check for a <span> with a single style=""
  // attribute that sets only the style we're looking for, if this type of
  // style supports it
  if (!mHTMLCSSUtils->IsCSSEditableProperty(element, aProperty,
                                            aAttribute, aValue) ||
      !element->IsHTML(nsGkAtoms::span) || element->GetAttrCount() != 1 ||
      !element->HasAttr(kNameSpaceID_None, nsGkAtoms::style)) {
    return false;
  }

  // Some CSS styles are not so simple.  For instance, underline is
  // "text-decoration: underline", which decomposes into four different text-*
  // properties.  So for now, we just create a span, add the desired style, and
  // see if it matches.
  nsCOMPtr<dom::Element> newSpan;
  nsresult res = CreateHTMLContent(NS_LITERAL_STRING("span"),
                                   getter_AddRefs(newSpan));
  NS_ASSERTION(NS_SUCCEEDED(res), "CreateHTMLContent failed");
  NS_ENSURE_SUCCESS(res, false);
  mHTMLCSSUtils->SetCSSEquivalentToHTMLStyle(newSpan, aProperty,
                                             aAttribute, aValue,
                                             /*suppress transaction*/ true);

  return mHTMLCSSUtils->ElementsSameStyle(newSpan, element);
}


nsresult
nsHTMLEditor::SetInlinePropertyOnTextNode( nsIDOMCharacterData *aTextNode, 
                                            PRInt32 aStartOffset,
                                            PRInt32 aEndOffset,
                                            nsIAtom *aProperty, 
                                            const nsAString *aAttribute,
                                            const nsAString *aValue)
{
  MOZ_ASSERT(aValue);
  NS_ENSURE_TRUE(aTextNode, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIDOMNode> parent;
  nsresult res = aTextNode->GetParentNode(getter_AddRefs(parent));
  NS_ENSURE_SUCCESS(res, res);

  if (!CanContainTag(parent, aProperty)) {
    return NS_OK;
  }
  
  // don't need to do anything if no characters actually selected
  if (aStartOffset == aEndOffset) return NS_OK;
  
  nsCOMPtr<nsIDOMNode> node = aTextNode;
  
  // don't need to do anything if property already set on node
  bool bHasProp;
  if (mHTMLCSSUtils->IsCSSEditableProperty(node, aProperty,
                                           aAttribute, aValue)) {
    // the HTML styles defined by aProperty/aAttribute has a CSS equivalence
    // in this implementation for node; let's check if it carries those css styles
    nsAutoString value(*aValue);
    mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(node, aProperty, aAttribute,
                                                       bHasProp, value,
                                                       COMPUTED_STYLE_TYPE);
  } else {
    IsTextPropertySetByContent(node, aProperty, aAttribute, aValue, bHasProp);
  }

  if (bHasProp) return NS_OK;
  
  // do we need to split the text node?
  PRUint32 textLen;
  aTextNode->GetLength(&textLen);

  if (PRUint32(aEndOffset) != textLen) {
    // we need to split off back of text node
    nsCOMPtr<nsIDOMNode> tmp;
    res = SplitNode(node, aEndOffset, getter_AddRefs(tmp));
    NS_ENSURE_SUCCESS(res, res);
    node = tmp;  // remember left node
  }

  if (aStartOffset) {
    // we need to split off front of text node
    nsCOMPtr<nsIDOMNode> tmp;
    res = SplitNode(node, aStartOffset, getter_AddRefs(tmp));
    NS_ENSURE_SUCCESS(res, res);
  }

  nsCOMPtr<nsIContent> content = do_QueryInterface(node);
  NS_ENSURE_STATE(content);

  if (aAttribute) {
    // look for siblings that are correct type of node
    nsIContent* sibling = GetPriorHTMLSibling(content);
    if (IsSimpleModifiableNode(sibling, aProperty, aAttribute, aValue)) {
      // previous sib is already right kind of inline node; slide this over into it
      return MoveNode(node, sibling->AsDOMNode(), -1);
    }
    sibling = GetNextHTMLSibling(content);
    if (IsSimpleModifiableNode(sibling, aProperty, aAttribute, aValue)) {
      // following sib is already right kind of inline node; slide this over into it
      return MoveNode(node, sibling->AsDOMNode(), 0);
    }
  }
  
  // reparent the node inside inline node with appropriate {attribute,value}
  return SetInlinePropertyOnNode(node, aProperty, aAttribute, aValue);
}


nsresult
nsHTMLEditor::SetInlinePropertyOnNodeImpl(nsIContent* aNode,
                                          nsIAtom* aProperty,
                                          const nsAString* aAttribute,
                                          const nsAString* aValue)
{
  MOZ_ASSERT(aNode && aProperty);
  MOZ_ASSERT(aValue);

  // If this is an element that can't be contained in a span, we have to
  // recurse to its children.
  if (!TagCanContain(nsGkAtoms::span, aNode->AsDOMNode())) {
    if (aNode->HasChildren()) {
      nsCOMArray<nsIContent> arrayOfNodes;

      // Populate the list.
      for (nsIContent* child = aNode->GetFirstChild();
           child;
           child = child->GetNextSibling()) {
        if (IsEditable(child)) {
          arrayOfNodes.AppendObject(child);
        }
      }

      // Then loop through the list, set the property on each node.
      PRInt32 listCount = arrayOfNodes.Count();
      for (PRInt32 j = 0; j < listCount; ++j) {
        nsresult rv = SetInlinePropertyOnNode(arrayOfNodes[j], aProperty,
                                              aAttribute, aValue);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
    return NS_OK;
  }

  // First check if there's an adjacent sibling we can put our node into.
  nsresult res;
  nsCOMPtr<nsIContent> previousSibling = GetPriorHTMLSibling(aNode);
  nsCOMPtr<nsIContent> nextSibling = GetNextHTMLSibling(aNode);
  if (IsSimpleModifiableNode(previousSibling, aProperty, aAttribute, aValue)) {
    res = MoveNode(aNode, previousSibling, -1);
    NS_ENSURE_SUCCESS(res, res);
    if (IsSimpleModifiableNode(nextSibling, aProperty, aAttribute, aValue)) {
      res = JoinNodes(previousSibling, nextSibling);
      NS_ENSURE_SUCCESS(res, res);
    }
    return NS_OK;
  }
  if (IsSimpleModifiableNode(nextSibling, aProperty, aAttribute, aValue)) {
    res = MoveNode(aNode, nextSibling, 0);
    NS_ENSURE_SUCCESS(res, res);
    return NS_OK;
  }

  // don't need to do anything if property already set on node
  if (mHTMLCSSUtils->IsCSSEditableProperty(aNode, aProperty,
                                           aAttribute, aValue)) {
    if (mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(
          aNode, aProperty, aAttribute, *aValue, COMPUTED_STYLE_TYPE)) {
      return NS_OK;
    }
  } else if (IsTextPropertySetByContent(aNode, aProperty,
                                        aAttribute, aValue)) {
    return NS_OK;
  }

  bool useCSS = (IsCSSEnabled() &&
                 mHTMLCSSUtils->IsCSSEditableProperty(aNode, aProperty,
                                                      aAttribute, aValue)) ||
                // bgcolor is always done using CSS
                aAttribute->EqualsLiteral("bgcolor");

  if (useCSS) {
    nsCOMPtr<dom::Element> tmp;
    // We only add style="" to <span>s with no attributes (bug 746515).  If we
    // don't have one, we need to make one.
    if (aNode->IsElement() && aNode->AsElement()->IsHTML(nsGkAtoms::span) &&
        !aNode->AsElement()->GetAttrCount()) {
      tmp = aNode->AsElement();
    } else {
      res = InsertContainerAbove(aNode, getter_AddRefs(tmp),
                                 NS_LITERAL_STRING("span"),
                                 nsnull, nsnull);
      NS_ENSURE_SUCCESS(res, res);
    }

    // Add the CSS styles corresponding to the HTML style request
    PRInt32 count;
    res = mHTMLCSSUtils->SetCSSEquivalentToHTMLStyle(tmp->AsDOMNode(),
                                                     aProperty, aAttribute,
                                                     aValue, &count, false);
    NS_ENSURE_SUCCESS(res, res);
    return NS_OK;
  }

  // is it already the right kind of node, but with wrong attribute?
  if (aNode->Tag() == aProperty) {
    // Just set the attribute on it.
    nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(aNode);
    return SetAttribute(elem, *aAttribute, *aValue);
  }

  // ok, chuck it in its very own container
  nsAutoString tag;
  aProperty->ToString(tag);
  ToLowerCase(tag);
  nsCOMPtr<nsIDOMNode> tmp;
  return InsertContainerAbove(aNode->AsDOMNode(), address_of(tmp), tag,
                              aAttribute, aValue);
}


nsresult
nsHTMLEditor::SetInlinePropertyOnNode(nsIDOMNode *aNode,
                                      nsIAtom *aProperty,
                                      const nsAString *aAttribute,
                                      const nsAString *aValue)
{
  // Before setting the property, we remove it if it's already set.
  // RemoveStyleInside might remove the node we're looking at or some of its
  // descendants, however, in which case we want to set the property on
  // whatever wound up in its place.  We have to save the original siblings and
  // parent to figure this out.
  NS_ENSURE_TRUE(aNode && aProperty, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIContent> node = do_QueryInterface(aNode);
  NS_ENSURE_STATE(node);

  return SetInlinePropertyOnNode(node, aProperty, aAttribute, aValue);
}

nsresult
nsHTMLEditor::SetInlinePropertyOnNode(nsIContent* aNode,
                                      nsIAtom* aProperty,
                                      const nsAString* aAttribute,
                                      const nsAString* aValue)
{
  MOZ_ASSERT(aNode);
  MOZ_ASSERT(aProperty);

  nsCOMPtr<nsIContent> previousSibling = aNode->GetPreviousSibling(),
                       nextSibling = aNode->GetNextSibling();
  nsCOMPtr<nsINode> parent = aNode->GetNodeParent();
  NS_ENSURE_STATE(parent);

  nsresult res = RemoveStyleInside(aNode->AsDOMNode(), aProperty, aAttribute);
  NS_ENSURE_SUCCESS(res, res);

  if (aNode->GetNodeParent()) {
    // The node is still where it was
    return SetInlinePropertyOnNodeImpl(aNode, aProperty,
                                       aAttribute, aValue);
  }

  // It's vanished.  Use the old siblings for reference to construct a
  // list.  But first, verify that the previous/next siblings are still
  // where we expect them; otherwise we have to give up.
  if ((previousSibling && previousSibling->GetNodeParent() != parent) ||
      (nextSibling && nextSibling->GetNodeParent() != parent)) {
    return NS_ERROR_UNEXPECTED;
  }
  nsCOMArray<nsIContent> nodesToSet;
  nsCOMPtr<nsIContent> cur = previousSibling
    ? previousSibling->GetNextSibling() : parent->GetFirstChild();
  while (cur && cur != nextSibling) {
    if (IsEditable(cur)) {
      nodesToSet.AppendObject(cur);
    }
    cur = cur->GetNextSibling();
  }

  PRInt32 nodesToSetCount = nodesToSet.Count();
  for (PRInt32 k = 0; k < nodesToSetCount; k++) {
    res = SetInlinePropertyOnNodeImpl(nodesToSet[k], aProperty,
                                      aAttribute, aValue);
    NS_ENSURE_SUCCESS(res, res);
  }

  return NS_OK;
}


nsresult nsHTMLEditor::SplitStyleAboveRange(nsIDOMRange *inRange, 
                                            nsIAtom *aProperty, 
                                            const nsAString *aAttribute)
{
  NS_ENSURE_TRUE(inRange, NS_ERROR_NULL_POINTER);
  nsresult res;
  nsCOMPtr<nsIDOMNode> startNode, endNode, origStartNode;
  PRInt32 startOffset, endOffset;

  res = inRange->GetStartContainer(getter_AddRefs(startNode));
  NS_ENSURE_SUCCESS(res, res);
  res = inRange->GetStartOffset(&startOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = inRange->GetEndContainer(getter_AddRefs(endNode));
  NS_ENSURE_SUCCESS(res, res);
  res = inRange->GetEndOffset(&endOffset);
  NS_ENSURE_SUCCESS(res, res);

  origStartNode = startNode;

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

  bool useCSS = IsCSSEnabled();

  bool isSet;
  while (tmp && !IsBlockNode(tmp))
  {
    isSet = false;
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
      nsresult rv = SplitNodeDeep(tmp, *aNode, *aOffset, &offset, false,
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

nsresult
nsHTMLEditor::ClearStyle(nsCOMPtr<nsIDOMNode>* aNode, PRInt32* aOffset,
                         nsIAtom* aProperty, const nsAString* aAttribute)
{
  nsCOMPtr<nsIDOMNode> leftNode, rightNode, tmp;
  nsresult res = SplitStyleAbovePoint(aNode, aOffset, aProperty, aAttribute,
                                      address_of(leftNode),
                                      address_of(rightNode));
  NS_ENSURE_SUCCESS(res, res);
  if (leftNode) {
    bool bIsEmptyNode;
    IsEmptyNode(leftNode, &bIsEmptyNode, false, true);
    if (bIsEmptyNode) {
      // delete leftNode if it became empty
      res = DeleteNode(leftNode);
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  if (rightNode) {
    nsCOMPtr<nsIDOMNode> secondSplitParent = GetLeftmostChild(rightNode);
    // don't try to split non-containers (br's, images, hr's, etc)
    if (!secondSplitParent) {
      secondSplitParent = rightNode;
    }
    nsCOMPtr<nsIDOMNode> savedBR;
    if (!IsContainer(secondSplitParent)) {
      if (nsTextEditUtils::IsBreak(secondSplitParent)) {
        savedBR = secondSplitParent;
      }

      secondSplitParent->GetParentNode(getter_AddRefs(tmp));
      secondSplitParent = tmp;
    }
    *aOffset = 0;
    res = SplitStyleAbovePoint(address_of(secondSplitParent),
                               aOffset, aProperty, aAttribute,
                               address_of(leftNode), address_of(rightNode));
    NS_ENSURE_SUCCESS(res, res);
    // should be impossible to not get a new leftnode here
    NS_ENSURE_TRUE(leftNode, NS_ERROR_FAILURE);
    nsCOMPtr<nsIDOMNode> newSelParent = GetLeftmostChild(leftNode);
    if (!newSelParent) {
      newSelParent = leftNode;
    }
    // If rightNode starts with a br, suck it out of right node and into
    // leftNode.  This is so we you don't revert back to the previous style
    // if you happen to click at the end of a line.
    if (savedBR) {
      res = MoveNode(savedBR, newSelParent, 0);
      NS_ENSURE_SUCCESS(res, res);
    }
    bool bIsEmptyNode;
    IsEmptyNode(rightNode, &bIsEmptyNode, false, true);
    if (bIsEmptyNode) {
      // delete rightNode if it became empty
      res = DeleteNode(rightNode);
      NS_ENSURE_SUCCESS(res, res);
    }
    // remove the style on this new hierarchy
    PRInt32 newSelOffset = 0;
    {
      // Track the point at the new hierarchy.  This is so we can know where
      // to put the selection after we call RemoveStyleInside().
      // RemoveStyleInside() could remove any and all of those nodes, so I
      // have to use the range tracking system to find the right spot to put
      // selection.
      nsAutoTrackDOMPoint tracker(mRangeUpdater,
                                  address_of(newSelParent), &newSelOffset);
      res = RemoveStyleInside(leftNode, aProperty, aAttribute);
      NS_ENSURE_SUCCESS(res, res);
    }
    // reset our node offset values to the resulting new sel point
    *aNode = newSelParent;
    *aOffset = newSelOffset;
  }

  return NS_OK;
}

bool nsHTMLEditor::NodeIsProperty(nsIDOMNode *aNode)
{
  NS_ENSURE_TRUE(aNode, false);
  if (!IsContainer(aNode))  return false;
  if (!IsEditable(aNode))   return false;
  if (IsBlockNode(aNode))   return false;
  if (NodeIsType(aNode, nsEditProperty::a)) return false;
  return true;
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
                                         // null here means remove all properties
                                         nsIAtom *aProperty,
                                         const nsAString *aAttribute,
                                         const bool aChildrenOnly)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);
  if (IsTextNode(aNode)) {
    return NS_OK;
  }
  nsresult res;

  // first process the children
  nsCOMPtr<nsIDOMNode> child, tmp;
  aNode->GetFirstChild(getter_AddRefs(child));
  while (child) {
    // cache next sibling since we might remove child
    child->GetNextSibling(getter_AddRefs(tmp));
    res = RemoveStyleInside(child, aProperty, aAttribute);
    NS_ENSURE_SUCCESS(res, res);
    child = tmp;
  }

  // then process the node itself
  if (
    (!aChildrenOnly &&
      (
        // node is prop we asked for
        (aProperty && NodeIsType(aNode, aProperty)) ||
        // but check for link (<a href=...)
        (aProperty == nsEditProperty::href && nsHTMLEditUtils::IsLink(aNode)) ||
        // and for named anchors
        (aProperty == nsEditProperty::name && nsHTMLEditUtils::IsNamedAnchor(aNode))
      )
    ) ||
    // or node is any prop and we asked for that
    (!aProperty && NodeIsProperty(aNode))
  ) {
    // if we weren't passed an attribute, then we want to 
    // remove any matching inlinestyles entirely
    if (!aAttribute || aAttribute->IsEmpty()) {
      NS_NAMED_LITERAL_STRING(styleAttr, "style");
      NS_NAMED_LITERAL_STRING(classAttr, "class");
      bool hasStyleAttr = HasAttr(aNode, &styleAttr);
      bool hasClassAttr = HasAttr(aNode, &classAttr);
      if (aProperty && (hasStyleAttr || hasClassAttr)) {
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
      }
      res = RemoveContainer(aNode);
      NS_ENSURE_SUCCESS(res, res);
    } else {
      // otherwise we just want to eliminate the attribute
      if (HasAttr(aNode, aAttribute)) {
        // if this matching attribute is the ONLY one on the node,
        // then remove the whole node.  Otherwise just nix the attribute.
        if (IsOnlyAttribute(aNode, aAttribute)) {
          res = RemoveContainer(aNode);
        } else {
          nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(aNode);
          NS_ENSURE_TRUE(elem, NS_ERROR_NULL_POINTER);
          res = RemoveAttribute(elem, *aAttribute);
        }
        NS_ENSURE_SUCCESS(res, res);
      }
    }
  }

  if (!aChildrenOnly &&
      mHTMLCSSUtils->IsCSSEditableProperty(aNode, aProperty, aAttribute)) {
    // the HTML style defined by aProperty/aAttribute has a CSS equivalence in
    // this implementation for the node aNode; let's check if it carries those
    // css styles
    nsAutoString propertyValue;
    bool isSet;
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
                                                    false);
      // remove the node if it is a span or font, if its style attribute is
      // empty or absent, and if it does not have a class nor an id
      RemoveElementIfNoStyleOrIdOrClass(aNode);
    }
  }

  if (aProperty == nsEditProperty::font &&    // or node is big or small and we are setting font size
      (nsHTMLEditUtils::IsBig(aNode) || nsHTMLEditUtils::IsSmall(aNode)) &&
      aAttribute && aAttribute->LowerCaseEqualsLiteral("size")) {
    return RemoveContainer(aNode);  // if we are setting font size, remove any nested bigs and smalls
  }
  return NS_OK;
}

bool nsHTMLEditor::IsOnlyAttribute(nsIDOMNode *aNode, 
                                     const nsAString *aAttribute)
{
  NS_ENSURE_TRUE(aNode && aAttribute, false);  // ooops

  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(content, false);  // ooops

  return IsOnlyAttribute(content, *aAttribute);
}

bool
nsHTMLEditor::IsOnlyAttribute(const nsIContent* aContent,
                              const nsAString& aAttribute)
{
  MOZ_ASSERT(aContent);

  PRUint32 attrCount = aContent->GetAttrCount();
  for (PRUint32 i = 0; i < attrCount; ++i) {
    const nsAttrName* name = aContent->GetAttrNameAt(i);
    if (!name->NamespaceEquals(kNameSpaceID_None)) {
      return false;
    }

    nsAutoString attrString;
    name->LocalName()->ToString(attrString);
    // if it's the attribute we know about, or a special _moz attribute,
    // keep looking
    if (!attrString.Equals(aAttribute, nsCaseInsensitiveStringComparator()) &&
        !StringBeginsWith(attrString, NS_LITERAL_STRING("_moz"))) {
      return false;
    }
  }
  // if we made it through all of them without finding a real attribute
  // other than aAttribute, then return true
  return true;
}

bool nsHTMLEditor::HasAttr(nsIDOMNode* aNode,
                           const nsAString* aAttribute)
{
  NS_ENSURE_TRUE(aNode, false);
  if (!aAttribute || aAttribute->IsEmpty()) {
    // everybody has the 'null' attribute
    return true;
  }

  // get element
  nsCOMPtr<dom::Element> element = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(element, false);

  nsCOMPtr<nsIAtom> atom = do_GetAtom(*aAttribute);
  NS_ENSURE_TRUE(atom, false);

  return element->HasAttr(kNameSpaceID_None, atom);
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
    parent = GetNodeLocation(tmp, &tmpOffset);
    tmp = parent;
  }
  NS_ENSURE_TRUE(tmp, NS_ERROR_NULL_POINTER);
  if (nsHTMLEditUtils::IsNamedAnchor(tmp))
  {
    parent = GetNodeLocation(tmp, &tmpOffset);
    startNode = parent;
    startOffset = tmpOffset;
  }

  tmp = endNode;
  while ( tmp && 
          !nsTextEditUtils::IsBody(tmp) &&
          !nsHTMLEditUtils::IsNamedAnchor(tmp))
  {
    parent = GetNodeLocation(tmp, &tmpOffset);
    tmp = parent;
  }
  NS_ENSURE_TRUE(tmp, NS_ERROR_NULL_POINTER);
  if (nsHTMLEditUtils::IsNamedAnchor(tmp))
  {
    parent = GetNodeLocation(tmp, &tmpOffset);
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
    parent = GetNodeLocation(startNode, &startOffset);
    startNode = parent;
  }
  NS_ENSURE_TRUE(startNode, NS_ERROR_NULL_POINTER);
  
  while ( endNode && 
          !nsTextEditUtils::IsBody(endNode) && 
          IsEditable(endNode) &&
          IsAtEndOfNode(endNode, endOffset) )
  {
    parent = GetNodeLocation(endNode, &endOffset);
    endNode = parent;
    endOffset++;  // we are AFTER this node
  }
  NS_ENSURE_TRUE(endNode, NS_ERROR_NULL_POINTER);
  
  res = inRange->SetStart(startNode, startOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = inRange->SetEnd(endNode, endOffset);
  return res;
}

bool nsHTMLEditor::IsAtFrontOfNode(nsIDOMNode *aNode, PRInt32 aOffset)
{
  NS_ENSURE_TRUE(aNode, false);  // oops
  if (!aOffset) {
    return true;
  }

  if (IsTextNode(aNode))
  {
    return false;
  }
  else
  {
    nsCOMPtr<nsIDOMNode> firstNode;
    GetFirstEditableChild(aNode, address_of(firstNode));
    NS_ENSURE_TRUE(firstNode, true); 
    PRInt32 offset = GetChildOffset(firstNode, aNode);
    if (offset < aOffset) return false;
    return true;
  }
}

bool nsHTMLEditor::IsAtEndOfNode(nsIDOMNode *aNode, PRInt32 aOffset)
{
  NS_ENSURE_TRUE(aNode, false);  // oops
  PRUint32 len;
  GetLengthOfDOMNode(aNode, len);
  if (aOffset == (PRInt32)len) return true;
  
  if (IsTextNode(aNode))
  {
    return false;
  }
  else
  {
    nsCOMPtr<nsIDOMNode> lastNode;
    GetLastEditableChild(aNode, address_of(lastNode));
    NS_ENSURE_TRUE(lastNode, true); 
    PRInt32 offset = GetChildOffset(lastNode, aNode);
    if (offset < aOffset) return true;
    return false;
  }
}


nsresult
nsHTMLEditor::GetInlinePropertyBase(nsIAtom *aProperty, 
                                    const nsAString *aAttribute,
                                    const nsAString *aValue,
                                    bool *aFirst, 
                                    bool *aAny, 
                                    bool *aAll,
                                    nsAString *outValue,
                                    bool aCheckDefaults)
{
  NS_ENSURE_TRUE(aProperty, NS_ERROR_NULL_POINTER);

  nsresult result;
  *aAny = false;
  *aAll = true;
  *aFirst = false;
  bool first = true;

  nsCOMPtr<nsISelection> selection;
  result = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(result, result);
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));

  bool isCollapsed = selection->Collapsed();
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
  if (NS_SUCCEEDED(result) && currentItem) {
    bool firstNodeInRange = true; // for each range, set a flag 
    nsCOMPtr<nsIDOMRange> range(do_QueryInterface(currentItem));

    if (isCollapsed) {
      range->GetStartContainer(getter_AddRefs(collapsedNode));
      NS_ENSURE_TRUE(collapsedNode, NS_ERROR_FAILURE);
      bool isSet, theSetting;
      nsString tOutString;
      if (aAttribute) {
        nsString tString(*aAttribute);
        mTypeInState->GetTypingState(isSet, theSetting, aProperty, tString,
                                     &tOutString);
        if (outValue) {
          outValue->Assign(tOutString);
        }
      } else {
        mTypeInState->GetTypingState(isSet, theSetting, aProperty);
      }
      if (isSet) {
        *aFirst = *aAny = *aAll = theSetting;
        return NS_OK;
      }

      // Bug 747889: we don't support CSS for fontSize values
      if ((aProperty != nsEditProperty::font ||
           !aAttribute->EqualsLiteral("size")) &&
          mHTMLCSSUtils->IsCSSEditableProperty(collapsedNode, aProperty,
                                               aAttribute)) {
        mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(
          collapsedNode, aProperty, aAttribute, isSet, tOutString,
          COMPUTED_STYLE_TYPE);
        if (outValue) {
          outValue->Assign(tOutString);
        }
        *aFirst = *aAny = *aAll = isSet;
        return NS_OK;
      }

      IsTextPropertySetByContent(collapsedNode, aProperty, aAttribute, aValue,
                                 isSet, outValue);
      *aFirst = *aAny = *aAll = isSet;

      if (!isSet && aCheckDefaults) {
        // style not set, but if it is a default then it will appear if
        // content is inserted, so we should report it as set (analogous to
        // TypeInState).
        PRInt32 index;
        if (aAttribute && TypeInState::FindPropInList(aProperty, *aAttribute,
                                                      outValue, mDefaultStyles,
                                                      index)) {
          *aFirst = *aAny = *aAll = true;
          if (outValue) {
            outValue->Assign(mDefaultStyles[index]->value);
          }
        }
      }
      return NS_OK;
    }

    // non-collapsed selection
    nsCOMPtr<nsIContentIterator> iter =
            do_CreateInstance("@mozilla.org/content/post-content-iterator;1");
    NS_ENSURE_TRUE(iter, NS_ERROR_NULL_POINTER);

    nsAutoString firstValue, theValue;

    nsCOMPtr<nsIDOMNode> endNode;
    PRInt32 endOffset;
    result = range->GetEndContainer(getter_AddRefs(endNode));
    NS_ENSURE_SUCCESS(result, result);
    result = range->GetEndOffset(&endOffset);
    NS_ENSURE_SUCCESS(result, result);

    for (iter->Init(range); !iter->IsDone(); iter->Next()) {
      if (!iter->GetCurrentNode()->IsContent()) {
        continue;
      }
      nsCOMPtr<nsIContent> content = iter->GetCurrentNode()->AsContent();
      nsCOMPtr<nsIDOMNode> node = content->AsDOMNode();

      if (nsTextEditUtils::IsBody(node)) {
        break;
      }

      nsCOMPtr<nsIDOMCharacterData> text;
      text = do_QueryInterface(content);
      
      // just ignore any non-editable nodes
      if (text && !IsEditable(text)) {
        continue;
      }
      if (text) {
        if (!isCollapsed && first && firstNodeInRange) {
          firstNodeInRange = false;
          PRInt32 startOffset;
          range->GetStartOffset(&startOffset);
          PRUint32 count;
          text->GetLength(&count);
          if (startOffset == (PRInt32)count) {
            continue;
          }
        } else if (node == endNode && !endOffset) {
          continue;
        }
      } else if (content->IsElement()) {
        // handle non-text leaf nodes here
        continue;
      }

      bool isSet = false;
      if (first) {
        if (mHTMLCSSUtils->IsCSSEditableProperty(node, aProperty,
                                                 aAttribute) &&
            // Bug 747889: we don't support CSS for fontSize values
            (aProperty != nsEditProperty::font ||
             !aAttribute->EqualsLiteral("size"))) {
          // the HTML styles defined by aProperty/aAttribute has a CSS
          // equivalence in this implementation for node; let's check if it
          // carries those css styles
          if (aValue) {
            firstValue.Assign(*aValue);
          }
          mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(node, aProperty, aAttribute,
                                                             isSet, firstValue,
                                                             COMPUTED_STYLE_TYPE);
        } else {
          IsTextPropertySetByContent(node, aProperty, aAttribute, aValue, isSet,
                                     &firstValue);
        }
        *aFirst = isSet;
        first = false;
        if (outValue) {
          *outValue = firstValue;
        }
      } else {
        if (mHTMLCSSUtils->IsCSSEditableProperty(node, aProperty,
                                                 aAttribute) &&
            // Bug 747889: we don't support CSS for fontSize values
            (aProperty != nsEditProperty::font ||
             !aAttribute->EqualsLiteral("size"))) {
          // the HTML styles defined by aProperty/aAttribute has a CSS equivalence
          // in this implementation for node; let's check if it carries those css styles
          if (aValue) {
            theValue.Assign(*aValue);
          }
          mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(node, aProperty, aAttribute,
                                                             isSet, theValue,
                                                             COMPUTED_STYLE_TYPE);
        } else {
          IsTextPropertySetByContent(node, aProperty, aAttribute, aValue, isSet,
                                     &theValue);
        }
        if (firstValue != theValue) {
          *aAll = false;
        }
      }

      if (isSet) {
        *aAny = true;
      } else {
        *aAll = false;
      }
    }
  }
  if (!*aAny) {
    // make sure that if none of the selection is set, we don't report all is
    // set
    *aAll = false;
  }
  return result;
}


NS_IMETHODIMP nsHTMLEditor::GetInlineProperty(nsIAtom *aProperty, 
                                              const nsAString &aAttribute, 
                                              const nsAString &aValue,
                                              bool *aFirst, 
                                              bool *aAny, 
                                              bool *aAll)
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
                                              bool *aFirst, 
                                              bool *aAny, 
                                              bool *aAll,
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
  MOZ_ASSERT_IF(aProperty, aAttribute);
  NS_ENSURE_TRUE(mRules, NS_ERROR_NOT_INITIALIZED);
  ForceCompositionEnd();

  nsresult res;
  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  bool useCSS = IsCSSEnabled();
  if (selection->Collapsed()) {
    // manipulating text attributes on a collapsed selection only sets state for the next text insertion

    // For links, aProperty uses "href", use "a" instead
    if (aProperty == nsEditProperty::href ||
        aProperty == nsEditProperty::name)
      aProperty = nsEditProperty::a;

    if (aProperty) {
      mTypeInState->ClearProp(aProperty, *aAttribute);
    } else {
      mTypeInState->ClearAllProps();
    }
    return NS_OK;
  }

  nsAutoEditBatch batchIt(this);
  nsAutoRules beginRulesSniffing(this, kOpRemoveTextProperty, nsIEditor::eNext);
  nsAutoSelectionReset selectionResetter(selection, this);
  nsAutoTxnsConserveSelection dontSpazMySelection(this);
  
  bool cancel, handled;
  nsTextRulesInfo ruleInfo(kOpRemoveTextProperty);
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  NS_ENSURE_SUCCESS(res, res);
  if (!cancel && !handled)
  {
    // get selection range enumerator
    nsCOMPtr<nsIEnumerator> enumerator;
    res = selection->GetEnumerator(getter_AddRefs(enumerator));
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
          bool isSet = false;
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
            bool isSet = false;
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
  // if it's collapsed set typing state
  if (selection->Collapsed()) {
    nsCOMPtr<nsIAtom> atom;
    if (aSizeChange==1) atom = nsEditProperty::big;
    else                atom = nsEditProperty::small;

    // Let's see in what kind of element the selection is
    PRInt32 offset;
    nsCOMPtr<nsIDOMNode> selectedNode;
    GetStartNodeAndOffset(selection, getter_AddRefs(selectedNode), &offset);
    NS_ENSURE_TRUE(selectedNode, NS_OK);
    if (IsTextNode(selectedNode)) {
      nsCOMPtr<nsIDOMNode> parent;
      res = selectedNode->GetParentNode(getter_AddRefs(parent));
      NS_ENSURE_SUCCESS(res, res);
      selectedNode = parent;
    }
    if (!CanContainTag(selectedNode, atom)) {
      return NS_OK;
    }

    // manipulating text attributes on a collapsed selection only sets state for the next text insertion
    mTypeInState->SetProp(atom, EmptyString(), EmptyString());
    return NS_OK;
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

      // iterate range and build up array
      res = iter->Init(range);
      if (NS_SUCCEEDED(res)) {
        nsCOMArray<nsIContent> arrayOfNodes;
        while (!iter->IsDone()) {
          NS_ENSURE_TRUE(iter->GetCurrentNode()->IsContent(), NS_ERROR_FAILURE);
          nsCOMPtr<nsIContent> node = iter->GetCurrentNode()->AsContent();

          if (IsEditable(node)) {
            arrayOfNodes.AppendObject(node);
          }

          iter->Next();
        }
        
        // now that we have the list, do the font size change on each node
        PRInt32 listCount = arrayOfNodes.Count();
        for (PRInt32 j = 0; j < listCount; ++j) {
          nsIContent* node = arrayOfNodes[j];
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
  if (!CanContainTag(parent, nsGkAtoms::big)) {
    return NS_OK;
  }

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
nsHTMLEditor::RelativeFontChangeHelper(PRInt32 aSizeChange, nsINode* aNode)
{
  MOZ_ASSERT(aNode);

  /*  This routine looks for all the font nodes in the tree rooted by aNode,
      including aNode itself, looking for font nodes that have the size attr
      set.  Any such nodes need to have big or small put inside them, since
      they override any big/small that are above them.
  */
  
  // Can only change font size by + or - 1
  if (aSizeChange != 1 && aSizeChange != -1) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  // If this is a font node with size, put big/small inside it.
  if (aNode->IsElement() && aNode->AsElement()->IsHTML(nsGkAtoms::font) &&
      aNode->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::size)) {
    // Cycle through children and adjust relative font size.
    for (PRUint32 i = aNode->GetChildCount(); i--; ) {
      nsresult rv = RelativeFontChangeOnNode(aSizeChange, aNode->GetChildAt(i));
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // Now cycle through the children.
  for (PRUint32 i = aNode->GetChildCount(); i--; ) {
    nsresult rv = RelativeFontChangeHelper(aSizeChange, aNode->GetChildAt(i));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


nsresult
nsHTMLEditor::RelativeFontChangeOnNode(PRInt32 aSizeChange, nsINode* aNode)
{
  MOZ_ASSERT(aNode);
  // Can only change font size by + or - 1
  if (aSizeChange != 1 && aSizeChange != -1) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsIAtom* atom;
  if (aSizeChange == 1) {
    atom = nsGkAtoms::big;
  } else {
    atom = nsGkAtoms::small;
  }
  
  // Is it the opposite of what we want?
  if (aNode->IsElement() &&
      ((aSizeChange == 1 && aNode->AsElement()->IsHTML(nsGkAtoms::small)) ||
       (aSizeChange == -1 && aNode->AsElement()->IsHTML(nsGkAtoms::big)))) {
    // first populate any nested font tags that have the size attr set
    nsresult rv = RelativeFontChangeHelper(aSizeChange, aNode);
    NS_ENSURE_SUCCESS(rv, rv);
    // in that case, just remove this node and pull up the children
    return RemoveContainer(aNode);
  }

  // can it be put inside a "big" or "small"?
  if (TagCanContain(atom, aNode->AsDOMNode())) {
    // first populate any nested font tags that have the size attr set
    nsresult rv = RelativeFontChangeHelper(aSizeChange, aNode);
    NS_ENSURE_SUCCESS(rv, rv);

    // ok, chuck it in.
    // first look at siblings of aNode for matching bigs or smalls.
    // if we find one, move aNode into it.
    nsIContent* sibling = GetPriorHTMLSibling(aNode);
    if (sibling && sibling->IsHTML(atom)) {
      // previous sib is already right kind of inline node; slide this over into it
      return MoveNode(aNode->AsDOMNode(), sibling->AsDOMNode(), -1);
    }

    sibling = GetNextHTMLSibling(aNode);
    if (sibling && sibling->IsHTML(atom)) {
      // following sib is already right kind of inline node; slide this over into it
      return MoveNode(aNode->AsDOMNode(), sibling->AsDOMNode(), 0);
    }

    // else insert it above aNode
    nsCOMPtr<nsIDOMNode> tmp;
    return InsertContainerAbove(aNode->AsDOMNode(), address_of(tmp),
                                nsAtomString(atom));
  }

  // none of the above?  then cycle through the children.
  // MOOSE: we should group the children together if possible
  // into a single "big" or "small".  For the moment they are
  // each getting their own.  
  for (PRUint32 i = aNode->GetChildCount(); i--; ) {
    nsresult rv = RelativeFontChangeOnNode(aSizeChange, aNode->GetChildAt(i));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLEditor::GetFontFaceState(bool *aMixed, nsAString &outFace)
{
  NS_ENSURE_TRUE(aMixed, NS_ERROR_FAILURE);
  *aMixed = true;
  outFace.Truncate();

  nsresult res;
  bool first, any, all;
  
  NS_NAMED_LITERAL_STRING(attr, "face");
  res = GetInlinePropertyBase(nsEditProperty::font, &attr, nsnull, &first, &any, &all, &outFace);
  NS_ENSURE_SUCCESS(res, res);
  if (any && !all) return res; // mixed
  if (all)
  {
    *aMixed = false;
    return res;
  }
  
  // if there is no font face, check for tt
  res = GetInlinePropertyBase(nsEditProperty::tt, nsnull, nsnull, &first, &any, &all,nsnull);
  NS_ENSURE_SUCCESS(res, res);
  if (any && !all) return res; // mixed
  if (all)
  {
    *aMixed = false;
    nsEditProperty::tt->ToString(outFace);
  }
  
  if (!any)
  {
    // there was no font face attrs of any kind.  We are in normal font.
    outFace.Truncate();
    *aMixed = false;
  }
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::GetFontColorState(bool *aMixed, nsAString &aOutColor)
{
  NS_ENSURE_TRUE(aMixed, NS_ERROR_NULL_POINTER);
  *aMixed = true;
  aOutColor.Truncate();
  
  nsresult res;
  NS_NAMED_LITERAL_STRING(colorStr, "color");
  bool first, any, all;
  
  res = GetInlinePropertyBase(nsEditProperty::font, &colorStr, nsnull, &first, &any, &all, &aOutColor);
  NS_ENSURE_SUCCESS(res, res);
  if (any && !all) return res; // mixed
  if (all)
  {
    *aMixed = false;
    return res;
  }
  
  if (!any)
  {
    // there was no font color attrs of any kind..
    aOutColor.Truncate();
    *aMixed = false;
  }
  return res;
}

// the return value is true only if the instance of the HTML editor we created
// can handle CSS styles (for instance, Composer can, Messenger can't) and if
// the CSS preference is checked
nsresult
nsHTMLEditor::GetIsCSSEnabled(bool *aIsCSSEnabled)
{
  *aIsCSSEnabled = IsCSSEnabled();
  return NS_OK;
}

static bool
HasNonEmptyAttribute(dom::Element* aElement, nsIAtom* aName)
{
  MOZ_ASSERT(aElement);

  nsAutoString value;
  return aElement->GetAttr(kNameSpaceID_None, aName, value) && !value.IsEmpty();
}

bool
nsHTMLEditor::HasStyleOrIdOrClass(dom::Element* aElement)
{
  MOZ_ASSERT(aElement);

  // remove the node if its style attribute is empty or absent,
  // and if it does not have a class nor an id
  return HasNonEmptyAttribute(aElement, nsGkAtoms::style) ||
         HasNonEmptyAttribute(aElement, nsGkAtoms::_class) ||
         HasNonEmptyAttribute(aElement, nsGkAtoms::id);
}

nsresult
nsHTMLEditor::RemoveElementIfNoStyleOrIdOrClass(nsIDOMNode* aElement)
{
  nsCOMPtr<dom::Element> element = do_QueryInterface(aElement);
  NS_ENSURE_TRUE(element, NS_ERROR_NULL_POINTER);

  // early way out if node is not the right kind of element
  if ((!element->IsHTML(nsGkAtoms::span) &&
       !element->IsHTML(nsGkAtoms::font)) ||
      HasStyleOrIdOrClass(element)) {
    return NS_OK;
  }

  return RemoveContainer(element);
}
