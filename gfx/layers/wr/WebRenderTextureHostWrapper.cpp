/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderTextureHostWrapper.h"

#include "mozilla/layers/AsyncImagePipelineManager.h"
#include "mozilla/layers/WebRenderTextureHost.h"
#include "mozilla/webrender/RenderTextureHostWrapper.h"
#include "mozilla/webrender/RenderThread.h"

namespace mozilla {
namespace layers {

WebRenderTextureHostWrapper::WebRenderTextureHostWrapper(
    AsyncImagePipelineManager* aManager)
    : mExternalImageId(aManager->GetNextExternalImageId()) {
  MOZ_ASSERT(aManager);
  MOZ_COUNT_CTOR(WebRenderTextureHostWrapper);

  RefPtr<wr::RenderTextureHost> texture = new wr::RenderTextureHostWrapper();
  wr::RenderThread::Get()->RegisterExternalImage(wr::AsUint64(mExternalImageId),
                                                 texture.forget());
}

WebRenderTextureHostWrapper::~WebRenderTextureHostWrapper() {
  MOZ_COUNT_DTOR(WebRenderTextureHostWrapper);
  wr::RenderThread::Get()->UnregisterExternalImage(
      wr::AsUint64(mExternalImageId));
}

void WebRenderTextureHostWrapper::UpdateWebRenderTextureHost(
    WebRenderTextureHost* aTextureHost) {
  MOZ_ASSERT(aTextureHost);
  mWrTextureHost = aTextureHost;
  wr::RenderThread::Get()->UpdateRenderTextureHost(
      wr::AsUint64(mExternalImageId),
      wr::AsUint64(aTextureHost->GetExternalImageKey()));
}

}  // namespace layers
}  // namespace mozilla
