/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/TextureSourceProvider.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/layers/PTextureParent.h"
#ifdef XP_DARWIN
#include "mozilla/layers/TextureSync.h"
#endif

namespace mozilla {
namespace layers {

TextureSourceProvider::~TextureSourceProvider()
{
  ReadUnlockTextures();
}

void
TextureSourceProvider::ReadUnlockTextures()
{
#ifdef XP_DARWIN
  nsClassHashtable<nsUint32HashKey, nsTArray<uint64_t>> texturesIdsToUnlockByPid;
  for (auto& texture : mUnlockAfterComposition) {
    auto bufferTexture = texture->AsBufferTextureHost();
    if (bufferTexture && bufferTexture->IsDirectMap()) {
      texture->ReadUnlock();
      auto actor = texture->GetIPDLActor();
      if (actor) {
        base::ProcessId pid = actor->OtherPid();
        nsTArray<uint64_t>* textureIds = texturesIdsToUnlockByPid.LookupOrAdd(pid);
        textureIds->AppendElement(TextureHost::GetTextureSerial(actor));
      }
    } else {
      texture->ReadUnlock();
    }
  }
  for (auto it = texturesIdsToUnlockByPid.ConstIter(); !it.Done(); it.Next()) {
    TextureSync::SetTexturesUnlocked(it.Key(), *it.UserData());
  }
#else
  for (auto& texture : mUnlockAfterComposition) {
    texture->ReadUnlock();
  }
#endif
  mUnlockAfterComposition.Clear();
}

void
TextureSourceProvider::UnlockAfterComposition(TextureHost* aTexture)
{
  mUnlockAfterComposition.AppendElement(aTexture);
}

bool
TextureSourceProvider::NotifyNotUsedAfterComposition(TextureHost* aTextureHost)
{
  mNotifyNotUsedAfterComposition.AppendElement(aTextureHost);

  // If Compositor holds many TextureHosts without compositing,
  // the TextureHosts should be flushed to reduce memory consumption.
  const int thresholdCount = 5;
  const double thresholdSec = 2.0f;
  if (mNotifyNotUsedAfterComposition.Length() > thresholdCount) {
    TimeStamp lastCompositionEndTime = GetLastCompositionEndTime();
    TimeDuration duration = lastCompositionEndTime ? TimeStamp::Now() - lastCompositionEndTime : TimeDuration();
    // Check if we could flush
    if (duration.ToSeconds() > thresholdSec) {
      FlushPendingNotifyNotUsed();
    }
  }
  return true;
}

void
TextureSourceProvider::FlushPendingNotifyNotUsed()
{
  for (auto& textureHost : mNotifyNotUsedAfterComposition) {
    textureHost->CallNotifyNotUsed();
  }
  mNotifyNotUsedAfterComposition.Clear();
}

void
TextureSourceProvider::Destroy()
{
  ReadUnlockTextures();
  FlushPendingNotifyNotUsed();
}

} // namespace layers
} // namespace mozilla
