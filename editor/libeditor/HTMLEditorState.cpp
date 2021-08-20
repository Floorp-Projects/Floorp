/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLEditor.h"

#include <algorithm>
#include <utility>

#include "HTMLEditUtils.h"
#include "WSRunObject.h"

#include "mozilla/Assertions.h"
#include "mozilla/CSSEditUtils.h"
#include "mozilla/EditAction.h"
#include "mozilla/EditorUtils.h"
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

  AutoTArray<OwningNonNull<nsIContent>, 64> arrayOfContents;
  nsresult rv = aHTMLEditor.CollectEditTargetNodesInExtendedSelectionRanges(
      arrayOfContents, EditSubAction::eCreateOrChangeList,
      HTMLEditor::CollectNonEditableNodes::No);
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "HTMLEditor::CollectEditTargetNodesInExtendedSelectionRanges("
        "eCreateOrChangeList, CollectNonEditableNodes::No) failed");
    aRv = EditorBase::ToGenericNSResult(rv);
    return;
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

  AutoTArray<OwningNonNull<nsIContent>, 64> arrayOfContents;
  nsresult rv = aHTMLEditor.CollectEditTargetNodesInExtendedSelectionRanges(
      arrayOfContents, EditSubAction::eCreateOrChangeList,
      HTMLEditor::CollectNonEditableNodes::No);
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "HTMLEditor::CollectEditTargetNodesInExtendedSelectionRanges("
        "eCreateOrChangeList, CollectNonEditableNodes::No) failed");
    aRv = EditorBase::ToGenericNSResult(rv);
    return;
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
    editTargetContent = atStartOfSelection.GetContainerAsContent();
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
        aHTMLEditor.GetActiveEditingHost());
    if (NS_WARN_IF(!editTargetContent)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
  }
  // Otherwise, use first selected node.
  // XXX Only for retreiving it, the following block treats all selected
  //     ranges.  `HTMLEditor` should have
  //     `GetFirstSelectionRangeExtendedToHardLineStartAndEnd()`.
  else {
    AutoTArray<RefPtr<nsRange>, 4> arrayOfRanges;
    aHTMLEditor.GetSelectionRangesExtendedToHardLineStartAndEnd(
        arrayOfRanges, EditSubAction::eSetOrClearAlignment);

    AutoTArray<OwningNonNull<nsIContent>, 64> arrayOfContents;
    nsresult rv = aHTMLEditor.CollectEditTargetNodes(
        arrayOfRanges, arrayOfContents, EditSubAction::eSetOrClearAlignment,
        HTMLEditor::CollectNonEditableNodes::Yes);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "HTMLEditor::CollectEditTargetNodes(eSetOrClearAlignment, "
          "CollectNonEditableNodes::Yes) failed");
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
    if (arrayOfContents.IsEmpty()) {
      NS_WARNING(
          "HTMLEditor::CollectEditTargetNodes(eSetOrClearAlignment, "
          "CollectNonEditableNodes::Yes) returned no contents");
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
    editTargetContent = arrayOfContents[0];
  }

  const RefPtr<dom::Element> maybeNonEditableBlockElement =
      HTMLEditUtils::GetInclusiveAncestorElement(
          *editTargetContent, HTMLEditUtils::ClosestBlockElement);
  if (NS_WARN_IF(!maybeNonEditableBlockElement)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  if (aHTMLEditor.IsCSSEnabled() &&
      CSSEditUtils::IsCSSEditableProperty(maybeNonEditableBlockElement, nullptr,
                                          nsGkAtoms::align)) {
    // We are in CSS mode and we know how to align this element with CSS
    nsAutoString value;
    // Let's get the value(s) of text-align or margin-left/margin-right
    DebugOnly<nsresult> rvIgnored =
        CSSEditUtils::GetComputedCSSEquivalentToHTMLInlineStyleSet(
            *maybeNonEditableBlockElement, nullptr, nsGkAtoms::align, value);
    if (NS_WARN_IF(aHTMLEditor.Destroyed())) {
      aRv.Throw(NS_ERROR_EDITOR_DESTROYED);
      return;
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "CSSEditUtils::GetComputedCSSEquivalentToHTMLInlineStyleSet(nsGkAtoms::"
        "align, "
        "eComputed) failed, but ignored");
    if (value.EqualsLiteral("center") || value.EqualsLiteral("-moz-center") ||
        value.EqualsLiteral("auto auto")) {
      mFirstAlign = nsIHTMLEditor::eCenter;
      return;
    }
    if (value.EqualsLiteral("right") || value.EqualsLiteral("-moz-right") ||
        value.EqualsLiteral("auto 0px")) {
      mFirstAlign = nsIHTMLEditor::eRight;
      return;
    }
    if (value.EqualsLiteral("justify")) {
      mFirstAlign = nsIHTMLEditor::eJustify;
      return;
    }
    // XXX In RTL document, is this expected?
    mFirstAlign = nsIHTMLEditor::eLeft;
    return;
  }

  for (nsIContent* containerContent :
       editTargetContent->InclusiveAncestorsOfType<nsIContent>()) {
    // If the node is a parent `<table>` element of edit target, let's break
    // here to materialize the 'inline-block' behaviour of html tables
    // regarding to text alignment.
    if (containerContent != editTargetContent &&
        containerContent->IsHTMLElement(nsGkAtoms::table)) {
      return;
    }

    if (CSSEditUtils::IsCSSEditableProperty(containerContent, nullptr,
                                            nsGkAtoms::align)) {
      nsAutoString value;
      DebugOnly<nsresult> rvIgnored = CSSEditUtils::GetSpecifiedProperty(
          *containerContent, *nsGkAtoms::textAlign, value);
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

    if (!HTMLEditUtils::SupportsAlignAttr(*containerContent)) {
      continue;
    }

    nsAutoString alignAttributeValue;
    containerContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::align,
                                           alignAttributeValue);
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

ParagraphStateAtSelection::ParagraphStateAtSelection(HTMLEditor& aHTMLEditor,
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

  AutoTArray<OwningNonNull<nsIContent>, 64> arrayOfContents;
  nsresult rv =
      CollectEditableFormatNodesInSelection(aHTMLEditor, arrayOfContents);
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
  for (int32_t i = arrayOfContents.Length() - 1; i >= 0; i--) {
    auto& content = arrayOfContents[i];
    nsAutoString format;
    if (HTMLEditUtils::IsBlockElement(content) &&
        !HTMLEditUtils::IsFormatNode(content)) {
      // XXX This RemoveObject() call has already been commented out and
      //     the above comment explained we're trying to replace non-format
      //     block nodes in the array.  According to the following blocks and
      //     `AppendDescendantFormatNodesAndFirstInlineNode()`, replacing
      //     non-format block with descendants format blocks makes sense.
      // arrayOfContents.RemoveObject(node);
      ParagraphStateAtSelection::AppendDescendantFormatNodesAndFirstInlineNode(
          arrayOfContents, *content->AsElement());
    }
  }

  // We might have an empty node list.  if so, find selection parent
  // and put that on the list
  if (arrayOfContents.IsEmpty()) {
    EditorRawDOMPoint atCaret(
        EditorBase::GetStartPoint(aHTMLEditor.SelectionRef()));
    if (NS_WARN_IF(!atCaret.IsSet())) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
    nsIContent* content = atCaret.GetContainerAsContent();
    if (NS_WARN_IF(!content)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
    arrayOfContents.AppendElement(*content);
  }

  dom::Element* bodyOrDocumentElement = aHTMLEditor.GetRoot();
  if (NS_WARN_IF(!bodyOrDocumentElement)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  for (auto& content : Reversed(arrayOfContents)) {
    nsAtom* paragraphStateOfNode = nsGkAtoms::_empty;
    if (HTMLEditUtils::IsFormatNode(content)) {
      MOZ_ASSERT(content->NodeInfo()->NameAtom());
      paragraphStateOfNode = content->NodeInfo()->NameAtom();
    }
    // Ignore non-format block node since its children have been appended
    // the list above so that we'll handle this descendants later.
    else if (HTMLEditUtils::IsBlockElement(content)) {
      continue;
    }
    // If we meet an inline node, let's get its parent format.
    else {
      for (nsINode* parentNode = content->GetParentNode(); parentNode;
           parentNode = parentNode->GetParentNode()) {
        // If we reach `HTMLDocument.body` or `Document.documentElement`,
        // there is no format.
        if (parentNode == bodyOrDocumentElement) {
          break;
        }
        if (HTMLEditUtils::IsFormatNode(parentNode)) {
          MOZ_ASSERT(parentNode->NodeInfo()->NameAtom());
          paragraphStateOfNode = parentNode->NodeInfo()->NameAtom();
          break;
        }
      }
    }

    // if this is the first node, we've found, remember it as the format
    if (!mFirstParagraphState) {
      mFirstParagraphState = paragraphStateOfNode;
      continue;
    }
    // else make sure it matches previously found format
    if (mFirstParagraphState != paragraphStateOfNode) {
      mIsMixed = true;
      break;
    }
  }
}

// static
void ParagraphStateAtSelection::AppendDescendantFormatNodesAndFirstInlineNode(
    nsTArray<OwningNonNull<nsIContent>>& aArrayOfContents,
    dom::Element& aNonFormatBlockElement) {
  MOZ_ASSERT(HTMLEditUtils::IsBlockElement(aNonFormatBlockElement));
  MOZ_ASSERT(!HTMLEditUtils::IsFormatNode(&aNonFormatBlockElement));

  // We only need to place any one inline inside this node onto
  // the list.  They are all the same for purposes of determining
  // paragraph style.  We use foundInline to track this as we are
  // going through the children in the loop below.
  bool foundInline = false;
  for (nsIContent* childContent = aNonFormatBlockElement.GetFirstChild();
       childContent; childContent = childContent->GetNextSibling()) {
    bool isBlock = HTMLEditUtils::IsBlockElement(*childContent);
    bool isFormat = HTMLEditUtils::IsFormatNode(childContent);
    // If the child is a non-format block element, let's check its children
    // recursively.
    if (isBlock && !isFormat) {
      ParagraphStateAtSelection::AppendDescendantFormatNodesAndFirstInlineNode(
          aArrayOfContents, *childContent->AsElement());
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
    HTMLEditor& aHTMLEditor,
    nsTArray<OwningNonNull<nsIContent>>& aArrayOfContents) {
  nsresult rv = aHTMLEditor.CollectEditTargetNodesInExtendedSelectionRanges(
      aArrayOfContents, EditSubAction::eCreateOrRemoveBlock,
      HTMLEditor::CollectNonEditableNodes::Yes);
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "HTMLEditor::CollectEditTargetNodesInExtendedSelectionRanges("
        "eCreateOrRemoveBlock, CollectNonEditableNodes::Yes) failed");
    return rv;
  }

  // Pre-process our list of nodes
  for (int32_t i = aArrayOfContents.Length() - 1; i >= 0; i--) {
    OwningNonNull<nsIContent> content = aArrayOfContents[i];

    // Remove all non-editable nodes.  Leave them be.
    if (!EditorUtils::IsEditableContent(content, EditorType::HTML)) {
      aArrayOfContents.RemoveElementAt(i);
      continue;
    }

    // Scan for table elements.  If we find table elements other than table,
    // replace it with a list of any editable non-table content.  Ditto for
    // list elements.
    if (HTMLEditUtils::IsAnyTableElement(content) ||
        HTMLEditUtils::IsAnyListElement(content) ||
        HTMLEditUtils::IsListItem(content)) {
      aArrayOfContents.RemoveElementAt(i);
      aHTMLEditor.CollectChildren(content, aArrayOfContents, i,
                                  HTMLEditor::CollectListChildren::Yes,
                                  HTMLEditor::CollectTableChildren::Yes,
                                  HTMLEditor::CollectNonEditableNodes::Yes);
    }
  }
  return NS_OK;
}

}  // namespace mozilla
