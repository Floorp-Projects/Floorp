/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "TypeInState.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/Element.h"
#include "mozilla/mozalloc.h"
#include "nsAString.h"
#include "nsAttrName.h"
#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsCaseTreatment.h"
#include "nsComponentManagerUtils.h"
#include "nsDebug.h"
#include "nsEditRules.h"
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
#include "nsIDOMCharacterData.h"
#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsIEditor.h"
#include "nsIEditorIMESupport.h"
#include "nsNameSpaceManager.h"
#include "nsINode.h"
#include "nsISupportsImpl.h"
#include "nsLiteralString.h"
#include "nsRange.h"
#include "nsReadableUtils.h"
#include "nsSelectionState.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "nsTextEditRules.h"
#include "nsTextEditUtils.h"
#include "nsUnicharUtils.h"
#include "nscore.h"

class nsISupports;

using namespace mozilla;
using namespace mozilla::dom;

static bool
IsEmptyTextNode(nsHTMLEditor* aThis, nsINode* aNode)
{
  bool isEmptyTextNode = false;
  return nsEditor::IsTextNode(aNode) &&
         NS_SUCCEEDED(aThis->IsEmptyNode(aNode, &isEmptyTextNode)) &&
         isEmptyTextNode;
}

NS_IMETHODIMP nsHTMLEditor::AddDefaultProperty(nsIAtom *aProperty, 
                                            const nsAString & aAttribute, 
                                            const nsAString & aValue)
{
  nsString outValue;
  int32_t index;
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
  int32_t index;
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
  uint32_t j, defcon = mDefaultStyles.Length();
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
  nsAutoRules beginRulesSniffing(this, EditAction::insertElement, nsIEditor::eNext);
  nsAutoSelectionReset selectionResetter(selection, this);
  nsAutoTxnsConserveSelection dontSpazMySelection(this);

  bool cancel, handled;
  nsTextRulesInfo ruleInfo(EditAction::setTextProperty);
  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);
  nsresult res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  NS_ENSURE_SUCCESS(res, res);
  if (!cancel && !handled) {
    // loop thru the ranges in the selection
    uint32_t rangeCount = selection->RangeCount();
    for (uint32_t rangeIdx = 0; rangeIdx < rangeCount; ++rangeIdx) {
      nsRefPtr<nsRange> range = selection->GetRangeAt(rangeIdx);

      // adjust range to include any ancestors whose children are entirely
      // selected
      res = PromoteInlineRange(range);
      NS_ENSURE_SUCCESS(res, res);

      // check for easy case: both range endpoints in same text node
      nsCOMPtr<nsINode> startNode = range->GetStartParent();
      nsCOMPtr<nsINode> endNode = range->GetEndParent();
      if (startNode && startNode == endNode && startNode->GetAsText()) {
        res = SetInlinePropertyOnTextNode(*startNode->GetAsText(),
                                          range->StartOffset(),
                                          range->EndOffset(),
                                          *aProperty, &aAttribute, aValue);
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
      if (startNode && startNode->GetAsText() && IsEditable(startNode)) {
        res = SetInlinePropertyOnTextNode(*startNode->GetAsText(),
                                          range->StartOffset(),
                                          startNode->Length(), *aProperty,
                                          &aAttribute, aValue);
        NS_ENSURE_SUCCESS(res, res);
      }

      // then loop through the list, set the property on each node
      int32_t listCount = arrayOfNodes.Count();
      int32_t j;
      for (j = 0; j < listCount; j++) {
        res = SetInlinePropertyOnNode(arrayOfNodes[j], aProperty,
                                      &aAttribute, &aValue);
        NS_ENSURE_SUCCESS(res, res);
      }

      // last check the end parent of the range to see if it needs to
      // be separately handled (it does if it's a text node, due to how the
      // subtree iterator works - it will not have reported it).
      if (endNode && endNode->GetAsText() && IsEditable(endNode)) {
        res = SetInlinePropertyOnTextNode(*endNode->GetAsText(), 0,
                                          range->EndOffset(), *aProperty,
                                          &aAttribute, aValue);
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
  if (element->IsHTMLElement(aProperty) && !element->GetAttrCount() &&
      (!aAttribute || aAttribute->IsEmpty())) {
    return true;
  }

  // Special cases for various equivalencies: <strong>, <em>, <s>
  if (!element->GetAttrCount() &&
      ((aProperty == nsGkAtoms::b &&
        element->IsHTMLElement(nsGkAtoms::strong)) ||
       (aProperty == nsGkAtoms::i &&
        element->IsHTMLElement(nsGkAtoms::em)) ||
       (aProperty == nsGkAtoms::strike &&
        element->IsHTMLElement(nsGkAtoms::s)))) {
    return true;
  }

  // Now look for things like <font>
  if (aAttribute && !aAttribute->IsEmpty()) {
    nsCOMPtr<nsIAtom> atom = do_GetAtom(*aAttribute);
    MOZ_ASSERT(atom);

    nsString attrValue;
    if (element->IsHTMLElement(aProperty) &&
        IsOnlyAttribute(element, *aAttribute) &&
        element->GetAttr(kNameSpaceID_None, atom, attrValue) &&
        attrValue.Equals(*aValue, nsCaseInsensitiveStringComparator())) {
      // This is not quite correct, because it excludes cases like
      // <font face=000> being the same as <font face=#000000>.
      // Property-specific handling is needed (bug 760211).
      return true;
    }
  }

  // No luck so far.  Now we check for a <span> with a single style=""
  // attribute that sets only the style we're looking for, if this type of
  // style supports it
  if (!mHTMLCSSUtils->IsCSSEditableProperty(element, aProperty, aAttribute) ||
      !element->IsHTMLElement(nsGkAtoms::span) ||
      element->GetAttrCount() != 1 ||
      !element->HasAttr(kNameSpaceID_None, nsGkAtoms::style)) {
    return false;
  }

  // Some CSS styles are not so simple.  For instance, underline is
  // "text-decoration: underline", which decomposes into four different text-*
  // properties.  So for now, we just create a span, add the desired style, and
  // see if it matches.
  nsCOMPtr<Element> newSpan = CreateHTMLContent(nsGkAtoms::span);
  NS_ASSERTION(newSpan, "CreateHTMLContent failed");
  NS_ENSURE_TRUE(newSpan, false);
  mHTMLCSSUtils->SetCSSEquivalentToHTMLStyle(newSpan, aProperty,
                                             aAttribute, aValue,
                                             /*suppress transaction*/ true);

  return mHTMLCSSUtils->ElementsSameStyle(newSpan, element);
}


nsresult
nsHTMLEditor::SetInlinePropertyOnTextNode(Text& aText,
                                          int32_t aStartOffset,
                                          int32_t aEndOffset,
                                          nsIAtom& aProperty,
                                          const nsAString* aAttribute,
                                          const nsAString& aValue)
{
  if (!aText.GetParentNode() ||
      !CanContainTag(*aText.GetParentNode(), aProperty)) {
    return NS_OK;
  }

  // Don't need to do anything if no characters actually selected
  if (aStartOffset == aEndOffset) {
    return NS_OK;
  }

  // Don't need to do anything if property already set on node
  if (mHTMLCSSUtils->IsCSSEditableProperty(&aText, &aProperty, aAttribute)) {
    // The HTML styles defined by aProperty/aAttribute have a CSS equivalence
    // for node; let's check if it carries those CSS styles
    if (mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(&aText, &aProperty,
          aAttribute, aValue, nsHTMLCSSUtils::eComputed)) {
      return NS_OK;
    }
  } else if (IsTextPropertySetByContent(&aText, &aProperty, aAttribute,
                                        &aValue)) {
    return NS_OK;
  }

  // Do we need to split the text node?
  ErrorResult rv;
  nsRefPtr<Text> text = &aText;
  if (uint32_t(aEndOffset) != aText.Length()) {
    // We need to split off back of text node
    text = SplitNode(aText, aEndOffset, rv)->GetAsText();
    NS_ENSURE_SUCCESS(rv.ErrorCode(), rv.ErrorCode());
  }

  if (aStartOffset) {
    // We need to split off front of text node
    SplitNode(*text, aStartOffset, rv);
    NS_ENSURE_SUCCESS(rv.ErrorCode(), rv.ErrorCode());
  }

  if (aAttribute) {
    // Look for siblings that are correct type of node
    nsIContent* sibling = GetPriorHTMLSibling(text);
    if (IsSimpleModifiableNode(sibling, &aProperty, aAttribute, &aValue)) {
      // Previous sib is already right kind of inline node; slide this over
      return MoveNode(text, sibling, -1);
    }
    sibling = GetNextHTMLSibling(text);
    if (IsSimpleModifiableNode(sibling, &aProperty, aAttribute, &aValue)) {
      // Following sib is already right kind of inline node; slide this over
      return MoveNode(text, sibling, 0);
    }
  }

  // Reparent the node inside inline node with appropriate {attribute,value}
  return SetInlinePropertyOnNode(text, &aProperty, aAttribute, &aValue);
}


nsresult
nsHTMLEditor::SetInlinePropertyOnNodeImpl(nsIContent* aNode,
                                          nsIAtom* aProperty,
                                          const nsAString* aAttribute,
                                          const nsAString* aValue)
{
  MOZ_ASSERT(aNode && aProperty);
  MOZ_ASSERT(aValue);

  nsCOMPtr<nsIAtom> attrAtom = aAttribute ? do_GetAtom(*aAttribute) : nullptr;

  // If this is an element that can't be contained in a span, we have to
  // recurse to its children.
  if (!TagCanContain(*nsGkAtoms::span, *aNode)) {
    if (aNode->HasChildren()) {
      nsCOMArray<nsIContent> arrayOfNodes;

      // Populate the list.
      for (nsIContent* child = aNode->GetFirstChild();
           child;
           child = child->GetNextSibling()) {
        if (IsEditable(child) && !IsEmptyTextNode(this, child)) {
          arrayOfNodes.AppendObject(child);
        }
      }

      // Then loop through the list, set the property on each node.
      int32_t listCount = arrayOfNodes.Count();
      for (int32_t j = 0; j < listCount; ++j) {
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
      res = JoinNodes(*previousSibling, *nextSibling);
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
  if (mHTMLCSSUtils->IsCSSEditableProperty(aNode, aProperty, aAttribute)) {
    if (mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(
          aNode, aProperty, aAttribute, *aValue, nsHTMLCSSUtils::eComputed)) {
      return NS_OK;
    }
  } else if (IsTextPropertySetByContent(aNode, aProperty,
                                        aAttribute, aValue)) {
    return NS_OK;
  }

  bool useCSS = (IsCSSEnabled() &&
                 mHTMLCSSUtils->IsCSSEditableProperty(aNode, aProperty, aAttribute)) ||
                // bgcolor is always done using CSS
                aAttribute->EqualsLiteral("bgcolor");

  if (useCSS) {
    nsCOMPtr<dom::Element> tmp;
    // We only add style="" to <span>s with no attributes (bug 746515).  If we
    // don't have one, we need to make one.
    if (aNode->IsHTMLElement(nsGkAtoms::span) &&
        !aNode->AsElement()->GetAttrCount()) {
      tmp = aNode->AsElement();
    } else {
      tmp = InsertContainerAbove(aNode, nsGkAtoms::span);
      NS_ENSURE_STATE(tmp);
    }

    // Add the CSS styles corresponding to the HTML style request
    int32_t count;
    res = mHTMLCSSUtils->SetCSSEquivalentToHTMLStyle(tmp->AsDOMNode(),
                                                     aProperty, aAttribute,
                                                     aValue, &count, false);
    NS_ENSURE_SUCCESS(res, res);
    return NS_OK;
  }

  // is it already the right kind of node, but with wrong attribute?
  if (aNode->IsHTMLElement(aProperty)) {
    // Just set the attribute on it.
    nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(aNode);
    return SetAttribute(elem, *aAttribute, *aValue);
  }

  // ok, chuck it in its very own container
  nsCOMPtr<Element> tmp = InsertContainerAbove(aNode, aProperty, attrAtom,
                                               aValue);
  NS_ENSURE_STATE(tmp);

  return NS_OK;
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
  nsCOMPtr<nsINode> parent = aNode->GetParentNode();
  NS_ENSURE_STATE(parent);

  nsresult res = RemoveStyleInside(aNode->AsDOMNode(), aProperty, aAttribute);
  NS_ENSURE_SUCCESS(res, res);

  if (aNode->GetParentNode()) {
    // The node is still where it was
    return SetInlinePropertyOnNodeImpl(aNode, aProperty,
                                       aAttribute, aValue);
  }

  // It's vanished.  Use the old siblings for reference to construct a
  // list.  But first, verify that the previous/next siblings are still
  // where we expect them; otherwise we have to give up.
  if ((previousSibling && previousSibling->GetParentNode() != parent) ||
      (nextSibling && nextSibling->GetParentNode() != parent)) {
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

  int32_t nodesToSetCount = nodesToSet.Count();
  for (int32_t k = 0; k < nodesToSetCount; k++) {
    res = SetInlinePropertyOnNodeImpl(nodesToSet[k], aProperty,
                                      aAttribute, aValue);
    NS_ENSURE_SUCCESS(res, res);
  }

  return NS_OK;
}


nsresult
nsHTMLEditor::SplitStyleAboveRange(nsRange* inRange, nsIAtom* aProperty,
                                   const nsAString* aAttribute)
{
  NS_ENSURE_TRUE(inRange, NS_ERROR_NULL_POINTER);
  nsresult res;
  nsCOMPtr<nsIDOMNode> startNode, endNode, origStartNode;
  int32_t startOffset, endOffset;

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
                                           int32_t *aOffset,
                                           nsIAtom *aProperty,          // null here means we split all properties
                                           const nsAString *aAttribute,
                                           nsCOMPtr<nsIDOMNode> *outLeftNode,
                                           nsCOMPtr<nsIDOMNode> *outRightNode)
{
  NS_ENSURE_TRUE(aNode && *aNode && aOffset, NS_ERROR_NULL_POINTER);
  if (outLeftNode)  *outLeftNode  = nullptr;
  if (outRightNode) *outRightNode = nullptr;
  // split any matching style nodes above the node/offset
  nsCOMPtr<nsIContent> node = do_QueryInterface(*aNode);
  NS_ENSURE_STATE(node);
  int32_t offset;

  bool useCSS = IsCSSEnabled();

  bool isSet;
  while (node && !IsBlockNode(node) && node->GetParentNode() &&
         IsEditable(node->GetParentNode())) {
    isSet = false;
    if (useCSS && mHTMLCSSUtils->IsCSSEditableProperty(node, aProperty, aAttribute)) {
      // the HTML style defined by aProperty/aAttribute has a CSS equivalence
      // in this implementation for the node; let's check if it carries those css styles
      nsAutoString firstValue;
      mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(GetAsDOMNode(node),
        aProperty, aAttribute, isSet, firstValue, nsHTMLCSSUtils::eSpecified);
    }
    if (// node is the correct inline prop
        (aProperty && node->IsHTMLElement(aProperty)) ||
        // node is href - test if really <a href=...
        (aProperty == nsGkAtoms::href && nsHTMLEditUtils::IsLink(node)) ||
        // or node is any prop, and we asked to split them all
        (!aProperty && NodeIsProperty(GetAsDOMNode(node))) ||
        // or the style is specified in the style attribute
        isSet) {
      // found a style node we need to split
      nsresult rv = SplitNodeDeep(GetAsDOMNode(node), *aNode, *aOffset,
                                  &offset, false, outLeftNode, outRightNode);
      NS_ENSURE_SUCCESS(rv, rv);
      // reset startNode/startOffset
      *aNode = GetAsDOMNode(node->GetParent());
      *aOffset = offset;
    }
    node = node->GetParent();
  }
  return NS_OK;
}

nsresult
nsHTMLEditor::ClearStyle(nsCOMPtr<nsIDOMNode>* aNode, int32_t* aOffset,
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
    nsCOMPtr<nsINode> rightNode_ = do_QueryInterface(rightNode);
    NS_ENSURE_STATE(rightNode_);
    nsCOMPtr<nsIDOMNode> secondSplitParent =
      GetAsDOMNode(GetLeftmostChild(rightNode_));
    // don't try to split non-containers (br's, images, hr's, etc)
    if (!secondSplitParent) {
      secondSplitParent = rightNode;
    }
    nsCOMPtr<Element> savedBR;
    if (!IsContainer(secondSplitParent)) {
      if (nsTextEditUtils::IsBreak(secondSplitParent)) {
        savedBR = do_QueryInterface(secondSplitParent);
        NS_ENSURE_STATE(savedBR);
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
    nsCOMPtr<nsINode> leftNode_ = do_QueryInterface(leftNode);
    NS_ENSURE_TRUE(leftNode_, NS_ERROR_FAILURE);
    nsCOMPtr<nsINode> newSelParent = GetLeftmostChild(leftNode_);
    if (!newSelParent) {
      newSelParent = do_QueryInterface(leftNode);
      NS_ENSURE_STATE(newSelParent);
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
    int32_t newSelOffset = 0;
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
    *aNode = GetAsDOMNode(newSelParent);
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
  if (NodeIsType(aNode, nsGkAtoms::a)) {
    return false;
  }
  return true;
}

nsresult nsHTMLEditor::ApplyDefaultProperties()
{
  nsresult res = NS_OK;
  uint32_t j, defcon = mDefaultStyles.Length();
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
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  NS_ENSURE_STATE(content);

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
  if (!aChildrenOnly &&
    (
      // node is prop we asked for
      (aProperty && NodeIsType(aNode, aProperty)) ||
      // but check for link (<a href=...)
      (aProperty == nsGkAtoms::href && nsHTMLEditUtils::IsLink(aNode)) ||
      // and for named anchors
      (aProperty == nsGkAtoms::name && nsHTMLEditUtils::IsNamedAnchor(aNode)) ||
      // or node is any prop and we asked for that
      (!aProperty && NodeIsProperty(aNode))
    )
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
        nsCOMPtr<Element> spanNode =
          InsertContainerAbove(content, nsGkAtoms::span);
        NS_ENSURE_STATE(spanNode);
        res = CloneAttribute(styleAttr, spanNode->AsDOMNode(), aNode);
        NS_ENSURE_SUCCESS(res, res);
        res = CloneAttribute(classAttr, spanNode->AsDOMNode(), aNode);
        NS_ENSURE_SUCCESS(res, res);
      }
      res = RemoveContainer(content);
      NS_ENSURE_SUCCESS(res, res);
    } else {
      // otherwise we just want to eliminate the attribute
      if (HasAttr(aNode, aAttribute)) {
        // if this matching attribute is the ONLY one on the node,
        // then remove the whole node.  Otherwise just nix the attribute.
        if (IsOnlyAttribute(aNode, aAttribute)) {
          res = RemoveContainer(content);
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
      mHTMLCSSUtils->IsCSSEditableProperty(content, aProperty, aAttribute)) {
    // the HTML style defined by aProperty/aAttribute has a CSS equivalence in
    // this implementation for the node aNode; let's check if it carries those
    // css styles
    nsAutoString propertyValue;
    bool isSet;
    mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(aNode, aProperty,
      aAttribute, isSet, propertyValue, nsHTMLCSSUtils::eSpecified);
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

  if (!aChildrenOnly &&
    (
      // Or node is big or small and we are setting font size
      aProperty == nsGkAtoms::font &&
      (nsHTMLEditUtils::IsBig(aNode) || nsHTMLEditUtils::IsSmall(aNode)) &&
      (aAttribute && aAttribute->LowerCaseEqualsLiteral("size"))
    )
  ) {
    // if we are setting font size, remove any nested bigs and smalls
    return RemoveContainer(content);
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

  uint32_t attrCount = aContent->GetAttrCount();
  for (uint32_t i = 0; i < attrCount; ++i) {
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


nsresult
nsHTMLEditor::PromoteRangeIfStartsOrEndsInNamedAnchor(nsRange* inRange)
{
  NS_ENSURE_TRUE(inRange, NS_ERROR_NULL_POINTER);
  nsresult res;
  nsCOMPtr<nsIDOMNode> startNode, endNode, parent, tmp;
  int32_t startOffset, endOffset, tmpOffset;
  
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

nsresult
nsHTMLEditor::PromoteInlineRange(nsRange* inRange)
{
  NS_ENSURE_TRUE(inRange, NS_ERROR_NULL_POINTER);
  nsresult res;
  nsCOMPtr<nsIDOMNode> startNode, endNode, parent;
  int32_t startOffset, endOffset;
  
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

bool nsHTMLEditor::IsAtFrontOfNode(nsIDOMNode *aNode, int32_t aOffset)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node, false);
  if (!aOffset) {
    return true;
  }

  if (IsTextNode(aNode))
  {
    return false;
  }
  else
  {
    nsCOMPtr<nsIContent> firstNode = GetFirstEditableChild(*node);
    NS_ENSURE_TRUE(firstNode, true); 
    int32_t offset = node->IndexOf(firstNode);
    if (offset < aOffset) return false;
    return true;
  }
}

bool nsHTMLEditor::IsAtEndOfNode(nsIDOMNode *aNode, int32_t aOffset)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node, false);
  uint32_t len = node->Length();
  if (aOffset == (int32_t)len) return true;
  
  if (IsTextNode(aNode))
  {
    return false;
  }
  else
  {
    nsCOMPtr<nsIContent> lastNode = GetLastEditableChild(*node);
    NS_ENSURE_TRUE(lastNode, true); 
    int32_t offset = node->IndexOf(lastNode);
    if (offset < aOffset) return true;
    return false;
  }
}


nsresult
nsHTMLEditor::GetInlinePropertyBase(nsIAtom& aProperty,
                                    const nsAString* aAttribute,
                                    const nsAString* aValue,
                                    bool* aFirst,
                                    bool* aAny,
                                    bool* aAll,
                                    nsAString* outValue,
                                    bool aCheckDefaults)
{
  *aAny = false;
  *aAll = true;
  *aFirst = false;
  bool first = true;

  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  bool isCollapsed = selection->Collapsed();
  nsRefPtr<nsRange> range = selection->GetRangeAt(0);
  // XXX: Should be a while loop, to get each separate range
  // XXX: ERROR_HANDLING can currentItem be null?
  if (range) {
    // For each range, set a flag
    bool firstNodeInRange = true;

    if (isCollapsed) {
      nsCOMPtr<nsINode> collapsedNode = range->GetStartParent();
      NS_ENSURE_TRUE(collapsedNode, NS_ERROR_FAILURE);
      bool isSet, theSetting;
      nsString tOutString;
      if (aAttribute) {
        nsString tString(*aAttribute);
        mTypeInState->GetTypingState(isSet, theSetting, &aProperty, tString,
                                     &tOutString);
        if (outValue) {
          outValue->Assign(tOutString);
        }
      } else {
        mTypeInState->GetTypingState(isSet, theSetting, &aProperty);
      }
      if (isSet) {
        *aFirst = *aAny = *aAll = theSetting;
        return NS_OK;
      }

      if (mHTMLCSSUtils->IsCSSEditableProperty(collapsedNode, &aProperty,
                                               aAttribute)) {
        *aFirst = *aAny = *aAll =
          mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(collapsedNode,
              &aProperty, aAttribute, tOutString, nsHTMLCSSUtils::eComputed);
        if (outValue) {
          outValue->Assign(tOutString);
        }
        return NS_OK;
      }

      isSet = IsTextPropertySetByContent(collapsedNode, &aProperty,
                                         aAttribute, aValue, outValue);
      *aFirst = *aAny = *aAll = isSet;

      if (!isSet && aCheckDefaults) {
        // Style not set, but if it is a default then it will appear if content
        // is inserted, so we should report it as set (analogous to
        // TypeInState).
        int32_t index;
        if (aAttribute && TypeInState::FindPropInList(&aProperty, *aAttribute,
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

    // Non-collapsed selection
    nsCOMPtr<nsIContentIterator> iter = NS_NewContentIterator();

    nsAutoString firstValue, theValue;

    nsCOMPtr<nsINode> endNode = range->GetEndParent();
    int32_t endOffset = range->EndOffset();

    for (iter->Init(range); !iter->IsDone(); iter->Next()) {
      if (!iter->GetCurrentNode()->IsContent()) {
        continue;
      }
      nsCOMPtr<nsIContent> content = iter->GetCurrentNode()->AsContent();

      if (content->IsHTMLElement(nsGkAtoms::body)) {
        break;
      }

      // just ignore any non-editable nodes
      if (content->GetAsText() && (!IsEditable(content) ||
                                   IsEmptyTextNode(this, content))) {
        continue;
      }
      if (content->GetAsText()) {
        if (!isCollapsed && first && firstNodeInRange) {
          firstNodeInRange = false;
          if (range->StartOffset() == (int32_t)content->Length()) {
            continue;
          }
        } else if (content == endNode && !endOffset) {
          continue;
        }
      } else if (content->IsElement()) {
        // handle non-text leaf nodes here
        continue;
      }

      bool isSet = false;
      if (first) {
        if (mHTMLCSSUtils->IsCSSEditableProperty(content, &aProperty,
                                                 aAttribute)) {
          // The HTML styles defined by aProperty/aAttribute have a CSS
          // equivalence in this implementation for node; let's check if it
          // carries those CSS styles
          if (aValue) {
            firstValue.Assign(*aValue);
          }
          isSet = mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(content,
              &aProperty, aAttribute, firstValue, nsHTMLCSSUtils::eComputed);
        } else {
          isSet = IsTextPropertySetByContent(content, &aProperty, aAttribute,
                                             aValue, &firstValue);
        }
        *aFirst = isSet;
        first = false;
        if (outValue) {
          *outValue = firstValue;
        }
      } else {
        if (mHTMLCSSUtils->IsCSSEditableProperty(content, &aProperty,
                                                 aAttribute)) {
          // The HTML styles defined by aProperty/aAttribute have a CSS
          // equivalence in this implementation for node; let's check if it
          // carries those CSS styles
          if (aValue) {
            theValue.Assign(*aValue);
          }
          isSet = mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(content,
              &aProperty, aAttribute, theValue, nsHTMLCSSUtils::eComputed);
        } else {
          isSet = IsTextPropertySetByContent(content, &aProperty, aAttribute,
                                             aValue, &theValue);
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
  return NS_OK;
}


NS_IMETHODIMP nsHTMLEditor::GetInlineProperty(nsIAtom *aProperty, 
                                              const nsAString &aAttribute, 
                                              const nsAString &aValue,
                                              bool *aFirst, 
                                              bool *aAny, 
                                              bool *aAll)
{
  NS_ENSURE_TRUE(aProperty && aFirst && aAny && aAll, NS_ERROR_NULL_POINTER);
  const nsAString *att = nullptr;
  if (!aAttribute.IsEmpty())
    att = &aAttribute;
  const nsAString *val = nullptr;
  if (!aValue.IsEmpty())
    val = &aValue;
  return GetInlinePropertyBase(*aProperty, att, val, aFirst, aAny, aAll, nullptr);
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
  const nsAString *att = nullptr;
  if (!aAttribute.IsEmpty())
    att = &aAttribute;
  const nsAString *val = nullptr;
  if (!aValue.IsEmpty())
    val = &aValue;
  return GetInlinePropertyBase(*aProperty, att, val, aFirst, aAny, aAll, &outValue);
}


NS_IMETHODIMP nsHTMLEditor::RemoveAllInlineProperties()
{
  nsAutoEditBatch batchIt(this);
  nsAutoRules beginRulesSniffing(this, EditAction::resetTextProperties, nsIEditor::eNext);

  nsresult res = RemoveInlinePropertyImpl(nullptr, nullptr);
  NS_ENSURE_SUCCESS(res, res);
  return ApplyDefaultProperties();
}

NS_IMETHODIMP nsHTMLEditor::RemoveInlineProperty(nsIAtom *aProperty, const nsAString &aAttribute)
{
  return RemoveInlinePropertyImpl(aProperty, &aAttribute);
}

nsresult
nsHTMLEditor::RemoveInlinePropertyImpl(nsIAtom* aProperty,
                                       const nsAString* aAttribute)
{
  MOZ_ASSERT_IF(aProperty, aAttribute);
  NS_ENSURE_TRUE(mRules, NS_ERROR_NOT_INITIALIZED);
  ForceCompositionEnd();

  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  if (selection->Collapsed()) {
    // Manipulating text attributes on a collapsed selection only sets state
    // for the next text insertion

    // For links, aProperty uses "href", use "a" instead
    if (aProperty == nsGkAtoms::href || aProperty == nsGkAtoms::name) {
      aProperty = nsGkAtoms::a;
    }

    if (aProperty) {
      mTypeInState->ClearProp(aProperty, *aAttribute);
    } else {
      mTypeInState->ClearAllProps();
    }
    return NS_OK;
  }

  nsAutoEditBatch batchIt(this);
  nsAutoRules beginRulesSniffing(this, EditAction::removeTextProperty,
                                 nsIEditor::eNext);
  nsAutoSelectionReset selectionResetter(selection, this);
  nsAutoTxnsConserveSelection dontSpazMySelection(this);

  bool cancel, handled;
  nsTextRulesInfo ruleInfo(EditAction::removeTextProperty);
  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);
  nsresult res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  NS_ENSURE_SUCCESS(res, res);
  if (!cancel && !handled) {
    // Loop through the ranges in the selection
    uint32_t rangeCount = selection->RangeCount();
    for (uint32_t rangeIdx = 0; rangeIdx < rangeCount; ++rangeIdx) {
      nsRefPtr<nsRange> range = selection->GetRangeAt(rangeIdx);
      if (aProperty == nsGkAtoms::name) {
        // Promote range if it starts or end in a named anchor and we want to
        // remove named anchors
        res = PromoteRangeIfStartsOrEndsInNamedAnchor(range);
      } else {
        // Adjust range to include any ancestors whose children are entirely
        // selected
        res = PromoteInlineRange(range);
      }
      NS_ENSURE_SUCCESS(res, res);

      // Remove this style from ancestors of our range endpoints, splitting
      // them as appropriate
      res = SplitStyleAboveRange(range, aProperty, aAttribute);
      NS_ENSURE_SUCCESS(res, res);

      // Check for easy case: both range endpoints in same text node
      nsCOMPtr<nsINode> startNode = range->GetStartParent();
      nsCOMPtr<nsINode> endNode = range->GetEndParent();
      if (startNode && startNode == endNode && startNode->GetAsText()) {
        // We're done with this range!
        if (IsCSSEnabled() &&
            mHTMLCSSUtils->IsCSSEditableProperty(startNode, aProperty,
                                                 aAttribute)) {
          // The HTML style defined by aProperty/aAttribute has a CSS
          // equivalence in this implementation for startNode
          if (mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(startNode,
                aProperty, aAttribute, EmptyString(),
                nsHTMLCSSUtils::eComputed)) {
            // startNode's computed style indicates the CSS equivalence to the
            // HTML style to remove is applied; but we found no element in the
            // ancestors of startNode carrying specified styles; assume it
            // comes from a rule and try to insert a span "inverting" the style
            if (mHTMLCSSUtils->IsCSSInvertible(*aProperty, aAttribute)) {
              NS_NAMED_LITERAL_STRING(value, "-moz-editor-invert-value");
              SetInlinePropertyOnTextNode(*startNode->GetAsText(),
                                          range->StartOffset(),
                                          range->EndOffset(), *aProperty,
                                          aAttribute, value);
            }
          }
        }
      } else {
        // Not the easy case.  Range not contained in single text node.
        nsCOMPtr<nsIContentIterator> iter = NS_NewContentSubtreeIterator();

        nsTArray<nsCOMPtr<nsINode>> arrayOfNodes;

        // Iterate range and build up array
        for (iter->Init(range); !iter->IsDone(); iter->Next()) {
          nsCOMPtr<nsINode> node = iter->GetCurrentNode();
          NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

          if (IsEditable(node)) {
            arrayOfNodes.AppendElement(node);
          }
        }

        // Loop through the list, remove the property on each node
        for (auto& node : arrayOfNodes) {
          res = RemoveStyleInside(GetAsDOMNode(node), aProperty, aAttribute);
          NS_ENSURE_SUCCESS(res, res);
          if (IsCSSEnabled() &&
              mHTMLCSSUtils->IsCSSEditableProperty(node, aProperty,
                                                   aAttribute) &&
              mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(node,
                  aProperty, aAttribute, EmptyString(),
                  nsHTMLCSSUtils::eComputed) &&
              // startNode's computed style indicates the CSS equivalence to
              // the HTML style to remove is applied; but we found no element
              // in the ancestors of startNode carrying specified styles;
              // assume it comes from a rule and let's try to insert a span
              // "inverting" the style
              mHTMLCSSUtils->IsCSSInvertible(*aProperty, aAttribute)) {
            NS_NAMED_LITERAL_STRING(value, "-moz-editor-invert-value");
            SetInlinePropertyOnNode(node->AsContent(), aProperty,
                                    aAttribute, &value);
          }
        }
      }
    }
  }
  if (!cancel) {
    // Post-process
    res = mRules->DidDoAction(selection, &ruleInfo, res);
    NS_ENSURE_SUCCESS(res, res);
  }
  return NS_OK;
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
nsHTMLEditor::RelativeFontChange( int32_t aSizeChange)
{
  // Can only change font size by + or - 1
  if ( !( (aSizeChange==1) || (aSizeChange==-1) ) )
    return NS_ERROR_ILLEGAL_VALUE;
  
  ForceCompositionEnd();

  // Get the selection 
  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);
  // Is the selection collapsed?
  // if it's collapsed set typing state
  if (selection->Collapsed()) {
    nsCOMPtr<nsIAtom> atom;
    if (aSizeChange == 1) {
      atom = nsGkAtoms::big;
    } else {
      atom = nsGkAtoms::small;
    }

    // Let's see in what kind of element the selection is
    int32_t offset;
    nsCOMPtr<nsINode> selectedNode;
    GetStartNodeAndOffset(selection, getter_AddRefs(selectedNode), &offset);
    if (selectedNode && IsTextNode(selectedNode)) {
      selectedNode = selectedNode->GetParentNode();
    }
    NS_ENSURE_TRUE(selectedNode, NS_OK);
    if (!CanContainTag(*selectedNode, *atom)) {
      return NS_OK;
    }

    // manipulating text attributes on a collapsed selection only sets state for the next text insertion
    mTypeInState->SetProp(atom, EmptyString(), EmptyString());
    return NS_OK;
  }
  
  // wrap with txn batching, rules sniffing, and selection preservation code
  nsAutoEditBatch batchIt(this);
  nsAutoRules beginRulesSniffing(this, EditAction::setTextProperty, nsIEditor::eNext);
  nsAutoSelectionReset selectionResetter(selection, this);
  nsAutoTxnsConserveSelection dontSpazMySelection(this);

  // loop thru the ranges in the selection
  uint32_t rangeCount = selection->RangeCount();
  for (uint32_t rangeIdx = 0; rangeIdx < rangeCount; ++rangeIdx) {
    nsRefPtr<nsRange> range = selection->GetRangeAt(rangeIdx);

    // adjust range to include any ancestors who's children are entirely selected
    nsresult res = PromoteInlineRange(range);
    NS_ENSURE_SUCCESS(res, res);
    
    // check for easy case: both range endpoints in same text node
    nsCOMPtr<nsIDOMNode> startNode, endNode;
    res = range->GetStartContainer(getter_AddRefs(startNode));
    NS_ENSURE_SUCCESS(res, res);
    res = range->GetEndContainer(getter_AddRefs(endNode));
    NS_ENSURE_SUCCESS(res, res);
    if ((startNode == endNode) && IsTextNode(startNode))
    {
      int32_t startOffset, endOffset;
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
        int32_t listCount = arrayOfNodes.Count();
        for (int32_t j = 0; j < listCount; ++j) {
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
        int32_t startOffset;
        uint32_t textLen;
        range->GetStartOffset(&startOffset);
        nodeAsText->GetLength(&textLen);
        res = RelativeFontChangeOnTextNode(aSizeChange, nodeAsText, startOffset, textLen);
        NS_ENSURE_SUCCESS(res, res);
      }
      if (IsTextNode(endNode) && IsEditable(endNode))
      {
        nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(endNode);
        int32_t endOffset;
        range->GetEndOffset(&endOffset);
        res = RelativeFontChangeOnTextNode(aSizeChange, nodeAsText, 0, endOffset);
        NS_ENSURE_SUCCESS(res, res);
      }
    }
  }
  
  return NS_OK;  
}

nsresult
nsHTMLEditor::RelativeFontChangeOnTextNode( int32_t aSizeChange, 
                                            nsIDOMCharacterData *aTextNode, 
                                            int32_t aStartOffset,
                                            int32_t aEndOffset)
{
  // Can only change font size by + or - 1
  if ( !( (aSizeChange==1) || (aSizeChange==-1) ) )
    return NS_ERROR_ILLEGAL_VALUE;
  nsCOMPtr<nsIContent> textNode = do_QueryInterface(aTextNode);
  NS_ENSURE_TRUE(textNode, NS_ERROR_NULL_POINTER);
  
  // don't need to do anything if no characters actually selected
  if (aStartOffset == aEndOffset) return NS_OK;
  
  if (!textNode->GetParentNode() ||
      !CanContainTag(*textNode->GetParentNode(), *nsGkAtoms::big)) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMNode> tmp;
  nsCOMPtr<nsIContent> node = do_QueryInterface(aTextNode);
  NS_ENSURE_STATE(node);

  // do we need to split the text node?
  uint32_t textLen;
  aTextNode->GetLength(&textLen);
  
  // -1 is a magic value meaning to the end of node
  if (aEndOffset == -1) aEndOffset = textLen;
  
  nsresult res = NS_OK;
  if ( (uint32_t)aEndOffset != textLen )
  {
    // we need to split off back of text node
    res = SplitNode(GetAsDOMNode(node), aEndOffset, getter_AddRefs(tmp));
    NS_ENSURE_SUCCESS(res, res);
    // remember left node
    node = do_QueryInterface(tmp);
  }
  if ( aStartOffset )
  {
    // we need to split off front of text node
    res = SplitNode(GetAsDOMNode(node), aStartOffset, getter_AddRefs(tmp));
    NS_ENSURE_SUCCESS(res, res);
  }

  // look for siblings that are correct type of node
  nsIAtom* nodeType = aSizeChange == 1 ? nsGkAtoms::big : nsGkAtoms::small;
  nsCOMPtr<nsIContent> sibling = GetPriorHTMLSibling(node);
  if (sibling && sibling->IsHTMLElement(nodeType)) {
    // previous sib is already right kind of inline node; slide this over into it
    res = MoveNode(node, sibling, -1);
    return res;
  }
  sibling = GetNextHTMLSibling(node);
  if (sibling && sibling->IsHTMLElement(nodeType)) {
    // following sib is already right kind of inline node; slide this over into it
    res = MoveNode(node, sibling, 0);
    return res;
  }
  
  // else reparent the node inside font node with appropriate relative size
  nsCOMPtr<Element> newElement = InsertContainerAbove(node, nodeType);
  NS_ENSURE_STATE(newElement);

  return NS_OK;
}


nsresult
nsHTMLEditor::RelativeFontChangeHelper(int32_t aSizeChange, nsINode* aNode)
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
  if (aNode->IsHTMLElement(nsGkAtoms::font) &&
      aNode->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::size)) {
    // Cycle through children and adjust relative font size.
    for (uint32_t i = aNode->GetChildCount(); i--; ) {
      nsresult rv = RelativeFontChangeOnNode(aSizeChange, aNode->GetChildAt(i));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // RelativeFontChangeOnNode already calls us recursively,
    // so we don't need to check our children again.
    return NS_OK;
  }

  // Otherwise cycle through the children.
  for (uint32_t i = aNode->GetChildCount(); i--; ) {
    nsresult rv = RelativeFontChangeHelper(aSizeChange, aNode->GetChildAt(i));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


nsresult
nsHTMLEditor::RelativeFontChangeOnNode(int32_t aSizeChange, nsIContent* aNode)
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
  if ((aSizeChange == 1 && aNode->IsHTMLElement(nsGkAtoms::small)) ||
       (aSizeChange == -1 && aNode->IsHTMLElement(nsGkAtoms::big))) {
    // first populate any nested font tags that have the size attr set
    nsresult rv = RelativeFontChangeHelper(aSizeChange, aNode);
    NS_ENSURE_SUCCESS(rv, rv);
    // in that case, just remove this node and pull up the children
    return RemoveContainer(aNode);
  }

  // can it be put inside a "big" or "small"?
  if (TagCanContain(*atom, *aNode)) {
    // first populate any nested font tags that have the size attr set
    nsresult rv = RelativeFontChangeHelper(aSizeChange, aNode);
    NS_ENSURE_SUCCESS(rv, rv);

    // ok, chuck it in.
    // first look at siblings of aNode for matching bigs or smalls.
    // if we find one, move aNode into it.
    nsIContent* sibling = GetPriorHTMLSibling(aNode);
    if (sibling && sibling->IsHTMLElement(atom)) {
      // previous sib is already right kind of inline node; slide this over into it
      return MoveNode(aNode, sibling, -1);
    }

    sibling = GetNextHTMLSibling(aNode);
    if (sibling && sibling->IsHTMLElement(atom)) {
      // following sib is already right kind of inline node; slide this over into it
      return MoveNode(aNode, sibling, 0);
    }

    // else insert it above aNode
    nsCOMPtr<Element> newElement = InsertContainerAbove(aNode, atom);
    NS_ENSURE_STATE(newElement);

    return NS_OK;
  }

  // none of the above?  then cycle through the children.
  // MOOSE: we should group the children together if possible
  // into a single "big" or "small".  For the moment they are
  // each getting their own.  
  for (uint32_t i = aNode->GetChildCount(); i--; ) {
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
  res = GetInlinePropertyBase(*nsGkAtoms::font, &attr, nullptr, &first, &any,
                              &all, &outFace);
  NS_ENSURE_SUCCESS(res, res);
  if (any && !all) return res; // mixed
  if (all)
  {
    *aMixed = false;
    return res;
  }
  
  // if there is no font face, check for tt
  res = GetInlinePropertyBase(*nsGkAtoms::tt, nullptr, nullptr, &first, &any,
                              &all,nullptr);
  NS_ENSURE_SUCCESS(res, res);
  if (any && !all) return res; // mixed
  if (all)
  {
    *aMixed = false;
    outFace.AssignLiteral("tt");
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
  
  res = GetInlinePropertyBase(*nsGkAtoms::font, &colorStr, nullptr, &first,
                              &any, &all, &aOutColor);
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
  if ((!element->IsHTMLElement(nsGkAtoms::span) &&
       !element->IsHTMLElement(nsGkAtoms::font)) ||
      HasStyleOrIdOrClass(element)) {
    return NS_OK;
  }

  return RemoveContainer(element);
}
