/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ErrorList.h"
#include "HTMLEditor.h"

#include "AutoRangeArray.h"
#include "EditAction.h"
#include "EditorUtils.h"
#include "HTMLEditHelpers.h"
#include "HTMLEditUtils.h"
#include "PendingStyles.h"
#include "SelectionState.h"

#include "mozilla/Assertions.h"
#include "mozilla/ContentIterator.h"
#include "mozilla/EditorForwards.h"
#include "mozilla/mozalloc.h"
#include "mozilla/dom/AncestorIterator.h"
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
#include "nsStyledElement.h"
#include "nsTArray.h"
#include "nsUnicharUtils.h"
#include "nscore.h"

class nsISupports;

namespace mozilla {

using namespace dom;

using EmptyCheckOption = HTMLEditUtils::EmptyCheckOption;
using LeafNodeType = HTMLEditUtils::LeafNodeType;
using LeafNodeTypes = HTMLEditUtils::LeafNodeTypes;
using WalkTreeOption = HTMLEditUtils::WalkTreeOption;

template nsresult HTMLEditor::SetInlinePropertiesAsSubAction(
    const AutoTArray<EditorInlineStyleAndValue, 1>& aStylesToSet);
template nsresult HTMLEditor::SetInlinePropertiesAsSubAction(
    const AutoTArray<EditorInlineStyleAndValue, 32>& aStylesToSet);

nsresult HTMLEditor::SetInlinePropertyAsAction(nsStaticAtom& aProperty,
                                               nsStaticAtom* aAttribute,
                                               const nsAString& aValue,
                                               nsIPrincipal* aPrincipal) {
  AutoEditActionDataSetter editActionData(
      *this,
      HTMLEditUtils::GetEditActionForFormatText(aProperty, aAttribute, true),
      aPrincipal);
  switch (editActionData.GetEditAction()) {
    case EditAction::eSetFontFamilyProperty:
      MOZ_ASSERT(!aValue.IsVoid());
      // XXX Should we trim unnecessary white-spaces?
      editActionData.SetData(aValue);
      break;
    case EditAction::eSetColorProperty:
    case EditAction::eSetBackgroundColorPropertyInline:
      editActionData.SetColorData(aValue);
      break;
    default:
      break;
  }

  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  // XXX Due to bug 1659276 and bug 1659924, we should not scroll selection
  //     into view after setting the new style.
  AutoPlaceholderBatch treatAsOneTransaction(*this, ScrollSelectionIntoView::No,
                                             __FUNCTION__);

  nsStaticAtom* property = &aProperty;
  nsStaticAtom* attribute = aAttribute;
  nsString value(aValue);

  AutoTArray<EditorInlineStyle, 1> stylesToRemove;
  if (&aProperty == nsGkAtoms::sup) {
    // Superscript and Subscript styles are mutually exclusive.
    stylesToRemove.AppendElement(EditorInlineStyle(*nsGkAtoms::sub));
  } else if (&aProperty == nsGkAtoms::sub) {
    // Superscript and Subscript styles are mutually exclusive.
    stylesToRemove.AppendElement(EditorInlineStyle(*nsGkAtoms::sup));
  }
  // Handling `<tt>` element code was implemented for composer (bug 115922).
  // This shouldn't work with `Document.execCommand()`.  Currently, aPrincipal
  // is set only when the root caller is Document::ExecCommand() so that
  // we should handle `<tt>` element only when aPrincipal is nullptr that
  // must be only when XUL command is executed on composer.
  else if (!aPrincipal) {
    if (&aProperty == nsGkAtoms::tt) {
      stylesToRemove.AppendElement(
          EditorInlineStyle(*nsGkAtoms::font, nsGkAtoms::face));
    } else if (&aProperty == nsGkAtoms::font && aAttribute == nsGkAtoms::face) {
      if (!value.LowerCaseEqualsASCII("tt")) {
        stylesToRemove.AppendElement(EditorInlineStyle(*nsGkAtoms::tt));
      } else {
        stylesToRemove.AppendElement(
            EditorInlineStyle(*nsGkAtoms::font, nsGkAtoms::face));
        // Override property, attribute and value if the new font face value is
        // "tt".
        property = nsGkAtoms::tt;
        attribute = nullptr;
        value.Truncate();
      }
    }
  }

  if (!stylesToRemove.IsEmpty()) {
    nsresult rv = RemoveInlinePropertiesAsSubAction(stylesToRemove);
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::RemoveInlinePropertiesAsSubAction() failed");
      return rv;
    }
  }

  AutoTArray<EditorInlineStyleAndValue, 1> styleToSet;
  styleToSet.AppendElement(
      attribute
          ? EditorInlineStyleAndValue(*property, *attribute, std::move(value))
          : EditorInlineStyleAndValue(*property));
  rv = SetInlinePropertiesAsSubAction(styleToSet);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::SetInlinePropertiesAsSubAction() failed");
  return EditorBase::ToGenericNSResult(rv);
}

NS_IMETHODIMP HTMLEditor::SetInlineProperty(const nsAString& aProperty,
                                            const nsAString& aAttribute,
                                            const nsAString& aValue) {
  nsStaticAtom* property = NS_GetStaticAtom(aProperty);
  if (NS_WARN_IF(!property)) {
    return NS_ERROR_INVALID_ARG;
  }
  nsStaticAtom* attribute = EditorUtils::GetAttributeAtom(aAttribute);
  AutoEditActionDataSetter editActionData(
      *this,
      HTMLEditUtils::GetEditActionForFormatText(*property, attribute, true));
  switch (editActionData.GetEditAction()) {
    case EditAction::eSetFontFamilyProperty:
      MOZ_ASSERT(!aValue.IsVoid());
      // XXX Should we trim unnecessary white-spaces?
      editActionData.SetData(aValue);
      break;
    case EditAction::eSetColorProperty:
    case EditAction::eSetBackgroundColorPropertyInline:
      editActionData.SetColorData(aValue);
      break;
    default:
      break;
  }
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  AutoTArray<EditorInlineStyleAndValue, 1> styleToSet;
  styleToSet.AppendElement(
      attribute ? EditorInlineStyleAndValue(*property, *attribute, aValue)
                : EditorInlineStyleAndValue(*property));
  rv = SetInlinePropertiesAsSubAction(styleToSet);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::SetInlinePropertiesAsSubAction() failed");
  return EditorBase::ToGenericNSResult(rv);
}

template <size_t N>
nsresult HTMLEditor::SetInlinePropertiesAsSubAction(
    const AutoTArray<EditorInlineStyleAndValue, N>& aStylesToSet) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!aStylesToSet.IsEmpty());

  DebugOnly<nsresult> rvIgnored = CommitComposition();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "EditorBase::CommitComposition() failed, but ignored");

  if (SelectionRef().IsCollapsed()) {
    // Manipulating text attributes on a collapsed selection only sets state
    // for the next text insertion
    mPendingStylesToApplyToNewContent->PreserveStyles(aStylesToSet);
    return NS_OK;
  }

  // XXX Shouldn't we return before calling `CommitComposition()`?
  if (IsInPlaintextMode()) {
    return NS_OK;
  }

  EditActionResult result = CanHandleHTMLEditSubAction();
  if (result.Failed() || result.Canceled()) {
    NS_WARNING_ASSERTION(result.Succeeded(),
                         "HTMLEditor::CanHandleHTMLEditSubAction() failed");
    return result.Rv();
  }

  AutoPlaceholderBatch treatAsOneTransaction(
      *this, ScrollSelectionIntoView::Yes, __FUNCTION__);
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eInsertElement, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  // TODO: We don't need AutoTransactionsConserveSelection here in the normal
  //       cases, but removing this may cause the behavior with the legacy
  //       mutation event listeners.  We should try to delete this in a bug.
  AutoTransactionsConserveSelection dontChangeMySelection(*this);

  AutoRangeArray selectionRanges(SelectionRef());
  for (const EditorInlineStyleAndValue& styleToSet : aStylesToSet) {
    // The ranges may be updated by changing the DOM tree.  In strictly
    // speaking, we should save and restore the ranges at every range loop,
    // but we've never done so and it may be expensive if there are a lot of
    // ranges.  Therefore, we should do it for every style handling for now.
    // TODO: We should collect everything required for removing the style before
    //       touching the DOM tree.  Then, we need to save and restore the
    //       ranges only once.
    MOZ_ALWAYS_TRUE(selectionRanges.SaveAndTrackRanges(*this));
    for (const OwningNonNull<nsRange>& selectionRange :
         selectionRanges.Ranges()) {
      // Adjust range to include any ancestors whose children are entirely
      // selected
      nsresult rv = PromoteInlineRange(*selectionRange);
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::PromoteInlineRange() failed");
        return rv;
      }

      EditorDOMRange range(selectionRange);
      if (NS_WARN_IF(!range.IsPositioned())) {
        continue;
      }

      // If range is in a text node, apply new style simply.
      if (range.InSameContainer() && range.StartRef().IsInTextNode()) {
        // MOZ_KnownLive(...ContainerAs<Text>()) because of grabbed by `range`.
        // MOZ_KnownLive(styleToSet.*) due to bug 1622253.
        SplitRangeOffFromNodeResult wrapTextInStyledElementResult =
            SetInlinePropertyOnTextNode(
                MOZ_KnownLive(*range.StartRef().ContainerAs<Text>()),
                range.StartRef().Offset(), range.EndRef().Offset(),
                MOZ_KnownLive(styleToSet.HTMLPropertyRef()),
                MOZ_KnownLive(styleToSet.mAttribute),
                styleToSet.mAttributeValue);
        if (wrapTextInStyledElementResult.isErr()) {
          NS_WARNING("HTMLEditor::SetInlinePropertyOnTextNode() failed");
          return wrapTextInStyledElementResult.unwrapErr();
        }
        // There is AutoTransactionsConserveSelection, so we don't need to
        // update selection here.
        wrapTextInStyledElementResult.IgnoreCaretPointSuggestion();
        continue;
      }

      // Collect editable nodes which are entirely contained in the range.
      AutoTArray<OwningNonNull<nsIContent>, 64> arrayOfContentsAroundRange;
      {
        ContentSubtreeIterator subtreeIter;
        // If there is no node which is entirely in the range,
        // `ContentSubtreeIterator::Init()` fails, but this is possible case,
        // don't warn it.
        if (NS_SUCCEEDED(
                subtreeIter.Init(range.StartRef().ToRawRangeBoundary(),
                                 range.EndRef().ToRawRangeBoundary()))) {
          for (; !subtreeIter.IsDone(); subtreeIter.Next()) {
            nsINode* node = subtreeIter.GetCurrentNode();
            if (NS_WARN_IF(!node)) {
              return NS_ERROR_FAILURE;
            }
            if (node->IsContent() &&
                EditorUtils::IsEditableContent(*node->AsContent(),
                                               EditorType::HTML)) {
              arrayOfContentsAroundRange.AppendElement(*node->AsContent());
            }
          }
        }
      }

      // If start node is a text node, apply new style to a part of it.
      if (range.StartRef().IsInTextNode() &&
          EditorUtils::IsEditableContent(*range.StartRef().ContainerAs<Text>(),
                                         EditorType::HTML)) {
        // MOZ_KnownLive(...ContainerAs<Text>()) because of grabbed by `range`.
        // MOZ_KnownLive(styleToSet.*) due to bug 1622253.
        SplitRangeOffFromNodeResult wrapTextInStyledElementResult =
            SetInlinePropertyOnTextNode(
                MOZ_KnownLive(*range.StartRef().ContainerAs<Text>()),
                range.StartRef().Offset(),
                range.StartRef().ContainerAs<Text>()->TextDataLength(),
                MOZ_KnownLive(styleToSet.HTMLPropertyRef()),
                MOZ_KnownLive(styleToSet.mAttribute),
                styleToSet.mAttributeValue);
        if (wrapTextInStyledElementResult.isErr()) {
          NS_WARNING("HTMLEditor::SetInlinePropertyOnTextNode() failed");
          return wrapTextInStyledElementResult.unwrapErr();
        }
        // There is AutoTransactionsConserveSelection, so we don't need to
        // update selection here.
        wrapTextInStyledElementResult.IgnoreCaretPointSuggestion();
      }

      // Then, apply new style to all nodes in the range entirely.
      for (auto& content : arrayOfContentsAroundRange) {
        // MOZ_KnownLive due to bug 1622253.
        Result<EditorDOMPoint, nsresult> setStyleResult =
            SetInlinePropertyOnNode(MOZ_KnownLive(*content),
                                    MOZ_KnownLive(styleToSet.HTMLPropertyRef()),
                                    MOZ_KnownLive(styleToSet.mAttribute),
                                    styleToSet.mAttributeValue);
        if (MOZ_UNLIKELY(setStyleResult.isErr())) {
          NS_WARNING("HTMLEditor::SetInlinePropertyOnNode() failed");
          return setStyleResult.unwrapErr();
        }
        // There is AutoTransactionsConserveSelection, so we don't need to
        // update selection here.
      }

      // Finally, if end node is a text node, apply new style to a part of it.
      if (range.EndRef().IsInTextNode() &&
          EditorUtils::IsEditableContent(*range.EndRef().ContainerAs<Text>(),
                                         EditorType::HTML)) {
        // MOZ_KnownLive(...ContainerAs<Text>()) because of grabbed by `range`.
        // MOZ_KnownLive(styleToSet.mAttribute) due to bug 1622253.
        SplitRangeOffFromNodeResult wrapTextInStyledElementResult =
            SetInlinePropertyOnTextNode(
                MOZ_KnownLive(*range.EndRef().ContainerAs<Text>()), 0,
                range.EndRef().Offset(),
                MOZ_KnownLive(styleToSet.HTMLPropertyRef()),
                MOZ_KnownLive(styleToSet.mAttribute),
                styleToSet.mAttributeValue);
        if (wrapTextInStyledElementResult.isErr()) {
          NS_WARNING("HTMLEditor::SetInlinePropertyOnTextNode() failed");
          return wrapTextInStyledElementResult.unwrapErr();
        }
        // There is AutoTransactionsConserveSelection, so we don't need to
        // update selection here.
        wrapTextInStyledElementResult.IgnoreCaretPointSuggestion();
      }
    }
    MOZ_ASSERT(selectionRanges.HasSavedRanges());
    selectionRanges.RestoreFromSavedRanges();
  }

  MOZ_ASSERT(!selectionRanges.HasSavedRanges());
  nsresult rv = selectionRanges.ApplyTo(SelectionRef());
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "AutoRangeArray::ApplyTo() failed");
  return rv;
}

Result<bool, nsresult> HTMLEditor::ElementIsGoodContainerForTheStyle(
    Element& aElement, nsAtom* aProperty, nsAtom* aAttribute,
    const nsAString* aValue) {
  // aContent can be null, in which case we'll return false in a few lines
  MOZ_ASSERT(aProperty);
  MOZ_ASSERT_IF(aAttribute, aValue);

  // First check for <b>, <i>, etc.
  if (aElement.IsHTMLElement(aProperty) && !aElement.GetAttrCount() &&
      !aAttribute) {
    return true;
  }

  // Special cases for various equivalencies: <strong>, <em>, <s>
  if (!aElement.GetAttrCount() &&
      ((aProperty == nsGkAtoms::b &&
        aElement.IsHTMLElement(nsGkAtoms::strong)) ||
       (aProperty == nsGkAtoms::i && aElement.IsHTMLElement(nsGkAtoms::em)) ||
       (aProperty == nsGkAtoms::strike &&
        aElement.IsHTMLElement(nsGkAtoms::s)))) {
    return true;
  }

  // Now look for things like <font>
  if (aAttribute) {
    nsString attrValue;
    if (aElement.IsHTMLElement(aProperty) &&
        IsOnlyAttribute(&aElement, aAttribute) &&
        aElement.GetAttr(kNameSpaceID_None, aAttribute, attrValue) &&
        attrValue.Equals(*aValue, nsCaseInsensitiveStringComparator)) {
      // This is not quite correct, because it excludes cases like
      // <font face=000> being the same as <font face=#000000>.
      // Property-specific handling is needed (bug 760211).
      return true;
    }
  }

  // No luck so far.  Now we check for a <span> with a single style=""
  // attribute that sets only the style we're looking for, if this type of
  // style supports it
  if (!CSSEditUtils::IsCSSEditableProperty(&aElement, aProperty, aAttribute) ||
      !aElement.IsHTMLElement(nsGkAtoms::span) ||
      aElement.GetAttrCount() != 1 ||
      !aElement.HasAttr(kNameSpaceID_None, nsGkAtoms::style)) {
    return false;
  }

  // Some CSS styles are not so simple.  For instance, underline is
  // "text-decoration: underline", which decomposes into four different text-*
  // properties.  So for now, we just create a span, add the desired style, and
  // see if it matches.
  RefPtr<Element> newSpanElement = CreateHTMLContent(nsGkAtoms::span);
  if (!newSpanElement) {
    NS_WARNING("EditorBase::CreateHTMLContent(nsGkAtoms::span) failed");
    return false;
  }
  nsStyledElement* styledNewSpanElement =
      nsStyledElement::FromNode(newSpanElement);
  if (!styledNewSpanElement) {
    return false;
  }
  // MOZ_KnownLive(*styledNewSpanElement): It's newSpanElement whose type is
  // RefPtr.
  Result<int32_t, nsresult> result =
      CSSEditUtils::SetCSSEquivalentToHTMLStyleWithoutTransaction(
          *this, MOZ_KnownLive(*styledNewSpanElement), aProperty, aAttribute,
          aValue);
  if (result.isErr()) {
    // The call shouldn't return destroyed error because it must be
    // impossible to run script with modifying the new orphan node.
    MOZ_ASSERT_UNREACHABLE("How did you destroy this editor?");
    if (NS_WARN_IF(result.inspectErr() == NS_ERROR_EDITOR_DESTROYED)) {
      return Err(NS_ERROR_EDITOR_DESTROYED);
    }
    return false;
  }
  nsStyledElement* styledElement = nsStyledElement::FromNode(&aElement);
  if (!styledElement) {
    return false;
  }
  return CSSEditUtils::DoStyledElementsHaveSameStyle(*styledNewSpanElement,
                                                     *styledElement);
}

SplitRangeOffFromNodeResult HTMLEditor::SetInlinePropertyOnTextNode(
    Text& aText, uint32_t aStartOffset, uint32_t aEndOffset, nsAtom& aProperty,
    nsAtom* aAttribute, const nsAString& aValue) {
  if (!aText.GetParentNode() ||
      !HTMLEditUtils::CanNodeContain(*aText.GetParentNode(), aProperty)) {
    return SplitRangeOffFromNodeResult(nullptr, &aText, nullptr);
  }

  // Don't need to do anything if no characters actually selected
  if (aStartOffset == aEndOffset) {
    return SplitRangeOffFromNodeResult(nullptr, &aText, nullptr);
  }

  // Don't need to do anything if property already set on node
  if (CSSEditUtils::IsCSSEditableProperty(&aText, &aProperty, aAttribute)) {
    // The HTML styles defined by aProperty/aAttribute have a CSS equivalence
    // for node; let's check if it carries those CSS styles
    nsAutoString value(aValue);
    Result<bool, nsresult> isComputedCSSEquivalentToHTMLInlineStyleOrError =
        CSSEditUtils::IsComputedCSSEquivalentToHTMLInlineStyleSet(
            *this, aText, &aProperty, aAttribute, value);
    if (isComputedCSSEquivalentToHTMLInlineStyleOrError.isErr()) {
      NS_WARNING(
          "CSSEditUtils::IsComputedCSSEquivalentToHTMLInlineStyleSet() failed");
      return SplitRangeOffFromNodeResult(
          isComputedCSSEquivalentToHTMLInlineStyleOrError.unwrapErr());
    }
    if (isComputedCSSEquivalentToHTMLInlineStyleOrError.unwrap()) {
      return SplitRangeOffFromNodeResult(nullptr, &aText, nullptr);
    }
  } else if (HTMLEditUtils::IsInlineStyleSetByElement(aText, aProperty,
                                                      aAttribute, &aValue)) {
    return SplitRangeOffFromNodeResult(nullptr, &aText, nullptr);
  }

  // Make the range an independent node.
  SplitNodeResult splitAtEndResult = [&]() MOZ_CAN_RUN_SCRIPT {
    EditorDOMPoint atEnd(&aText, aEndOffset);
    if (atEnd.IsEndOfContainer()) {
      return SplitNodeResult::NotHandled(atEnd, GetSplitNodeDirection());
    }
    // We need to split off back of text node
    SplitNodeResult splitNodeResult = SplitNodeWithTransaction(atEnd);
    if (splitNodeResult.isErr()) {
      NS_WARNING("HTMLEditor::SplitNodeWithTransaction() failed");
      return splitNodeResult;
    }
    if (MOZ_UNLIKELY(!splitNodeResult.HasCaretPointSuggestion())) {
      NS_WARNING(
          "HTMLEditor::SplitNodeWithTransaction() didn't suggest caret "
          "point");
      return SplitNodeResult(NS_ERROR_FAILURE);
    }
    return splitNodeResult;
  }();
  if (MOZ_UNLIKELY(splitAtEndResult.isErr())) {
    return SplitRangeOffFromNodeResult(splitAtEndResult.unwrapErr());
  }
  EditorDOMPoint pointToPutCaret = splitAtEndResult.UnwrapCaretPoint();
  SplitNodeResult splitAtStartResult = [&]() MOZ_CAN_RUN_SCRIPT {
    EditorDOMPoint atStart(splitAtEndResult.DidSplit()
                               ? splitAtEndResult.GetPreviousContent()
                               : &aText,
                           aStartOffset);
    if (atStart.IsStartOfContainer()) {
      return SplitNodeResult::NotHandled(atStart, GetSplitNodeDirection());
    }
    // We need to split off front of text node
    SplitNodeResult splitNodeResult = SplitNodeWithTransaction(atStart);
    if (splitNodeResult.isErr()) {
      NS_WARNING("HTMLEditor::SplitNodeWithTransaction() failed");
      return splitNodeResult;
    }
    if (MOZ_UNLIKELY(!splitNodeResult.HasCaretPointSuggestion())) {
      NS_WARNING(
          "HTMLEditor::SplitNodeWithTransaction() didn't suggest caret "
          "point");
      return SplitNodeResult(NS_ERROR_FAILURE);
    }
    return splitNodeResult;
  }();
  if (MOZ_UNLIKELY(splitAtStartResult.isErr())) {
    return SplitRangeOffFromNodeResult(splitAtStartResult.unwrapErr());
  }
  if (splitAtStartResult.HasCaretPointSuggestion()) {
    pointToPutCaret = splitAtStartResult.UnwrapCaretPoint();
  }

  MOZ_ASSERT_IF(splitAtStartResult.DidSplit(),
                splitAtStartResult.GetPreviousContent()->IsText());
  MOZ_ASSERT_IF(splitAtStartResult.DidSplit(),
                splitAtStartResult.GetNextContent()->IsText());
  MOZ_ASSERT_IF(splitAtEndResult.DidSplit(),
                splitAtEndResult.GetPreviousContent()->IsText());
  MOZ_ASSERT_IF(splitAtEndResult.DidSplit(),
                splitAtEndResult.GetNextContent()->IsText());
  // Note that those text nodes are grabbed by splitAtStartResult,
  // splitAtEndResult or the callers.  Therefore, we don't need to make them
  // strong pointer.
  Text* const leftTextNode =
      splitAtStartResult.DidSplit()
          ? Text::FromNode(splitAtStartResult.GetPreviousContent())
          : nullptr;
  Text* const middleTextNode =
      splitAtStartResult.DidSplit()
          ? Text::FromNode(splitAtStartResult.GetNextContent())
          : (splitAtEndResult.DidSplit()
                 ? Text::FromNode(splitAtEndResult.GetPreviousContent())
                 : &aText);
  Text* const rightTextNode =
      splitAtEndResult.DidSplit()
          ? Text::FromNode(splitAtEndResult.GetNextContent())
          : nullptr;
  if (aAttribute) {
    // Look for siblings that are correct type of node
    nsIContent* sibling = HTMLEditUtils::GetPreviousSibling(
        *middleTextNode, {WalkTreeOption::IgnoreNonEditableNode});
    if (sibling && sibling->IsElement()) {
      OwningNonNull<Element> element(*sibling->AsElement());
      Result<bool, nsresult> result = ElementIsGoodContainerForTheStyle(
          element, &aProperty, aAttribute, &aValue);
      if (MOZ_UNLIKELY(result.isErr())) {
        NS_WARNING("HTMLEditor::ElementIsGoodContainerForTheStyle() failed");
        return SplitRangeOffFromNodeResult(result.unwrapErr());
      }
      if (result.inspect()) {
        // Previous sib is already right kind of inline node; slide this over
        MoveNodeResult moveTextNodeResult = MoveNodeToEndWithTransaction(
            MOZ_KnownLive(*middleTextNode), element);
        if (moveTextNodeResult.isErr()) {
          NS_WARNING("HTMLEditor::MoveNodeToEndWithTransaction() failed");
          return SplitRangeOffFromNodeResult(moveTextNodeResult.unwrapErr());
        }
        moveTextNodeResult.MoveCaretPointTo(
            pointToPutCaret, {SuggestCaret::OnlyIfHasSuggestion});
        return SplitRangeOffFromNodeResult(leftTextNode, middleTextNode,
                                           rightTextNode,
                                           std::move(pointToPutCaret));
      }
    }
    sibling = HTMLEditUtils::GetNextSibling(
        *middleTextNode, {WalkTreeOption::IgnoreNonEditableNode});
    if (sibling && sibling->IsElement()) {
      OwningNonNull<Element> element(*sibling->AsElement());
      Result<bool, nsresult> result = ElementIsGoodContainerForTheStyle(
          element, &aProperty, aAttribute, &aValue);
      if (MOZ_UNLIKELY(result.isErr())) {
        NS_WARNING("HTMLEditor::ElementIsGoodContainerForTheStyle() failed");
        return SplitRangeOffFromNodeResult(result.unwrapErr());
      }
      if (result.inspect()) {
        // Following sib is already right kind of inline node; slide this over
        MoveNodeResult moveTextNodeResult = MoveNodeWithTransaction(
            MOZ_KnownLive(*middleTextNode), EditorDOMPoint(sibling, 0u));
        if (moveTextNodeResult.isErr()) {
          NS_WARNING("HTMLEditor::MoveNodeWithTransaction() failed");
          return SplitRangeOffFromNodeResult(moveTextNodeResult.unwrapErr());
        }
        moveTextNodeResult.MoveCaretPointTo(
            pointToPutCaret, {SuggestCaret::OnlyIfHasSuggestion});
        return SplitRangeOffFromNodeResult(leftTextNode, middleTextNode,
                                           rightTextNode,
                                           std::move(pointToPutCaret));
      }
    }
  }

  // Wrap the node inside inline node with appropriate {attribute,value}
  Result<EditorDOMPoint, nsresult> setStyleResult = SetInlinePropertyOnNode(
      MOZ_KnownLive(*middleTextNode), aProperty, aAttribute, aValue);
  if (MOZ_UNLIKELY(setStyleResult.isErr())) {
    NS_WARNING("HTMLEditor::SetInlinePropertyOnNode() failed");
    return SplitRangeOffFromNodeResult(setStyleResult.unwrapErr());
  }
  return SplitRangeOffFromNodeResult(leftTextNode, middleTextNode,
                                     rightTextNode, setStyleResult.unwrap());
}

Result<EditorDOMPoint, nsresult> HTMLEditor::SetInlinePropertyOnNodeImpl(
    nsIContent& aContent, nsAtom& aProperty, nsAtom* aAttribute,
    const nsAString& aValue) {
  // If this is an element that can't be contained in a span, we have to
  // recurse to its children.
  if (!HTMLEditUtils::CanNodeContain(*nsGkAtoms::span, aContent)) {
    if (!aContent.HasChildren()) {
      return EditorDOMPoint();
    }

    AutoTArray<OwningNonNull<nsIContent>, 32> arrayOfContents;
    // Populate the list.
    for (nsCOMPtr<nsIContent> child = aContent.GetFirstChild(); child;
         child = child->GetNextSibling()) {
      if (EditorUtils::IsEditableContent(*child, EditorType::HTML) &&
          (!child->IsText() ||
           HTMLEditUtils::IsVisibleTextNode(*child->AsText()))) {
        arrayOfContents.AppendElement(*child);
      }
    }

    // Then loop through the list, set the property on each node.
    EditorDOMPoint pointToPutCaret;
    for (const OwningNonNull<nsIContent>& content : arrayOfContents) {
      // MOZ_KnownLive because 'arrayOfContents' is guaranteed to
      // keep it alive.
      Result<EditorDOMPoint, nsresult> setInlinePropertyResult =
          SetInlinePropertyOnNode(MOZ_KnownLive(content), aProperty, aAttribute,
                                  aValue);
      if (MOZ_UNLIKELY(setInlinePropertyResult.isErr())) {
        NS_WARNING("HTMLEditor::SetInlinePropertyOnNode() failed");
        return setInlinePropertyResult;
      }
      if (setInlinePropertyResult.inspect().IsSet()) {
        pointToPutCaret = setInlinePropertyResult.unwrap();
      }
    }
    return pointToPutCaret;
  }

  // First check if there's an adjacent sibling we can put our node into.
  nsCOMPtr<nsIContent> previousSibling = HTMLEditUtils::GetPreviousSibling(
      aContent, {WalkTreeOption::IgnoreNonEditableNode});
  nsCOMPtr<nsIContent> nextSibling = HTMLEditUtils::GetNextSibling(
      aContent, {WalkTreeOption::IgnoreNonEditableNode});
  if (previousSibling && previousSibling->IsElement()) {
    OwningNonNull<Element> previousElement(*previousSibling->AsElement());
    Result<bool, nsresult> canMoveIntoPreviousSibling =
        ElementIsGoodContainerForTheStyle(previousElement, &aProperty,
                                          aAttribute, &aValue);
    if (canMoveIntoPreviousSibling.isErr()) {
      NS_WARNING("HTMLEditor::ElementIsGoodContainerForTheStyle() failed");
      return canMoveIntoPreviousSibling.propagateErr();
    }
    if (canMoveIntoPreviousSibling.inspect()) {
      MoveNodeResult moveNodeResult =
          MoveNodeToEndWithTransaction(aContent, *previousSibling);
      if (moveNodeResult.isErr()) {
        NS_WARNING("HTMLEditor::MoveNodeToEndWithTransaction() failed");
        return Err(moveNodeResult.unwrapErr());
      }
      if (!nextSibling || !nextSibling->IsElement()) {
        return moveNodeResult.UnwrapCaretPoint();
      }
      OwningNonNull<Element> nextElement(*nextSibling->AsElement());
      Result<bool, nsresult> canMoveIntoNextSibling =
          ElementIsGoodContainerForTheStyle(nextElement, &aProperty, aAttribute,
                                            &aValue);
      if (canMoveIntoNextSibling.isErr()) {
        NS_WARNING("HTMLEditor::ElementIsGoodContainerForTheStyle() failed");
        moveNodeResult.IgnoreCaretPointSuggestion();
        return canMoveIntoNextSibling.propagateErr();
      }
      if (!canMoveIntoNextSibling.inspect()) {
        return moveNodeResult.UnwrapCaretPoint();
      }
      moveNodeResult.IgnoreCaretPointSuggestion();

      // JoinNodesWithTransaction (DoJoinNodes) tries to collapse selection to
      // the joined point and we want to skip updating `Selection` here.
      AutoTransactionsConserveSelection dontChangeMySelection(*this);
      JoinNodesResult joinNodesResult =
          JoinNodesWithTransaction(*previousSibling, *nextSibling);
      if (joinNodesResult.Failed()) {
        NS_WARNING("HTMLEditor::JoinNodesWithTransaction() failed");
        return Err(joinNodesResult.Rv());
      }
      // So, let's take it.
      return joinNodesResult.AtJoinedPoint<EditorDOMPoint>();
    }
  }

  if (nextSibling && nextSibling->IsElement()) {
    OwningNonNull<Element> nextElement(*nextSibling->AsElement());
    Result<bool, nsresult> canMoveIntoNextSibling =
        ElementIsGoodContainerForTheStyle(nextElement, &aProperty, aAttribute,
                                          &aValue);
    if (canMoveIntoNextSibling.isErr()) {
      NS_WARNING("HTMLEditor::ElementIsGoodContainerForTheStyle() failed");
      return canMoveIntoNextSibling.propagateErr();
    }
    if (canMoveIntoNextSibling.inspect()) {
      MoveNodeResult moveNodeResult =
          MoveNodeWithTransaction(aContent, EditorDOMPoint(nextElement, 0u));
      if (moveNodeResult.isErr()) {
        NS_WARNING("HTMLEditor::MoveNodeWithTransaction() failed");
        return Err(moveNodeResult.unwrapErr());
      }
      return moveNodeResult.UnwrapCaretPoint();
    }
  }

  // Don't need to do anything if property already set on node
  if (CSSEditUtils::IsCSSEditableProperty(&aContent, &aProperty, aAttribute)) {
    nsAutoString value(aValue);
    Result<bool, nsresult> isComputedCSSEquivalentToHTMLInlineStyleOrError =
        CSSEditUtils::IsComputedCSSEquivalentToHTMLInlineStyleSet(
            *this, aContent, &aProperty, aAttribute, value);
    if (isComputedCSSEquivalentToHTMLInlineStyleOrError.isErr()) {
      NS_WARNING(
          "CSSEditUtils::IsComputedCSSEquivalentToHTMLInlineStyleSet() failed");
      return isComputedCSSEquivalentToHTMLInlineStyleOrError.propagateErr();
    }
    if (isComputedCSSEquivalentToHTMLInlineStyleOrError.unwrap()) {
      return EditorDOMPoint();
    }
  } else if (HTMLEditUtils::IsInlineStyleSetByElement(aContent, aProperty,
                                                      aAttribute, &aValue)) {
    return EditorDOMPoint();
  }

  auto ShouldUseCSS = [&]() {
    return (IsCSSEnabled() && CSSEditUtils::IsCSSEditableProperty(
                                  &aContent, &aProperty, aAttribute)) ||
           // bgcolor is always done using CSS
           aAttribute == nsGkAtoms::bgcolor ||
           // called for removing parent style, we should use CSS with
           // `<span>` element.
           aValue.EqualsLiteral("-moz-editor-invert-value");
  };

  if (ShouldUseCSS()) {
    RefPtr<Element> spanElement;
    EditorDOMPoint pointToPutCaret;
    // We only add style="" to <span>s with no attributes (bug 746515).  If we
    // don't have one, we need to make one.
    if (aContent.IsHTMLElement(nsGkAtoms::span) &&
        !aContent.AsElement()->GetAttrCount()) {
      spanElement = aContent.AsElement();
    } else {
      CreateElementResult wrapWithSpanElementResult =
          InsertContainerWithTransaction(aContent, *nsGkAtoms::span);
      if (wrapWithSpanElementResult.isErr()) {
        NS_WARNING(
            "HTMLEditor::InsertContainerWithTransaction(nsGkAtoms::span) "
            "failed");
        return Err(wrapWithSpanElementResult.unwrapErr());
      }
      MOZ_ASSERT(wrapWithSpanElementResult.GetNewNode());
      wrapWithSpanElementResult.MoveCaretPointTo(
          pointToPutCaret, {SuggestCaret::OnlyIfHasSuggestion});
      spanElement = wrapWithSpanElementResult.UnwrapNewNode();
    }

    // Add the CSS styles corresponding to the HTML style request
    if (nsStyledElement* spanStyledElement =
            nsStyledElement::FromNode(spanElement)) {
      // MOZ_KnownLive(*spanStyledElement): It's spanElement whose type is
      // RefPtr.
      Result<int32_t, nsresult> result =
          CSSEditUtils::SetCSSEquivalentToHTMLStyleWithTransaction(
              *this, MOZ_KnownLive(*spanStyledElement), &aProperty, aAttribute,
              &aValue);
      if (result.isErr()) {
        if (result.inspectErr() == NS_ERROR_EDITOR_DESTROYED) {
          NS_WARNING(
              "CSSEditUtils::SetCSSEquivalentToHTMLStyleWithTransaction() "
              "failed");
          return Err(NS_ERROR_EDITOR_DESTROYED);
        }
        NS_WARNING(
            "CSSEditUtils::SetCSSEquivalentToHTMLStyleWithTransaction() "
            "failed, but ignored");
      }
    }
    return pointToPutCaret;
  }

  // is it already the right kind of node, but with wrong attribute?
  if (aContent.IsHTMLElement(&aProperty)) {
    if (NS_WARN_IF(!aAttribute)) {
      return Err(NS_ERROR_INVALID_ARG);
    }
    // Just set the attribute on it.
    nsresult rv = SetAttributeWithTransaction(
        MOZ_KnownLive(*aContent.AsElement()), *aAttribute, aValue);
    if (NS_WARN_IF(Destroyed())) {
      return Err(NS_ERROR_EDITOR_DESTROYED);
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::SetAttributeWithTransaction() failed");
      return Err(rv);
    }
    return EditorDOMPoint();
  }

  // ok, chuck it in its very own container
  CreateElementResult wrapWithNewElementToFormatResult =
      InsertContainerWithTransaction(
          aContent, aProperty, aAttribute ? *aAttribute : *nsGkAtoms::_empty,
          aValue);
  if (wrapWithNewElementToFormatResult.isErr()) {
    NS_WARNING("HTMLEditor::InsertContainerWithTransaction() failed");
    return Err(wrapWithNewElementToFormatResult.unwrapErr());
  }
  MOZ_ASSERT(wrapWithNewElementToFormatResult.GetNewNode());
  return wrapWithNewElementToFormatResult.UnwrapCaretPoint();
}

Result<EditorDOMPoint, nsresult> HTMLEditor::SetInlinePropertyOnNode(
    nsIContent& aContent, nsAtom& aProperty, nsAtom* aAttribute,
    const nsAString& aValue) {
  if (NS_WARN_IF(!aContent.GetParentNode())) {
    return Err(NS_ERROR_FAILURE);
  }
  OwningNonNull<nsINode> parent = *aContent.GetParentNode();
  nsCOMPtr<nsIContent> previousSibling = aContent.GetPreviousSibling(),
                       nextSibling = aContent.GetNextSibling();
  EditorDOMPoint pointToPutCaret;
  if (aContent.IsElement()) {
    Result<EditorDOMPoint, nsresult> removeStyleResult =
        RemoveStyleInside(MOZ_KnownLive(*aContent.AsElement()), &aProperty,
                          aAttribute, SpecifiedStyle::Preserve);
    if (MOZ_UNLIKELY(removeStyleResult.isErr())) {
      NS_WARNING("HTMLEditor::RemoveStyleInside() failed");
      return removeStyleResult.propagateErr();
    }
    if (removeStyleResult.inspect().IsSet()) {
      pointToPutCaret = removeStyleResult.unwrap();
    }
  }

  if (aContent.GetParentNode()) {
    // The node is still where it was
    Result<EditorDOMPoint, nsresult> setStyleResult =
        SetInlinePropertyOnNodeImpl(aContent, aProperty, aAttribute, aValue);
    NS_WARNING_ASSERTION(setStyleResult.isOk(),
                         "HTMLEditor::SetInlinePropertyOnNodeImpl() failed");
    return setStyleResult;
  }

  // It's vanished.  Use the old siblings for reference to construct a
  // list.  But first, verify that the previous/next siblings are still
  // where we expect them; otherwise we have to give up.
  if (NS_WARN_IF(previousSibling &&
                 previousSibling->GetParentNode() != parent) ||
      NS_WARN_IF(nextSibling && nextSibling->GetParentNode() != parent)) {
    return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }
  AutoTArray<OwningNonNull<nsIContent>, 24> nodesToSet;
  for (nsIContent* content = previousSibling ? previousSibling->GetNextSibling()
                                             : parent->GetFirstChild();
       content && content != nextSibling; content = content->GetNextSibling()) {
    if (EditorUtils::IsEditableContent(*content, EditorType::HTML)) {
      nodesToSet.AppendElement(*content);
    }
  }

  for (OwningNonNull<nsIContent>& content : nodesToSet) {
    // MOZ_KnownLive because 'nodesToSet' is guaranteed to
    // keep it alive.
    Result<EditorDOMPoint, nsresult> setStyleResult =
        SetInlinePropertyOnNodeImpl(MOZ_KnownLive(content), aProperty,
                                    aAttribute, aValue);
    if (MOZ_UNLIKELY(setStyleResult.isErr())) {
      NS_WARNING("HTMLEditor::SetInlinePropertyOnNodeImpl() failed");
      return setStyleResult;
    }
    if (setStyleResult.inspect().IsSet()) {
      pointToPutCaret = setStyleResult.unwrap();
    }
  }

  return pointToPutCaret;
}

SplitRangeOffResult HTMLEditor::SplitAncestorStyledInlineElementsAtRangeEdges(
    const EditorDOMRange& aRange, nsAtom* aProperty, nsAtom* aAttribute) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!aRange.IsPositioned())) {
    return SplitRangeOffResult(NS_ERROR_FAILURE);
  }

  EditorDOMRange range(aRange);

  // split any matching style nodes above the start of range
  SplitNodeResult resultAtStart = [&]() MOZ_CAN_RUN_SCRIPT {
    AutoTrackDOMRange tracker(RangeUpdaterRef(), &range);
    SplitNodeResult result = SplitAncestorStyledInlineElementsAt(
        range.StartRef(), aProperty, aAttribute);
    if (result.isErr()) {
      NS_WARNING("HTMLEditor::SplitAncestorStyledInlineElementsAt() failed");
      return SplitNodeResult(result.unwrapErr());
    }
    tracker.FlushAndStopTracking();
    if (result.Handled()) {
      auto startOfRange = result.AtSplitPoint<EditorDOMPoint>();
      if (!startOfRange.IsSet()) {
        result.IgnoreCaretPointSuggestion();
        NS_WARNING(
            "HTMLEditor::SplitAncestorStyledInlineElementsAt() didn't return "
            "split point");
        return SplitNodeResult(NS_ERROR_FAILURE);
      }
      range.SetStart(std::move(startOfRange));
    }
    return result;
  }();
  if (resultAtStart.isErr()) {
    return SplitRangeOffResult(resultAtStart.unwrapErr());
  }

  // second verse, same as the first...
  SplitNodeResult resultAtEnd = [&]() MOZ_CAN_RUN_SCRIPT {
    AutoTrackDOMRange tracker(RangeUpdaterRef(), &range);
    SplitNodeResult result = SplitAncestorStyledInlineElementsAt(
        range.EndRef(), aProperty, aAttribute);
    if (result.isErr()) {
      NS_WARNING("HTMLEditor::SplitAncestorStyledInlineElementsAt() failed");
      return SplitNodeResult(result.unwrapErr());
    }
    tracker.FlushAndStopTracking();
    if (result.Handled()) {
      auto endOfRange = result.AtSplitPoint<EditorDOMPoint>();
      if (!endOfRange.IsSet()) {
        result.IgnoreCaretPointSuggestion();
        NS_WARNING(
            "HTMLEditor::SplitAncestorStyledInlineElementsAt() didn't return "
            "split point");
        return SplitNodeResult(NS_ERROR_FAILURE);
      }
      range.SetEnd(std::move(endOfRange));
    }
    return result;
  }();
  if (resultAtEnd.isErr()) {
    resultAtStart.IgnoreCaretPointSuggestion();
    return SplitRangeOffResult(resultAtEnd.unwrapErr());
  }

  return SplitRangeOffResult(std::move(range), std::move(resultAtStart),
                             std::move(resultAtEnd));
}

SplitNodeResult HTMLEditor::SplitAncestorStyledInlineElementsAt(
    const EditorDOMPoint& aPointToSplit, nsAtom* aProperty,
    nsAtom* aAttribute) {
  if (NS_WARN_IF(!aPointToSplit.IsInContentNode())) {
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

  AutoTArray<OwningNonNull<nsIContent>, 24> arrayOfParents;
  for (nsIContent* content :
       aPointToSplit.GetContainer()->InclusiveAncestorsOfType<nsIContent>()) {
    if (HTMLEditUtils::IsBlockElement(*content) || !content->GetParent() ||
        !EditorUtils::IsEditableContent(*content->GetParent(),
                                        EditorType::HTML)) {
      break;
    }
    arrayOfParents.AppendElement(*content);
  }

  // Split any matching style nodes above the point.
  SplitNodeResult result =
      SplitNodeResult::NotHandled(aPointToSplit, GetSplitNodeDirection());
  MOZ_ASSERT(!result.Handled());
  EditorDOMPoint pointToPutCaret;
  for (OwningNonNull<nsIContent>& content : arrayOfParents) {
    bool isSetByCSS = false;
    if (useCSS &&
        CSSEditUtils::IsCSSEditableProperty(content, aProperty, aAttribute)) {
      // The HTML style defined by aProperty/aAttribute has a CSS equivalence
      // in this implementation for the node; let's check if it carries those
      // CSS styles
      nsAutoString firstValue;
      Result<bool, nsresult> isSpecifiedByCSSOrError =
          CSSEditUtils::IsSpecifiedCSSEquivalentToHTMLInlineStyleSet(
              *this, *content, aProperty, aAttribute, firstValue);
      if (isSpecifiedByCSSOrError.isErr()) {
        result.IgnoreCaretPointSuggestion();
        NS_WARNING(
            "CSSEditUtils::IsSpecifiedCSSEquivalentToHTMLInlineStyleSet() "
            "failed");
        return SplitNodeResult(isSpecifiedByCSSOrError.unwrapErr());
      }
      isSetByCSS = isSpecifiedByCSSOrError.unwrap();
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
      else if (!EditorUtils::IsEditableContent(content, EditorType::HTML) ||
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
    AutoTrackDOMPoint trackPointToPutCaret(RangeUpdaterRef(), &pointToPutCaret);
    SplitNodeResult splitNodeResult = SplitNodeDeepWithTransaction(
        MOZ_KnownLive(content), result.AtSplitPoint<EditorDOMPoint>(),
        SplitAtEdges::eAllowToCreateEmptyContainer);
    if (splitNodeResult.isErr()) {
      NS_WARNING("HTMLEditor::SplitNodeDeepWithTransaction() failed");
      return splitNodeResult;
    }
    trackPointToPutCaret.FlushAndStopTracking();
    splitNodeResult.MoveCaretPointTo(pointToPutCaret,
                                     {SuggestCaret::OnlyIfHasSuggestion});

    // If it's not handled, it means that `content` is not a splitable node
    // like a void element even if it has some children, and the split point
    // is middle of it.
    if (!splitNodeResult.Handled()) {
      continue;
    }
    // Mark the final result as handled forcibly.
    result = splitNodeResult.ToHandledResult();
    MOZ_ASSERT(result.Handled());
  }

  return pointToPutCaret.IsSet()
             ? SplitNodeResult(std::move(result), std::move(pointToPutCaret))
             : std::move(result);
}

Result<EditorDOMPoint, nsresult> HTMLEditor::ClearStyleAt(
    const EditorDOMPoint& aPoint, nsAtom* aProperty, nsAtom* aAttribute,
    SpecifiedStyle aSpecifiedStyle) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!aPoint.IsSet())) {
    return Err(NS_ERROR_INVALID_ARG);
  }

  // TODO: We should rewrite this to stop unnecessary element creation and
  //       deleting it later because it causes the original element may be
  //       removed from the DOM tree even if same element is still in the
  //       DOM tree from point of view of users.

  // First, split inline elements at the point.
  // E.g., if aProperty is nsGkAtoms::b and `<p><b><i>a[]bc</i></b></p>`,
  //       we want to make it as `<p><b><i>a</i></b><b><i>bc</i></b></p>`.
  EditorDOMPoint pointToPutCaret(aPoint);
  SplitNodeResult splitResult =
      SplitAncestorStyledInlineElementsAt(aPoint, aProperty, aAttribute);
  if (splitResult.isErr()) {
    NS_WARNING("HTMLEditor::SplitAncestorStyledInlineElementsAt() failed");
    return Err(splitResult.unwrapErr());
  }
  splitResult.MoveCaretPointTo(pointToPutCaret, *this,
                               {SuggestCaret::OnlyIfHasSuggestion,
                                SuggestCaret::OnlyIfTransactionsAllowedToDoIt});

  // If there is no styled inline elements of aProperty/aAttribute, we just
  // return the given point.
  // E.g., `<p><i>a[]bc</i></p>` for nsGkAtoms::b.
  if (!splitResult.Handled()) {
    return pointToPutCaret;
  }

  // If it did split nodes, but topmost ancestor inline element is split
  // at start of it, we don't need the empty inline element.  Let's remove
  // it now.  Then, we'll get the following DOM tree if there is no "a" in the
  // above case:
  // <p><b><i>bc</i></b></p>
  //   ^^
  if (splitResult.GetPreviousContent() &&
      HTMLEditUtils::IsEmptyNode(
          *splitResult.GetPreviousContent(),
          {EmptyCheckOption::TreatSingleBRElementAsVisible,
           EmptyCheckOption::TreatListItemAsVisible,
           EmptyCheckOption::TreatTableCellAsVisible})) {
    AutoTrackDOMPoint trackPointToPutCaret(RangeUpdaterRef(), &pointToPutCaret);
    // Delete previous node if it's empty.
    // MOZ_KnownLive(splitResult.GetPreviousContent()):
    // It's grabbed by splitResult.
    nsresult rv = DeleteNodeWithTransaction(
        MOZ_KnownLive(*splitResult.GetPreviousContent()));
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
      return Err(rv);
    }
  }

  // If we reached block from end of a text node, we can do nothing here.
  // E.g., `<p style="font-weight: bold;">a[]bc</p>` for nsGkAtoms::b and
  // we're in CSS mode.
  // XXX Chrome resets block style and creates `<span>` elements for each
  //     line in this case.
  if (!splitResult.GetNextContent()) {
    return pointToPutCaret;
  }

  // Otherwise, the next node is topmost ancestor inline element which has
  // the style.  We want to put caret between the split nodes, but we need
  // to keep other styles.  Therefore, next, we need to split at start of
  // the next node.  The first example should become
  // `<p><b><i>a</i></b><b><i></i></b><b><i>bc</i></b></p>`.
  //                    ^^^^^^^^^^^^^^
  nsIContent* firstLeafChildOfNextNode = HTMLEditUtils::GetFirstLeafContent(
      *splitResult.GetNextContent(), {LeafNodeType::OnlyLeafNode});
  EditorDOMPoint atStartOfNextNode(firstLeafChildOfNextNode
                                       ? firstLeafChildOfNextNode
                                       : splitResult.GetNextContent(),
                                   0);
  RefPtr<HTMLBRElement> brElement;
  // But don't try to split non-containers like `<br>`, `<hr>` and `<img>`
  // element.
  if (!atStartOfNextNode.IsInContentNode() ||
      !HTMLEditUtils::IsContainerNode(
          *atStartOfNextNode.ContainerAs<nsIContent>())) {
    // If it's a `<br>` element, let's move it into new node later.
    brElement = HTMLBRElement::FromNode(atStartOfNextNode.GetContainer());
    if (!atStartOfNextNode.GetContainerParentAs<nsIContent>()) {
      NS_WARNING("atStartOfNextNode was in an orphan node");
      return Err(NS_ERROR_FAILURE);
    }
    atStartOfNextNode.Set(atStartOfNextNode.GetContainerParent(), 0);
  }
  SplitNodeResult splitResultAtStartOfNextNode =
      SplitAncestorStyledInlineElementsAt(atStartOfNextNode, aProperty,
                                          aAttribute);
  if (splitResultAtStartOfNextNode.isErr()) {
    NS_WARNING("HTMLEditor::SplitAncestorStyledInlineElementsAt() failed");
    return Err(splitResultAtStartOfNextNode.unwrapErr());
  }
  splitResultAtStartOfNextNode.MoveCaretPointTo(
      pointToPutCaret, *this,
      {SuggestCaret::OnlyIfHasSuggestion,
       SuggestCaret::OnlyIfTransactionsAllowedToDoIt});

  if (splitResultAtStartOfNextNode.Handled() &&
      splitResultAtStartOfNextNode.GetNextContent()) {
    // If the right inline elements are empty, we should remove them.  E.g.,
    // if the split point is at end of a text node (or end of an inline
    // element), e.g., <div><b><i>abc[]</i></b></div>, then now, it's been
    // changed to:
    // <div><b><i>abc</i></b><b><i>[]</i></b><b><i></i></b></div>
    //                                       ^^^^^^^^^^^^^^
    // We will change it to:
    // <div><b><i>abc</i></b><b><i>[]</i></b></div>
    //                                      ^^
    // And if it has only padding <br> element, we should move it into the
    // previous <i> which will have new content.
    bool seenBR = false;
    if (HTMLEditUtils::IsEmptyNode(
            *splitResultAtStartOfNextNode.GetNextContent(),
            {EmptyCheckOption::TreatListItemAsVisible,
             EmptyCheckOption::TreatTableCellAsVisible},
            &seenBR)) {
      // Delete next node if it's empty.
      // MOZ_KnownLive(splitResultAtStartOfNextNode.GetNextContent()):
      // It's grabbed by splitResultAtStartOfNextNode.
      nsresult rv = DeleteNodeWithTransaction(
          MOZ_KnownLive(*splitResultAtStartOfNextNode.GetNextContent()));
      if (NS_FAILED(rv)) {
        NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
        return Err(rv);
      }
      if (seenBR && !brElement) {
        brElement = HTMLEditUtils::GetFirstBRElement(
            *splitResultAtStartOfNextNode.GetNextContent()->AsElement());
      }
    }
  }

  // If there is no content, we should return here.
  // XXX Is this possible case without mutation event listener?
  if (NS_WARN_IF(!splitResultAtStartOfNextNode.Handled()) ||
      !splitResultAtStartOfNextNode.GetPreviousContent()) {
    // XXX This is really odd, but we retrun this value...
    const EditorRawDOMPoint& splitPoint =
        splitResult.AtSplitPoint<EditorRawDOMPoint>();
    const EditorRawDOMPoint& splitPointAtStartOfNextNode =
        splitResultAtStartOfNextNode.AtSplitPoint<EditorRawDOMPoint>();
    return EditorDOMPoint(splitPoint.GetContainer(),
                          splitPointAtStartOfNextNode.Offset());
  }

  // Now, we want to put `<br>` element into the empty split node if
  // it was in next node of the first split.
  // E.g., `<p><b><i>a</i></b><b><i><br></i></b><b><i>bc</i></b></p>`
  nsIContent* firstLeafChildOfPreviousNode = HTMLEditUtils::GetFirstLeafContent(
      *splitResultAtStartOfNextNode.GetPreviousContent(),
      {LeafNodeType::OnlyLeafNode});
  pointToPutCaret.Set(firstLeafChildOfPreviousNode
                          ? firstLeafChildOfPreviousNode
                          : splitResultAtStartOfNextNode.GetPreviousContent(),
                      0);

  // If the right node starts with a `<br>`, suck it out of right node and into
  // the left node left node.  This is so we you don't revert back to the
  // previous style if you happen to click at the end of a line.
  if (brElement) {
    MoveNodeResult moveBRElementResult =
        MoveNodeWithTransaction(*brElement, pointToPutCaret);
    if (moveBRElementResult.isErr()) {
      NS_WARNING("HTMLEditor::MoveNodeWithTransaction() failed");
      return Err(moveBRElementResult.unwrapErr());
    }
    moveBRElementResult.MoveCaretPointTo(
        pointToPutCaret, *this,
        {SuggestCaret::OnlyIfHasSuggestion,
         SuggestCaret::OnlyIfTransactionsAllowedToDoIt});

    if (splitResultAtStartOfNextNode.GetNextContent() &&
        splitResultAtStartOfNextNode.GetNextContent()->IsInComposedDoc()) {
      // If we split inline elements at immediately before <br> element which is
      // the last visible content in the right element, we don't need the right
      // element anymore.  Otherwise, we'll create the following DOM tree:
      // - <b>abc</b>{}<br><b></b>
      //                   ^^^^^^^
      // - <b><i>abc</i></b><i><br></i><b></b>
      //                               ^^^^^^^
      if (HTMLEditUtils::IsEmptyNode(
              *splitResultAtStartOfNextNode.GetNextContent(),
              {EmptyCheckOption::TreatSingleBRElementAsVisible,
               EmptyCheckOption::TreatListItemAsVisible,
               EmptyCheckOption::TreatTableCellAsVisible})) {
        // MOZ_KnownLive because the result is grabbed by
        // splitResultAtStartOfNextNode.
        nsresult rv = DeleteNodeWithTransaction(
            MOZ_KnownLive(*splitResultAtStartOfNextNode.GetNextContent()));
        if (NS_FAILED(rv)) {
          NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
          return Err(rv);
        }
      }
      // If the next content has only one <br> element, there may be empty
      // inline elements around it.  We don't need them anymore because user
      // cannot put caret into them.  E.g., <b><i>abc[]<br></i><br></b> has
      // been changed to <b><i>abc</i></b><i>{}<br></i><b><i></i><br></b> now.
      //                                               ^^^^^^^^^^^^^^^^^^
      // We don't need the empty <i>.
      else if (HTMLEditUtils::IsEmptyNode(
                   *splitResultAtStartOfNextNode.GetNextContent(),
                   {EmptyCheckOption::TreatListItemAsVisible,
                    EmptyCheckOption::TreatTableCellAsVisible})) {
        AutoTArray<OwningNonNull<nsIContent>, 4> emptyInlineContainerElements;
        HTMLEditUtils::CollectEmptyInlineContainerDescendants(
            *splitResultAtStartOfNextNode.GetNextContent()->AsElement(),
            emptyInlineContainerElements,
            {EmptyCheckOption::TreatSingleBRElementAsVisible,
             EmptyCheckOption::TreatListItemAsVisible,
             EmptyCheckOption::TreatTableCellAsVisible});
        for (const OwningNonNull<nsIContent>& emptyInlineContainerElement :
             emptyInlineContainerElements) {
          // MOZ_KnownLive(emptyInlineContainerElement) due to bug 1622253.
          nsresult rv = DeleteNodeWithTransaction(
              MOZ_KnownLive(emptyInlineContainerElement));
          if (NS_FAILED(rv)) {
            NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
            return Err(rv);
          }
        }
      }
    }

    // Update the child.
    pointToPutCaret.Set(pointToPutCaret.GetContainer(), 0);
  }
  // Finally, remove the specified style in the previous node at the
  // second split and tells good insertion point to the caller.  I.e., we
  // want to make the first example as:
  // `<p><b><i>a</i></b><i>[]</i><b><i>bc</i></b></p>`
  //                    ^^^^^^^^^
  if (Element* previousElementOfSplitPoint = Element::FromNode(
          splitResultAtStartOfNextNode.GetPreviousContent())) {
    // Track the point at the new hierarchy.  This is so we can know where
    // to put the selection after we call RemoveStyleInside().
    // RemoveStyleInside() could remove any and all of those nodes, so I
    // have to use the range tracking system to find the right spot to put
    // selection.
    AutoTrackDOMPoint tracker(RangeUpdaterRef(), &pointToPutCaret);
    // MOZ_KnownLive(previousElementOfSplitPoint):
    // It's grabbed by splitResultAtStartOfNextNode.
    Result<EditorDOMPoint, nsresult> removeStyleResult =
        RemoveStyleInside(MOZ_KnownLive(*previousElementOfSplitPoint),
                          aProperty, aAttribute, aSpecifiedStyle);
    if (MOZ_UNLIKELY(removeStyleResult.isErr())) {
      NS_WARNING("HTMLEditor::RemoveStyleInside() failed");
      return removeStyleResult;
    }
    // We've already computed a suggested caret position at start of first leaf
    // which is stored in pointToPutCaret, so we don't need to update it here.
  }
  return pointToPutCaret;
}

Result<EditorDOMPoint, nsresult> HTMLEditor::RemoveStyleInside(
    Element& aElement, nsAtom* aProperty, nsAtom* aAttribute,
    SpecifiedStyle aSpecifiedStyle) {
  // First, handle all descendants.
  AutoTArray<OwningNonNull<nsIContent>, 32> arrayOfChildContents;
  HTMLEditor::GetChildNodesOf(aElement, arrayOfChildContents);
  EditorDOMPoint pointToPutCaret;
  for (const OwningNonNull<nsIContent>& child : arrayOfChildContents) {
    if (!child->IsElement()) {
      continue;
    }
    Result<EditorDOMPoint, nsresult> removeStyleResult =
        RemoveStyleInside(MOZ_KnownLive(*child->AsElement()), aProperty,
                          aAttribute, aSpecifiedStyle);
    if (MOZ_UNLIKELY(removeStyleResult.isErr())) {
      NS_WARNING("HTMLEditor::RemoveStyleInside() failed");
      return removeStyleResult;
    }
    if (removeStyleResult.inspect().IsSet()) {
      pointToPutCaret = removeStyleResult.unwrap();
    }
  }

  // Next, remove the element or its attribute.
  auto ShouldRemoveHTMLStyle = [&]() {
    if (aProperty) {
      return
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
    if (EditorUtils::IsEditableContent(aElement, EditorType::HTML)) {
      // or removing all styles and the element is a presentation element.
      return HTMLEditUtils::IsRemovableInlineStyleElement(aElement);
    }
    return false;
  };

  if (ShouldRemoveHTMLStyle()) {
    // If aAttribute is nullptr, we want to remove any matching inline styles
    // entirely.
    if (!aAttribute) {
      // If some style rules are specified to aElement, we need to keep them
      // as far as possible.
      // XXX Why don't we clone `id` attribute?
      if (aProperty && aSpecifiedStyle != SpecifiedStyle::Discard &&
          (aElement.HasAttr(kNameSpaceID_None, nsGkAtoms::style) ||
           aElement.HasAttr(kNameSpaceID_None, nsGkAtoms::_class))) {
        // Move `style` attribute and `class` element to span element before
        // removing aElement from the tree.
        CreateElementResult wrapWithSpanElementResult =
            InsertContainerWithTransaction(aElement, *nsGkAtoms::span);
        if (wrapWithSpanElementResult.isErr()) {
          NS_WARNING(
              "HTMLEditor::InsertContainerWithTransaction(nsGkAtoms::span) "
              "failed");
          return Err(wrapWithSpanElementResult.unwrapErr());
        }
        MOZ_ASSERT(wrapWithSpanElementResult.GetNewNode());
        wrapWithSpanElementResult.MoveCaretPointTo(
            pointToPutCaret, {SuggestCaret::OnlyIfHasSuggestion});
        const RefPtr<Element> spanElement =
            wrapWithSpanElementResult.UnwrapNewNode();
        nsresult rv = CloneAttributeWithTransaction(*nsGkAtoms::style,
                                                    *spanElement, aElement);
        if (NS_WARN_IF(Destroyed())) {
          return Err(NS_ERROR_EDITOR_DESTROYED);
        }
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "EditorBase::CloneAttributeWithTransaction(nsGkAtoms::style) "
              "failed");
          return Err(rv);
        }
        rv = CloneAttributeWithTransaction(*nsGkAtoms::_class, *spanElement,
                                           aElement);
        if (NS_WARN_IF(Destroyed())) {
          return Err(NS_ERROR_EDITOR_DESTROYED);
        }
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "EditorBase::CloneAttributeWithTransaction(nsGkAtoms::_class) "
              "failed");
          return Err(rv);
        }
      }
      Result<EditorDOMPoint, nsresult> unwrapElementResult =
          RemoveContainerWithTransaction(aElement);
      if (MOZ_UNLIKELY(unwrapElementResult.isErr())) {
        NS_WARNING("HTMLEditor::RemoveContainerWithTransaction() failed");
        return unwrapElementResult.propagateErr();
      }
      if (AllowsTransactionsToChangeSelection() &&
          unwrapElementResult.inspect().IsSet()) {
        pointToPutCaret = unwrapElementResult.unwrap();
      }
    }
    // If aAttribute is specified, we want to remove only the attribute
    // unless it's the last attribute of aElement.
    else if (aElement.HasAttr(kNameSpaceID_None, aAttribute)) {
      if (IsOnlyAttribute(&aElement, aAttribute)) {
        Result<EditorDOMPoint, nsresult> unwrapElementResult =
            RemoveContainerWithTransaction(aElement);
        if (MOZ_UNLIKELY(unwrapElementResult.isErr())) {
          NS_WARNING("HTMLEditor::RemoveContainerWithTransaction() failed");
          return unwrapElementResult.propagateErr();
        }
        if (unwrapElementResult.inspect().IsSet()) {
          pointToPutCaret = unwrapElementResult.unwrap();
        }
      } else {
        nsresult rv = RemoveAttributeWithTransaction(aElement, *aAttribute);
        if (NS_WARN_IF(Destroyed())) {
          return Err(NS_ERROR_EDITOR_DESTROYED);
        }
        if (NS_FAILED(rv)) {
          NS_WARNING("EditorBase::RemoveAttributeWithTransaction() failed");
          return Err(rv);
        }
      }
    }
  }

  // Then, remove CSS style if specified.
  // XXX aElement may have already been removed from the DOM tree.  Why
  //     do we keep handling aElement here??
  if (CSSEditUtils::IsCSSEditableProperty(&aElement, aProperty, aAttribute)) {
    Result<bool, nsresult> elementHasSpecifiedCSSEquivalentStylesOrError =
        CSSEditUtils::HaveSpecifiedCSSEquivalentStyles(*this, aElement,
                                                       aProperty, aAttribute);
    if (elementHasSpecifiedCSSEquivalentStylesOrError.isErr()) {
      NS_WARNING("CSSEditUtils::HaveSpecifiedCSSEquivalentStyles() failed");
      return elementHasSpecifiedCSSEquivalentStylesOrError.propagateErr();
    }
    if (elementHasSpecifiedCSSEquivalentStylesOrError.unwrap()) {
      if (nsStyledElement* styledElement =
              nsStyledElement::FromNode(&aElement)) {
        // If aElement has CSS declaration of the given style, remove it.
        // MOZ_KnownLive(*styledElement): It's aElement and its lifetime must be
        // guaranteed by the caller because of MOZ_CAN_RUN_SCRIPT method.
        nsresult rv =
            CSSEditUtils::RemoveCSSEquivalentToHTMLStyleWithTransaction(
                *this, MOZ_KnownLive(*styledElement), aProperty, aAttribute,
                nullptr);
        if (rv == NS_ERROR_EDITOR_DESTROYED) {
          NS_WARNING(
              "CSSEditUtils::RemoveCSSEquivalentToHTMLStyleWithTransaction() "
              "destroyed the editor");
          return Err(NS_ERROR_EDITOR_DESTROYED);
        }
        NS_WARNING_ASSERTION(
            NS_SUCCEEDED(rv),
            "CSSEditUtils::RemoveCSSEquivalentToHTMLStyleWithTransaction() "
            "failed, but ignored");
      }
      // Additionally, remove aElement itself if it's a `<span>` or `<font>`
      // and it does not have non-empty `style`, `id` nor `class` attribute.
      if (aElement.IsAnyOfHTMLElements(nsGkAtoms::span, nsGkAtoms::font) &&
          !HTMLEditor::HasStyleOrIdOrClassAttribute(aElement)) {
        Result<EditorDOMPoint, nsresult> unwrapSpanOrFontElementResult =
            RemoveContainerWithTransaction(aElement);
        if (MOZ_UNLIKELY(unwrapSpanOrFontElementResult.isErr() &&
                         unwrapSpanOrFontElementResult.inspectErr() ==
                             NS_ERROR_EDITOR_DESTROYED)) {
          NS_WARNING("HTMLEditor::RemoveContainerWithTransaction() failed");
          return Err(NS_ERROR_EDITOR_DESTROYED);
        }
        NS_WARNING_ASSERTION(
            unwrapSpanOrFontElementResult.isOk(),
            "HTMLEditor::RemoveContainerWithTransaction() failed, but ignored");
        if (MOZ_LIKELY(unwrapSpanOrFontElementResult.isOk()) &&
            unwrapSpanOrFontElementResult.inspect().IsSet()) {
          pointToPutCaret = unwrapSpanOrFontElementResult.unwrap();
        }
      }
    }
  }

  if (aProperty != nsGkAtoms::font || aAttribute != nsGkAtoms::size ||
      !aElement.IsAnyOfHTMLElements(nsGkAtoms::big, nsGkAtoms::small)) {
    return pointToPutCaret;
  }

  // Finally, remove aElement if it's a `<big>` or `<small>` element and
  // we're removing `<font size>`.
  Result<EditorDOMPoint, nsresult> unwrapBigOrSmallElementResult =
      RemoveContainerWithTransaction(aElement);
  NS_WARNING_ASSERTION(unwrapBigOrSmallElementResult.isOk(),
                       "HTMLEditor::RemoveContainerWithTransaction() failed");
  return unwrapBigOrSmallElementResult;
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
      if (!StringBeginsWith(attrString, u"_moz"_ns)) {
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
  // XXX Shouldn't ignore the editing host.
  if (NS_WARN_IF(!aRange.GetStartContainer()) ||
      NS_WARN_IF(!aRange.GetEndContainer())) {
    return NS_ERROR_INVALID_ARG;
  }
  EditorRawDOMPoint newRangeStart(aRange.StartRef());
  for (Element* element :
       aRange.GetStartContainer()->InclusiveAncestorsOfType<Element>()) {
    if (element->IsHTMLElement(nsGkAtoms::body)) {
      break;
    }
    if (!HTMLEditUtils::IsNamedAnchor(element)) {
      continue;
    }
    newRangeStart.Set(element);
    break;
  }

  if (!newRangeStart.IsInContentNode()) {
    NS_WARNING(
        "HTMLEditor::PromoteRangeIfStartsOrEndsInNamedAnchor() reached root "
        "element from start container");
    return NS_ERROR_FAILURE;
  }

  EditorRawDOMPoint newRangeEnd(aRange.EndRef());
  for (Element* element :
       aRange.GetEndContainer()->InclusiveAncestorsOfType<Element>()) {
    if (element->IsHTMLElement(nsGkAtoms::body)) {
      break;
    }
    if (!HTMLEditUtils::IsNamedAnchor(element)) {
      continue;
    }
    newRangeEnd.SetAfter(element);
    break;
  }

  if (!newRangeEnd.IsInContentNode()) {
    NS_WARNING(
        "HTMLEditor::PromoteRangeIfStartsOrEndsInNamedAnchor() reached root "
        "element from end container");
    return NS_ERROR_FAILURE;
  }

  if (newRangeStart == aRange.StartRef() && newRangeEnd == aRange.EndRef()) {
    return NS_OK;
  }

  nsresult rv = aRange.SetStartAndEnd(newRangeStart.ToRawRangeBoundary(),
                                      newRangeEnd.ToRawRangeBoundary());
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "nsRange::SetStartAndEnd() failed");
  return rv;
}

nsresult HTMLEditor::PromoteInlineRange(nsRange& aRange) {
  if (NS_WARN_IF(!aRange.GetStartContainer()) ||
      NS_WARN_IF(!aRange.GetEndContainer())) {
    return NS_ERROR_INVALID_ARG;
  }
  EditorRawDOMPoint newRangeStart(aRange.StartRef());
  for (nsIContent* content :
       aRange.GetStartContainer()->InclusiveAncestorsOfType<nsIContent>()) {
    MOZ_ASSERT(newRangeStart.GetContainer() == content);
    if (content->IsHTMLElement(nsGkAtoms::body) ||
        !EditorUtils::IsEditableContent(*content, EditorType::HTML) ||
        !IsStartOfContainerOrBeforeFirstEditableChild(newRangeStart)) {
      break;
    }
    newRangeStart.Set(content);
  }
  if (!newRangeStart.IsInContentNode()) {
    NS_WARNING(
        "HTMLEditor::PromoteInlineRange() reached root element from start "
        "container");
    return NS_ERROR_FAILURE;
  }

  EditorRawDOMPoint newRangeEnd(aRange.EndRef());
  for (nsIContent* content :
       aRange.GetEndContainer()->InclusiveAncestorsOfType<nsIContent>()) {
    MOZ_ASSERT(newRangeEnd.GetContainer() == content);
    if (content->IsHTMLElement(nsGkAtoms::body) ||
        !EditorUtils::IsEditableContent(*content, EditorType::HTML) ||
        !IsEndOfContainerOrEqualsOrAfterLastEditableChild(newRangeEnd)) {
      break;
    }
    newRangeEnd.SetAfter(content);
  }
  if (!newRangeEnd.IsInContentNode()) {
    NS_WARNING(
        "HTMLEditor::PromoteInlineRange() reached root element from end "
        "container");
    return NS_ERROR_FAILURE;
  }

  if (newRangeStart == aRange.StartRef() && newRangeEnd == aRange.EndRef()) {
    return NS_OK;
  }

  nsresult rv = aRange.SetStartAndEnd(newRangeStart.ToRawRangeBoundary(),
                                      newRangeEnd.ToRawRangeBoundary());
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "nsRange::SetStartAndEnd() failed");
  return rv;
}

bool HTMLEditor::IsStartOfContainerOrBeforeFirstEditableChild(
    const EditorRawDOMPoint& aPoint) const {
  MOZ_ASSERT(aPoint.IsSet());

  if (aPoint.IsStartOfContainer()) {
    return true;
  }

  if (aPoint.IsInTextNode()) {
    return false;
  }

  nsIContent* firstEditableChild = HTMLEditUtils::GetFirstChild(
      *aPoint.GetContainer(), {WalkTreeOption::IgnoreNonEditableNode});
  if (!firstEditableChild) {
    return true;
  }
  return EditorRawDOMPoint(firstEditableChild).Offset() >= aPoint.Offset();
}

bool HTMLEditor::IsEndOfContainerOrEqualsOrAfterLastEditableChild(
    const EditorRawDOMPoint& aPoint) const {
  MOZ_ASSERT(aPoint.IsSet());

  if (aPoint.IsEndOfContainer()) {
    return true;
  }

  if (aPoint.IsInTextNode()) {
    return false;
  }

  nsIContent* lastEditableChild = HTMLEditUtils::GetLastChild(
      *aPoint.GetContainer(), {WalkTreeOption::IgnoreNonEditableNode});
  if (!lastEditableChild) {
    return true;
  }
  return EditorRawDOMPoint(lastEditableChild).Offset() < aPoint.Offset();
}

nsresult HTMLEditor::GetInlinePropertyBase(nsStaticAtom& aHTMLProperty,
                                           nsAtom* aAttribute,
                                           const nsAString* aValue,
                                           bool* aFirst, bool* aAny, bool* aAll,
                                           nsAString* outValue) const {
  MOZ_ASSERT(IsEditActionDataAvailable());

  *aAny = false;
  *aAll = true;
  *aFirst = false;
  bool first = true;

  bool isCollapsed = SelectionRef().IsCollapsed();
  RefPtr<nsRange> range = SelectionRef().GetRangeAt(0);
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
      nsString tOutString;
      const PendingStyleState styleState = [&]() {
        if (aAttribute) {
          auto state = mPendingStylesToApplyToNewContent->GetStyleState(
              aHTMLProperty, aAttribute, &tOutString);
          if (outValue) {
            outValue->Assign(tOutString);
          }
          return state;
        }
        return mPendingStylesToApplyToNewContent->GetStyleState(aHTMLProperty);
      }();
      if (styleState != PendingStyleState::NotUpdated) {
        *aFirst = *aAny = *aAll =
            (styleState == PendingStyleState::BeingPreserved);
        return NS_OK;
      }

      if (collapsedNode->IsContent() &&
          CSSEditUtils::IsCSSEditableProperty(collapsedNode, &aHTMLProperty,
                                              aAttribute)) {
        if (aValue) {
          tOutString.Assign(*aValue);
        }
        Result<bool, nsresult> isComputedCSSEquivalentToHTMLInlineStyleOrError =
            CSSEditUtils::IsComputedCSSEquivalentToHTMLInlineStyleSet(
                *this, MOZ_KnownLive(*collapsedNode->AsContent()),
                &aHTMLProperty, aAttribute, tOutString);
        if (isComputedCSSEquivalentToHTMLInlineStyleOrError.isErr()) {
          NS_WARNING(
              "CSSEditUtils::IsComputedCSSEquivalentToHTMLInlineStyleSet() "
              "failed");
          return isComputedCSSEquivalentToHTMLInlineStyleOrError.unwrapErr();
        }
        *aFirst = *aAny = *aAll =
            isComputedCSSEquivalentToHTMLInlineStyleOrError.unwrap();
        if (outValue) {
          outValue->Assign(tOutString);
        }
        return NS_OK;
      }

      *aFirst = *aAny = *aAll = collapsedNode->IsContent() &&
                                HTMLEditUtils::IsInlineStyleSetByElement(
                                    *collapsedNode->AsContent(), aHTMLProperty,
                                    aAttribute, aValue, outValue);
      return NS_OK;
    }

    // Non-collapsed selection

    nsAutoString firstValue, theValue;

    nsCOMPtr<nsINode> endNode = range->GetEndContainer();
    uint32_t endOffset = range->EndOffset();

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
      if (content->IsText() &&
          (!EditorUtils::IsEditableContent(*content, EditorType::HTML) ||
           !HTMLEditUtils::IsVisibleTextNode(*content->AsText()))) {
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
      bool useTextDecoration =
          &aHTMLProperty == nsGkAtoms::u || &aHTMLProperty == nsGkAtoms::strike;
      if (first) {
        if (CSSEditUtils::IsCSSEditableProperty(content, &aHTMLProperty,
                                                aAttribute)) {
          // The HTML styles defined by aHTMLProperty/aAttribute have a CSS
          // equivalence in this implementation for node; let's check if it
          // carries those CSS styles
          if (aValue) {
            firstValue.Assign(*aValue);
          }
          Result<bool, nsresult>
              isComputedCSSEquivalentToHTMLInlineStyleOrError =
                  CSSEditUtils::IsComputedCSSEquivalentToHTMLInlineStyleSet(
                      *this, *content, &aHTMLProperty, aAttribute, firstValue);
          if (isComputedCSSEquivalentToHTMLInlineStyleOrError.isErr()) {
            NS_WARNING(
                "CSSEditUtils::IsComputedCSSEquivalentToHTMLInlineStyleSet() "
                "failed");
            return isComputedCSSEquivalentToHTMLInlineStyleOrError.unwrapErr();
          }
          isSet = isComputedCSSEquivalentToHTMLInlineStyleOrError.unwrap();
        } else {
          isSet = HTMLEditUtils::IsInlineStyleSetByElement(
              *content, aHTMLProperty, aAttribute, aValue, &firstValue);
        }
        *aFirst = isSet;
        first = false;
        if (outValue) {
          *outValue = firstValue;
        }
      } else {
        if (CSSEditUtils::IsCSSEditableProperty(content, &aHTMLProperty,
                                                aAttribute)) {
          // The HTML styles defined by aHTMLProperty/aAttribute have a CSS
          // equivalence in this implementation for node; let's check if it
          // carries those CSS styles
          if (aValue) {
            theValue.Assign(*aValue);
          }
          Result<bool, nsresult>
              isComputedCSSEquivalentToHTMLInlineStyleOrError =
                  CSSEditUtils::IsComputedCSSEquivalentToHTMLInlineStyleSet(
                      *this, *content, &aHTMLProperty, aAttribute, theValue);
          if (isComputedCSSEquivalentToHTMLInlineStyleOrError.isErr()) {
            NS_WARNING(
                "CSSEditUtils::IsComputedCSSEquivalentToHTMLInlineStyleSet() "
                "failed");
            return isComputedCSSEquivalentToHTMLInlineStyleOrError.unwrapErr();
          }
          isSet = isComputedCSSEquivalentToHTMLInlineStyleOrError.unwrap();
        } else {
          isSet = HTMLEditUtils::IsInlineStyleSetByElement(
              *content, aHTMLProperty, aAttribute, aValue, &theValue);
        }

        if (firstValue != theValue &&
            // For text-decoration related HTML properties, i.e. <u> and
            // <strike>, we have to also check |isSet| because text-decoration
            // is a shorthand property, and it may contains other unrelated
            // longhand components, e.g. text-decoration-color, so we have to do
            // an extra check before setting |*aAll| to false.
            // e.g.
            //   firstValue: "underline rgb(0, 0, 0)"
            //   theValue: "underline rgb(0, 0, 238)" // <a> uses blue color
            // These two values should be the same if we are checking `<u>`.
            // That's why we need to check |*aFirst| and |isSet|.
            //
            // This is a work-around for text-decoration.
            // The spec issue: https://github.com/w3c/editing/issues/241.
            // Once this spec issue is resolved, we could drop this work-around
            // check.
            (!useTextDecoration || *aFirst != isSet)) {
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

nsresult HTMLEditor::GetInlineProperty(nsStaticAtom& aHTMLProperty,
                                       nsAtom* aAttribute,
                                       const nsAString& aValue, bool* aFirst,
                                       bool* aAny, bool* aAll) const {
  if (NS_WARN_IF(!aFirst) || NS_WARN_IF(!aAny) || NS_WARN_IF(!aAll)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  const nsAString* val = !aValue.IsEmpty() ? &aValue : nullptr;
  nsresult rv = GetInlinePropertyBase(aHTMLProperty, aAttribute, val, aFirst,
                                      aAny, aAll, nullptr);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::GetInlinePropertyBase() failed");
  return EditorBase::ToGenericNSResult(rv);
}

NS_IMETHODIMP HTMLEditor::GetInlinePropertyWithAttrValue(
    const nsAString& aHTMLProperty, const nsAString& aAttribute,
    const nsAString& aValue, bool* aFirst, bool* aAny, bool* aAll,
    nsAString& outValue) {
  nsStaticAtom* property = NS_GetStaticAtom(aHTMLProperty);
  if (NS_WARN_IF(!property)) {
    return NS_ERROR_INVALID_ARG;
  }
  nsStaticAtom* attribute = EditorUtils::GetAttributeAtom(aAttribute);
  // MOZ_KnownLive because nsStaticAtom is available until shutting down.
  nsresult rv = GetInlinePropertyWithAttrValue(MOZ_KnownLive(*property),
                                               MOZ_KnownLive(attribute), aValue,
                                               aFirst, aAny, aAll, outValue);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::GetInlinePropertyWithAttrValue() failed");
  return rv;
}

nsresult HTMLEditor::GetInlinePropertyWithAttrValue(
    nsStaticAtom& aHTMLProperty, nsAtom* aAttribute, const nsAString& aValue,
    bool* aFirst, bool* aAny, bool* aAll, nsAString& outValue) {
  if (NS_WARN_IF(!aFirst) || NS_WARN_IF(!aAny) || NS_WARN_IF(!aAll)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  const nsAString* val = !aValue.IsEmpty() ? &aValue : nullptr;
  nsresult rv = GetInlinePropertyBase(aHTMLProperty, aAttribute, val, aFirst,
                                      aAny, aAll, &outValue);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::GetInlinePropertyBase() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult HTMLEditor::RemoveAllInlinePropertiesAsAction(
    nsIPrincipal* aPrincipal) {
  AutoEditActionDataSetter editActionData(
      *this, EditAction::eRemoveAllInlineStyleProperties, aPrincipal);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  AutoPlaceholderBatch treatAsOneTransaction(
      *this, ScrollSelectionIntoView::Yes, __FUNCTION__);
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eRemoveAllTextProperties, nsIEditor::eNext,
      ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return EditorBase::ToGenericNSResult(ignoredError.StealNSResult());
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  AutoTArray<EditorInlineStyle, 1> removeAllInlineStyles;
  removeAllInlineStyles.AppendElement(EditorInlineStyle::RemoveAllStyles());
  rv = RemoveInlinePropertiesAsSubAction(removeAllInlineStyles);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::RemoveInlinePropertiesAsSubAction() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult HTMLEditor::RemoveInlinePropertyAsAction(nsStaticAtom& aHTMLProperty,
                                                  nsStaticAtom* aAttribute,
                                                  nsIPrincipal* aPrincipal) {
  AutoEditActionDataSetter editActionData(
      *this,
      HTMLEditUtils::GetEditActionForFormatText(aHTMLProperty, aAttribute,
                                                false),
      aPrincipal);
  switch (editActionData.GetEditAction()) {
    case EditAction::eRemoveFontFamilyProperty:
      MOZ_ASSERT(!u""_ns.IsVoid());
      editActionData.SetData(u""_ns);
      break;
    case EditAction::eRemoveColorProperty:
    case EditAction::eRemoveBackgroundColorPropertyInline:
      editActionData.SetColorData(u""_ns);
      break;
    default:
      break;
  }
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  AutoTArray<EditorInlineStyle, 8> removeInlineStyleAndRelatedElements;
  AppendInlineStyleAndRelatedStyle(EditorInlineStyle(aHTMLProperty, aAttribute),
                                   removeInlineStyleAndRelatedElements);
  rv = RemoveInlinePropertiesAsSubAction(removeInlineStyleAndRelatedElements);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::RemoveInlinePropertiesAsSubAction() failed");
  return EditorBase::ToGenericNSResult(rv);
}

NS_IMETHODIMP HTMLEditor::RemoveInlineProperty(const nsAString& aProperty,
                                               const nsAString& aAttribute) {
  nsStaticAtom* property = NS_GetStaticAtom(aProperty);
  nsStaticAtom* attribute = EditorUtils::GetAttributeAtom(aAttribute);

  AutoEditActionDataSetter editActionData(
      *this,
      HTMLEditUtils::GetEditActionForFormatText(*property, attribute, false));
  switch (editActionData.GetEditAction()) {
    case EditAction::eRemoveFontFamilyProperty:
      MOZ_ASSERT(!u""_ns.IsVoid());
      editActionData.SetData(u""_ns);
      break;
    case EditAction::eRemoveColorProperty:
    case EditAction::eRemoveBackgroundColorPropertyInline:
      editActionData.SetColorData(u""_ns);
      break;
    default:
      break;
  }
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  AutoTArray<EditorInlineStyle, 1> removeOneInlineStyle;
  removeOneInlineStyle.AppendElement(EditorInlineStyle(*property, attribute));
  rv = RemoveInlinePropertiesAsSubAction(removeOneInlineStyle);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::RemoveInlinePropertiesAsSubAction() failed");
  return EditorBase::ToGenericNSResult(rv);
}

void HTMLEditor::AppendInlineStyleAndRelatedStyle(
    const EditorInlineStyle& aStyleToRemove,
    nsTArray<EditorInlineStyle>& aStylesToRemove) const {
  if (aStyleToRemove.mHTMLProperty == nsGkAtoms::b) {
    EditorInlineStyle strong(*nsGkAtoms::strong);
    if (!aStylesToRemove.Contains(strong)) {
      aStylesToRemove.AppendElement(std::move(strong));
    }
  } else if (aStyleToRemove.mHTMLProperty == nsGkAtoms::i) {
    EditorInlineStyle em(*nsGkAtoms::em);
    if (!aStylesToRemove.Contains(em)) {
      aStylesToRemove.AppendElement(std::move(em));
    }
  } else if (aStyleToRemove.mHTMLProperty == nsGkAtoms::strike) {
    EditorInlineStyle s(*nsGkAtoms::s);
    if (!aStylesToRemove.Contains(s)) {
      aStylesToRemove.AppendElement(std::move(s));
    }
  } else if (aStyleToRemove.mHTMLProperty == nsGkAtoms::font) {
    if (aStyleToRemove.mAttribute == nsGkAtoms::size) {
      EditorInlineStyle big(*nsGkAtoms::big), small(*nsGkAtoms::small);
      if (!aStylesToRemove.Contains(big)) {
        aStylesToRemove.AppendElement(std::move(big));
      }
      if (!aStylesToRemove.Contains(small)) {
        aStylesToRemove.AppendElement(std::move(small));
      }
    }
    // Handling <tt> element code was implemented for composer (bug 115922).
    // This shouldn't work with Document.execCommand() for compatibility with
    // the other browsers.  Currently, edit action principal is set only when
    // the root caller is Document::ExecCommand() so that we should handle <tt>
    // element only when the principal is nullptr that must be only when XUL
    // command is executed on composer.
    else if (aStyleToRemove.mAttribute == nsGkAtoms::face &&
             !GetEditActionPrincipal()) {
      EditorInlineStyle tt(*nsGkAtoms::tt);
      if (!aStylesToRemove.Contains(tt)) {
        aStylesToRemove.AppendElement(std::move(tt));
      }
    }
  }
  if (!aStylesToRemove.Contains(aStyleToRemove)) {
    aStylesToRemove.AppendElement(aStyleToRemove);
  }
}

nsresult HTMLEditor::RemoveInlinePropertiesAsSubAction(
    const nsTArray<EditorInlineStyle>& aStylesToRemove) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!aStylesToRemove.IsEmpty());

  DebugOnly<nsresult> rvIgnored = CommitComposition();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "EditorBase::CommitComposition() failed, but ignored");

  if (SelectionRef().IsCollapsed()) {
    // Manipulating text attributes on a collapsed selection only sets state
    // for the next text insertion
    mPendingStylesToApplyToNewContent->ClearStyles(aStylesToRemove);
    return NS_OK;
  }

  // XXX Shouldn't we quit before calling `CommitComposition()`?
  if (IsInPlaintextMode()) {
    return NS_OK;
  }

  EditActionResult result = CanHandleHTMLEditSubAction();
  if (result.Failed() || result.Canceled()) {
    NS_WARNING_ASSERTION(result.Succeeded(),
                         "HTMLEditor::CanHandleHTMLEditSubAction() failed");
    return result.Rv();
  }

  AutoPlaceholderBatch treatAsOneTransaction(
      *this, ScrollSelectionIntoView::Yes, __FUNCTION__);
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eRemoveTextProperty, nsIEditor::eNext,
      ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  // TODO: We don't need AutoTransactionsConserveSelection here in the normal
  //       cases, but removing this may cause the behavior with the legacy
  //       mutation event listeners.  We should try to delete this in a bug.
  AutoTransactionsConserveSelection dontChangeMySelection(*this);

  AutoRangeArray selectionRanges(SelectionRef());
  for (const EditorInlineStyle& styleToRemove : aStylesToRemove) {
    // The ranges may be updated by changing the DOM tree.  In strictly
    // speaking, we should save and restore the ranges at every range loop,
    // but we've never done so and it may be expensive if there are a lot of
    // ranges.  Therefore, we should do it for every style handling for now.
    // TODO: We should collect everything required for removing the style before
    //       touching the DOM tree.  Then, we need to save and restore the
    //       ranges only once.
    MOZ_ALWAYS_TRUE(selectionRanges.SaveAndTrackRanges(*this));
    const bool isCSSInvertibleStyle =
        styleToRemove.mHTMLProperty &&
        CSSEditUtils::IsCSSInvertible(*styleToRemove.mHTMLProperty,
                                      styleToRemove.mAttribute);
    for (const OwningNonNull<nsRange>& range : selectionRanges.Ranges()) {
      if (styleToRemove.mHTMLProperty == nsGkAtoms::name) {
        // Promote range if it starts or end in a named anchor and we want to
        // remove named anchors
        nsresult rv = PromoteRangeIfStartsOrEndsInNamedAnchor(*range);
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "HTMLEditor::PromoteRangeIfStartsOrEndsInNamedAnchor() failed");
          return rv;
        }
      } else {
        // Adjust range to include any ancestors whose children are entirely
        // selected
        nsresult rv = PromoteInlineRange(*range);
        if (NS_FAILED(rv)) {
          NS_WARNING("HTMLEditor::PromoteInlineRange() failed");
          return rv;
        }
      }

      // Remove this style from ancestors of our range endpoints, splitting
      // them as appropriate
      SplitRangeOffResult splitRangeOffResult =
          SplitAncestorStyledInlineElementsAtRangeEdges(
              EditorDOMRange(range), styleToRemove.mHTMLProperty,
              styleToRemove.mAttribute);
      if (splitRangeOffResult.isErr()) {
        NS_WARNING(
            "HTMLEditor::SplitAncestorStyledInlineElementsAtRangeEdges() "
            "failed");
        return splitRangeOffResult.unwrapErr();
      }
      // There is AutoTransactionsConserveSelection, so we don't need to
      // update selection here.
      splitRangeOffResult.IgnoreCaretPointSuggestion();

      // XXX Modifying `range` means that we may modify ranges in `Selection`.
      //     Is this intentional?  Note that the range may be not in
      //     `Selection` too.  It seems that at least one of them is not
      //     an unexpected case.
      const EditorDOMRange& splitRange = splitRangeOffResult.RangeRef();
      if (NS_WARN_IF(!splitRange.IsPositioned())) {
        continue;
      }

      AutoTArray<OwningNonNull<nsIContent>, 64> arrayOfContentsToInvertStyle;
      {
        // Collect top level children in the range first.
        // TODO: Perhaps, HTMLEditUtils::IsSplittableNode should be used here
        //       instead of EditorUtils::IsEditableContent.
        AutoTArray<OwningNonNull<nsIContent>, 64> arrayOfContentsAroundRange;
        if (splitRange.InSameContainer() &&
            splitRange.StartRef().IsInTextNode()) {
          if (!EditorUtils::IsEditableContent(
                  *splitRange.StartRef().ContainerAs<Text>(),
                  EditorType::HTML)) {
            continue;
          }
          arrayOfContentsAroundRange.AppendElement(
              *splitRange.StartRef().ContainerAs<Text>());
        } else if (splitRange.IsInTextNodes() &&
                   splitRange.InAdjacentSiblings()) {
          // Adjacent siblings are in a same element, so the editable state of
          // both text nodes are always same.
          if (!EditorUtils::IsEditableContent(
                  *splitRange.StartRef().ContainerAs<Text>(),
                  EditorType::HTML)) {
            continue;
          }
          arrayOfContentsAroundRange.AppendElement(
              *splitRange.StartRef().ContainerAs<Text>());
          arrayOfContentsAroundRange.AppendElement(
              *splitRange.EndRef().ContainerAs<Text>());
        } else {
          // Append first node if it's a text node but selected not entirely.
          if (splitRange.StartRef().IsInTextNode() &&
              !splitRange.StartRef().IsStartOfContainer() &&
              EditorUtils::IsEditableContent(
                  *splitRange.StartRef().ContainerAs<Text>(),
                  EditorType::HTML)) {
            arrayOfContentsAroundRange.AppendElement(
                *splitRange.StartRef().ContainerAs<Text>());
          }
          // Append all entirely selected nodes.
          ContentSubtreeIterator subtreeIter;
          if (NS_SUCCEEDED(
                  subtreeIter.Init(splitRange.StartRef().ToRawRangeBoundary(),
                                   splitRange.EndRef().ToRawRangeBoundary()))) {
            for (; !subtreeIter.IsDone(); subtreeIter.Next()) {
              nsCOMPtr<nsINode> node = subtreeIter.GetCurrentNode();
              if (NS_WARN_IF(!node)) {
                return NS_ERROR_FAILURE;
              }
              if (node->IsContent() &&
                  EditorUtils::IsEditableContent(*node->AsContent(),
                                                 EditorType::HTML)) {
                arrayOfContentsAroundRange.AppendElement(*node->AsContent());
              }
            }
          }
          // Append last node if it's a text node but selected not entirely.
          if (!splitRange.InSameContainer() &&
              splitRange.EndRef().IsInTextNode() &&
              !splitRange.EndRef().IsEndOfContainer() &&
              EditorUtils::IsEditableContent(
                  *splitRange.EndRef().ContainerAs<Text>(), EditorType::HTML)) {
            arrayOfContentsAroundRange.AppendElement(
                *splitRange.EndRef().ContainerAs<Text>());
          }
        }
        if (isCSSInvertibleStyle) {
          arrayOfContentsToInvertStyle.SetCapacity(
              arrayOfContentsAroundRange.Length());
        }

        for (OwningNonNull<nsIContent>& content : arrayOfContentsAroundRange) {
          // We should remove style from the element and its descendants.
          if (content->IsElement()) {
            Result<EditorDOMPoint, nsresult> removeStyleResult =
                RemoveStyleInside(MOZ_KnownLive(*content->AsElement()),
                                  styleToRemove.mHTMLProperty,
                                  styleToRemove.mAttribute,
                                  SpecifiedStyle::Preserve);
            if (MOZ_UNLIKELY(removeStyleResult.isErr())) {
              NS_WARNING("HTMLEditor::RemoveStyleInside() failed");
              return removeStyleResult.unwrapErr();
            }
            // There is AutoTransactionsConserveSelection, so we don't need to
            // update selection here.

            // If the element was removed from the DOM tree by
            // RemoveStyleInside, we need to do nothing for it anymore.
            if (!content->GetParentNode()) {
              continue;
            }
          }

          if (isCSSInvertibleStyle) {
            arrayOfContentsToInvertStyle.AppendElement(content);
          }

          // If the style is specified in parent block and we can remove the
          // style with inserting new <span> element, we should do it.
          Result<bool, nsresult> isRemovableParentStyleOrError =
              IsRemovableParentStyleWithNewSpanElement(
                  MOZ_KnownLive(content), styleToRemove.mHTMLProperty,
                  styleToRemove.mAttribute);
          if (MOZ_UNLIKELY(isRemovableParentStyleOrError.isErr())) {
            NS_WARNING(
                "HTMLEditor::IsRemovableParentStyleWithNewSpanElement() "
                "failed");
            return isRemovableParentStyleOrError.unwrapErr();
          }
          if (!isRemovableParentStyleOrError.unwrap()) {
            // E.g., text-decoration cannot be override visually in children.
            // In such cases, we can do nothing.
            continue;
          }

          // If it's not a text node, should wrap it into a new element,
          // move it into direct child which has same style, or specify
          // the style to its parent.
          if (!content->IsText()) {
            // XXX Do we need to call this even when data node or something?  If
            //     so, for what?
            // MOZ_KnownLive because 'arrayOfContents' is guaranteed to
            // keep it alive.
            Result<EditorDOMPoint, nsresult> setStyleResult =
                SetInlinePropertyOnNode(
                    MOZ_KnownLive(content), *styleToRemove.mHTMLProperty,
                    styleToRemove.mAttribute, u"-moz-editor-invert-value"_ns);
            if (MOZ_UNLIKELY(setStyleResult.isErr())) {
              if (NS_WARN_IF(setStyleResult.unwrapErr() ==
                             NS_ERROR_EDITOR_DESTROYED)) {
                NS_WARNING(
                    "HTMLEditor::SetInlinePropertyOnNode(-moz-editor-invert-"
                    "value) failed");
                return NS_ERROR_EDITOR_DESTROYED;
              }
              NS_WARNING(
                  "HTMLEditor::SetInlinePropertyOnNode(-moz-editor-invert-"
                  "value) failed, but ignored");
            }
            // There is AutoTransactionsConserveSelection, so we don't need to
            // update selection here.
            continue;
          }

          // If current node is a text node, we need to create `<span>` element
          // for it to overwrite parent style.  Unfortunately, all browsers
          // don't join text nodes when removing a style.  Therefore, there
          // may be multiple text nodes as adjacent siblings.  That's the
          // reason why we need to handle text nodes in this loop.
          uint32_t startOffset = content == splitRange.StartRef().GetContainer()
                                     ? splitRange.StartRef().Offset()
                                     : 0;
          uint32_t endOffset = content == splitRange.EndRef().GetContainer()
                                   ? splitRange.EndRef().Offset()
                                   : content->Length();
          SplitRangeOffFromNodeResult wrapTextInStyledElementResult =
              SetInlinePropertyOnTextNode(
                  MOZ_KnownLive(*content->AsText()), startOffset, endOffset,
                  *styleToRemove.mHTMLProperty, styleToRemove.mAttribute,
                  u"-moz-editor-invert-value"_ns);
          if (wrapTextInStyledElementResult.isErr()) {
            NS_WARNING(
                "HTMLEditor::SetInlinePropertyOnTextNode(-moz-editor-invert-"
                "value) failed");
            return wrapTextInStyledElementResult.unwrapErr();
          }
          // There is AutoTransactionsConserveSelection, so we don't need to
          // update selection here.
          wrapTextInStyledElementResult.IgnoreCaretPointSuggestion();
          // If we've split the content, let's swap content in
          // arrayOfContentsToInvertStyle with the text node which is applied
          // the style.
          if (isCSSInvertibleStyle) {
            MOZ_ASSERT(
                wrapTextInStyledElementResult.GetMiddleContentAs<Text>());
            if (Text* textNode =
                    wrapTextInStyledElementResult.GetMiddleContentAs<Text>()) {
              if (textNode != content) {
                arrayOfContentsToInvertStyle.ReplaceElementAt(
                    arrayOfContentsToInvertStyle.Length() - 1,
                    OwningNonNull<nsIContent>(*textNode));
              }
            }
          }
        }
      }

      if (arrayOfContentsToInvertStyle.IsEmpty()) {
        continue;
      }
      MOZ_ASSERT(isCSSInvertibleStyle);

      // Finally, we should remove the style from all leaf text nodes if
      // they still have the style.
      AutoTArray<OwningNonNull<Text>, 32> leafTextNodes;
      for (const OwningNonNull<nsIContent>& content :
           arrayOfContentsToInvertStyle) {
        // XXX Should we ignore content which has already removed from the
        //     DOM tree by the previous for-loop?
        if (content->IsElement()) {
          CollectEditableLeafTextNodes(*content->AsElement(), leafTextNodes);
        }
      }
      for (const OwningNonNull<Text>& textNode : leafTextNodes) {
        Result<bool, nsresult> isRemovableParentStyleOrError =
            IsRemovableParentStyleWithNewSpanElement(
                MOZ_KnownLive(textNode), styleToRemove.mHTMLProperty,
                styleToRemove.mAttribute);
        if (isRemovableParentStyleOrError.isErr()) {
          NS_WARNING(
              "HTMLEditor::IsRemovableParentStyleWithNewSpanElement() "
              "failed");
          return isRemovableParentStyleOrError.unwrapErr();
        }
        if (!isRemovableParentStyleOrError.unwrap()) {
          continue;
        }
        // MOZ_KnownLive because 'leafTextNodes' is guaranteed to
        // keep it alive.
        SplitRangeOffFromNodeResult wrapTextInStyledElementResult =
            SetInlinePropertyOnTextNode(
                MOZ_KnownLive(textNode), 0, textNode->TextLength(),
                *styleToRemove.mHTMLProperty, styleToRemove.mAttribute,
                u"-moz-editor-invert-value"_ns);
        if (wrapTextInStyledElementResult.isErr()) {
          NS_WARNING(
              "HTMLEditor::SetInlinePropertyOnTextNode(-moz-editor-invert-"
              "value) failed");
          return wrapTextInStyledElementResult.unwrapErr();
        }
        // There is AutoTransactionsConserveSelection, so we don't need to
        // update selection here.
        wrapTextInStyledElementResult.IgnoreCaretPointSuggestion();
      }  // for-loop of leafTextNodes
    }    // for-loop of selectionRanges
    MOZ_ASSERT(selectionRanges.HasSavedRanges());
    selectionRanges.RestoreFromSavedRanges();
  }  // for-loop of styles

  MOZ_ASSERT(!selectionRanges.HasSavedRanges());
  nsresult rv = selectionRanges.ApplyTo(SelectionRef());
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "AutoRangeArray::ApplyTo() failed");
  return rv;
}

Result<bool, nsresult> HTMLEditor::IsRemovableParentStyleWithNewSpanElement(
    nsIContent& aContent, nsAtom* aHTMLProperty, nsAtom* aAttribute) const {
  // We don't support to remove all inline styles with this path.
  if (!aHTMLProperty) {
    return false;
  }

  // First check whether the style is invertible since this is the fastest
  // check.
  if (!CSSEditUtils::IsCSSInvertible(*aHTMLProperty, aAttribute)) {
    return false;
  }

  // If parent block has the removing style, we should create `<span>`
  // element to remove the style even in HTML mode since Chrome does it.
  if (!CSSEditUtils::IsCSSEditableProperty(&aContent, aHTMLProperty,
                                           aAttribute)) {
    return false;
  }

  // aContent's computed style indicates the CSS equivalence to
  // the HTML style to remove is applied; but we found no element
  // in the ancestors of aContent carrying specified styles;
  // assume it comes from a rule and let's try to insert a span
  // "inverting" the style
  nsAutoString emptyString;
  Result<bool, nsresult> isComputedCSSEquivalentToHTMLInlineStyleOrError =
      CSSEditUtils::IsComputedCSSEquivalentToHTMLInlineStyleSet(
          *this, aContent, aHTMLProperty, aAttribute, emptyString);
  NS_WARNING_ASSERTION(
      isComputedCSSEquivalentToHTMLInlineStyleOrError.isOk(),
      "CSSEditUtils::IsComputedCSSEquivalentToHTMLInlineStyleSet() "
      "failed");
  return isComputedCSSEquivalentToHTMLInlineStyleOrError;
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

nsresult HTMLEditor::IncreaseFontSizeAsAction(nsIPrincipal* aPrincipal) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eIncrementFontSize,
                                          aPrincipal);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  rv = IncrementOrDecrementFontSizeAsSubAction(FontSize::incr);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::IncrementOrDecrementFontSizeAsSubAction("
                       "FontSize::incr) failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult HTMLEditor::DecreaseFontSizeAsAction(nsIPrincipal* aPrincipal) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eDecrementFontSize,
                                          aPrincipal);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  rv = IncrementOrDecrementFontSizeAsSubAction(FontSize::decr);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::IncrementOrDecrementFontSizeAsSubAction("
                       "FontSize::decr) failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult HTMLEditor::IncrementOrDecrementFontSizeAsSubAction(
    FontSize aIncrementOrDecrement) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // Committing composition and changing font size should be undone together.
  AutoPlaceholderBatch treatAsOneTransaction(
      *this, ScrollSelectionIntoView::Yes, __FUNCTION__);

  DebugOnly<nsresult> rvIgnored = CommitComposition();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "EditorBase::CommitComposition() failed, but ignored");

  // If selection is collapsed, set typing state
  if (SelectionRef().IsCollapsed()) {
    nsStaticAtom& bigOrSmallTagName = aIncrementOrDecrement == FontSize::incr
                                          ? *nsGkAtoms::big
                                          : *nsGkAtoms::small;

    // Let's see in what kind of element the selection is
    if (!SelectionRef().RangeCount()) {
      return NS_OK;
    }
    const auto firstRangeStartPoint =
        EditorBase::GetFirstSelectionStartPoint<EditorRawDOMPoint>();
    if (NS_WARN_IF(!firstRangeStartPoint.IsSet())) {
      return NS_OK;
    }
    Element* element =
        firstRangeStartPoint.GetContainerOrContainerParentElement();
    if (NS_WARN_IF(!element)) {
      return NS_OK;
    }
    if (!HTMLEditUtils::CanNodeContain(*element, bigOrSmallTagName)) {
      return NS_OK;
    }

    // Manipulating text attributes on a collapsed selection only sets state
    // for the next text insertion
    mPendingStylesToApplyToNewContent->PreserveStyle(bigOrSmallTagName, nullptr,
                                                     u""_ns);
    return NS_OK;
  }

  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eSetTextProperty, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  // TODO: We don't need AutoTransactionsConserveSelection here in the normal
  //       cases, but removing this may cause the behavior with the legacy
  //       mutation event listeners.  We should try to delete this in a bug.
  AutoTransactionsConserveSelection dontChangeMySelection(*this);

  AutoRangeArray selectionRanges(SelectionRef());
  MOZ_ALWAYS_TRUE(selectionRanges.SaveAndTrackRanges(*this));
  for (const OwningNonNull<nsRange>& domRange : selectionRanges.Ranges()) {
    // Adjust range to include any ancestors with entirely selected children
    if (NS_FAILED(PromoteInlineRange(*domRange))) {
      NS_WARNING("HTMLEditor::PromoteInlineRange() failed");
      // for consistency with setting/removing inline styles, we should keep
      // handling the other ranges.
      continue;
    }

    EditorDOMRange range(domRange);
    if (NS_WARN_IF(!range.IsPositioned())) {
      continue;
    }

    if (range.InSameContainer() && range.StartRef().IsInTextNode()) {
      CreateElementResult wrapWithBigOrSmallElementResult =
          SetFontSizeOnTextNode(
              MOZ_KnownLive(*range.StartRef().ContainerAs<Text>()),
              range.StartRef().Offset(), range.EndRef().Offset(),
              aIncrementOrDecrement);
      if (wrapWithBigOrSmallElementResult.isErr()) {
        NS_WARNING("HTMLEditor::SetFontSizeOnTextNode() failed");
        return wrapWithBigOrSmallElementResult.unwrapErr();
      }
      // There is an AutoTransactionsConserveSelection instance so that we don't
      // need to update selection for this change.
      wrapWithBigOrSmallElementResult.IgnoreCaretPointSuggestion();
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

    // Iterate range and build up array
    ContentSubtreeIterator subtreeIter;
    if (NS_SUCCEEDED(subtreeIter.Init(range.StartRef().ToRawRangeBoundary(),
                                      range.EndRef().ToRawRangeBoundary()))) {
      nsTArray<OwningNonNull<nsIContent>> arrayOfContents;
      for (; !subtreeIter.IsDone(); subtreeIter.Next()) {
        if (NS_WARN_IF(!subtreeIter.GetCurrentNode()->IsContent())) {
          return NS_ERROR_FAILURE;
        }
        OwningNonNull<nsIContent> content =
            *subtreeIter.GetCurrentNode()->AsContent();

        if (EditorUtils::IsEditableContent(content, EditorType::HTML)) {
          arrayOfContents.AppendElement(content);
        }
      }

      // Now that we have the list, do the font size change on each node
      for (OwningNonNull<nsIContent>& content : arrayOfContents) {
        // MOZ_KnownLive because of bug 1622253
        Result<EditorDOMPoint, nsresult> fontChangeOnNodeResult =
            SetFontSizeWithBigOrSmallElement(MOZ_KnownLive(content),
                                             aIncrementOrDecrement);
        if (MOZ_UNLIKELY(fontChangeOnNodeResult.isErr())) {
          NS_WARNING("HTMLEditor::SetFontSizeWithBigOrSmallElement() failed");
          return fontChangeOnNodeResult.unwrapErr();
        }
        // There is an AutoTransactionsConserveSelection, so we don't need to
        // update selection here.
      }
    }
    // Now check the start and end parents of the range to see if they need
    // to be separately handled (they do if they are text nodes, due to how
    // the subtree iterator works - it will not have reported them).
    if (range.StartRef().IsInTextNode() &&
        !range.StartRef().IsEndOfContainer() &&
        EditorUtils::IsEditableContent(*range.StartRef().ContainerAs<Text>(),
                                       EditorType::HTML)) {
      CreateElementResult wrapWithBigOrSmallElementResult =
          SetFontSizeOnTextNode(
              MOZ_KnownLive(*range.StartRef().ContainerAs<Text>()),
              range.StartRef().Offset(),
              range.StartRef().ContainerAs<Text>()->TextDataLength(),
              aIncrementOrDecrement);
      if (wrapWithBigOrSmallElementResult.isErr()) {
        NS_WARNING("HTMLEditor::SetFontSizeOnTextNode() failed");
        return wrapWithBigOrSmallElementResult.unwrapErr();
      }
      // There is an AutoTransactionsConserveSelection instance so that we
      // don't need to update selection for this change.
      wrapWithBigOrSmallElementResult.IgnoreCaretPointSuggestion();
    }
    if (range.EndRef().IsInTextNode() && !range.EndRef().IsStartOfContainer() &&
        EditorUtils::IsEditableContent(*range.EndRef().ContainerAs<Text>(),
                                       EditorType::HTML)) {
      CreateElementResult wrapWithBigOrSmallElementResult =
          SetFontSizeOnTextNode(
              MOZ_KnownLive(*range.EndRef().ContainerAs<Text>()), 0u,
              range.EndRef().Offset(), aIncrementOrDecrement);
      if (wrapWithBigOrSmallElementResult.isErr()) {
        NS_WARNING("HTMLEditor::SetFontSizeOnTextNode() failed");
        return wrapWithBigOrSmallElementResult.unwrapErr();
      }
      // There is an AutoTransactionsConserveSelection instance so that we
      // don't need to update selection for this change.
      wrapWithBigOrSmallElementResult.IgnoreCaretPointSuggestion();
    }
  }

  MOZ_ASSERT(selectionRanges.HasSavedRanges());
  selectionRanges.RestoreFromSavedRanges();
  nsresult rv = selectionRanges.ApplyTo(SelectionRef());
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "AutoRangeArray::ApplyTo() failed");
  return rv;
}

CreateElementResult HTMLEditor::SetFontSizeOnTextNode(
    Text& aTextNode, uint32_t aStartOffset, uint32_t aEndOffset,
    FontSize aIncrementOrDecrement) {
  // Don't need to do anything if no characters actually selected
  if (aStartOffset == aEndOffset) {
    return CreateElementResult::NotHandled();
  }

  if (!aTextNode.GetParentNode() ||
      !HTMLEditUtils::CanNodeContain(*aTextNode.GetParentNode(),
                                     *nsGkAtoms::big)) {
    return CreateElementResult::NotHandled();
  }

  aEndOffset = std::min(aTextNode.Length(), aEndOffset);

  // Make the range an independent node.
  RefPtr<Text> textNodeForTheRange = &aTextNode;

  EditorDOMPoint pointToPutCaret;
  {
    auto pointToPutCaretOrError =
        [&]() MOZ_CAN_RUN_SCRIPT -> Result<EditorDOMPoint, nsresult> {
      EditorDOMPoint pointToPutCaret;
      // Split at the end of the range.
      EditorDOMPoint atEnd(textNodeForTheRange, aEndOffset);
      if (!atEnd.IsEndOfContainer()) {
        // We need to split off back of text node
        SplitNodeResult splitAtEndResult = SplitNodeWithTransaction(atEnd);
        if (splitAtEndResult.isErr()) {
          NS_WARNING("HTMLEditor::SplitNodeWithTransaction() failed");
          return Err(splitAtEndResult.unwrapErr());
        }
        if (MOZ_UNLIKELY(!splitAtEndResult.HasCaretPointSuggestion())) {
          NS_WARNING(
              "HTMLEditor::SplitNodeWithTransaction() didn't suggest caret "
              "point");
          return Err(NS_ERROR_FAILURE);
        }
        splitAtEndResult.MoveCaretPointTo(pointToPutCaret, *this, {});
        MOZ_ASSERT_IF(AllowsTransactionsToChangeSelection(),
                      pointToPutCaret.IsSet());
        textNodeForTheRange =
            Text::FromNodeOrNull(splitAtEndResult.GetPreviousContent());
        MOZ_DIAGNOSTIC_ASSERT(textNodeForTheRange);
        // When adding caret suggestion to SplitNodeResult, here didn't change
        // selection so that just ignore it.
        splitAtEndResult.IgnoreCaretPointSuggestion();
      }

      // Split at the start of the range.
      EditorDOMPoint atStart(textNodeForTheRange, aStartOffset);
      if (!atStart.IsStartOfContainer()) {
        // We need to split off front of text node
        SplitNodeResult splitAtStartResult = SplitNodeWithTransaction(atStart);
        if (splitAtStartResult.isErr()) {
          NS_WARNING("HTMLEditor::SplitNodeWithTransaction() failed");
          return Err(splitAtStartResult.unwrapErr());
        }
        if (MOZ_UNLIKELY(!splitAtStartResult.HasCaretPointSuggestion())) {
          NS_WARNING(
              "HTMLEditor::SplitNodeWithTransaction() didn't suggest caret "
              "point");
          return Err(NS_ERROR_FAILURE);
        }
        splitAtStartResult.MoveCaretPointTo(pointToPutCaret, *this, {});
        MOZ_ASSERT_IF(AllowsTransactionsToChangeSelection(),
                      pointToPutCaret.IsSet());
        textNodeForTheRange =
            Text::FromNodeOrNull(splitAtStartResult.GetNextContent());
        MOZ_DIAGNOSTIC_ASSERT(textNodeForTheRange);
        // When adding caret suggestion to SplitNodeResult, here didn't change
        // selection so that just ignore it.
        splitAtStartResult.IgnoreCaretPointSuggestion();
      }

      return pointToPutCaret;
    }();
    if (MOZ_UNLIKELY(pointToPutCaretOrError.isErr())) {
      // Don't warn here since it should be done in the lambda.
      return CreateElementResult(pointToPutCaretOrError.unwrapErr());
    }
    pointToPutCaret = pointToPutCaretOrError.unwrap();
  }

  // Look for siblings that are correct type of node
  nsStaticAtom* const bigOrSmallTagName =
      aIncrementOrDecrement == FontSize::incr ? nsGkAtoms::big
                                              : nsGkAtoms::small;
  nsCOMPtr<nsIContent> sibling = HTMLEditUtils::GetPreviousSibling(
      *textNodeForTheRange, {WalkTreeOption::IgnoreNonEditableNode});
  if (sibling && sibling->IsHTMLElement(bigOrSmallTagName)) {
    // Previous sib is already right kind of inline node; slide this over
    MoveNodeResult moveTextNodeResult =
        MoveNodeToEndWithTransaction(*textNodeForTheRange, *sibling);
    if (moveTextNodeResult.isErr()) {
      NS_WARNING("HTMLEditor::MoveNodeToEndWithTransaction() failed");
      return CreateElementResult(moveTextNodeResult.unwrapErr());
    }
    moveTextNodeResult.MoveCaretPointTo(pointToPutCaret, *this,
                                        {SuggestCaret::OnlyIfHasSuggestion});
    // XXX Should we return the new container?
    return CreateElementResult::NotHandled(std::move(pointToPutCaret));
  }
  sibling = HTMLEditUtils::GetNextSibling(
      *textNodeForTheRange, {WalkTreeOption::IgnoreNonEditableNode});
  if (sibling && sibling->IsHTMLElement(bigOrSmallTagName)) {
    // Following sib is already right kind of inline node; slide this over
    MoveNodeResult moveTextNodeResult = MoveNodeWithTransaction(
        *textNodeForTheRange, EditorDOMPoint(sibling, 0u));
    if (moveTextNodeResult.isErr()) {
      NS_WARNING("HTMLEditor::MoveNodeWithTransaction() failed");
      return CreateElementResult(moveTextNodeResult.unwrapErr());
    }
    moveTextNodeResult.MoveCaretPointTo(pointToPutCaret, *this,
                                        {SuggestCaret::OnlyIfHasSuggestion});
    // XXX Should we return the new container?
    return CreateElementResult::NotHandled(std::move(pointToPutCaret));
  }

  // Else wrap the node inside font node with appropriate relative size
  CreateElementResult wrapTextWithBigOrSmallElementResult =
      InsertContainerWithTransaction(*textNodeForTheRange,
                                     MOZ_KnownLive(*bigOrSmallTagName));
  if (wrapTextWithBigOrSmallElementResult.isErr()) {
    NS_WARNING("HTMLEditor::InsertContainerWithTransaction() failed");
    return wrapTextWithBigOrSmallElementResult;
  }
  wrapTextWithBigOrSmallElementResult.MoveCaretPointTo(
      pointToPutCaret, {SuggestCaret::OnlyIfHasSuggestion});
  return CreateElementResult(
      wrapTextWithBigOrSmallElementResult.UnwrapNewNode(),
      std::move(pointToPutCaret));
}

Result<EditorDOMPoint, nsresult> HTMLEditor::SetFontSizeOfFontElementChildren(
    nsIContent& aContent, FontSize aIncrementOrDecrement) {
  // This routine looks for all the font nodes in the tree rooted by aNode,
  // including aNode itself, looking for font nodes that have the size attr
  // set.  Any such nodes need to have big or small put inside them, since
  // they override any big/small that are above them.

  // If this is a font node with size, put big/small inside it.
  if (aContent.IsHTMLElement(nsGkAtoms::font) &&
      aContent.AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::size)) {
    EditorDOMPoint pointToPutCaret;

    // Cycle through children and adjust relative font size.
    AutoTArray<OwningNonNull<nsIContent>, 32> arrayOfContents;
    HTMLEditor::GetChildNodesOf(aContent, arrayOfContents);
    for (const auto& child : arrayOfContents) {
      // MOZ_KnownLive because of bug 1622253
      Result<EditorDOMPoint, nsresult> setFontSizeOfChildResult =
          SetFontSizeWithBigOrSmallElement(MOZ_KnownLive(child),
                                           aIncrementOrDecrement);
      if (MOZ_UNLIKELY(setFontSizeOfChildResult.isErr())) {
        NS_WARNING("HTMLEditor::WrapContentInBigOrSmallElement() failed");
        return setFontSizeOfChildResult;
      }
      if (setFontSizeOfChildResult.inspect().IsSet()) {
        pointToPutCaret = setFontSizeOfChildResult.unwrap();
      }
    }

    // WrapContentInBigOrSmallElement already calls us recursively,
    // so we don't need to check our children again.
    return pointToPutCaret;
  }

  // Otherwise cycle through the children.
  EditorDOMPoint pointToPutCaret;
  AutoTArray<OwningNonNull<nsIContent>, 32> arrayOfContents;
  HTMLEditor::GetChildNodesOf(aContent, arrayOfContents);
  for (const auto& child : arrayOfContents) {
    // MOZ_KnownLive because of bug 1622253
    Result<EditorDOMPoint, nsresult> fontSizeChangeResult =
        SetFontSizeOfFontElementChildren(MOZ_KnownLive(child),
                                         aIncrementOrDecrement);
    if (MOZ_UNLIKELY(fontSizeChangeResult.isErr())) {
      NS_WARNING("HTMLEditor::SetFontSizeOfFontElementChildren() failed");
      return fontSizeChangeResult;
    }
    if (fontSizeChangeResult.inspect().IsSet()) {
      pointToPutCaret = fontSizeChangeResult.unwrap();
    }
  }

  return pointToPutCaret;
}

Result<EditorDOMPoint, nsresult> HTMLEditor::SetFontSizeWithBigOrSmallElement(
    nsIContent& aContent, FontSize aIncrementOrDecrement) {
  nsStaticAtom* const bigOrSmallTagName =
      aIncrementOrDecrement == FontSize::incr ? nsGkAtoms::big
                                              : nsGkAtoms::small;

  // Is aContent the opposite of what we want?
  if ((aIncrementOrDecrement == FontSize::incr &&
       aContent.IsHTMLElement(nsGkAtoms::small)) ||
      (aIncrementOrDecrement == FontSize::decr &&
       aContent.IsHTMLElement(nsGkAtoms::big))) {
    // First, populate any nested font elements that have the size attr set
    Result<EditorDOMPoint, nsresult> fontSizeChangeOfDescendantsResult =
        SetFontSizeOfFontElementChildren(aContent, aIncrementOrDecrement);
    if (MOZ_UNLIKELY(fontSizeChangeOfDescendantsResult.isErr())) {
      NS_WARNING("HTMLEditor::SetFontSizeOfFontElementChildren() failed");
      return fontSizeChangeOfDescendantsResult;
    }
    EditorDOMPoint pointToPutCaret = fontSizeChangeOfDescendantsResult.unwrap();
    // In that case, just unwrap the <big> or <small> element.
    Result<EditorDOMPoint, nsresult> unwrapBigOrSmallElementResult =
        RemoveContainerWithTransaction(MOZ_KnownLive(*aContent.AsElement()));
    if (MOZ_UNLIKELY(unwrapBigOrSmallElementResult.isErr())) {
      NS_WARNING("HTMLEditor::RemoveContainerWithTransaction() failed");
      return unwrapBigOrSmallElementResult;
    }
    if (unwrapBigOrSmallElementResult.inspect().IsSet()) {
      pointToPutCaret = unwrapBigOrSmallElementResult.unwrap();
    }
    return pointToPutCaret;
  }

  if (HTMLEditUtils::CanNodeContain(*bigOrSmallTagName, aContent)) {
    // First, populate any nested font tags that have the size attr set
    Result<EditorDOMPoint, nsresult> fontSizeChangeOfDescendantsResult =
        SetFontSizeOfFontElementChildren(aContent, aIncrementOrDecrement);
    if (MOZ_UNLIKELY(fontSizeChangeOfDescendantsResult.isErr())) {
      NS_WARNING("HTMLEditor::SetFontSizeOfFontElementChildren() failed");
      return fontSizeChangeOfDescendantsResult;
    }

    EditorDOMPoint pointToPutCaret = fontSizeChangeOfDescendantsResult.unwrap();

    // Next, if next or previous is <big> or <small>, move aContent into it.
    nsCOMPtr<nsIContent> sibling = HTMLEditUtils::GetPreviousSibling(
        aContent, {WalkTreeOption::IgnoreNonEditableNode});
    if (sibling && sibling->IsHTMLElement(bigOrSmallTagName)) {
      MoveNodeResult moveNodeResult =
          MoveNodeToEndWithTransaction(aContent, *sibling);
      if (moveNodeResult.isErr()) {
        NS_WARNING("HTMLEditor::MoveNodeToEndWithTransaction() failed");
        return Err(moveNodeResult.unwrapErr());
      }
      moveNodeResult.MoveCaretPointTo(pointToPutCaret,
                                      {SuggestCaret::OnlyIfHasSuggestion});
      return pointToPutCaret;
    }

    sibling = HTMLEditUtils::GetNextSibling(
        aContent, {WalkTreeOption::IgnoreNonEditableNode});
    if (sibling && sibling->IsHTMLElement(bigOrSmallTagName)) {
      MoveNodeResult moveNodeResult =
          MoveNodeWithTransaction(aContent, EditorDOMPoint(sibling, 0u));
      if (moveNodeResult.isErr()) {
        NS_WARNING("HTMLEditor::MoveNodeWithTransaction() failed");
        return Err(moveNodeResult.unwrapErr());
      }
      moveNodeResult.MoveCaretPointTo(pointToPutCaret,
                                      {SuggestCaret::OnlyIfHasSuggestion});
      return pointToPutCaret;
    }

    // Otherwise, wrap aContent in new <big> or <small>
    CreateElementResult wrapInBigOrSmallElementResult =
        InsertContainerWithTransaction(aContent,
                                       MOZ_KnownLive(*bigOrSmallTagName));
    if (wrapInBigOrSmallElementResult.isErr()) {
      NS_WARNING("HTMLEditor::InsertContainerWithTransaction() failed");
      return Err(wrapInBigOrSmallElementResult.unwrapErr());
    }
    MOZ_ASSERT(wrapInBigOrSmallElementResult.GetNewNode());
    wrapInBigOrSmallElementResult.MoveCaretPointTo(
        pointToPutCaret, {SuggestCaret::OnlyIfHasSuggestion});
    return pointToPutCaret;
  }

  // none of the above?  then cycle through the children.
  // MOOSE: we should group the children together if possible
  // into a single "big" or "small".  For the moment they are
  // each getting their own.
  EditorDOMPoint pointToPutCaret;
  AutoTArray<OwningNonNull<nsIContent>, 32> arrayOfContents;
  HTMLEditor::GetChildNodesOf(aContent, arrayOfContents);
  for (const auto& child : arrayOfContents) {
    // MOZ_KnownLive because of bug 1622253
    Result<EditorDOMPoint, nsresult> setFontSizeOfChildResult =
        SetFontSizeWithBigOrSmallElement(MOZ_KnownLive(child),
                                         aIncrementOrDecrement);
    if (MOZ_UNLIKELY(setFontSizeOfChildResult.isErr())) {
      NS_WARNING("HTMLEditor::SetFontSizeWithBigOrSmallElement() failed");
      return setFontSizeOfChildResult;
    }
    if (setFontSizeOfChildResult.inspect().IsSet()) {
      pointToPutCaret = setFontSizeOfChildResult.unwrap();
    }
  }

  return pointToPutCaret;
}

NS_IMETHODIMP HTMLEditor::GetFontFaceState(bool* aMixed, nsAString& outFace) {
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
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "HTMLEditor::GetInlinePropertyBase(nsGkAtoms::font, nsGkAtoms::face) "
        "failed");
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
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::GetInlinePropertyBase(nsGkAtoms::tt) failed");
    return EditorBase::ToGenericNSResult(rv);
  }
  if (any && !all) {
    return NS_OK;  // mixed
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
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "HTMLEditor::GetInlinePropertyBase(nsGkAtoms::font, nsGkAtoms::color) "
        "failed");
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

// The return value is true only if the instance of the HTML editor we created
// can handle CSS styles and if the CSS preference is checked.
NS_IMETHODIMP HTMLEditor::GetIsCSSEnabled(bool* aIsCSSEnabled) {
  *aIsCSSEnabled = IsCSSEnabled();
  return NS_OK;
}

bool HTMLEditor::HasStyleOrIdOrClassAttribute(Element& aElement) {
  return aElement.HasNonEmptyAttr(nsGkAtoms::style) ||
         aElement.HasNonEmptyAttr(nsGkAtoms::_class) ||
         aElement.HasNonEmptyAttr(nsGkAtoms::id);
}

}  // namespace mozilla
