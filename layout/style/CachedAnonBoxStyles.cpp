/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIMemoryReporter.h"
#include "mozilla/CachedAnonBoxStyles.h"
#include "mozilla/ServoStyleContext.h"

namespace mozilla {

void
CachedAnonBoxStyles::Insert(ServoStyleContext* aStyle)
{
  MOZ_ASSERT(aStyle);
  MOZ_ASSERT(aStyle->IsInheritingAnonBox());

  if (IsEmpty()) {
    RefPtr<ServoStyleContext> s = aStyle;
    mBits = reinterpret_cast<uintptr_t>(s.forget().take());
    MOZ_ASSERT(!IsEmpty() && !IsIndirect());
  } else if (IsIndirect()) {
    AsIndirect()->AppendElement(aStyle);
  } else {
    IndirectCache* cache = new IndirectCache();
    cache->AppendElement(dont_AddRef(AsDirect()));
    cache->AppendElement(aStyle);
    mBits = reinterpret_cast<uintptr_t>(cache) | 1;
    MOZ_ASSERT(IsIndirect());
  }
}

ServoStyleContext*
CachedAnonBoxStyles::Lookup(nsAtom* aAnonBox) const
{
  MOZ_ASSERT(nsCSSAnonBoxes::IsInheritingAnonBox(aAnonBox));
  if (IsIndirect()) {
    for (auto& style : *AsIndirect()) {
      if (style->GetPseudo() == aAnonBox) {
        return style;
      }
    }

    return nullptr;
  }

  ServoStyleContext* direct = AsDirect();
  return direct && direct->GetPseudo() == aAnonBox ? direct : nullptr;
}

void
CachedAnonBoxStyles::AddSizeOfIncludingThis(nsWindowSizes& aSizes, size_t* aCVsSize) const
{
  if (IsIndirect()) {
    for (auto& style : *AsIndirect()) {
      if (!aSizes.mState.HaveSeenPtr(style)) {
        style->AddSizeOfIncludingThis(aSizes, aCVsSize);
      }
    }

    return;
  }

  ServoStyleContext* direct = AsDirect();
  if (direct && !aSizes.mState.HaveSeenPtr(direct)) {
    direct->AddSizeOfIncludingThis(aSizes, aCVsSize);
  }
}

} // namespace mozilla
