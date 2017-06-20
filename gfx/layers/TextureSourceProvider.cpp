/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/TextureSourceProvider.h"
#include "mozilla/layers/TextureHost.h"

namespace mozilla {
namespace layers {

TextureSourceProvider::~TextureSourceProvider()
{
  ReadUnlockTextures();
}

void
TextureSourceProvider::ReadUnlockTextures()
{
  for (auto& texture : mUnlockAfterComposition) {
    texture->ReadUnlock();
  }
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
