/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureDIB.h"
#include "gfx2DGlue.h"
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/ipc/ProtocolUtils.h"

namespace mozilla {

using namespace gfx;

namespace layers {

bool
TextureClientDIB::Lock(OpenMode)
{
  MOZ_ASSERT(!mIsLocked);
  if (!IsValid()) {
    return false;
  }
  mIsLocked = true;
  return true;
}

void
TextureClientDIB::Unlock()
{
  MOZ_ASSERT(mIsLocked, "Unlocked called while the texture is not locked!");
  if (mDrawTarget) {
    if (mReadbackSink) {
      RefPtr<SourceSurface> snapshot = mDrawTarget->Snapshot();
      RefPtr<DataSourceSurface> dataSurf = snapshot->GetDataSurface();
      mReadbackSink->ProcessReadback(dataSurf);
    }

    mDrawTarget->Flush();
    mDrawTarget = nullptr;
  }

  mIsLocked = false;
}

gfx::DrawTarget*
TextureClientDIB::BorrowDrawTarget()
{
  MOZ_ASSERT(mIsLocked && IsAllocated());

  if (!mDrawTarget) {
    mDrawTarget =
      gfxPlatform::GetPlatform()->CreateDrawTargetForSurface(mSurface, mSize);
  }

  return mDrawTarget;
}

TextureClientMemoryDIB::TextureClientMemoryDIB(ISurfaceAllocator* aAllocator,
                                   gfx::SurfaceFormat aFormat,
                                   TextureFlags aFlags)
  : TextureClientDIB(aAllocator, aFormat, aFlags)
{
  MOZ_COUNT_CTOR(TextureClientMemoryDIB);
}

TextureClientMemoryDIB::~TextureClientMemoryDIB()
{
  MOZ_COUNT_DTOR(TextureClientMemoryDIB);
}

already_AddRefed<TextureClient>
TextureClientMemoryDIB::CreateSimilar(TextureFlags aFlags,
                                      TextureAllocationFlags aAllocFlags) const
{
  RefPtr<TextureClient> tex = new TextureClientMemoryDIB(mAllocator, mFormat,
                                                         mFlags | aFlags);

  if (!tex->AllocateForSurface(mSize, aAllocFlags)) {
    return nullptr;
  }

  return tex.forget();
}

bool
TextureClientMemoryDIB::ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor)
{
  MOZ_ASSERT(IsValid());
  if (!IsAllocated()) {
    return false;
  }

  MOZ_ASSERT(mSurface);
  // The host will release this ref when it receives the surface descriptor.
  // We AddRef in case we die before the host receives the pointer.
  aOutDescriptor = SurfaceDescriptorDIB(reinterpret_cast<uintptr_t>(mSurface.get()));
  mSurface->AddRef();
  return true;
}

bool
TextureClientMemoryDIB::AllocateForSurface(gfx::IntSize aSize, TextureAllocationFlags aFlags)
{
  MOZ_ASSERT(!IsAllocated());
  mSize = aSize;

  mSurface = new gfxWindowsSurface(aSize, SurfaceFormatToImageFormat(mFormat));
  if (!mSurface || mSurface->CairoStatus())
  {
    NS_WARNING("Could not create surface");
    mSurface = nullptr;
    return false;
  }

  return true;
}

TextureClientShmemDIB::TextureClientShmemDIB(ISurfaceAllocator* aAllocator,
                                             gfx::SurfaceFormat aFormat,
                                             TextureFlags aFlags)
  : TextureClientDIB(aAllocator, aFormat, aFlags)
  , mFileMapping(NULL)
  , mHostHandle(NULL)
  , mDC(NULL)
  , mBitmap(NULL)
{
  MOZ_COUNT_CTOR(TextureClientShmemDIB);
}

TextureClientShmemDIB::~TextureClientShmemDIB()
{
  MOZ_COUNT_DTOR(TextureClientShmemDIB);

  ::DeleteObject(mBitmap);
  ::DeleteDC(mDC);
  ::CloseHandle(mFileMapping);
}

already_AddRefed<TextureClient>
TextureClientShmemDIB::CreateSimilar(TextureFlags aFlags,
                                     TextureAllocationFlags aAllocFlags) const
{
  RefPtr<TextureClient> tex = new TextureClientShmemDIB(mAllocator, mFormat,
                                                        mFlags | aFlags);

  if (!tex->AllocateForSurface(mSize, aAllocFlags)) {
    return nullptr;
  }

  return tex.forget();
}

bool
TextureClientShmemDIB::ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor)
{
  MOZ_ASSERT(IsValid());
  MOZ_ASSERT(mAllocator->ParentPid() != base::ProcessId());
  if (!IsAllocated() || GetFormat() == gfx::SurfaceFormat::UNKNOWN) {
    return false;
  }

  ::GdiFlush();
  aOutDescriptor = SurfaceDescriptorFileMapping((WindowsHandle)mHostHandle, mFormat, mSize);
  return true;
}

bool
TextureClientShmemDIB::AllocateForSurface(gfx::IntSize aSize, TextureAllocationFlags aFlags)
{
  MOZ_ASSERT(!IsAllocated());
  MOZ_ASSERT(mAllocator->ParentPid() != base::ProcessId());

  mSize = aSize;

  DWORD mapSize = mSize.width * mSize.height * BytesPerPixel(mFormat);
  mFileMapping = ::CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, mapSize, NULL);

  if (!mFileMapping) {
    gfxCriticalError() << "Failed to create memory file mapping for " << mapSize << " bytes.";
    return false;
  }

  uint8_t* data = (uint8_t*)::MapViewOfFile(mFileMapping, FILE_MAP_WRITE | FILE_MAP_READ, 0, 0, mSize.width * mSize.height * BytesPerPixel(mFormat));

  memset(data, 0x80, mSize.width * mSize.height * BytesPerPixel(mFormat));

  ::UnmapViewOfFile(mFileMapping);

  BITMAPV4HEADER header;
  memset(&header, 0, sizeof(BITMAPV4HEADER));
  header.bV4Size          = sizeof(BITMAPV4HEADER);
  header.bV4Width         = mSize.width;
  header.bV4Height        = -LONG(mSize.height); // top-to-buttom DIB
  header.bV4Planes        = 1;
  header.bV4BitCount      = 32;
  header.bV4V4Compression = BI_BITFIELDS;
  header.bV4RedMask       = 0x00FF0000;
  header.bV4GreenMask     = 0x0000FF00;
  header.bV4BlueMask      = 0x000000FF;

  mDC = ::CreateCompatibleDC(::GetDC(NULL));

  if (!mDC) {
    ::CloseHandle(mFileMapping);
    gfxCriticalError() << "Failed to create DC for bitmap.";
    return false;
  }

  void* bits;
  mBitmap = ::CreateDIBSection(mDC, (BITMAPINFO*)&header, DIB_RGB_COLORS, &bits, mFileMapping, 0);

  if (!mBitmap) {
    gfxCriticalError() << "Failed to create DIB section for a bitmap of size " << mSize;
    ::CloseHandle(mFileMapping);
    ::DeleteDC(mDC);
    return false;
  }

  ::SelectObject(mDC, mBitmap);

  mSurface = new gfxWindowsSurface(mDC, 0);
  if (mSurface->CairoStatus())
  {
    ::DeleteObject(mBitmap);
    ::DeleteDC(mDC);
    ::CloseHandle(mFileMapping);
    gfxCriticalError() << "Could not create surface, status: " << mSurface->CairoStatus();
    mSurface = nullptr;
    return false;
  }

  if (!ipc::DuplicateHandle(mFileMapping, mAllocator->ParentPid(), &mHostHandle, 0, DUPLICATE_SAME_ACCESS)) {
    gfxCriticalError() << "Failed to duplicate handle to parent process for surface.";
    ::DeleteObject(mBitmap);
    ::DeleteDC(mDC);
    ::CloseHandle(mFileMapping);
    return false;
  }

  return true;
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

  nsRefPtr<gfxImageSurface> imgSurf = mSurface->GetAsImageSurface();

  RefPtr<DataSourceSurface> surf = Factory::CreateWrappingDataSourceSurface(imgSurf->Data(), imgSurf->Stride(), mSize, mFormat);

  if (!mTextureSource->Update(surf, const_cast<nsIntRegion*>(aRegion))) {
    mTextureSource = nullptr;
  }
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

  uint8_t* data = (uint8_t*)::MapViewOfFile(mFileMapping, FILE_MAP_READ, 0, 0, mSize.width * mSize.height * BytesPerPixel(mFormat));

  if (data) {
    RefPtr<DataSourceSurface> surf = Factory::CreateWrappingDataSourceSurface(data, mSize.width * BytesPerPixel(mFormat), mSize, mFormat);

    if (!mTextureSource->Update(surf, const_cast<nsIntRegion*>(aRegion))) {
      mTextureSource = nullptr;
    }
  } else {
    mTextureSource = nullptr;
  }

  ::UnmapViewOfFile(data);
}

}
}
