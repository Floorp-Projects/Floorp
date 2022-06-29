/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderImageHost.h"

#include <utility>

#include "mozilla/ScopeExit.h"
#include "mozilla/layers/AsyncImagePipelineManager.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/CompositorVsyncScheduler.h"  // for CompositorVsyncScheduler
#include "mozilla/layers/RemoteTextureMap.h"
#include "mozilla/layers/WebRenderBridgeParent.h"
#include "mozilla/layers/WebRenderTextureHost.h"
#include "nsAString.h"
#include "nsDebug.h"          // for NS_WARNING, NS_ASSERTION
#include "nsPrintfCString.h"  // for nsPrintfCString
#include "nsString.h"         // for nsAutoCString

namespace mozilla {

using namespace gfx;

namespace layers {

class ISurfaceAllocator;

WebRenderImageHost::WebRenderImageHost(const TextureInfo& aTextureInfo)
    : CompositableHost(aTextureInfo),
      ImageComposite(),
      mCurrentAsyncImageManager(nullptr) {}

WebRenderImageHost::~WebRenderImageHost() { MOZ_ASSERT(mWrBridges.empty()); }

void WebRenderImageHost::OnReleased() {
  if (mRemoteTextureConsumer) {
    mRemoteTextureConsumer = nullptr;
  }
}

void WebRenderImageHost::UseTextureHost(
    const nsTArray<TimedTexture>& aTextures) {
  CompositableHost::UseTextureHost(aTextures);
  MOZ_ASSERT(aTextures.Length() >= 1);

  if (mRemoteTextureConsumer) {
    mRemoteTextureConsumer = nullptr;
  }

  nsTArray<TimedImage> newImages;

  for (uint32_t i = 0; i < aTextures.Length(); ++i) {
    const TimedTexture& t = aTextures[i];
    MOZ_ASSERT(t.mTexture);
    if (i + 1 < aTextures.Length() && t.mProducerID == mLastProducerID &&
        t.mFrameID < mLastFrameID) {
      // Ignore frames before a frame that we already composited. We don't
      // ever want to display these frames. This could be important if
      // the frame producer adjusts timestamps (e.g. to track the audio clock)
      // and the new frame times are earlier.
      continue;
    }
    TimedImage& img = *newImages.AppendElement();
    img.mTextureHost = t.mTexture;
    img.mTimeStamp = t.mTimeStamp;
    img.mPictureRect = t.mPictureRect;
    img.mFrameID = t.mFrameID;
    img.mProducerID = t.mProducerID;
    img.mTextureHost->SetCropRect(img.mPictureRect);
  }

  SetImages(std::move(newImages));

  if (GetAsyncRef()) {
    for (const auto& it : mWrBridges) {
      RefPtr<WebRenderBridgeParent> wrBridge = it.second->WrBridge();
      if (wrBridge && wrBridge->CompositorScheduler()) {
        wrBridge->CompositorScheduler()->ScheduleComposition(
            wr::RenderReasons::ASYNC_IMAGE);
      }
    }
  }

  // Video producers generally send replacement images with the same frameID but
  // slightly different timestamps in order to sync with the audio clock. This
  // means that any CompositeUntil() call we made in Composite() may no longer
  // guarantee that we'll composite until the next frame is ready. Fix that
  // here.
  if (mLastFrameID >= 0 && !mWrBridges.empty()) {
    for (const auto& img : Images()) {
      bool frameComesAfter =
          img.mFrameID > mLastFrameID || img.mProducerID != mLastProducerID;
      if (frameComesAfter && !img.mTimeStamp.IsNull()) {
        for (const auto& it : mWrBridges) {
          RefPtr<WebRenderBridgeParent> wrBridge = it.second->WrBridge();
          if (wrBridge) {
            wrBridge->AsyncImageManager()->CompositeUntil(
                img.mTimeStamp + TimeDuration::FromMilliseconds(BIAS_TIME_MS));
          }
        }
        break;
      }
    }
  }
}

void WebRenderImageHost::UseRemoteTexture(const RemoteTextureId aTextureId,
                                          const RemoteTextureOwnerId aOwnerId,
                                          const CompositableHandle& aHandle,
                                          const base::ProcessId aForPid,
                                          const gfx::IntSize aSize,
                                          const TextureFlags aFlags) {
  MOZ_ASSERT(bool(GetAsyncRef()));

  RefPtr<WebRenderImageHost> self = this;
  const auto callback = [=](const RemoteTextureId textureId,
                            const RemoteTextureOwnerId ownerId,
                            const base::ProcessId forPid) {
    MOZ_ASSERT(aTextureId == textureId);
    MOZ_ASSERT(aOwnerId == ownerId);
    MOZ_ASSERT(aForPid == forPid);

    if (CompositorThreadHolder::IsInCompositorThread()) {
      self->UseRemoteTexture(aTextureId, aOwnerId, aHandle, aForPid, aSize,
                             aFlags);
      return;
    }

    CompositorThread()->Dispatch(
        NS_NewRunnableFunction("WebRenderImageHost::UseRemoteTexture", [=]() {
          self->UseRemoteTexture(aTextureId, aOwnerId, aHandle, aForPid, aSize,
                                 aFlags);
        }));
  };

  if (mRemoteTextureConsumer && mRemoteTextureConsumer->mOwnerId != aOwnerId) {
    mRemoteTextureConsumer = nullptr;
  }

  if (!mRemoteTextureConsumer) {
    mRemoteTextureConsumer = RemoteTextureMap::Get()->RegisterTextureConsumer(
        aOwnerId, aHandle, aForPid);
  }

  RefPtr<TextureHost> texture = mRemoteTextureConsumer->GetTextureHost(
      aTextureId, aSize, aFlags, callback);
  SetCurrentTextureHost(texture);

  if (GetAsyncRef()) {
    for (const auto& it : mWrBridges) {
      RefPtr<WebRenderBridgeParent> wrBridge = it.second->WrBridge();
      if (wrBridge && wrBridge->CompositorScheduler()) {
        wrBridge->CompositorScheduler()->ScheduleComposition(
            wr::RenderReasons::ASYNC_IMAGE);
      }
    }
  }
}

void WebRenderImageHost::CleanupResources() {
  ClearImages();
  SetCurrentTextureHost(nullptr);
}

void WebRenderImageHost::RemoveTextureHost(TextureHost* aTexture) {
  CompositableHost::RemoveTextureHost(aTexture);
  RemoveImagesWithTextureHost(aTexture);
}

TimeStamp WebRenderImageHost::GetCompositionTime() const {
  TimeStamp time;

  MOZ_ASSERT(mCurrentAsyncImageManager);
  if (mCurrentAsyncImageManager) {
    time = mCurrentAsyncImageManager->GetCompositionTime();
  }
  return time;
}

CompositionOpportunityId WebRenderImageHost::GetCompositionOpportunityId()
    const {
  CompositionOpportunityId id;

  MOZ_ASSERT(mCurrentAsyncImageManager);
  if (mCurrentAsyncImageManager) {
    id = mCurrentAsyncImageManager->GetCompositionOpportunityId();
  }
  return id;
}

void WebRenderImageHost::AppendImageCompositeNotification(
    const ImageCompositeNotificationInfo& aInfo) const {
  if (mCurrentAsyncImageManager) {
    mCurrentAsyncImageManager->AppendImageCompositeNotification(aInfo);
  }
}

TextureHost* WebRenderImageHost::GetAsTextureHostForComposite(
    AsyncImagePipelineManager* aAsyncImageManager) {
  if (mRemoteTextureConsumer) {
    return mCurrentTextureHost;
  }

  mCurrentAsyncImageManager = aAsyncImageManager;
  const auto onExit =
      mozilla::MakeScopeExit([&]() { mCurrentAsyncImageManager = nullptr; });

  int imageIndex = ChooseImageIndex();
  if (imageIndex < 0) {
    SetCurrentTextureHost(nullptr);
    return nullptr;
  }

  if (uint32_t(imageIndex) + 1 < ImagesCount()) {
    mCurrentAsyncImageManager->CompositeUntil(
        GetImage(imageIndex + 1)->mTimeStamp +
        TimeDuration::FromMilliseconds(BIAS_TIME_MS));
  }

  const TimedImage* img = GetImage(imageIndex);
  SetCurrentTextureHost(img->mTextureHost);

  if (mCurrentAsyncImageManager->GetCompositionTime()) {
    // We are in a composition. Send ImageCompositeNotifications.
    OnFinishRendering(imageIndex, img, mAsyncRef.mProcessId, mAsyncRef.mHandle);
  }

  return mCurrentTextureHost;
}

void WebRenderImageHost::SetCurrentTextureHost(TextureHost* aTexture) {
  if (aTexture == mCurrentTextureHost.get()) {
    return;
  }
  mCurrentTextureHost = aTexture;
}

void WebRenderImageHost::Dump(std::stringstream& aStream, const char* aPrefix,
                              bool aDumpHtml) {
  for (const auto& img : Images()) {
    aStream << aPrefix;
    aStream << (aDumpHtml ? "<ul><li>TextureHost: " : "TextureHost: ");
    DumpTextureHost(aStream, img.mTextureHost);
    aStream << (aDumpHtml ? " </li></ul> " : " ");
  }
}

void WebRenderImageHost::SetWrBridge(const wr::PipelineId& aPipelineId,
                                     WebRenderBridgeParent* aWrBridge) {
  MOZ_ASSERT(aWrBridge);
  MOZ_ASSERT(!mCurrentAsyncImageManager);
#ifdef DEBUG
  const auto it = mWrBridges.find(wr::AsUint64(aPipelineId));
  MOZ_ASSERT(it == mWrBridges.end());
#endif
  RefPtr<WebRenderBridgeParentRef> ref =
      aWrBridge->GetWebRenderBridgeParentRef();
  mWrBridges.emplace(wr::AsUint64(aPipelineId), ref);
}

void WebRenderImageHost::ClearWrBridge(const wr::PipelineId& aPipelineId,
                                       WebRenderBridgeParent* aWrBridge) {
  MOZ_ASSERT(aWrBridge);
  MOZ_ASSERT(!mCurrentAsyncImageManager);

  const auto it = mWrBridges.find(wr::AsUint64(aPipelineId));
  MOZ_ASSERT(it != mWrBridges.end());
  if (it == mWrBridges.end()) {
    gfxCriticalNote << "WrBridge mismatch happened";
    return;
  }
  mWrBridges.erase(it);
  SetCurrentTextureHost(nullptr);
}

}  // namespace layers
}  // namespace mozilla
