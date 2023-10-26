/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLEditor.h"

#include <algorithm>
#include <utility>

#include "AutoRangeArray.h"
#include "CSSEditUtils.h"
#include "EditAction.h"
#include "EditorUtils.h"
#include "HTMLEditHelpers.h"
#include "HTMLEditUtils.h"
#include "WSRunObject.h"

#include "mozilla/Assertions.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Selection.h"

#include "nsAString.h"
#include "nsAlgorithm.h"
#include "nsAtom.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsINode.h"
#include "nsRange.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsTArray.h"

// NOTE: This file was split from:
//   https://searchfox.org/mozilla-central/rev/c409dd9235c133ab41eba635f906aa16e050c197/editor/libeditor/HTMLEditSubActionHandler.cpp

namespace mozilla {

using EditorType = EditorUtils::EditorType;
using WalkTreeOption = HTMLEditUtils::WalkTreeOption;

/*****************************************************************************
 * ListElementSelectionState
 ****************************************************************************/

ListElementSelectionState::ListElementSelectionState(HTMLEditor& aHTMLEditor,
                                                     ErrorResult& aRv) {
  MOZ_ASSERT(!aRv.Failed());

  if (NS_WARN_IF(aHTMLEditor.Destroyed())) {
    aRv.Throw(NS_ERROR_EDITOR_DESTROYED);
    return;
  }

  // XXX Should we create another constructor which won't create
  //     AutoEditActionDataSetter?  Or should we create another
  //     AutoEditActionDataSetter which won't nest edit action?
  EditorBase::AutoEditActionDataSetter editActionData(aHTMLEditor,
                                                      EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    aRv = EditorBase::ToGenericNSResult(NS_ERROR_EDITOR_DESTROYED);
    return;
  }

  Element* editingHostOrRoot = aHTMLEditor.ComputeEditingHost();
  if (!editingHostOrRoot) {
    // This is not a handler of editing command so that if there is no active
    // editing host, let's use the <body> or document element instead.
    editingHostOrRoot = aHTMLEditor.GetRoot();
    if (!editingHostOrRoot) {
      return;
    }
  }

  AutoTArray<OwningNonNull<nsIContent>, 64> arrayOfContents;
  {
    AutoRangeArray extendedSelectionRanges(aHTMLEditor.SelectionRef());
    extendedSelectionRanges.ExtendRangesToWrapLinesToHandleBlockLevelEditAction(
        EditSubAction::eCreateOrChangeList, *editingHostOrRoot);
    nsresult rv = extendedSelectionRanges.CollectEditTargetNodes(
        aHTMLEditor, arrayOfContents, EditSubAction::eCreateOrChangeList,
        AutoRangeArray::CollectNonEditableNodes::No);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "AutoRangeArray::CollectEditTargetNodes(EditSubAction::"
          "eCreateOrChangeList, CollectNonEditableNodes::No) failed");
      aRv = EditorBase::ToGenericNSResult(rv);
      return;
    }
  }

  // Examine list type for nodes in selection.
  for (const auto& content : arrayOfContents) {
    if (!content->IsElement()) {
      mIsOtherContentSelected = true;
    } else if (content->IsHTMLElement(nsGkAtoms::ul)) {
      mIsULElementSelected = true;
    } else if (content->IsHTMLElement(nsGkAtoms::ol)) {
      mIsOLElementSelected = true;
    } else if (content->IsHTMLElement(nsGkAtoms::li)) {
      if (dom::Element* parent = content->GetParentElement()) {
        if (parent->IsHTMLElement(nsGkAtoms::ul)) {
          mIsULElementSelected = true;
        } else if (parent->IsHTMLElement(nsGkAtoms::ol)) {
          mIsOLElementSelected = true;
        }
      }
    } else if (content->IsAnyOfHTMLElements(nsGkAtoms::dl, nsGkAtoms::dt,
                                            nsGkAtoms::dd)) {
      mIsDLElementSelected = true;
    } else {
      mIsOtherContentSelected = true;
    }

    if (mIsULElementSelected && mIsOLElementSelected && mIsDLElementSelected &&
        mIsOtherContentSelected) {
      break;
    }
  }
}

/*****************************************************************************
 * ListItemElementSelectionState
 ****************************************************************************/

ListItemElementSelectionState::ListItemElementSelectionState(
    HTMLEditor& aHTMLEditor, ErrorResult& aRv) {
  MOZ_ASSERT(!aRv.Failed());

  if (NS_WARN_IF(aHTMLEditor.Destroyed())) {
    aRv.Throw(NS_ERROR_EDITOR_DESTROYED);
    return;
  }

  // XXX Should we create another constructor which won't create
  //     AutoEditActionDataSetter?  Or should we create another
  //     AutoEditActionDataSetter which won't nest edit action?
  EditorBase::AutoEditActionDataSetter editActionData(aHTMLEditor,
                                                      EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    aRv = EditorBase::ToGenericNSResult(NS_ERROR_EDITOR_DESTROYED);
    return;
  }

  Element* editingHostOrRoot = aHTMLEditor.ComputeEditingHost();
  if (!editingHostOrRoot) {
    // This is not a handler of editing command so that if there is no active
    // editing host, let's use the <body> or document element instead.
    editingHostOrRoot = aHTMLEditor.GetRoot();
    if (!editingHostOrRoot) {
      return;
    }
  }

  AutoTArray<OwningNonNull<nsIContent>, 64> arrayOfContents;
  {
    AutoRangeArray extendedSelectionRanges(aHTMLEditor.SelectionRef());
    extendedSelectionRanges.ExtendRangesToWrapLinesToHandleBlockLevelEditAction(
        EditSubAction::eCreateOrChangeList, *editingHostOrRoot);
    nsresult rv = extendedSelectionRanges.CollectEditTargetNodes(
        aHTMLEditor, arrayOfContents, EditSubAction::eCreateOrChangeList,
        AutoRangeArray::CollectNonEditableNodes::No);
    if (NS_FAILED(rv)) {
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "AutoRangeArray::CollectEditTargetNodes(EditSubAction::"
          "eCreateOrChangeList, CollectNonEditableNodes::No) failed");
      aRv = EditorBase::ToGenericNSResult(rv);
      return;
    }
  }

  // examine list type for nodes in selection
  for (const auto& content : arrayOfContents) {
    if (!content->IsElement()) {
      mIsOtherElementSelected = true;
    } else if (content->IsAnyOfHTMLElements(nsGkAtoms::ul, nsGkAtoms::ol,
                                            nsGkAtoms::li)) {
      mIsLIElementSelected = true;
    } else if (content->IsHTMLElement(nsGkAtoms::dt)) {
      mIsDTElementSelected = true;
    } else if (content->IsHTMLElement(nsGkAtoms::dd)) {
      mIsDDElementSelected = true;
    } else if (content->IsHTMLElement(nsGkAtoms::dl)) {
      if (mIsDTElementSelected && mIsDDElementSelected) {
        continue;
      }
      // need to look inside dl and see which types of items it has
      DefinitionListItemScanner scanner(*content->AsElement());
      mIsDTElementSelected |= scanner.DTElementFound();
      mIsDDElementSelected |= scanner.DDElementFound();
    } else {
      mIsOtherElementSelected = true;
    }

    if (mIsLIElementSelected && mIsDTElementSelected && mIsDDElementSelected &&
        mIsOtherElementSelected) {
      break;
    }
  }
}

/*****************************************************************************
 * AlignStateAtSelection
 ****************************************************************************/

AlignStateAtSelection::AlignStateAtSelection(HTMLEditor& aHTMLEditor,
                                             ErrorResult& aRv) {
  MOZ_ASSERT(!aRv.Failed());

  if (NS_WARN_IF(aHTMLEditor.Destroyed())) {
    aRv = EditorBase::ToGenericNSResult(NS_ERROR_EDITOR_DESTROYED);
    return;
  }

  // XXX Should we create another constructor which won't create
  //     AutoEditActionDataSetter?  Or should we create another
  //     AutoEditActionDataSetter which won't nest edit action?
  EditorBase::AutoEditActionDataSetter editActionData(aHTMLEditor,
                                                      EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    aRv = EditorBase::ToGenericNSResult(NS_ERROR_EDITOR_DESTROYED);
    return;
  }

  if (aHTMLEditor.IsSelectionRangeContainerNotContent()) {
    NS_WARNING("Some selection containers are not content node, but ignored");
    return;
  }

  // For now, just return first alignment.  We don't check if it's mixed.
  // This is for efficiency given that our current UI doesn't care if it's
  // mixed.
  // cmanske: NOT TRUE! We would like to pay attention to mixed state in
  // [Format] -> [Align] submenu!

  // This routine assumes that alignment is done ONLY by `<div>` elements
  // if aHTMLEditor is not in CSS mode.

  if (NS_WARN_IF(!aHTMLEditor.GetRoot())) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  OwningNonNull<dom::Element> bodyOrDocumentElement = *aHTMLEditor.GetRoot();
  EditorRawDOMPoint atBodyOrDocumentElement(bodyOrDocumentElement);

  const nsRange* firstRange = aHTMLEditor.SelectionRef().GetRangeAt(0);
  mFoundSelectionRanges = !!firstRange;
  if (!mFoundSelectionRanges) {
    NS_WARNING("There was no selection range");
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
  EditorRawDOMPoint atStartOfSelection(firstRange->StartRef());
  if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
  MOZ_ASSERT(atStartOfSelection.IsSetAndValid());

  nsIContent* editTargetContent = nullptr;
  // If selection is collapsed or in a text node, take the container.
  if (aHTMLEditor.SelectionRef().IsCollapsed() ||
      atStartOfSelection.IsInTextNode()) {
    editTargetContent = atStartOfSelection.GetContainerAs<nsIContent>();
    if (NS_WARN_IF(!editTargetContent)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
  }
  // If selection container is the `<body>` element which is set to
  // `HTMLDocument.body`, take first editable node in it.
  // XXX Why don't we just compare `atStartOfSelection.GetChild()` and
  //     `bodyOrDocumentElement`?  Then, we can avoid computing the
  //     offset.
  else if (atStartOfSelection.IsContainerHTMLElement(nsGkAtoms::html) &&
           atBodyOrDocumentElement.IsSet() &&
           atStartOfSelection.Offset() == atBodyOrDocumentElement.Offset()) {
    editTargetContent = HTMLEditUtils::GetNextContent(
        atStartOfSelection, {WalkTreeOption::IgnoreNonEditableNode},
        BlockInlineCheck::Unused, aHTMLEditor.ComputeEditingHost());
    if (NS_WARN_IF(!editTargetContent)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
  }
  // Otherwise, use first selected node.
  // XXX Only for retrieving it, the following block treats all selected
  //     ranges.  `HTMLEditor` should have
  //     `GetFirstSelectionRangeExtendedToHardLineStartAndEnd()`.
  else {
    Element* editingHostOrRoot = aHTMLEditor.ComputeEditingHost();
    if (!editingHostOrRoot) {
      // This is not a handler of editing command so that if there is no active
      // editing host, let's use the <body> or document element instead.
      editingHostOrRoot = aHTMLEditor.GetRoot();
      if (!editingHostOrRoot) {
        return;
      }
    }
    AutoRangeArray extendedSelectionRanges(aHTMLEditor.SelectionRef());
    extendedSelectionRanges.ExtendRangesToWrapLinesToHandleBlockLevelEditAction(
        EditSubAction::eSetOrClearAlignment, *editingHostOrRoot);

    AutoTArray<OwningNonNull<nsIContent>, 64> arrayOfContents;
    nsresult rv = extendedSelectionRanges.CollectEditTargetNodes(
        aHTMLEditor, arrayOfContents, EditSubAction::eSetOrClearAlignment,
        AutoRangeArray::CollectNonEditableNodes::Yes);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "AutoRangeArray::CollectEditTargetNodes(eSetOrClearAlignment, "
          "CollectNonEditableNodes::Yes) failed");
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
    if (arrayOfContents.IsEmpty()) {
      NS_WARNING(
          "AutoRangeArray::CollectEditTargetNodes(eSetOrClearAlignment, "
          "CollectNonEditableNodes::Yes) returned no contents");
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
    editTargetContent = arrayOfContents[0];
  }

  const RefPtr<dom::Element> maybeNonEditableBlockElement =
      HTMLEditUtils::GetInclusiveAncestorElement(
          *editTargetContent, HTMLEditUtils::ClosestBlockElement,
          BlockInlineCheck::UseHTMLDefaultStyle);
  if (NS_WARN_IF(!maybeNonEditableBlockElement)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  if (aHTMLEditor.IsCSSEnabled() && EditorElementStyle::Align().IsCSSSettable(
                                        *maybeNonEditableBlockElement)) {
    // We are in CSS mode and we know how to align this element with CSS
    nsAutoString value;
    // Let's get the value(s) of text-align or margin-left/margin-right
    DebugOnly<nsresult> rvIgnored = CSSEditUtils::GetComputedCSSEquivalentTo(
        *maybeNonEditableBlockElement, EditorElementStyle::Align(), value);
    if (NS_WARN_IF(aHTMLEditor.Destroyed())) {
      aRv.Throw(NS_ERROR_EDITOR_DESTROYED);
      return;
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "CSSEditUtils::GetComputedCSSEquivalentTo("
                         "EditorElementStyle::Align()) failed, but ignored");
    if (value.EqualsLiteral(u"center") || value.EqualsLiteral(u"-moz-center") ||
        value.EqualsLiteral(u"auto auto")) {
      mFirstAlign = nsIHTMLEditor::eCenter;
      return;
    }
    if (value.EqualsLiteral(u"right") || value.EqualsLiteral(u"-moz-right") ||
        value.EqualsLiteral(u"auto 0px")) {
      mFirstAlign = nsIHTMLEditor::eRight;
      return;
    }
    if (value.EqualsLiteral(u"justify")) {
      mFirstAlign = nsIHTMLEditor::eJustify;
      return;
    }
    // XXX In RTL document, is this expected?
    mFirstAlign = nsIHTMLEditor::eLeft;
    return;
  }

  for (Element* const containerElement :
       editTargetContent->InclusiveAncestorsOfType<Element>()) {
    // If the node is a parent `<table>` element of edit target, let's break
    // here to materialize the 'inline-block' behaviour of html tables
    // regarding to text alignment.
    if (containerElement != editTargetContent &&
        containerElement->IsHTMLElement(nsGkAtoms::table)) {
      return;
    }

    if (EditorElementStyle::Align().IsCSSSettable(*containerElement)) {
      nsAutoString value;
      DebugOnly<nsresult> rvIgnored = CSSEditUtils::GetSpecifiedProperty(
          *containerElement, *nsGkAtoms::textAlign, value);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                           "CSSEditUtils::GetSpecifiedProperty(nsGkAtoms::"
                           "textAlign) failed, but ignored");
      if (!value.IsEmpty()) {
        if (value.EqualsLiteral("center")) {
          mFirstAlign = nsIHTMLEditor::eCenter;
          return;
        }
        if (value.EqualsLiteral("right")) {
          mFirstAlign = nsIHTMLEditor::eRight;
          return;
        }
        if (value.EqualsLiteral("justify")) {
          mFirstAlign = nsIHTMLEditor::eJustify;
          return;
        }
        if (value.EqualsLiteral("left")) {
          mFirstAlign = nsIHTMLEditor::eLeft;
          return;
        }
        // XXX
        // text-align: start and end aren't supported yet
      }
    }

    if (!HTMLEditUtils::SupportsAlignAttr(*containerElement)) {
      continue;
    }

    nsAutoString alignAttributeValue;
    containerElement->GetAttr(nsGkAtoms::align, alignAttributeValue);
    if (alignAttributeValue.IsEmpty()) {
      continue;
    }

    if (alignAttributeValue.LowerCaseEqualsASCII("center")) {
      mFirstAlign = nsIHTMLEditor::eCenter;
      return;
    }
    if (alignAttributeValue.LowerCaseEqualsASCII("right")) {
      mFirstAlign = nsIHTMLEditor::eRight;
      return;
    }
    // XXX This is odd case.  `<div align="justify">` is not in any standards.
    if (alignAttributeValue.LowerCaseEqualsASCII("justify")) {
      mFirstAlign = nsIHTMLEditor::eJustify;
      return;
    }
    // XXX In RTL document, is this expected?
    mFirstAlign = nsIHTMLEditor::eLeft;
    return;
  }
}

/*****************************************************************************
 * ParagraphStateAtSelection
 ****************************************************************************/

ParagraphStateAtSelection::ParagraphStateAtSelection(
    HTMLEditor& aHTMLEditor, FormatBlockMode aFormatBlockMode,
    ErrorResult& aRv) {
  if (NS_WARN_IF(aHTMLEditor.Destroyed())) {
    aRv = EditorBase::ToGenericNSResult(NS_ERROR_EDITOR_DESTROYED);
    return;
  }

  // XXX Should we create another constructor which won't create
  //     AutoEditActionDataSetter?  Or should we create another
  //     AutoEditActionDataSetter which won't nest edit action?
  EditorBase::AutoEditActionDataSetter editActionData(aHTMLEditor,
                                                      EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    aRv = EditorBase::ToGenericNSResult(NS_ERROR_EDITOR_DESTROYED);
    return;
  }

  if (aHTMLEditor.IsSelectionRangeContainerNotContent()) {
    NS_WARNING("Some selection containers are not content node, but ignored");
    return;
  }

  if (MOZ_UNLIKELY(!aHTMLEditor.SelectionRef().RangeCount())) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  const Element* const editingHostOrBodyOrDocumentElement = [&]() -> Element* {
    if (Element* editingHost = aHTMLEditor.ComputeEditingHost()) {
      return editingHost;
    }
    return aHTMLEditor.GetRoot();
  }();
  if (!editingHostOrBodyOrDocumentElement ||
      !HTMLEditUtils::IsSimplyEditableNode(
          *editingHostOrBodyOrDocumentElement)) {
    return;
  }

  AutoTArray<OwningNonNull<nsIContent>, 64> arrayOfContents;
  nsresult rv = CollectEditableFormatNodesInSelection(
      aHTMLEditor, aFormatBlockMode, *editingHostOrBodyOrDocumentElement,
      arrayOfContents);
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "ParagraphStateAtSelection::CollectEditableFormatNodesInSelection() "
        "failed");
    aRv.Throw(rv);
    return;
  }

  // We need to append descendant format block if block nodes are not format
  // block.  This is so we only have to look "up" the hierarchy to find
  // format nodes, instead of both up and down.
  for (size_t index : Reversed(IntegerRange(arrayOfContents.Length()))) {
    OwningNonNull<nsIContent>& content = arrayOfContents[index];
    if (HTMLEditUtils::IsBlockElement(content,
                                      BlockInlineCheck::UseHTMLDefaultStyle) &&
        !HTMLEditor::IsFormatElement(aFormatBlockMode, content)) {
      // XXX This RemoveObject() call has already been commented out and
      //     the above comment explained we're trying to replace non-format
      //     block nodes in the array.  According to the following blocks and
      //     `AppendDescendantFormatNodesAndFirstInlineNode()`, replacing
      //     non-format block with descendants format blocks makes sense.
      // arrayOfContents.RemoveObject(node);
      ParagraphStateAtSelection::AppendDescendantFormatNodesAndFirstInlineNode(
          arrayOfContents, aFormatBlockMode, *content->AsElement());
    }
  }

  // We might have an empty node list.  if so, find selection parent
  // and put that on the list
  if (arrayOfContents.IsEmpty()) {
    const auto atCaret =
        aHTMLEditor.GetFirstSelectionStartPoint<EditorRawDOMPoint>();
    if (NS_WARN_IF(!atCaret.IsInContentNode())) {
      MOZ_ASSERT(false,
                 "We've already checked whether there is a selection range, "
                 "but we have no range right now.");
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
    arrayOfContents.AppendElement(*atCaret.ContainerAs<nsIContent>());
  }

  for (auto& content : Reversed(arrayOfContents)) {
    const Element* formatElement = nullptr;
    if (HTMLEditor::IsFormatElement(aFormatBlockMode, content)) {
      formatElement = content->AsElement();
    }
    // Ignore inline contents since its children have been appended
    // the list above so that we'll handle this descendants later.
    // XXX: It's odd to ignore block children to consider the mixed state.
    else if (HTMLEditUtils::IsBlockElement(
                 content, BlockInlineCheck::UseHTMLDefaultStyle)) {
      continue;
    }
    // If we meet an inline node, let's get its parent format.
    else {
      for (Element* parentElement : content->AncestorsOfType<Element>()) {
        // If we reach `HTMLDocument.body` or `Document.documentElement`,
        // there is no format.
        if (parentElement == editingHostOrBodyOrDocumentElement) {
          break;
        }
        if (HTMLEditor::IsFormatElement(aFormatBlockMode, *parentElement)) {
          MOZ_ASSERT(parentElement->NodeInfo()->NameAtom());
          formatElement = parentElement;
          break;
        }
      }
    }

    auto FormatElementIsInclusiveDescendantOfFormatDLElement = [&]() {
      if (aFormatBlockMode == FormatBlockMode::XULParagraphStateCommand) {
        return false;
      }
      if (!formatElement) {
        return false;
      }
      for (const Element* const element :
           formatElement->InclusiveAncestorsOfType<Element>()) {
        if (element->IsHTMLElement(nsGkAtoms::dl)) {
          return true;
        }
        if (element->IsAnyOfHTMLElements(nsGkAtoms::dd, nsGkAtoms::dt)) {
          continue;
        }
        if (HTMLEditUtils::IsFormatElementForFormatBlockCommand(
                *formatElement)) {
          return false;
        }
      }
      return false;
    };

    // if this is the first node, we've found, remember it as the format
    if (!mFirstParagraphState) {
      mFirstParagraphState = formatElement
                                 ? formatElement->NodeInfo()->NameAtom()
                                 : nsGkAtoms::_empty;
      mIsInDLElement = FormatElementIsInclusiveDescendantOfFormatDLElement();
      continue;
    }
    mIsInDLElement &= FormatElementIsInclusiveDescendantOfFormatDLElement();
    // else make sure it matches previously found format
    if ((!formatElement && mFirstParagraphState != nsGkAtoms::_empty) ||
        (formatElement &&
         !formatElement->IsHTMLElement(mFirstParagraphState))) {
      mIsMixed = true;
      break;
    }
  }
}

// static
void ParagraphStateAtSelection::AppendDescendantFormatNodesAndFirstInlineNode(
    nsTArray<OwningNonNull<nsIContent>>& aArrayOfContents,
    FormatBlockMode aFormatBlockMode, dom::Element& aNonFormatBlockElement) {
  MOZ_ASSERT(HTMLEditUtils::IsBlockElement(
      aNonFormatBlockElement, BlockInlineCheck::UseHTMLDefaultStyle));
  MOZ_ASSERT(
      !HTMLEditor::IsFormatElement(aFormatBlockMode, aNonFormatBlockElement));

  // We only need to place any one inline inside this node onto
  // the list.  They are all the same for purposes of determining
  // paragraph style.  We use foundInline to track this as we are
  // going through the children in the loop below.
  bool foundInline = false;
  for (nsIContent* childContent = aNonFormatBlockElement.GetFirstChild();
       childContent; childContent = childContent->GetNextSibling()) {
    const bool isBlock = HTMLEditUtils::IsBlockElement(
        *childContent, BlockInlineCheck::UseHTMLDefaultStyle);
    const bool isFormat =
        HTMLEditor::IsFormatElement(aFormatBlockMode, *childContent);
    // If the child is a non-format block element, let's check its children
    // recursively.
    if (isBlock && !isFormat) {
      ParagraphStateAtSelection::AppendDescendantFormatNodesAndFirstInlineNode(
          aArrayOfContents, aFormatBlockMode, *childContent->AsElement());
      continue;
    }

    // If it's a format block, append it.
    if (isFormat) {
      aArrayOfContents.AppendElement(*childContent);
      continue;
    }

    MOZ_ASSERT(!isBlock);

    // If we haven't found inline node, append only this first inline node.
    // XXX I think that this makes sense if caller of this removes
    //     aNonFormatBlockElement from aArrayOfContents because the last loop
    //     of the constructor can check parent format block with
    //     aNonFormatBlockElement.
    if (!foundInline) {
      foundInline = true;
      aArrayOfContents.AppendElement(*childContent);
      continue;
    }
  }
}

// static
nsresult ParagraphStateAtSelection::CollectEditableFormatNodesInSelection(
    HTMLEditor& aHTMLEditor, FormatBlockMode aFormatBlockMode,
    const Element& aEditingHost,
    nsTArray<OwningNonNull<nsIContent>>& aArrayOfContents) {
  {
    AutoRangeArray extendedSelectionRanges(aHTMLEditor.SelectionRef());
    extendedSelectionRanges.ExtendRangesToWrapLinesToHandleBlockLevelEditAction(
        aFormatBlockMode == FormatBlockMode::HTMLFormatBlockCommand
            ? EditSubAction::eFormatBlockForHTMLCommand
            : EditSubAction::eCreateOrRemoveBlock,
        aEditingHost);
    nsresult rv = extendedSelectionRanges.CollectEditTargetNodes(
        aHTMLEditor, aArrayOfContents,
        aFormatBlockMode == FormatBlockMode::HTMLFormatBlockCommand
            ? EditSubAction::eFormatBlockForHTMLCommand
            : EditSubAction::eCreateOrRemoveBlock,
        AutoRangeArray::CollectNonEditableNodes::Yes);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "AutoRangeArray::CollectEditTargetNodes("
          "CollectNonEditableNodes::Yes) failed");
      return rv;
    }
  }

  // Pre-process our list of nodes
  for (size_t index : Reversed(IntegerRange(aArrayOfContents.Length()))) {
    OwningNonNull<nsIContent> content = aArrayOfContents[index];

    // Remove all non-editable nodes.  Leave them be.
    if (!EditorUtils::IsEditableContent(content, EditorType::HTML)) {
      aArrayOfContents.RemoveElementAt(index);
      continue;
    }

    // Scan for table elements.  If we find table elements other than table,
    // replace it with a list of any editable non-table content.  Ditto for
    // list elements.
    if (HTMLEditUtils::IsAnyTableElement(content) ||
        HTMLEditUtils::IsAnyListElement(content) ||
        HTMLEditUtils::IsListItem(content)) {
      aArrayOfContents.RemoveElementAt(index);
      HTMLEditUtils::CollectChildren(
          content, aArrayOfContents, index,
          {CollectChildrenOption::CollectListChildren,
           CollectChildrenOption::CollectTableChildren});
    }
  }
  return NS_OK;
}

}  // namespace mozilla
