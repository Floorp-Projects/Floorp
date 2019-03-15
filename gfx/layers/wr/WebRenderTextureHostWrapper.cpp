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
#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/webrender/RenderThread.h"

namespace mozilla {
namespace layers {

class ScheduleUpdateRenderTextureHost : public wr::NotificationHandler {
 public:
  ScheduleUpdateRenderTextureHost(uint64_t aSrcExternalImageId,
                                  uint64_t aWrappedExternalImageId)
      : mSrcExternalImageId(aSrcExternalImageId),
        mWrappedExternalImageId(aWrappedExternalImageId) {}

  virtual void Notify(wr::Checkpoint aCheckpoint) override {
    if (aCheckpoint == wr::Checkpoint::FrameTexturesUpdated) {
      MOZ_ASSERT(wr::RenderThread::IsInRenderThread());
      wr::RenderThread::Get()->UpdateRenderTextureHost(mSrcExternalImageId,
                                                       mWrappedExternalImageId);
    } else {
      MOZ_ASSERT(aCheckpoint == wr::Checkpoint::TransactionDropped);
    }
  }

 protected:
  uint64_t mSrcExternalImageId;
  uint64_t mWrappedExternalImageId;
};

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
    wr::TransactionBuilder& aTxn, WebRenderTextureHost* aTextureHost) {
  MOZ_ASSERT(aTextureHost);

  // AsyncImagePipelineManager is responsible of holding compositable ref of
  // wrapped WebRenderTextureHost by using ForwardingTextureHost.
  // ScheduleUpdateRenderTextureHost does not need to handle it.

  aTxn.Notify(wr::Checkpoint::FrameTexturesUpdated,
              MakeUnique<ScheduleUpdateRenderTextureHost>(
                  wr::AsUint64(mExternalImageId),
                  wr::AsUint64(aTextureHost->GetExternalImageKey())));

  mWrTextureHost = aTextureHost;
}

}  // namespace layers
}  // namespace mozilla
