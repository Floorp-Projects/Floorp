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
  mozAutoDocUpdate updateBatch(document, UPDATE_CONTENT_MODEL, true);

  // Use Clone for creating the new node so that the new node is of same class
  // as this node!
  nsGenericDOMDataNode* clone = CloneDataNode(mNodeInfo, false);
  MOZ_ASSERT(clone && clone->IsText());
  RefPtr<Text> newContent = static_cast<Text*>(clone);

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

} // namespace dom
} // namespace mozilla
