/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuartzSupport.h"
#include "nsDebug.h"

#import <QuartzCore/QuartzCore.h>
#import <AppKit/NSOpenGL.h>
#include <dlfcn.h>

#define IOSURFACE_FRAMEWORK_PATH \
  "/System/Library/Frameworks/IOSurface.framework/IOSurface"
#define OPENGL_FRAMEWORK_PATH \
  "/System/Library/Frameworks/OpenGL.framework/OpenGL"
#define COREGRAPHICS_FRAMEWORK_PATH \
  "/System/Library/Frameworks/ApplicationServices.framework/Frameworks/CoreGraphics.framework/CoreGraphics"

using mozilla::RefPtr;
using mozilla::TemporaryRef;

// IOSurface signatures
typedef CFTypeRef IOSurfacePtr;
typedef IOSurfacePtr (*IOSurfaceCreateFunc) (CFDictionaryRef properties);
typedef IOSurfacePtr (*IOSurfaceLookupFunc) (uint32_t io_surface_id);
typedef IOSurfaceID (*IOSurfaceGetIDFunc) (CFTypeRef io_surface);
typedef IOReturn (*IOSurfaceLockFunc) (CFTypeRef io_surface, 
                                       uint32_t options, 
                                       uint32_t *seed);
typedef IOReturn (*IOSurfaceUnlockFunc) (CFTypeRef io_surface, 
                                         uint32_t options, 
                                         uint32_t *seed);
typedef void* (*IOSurfaceGetBaseAddressFunc) (CFTypeRef io_surface);
typedef size_t (*IOSurfaceGetWidthFunc) (IOSurfacePtr io_surface);
typedef size_t (*IOSurfaceGetHeightFunc) (IOSurfacePtr io_surface);
typedef size_t (*IOSurfaceGetBytesPerRowFunc) (IOSurfacePtr io_surface);
typedef CGLError (*CGLTexImageIOSurface2DFunc) (CGLContextObj ctxt,
                             GLenum target, GLenum internalFormat,
                             GLsizei width, GLsizei height,
                             GLenum format, GLenum type,
                             IOSurfacePtr ioSurface, GLuint plane);
typedef CGContextRef (*IOSurfaceContextCreateFunc)(CFTypeRef io_surface,
                             unsigned width, unsigned height,
                             unsigned bitsPerComponent, unsigned bytes,
                             CGColorSpaceRef colorSpace, CGBitmapInfo bitmapInfo);
typedef CGImageRef (*IOSurfaceContextCreateImageFunc)(CGContextRef ref);
typedef IOSurfacePtr (*IOSurfaceContextGetSurfaceFunc)(CGContextRef ref);

#define GET_CONST(const_name) \
  ((CFStringRef*) dlsym(sIOSurfaceFramework, const_name))
#define GET_IOSYM(dest,sym_name) \
  (typeof(dest)) dlsym(sIOSurfaceFramework, sym_name)
#define GET_CGLSYM(dest,sym_name) \
  (typeof(dest)) dlsym(sOpenGLFramework, sym_name)
#define GET_CGSYM(dest,sym_name) \
  (typeof(dest)) dlsym(sCoreGraphicsFramework, sym_name)

class MacIOSurfaceLib: public MacIOSurface {
public:
  static void                        *sIOSurfaceFramework;
  static void                        *sOpenGLFramework;
  static void                        *sCoreGraphicsFramework;
  static bool                         isLoaded;
  static IOSurfaceCreateFunc          sCreate;
  static IOSurfaceGetIDFunc           sGetID;
  static IOSurfaceLookupFunc          sLookup;
  static IOSurfaceGetBaseAddressFunc  sGetBaseAddress;
  static IOSurfaceLockFunc            sLock;
  static IOSurfaceUnlockFunc          sUnlock;
  static IOSurfaceGetWidthFunc        sWidth;
  static IOSurfaceGetHeightFunc       sHeight;
  static IOSurfaceGetBytesPerRowFunc  sBytesPerRow;
  static CGLTexImageIOSurface2DFunc   sTexImage;
  static IOSurfaceContextCreateFunc   sIOSurfaceContextCreate;
  static IOSurfaceContextCreateImageFunc  sIOSurfaceContextCreateImage;
  static IOSurfaceContextGetSurfaceFunc   sIOSurfaceContextGetSurface;
  static CFStringRef                  kPropWidth;
  static CFStringRef                  kPropHeight;
  static CFStringRef                  kPropBytesPerElem;
  static CFStringRef                  kPropBytesPerRow;
  static CFStringRef                  kPropIsGlobal;

  static bool isInit();
  static CFStringRef GetIOConst(const char* symbole);
  static IOSurfacePtr IOSurfaceCreate(CFDictionaryRef properties);
  static IOSurfacePtr IOSurfaceLookup(IOSurfaceID aIOSurfaceID);
  static IOSurfaceID  IOSurfaceGetID(IOSurfacePtr aIOSurfacePtr);
  static void        *IOSurfaceGetBaseAddress(IOSurfacePtr aIOSurfacePtr);
  static size_t       IOSurfaceGetWidth(IOSurfacePtr aIOSurfacePtr);
  static size_t       IOSurfaceGetHeight(IOSurfacePtr aIOSurfacePtr);
  static size_t       IOSurfaceGetBytesPerRow(IOSurfacePtr aIOSurfacePtr);
  static IOReturn     IOSurfaceLock(IOSurfacePtr aIOSurfacePtr, 
                                    uint32_t options, uint32_t *seed);
  static IOReturn     IOSurfaceUnlock(IOSurfacePtr aIOSurfacePtr, 
                                      uint32_t options, uint32_t *seed);
  static CGLError     CGLTexImageIOSurface2D(CGLContextObj ctxt,
                             GLenum target, GLenum internalFormat,
                             GLsizei width, GLsizei height,
                             GLenum format, GLenum type,
                             IOSurfacePtr ioSurface, GLuint plane);
  static CGContextRef IOSurfaceContextCreate(IOSurfacePtr aIOSurfacePtr,
                             unsigned aWidth, unsigned aHeight,
                             unsigned aBitsPerCompoent, unsigned aBytes,
                             CGColorSpaceRef aColorSpace, CGBitmapInfo bitmapInfo);
  static CGImageRef   IOSurfaceContextCreateImage(CGContextRef ref);
  static IOSurfacePtr IOSurfaceContextGetSurface(CGContextRef ref);
  static unsigned int (*sCGContextGetTypePtr) (CGContextRef);
  static void LoadLibrary();
  static void CloseLibrary();

  // Static deconstructor
  static class LibraryUnloader {
  public:
    ~LibraryUnloader() {
      CloseLibrary();
    }
  } sLibraryUnloader;
};

MacIOSurfaceLib::LibraryUnloader MacIOSurfaceLib::sLibraryUnloader;
bool                          MacIOSurfaceLib::isLoaded = false;
void*                         MacIOSurfaceLib::sIOSurfaceFramework;
void*                         MacIOSurfaceLib::sOpenGLFramework;
void*                         MacIOSurfaceLib::sCoreGraphicsFramework;
IOSurfaceCreateFunc           MacIOSurfaceLib::sCreate;
IOSurfaceGetIDFunc            MacIOSurfaceLib::sGetID;
IOSurfaceLookupFunc           MacIOSurfaceLib::sLookup;
IOSurfaceGetBaseAddressFunc   MacIOSurfaceLib::sGetBaseAddress;
IOSurfaceGetHeightFunc        MacIOSurfaceLib::sWidth;
IOSurfaceGetWidthFunc         MacIOSurfaceLib::sHeight;
IOSurfaceGetBytesPerRowFunc   MacIOSurfaceLib::sBytesPerRow;
IOSurfaceLockFunc             MacIOSurfaceLib::sLock;
IOSurfaceUnlockFunc           MacIOSurfaceLib::sUnlock;
CGLTexImageIOSurface2DFunc    MacIOSurfaceLib::sTexImage;
IOSurfaceContextCreateFunc    MacIOSurfaceLib::sIOSurfaceContextCreate;
IOSurfaceContextCreateImageFunc   MacIOSurfaceLib::sIOSurfaceContextCreateImage;
IOSurfaceContextGetSurfaceFunc    MacIOSurfaceLib::sIOSurfaceContextGetSurface;
unsigned int                  (*MacIOSurfaceLib::sCGContextGetTypePtr) (CGContextRef) = NULL;

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
  if (!sIOSurfaceFramework) {
    NS_ERROR("MacIOSurfaceLib failed to initialize");
  }
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
    return NULL;
  return sIOSurfaceContextCreate(aIOSurfacePtr, aWidth, aHeight, aBitsPerComponent, aBytes, aColorSpace, bitmapInfo);
}

CGImageRef MacIOSurfaceLib::IOSurfaceContextCreateImage(CGContextRef aContext) {
  if (!sIOSurfaceContextCreateImage)
    return NULL;
  return sIOSurfaceContextCreateImage(aContext);
}

IOSurfacePtr MacIOSurfaceLib::IOSurfaceContextGetSurface(CGContextRef aContext) {
  if (!sIOSurfaceContextGetSurface)
    return NULL;
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
    sIOSurfaceFramework = nsnull;
    sOpenGLFramework = nsnull;
    sCoreGraphicsFramework = nsnull;
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
      !sBytesPerRow) {
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

TemporaryRef<MacIOSurface> MacIOSurface::CreateIOSurface(int aWidth, int aHeight) {
  if (!MacIOSurfaceLib::isInit())
    return nullptr;

  CFMutableDictionaryRef props = ::CFDictionaryCreateMutable(
                      kCFAllocatorDefault, 4,
                      &kCFTypeDictionaryKeyCallBacks,
                      &kCFTypeDictionaryValueCallBacks);
  if (!props)
    return nullptr;

  int32_t bytesPerElem = 4;
  CFNumberRef cfWidth = ::CFNumberCreate(NULL, kCFNumberSInt32Type, &aWidth);
  CFNumberRef cfHeight = ::CFNumberCreate(NULL, kCFNumberSInt32Type, &aHeight);
  CFNumberRef cfBytesPerElem = ::CFNumberCreate(NULL, kCFNumberSInt32Type, &bytesPerElem);
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

  RefPtr<MacIOSurface> ioSurface = new MacIOSurface(surfaceRef);
  if (!ioSurface) {
    ::CFRelease(surfaceRef);
    return nullptr;
  }

  return ioSurface.forget();
}

TemporaryRef<MacIOSurface> MacIOSurface::LookupSurface(IOSurfaceID aIOSurfaceID) { 
  if (!MacIOSurfaceLib::isInit())
    return nullptr;

  IOSurfacePtr surfaceRef = MacIOSurfaceLib::IOSurfaceLookup(aIOSurfaceID);
  if (!surfaceRef)
    return nullptr;

  RefPtr<MacIOSurface> ioSurface = new MacIOSurface(surfaceRef);
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
  return MacIOSurfaceLib::IOSurfaceGetWidth(mIOSurfacePtr);
}

size_t MacIOSurface::GetHeight() { 
  return MacIOSurfaceLib::IOSurfaceGetHeight(mIOSurfacePtr);
}

size_t MacIOSurface::GetBytesPerRow() { 
  return MacIOSurfaceLib::IOSurfaceGetBytesPerRow(mIOSurfacePtr);
}

#define READ_ONLY 0x1
void MacIOSurface::Lock() {
  MacIOSurfaceLib::IOSurfaceLock(mIOSurfacePtr, READ_ONLY, NULL);
}

void MacIOSurface::Unlock() {
  MacIOSurfaceLib::IOSurfaceUnlock(mIOSurfacePtr, READ_ONLY, NULL);
}

#include "SourceSurfaceRawData.h"
using mozilla::gfx::SourceSurface;
using mozilla::gfx::SourceSurfaceRawData;
using mozilla::gfx::IntSize;

TemporaryRef<SourceSurface>
MacIOSurface::GetAsSurface() {
  Lock();
  size_t bytesPerRow = GetBytesPerRow();
  size_t ioWidth = GetWidth();
  size_t ioHeight = GetHeight();

  unsigned char* ioData = (unsigned char*)GetBaseAddress();
  unsigned char* dataCpy = (unsigned char*)malloc(bytesPerRow*ioHeight);
  for (size_t i = 0; i < ioHeight; i++) {
    memcpy(dataCpy + i * bytesPerRow,
           ioData + i * bytesPerRow, ioWidth * 4);
  }

  Unlock();

  RefPtr<SourceSurfaceRawData> surf = new SourceSurfaceRawData();
  surf->InitWrappingData(dataCpy, IntSize(ioWidth, ioHeight), bytesPerRow, mozilla::gfx::FORMAT_B8G8R8A8, true);

  return surf.forget();
}

CGLError 
MacIOSurface::CGLTexImageIOSurface2D(void *c,
                                    GLenum internalFormat, GLenum format, 
                                    GLenum type, GLuint plane)
{
  NSOpenGLContext *ctxt = static_cast<NSOpenGLContext*>(c);
  return MacIOSurfaceLib::CGLTexImageIOSurface2D((CGLContextObj)[ctxt CGLContextObj],
                                                GL_TEXTURE_RECTANGLE_ARB,
                                                internalFormat,
                                                GetWidth(), GetHeight(),
                                                format, type,
                                                mIOSurfacePtr, plane);
}

CGColorSpaceRef CreateSystemColorSpace() {
    CMProfileRef system_profile = nullptr;
    CGColorSpaceRef cspace = nullptr;

    if (::CMGetSystemProfile(&system_profile) == noErr) {
      // Create a colorspace with the systems profile
      cspace = ::CGColorSpaceCreateWithPlatformColorSpace(system_profile);
      ::CMCloseProfile(system_profile);
    } else {
      // Default to generic
      cspace = ::CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
    }

    return cspace;
}

CGContextRef MacIOSurface::CreateIOSurfaceContext() {
  CGContextRef ref = MacIOSurfaceLib::IOSurfaceContextCreate(mIOSurfacePtr, GetWidth(), GetHeight(),
                                                8, 32, CreateSystemColorSpace(), 0x2002);
  return ref;
}

nsCARenderer::~nsCARenderer() {
  Destroy();
}

void cgdata_release_callback(void *aCGData, const void *data, size_t size) {
  if (aCGData) {
    free(aCGData);
  }
}

void nsCARenderer::Destroy() {
  if (mCARenderer) {
    CARenderer* caRenderer = (CARenderer*)mCARenderer;
    // Bug 556453:
    // Explicitly remove the layer from the renderer
    // otherwise it does not always happen right away.
    caRenderer.layer = nullptr;
    [caRenderer release];
  }
  if (mOpenGLContext) {
    if (mFBO || mIOTexture || mFBOTexture) {
      // Release these resources with the context that allocated them
      CGLContextObj oldContext = ::CGLGetCurrentContext();
      ::CGLSetCurrentContext(mOpenGLContext);

      if (mFBOTexture) {
        ::glDeleteTextures(1, &mFBOTexture);
      }
      if (mIOTexture) {
        ::glDeleteTextures(1, &mIOTexture);
      }
      if (mFBO) {
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
        ::glDeleteFramebuffersEXT(1, &mFBO);
      }

      if (oldContext)
        ::CGLSetCurrentContext(oldContext);
    }
    ::CGLDestroyContext((CGLContextObj)mOpenGLContext);
  }
  if (mCGImage) {
    ::CGImageRelease(mCGImage);
  }
  // mCGData is deallocated by cgdata_release_callback

  mCARenderer = nil;
  mFBOTexture = 0;
  mOpenGLContext = nullptr;
  mCGImage = nullptr;
  mIOSurface = nullptr;
  mFBO = 0;
  mIOTexture = 0;
}

nsresult nsCARenderer::SetupRenderer(void *aCALayer, int aWidth, int aHeight,
                                     AllowOfflineRendererEnum aAllowOfflineRenderer) {
  mAllowOfflineRenderer = aAllowOfflineRenderer;

  if (aWidth == 0 || aHeight == 0)
    return NS_ERROR_FAILURE;

  if (aWidth == mUnsupportedWidth &&
      aHeight == mUnsupportedHeight) {
    return NS_ERROR_FAILURE;
  }

  CALayer* layer = (CALayer*)aCALayer;
  CARenderer* caRenderer = nullptr;

  CGLPixelFormatAttribute attributes[] = {
    kCGLPFAAccelerated,
    kCGLPFADepthSize, (CGLPixelFormatAttribute)24,
    kCGLPFAAllowOfflineRenderers,
    (CGLPixelFormatAttribute)0
  };

  if (mAllowOfflineRenderer == DISALLOW_OFFLINE_RENDERER) {
    attributes[3] = (CGLPixelFormatAttribute)0;
  }

  GLint screen;
  CGLPixelFormatObj format;
  if (::CGLChoosePixelFormat(attributes, &format, &screen) != kCGLNoError) {
    mUnsupportedWidth = aWidth;
    mUnsupportedHeight = aHeight;
    Destroy();
    return NS_ERROR_FAILURE;
  }

  if (::CGLCreateContext(format, nullptr, &mOpenGLContext) != kCGLNoError) {
    mUnsupportedWidth = aWidth;
    mUnsupportedHeight = aHeight;
    Destroy();
    return NS_ERROR_FAILURE;
  }
  ::CGLDestroyPixelFormat(format);

  caRenderer = [[CARenderer rendererWithCGLContext:mOpenGLContext 
                            options:nil] retain];
  mCARenderer = caRenderer;
  if (caRenderer == nil) {
    mUnsupportedWidth = aWidth;
    mUnsupportedHeight = aHeight;
    Destroy();
    return NS_ERROR_FAILURE;
  }

  caRenderer.layer = layer;
  SetBounds(aWidth, aHeight);

  // We target rendering to a CGImage if no shared IOSurface are given.
  if (!mIOSurface) {
    mCGData = malloc(aWidth*aHeight*4);
    if (!mCGData) {
      mUnsupportedWidth = aWidth;
      mUnsupportedHeight = aHeight;
      Destroy();
      return NS_ERROR_FAILURE;
    }
    memset(mCGData, 0, aWidth*aHeight*4);

    CGDataProviderRef dataProvider = nullptr;
    dataProvider = ::CGDataProviderCreateWithData(mCGData,
                                        mCGData, aHeight*aWidth*4,
                                        cgdata_release_callback);
    if (!dataProvider) {
      cgdata_release_callback(mCGData, mCGData, aHeight*aWidth*4);
      mUnsupportedWidth = aWidth;
      mUnsupportedHeight = aHeight;
      Destroy();
      return NS_ERROR_FAILURE;
    }

    CGColorSpaceRef colorSpace = CreateSystemColorSpace();

    mCGImage = ::CGImageCreate(aWidth, aHeight, 8, 32, aWidth * 4, colorSpace,
                kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host,
                dataProvider, NULL, true, kCGRenderingIntentDefault);

    ::CGDataProviderRelease(dataProvider);
    if (colorSpace) {
      ::CGColorSpaceRelease(colorSpace);
    }
    if (!mCGImage) {
      mUnsupportedWidth = aWidth;
      mUnsupportedHeight = aHeight;
      Destroy();
      return NS_ERROR_FAILURE;
    }
  }

  CGLContextObj oldContext = ::CGLGetCurrentContext();
  ::CGLSetCurrentContext(mOpenGLContext);

  if (mIOSurface) {
    // Create the IOSurface mapped texture.
    ::glGenTextures(1, &mIOTexture);
    ::glBindTexture(GL_TEXTURE_RECTANGLE_ARB, mIOTexture);
    ::glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    ::glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    MacIOSurfaceLib::CGLTexImageIOSurface2D(mOpenGLContext, GL_TEXTURE_RECTANGLE_ARB,
                                           GL_RGBA, aWidth, aHeight,
                                           GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                                           mIOSurface->mIOSurfacePtr, 0);
    ::glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
  } else {
    ::glGenTextures(1, &mFBOTexture);
    ::glBindTexture(GL_TEXTURE_RECTANGLE_ARB, mFBOTexture);
    ::glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    ::glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ::glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
  }

  // Create the fbo
  ::glGenFramebuffersEXT(1, &mFBO);
  ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, mFBO);
  if (mIOSurface) {
   ::glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                GL_TEXTURE_RECTANGLE_ARB, mIOTexture, 0);
  } else {
    ::glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                GL_TEXTURE_RECTANGLE_ARB, mFBOTexture, 0);
  }


  // Make sure that the Framebuffer configuration is supported on the client machine
  GLenum fboStatus;
  fboStatus = ::glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
  if (fboStatus != GL_FRAMEBUFFER_COMPLETE_EXT) {
    NS_ERROR("FBO not supported");
    if (oldContext)
      ::CGLSetCurrentContext(oldContext);
    mUnsupportedWidth = aWidth;
    mUnsupportedHeight = aHeight;
    Destroy();
    return NS_ERROR_FAILURE;
  }

  SetViewport(aWidth, aHeight);

  GLenum result = ::glGetError();
  if (result != GL_NO_ERROR) {
    NS_ERROR("Unexpected OpenGL Error");
    mUnsupportedWidth = aWidth;
    mUnsupportedHeight = aHeight;
    Destroy();
    if (oldContext)
      ::CGLSetCurrentContext(oldContext);
    return NS_ERROR_FAILURE;
  }

  if (oldContext)
    ::CGLSetCurrentContext(oldContext);

  return NS_OK;
}

void nsCARenderer::SetBounds(int aWidth, int aHeight) {
  CARenderer* caRenderer = (CARenderer*)mCARenderer;
  CALayer* layer = [mCARenderer layer];

  // Create a transaction and disable animations
  // to make the position update instant.
  [CATransaction begin];
  NSMutableDictionary *newActions = [[NSMutableDictionary alloc] initWithObjectsAndKeys:[NSNull null], @"onOrderIn",
                                   [NSNull null], @"onOrderOut",
                                   [NSNull null], @"sublayers",
                                   [NSNull null], @"contents",
                                   [NSNull null], @"position",
                                   [NSNull null], @"bounds",
                                   nil];
  layer.actions = newActions;
  [newActions release];

  [CATransaction setValue: [NSNumber numberWithFloat:0.0f] forKey: kCATransactionAnimationDuration];
  [CATransaction setValue: (id) kCFBooleanTrue forKey: kCATransactionDisableActions];
  [layer setBounds:CGRectMake(0, 0, aWidth, aHeight)];
  [layer setPosition:CGPointMake(aWidth/2.0, aHeight/2.0)];
  caRenderer.bounds = CGRectMake(0, 0, aWidth, aHeight);
  [CATransaction commit];

}

void nsCARenderer::SetViewport(int aWidth, int aHeight) {
  ::glViewport(0.0, 0.0, aWidth, aHeight);
  ::glMatrixMode(GL_PROJECTION);
  ::glLoadIdentity();
  ::glOrtho (0.0, aWidth, 0.0, aHeight, -1, 1);

  // Render upside down to speed up CGContextDrawImage
  ::glTranslatef(0.0f, aHeight, 0.0);
  ::glScalef(1.0, -1.0, 1.0);
}

void nsCARenderer::AttachIOSurface(RefPtr<MacIOSurface> aSurface) {
  if (mIOSurface &&
      aSurface->GetIOSurfaceID() == mIOSurface->GetIOSurfaceID()) {
    // This object isn't needed since we already have a
    // handle to the same io surface.
    aSurface = nullptr;
    return;
  }

  mIOSurface = aSurface;

  // Update the framebuffer and viewport
  if (mCARenderer) {
    CARenderer* caRenderer = (CARenderer*)mCARenderer;
    int width = caRenderer.bounds.size.width;
    int height = caRenderer.bounds.size.height;

    CGLContextObj oldContext = ::CGLGetCurrentContext();
    ::CGLSetCurrentContext(mOpenGLContext);
    ::glBindTexture(GL_TEXTURE_RECTANGLE_ARB, mIOTexture);
    MacIOSurfaceLib::CGLTexImageIOSurface2D(mOpenGLContext, GL_TEXTURE_RECTANGLE_ARB,
                                           GL_RGBA, mIOSurface->GetWidth(), mIOSurface->GetHeight(),
                                           GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                                           mIOSurface->mIOSurfacePtr, 0);
    ::glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);

    // Rebind the FBO to make it live
    ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, mFBO);

    if (mIOSurface->GetWidth() != width || mIOSurface->GetHeight() != height) {
      width = mIOSurface->GetWidth();
      height = mIOSurface->GetHeight();
      SetBounds(width, height);
      SetViewport(width, height);
    }

    if (oldContext) {
      ::CGLSetCurrentContext(oldContext);
    }
  }
}

IOSurfaceID nsCARenderer::GetIOSurfaceID() {
  if (!mIOSurface) {
    return 0;
  }

  return mIOSurface->GetIOSurfaceID();
}

nsresult nsCARenderer::Render(int aWidth, int aHeight, 
                              CGImageRef *aOutCGImage) {
  if (!aOutCGImage && !mIOSurface) {
    NS_ERROR("No target destination for rendering");
  } else if (aOutCGImage) {
    // We are expected to return a CGImageRef, we will set
    // it to NULL in case we fail before the image is ready.
    *aOutCGImage = NULL;
  }

  if (aWidth == 0 || aHeight == 0)
    return NS_OK;

  if (!mCARenderer) {
    return NS_ERROR_FAILURE;
  }

  CARenderer* caRenderer = (CARenderer*)mCARenderer;
  int renderer_width = caRenderer.bounds.size.width;
  int renderer_height = caRenderer.bounds.size.height;

  if (renderer_width != aWidth || renderer_height != aHeight) {
    // XXX: This should be optimized to not rescale the buffer
    //      if we are resizing down.
    CALayer* caLayer = [caRenderer layer];
    Destroy();
    if (SetupRenderer(caLayer, aWidth, aHeight,
                      mAllowOfflineRenderer) != NS_OK) {
      return NS_ERROR_FAILURE;
    }

    caRenderer = (CARenderer*)mCARenderer;
  }

  CGLContextObj oldContext = ::CGLGetCurrentContext();
  ::CGLSetCurrentContext(mOpenGLContext);
  if (!mIOSurface) {
    // If no shared IOSurface is given render to our own
    // texture for readback.
    ::glGenTextures(1, &mFBOTexture);
  }

  GLenum result = ::glGetError();
  if (result != GL_NO_ERROR) {
    NS_ERROR("Unexpected OpenGL Error");
    Destroy();
    if (oldContext)
      ::CGLSetCurrentContext(oldContext);
    return NS_ERROR_FAILURE;
  }

  ::glClearColor(0.0, 0.0, 0.0, 0.0);
  ::glClear(GL_COLOR_BUFFER_BIT);

  [CATransaction commit];
  double caTime = ::CACurrentMediaTime();
  [caRenderer beginFrameAtTime:caTime timeStamp:NULL];
  [caRenderer addUpdateRect:CGRectMake(0,0, aWidth, aHeight)];
  [caRenderer render];
  [caRenderer endFrame];

  // Read the data back either to the IOSurface or mCGImage.
  if (mIOSurface) {
    ::glFlush();
  } else {
    ::glPixelStorei(GL_PACK_ALIGNMENT, 4);
    ::glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    ::glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    ::glPixelStorei(GL_PACK_SKIP_PIXELS, 0);

    ::glReadPixels(0.0f, 0.0f, aWidth, aHeight,
                        GL_BGRA, GL_UNSIGNED_BYTE,
                        mCGData);

    *aOutCGImage = mCGImage;
  }

  if (oldContext) {
    ::CGLSetCurrentContext(oldContext);
  }

  return NS_OK;
}

nsresult nsCARenderer::DrawSurfaceToCGContext(CGContextRef aContext, 
                                              MacIOSurface *surf, 
                                              CGColorSpaceRef aColorSpace,
                                              int aX, int aY,
                                              size_t aWidth, size_t aHeight) {
  surf->Lock();
  size_t bytesPerRow = surf->GetBytesPerRow();
  size_t ioWidth = surf->GetWidth();
  size_t ioHeight = surf->GetHeight();

  // We get rendering glitches if we use a width/height that falls
  // outside of the IOSurface.
  if (aWidth + aX > ioWidth) 
    aWidth = ioWidth - aX;
  if (aHeight + aY > ioHeight) 
    aHeight = ioHeight - aY;

  if (aX < 0 || aX >= ioWidth ||
      aY < 0 || aY >= ioHeight) {
    surf->Unlock();
    return NS_ERROR_FAILURE;
  }

  void* ioData = surf->GetBaseAddress();
  CGDataProviderRef dataProvider = ::CGDataProviderCreateWithData(ioData,
                                      ioData, ioHeight*(bytesPerRow)*4, 
                                      NULL); //No release callback 
  if (!dataProvider) {
    surf->Unlock();
    return NS_ERROR_FAILURE;
  }

  CGImageRef cgImage = ::CGImageCreate(ioWidth, ioHeight, 8, 32, bytesPerRow,
              aColorSpace, kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host,
              dataProvider, NULL, true, kCGRenderingIntentDefault);
  ::CGDataProviderRelease(dataProvider);
  if (!cgImage) {
    surf->Unlock();
    return NS_ERROR_FAILURE;
  }
  CGImageRef subImage = ::CGImageCreateWithImageInRect(cgImage,
                                       ::CGRectMake(aX, aY, aWidth, aHeight));
  if (!subImage) {
    ::CGImageRelease(cgImage);
    surf->Unlock();
    return NS_ERROR_FAILURE;
  }

  ::CGContextScaleCTM(aContext, 1.0f, -1.0f);
  ::CGContextDrawImage(aContext, 
                       CGRectMake(aX, -(CGFloat)aY - (CGFloat)aHeight, 
                                  aWidth, aHeight), 
                       subImage);

  ::CGImageRelease(subImage);
  ::CGImageRelease(cgImage);
  surf->Unlock();
  return NS_OK;
}

void nsCARenderer::DettachCALayer() {
  CARenderer* caRenderer = (CARenderer*)mCARenderer;

  caRenderer.layer = nil;
}

void nsCARenderer::AttachCALayer(void *aCALayer) {
  CARenderer* caRenderer = (CARenderer*)mCARenderer;

  CALayer* caLayer = (CALayer*)aCALayer;
  caRenderer.layer = caLayer;
}

#ifdef DEBUG

int sSaveToDiskSequence = 0;
void nsCARenderer::SaveToDisk(MacIOSurface *surf) {
  surf->Lock();
  size_t bytesPerRow = surf->GetBytesPerRow();
  size_t ioWidth = surf->GetWidth();
  size_t ioHeight = surf->GetHeight();
  void* ioData = surf->GetBaseAddress();
  CGDataProviderRef dataProvider = ::CGDataProviderCreateWithData(ioData,
                                      ioData, ioHeight*(bytesPerRow)*4, 
                                      NULL); //No release callback 
  if (!dataProvider) {
    surf->Unlock();
    return;
  }

  CGColorSpaceRef colorSpace = CreateSystemColorSpace();
  CGImageRef cgImage = ::CGImageCreate(ioWidth, ioHeight, 8, 32, bytesPerRow,
              colorSpace, kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host,
              dataProvider, NULL, true, kCGRenderingIntentDefault);
  ::CGDataProviderRelease(dataProvider);
  ::CGColorSpaceRelease(colorSpace);
  if (!cgImage) {
    surf->Unlock();
    return;
  }

  char cstr[1000];

  sprintf(cstr, "file:///Users/benoitgirard/debug/iosurface_%i.png", ++sSaveToDiskSequence);

  CFStringRef cfStr = ::CFStringCreateWithCString(kCFAllocatorDefault, cstr, kCFStringEncodingMacRoman);

  printf("Exporting: %s\n", cstr);
  CFURLRef url = ::CFURLCreateWithString( NULL, cfStr, NULL);
  ::CFRelease(cfStr);

  CFStringRef type = kUTTypePNG;
  size_t count = 1;
  CFDictionaryRef options = NULL;
  CGImageDestinationRef dest = ::CGImageDestinationCreateWithURL(url, type, count, options);
  ::CFRelease(url);

  ::CGImageDestinationAddImage(dest, cgImage, NULL);

  ::CGImageDestinationFinalize(dest);
  ::CFRelease(dest);
  ::CGImageRelease(cgImage);

  surf->Unlock();

  return;

}

#endif

CGImageRef MacIOSurface::CreateImageFromIOSurfaceContext(CGContextRef aContext) {
  if (!MacIOSurfaceLib::isInit())
    return nsnull;

  return MacIOSurfaceLib::IOSurfaceContextCreateImage(aContext);
}

TemporaryRef<MacIOSurface> MacIOSurface::IOSurfaceContextGetSurface(CGContextRef aContext) {
  if (!MacIOSurfaceLib::isInit())
    return nsnull;

  IOSurfacePtr surfaceRef = MacIOSurfaceLib::IOSurfaceContextGetSurface(aContext);
  if (!surfaceRef)
    return nsnull;

  // Retain the IOSurface because MacIOSurface will release it
  CFRetain(surfaceRef);

  RefPtr<MacIOSurface> ioSurface = new MacIOSurface(surfaceRef);
  if (!ioSurface) {
    ::CFRelease(surfaceRef);
    return nsnull;
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


