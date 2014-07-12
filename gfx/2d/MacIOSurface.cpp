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

using namespace mozilla;
// IOSurface signatures
#define IOSURFACE_FRAMEWORK_PATH \
  "/System/Library/Frameworks/IOSurface.framework/IOSurface"
#define OPENGL_FRAMEWORK_PATH \
  "/System/Library/Frameworks/OpenGL.framework/OpenGL"
#define COREGRAPHICS_FRAMEWORK_PATH \
  "/System/Library/Frameworks/ApplicationServices.framework/Frameworks/CoreGraphics.framework/CoreGraphics"



#define GET_CONST(const_name) \
  ((CFStringRef*) dlsym(sIOSurfaceFramework, const_name))
#define GET_IOSYM(dest,sym_name) \
  (typeof(dest)) dlsym(sIOSurfaceFramework, sym_name)
#define GET_CGLSYM(dest,sym_name) \
  (typeof(dest)) dlsym(sOpenGLFramework, sym_name)
#define GET_CGSYM(dest,sym_name) \
  (typeof(dest)) dlsym(sCoreGraphicsFramework, sym_name)

MacIOSurfaceLib::LibraryUnloader MacIOSurfaceLib::sLibraryUnloader;
bool                          MacIOSurfaceLib::isLoaded = false;
void*                         MacIOSurfaceLib::sIOSurfaceFramework;
void*                         MacIOSurfaceLib::sOpenGLFramework;
void*                         MacIOSurfaceLib::sCoreGraphicsFramework;
IOSurfaceCreateFunc           MacIOSurfaceLib::sCreate;
IOSurfaceGetIDFunc            MacIOSurfaceLib::sGetID;
IOSurfaceLookupFunc           MacIOSurfaceLib::sLookup;
IOSurfaceGetBaseAddressFunc   MacIOSurfaceLib::sGetBaseAddress;
IOSurfaceGetWidthFunc         MacIOSurfaceLib::sWidth;
IOSurfaceGetHeightFunc        MacIOSurfaceLib::sHeight;
IOSurfaceGetBytesPerRowFunc   MacIOSurfaceLib::sBytesPerRow;
IOSurfaceGetPropertyMaximumFunc   MacIOSurfaceLib::sGetPropertyMaximum;
IOSurfaceLockFunc             MacIOSurfaceLib::sLock;
IOSurfaceUnlockFunc           MacIOSurfaceLib::sUnlock;
CGLTexImageIOSurface2DFunc    MacIOSurfaceLib::sTexImage;
IOSurfaceContextCreateFunc    MacIOSurfaceLib::sIOSurfaceContextCreate;
IOSurfaceContextCreateImageFunc   MacIOSurfaceLib::sIOSurfaceContextCreateImage;
IOSurfaceContextGetSurfaceFunc    MacIOSurfaceLib::sIOSurfaceContextGetSurface;
unsigned int                  (*MacIOSurfaceLib::sCGContextGetTypePtr) (CGContextRef) = nullptr;

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

size_t MacIOSurfaceLib::IOSurfaceGetWidth(IOSurfacePtr aIOSurfacePtr) {
  return sWidth(aIOSurfacePtr);
}

size_t MacIOSurfaceLib::IOSurfaceGetHeight(IOSurfacePtr aIOSurfacePtr) {
  return sHeight(aIOSurfacePtr);
}

size_t MacIOSurfaceLib::IOSurfaceGetBytesPerRow(IOSurfacePtr aIOSurfacePtr) {
  return sBytesPerRow(aIOSurfacePtr);
}

size_t MacIOSurfaceLib::IOSurfaceGetPropertyMaximum(CFStringRef property) {
  return sGetPropertyMaximum(property);
}

IOReturn MacIOSurfaceLib::IOSurfaceLock(IOSurfacePtr aIOSurfacePtr, 
                                       uint32_t options, uint32_t *seed) {
  return sLock(aIOSurfacePtr, options, seed);
}

IOReturn MacIOSurfaceLib::IOSurfaceUnlock(IOSurfacePtr aIOSurfacePtr, 
                                         uint32_t options, uint32_t *seed) {
  return sUnlock(aIOSurfacePtr, options, seed);
}

CGLError MacIOSurfaceLib::CGLTexImageIOSurface2D(CGLContextObj ctxt,
                             GLenum target, GLenum internalFormat,
                             GLsizei width, GLsizei height,
                             GLenum format, GLenum type,
                             IOSurfacePtr ioSurface, GLuint plane) {
  return sTexImage(ctxt, target, internalFormat, width, height,
                   format, type, ioSurface, plane);
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
  if (!sIOSurfaceFramework || !sOpenGLFramework || !sCoreGraphicsFramework) {
    if (sIOSurfaceFramework)
      dlclose(sIOSurfaceFramework);
    if (sOpenGLFramework)
      dlclose(sOpenGLFramework);
    if (sCoreGraphicsFramework)
      dlclose(sCoreGraphicsFramework);
    sIOSurfaceFramework = nullptr;
    sOpenGLFramework = nullptr;
    sCoreGraphicsFramework = nullptr;
    return;
  }

  kPropWidth = GetIOConst("kIOSurfaceWidth");
  kPropHeight = GetIOConst("kIOSurfaceHeight");
  kPropBytesPerElem = GetIOConst("kIOSurfaceBytesPerElement");
  kPropBytesPerRow = GetIOConst("kIOSurfaceBytesPerRow");
  kPropIsGlobal = GetIOConst("kIOSurfaceIsGlobal");
  sCreate = GET_IOSYM(sCreate, "IOSurfaceCreate");
  sGetID  = GET_IOSYM(sGetID,  "IOSurfaceGetID");
  sWidth = GET_IOSYM(sWidth, "IOSurfaceGetWidth");
  sHeight = GET_IOSYM(sHeight, "IOSurfaceGetHeight");
  sBytesPerRow = GET_IOSYM(sBytesPerRow, "IOSurfaceGetBytesPerRow");
  sGetPropertyMaximum = GET_IOSYM(sGetPropertyMaximum, "IOSurfaceGetPropertyMaximum");
  sLookup = GET_IOSYM(sLookup, "IOSurfaceLookup");
  sLock = GET_IOSYM(sLock, "IOSurfaceLock");
  sUnlock = GET_IOSYM(sUnlock, "IOSurfaceUnlock");
  sGetBaseAddress = GET_IOSYM(sGetBaseAddress, "IOSurfaceGetBaseAddress");
  sTexImage = GET_CGLSYM(sTexImage, "CGLTexImageIOSurface2D");
  sCGContextGetTypePtr = (unsigned int (*)(CGContext*))dlsym(RTLD_DEFAULT, "CGContextGetType");

  // Optional symbols
  sIOSurfaceContextCreate = GET_CGSYM(sIOSurfaceContextCreate, "CGIOSurfaceContextCreate");
  sIOSurfaceContextCreateImage = GET_CGSYM(sIOSurfaceContextCreateImage, "CGIOSurfaceContextCreateImage");
  sIOSurfaceContextGetSurface = GET_CGSYM(sIOSurfaceContextGetSurface, "CGIOSurfaceContextGetSurface");

  if (!sCreate || !sGetID || !sLookup || !sTexImage || !sGetBaseAddress ||
      !kPropWidth || !kPropHeight || !kPropBytesPerElem || !kPropIsGlobal ||
      !sLock || !sUnlock || !sWidth || !sHeight || !kPropBytesPerRow ||
      !sBytesPerRow || !sGetPropertyMaximum) {
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
  sIOSurfaceFramework = nullptr;
  sOpenGLFramework = nullptr;
}

MacIOSurface::~MacIOSurface() {
  CFRelease(mIOSurfacePtr);
}

TemporaryRef<MacIOSurface> MacIOSurface::CreateIOSurface(int aWidth, int aHeight,
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

  return ioSurface.forget();
}

TemporaryRef<MacIOSurface> MacIOSurface::LookupSurface(IOSurfaceID aIOSurfaceID,
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
  return ioSurface.forget();
}

IOSurfaceID MacIOSurface::GetIOSurfaceID() { 
  return MacIOSurfaceLib::IOSurfaceGetID(mIOSurfacePtr);
}

void* MacIOSurface::GetBaseAddress() { 
  return MacIOSurfaceLib::IOSurfaceGetBaseAddress(mIOSurfacePtr);
}

size_t MacIOSurface::GetWidth() {
  size_t intScaleFactor = ceil(mContentsScaleFactor);
  return GetDevicePixelWidth() / intScaleFactor;
}

size_t MacIOSurface::GetHeight() {
  size_t intScaleFactor = ceil(mContentsScaleFactor);
  return GetDevicePixelHeight() / intScaleFactor;
}

/*static*/ size_t MacIOSurface::GetMaxWidth() {
  return MacIOSurfaceLib::IOSurfaceGetPropertyMaximum(MacIOSurfaceLib::kPropWidth);
}

/*static*/ size_t MacIOSurface::GetMaxHeight() {
  return MacIOSurfaceLib::IOSurfaceGetPropertyMaximum(MacIOSurfaceLib::kPropHeight);
}

size_t MacIOSurface::GetDevicePixelWidth() {
  return MacIOSurfaceLib::IOSurfaceGetWidth(mIOSurfacePtr);
}

size_t MacIOSurface::GetDevicePixelHeight() {
  return MacIOSurfaceLib::IOSurfaceGetHeight(mIOSurfacePtr);
}

size_t MacIOSurface::GetBytesPerRow() { 
  return MacIOSurfaceLib::IOSurfaceGetBytesPerRow(mIOSurfacePtr);
}

#define READ_ONLY 0x1
void MacIOSurface::Lock() {
  MacIOSurfaceLib::IOSurfaceLock(mIOSurfacePtr, READ_ONLY, nullptr);
}

void MacIOSurface::Unlock() {
  MacIOSurfaceLib::IOSurfaceUnlock(mIOSurfacePtr, READ_ONLY, nullptr);
}

#include "SourceSurfaceRawData.h"
using mozilla::gfx::SourceSurface;
using mozilla::gfx::SourceSurfaceRawData;
using mozilla::gfx::IntSize;
using mozilla::gfx::SurfaceFormat;

TemporaryRef<SourceSurface>
MacIOSurface::GetAsSurface() {
  Lock();
  size_t bytesPerRow = GetBytesPerRow();
  size_t ioWidth = GetDevicePixelWidth();
  size_t ioHeight = GetDevicePixelHeight();

  unsigned char* ioData = (unsigned char*)GetBaseAddress();
  unsigned char* dataCpy = (unsigned char*)malloc(bytesPerRow*ioHeight);
  for (size_t i = 0; i < ioHeight; i++) {
    memcpy(dataCpy + i * bytesPerRow,
           ioData + i * bytesPerRow, ioWidth * 4);
  }

  Unlock();

  SurfaceFormat format = HasAlpha() ? mozilla::gfx::SurfaceFormat::B8G8R8A8 :
                                      mozilla::gfx::SurfaceFormat::B8G8R8X8;

  RefPtr<SourceSurfaceRawData> surf = new SourceSurfaceRawData();
  surf->InitWrappingData(dataCpy, IntSize(ioWidth, ioHeight), bytesPerRow, format, true);

  return surf.forget();
}

CGLError
MacIOSurface::CGLTexImageIOSurface2D(CGLContextObj ctx)
{
  return MacIOSurfaceLib::CGLTexImageIOSurface2D(ctx,
                                                GL_TEXTURE_RECTANGLE_ARB,
                                                HasAlpha() ? GL_RGBA : GL_RGB,
                                                GetDevicePixelWidth(),
                                                GetDevicePixelHeight(),
                                                GL_BGRA,
                                                GL_UNSIGNED_INT_8_8_8_8_REV,
                                                mIOSurfacePtr, 0);
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

TemporaryRef<MacIOSurface> MacIOSurface::IOSurfaceContextGetSurface(CGContextRef aContext,
                                                                    double aContentsScaleFactor,
                                                                    bool aHasAlpha) {
  if (!MacIOSurfaceLib::isInit() || aContentsScaleFactor <= 0)
    return nullptr;

  IOSurfacePtr surfaceRef = MacIOSurfaceLib::IOSurfaceContextGetSurface(aContext);
  if (!surfaceRef)
    return nullptr;

  // Retain the IOSurface because MacIOSurface will release it
  CFRetain(surfaceRef);

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


