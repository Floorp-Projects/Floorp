/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/HTMLEditor.h"

#include "HTMLEditUtils.h"
#include "TextEditUtils.h"
#include "TypeInState.h"
#include "mozilla/Assertions.h"
#include "mozilla/EditorUtils.h"
#include "mozilla/SelectionState.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/Element.h"
#include "mozilla/mozalloc.h"
#include "nsAString.h"
#include "nsAttrName.h"
#include "nsCOMPtr.h"
#include "nsCaseTreatment.h"
#include "nsComponentManagerUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsIAtom.h"
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsIDOMElement.h"
#include "nsIEditor.h"
#include "nsIEditorIMESupport.h"
#include "nsIEditRules.h"
#include "nsNameSpaceManager.h"
#include "nsINode.h"
#include "nsISupportsImpl.h"
#include "nsLiteralString.h"
#include "nsRange.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "nsUnicharUtils.h"
#include "nscore.h"

class nsISupports;

namespace mozilla {

using namespace dom;

static bool
IsEmptyTextNode(HTMLEditor* aThis, nsINode* aNode)
{
  bool isEmptyTextNode = false;
  return EditorBase::IsTextNode(aNode) &&
         NS_SUCCEEDED(aThis->IsEmptyNode(aNode, &isEmptyTextNode)) &&
         isEmptyTextNode;
}

NS_IMETHODIMP
HTMLEditor::AddDefaultProperty(nsIAtom* aProperty,
                               const nsAString& aAttribute,
                               const nsAString& aValue)
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

NS_IMETHODIMP
HTMLEditor::RemoveDefaultProperty(nsIAtom* aProperty,
                                  const nsAString& aAttribute,
                                  const nsAString& aValue)
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

NS_IMETHODIMP
HTMLEditor::RemoveAllDefaultProperties()
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
HTMLEditor::SetInlineProperty(nsIAtom* aProperty,
                              const nsAString& aAttribute,
                              const nsAString& aValue)
{
  NS_ENSURE_TRUE(aProperty, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(mRules, NS_ERROR_NOT_INITIALIZED);
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);
  ForceCompositionEnd();

  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  if (selection->Collapsed()) {
    // Manipulating text attributes on a collapsed selection only sets state
    // for the next text insertion
    mTypeInState->SetProp(aProperty, aAttribute, aValue);
    return NS_OK;
  }

  AutoEditBatch batchIt(this);
  AutoRules beginRulesSniffing(this, EditAction::insertElement,
                               nsIEditor::eNext);
  AutoSelectionRestorer selectionRestorer(selection, this);
  AutoTransactionsConserveSelection dontSpazMySelection(this);

  bool cancel, handled;
  TextRulesInfo ruleInfo(EditAction::setTextProperty);
  // Protect the edit rules object from dying
  nsresult res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  NS_ENSURE_SUCCESS(res, res);
  if (!cancel && !handled) {
    // Loop through the ranges in the selection
    uint32_t rangeCount = selection->RangeCount();
    for (uint32_t rangeIdx = 0; rangeIdx < rangeCount; rangeIdx++) {
      RefPtr<nsRange> range = selection->GetRangeAt(rangeIdx);

      // Adjust range to include any ancestors whose children are entirely
      // selected
      res = PromoteInlineRange(*range);
      NS_ENSURE_SUCCESS(res, res);

      // Check for easy case: both range endpoints in same text node
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

      OwningNonNull<nsIContentIterator> iter = NS_NewContentSubtreeIterator();

      nsTArray<OwningNonNull<nsIContent>> arrayOfNodes;

      // Iterate range and build up array
      res = iter->Init(range);
      // Init returns an error if there are no nodes in range.  This can easily
      // happen with the subtree iterator if the selection doesn't contain any
      // *whole* nodes.
      if (NS_SUCCEEDED(res)) {
        for (; !iter->IsDone(); iter->Next()) {
          OwningNonNull<nsINode> node = *iter->GetCurrentNode();

          if (node->IsContent() && IsEditable(node)) {
            arrayOfNodes.AppendElement(*node->AsContent());
          }
        }
      }
      // First check the start parent of the range to see if it needs to be
      // separately handled (it does if it's a text node, due to how the
      // subtree iterator works - it will not have reported it).
      if (startNode && startNode->GetAsText() && IsEditable(startNode)) {
        res = SetInlinePropertyOnTextNode(*startNode->GetAsText(),
                                          range->StartOffset(),
                                          startNode->Length(), *aProperty,
                                          &aAttribute, aValue);
        NS_ENSURE_SUCCESS(res, res);
      }

      // Then loop through the list, set the property on each node
      for (auto& node : arrayOfNodes) {
        res = SetInlinePropertyOnNode(*node, *aProperty, &aAttribute, aValue);
        NS_ENSURE_SUCCESS(res, res);
      }

      // Last check the end parent of the range to see if it needs to be
      // separately handled (it does if it's a text node, due to how the
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
    // Post-process
    return mRules->DidDoAction(selection, &ruleInfo, res);
  }
  return NS_OK;
}

// Helper function for SetInlinePropertyOn*: is aNode a simple old <b>, <font>,
// <span style="">, etc. that we can reuse instead of creating a new one?
bool
HTMLEditor::IsSimpleModifiableNode(nsIContent* aContent,
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
    nsCOMPtr<nsIAtom> atom = NS_Atomize(*aAttribute);
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
  if (!mCSSEditUtils->IsCSSEditableProperty(element, aProperty, aAttribute) ||
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
  mCSSEditUtils->SetCSSEquivalentToHTMLStyle(newSpan, aProperty,
                                             aAttribute, aValue,
                                             /*suppress transaction*/ true);

  return mCSSEditUtils->ElementsSameStyle(newSpan, element);
}

nsresult
HTMLEditor::SetInlinePropertyOnTextNode(Text& aText,
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
  if (mCSSEditUtils->IsCSSEditableProperty(&aText, &aProperty, aAttribute)) {
    // The HTML styles defined by aProperty/aAttribute have a CSS equivalence
    // for node; let's check if it carries those CSS styles
    if (mCSSEditUtils->IsCSSEquivalentToHTMLInlineStyleSet(&aText, &aProperty,
          aAttribute, aValue, CSSEditUtils::eComputed)) {
      return NS_OK;
    }
  } else if (IsTextPropertySetByContent(&aText, &aProperty, aAttribute,
                                        &aValue)) {
    return NS_OK;
  }

  // Do we need to split the text node?
  ErrorResult rv;
  RefPtr<Text> text = &aText;
  if (uint32_t(aEndOffset) != aText.Length()) {
    // We need to split off back of text node
    text = SplitNode(aText, aEndOffset, rv)->GetAsText();
    NS_ENSURE_TRUE(!rv.Failed(), rv.StealNSResult());
  }

  if (aStartOffset) {
    // We need to split off front of text node
    SplitNode(*text, aStartOffset, rv);
    NS_ENSURE_TRUE(!rv.Failed(), rv.StealNSResult());
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
  return SetInlinePropertyOnNode(*text, aProperty, aAttribute, aValue);
}

nsresult
HTMLEditor::SetInlinePropertyOnNodeImpl(nsIContent& aNode,
                                        nsIAtom& aProperty,
                                        const nsAString* aAttribute,
                                        const nsAString& aValue)
{
  nsCOMPtr<nsIAtom> attrAtom = aAttribute ? NS_Atomize(*aAttribute) : nullptr;

  // If this is an element that can't be contained in a span, we have to
  // recurse to its children.
  if (!TagCanContain(*nsGkAtoms::span, aNode)) {
    if (aNode.HasChildren()) {
      nsTArray<OwningNonNull<nsIContent>> arrayOfNodes;

      // Populate the list.
      for (nsCOMPtr<nsIContent> child = aNode.GetFirstChild();
           child;
           child = child->GetNextSibling()) {
        if (IsEditable(child) && !IsEmptyTextNode(this, child)) {
          arrayOfNodes.AppendElement(*child);
        }
      }

      // Then loop through the list, set the property on each node.
      for (auto& node : arrayOfNodes) {
        nsresult rv = SetInlinePropertyOnNode(node, aProperty, aAttribute,
                                              aValue);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
    return NS_OK;
  }

  // First check if there's an adjacent sibling we can put our node into.
  nsresult res;
  nsCOMPtr<nsIContent> previousSibling = GetPriorHTMLSibling(&aNode);
  nsCOMPtr<nsIContent> nextSibling = GetNextHTMLSibling(&aNode);
  if (IsSimpleModifiableNode(previousSibling, &aProperty, aAttribute, &aValue)) {
    res = MoveNode(&aNode, previousSibling, -1);
    NS_ENSURE_SUCCESS(res, res);
    if (IsSimpleModifiableNode(nextSibling, &aProperty, aAttribute, &aValue)) {
      res = JoinNodes(*previousSibling, *nextSibling);
      NS_ENSURE_SUCCESS(res, res);
    }
    return NS_OK;
  }
  if (IsSimpleModifiableNode(nextSibling, &aProperty, aAttribute, &aValue)) {
    res = MoveNode(&aNode, nextSibling, 0);
    NS_ENSURE_SUCCESS(res, res);
    return NS_OK;
  }

  // Don't need to do anything if property already set on node
  if (mCSSEditUtils->IsCSSEditableProperty(&aNode, &aProperty, aAttribute)) {
    if (mCSSEditUtils->IsCSSEquivalentToHTMLInlineStyleSet(
          &aNode, &aProperty, aAttribute, aValue, CSSEditUtils::eComputed)) {
      return NS_OK;
    }
  } else if (IsTextPropertySetByContent(&aNode, &aProperty,
                                        aAttribute, &aValue)) {
    return NS_OK;
  }

  bool useCSS = (IsCSSEnabled() &&
                 mCSSEditUtils->IsCSSEditableProperty(&aNode, &aProperty,
                                                      aAttribute)) ||
                // bgcolor is always done using CSS
                aAttribute->EqualsLiteral("bgcolor");

  if (useCSS) {
    nsCOMPtr<dom::Element> tmp;
    // We only add style="" to <span>s with no attributes (bug 746515).  If we
    // don't have one, we need to make one.
    if (aNode.IsHTMLElement(nsGkAtoms::span) &&
        !aNode.AsElement()->GetAttrCount()) {
      tmp = aNode.AsElement();
    } else {
      tmp = InsertContainerAbove(&aNode, nsGkAtoms::span);
      NS_ENSURE_STATE(tmp);
    }

    // Add the CSS styles corresponding to the HTML style request
    int32_t count;
    res = mCSSEditUtils->SetCSSEquivalentToHTMLStyle(tmp->AsDOMNode(),
                                                     &aProperty, aAttribute,
                                                     &aValue, &count, false);
    NS_ENSURE_SUCCESS(res, res);
    return NS_OK;
  }

  // is it already the right kind of node, but with wrong attribute?
  if (aNode.IsHTMLElement(&aProperty)) {
    // Just set the attribute on it.
    nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(&aNode);
    return SetAttribute(elem, *aAttribute, aValue);
  }

  // ok, chuck it in its very own container
  nsCOMPtr<Element> tmp = InsertContainerAbove(&aNode, &aProperty, attrAtom,
                                               &aValue);
  NS_ENSURE_STATE(tmp);

  return NS_OK;
}

nsresult
HTMLEditor::SetInlinePropertyOnNode(nsIContent& aNode,
                                    nsIAtom& aProperty,
                                    const nsAString* aAttribute,
                                    const nsAString& aValue)
{
  nsCOMPtr<nsIContent> previousSibling = aNode.GetPreviousSibling(),
                       nextSibling = aNode.GetNextSibling();
  NS_ENSURE_STATE(aNode.GetParentNode());
  OwningNonNull<nsINode> parent = *aNode.GetParentNode();

  nsresult res = RemoveStyleInside(aNode, &aProperty, aAttribute);
  NS_ENSURE_SUCCESS(res, res);

  if (aNode.GetParentNode()) {
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
  nsTArray<OwningNonNull<nsIContent>> nodesToSet;
  nsCOMPtr<nsIContent> cur = previousSibling
    ? previousSibling->GetNextSibling() : parent->GetFirstChild();
  for (; cur && cur != nextSibling; cur = cur->GetNextSibling()) {
    if (IsEditable(cur)) {
      nodesToSet.AppendElement(*cur);
    }
  }

  for (auto& node : nodesToSet) {
    res = SetInlinePropertyOnNodeImpl(node, aProperty, aAttribute, aValue);
    NS_ENSURE_SUCCESS(res, res);
  }

  return NS_OK;
}

nsresult
HTMLEditor::SplitStyleAboveRange(nsRange* inRange,
                                 nsIAtom* aProperty,
                                 const nsAString* aAttribute)
{
  NS_ENSURE_TRUE(inRange, NS_ERROR_NULL_POINTER);
  nsresult res;

  nsCOMPtr<nsINode> startNode = inRange->GetStartParent();
  int32_t startOffset = inRange->StartOffset();
  nsCOMPtr<nsINode> endNode = inRange->GetEndParent();
  int32_t endOffset = inRange->EndOffset();

  nsCOMPtr<nsINode> origStartNode = startNode;

  // split any matching style nodes above the start of range
  {
    AutoTrackDOMPoint tracker(mRangeUpdater, address_of(endNode), &endOffset);
    res = SplitStyleAbovePoint(address_of(startNode), &startOffset, aProperty,
                               aAttribute);
    NS_ENSURE_SUCCESS(res, res);
  }

  // second verse, same as the first...
  res = SplitStyleAbovePoint(address_of(endNode), &endOffset, aProperty,
                             aAttribute);
  NS_ENSURE_SUCCESS(res, res);

  // reset the range
  res = inRange->SetStart(startNode, startOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = inRange->SetEnd(endNode, endOffset);
  return res;
}

nsresult
HTMLEditor::SplitStyleAbovePoint(nsCOMPtr<nsINode>* aNode,
                                 int32_t* aOffset,
                                 // null here means we split all properties
                                 nsIAtom* aProperty,
                                 const nsAString* aAttribute,
                                 nsIContent** aOutLeftNode,
                                 nsIContent** aOutRightNode)
{
  NS_ENSURE_TRUE(aNode && *aNode && aOffset, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE((*aNode)->IsContent(), NS_OK);

  // Split any matching style nodes above the node/offset
  OwningNonNull<nsIContent> node = *(*aNode)->AsContent();

  bool useCSS = IsCSSEnabled();

  bool isSet;
  while (!IsBlockNode(node) && node->GetParent() &&
         IsEditable(node->GetParent())) {
    isSet = false;
    if (useCSS && mCSSEditUtils->IsCSSEditableProperty(node, aProperty,
                                                       aAttribute)) {
      // The HTML style defined by aProperty/aAttribute has a CSS equivalence
      // in this implementation for the node; let's check if it carries those
      // CSS styles
      nsAutoString firstValue;
      mCSSEditUtils->IsCSSEquivalentToHTMLInlineStyleSet(GetAsDOMNode(node),
        aProperty, aAttribute, isSet, firstValue, CSSEditUtils::eSpecified);
    }
    if (// node is the correct inline prop
        (aProperty && node->IsHTMLElement(aProperty)) ||
        // node is href - test if really <a href=...
        (aProperty == nsGkAtoms::href && HTMLEditUtils::IsLink(node)) ||
        // or node is any prop, and we asked to split them all
        (!aProperty && NodeIsProperty(node)) ||
        // or the style is specified in the style attribute
        isSet) {
      // Found a style node we need to split
      int32_t offset = SplitNodeDeep(*node, *(*aNode)->AsContent(), *aOffset,
                                     EmptyContainers::yes, aOutLeftNode,
                                     aOutRightNode);
      NS_ENSURE_TRUE(offset != -1, NS_ERROR_FAILURE);
      // reset startNode/startOffset
      *aNode = node->GetParent();
      *aOffset = offset;
    }
    node = node->GetParent();
  }

  return NS_OK;
}

nsresult
HTMLEditor::ClearStyle(nsCOMPtr<nsINode>* aNode,
                       int32_t* aOffset,
                       nsIAtom* aProperty,
                       const nsAString* aAttribute)
{
  nsCOMPtr<nsIContent> leftNode, rightNode;
  nsresult res = SplitStyleAbovePoint(aNode, aOffset, aProperty,
                                      aAttribute, getter_AddRefs(leftNode),
                                      getter_AddRefs(rightNode));
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
    nsCOMPtr<nsINode> secondSplitParent = GetLeftmostChild(rightNode);
    // don't try to split non-containers (br's, images, hr's, etc.)
    if (!secondSplitParent) {
      secondSplitParent = rightNode;
    }
    nsCOMPtr<Element> savedBR;
    if (!IsContainer(secondSplitParent)) {
      if (TextEditUtils::IsBreak(secondSplitParent)) {
        savedBR = do_QueryInterface(secondSplitParent);
        NS_ENSURE_STATE(savedBR);
      }

      secondSplitParent = secondSplitParent->GetParentNode();
    }
    *aOffset = 0;
    res = SplitStyleAbovePoint(address_of(secondSplitParent),
                               aOffset, aProperty, aAttribute,
                               getter_AddRefs(leftNode),
                               getter_AddRefs(rightNode));
    NS_ENSURE_SUCCESS(res, res);
    // should be impossible to not get a new leftnode here
    nsCOMPtr<nsINode> newSelParent = GetLeftmostChild(leftNode);
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
    int32_t newSelOffset = 0;
    {
      // Track the point at the new hierarchy.  This is so we can know where
      // to put the selection after we call RemoveStyleInside().
      // RemoveStyleInside() could remove any and all of those nodes, so I
      // have to use the range tracking system to find the right spot to put
      // selection.
      AutoTrackDOMPoint tracker(mRangeUpdater,
                                address_of(newSelParent), &newSelOffset);
      res = RemoveStyleInside(*leftNode, aProperty, aAttribute);
      NS_ENSURE_SUCCESS(res, res);
    }
    // reset our node offset values to the resulting new sel point
    *aNode = newSelParent;
    *aOffset = newSelOffset;
  }

  return NS_OK;
}

bool
HTMLEditor::NodeIsProperty(nsINode& aNode)
{
  return IsContainer(&aNode) && IsEditable(&aNode) && !IsBlockNode(&aNode) &&
         !aNode.IsHTMLElement(nsGkAtoms::a);
}

nsresult
HTMLEditor::ApplyDefaultProperties()
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

nsresult
HTMLEditor::RemoveStyleInside(nsIContent& aNode,
                              nsIAtom* aProperty,
                              const nsAString* aAttribute,
                              const bool aChildrenOnly /* = false */)
{
  if (aNode.GetAsText()) {
    return NS_OK;
  }

  // first process the children
  RefPtr<nsIContent> child = aNode.GetFirstChild();
  while (child) {
    // cache next sibling since we might remove child
    nsCOMPtr<nsIContent> next = child->GetNextSibling();
    nsresult res = RemoveStyleInside(*child, aProperty, aAttribute);
    NS_ENSURE_SUCCESS(res, res);
    child = next.forget();
  }

  // then process the node itself
  if (!aChildrenOnly &&
    (
      // node is prop we asked for
      (aProperty && aNode.NodeInfo()->NameAtom() == aProperty) ||
      // but check for link (<a href=...)
      (aProperty == nsGkAtoms::href && HTMLEditUtils::IsLink(&aNode)) ||
      // and for named anchors
      (aProperty == nsGkAtoms::name && HTMLEditUtils::IsNamedAnchor(&aNode)) ||
      // or node is any prop and we asked for that
      (!aProperty && NodeIsProperty(aNode))
    )
  ) {
    nsresult res;
    // if we weren't passed an attribute, then we want to
    // remove any matching inlinestyles entirely
    if (!aAttribute || aAttribute->IsEmpty()) {
      NS_NAMED_LITERAL_STRING(styleAttr, "style");
      NS_NAMED_LITERAL_STRING(classAttr, "class");

      bool hasStyleAttr = aNode.HasAttr(kNameSpaceID_None, nsGkAtoms::style);
      bool hasClassAttr = aNode.HasAttr(kNameSpaceID_None, nsGkAtoms::_class);
      if (aProperty && (hasStyleAttr || hasClassAttr)) {
        // aNode carries inline styles or a class attribute so we can't
        // just remove the element... We need to create above the element
        // a span that will carry those styles or class, then we can delete
        // the node.
        nsCOMPtr<Element> spanNode =
          InsertContainerAbove(&aNode, nsGkAtoms::span);
        NS_ENSURE_STATE(spanNode);
        res = CloneAttribute(styleAttr, spanNode->AsDOMNode(), aNode.AsDOMNode());
        NS_ENSURE_SUCCESS(res, res);
        res = CloneAttribute(classAttr, spanNode->AsDOMNode(), aNode.AsDOMNode());
        NS_ENSURE_SUCCESS(res, res);
      }
      res = RemoveContainer(&aNode);
      NS_ENSURE_SUCCESS(res, res);
    } else {
      // otherwise we just want to eliminate the attribute
      nsCOMPtr<nsIAtom> attribute = NS_Atomize(*aAttribute);
      if (aNode.HasAttr(kNameSpaceID_None, attribute)) {
        // if this matching attribute is the ONLY one on the node,
        // then remove the whole node.  Otherwise just nix the attribute.
        if (IsOnlyAttribute(&aNode, *aAttribute)) {
          res = RemoveContainer(&aNode);
        } else {
          nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(&aNode);
          NS_ENSURE_TRUE(elem, NS_ERROR_NULL_POINTER);
          res = RemoveAttribute(elem, *aAttribute);
        }
        NS_ENSURE_SUCCESS(res, res);
      }
    }
  }

  if (!aChildrenOnly &&
      mCSSEditUtils->IsCSSEditableProperty(&aNode, aProperty, aAttribute)) {
    // the HTML style defined by aProperty/aAttribute has a CSS equivalence in
    // this implementation for the node aNode; let's check if it carries those
    // css styles
    nsAutoString propertyValue;
    bool isSet = mCSSEditUtils->IsCSSEquivalentToHTMLInlineStyleSet(&aNode,
      aProperty, aAttribute, propertyValue, CSSEditUtils::eSpecified);
    if (isSet && aNode.IsElement()) {
      // yes, tmp has the corresponding css declarations in its style attribute
      // let's remove them
      mCSSEditUtils->RemoveCSSEquivalentToHTMLStyle(aNode.AsElement(),
                                                    aProperty,
                                                    aAttribute,
                                                    &propertyValue,
                                                    false);
      // remove the node if it is a span or font, if its style attribute is
      // empty or absent, and if it does not have a class nor an id
      RemoveElementIfNoStyleOrIdOrClass(*aNode.AsElement());
    }
  }

  if (!aChildrenOnly &&
    (
      // Or node is big or small and we are setting font size
      aProperty == nsGkAtoms::font &&
      (aNode.IsHTMLElement(nsGkAtoms::big) || aNode.IsHTMLElement(nsGkAtoms::small)) &&
      (aAttribute && aAttribute->LowerCaseEqualsLiteral("size"))
    )
  ) {
    // if we are setting font size, remove any nested bigs and smalls
    return RemoveContainer(&aNode);
  }
  return NS_OK;
}

bool
HTMLEditor::IsOnlyAttribute(const nsIContent* aContent,
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

nsresult
HTMLEditor::PromoteRangeIfStartsOrEndsInNamedAnchor(nsRange& aRange)
{
  // We assume that <a> is not nested.
  nsCOMPtr<nsINode> startNode = aRange.GetStartParent();
  int32_t startOffset = aRange.StartOffset();
  nsCOMPtr<nsINode> endNode = aRange.GetEndParent();
  int32_t endOffset = aRange.EndOffset();

  nsCOMPtr<nsINode> parent = startNode;

  while (parent && !parent->IsHTMLElement(nsGkAtoms::body) &&
         !HTMLEditUtils::IsNamedAnchor(parent)) {
    parent = parent->GetParentNode();
  }
  NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);

  if (HTMLEditUtils::IsNamedAnchor(parent)) {
    startNode = parent->GetParentNode();
    startOffset = startNode ? startNode->IndexOf(parent) : -1;
  }

  parent = endNode;
  while (parent && !parent->IsHTMLElement(nsGkAtoms::body) &&
         !HTMLEditUtils::IsNamedAnchor(parent)) {
    parent = parent->GetParentNode();
  }
  NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);

  if (HTMLEditUtils::IsNamedAnchor(parent)) {
    endNode = parent->GetParentNode();
    endOffset = endNode ? endNode->IndexOf(parent) + 1 : 0;
  }

  nsresult res = aRange.SetStart(startNode, startOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = aRange.SetEnd(endNode, endOffset);
  NS_ENSURE_SUCCESS(res, res);

  return NS_OK;
}

nsresult
HTMLEditor::PromoteInlineRange(nsRange& aRange)
{
  nsCOMPtr<nsINode> startNode = aRange.GetStartParent();
  int32_t startOffset = aRange.StartOffset();
  nsCOMPtr<nsINode> endNode = aRange.GetEndParent();
  int32_t endOffset = aRange.EndOffset();

  while (startNode && !startNode->IsHTMLElement(nsGkAtoms::body) &&
         IsEditable(startNode) && IsAtFrontOfNode(*startNode, startOffset)) {
    nsCOMPtr<nsINode> parent = startNode->GetParentNode();
    NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);
    startOffset = parent->IndexOf(startNode);
    startNode = parent;
  }

  while (endNode && !endNode->IsHTMLElement(nsGkAtoms::body) &&
         IsEditable(endNode) && IsAtEndOfNode(*endNode, endOffset)) {
    nsCOMPtr<nsINode> parent = endNode->GetParentNode();
    NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);
    // We are AFTER this node
    endOffset = 1 + parent->IndexOf(endNode);
    endNode = parent;
  }

  nsresult res = aRange.SetStart(startNode, startOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = aRange.SetEnd(endNode, endOffset);
  NS_ENSURE_SUCCESS(res, res);

  return NS_OK;
}

bool
HTMLEditor::IsAtFrontOfNode(nsINode& aNode,
                            int32_t aOffset)
{
  if (!aOffset) {
    return true;
  }

  if (IsTextNode(&aNode)) {
    return false;
  }

  nsCOMPtr<nsIContent> firstNode = GetFirstEditableChild(aNode);
  NS_ENSURE_TRUE(firstNode, true);
  if (aNode.IndexOf(firstNode) < aOffset) {
    return false;
  }
  return true;
}

bool
HTMLEditor::IsAtEndOfNode(nsINode& aNode,
                          int32_t aOffset)
{
  if (aOffset == (int32_t)aNode.Length()) {
    return true;
  }

  if (IsTextNode(&aNode)) {
    return false;
  }

  nsCOMPtr<nsIContent> lastNode = GetLastEditableChild(aNode);
  NS_ENSURE_TRUE(lastNode, true);
  if (aNode.IndexOf(lastNode) < aOffset) {
    return true;
  }
  return false;
}


nsresult
HTMLEditor::GetInlinePropertyBase(nsIAtom& aProperty,
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

  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  bool isCollapsed = selection->Collapsed();
  RefPtr<nsRange> range = selection->GetRangeAt(0);
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

      if (mCSSEditUtils->IsCSSEditableProperty(collapsedNode, &aProperty,
                                               aAttribute)) {
        if (aValue) {
          tOutString.Assign(*aValue);
        }
        *aFirst = *aAny = *aAll =
          mCSSEditUtils->IsCSSEquivalentToHTMLInlineStyleSet(collapsedNode,
              &aProperty, aAttribute, tOutString, CSSEditUtils::eComputed);
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
        if (mCSSEditUtils->IsCSSEditableProperty(content, &aProperty,
                                                 aAttribute)) {
          // The HTML styles defined by aProperty/aAttribute have a CSS
          // equivalence in this implementation for node; let's check if it
          // carries those CSS styles
          if (aValue) {
            firstValue.Assign(*aValue);
          }
          isSet = mCSSEditUtils->IsCSSEquivalentToHTMLInlineStyleSet(content,
              &aProperty, aAttribute, firstValue, CSSEditUtils::eComputed);
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
        if (mCSSEditUtils->IsCSSEditableProperty(content, &aProperty,
                                                 aAttribute)) {
          // The HTML styles defined by aProperty/aAttribute have a CSS
          // equivalence in this implementation for node; let's check if it
          // carries those CSS styles
          if (aValue) {
            theValue.Assign(*aValue);
          }
          isSet = mCSSEditUtils->IsCSSEquivalentToHTMLInlineStyleSet(content,
              &aProperty, aAttribute, theValue, CSSEditUtils::eComputed);
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

NS_IMETHODIMP
HTMLEditor::GetInlineProperty(nsIAtom* aProperty,
                              const nsAString& aAttribute,
                              const nsAString& aValue,
                              bool* aFirst,
                              bool* aAny,
                              bool* aAll)
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

NS_IMETHODIMP
HTMLEditor::GetInlinePropertyWithAttrValue(nsIAtom* aProperty,
                                           const nsAString& aAttribute,
                                           const nsAString& aValue,
                                           bool* aFirst,
                                           bool* aAny,
                                           bool* aAll,
                                           nsAString& outValue)
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

NS_IMETHODIMP
HTMLEditor::RemoveAllInlineProperties()
{
  AutoEditBatch batchIt(this);
  AutoRules beginRulesSniffing(this, EditAction::resetTextProperties,
                               nsIEditor::eNext);

  nsresult res = RemoveInlinePropertyImpl(nullptr, nullptr);
  NS_ENSURE_SUCCESS(res, res);
  return ApplyDefaultProperties();
}

NS_IMETHODIMP
HTMLEditor::RemoveInlineProperty(nsIAtom* aProperty,
                                 const nsAString& aAttribute)
{
  return RemoveInlinePropertyImpl(aProperty, &aAttribute);
}

nsresult
HTMLEditor::RemoveInlinePropertyImpl(nsIAtom* aProperty,
                                     const nsAString* aAttribute)
{
  MOZ_ASSERT_IF(aProperty, aAttribute);
  NS_ENSURE_TRUE(mRules, NS_ERROR_NOT_INITIALIZED);
  ForceCompositionEnd();

  RefPtr<Selection> selection = GetSelection();
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

  AutoEditBatch batchIt(this);
  AutoRules beginRulesSniffing(this, EditAction::removeTextProperty,
                               nsIEditor::eNext);
  AutoSelectionRestorer selectionRestorer(selection, this);
  AutoTransactionsConserveSelection dontSpazMySelection(this);

  bool cancel, handled;
  TextRulesInfo ruleInfo(EditAction::removeTextProperty);
  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);
  nsresult res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  NS_ENSURE_SUCCESS(res, res);
  if (!cancel && !handled) {
    // Loop through the ranges in the selection
    uint32_t rangeCount = selection->RangeCount();
    for (uint32_t rangeIdx = 0; rangeIdx < rangeCount; ++rangeIdx) {
      OwningNonNull<nsRange> range = *selection->GetRangeAt(rangeIdx);
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
            mCSSEditUtils->IsCSSEditableProperty(startNode, aProperty,
                                                 aAttribute)) {
          // The HTML style defined by aProperty/aAttribute has a CSS
          // equivalence in this implementation for startNode
          if (mCSSEditUtils->IsCSSEquivalentToHTMLInlineStyleSet(startNode,
                aProperty, aAttribute, EmptyString(),
                CSSEditUtils::eComputed)) {
            // startNode's computed style indicates the CSS equivalence to the
            // HTML style to remove is applied; but we found no element in the
            // ancestors of startNode carrying specified styles; assume it
            // comes from a rule and try to insert a span "inverting" the style
            if (mCSSEditUtils->IsCSSInvertible(*aProperty, aAttribute)) {
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

        nsTArray<OwningNonNull<nsIContent>> arrayOfNodes;

        // Iterate range and build up array
        for (iter->Init(range); !iter->IsDone(); iter->Next()) {
          nsCOMPtr<nsINode> node = iter->GetCurrentNode();
          NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

          if (IsEditable(node) && node->IsContent()) {
            arrayOfNodes.AppendElement(*node->AsContent());
          }
        }

        // Loop through the list, remove the property on each node
        for (auto& node : arrayOfNodes) {
          res = RemoveStyleInside(node, aProperty, aAttribute);
          NS_ENSURE_SUCCESS(res, res);
          if (IsCSSEnabled() &&
              mCSSEditUtils->IsCSSEditableProperty(node, aProperty,
                                                   aAttribute) &&
              mCSSEditUtils->IsCSSEquivalentToHTMLInlineStyleSet(node,
                  aProperty, aAttribute, EmptyString(),
                  CSSEditUtils::eComputed) &&
              // startNode's computed style indicates the CSS equivalence to
              // the HTML style to remove is applied; but we found no element
              // in the ancestors of startNode carrying specified styles;
              // assume it comes from a rule and let's try to insert a span
              // "inverting" the style
              mCSSEditUtils->IsCSSInvertible(*aProperty, aAttribute)) {
            NS_NAMED_LITERAL_STRING(value, "-moz-editor-invert-value");
            SetInlinePropertyOnNode(node, *aProperty, aAttribute, value);
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

NS_IMETHODIMP
HTMLEditor::IncreaseFontSize()
{
  return RelativeFontChange(FontSize::incr);
}

NS_IMETHODIMP
HTMLEditor::DecreaseFontSize()
{
  return RelativeFontChange(FontSize::decr);
}

nsresult
HTMLEditor::RelativeFontChange(FontSize aDir)
{
  ForceCompositionEnd();

  // Get the selection
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);
  // If selection is collapsed, set typing state
  if (selection->Collapsed()) {
    nsIAtom& atom = aDir == FontSize::incr ? *nsGkAtoms::big :
                                             *nsGkAtoms::small;

    // Let's see in what kind of element the selection is
    NS_ENSURE_TRUE(selection->RangeCount() &&
                   selection->GetRangeAt(0)->GetStartParent(), NS_OK);
    OwningNonNull<nsINode> selectedNode =
      *selection->GetRangeAt(0)->GetStartParent();
    if (IsTextNode(selectedNode)) {
      NS_ENSURE_TRUE(selectedNode->GetParentNode(), NS_OK);
      selectedNode = *selectedNode->GetParentNode();
    }
    if (!CanContainTag(selectedNode, atom)) {
      return NS_OK;
    }

    // Manipulating text attributes on a collapsed selection only sets state
    // for the next text insertion
    mTypeInState->SetProp(&atom, EmptyString(), EmptyString());
    return NS_OK;
  }

  // Wrap with txn batching, rules sniffing, and selection preservation code
  AutoEditBatch batchIt(this);
  AutoRules beginRulesSniffing(this, EditAction::setTextProperty,
                               nsIEditor::eNext);
  AutoSelectionRestorer selectionRestorer(selection, this);
  AutoTransactionsConserveSelection dontSpazMySelection(this);

  // Loop through the ranges in the selection
  uint32_t rangeCount = selection->RangeCount();
  for (uint32_t rangeIdx = 0; rangeIdx < rangeCount; ++rangeIdx) {
    RefPtr<nsRange> range = selection->GetRangeAt(rangeIdx);

    // Adjust range to include any ancestors with entirely selected children
    nsresult res = PromoteInlineRange(*range);
    NS_ENSURE_SUCCESS(res, res);

    // Check for easy case: both range endpoints in same text node
    nsCOMPtr<nsINode> startNode = range->GetStartParent();
    nsCOMPtr<nsINode> endNode = range->GetEndParent();
    if (startNode == endNode && IsTextNode(startNode)) {
      res = RelativeFontChangeOnTextNode(aDir, *startNode->GetAsText(),
                                         range->StartOffset(),
                                         range->EndOffset());
      NS_ENSURE_SUCCESS(res, res);
    } else {
      // Not the easy case.  Range not contained in single text node.  There
      // are up to three phases here.  There are all the nodes reported by the
      // subtree iterator to be processed.  And there are potentially a
      // starting textnode and an ending textnode which are only partially
      // contained by the range.

      // Let's handle the nodes reported by the iterator.  These nodes are
      // entirely contained in the selection range.  We build up a list of them
      // (since doing operations on the document during iteration would perturb
      // the iterator).

      OwningNonNull<nsIContentIterator> iter = NS_NewContentSubtreeIterator();

      // Iterate range and build up array
      res = iter->Init(range);
      if (NS_SUCCEEDED(res)) {
        nsTArray<OwningNonNull<nsIContent>> arrayOfNodes;
        for (; !iter->IsDone(); iter->Next()) {
          NS_ENSURE_TRUE(iter->GetCurrentNode()->IsContent(), NS_ERROR_FAILURE);
          OwningNonNull<nsIContent> node = *iter->GetCurrentNode()->AsContent();

          if (IsEditable(node)) {
            arrayOfNodes.AppendElement(node);
          }
        }

        // Now that we have the list, do the font size change on each node
        for (auto& node : arrayOfNodes) {
          res = RelativeFontChangeOnNode(aDir == FontSize::incr ? +1 : -1,
                                         node);
          NS_ENSURE_SUCCESS(res, res);
        }
      }
      // Now check the start and end parents of the range to see if they need
      // to be separately handled (they do if they are text nodes, due to how
      // the subtree iterator works - it will not have reported them).
      if (IsTextNode(startNode) && IsEditable(startNode)) {
        res = RelativeFontChangeOnTextNode(aDir, *startNode->GetAsText(),
                                           range->StartOffset(),
                                           startNode->Length());
        NS_ENSURE_SUCCESS(res, res);
      }
      if (IsTextNode(endNode) && IsEditable(endNode)) {
        res = RelativeFontChangeOnTextNode(aDir, *endNode->GetAsText(), 0,
                                           range->EndOffset());
        NS_ENSURE_SUCCESS(res, res);
      }
    }
  }

  return NS_OK;
}

nsresult
HTMLEditor::RelativeFontChangeOnTextNode(FontSize aDir,
                                         Text& aTextNode,
                                         int32_t aStartOffset,
                                         int32_t aEndOffset)
{
  // Don't need to do anything if no characters actually selected
  if (aStartOffset == aEndOffset) {
    return NS_OK;
  }

  if (!aTextNode.GetParentNode() ||
      !CanContainTag(*aTextNode.GetParentNode(), *nsGkAtoms::big)) {
    return NS_OK;
  }

  OwningNonNull<nsIContent> node = aTextNode;

  // Do we need to split the text node?

  // -1 is a magic value meaning to the end of node
  if (aEndOffset == -1) {
    aEndOffset = aTextNode.Length();
  }

  ErrorResult rv;
  if ((uint32_t)aEndOffset != aTextNode.Length()) {
    // We need to split off back of text node
    node = SplitNode(node, aEndOffset, rv);
    NS_ENSURE_TRUE(!rv.Failed(), rv.StealNSResult());
  }
  if (aStartOffset) {
    // We need to split off front of text node
    SplitNode(node, aStartOffset, rv);
    NS_ENSURE_TRUE(!rv.Failed(), rv.StealNSResult());
  }

  // Look for siblings that are correct type of node
  nsIAtom* nodeType = aDir == FontSize::incr ? nsGkAtoms::big
                                             : nsGkAtoms::small;
  nsCOMPtr<nsIContent> sibling = GetPriorHTMLSibling(node);
  if (sibling && sibling->IsHTMLElement(nodeType)) {
    // Previous sib is already right kind of inline node; slide this over
    nsresult res = MoveNode(node, sibling, -1);
    NS_ENSURE_SUCCESS(res, res);
    return NS_OK;
  }
  sibling = GetNextHTMLSibling(node);
  if (sibling && sibling->IsHTMLElement(nodeType)) {
    // Following sib is already right kind of inline node; slide this over
    nsresult res = MoveNode(node, sibling, 0);
    NS_ENSURE_SUCCESS(res, res);
    return NS_OK;
  }

  // Else reparent the node inside font node with appropriate relative size
  nsCOMPtr<Element> newElement = InsertContainerAbove(node, nodeType);
  NS_ENSURE_STATE(newElement);

  return NS_OK;
}

nsresult
HTMLEditor::RelativeFontChangeHelper(int32_t aSizeChange,
                                     nsINode* aNode)
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
HTMLEditor::RelativeFontChangeOnNode(int32_t aSizeChange,
                                     nsIContent* aNode)
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
HTMLEditor::GetFontFaceState(bool* aMixed,
                             nsAString& outFace)
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
HTMLEditor::GetFontColorState(bool* aMixed,
                              nsAString& aOutColor)
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
HTMLEditor::GetIsCSSEnabled(bool* aIsCSSEnabled)
{
  *aIsCSSEnabled = IsCSSEnabled();
  return NS_OK;
}

static bool
HasNonEmptyAttribute(Element* aElement,
                     nsIAtom* aName)
{
  MOZ_ASSERT(aElement);

  nsAutoString value;
  return aElement->GetAttr(kNameSpaceID_None, aName, value) && !value.IsEmpty();
}

bool
HTMLEditor::HasStyleOrIdOrClass(Element* aElement)
{
  MOZ_ASSERT(aElement);

  // remove the node if its style attribute is empty or absent,
  // and if it does not have a class nor an id
  return HasNonEmptyAttribute(aElement, nsGkAtoms::style) ||
         HasNonEmptyAttribute(aElement, nsGkAtoms::_class) ||
         HasNonEmptyAttribute(aElement, nsGkAtoms::id);
}

nsresult
HTMLEditor::RemoveElementIfNoStyleOrIdOrClass(Element& aElement)
{
  // early way out if node is not the right kind of element
  if ((!aElement.IsHTMLElement(nsGkAtoms::span) &&
       !aElement.IsHTMLElement(nsGkAtoms::font)) ||
      HasStyleOrIdOrClass(&aElement)) {
    return NS_OK;
  }

  return RemoveContainer(&aElement);
}

} // namespace mozilla
