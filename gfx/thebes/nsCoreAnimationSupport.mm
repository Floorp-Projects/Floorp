/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCoreAnimationSupport.h"
#include "nsDebug.h"

#import <QuartzCore/QuartzCore.h>
#import <AppKit/NSOpenGL.h>
#include <dlfcn.h>

#define IOSURFACE_FRAMEWORK_PATH \
  "/System/Library/Frameworks/IOSurface.framework/IOSurface"
#define OPENGL_FRAMEWORK_PATH \
  "/System/Library/Frameworks/OpenGL.framework/OpenGL"


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

#define GET_CONST(const_name) \
  ((CFStringRef*) dlsym(sIOSurfaceFramework, const_name))
#define GET_IOSYM(dest,sym_name) \
  (typeof(dest)) dlsym(sIOSurfaceFramework, sym_name)
#define GET_CGLSYM(dest,sym_name) \
  (typeof(dest)) dlsym(sOpenGLFramework, sym_name)

class nsIOSurfaceLib: public nsIOSurface {
public:
  static void                        *sIOSurfaceFramework;
  static void                        *sOpenGLFramework;
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

nsIOSurfaceLib::LibraryUnloader nsIOSurfaceLib::sLibraryUnloader;
bool                          nsIOSurfaceLib::isLoaded = false;
void*                         nsIOSurfaceLib::sIOSurfaceFramework;
void*                         nsIOSurfaceLib::sOpenGLFramework;
IOSurfaceCreateFunc           nsIOSurfaceLib::sCreate;
IOSurfaceGetIDFunc            nsIOSurfaceLib::sGetID;
IOSurfaceLookupFunc           nsIOSurfaceLib::sLookup;
IOSurfaceGetBaseAddressFunc   nsIOSurfaceLib::sGetBaseAddress;
IOSurfaceGetHeightFunc        nsIOSurfaceLib::sWidth;
IOSurfaceGetWidthFunc         nsIOSurfaceLib::sHeight;
IOSurfaceGetBytesPerRowFunc   nsIOSurfaceLib::sBytesPerRow;
IOSurfaceLockFunc             nsIOSurfaceLib::sLock;
IOSurfaceUnlockFunc           nsIOSurfaceLib::sUnlock;
CGLTexImageIOSurface2DFunc    nsIOSurfaceLib::sTexImage;
CFStringRef                   nsIOSurfaceLib::kPropWidth;
CFStringRef                   nsIOSurfaceLib::kPropHeight;
CFStringRef                   nsIOSurfaceLib::kPropBytesPerElem;
CFStringRef                   nsIOSurfaceLib::kPropBytesPerRow;
CFStringRef                   nsIOSurfaceLib::kPropIsGlobal;

bool nsIOSurfaceLib::isInit() {
  // Guard against trying to reload the library
  // if it is not available.
  if (!isLoaded)
    LoadLibrary();
  if (!sIOSurfaceFramework) {
    NS_ERROR("nsIOSurfaceLib failed to initialize");
  }
  return sIOSurfaceFramework;
}

IOSurfacePtr nsIOSurfaceLib::IOSurfaceCreate(CFDictionaryRef properties) {
  return sCreate(properties);
}

IOSurfacePtr nsIOSurfaceLib::IOSurfaceLookup(IOSurfaceID aIOSurfaceID) {
  return sLookup(aIOSurfaceID);
}

IOSurfaceID nsIOSurfaceLib::IOSurfaceGetID(IOSurfacePtr aIOSurfacePtr) {
  return sGetID(aIOSurfacePtr);
}

void* nsIOSurfaceLib::IOSurfaceGetBaseAddress(IOSurfacePtr aIOSurfacePtr) {
  return sGetBaseAddress(aIOSurfacePtr);
}

size_t nsIOSurfaceLib::IOSurfaceGetWidth(IOSurfacePtr aIOSurfacePtr) {
  return sWidth(aIOSurfacePtr);
}

size_t nsIOSurfaceLib::IOSurfaceGetHeight(IOSurfacePtr aIOSurfacePtr) {
  return sHeight(aIOSurfacePtr);
}

size_t nsIOSurfaceLib::IOSurfaceGetBytesPerRow(IOSurfacePtr aIOSurfacePtr) {
  return sBytesPerRow(aIOSurfacePtr);
}

IOReturn nsIOSurfaceLib::IOSurfaceLock(IOSurfacePtr aIOSurfacePtr, 
                                       uint32_t options, uint32_t *seed) {
  return sLock(aIOSurfacePtr, options, seed);
}

IOReturn nsIOSurfaceLib::IOSurfaceUnlock(IOSurfacePtr aIOSurfacePtr, 
                                         uint32_t options, uint32_t *seed) {
  return sUnlock(aIOSurfacePtr, options, seed);
}

CGLError nsIOSurfaceLib::CGLTexImageIOSurface2D(CGLContextObj ctxt,
                             GLenum target, GLenum internalFormat,
                             GLsizei width, GLsizei height,
                             GLenum format, GLenum type,
                             IOSurfacePtr ioSurface, GLuint plane) {
  return sTexImage(ctxt, target, internalFormat, width, height,
                   format, type, ioSurface, plane);
}

CFStringRef nsIOSurfaceLib::GetIOConst(const char* symbole) {
  CFStringRef *address = (CFStringRef*)dlsym(sIOSurfaceFramework, symbole);
  if (!address)
    return nsnull;

  return *address;
}

void nsIOSurfaceLib::LoadLibrary() {
  if (isLoaded) {
    return;
  } 
  isLoaded = true;
  sIOSurfaceFramework = dlopen(IOSURFACE_FRAMEWORK_PATH, 
                            RTLD_LAZY | RTLD_LOCAL);
  sOpenGLFramework = dlopen(OPENGL_FRAMEWORK_PATH, 
                            RTLD_LAZY | RTLD_LOCAL);
  if (!sIOSurfaceFramework) {
    return;
  }
  if (!sOpenGLFramework) {
    dlclose(sIOSurfaceFramework);
    sIOSurfaceFramework = nsnull;
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

  if (!sCreate || !sGetID || !sLookup || !sTexImage || !sGetBaseAddress ||
      !kPropWidth || !kPropHeight || !kPropBytesPerElem || !kPropIsGlobal ||
      !sLock || !sUnlock || !sWidth || !sHeight || !kPropBytesPerRow ||
      !sBytesPerRow) {
    CloseLibrary();
  }
}

void nsIOSurfaceLib::CloseLibrary() {
  if (sIOSurfaceFramework) {
    dlclose(sIOSurfaceFramework);
  }
  if (sOpenGLFramework) {
    dlclose(sOpenGLFramework);
  }
  sIOSurfaceFramework = nsnull;
  sOpenGLFramework = nsnull;
}

nsIOSurface::~nsIOSurface() {
  CFRelease(mIOSurfacePtr);
}

already_AddRefed<nsIOSurface> nsIOSurface::CreateIOSurface(int aWidth, int aHeight) { 
  if (!nsIOSurfaceLib::isInit())
    return nsnull;

  CFMutableDictionaryRef props = ::CFDictionaryCreateMutable(
                      kCFAllocatorDefault, 4,
                      &kCFTypeDictionaryKeyCallBacks,
                      &kCFTypeDictionaryValueCallBacks);
  if (!props)
    return nsnull;

  int32_t bytesPerElem = 4;
  CFNumberRef cfWidth = ::CFNumberCreate(NULL, kCFNumberSInt32Type, &aWidth);
  CFNumberRef cfHeight = ::CFNumberCreate(NULL, kCFNumberSInt32Type, &aHeight);
  CFNumberRef cfBytesPerElem = ::CFNumberCreate(NULL, kCFNumberSInt32Type, &bytesPerElem);
  ::CFDictionaryAddValue(props, nsIOSurfaceLib::kPropWidth,
                                cfWidth);
  ::CFRelease(cfWidth);
  ::CFDictionaryAddValue(props, nsIOSurfaceLib::kPropHeight,
                                cfHeight);
  ::CFRelease(cfHeight);
  ::CFDictionaryAddValue(props, nsIOSurfaceLib::kPropBytesPerElem, 
                                cfBytesPerElem);
  ::CFRelease(cfBytesPerElem);
  ::CFDictionaryAddValue(props, nsIOSurfaceLib::kPropIsGlobal, 
                                kCFBooleanTrue);

  IOSurfacePtr surfaceRef = nsIOSurfaceLib::IOSurfaceCreate(props);
  ::CFRelease(props);

  if (!surfaceRef)
    return nsnull;

  nsRefPtr<nsIOSurface> ioSurface = new nsIOSurface(surfaceRef);
  if (!ioSurface) {
    ::CFRelease(surfaceRef);
    return nsnull;
  }

  return ioSurface.forget();
}

already_AddRefed<nsIOSurface> nsIOSurface::LookupSurface(IOSurfaceID aIOSurfaceID) { 
  if (!nsIOSurfaceLib::isInit())
    return nsnull;

  IOSurfacePtr surfaceRef = nsIOSurfaceLib::IOSurfaceLookup(aIOSurfaceID);
  if (!surfaceRef)
    return nsnull;

  nsRefPtr<nsIOSurface> ioSurface = new nsIOSurface(surfaceRef);
  if (!ioSurface) {
    ::CFRelease(surfaceRef);
    return nsnull;
  }
  return ioSurface.forget();
}

IOSurfaceID nsIOSurface::GetIOSurfaceID() { 
  return nsIOSurfaceLib::IOSurfaceGetID(mIOSurfacePtr);
}

void* nsIOSurface::GetBaseAddress() { 
  return nsIOSurfaceLib::IOSurfaceGetBaseAddress(mIOSurfacePtr);
}

size_t nsIOSurface::GetWidth() { 
  return nsIOSurfaceLib::IOSurfaceGetWidth(mIOSurfacePtr);
}

size_t nsIOSurface::GetHeight() { 
  return nsIOSurfaceLib::IOSurfaceGetHeight(mIOSurfacePtr);
}

size_t nsIOSurface::GetBytesPerRow() { 
  return nsIOSurfaceLib::IOSurfaceGetBytesPerRow(mIOSurfacePtr);
}

#define READ_ONLY 0x1
void nsIOSurface::Lock() {
  nsIOSurfaceLib::IOSurfaceLock(mIOSurfacePtr, READ_ONLY, NULL);
}

void nsIOSurface::Unlock() {
  nsIOSurfaceLib::IOSurfaceUnlock(mIOSurfacePtr, READ_ONLY, NULL);
}

#include "gfxImageSurface.h"

already_AddRefed<gfxASurface>
nsIOSurface::GetAsSurface() {
  Lock();
  size_t bytesPerRow = GetBytesPerRow();
  size_t ioWidth = GetWidth();
  size_t ioHeight = GetHeight();

  unsigned char* ioData = (unsigned char*)GetBaseAddress();

  nsRefPtr<gfxImageSurface> imgSurface =
    new gfxImageSurface(gfxIntSize(ioWidth, ioHeight), gfxASurface::ImageFormatARGB32);

  for (int i = 0; i < ioHeight; i++) {
    memcpy(imgSurface->Data() + i * imgSurface->Stride(),
           ioData + i * bytesPerRow, ioWidth * 4);
  }

  Unlock();

  return imgSurface.forget();
}

CGLError 
nsIOSurface::CGLTexImageIOSurface2D(void *c,
                                    GLenum internalFormat, GLenum format, 
                                    GLenum type, GLuint plane)
{
  NSOpenGLContext *ctxt = static_cast<NSOpenGLContext*>(c);
  return nsIOSurfaceLib::CGLTexImageIOSurface2D((CGLContextObj)[ctxt CGLContextObj],
                                                GL_TEXTURE_RECTANGLE_ARB,
                                                internalFormat,
                                                GetWidth(), GetHeight(),
                                                format, type,
                                                mIOSurfacePtr, plane);
}

nsCARenderer::~nsCARenderer() {
  Destroy();
}

CGColorSpaceRef CreateSystemColorSpace() {
    CMProfileRef system_profile = nsnull;
    CGColorSpaceRef cspace = nsnull;

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
    caRenderer.layer = nsnull;
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
  mOpenGLContext = nsnull;
  mCGImage = nsnull;
  mIOSurface = nsnull;
  mFBO = nsnull;
  mIOTexture = nsnull;
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
  CARenderer* caRenderer = nsnull;

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

  if (::CGLCreateContext(format, nsnull, &mOpenGLContext) != kCGLNoError) {
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

    CGDataProviderRef dataProvider = nsnull;
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
    nsIOSurfaceLib::CGLTexImageIOSurface2D(mOpenGLContext, GL_TEXTURE_RECTANGLE_ARB,
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

void nsCARenderer::AttachIOSurface(nsRefPtr<nsIOSurface> aSurface) {
  if (mIOSurface &&
      aSurface->GetIOSurfaceID() == mIOSurface->GetIOSurfaceID()) {
    // This object isn't needed since we already have a
    // handle to the same io surface.
    aSurface = nsnull;
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
    nsIOSurfaceLib::CGLTexImageIOSurface2D(mOpenGLContext, GL_TEXTURE_RECTANGLE_ARB,
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
                                              nsIOSurface *surf, 
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
void nsCARenderer::SaveToDisk(nsIOSurface *surf) {
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

