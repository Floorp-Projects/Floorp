/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERUSERDATA_H
#define GFX_WEBRENDERUSERDATA_H

#include <vector>
#include "BasicLayers.h"  // for BasicLayerManager
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/layers/AnimationInfo.h"
#include "mozilla/dom/RemoteBrowser.h"
#include "mozilla/UniquePtr.h"
#include "nsIFrame.h"
#include "ImageTypes.h"

class nsDisplayItemGeometry;

namespace mozilla {
namespace webgpu {
class WebGPUChild;
}

namespace wr {
class IpcResourceUpdateQueue;
}

namespace gfx {
class SourceSurface;
}

namespace layers {
class CanvasLayer;
class ImageClient;
class ImageContainer;
class WebRenderBridgeChild;
class WebRenderCanvasData;
class WebRenderCanvasRenderer;
class WebRenderImageData;
class WebRenderFallbackData;
class WebRenderLocalCanvasData;
class RenderRootStateManager;
class WebRenderGroupData;

class WebRenderBackgroundData {
 public:
  WebRenderBackgroundData(wr::LayoutRect aBounds, wr::ColorF aColor)
      : mBounds(aBounds), mColor(aColor) {}
  void AddWebRenderCommands(wr::DisplayListBuilder& aBuilder);

 protected:
  wr::LayoutRect mBounds;
  wr::ColorF mColor;
};

/// Parent class for arbitrary WebRender-specific data that can be associated
/// to an nsFrame.
class WebRenderUserData {
 public:
  typedef nsTHashtable<nsRefPtrHashKey<WebRenderUserData>>
      WebRenderUserDataRefTable;

  static bool SupportsAsyncUpdate(nsIFrame* aFrame);

  static bool ProcessInvalidateForImage(nsIFrame* aFrame, DisplayItemType aType,
                                        ContainerProducerID aProducerId);

  NS_INLINE_DECL_REFCOUNTING(WebRenderUserData)

  WebRenderUserData(RenderRootStateManager* aManager, nsDisplayItem* aItem);
  WebRenderUserData(RenderRootStateManager* aManager, uint32_t mDisplayItemKey,
                    nsIFrame* aFrame);

  virtual WebRenderImageData* AsImageData() { return nullptr; }
  virtual WebRenderFallbackData* AsFallbackData() { return nullptr; }
  virtual WebRenderCanvasData* AsCanvasData() { return nullptr; }
  virtual WebRenderLocalCanvasData* AsLocalCanvasData() { return nullptr; }
  virtual WebRenderGroupData* AsGroupData() { return nullptr; }

  enum class UserDataType {
    eImage,
    eFallback,
    eAPZAnimation,
    eAnimation,
    eCanvas,
    eLocalCanvas,
    eRemote,
    eGroup,
    eMask,
  };

  virtual UserDataType GetType() = 0;
  bool IsUsed() { return mUsed; }
  void SetUsed(bool aUsed) { mUsed = aUsed; }
  nsIFrame* GetFrame() { return mFrame; }
  uint32_t GetDisplayItemKey() { return mDisplayItemKey; }
  void RemoveFromTable();
  virtual nsDisplayItemGeometry* GetGeometry() { return nullptr; }

 protected:
  virtual ~WebRenderUserData();

  WebRenderBridgeChild* WrBridge() const;

  RefPtr<RenderRootStateManager> mManager;
  nsIFrame* mFrame;
  uint32_t mDisplayItemKey;
  WebRenderUserDataRefTable* mTable;
  bool mUsed;
};

struct WebRenderUserDataKey {
  WebRenderUserDataKey(uint32_t aFrameKey,
                       WebRenderUserData::UserDataType aType)
      : mFrameKey(aFrameKey), mType(aType) {}

  bool operator==(const WebRenderUserDataKey& other) const {
    return mFrameKey == other.mFrameKey && mType == other.mType;
  }
  PLDHashNumber Hash() const {
    return HashGeneric(
        mFrameKey,
        static_cast<std::underlying_type<decltype(mType)>::type>(mType));
  }

  uint32_t mFrameKey;
  WebRenderUserData::UserDataType mType;
};

typedef nsRefPtrHashtable<
    nsGenericHashKey<mozilla::layers::WebRenderUserDataKey>, WebRenderUserData>
    WebRenderUserDataTable;

/// Holds some data used to share TextureClient/ImageClient with the parent
/// process.
class WebRenderImageData : public WebRenderUserData {
 public:
  WebRenderImageData(RenderRootStateManager* aManager, nsDisplayItem* aItem);
  WebRenderImageData(RenderRootStateManager* aManager, uint32_t aDisplayItemKey,
                     nsIFrame* aFrame);
  virtual ~WebRenderImageData();

  WebRenderImageData* AsImageData() override { return this; }
  UserDataType GetType() override { return UserDataType::eImage; }
  static UserDataType Type() { return UserDataType::eImage; }
  Maybe<wr::ImageKey> GetImageKey() { return mKey; }
  void SetImageKey(const wr::ImageKey& aKey);
  already_AddRefed<ImageClient> GetImageClient();

  Maybe<wr::ImageKey> UpdateImageKey(ImageContainer* aContainer,
                                     wr::IpcResourceUpdateQueue& aResources,
                                     bool aFallback = false);

  void CreateAsyncImageWebRenderCommands(
      mozilla::wr::DisplayListBuilder& aBuilder, ImageContainer* aContainer,
      const StackingContextHelper& aSc, const LayoutDeviceRect& aBounds,
      const LayoutDeviceRect& aSCBounds, const gfx::Matrix4x4& aSCTransform,
      const gfx::MaybeIntSize& aScaleToSize, const wr::ImageRendering& aFilter,
      const wr::MixBlendMode& aMixBlendMode, bool aIsBackfaceVisible);

  void CreateImageClientIfNeeded();

  bool IsAsync() { return mPipelineId.isSome(); }

  bool UsingSharedSurface(ContainerProducerID aProducerId) const;

  void ClearImageKey();

 protected:
  Maybe<wr::ImageKey> mKey;
  RefPtr<TextureClient> mTextureOfImage;
  RefPtr<ImageClient> mImageClient;
  Maybe<wr::PipelineId> mPipelineId;
  RefPtr<ImageContainer> mContainer;
  // The key can be owned by a shared surface that is used by several elements.
  // when this is the case the shared surface is responsible for managing the
  // destruction of the key.
  // TODO: we surely can come up with a simpler/safer way to model this.
  bool mOwnsKey;
};

/// Used for fallback rendering.
///
/// In most cases this uses blob images but it can also render on the content
/// side directly into a texture.
class WebRenderFallbackData : public WebRenderUserData {
 public:
  WebRenderFallbackData(RenderRootStateManager* aManager, nsDisplayItem* aItem);
  virtual ~WebRenderFallbackData();

  WebRenderFallbackData* AsFallbackData() override { return this; }
  UserDataType GetType() override { return UserDataType::eFallback; }
  static UserDataType Type() { return UserDataType::eFallback; }

  void SetInvalid(bool aInvalid) { mInvalid = aInvalid; }
  bool IsInvalid() { return mInvalid; }
  void SetFonts(const std::vector<RefPtr<gfx::ScaledFont>>& aFonts) {
    mFonts = aFonts;
  }
  Maybe<wr::BlobImageKey> GetBlobImageKey() { return mBlobKey; }
  void SetBlobImageKey(const wr::BlobImageKey& aKey);
  Maybe<wr::ImageKey> GetImageKey();

  /// Create a WebRenderImageData to manage the image we are about to render
  /// into.
  WebRenderImageData* PaintIntoImage();

  std::vector<RefPtr<gfx::SourceSurface>> mExternalSurfaces;
  RefPtr<BasicLayerManager> mBasicLayerManager;
  UniquePtr<nsDisplayItemGeometry> mGeometry;
  nsRect mBounds;
  nsRect mBuildingRect;
  gfx::Size mScale;

 protected:
  void ClearImageKey();

  std::vector<RefPtr<gfx::ScaledFont>> mFonts;
  Maybe<wr::BlobImageKey> mBlobKey;
  // When rendering into a blob image, mImageData is null. It is non-null only
  // when we render directly into a texture on the content side.
  RefPtr<WebRenderImageData> mImageData;
  bool mInvalid;
};

class WebRenderAPZAnimationData : public WebRenderUserData {
 public:
  WebRenderAPZAnimationData(RenderRootStateManager* aManager,
                            nsDisplayItem* aItem);
  virtual ~WebRenderAPZAnimationData() = default;

  UserDataType GetType() override { return UserDataType::eAPZAnimation; }
  static UserDataType Type() { return UserDataType::eAPZAnimation; }
  uint64_t GetAnimationId() { return mAnimationId; }

 private:
  uint64_t mAnimationId;
};

class WebRenderAnimationData : public WebRenderUserData {
 public:
  WebRenderAnimationData(RenderRootStateManager* aManager,
                         nsDisplayItem* aItem);
  virtual ~WebRenderAnimationData();

  UserDataType GetType() override { return UserDataType::eAnimation; }
  static UserDataType Type() { return UserDataType::eAnimation; }
  AnimationInfo& GetAnimationInfo() { return mAnimationInfo; }

 protected:
  AnimationInfo mAnimationInfo;
};

class WebRenderCanvasData : public WebRenderUserData {
 public:
  WebRenderCanvasData(RenderRootStateManager* aManager, nsDisplayItem* aItem);
  virtual ~WebRenderCanvasData();

  WebRenderCanvasData* AsCanvasData() override { return this; }
  UserDataType GetType() override { return UserDataType::eCanvas; }
  static UserDataType Type() { return UserDataType::eCanvas; }

  void ClearCanvasRenderer();
  WebRenderCanvasRendererAsync* GetCanvasRenderer();
  WebRenderCanvasRendererAsync* CreateCanvasRenderer();

  void SetImageContainer(ImageContainer* aImageContainer);
  ImageContainer* GetImageContainer();
  void ClearImageContainer();

 protected:
  UniquePtr<WebRenderCanvasRendererAsync> mCanvasRenderer;
  RefPtr<ImageContainer> mContainer;
};

// WebRender data assocatiated with canvases that don't need to
// synchronize across content-GPU process barrier.
class WebRenderLocalCanvasData : public WebRenderUserData {
 public:
  WebRenderLocalCanvasData(RenderRootStateManager* aManager,
                           nsDisplayItem* aItem);
  virtual ~WebRenderLocalCanvasData();

  WebRenderLocalCanvasData* AsLocalCanvasData() override { return this; }
  UserDataType GetType() override { return UserDataType::eLocalCanvas; }
  static UserDataType Type() { return UserDataType::eLocalCanvas; }

  void RequestFrameReadback();
  void RefreshExternalImage();

  // TODO: introduce a CanvasRenderer derivative to store here?

  WeakPtr<webgpu::WebGPUChild> mGpuBridge;
  uint64_t mGpuTextureId = 0;
  wr::ExternalImageId mExternalImageId = {0};
  wr::ImageKey mImageKey = {};
  wr::ImageDescriptor mDescriptor;
  gfx::SurfaceFormat mFormat = gfx::SurfaceFormat::UNKNOWN;
  bool mDirty = false;
};

class WebRenderRemoteData : public WebRenderUserData {
 public:
  WebRenderRemoteData(RenderRootStateManager* aManager, nsDisplayItem* aItem);
  virtual ~WebRenderRemoteData();

  UserDataType GetType() override { return UserDataType::eRemote; }
  static UserDataType Type() { return UserDataType::eRemote; }

  void SetRemoteBrowser(dom::RemoteBrowser* aBrowser) {
    mRemoteBrowser = aBrowser;
  }

 protected:
  RefPtr<dom::RemoteBrowser> mRemoteBrowser;
};

extern void DestroyWebRenderUserDataTable(WebRenderUserDataTable* aTable);

struct WebRenderUserDataProperty {
  NS_DECLARE_FRAME_PROPERTY_WITH_DTOR(Key, WebRenderUserDataTable,
                                      DestroyWebRenderUserDataTable)
};

template <class T>
already_AddRefed<T> GetWebRenderUserData(const nsIFrame* aFrame,
                                         uint32_t aPerFrameKey) {
  MOZ_ASSERT(aFrame);
  WebRenderUserDataTable* userDataTable =
      aFrame->GetProperty(WebRenderUserDataProperty::Key());
  if (!userDataTable) {
    return nullptr;
  }

  WebRenderUserData* data =
      userDataTable->GetWeak(WebRenderUserDataKey(aPerFrameKey, T::Type()));
  if (data) {
    RefPtr<T> result = static_cast<T*>(data);
    return result.forget();
  }

  return nullptr;
}

}  // namespace layers
}  // namespace mozilla

#endif /* GFX_WEBRENDERUSERDATA_H */
