/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProxyAccessible.h"
#include "DocAccessibleParent.h"
#include "mozilla/unused.h"
#include "mozilla/a11y/Platform.h"

namespace mozilla {
namespace a11y {

void
ProxyAccessible::Shutdown()
{
  MOZ_ASSERT(!mOuterDoc);

  uint32_t childCount = mChildren.Length();
  for (uint32_t idx = 0; idx < childCount; idx++)
    mChildren[idx]->Shutdown();

  mChildren.Clear();
  ProxyDestroyed(this);
  mDoc->RemoveAccessible(this);
}

void
ProxyAccessible::SetChildDoc(DocAccessibleParent* aParent)
{
  if (aParent) {
    MOZ_ASSERT(mChildren.IsEmpty());
    mChildren.AppendElement(aParent);
    mOuterDoc = true;
  } else {
    MOZ_ASSERT(mChildren.Length() == 1);
    mChildren.Clear();
    mOuterDoc = false;
  }
}

uint64_t
ProxyAccessible::State() const
{
  uint64_t state = 0;
  unused << mDoc->SendState(mID, &state);
  return state;
}

void
ProxyAccessible::Name(nsString& aName) const
{
  unused << mDoc->SendName(mID, &aName);
}
}
}
