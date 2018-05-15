/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Text.h"
#include "nsTextNode.h"
#include "mozAutoDocUpdate.h"

namespace mozilla {
namespace dom {

already_AddRefed<Text>
Text::SplitText(uint32_t aOffset, ErrorResult& aRv)
{
  nsAutoString cutText;
  uint32_t length = TextLength();

  if (aOffset > length) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  uint32_t cutStartOffset = aOffset;
  uint32_t cutLength = length - aOffset;
  SubstringData(cutStartOffset, cutLength, cutText, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsIDocument* document = GetComposedDoc();
  mozAutoDocUpdate updateBatch(document, true);

  // Use Clone for creating the new node so that the new node is of same class
  // as this node!
  RefPtr<CharacterData> clone = CloneDataNode(mNodeInfo, false);
  MOZ_ASSERT(clone && clone->IsText());
  RefPtr<Text> newContent = static_cast<Text*>(clone.get());

  // nsRange expects the CharacterDataChanged notification is followed
  // by an insertion of |newContent|. If you change this code,
  // make sure you make the appropriate changes in nsRange.
  newContent->SetText(cutText, true); // XXX should be false?

  CharacterDataChangeInfo::Details details = {
    CharacterDataChangeInfo::Details::eSplit, newContent
  };
  nsresult rv = SetTextInternal(cutStartOffset, cutLength, nullptr, 0, true,
                                &details);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  nsCOMPtr<nsINode> parent = GetParentNode();
  if (parent) {
    nsCOMPtr<nsIContent> beforeNode = GetNextSibling();
    parent->InsertChildBefore(newContent, beforeNode, true);
  }

  return newContent.forget();
}

static Text*
FirstLogicallyAdjacentTextNode(Text* aNode)
{
  do {
    nsIContent* sibling = aNode->GetPreviousSibling();
    if (!sibling || !sibling->IsText()) {
      return aNode;
    }
    aNode = static_cast<Text*>(sibling);
  } while (1);  // Must run out of previous siblings eventually!
}

static Text*
LastLogicallyAdjacentTextNode(Text* aNode)
{
  do {
    nsIContent* sibling = aNode->GetNextSibling();
    if (!sibling || !sibling->IsText()) {
      return aNode;
    }

    aNode = static_cast<Text*>(sibling);
  } while (1); // Must run out of next siblings eventually!
}

void
Text::GetWholeText(nsAString& aWholeText,
                   ErrorResult& aRv)
{
  nsIContent* parent = GetParent();

  // Handle parent-less nodes
  if (!parent) {
    GetData(aWholeText);
    return;
  }

  int32_t index = parent->ComputeIndexOf(this);
  NS_WARNING_ASSERTION(index >= 0,
                       "Trying to use .wholeText with an anonymous"
                       "text node child of a binding parent?");
  if (NS_WARN_IF(index < 0)) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }

  Text* first = FirstLogicallyAdjacentTextNode(this);
  Text* last = LastLogicallyAdjacentTextNode(this);

  aWholeText.Truncate();

  nsAutoString tmp;

  while (true) {
    first->GetData(tmp);
    aWholeText.Append(tmp);

    if (first == last) {
      break;
    }

    nsIContent* next = first->GetNextSibling();
    MOZ_ASSERT(next && next->IsText(),
               "How did we run out of text before hitting `last`?");
    first = static_cast<Text*>(next);
  }
}

/* static */ already_AddRefed<Text>
Text::Constructor(const GlobalObject& aGlobal,
                  const nsAString& aData, ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window || !window->GetDoc()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return window->GetDoc()->CreateTextNode(aData);
}

bool
Text::HasTextForTranslation()
{
  if (mText.Is2b()) {
    // The fragment contains non-8bit characters which means there
    // was at least one "interesting" character to trigger non-8bit.
    return true;
  }

  if (HasFlag(NS_CACHED_TEXT_IS_ONLY_WHITESPACE) &&
      HasFlag(NS_TEXT_IS_ONLY_WHITESPACE)) {
    return false;
  }

  const char* cp = mText.Get1b();
  const char* end = cp + mText.GetLength();

  unsigned char ch;
  for (; cp < end; cp++) {
    ch = *cp;

    // These are the characters that are letters
    // in the first 256 UTF-8 codepoints.
    if ((ch >= 'a' && ch <= 'z') ||
       (ch >= 'A' && ch <= 'Z') ||
       (ch >= 192 && ch <= 214) ||
       (ch >= 216 && ch <= 246) ||
       (ch >= 248)) {
      return true;
    }
  }

  return false;
}

} // namespace dom
} // namespace mozilla
