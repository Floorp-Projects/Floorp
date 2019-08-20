/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EditorUtils.h"

#include "mozilla/ContentIterator.h"
#include "mozilla/EditorDOMPoint.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/TextEditor.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/Text.h"
#include "nsContentUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsError.h"
#include "nsIContent.h"
#include "nsIDocShell.h"
#include "mozilla/dom/Document.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsINode.h"

class nsISupports;
class nsRange;

namespace mozilla {

using namespace dom;

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

void DOMIterator::AppendList(
    const BoolDomIterFunctor& functor,
    nsTArray<OwningNonNull<nsINode>>& arrayOfNodes) const {
  // Iterate through dom and build list
  for (; !mIter->IsDone(); mIter->Next()) {
    nsCOMPtr<nsINode> node = mIter->GetCurrentNode();

    if (functor(node)) {
      arrayOfNodes.AppendElement(*node);
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

}  // namespace mozilla
