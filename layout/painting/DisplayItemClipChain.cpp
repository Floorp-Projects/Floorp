/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DisplayItemClipChain.h"

namespace mozilla {

/* static */ const DisplayItemClip*
DisplayItemClipChain::ClipForASR(const DisplayItemClipChain* aClipChain, const ActiveScrolledRoot* aASR)
{
  while (aClipChain && !ActiveScrolledRoot::IsAncestor(aClipChain->mASR, aASR)) {
    aClipChain = aClipChain->mParent;
  }
  return (aClipChain && aClipChain->mASR == aASR) ? &aClipChain->mClip : nullptr;
}

bool
DisplayItemClipChain::Equal(const DisplayItemClipChain* aClip1, const DisplayItemClipChain* aClip2)
{
  if (aClip1 == aClip2) {
    return true;
  }

  if (!aClip1 || !aClip2) {
    return false;
  }

  bool ret = aClip1->mASR == aClip2->mASR &&
         aClip1->mClip == aClip2->mClip &&
         Equal(aClip1->mParent, aClip2->mParent);
  // Sanity check: if two clip chains are equal they must hash to the same
  // thing too, or Bad Things (TM) will happen.
  MOZ_ASSERT(!ret || (Hash(aClip1) == Hash(aClip2)));
  return ret;
}

uint32_t
DisplayItemClipChain::Hash(const DisplayItemClipChain* aClip)
{
  if (!aClip) {
    return 0;
  }

  // We include the number of rounded rects in the hash but not their contents.
  // This is to keep the hash fast, because most clips will not have rounded
  // rects and including them will slow down the hash in the common case. Note
  // that the ::Equal check still checks the rounded rect contents, so in case
  // of hash collisions the clip chains can still be distinguished using that.
  uint32_t hash = HashGeneric(aClip->mASR, aClip->mClip.GetRoundedRectCount());
  if (aClip->mClip.HasClip()) {
    const nsRect& rect = aClip->mClip.GetClipRect();
    // empty rects are considered equal in DisplayItemClipChain::Equal, even
    // though they may have different x and y coordinates. So make sure they
    // hash to the same thing in those cases too.
    if (!rect.IsEmpty()) {
      hash = AddToHash(hash, rect.x, rect.y, rect.width, rect.height);
    }
  }

  return hash;
}

/* static */ nsCString
DisplayItemClipChain::ToString(const DisplayItemClipChain* aClipChain)
{
  nsAutoCString str;
  for (auto* sc = aClipChain; sc; sc = sc->mParent) {
    if (sc->mASR) {
      str.AppendPrintf("0x%p <%s> [0x%p]", sc, sc->mClip.ToString().get(), sc->mASR->mScrollableFrame);
    } else {
      str.AppendPrintf("0x%p <%s> [root asr]", sc, sc->mClip.ToString().get());
    }
    if (sc->mParent) {
      str.AppendLiteral(", ");
    }
  }
  return str;
}

bool
DisplayItemClipChain::HasRoundedCorners() const
{
  return mClip.GetRoundedRectCount() > 0 || (mParent && mParent->HasRoundedCorners());
}

} // namespace mozilla
