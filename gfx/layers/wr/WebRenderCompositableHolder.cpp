/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderCompositableHolder.h"

#include "CompositableHost.h"
//#include "mozilla/layers/CompositorBridgeParent.h"

namespace mozilla {

using namespace gfx;

namespace layers {

WebRenderCompositableHolder::WebRenderCompositableHolder()
{
  MOZ_COUNT_CTOR(WebRenderCompositableHolder);
}

WebRenderCompositableHolder::~WebRenderCompositableHolder()
{
  MOZ_COUNT_DTOR(WebRenderCompositableHolder);
  Destroy();
}

void
WebRenderCompositableHolder::Destroy()
{
  mCompositableHosts.Clear();
}

void
WebRenderCompositableHolder::AddExternalImageId(uint64_t aExternalImageId, CompositableHost* aHost)
{
  MOZ_ASSERT(!mCompositableHosts.Get(aExternalImageId));
  mCompositableHosts.Put(aExternalImageId, aHost);
}

void
WebRenderCompositableHolder::RemoveExternalImageId(uint64_t aExternalImageId)
{
  MOZ_ASSERT(mCompositableHosts.Get(aExternalImageId));
  mCompositableHosts.Remove(aExternalImageId);
}

void
WebRenderCompositableHolder::UpdateExternalImages()
{
  for (auto iter = mCompositableHosts.Iter(); !iter.Done(); iter.Next()) {
    RefPtr<CompositableHost>& host = iter.Data();
    // XXX Change to correct TextrueSource handling here.
    host->BindTextureSource();
  }
}

} // namespace layers
} // namespace mozilla
