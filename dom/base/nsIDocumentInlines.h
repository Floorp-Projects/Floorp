/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIDocumentInlines_h
#define nsIDocumentInlines_h

#include "nsContentUtils.h"
#include "nsIDocument.h"
#include "mozilla/dom/HTMLBodyElement.h"
#include "nsStyleSheetService.h"

inline mozilla::dom::HTMLBodyElement*
nsIDocument::GetBodyElement()
{
  return static_cast<mozilla::dom::HTMLBodyElement*>(GetHtmlChildElement(nsGkAtoms::body));
}

template<typename T>
size_t
nsIDocument::FindDocStyleSheetInsertionPoint(
    const nsTArray<T>& aDocSheets,
    const mozilla::StyleSheet& aSheet)
{
  nsStyleSheetService* sheetService = nsStyleSheetService::GetInstance();

  // lowest index first
  int32_t newDocIndex = IndexOfSheet(aSheet);

  size_t count = aDocSheets.Length();
  size_t index = 0;
  for (; index < count; index++) {
    auto* sheet = static_cast<mozilla::StyleSheet*>(aDocSheets[index]);
    MOZ_ASSERT(sheet);
    int32_t sheetDocIndex = IndexOfSheet(*sheet);
    if (sheetDocIndex > newDocIndex)
      break;

    // If the sheet is not owned by the document it can be an author
    // sheet registered at nsStyleSheetService or an additional author
    // sheet on the document, which means the new
    // doc sheet should end up before it.
    if (sheetDocIndex < 0) {
      if (sheetService) {
        auto& authorSheets =
          *sheetService->AuthorStyleSheets(GetStyleBackendType());
        if (authorSheets.IndexOf(sheet) != authorSheets.NoIndex) {
          break;
        }
      }
      if (sheet == GetFirstAdditionalAuthorSheet()) {
        break;
      }
    }
  }

  return size_t(index);
}

inline void
nsIDocument::SetServoRestyleRoot(nsINode* aRoot, uint32_t aDirtyBits)
{
  MOZ_ASSERT(aRoot);
  MOZ_ASSERT(aDirtyBits);
  MOZ_ASSERT((aDirtyBits & ~Element::kAllServoDescendantBits) == 0);
  MOZ_ASSERT((aDirtyBits & mServoRestyleRootDirtyBits) == mServoRestyleRootDirtyBits);

  // NOTE(emilio): The !aRoot->IsElement() check allows us to handle cases where
  // we change the restyle root during unbinding of a subtree where the root is
  // not unbound yet (and thus hasn't cleared the restyle root yet).
  //
  // In that case the tree can be in a somewhat inconsistent state (with the
  // document no longer being the subtree root of the current root, but the root
  // not having being unbound first).
  //
  // In that case, given there's no common ancestor, aRoot should be the
  // document, and we allow that, provided that the previous root will
  // eventually be unbound and the dirty bits will be cleared.
  //
  // If we want to enforce calling into this method with the tree in a
  // consistent state, we'd need to move all the state changes that happen on
  // content unbinding for parents, like fieldset validity stuff and ancestor
  // direction changes off script runners or, alternatively, nulling out the
  // document and parent node _after_ nulling out the children's, and then
  // remove that line.
  MOZ_ASSERT(!mServoRestyleRoot ||
             mServoRestyleRoot == aRoot ||
             !aRoot->IsElement() ||
             nsContentUtils::ContentIsFlattenedTreeDescendantOfForStyle(mServoRestyleRoot, aRoot));
  MOZ_ASSERT(aRoot == aRoot->OwnerDocAsNode() || aRoot->IsElement());
  mServoRestyleRoot = aRoot;
  mServoRestyleRootDirtyBits = aDirtyBits;
}

#endif // nsIDocumentInlines_h
