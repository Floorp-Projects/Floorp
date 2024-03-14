/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MacIOSurface.h"
#include <OpenGL/gl.h>
#include <OpenGL/CGLIOSurface.h>
#include <QuartzCore/QuartzCore.h>
#include "GLConsts.h"
#include "GLContextCGL.h"
#include "gfxMacUtils.h"
#include "nsPrintfCString.h"
#include "mozilla/Assertions.h"
#include "mozilla/RefPtr.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/StaticPrefs_gfx.h"

using namespace mozilla;

MacIOSurface::MacIOSurface(CFTypeRefPtr<IOSurfaceRef> aIOSurfaceRef,
                           bool aHasAlpha, gfx::YUVColorSpace aColorSpace)
    : mIOSurfaceRef(std::move(aIOSurfaceRef)),
      mHasAlpha(aHasAlpha),
      mColorSpace(aColorSpace) {
  IncrementUseCount();
}

MacIOSurface::~MacIOSurface() {
  MOZ_RELEASE_ASSERT(!IsLocked(), "Destroying locked surface");
  DecrementUseCount();
}

void AddDictionaryInt(const CFTypeRefPtr<CFMutableDictionaryRef>& aDict,
                      const void* aType, uint32_t aValue) {
  auto cfValue = CFTypeRefPtr<CFNumberRef>::WrapUnderCreateRule(
      ::CFNumberCreate(nullptr, kCFNumberSInt32Type, &aValue));
  ::CFDictionaryAddValue(aDict.get(), aType, cfValue.get());
}

void SetSizeProperties(const CFTypeRefPtr<CFMutableDictionaryRef>& aDict,
                       int aWidth, int aHeight, int aBytesPerPixel) {
  AddDictionaryInt(aDict, kIOSurfaceWidth, aWidth);
  AddDictionaryInt(aDict, kIOSurfaceHeight, aHeight);
  ::CFDictionaryAddValue(aDict.get(), kIOSurfaceIsGlobal, kCFBooleanTrue);
  AddDictionaryInt(aDict, kIOSurfaceBytesPerElement, aBytesPerPixel);

  size_t bytesPerRow =
      IOSurfaceAlignProperty(kIOSurfaceBytesPerRow, aWidth * aBytesPerPixel);
  AddDictionaryInt(aDict, kIOSurfaceBytesPerRow, bytesPerRow);

  // Add a SIMD register worth of extra bytes to the end of the allocation for
  // SWGL.
  size_t totalBytes =
      IOSurfaceAlignProperty(kIOSurfaceAllocSize, aHeight * bytesPerRow + 16);
  AddDictionaryInt(aDict, kIOSurfaceAllocSize, totalBytes);
}

/* static */
already_AddRefed<MacIOSurface> MacIOSurface::CreateIOSurface(int aWidth,
                                                             int aHeight,
                                                             bool aHasAlpha) {
  auto props = CFTypeRefPtr<CFMutableDictionaryRef>::WrapUnderCreateRule(
      ::CFDictionaryCreateMutable(kCFAllocatorDefault, 4,
                                  &kCFTypeDictionaryKeyCallBacks,
                                  &kCFTypeDictionaryValueCallBacks));
  if (!props) return nullptr;

  MOZ_ASSERT((size_t)aWidth <= GetMaxWidth());
  MOZ_ASSERT((size_t)aHeight <= GetMaxHeight());

  int32_t bytesPerElem = 4;
  SetSizeProperties(props, aWidth, aHeight, bytesPerElem);

  AddDictionaryInt(props, kIOSurfacePixelFormat,
                   (uint32_t)kCVPixelFormatType_32BGRA);

  CFTypeRefPtr<IOSurfaceRef> surfaceRef =
      CFTypeRefPtr<IOSurfaceRef>::WrapUnderCreateRule(
          ::IOSurfaceCreate(props.get()));

  if (StaticPrefs::gfx_color_management_native_srgb()) {
    IOSurfaceSetValue(surfaceRef.get(), CFSTR("IOSurfaceColorSpace"),
                      kCGColorSpaceSRGB);
  }

  if (!surfaceRef) {
    return nullptr;
  }

  RefPtr<MacIOSurface> ioSurface =
      new MacIOSurface(std::move(surfaceRef), aHasAlpha);

  return ioSurface.forget();
}

size_t CreatePlaneDictionary(CFTypeRefPtr<CFMutableDictionaryRef>& aDict,
                             const gfx::IntSize& aSize, size_t aOffset,
                             size_t aBytesPerPixel) {
  size_t bytesPerRow = IOSurfaceAlignProperty(kIOSurfacePlaneBytesPerRow,
                                              aSize.width * aBytesPerPixel);
  // Add a SIMD register worth of extra bytes to the end of the allocation for
  // SWGL.
  size_t totalBytes = IOSurfaceAlignProperty(kIOSurfacePlaneSize,
                                             aSize.height * bytesPerRow + 16);

  aDict = CFTypeRefPtr<CFMutableDictionaryRef>::WrapUnderCreateRule(
      ::CFDictionaryCreateMutable(kCFAllocatorDefault, 4,
                                  &kCFTypeDictionaryKeyCallBacks,
                                  &kCFTypeDictionaryValueCallBacks));

  AddDictionaryInt(aDict, kIOSurfacePlaneWidth, aSize.width);
  AddDictionaryInt(aDict, kIOSurfacePlaneHeight, aSize.height);
  AddDictionaryInt(aDict, kIOSurfacePlaneBytesPerRow, bytesPerRow);
  AddDictionaryInt(aDict, kIOSurfacePlaneOffset, aOffset);
  AddDictionaryInt(aDict, kIOSurfacePlaneSize, totalBytes);
  AddDictionaryInt(aDict, kIOSurfacePlaneBytesPerElement, aBytesPerPixel);

  return totalBytes;
}

/* static */
already_AddRefed<MacIOSurface> MacIOSurface::CreateNV12OrP010Surface(
    const IntSize& aYSize, const IntSize& aCbCrSize, YUVColorSpace aColorSpace,
    TransferFunction aTransferFunction, ColorRange aColorRange,
    ColorDepth aColorDepth) {
  MOZ_ASSERT(aColorSpace == YUVColorSpace::BT601 ||
             aColorSpace == YUVColorSpace::BT709 ||
             aColorSpace == YUVColorSpace::BT2020);
  MOZ_ASSERT(aColorRange == ColorRange::LIMITED ||
             aColorRange == ColorRange::FULL);
  MOZ_ASSERT(aColorDepth == ColorDepth::COLOR_8 ||
             aColorDepth == ColorDepth::COLOR_10);

  auto props = CFTypeRefPtr<CFMutableDictionaryRef>::WrapUnderCreateRule(
      ::CFDictionaryCreateMutable(kCFAllocatorDefault, 4,
                                  &kCFTypeDictionaryKeyCallBacks,
                                  &kCFTypeDictionaryValueCallBacks));
  if (!props) return nullptr;

  MOZ_ASSERT((size_t)aYSize.width <= GetMaxWidth());
  MOZ_ASSERT((size_t)aYSize.height <= GetMaxHeight());

  AddDictionaryInt(props, kIOSurfaceWidth, aYSize.width);
  AddDictionaryInt(props, kIOSurfaceHeight, aYSize.height);
  ::CFDictionaryAddValue(props.get(), kIOSurfaceIsGlobal, kCFBooleanTrue);

  if (aColorRange == ColorRange::LIMITED) {
    if (aColorDepth == ColorDepth::COLOR_8) {
      AddDictionaryInt(
          props, kIOSurfacePixelFormat,
          (uint32_t)kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange);
    } else {
      AddDictionaryInt(
          props, kIOSurfacePixelFormat,
          (uint32_t)kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange);
    }
  } else {
    if (aColorDepth == ColorDepth::COLOR_8) {
      AddDictionaryInt(
          props, kIOSurfacePixelFormat,
          (uint32_t)kCVPixelFormatType_420YpCbCr8BiPlanarFullRange);
    } else {
      AddDictionaryInt(
          props, kIOSurfacePixelFormat,
          (uint32_t)kCVPixelFormatType_420YpCbCr10BiPlanarFullRange);
    }
  }

  size_t bytesPerPixel = (aColorDepth == ColorDepth::COLOR_8) ? 1 : 2;

  CFTypeRefPtr<CFMutableDictionaryRef> planeProps[2];
  size_t yPlaneBytes =
      CreatePlaneDictionary(planeProps[0], aYSize, 0, bytesPerPixel);
  size_t cbCrOffset =
      IOSurfaceAlignProperty(kIOSurfacePlaneOffset, yPlaneBytes);
  size_t cbCrPlaneBytes = CreatePlaneDictionary(planeProps[1], aCbCrSize,
                                                cbCrOffset, bytesPerPixel * 2);
  size_t totalBytes =
      IOSurfaceAlignProperty(kIOSurfaceAllocSize, cbCrOffset + cbCrPlaneBytes);

  AddDictionaryInt(props, kIOSurfaceAllocSize, totalBytes);

  auto array = CFTypeRefPtr<CFArrayRef>::WrapUnderCreateRule(
      CFArrayCreate(kCFAllocatorDefault, (const void**)planeProps, 2,
                    &kCFTypeArrayCallBacks));
  ::CFDictionaryAddValue(props.get(), kIOSurfacePlaneInfo, array.get());

  CFTypeRefPtr<IOSurfaceRef> surfaceRef =
      CFTypeRefPtr<IOSurfaceRef>::WrapUnderCreateRule(
          ::IOSurfaceCreate(props.get()));

  if (!surfaceRef) {
    return nullptr;
  }

  // Setup the correct YCbCr conversion matrix, color primaries, and transfer
  // functions on the IOSurface, in case we pass this directly to CoreAnimation.
  // For keys and values, we'd like to use values specified by the API, but
  // those are only defined for CVImageBuffers. Luckily, when an image buffer is
  // converted into an IOSurface, the keys are transformed but the values are
  // the same. Since we are creating the IOSurface directly, we use hard-coded
  // keys derived from inspecting the extracted IOSurfaces in the copying case,
  // but we use the API-defined values from CVImageBuffer.
  if (aColorSpace == YUVColorSpace::BT601) {
    IOSurfaceSetValue(surfaceRef.get(), CFSTR("IOSurfaceYCbCrMatrix"),
                      kCVImageBufferYCbCrMatrix_ITU_R_601_4);
  } else if (aColorSpace == YUVColorSpace::BT709) {
    IOSurfaceSetValue(surfaceRef.get(), CFSTR("IOSurfaceYCbCrMatrix"),
                      kCVImageBufferYCbCrMatrix_ITU_R_709_2);
    IOSurfaceSetValue(surfaceRef.get(), CFSTR("IOSurfaceColorPrimaries"),
                      kCVImageBufferColorPrimaries_ITU_R_709_2);
  } else {
    IOSurfaceSetValue(surfaceRef.get(), CFSTR("IOSurfaceYCbCrMatrix"),
                      kCVImageBufferYCbCrMatrix_ITU_R_2020);
    IOSurfaceSetValue(surfaceRef.get(), CFSTR("IOSurfaceColorPrimaries"),
                      kCVImageBufferColorPrimaries_ITU_R_2020);
  }

  // Transfer function is applied independently from the colorSpace.
  IOSurfaceSetValue(
      surfaceRef.get(), CFSTR("IOSurfaceTransferFunction"),
      gfxMacUtils::CFStringForTransferFunction(aTransferFunction));

  // Override the color space to be the same as the main display, so that
  // CoreAnimation won't try to do any color correction (from the IOSurface
  // space, to the display). In the future we may want to try specifying this
  // correctly, but probably only once we do the same for videos drawn through
  // our gfx code.
  auto colorSpace = CFTypeRefPtr<CGColorSpaceRef>::WrapUnderCreateRule(
      CGDisplayCopyColorSpace(CGMainDisplayID()));
  auto colorData = CFTypeRefPtr<CFDataRef>::WrapUnderCreateRule(
      CGColorSpaceCopyICCData(colorSpace.get()));
  IOSurfaceSetValue(surfaceRef.get(), CFSTR("IOSurfaceColorSpace"),
                    colorData.get());

  RefPtr<MacIOSurface> ioSurface =
      new MacIOSurface(std::move(surfaceRef), false, aColorSpace);

  return ioSurface.forget();
}

/* static */
already_AddRefed<MacIOSurface> MacIOSurface::CreateYUV422Surface(
    const IntSize& aSize, YUVColorSpace aColorSpace, ColorRange aColorRange) {
  MOZ_ASSERT(aColorSpace == YUVColorSpace::BT601 ||
             aColorSpace == YUVColorSpace::BT709);
  MOZ_ASSERT(aColorRange == ColorRange::LIMITED ||
             aColorRange == ColorRange::FULL);

  auto props = CFTypeRefPtr<CFMutableDictionaryRef>::WrapUnderCreateRule(
      ::CFDictionaryCreateMutable(kCFAllocatorDefault, 4,
                                  &kCFTypeDictionaryKeyCallBacks,
                                  &kCFTypeDictionaryValueCallBacks));
  if (!props) return nullptr;

  MOZ_ASSERT((size_t)aSize.width <= GetMaxWidth());
  MOZ_ASSERT((size_t)aSize.height <= GetMaxHeight());

  SetSizeProperties(props, aSize.width, aSize.height, 2);

  if (aColorRange == ColorRange::LIMITED) {
    AddDictionaryInt(props, kIOSurfacePixelFormat,
                     (uint32_t)kCVPixelFormatType_422YpCbCr8_yuvs);
  } else {
    AddDictionaryInt(props, kIOSurfacePixelFormat,
                     (uint32_t)kCVPixelFormatType_422YpCbCr8FullRange);
  }

  CFTypeRefPtr<IOSurfaceRef> surfaceRef =
      CFTypeRefPtr<IOSurfaceRef>::WrapUnderCreateRule(
          ::IOSurfaceCreate(props.get()));

  if (!surfaceRef) {
    return nullptr;
  }

  // Setup the correct YCbCr conversion matrix on the IOSurface, in case we pass
  // this directly to CoreAnimation.
  if (aColorSpace == YUVColorSpace::BT601) {
    IOSurfaceSetValue(surfaceRef.get(), CFSTR("IOSurfaceYCbCrMatrix"),
                      CFSTR("ITU_R_601_4"));
  } else {
    IOSurfaceSetValue(surfaceRef.get(), CFSTR("IOSurfaceYCbCrMatrix"),
                      CFSTR("ITU_R_709_2"));
  }
  // Override the color space to be the same as the main display, so that
  // CoreAnimation won't try to do any color correction (from the IOSurface
  // space, to the display). In the future we may want to try specifying this
  // correctly, but probably only once we do the same for videos drawn through
  // our gfx code.
  auto colorSpace = CFTypeRefPtr<CGColorSpaceRef>::WrapUnderCreateRule(
      CGDisplayCopyColorSpace(CGMainDisplayID()));
  auto colorData = CFTypeRefPtr<CFDataRef>::WrapUnderCreateRule(
      CGColorSpaceCopyICCData(colorSpace.get()));
  IOSurfaceSetValue(surfaceRef.get(), CFSTR("IOSurfaceColorSpace"),
                    colorData.get());

  RefPtr<MacIOSurface> ioSurface =
      new MacIOSurface(std::move(surfaceRef), false, aColorSpace);

  return ioSurface.forget();
}

/* static */
already_AddRefed<MacIOSurface> MacIOSurface::LookupSurface(
    IOSurfaceID aIOSurfaceID, bool aHasAlpha, gfx::YUVColorSpace aColorSpace) {
  CFTypeRefPtr<IOSurfaceRef> surfaceRef =
      CFTypeRefPtr<IOSurfaceRef>::WrapUnderCreateRule(
          ::IOSurfaceLookup(aIOSurfaceID));
  if (!surfaceRef) return nullptr;

  RefPtr<MacIOSurface> ioSurface =
      new MacIOSurface(std::move(surfaceRef), aHasAlpha, aColorSpace);

  return ioSurface.forget();
}

IOSurfaceID MacIOSurface::GetIOSurfaceID() const {
  return ::IOSurfaceGetID(mIOSurfaceRef.get());
}

void* MacIOSurface::GetBaseAddress() const {
  return ::IOSurfaceGetBaseAddress(mIOSurfaceRef.get());
}

void* MacIOSurface::GetBaseAddressOfPlane(size_t aPlaneIndex) const {
  return ::IOSurfaceGetBaseAddressOfPlane(mIOSurfaceRef.get(), aPlaneIndex);
}

size_t MacIOSurface::GetWidth(size_t plane) const {
  return GetDevicePixelWidth(plane);
}

size_t MacIOSurface::GetHeight(size_t plane) const {
  return GetDevicePixelHeight(plane);
}

size_t MacIOSurface::GetPlaneCount() const {
  return ::IOSurfaceGetPlaneCount(mIOSurfaceRef.get());
}

/*static*/
size_t MacIOSurface::GetMaxWidth() {
  return ::IOSurfaceGetPropertyMaximum(kIOSurfaceWidth);
}

/*static*/
size_t MacIOSurface::GetMaxHeight() {
  return ::IOSurfaceGetPropertyMaximum(kIOSurfaceHeight);
}

size_t MacIOSurface::GetDevicePixelWidth(size_t plane) const {
  return ::IOSurfaceGetWidthOfPlane(mIOSurfaceRef.get(), plane);
}

size_t MacIOSurface::GetDevicePixelHeight(size_t plane) const {
  return ::IOSurfaceGetHeightOfPlane(mIOSurfaceRef.get(), plane);
}

size_t MacIOSurface::GetBytesPerRow(size_t plane) const {
  return ::IOSurfaceGetBytesPerRowOfPlane(mIOSurfaceRef.get(), plane);
}

size_t MacIOSurface::GetAllocSize() const {
  return ::IOSurfaceGetAllocSize(mIOSurfaceRef.get());
}

OSType MacIOSurface::GetPixelFormat() const {
  return ::IOSurfaceGetPixelFormat(mIOSurfaceRef.get());
}

void MacIOSurface::IncrementUseCount() {
  ::IOSurfaceIncrementUseCount(mIOSurfaceRef.get());
}

void MacIOSurface::DecrementUseCount() {
  ::IOSurfaceDecrementUseCount(mIOSurfaceRef.get());
}

void MacIOSurface::Lock(bool aReadOnly) {
  MOZ_RELEASE_ASSERT(!mIsLocked, "double MacIOSurface lock");
  ::IOSurfaceLock(mIOSurfaceRef.get(), aReadOnly ? kIOSurfaceLockReadOnly : 0,
                  nullptr);
  mIsLocked = true;
}

void MacIOSurface::Unlock(bool aReadOnly) {
  MOZ_RELEASE_ASSERT(mIsLocked, "MacIOSurface unlock without being locked");
  ::IOSurfaceUnlock(mIOSurfaceRef.get(), aReadOnly ? kIOSurfaceLockReadOnly : 0,
                    nullptr);
  mIsLocked = false;
}

using mozilla::gfx::ColorDepth;
using mozilla::gfx::IntSize;
using mozilla::gfx::SourceSurface;
using mozilla::gfx::SurfaceFormat;

static void MacIOSurfaceBufferDeallocator(void* aClosure) {
  MOZ_ASSERT(aClosure);

  delete[] static_cast<unsigned char*>(aClosure);
}

already_AddRefed<SourceSurface> MacIOSurface::GetAsSurface() {
  Lock();
  size_t bytesPerRow = GetBytesPerRow();
  size_t ioWidth = GetDevicePixelWidth();
  size_t ioHeight = GetDevicePixelHeight();

  unsigned char* ioData = (unsigned char*)GetBaseAddress();
  auto* dataCpy =
      new unsigned char[bytesPerRow * ioHeight / sizeof(unsigned char)];
  for (size_t i = 0; i < ioHeight; i++) {
    memcpy(dataCpy + i * bytesPerRow, ioData + i * bytesPerRow, ioWidth * 4);
  }

  Unlock();

  SurfaceFormat format = HasAlpha() ? mozilla::gfx::SurfaceFormat::B8G8R8A8
                                    : mozilla::gfx::SurfaceFormat::B8G8R8X8;

  RefPtr<mozilla::gfx::DataSourceSurface> surf =
      mozilla::gfx::Factory::CreateWrappingDataSourceSurface(
          dataCpy, bytesPerRow, IntSize(ioWidth, ioHeight), format,
          &MacIOSurfaceBufferDeallocator, static_cast<void*>(dataCpy));

  return surf.forget();
}

already_AddRefed<mozilla::gfx::DrawTarget> MacIOSurface::GetAsDrawTargetLocked(
    mozilla::gfx::BackendType aBackendType) {
  MOZ_RELEASE_ASSERT(
      IsLocked(),
      "Only call GetAsDrawTargetLocked while the surface is locked.");

  size_t bytesPerRow = GetBytesPerRow();
  size_t ioWidth = GetDevicePixelWidth();
  size_t ioHeight = GetDevicePixelHeight();
  unsigned char* ioData = (unsigned char*)GetBaseAddress();
  SurfaceFormat format = GetFormat();
  return mozilla::gfx::Factory::CreateDrawTargetForData(
      aBackendType, ioData, IntSize(ioWidth, ioHeight), bytesPerRow, format);
}

SurfaceFormat MacIOSurface::GetFormat() const {
  switch (GetPixelFormat()) {
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
    case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
      return SurfaceFormat::NV12;
    case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
    case kCVPixelFormatType_420YpCbCr10BiPlanarFullRange:
      return SurfaceFormat::P010;
    case kCVPixelFormatType_422YpCbCr8_yuvs:
    case kCVPixelFormatType_422YpCbCr8FullRange:
      return SurfaceFormat::YUV422;
    case kCVPixelFormatType_32BGRA:
      return HasAlpha() ? SurfaceFormat::B8G8R8A8 : SurfaceFormat::B8G8R8X8;
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown format");
      return SurfaceFormat::B8G8R8A8;
  }
}

SurfaceFormat MacIOSurface::GetReadFormat() const {
  SurfaceFormat format = GetFormat();
  if (format == SurfaceFormat::YUV422) {
    return SurfaceFormat::R8G8B8X8;
  }
  return format;
}

ColorDepth MacIOSurface::GetColorDepth() const {
  switch (GetPixelFormat()) {
    case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
    case kCVPixelFormatType_420YpCbCr10BiPlanarFullRange:
      return ColorDepth::COLOR_10;
    default:
      return ColorDepth::COLOR_8;
  }
}

CGLError MacIOSurface::CGLTexImageIOSurface2D(CGLContextObj ctx, GLenum target,
                                              GLenum internalFormat,
                                              GLsizei width, GLsizei height,
                                              GLenum format, GLenum type,
                                              GLuint plane) const {
  return ::CGLTexImageIOSurface2D(ctx, target, internalFormat, width, height,
                                  format, type, mIOSurfaceRef.get(), plane);
}

CGLError MacIOSurface::CGLTexImageIOSurface2D(
    mozilla::gl::GLContext* aGL, CGLContextObj ctx, size_t plane,
    mozilla::gfx::SurfaceFormat* aOutReadFormat) {
  MOZ_ASSERT(plane >= 0);
  bool isCompatibilityProfile = aGL->IsCompatibilityProfile();
  OSType pixelFormat = GetPixelFormat();

  GLenum internalFormat;
  GLenum format;
  GLenum type;
  if (pixelFormat == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange ||
      pixelFormat == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange) {
    MOZ_ASSERT(GetPlaneCount() == 2);
    MOZ_ASSERT(plane < 2);

    // The LOCAL_GL_LUMINANCE and LOCAL_GL_LUMINANCE_ALPHA are the deprecated
    // format. So, use LOCAL_GL_RED and LOCAL_GL_RB if we use core profile.
    // https://www.khronos.org/opengl/wiki/Image_Format#Legacy_Image_Formats
    if (plane == 0) {
      internalFormat = format =
          (isCompatibilityProfile) ? (LOCAL_GL_LUMINANCE) : (LOCAL_GL_RED);
    } else {
      internalFormat = format =
          (isCompatibilityProfile) ? (LOCAL_GL_LUMINANCE_ALPHA) : (LOCAL_GL_RG);
    }
    type = LOCAL_GL_UNSIGNED_BYTE;
    if (aOutReadFormat) {
      *aOutReadFormat = mozilla::gfx::SurfaceFormat::NV12;
    }
  } else if (pixelFormat == kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange ||
             pixelFormat == kCVPixelFormatType_420YpCbCr10BiPlanarFullRange) {
    MOZ_ASSERT(GetPlaneCount() == 2);
    MOZ_ASSERT(plane < 2);

    // The LOCAL_GL_LUMINANCE and LOCAL_GL_LUMINANCE_ALPHA are the deprecated
    // format. So, use LOCAL_GL_RED and LOCAL_GL_RB if we use core profile.
    // https://www.khronos.org/opengl/wiki/Image_Format#Legacy_Image_Formats
    if (plane == 0) {
      internalFormat = format =
          (isCompatibilityProfile) ? (LOCAL_GL_LUMINANCE) : (LOCAL_GL_RED);
    } else {
      internalFormat = format =
          (isCompatibilityProfile) ? (LOCAL_GL_LUMINANCE_ALPHA) : (LOCAL_GL_RG);
    }
    type = LOCAL_GL_UNSIGNED_SHORT;
    if (aOutReadFormat) {
      *aOutReadFormat = mozilla::gfx::SurfaceFormat::P010;
    }
  } else if (pixelFormat == kCVPixelFormatType_422YpCbCr8_yuvs ||
             pixelFormat == kCVPixelFormatType_422YpCbCr8FullRange) {
    MOZ_ASSERT(plane == 0);
    // The YCBCR_422_APPLE ext is only available in compatibility profile. So,
    // we should use RGB_422_APPLE for core profile. The difference between
    // YCBCR_422_APPLE and RGB_422_APPLE is that the YCBCR_422_APPLE converts
    // the YCbCr value to RGB with REC 601 conversion. But the RGB_422_APPLE
    // doesn't contain color conversion. You should do the color conversion by
    // yourself for RGB_422_APPLE.
    //
    // https://www.khronos.org/registry/OpenGL/extensions/APPLE/APPLE_ycbcr_422.txt
    // https://www.khronos.org/registry/OpenGL/extensions/APPLE/APPLE_rgb_422.txt
    if (isCompatibilityProfile) {
      format = LOCAL_GL_YCBCR_422_APPLE;
      if (aOutReadFormat) {
        *aOutReadFormat = mozilla::gfx::SurfaceFormat::R8G8B8X8;
      }
    } else {
      format = LOCAL_GL_RGB_422_APPLE;
      if (aOutReadFormat) {
        *aOutReadFormat = mozilla::gfx::SurfaceFormat::YUV422;
      }
    }
    internalFormat = LOCAL_GL_RGB;
    type = LOCAL_GL_UNSIGNED_SHORT_8_8_REV_APPLE;
  } else {
    MOZ_ASSERT(plane == 0);

    internalFormat = HasAlpha() ? LOCAL_GL_RGBA : LOCAL_GL_RGB;
    format = LOCAL_GL_BGRA;
    type = LOCAL_GL_UNSIGNED_INT_8_8_8_8_REV;
    if (aOutReadFormat) {
      *aOutReadFormat = HasAlpha() ? mozilla::gfx::SurfaceFormat::R8G8B8A8
                                   : mozilla::gfx::SurfaceFormat::R8G8B8X8;
    }
  }

  auto err =
      CGLTexImageIOSurface2D(ctx, LOCAL_GL_TEXTURE_RECTANGLE_ARB,
                             internalFormat, GetDevicePixelWidth(plane),
                             GetDevicePixelHeight(plane), format, type, plane);
  if (err) {
    const auto formatChars = (const char*)&pixelFormat;
    const char formatStr[] = {formatChars[3], formatChars[2], formatChars[1],
                              formatChars[0], 0};
    const nsPrintfCString errStr(
        "CGLTexImageIOSurface2D(context, target, 0x%04x,"
        " %u, %u, 0x%04x, 0x%04x, iosurfPtr, %u) -> %i",
        internalFormat, uint32_t(GetDevicePixelWidth(plane)),
        uint32_t(GetDevicePixelHeight(plane)), format, type,
        (unsigned int)plane, err);
    gfxCriticalError() << errStr.get() << " (iosurf format: " << formatStr
                       << ")";
  }
  return err;
}

void MacIOSurface::SetColorSpace(const mozilla::gfx::ColorSpace2 cs) const {
  Maybe<CFStringRef> str;
  switch (cs) {
    case gfx::ColorSpace2::UNKNOWN:
      break;
    case gfx::ColorSpace2::SRGB:
      str = Some(kCGColorSpaceSRGB);
      break;
    case gfx::ColorSpace2::DISPLAY_P3:
      str = Some(kCGColorSpaceDisplayP3);
      break;
    case gfx::ColorSpace2::BT601_525:  // Doesn't really have a better option.
    case gfx::ColorSpace2::BT709:
      str = Some(kCGColorSpaceITUR_709);
      break;
    case gfx::ColorSpace2::BT2020:
      str = Some(kCGColorSpaceITUR_2020);
      break;
  }
  if (str) {
    IOSurfaceSetValue(mIOSurfaceRef.get(), CFSTR("IOSurfaceColorSpace"), *str);
  }
}
