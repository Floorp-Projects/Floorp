/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_DocumentInlines_h
#define mozilla_dom_DocumentInlines_h

#include "mozilla/dom/Document.h"

#include "mozilla/PresShell.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/dom/HTMLBodyElement.h"
#include "nsContentUtils.h"
#include "nsPresContext.h"
#include "nsStyleSheetService.h"

namespace mozilla::dom {

inline PresShell* Document::GetObservingPresShell() const {
  return mPresShell && mPresShell->IsObservingDocument() ? mPresShell : nullptr;
}

inline nsPresContext* Document::GetPresContext() const {
  PresShell* presShell = GetPresShell();
  return presShell ? presShell->GetPresContext() : nullptr;
}

inline HTMLBodyElement* Document::GetBodyElement() {
  return static_cast<HTMLBodyElement*>(GetHtmlChildElement(nsGkAtoms::body));
}

inline void Document::SetServoRestyleRoot(nsINode* aRoot, uint32_t aDirtyBits) {
  MOZ_ASSERT(aRoot);

  MOZ_ASSERT(!mServoRestyleRoot || mServoRestyleRoot == aRoot ||
             nsContentUtils::ContentIsFlattenedTreeDescendantOfForStyle(
                 mServoRestyleRoot, aRoot));
  MOZ_ASSERT(aRoot == aRoot->OwnerDocAsNode() || aRoot->IsElement());
  mServoRestyleRoot = aRoot;
  SetServoRestyleRootDirtyBits(aDirtyBits);
}

// Note: we break this out of SetServoRestyleRoot so that callers can add
// bits without doing a no-op assignment to the restyle root, which would
// involve cycle-collected refcount traffic.
inline void Document::SetServoRestyleRootDirtyBits(uint32_t aDirtyBits) {
  MOZ_ASSERT(aDirtyBits);
  MOZ_ASSERT((aDirtyBits & ~Element::kAllServoDescendantBits) == 0);
  MOZ_ASSERT((aDirtyBits & mServoRestyleRootDirtyBits) ==
             mServoRestyleRootDirtyBits);
  MOZ_ASSERT(mServoRestyleRoot);
  mServoRestyleRootDirtyBits = aDirtyBits;
}

inline ServoStyleSet& Document::EnsureStyleSet() const {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mStyleSet) {
    Document* doc = const_cast<Document*>(this);
    doc->mStyleSet = MakeUnique<ServoStyleSet>(*doc);
  }
  return *(mStyleSet.get());
}

}  // namespace mozilla::dom

#endif  // mozilla_dom_DocumentInlines_h
