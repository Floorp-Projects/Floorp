/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MacIOSurface.h"
#include <OpenGL/gl.h>
#include <QuartzCore/QuartzCore.h>
#include <dlfcn.h>
#include "mozilla/RefPtr.h"
#include "mozilla/Assertions.h"
#include "GLConsts.h"

using namespace mozilla;
// IOSurface signatures
#define IOSURFACE_FRAMEWORK_PATH \
  "/System/Library/Frameworks/IOSurface.framework/IOSurface"
#define OPENGL_FRAMEWORK_PATH \
  "/System/Library/Frameworks/OpenGL.framework/OpenGL"
#define COREGRAPHICS_FRAMEWORK_PATH \
  "/System/Library/Frameworks/ApplicationServices.framework/Frameworks/" \
  "CoreGraphics.framework/CoreGraphics"
#define COREVIDEO_FRAMEWORK_PATH \
  "/System/Library/Frameworks/ApplicationServices.framework/Frameworks/" \
  "CoreVideo.framework/CoreVideo"

#define GET_CONST(const_name) \
  ((CFStringRef*) dlsym(sIOSurfaceFramework, const_name))
#define GET_IOSYM(dest,sym_name) \
  (typeof(dest)) dlsym(sIOSurfaceFramework, sym_name)
#define GET_CGLSYM(dest,sym_name) \
  (typeof(dest)) dlsym(sOpenGLFramework, sym_name)
#define GET_CGSYM(dest,sym_name) \
  (typeof(dest)) dlsym(sCoreGraphicsFramework, sym_name)
#define GET_CVSYM(dest, sym_name) \
  (typeof(dest)) dlsym(sCoreVideoFramework, sym_name)

MacIOSurfaceLib::LibraryUnloader MacIOSurfaceLib::sLibraryUnloader;
bool                          MacIOSurfaceLib::isLoaded = false;
void*                         MacIOSurfaceLib::sIOSurfaceFramework;
void*                         MacIOSurfaceLib::sOpenGLFramework;
void*                         MacIOSurfaceLib::sCoreGraphicsFramework;
void*                         MacIOSurfaceLib::sCoreVideoFramework;
IOSurfaceCreateFunc           MacIOSurfaceLib::sCreate;
IOSurfaceGetIDFunc            MacIOSurfaceLib::sGetID;
IOSurfaceLookupFunc           MacIOSurfaceLib::sLookup;
IOSurfaceGetBaseAddressFunc   MacIOSurfaceLib::sGetBaseAddress;
IOSurfaceGetBaseAddressOfPlaneFunc  MacIOSurfaceLib::sGetBaseAddressOfPlane;
IOSurfaceSizePlaneTFunc       MacIOSurfaceLib::sWidth;
IOSurfaceSizePlaneTFunc       MacIOSurfaceLib::sHeight;
IOSurfaceSizeTFunc            MacIOSurfaceLib::sPlaneCount;
IOSurfaceSizePlaneTFunc       MacIOSurfaceLib::sBytesPerRow;
IOSurfaceGetPropertyMaximumFunc   MacIOSurfaceLib::sGetPropertyMaximum;
IOSurfaceVoidFunc             MacIOSurfaceLib::sIncrementUseCount;
IOSurfaceVoidFunc             MacIOSurfaceLib::sDecrementUseCount;
IOSurfaceLockFunc             MacIOSurfaceLib::sLock;
IOSurfaceUnlockFunc           MacIOSurfaceLib::sUnlock;
CGLTexImageIOSurface2DFunc    MacIOSurfaceLib::sTexImage;
IOSurfaceContextCreateFunc    MacIOSurfaceLib::sIOSurfaceContextCreate;
IOSurfaceContextCreateImageFunc   MacIOSurfaceLib::sIOSurfaceContextCreateImage;
IOSurfaceContextGetSurfaceFunc    MacIOSurfaceLib::sIOSurfaceContextGetSurface;
CVPixelBufferGetIOSurfaceFunc MacIOSurfaceLib::sCVPixelBufferGetIOSurface;
unsigned int                  (*MacIOSurfaceLib::sCGContextGetTypePtr) (CGContextRef) = nullptr;
IOSurfacePixelFormatFunc      MacIOSurfaceLib::sPixelFormat;

CFStringRef                   MacIOSurfaceLib::kPropWidth;
CFStringRef                   MacIOSurfaceLib::kPropHeight;
CFStringRef                   MacIOSurfaceLib::kPropBytesPerElem;
CFStringRef                   MacIOSurfaceLib::kPropBytesPerRow;
CFStringRef                   MacIOSurfaceLib::kPropIsGlobal;

bool MacIOSurfaceLib::isInit() {
  // Guard against trying to reload the library
  // if it is not available.
  if (!isLoaded)
    LoadLibrary();
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

void* MacIOSurfaceLib::IOSurfaceGetBaseAddressOfPlane(IOSurfacePtr aIOSurfacePtr,
                                                      size_t planeIndex) {
  return sGetBaseAddressOfPlane(aIOSurfacePtr, planeIndex);
}

size_t MacIOSurfaceLib::IOSurfaceGetPlaneCount(IOSurfacePtr aIOSurfacePtr) {
  return sPlaneCount(aIOSurfacePtr);
}

size_t MacIOSurfaceLib::IOSurfaceGetWidth(IOSurfacePtr aIOSurfacePtr, size_t plane) {
  return sWidth(aIOSurfacePtr, plane);
}

size_t MacIOSurfaceLib::IOSurfaceGetHeight(IOSurfacePtr aIOSurfacePtr, size_t plane) {
  return sHeight(aIOSurfacePtr, plane);
}

size_t MacIOSurfaceLib::IOSurfaceGetBytesPerRow(IOSurfacePtr aIOSurfacePtr, size_t plane) {
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
                                         uint32_t options, uint32_t *seed) {
  return sUnlock(aIOSurfacePtr, options, seed);
}

void MacIOSurfaceLib::IOSurfaceIncrementUseCount(IOSurfacePtr aIOSurfacePtr) {
  sIncrementUseCount(aIOSurfacePtr);
}

void MacIOSurfaceLib::IOSurfaceDecrementUseCount(IOSurfacePtr aIOSurfacePtr) {
  sDecrementUseCount(aIOSurfacePtr);
}

CGLError MacIOSurfaceLib::CGLTexImageIOSurface2D(CGLContextObj ctxt,
                             GLenum target, GLenum internalFormat,
                             GLsizei width, GLsizei height,
                             GLenum format, GLenum type,
                             IOSurfacePtr ioSurface, GLuint plane) {
  return sTexImage(ctxt, target, internalFormat, width, height,
                   format, type, ioSurface, plane);
}

IOSurfacePtr MacIOSurfaceLib::CVPixelBufferGetIOSurface(CVPixelBufferRef aPixelBuffer) {
  return sCVPixelBufferGetIOSurface(aPixelBuffer);
}

CGContextRef MacIOSurfaceLib::IOSurfaceContextCreate(IOSurfacePtr aIOSurfacePtr,
                             unsigned aWidth, unsigned aHeight,
                             unsigned aBitsPerComponent, unsigned aBytes,
                             CGColorSpaceRef aColorSpace, CGBitmapInfo bitmapInfo) {
  if (!sIOSurfaceContextCreate)
    return nullptr;
  return sIOSurfaceContextCreate(aIOSurfacePtr, aWidth, aHeight, aBitsPerComponent, aBytes, aColorSpace, bitmapInfo);
}

CGImageRef MacIOSurfaceLib::IOSurfaceContextCreateImage(CGContextRef aContext) {
  if (!sIOSurfaceContextCreateImage)
    return nullptr;
  return sIOSurfaceContextCreateImage(aContext);
}

IOSurfacePtr MacIOSurfaceLib::IOSurfaceContextGetSurface(CGContextRef aContext) {
  if (!sIOSurfaceContextGetSurface)
    return nullptr;
  return sIOSurfaceContextGetSurface(aContext);
}

CFStringRef MacIOSurfaceLib::GetIOConst(const char* symbole) {
  CFStringRef *address = (CFStringRef*)dlsym(sIOSurfaceFramework, symbole);
  if (!address)
    return nullptr;

  return *address;
}

void MacIOSurfaceLib::LoadLibrary() {
  if (isLoaded) {
    return;
  } 
  isLoaded = true;
  sIOSurfaceFramework = dlopen(IOSURFACE_FRAMEWORK_PATH,
                            RTLD_LAZY | RTLD_LOCAL);
  sOpenGLFramework = dlopen(OPENGL_FRAMEWORK_PATH,
                            RTLD_LAZY | RTLD_LOCAL);

  sCoreGraphicsFramework = dlopen(COREGRAPHICS_FRAMEWORK_PATH,
                            RTLD_LAZY | RTLD_LOCAL);

  sCoreVideoFramework = dlopen(COREVIDEO_FRAMEWORK_PATH,
                            RTLD_LAZY | RTLD_LOCAL);

  if (!sIOSurfaceFramework || !sOpenGLFramework || !sCoreGraphicsFramework ||
      !sCoreVideoFramework) {
    if (sIOSurfaceFramework)
      dlclose(sIOSurfaceFramework);
    if (sOpenGLFramework)
      dlclose(sOpenGLFramework);
    if (sCoreGraphicsFramework)
      dlclose(sCoreGraphicsFramework);
    if (sCoreVideoFramework)
      dlclose(sCoreVideoFramework);
    sIOSurfaceFramework = nullptr;
    sOpenGLFramework = nullptr;
    sCoreGraphicsFramework = nullptr;
    sCoreVideoFramework = nullptr;
    return;
  }

  kPropWidth = GetIOConst("kIOSurfaceWidth");
  kPropHeight = GetIOConst("kIOSurfaceHeight");
  kPropBytesPerElem = GetIOConst("kIOSurfaceBytesPerElement");
  kPropBytesPerRow = GetIOConst("kIOSurfaceBytesPerRow");
  kPropIsGlobal = GetIOConst("kIOSurfaceIsGlobal");
  sCreate = GET_IOSYM(sCreate, "IOSurfaceCreate");
  sGetID  = GET_IOSYM(sGetID,  "IOSurfaceGetID");
  sWidth = GET_IOSYM(sWidth, "IOSurfaceGetWidthOfPlane");
  sHeight = GET_IOSYM(sHeight, "IOSurfaceGetHeightOfPlane");
  sBytesPerRow = GET_IOSYM(sBytesPerRow, "IOSurfaceGetBytesPerRowOfPlane");
  sGetPropertyMaximum = GET_IOSYM(sGetPropertyMaximum, "IOSurfaceGetPropertyMaximum");
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
  sCGContextGetTypePtr = (unsigned int (*)(CGContext*))dlsym(RTLD_DEFAULT, "CGContextGetType");

  sCVPixelBufferGetIOSurface =
    GET_CVSYM(sCVPixelBufferGetIOSurface, "CVPixelBufferGetIOSurface");

  // Optional symbols
  sIOSurfaceContextCreate = GET_CGSYM(sIOSurfaceContextCreate, "CGIOSurfaceContextCreate");
  sIOSurfaceContextCreateImage = GET_CGSYM(sIOSurfaceContextCreateImage, "CGIOSurfaceContextCreateImage");
  sIOSurfaceContextGetSurface = GET_CGSYM(sIOSurfaceContextGetSurface, "CGIOSurfaceContextGetSurface");

  if (!sCreate || !sGetID || !sLookup || !sTexImage || !sGetBaseAddress ||
      !sGetBaseAddressOfPlane || !sPlaneCount ||
      !kPropWidth || !kPropHeight || !kPropBytesPerElem || !kPropIsGlobal ||
      !sLock || !sUnlock || !sIncrementUseCount || !sDecrementUseCount ||
      !sWidth || !sHeight || !kPropBytesPerRow ||
      !sBytesPerRow || !sGetPropertyMaximum || !sCVPixelBufferGetIOSurface) {
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

MacIOSurface::MacIOSurface(const void* aIOSurfacePtr,
                           double aContentsScaleFactor, bool aHasAlpha)
  : mIOSurfacePtr(aIOSurfacePtr)
  , mContentsScaleFactor(aContentsScaleFactor)
  , mHasAlpha(aHasAlpha)
{
  CFRetain(mIOSurfacePtr);
  IncrementUseCount();
}

MacIOSurface::~MacIOSurface() {
  DecrementUseCount();
  CFRelease(mIOSurfacePtr);
}

already_AddRefed<MacIOSurface> MacIOSurface::CreateIOSurface(int aWidth, int aHeight,
                                                         double aContentsScaleFactor,
                                                         bool aHasAlpha) {
  if (!MacIOSurfaceLib::isInit() || aContentsScaleFactor <= 0)
    return nullptr;

  CFMutableDictionaryRef props = ::CFDictionaryCreateMutable(
                      kCFAllocatorDefault, 4,
                      &kCFTypeDictionaryKeyCallBacks,
                      &kCFTypeDictionaryValueCallBacks);
  if (!props)
    return nullptr;

  MOZ_ASSERT((size_t)aWidth <= GetMaxWidth());
  MOZ_ASSERT((size_t)aHeight <= GetMaxHeight());

  int32_t bytesPerElem = 4;
  size_t intScaleFactor = ceil(aContentsScaleFactor);
  aWidth *= intScaleFactor;
  aHeight *= intScaleFactor;
  CFNumberRef cfWidth = ::CFNumberCreate(nullptr, kCFNumberSInt32Type, &aWidth);
  CFNumberRef cfHeight = ::CFNumberCreate(nullptr, kCFNumberSInt32Type, &aHeight);
  CFNumberRef cfBytesPerElem = ::CFNumberCreate(nullptr, kCFNumberSInt32Type, &bytesPerElem);
  ::CFDictionaryAddValue(props, MacIOSurfaceLib::kPropWidth,
                                cfWidth);
  ::CFRelease(cfWidth);
  ::CFDictionaryAddValue(props, MacIOSurfaceLib::kPropHeight,
                                cfHeight);
  ::CFRelease(cfHeight);
  ::CFDictionaryAddValue(props, MacIOSurfaceLib::kPropBytesPerElem, 
                                cfBytesPerElem);
  ::CFRelease(cfBytesPerElem);
  ::CFDictionaryAddValue(props, MacIOSurfaceLib::kPropIsGlobal, 
                                kCFBooleanTrue);

  IOSurfacePtr surfaceRef = MacIOSurfaceLib::IOSurfaceCreate(props);
  ::CFRelease(props);

  if (!surfaceRef)
    return nullptr;

  RefPtr<MacIOSurface> ioSurface = new MacIOSurface(surfaceRef, aContentsScaleFactor, aHasAlpha);
  if (!ioSurface) {
    ::CFRelease(surfaceRef);
    return nullptr;
  }

  // Release the IOSurface because MacIOSurface retained it
  CFRelease(surfaceRef);

  return ioSurface.forget();
}

already_AddRefed<MacIOSurface> MacIOSurface::LookupSurface(IOSurfaceID aIOSurfaceID,
                                                       double aContentsScaleFactor,
                                                       bool aHasAlpha) { 
  if (!MacIOSurfaceLib::isInit() || aContentsScaleFactor <= 0)
    return nullptr;

  IOSurfacePtr surfaceRef = MacIOSurfaceLib::IOSurfaceLookup(aIOSurfaceID);
  if (!surfaceRef)
    return nullptr;

  RefPtr<MacIOSurface> ioSurface = new MacIOSurface(surfaceRef, aContentsScaleFactor, aHasAlpha);
  if (!ioSurface) {
    ::CFRelease(surfaceRef);
    return nullptr;
  }

  // Release the IOSurface because MacIOSurface retained it
  CFRelease(surfaceRef);

  return ioSurface.forget();
}

IOSurfaceID MacIOSurface::GetIOSurfaceID() { 
  return MacIOSurfaceLib::IOSurfaceGetID(mIOSurfacePtr);
}

void* MacIOSurface::GetBaseAddress() { 
  return MacIOSurfaceLib::IOSurfaceGetBaseAddress(mIOSurfacePtr);
}

void* MacIOSurface::GetBaseAddressOfPlane(size_t aPlaneIndex)
{
  return MacIOSurfaceLib::IOSurfaceGetBaseAddressOfPlane(mIOSurfacePtr,
                                                         aPlaneIndex);
}

size_t MacIOSurface::GetWidth(size_t plane) {
  size_t intScaleFactor = ceil(mContentsScaleFactor);
  return GetDevicePixelWidth(plane) / intScaleFactor;
}

size_t MacIOSurface::GetHeight(size_t plane) {
  size_t intScaleFactor = ceil(mContentsScaleFactor);
  return GetDevicePixelHeight(plane) / intScaleFactor;
}

size_t MacIOSurface::GetPlaneCount() {
  return MacIOSurfaceLib::IOSurfaceGetPlaneCount(mIOSurfacePtr);
}

/*static*/ size_t MacIOSurface::GetMaxWidth() {
  if (!MacIOSurfaceLib::isInit())
    return -1;
  return MacIOSurfaceLib::IOSurfaceGetPropertyMaximum(MacIOSurfaceLib::kPropWidth);
}

/*static*/ size_t MacIOSurface::GetMaxHeight() {
  if (!MacIOSurfaceLib::isInit())
    return -1;
  return MacIOSurfaceLib::IOSurfaceGetPropertyMaximum(MacIOSurfaceLib::kPropHeight);
}

size_t MacIOSurface::GetDevicePixelWidth(size_t plane) {
  return MacIOSurfaceLib::IOSurfaceGetWidth(mIOSurfacePtr, plane);
}

size_t MacIOSurface::GetDevicePixelHeight(size_t plane) {
  return MacIOSurfaceLib::IOSurfaceGetHeight(mIOSurfacePtr, plane);
}

size_t MacIOSurface::GetBytesPerRow(size_t plane) {
  return MacIOSurfaceLib::IOSurfaceGetBytesPerRow(mIOSurfacePtr, plane);
}

OSType MacIOSurface::GetPixelFormat() {
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
  MacIOSurfaceLib::IOSurfaceLock(mIOSurfacePtr, aReadOnly ? READ_ONLY : 0, nullptr);
}

void MacIOSurface::Unlock(bool aReadOnly) {
  MacIOSurfaceLib::IOSurfaceUnlock(mIOSurfacePtr, aReadOnly ? READ_ONLY : 0, nullptr);
}

using mozilla::gfx::SourceSurface;
using mozilla::gfx::IntSize;
using mozilla::gfx::SurfaceFormat;

void
MacIOSurfaceBufferDeallocator(void* aClosure)
{
  MOZ_ASSERT(aClosure);

  delete [] static_cast<unsigned char*>(aClosure);
}

already_AddRefed<SourceSurface>
MacIOSurface::GetAsSurface() {
  Lock();
  size_t bytesPerRow = GetBytesPerRow();
  size_t ioWidth = GetDevicePixelWidth();
  size_t ioHeight = GetDevicePixelHeight();

  unsigned char* ioData = (unsigned char*)GetBaseAddress();
  unsigned char* dataCpy =
      new unsigned char[bytesPerRow * ioHeight / sizeof(unsigned char)];
  for (size_t i = 0; i < ioHeight; i++) {
    memcpy(dataCpy + i * bytesPerRow,
           ioData + i * bytesPerRow, ioWidth * 4);
  }

  Unlock();

  SurfaceFormat format = HasAlpha() ? mozilla::gfx::SurfaceFormat::B8G8R8A8 :
                                      mozilla::gfx::SurfaceFormat::B8G8R8X8;

  RefPtr<mozilla::gfx::DataSourceSurface> surf =
      mozilla::gfx::Factory::CreateWrappingDataSourceSurface(dataCpy,
                                                             bytesPerRow,
                                                             IntSize(ioWidth, ioHeight),
                                                             format,
                                                             &MacIOSurfaceBufferDeallocator,
                                                             static_cast<void*>(dataCpy));

  return surf.forget();
}

SurfaceFormat
MacIOSurface::GetFormat()
{
  OSType pixelFormat = GetPixelFormat();
  if (pixelFormat == '420v') {
    return SurfaceFormat::NV12;
  } else if (pixelFormat == '2vuy') {
    return SurfaceFormat::YUV422;
  } else  {
    return HasAlpha() ? SurfaceFormat::R8G8B8A8 : SurfaceFormat::R8G8B8X8;
  }
}

SurfaceFormat
MacIOSurface::GetReadFormat()
{
  OSType pixelFormat = GetPixelFormat();
  if (pixelFormat == '420v') {
    return SurfaceFormat::NV12;
  } else if (pixelFormat == '2vuy') {
    return SurfaceFormat::R8G8B8X8;
  } else  {
    return HasAlpha() ? SurfaceFormat::R8G8B8A8 : SurfaceFormat::R8G8B8X8;
  }
}

CGLError
MacIOSurface::CGLTexImageIOSurface2D(CGLContextObj ctx, size_t plane)
{
  MOZ_ASSERT(plane >= 0);
  OSType pixelFormat = GetPixelFormat();

  GLenum internalFormat;
  GLenum format;
  GLenum type;
  if (pixelFormat == '420v') {
    MOZ_ASSERT(GetPlaneCount() == 2);
    MOZ_ASSERT(plane < 2);

    if (plane == 0) {
      internalFormat = format = GL_LUMINANCE;
    } else {
      internalFormat = format = GL_LUMINANCE_ALPHA;
    }
    type = GL_UNSIGNED_BYTE;
  } else if (pixelFormat == '2vuy') {
    MOZ_ASSERT(plane == 0);

    internalFormat = GL_RGB;
    format = LOCAL_GL_YCBCR_422_APPLE;
    type = GL_UNSIGNED_SHORT_8_8_APPLE;
  } else  {
    MOZ_ASSERT(plane == 0);

    internalFormat = HasAlpha() ? GL_RGBA : GL_RGB;
    format = GL_BGRA;
    type = GL_UNSIGNED_INT_8_8_8_8_REV;
  }
  CGLError temp =  MacIOSurfaceLib::CGLTexImageIOSurface2D(ctx,
                                                GL_TEXTURE_RECTANGLE_ARB,
                                                internalFormat,
                                                GetDevicePixelWidth(plane),
                                                GetDevicePixelHeight(plane),
                                                format,
                                                type,
                                                mIOSurfacePtr, plane);
  return temp;
}

static
CGColorSpaceRef CreateSystemColorSpace() {
  CGColorSpaceRef cspace = ::CGDisplayCopyColorSpace(::CGMainDisplayID());
  if (!cspace) {
    cspace = ::CGColorSpaceCreateDeviceRGB();
  }
  return cspace;
}

CGContextRef MacIOSurface::CreateIOSurfaceContext() {
  CGColorSpaceRef cspace = CreateSystemColorSpace();
  CGContextRef ref = MacIOSurfaceLib::IOSurfaceContextCreate(mIOSurfacePtr,
                                                GetDevicePixelWidth(),
                                                GetDevicePixelHeight(),
                                                8, 32, cspace, 0x2002);
  ::CGColorSpaceRelease(cspace);
  return ref;
}

CGImageRef MacIOSurface::CreateImageFromIOSurfaceContext(CGContextRef aContext) {
  if (!MacIOSurfaceLib::isInit())
    return nullptr;

  return MacIOSurfaceLib::IOSurfaceContextCreateImage(aContext);
}

already_AddRefed<MacIOSurface> MacIOSurface::IOSurfaceContextGetSurface(CGContextRef aContext,
                                                                    double aContentsScaleFactor,
                                                                    bool aHasAlpha) {
  if (!MacIOSurfaceLib::isInit() || aContentsScaleFactor <= 0)
    return nullptr;

  IOSurfacePtr surfaceRef = MacIOSurfaceLib::IOSurfaceContextGetSurface(aContext);
  if (!surfaceRef)
    return nullptr;

  RefPtr<MacIOSurface> ioSurface = new MacIOSurface(surfaceRef, aContentsScaleFactor, aHasAlpha);
  if (!ioSurface) {
    ::CFRelease(surfaceRef);
    return nullptr;
  }
  return ioSurface.forget();
}


CGContextType GetContextType(CGContextRef ref)
{
  if (!MacIOSurfaceLib::isInit() || !MacIOSurfaceLib::sCGContextGetTypePtr)
    return CG_CONTEXT_TYPE_UNKNOWN;

  unsigned int type = MacIOSurfaceLib::sCGContextGetTypePtr(ref);
  if (type == CG_CONTEXT_TYPE_BITMAP) {
    return CG_CONTEXT_TYPE_BITMAP;
  } else if (type == CG_CONTEXT_TYPE_IOSURFACE) {
    return CG_CONTEXT_TYPE_IOSURFACE;
  } else {
    return CG_CONTEXT_TYPE_UNKNOWN;
  }
}


