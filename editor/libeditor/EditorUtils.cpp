/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EditorUtils.h"

#include "mozilla/ComputedStyle.h"
#include "mozilla/ContentIterator.h"
#include "mozilla/EditorDOMPoint.h"
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
#include "nsIContent.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsINode.h"
#include "nsStyleStruct.h"

class nsISupports;
class nsRange;

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
 * some helper classes for iterating the dom tree
 *****************************************************************************/

DOMIterator::DOMIterator(nsINode& aNode MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
    : mIter(&mPostOrderIter) {
  MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  DebugOnly<nsresult> rv = mIter->Init(&aNode);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

nsresult DOMIterator::Init(nsRange& aRange) { return mIter->Init(&aRange); }

nsresult DOMIterator::Init(const RawRangeBoundary& aStartRef,
                           const RawRangeBoundary& aEndRef) {
  return mIter->Init(aStartRef, aEndRef);
}

DOMIterator::DOMIterator(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM_IN_IMPL)
    : mIter(&mPostOrderIter) {
  MOZ_GUARD_OBJECT_NOTIFIER_INIT;
}

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

DOMSubtreeIterator::DOMSubtreeIterator(
    MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM_IN_IMPL)
    : DOMIterator(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM_TO_PARENT) {
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
