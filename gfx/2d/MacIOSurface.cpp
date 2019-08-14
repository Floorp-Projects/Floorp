/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MacIOSurface.h"
#include <OpenGL/gl.h>
#include <QuartzCore/QuartzCore.h>
#include <dlfcn.h>
#include "GLConsts.h"
#include "GLContextCGL.h"
#include "mozilla/Assertions.h"
#include "mozilla/RefPtr.h"

using namespace mozilla;
// IOSurface signatures
#define IOSURFACE_FRAMEWORK_PATH \
  "/System/Library/Frameworks/IOSurface.framework/IOSurface"
#define OPENGL_FRAMEWORK_PATH \
  "/System/Library/Frameworks/OpenGL.framework/OpenGL"
#define COREVIDEO_FRAMEWORK_PATH                                         \
  "/System/Library/Frameworks/ApplicationServices.framework/Frameworks/" \
  "CoreVideo.framework/CoreVideo"

#define GET_CONST(const_name) \
  ((CFStringRef*)dlsym(sIOSurfaceFramework, const_name))
#define GET_IOSYM(dest, sym_name) \
  (typeof(dest)) dlsym(sIOSurfaceFramework, sym_name)
#define GET_CGLSYM(dest, sym_name) \
  (typeof(dest)) dlsym(sOpenGLFramework, sym_name)
#define GET_CVSYM(dest, sym_name) \
  (typeof(dest)) dlsym(sCoreVideoFramework, sym_name)

MacIOSurfaceLib::LibraryUnloader MacIOSurfaceLib::sLibraryUnloader;
bool MacIOSurfaceLib::isLoaded = false;
void* MacIOSurfaceLib::sIOSurfaceFramework;
void* MacIOSurfaceLib::sOpenGLFramework;
void* MacIOSurfaceLib::sCoreVideoFramework;
IOSurfaceCreateFunc MacIOSurfaceLib::sCreate;
IOSurfaceGetIDFunc MacIOSurfaceLib::sGetID;
IOSurfaceLookupFunc MacIOSurfaceLib::sLookup;
IOSurfaceGetBaseAddressFunc MacIOSurfaceLib::sGetBaseAddress;
IOSurfaceGetBaseAddressOfPlaneFunc MacIOSurfaceLib::sGetBaseAddressOfPlane;
IOSurfaceSizePlaneTFunc MacIOSurfaceLib::sWidth;
IOSurfaceSizePlaneTFunc MacIOSurfaceLib::sHeight;
IOSurfaceSizeTFunc MacIOSurfaceLib::sPlaneCount;
IOSurfaceSizePlaneTFunc MacIOSurfaceLib::sBytesPerRow;
IOSurfaceGetPropertyMaximumFunc MacIOSurfaceLib::sGetPropertyMaximum;
IOSurfaceVoidFunc MacIOSurfaceLib::sIncrementUseCount;
IOSurfaceVoidFunc MacIOSurfaceLib::sDecrementUseCount;
IOSurfaceLockFunc MacIOSurfaceLib::sLock;
IOSurfaceUnlockFunc MacIOSurfaceLib::sUnlock;
CGLTexImageIOSurface2DFunc MacIOSurfaceLib::sTexImage;
CVPixelBufferGetIOSurfaceFunc MacIOSurfaceLib::sCVPixelBufferGetIOSurface;
IOSurfacePixelFormatFunc MacIOSurfaceLib::sPixelFormat;

CFStringRef MacIOSurfaceLib::kPropWidth;
CFStringRef MacIOSurfaceLib::kPropHeight;
CFStringRef MacIOSurfaceLib::kPropBytesPerElem;
CFStringRef MacIOSurfaceLib::kPropBytesPerRow;
CFStringRef MacIOSurfaceLib::kPropIsGlobal;

bool MacIOSurfaceLib::isInit() {
  // Guard against trying to reload the library
  // if it is not available.
  if (!isLoaded) LoadLibrary();
  MOZ_ASSERT(sIOSurfaceFramework);
  return sIOSurfaceFramework;
}

IOSurfacePtr MacIOSurfaceLib::IOSurfaceCreate(CFDictionaryRef properties) {
  return sCreate(properties);
}

IOSurfacePtr MacIOSurfaceLib::IOSurfaceLookup(IOSurfaceID aIOSurfaceID) {
  return sLookup(aIOSurfaceID);
}

IOSurfaceID MacIOSurfaceLib::IOSurfaceGetID(IOSurfacePtr aIOSurfacePtr) {
  return sGetID(aIOSurfacePtr);
}

void* MacIOSurfaceLib::IOSurfaceGetBaseAddress(IOSurfacePtr aIOSurfacePtr) {
  return sGetBaseAddress(aIOSurfacePtr);
}

void* MacIOSurfaceLib::IOSurfaceGetBaseAddressOfPlane(
    IOSurfacePtr aIOSurfacePtr, size_t planeIndex) {
  return sGetBaseAddressOfPlane(aIOSurfacePtr, planeIndex);
}

size_t MacIOSurfaceLib::IOSurfaceGetPlaneCount(IOSurfacePtr aIOSurfacePtr) {
  return sPlaneCount(aIOSurfacePtr);
}

size_t MacIOSurfaceLib::IOSurfaceGetWidth(IOSurfacePtr aIOSurfacePtr,
                                          size_t plane) {
  return sWidth(aIOSurfacePtr, plane);
}

size_t MacIOSurfaceLib::IOSurfaceGetHeight(IOSurfacePtr aIOSurfacePtr,
                                           size_t plane) {
  return sHeight(aIOSurfacePtr, plane);
}

size_t MacIOSurfaceLib::IOSurfaceGetBytesPerRow(IOSurfacePtr aIOSurfacePtr,
                                                size_t plane) {
  return sBytesPerRow(aIOSurfacePtr, plane);
}

size_t MacIOSurfaceLib::IOSurfaceGetPropertyMaximum(CFStringRef property) {
  return sGetPropertyMaximum(property);
}

OSType MacIOSurfaceLib::IOSurfaceGetPixelFormat(IOSurfacePtr aIOSurfacePtr) {
  return sPixelFormat(aIOSurfacePtr);
}

IOReturn MacIOSurfaceLib::IOSurfaceLock(IOSurfacePtr aIOSurfacePtr,
                                        uint32_t options, uint32_t* seed) {
  return sLock(aIOSurfacePtr, options, seed);
}

IOReturn MacIOSurfaceLib::IOSurfaceUnlock(IOSurfacePtr aIOSurfacePtr,
                                          uint32_t options, uint32_t* seed) {
  return sUnlock(aIOSurfacePtr, options, seed);
}

void MacIOSurfaceLib::IOSurfaceIncrementUseCount(IOSurfacePtr aIOSurfacePtr) {
  sIncrementUseCount(aIOSurfacePtr);
}

void MacIOSurfaceLib::IOSurfaceDecrementUseCount(IOSurfacePtr aIOSurfacePtr) {
  sDecrementUseCount(aIOSurfacePtr);
}

CGLError MacIOSurfaceLib::CGLTexImageIOSurface2D(
    CGLContextObj ctxt, GLenum target, GLenum internalFormat, GLsizei width,
    GLsizei height, GLenum format, GLenum type, IOSurfacePtr ioSurface,
    GLuint plane) {
  return sTexImage(ctxt, target, internalFormat, width, height, format, type,
                   ioSurface, plane);
}

IOSurfacePtr MacIOSurfaceLib::CVPixelBufferGetIOSurface(
    CVPixelBufferRef aPixelBuffer) {
  return sCVPixelBufferGetIOSurface(aPixelBuffer);
}

CFStringRef MacIOSurfaceLib::GetIOConst(const char* symbole) {
  CFStringRef* address = (CFStringRef*)dlsym(sIOSurfaceFramework, symbole);
  if (!address) return nullptr;

  return *address;
}

void MacIOSurfaceLib::LoadLibrary() {
  if (isLoaded) {
    return;
  }
  isLoaded = true;
  sIOSurfaceFramework =
      dlopen(IOSURFACE_FRAMEWORK_PATH, RTLD_LAZY | RTLD_LOCAL);
  sOpenGLFramework = dlopen(OPENGL_FRAMEWORK_PATH, RTLD_LAZY | RTLD_LOCAL);

  sCoreVideoFramework =
      dlopen(COREVIDEO_FRAMEWORK_PATH, RTLD_LAZY | RTLD_LOCAL);

  if (!sIOSurfaceFramework || !sOpenGLFramework || !sCoreVideoFramework) {
    if (sIOSurfaceFramework) dlclose(sIOSurfaceFramework);
    if (sOpenGLFramework) dlclose(sOpenGLFramework);
    if (sCoreVideoFramework) dlclose(sCoreVideoFramework);
    sIOSurfaceFramework = nullptr;
    sOpenGLFramework = nullptr;
    sCoreVideoFramework = nullptr;
    return;
  }

  kPropWidth = GetIOConst("kIOSurfaceWidth");
  kPropHeight = GetIOConst("kIOSurfaceHeight");
  kPropBytesPerElem = GetIOConst("kIOSurfaceBytesPerElement");
  kPropBytesPerRow = GetIOConst("kIOSurfaceBytesPerRow");
  kPropIsGlobal = GetIOConst("kIOSurfaceIsGlobal");
  sCreate = GET_IOSYM(sCreate, "IOSurfaceCreate");
  sGetID = GET_IOSYM(sGetID, "IOSurfaceGetID");
  sWidth = GET_IOSYM(sWidth, "IOSurfaceGetWidthOfPlane");
  sHeight = GET_IOSYM(sHeight, "IOSurfaceGetHeightOfPlane");
  sBytesPerRow = GET_IOSYM(sBytesPerRow, "IOSurfaceGetBytesPerRowOfPlane");
  sGetPropertyMaximum =
      GET_IOSYM(sGetPropertyMaximum, "IOSurfaceGetPropertyMaximum");
  sLookup = GET_IOSYM(sLookup, "IOSurfaceLookup");
  sLock = GET_IOSYM(sLock, "IOSurfaceLock");
  sUnlock = GET_IOSYM(sUnlock, "IOSurfaceUnlock");
  sIncrementUseCount =
      GET_IOSYM(sIncrementUseCount, "IOSurfaceIncrementUseCount");
  sDecrementUseCount =
      GET_IOSYM(sDecrementUseCount, "IOSurfaceDecrementUseCount");
  sGetBaseAddress = GET_IOSYM(sGetBaseAddress, "IOSurfaceGetBaseAddress");
  sGetBaseAddressOfPlane =
      GET_IOSYM(sGetBaseAddressOfPlane, "IOSurfaceGetBaseAddressOfPlane");
  sPlaneCount = GET_IOSYM(sPlaneCount, "IOSurfaceGetPlaneCount");
  sPixelFormat = GET_IOSYM(sPixelFormat, "IOSurfaceGetPixelFormat");

  sTexImage = GET_CGLSYM(sTexImage, "CGLTexImageIOSurface2D");

  sCVPixelBufferGetIOSurface =
      GET_CVSYM(sCVPixelBufferGetIOSurface, "CVPixelBufferGetIOSurface");

  if (!sCreate || !sGetID || !sLookup || !sTexImage || !sGetBaseAddress ||
      !sGetBaseAddressOfPlane || !sPlaneCount || !kPropWidth || !kPropHeight ||
      !kPropBytesPerElem || !kPropIsGlobal || !sLock || !sUnlock ||
      !sIncrementUseCount || !sDecrementUseCount || !sWidth || !sHeight ||
      !kPropBytesPerRow || !sBytesPerRow || !sGetPropertyMaximum ||
      !sCVPixelBufferGetIOSurface) {
    CloseLibrary();
  }
}

void MacIOSurfaceLib::CloseLibrary() {
  if (sIOSurfaceFramework) {
    dlclose(sIOSurfaceFramework);
  }
  if (sOpenGLFramework) {
    dlclose(sOpenGLFramework);
  }
  if (sCoreVideoFramework) {
    dlclose(sCoreVideoFramework);
  }
  sIOSurfaceFramework = nullptr;
  sOpenGLFramework = nullptr;
  sCoreVideoFramework = nullptr;
}

MacIOSurface::MacIOSurface(IOSurfacePtr aIOSurfacePtr,
                           double aContentsScaleFactor, bool aHasAlpha,
                           gfx::YUVColorSpace aColorSpace)
    : mIOSurfacePtr(aIOSurfacePtr),
      mContentsScaleFactor(aContentsScaleFactor),
      mHasAlpha(aHasAlpha),
      mColorSpace(aColorSpace) {
  CFRetain(mIOSurfacePtr);
  IncrementUseCount();
}

MacIOSurface::~MacIOSurface() {
  DecrementUseCount();
  CFRelease(mIOSurfacePtr);
}

/* static */
already_AddRefed<MacIOSurface> MacIOSurface::CreateIOSurface(
    int aWidth, int aHeight, double aContentsScaleFactor, bool aHasAlpha) {
  if (!MacIOSurfaceLib::isInit() || aContentsScaleFactor <= 0) return nullptr;

  CFMutableDictionaryRef props = ::CFDictionaryCreateMutable(
      kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks);
  if (!props) return nullptr;

  MOZ_ASSERT((size_t)aWidth <= GetMaxWidth());
  MOZ_ASSERT((size_t)aHeight <= GetMaxHeight());

  int32_t bytesPerElem = 4;
  size_t intScaleFactor = ceil(aContentsScaleFactor);
  aWidth *= intScaleFactor;
  aHeight *= intScaleFactor;
  CFNumberRef cfWidth = ::CFNumberCreate(nullptr, kCFNumberSInt32Type, &aWidth);
  CFNumberRef cfHeight =
      ::CFNumberCreate(nullptr, kCFNumberSInt32Type, &aHeight);
  CFNumberRef cfBytesPerElem =
      ::CFNumberCreate(nullptr, kCFNumberSInt32Type, &bytesPerElem);
  ::CFDictionaryAddValue(props, MacIOSurfaceLib::kPropWidth, cfWidth);
  ::CFRelease(cfWidth);
  ::CFDictionaryAddValue(props, MacIOSurfaceLib::kPropHeight, cfHeight);
  ::CFRelease(cfHeight);
  ::CFDictionaryAddValue(props, MacIOSurfaceLib::kPropBytesPerElem,
                         cfBytesPerElem);
  ::CFRelease(cfBytesPerElem);
  ::CFDictionaryAddValue(props, MacIOSurfaceLib::kPropIsGlobal, kCFBooleanTrue);

  IOSurfacePtr surfaceRef = MacIOSurfaceLib::IOSurfaceCreate(props);
  ::CFRelease(props);

  if (!surfaceRef) return nullptr;

  RefPtr<MacIOSurface> ioSurface =
      new MacIOSurface(surfaceRef, aContentsScaleFactor, aHasAlpha);

  // Release the IOSurface because MacIOSurface retained it
  CFRelease(surfaceRef);

  return ioSurface.forget();
}

/* static */
already_AddRefed<MacIOSurface> MacIOSurface::LookupSurface(
    IOSurfaceID aIOSurfaceID, double aContentsScaleFactor, bool aHasAlpha,
    gfx::YUVColorSpace aColorSpace) {
  if (!MacIOSurfaceLib::isInit() || aContentsScaleFactor <= 0) return nullptr;

  IOSurfacePtr surfaceRef = MacIOSurfaceLib::IOSurfaceLookup(aIOSurfaceID);
  if (!surfaceRef) return nullptr;

  RefPtr<MacIOSurface> ioSurface = new MacIOSurface(
      surfaceRef, aContentsScaleFactor, aHasAlpha, aColorSpace);

  // Release the IOSurface because MacIOSurface retained it
  CFRelease(surfaceRef);

  return ioSurface.forget();
}

IOSurfaceID MacIOSurface::GetIOSurfaceID() const {
  return MacIOSurfaceLib::IOSurfaceGetID(mIOSurfacePtr);
}

void* MacIOSurface::GetBaseAddress() const {
  return MacIOSurfaceLib::IOSurfaceGetBaseAddress(mIOSurfacePtr);
}

void* MacIOSurface::GetBaseAddressOfPlane(size_t aPlaneIndex) const {
  return MacIOSurfaceLib::IOSurfaceGetBaseAddressOfPlane(mIOSurfacePtr,
                                                         aPlaneIndex);
}

size_t MacIOSurface::GetWidth(size_t plane) const {
  size_t intScaleFactor = ceil(mContentsScaleFactor);
  return GetDevicePixelWidth(plane) / intScaleFactor;
}

size_t MacIOSurface::GetHeight(size_t plane) const {
  size_t intScaleFactor = ceil(mContentsScaleFactor);
  return GetDevicePixelHeight(plane) / intScaleFactor;
}

size_t MacIOSurface::GetPlaneCount() const {
  return MacIOSurfaceLib::IOSurfaceGetPlaneCount(mIOSurfacePtr);
}

/*static*/
size_t MacIOSurface::GetMaxWidth() {
  if (!MacIOSurfaceLib::isInit()) return -1;
  return MacIOSurfaceLib::IOSurfaceGetPropertyMaximum(
      MacIOSurfaceLib::kPropWidth);
}

/*static*/
size_t MacIOSurface::GetMaxHeight() {
  if (!MacIOSurfaceLib::isInit()) return -1;
  return MacIOSurfaceLib::IOSurfaceGetPropertyMaximum(
      MacIOSurfaceLib::kPropHeight);
}

size_t MacIOSurface::GetDevicePixelWidth(size_t plane) const {
  return MacIOSurfaceLib::IOSurfaceGetWidth(mIOSurfacePtr, plane);
}

size_t MacIOSurface::GetDevicePixelHeight(size_t plane) const {
  return MacIOSurfaceLib::IOSurfaceGetHeight(mIOSurfacePtr, plane);
}

size_t MacIOSurface::GetBytesPerRow(size_t plane) const {
  return MacIOSurfaceLib::IOSurfaceGetBytesPerRow(mIOSurfacePtr, plane);
}

OSType MacIOSurface::GetPixelFormat() const {
  return MacIOSurfaceLib::IOSurfaceGetPixelFormat(mIOSurfacePtr);
}

void MacIOSurface::IncrementUseCount() {
  MacIOSurfaceLib::IOSurfaceIncrementUseCount(mIOSurfacePtr);
}

void MacIOSurface::DecrementUseCount() {
  MacIOSurfaceLib::IOSurfaceDecrementUseCount(mIOSurfacePtr);
}

#define READ_ONLY 0x1
void MacIOSurface::Lock(bool aReadOnly) {
  MacIOSurfaceLib::IOSurfaceLock(mIOSurfacePtr, aReadOnly ? READ_ONLY : 0,
                                 nullptr);
}

void MacIOSurface::Unlock(bool aReadOnly) {
  MacIOSurfaceLib::IOSurfaceUnlock(mIOSurfacePtr, aReadOnly ? READ_ONLY : 0,
                                   nullptr);
}

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

SurfaceFormat MacIOSurface::GetFormat() const {
  OSType pixelFormat = GetPixelFormat();
  if (pixelFormat == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange ||
      pixelFormat == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange) {
    return SurfaceFormat::NV12;
  } else if (pixelFormat == kCVPixelFormatType_422YpCbCr8) {
    return SurfaceFormat::YUV422;
  } else {
    return HasAlpha() ? SurfaceFormat::R8G8B8A8 : SurfaceFormat::R8G8B8X8;
  }
}

SurfaceFormat MacIOSurface::GetReadFormat() const {
  OSType pixelFormat = GetPixelFormat();
  if (pixelFormat == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange ||
      pixelFormat == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange) {
    return SurfaceFormat::NV12;
  } else if (pixelFormat == kCVPixelFormatType_422YpCbCr8) {
    return SurfaceFormat::R8G8B8X8;
  } else {
    return HasAlpha() ? SurfaceFormat::R8G8B8A8 : SurfaceFormat::R8G8B8X8;
  }
}

CGLError MacIOSurface::CGLTexImageIOSurface2D(CGLContextObj ctx, GLenum target,
                                              GLenum internalFormat,
                                              GLsizei width, GLsizei height,
                                              GLenum format, GLenum type,
                                              GLuint plane) const {
  return MacIOSurfaceLib::CGLTexImageIOSurface2D(ctx, target, internalFormat,
                                                 width, height, format, type,
                                                 mIOSurfacePtr, plane);
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
  } else if (pixelFormat == kCVPixelFormatType_422YpCbCr8) {
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
    type = LOCAL_GL_UNSIGNED_SHORT_8_8_APPLE;
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

  return CGLTexImageIOSurface2D(ctx, LOCAL_GL_TEXTURE_RECTANGLE_ARB,
                                internalFormat, GetDevicePixelWidth(plane),
                                GetDevicePixelHeight(plane), format, type,
                                plane);
}
