/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureHost.h"

#include "CompositableHost.h"  // for CompositableHost
#include "mozilla/gfx/2D.h"    // for DataSourceSurface, Factory
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/ipc/Shmem.h"  // for Shmem
#include "mozilla/layers/AsyncImagePipelineManager.h"
#include "mozilla/layers/BufferTexture.h"
#include "mozilla/layers/CompositableTransactionParent.h"  // for CompositableParentManager
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/Compositor.h"         // for Compositor
#include "mozilla/layers/ISurfaceAllocator.h"  // for ISurfaceAllocator
#include "mozilla/layers/ImageBridgeParent.h"  // for ImageBridgeParent
#include "mozilla/layers/LayersSurfaces.h"     // for SurfaceDescriptor, etc
#include "mozilla/layers/TextureHostOGL.h"     // for TextureHostOGL
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/TextureClient.h"
#ifdef XP_DARWIN
#  include "mozilla/layers/TextureSync.h"
#endif
#include "mozilla/layers/GPUVideoTextureHost.h"
#include "mozilla/layers/WebRenderTextureHost.h"
#include "mozilla/StaticPrefs_layers.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/webrender/RenderBufferTextureHost.h"
#include "mozilla/webrender/RenderExternalTextureHost.h"
#include "mozilla/webrender/RenderThread.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "nsAString.h"
#include "mozilla/RefPtr.h"   // for nsRefPtr
#include "nsPrintfCString.h"  // for nsPrintfCString
#include "mozilla/layers/PTextureParent.h"
#include "mozilla/Unused.h"
#include <limits>
#include "../opengl/CompositorOGL.h"

#include "gfxUtils.h"
#include "IPDLActor.h"

#ifdef MOZ_ENABLE_D3D10_LAYER
#  include "../d3d11/CompositorD3D11.h"
#endif

#ifdef XP_MACOSX
#  include "../opengl/MacIOSurfaceTextureHostOGL.h"
#endif

#ifdef XP_WIN
#  include "mozilla/layers/TextureD3D11.h"
#endif

#if 0
#  define RECYCLE_LOG(...) printf_stderr(__VA_ARGS__)
#else
#  define RECYCLE_LOG(...) \
    do {                   \
    } while (0)
#endif

namespace mozilla {
namespace layers {

/**
 * TextureParent is the host-side IPDL glue between TextureClient and
 * TextureHost. It is an IPDL actor just like LayerParent, CompositableParent,
 * etc.
 */
class TextureParent : public ParentActor<PTextureParent> {
 public:
  TextureParent(HostIPCAllocator* aAllocator, uint64_t aSerial,
                const wr::MaybeExternalImageId& aExternalImageId);

  virtual ~TextureParent();

  bool Init(const SurfaceDescriptor& aSharedData,
            const ReadLockDescriptor& aReadLock,
            const LayersBackend& aLayersBackend, const TextureFlags& aFlags);

  void NotifyNotUsed(uint64_t aTransactionId);

  mozilla::ipc::IPCResult RecvRecycleTexture(
      const TextureFlags& aTextureFlags) final;

  TextureHost* GetTextureHost() { return mTextureHost; }

  void Destroy() override;

  uint64_t GetSerial() const { return mSerial; }

  HostIPCAllocator* mSurfaceAllocator;
  RefPtr<TextureHost> mTextureHost;
  // mSerial is unique in TextureClient's process.
  const uint64_t mSerial;
  wr::MaybeExternalImageId mExternalImageId;
};

static bool WrapWithWebRenderTextureHost(ISurfaceAllocator* aDeallocator,
                                         LayersBackend aBackend,
                                         TextureFlags aFlags) {
  if ((aFlags & TextureFlags::SNAPSHOT) ||
      (aBackend != LayersBackend::LAYERS_WR) ||
      (!aDeallocator->UsesImageBridge() &&
       !aDeallocator->AsCompositorBridgeParentBase())) {
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
PTextureParent* TextureHost::CreateIPDLActor(
    HostIPCAllocator* aAllocator, const SurfaceDescriptor& aSharedData,
    const ReadLockDescriptor& aReadLock, LayersBackend aLayersBackend,
    TextureFlags aFlags, uint64_t aSerial,
    const wr::MaybeExternalImageId& aExternalImageId) {
  TextureParent* actor =
      new TextureParent(aAllocator, aSerial, aExternalImageId);
  if (!actor->Init(aSharedData, aReadLock, aLayersBackend, aFlags)) {
    actor->ActorDestroy(ipc::IProtocol::ActorDestroyReason::FailedConstructor);
    delete actor;
    return nullptr;
  }
  return actor;
}

// static
bool TextureHost::DestroyIPDLActor(PTextureParent* actor) {
  delete actor;
  return true;
}

// static
bool TextureHost::SendDeleteIPDLActor(PTextureParent* actor) {
  return PTextureParent::Send__delete__(actor);
}

// static
TextureHost* TextureHost::AsTextureHost(PTextureParent* actor) {
  if (!actor) {
    return nullptr;
  }
  return static_cast<TextureParent*>(actor)->mTextureHost;
}

// static
uint64_t TextureHost::GetTextureSerial(PTextureParent* actor) {
  if (!actor) {
    return UINT64_MAX;
  }
  return static_cast<TextureParent*>(actor)->mSerial;
}

PTextureParent* TextureHost::GetIPDLActor() { return mActor; }

void TextureHost::SetLastFwdTransactionId(uint64_t aTransactionId) {
  MOZ_ASSERT(mFwdTransactionId <= aTransactionId);
  mFwdTransactionId = aTransactionId;
}

already_AddRefed<TextureHost> CreateDummyBufferTextureHost(
    mozilla::layers::LayersBackend aBackend,
    mozilla::layers::TextureFlags aFlags) {
  // Ensure that the host will delete the memory.
  aFlags &= ~TextureFlags::DEALLOCATE_CLIENT;
  UniquePtr<TextureData> textureData(BufferTextureData::Create(
      gfx::IntSize(1, 1), gfx::SurfaceFormat::B8G8R8A8, gfx::BackendType::SKIA,
      aBackend, aFlags, TextureAllocationFlags::ALLOC_DEFAULT, nullptr));
  SurfaceDescriptor surfDesc;
  textureData->Serialize(surfDesc);
  const SurfaceDescriptorBuffer& bufferDesc =
      surfDesc.get_SurfaceDescriptorBuffer();
  const MemoryOrShmem& data = bufferDesc.data();
  RefPtr<TextureHost> host =
      new MemoryTextureHost(reinterpret_cast<uint8_t*>(data.get_uintptr_t()),
                            bufferDesc.desc(), aFlags);
  return host.forget();
}

already_AddRefed<TextureHost> TextureHost::Create(
    const SurfaceDescriptor& aDesc, const ReadLockDescriptor& aReadLock,
    ISurfaceAllocator* aDeallocator, LayersBackend aBackend,
    TextureFlags aFlags, wr::MaybeExternalImageId& aExternalImageId) {
  RefPtr<TextureHost> result;

  switch (aDesc.type()) {
    case SurfaceDescriptor::TSurfaceDescriptorBuffer:
    case SurfaceDescriptor::TSurfaceDescriptorGPUVideo:
      result = CreateBackendIndependentTextureHost(aDesc, aDeallocator,
                                                   aBackend, aFlags);
      break;

    case SurfaceDescriptor::TEGLImageDescriptor:
    case SurfaceDescriptor::TSurfaceTextureDescriptor:
    case SurfaceDescriptor::TSurfaceDescriptorAndroidHardwareBuffer:
    case SurfaceDescriptor::TSurfaceDescriptorSharedGLTexture:
    case SurfaceDescriptor::TSurfaceDescriptorDMABuf:
      result = CreateTextureHostOGL(aDesc, aDeallocator, aBackend, aFlags);
      break;

    case SurfaceDescriptor::TSurfaceDescriptorMacIOSurface:
      result = CreateTextureHostOGL(aDesc, aDeallocator, aBackend, aFlags);
      break;

#ifdef XP_WIN
    case SurfaceDescriptor::TSurfaceDescriptorD3D10:
    case SurfaceDescriptor::TSurfaceDescriptorDXGIYCbCr:
      result = CreateTextureHostD3D11(aDesc, aDeallocator, aBackend, aFlags);
      break;
#endif
    case SurfaceDescriptor::TSurfaceDescriptorRecorded: {
      const SurfaceDescriptorRecorded& desc =
          aDesc.get_SurfaceDescriptorRecorded();
      UniquePtr<SurfaceDescriptor> realDesc =
          aDeallocator->AsCompositorBridgeParentBase()
              ->LookupSurfaceDescriptorForClientTexture(desc.textureId());
      if (!realDesc) {
        gfxCriticalNote << "Failed to get descriptor for recorded texture.";
        // Create a dummy to prevent any crashes due to missing IPDL actors.
        result = CreateDummyBufferTextureHost(aBackend, aFlags);
        break;
      }

      result = TextureHost::Create(*realDesc, aReadLock, aDeallocator, aBackend,
                                   aFlags, aExternalImageId);
      return result.forget();
    }
    default:
      MOZ_CRASH("GFX: Unsupported Surface type host");
  }

  if (!result) {
    gfxCriticalNote << "TextureHost creation failure type=" << aDesc.type();
  }

  if (result && WrapWithWebRenderTextureHost(aDeallocator, aBackend, aFlags)) {
    MOZ_ASSERT(aExternalImageId.isSome());
    result =
        new WebRenderTextureHost(aDesc, aFlags, result, aExternalImageId.ref());
  }

  if (result) {
    result->DeserializeReadLock(aReadLock, aDeallocator);
  }

  return result.forget();
}

already_AddRefed<TextureHost> CreateBackendIndependentTextureHost(
    const SurfaceDescriptor& aDesc, ISurfaceAllocator* aDeallocator,
    LayersBackend aBackend, TextureFlags aFlags) {
  RefPtr<TextureHost> result;
  switch (aDesc.type()) {
    case SurfaceDescriptor::TSurfaceDescriptorBuffer: {
      const SurfaceDescriptorBuffer& bufferDesc =
          aDesc.get_SurfaceDescriptorBuffer();
      const MemoryOrShmem& data = bufferDesc.data();
      switch (data.type()) {
        case MemoryOrShmem::TShmem: {
          const ipc::Shmem& shmem = data.get_Shmem();
          const BufferDescriptor& desc = bufferDesc.desc();
          if (!shmem.IsReadable()) {
            // We failed to map the shmem so we can't verify its size. This
            // should not be a fatal error, so just create the texture with
            // nothing backing it.
            result = new ShmemTextureHost(shmem, desc, aDeallocator, aFlags);
            break;
          }

          size_t bufSize = shmem.Size<char>();
          size_t reqSize = SIZE_MAX;
          switch (desc.type()) {
            case BufferDescriptor::TYCbCrDescriptor: {
              const YCbCrDescriptor& ycbcr = desc.get_YCbCrDescriptor();
              reqSize = ImageDataSerializer::ComputeYCbCrBufferSize(
                  ycbcr.ySize(), ycbcr.yStride(), ycbcr.cbCrSize(),
                  ycbcr.cbCrStride(), ycbcr.yOffset(), ycbcr.cbOffset(),
                  ycbcr.crOffset());
              break;
            }
            case BufferDescriptor::TRGBDescriptor: {
              const RGBDescriptor& rgb = desc.get_RGBDescriptor();
              reqSize = ImageDataSerializer::ComputeRGBBufferSize(rgb.size(),
                                                                  rgb.format());
              break;
            }
            default:
              gfxCriticalError()
                  << "Bad buffer host descriptor " << (int)desc.type();
              MOZ_CRASH("GFX: Bad descriptor");
          }

          if (reqSize == 0 || bufSize < reqSize) {
            NS_ERROR(
                "A client process gave a shmem too small to fit for its "
                "descriptor!");
            return nullptr;
          }

          result = new ShmemTextureHost(shmem, desc, aDeallocator, aFlags);
          break;
        }
        case MemoryOrShmem::Tuintptr_t: {
          if (!aDeallocator->IsSameProcess()) {
            NS_ERROR(
                "A client process is trying to peek at our address space using "
                "a MemoryTexture!");
            return nullptr;
          }

          result = new MemoryTextureHost(
              reinterpret_cast<uint8_t*>(data.get_uintptr_t()),
              bufferDesc.desc(), aFlags);
          break;
        }
        default:
          gfxCriticalError()
              << "Failed texture host for backend " << (int)data.type();
          MOZ_CRASH("GFX: No texture host for backend");
      }
      break;
    }
    case SurfaceDescriptor::TSurfaceDescriptorGPUVideo: {
      MOZ_ASSERT(aDesc.get_SurfaceDescriptorGPUVideo().type() ==
                 SurfaceDescriptorGPUVideo::TSurfaceDescriptorRemoteDecoder);
      result = GPUVideoTextureHost::CreateFromDescriptor(
          aFlags, aDesc.get_SurfaceDescriptorGPUVideo());
      break;
    }
    default: {
      NS_WARNING("No backend independent TextureHost for this descriptor type");
    }
  }
  return result.forget();
}

TextureHost::TextureHost(TextureFlags aFlags)
    : AtomicRefCountedWithFinalize("TextureHost"),
      mActor(nullptr),
      mFlags(aFlags),
      mCompositableCount(0),
      mFwdTransactionId(0),
      mReadLocked(false) {}

TextureHost::~TextureHost() {
  if (mReadLocked) {
    // If we still have a ReadLock, unlock it. At this point we don't care about
    // the texture client being written into on the other side since it should
    // be destroyed by now. But we will hit assertions if we don't ReadUnlock
    // before destroying the lock itself.
    ReadUnlock();
    MaybeNotifyUnlocked();
  }
}

void TextureHost::Finalize() {
  MaybeDestroyRenderTexture();

  if (!(GetFlags() & TextureFlags::DEALLOCATE_CLIENT)) {
    DeallocateSharedData();
    DeallocateDeviceData();
  }
}

void TextureHost::UnbindTextureSource() {
  if (mReadLocked) {
    // This TextureHost is not used anymore. Since most compositor backends are
    // working asynchronously under the hood a compositor could still be using
    // this texture, so it is generally best to wait until the end of the next
    // composition before calling ReadUnlock. We ask the compositor to take care
    // of that for us.
    if (mProvider) {
      mProvider->UnlockAfterComposition(this);
    } else {
      // GetCompositor returned null which means no compositor can be using this
      // texture. We can ReadUnlock right away.
      ReadUnlock();
      MaybeNotifyUnlocked();
    }
  }
}

void TextureHost::RecycleTexture(TextureFlags aFlags) {
  MOZ_ASSERT(GetFlags() & TextureFlags::RECYCLE);
  MOZ_ASSERT(aFlags & TextureFlags::RECYCLE);
  mFlags = aFlags;
}

void TextureHost::NotifyNotUsed() {
  if (!mActor) {
    return;
  }

  // Do not need to call NotifyNotUsed() if TextureHost does not have
  // TextureFlags::RECYCLE flag nor TextureFlags::WAIT_HOST_USAGE_END flag.
  if (!(GetFlags() & TextureFlags::RECYCLE) &&
      !(GetFlags() & TextureFlags::WAIT_HOST_USAGE_END)) {
    return;
  }

  // The following cases do not need to defer NotifyNotUsed until next
  // Composite.
  // - TextureHost does not have Compositor.
  // - Compositor is BasicCompositor.
  // - TextureHost has intermediate buffer.
  //   end of buffer usage.
  if (!mProvider || HasIntermediateBuffer() ||
      !mProvider->NotifyNotUsedAfterComposition(this)) {
    static_cast<TextureParent*>(mActor)->NotifyNotUsed(mFwdTransactionId);
    return;
  }
}

void TextureHost::CallNotifyNotUsed() {
  if (!mActor) {
    return;
  }
  static_cast<TextureParent*>(mActor)->NotifyNotUsed(mFwdTransactionId);
}

void TextureHost::MaybeDestroyRenderTexture() {
  if (mExternalImageId.isNothing()) {
    // RenderTextureHost was not created
    return;
  }
  // When TextureHost created RenderTextureHost, delete it here.
  TextureHost::DestroyRenderTexture(mExternalImageId.ref());
}

void TextureHost::DestroyRenderTexture(
    const wr::ExternalImageId& aExternalImageId) {
  wr::RenderThread::Get()->UnregisterExternalImage(
      wr::AsUint64(aExternalImageId));
}

void TextureHost::EnsureRenderTexture(
    const wr::MaybeExternalImageId& aExternalImageId) {
  if (aExternalImageId.isNothing()) {
    // TextureHost is wrapped by GPUVideoTextureHost.
    if (mExternalImageId.isSome()) {
      // RenderTextureHost was already created.
      return;
    }
    mExternalImageId =
        Some(AsyncImagePipelineManager::GetNextExternalImageId());
  } else {
    // TextureHost is wrapped by WebRenderTextureHost.
    if (aExternalImageId == mExternalImageId) {
      // The texture has already been created.
      return;
    }
    MOZ_ASSERT(mExternalImageId.isNothing());
    mExternalImageId = aExternalImageId;
  }
  CreateRenderTexture(mExternalImageId.ref());
}

void TextureHost::Updated(const nsIntRegion* aRegion) {
  UpdatedInternal(aRegion);
}

TextureSource::TextureSource() : mCompositableCount(0) {}

TextureSource::~TextureSource() = default;
BufferTextureHost::BufferTextureHost(const BufferDescriptor& aDesc,
                                     TextureFlags aFlags)
    : TextureHost(aFlags),
      mUpdateSerial(1),
      mLocked(false),
      mNeedsFullUpdate(false) {
  mDescriptor = aDesc;
  switch (mDescriptor.type()) {
    case BufferDescriptor::TYCbCrDescriptor: {
      const YCbCrDescriptor& ycbcr = mDescriptor.get_YCbCrDescriptor();
      mSize = ycbcr.display().Size();
      mFormat = gfx::SurfaceFormat::YUV;
      mHasIntermediateBuffer = ycbcr.hasIntermediateBuffer();
      break;
    }
    case BufferDescriptor::TRGBDescriptor: {
      const RGBDescriptor& rgb = mDescriptor.get_RGBDescriptor();
      mSize = rgb.size();
      mFormat = rgb.format();
      mHasIntermediateBuffer = rgb.hasIntermediateBuffer();
      break;
    }
    default:
      gfxCriticalError() << "Bad buffer host descriptor "
                         << (int)mDescriptor.type();
      MOZ_CRASH("GFX: Bad descriptor");
  }
  if (aFlags & TextureFlags::COMPONENT_ALPHA) {
    // One texture of a component alpha texture pair will start out all white.
    // This hack allows us to easily make sure that white will be uploaded.
    // See bug 1138934
    mNeedsFullUpdate = true;
  }

#ifdef XP_MACOSX
  const int kMinSize = 1024;
  const int kMaxSize = 4096;
  mUseExternalTextures =
      kMaxSize >= mSize.width && mSize.width >= kMinSize &&
      kMaxSize >= mSize.height && mSize.height >= kMinSize &&
      StaticPrefs::gfx_webrender_enable_client_storage_AtStartup();
#else
  mUseExternalTextures = false;
#endif
}

BufferTextureHost::~BufferTextureHost() = default;

void BufferTextureHost::UpdatedInternal(const nsIntRegion* aRegion) {
  ++mUpdateSerial;
  // If the last frame wasn't uploaded yet, and we -don't- have a partial
  // update, we still need to update the full surface.
  if (aRegion && !mNeedsFullUpdate) {
    mMaybeUpdatedRegion.OrWith(*aRegion);
  } else {
    mNeedsFullUpdate = true;
  }
  if (GetFlags() & TextureFlags::IMMEDIATE_UPLOAD) {
    DebugOnly<bool> result =
        MaybeUpload(!mNeedsFullUpdate ? &mMaybeUpdatedRegion : nullptr);
    NS_WARNING_ASSERTION(result, "Failed to upload a texture");
  }
}

void BufferTextureHost::SetTextureSourceProvider(
    TextureSourceProvider* aProvider) {
  if (mProvider == aProvider) {
    return;
  }
  if (mFirstSource && mFirstSource->IsOwnedBy(this)) {
    mFirstSource->SetOwner(nullptr);
  }
  if (mFirstSource) {
    mFirstSource = nullptr;
    mNeedsFullUpdate = true;
  }
  mProvider = aProvider;
}

void BufferTextureHost::DeallocateDeviceData() {
  if (mFirstSource && mFirstSource->NumCompositableRefs() > 0) {
    // WrappingTextureSourceYCbCrBasic wraps YUV format BufferTextureHost.
    // When BufferTextureHost is destroyed, data of
    // WrappingTextureSourceYCbCrBasic becomes invalid.
    if (mFirstSource->AsWrappingTextureSourceYCbCrBasic() &&
        mFirstSource->IsOwnedBy(this)) {
      mFirstSource->SetOwner(nullptr);
      mFirstSource->DeallocateDeviceData();
    }
    return;
  }

  if (!mFirstSource || !mFirstSource->IsOwnedBy(this)) {
    mFirstSource = nullptr;
    return;
  }

  mFirstSource->SetOwner(nullptr);

  RefPtr<TextureSource> it = mFirstSource;
  while (it) {
    it->DeallocateDeviceData();
    it = it->GetNextSibling();
  }
}

bool BufferTextureHost::Lock() {
  MOZ_ASSERT(!mLocked);
  if (!UploadIfNeeded()) {
    return false;
  }
  mLocked = !!mFirstSource;
  return mLocked;
}

void BufferTextureHost::Unlock() {
  MOZ_ASSERT(mLocked);
  mLocked = false;
}

void BufferTextureHost::CreateRenderTexture(
    const wr::ExternalImageId& aExternalImageId) {
  RefPtr<wr::RenderTextureHost> texture;

  if (UseExternalTextures()) {
    texture =
        new wr::RenderExternalTextureHost(GetBuffer(), GetBufferDescriptor());
  } else {
    texture =
        new wr::RenderBufferTextureHost(GetBuffer(), GetBufferDescriptor());
  }

  wr::RenderThread::Get()->RegisterExternalImage(wr::AsUint64(aExternalImageId),
                                                 texture.forget());
}

uint32_t BufferTextureHost::NumSubTextures() {
  if (GetFormat() == gfx::SurfaceFormat::YUV) {
    return 3;
  }

  return 1;
}

void BufferTextureHost::PushResourceUpdates(
    wr::TransactionBuilder& aResources, ResourceUpdateOp aOp,
    const Range<wr::ImageKey>& aImageKeys, const wr::ExternalImageId& aExtID) {
  auto method = aOp == TextureHost::ADD_IMAGE
                    ? &wr::TransactionBuilder::AddExternalImage
                    : &wr::TransactionBuilder::UpdateExternalImage;

  // Use native textures if our backend requires it, or if our backend doesn't
  // forbid it and we want to use them.
  NativeTexturePolicy policy =
      BackendNativeTexturePolicy(aResources.GetBackendType(), GetSize());
  bool useNativeTexture =
      (policy == REQUIRE) || (policy != FORBID && UseExternalTextures());
  auto imageType = useNativeTexture ? wr::ExternalImageType::TextureHandle(
                                          wr::ImageBufferKind::TextureRect)
                                    : wr::ExternalImageType::Buffer();

  if (GetFormat() != gfx::SurfaceFormat::YUV) {
    MOZ_ASSERT(aImageKeys.length() == 1);

    wr::ImageDescriptor descriptor(
        GetSize(),
        ImageDataSerializer::ComputeRGBStride(GetFormat(), GetSize().width),
        GetFormat());
    (aResources.*method)(aImageKeys[0], descriptor, aExtID, imageType, 0);
  } else {
    MOZ_ASSERT(aImageKeys.length() == 3);

    const layers::YCbCrDescriptor& desc = mDescriptor.get_YCbCrDescriptor();
    wr::ImageDescriptor yDescriptor(
        desc.ySize(), desc.yStride(),
        SurfaceFormatForColorDepth(desc.colorDepth()));
    wr::ImageDescriptor cbcrDescriptor(
        desc.cbCrSize(), desc.cbCrStride(),
        SurfaceFormatForColorDepth(desc.colorDepth()));
    (aResources.*method)(aImageKeys[0], yDescriptor, aExtID, imageType, 0);
    (aResources.*method)(aImageKeys[1], cbcrDescriptor, aExtID, imageType, 1);
    (aResources.*method)(aImageKeys[2], cbcrDescriptor, aExtID, imageType, 2);
  }
}

void BufferTextureHost::PushDisplayItems(wr::DisplayListBuilder& aBuilder,
                                         const wr::LayoutRect& aBounds,
                                         const wr::LayoutRect& aClip,
                                         wr::ImageRendering aFilter,
                                         const Range<wr::ImageKey>& aImageKeys,
                                         PushDisplayItemFlagSet aFlags) {
  // SWGL should always try to bypass shaders and composite directly.
  bool preferCompositorSurface =
      aFlags.contains(PushDisplayItemFlag::PREFER_COMPOSITOR_SURFACE);
  bool useExternalSurface =
      aFlags.contains(PushDisplayItemFlag::SUPPORTS_EXTERNAL_BUFFER_TEXTURES);
  if (GetFormat() != gfx::SurfaceFormat::YUV) {
    MOZ_ASSERT(aImageKeys.length() == 1);
    aBuilder.PushImage(aBounds, aClip, true, aFilter, aImageKeys[0],
                       !(mFlags & TextureFlags::NON_PREMULTIPLIED),
                       wr::ColorF{1.0f, 1.0f, 1.0f, 1.0f},
                       preferCompositorSurface, useExternalSurface);
  } else {
    MOZ_ASSERT(aImageKeys.length() == 3);
    const YCbCrDescriptor& desc = mDescriptor.get_YCbCrDescriptor();
    aBuilder.PushYCbCrPlanarImage(
        aBounds, aClip, true, aImageKeys[0], aImageKeys[1], aImageKeys[2],
        wr::ToWrColorDepth(desc.colorDepth()),
        wr::ToWrYuvColorSpace(desc.yUVColorSpace()),
        wr::ToWrColorRange(desc.colorRange()), aFilter, preferCompositorSurface,
        useExternalSurface);
  }
}

void TextureHost::DeserializeReadLock(const ReadLockDescriptor& aDesc,
                                      ISurfaceAllocator* aAllocator) {
  if (mReadLock) {
    return;
  }

  mReadLock = TextureReadLock::Deserialize(aDesc, aAllocator);
}

void TextureHost::SetReadLocked() {
  if (!mReadLock) {
    return;
  }
  // If mReadLocked is true it means we haven't read unlocked yet and the
  // content side should not have been able to write into this texture and read
  // lock again!
  MOZ_ASSERT(!mReadLocked);
  mReadLocked = true;
  if (mProvider) {
    mProvider->MaybeUnlockBeforeNextComposition(this);
  }
}

void TextureHost::ReadUnlock() {
  if (mReadLock && mReadLocked) {
    mReadLock->ReadUnlock();
    mReadLocked = false;
  }
}

bool TextureHost::NeedsYFlip() const {
  return bool(mFlags & TextureFlags::ORIGIN_BOTTOM_LEFT);
}

bool BufferTextureHost::EnsureWrappingTextureSource() {
  MOZ_ASSERT(!mHasIntermediateBuffer);

  if (mFirstSource && mFirstSource->IsOwnedBy(this)) {
    return true;
  }
  // We don't own it, apparently.
  if (mFirstSource) {
    mNeedsFullUpdate = true;
    mFirstSource = nullptr;
  }

  if (!mProvider) {
    return false;
  }

  if (mFormat == gfx::SurfaceFormat::YUV) {
    mFirstSource = mProvider->CreateDataTextureSourceAroundYCbCr(this);
  } else {
    uint8_t* data = GetBuffer();
    if (!data) {
      return false;
    }
    RefPtr<gfx::DataSourceSurface> surf =
        gfx::Factory::CreateWrappingDataSourceSurface(
            data, ImageDataSerializer::ComputeRGBStride(mFormat, mSize.width),
            mSize, mFormat);
    if (!surf) {
      return false;
    }
    mFirstSource = mProvider->CreateDataTextureSourceAround(surf);
  }

  if (!mFirstSource) {
    // BasicCompositor::CreateDataTextureSourceAround never returns null
    // and we don't expect to take this branch if we are using another backend.
    // Returning false is fine but if we get into this situation it probably
    // means something fishy is going on, like a texture being used with
    // several compositor backends.
    NS_WARNING("Failed to use a BufferTextureHost without intermediate buffer");
    return false;
  }

  mFirstSource->SetUpdateSerial(mUpdateSerial);
  mFirstSource->SetOwner(this);

  return true;
}

static bool IsCompatibleTextureSource(TextureSource* aTexture,
                                      const BufferDescriptor& aDescriptor,
                                      TextureSourceProvider* aProvider) {
  if (!aProvider) {
    return false;
  }

  switch (aDescriptor.type()) {
    case BufferDescriptor::TYCbCrDescriptor: {
      const YCbCrDescriptor& ycbcr = aDescriptor.get_YCbCrDescriptor();

      if (!aProvider->SupportsEffect(EffectTypes::YCBCR)) {
        return aTexture->GetFormat() == gfx::SurfaceFormat::B8G8R8X8 &&
               aTexture->GetSize() == ycbcr.ySize();
      }

      if (aTexture->GetFormat() != gfx::SurfaceFormat::A8 ||
          aTexture->GetSize() != ycbcr.ySize()) {
        return false;
      }

      auto cbTexture = aTexture->GetSubSource(1);
      if (!cbTexture || cbTexture->GetFormat() != gfx::SurfaceFormat::A8 ||
          cbTexture->GetSize() != ycbcr.cbCrSize()) {
        return false;
      }

      auto crTexture = aTexture->GetSubSource(2);
      if (!crTexture || crTexture->GetFormat() != gfx::SurfaceFormat::A8 ||
          crTexture->GetSize() != ycbcr.cbCrSize()) {
        return false;
      }

      return true;
    }
    case BufferDescriptor::TRGBDescriptor: {
      const RGBDescriptor& rgb = aDescriptor.get_RGBDescriptor();
      return aTexture->GetFormat() == rgb.format() &&
             aTexture->GetSize() == rgb.size();
    }
    default: {
      return false;
    }
  }
}

void BufferTextureHost::PrepareTextureSource(
    CompositableTextureSourceRef& aTexture) {
  if (!mHasIntermediateBuffer) {
    EnsureWrappingTextureSource();
  }

  if (mFirstSource && mFirstSource->IsOwnedBy(this)) {
    // We are already attached to a TextureSource, nothing to do except tell
    // the compositable to use it.
    aTexture = mFirstSource.get();
    return;
  }

  // We don't own it, apparently.
  if (mFirstSource) {
    mNeedsFullUpdate = true;
    mFirstSource = nullptr;
  }

  DataTextureSource* texture =
      aTexture.get() ? aTexture->AsDataTextureSource() : nullptr;

  bool compatibleFormats =
      texture && IsCompatibleTextureSource(texture, mDescriptor, mProvider);

  bool shouldCreateTexture = !compatibleFormats ||
                             texture->NumCompositableRefs() > 1 ||
                             texture->HasOwner();

  if (!shouldCreateTexture) {
    mFirstSource = texture;
    mFirstSource->SetOwner(this);
    mNeedsFullUpdate = true;

    // It's possible that texture belonged to a different compositor,
    // so make sure we update it (and all of its siblings) to the
    // current one.
    RefPtr<TextureSource> it = mFirstSource;
    while (it) {
      it->SetTextureSourceProvider(mProvider);
      it = it->GetNextSibling();
    }
  }
}

bool BufferTextureHost::BindTextureSource(
    CompositableTextureSourceRef& aTexture) {
  MOZ_ASSERT(mLocked);
  MOZ_ASSERT(mFirstSource);
  aTexture = mFirstSource;
  return !!aTexture;
}

bool BufferTextureHost::AcquireTextureSource(
    CompositableTextureSourceRef& aTexture) {
  if (!UploadIfNeeded()) {
    return false;
  }
  aTexture = mFirstSource;
  return !!mFirstSource;
}

void BufferTextureHost::ReadUnlock() {
  if (mFirstSource) {
    mFirstSource->Sync(true);
  }

  TextureHost::ReadUnlock();
}

void BufferTextureHost::MaybeNotifyUnlocked() {
#ifdef XP_DARWIN
  auto actor = GetIPDLActor();
  if (actor) {
    AutoTArray<uint64_t, 1> serials;
    serials.AppendElement(TextureHost::GetTextureSerial(actor));
    TextureSync::SetTexturesUnlocked(actor->OtherPid(), serials);
  }
#endif
}

void BufferTextureHost::UnbindTextureSource() {
  if (mFirstSource && mFirstSource->IsOwnedBy(this)) {
    mFirstSource->Unbind();
  }

  // This texture is not used by any layer anymore.
  // If the texture doesn't have an intermediate buffer, it means we are
  // compositing synchronously on the CPU, so we don't need to wait until
  // the end of the next composition to ReadUnlock (which other textures do
  // by default).
  // If the texture has an intermediate buffer we don't care either because
  // texture uploads are also performed synchronously for BufferTextureHost.
  ReadUnlock();
  MaybeNotifyUnlocked();
}

gfx::SurfaceFormat BufferTextureHost::GetFormat() const {
  // mFormat is the format of the data that we share with the content process.
  // GetFormat, on the other hand, expects the format that we present to the
  // Compositor (it is used to choose the effect type).
  // if the compositor does not support YCbCr effects, we give it a RGBX texture
  // instead (see BufferTextureHost::Upload)
  if (mFormat == gfx::SurfaceFormat::YUV && mProvider &&
      !mProvider->SupportsEffect(EffectTypes::YCBCR)) {
    return gfx::SurfaceFormat::R8G8B8X8;
  }
  return mFormat;
}

gfx::YUVColorSpace BufferTextureHost::GetYUVColorSpace() const {
  if (mFormat == gfx::SurfaceFormat::YUV) {
    const YCbCrDescriptor& desc = mDescriptor.get_YCbCrDescriptor();
    return desc.yUVColorSpace();
  }
  return gfx::YUVColorSpace::Identity;
}

gfx::ColorDepth BufferTextureHost::GetColorDepth() const {
  if (mFormat == gfx::SurfaceFormat::YUV) {
    const YCbCrDescriptor& desc = mDescriptor.get_YCbCrDescriptor();
    return desc.colorDepth();
  }
  return gfx::ColorDepth::COLOR_8;
}

gfx::ColorRange BufferTextureHost::GetColorRange() const {
  if (mFormat == gfx::SurfaceFormat::YUV) {
    const YCbCrDescriptor& desc = mDescriptor.get_YCbCrDescriptor();
    return desc.colorRange();
  }
  return TextureHost::GetColorRange();
}

bool BufferTextureHost::UploadIfNeeded() {
  return MaybeUpload(!mNeedsFullUpdate ? &mMaybeUpdatedRegion : nullptr);
}

bool BufferTextureHost::MaybeUpload(nsIntRegion* aRegion) {
  auto serial = mFirstSource ? mFirstSource->GetUpdateSerial() : 0;

  if (serial == mUpdateSerial) {
    return true;
  }

  if (serial == 0) {
    // 0 means the source has no valid content
    aRegion = nullptr;
  }

  if (!Upload(aRegion)) {
    return false;
  }

  if (mHasIntermediateBuffer) {
    // We just did the texture upload, the content side can now freely write
    // into the shared buffer.
    ReadUnlock();
    MaybeNotifyUnlocked();
  }

  // We no longer have an invalid region.
  mNeedsFullUpdate = false;
  mMaybeUpdatedRegion.SetEmpty();

  // If upload returns true we know mFirstSource is not null
  mFirstSource->SetUpdateSerial(mUpdateSerial);
  return true;
}

bool BufferTextureHost::Upload(nsIntRegion* aRegion) {
  uint8_t* buf = GetBuffer();
  if (!buf) {
    // We don't have a buffer; a possible cause is that the IPDL actor
    // is already dead. This inevitably happens as IPDL actors can die
    // at any time, so we want to silently return in this case.
    // another possible cause is that IPDL failed to map the shmem when
    // deserializing it.
    return false;
  }
  if (!mProvider) {
    // This can happen if we send textures to a compositable that isn't yet
    // attached to a layer.
    return false;
  }
  if (!mHasIntermediateBuffer && EnsureWrappingTextureSource()) {
    if (!mFirstSource || !mFirstSource->IsDirectMap()) {
      return true;
    }
  }

  if (mFormat == gfx::SurfaceFormat::UNKNOWN) {
    NS_WARNING("BufferTextureHost: unsupported format!");
    return false;
  } else if (mFormat == gfx::SurfaceFormat::YUV) {
    const YCbCrDescriptor& desc = mDescriptor.get_YCbCrDescriptor();

    if (!mProvider->SupportsEffect(EffectTypes::YCBCR)) {
      RefPtr<gfx::DataSourceSurface> surf =
          ImageDataSerializer::DataSourceSurfaceFromYCbCrDescriptor(
              buf, mDescriptor.get_YCbCrDescriptor());
      if (NS_WARN_IF(!surf)) {
        return false;
      }
      if (!mFirstSource) {
        mFirstSource = mProvider->CreateDataTextureSource(
            mFlags | TextureFlags::RGB_FROM_YCBCR);
        mFirstSource->SetOwner(this);
      }
      return mFirstSource->Update(surf, aRegion);
    }

    RefPtr<DataTextureSource> srcY;
    RefPtr<DataTextureSource> srcU;
    RefPtr<DataTextureSource> srcV;
    if (!mFirstSource) {
      // We don't support BigImages for YCbCr compositing.
      srcY = mProvider->CreateDataTextureSource(
          mFlags | TextureFlags::DISALLOW_BIGIMAGE);
      srcU = mProvider->CreateDataTextureSource(
          mFlags | TextureFlags::DISALLOW_BIGIMAGE);
      srcV = mProvider->CreateDataTextureSource(
          mFlags | TextureFlags::DISALLOW_BIGIMAGE);
      mFirstSource = srcY;
      mFirstSource->SetOwner(this);
      srcY->SetNextSibling(srcU);
      srcU->SetNextSibling(srcV);
    } else {
      // mFormat never changes so if this was created as a YCbCr host and
      // already contains a source it should already have 3 sources.
      // BufferTextureHost only uses DataTextureSources so it is safe to assume
      // all 3 sources are DataTextureSource.
      MOZ_ASSERT(mFirstSource->GetNextSibling());
      MOZ_ASSERT(mFirstSource->GetNextSibling()->GetNextSibling());
      srcY = mFirstSource;
      srcU = mFirstSource->GetNextSibling()->AsDataTextureSource();
      srcV = mFirstSource->GetNextSibling()
                 ->GetNextSibling()
                 ->AsDataTextureSource();
    }

    RefPtr<gfx::DataSourceSurface> tempY =
        gfx::Factory::CreateWrappingDataSourceSurface(
            ImageDataSerializer::GetYChannel(buf, desc), desc.yStride(),
            desc.ySize(), SurfaceFormatForColorDepth(desc.colorDepth()));
    RefPtr<gfx::DataSourceSurface> tempCb =
        gfx::Factory::CreateWrappingDataSourceSurface(
            ImageDataSerializer::GetCbChannel(buf, desc), desc.cbCrStride(),
            desc.cbCrSize(), SurfaceFormatForColorDepth(desc.colorDepth()));
    RefPtr<gfx::DataSourceSurface> tempCr =
        gfx::Factory::CreateWrappingDataSourceSurface(
            ImageDataSerializer::GetCrChannel(buf, desc), desc.cbCrStride(),
            desc.cbCrSize(), SurfaceFormatForColorDepth(desc.colorDepth()));
    // We don't support partial updates for Y U V textures
    NS_ASSERTION(!aRegion, "Unsupported partial updates for YCbCr textures");
    if (!tempY || !tempCb || !tempCr || !srcY->Update(tempY) ||
        !srcU->Update(tempCb) || !srcV->Update(tempCr)) {
      NS_WARNING("failed to update the DataTextureSource");
      return false;
    }
  } else {
    // non-YCbCr case
    nsIntRegion* regionToUpdate = aRegion;
    if (!mFirstSource) {
      mFirstSource = mProvider->CreateDataTextureSource(mFlags);
      mFirstSource->SetOwner(this);
      if (mFlags & TextureFlags::COMPONENT_ALPHA) {
        // Update the full region the first time for component alpha textures.
        regionToUpdate = nullptr;
      }
    }

    RefPtr<gfx::DataSourceSurface> surf =
        gfx::Factory::CreateWrappingDataSourceSurface(
            GetBuffer(),
            ImageDataSerializer::ComputeRGBStride(mFormat, mSize.width), mSize,
            mFormat);
    if (!surf) {
      return false;
    }

    if (!mFirstSource->Update(surf.get(), regionToUpdate)) {
      NS_WARNING("failed to update the DataTextureSource");
      return false;
    }
  }
  MOZ_ASSERT(mFirstSource);
  return true;
}

already_AddRefed<gfx::DataSourceSurface> BufferTextureHost::GetAsSurface() {
  RefPtr<gfx::DataSourceSurface> result;
  if (mFormat == gfx::SurfaceFormat::UNKNOWN) {
    NS_WARNING("BufferTextureHost: unsupported format!");
    return nullptr;
  } else if (mFormat == gfx::SurfaceFormat::YUV) {
    result = ImageDataSerializer::DataSourceSurfaceFromYCbCrDescriptor(
        GetBuffer(), mDescriptor.get_YCbCrDescriptor());
    if (NS_WARN_IF(!result)) {
      return nullptr;
    }
  } else {
    result = gfx::Factory::CreateWrappingDataSourceSurface(
        GetBuffer(),
        ImageDataSerializer::GetRGBStride(mDescriptor.get_RGBDescriptor()),
        mSize, mFormat);
  }
  return result.forget();
}

ShmemTextureHost::ShmemTextureHost(const ipc::Shmem& aShmem,
                                   const BufferDescriptor& aDesc,
                                   ISurfaceAllocator* aDeallocator,
                                   TextureFlags aFlags)
    : BufferTextureHost(aDesc, aFlags), mDeallocator(aDeallocator) {
  if (aShmem.IsReadable()) {
    mShmem = MakeUnique<ipc::Shmem>(aShmem);
  } else {
    // This can happen if we failed to map the shmem on this process, perhaps
    // because it was big and we didn't have enough contiguous address space
    // available, even though we did on the child process.
    // As a result this texture will be in an invalid state and Lock will
    // always fail.

    gfxCriticalNote << "Failed to create a valid ShmemTextureHost";
  }

  MOZ_COUNT_CTOR(ShmemTextureHost);
}

ShmemTextureHost::~ShmemTextureHost() {
  MOZ_ASSERT(!mShmem || (mFlags & TextureFlags::DEALLOCATE_CLIENT),
             "Leaking our buffer");
  DeallocateDeviceData();
  MOZ_COUNT_DTOR(ShmemTextureHost);
}

void ShmemTextureHost::DeallocateSharedData() {
  if (mShmem) {
    MOZ_ASSERT(mDeallocator,
               "Shared memory would leak without a ISurfaceAllocator");
    mDeallocator->AsShmemAllocator()->DeallocShmem(*mShmem);
    mShmem = nullptr;
  }
}

void ShmemTextureHost::ForgetSharedData() {
  if (mShmem) {
    mShmem = nullptr;
  }
}

void ShmemTextureHost::OnShutdown() { mShmem = nullptr; }

uint8_t* ShmemTextureHost::GetBuffer() {
  return mShmem ? mShmem->get<uint8_t>() : nullptr;
}

size_t ShmemTextureHost::GetBufferSize() {
  return mShmem ? mShmem->Size<uint8_t>() : 0;
}

MemoryTextureHost::MemoryTextureHost(uint8_t* aBuffer,
                                     const BufferDescriptor& aDesc,
                                     TextureFlags aFlags)
    : BufferTextureHost(aDesc, aFlags), mBuffer(aBuffer) {
  MOZ_COUNT_CTOR(MemoryTextureHost);
}

MemoryTextureHost::~MemoryTextureHost() {
  MOZ_ASSERT(!mBuffer || (mFlags & TextureFlags::DEALLOCATE_CLIENT),
             "Leaking our buffer");
  DeallocateDeviceData();
  MOZ_COUNT_DTOR(MemoryTextureHost);
}

void MemoryTextureHost::DeallocateSharedData() {
  if (mBuffer) {
    GfxMemoryImageReporter::WillFree(mBuffer);
  }
  delete[] mBuffer;
  mBuffer = nullptr;
}

void MemoryTextureHost::ForgetSharedData() { mBuffer = nullptr; }

uint8_t* MemoryTextureHost::GetBuffer() { return mBuffer; }

size_t MemoryTextureHost::GetBufferSize() {
  // MemoryTextureHost just trusts that the buffer size is large enough to read
  // anything we need to. That's because MemoryTextureHost has to trust the
  // buffer pointer anyway, so the security model here is just that
  // MemoryTexture's are restricted to same-process clients.
  return std::numeric_limits<size_t>::max();
}

TextureParent::TextureParent(HostIPCAllocator* aSurfaceAllocator,
                             uint64_t aSerial,
                             const wr::MaybeExternalImageId& aExternalImageId)
    : mSurfaceAllocator(aSurfaceAllocator),
      mSerial(aSerial),
      mExternalImageId(aExternalImageId) {
  MOZ_COUNT_CTOR(TextureParent);
}

TextureParent::~TextureParent() { MOZ_COUNT_DTOR(TextureParent); }

void TextureParent::NotifyNotUsed(uint64_t aTransactionId) {
  if (!mTextureHost) {
    return;
  }
  mSurfaceAllocator->NotifyNotUsed(this, aTransactionId);
}

bool TextureParent::Init(const SurfaceDescriptor& aSharedData,
                         const ReadLockDescriptor& aReadLock,
                         const LayersBackend& aBackend,
                         const TextureFlags& aFlags) {
  mTextureHost = TextureHost::Create(aSharedData, aReadLock, mSurfaceAllocator,
                                     aBackend, aFlags, mExternalImageId);
  if (mTextureHost) {
    mTextureHost->mActor = this;
  }

  return !!mTextureHost;
}

void TextureParent::Destroy() {
  if (!mTextureHost) {
    return;
  }

  if (mTextureHost->mReadLocked) {
    // ReadUnlock here to make sure the ReadLock's shmem does not outlive the
    // protocol that created it.
    mTextureHost->ReadUnlock();
    mTextureHost->MaybeNotifyUnlocked();
  }

  if (mTextureHost->GetFlags() & TextureFlags::DEALLOCATE_CLIENT) {
    mTextureHost->ForgetSharedData();
  }

  mTextureHost->mActor = nullptr;
  mTextureHost = nullptr;
}

void TextureHost::ReceivedDestroy(PTextureParent* aActor) {
  static_cast<TextureParent*>(aActor)->RecvDestroy();
}

mozilla::ipc::IPCResult TextureParent::RecvRecycleTexture(
    const TextureFlags& aTextureFlags) {
  if (!mTextureHost) {
    return IPC_OK();
  }
  mTextureHost->RecycleTexture(aTextureFlags);
  return IPC_OK();
}

////////////////////////////////////////////////////////////////////////////////

}  // namespace layers
}  // namespace mozilla
