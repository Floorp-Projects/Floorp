/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
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

  return aClip1->mASR == aClip2->mASR &&
         aClip1->mClip == aClip2->mClip &&
         Equal(aClip1->mParent, aClip2->mParent);
}

/* static */ nsCString
DisplayItemClipChain::ToString(const DisplayItemClipChain* aClipChain)
{
  nsAutoCString str;
  for (auto* sc = aClipChain; sc; sc = sc->mParent) {
    if (sc->mASR) {
      str.AppendPrintf("<%s> [0x%p]", sc->mClip.ToString().get(), sc->mASR->mScrollableFrame);

    } else {
      str.AppendPrintf("<%s> [root asr]", sc->mClip.ToString().get());
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
