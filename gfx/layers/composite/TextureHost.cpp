/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/TextureHost.h"
#include "CompositableHost.h"           // for CompositableHost
#include "LayersLogging.h"              // for AppendToString
#include "gfx2DGlue.h"                  // for ToIntSize
#include "gfxImageSurface.h"            // for gfxImageSurface
#include "mozilla/gfx/2D.h"             // for DataSourceSurface, Factory
#include "mozilla/ipc/Shmem.h"          // for Shmem
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/ISurfaceAllocator.h"  // for ISurfaceAllocator
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor, etc
#ifdef MOZ_X11
#include "mozilla/layers/X11TextureHost.h"
#endif
#include "mozilla/layers/YCbCrImageDataSerializer.h"
#include "nsAString.h"
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsPrintfCString.h"            // for nsPrintfCString
#include "mozilla/layers/PTextureParent.h"
#include "mozilla/unused.h"
#include <limits>

#if 0
#define RECYCLE_LOG(...) printf_stderr(__VA_ARGS__)
#else
#define RECYCLE_LOG(...) do { } while (0)
#endif

struct nsIntPoint;

namespace mozilla {
namespace layers {

/**
 * TextureParent is the host-side IPDL glue between TextureClient and TextureHost.
 * It is an IPDL actor just like LayerParent, CompositableParent, etc.
 */
class TextureParent : public PTextureParent
{
public:
  TextureParent(ISurfaceAllocator* aAllocator);

  ~TextureParent();

  bool Init(const SurfaceDescriptor& aSharedData,
            const TextureFlags& aFlags);

  void CompositorRecycle();
  virtual bool RecvClientRecycle() MOZ_OVERRIDE;

  virtual bool RecvRemoveTexture() MOZ_OVERRIDE;

  virtual bool RecvRemoveTextureSync() MOZ_OVERRIDE;

  TextureHost* GetTextureHost() { return mTextureHost; }

  void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

  ISurfaceAllocator* mAllocator;
  RefPtr<TextureHost> mWaitForClientRecycle;
  RefPtr<TextureHost> mTextureHost;
};

// static
PTextureParent*
TextureHost::CreateIPDLActor(ISurfaceAllocator* aAllocator,
                             const SurfaceDescriptor& aSharedData,
                             TextureFlags aFlags)
{
  if (aSharedData.type() == SurfaceDescriptor::TSurfaceDescriptorMemory &&
      !aAllocator->IsSameProcess())
  {
    NS_ERROR("A client process is trying to peek at our address space using a MemoryTexture!");
    return nullptr;
  }
  TextureParent* actor = new TextureParent(aAllocator);
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

// implemented in TextureOGL.cpp
TemporaryRef<DeprecatedTextureHost> CreateDeprecatedTextureHostOGL(SurfaceDescriptorType aDescriptorType,
                                                           uint32_t aDeprecatedTextureHostFlags,
                                                           uint32_t aTextureFlags);
// implemented in BasicCompositor.cpp
TemporaryRef<DeprecatedTextureHost> CreateBasicDeprecatedTextureHost(SurfaceDescriptorType aDescriptorType,
                                                             uint32_t aDeprecatedTextureHostFlags,
                                                             uint32_t aTextureFlags);

#ifdef XP_WIN
TemporaryRef<DeprecatedTextureHost> CreateDeprecatedTextureHostD3D9(SurfaceDescriptorType aDescriptorType,
                                                            uint32_t aDeprecatedTextureHostFlags,
                                                            uint32_t aTextureFlags);

TemporaryRef<DeprecatedTextureHost> CreateDeprecatedTextureHostD3D11(SurfaceDescriptorType aDescriptorType,
                                                             uint32_t aDeprecatedTextureHostFlags,
                                                             uint32_t aTextureFlags);
#endif

/* static */ TemporaryRef<DeprecatedTextureHost>
DeprecatedTextureHost::CreateDeprecatedTextureHost(SurfaceDescriptorType aDescriptorType,
                                           uint32_t aDeprecatedTextureHostFlags,
                                           uint32_t aTextureFlags,
                                           CompositableHost* aCompositableHost)
{
  switch (Compositor::GetBackend()) {
    case LayersBackend::LAYERS_OPENGL:
      {
      RefPtr<DeprecatedTextureHost> result;
      result = CreateDeprecatedTextureHostOGL(aDescriptorType,
                                        aDeprecatedTextureHostFlags,
                                        aTextureFlags);
      if (aCompositableHost) {
        result->SetCompositableBackendSpecificData(aCompositableHost->GetCompositableBackendSpecificData());
      }
      return result;
      }
#ifdef XP_WIN
    case LayersBackend::LAYERS_D3D9:
      return CreateDeprecatedTextureHostD3D9(aDescriptorType,
                                         aDeprecatedTextureHostFlags,
                                         aTextureFlags);
    case LayersBackend::LAYERS_D3D11:
      return CreateDeprecatedTextureHostD3D11(aDescriptorType,
                                          aDeprecatedTextureHostFlags,
                                          aTextureFlags);
#endif
    case LayersBackend::LAYERS_BASIC:
      return CreateBasicDeprecatedTextureHost(aDescriptorType,
                                          aDeprecatedTextureHostFlags,
                                          aTextureFlags);
    default:
      MOZ_CRASH("Couldn't create texture host");
  }
}

// implemented in TextureHostOGL.cpp
TemporaryRef<TextureHost> CreateTextureHostOGL(const SurfaceDescriptor& aDesc,
                                               ISurfaceAllocator* aDeallocator,
                                               TextureFlags aFlags);

// implemented in TextureHostBasic.cpp
TemporaryRef<TextureHost> CreateTextureHostBasic(const SurfaceDescriptor& aDesc,
                                                 ISurfaceAllocator* aDeallocator,
                                                 TextureFlags aFlags);

// implemented in TextureD3D11.cpp
TemporaryRef<TextureHost> CreateTextureHostD3D11(const SurfaceDescriptor& aDesc,
                                                 ISurfaceAllocator* aDeallocator,
                                                 TextureFlags aFlags);

// implemented in TextureD3D9.cpp
TemporaryRef<TextureHost> CreateTextureHostD3D9(const SurfaceDescriptor& aDesc,
                                                ISurfaceAllocator* aDeallocator,
                                                TextureFlags aFlags);

// static
TemporaryRef<TextureHost>
TextureHost::Create(const SurfaceDescriptor& aDesc,
                    ISurfaceAllocator* aDeallocator,
                    TextureFlags aFlags)
{
  switch (aDesc.type()) {
    case SurfaceDescriptor::TSurfaceDescriptorShmem:
    case SurfaceDescriptor::TSurfaceDescriptorMemory:
      return CreateBackendIndependentTextureHost(aDesc, aDeallocator, aFlags);
    case SurfaceDescriptor::TSharedTextureDescriptor:
    case SurfaceDescriptor::TSurfaceDescriptorGralloc:
    case SurfaceDescriptor::TNewSurfaceDescriptorGralloc:
    case SurfaceDescriptor::TSurfaceStreamDescriptor:
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
      RefPtr<TextureHost> result = new X11TextureHost(aFlags, desc);
      return result;
    }
#endif
#ifdef XP_WIN
    case SurfaceDescriptor::TSurfaceDescriptorD3D9:
    case SurfaceDescriptor::TSurfaceDescriptorDIB:
      return CreateTextureHostD3D9(aDesc, aDeallocator, aFlags);
    case SurfaceDescriptor::TSurfaceDescriptorD3D10:
      return CreateTextureHostD3D11(aDesc, aDeallocator, aFlags);
#endif
    default:
      MOZ_CRASH("Unsupported Surface type");
  }
}

TemporaryRef<TextureHost>
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
    default: {
      NS_WARNING("No backend independent TextureHost for this descriptor type");
    }
  }
  return result;
}

void
TextureHost::SetCompositableBackendSpecificData(CompositableBackendSpecificData* aBackendData)
{
    mCompositableBackendData = aBackendData;
}


TextureHost::TextureHost(TextureFlags aFlags)
    : mActor(nullptr)
    , mFlags(aFlags)
{}

TextureHost::~TextureHost()
{
}

void TextureHost::Finalize()
{
  if (!(GetFlags() & TEXTURE_DEALLOCATE_CLIENT)) {
    DeallocateSharedData();
    DeallocateDeviceData();
  }
}

void
TextureHost::PrintInfo(nsACString& aTo, const char* aPrefix)
{
  aTo += aPrefix;
  aTo += nsPrintfCString("%s (0x%p)", Name(), this);
  // Note: the TextureHost needs to be locked before it is safe to call
  //       GetSize() and GetFormat() on it.
  if (Lock()) {
    AppendToString(aTo, GetSize(), " [size=", "]");
    AppendToString(aTo, GetFormat(), " [format=", "]");
    Unlock();
  }
  AppendToString(aTo, mFlags, " [flags=", "]");
}

void
TextureSource::SetCompositableBackendSpecificData(CompositableBackendSpecificData* aBackendData)
{
    mCompositableBackendData = aBackendData;
}

TextureSource::TextureSource()
{
    MOZ_COUNT_CTOR(TextureSource);
}
TextureSource::~TextureSource()
{
    MOZ_COUNT_DTOR(TextureSource);
}

DeprecatedTextureHost::DeprecatedTextureHost()
  : mFlags(0)
  , mBuffer(nullptr)
  , mDeAllocator(nullptr)
  , mFormat(gfx::SurfaceFormat::UNKNOWN)
{
  MOZ_COUNT_CTOR(DeprecatedTextureHost);
}

DeprecatedTextureHost::~DeprecatedTextureHost()
{
  if (mBuffer) {
    if (!(mFlags & TEXTURE_DEALLOCATE_CLIENT)) {
      if (mDeAllocator) {
        mDeAllocator->DestroySharedSurface(mBuffer);
      } else {
        MOZ_ASSERT(mBuffer->type() == SurfaceDescriptor::Tnull_t);
      }
    }
    delete mBuffer;
  }
  MOZ_COUNT_DTOR(DeprecatedTextureHost);
}

void
DeprecatedTextureHost::Update(const SurfaceDescriptor& aImage,
                          nsIntRegion* aRegion,
                          nsIntPoint* aOffset)
{
  UpdateImpl(aImage, aRegion, aOffset);
}

void
DeprecatedTextureHost::SwapTextures(const SurfaceDescriptor& aImage,
                                SurfaceDescriptor* aResult,
                                nsIntRegion* aRegion)
{
  SwapTexturesImpl(aImage, aRegion);

  MOZ_ASSERT(mBuffer, "trying to swap a non-buffered texture host?");
  if (aResult) {
    *aResult = *mBuffer;
  }
  *mBuffer = aImage;
  // The following SetBuffer call was added in bug 912725 as a fix for the
  // hacky fix introduced in gecko 23 for bug 862324.
  // Note that it is a no-op in the generic case, but not for
  // GrallocDeprecatedTextureHostOGL which overrides SetBuffer to make it
  // register the TextureHost with the GrallocBufferActor.
  // The reason why this SetBuffer calls is needed here is that just above we
  // overwrote *mBuffer in place, so we need to tell the new mBuffer about this
  // TextureHost.
  SetBuffer(mBuffer, mDeAllocator);
}

void
DeprecatedTextureHost::OnShutdown()
{
  if (ISurfaceAllocator::IsShmem(mBuffer)) {
    *mBuffer = SurfaceDescriptor();
    mBuffer = nullptr;
  }
}

void
DeprecatedTextureHost::PrintInfo(nsACString& aTo, const char* aPrefix)
{
  aTo += aPrefix;
  aTo += nsPrintfCString("%s (0x%p)", Name(), this);
  AppendToString(aTo, GetSize(), " [size=", "]");
  AppendToString(aTo, GetFormat(), " [format=", "]");
  AppendToString(aTo, mFlags, " [flags=", "]");
}

void
DeprecatedTextureHost::SetBuffer(SurfaceDescriptor* aBuffer, ISurfaceAllocator* aAllocator)
{
  MOZ_ASSERT(!mBuffer || mBuffer == aBuffer, "Will leak the old mBuffer");
  mBuffer = aBuffer;
  mDeAllocator = aAllocator;
}

BufferTextureHost::BufferTextureHost(gfx::SurfaceFormat aFormat,
                                     TextureFlags aFlags)
: TextureHost(aFlags)
, mCompositor(nullptr)
, mFormat(aFormat)
, mUpdateSerial(1)
, mLocked(false)
, mPartialUpdate(false)
{}

BufferTextureHost::~BufferTextureHost()
{}

void
BufferTextureHost::Updated(const nsIntRegion* aRegion)
{
  ++mUpdateSerial;
  if (aRegion) {
    mPartialUpdate = true;
    mMaybeUpdatedRegion = *aRegion;
  } else {
    mPartialUpdate = false;
  }
  if (GetFlags() & TEXTURE_IMMEDIATE_UPLOAD) {
    DebugOnly<bool> result = MaybeUpload(mPartialUpdate ? &mMaybeUpdatedRegion : nullptr);
    NS_WARN_IF_FALSE(result, "Failed to upload a texture");
  }
}

void
BufferTextureHost::SetCompositor(Compositor* aCompositor)
{
  if (mCompositor == aCompositor) {
    return;
  }
  RefPtr<NewTextureSource> it = mFirstSource;
  while (it) {
    it->SetCompositor(aCompositor);
    it = it->GetNextSibling();
  }
  mCompositor = aCompositor;
}

void
BufferTextureHost::DeallocateDeviceData()
{
  RefPtr<NewTextureSource> it = mFirstSource;
  while (it) {
    it->DeallocateDeviceData();
    it = it->GetNextSibling();
  }
}

bool
BufferTextureHost::Lock()
{
  mLocked = true;
  return true;
}

void
BufferTextureHost::Unlock()
{
  mLocked = false;
}

NewTextureSource*
BufferTextureHost::GetTextureSources()
{
  MOZ_ASSERT(mLocked, "should never be called while not locked");
  if (!MaybeUpload(mPartialUpdate ? &mMaybeUpdatedRegion : nullptr)) {
      return nullptr;
  }
  return mFirstSource;
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
    !mCompositor->SupportsEffect(EFFECT_YCBCR)) {
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
    NS_WARNING("Tried to upload without a compositor. Skipping texture upload...");
    // If we are in this situation it means we should have called SetCompositor
    // earlier. It is conceivable that on certain rare conditions with async-video
    // we may end up here for the first frame, but this should not happen repeatedly.
    return false;
  }
  if (mFormat == gfx::SurfaceFormat::UNKNOWN) {
    NS_WARNING("BufferTextureHost: unsupported format!");
    return false;
  } else if (mFormat == gfx::SurfaceFormat::YUV) {
    YCbCrImageDataDeserializer yuvDeserializer(GetBuffer(), GetBufferSize());
    MOZ_ASSERT(yuvDeserializer.IsValid());

    if (!mCompositor->SupportsEffect(EFFECT_YCBCR)) {
      RefPtr<gfx::DataSourceSurface> surf = yuvDeserializer.ToDataSourceSurface();
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
      srcY = mCompositor->CreateDataTextureSource(mFlags|TEXTURE_DISALLOW_BIGIMAGE);
      srcU = mCompositor->CreateDataTextureSource(mFlags|TEXTURE_DISALLOW_BIGIMAGE);
      srcV = mCompositor->CreateDataTextureSource(mFlags|TEXTURE_DISALLOW_BIGIMAGE);
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
    if (!srcY->Update(tempY) ||
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
  return true;
}

TemporaryRef<gfx::DataSourceSurface>
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
, mShmem(new ipc::Shmem(aShmem))
, mDeallocator(aDeallocator)
{
  MOZ_COUNT_CTOR(ShmemTextureHost);
}

ShmemTextureHost::~ShmemTextureHost()
{
  DeallocateDeviceData();
  delete mShmem;
  MOZ_COUNT_DTOR(ShmemTextureHost);
}

void
ShmemTextureHost::DeallocateSharedData()
{
  if (mShmem) {
    MOZ_ASSERT(mDeallocator,
               "Shared memory would leak without a ISurfaceAllocator");
    mDeallocator->DeallocShmem(*mShmem);
    delete mShmem;
    mShmem = nullptr;
  }
}

void
ShmemTextureHost::ForgetSharedData()
{
  if (mShmem) {
    delete mShmem;
    mShmem = nullptr;
  }
}

void
ShmemTextureHost::OnShutdown()
{
  delete mShmem;
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
}

MemoryTextureHost::~MemoryTextureHost()
{
  DeallocateDeviceData();
  NS_ASSERTION(!mBuffer || (mFlags & TEXTURE_DEALLOCATE_CLIENT),
               "Leaking our buffer");
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

TextureParent::TextureParent(ISurfaceAllocator* aAllocator)
: mAllocator(aAllocator)
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

static void RecycleCallback(TextureHost* textureHost, void* aClosure) {
  TextureParent* tp = reinterpret_cast<TextureParent*>(aClosure);
  tp->CompositorRecycle();
}

void
TextureParent::CompositorRecycle()
{
  mTextureHost->ClearRecycleCallback();
  mozilla::unused << SendCompositorRecycle();

  // Don't forget to prepare for the next reycle
  mWaitForClientRecycle = mTextureHost;
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
                                     mAllocator,
                                     aFlags);
  if (mTextureHost) {
    mTextureHost->mActor = this;
    if (aFlags & TEXTURE_RECYCLE) {
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
TextureParent::RecvRemoveTextureSync()
{
  // we don't need to send a reply in the synchronous case since the child side
  // has the guarantee that this message has been handled synchronously.
  return PTextureParent::Send__delete__(this);
}

void
TextureParent::ActorDestroy(ActorDestroyReason why)
{
  if (!mTextureHost) {
    return;
  }

  switch (why) {
  case AncestorDeletion:
  case Deletion:
  case NormalShutdown:
  case AbnormalShutdown:
    break;
  case FailedConstructor:
    NS_RUNTIMEABORT("FailedConstructor isn't possible in PTexture");
  }

  if (mTextureHost->GetFlags() & TEXTURE_RECYCLE) {
    RECYCLE_LOG("clear recycling for tile %p\n", this);
    mTextureHost->ClearRecycleCallback();
  }
  if (mTextureHost->GetFlags() & TEXTURE_DEALLOCATE_CLIENT) {
    mTextureHost->ForgetSharedData();
  }

  mTextureHost->mActor = nullptr;
  mTextureHost = nullptr;
}

} // namespace
} // namespace
