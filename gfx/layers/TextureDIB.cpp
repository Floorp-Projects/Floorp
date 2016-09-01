/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureDIB.h"
#include "gfx2DGlue.h"
#include "mozilla/gfx/DataSurfaceHelpers.h" // For BufferSizeFromDimensions
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/ipc/ProtocolUtils.h"

namespace mozilla {

using namespace gfx;

namespace layers {

/**
  * Can only be drawn into through Cairo.
  * The coresponding TextureHost depends on the compositor
  */
class MemoryDIBTextureData : public DIBTextureData
{
public:
  virtual bool Serialize(SurfaceDescriptor& aOutDescriptor) override;

  virtual TextureData*
  CreateSimilar(ClientIPCAllocator* aAllocator,
                TextureFlags aFlags = TextureFlags::DEFAULT,
                TextureAllocationFlags aAllocFlags = ALLOC_DEFAULT) const override;

  virtual bool UpdateFromSurface(gfx::SourceSurface* aSurface) override;

  static
  DIBTextureData* Create(gfx::IntSize aSize, gfx::SurfaceFormat aFormat);

  virtual void Deallocate(ClientIPCAllocator* aAllocator) override
  {
    mSurface = nullptr;
  }

  MemoryDIBTextureData(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                       gfxWindowsSurface* aSurface)
  : DIBTextureData(aSize, aFormat, aSurface)
  {
    MOZ_COUNT_CTOR(MemoryDIBTextureData);
  }

  virtual ~MemoryDIBTextureData()
  {
    MOZ_COUNT_DTOR(MemoryDIBTextureData);
  }
};

/**
  * Can only be drawn into through Cairo.
  * The coresponding TextureHost depends on the compositor
  */
class ShmemDIBTextureData : public DIBTextureData
{
public:
  virtual bool Serialize(SurfaceDescriptor& aOutDescriptor) override;

  virtual TextureData*
  CreateSimilar(ClientIPCAllocator* aAllocator,
                TextureFlags aFlags = TextureFlags::DEFAULT,
                TextureAllocationFlags aAllocFlags = ALLOC_DEFAULT) const override;

  virtual bool UpdateFromSurface(gfx::SourceSurface* aSurface) override;

  static
  DIBTextureData* Create(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                         ClientIPCAllocator* aAllocator);

  void DeallocateData()
  {
    if (mSurface) {
      ::DeleteObject(mBitmap);
      ::DeleteDC(mDC);
      ::CloseHandle(mFileMapping);
      mBitmap = NULL;
      mDC = NULL;
      mFileMapping = NULL;
      mSurface = nullptr;
    }
  }

  virtual void Deallocate(ClientIPCAllocator* aAllocator) override
  {
    DeallocateData();
  }

  ShmemDIBTextureData(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                      gfxWindowsSurface* aSurface,
                      HANDLE aFileMapping, HANDLE aHostHandle,
                      HDC aDC, HBITMAP aBitmap)
  : DIBTextureData(aSize, aFormat, aSurface)
  , mFileMapping(aFileMapping)
  , mHostHandle(aHostHandle)
  , mDC(aDC)
  , mBitmap(aBitmap)
  {
    MOZ_COUNT_CTOR(ShmemDIBTextureData);
  }

  virtual ~ShmemDIBTextureData() 
  {
    MOZ_COUNT_DTOR(ShmemDIBTextureData);

    // The host side has its own references and handles to this data, we can
    // safely clear ours.
    DeallocateData();
  }

  HANDLE mFileMapping;
  HANDLE mHostHandle;
  HDC mDC;
  HBITMAP mBitmap;
};

void
DIBTextureData::FillInfo(TextureData::Info& aInfo) const
{
  aInfo.size = mSize;
  aInfo.format = mFormat;
  aInfo.hasIntermediateBuffer = true;
  aInfo.hasSynchronization = false;
  aInfo.supportsMoz2D = true;
  aInfo.canExposeMappedData = false;
}

already_AddRefed<gfx::DrawTarget>
DIBTextureData::BorrowDrawTarget()
{
  return gfxPlatform::GetPlatform()->CreateDrawTargetForSurface(mSurface, mSize);
}

DIBTextureData*
DIBTextureData::Create(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                       ClientIPCAllocator* aAllocator)
{
  if (!aAllocator) {
    return nullptr;
  }
  if (aFormat == gfx::SurfaceFormat::UNKNOWN) {
    return nullptr;
  }
  if (aAllocator->IsSameProcess()) {
    return MemoryDIBTextureData::Create(aSize, aFormat);
  } else {
    return ShmemDIBTextureData::Create(aSize, aFormat, aAllocator);
  }
}

TextureData*
MemoryDIBTextureData::CreateSimilar(ClientIPCAllocator* aAllocator,
                                    TextureFlags aFlags,
                                    TextureAllocationFlags aAllocFlags) const
{
  if (!aAllocator) {
    return nullptr;
  }
  return MemoryDIBTextureData::Create(mSize, mFormat);
}

bool
MemoryDIBTextureData::Serialize(SurfaceDescriptor& aOutDescriptor)
{
  MOZ_ASSERT(mSurface);
  // The host will release this ref when it receives the surface descriptor.
  // We AddRef in case we die before the host receives the pointer.
  aOutDescriptor = SurfaceDescriptorDIB(reinterpret_cast<uintptr_t>(mSurface.get()));
  mSurface->AddRef();
  return true;
}

DIBTextureData*
MemoryDIBTextureData::Create(gfx::IntSize aSize, gfx::SurfaceFormat aFormat)
{
  RefPtr<gfxWindowsSurface> surface
    = new gfxWindowsSurface(aSize, SurfaceFormatToImageFormat(aFormat));
  if (!surface || surface->CairoStatus()) {
    NS_WARNING("Could not create DIB surface");
    return nullptr;
  }

  return new MemoryDIBTextureData(aSize, aFormat, surface);
}

bool
MemoryDIBTextureData::UpdateFromSurface(gfx::SourceSurface* aSurface)
{
  RefPtr<gfxImageSurface> imgSurf = mSurface->GetAsImageSurface();

  RefPtr<DataSourceSurface> srcSurf = aSurface->GetDataSurface();

  if (!srcSurf) {
    gfxCriticalError() << "Failed to GetDataSurface in UpdateFromSurface (DIB).";
    return false;
  }

  DataSourceSurface::MappedSurface sourceMap;
  if (!srcSurf->Map(gfx::DataSourceSurface::READ, &sourceMap)) {
    gfxCriticalError() << "Failed to map source surface for UpdateFromSurface.";
    return false;
  }

  for (int y = 0; y < srcSurf->GetSize().height; y++) {
    memcpy(imgSurf->Data() + imgSurf->Stride() * y,
           sourceMap.mData + sourceMap.mStride * y,
           srcSurf->GetSize().width * BytesPerPixel(srcSurf->GetFormat()));
  }

  srcSurf->Unmap();
  return true;
}

TextureData*
ShmemDIBTextureData::CreateSimilar(ClientIPCAllocator* aAllocator,
                                   TextureFlags aFlags,
                                   TextureAllocationFlags aAllocFlags) const
{
  if (!aAllocator) {
    return nullptr;
  }
  return ShmemDIBTextureData::Create(mSize, mFormat, aAllocator);
}

bool
ShmemDIBTextureData::UpdateFromSurface(gfx::SourceSurface* aSurface)
{

  RefPtr<DataSourceSurface> srcSurf = aSurface->GetDataSurface();

  if (!srcSurf) {
    gfxCriticalError() << "Failed to GetDataSurface in UpdateFromSurface (DTD).";
    return false;
  }

  DataSourceSurface::MappedSurface sourceMap;
  if (!srcSurf->Map(gfx::DataSourceSurface::READ, &sourceMap)) {
    gfxCriticalError() << "Failed to map source surface for UpdateFromSurface.";
    return false;
  }

  GdiFlush();

  uint32_t stride = mSize.width * BytesPerPixel(mFormat);
  uint8_t* data = (uint8_t*)::MapViewOfFile(mFileMapping, FILE_MAP_WRITE, 0, 0, stride * mSize.height);

  if (!data) {
    gfxCriticalError() << "Failed to map view of file for UpdateFromSurface.";
    srcSurf->Unmap();
    return false;
  }

  for (int y = 0; y < srcSurf->GetSize().height; y++) {
    memcpy(data + stride * y,
           sourceMap.mData + sourceMap.mStride * y,
           srcSurf->GetSize().width * BytesPerPixel(srcSurf->GetFormat()));
  }

  ::UnmapViewOfFile(data);

  srcSurf->Unmap();
  return true;
}

bool
ShmemDIBTextureData::Serialize(SurfaceDescriptor& aOutDescriptor)
{
  if (mFormat == gfx::SurfaceFormat::UNKNOWN) {
    return false;
  }

  ::GdiFlush();
  aOutDescriptor = SurfaceDescriptorFileMapping((WindowsHandle)mHostHandle, mFormat, mSize);
  return true;
}

DIBTextureData*
ShmemDIBTextureData::Create(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                            ClientIPCAllocator* aAllocator)
{
  MOZ_ASSERT(aAllocator->GetParentPid() != base::ProcessId());

  DWORD mapSize = aSize.width * aSize.height * BytesPerPixel(aFormat);
  HANDLE fileMapping = ::CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, mapSize, NULL);

  if (!fileMapping) {
    gfxCriticalError() << "Failed to create memory file mapping for " << mapSize << " bytes.";
    return nullptr;
  }

  BITMAPV4HEADER header;
  memset(&header, 0, sizeof(BITMAPV4HEADER));
  header.bV4Size          = sizeof(BITMAPV4HEADER);
  header.bV4Width         = aSize.width;
  header.bV4Height        = -LONG(aSize.height); // top-to-buttom DIB
  header.bV4Planes        = 1;
  header.bV4BitCount      = 32;
  header.bV4V4Compression = BI_BITFIELDS;
  header.bV4RedMask       = 0x00FF0000;
  header.bV4GreenMask     = 0x0000FF00;
  header.bV4BlueMask      = 0x000000FF;

  HDC nulldc = ::GetDC(NULL);

  HDC dc = ::CreateCompatibleDC(nulldc);

  ::ReleaseDC(nullptr, nulldc);

  if (!dc) {
    ::CloseHandle(fileMapping);
    gfxCriticalError() << "Failed to create DC for bitmap.";
    return nullptr;
  }

  void* bits;
  HBITMAP bitmap = ::CreateDIBSection(dc, (BITMAPINFO*)&header,
                                      DIB_RGB_COLORS, &bits,
                                      fileMapping, 0);

  if (!bitmap) {
    gfxCriticalError() << "Failed to create DIB section for a bitmap of size "
                       << aSize << " and mapSize " << mapSize;
    ::CloseHandle(fileMapping);
    ::DeleteDC(dc);
    return nullptr;
  }

  ::SelectObject(dc, bitmap);

  RefPtr<gfxWindowsSurface> surface = new gfxWindowsSurface(dc, 0);
  if (surface->CairoStatus())
  {
    ::DeleteObject(bitmap);
    ::DeleteDC(dc);
    ::CloseHandle(fileMapping);
    gfxCriticalError() << "Could not create surface, status: "
                       << surface->CairoStatus();
    return nullptr;
  }

  HANDLE hostHandle = NULL;

  if (!ipc::DuplicateHandle(fileMapping, aAllocator->GetParentPid(),
                            &hostHandle, 0, DUPLICATE_SAME_ACCESS)) {
    gfxCriticalError() << "Failed to duplicate handle to parent process for surface.";
    ::DeleteObject(bitmap);
    ::DeleteDC(dc);
    ::CloseHandle(fileMapping);
    return nullptr;
  }

  return new ShmemDIBTextureData(aSize, aFormat, surface,
                                 fileMapping, hostHandle,
                                 dc, bitmap);
}


bool
TextureHostDirectUpload::Lock()
{
  MOZ_ASSERT(!mIsLocked);
  mIsLocked = true;
  return true;
}

void
TextureHostDirectUpload::Unlock()
{
  MOZ_ASSERT(mIsLocked);
  mIsLocked = false;
}

void
TextureHostDirectUpload::SetCompositor(Compositor* aCompositor)
{
  mCompositor = aCompositor;
}

void
TextureHostDirectUpload::DeallocateDeviceData()
{
  if (mTextureSource) {
    mTextureSource->DeallocateDeviceData();
  }
}

bool
TextureHostDirectUpload::BindTextureSource(CompositableTextureSourceRef& aTexture)
{
  if (!mTextureSource) {
    Updated();
  }

  aTexture = mTextureSource;
  return !!aTexture;
}

DIBTextureHost::DIBTextureHost(TextureFlags aFlags,
                               const SurfaceDescriptorDIB& aDescriptor)
  : TextureHostDirectUpload(aFlags, SurfaceFormat::B8G8R8X8, IntSize())
{
  // We added an extra ref for transport, so we shouldn't AddRef now.
  mSurface =
    dont_AddRef(reinterpret_cast<gfxWindowsSurface*>(aDescriptor.surface()));
  MOZ_ASSERT(mSurface);

  mSize = mSurface->GetSize();
  mFormat = mSurface->GetSurfaceFormat();
}

void
DIBTextureHost::UpdatedInternal(const nsIntRegion* aRegion)
{
  if (!mCompositor) {
    // This can happen if we send textures to a compositable that isn't yet
    // attached to a layer.
    return;
  }

  if (!mTextureSource) {
    mTextureSource = mCompositor->CreateDataTextureSource(mFlags);
  }

  if (mSurface->CairoStatus()) {
      gfxWarning() << "Bad Cairo surface internal update " << mSurface->CairoStatus();
      mTextureSource = nullptr;
      return;
  }
  RefPtr<gfxImageSurface> imgSurf = mSurface->GetAsImageSurface();

  RefPtr<DataSourceSurface> surf = Factory::CreateWrappingDataSourceSurface(imgSurf->Data(), imgSurf->Stride(), mSize, mFormat);

  if (!surf || !mTextureSource->Update(surf, const_cast<nsIntRegion*>(aRegion))) {
    mTextureSource = nullptr;
  }

  ReadUnlock();
}

TextureHostFileMapping::TextureHostFileMapping(TextureFlags aFlags,
                                               const SurfaceDescriptorFileMapping& aDescriptor)
  : TextureHostDirectUpload(aFlags, aDescriptor.format(), aDescriptor.size())
  , mFileMapping((HANDLE)aDescriptor.handle())
{
}

TextureHostFileMapping::~TextureHostFileMapping()
{
  ::CloseHandle(mFileMapping);
}

UserDataKey kFileMappingKey;

static void UnmapFileData(void* aData)
{
  MOZ_ASSERT(aData);
  ::UnmapViewOfFile(aData);
}

void
TextureHostFileMapping::UpdatedInternal(const nsIntRegion* aRegion)
{
  if (!mCompositor) {
    // This can happen if we send textures to a compositable that isn't yet
    // attached to a layer.
    return;
  }

  if (!mTextureSource) {
    mTextureSource = mCompositor->CreateDataTextureSource(mFlags);
  }

  uint8_t* data = nullptr;
  int32_t totalBytes = BufferSizeFromDimensions(mSize.width, mSize.height, BytesPerPixel(mFormat));
  if (totalBytes > 0) {
    data = (uint8_t*)::MapViewOfFile(mFileMapping, FILE_MAP_READ, 0, 0, totalBytes);
  }

  if (data) {
    RefPtr<DataSourceSurface> surf = Factory::CreateWrappingDataSourceSurface(data, mSize.width * BytesPerPixel(mFormat), mSize, mFormat);
    if (surf) {
        surf->AddUserData(&kFileMappingKey, data, UnmapFileData);
        if (!mTextureSource->Update(surf, const_cast<nsIntRegion*>(aRegion))) {
          mTextureSource = nullptr;
        }
    } else {
      mTextureSource = nullptr;
    }
  } else {
    mTextureSource = nullptr;
  }

  ReadUnlock();
}

}
}
