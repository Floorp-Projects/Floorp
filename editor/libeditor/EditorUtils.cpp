/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EditorUtils.h"

#include "gfxFontUtils.h"
#include "WSRunObject.h"
#include "mozilla/ComputedStyle.h"
#include "mozilla/ContentIterator.h"
#include "mozilla/EditorDOMPoint.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/TextEditor.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/HTMLBRElement.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/Text.h"
#include "nsContentUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsComputedDOMStyle.h"
#include "nsError.h"
#include "nsFrameSelection.h"
#include "nsIContent.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsINode.h"
#include "nsRange.h"
#include "nsStyleStruct.h"
#include "nsTextFragment.h"

class nsISupports;

namespace mozilla {

using namespace dom;

template void DOMIterator::AppendAllNodesToArray(
    nsTArray<OwningNonNull<nsIContent>>& aArrayOfNodes) const;
template void DOMIterator::AppendAllNodesToArray(
    nsTArray<OwningNonNull<HTMLBRElement>>& aArrayOfNodes) const;
template void DOMIterator::AppendNodesToArray(
    BoolFunctor aFunctor, nsTArray<OwningNonNull<nsIContent>>& aArrayOfNodes,
    void* aClosure) const;
template void DOMIterator::AppendNodesToArray(
    BoolFunctor aFunctor, nsTArray<OwningNonNull<Element>>& aArrayOfNodes,
    void* aClosure) const;
template void DOMIterator::AppendNodesToArray(
    BoolFunctor aFunctor, nsTArray<OwningNonNull<Text>>& aArrayOfNodes,
    void* aClosure) const;

/******************************************************************************
 * mozilla::EditActionResult
 *****************************************************************************/

EditActionResult& EditActionResult::operator|=(
    const MoveNodeResult& aMoveNodeResult) {
  mHandled |= aMoveNodeResult.Handled();
  // When both result are same, keep the result.
  if (mRv == aMoveNodeResult.Rv()) {
    return *this;
  }
  // If one of the result is NS_ERROR_EDITOR_DESTROYED, use it since it's
  // the most important error code for editor.
  if (EditorDestroyed() || aMoveNodeResult.EditorDestroyed()) {
    mRv = NS_ERROR_EDITOR_DESTROYED;
    return *this;
  }
  // If aMoveNodeResult hasn't been set explicit nsresult value, keep current
  // result.
  if (aMoveNodeResult.Rv() == NS_ERROR_NOT_INITIALIZED) {
    return *this;
  }
  // If one of the results is error, use NS_ERROR_FAILURE.
  if (Failed() || aMoveNodeResult.Failed()) {
    mRv = NS_ERROR_FAILURE;
    return *this;
  }
  // Otherwise, use generic success code, NS_OK.
  mRv = NS_OK;
  return *this;
}

/******************************************************************************
 * mozilla::AutoRangeArray
 *****************************************************************************/

// static
bool AutoRangeArray::IsEditableRange(const dom::AbstractRange& aRange,
                                     const Element& aEditingHost) {
  // TODO: Perhaps, we should check whether the start/end boundaries are
  //       first/last point of non-editable element.
  //       See https://github.com/w3c/editing/issues/283#issuecomment-788654850
  EditorRawDOMPoint atStart(aRange.StartRef());
  const bool isStartEditable =
      atStart.IsInContentNode() &&
      EditorUtils::IsEditableContent(*atStart.ContainerAsContent(),
                                     EditorUtils::EditorType::HTML);
  if (!isStartEditable) {
    return false;
  }

  if (!aRange.Collapsed()) {
    EditorRawDOMPoint atEnd(aRange.EndRef());
    const bool isEndEditable =
        atEnd.IsInContentNode() &&
        EditorUtils::IsEditableContent(*atEnd.ContainerAsContent(),
                                       EditorUtils::EditorType::HTML);
    if (!isEndEditable) {
      return false;
    }

    // Now, both start and end points are editable, but if they are in
    // different editing host, we cannot edit the range.
    if (atStart.ContainerAsContent() != atEnd.ContainerAsContent() &&
        atStart.ContainerAsContent()->GetEditingHost() !=
            atEnd.ContainerAsContent()->GetEditingHost()) {
      return false;
    }
  }

  // HTMLEditor does not support modifying outside `<body>` element for now.
  nsINode* commonAncestor = aRange.GetClosestCommonInclusiveAncestor();
  return commonAncestor && commonAncestor->IsContent() &&
         commonAncestor->IsInclusiveDescendantOf(&aEditingHost);
}

void AutoRangeArray::EnsureOnlyEditableRanges(const Element& aEditingHost) {
  for (size_t i = mRanges.Length(); i > 0; i--) {
    const OwningNonNull<nsRange>& range = mRanges[i - 1];
    if (!AutoRangeArray::IsEditableRange(range, aEditingHost)) {
      mRanges.RemoveElementAt(i - 1);
    }
  }
  mAnchorFocusRange = mRanges.IsEmpty() ? nullptr : mRanges.LastElement().get();
}

Result<nsIEditor::EDirection, nsresult>
AutoRangeArray::ExtendAnchorFocusRangeFor(
    const EditorBase& aEditorBase, nsIEditor::EDirection aDirectionAndAmount) {
  MOZ_ASSERT(aEditorBase.IsEditActionDataAvailable());
  MOZ_ASSERT(mAnchorFocusRange);
  MOZ_ASSERT(mAnchorFocusRange->IsPositioned());
  MOZ_ASSERT(mAnchorFocusRange->StartRef().IsSet());
  MOZ_ASSERT(mAnchorFocusRange->EndRef().IsSet());

  if (!EditorUtils::IsFrameSelectionRequiredToExtendSelection(
          aDirectionAndAmount, *this)) {
    return aDirectionAndAmount;
  }

  const RefPtr<Selection>& selection = aEditorBase.SelectionRefPtr();
  if (NS_WARN_IF(!selection->RangeCount())) {
    return Err(NS_ERROR_FAILURE);
  }

  // At this point, the anchor-focus ranges must match for bidi information.
  // See `EditorBase::AutoCaretBidiLevelManager`.
  MOZ_ASSERT(selection->GetAnchorFocusRange()->StartRef() ==
             mAnchorFocusRange->StartRef());
  MOZ_ASSERT(selection->GetAnchorFocusRange()->EndRef() ==
             mAnchorFocusRange->EndRef());

  RefPtr<nsFrameSelection> frameSelection = selection->GetFrameSelection();
  if (NS_WARN_IF(!frameSelection)) {
    return Err(NS_ERROR_NOT_INITIALIZED);
  }

  RefPtr<Element> editingHost;
  if (aEditorBase.IsHTMLEditor()) {
    editingHost = aEditorBase.AsHTMLEditor()->GetActiveEditingHost();
    if (!editingHost) {
      return Err(NS_ERROR_FAILURE);
    }
  }

  Result<RefPtr<nsRange>, nsresult> result(NS_ERROR_UNEXPECTED);
  nsIEditor::EDirection directionAndAmountResult = aDirectionAndAmount;
  switch (aDirectionAndAmount) {
    case nsIEditor::eNextWord:
      result = frameSelection->CreateRangeExtendedToNextWordBoundary<nsRange>();
      if (NS_WARN_IF(aEditorBase.Destroyed())) {
        return Err(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(
          result.isOk(),
          "nsFrameSelection::CreateRangeExtendedToNextWordBoundary() failed");
      // DeleteSelectionWithTransaction() doesn't handle these actions
      // because it's inside batching, so don't confuse it:
      directionAndAmountResult = nsIEditor::eNone;
      break;
    case nsIEditor::ePreviousWord:
      result =
          frameSelection->CreateRangeExtendedToPreviousWordBoundary<nsRange>();
      if (NS_WARN_IF(aEditorBase.Destroyed())) {
        return Err(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(
          result.isOk(),
          "nsFrameSelection::CreateRangeExtendedToPreviousWordBoundary() "
          "failed");
      // DeleteSelectionWithTransaction() doesn't handle these actions
      // because it's inside batching, so don't confuse it:
      directionAndAmountResult = nsIEditor::eNone;
      break;
    case nsIEditor::eNext:
      result =
          frameSelection
              ->CreateRangeExtendedToNextGraphemeClusterBoundary<nsRange>();
      if (NS_WARN_IF(aEditorBase.Destroyed())) {
        return Err(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(result.isOk(),
                           "nsFrameSelection::"
                           "CreateRangeExtendedToNextGraphemeClusterBoundary() "
                           "failed");
      // Don't set directionAndAmount to eNone (see Bug 502259)
      break;
    case nsIEditor::ePrevious: {
      // Only extend the selection where the selection is after a UTF-16
      // surrogate pair or a variation selector.
      // For other cases we don't want to do that, in order
      // to make sure that pressing backspace will only delete the last
      // typed character.
      // XXX This is odd if the previous one is a sequence for a grapheme
      //     cluster.
      EditorDOMPoint atStartOfSelection(GetStartPointOfFirstRange());
      if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
        return Err(NS_ERROR_FAILURE);
      }

      // node might be anonymous DIV, so we find better text node
      EditorRawDOMPoint insertionPoint =
          aEditorBase.FindBetterInsertionPoint(atStartOfSelection);
      if (!insertionPoint.IsSet()) {
        NS_WARNING(
            "EditorBase::FindBetterInsertionPoint() failed, but ignored");
        return aDirectionAndAmount;
      }

      if (!insertionPoint.IsInTextNode()) {
        return aDirectionAndAmount;
      }

      const nsTextFragment* data =
          &insertionPoint.GetContainerAsText()->TextFragment();
      uint32_t offset = insertionPoint.Offset();
      if (!(offset > 1 &&
            data->IsLowSurrogateFollowingHighSurrogateAt(offset - 1)) &&
          !(offset > 0 &&
            gfxFontUtils::IsVarSelector(data->CharAt(offset - 1)))) {
        return aDirectionAndAmount;
      }
      // Different from the `eNext` case, we look for character boundary.
      // I'm not sure whether this inconsistency between "Delete" and
      // "Backspace" is intentional or not.
      result = frameSelection
                   ->CreateRangeExtendedToPreviousCharacterBoundary<nsRange>();
      if (NS_WARN_IF(aEditorBase.Destroyed())) {
        return Err(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(
          result.isOk(),
          "nsFrameSelection::"
          "CreateRangeExtendedToPreviousGraphemeClusterBoundary() failed");
      break;
    }
    case nsIEditor::eToBeginningOfLine:
      result =
          frameSelection->CreateRangeExtendedToPreviousHardLineBreak<nsRange>();
      if (NS_WARN_IF(aEditorBase.Destroyed())) {
        return Err(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(
          result.isOk(),
          "nsFrameSelection::CreateRangeExtendedToPreviousHardLineBreak() "
          "failed");
      directionAndAmountResult = nsIEditor::eNone;
      break;
    case nsIEditor::eToEndOfLine:
      result =
          frameSelection->CreateRangeExtendedToNextHardLineBreak<nsRange>();
      if (NS_WARN_IF(aEditorBase.Destroyed())) {
        return Err(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(
          result.isOk(),
          "nsFrameSelection::CreateRangeExtendedToNextHardLineBreak() failed");
      directionAndAmountResult = nsIEditor::eNext;
      break;
    default:
      return aDirectionAndAmount;
  }

  if (result.isErr()) {
    return Err(result.inspectErr());
  }
  RefPtr<nsRange> extendedRange(result.unwrap().forget());
  if (!extendedRange || NS_WARN_IF(!extendedRange->IsPositioned())) {
    NS_WARNING("Failed to extend the range, but ignored");
    return directionAndAmountResult;
  }

  // If the new range isn't editable, keep using the original range.
  if (aEditorBase.IsHTMLEditor() &&
      !AutoRangeArray::IsEditableRange(*extendedRange, *editingHost)) {
    return aDirectionAndAmount;
  }

  if (NS_WARN_IF(!frameSelection->IsValidSelectionPoint(
          extendedRange->GetStartContainer())) ||
      NS_WARN_IF(!frameSelection->IsValidSelectionPoint(
          extendedRange->GetEndContainer()))) {
    NS_WARNING("A range was extended to outer of selection limiter");
    return Err(NS_ERROR_FAILURE);
  }

  // Swap focus/anchor range with the extended range.
  DebugOnly<bool> found = false;
  for (OwningNonNull<nsRange>& range : mRanges) {
    if (range == mAnchorFocusRange) {
      range = *extendedRange;
      found = true;
      break;
    }
  }
  MOZ_ASSERT(found);
  mAnchorFocusRange.swap(extendedRange);
  return directionAndAmountResult;
}

Result<bool, nsresult>
AutoRangeArray::ShrinkRangesIfStartFromOrEndAfterAtomicContent(
    const HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
    IfSelectingOnlyOneAtomicContent aIfSelectingOnlyOneAtomicContent,
    const Element* aEditingHost) {
  if (IsCollapsed()) {
    return false;
  }

  switch (aDirectionAndAmount) {
    case nsIEditor::eNext:
    case nsIEditor::eNextWord:
    case nsIEditor::ePrevious:
    case nsIEditor::ePreviousWord:
      break;
    default:
      return false;
  }

  bool changed = false;
  for (auto& range : mRanges) {
    MOZ_ASSERT(!range->IsInSelection(),
               "Changing range in selection may cause running script");
    Result<bool, nsresult> result =
        WSRunScanner::ShrinkRangeIfStartsFromOrEndsAfterAtomicContent(
            aHTMLEditor, range, aEditingHost);
    if (result.isErr()) {
      NS_WARNING(
          "WSRunScanner::ShrinkRangeIfStartsFromOrEndsAfterAtomicContent() "
          "failed");
      return Err(result.inspectErr());
    }
    changed |= result.inspect();
  }

  if (mRanges.Length() == 1 && aIfSelectingOnlyOneAtomicContent ==
                                   IfSelectingOnlyOneAtomicContent::Collapse) {
    MOZ_ASSERT(mRanges[0].get() == mAnchorFocusRange.get());
    if (mAnchorFocusRange->GetStartContainer() ==
            mAnchorFocusRange->GetEndContainer() &&
        mAnchorFocusRange->GetChildAtStartOffset() &&
        mAnchorFocusRange->StartRef().GetNextSiblingOfChildAtOffset() ==
            mAnchorFocusRange->GetChildAtEndOffset()) {
      mAnchorFocusRange->Collapse(aDirectionAndAmount == nsIEditor::eNext ||
                                  aDirectionAndAmount == nsIEditor::eNextWord);
      changed = true;
    }
  }

  return changed;
}

/******************************************************************************
 * some helper classes for iterating the dom tree
 *****************************************************************************/

DOMIterator::DOMIterator(nsINode& aNode) : mIter(&mPostOrderIter) {
  DebugOnly<nsresult> rv = mIter->Init(&aNode);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

nsresult DOMIterator::Init(nsRange& aRange) { return mIter->Init(&aRange); }

nsresult DOMIterator::Init(const RawRangeBoundary& aStartRef,
                           const RawRangeBoundary& aEndRef) {
  return mIter->Init(aStartRef, aEndRef);
}

DOMIterator::DOMIterator() : mIter(&mPostOrderIter) {}

template <class NodeClass>
void DOMIterator::AppendAllNodesToArray(
    nsTArray<OwningNonNull<NodeClass>>& aArrayOfNodes) const {
  for (; !mIter->IsDone(); mIter->Next()) {
    if (NodeClass* node = NodeClass::FromNode(mIter->GetCurrentNode())) {
      aArrayOfNodes.AppendElement(*node);
    }
  }
}

template <class NodeClass>
void DOMIterator::AppendNodesToArray(
    BoolFunctor aFunctor, nsTArray<OwningNonNull<NodeClass>>& aArrayOfNodes,
    void* aClosure /* = nullptr */) const {
  for (; !mIter->IsDone(); mIter->Next()) {
    NodeClass* node = NodeClass::FromNode(mIter->GetCurrentNode());
    if (node && aFunctor(*node, aClosure)) {
      aArrayOfNodes.AppendElement(*node);
    }
  }
}

DOMSubtreeIterator::DOMSubtreeIterator() : DOMIterator() {
  mIter = &mSubtreeIter;
}

nsresult DOMSubtreeIterator::Init(nsRange& aRange) {
  return mIter->Init(&aRange);
}

/******************************************************************************
 * some general purpose editor utils
 *****************************************************************************/

bool EditorUtils::IsDescendantOf(const nsINode& aNode, const nsINode& aParent,
                                 EditorRawDOMPoint* aOutPoint /* = nullptr */) {
  if (aOutPoint) {
    aOutPoint->Clear();
  }

  if (&aNode == &aParent) {
    return false;
  }

  for (const nsINode* node = &aNode; node; node = node->GetParentNode()) {
    if (node->GetParentNode() == &aParent) {
      if (aOutPoint) {
        MOZ_ASSERT(node->IsContent());
        aOutPoint->Set(node->AsContent());
      }
      return true;
    }
  }

  return false;
}

bool EditorUtils::IsDescendantOf(const nsINode& aNode, const nsINode& aParent,
                                 EditorDOMPoint* aOutPoint) {
  MOZ_ASSERT(aOutPoint);
  aOutPoint->Clear();
  if (&aNode == &aParent) {
    return false;
  }

  for (const nsINode* node = &aNode; node; node = node->GetParentNode()) {
    if (node->GetParentNode() == &aParent) {
      MOZ_ASSERT(node->IsContent());
      aOutPoint->Set(node->AsContent());
      return true;
    }
  }

  return false;
}

// static
void EditorUtils::MaskString(nsString& aString, Text* aText,
                             uint32_t aStartOffsetInString,
                             uint32_t aStartOffsetInText) {
  MOZ_ASSERT(aText->HasFlag(NS_MAYBE_MASKED));
  MOZ_ASSERT(aStartOffsetInString == 0 || aStartOffsetInText == 0);

  uint32_t unmaskStart = UINT32_MAX, unmaskLength = 0;
  TextEditor* textEditor =
      nsContentUtils::GetTextEditorFromAnonymousNodeWithoutCreation(aText);
  if (textEditor && textEditor->UnmaskedLength() > 0) {
    unmaskStart = textEditor->UnmaskedStart();
    unmaskLength = textEditor->UnmaskedLength();
    // If text is copied from after unmasked range, we can treat this case
    // as mask all.
    if (aStartOffsetInText >= unmaskStart + unmaskLength) {
      unmaskLength = 0;
      unmaskStart = UINT32_MAX;
    } else {
      // If text is copied from middle of unmasked range, reduce the length
      // and adjust start offset.
      if (aStartOffsetInText > unmaskStart) {
        unmaskLength = unmaskStart + unmaskLength - aStartOffsetInText;
        unmaskStart = 0;
      }
      // If text is copied from before start of unmasked range, just adjust
      // the start offset.
      else {
        unmaskStart -= aStartOffsetInText;
      }
      // Make the range is in the string.
      unmaskStart += aStartOffsetInString;
    }
  }

  const char16_t kPasswordMask = TextEditor::PasswordMask();
  for (uint32_t i = aStartOffsetInString; i < aString.Length(); ++i) {
    bool isSurrogatePair = NS_IS_HIGH_SURROGATE(aString.CharAt(i)) &&
                           i < aString.Length() - 1 &&
                           NS_IS_LOW_SURROGATE(aString.CharAt(i + 1));
    if (i < unmaskStart || i >= unmaskStart + unmaskLength) {
      if (isSurrogatePair) {
        aString.SetCharAt(kPasswordMask, i);
        aString.SetCharAt(kPasswordMask, i + 1);
      } else {
        aString.SetCharAt(kPasswordMask, i);
      }
    }

    // Skip the following low surrogate.
    if (isSurrogatePair) {
      ++i;
    }
  }
}

// static
bool EditorUtils::IsContentPreformatted(nsIContent& aContent) {
  // Look at the node (and its parent if it's not an element), and grab its
  // ComputedStyle.
  Element* element = aContent.GetAsElementOrParentElement();
  if (!element) {
    return false;
  }

  RefPtr<ComputedStyle> elementStyle =
      nsComputedDOMStyle::GetComputedStyleNoFlush(element, nullptr);
  if (!elementStyle) {
    // Consider nodes without a ComputedStyle to be NOT preformatted:
    // For instance, this is true of JS tags inside the body (which show
    // up as #text nodes but have no ComputedStyle).
    return false;
  }

  return elementStyle->StyleText()->WhiteSpaceIsSignificant();
}

bool EditorUtils::IsPointInSelection(const Selection& aSelection,
                                     const nsINode& aParentNode,
                                     int32_t aOffset) {
  if (aSelection.IsCollapsed()) {
    return false;
  }

  uint32_t rangeCount = aSelection.RangeCount();
  for (uint32_t i = 0; i < rangeCount; i++) {
    RefPtr<const nsRange> range = aSelection.GetRangeAt(i);
    if (!range) {
      // Don't bail yet, iterate through them all
      continue;
    }

    IgnoredErrorResult ignoredError;
    bool nodeIsInSelection =
        range->IsPointInRange(aParentNode, aOffset, ignoredError) &&
        !ignoredError.Failed();
    NS_WARNING_ASSERTION(!ignoredError.Failed(),
                         "nsRange::IsPointInRange() failed");

    // Done when we find a range that we are in
    if (nodeIsInSelection) {
      return true;
    }
  }

  return false;
}

}  // namespace mozilla
