/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "mozilla/dom/UnionTypes.h"
#include "Queue.h"

#include <algorithm>

#include "CommandBuffer.h"
#include "CommandEncoder.h"
#include "ipc/WebGPUChild.h"
#include "mozilla/Casting.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/dom/ImageBitmap.h"
#include "mozilla/dom/OffscreenCanvas.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WebGLTexelConversions.h"
#include "mozilla/dom/WebGLTypes.h"
#include "nsLayoutUtils.h"
#include "Utility.h"

namespace mozilla::webgpu {

GPU_IMPL_CYCLE_COLLECTION(Queue, mParent, mBridge)
GPU_IMPL_JS_WRAP(Queue)

Queue::Queue(Device* const aParent, WebGPUChild* aBridge, RawId aId)
    : ChildOf(aParent), mBridge(aBridge), mId(aId) {
  MOZ_RELEASE_ASSERT(aId);
}

Queue::~Queue() { Cleanup(); }

void Queue::Submit(
    const dom::Sequence<OwningNonNull<CommandBuffer>>& aCommandBuffers) {
  nsTArray<RawId> list(aCommandBuffers.Length());
  for (uint32_t i = 0; i < aCommandBuffers.Length(); ++i) {
    auto idMaybe = aCommandBuffers[i]->Commit();
    if (idMaybe) {
      list.AppendElement(*idMaybe);
    }
  }

  mBridge->QueueSubmit(mId, mParent->mId, list);
}

already_AddRefed<dom::Promise> Queue::OnSubmittedWorkDone(ErrorResult& aRv) {
  RefPtr<dom::Promise> promise = dom::Promise::Create(GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  mBridge->QueueOnSubmittedWorkDone(mId, promise);

  return promise.forget();
}

void Queue::WriteBuffer(const Buffer& aBuffer, uint64_t aBufferOffset,
                        const dom::ArrayBufferViewOrArrayBuffer& aData,
                        uint64_t aDataOffset,
                        const dom::Optional<uint64_t>& aSize,
                        ErrorResult& aRv) {
  if (!aBuffer.mId) {
    // Invalid buffers are unknown to the parent -- don't try to write
    // to them.
    return;
  }

  dom::ProcessTypedArraysFixed(aData, [&](const Span<const uint8_t>& aData) {
    uint64_t length = aData.Length();
    const auto checkedSize = aSize.WasPassed()
                                 ? CheckedInt<size_t>(aSize.Value())
                                 : CheckedInt<size_t>(length) - aDataOffset;
    if (!checkedSize.isValid()) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }

    const auto& size = checkedSize.value();
    if (aDataOffset + size > length) {
      aRv.ThrowAbortError(nsPrintfCString("Wrong data size %" PRIuPTR, size));
      return;
    }

    if (size % 4 != 0) {
      aRv.ThrowAbortError("Byte size must be a multiple of 4");
      return;
    }

    auto alloc = mozilla::ipc::UnsafeSharedMemoryHandle::CreateAndMap(size);
    if (alloc.isNothing()) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }

    auto handle = std::move(alloc.ref().first);
    auto mapping = std::move(alloc.ref().second);

    memcpy(mapping.Bytes().data(), aData.Elements() + aDataOffset, size);
    ipc::ByteBuf bb;
    ffi::wgpu_queue_write_buffer(aBuffer.mId, aBufferOffset, ToFFI(&bb));
    if (!mBridge->SendQueueWriteAction(mId, mParent->mId, std::move(bb),
                                       std::move(handle))) {
      MOZ_CRASH("IPC failure");
    }
  });
}

void Queue::WriteTexture(const dom::GPUImageCopyTexture& aDestination,
                         const dom::ArrayBufferViewOrArrayBuffer& aData,
                         const dom::GPUImageDataLayout& aDataLayout,
                         const dom::GPUExtent3D& aSize, ErrorResult& aRv) {
  ffi::WGPUImageCopyTexture copyView = {};
  CommandEncoder::ConvertTextureCopyViewToFFI(aDestination, &copyView);
  ffi::WGPUImageDataLayout dataLayout = {};
  CommandEncoder::ConvertTextureDataLayoutToFFI(aDataLayout, &dataLayout);
  dataLayout.offset = 0;  // our Shmem has the contents starting from 0.
  ffi::WGPUExtent3d extent = {};
  ConvertExtent3DToFFI(aSize, &extent);

  dom::ProcessTypedArraysFixed(aData, [&](const Span<const uint8_t>& aData) {
    if (aData.IsEmpty()) {
      aRv.ThrowAbortError("Input size cannot be zero.");
      return;
    }

    const auto checkedSize =
        CheckedInt<size_t>(aData.Length()) - aDataLayout.mOffset;
    if (!checkedSize.isValid()) {
      aRv.ThrowAbortError("Offset is higher than the size");
      return;
    }
    const auto size = checkedSize.value();

    auto alloc = mozilla::ipc::UnsafeSharedMemoryHandle::CreateAndMap(size);
    if (alloc.isNothing()) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }

    auto handle = std::move(alloc.ref().first);
    auto mapping = std::move(alloc.ref().second);

    memcpy(mapping.Bytes().data(), aData.Elements() + aDataLayout.mOffset,
           size);

    ipc::ByteBuf bb;
    ffi::wgpu_queue_write_texture(copyView, dataLayout, extent, ToFFI(&bb));
    if (!mBridge->SendQueueWriteAction(mId, mParent->mId, std::move(bb),
                                       std::move(handle))) {
      MOZ_CRASH("IPC failure");
    }
  });
}

static WebGLTexelFormat ToWebGLTexelFormat(gfx::SurfaceFormat aFormat) {
  switch (aFormat) {
    case gfx::SurfaceFormat::B8G8R8A8:
    case gfx::SurfaceFormat::B8G8R8X8:
      return WebGLTexelFormat::BGRA8;
    case gfx::SurfaceFormat::R8G8B8A8:
    case gfx::SurfaceFormat::R8G8B8X8:
      return WebGLTexelFormat::RGBA8;
    default:
      return WebGLTexelFormat::FormatNotSupportingAnyConversion;
  }
}

static WebGLTexelFormat ToWebGLTexelFormat(dom::GPUTextureFormat aFormat) {
  // TODO: We need support for Rbg10a2unorm as well.
  switch (aFormat) {
    case dom::GPUTextureFormat::R8unorm:
      return WebGLTexelFormat::R8;
    case dom::GPUTextureFormat::R16float:
      return WebGLTexelFormat::R16F;
    case dom::GPUTextureFormat::R32float:
      return WebGLTexelFormat::R32F;
    case dom::GPUTextureFormat::Rg8unorm:
      return WebGLTexelFormat::RG8;
    case dom::GPUTextureFormat::Rg16float:
      return WebGLTexelFormat::RG16F;
    case dom::GPUTextureFormat::Rg32float:
      return WebGLTexelFormat::RG32F;
    case dom::GPUTextureFormat::Rgba8unorm:
    case dom::GPUTextureFormat::Rgba8unorm_srgb:
      return WebGLTexelFormat::RGBA8;
    case dom::GPUTextureFormat::Bgra8unorm:
    case dom::GPUTextureFormat::Bgra8unorm_srgb:
      return WebGLTexelFormat::BGRA8;
    case dom::GPUTextureFormat::Rgba16float:
      return WebGLTexelFormat::RGBA16F;
    case dom::GPUTextureFormat::Rgba32float:
      return WebGLTexelFormat::RGBA32F;
    default:
      return WebGLTexelFormat::FormatNotSupportingAnyConversion;
  }
}

void Queue::CopyExternalImageToTexture(
    const dom::GPUImageCopyExternalImage& aSource,
    const dom::GPUImageCopyTextureTagged& aDestination,
    const dom::GPUExtent3D& aCopySize, ErrorResult& aRv) {
  const auto dstFormat = ToWebGLTexelFormat(aDestination.mTexture->Format());
  if (dstFormat == WebGLTexelFormat::FormatNotSupportingAnyConversion) {
    aRv.ThrowInvalidStateError("Unsupported destination format");
    return;
  }

  const uint32_t surfaceFlags = nsLayoutUtils::SFE_ALLOW_NON_PREMULT;
  SurfaceFromElementResult sfeResult;
  switch (aSource.mSource.GetType()) {
    case decltype(aSource.mSource)::Type::eImageBitmap: {
      const auto& bitmap = aSource.mSource.GetAsImageBitmap();
      if (bitmap->IsClosed()) {
        aRv.ThrowInvalidStateError("Detached ImageBitmap");
        return;
      }

      sfeResult = nsLayoutUtils::SurfaceFromImageBitmap(bitmap, surfaceFlags);
      break;
    }
    case decltype(aSource.mSource)::Type::eHTMLCanvasElement: {
      MOZ_ASSERT(NS_IsMainThread());

      const auto& canvas = aSource.mSource.GetAsHTMLCanvasElement();
      if (canvas->Width() == 0 || canvas->Height() == 0) {
        aRv.ThrowInvalidStateError("Zero-sized HTMLCanvasElement");
        return;
      }

      sfeResult = nsLayoutUtils::SurfaceFromElement(canvas, surfaceFlags);
      break;
    }
    case decltype(aSource.mSource)::Type::eOffscreenCanvas: {
      const auto& canvas = aSource.mSource.GetAsOffscreenCanvas();
      if (canvas->Width() == 0 || canvas->Height() == 0) {
        aRv.ThrowInvalidStateError("Zero-sized OffscreenCanvas");
        return;
      }

      sfeResult =
          nsLayoutUtils::SurfaceFromOffscreenCanvas(canvas, surfaceFlags);
      break;
    }
  }

  if (!sfeResult.mCORSUsed) {
    nsIGlobalObject* global = mParent->GetOwnerGlobal();
    nsIPrincipal* dstPrincipal = global ? global->PrincipalOrNull() : nullptr;
    if (!sfeResult.mPrincipal || !dstPrincipal ||
        !dstPrincipal->Subsumes(sfeResult.mPrincipal)) {
      aRv.ThrowSecurityError("Cross-origin elements require CORS!");
      return;
    }
  }

  if (sfeResult.mIsWriteOnly) {
    aRv.ThrowSecurityError("Write only source data not supported!");
    return;
  }

  RefPtr<gfx::SourceSurface> surface = sfeResult.GetSourceSurface();
  if (!surface) {
    aRv.ThrowInvalidStateError("No surface available from source");
    return;
  }

  RefPtr<gfx::DataSourceSurface> dataSurface = surface->GetDataSurface();
  if (!dataSurface) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  bool srcPremultiplied;
  switch (sfeResult.mAlphaType) {
    case gfxAlphaType::Premult:
      srcPremultiplied = true;
      break;
    case gfxAlphaType::NonPremult:
      srcPremultiplied = false;
      break;
    case gfxAlphaType::Opaque:
      // No (un)premultiplication necessary so match the output.
      srcPremultiplied = aDestination.mPremultipliedAlpha;
      break;
  }

  const auto surfaceFormat = dataSurface->GetFormat();
  const auto srcFormat = ToWebGLTexelFormat(surfaceFormat);
  if (srcFormat == WebGLTexelFormat::FormatNotSupportingAnyConversion) {
    gfxCriticalError() << "Unsupported surface format from source "
                       << surfaceFormat;
    MOZ_CRASH();
  }

  gfx::DataSourceSurface::ScopedMap map(dataSurface,
                                        gfx::DataSourceSurface::READ);
  if (!map.IsMapped()) {
    aRv.ThrowInvalidStateError("Cannot map surface from source");
    return;
  }

  if (!aSource.mOrigin.IsGPUOrigin2DDict()) {
    aRv.ThrowInvalidStateError("Cannot get origin from source");
    return;
  }

  ffi::WGPUExtent3d extent = {};
  ConvertExtent3DToFFI(aCopySize, &extent);
  if (extent.depth_or_array_layers > 1) {
    aRv.ThrowOperationError("Depth is greater than 1");
    return;
  }

  uint32_t srcOriginX;
  uint32_t srcOriginY;
  if (aSource.mOrigin.IsRangeEnforcedUnsignedLongSequence()) {
    const auto& seq = aSource.mOrigin.GetAsRangeEnforcedUnsignedLongSequence();
    srcOriginX = seq.Length() > 0 ? seq[0] : 0;
    srcOriginY = seq.Length() > 1 ? seq[1] : 0;
  } else if (aSource.mOrigin.IsGPUOrigin2DDict()) {
    const auto& dict = aSource.mOrigin.GetAsGPUOrigin2DDict();
    srcOriginX = dict.mX;
    srcOriginY = dict.mY;
  } else {
    MOZ_CRASH("Unexpected origin type!");
  }

  const auto checkedMaxWidth = CheckedInt<uint32_t>(srcOriginX) + extent.width;
  const auto checkedMaxHeight =
      CheckedInt<uint32_t>(srcOriginY) + extent.height;
  if (!checkedMaxWidth.isValid() || !checkedMaxHeight.isValid()) {
    aRv.ThrowOperationError("Offset and copy size exceed integer bounds");
    return;
  }

  const gfx::IntSize surfaceSize = dataSurface->GetSize();
  const auto surfaceWidth = AssertedCast<uint32_t>(surfaceSize.width);
  const auto surfaceHeight = AssertedCast<uint32_t>(surfaceSize.height);
  if (surfaceWidth < checkedMaxWidth.value() ||
      surfaceHeight < checkedMaxHeight.value()) {
    aRv.ThrowOperationError("Offset and copy size exceed surface bounds");
    return;
  }

  const auto dstWidth = extent.width;
  const auto dstHeight = extent.height;
  if (dstWidth == 0 || dstHeight == 0) {
    aRv.ThrowOperationError("Destination size is empty");
    return;
  }

  if (!aDestination.mTexture->mBytesPerBlock) {
    // TODO(bug 1781071) This should emmit a GPUValidationError on the device
    // timeline.
    aRv.ThrowInvalidStateError("Invalid destination format");
    return;
  }

  // Note: This assumes bytes per block == bytes per pixel which is the case
  // here because the spec only allows non-compressed texture formats for the
  // destination.
  const auto dstStride = CheckedInt<uint32_t>(extent.width) *
                         aDestination.mTexture->mBytesPerBlock.value();
  const auto dstByteLength = dstStride * extent.height;
  if (!dstStride.isValid() || !dstByteLength.isValid()) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  auto alloc = mozilla::ipc::UnsafeSharedMemoryHandle::CreateAndMap(
      dstByteLength.value());
  if (alloc.isNothing()) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  auto handle = std::move(alloc.ref().first);
  auto mapping = std::move(alloc.ref().second);

  const int32_t pixelSize = gfx::BytesPerPixel(surfaceFormat);
  auto* dstBegin = mapping.Bytes().data();
  const auto* srcBegin =
      map.GetData() + srcOriginX * pixelSize + srcOriginY * map.GetStride();
  const auto srcOriginPos = gl::OriginPos::TopLeft;
  const auto srcStride = AssertedCast<uint32_t>(map.GetStride());
  const auto dstOriginPos =
      aSource.mFlipY ? gl::OriginPos::BottomLeft : gl::OriginPos::TopLeft;
  bool wasTrivial;

  auto dstStrideVal = dstStride.value();

  if (!ConvertImage(dstWidth, dstHeight, srcBegin, srcStride, srcOriginPos,
                    srcFormat, srcPremultiplied, dstBegin, dstStrideVal,
                    dstOriginPos, dstFormat, aDestination.mPremultipliedAlpha,
                    &wasTrivial)) {
    MOZ_ASSERT_UNREACHABLE("ConvertImage failed!");
    aRv.ThrowInvalidStateError(
        nsPrintfCString("Failed to convert source to destination format "
                        "(%i/%i), please file a bug!",
                        (int)srcFormat, (int)dstFormat));
    return;
  }

  ffi::WGPUImageDataLayout dataLayout = {0, &dstStrideVal, &dstHeight};
  ffi::WGPUImageCopyTexture copyView = {};
  CommandEncoder::ConvertTextureCopyViewToFFI(aDestination, &copyView);
  ipc::ByteBuf bb;
  ffi::wgpu_queue_write_texture(copyView, dataLayout, extent, ToFFI(&bb));
  if (!mBridge->SendQueueWriteAction(mId, mParent->mId, std::move(bb),
                                     std::move(handle))) {
    MOZ_CRASH("IPC failure");
  }
}

}  // namespace mozilla::webgpu
