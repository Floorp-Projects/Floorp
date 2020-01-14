/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/HTMLEditor.h"

#include "HTMLEditUtils.h"
#include "TypeInState.h"
#include "mozilla/Assertions.h"
#include "mozilla/ContentIterator.h"
#include "mozilla/EditAction.h"
#include "mozilla/EditorUtils.h"
#include "mozilla/SelectionState.h"
#include "mozilla/mozalloc.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLBRElement.h"
#include "mozilla/dom/Selection.h"
#include "nsAString.h"
#include "nsAttrName.h"
#include "nsCOMPtr.h"
#include "nsCaseTreatment.h"
#include "nsComponentManagerUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsAtom.h"
#include "nsIContent.h"
#include "nsNameSpaceManager.h"
#include "nsINode.h"
#include "nsIPrincipal.h"
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

static already_AddRefed<nsAtom> AtomizeAttribute(const nsAString& aAttribute) {
  if (aAttribute.IsEmpty()) {
    return nullptr;  // Don't use nsGkAtoms::_empty for attribute.
  }
  return NS_Atomize(aAttribute);
}

bool HTMLEditor::IsEmptyTextNode(nsINode& aNode) const {
  bool isEmptyTextNode = false;
  return EditorBase::IsTextNode(&aNode) &&
         NS_SUCCEEDED(IsEmptyNode(&aNode, &isEmptyTextNode)) && isEmptyTextNode;
}

nsresult HTMLEditor::SetInlinePropertyAsAction(nsAtom& aProperty,
                                               nsAtom* aAttribute,
                                               const nsAString& aValue,
                                               nsIPrincipal* aPrincipal) {
  AutoEditActionDataSetter editActionData(
      *this,
      HTMLEditUtils::GetEditActionForFormatText(aProperty, aAttribute, true),
      aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  switch (editActionData.GetEditAction()) {
    case EditAction::eSetFontFamilyProperty:
      MOZ_ASSERT(!aValue.IsVoid());
      // XXX Should we trim unnecessary whitespaces?
      editActionData.SetData(aValue);
      break;
    case EditAction::eSetColorProperty:
    case EditAction::eSetBackgroundColorPropertyInline:
      editActionData.SetColorData(aValue);
      break;
    default:
      break;
  }

  AutoTransactionBatch treatAsOneTransaction(*this);

  if (&aProperty == nsGkAtoms::sup) {
    // Superscript and Subscript styles are mutually exclusive.
    nsresult rv = RemoveInlinePropertyInternal(nsGkAtoms::sub, nullptr);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return EditorBase::ToGenericNSResult(rv);
    }
  } else if (&aProperty == nsGkAtoms::sub) {
    // Superscript and Subscript styles are mutually exclusive.
    nsresult rv = RemoveInlinePropertyInternal(nsGkAtoms::sup, nullptr);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return EditorBase::ToGenericNSResult(rv);
    }
  }
  nsresult rv = SetInlinePropertyInternal(aProperty, aAttribute, aValue);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "SetInlinePropertyInternal() failed");
  return EditorBase::ToGenericNSResult(rv);
}

NS_IMETHODIMP
HTMLEditor::SetInlineProperty(const nsAString& aProperty,
                              const nsAString& aAttribute,
                              const nsAString& aValue) {
  RefPtr<nsAtom> property = NS_Atomize(aProperty);
  if (NS_WARN_IF(!property)) {
    return NS_ERROR_INVALID_ARG;
  }
  RefPtr<nsAtom> attribute = AtomizeAttribute(aAttribute);
  AutoEditActionDataSetter editActionData(
      *this,
      HTMLEditUtils::GetEditActionForFormatText(*property, attribute, true));
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  switch (editActionData.GetEditAction()) {
    case EditAction::eSetFontFamilyProperty:
      MOZ_ASSERT(!aValue.IsVoid());
      // XXX Should we trim unnecessary whitespaces?
      editActionData.SetData(aValue);
      break;
    case EditAction::eSetColorProperty:
    case EditAction::eSetBackgroundColorPropertyInline:
      editActionData.SetColorData(aValue);
      break;
    default:
      break;
  }
  nsresult rv = SetInlinePropertyInternal(*property, attribute, aValue);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "SetInlinePropertyInternal() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult HTMLEditor::SetInlinePropertyInternal(
    nsAtom& aProperty, nsAtom* aAttribute, const nsAString& aAttributeValue) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!mInitSucceeded)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  CommitComposition();

  if (SelectionRefPtr()->IsCollapsed()) {
    // Manipulating text attributes on a collapsed selection only sets state
    // for the next text insertion
    mTypeInState->SetProp(&aProperty, aAttribute, aAttributeValue);
    return NS_OK;
  }

  // XXX Shouldn't we return before calling `CommitComposition()`?
  if (IsPlaintextEditor()) {
    return NS_OK;
  }

  EditActionResult result = CanHandleHTMLEditSubAction();
  if (NS_WARN_IF(result.Failed()) || result.Canceled()) {
    return result.Rv();
  }

  AutoPlaceholderBatch treatAsOneTransaction(*this);
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eInsertElement, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  {
    AutoSelectionRestorer restoreSelectionLater(*this);
    AutoTransactionsConserveSelection dontChangeMySelection(*this);

    // Loop through the ranges in the selection
    // XXX This is different from `SetCSSBackgroundColorWithTransaction()`.
    //     It refers `Selection::GetRangeAt()` in each time.  The result may
    //     be different if mutation event listener changes the `Selection`.
    AutoRangeArray arrayOfRanges(SelectionRefPtr());
    for (auto& range : arrayOfRanges.mRanges) {
      // Adjust range to include any ancestors whose children are entirely
      // selected
      nsresult rv = PromoteInlineRange(*range);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      // XXX Shouldn't we skip the range if it's been collapsed by mutation
      //     event listener?

      EditorDOMPoint startOfRange(range->StartRef());
      EditorDOMPoint endOfRange(range->EndRef());
      if (NS_WARN_IF(!startOfRange.IsSet()) ||
          NS_WARN_IF(!endOfRange.IsSet())) {
        continue;
      }

      // If range is in a text node, apply new style simply.
      if (startOfRange.GetContainer() == endOfRange.GetContainer() &&
          startOfRange.IsInTextNode()) {
        nsresult rv = SetInlinePropertyOnTextNode(
            MOZ_KnownLive(*startOfRange.GetContainerAsText()),
            startOfRange.Offset(), endOfRange.Offset(), aProperty, aAttribute,
            aAttributeValue);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        continue;
      }

      // Collect editable nodes which are entirely contained in the range.
      AutoTArray<OwningNonNull<nsIContent>, 64> arrayOfContents;
      ContentSubtreeIterator subtreeIter;
      // If there is no node which is entirely in the range,
      // `ContentSubtreeIterator::Init()` fails, but this is possible case,
      // don't warn it.
      if (NS_SUCCEEDED(subtreeIter.Init(range))) {
        for (; !subtreeIter.IsDone(); subtreeIter.Next()) {
          nsINode* node = subtreeIter.GetCurrentNode();
          if (NS_WARN_IF(!node)) {
            return NS_ERROR_FAILURE;
          }
          if (node->IsContent() && IsEditable(node)) {
            arrayOfContents.AppendElement(*node->AsContent());
          }
        }
      }

      // If start node is a text node, apply new style to a part of it.
      if (startOfRange.IsInTextNode() &&
          IsEditable(startOfRange.GetContainer())) {
        nsresult rv = SetInlinePropertyOnTextNode(
            MOZ_KnownLive(*startOfRange.GetContainerAsText()),
            startOfRange.Offset(), startOfRange.GetContainer()->Length(),
            aProperty, aAttribute, aAttributeValue);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }

      // Then, apply new style to all nodes in the range entirely.
      for (auto& content : arrayOfContents) {
        nsresult rv = SetInlinePropertyOnNode(*content, aProperty, aAttribute,
                                              aAttributeValue);
        if (NS_WARN_IF(Destroyed())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }

      // Finally, if end node is a text node, apply new style to a part ot it.
      if (endOfRange.IsInTextNode() && IsEditable(endOfRange.GetContainer())) {
        nsresult rv = SetInlinePropertyOnTextNode(
            MOZ_KnownLive(*endOfRange.GetContainerAsText()), 0,
            endOfRange.Offset(), aProperty, aAttribute, aAttributeValue);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
    }
  }
  // Restoring `Selection` may have destroyed us.
  return NS_WARN_IF(Destroyed()) ? NS_ERROR_EDITOR_DESTROYED : NS_OK;
}

// Helper function for SetInlinePropertyOn*: is aNode a simple old <b>, <font>,
// <span style="">, etc. that we can reuse instead of creating a new one?
bool HTMLEditor::IsSimpleModifiableNode(nsIContent* aContent, nsAtom* aProperty,
                                        nsAtom* aAttribute,
                                        const nsAString* aValue) {
  // aContent can be null, in which case we'll return false in a few lines
  MOZ_ASSERT(aProperty);
  MOZ_ASSERT_IF(aAttribute, aValue);

  nsCOMPtr<dom::Element> element = do_QueryInterface(aContent);
  if (!element) {
    return false;
  }

  // First check for <b>, <i>, etc.
  if (element->IsHTMLElement(aProperty) && !element->GetAttrCount() &&
      !aAttribute) {
    return true;
  }

  // Special cases for various equivalencies: <strong>, <em>, <s>
  if (!element->GetAttrCount() &&
      ((aProperty == nsGkAtoms::b &&
        element->IsHTMLElement(nsGkAtoms::strong)) ||
       (aProperty == nsGkAtoms::i && element->IsHTMLElement(nsGkAtoms::em)) ||
       (aProperty == nsGkAtoms::strike &&
        element->IsHTMLElement(nsGkAtoms::s)))) {
    return true;
  }

  // Now look for things like <font>
  if (aAttribute) {
    nsString attrValue;
    if (element->IsHTMLElement(aProperty) &&
        IsOnlyAttribute(element, aAttribute) &&
        element->GetAttr(kNameSpaceID_None, aAttribute, attrValue) &&
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
  if (!CSSEditUtils::IsCSSEditableProperty(element, aProperty, aAttribute) ||
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
  mCSSEditUtils->SetCSSEquivalentToHTMLStyle(newSpan, aProperty, aAttribute,
                                             aValue,
                                             /*suppress transaction*/ true);

  return CSSEditUtils::ElementsSameStyle(newSpan, element);
}

nsresult HTMLEditor::SetInlinePropertyOnTextNode(
    Text& aText, uint32_t aStartOffset, uint32_t aEndOffset, nsAtom& aProperty,
    nsAtom* aAttribute, const nsAString& aValue) {
  if (!aText.GetParentNode() ||
      !CanContainTag(*aText.GetParentNode(), aProperty)) {
    return NS_OK;
  }

  // Don't need to do anything if no characters actually selected
  if (aStartOffset == aEndOffset) {
    return NS_OK;
  }

  // Don't need to do anything if property already set on node
  if (CSSEditUtils::IsCSSEditableProperty(&aText, &aProperty, aAttribute)) {
    // The HTML styles defined by aProperty/aAttribute have a CSS equivalence
    // for node; let's check if it carries those CSS styles
    if (CSSEditUtils::IsCSSEquivalentToHTMLInlineStyleSet(
            &aText, &aProperty, aAttribute, aValue, CSSEditUtils::eComputed)) {
      return NS_OK;
    }
  } else if (IsTextPropertySetByContent(&aText, &aProperty, aAttribute,
                                        &aValue)) {
    return NS_OK;
  }

  // Make the range an independent node.
  nsCOMPtr<nsIContent> textNodeForTheRange = &aText;

  // Split at the end of the range.
  EditorDOMPoint atEnd(textNodeForTheRange, aEndOffset);
  if (!atEnd.IsEndOfContainer()) {
    // We need to split off back of text node
    ErrorResult error;
    textNodeForTheRange = SplitNodeWithTransaction(atEnd, error);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
  }

  // Split at the start of the range.
  EditorDOMPoint atStart(textNodeForTheRange, aStartOffset);
  if (!atStart.IsStartOfContainer()) {
    // We need to split off front of text node
    ErrorResult error;
    nsCOMPtr<nsIContent> newLeftNode = SplitNodeWithTransaction(atStart, error);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
    Unused << newLeftNode;
  }

  if (aAttribute) {
    // Look for siblings that are correct type of node
    nsCOMPtr<nsIContent> sibling = GetPriorHTMLSibling(textNodeForTheRange);
    if (IsSimpleModifiableNode(sibling, &aProperty, aAttribute, &aValue)) {
      // Previous sib is already right kind of inline node; slide this over
      nsresult rv =
          MoveNodeToEndWithTransaction(*textNodeForTheRange, *sibling);
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "MoveNodeToEndWithTransaction() failed");
      return rv;
    }
    sibling = GetNextHTMLSibling(textNodeForTheRange);
    if (IsSimpleModifiableNode(sibling, &aProperty, aAttribute, &aValue)) {
      // Following sib is already right kind of inline node; slide this over
      nsresult rv = MoveNodeWithTransaction(*textNodeForTheRange,
                                            EditorDOMPoint(sibling, 0));
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "MoveNodeWithTransaction() failed");
      return rv;
    }
  }

  // Reparent the node inside inline node with appropriate {attribute,value}
  nsresult rv = SetInlinePropertyOnNode(*textNodeForTheRange, aProperty,
                                        aAttribute, aValue);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "SetInlinePropertyOnNode() failed");
  return rv;
}

nsresult HTMLEditor::SetInlinePropertyOnNodeImpl(nsIContent& aNode,
                                                 nsAtom& aProperty,
                                                 nsAtom* aAttribute,
                                                 const nsAString& aValue) {
  // If this is an element that can't be contained in a span, we have to
  // recurse to its children.
  if (!TagCanContain(*nsGkAtoms::span, aNode)) {
    if (aNode.HasChildren()) {
      nsTArray<OwningNonNull<nsIContent>> arrayOfNodes;

      // Populate the list.
      for (nsCOMPtr<nsIContent> child = aNode.GetFirstChild(); child;
           child = child->GetNextSibling()) {
        if (IsEditable(child) && !IsEmptyTextNode(*child)) {
          arrayOfNodes.AppendElement(*child);
        }
      }

      // Then loop through the list, set the property on each node.
      for (auto& node : arrayOfNodes) {
        nsresult rv =
            SetInlinePropertyOnNode(node, aProperty, aAttribute, aValue);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
    return NS_OK;
  }

  // First check if there's an adjacent sibling we can put our node into.
  nsCOMPtr<nsIContent> previousSibling = GetPriorHTMLSibling(&aNode);
  nsCOMPtr<nsIContent> nextSibling = GetNextHTMLSibling(&aNode);
  if (IsSimpleModifiableNode(previousSibling, &aProperty, aAttribute,
                             &aValue)) {
    nsresult rv = MoveNodeToEndWithTransaction(aNode, *previousSibling);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    if (IsSimpleModifiableNode(nextSibling, &aProperty, aAttribute, &aValue)) {
      rv = JoinNodesWithTransaction(*previousSibling, *nextSibling);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
    return NS_OK;
  }
  if (IsSimpleModifiableNode(nextSibling, &aProperty, aAttribute, &aValue)) {
    nsresult rv =
        MoveNodeWithTransaction(aNode, EditorDOMPoint(nextSibling, 0));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  // Don't need to do anything if property already set on node
  if (CSSEditUtils::IsCSSEditableProperty(&aNode, &aProperty, aAttribute)) {
    if (CSSEditUtils::IsCSSEquivalentToHTMLInlineStyleSet(
            &aNode, &aProperty, aAttribute, aValue, CSSEditUtils::eComputed)) {
      return NS_OK;
    }
  } else if (IsTextPropertySetByContent(&aNode, &aProperty, aAttribute,
                                        &aValue)) {
    return NS_OK;
  }

  bool useCSS = (IsCSSEnabled() && CSSEditUtils::IsCSSEditableProperty(
                                       &aNode, &aProperty, aAttribute)) ||
                // bgcolor is always done using CSS
                aAttribute == nsGkAtoms::bgcolor ||
                // called for removing parent style, we should use CSS with
                // `<span>` element.
                aValue.EqualsLiteral("-moz-editor-invert-value");

  if (useCSS) {
    RefPtr<Element> spanElement;
    // We only add style="" to <span>s with no attributes (bug 746515).  If we
    // don't have one, we need to make one.
    if (aNode.IsHTMLElement(nsGkAtoms::span) &&
        !aNode.AsElement()->GetAttrCount()) {
      spanElement = aNode.AsElement();
    } else {
      spanElement = InsertContainerWithTransaction(aNode, *nsGkAtoms::span);
      if (NS_WARN_IF(!spanElement)) {
        return NS_ERROR_FAILURE;
      }
    }

    // Add the CSS styles corresponding to the HTML style request
    mCSSEditUtils->SetCSSEquivalentToHTMLStyle(spanElement, &aProperty,
                                               aAttribute, &aValue, false);
    return NS_OK;
  }

  // is it already the right kind of node, but with wrong attribute?
  if (aNode.IsHTMLElement(&aProperty)) {
    if (NS_WARN_IF(!aAttribute)) {
      return NS_ERROR_FAILURE;
    }
    // Just set the attribute on it.
    return SetAttributeWithTransaction(MOZ_KnownLive(*aNode.AsElement()),
                                       *aAttribute, aValue);
  }

  // ok, chuck it in its very own container
  RefPtr<Element> tmp = InsertContainerWithTransaction(
      aNode, aProperty, aAttribute ? *aAttribute : *nsGkAtoms::_empty, aValue);
  if (NS_WARN_IF(!tmp)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult HTMLEditor::SetInlinePropertyOnNode(nsIContent& aNode,
                                             nsAtom& aProperty,
                                             nsAtom* aAttribute,
                                             const nsAString& aValue) {
  nsCOMPtr<nsIContent> previousSibling = aNode.GetPreviousSibling(),
                       nextSibling = aNode.GetNextSibling();
  NS_ENSURE_STATE(aNode.GetParentNode());
  OwningNonNull<nsINode> parent = *aNode.GetParentNode();

  if (aNode.IsElement()) {
    nsresult rv = RemoveStyleInside(MOZ_KnownLive(*aNode.AsElement()),
                                    &aProperty, aAttribute);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (aNode.GetParentNode()) {
    // The node is still where it was
    return SetInlinePropertyOnNodeImpl(aNode, aProperty, aAttribute, aValue);
  }

  // It's vanished.  Use the old siblings for reference to construct a
  // list.  But first, verify that the previous/next siblings are still
  // where we expect them; otherwise we have to give up.
  if ((previousSibling && previousSibling->GetParentNode() != parent) ||
      (nextSibling && nextSibling->GetParentNode() != parent)) {
    return NS_ERROR_UNEXPECTED;
  }
  nsTArray<OwningNonNull<nsIContent>> nodesToSet;
  nsCOMPtr<nsIContent> cur = previousSibling ? previousSibling->GetNextSibling()
                                             : parent->GetFirstChild();
  for (; cur && cur != nextSibling; cur = cur->GetNextSibling()) {
    if (IsEditable(cur)) {
      nodesToSet.AppendElement(*cur);
    }
  }

  for (auto& node : nodesToSet) {
    nsresult rv =
        SetInlinePropertyOnNodeImpl(node, aProperty, aAttribute, aValue);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

SplitRangeOffResult HTMLEditor::SplitAncestorStyledInlineElementsAtRangeEdges(
    const EditorDOMPoint& aStartOfRange, const EditorDOMPoint& aEndOfRange,
    nsAtom* aProperty, nsAtom* aAttribute) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!aStartOfRange.IsSet()) || NS_WARN_IF(!aEndOfRange.IsSet())) {
    return SplitRangeOffResult(NS_ERROR_INVALID_ARG);
  }

  EditorDOMPoint startOfRange(aStartOfRange);
  EditorDOMPoint endOfRange(aEndOfRange);

  // split any matching style nodes above the start of range
  SplitNodeResult resultAtStart(NS_ERROR_NOT_INITIALIZED);
  {
    AutoTrackDOMPoint tracker(RangeUpdaterRef(), &endOfRange);
    resultAtStart = SplitAncestorStyledInlineElementsAt(startOfRange, aProperty,
                                                        aAttribute);
    if (NS_WARN_IF(resultAtStart.Failed())) {
      return SplitRangeOffResult(resultAtStart.Rv());
    }
    if (resultAtStart.Handled()) {
      startOfRange = resultAtStart.SplitPoint();
      if (NS_WARN_IF(!startOfRange.IsSet())) {
        return SplitRangeOffResult(NS_ERROR_FAILURE);
      }
    }
  }

  // second verse, same as the first...
  SplitNodeResult resultAtEnd(NS_ERROR_NOT_INITIALIZED);
  {
    AutoTrackDOMPoint tracker(RangeUpdaterRef(), &startOfRange);
    resultAtEnd =
        SplitAncestorStyledInlineElementsAt(endOfRange, aProperty, aAttribute);
    if (NS_WARN_IF(resultAtEnd.Failed())) {
      return SplitRangeOffResult(resultAtEnd.Rv());
    }
    if (resultAtEnd.Handled()) {
      endOfRange = resultAtEnd.SplitPoint();
      if (NS_WARN_IF(!endOfRange.IsSet())) {
        return SplitRangeOffResult(NS_ERROR_FAILURE);
      }
    }
  }

  return SplitRangeOffResult(startOfRange, resultAtStart, endOfRange,
                             resultAtEnd);
}

SplitNodeResult HTMLEditor::SplitAncestorStyledInlineElementsAt(
    const EditorDOMPoint& aPointToSplit, nsAtom* aProperty,
    nsAtom* aAttribute) {
  if (NS_WARN_IF(!aPointToSplit.IsSet()) ||
      NS_WARN_IF(!aPointToSplit.GetContainerAsContent())) {
    return SplitNodeResult(NS_ERROR_INVALID_ARG);
  }

  // We assume that this method is called only when we're removing style(s).
  // Even if we're in HTML mode and there is no presentation element in the
  // block, we may need to overwrite the block's style with `<span>` element
  // and CSS.  For example, `<h1>` element has `font-weight: bold;` as its
  // default style.  If `Document.execCommand("bold")` is called for its
  // text, we should make it unbold.  Therefore, we shouldn't check
  // IsCSSEnabled() in most cases.  However, there is an exception.
  // FontFaceStateCommand::SetState() calls RemoveInlinePropertyAsAction()
  // with nsGkAtoms::tt before calling SetInlinePropertyAsAction() if we
  // are handling a XUL command.  Only in that case, we need to check
  // IsCSSEnabled().
  bool useCSS = aProperty != nsGkAtoms::tt || IsCSSEnabled();

  // Split any matching style nodes above the point.
  SplitNodeResult result(aPointToSplit);
  MOZ_ASSERT(!result.Handled());
  for (nsCOMPtr<nsIContent> content = aPointToSplit.GetContainerAsContent();
       !IsBlockNode(content) && content->GetParent() &&
       IsEditable(content->GetParent());
       content = content->GetParent()) {
    bool isSetByCSS = false;
    if (useCSS &&
        CSSEditUtils::IsCSSEditableProperty(content, aProperty, aAttribute)) {
      // The HTML style defined by aProperty/aAttribute has a CSS equivalence
      // in this implementation for the node; let's check if it carries those
      // CSS styles
      nsAutoString firstValue;
      isSetByCSS = CSSEditUtils::IsCSSEquivalentToHTMLInlineStyleSet(
          content, aProperty, aAttribute, firstValue, CSSEditUtils::eSpecified);
    }
    if (!isSetByCSS) {
      if (!content->IsElement()) {
        continue;
      }
      // If aProperty is set, we need to split only elements which applies the
      // given style.
      if (aProperty) {
        // If the content is an inline element represents aProperty or
        // the content is a link element and aProperty is `href`, we should
        // split the content.
        if (!content->IsHTMLElement(aProperty) &&
            !(aProperty == nsGkAtoms::href && HTMLEditUtils::IsLink(content))) {
          continue;
        }
      }
      // If aProperty is nullptr, we need to split any style.
      else if (!IsEditable(content) ||
               !HTMLEditUtils::IsRemovableInlineStyleElement(
                   *content->AsElement())) {
        continue;
      }
    }

    // Found a style node we need to split.
    // XXX If first content is a text node and CSS is enabled, we call this
    //     with text node but in such case, this does nothing, but returns
    //     as handled with setting only previous or next node.  If its parent
    //     is a block, we do nothing but return as handled.
    SplitNodeResult splitNodeResult = SplitNodeDeepWithTransaction(
        *content, result.SplitPoint(),
        SplitAtEdges::eAllowToCreateEmptyContainer);
    if (NS_WARN_IF(splitNodeResult.Failed())) {
      return splitNodeResult;
    }
    MOZ_ASSERT(splitNodeResult.Handled());
    // Mark the final result as handled forcibly.
    result = SplitNodeResult(splitNodeResult.GetPreviousNode(),
                             splitNodeResult.GetNextNode());
    MOZ_ASSERT(result.Handled());
  }

  return result;
}

EditResult HTMLEditor::ClearStyleAt(const EditorDOMPoint& aPoint,
                                    nsAtom* aProperty, nsAtom* aAttribute) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!aPoint.IsSet())) {
    return EditResult(NS_ERROR_INVALID_ARG);
  }

  // First, split inline elements at the point.
  // E.g., if aProperty is nsGkAtoms::b and `<p><b><i>a[]bc</i></b></p>`,
  //       we want to make it as `<p><b><i>a</i></b><b><i>bc</i></b></p>`.
  SplitNodeResult splitResult =
      SplitAncestorStyledInlineElementsAt(aPoint, aProperty, aAttribute);
  if (NS_WARN_IF(splitResult.Failed())) {
    return EditResult(splitResult.Rv());
  }

  // If there is no styled inline elements of aProperty/aAttribute, we just
  // return the given point.
  // E.g., `<p><i>a[]bc</i></p>` for nsGkAtoms::b.
  if (!splitResult.Handled()) {
    return EditResult(aPoint);
  }

  // If it did split nodes, but topmost ancestor inline element is split
  // at start of it, we don't need the empty inline element.  Let's remove
  // it now.
  if (splitResult.GetPreviousNode()) {
    bool isEmpty = false;
    IsEmptyNode(splitResult.GetPreviousNode(), &isEmpty, false, true);
    if (isEmpty) {
      // Delete previous node if it's empty.
      nsresult rv = DeleteNodeWithTransaction(
          MOZ_KnownLive(*splitResult.GetPreviousNode()));
      if (NS_WARN_IF(Destroyed())) {
        return EditResult(NS_ERROR_EDITOR_DESTROYED);
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return EditResult(rv);
      }
    }
  }

  // If we reached block from end of a text node, we can do nothing here.
  // E.g., `<p style="font-weight: bold;">a[]bc</p>` for nsGkAtoms::b and
  // we're in CSS mode.
  // XXX Chrome resets block style and creates `<span>` elements for each
  //     line in this case.
  if (!splitResult.GetNextNode()) {
    MOZ_ASSERT(IsCSSEnabled());
    return EditResult(aPoint);
  }

  // Otherwise, the next node is topmost ancestor inline element which has
  // the style.  We want to put caret between the split nodes, but we need
  // to keep other styles.  Therefore, next, we need to split at start of
  // the next node.  The first example should become
  // `<p><b><i>a</i></b><b><i></i></b><b><i>bc</i></b></p>`.
  //                    ^^^^^^^^^^^^^^
  nsIContent* leftmostChildOfNextNode =
      GetLeftmostChild(splitResult.GetNextNode());
  EditorDOMPoint atStartOfNextNode(leftmostChildOfNextNode
                                       ? leftmostChildOfNextNode
                                       : splitResult.GetNextNode(),
                                   0);
  RefPtr<HTMLBRElement> brElement;
  // But don't try to split non-containers like `<br>`, `<hr>` and `<img>`
  // element.
  if (!IsContainer(atStartOfNextNode.GetContainer())) {
    // If it's a `<br>` element, let's move it into new node later.
    brElement = HTMLBRElement::FromNode(atStartOfNextNode.GetContainer());
    if (NS_WARN_IF(!atStartOfNextNode.GetContainerParentAsContent())) {
      return EditResult(NS_ERROR_FAILURE);
    }
    atStartOfNextNode.Set(atStartOfNextNode.GetContainerParent(), 0);
  }
  SplitNodeResult splitResultAtStartOfNextNode =
      SplitAncestorStyledInlineElementsAt(atStartOfNextNode, aProperty,
                                          aAttribute);
  if (NS_WARN_IF(splitResultAtStartOfNextNode.Failed())) {
    return EditResult(splitResultAtStartOfNextNode.Rv());
  }

  // Let's remove the next node if it becomes empty by splitting it.
  // XXX Is this possible case without mutation event listener?
  if (splitResultAtStartOfNextNode.Handled() &&
      splitResultAtStartOfNextNode.GetNextNode()) {
    bool isEmpty = false;
    IsEmptyNode(splitResultAtStartOfNextNode.GetNextNode(), &isEmpty, false,
                true);
    if (isEmpty) {
      // Delete next node if it's empty.
      nsresult rv = DeleteNodeWithTransaction(
          MOZ_KnownLive(*splitResultAtStartOfNextNode.GetNextNode()));
      if (NS_WARN_IF(Destroyed())) {
        return EditResult(NS_ERROR_EDITOR_DESTROYED);
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return EditResult(rv);
      }
    }
  }

  // If there is no content, we should return here.
  // XXX Is this possible case without mutation event listener?
  if (NS_WARN_IF(!splitResultAtStartOfNextNode.Handled()) ||
      !splitResultAtStartOfNextNode.GetPreviousNode()) {
    // XXX This is really odd, but we retrun this value...
    return EditResult(
        EditorDOMPoint(splitResult.SplitPoint().GetContainer(),
                       splitResultAtStartOfNextNode.SplitPoint().Offset()));
  }

  // Now, we want to put `<br>` element into the empty split node if
  // it was in next node of the first split.
  // E.g., `<p><b><i>a</i></b><b><i><br></i></b><b><i>bc</i></b></p>`
  nsIContent* leftmostChild =
      GetLeftmostChild(splitResultAtStartOfNextNode.GetPreviousNode());
  EditorDOMPoint pointToPutCaret(
      leftmostChild ? leftmostChild
                    : splitResultAtStartOfNextNode.GetPreviousNode(),
      0);
  // If the right node starts with a `<br>`, suck it out of right node and into
  // the left node left node.  This is so we you don't revert back to the
  // previous style if you happen to click at the end of a line.
  if (brElement) {
    nsresult rv = MoveNodeWithTransaction(*brElement, pointToPutCaret);
    if (NS_WARN_IF(Destroyed())) {
      return EditResult(NS_ERROR_EDITOR_DESTROYED);
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return EditResult(rv);
    }
    // Update the child.
    pointToPutCaret.Set(pointToPutCaret.GetContainer(), 0);
  }
  // Finally, remove the specified style in the previous node at the
  // second split and tells good insertion point to the caller.  I.e., we
  // want to make the first example as:
  // `<p><b><i>a</i></b><i>[]</i><b><i>bc</i></b></p>`
  //                    ^^^^^^^^^
  if (splitResultAtStartOfNextNode.GetPreviousNode()->IsElement()) {
    // Track the point at the new hierarchy.  This is so we can know where
    // to put the selection after we call RemoveStyleInside().
    // RemoveStyleInside() could remove any and all of those nodes, so I
    // have to use the range tracking system to find the right spot to put
    // selection.
    AutoTrackDOMPoint tracker(RangeUpdaterRef(), &pointToPutCaret);
    nsresult rv = RemoveStyleInside(
        MOZ_KnownLive(
            *splitResultAtStartOfNextNode.GetPreviousNode()->AsElement()),
        aProperty, aAttribute);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return EditResult(rv);
    }
  }
  return EditResult(pointToPutCaret);
}

nsresult HTMLEditor::RemoveStyleInside(Element& aElement, nsAtom* aProperty,
                                       nsAtom* aAttribute) {
  // First, handle all descendants.
  RefPtr<nsIContent> child = aElement.GetFirstChild();
  while (child) {
    // cache next sibling since we might remove child
    // XXX Well, the next sibling is moved from `aElement`, shouldn't we skip
    //     it here?
    nsCOMPtr<nsIContent> nextSibling = child->GetNextSibling();
    if (child->IsElement()) {
      nsresult rv = RemoveStyleInside(MOZ_KnownLive(*child->AsElement()),
                                      aProperty, aAttribute);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
    child = nextSibling.forget();
  }

  // Next, remove the element or its attribute.
  bool removeHTMLStyle = false;
  if (aProperty) {
    removeHTMLStyle =
        // If the element is a presentation element of aProperty
        aElement.NodeInfo()->NameAtom() == aProperty ||
        // or an `<a>` element with `href` attribute
        (aProperty == nsGkAtoms::href && HTMLEditUtils::IsLink(&aElement)) ||
        // or an `<a>` element with `name` attribute
        (aProperty == nsGkAtoms::name &&
         HTMLEditUtils::IsNamedAnchor(&aElement));
  }
  // XXX Why do we check if aElement is editable only when aProperty is
  //     nullptr?
  else if (IsEditable(&aElement)) {
    // or removing all styles and the element is a presentation element.
    removeHTMLStyle = HTMLEditUtils::IsRemovableInlineStyleElement(aElement);
  }

  if (removeHTMLStyle) {
    // If aAttribute is nullptr, we want to remove any matching inline styles
    // entirely.
    if (!aAttribute) {
      // If some style rules are specified to aElement, we need to keep them
      // as far as possible.
      // XXX Why don't we clone `id` attribute?
      if (aProperty &&
          (aElement.HasAttr(kNameSpaceID_None, nsGkAtoms::style) ||
           aElement.HasAttr(kNameSpaceID_None, nsGkAtoms::_class))) {
        // Move `style` attribute and `class` element to span element before
        // removing aElement from the tree.
        RefPtr<Element> spanElement =
            InsertContainerWithTransaction(aElement, *nsGkAtoms::span);
        if (NS_WARN_IF(Destroyed())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(!spanElement)) {
          return NS_ERROR_FAILURE;
        }
        nsresult rv = CloneAttributeWithTransaction(*nsGkAtoms::style,
                                                    *spanElement, aElement);
        if (NS_WARN_IF(Destroyed())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        rv = CloneAttributeWithTransaction(*nsGkAtoms::_class, *spanElement,
                                           aElement);
        if (NS_WARN_IF(Destroyed())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
      nsresult rv = RemoveContainerWithTransaction(aElement);
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
    // If aAttribute is specified, we want to remove only the attribute
    // unless it's the last attribute of aElement.
    else if (aElement.HasAttr(kNameSpaceID_None, aAttribute)) {
      if (IsOnlyAttribute(&aElement, aAttribute)) {
        nsresult rv = RemoveContainerWithTransaction(aElement);
        if (NS_WARN_IF(Destroyed())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      } else {
        nsresult rv = RemoveAttributeWithTransaction(aElement, *aAttribute);
        if (NS_WARN_IF(Destroyed())) {
          return NS_ERROR_EDITOR_DESTROYED;
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
    }
  }

  // Then, remove CSS style if specified.
  // XXX aElement may have already been removed from the DOM tree.  Why
  //     do we keep handling aElement here??
  if (CSSEditUtils::IsCSSEditableProperty(&aElement, aProperty, aAttribute) &&
      CSSEditUtils::HaveCSSEquivalentStyles(aElement, aProperty, aAttribute,
                                            CSSEditUtils::eSpecified)) {
    // If aElement has CSS declaration of the given style, remove it.
    mCSSEditUtils->RemoveCSSEquivalentToHTMLStyle(&aElement, aProperty,
                                                  aAttribute, nullptr, false);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    // Additionally, remove aElement itself if it's a `<span>` or `<font>`
    // and it does not have non-empty `style`, `id` nor `class` attribute.
    if (aElement.IsAnyOfHTMLElements(nsGkAtoms::span, nsGkAtoms::font) &&
        !HTMLEditor::HasStyleOrIdOrClassAttribute(aElement)) {
      DebugOnly<nsresult> rvIgnored = RemoveContainerWithTransaction(aElement);
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "RemoveContainerWithTransaction() failed, but ignored");
    }
  }

  // Finally, remove aElement if it's a `<big>` or `<small>` element and
  // we're removing `<font size>`.
  if (aProperty == nsGkAtoms::font && aAttribute == nsGkAtoms::size &&
      aElement.IsAnyOfHTMLElements(nsGkAtoms::big, nsGkAtoms::small)) {
    nsresult rv = RemoveContainerWithTransaction(aElement);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "RemoveContainerWithTransaction() failed");
    return rv;
  }

  return NS_OK;
}

bool HTMLEditor::IsOnlyAttribute(const Element* aElement, nsAtom* aAttribute) {
  MOZ_ASSERT(aElement);

  uint32_t attrCount = aElement->GetAttrCount();
  for (uint32_t i = 0; i < attrCount; ++i) {
    const nsAttrName* name = aElement->GetAttrNameAt(i);
    if (!name->NamespaceEquals(kNameSpaceID_None)) {
      return false;
    }

    // if it's the attribute we know about, or a special _moz attribute,
    // keep looking
    if (name->LocalName() != aAttribute) {
      nsAutoString attrString;
      name->LocalName()->ToString(attrString);
      if (!StringBeginsWith(attrString, NS_LITERAL_STRING("_moz"))) {
        return false;
      }
    }
  }
  // if we made it through all of them without finding a real attribute
  // other than aAttribute, then return true
  return true;
}

nsresult HTMLEditor::PromoteRangeIfStartsOrEndsInNamedAnchor(nsRange& aRange) {
  // We assume that <a> is not nested.
  nsCOMPtr<nsINode> startNode = aRange.GetStartContainer();
  int32_t startOffset = aRange.StartOffset();
  nsCOMPtr<nsINode> endNode = aRange.GetEndContainer();
  int32_t endOffset = aRange.EndOffset();

  nsCOMPtr<nsINode> parent = startNode;

  while (parent && !parent->IsHTMLElement(nsGkAtoms::body) &&
         !HTMLEditUtils::IsNamedAnchor(parent)) {
    parent = parent->GetParentNode();
  }
  NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);

  if (HTMLEditUtils::IsNamedAnchor(parent)) {
    startNode = parent->GetParentNode();
    startOffset = startNode ? startNode->ComputeIndexOf(parent) : -1;
  }

  parent = endNode;
  while (parent && !parent->IsHTMLElement(nsGkAtoms::body) &&
         !HTMLEditUtils::IsNamedAnchor(parent)) {
    parent = parent->GetParentNode();
  }
  NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);

  if (HTMLEditUtils::IsNamedAnchor(parent)) {
    endNode = parent->GetParentNode();
    endOffset = endNode ? endNode->ComputeIndexOf(parent) + 1 : 0;
  }

  nsresult rv =
      aRange.SetStartAndEnd(startNode, startOffset, endNode, endOffset);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult HTMLEditor::PromoteInlineRange(nsRange& aRange) {
  nsCOMPtr<nsINode> startNode = aRange.GetStartContainer();
  int32_t startOffset = aRange.StartOffset();
  nsCOMPtr<nsINode> endNode = aRange.GetEndContainer();
  int32_t endOffset = aRange.EndOffset();

  while (startNode && !startNode->IsHTMLElement(nsGkAtoms::body) &&
         IsEditable(startNode) && IsAtFrontOfNode(*startNode, startOffset)) {
    nsCOMPtr<nsINode> parent = startNode->GetParentNode();
    NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);
    startOffset = parent->ComputeIndexOf(startNode);
    startNode = parent;
  }

  while (endNode && !endNode->IsHTMLElement(nsGkAtoms::body) &&
         IsEditable(endNode) && IsAtEndOfNode(*endNode, endOffset)) {
    nsCOMPtr<nsINode> parent = endNode->GetParentNode();
    NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);
    // We are AFTER this node
    endOffset = 1 + parent->ComputeIndexOf(endNode);
    endNode = parent;
  }

  nsresult rv =
      aRange.SetStartAndEnd(startNode, startOffset, endNode, endOffset);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

bool HTMLEditor::IsAtFrontOfNode(nsINode& aNode, int32_t aOffset) {
  if (!aOffset) {
    return true;
  }

  if (IsTextNode(&aNode)) {
    return false;
  }

  nsCOMPtr<nsIContent> firstNode = GetFirstEditableChild(aNode);
  NS_ENSURE_TRUE(firstNode, true);
  if (aNode.ComputeIndexOf(firstNode) < aOffset) {
    return false;
  }
  return true;
}

bool HTMLEditor::IsAtEndOfNode(nsINode& aNode, int32_t aOffset) {
  if (aOffset == (int32_t)aNode.Length()) {
    return true;
  }

  if (IsTextNode(&aNode)) {
    return false;
  }

  nsCOMPtr<nsIContent> lastNode = GetLastEditableChild(aNode);
  NS_ENSURE_TRUE(lastNode, true);
  if (aNode.ComputeIndexOf(lastNode) < aOffset) {
    return true;
  }
  return false;
}

nsresult HTMLEditor::GetInlinePropertyBase(nsAtom& aProperty,
                                           nsAtom* aAttribute,
                                           const nsAString* aValue,
                                           bool* aFirst, bool* aAny, bool* aAll,
                                           nsAString* outValue) const {
  MOZ_ASSERT(IsEditActionDataAvailable());

  *aAny = false;
  *aAll = true;
  *aFirst = false;
  bool first = true;

  bool isCollapsed = SelectionRefPtr()->IsCollapsed();
  RefPtr<nsRange> range = SelectionRefPtr()->GetRangeAt(0);
  // XXX: Should be a while loop, to get each separate range
  // XXX: ERROR_HANDLING can currentItem be null?
  if (range) {
    // For each range, set a flag
    bool firstNodeInRange = true;

    if (isCollapsed) {
      nsCOMPtr<nsINode> collapsedNode = range->GetStartContainer();
      if (NS_WARN_IF(!collapsedNode)) {
        return NS_ERROR_FAILURE;
      }
      bool isSet, theSetting;
      nsString tOutString;
      if (aAttribute) {
        mTypeInState->GetTypingState(isSet, theSetting, &aProperty, aAttribute,
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

      if (CSSEditUtils::IsCSSEditableProperty(collapsedNode, &aProperty,
                                              aAttribute)) {
        if (aValue) {
          tOutString.Assign(*aValue);
        }
        *aFirst = *aAny = *aAll =
            CSSEditUtils::IsCSSEquivalentToHTMLInlineStyleSet(
                collapsedNode, &aProperty, aAttribute, tOutString,
                CSSEditUtils::eComputed);
        if (outValue) {
          outValue->Assign(tOutString);
        }
        return NS_OK;
      }

      isSet = IsTextPropertySetByContent(collapsedNode, &aProperty, aAttribute,
                                         aValue, outValue);
      *aFirst = *aAny = *aAll = isSet;
      return NS_OK;
    }

    // Non-collapsed selection

    nsAutoString firstValue, theValue;

    nsCOMPtr<nsINode> endNode = range->GetEndContainer();
    int32_t endOffset = range->EndOffset();

    PostContentIterator postOrderIter;
    DebugOnly<nsresult> rvIgnored = postOrderIter.Init(range);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "Failed to initialize post-order content iterator");
    for (; !postOrderIter.IsDone(); postOrderIter.Next()) {
      if (!postOrderIter.GetCurrentNode()->IsContent()) {
        continue;
      }
      nsCOMPtr<nsIContent> content =
          postOrderIter.GetCurrentNode()->AsContent();

      if (content->IsHTMLElement(nsGkAtoms::body)) {
        break;
      }

      // just ignore any non-editable nodes
      if (content->GetAsText() &&
          (!IsEditable(content) || IsEmptyTextNode(*content))) {
        continue;
      }
      if (content->GetAsText()) {
        if (!isCollapsed && first && firstNodeInRange) {
          firstNodeInRange = false;
          if (range->StartOffset() == content->Length()) {
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
        if (CSSEditUtils::IsCSSEditableProperty(content, &aProperty,
                                                aAttribute)) {
          // The HTML styles defined by aProperty/aAttribute have a CSS
          // equivalence in this implementation for node; let's check if it
          // carries those CSS styles
          if (aValue) {
            firstValue.Assign(*aValue);
          }
          isSet = CSSEditUtils::IsCSSEquivalentToHTMLInlineStyleSet(
              content, &aProperty, aAttribute, firstValue,
              CSSEditUtils::eComputed);
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
        if (CSSEditUtils::IsCSSEditableProperty(content, &aProperty,
                                                aAttribute)) {
          // The HTML styles defined by aProperty/aAttribute have a CSS
          // equivalence in this implementation for node; let's check if it
          // carries those CSS styles
          if (aValue) {
            theValue.Assign(*aValue);
          }
          isSet = CSSEditUtils::IsCSSEquivalentToHTMLInlineStyleSet(
              content, &aProperty, aAttribute, theValue,
              CSSEditUtils::eComputed);
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
HTMLEditor::GetInlineProperty(const nsAString& aProperty,
                              const nsAString& aAttribute,
                              const nsAString& aValue, bool* aFirst, bool* aAny,
                              bool* aAll) {
  RefPtr<nsAtom> property = NS_Atomize(aProperty);
  RefPtr<nsAtom> attribute = AtomizeAttribute(aAttribute);
  return GetInlineProperty(property, attribute, aValue, aFirst, aAny, aAll);
}

nsresult HTMLEditor::GetInlineProperty(nsAtom* aProperty, nsAtom* aAttribute,
                                       const nsAString& aValue, bool* aFirst,
                                       bool* aAny, bool* aAll) const {
  if (NS_WARN_IF(!aProperty) || NS_WARN_IF(!aFirst) || NS_WARN_IF(!aAny) ||
      NS_WARN_IF(!aAll)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  const nsAString* val = nullptr;
  if (!aValue.IsEmpty()) val = &aValue;
  nsresult rv = GetInlinePropertyBase(*aProperty, aAttribute, val, aFirst, aAny,
                                      aAll, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::GetInlinePropertyWithAttrValue(const nsAString& aProperty,
                                           const nsAString& aAttribute,
                                           const nsAString& aValue,
                                           bool* aFirst, bool* aAny, bool* aAll,
                                           nsAString& outValue) {
  RefPtr<nsAtom> property = NS_Atomize(aProperty);
  RefPtr<nsAtom> attribute = AtomizeAttribute(aAttribute);
  return GetInlinePropertyWithAttrValue(property, attribute, aValue, aFirst,
                                        aAny, aAll, outValue);
}

nsresult HTMLEditor::GetInlinePropertyWithAttrValue(
    nsAtom* aProperty, nsAtom* aAttribute, const nsAString& aValue,
    bool* aFirst, bool* aAny, bool* aAll, nsAString& outValue) {
  if (NS_WARN_IF(!aProperty) || NS_WARN_IF(!aFirst) || NS_WARN_IF(!aAny) ||
      NS_WARN_IF(!aAll)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  const nsAString* val = nullptr;
  if (!aValue.IsEmpty()) val = &aValue;
  nsresult rv = GetInlinePropertyBase(*aProperty, aAttribute, val, aFirst, aAny,
                                      aAll, &outValue);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::RemoveAllInlineProperties() {
  nsresult rv = RemoveAllInlinePropertiesAsAction();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to remove all inline styles");
  return rv;
}

nsresult HTMLEditor::RemoveAllInlinePropertiesAsAction(
    nsIPrincipal* aPrincipal) {
  AutoEditActionDataSetter editActionData(
      *this, EditAction::eRemoveAllInlineStyleProperties, aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  AutoPlaceholderBatch treatAsOneTransaction(*this);
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eRemoveAllTextProperties, nsIEditor::eNext,
      ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return EditorBase::ToGenericNSResult(ignoredError.StealNSResult());
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  nsresult rv = RemoveInlinePropertyInternal(nullptr, nullptr);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "RemoveInlinePropertyInternal() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult HTMLEditor::RemoveInlinePropertyAsAction(nsAtom& aProperty,
                                                  nsAtom* aAttribute,
                                                  nsIPrincipal* aPrincipal) {
  AutoEditActionDataSetter editActionData(
      *this,
      HTMLEditUtils::GetEditActionForFormatText(aProperty, aAttribute, false),
      aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  switch (editActionData.GetEditAction()) {
    case EditAction::eRemoveFontFamilyProperty:
      MOZ_ASSERT(!EmptyString().IsVoid());
      editActionData.SetData(EmptyString());
      break;
    case EditAction::eRemoveColorProperty:
    case EditAction::eRemoveBackgroundColorPropertyInline:
      editActionData.SetColorData(EmptyString());
      break;
    default:
      break;
  }
  nsresult rv = RemoveInlinePropertyInternal(&aProperty, aAttribute);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "RemoveInlinePropertyInternal() failed");
  return EditorBase::ToGenericNSResult(rv);
}

NS_IMETHODIMP
HTMLEditor::RemoveInlineProperty(const nsAString& aProperty,
                                 const nsAString& aAttribute) {
  RefPtr<nsAtom> property = NS_Atomize(aProperty);
  RefPtr<nsAtom> attribute = AtomizeAttribute(aAttribute);

  AutoEditActionDataSetter editActionData(
      *this,
      HTMLEditUtils::GetEditActionForFormatText(*property, attribute, false));
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  switch (editActionData.GetEditAction()) {
    case EditAction::eRemoveFontFamilyProperty:
      MOZ_ASSERT(!EmptyString().IsVoid());
      editActionData.SetData(EmptyString());
      break;
    case EditAction::eRemoveColorProperty:
    case EditAction::eRemoveBackgroundColorPropertyInline:
      editActionData.SetColorData(EmptyString());
      break;
    default:
      break;
  }
  nsresult rv = RemoveInlinePropertyInternal(property, attribute);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "RemoveInlinePropertyInternal() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult HTMLEditor::RemoveInlinePropertyInternal(nsAtom* aProperty,
                                                  nsAtom* aAttribute) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(aAttribute != nsGkAtoms::_empty);

  if (NS_WARN_IF(!mInitSucceeded)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  CommitComposition();

  if (SelectionRefPtr()->IsCollapsed()) {
    // Manipulating text attributes on a collapsed selection only sets state
    // for the next text insertion

    // For links, aProperty uses "href", use "a" instead
    if (aProperty == nsGkAtoms::href || aProperty == nsGkAtoms::name) {
      aProperty = nsGkAtoms::a;
    }

    if (aProperty) {
      mTypeInState->ClearProp(aProperty, aAttribute);
    } else {
      mTypeInState->ClearAllProps();
    }
    return NS_OK;
  }

  // XXX Shouldn't we quit before calling `CommitComposition()`?
  if (IsPlaintextEditor()) {
    return NS_OK;
  }

  EditActionResult result = CanHandleHTMLEditSubAction();
  if (NS_WARN_IF(result.Failed()) || result.Canceled()) {
    return result.Rv();
  }

  AutoPlaceholderBatch treatAsOneTransaction(*this);
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eRemoveTextProperty, nsIEditor::eNext,
      ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  {
    AutoSelectionRestorer restoreSelectionLater(*this);
    AutoTransactionsConserveSelection dontChangeMySelection(*this);

    // Loop through the ranges in the selection.
    // XXX Although `Selection` will be restored by AutoSelectionRestorer,
    //     AutoRangeArray just grabs the ranges in `Selection`.  Therefore,
    //     modifying each range may notify selection listener.  So perhaps,
    //     we should clone each range here instead.
    AutoRangeArray arrayOfRanges(SelectionRefPtr());
    for (auto& range : arrayOfRanges.mRanges) {
      if (aProperty == nsGkAtoms::name) {
        // Promote range if it starts or end in a named anchor and we want to
        // remove named anchors
        nsresult rv = PromoteRangeIfStartsOrEndsInNamedAnchor(*range);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      } else {
        // Adjust range to include any ancestors whose children are entirely
        // selected
        nsresult rv = PromoteInlineRange(*range);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }

      // Remove this style from ancestors of our range endpoints, splitting
      // them as appropriate
      SplitRangeOffResult splitRangeOffResult =
          SplitAncestorStyledInlineElementsAtRangeEdges(
              EditorDOMPoint(range->StartRef()),
              EditorDOMPoint(range->EndRef()), aProperty, aAttribute);
      if (NS_WARN_IF(splitRangeOffResult.Failed())) {
        return splitRangeOffResult.Rv();
      }

      // XXX Modifying `range` means that we may modify ranges in `Selection`.
      //     Is this intentional?  Note that the range may be not in
      //     `Selection` too.  It seems that at least one of them is not
      //     an unexpected case.
      EditorDOMPoint startOfRange(splitRangeOffResult.SplitPointAtStart());
      EditorDOMPoint endOfRange(splitRangeOffResult.SplitPointAtEnd());
      if (NS_WARN_IF(!startOfRange.IsSet()) ||
          NS_WARN_IF(!endOfRange.IsSet())) {
        continue;
      }

      nsresult rv = range->SetStartAndEnd(startOfRange.ToRawRangeBoundary(),
                                          endOfRange.ToRawRangeBoundary());
      // Note that modifying a range in `Selection` may run script so that
      // we might have been destroyed here.
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      // Collect editable nodes which are entirely contained in the range.
      AutoTArray<OwningNonNull<nsIContent>, 64> arrayOfContents;
      if (startOfRange.GetContainer() == endOfRange.GetContainer() &&
          startOfRange.IsInTextNode()) {
        if (!IsEditable(startOfRange.GetContainer())) {
          continue;
        }
        arrayOfContents.AppendElement(*startOfRange.GetContainerAsText());
      } else if (startOfRange.IsInTextNode() && endOfRange.IsInTextNode() &&
                 startOfRange.GetContainer()->GetNextSibling() ==
                     endOfRange.GetContainer()) {
        if (IsEditable(startOfRange.GetContainer())) {
          arrayOfContents.AppendElement(*startOfRange.GetContainerAsText());
        }
        if (IsEditable(endOfRange.GetContainer())) {
          arrayOfContents.AppendElement(*endOfRange.GetContainerAsText());
        }
        if (arrayOfContents.IsEmpty()) {
          continue;
        }
      } else {
        // Append first node if it's a text node but selected not entirely.
        if (startOfRange.IsInTextNode() && !startOfRange.IsStartOfContainer() &&
            IsEditable(startOfRange.GetContainer())) {
          arrayOfContents.AppendElement(*startOfRange.GetContainerAsText());
        }
        // Append all entirely selected nodes.
        ContentSubtreeIterator subtreeIter;
        if (NS_SUCCEEDED(subtreeIter.Init(range))) {
          for (; !subtreeIter.IsDone(); subtreeIter.Next()) {
            nsCOMPtr<nsINode> node = subtreeIter.GetCurrentNode();
            if (NS_WARN_IF(!node)) {
              return NS_ERROR_FAILURE;
            }
            if (node->IsContent() && IsEditable(node)) {
              arrayOfContents.AppendElement(*node->AsContent());
            }
          }
        }
        // Append last node if it's a text node but selected not entirely.
        if (startOfRange.GetContainer() != endOfRange.GetContainer() &&
            endOfRange.IsInTextNode() && !endOfRange.IsEndOfContainer() &&
            IsEditable(endOfRange.GetContainer())) {
          arrayOfContents.AppendElement(*endOfRange.GetContainerAsText());
        }
      }

      for (auto& content : arrayOfContents) {
        if (content->IsElement()) {
          nsresult rv = RemoveStyleInside(MOZ_KnownLive(*content->AsElement()),
                                          aProperty, aAttribute);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
        }

        if (!HTMLEditor::IsRemovableParentStyleWithNewSpanElement(
                content, aProperty, aAttribute)) {
          continue;
        }

        if (!content->IsText()) {
          // XXX Do we need to call this even when data node or something?  If
          //     so, for what?
          DebugOnly<nsresult> rvIgnored = SetInlinePropertyOnNode(
              content, *aProperty, aAttribute,
              NS_LITERAL_STRING("-moz-editor-invert-value"));
          if (NS_WARN_IF(Destroyed())) {
            return NS_ERROR_EDITOR_DESTROYED;
          }
          NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                               "SetInlinePropertyOnNode() "
                               "failed, but ignored");
          continue;
        }

        // If current node is a text node, we need to create `<span>` element
        // for it to overwrite parent style.  Unfortunately, all browsers
        // don't join text nodes when removing a style.  Therefore, there
        // may be multiple text nodes as adjacent siblings.  That's the
        // reason why we need to handle text nodes in this loop.
        uint32_t startOffset =
            content == startOfRange.GetContainer() ? startOfRange.Offset() : 0;
        uint32_t endOffset = content == endOfRange.GetContainer()
                                 ? endOfRange.Offset()
                                 : content->Length();
        nsresult rv = SetInlinePropertyOnTextNode(
            MOZ_KnownLive(*content->AsText()), startOffset, endOffset,
            *aProperty, aAttribute,
            NS_LITERAL_STRING("-moz-editor-invert-value"));
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }

      // For avoiding unnecessary loop cost, check whether the style is
      // invertible first.
      if (aProperty && CSSEditUtils::IsCSSInvertible(*aProperty, aAttribute)) {
        // Finally, we should remove the style from all leaf text nodes if they
        // still have the style.
        AutoTArray<OwningNonNull<Text>, 32> leafTextNodes;
        for (auto& content : arrayOfContents) {
          if (content->IsElement()) {
            CollectEditableLeafTextNodes(*content->AsElement(), leafTextNodes);
          }
        }
        for (auto& textNode : leafTextNodes) {
          if (!HTMLEditor::IsRemovableParentStyleWithNewSpanElement(
                  textNode, aProperty, aAttribute)) {
            continue;
          }
          nsresult rv = SetInlinePropertyOnTextNode(
              textNode, 0, textNode->TextLength(), *aProperty, aAttribute,
              NS_LITERAL_STRING("-moz-editor-invert-value"));
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
        }
      }
    }
  }

  // Restoring `Selection` may cause destroying us.
  return NS_WARN_IF(Destroyed()) ? NS_ERROR_EDITOR_DESTROYED : NS_OK;
}

// static
bool HTMLEditor::IsRemovableParentStyleWithNewSpanElement(nsIContent& aContent,
                                                          nsAtom* aProperty,
                                                          nsAtom* aAttribute) {
  // We don't support to remove all inline styles with this path.
  if (!aProperty) {
    return false;
  }

  // First check whether the style is invertible since this is the fastest
  // check.
  if (!CSSEditUtils::IsCSSInvertible(*aProperty, aAttribute)) {
    return false;
  }

  // If parent block has the removing style, we should create `<span>`
  // element to remove the style even in HTML mode since Chrome does it.
  if (!CSSEditUtils::IsCSSEditableProperty(&aContent, aProperty, aAttribute)) {
    return false;
  }

  // aContent's computed style indicates the CSS equivalence to
  // the HTML style to remove is applied; but we found no element
  // in the ancestors of aContent carrying specified styles;
  // assume it comes from a rule and let's try to insert a span
  // "inverting" the style
  return CSSEditUtils::IsCSSEquivalentToHTMLInlineStyleSet(
      &aContent, aProperty, aAttribute, EmptyString(), CSSEditUtils::eComputed);
}

void HTMLEditor::CollectEditableLeafTextNodes(
    Element& aElement, nsTArray<OwningNonNull<Text>>& aLeafTextNodes) const {
  for (nsIContent* child = aElement.GetFirstChild(); child;
       child = child->GetNextSibling()) {
    if (child->IsElement()) {
      CollectEditableLeafTextNodes(*child->AsElement(), aLeafTextNodes);
      continue;
    }
    if (child->IsText()) {
      aLeafTextNodes.AppendElement(*child->AsText());
    }
  }
}

NS_IMETHODIMP
HTMLEditor::IncreaseFontSize() {
  nsresult rv = IncreaseFontSizeAsAction();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to increase font size");
  return rv;
}

nsresult HTMLEditor::IncreaseFontSizeAsAction(nsIPrincipal* aPrincipal) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eIncrementFontSize,
                                          aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = RelativeFontChange(FontSize::incr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::DecreaseFontSize() {
  nsresult rv = DecreaseFontSizeAsAction();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to decrease font size");
  return rv;
}

nsresult HTMLEditor::DecreaseFontSizeAsAction(nsIPrincipal* aPrincipal) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eDecrementFontSize,
                                          aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = RelativeFontChange(FontSize::decr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

nsresult HTMLEditor::RelativeFontChange(FontSize aDir) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  CommitComposition();

  // If selection is collapsed, set typing state
  if (SelectionRefPtr()->IsCollapsed()) {
    nsAtom& atom = aDir == FontSize::incr ? *nsGkAtoms::big : *nsGkAtoms::small;

    // Let's see in what kind of element the selection is
    if (NS_WARN_IF(!SelectionRefPtr()->RangeCount())) {
      return NS_OK;
    }
    RefPtr<nsRange> firstRange = SelectionRefPtr()->GetRangeAt(0);
    if (NS_WARN_IF(!firstRange) ||
        NS_WARN_IF(!firstRange->GetStartContainer())) {
      return NS_OK;
    }
    OwningNonNull<nsINode> selectedNode = *firstRange->GetStartContainer();
    if (IsTextNode(selectedNode)) {
      if (NS_WARN_IF(!selectedNode->GetParentNode())) {
        return NS_OK;
      }
      selectedNode = *selectedNode->GetParentNode();
    }
    if (!CanContainTag(selectedNode, atom)) {
      return NS_OK;
    }

    // Manipulating text attributes on a collapsed selection only sets state
    // for the next text insertion
    mTypeInState->SetProp(&atom, nullptr, EmptyString());
    return NS_OK;
  }

  // Wrap with txn batching, rules sniffing, and selection preservation code
  AutoPlaceholderBatch treatAsOneTransaction(*this);
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eSetTextProperty, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  AutoSelectionRestorer restoreSelectionLater(*this);
  AutoTransactionsConserveSelection dontChangeMySelection(*this);

  // Loop through the ranges in the selection
  AutoRangeArray arrayOfRanges(SelectionRefPtr());
  for (auto& range : arrayOfRanges.mRanges) {
    // Adjust range to include any ancestors with entirely selected children
    nsresult rv = PromoteInlineRange(*range);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // Check for easy case: both range endpoints in same text node
    nsCOMPtr<nsINode> startNode = range->GetStartContainer();
    nsCOMPtr<nsINode> endNode = range->GetEndContainer();
    if (startNode == endNode && IsTextNode(startNode)) {
      rv = RelativeFontChangeOnTextNode(
          aDir, MOZ_KnownLive(*startNode->GetAsText()), range->StartOffset(),
          range->EndOffset());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
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

      // Iterate range and build up array
      ContentSubtreeIterator subtreeIter;
      rv = subtreeIter.Init(range);
      if (NS_SUCCEEDED(rv)) {
        nsTArray<OwningNonNull<nsIContent>> arrayOfNodes;
        for (; !subtreeIter.IsDone(); subtreeIter.Next()) {
          if (NS_WARN_IF(!subtreeIter.GetCurrentNode()->IsContent())) {
            return NS_ERROR_FAILURE;
          }
          OwningNonNull<nsIContent> node =
              *subtreeIter.GetCurrentNode()->AsContent();

          if (IsEditable(node)) {
            arrayOfNodes.AppendElement(node);
          }
        }

        // Now that we have the list, do the font size change on each node
        for (auto& node : arrayOfNodes) {
          rv = RelativeFontChangeOnNode(aDir == FontSize::incr ? +1 : -1, node);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
        }
      }
      // Now check the start and end parents of the range to see if they need
      // to be separately handled (they do if they are text nodes, due to how
      // the subtree iterator works - it will not have reported them).
      if (IsTextNode(startNode) && IsEditable(startNode)) {
        rv = RelativeFontChangeOnTextNode(
            aDir, MOZ_KnownLive(*startNode->GetAsText()), range->StartOffset(),
            startNode->Length());
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
      if (IsTextNode(endNode) && IsEditable(endNode)) {
        rv = RelativeFontChangeOnTextNode(
            aDir, MOZ_KnownLive(*endNode->GetAsText()), 0, range->EndOffset());
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
    }
  }

  return NS_OK;
}

nsresult HTMLEditor::RelativeFontChangeOnTextNode(FontSize aDir,
                                                  Text& aTextNode,
                                                  int32_t aStartOffset,
                                                  int32_t aEndOffset) {
  // Don't need to do anything if no characters actually selected
  if (aStartOffset == aEndOffset) {
    return NS_OK;
  }

  if (!aTextNode.GetParentNode() ||
      !CanContainTag(*aTextNode.GetParentNode(), *nsGkAtoms::big)) {
    return NS_OK;
  }

  // -1 is a magic value meaning to the end of node
  if (aEndOffset == -1) {
    aEndOffset = aTextNode.Length();
  }

  // Make the range an independent node.
  nsCOMPtr<nsIContent> textNodeForTheRange = &aTextNode;

  // Split at the end of the range.
  EditorDOMPoint atEnd(textNodeForTheRange, aEndOffset);
  if (!atEnd.IsEndOfContainer()) {
    // We need to split off back of text node
    ErrorResult error;
    textNodeForTheRange = SplitNodeWithTransaction(atEnd, error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
  }

  // Split at the start of the range.
  EditorDOMPoint atStart(textNodeForTheRange, aStartOffset);
  if (!atStart.IsStartOfContainer()) {
    // We need to split off front of text node
    ErrorResult error;
    nsCOMPtr<nsIContent> newLeftNode = SplitNodeWithTransaction(atStart, error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
    Unused << newLeftNode;
  }

  // Look for siblings that are correct type of node
  nsAtom* nodeType = aDir == FontSize::incr ? nsGkAtoms::big : nsGkAtoms::small;
  nsCOMPtr<nsIContent> sibling = GetPriorHTMLSibling(textNodeForTheRange);
  if (sibling && sibling->IsHTMLElement(nodeType)) {
    // Previous sib is already right kind of inline node; slide this over
    nsresult rv = MoveNodeToEndWithTransaction(*textNodeForTheRange, *sibling);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }
  sibling = GetNextHTMLSibling(textNodeForTheRange);
  if (sibling && sibling->IsHTMLElement(nodeType)) {
    // Following sib is already right kind of inline node; slide this over
    nsresult rv = MoveNodeWithTransaction(*textNodeForTheRange,
                                          EditorDOMPoint(sibling, 0));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  // Else reparent the node inside font node with appropriate relative size
  RefPtr<Element> newElement = InsertContainerWithTransaction(
      *textNodeForTheRange, MOZ_KnownLive(*nodeType));
  if (NS_WARN_IF(!newElement)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult HTMLEditor::RelativeFontChangeHelper(int32_t aSizeChange,
                                              nsINode* aNode) {
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
    AutoTArray<nsCOMPtr<nsIContent>, 10> childList;
    for (nsIContent* child = aNode->GetFirstChild(); child;
         child = child->GetNextSibling()) {
      childList.AppendElement(child);
    }

    for (const auto& child : childList) {
      nsresult rv = RelativeFontChangeOnNode(aSizeChange, child);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // RelativeFontChangeOnNode already calls us recursively,
    // so we don't need to check our children again.
    return NS_OK;
  }

  // Otherwise cycle through the children.
  AutoTArray<nsCOMPtr<nsIContent>, 10> childList;
  for (nsIContent* child = aNode->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    childList.AppendElement(child);
  }

  for (const auto& child : childList) {
    nsresult rv = RelativeFontChangeHelper(aSizeChange, child);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult HTMLEditor::RelativeFontChangeOnNode(int32_t aSizeChange,
                                              nsIContent* aNode) {
  MOZ_ASSERT(aNode);
  // Can only change font size by + or - 1
  if (aSizeChange != 1 && aSizeChange != -1) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsAtom* atom;
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
    return RemoveContainerWithTransaction(MOZ_KnownLive(*aNode->AsElement()));
  }

  // can it be put inside a "big" or "small"?
  if (TagCanContain(*atom, *aNode)) {
    // first populate any nested font tags that have the size attr set
    nsresult rv = RelativeFontChangeHelper(aSizeChange, aNode);
    NS_ENSURE_SUCCESS(rv, rv);

    // ok, chuck it in.
    // first look at siblings of aNode for matching bigs or smalls.
    // if we find one, move aNode into it.
    nsCOMPtr<nsIContent> sibling = GetPriorHTMLSibling(aNode);
    if (sibling && sibling->IsHTMLElement(atom)) {
      // previous sib is already right kind of inline node; slide this over into
      // it
      return MoveNodeToEndWithTransaction(*aNode, *sibling);
    }

    sibling = GetNextHTMLSibling(aNode);
    if (sibling && sibling->IsHTMLElement(atom)) {
      // following sib is already right kind of inline node; slide this over
      // into it
      return MoveNodeWithTransaction(*aNode, EditorDOMPoint(sibling, 0));
    }

    // else insert it above aNode
    RefPtr<Element> newElement =
        InsertContainerWithTransaction(*aNode, MOZ_KnownLive(*atom));
    if (NS_WARN_IF(!newElement)) {
      return NS_ERROR_FAILURE;
    }
    return NS_OK;
  }

  // none of the above?  then cycle through the children.
  // MOOSE: we should group the children together if possible
  // into a single "big" or "small".  For the moment they are
  // each getting their own.
  AutoTArray<nsCOMPtr<nsIContent>, 10> childList;
  for (nsIContent* child = aNode->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    childList.AppendElement(child);
  }

  for (const auto& child : childList) {
    nsresult rv = RelativeFontChangeOnNode(aSizeChange, child);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::GetFontFaceState(bool* aMixed, nsAString& outFace) {
  if (NS_WARN_IF(!aMixed)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aMixed = true;
  outFace.Truncate();

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  bool first, any, all;

  nsresult rv = GetInlinePropertyBase(*nsGkAtoms::font, nsGkAtoms::face,
                                      nullptr, &first, &any, &all, &outFace);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  if (any && !all) {
    return NS_OK;  // mixed
  }
  if (all) {
    *aMixed = false;
    return NS_OK;
  }

  // if there is no font face, check for tt
  rv = GetInlinePropertyBase(*nsGkAtoms::tt, nullptr, nullptr, &first, &any,
                             &all, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  if (any && !all) {
    return EditorBase::ToGenericNSResult(rv);  // mixed
  }
  if (all) {
    *aMixed = false;
    outFace.AssignLiteral("tt");
  }

  if (!any) {
    // there was no font face attrs of any kind.  We are in normal font.
    outFace.Truncate();
    *aMixed = false;
  }
  return NS_OK;
}

nsresult HTMLEditor::GetFontColorState(bool* aMixed, nsAString& aOutColor) {
  if (NS_WARN_IF(!aMixed)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aMixed = true;
  aOutColor.Truncate();

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  bool first, any, all;
  nsresult rv = GetInlinePropertyBase(*nsGkAtoms::font, nsGkAtoms::color,
                                      nullptr, &first, &any, &all, &aOutColor);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }

  if (any && !all) {
    return NS_OK;  // mixed
  }
  if (all) {
    *aMixed = false;
    return NS_OK;
  }

  if (!any) {
    // there was no font color attrs of any kind..
    aOutColor.Truncate();
    *aMixed = false;
  }
  return NS_OK;
}

// the return value is true only if the instance of the HTML editor we created
// can handle CSS styles (for instance, Composer can, Messenger can't) and if
// the CSS preference is checked
NS_IMETHODIMP
HTMLEditor::GetIsCSSEnabled(bool* aIsCSSEnabled) {
  *aIsCSSEnabled = IsCSSEnabled();
  return NS_OK;
}

bool HTMLEditor::HasStyleOrIdOrClassAttribute(Element& aElement) {
  return aElement.HasNonEmptyAttr(nsGkAtoms::style) ||
         aElement.HasNonEmptyAttr(nsGkAtoms::_class) ||
         aElement.HasNonEmptyAttr(nsGkAtoms::id);
}

}  // namespace mozilla
