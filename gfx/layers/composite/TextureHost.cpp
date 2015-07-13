/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureHost.h"

#include "CompositableHost.h"           // for CompositableHost
#include "LayersLogging.h"              // for AppendToString
#include "mozilla/gfx/2D.h"             // for DataSourceSurface, Factory
#include "mozilla/ipc/Shmem.h"          // for Shmem
#include "mozilla/layers/CompositableTransactionParent.h" // for CompositableParentManager
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/ISurfaceAllocator.h"  // for ISurfaceAllocator
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor, etc
#include "mozilla/layers/TextureHostOGL.h"  // for TextureHostOGL
#include "mozilla/layers/YCbCrImageDataSerializer.h"
#include "nsAString.h"
#include "nsRefPtr.h"                   // for nsRefPtr
#include "nsPrintfCString.h"            // for nsPrintfCString
#include "mozilla/layers/PTextureParent.h"
#include "mozilla/unused.h"
#include <limits>
#include "../opengl/CompositorOGL.h"
#include "gfxUtils.h"

#ifdef MOZ_ENABLE_D3D10_LAYER
#include "../d3d11/CompositorD3D11.h"
#endif

#ifdef MOZ_WIDGET_GONK
#include "../opengl/GrallocTextureClient.h"
#include "../opengl/GrallocTextureHost.h"
#endif

#ifdef MOZ_X11
#include "mozilla/layers/X11TextureHost.h"
#endif

#ifdef XP_MACOSX
#include "../opengl/MacIOSurfaceTextureHostOGL.h"
#endif

#ifdef XP_WIN
#include "mozilla/layers/TextureDIB.h"
#endif

#if 0
#define RECYCLE_LOG(...) printf_stderr(__VA_ARGS__)
#else
#define RECYCLE_LOG(...) do { } while (0)
#endif

namespace mozilla {
namespace layers {

/**
 * TextureParent is the host-side IPDL glue between TextureClient and TextureHost.
 * It is an IPDL actor just like LayerParent, CompositableParent, etc.
 */
class TextureParent : public PTextureParent
{
public:
  explicit TextureParent(CompositableParentManager* aManager);

  ~TextureParent();

  bool Init(const SurfaceDescriptor& aSharedData,
            const TextureFlags& aFlags);

  void CompositorRecycle();

  virtual bool RecvClientRecycle() override;

  virtual bool RecvClearTextureHostSync() override;

  virtual bool RecvRemoveTexture() override;

  virtual bool RecvRecycleTexture(const TextureFlags& aTextureFlags) override;

  TextureHost* GetTextureHost() { return mTextureHost; }

  void ActorDestroy(ActorDestroyReason why) override;

  void ClearTextureHost();

  CompositableParentManager* mCompositableManager;
  RefPtr<TextureHost> mWaitForClientRecycle;
  RefPtr<TextureHost> mTextureHost;
};

////////////////////////////////////////////////////////////////////////////////

// static
PTextureParent*
TextureHost::CreateIPDLActor(CompositableParentManager* aManager,
                             const SurfaceDescriptor& aSharedData,
                             TextureFlags aFlags)
{
  if (aSharedData.type() == SurfaceDescriptor::TSurfaceDescriptorMemory &&
      !aManager->IsSameProcess())
  {
    NS_ERROR("A client process is trying to peek at our address space using a MemoryTexture!");
    return nullptr;
  }
  TextureParent* actor = new TextureParent(aManager);
  if (!actor->Init(aSharedData, aFlags)) {
    delete actor;
    return nullptr;
  }
  return actor;
}

// static
bool
TextureHost::DestroyIPDLActor(PTextureParent* actor)
{
  delete actor;
  return true;
}

// static
bool
TextureHost::SendDeleteIPDLActor(PTextureParent* actor)
{
  return PTextureParent::Send__delete__(actor);
}

// static
TextureHost*
TextureHost::AsTextureHost(PTextureParent* actor)
{
  return actor? static_cast<TextureParent*>(actor)->mTextureHost : nullptr;
}

PTextureParent*
TextureHost::GetIPDLActor()
{
  return mActor;
}

bool
TextureHost::SetReleaseFenceHandle(const FenceHandle& aReleaseFenceHandle)
{
  if (!aReleaseFenceHandle.IsValid()) {
    // HWC might not provide Fence.
    // In this case, HWC implicitly handles buffer's fence.
    return false;
  }

  mReleaseFenceHandle.Merge(aReleaseFenceHandle);

  return true;
}

FenceHandle
TextureHost::GetAndResetReleaseFenceHandle()
{
  FenceHandle fence;
  mReleaseFenceHandle.TransferToAnotherFenceHandle(fence);
  return fence;
}

void
TextureHost::SetAcquireFenceHandle(const FenceHandle& aAcquireFenceHandle)
{
  mAcquireFenceHandle = aAcquireFenceHandle;
}

FenceHandle
TextureHost::GetAndResetAcquireFenceHandle()
{
  nsRefPtr<FenceHandle::FdObj> fdObj = mAcquireFenceHandle.GetAndResetFdObj();
  return FenceHandle(fdObj);
}

// implemented in TextureHostOGL.cpp
already_AddRefed<TextureHost> CreateTextureHostOGL(const SurfaceDescriptor& aDesc,
                                               ISurfaceAllocator* aDeallocator,
                                               TextureFlags aFlags);

// implemented in TextureHostBasic.cpp
already_AddRefed<TextureHost> CreateTextureHostBasic(const SurfaceDescriptor& aDesc,
                                                 ISurfaceAllocator* aDeallocator,
                                                 TextureFlags aFlags);

// implemented in TextureD3D11.cpp
already_AddRefed<TextureHost> CreateTextureHostD3D11(const SurfaceDescriptor& aDesc,
                                                 ISurfaceAllocator* aDeallocator,
                                                 TextureFlags aFlags);

// implemented in TextureD3D9.cpp
already_AddRefed<TextureHost> CreateTextureHostD3D9(const SurfaceDescriptor& aDesc,
                                                ISurfaceAllocator* aDeallocator,
                                                TextureFlags aFlags);

// static
already_AddRefed<TextureHost>
TextureHost::Create(const SurfaceDescriptor& aDesc,
                    ISurfaceAllocator* aDeallocator,
                    TextureFlags aFlags)
{
  switch (aDesc.type()) {
    case SurfaceDescriptor::TSurfaceDescriptorShmem:
    case SurfaceDescriptor::TSurfaceDescriptorMemory:
    case SurfaceDescriptor::TSurfaceDescriptorDIB:
    case SurfaceDescriptor::TSurfaceDescriptorFileMapping:
      return CreateBackendIndependentTextureHost(aDesc, aDeallocator, aFlags);

    case SurfaceDescriptor::TEGLImageDescriptor:
    case SurfaceDescriptor::TNewSurfaceDescriptorGralloc:
    case SurfaceDescriptor::TSurfaceTextureDescriptor:
      return CreateTextureHostOGL(aDesc, aDeallocator, aFlags);

    case SurfaceDescriptor::TSurfaceDescriptorMacIOSurface:
      if (Compositor::GetBackend() == LayersBackend::LAYERS_OPENGL) {
        return CreateTextureHostOGL(aDesc, aDeallocator, aFlags);
      } else {
        return CreateTextureHostBasic(aDesc, aDeallocator, aFlags);
      }

#ifdef MOZ_X11
    case SurfaceDescriptor::TSurfaceDescriptorX11: {
      const SurfaceDescriptorX11& desc = aDesc.get_SurfaceDescriptorX11();
      return MakeAndAddRef<X11TextureHost>(aFlags, desc);
    }
#endif

#ifdef XP_WIN
    case SurfaceDescriptor::TSurfaceDescriptorD3D9:
      return CreateTextureHostD3D9(aDesc, aDeallocator, aFlags);

    case SurfaceDescriptor::TSurfaceDescriptorD3D10:
    case SurfaceDescriptor::TSurfaceDescriptorDXGIYCbCr:
      if (Compositor::GetBackend() == LayersBackend::LAYERS_D3D9) {
        return CreateTextureHostD3D9(aDesc, aDeallocator, aFlags);
      } else {
        return CreateTextureHostD3D11(aDesc, aDeallocator, aFlags);
      }
#endif
    default:
      MOZ_CRASH("Unsupported Surface type");
  }
}

already_AddRefed<TextureHost>
CreateBackendIndependentTextureHost(const SurfaceDescriptor& aDesc,
                                    ISurfaceAllocator* aDeallocator,
                                    TextureFlags aFlags)
{
  RefPtr<TextureHost> result;
  switch (aDesc.type()) {
    case SurfaceDescriptor::TSurfaceDescriptorShmem: {
      const SurfaceDescriptorShmem& descriptor = aDesc.get_SurfaceDescriptorShmem();
      result = new ShmemTextureHost(descriptor.data(),
                                    descriptor.format(),
                                    aDeallocator,
                                    aFlags);
      break;
    }
    case SurfaceDescriptor::TSurfaceDescriptorMemory: {
      const SurfaceDescriptorMemory& descriptor = aDesc.get_SurfaceDescriptorMemory();
      result = new MemoryTextureHost(reinterpret_cast<uint8_t*>(descriptor.data()),
                                     descriptor.format(),
                                     aFlags);
      break;
    }
#ifdef XP_WIN
    case SurfaceDescriptor::TSurfaceDescriptorDIB: {
      result = new DIBTextureHost(aFlags, aDesc);
      break;
    }
    case SurfaceDescriptor::TSurfaceDescriptorFileMapping: {
      result = new TextureHostFileMapping(aFlags, aDesc);
      break;
    }
#endif
    default: {
      NS_WARNING("No backend independent TextureHost for this descriptor type");
    }
  }
  return result.forget();
}

void
TextureHost::CompositorRecycle()
{
  if (!mActor) {
    return;
  }
  static_cast<TextureParent*>(mActor)->CompositorRecycle();
}

TextureHost::TextureHost(TextureFlags aFlags)
    : mActor(nullptr)
    , mFlags(aFlags)
    , mCompositableCount(0)
{}

TextureHost::~TextureHost()
{
}

void TextureHost::Finalize()
{
  if (!(GetFlags() & TextureFlags::DEALLOCATE_CLIENT)) {
    DeallocateSharedData();
    DeallocateDeviceData();
  }
}

void
TextureHost::RecycleTexture(TextureFlags aFlags)
{
  MOZ_ASSERT(GetFlags() & TextureFlags::RECYCLE);
  MOZ_ASSERT(aFlags & TextureFlags::RECYCLE);
  MOZ_ASSERT(!HasRecycleCallback());
  mFlags = aFlags;
}

void
TextureHost::PrintInfo(std::stringstream& aStream, const char* aPrefix)
{
  aStream << aPrefix;
  aStream << nsPrintfCString("%s (0x%p)", Name(), this).get();
  // Note: the TextureHost needs to be locked before it is safe to call
  //       GetSize() and GetFormat() on it.
  if (Lock()) {
    AppendToString(aStream, GetSize(), " [size=", "]");
    AppendToString(aStream, GetFormat(), " [format=", "]");
    Unlock();
  }
  AppendToString(aStream, mFlags, " [flags=", "]");
#ifdef MOZ_DUMP_PAINTING
  if (gfxPrefs::LayersDumpTexture() || profiler_feature_active("layersdump")) {
    nsAutoCString pfx(aPrefix);
    pfx += "  ";

    aStream << "\n" << pfx.get() << "Surface: ";
    RefPtr<gfx::DataSourceSurface> dSurf = GetAsSurface();
    if (dSurf) {
      aStream << gfxUtils::GetAsLZ4Base64Str(dSurf).get();
    }
  }
#endif
}

void
TextureHost::Updated(const nsIntRegion* aRegion)
{
    LayerScope::ContentChanged(this);
    UpdatedInternal(aRegion);
}

TextureSource::TextureSource()
: mCompositableCount(0)
{
    MOZ_COUNT_CTOR(TextureSource);
}

TextureSource::~TextureSource()
{
    MOZ_COUNT_DTOR(TextureSource);
}

BufferTextureHost::BufferTextureHost(gfx::SurfaceFormat aFormat,
                                     TextureFlags aFlags)
: TextureHost(aFlags)
, mCompositor(nullptr)
, mFormat(aFormat)
, mUpdateSerial(1)
, mLocked(false)
, mNeedsFullUpdate(false)
{
  if (aFlags & TextureFlags::COMPONENT_ALPHA) {
    // One texture of a component alpha texture pair will start out all white.
    // This hack allows us to easily make sure that white will be uploaded.
    // See bug 1138934
    mNeedsFullUpdate = true;
  }
}

void
BufferTextureHost::InitSize()
{
  if (mFormat == gfx::SurfaceFormat::YUV) {
    YCbCrImageDataDeserializer yuvDeserializer(GetBuffer(), GetBufferSize());
    if (yuvDeserializer.IsValid()) {
      mSize = yuvDeserializer.GetYSize();
    }
  } else if (mFormat != gfx::SurfaceFormat::UNKNOWN) {
    ImageDataDeserializer deserializer(GetBuffer(), GetBufferSize());
    if (deserializer.IsValid()) {
      mSize = deserializer.GetSize();
    }
  }
}

BufferTextureHost::~BufferTextureHost()
{}

void
BufferTextureHost::UpdatedInternal(const nsIntRegion* aRegion)
{
  ++mUpdateSerial;
  // If the last frame wasn't uploaded yet, and we -don't- have a partial update,
  // we still need to update the full surface.
  if (aRegion && !mNeedsFullUpdate) {
    mMaybeUpdatedRegion = mMaybeUpdatedRegion.Or(mMaybeUpdatedRegion, *aRegion);
  } else {
    mNeedsFullUpdate = true;
  }
  if (GetFlags() & TextureFlags::IMMEDIATE_UPLOAD) {
    DebugOnly<bool> result = MaybeUpload(!mNeedsFullUpdate ? &mMaybeUpdatedRegion : nullptr);
    NS_WARN_IF_FALSE(result, "Failed to upload a texture");
  }
}

void
BufferTextureHost::SetCompositor(Compositor* aCompositor)
{
  MOZ_ASSERT(aCompositor);
  if (mCompositor == aCompositor) {
    return;
  }
  RefPtr<TextureSource> it = mFirstSource;
  while (it) {
    it->SetCompositor(aCompositor);
    it = it->GetNextSibling();
  }
  mFirstSource = nullptr;
  mCompositor = aCompositor;
}

void
BufferTextureHost::DeallocateDeviceData()
{
  RefPtr<TextureSource> it = mFirstSource;
  while (it) {
    it->DeallocateDeviceData();
    it = it->GetNextSibling();
  }
}

bool
BufferTextureHost::Lock()
{
  MOZ_ASSERT(!mLocked);
  if (!MaybeUpload(!mNeedsFullUpdate ? &mMaybeUpdatedRegion : nullptr)) {
      return false;
  }
  mLocked = !!mFirstSource;
  return mLocked;
}

void
BufferTextureHost::Unlock()
{
  MOZ_ASSERT(mLocked);
  mLocked = false;
}

bool
BufferTextureHost::BindTextureSource(CompositableTextureSourceRef& aTexture)
{
  MOZ_ASSERT(mLocked);
  MOZ_ASSERT(mFirstSource);
  aTexture = mFirstSource;
  return !!aTexture;
}

gfx::SurfaceFormat
BufferTextureHost::GetFormat() const
{
  // mFormat is the format of the data that we share with the content process.
  // GetFormat, on the other hand, expects the format that we present to the
  // Compositor (it is used to choose the effect type).
  // if the compositor does not support YCbCr effects, we give it a RGBX texture
  // instead (see BufferTextureHost::Upload)
  if (mFormat == gfx::SurfaceFormat::YUV &&
    mCompositor &&
    !mCompositor->SupportsEffect(EffectTypes::YCBCR)) {
    return gfx::SurfaceFormat::R8G8B8X8;
  }
  return mFormat;
}

bool
BufferTextureHost::MaybeUpload(nsIntRegion *aRegion)
{
  if (mFirstSource && mFirstSource->GetUpdateSerial() == mUpdateSerial) {
    return true;
  }
  if (!Upload(aRegion)) {
    return false;
  }

  // We no longer have an invalid region.
  mNeedsFullUpdate = false;
  mMaybeUpdatedRegion.SetEmpty();

  // If upload returns true we know mFirstSource is not null
  mFirstSource->SetUpdateSerial(mUpdateSerial);
  return true;
}

bool
BufferTextureHost::Upload(nsIntRegion *aRegion)
{
  if (!GetBuffer()) {
    // We don't have a buffer; a possible cause is that the IPDL actor
    // is already dead. This inevitably happens as IPDL actors can die
    // at any time, so we want to silently return in this case.
    return false;
  }
  if (!mCompositor) {
    // This can happen if we send textures to a compositable that isn't yet
    // attached to a layer.
    return false;
  }
  if (mFormat == gfx::SurfaceFormat::UNKNOWN) {
    NS_WARNING("BufferTextureHost: unsupported format!");
    return false;
  } else if (mFormat == gfx::SurfaceFormat::YUV) {
    YCbCrImageDataDeserializer yuvDeserializer(GetBuffer(), GetBufferSize());
    MOZ_ASSERT(yuvDeserializer.IsValid());

    if (!mCompositor->SupportsEffect(EffectTypes::YCBCR)) {
      RefPtr<gfx::DataSourceSurface> surf = yuvDeserializer.ToDataSourceSurface();
      if (NS_WARN_IF(!surf)) {
        return false;
      }
      if (!mFirstSource) {
        mFirstSource = mCompositor->CreateDataTextureSource(mFlags);
      }
      mFirstSource->Update(surf, aRegion);
      return true;
    }

    RefPtr<DataTextureSource> srcY;
    RefPtr<DataTextureSource> srcU;
    RefPtr<DataTextureSource> srcV;
    if (!mFirstSource) {
      // We don't support BigImages for YCbCr compositing.
      srcY = mCompositor->CreateDataTextureSource(mFlags|TextureFlags::DISALLOW_BIGIMAGE);
      srcU = mCompositor->CreateDataTextureSource(mFlags|TextureFlags::DISALLOW_BIGIMAGE);
      srcV = mCompositor->CreateDataTextureSource(mFlags|TextureFlags::DISALLOW_BIGIMAGE);
      mFirstSource = srcY;
      srcY->SetNextSibling(srcU);
      srcU->SetNextSibling(srcV);
    } else {
      // mFormat never changes so if this was created as a YCbCr host and already
      // contains a source it should already have 3 sources.
      // BufferTextureHost only uses DataTextureSources so it is safe to assume
      // all 3 sources are DataTextureSource.
      MOZ_ASSERT(mFirstSource->GetNextSibling());
      MOZ_ASSERT(mFirstSource->GetNextSibling()->GetNextSibling());
      srcY = mFirstSource;
      srcU = mFirstSource->GetNextSibling()->AsDataTextureSource();
      srcV = mFirstSource->GetNextSibling()->GetNextSibling()->AsDataTextureSource();
    }


    RefPtr<gfx::DataSourceSurface> tempY =
      gfx::Factory::CreateWrappingDataSourceSurface(yuvDeserializer.GetYData(),
                                                    yuvDeserializer.GetYStride(),
                                                    yuvDeserializer.GetYSize(),
                                                    gfx::SurfaceFormat::A8);
    RefPtr<gfx::DataSourceSurface> tempCb =
      gfx::Factory::CreateWrappingDataSourceSurface(yuvDeserializer.GetCbData(),
                                                    yuvDeserializer.GetCbCrStride(),
                                                    yuvDeserializer.GetCbCrSize(),
                                                    gfx::SurfaceFormat::A8);
    RefPtr<gfx::DataSourceSurface> tempCr =
      gfx::Factory::CreateWrappingDataSourceSurface(yuvDeserializer.GetCrData(),
                                                    yuvDeserializer.GetCbCrStride(),
                                                    yuvDeserializer.GetCbCrSize(),
                                                    gfx::SurfaceFormat::A8);
    // We don't support partial updates for Y U V textures
    NS_ASSERTION(!aRegion, "Unsupported partial updates for YCbCr textures");
    if (!tempY ||
        !tempCb ||
        !tempCr ||
        !srcY->Update(tempY) ||
        !srcU->Update(tempCb) ||
        !srcV->Update(tempCr)) {
      NS_WARNING("failed to update the DataTextureSource");
      return false;
    }
  } else {
    // non-YCbCr case
    if (!mFirstSource) {
      mFirstSource = mCompositor->CreateDataTextureSource();
    }
    ImageDataDeserializer deserializer(GetBuffer(), GetBufferSize());
    if (!deserializer.IsValid()) {
      NS_ERROR("Failed to deserialize image!");
      return false;
    }

    RefPtr<gfx::DataSourceSurface> surf = deserializer.GetAsSurface();
    if (!surf) {
      return false;
    }

    if (!mFirstSource->Update(surf.get(), aRegion)) {
      NS_WARNING("failed to update the DataTextureSource");
      return false;
    }
  }
  MOZ_ASSERT(mFirstSource);
  return true;
}

already_AddRefed<gfx::DataSourceSurface>
BufferTextureHost::GetAsSurface()
{
  RefPtr<gfx::DataSourceSurface> result;
  if (mFormat == gfx::SurfaceFormat::UNKNOWN) {
    NS_WARNING("BufferTextureHost: unsupported format!");
    return nullptr;
  } else if (mFormat == gfx::SurfaceFormat::YUV) {
    YCbCrImageDataDeserializer yuvDeserializer(GetBuffer(), GetBufferSize());
    if (!yuvDeserializer.IsValid()) {
      return nullptr;
    }
    result = yuvDeserializer.ToDataSourceSurface();
    if (NS_WARN_IF(!result)) {
      return nullptr;
    }
  } else {
    ImageDataDeserializer deserializer(GetBuffer(), GetBufferSize());
    if (!deserializer.IsValid()) {
      NS_ERROR("Failed to deserialize image!");
      return nullptr;
    }
    result = deserializer.GetAsSurface();
  }
  return result.forget();
}

ShmemTextureHost::ShmemTextureHost(const ipc::Shmem& aShmem,
                                   gfx::SurfaceFormat aFormat,
                                   ISurfaceAllocator* aDeallocator,
                                   TextureFlags aFlags)
: BufferTextureHost(aFormat, aFlags)
, mShmem(MakeUnique<ipc::Shmem>(aShmem))
, mDeallocator(aDeallocator)
{
  MOZ_COUNT_CTOR(ShmemTextureHost);
  InitSize();
}

ShmemTextureHost::~ShmemTextureHost()
{
  MOZ_ASSERT(!mShmem || (mFlags & TextureFlags::DEALLOCATE_CLIENT),
             "Leaking our buffer");
  DeallocateDeviceData();
  MOZ_COUNT_DTOR(ShmemTextureHost);
}

void
ShmemTextureHost::DeallocateSharedData()
{
  if (mShmem) {
    MOZ_ASSERT(mDeallocator,
               "Shared memory would leak without a ISurfaceAllocator");
    mDeallocator->DeallocShmem(*mShmem);
    mShmem = nullptr;
  }
}

void
ShmemTextureHost::ForgetSharedData()
{
  if (mShmem) {
    mShmem = nullptr;
  }
}

void
ShmemTextureHost::OnShutdown()
{
  mShmem = nullptr;
}

uint8_t* ShmemTextureHost::GetBuffer()
{
  return mShmem ? mShmem->get<uint8_t>() : nullptr;
}

size_t ShmemTextureHost::GetBufferSize()
{
  return mShmem ? mShmem->Size<uint8_t>() : 0;
}

MemoryTextureHost::MemoryTextureHost(uint8_t* aBuffer,
                                     gfx::SurfaceFormat aFormat,
                                     TextureFlags aFlags)
: BufferTextureHost(aFormat, aFlags)
, mBuffer(aBuffer)
{
  MOZ_COUNT_CTOR(MemoryTextureHost);
  InitSize();
}

MemoryTextureHost::~MemoryTextureHost()
{
  MOZ_ASSERT(!mBuffer || (mFlags & TextureFlags::DEALLOCATE_CLIENT),
             "Leaking our buffer");
  DeallocateDeviceData();
  MOZ_COUNT_DTOR(MemoryTextureHost);
}

void
MemoryTextureHost::DeallocateSharedData()
{
  if (mBuffer) {
    GfxMemoryImageReporter::WillFree(mBuffer);
  }
  delete[] mBuffer;
  mBuffer = nullptr;
}

void
MemoryTextureHost::ForgetSharedData()
{
  mBuffer = nullptr;
}

uint8_t* MemoryTextureHost::GetBuffer()
{
  return mBuffer;
}

size_t MemoryTextureHost::GetBufferSize()
{
  // MemoryTextureHost just trusts that the buffer size is large enough to read
  // anything we need to. That's because MemoryTextureHost has to trust the buffer
  // pointer anyway, so the security model here is just that MemoryTexture's
  // are restricted to same-process clients.
  return std::numeric_limits<size_t>::max();
}

TextureParent::TextureParent(CompositableParentManager* aCompositableManager)
: mCompositableManager(aCompositableManager)
{
  MOZ_COUNT_CTOR(TextureParent);
}

TextureParent::~TextureParent()
{
  MOZ_COUNT_DTOR(TextureParent);
  if (mTextureHost) {
    mTextureHost->ClearRecycleCallback();
  }
}

static void RecycleCallback(TextureHost*, void* aClosure) {
  TextureParent* tp = reinterpret_cast<TextureParent*>(aClosure);
  tp->CompositorRecycle();
}

void
TextureParent::CompositorRecycle()
{
  mTextureHost->ClearRecycleCallback();

  if (mTextureHost->GetFlags() & TextureFlags::RECYCLE) {
    mozilla::unused << SendCompositorRecycle();
    // Don't forget to prepare for the next reycle
    // if TextureClient request it.
    mWaitForClientRecycle = mTextureHost;
  }
}

bool
TextureParent::RecvClientRecycle()
{
  // This will allow the RecycleCallback to be called once the compositor
  // releases any external references to TextureHost.
  mTextureHost->SetRecycleCallback(RecycleCallback, this);
  if (!mWaitForClientRecycle) {
    RECYCLE_LOG("Not a recycable tile");
  }
  mWaitForClientRecycle = nullptr;
  return true;
}

bool
TextureParent::Init(const SurfaceDescriptor& aSharedData,
                    const TextureFlags& aFlags)
{
  mTextureHost = TextureHost::Create(aSharedData,
                                     mCompositableManager,
                                     aFlags);
  if (mTextureHost) {
    mTextureHost->mActor = this;
    if (aFlags & TextureFlags::RECYCLE) {
      mWaitForClientRecycle = mTextureHost;
      RECYCLE_LOG("Setup recycling for tile %p\n", this);
    }
  }

  return !!mTextureHost;
}

bool
TextureParent::RecvRemoveTexture()
{
  return PTextureParent::Send__delete__(this);
}

bool
TextureParent::RecvClearTextureHostSync()
{
  ClearTextureHost();
  return true;
}

void
TextureParent::ActorDestroy(ActorDestroyReason why)
{
  switch (why) {
  case AncestorDeletion:
  case Deletion:
  case NormalShutdown:
  case AbnormalShutdown:
    break;
  case FailedConstructor:
    NS_RUNTIMEABORT("FailedConstructor isn't possible in PTexture");
  }

  ClearTextureHost();
}

void
TextureParent::ClearTextureHost()
{
  if (!mTextureHost) {
    return;
  }

  if (mTextureHost->GetFlags() & TextureFlags::RECYCLE) {
    RECYCLE_LOG("clear recycling for tile %p\n", this);
    mTextureHost->ClearRecycleCallback();
  }
  if (mTextureHost->GetFlags() & TextureFlags::DEALLOCATE_CLIENT) {
    mTextureHost->ForgetSharedData();
  }

  // Clear recycle callback.
  mTextureHost->ClearRecycleCallback();
  mWaitForClientRecycle = nullptr;

  mTextureHost->mActor = nullptr;
  mTextureHost = nullptr;
}

bool
TextureParent::RecvRecycleTexture(const TextureFlags& aTextureFlags)
{
  if (!mTextureHost) {
    return true;
  }
  mTextureHost->RecycleTexture(aTextureFlags);
  return true;
}

////////////////////////////////////////////////////////////////////////////////

} // namespace layers
} // namespace mozilla
