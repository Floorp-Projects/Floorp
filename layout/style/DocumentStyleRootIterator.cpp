/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocumentStyleRootIterator.h"

#include "mozilla/dom/Element.h"
#include "nsContentUtils.h"

namespace mozilla {

DocumentStyleRootIterator::DocumentStyleRootIterator(nsINode* aStyleRoot)
  : mPosition(0)
{
  MOZ_COUNT_CTOR(DocumentStyleRootIterator);
  MOZ_ASSERT(aStyleRoot);
  if (aStyleRoot->IsElement()) {
    mStyleRoots.AppendElement(aStyleRoot->AsElement());
    return;
  }

  nsIDocument* doc = aStyleRoot->OwnerDoc();
  MOZ_ASSERT(doc == aStyleRoot);
  if (Element* root = doc->GetRootElement()) {
    mStyleRoots.AppendElement(root);
  }
  nsContentUtils::AppendDocumentLevelNativeAnonymousContentTo(
      doc, mStyleRoots);
}

Element*
DocumentStyleRootIterator::GetNextStyleRoot()
{
  for (;;) {
    if (mPosition >= mStyleRoots.Length()) {
      return nullptr;
    }

    nsIContent* next = mStyleRoots[mPosition];
    ++mPosition;

    if (next->IsElement()) {
      return next->AsElement();
    }
  }
}

} // namespace mozilla
