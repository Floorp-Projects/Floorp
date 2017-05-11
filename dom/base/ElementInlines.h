/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ElementInlines_h
#define mozilla_dom_ElementInlines_h

#include "mozilla/dom/Element.h"
#include "mozilla/ServoBindings.h"
#include "nsIContentInlines.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIPresShellInlines.h"

namespace mozilla {
namespace dom {

inline void
Element::RegisterActivityObserver()
{
  OwnerDoc()->RegisterActivityObserver(this);
}

inline void
Element::UnregisterActivityObserver()
{
  OwnerDoc()->UnregisterActivityObserver(this);
}

inline Element*
Element::GetFlattenedTreeParentElement() const
{
  nsINode* parentNode = GetFlattenedTreeParentNode();
  if MOZ_LIKELY(parentNode && parentNode->IsElement()) {
    return parentNode->AsElement();
  }

  return nullptr;
}

inline Element*
Element::GetFlattenedTreeParentElementForStyle() const
{
  nsINode* parentNode = GetFlattenedTreeParentNodeForStyle();
  if MOZ_LIKELY(parentNode && parentNode->IsElement()) {
    return parentNode->AsElement();
  }

  return nullptr;
}

inline void
Element::NoteDirtyDescendantsForServo()
{
  if (!HasServoData()) {
    // The dirty descendants bit only applies to styled elements.
    return;
  }

  Element* curr = this;
  while (curr && !curr->HasDirtyDescendantsForServo()) {
    curr->SetHasDirtyDescendantsForServo();
    curr = curr->GetFlattenedTreeParentElementForStyle();
  }

  if (nsIPresShell* shell = OwnerDoc()->GetShell()) {
    shell->EnsureStyleFlush();
  }

  MOZ_ASSERT(DirtyDescendantsBitIsPropagatedForServo());
}

#ifdef DEBUG
inline bool
Element::DirtyDescendantsBitIsPropagatedForServo()
{
  Element* curr = this;
  while (curr) {
    if (!curr->HasDirtyDescendantsForServo()) {
      return false;
    }
    nsINode* parentNode = curr->GetParentNode();
    curr = curr->GetFlattenedTreeParentElementForStyle();
    MOZ_ASSERT_IF(!curr,
                  parentNode == OwnerDoc() ||
                  parentNode == parentNode->OwnerDoc()->GetRootElement());
  }
  return true;
}
#endif

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ElementInlines_h
