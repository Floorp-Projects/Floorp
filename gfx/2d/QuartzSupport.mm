/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuartzSupport.h"
#include "nsDebug.h"
#include "MacIOSurface.h"
#include "mozilla/Sprintf.h"

#import <QuartzCore/QuartzCore.h>
#import <AppKit/NSOpenGL.h>
#import <OpenGL/CGLIOSurface.h>
#include <dlfcn.h>
#include "GLDefs.h"

#define IOSURFACE_FRAMEWORK_PATH "/System/Library/Frameworks/IOSurface.framework/IOSurface"
#define OPENGL_FRAMEWORK_PATH "/System/Library/Frameworks/OpenGL.framework/OpenGL"
#define COREGRAPHICS_FRAMEWORK_PATH                                                             \
  "/System/Library/Frameworks/ApplicationServices.framework/Frameworks/CoreGraphics.framework/" \
  "CoreGraphics"

@interface CALayer (ContentsScale)
- (double)contentsScale;
- (void)setContentsScale:(double)scale;
@end

CGColorSpaceRef CreateSystemColorSpace() {
  CGColorSpaceRef cspace = ::CGDisplayCopyColorSpace(::CGMainDisplayID());
  if (!cspace) {
    cspace = ::CGColorSpaceCreateDeviceRGB();
  }
  return cspace;
}

nsCARenderer::~nsCARenderer() { Destroy(); }

static void cgdata_release_callback(void* aCGData, const void* data, size_t size) {
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
  if (mWrapperCALayer) {
    CALayer* wrapperLayer = (CALayer*)mWrapperCALayer;
    [wrapperLayer release];
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

      if (oldContext) ::CGLSetCurrentContext(oldContext);
    }
    ::CGLDestroyContext((CGLContextObj)mOpenGLContext);
  }
  if (mCGImage) {
    ::CGImageRelease(mCGImage);
  }
  // mCGData is deallocated by cgdata_release_callback

  mCARenderer = nil;
  mWrapperCALayer = nil;
  mFBOTexture = 0;
  mOpenGLContext = nullptr;
  mCGImage = nullptr;
  mIOSurface = nullptr;
  mFBO = 0;
  mIOTexture = 0;
}

nsresult nsCARenderer::SetupRenderer(void* aCALayer, int aWidth, int aHeight,
                                     double aContentsScaleFactor,
                                     AllowOfflineRendererEnum aAllowOfflineRenderer) {
  mAllowOfflineRenderer = aAllowOfflineRenderer;

  if (aWidth == 0 || aHeight == 0 || aContentsScaleFactor <= 0) return NS_ERROR_FAILURE;

  if (aWidth == mUnsupportedWidth && aHeight == mUnsupportedHeight) {
    return NS_ERROR_FAILURE;
  }

  CGLPixelFormatAttribute attributes[] = {kCGLPFAAccelerated, kCGLPFADepthSize,
                                          (CGLPixelFormatAttribute)24, kCGLPFAAllowOfflineRenderers,
                                          (CGLPixelFormatAttribute)0};

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

  CARenderer* caRenderer = [[CARenderer rendererWithCGLContext:mOpenGLContext options:nil] retain];
  if (caRenderer == nil) {
    mUnsupportedWidth = aWidth;
    mUnsupportedHeight = aHeight;
    Destroy();
    return NS_ERROR_FAILURE;
  }
  CALayer* wrapperCALayer = [[CALayer layer] retain];
  if (wrapperCALayer == nil) {
    [caRenderer release];
    mUnsupportedWidth = aWidth;
    mUnsupportedHeight = aHeight;
    Destroy();
    return NS_ERROR_FAILURE;
  }

  mCARenderer = caRenderer;
  mWrapperCALayer = wrapperCALayer;
  caRenderer.layer = wrapperCALayer;
  [wrapperCALayer addSublayer:(CALayer*)aCALayer];
  mContentsScaleFactor = aContentsScaleFactor;
  size_t intScaleFactor = ceil(mContentsScaleFactor);
  SetBounds(aWidth, aHeight);

  // We target rendering to a CGImage if no shared IOSurface are given.
  if (!mIOSurface) {
    mCGData = malloc(aWidth * intScaleFactor * aHeight * 4 * intScaleFactor);
    if (!mCGData) {
      mUnsupportedWidth = aWidth;
      mUnsupportedHeight = aHeight;
      Destroy();
      return NS_ERROR_FAILURE;
    }
    memset(mCGData, 0, aWidth * intScaleFactor * aHeight * 4 * intScaleFactor);

    CGDataProviderRef dataProvider = nullptr;
    dataProvider = ::CGDataProviderCreateWithData(
        mCGData, mCGData, aHeight * intScaleFactor * aWidth * 4 * intScaleFactor,
        cgdata_release_callback);
    if (!dataProvider) {
      cgdata_release_callback(mCGData, mCGData,
                              aHeight * intScaleFactor * aWidth * 4 * intScaleFactor);
      mUnsupportedWidth = aWidth;
      mUnsupportedHeight = aHeight;
      Destroy();
      return NS_ERROR_FAILURE;
    }

    CGColorSpaceRef colorSpace = CreateSystemColorSpace();

    mCGImage = ::CGImageCreate(aWidth * intScaleFactor, aHeight * intScaleFactor, 8, 32,
                               aWidth * intScaleFactor * 4, colorSpace,
                               kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host,
                               dataProvider, nullptr, true, kCGRenderingIntentDefault);

    ::CGDataProviderRelease(dataProvider);
    ::CGColorSpaceRelease(colorSpace);
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
    ::CGLTexImageIOSurface2D(mOpenGLContext, GL_TEXTURE_RECTANGLE_ARB, GL_RGBA,
                             aWidth * intScaleFactor, aHeight * intScaleFactor, GL_BGRA,
                             GL_UNSIGNED_INT_8_8_8_8_REV, mIOSurface->GetIOSurfaceRef().get(), 0);
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
    if (oldContext) ::CGLSetCurrentContext(oldContext);
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
    if (oldContext) ::CGLSetCurrentContext(oldContext);
    return NS_ERROR_FAILURE;
  }

  if (oldContext) ::CGLSetCurrentContext(oldContext);

  return NS_OK;
}

void nsCARenderer::SetBounds(int aWidth, int aHeight) {
  CARenderer* caRenderer = (CARenderer*)mCARenderer;
  CALayer* wrapperLayer = (CALayer*)mWrapperCALayer;
  NSArray* sublayers = [wrapperLayer sublayers];
  CALayer* pluginLayer = (CALayer*)[sublayers objectAtIndex:0];

  // Create a transaction and disable animations
  // to make the position update instant.
  [CATransaction begin];
  NSMutableDictionary* newActions = [[NSMutableDictionary alloc]
      initWithObjectsAndKeys:[NSNull null], @"onOrderIn", [NSNull null], @"onOrderOut",
                             [NSNull null], @"sublayers", [NSNull null], @"contents", [NSNull null],
                             @"position", [NSNull null], @"bounds", nil];
  wrapperLayer.actions = newActions;
  [newActions release];

  // If we're in HiDPI mode, mContentsScaleFactor will (presumably) be 2.0.
  // For some reason, to make things work properly in HiDPI mode we need to
  // make caRenderer's 'bounds' and 'layer' different sizes -- to set 'bounds'
  // to the size of 'layer's backing store.  And to avoid this possibly
  // confusing the plugin, we need to hide it's effects from the plugin by
  // making pluginLayer (usually the CALayer* provided by the plugin) a
  // sublayer of our own wrapperLayer (see bug 829284).
  size_t intScaleFactor = ceil(mContentsScaleFactor);
  [CATransaction setValue:[NSNumber numberWithFloat:0.0f] forKey:kCATransactionAnimationDuration];
  [CATransaction setValue:(id)kCFBooleanTrue forKey:kCATransactionDisableActions];
  [wrapperLayer setBounds:CGRectMake(0, 0, aWidth, aHeight)];
  [wrapperLayer setPosition:CGPointMake(aWidth / 2.0, aHeight / 2.0)];
  [pluginLayer setBounds:CGRectMake(0, 0, aWidth, aHeight)];
  [pluginLayer setFrame:CGRectMake(0, 0, aWidth, aHeight)];
  caRenderer.bounds = CGRectMake(0, 0, aWidth * intScaleFactor, aHeight * intScaleFactor);
  if (mContentsScaleFactor != 1.0) {
    CGAffineTransform affineTransform = [wrapperLayer affineTransform];
    affineTransform.a = mContentsScaleFactor;
    affineTransform.d = mContentsScaleFactor;
    affineTransform.tx = ((double)aWidth) / mContentsScaleFactor;
    affineTransform.ty = ((double)aHeight) / mContentsScaleFactor;
    [wrapperLayer setAffineTransform:affineTransform];
  } else {
    // These settings are the default values.  But they might have been
    // changed as above if we were previously running in a HiDPI mode
    // (i.e. if we just switched from that to a non-HiDPI mode).
    [wrapperLayer setAffineTransform:CGAffineTransformIdentity];
  }
  [CATransaction commit];
}

void nsCARenderer::SetViewport(int aWidth, int aHeight) {
  size_t intScaleFactor = ceil(mContentsScaleFactor);
  aWidth *= intScaleFactor;
  aHeight *= intScaleFactor;

  ::glViewport(0.0, 0.0, aWidth, aHeight);
  ::glMatrixMode(GL_PROJECTION);
  ::glLoadIdentity();
  ::glOrtho(0.0, aWidth, 0.0, aHeight, -1, 1);

  // Render upside down to speed up CGContextDrawImage
  ::glTranslatef(0.0f, aHeight, 0.0);
  ::glScalef(1.0, -1.0, 1.0);
}

void nsCARenderer::AttachIOSurface(MacIOSurface* aSurface) {
  if (mIOSurface && aSurface->GetIOSurfaceID() == mIOSurface->GetIOSurfaceID()) {
    return;
  }

  mIOSurface = aSurface;

  // Update the framebuffer and viewport
  if (mCARenderer) {
    CARenderer* caRenderer = (CARenderer*)mCARenderer;
    size_t intScaleFactor = ceil(mContentsScaleFactor);
    int width = caRenderer.bounds.size.width / intScaleFactor;
    int height = caRenderer.bounds.size.height / intScaleFactor;

    CGLContextObj oldContext = ::CGLGetCurrentContext();
    ::CGLSetCurrentContext(mOpenGLContext);
    ::glBindTexture(GL_TEXTURE_RECTANGLE_ARB, mIOTexture);
    ::CGLTexImageIOSurface2D(mOpenGLContext, GL_TEXTURE_RECTANGLE_ARB, GL_RGBA,
                             mIOSurface->GetDevicePixelWidth(), mIOSurface->GetDevicePixelHeight(),
                             GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                             mIOSurface->GetIOSurfaceRef().get(), 0);
    ::glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);

    // Rebind the FBO to make it live
    ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, mFBO);

    if (static_cast<int>(mIOSurface->GetWidth()) != width ||
        static_cast<int>(mIOSurface->GetHeight()) != height) {
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

nsresult nsCARenderer::Render(int aWidth, int aHeight, double aContentsScaleFactor,
                              CGImageRef* aOutCGImage) {
  if (!aOutCGImage && !mIOSurface) {
    NS_ERROR("No target destination for rendering");
  } else if (aOutCGImage) {
    // We are expected to return a CGImageRef, we will set
    // it to nullptr in case we fail before the image is ready.
    *aOutCGImage = nullptr;
  }

  if (aWidth == 0 || aHeight == 0 || aContentsScaleFactor <= 0) return NS_OK;

  if (!mCARenderer || !mWrapperCALayer) {
    return NS_ERROR_FAILURE;
  }

  CARenderer* caRenderer = (CARenderer*)mCARenderer;
  CALayer* wrapperLayer = (CALayer*)mWrapperCALayer;
  size_t intScaleFactor = ceil(aContentsScaleFactor);
  int renderer_width = caRenderer.bounds.size.width / intScaleFactor;
  int renderer_height = caRenderer.bounds.size.height / intScaleFactor;

  if (renderer_width != aWidth || renderer_height != aHeight ||
      mContentsScaleFactor != aContentsScaleFactor) {
    // XXX: This should be optimized to not rescale the buffer
    //      if we are resizing down.
    // caLayer may be the CALayer* provided by the plugin, so we need to
    // preserve it across the call to Destroy().
    NSArray* sublayers = [wrapperLayer sublayers];
    CALayer* caLayer = (CALayer*)[sublayers objectAtIndex:0];
    // mIOSurface is set by AttachIOSurface(), not by SetupRenderer().  So
    // since it may have been set by a prior call to AttachIOSurface(), we
    // need to preserve it across the call to Destroy().
    RefPtr<MacIOSurface> ioSurface = mIOSurface;
    Destroy();
    mIOSurface = ioSurface;
    if (SetupRenderer(caLayer, aWidth, aHeight, aContentsScaleFactor, mAllowOfflineRenderer) !=
        NS_OK) {
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
    if (oldContext) ::CGLSetCurrentContext(oldContext);
    return NS_ERROR_FAILURE;
  }

  ::glClearColor(0.0, 0.0, 0.0, 0.0);
  ::glClear(GL_COLOR_BUFFER_BIT);

  [CATransaction commit];
  double caTime = ::CACurrentMediaTime();
  [caRenderer beginFrameAtTime:caTime timeStamp:nullptr];
  [caRenderer addUpdateRect:CGRectMake(0, 0, aWidth * intScaleFactor, aHeight * intScaleFactor)];
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

    ::glReadPixels(0.0f, 0.0f, aWidth * intScaleFactor, aHeight * intScaleFactor, GL_BGRA,
                   GL_UNSIGNED_BYTE, mCGData);

    *aOutCGImage = mCGImage;
  }

  if (oldContext) {
    ::CGLSetCurrentContext(oldContext);
  }

  return NS_OK;
}

nsresult nsCARenderer::DrawSurfaceToCGContext(CGContextRef aContext, MacIOSurface* surf,
                                              CGColorSpaceRef aColorSpace, int aX, int aY,
                                              size_t aWidth, size_t aHeight) {
  surf->Lock();
  size_t bytesPerRow = surf->GetBytesPerRow();
  size_t ioWidth = surf->GetWidth();
  size_t ioHeight = surf->GetHeight();

  // We get rendering glitches if we use a width/height that falls
  // outside of the IOSurface.
  if (aWidth + aX > ioWidth) aWidth = ioWidth - aX;
  if (aHeight + aY > ioHeight) aHeight = ioHeight - aY;

  if (aX < 0 || static_cast<size_t>(aX) >= ioWidth || aY < 0 ||
      static_cast<size_t>(aY) >= ioHeight) {
    surf->Unlock();
    return NS_ERROR_FAILURE;
  }

  void* ioData = surf->GetBaseAddress();
  CGDataProviderRef dataProvider =
      ::CGDataProviderCreateWithData(ioData, ioData, ioHeight * (bytesPerRow)*4,
                                     nullptr);  // No release callback
  if (!dataProvider) {
    surf->Unlock();
    return NS_ERROR_FAILURE;
  }

  CGImageRef cgImage = ::CGImageCreate(ioWidth, ioHeight, 8, 32, bytesPerRow, aColorSpace,
                                       kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host,
                                       dataProvider, nullptr, true, kCGRenderingIntentDefault);
  ::CGDataProviderRelease(dataProvider);
  if (!cgImage) {
    surf->Unlock();
    return NS_ERROR_FAILURE;
  }
  CGImageRef subImage =
      ::CGImageCreateWithImageInRect(cgImage, ::CGRectMake(aX, aY, aWidth, aHeight));
  if (!subImage) {
    ::CGImageRelease(cgImage);
    surf->Unlock();
    return NS_ERROR_FAILURE;
  }

  ::CGContextScaleCTM(aContext, 1.0f, -1.0f);
  ::CGContextDrawImage(aContext, CGRectMake(aX, -(CGFloat)aY - (CGFloat)aHeight, aWidth, aHeight),
                       subImage);

  ::CGImageRelease(subImage);
  ::CGImageRelease(cgImage);
  surf->Unlock();
  return NS_OK;
}

void nsCARenderer::DetachCALayer() {
  CALayer* wrapperLayer = (CALayer*)mWrapperCALayer;
  NSArray* sublayers = [wrapperLayer sublayers];
  CALayer* oldLayer = (CALayer*)[sublayers objectAtIndex:0];
  [oldLayer removeFromSuperlayer];
}

void nsCARenderer::AttachCALayer(void* aCALayer) {
  CALayer* wrapperLayer = (CALayer*)mWrapperCALayer;
  NSArray* sublayers = [wrapperLayer sublayers];
  CALayer* oldLayer = (CALayer*)[sublayers objectAtIndex:0];
  [oldLayer removeFromSuperlayer];
  [wrapperLayer addSublayer:(CALayer*)aCALayer];
}

#ifdef DEBUG

int sSaveToDiskSequence = 0;
void nsCARenderer::SaveToDisk(MacIOSurface* surf) {
  surf->Lock();
  size_t bytesPerRow = surf->GetBytesPerRow();
  size_t ioWidth = surf->GetWidth();
  size_t ioHeight = surf->GetHeight();
  void* ioData = surf->GetBaseAddress();
  CGDataProviderRef dataProvider =
      ::CGDataProviderCreateWithData(ioData, ioData, ioHeight * (bytesPerRow)*4,
                                     nullptr);  // No release callback
  if (!dataProvider) {
    surf->Unlock();
    return;
  }

  CGColorSpaceRef colorSpace = CreateSystemColorSpace();
  CGImageRef cgImage = ::CGImageCreate(ioWidth, ioHeight, 8, 32, bytesPerRow, colorSpace,
                                       kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host,
                                       dataProvider, nullptr, true, kCGRenderingIntentDefault);
  ::CGDataProviderRelease(dataProvider);
  ::CGColorSpaceRelease(colorSpace);
  if (!cgImage) {
    surf->Unlock();
    return;
  }

  char cstr[1000];
  SprintfLiteral(cstr, "file:///Users/benoitgirard/debug/iosurface_%i.png", ++sSaveToDiskSequence);

  CFStringRef cfStr =
      ::CFStringCreateWithCString(kCFAllocatorDefault, cstr, kCFStringEncodingMacRoman);

  printf("Exporting: %s\n", cstr);
  CFURLRef url = ::CFURLCreateWithString(nullptr, cfStr, nullptr);
  ::CFRelease(cfStr);

  CFStringRef type = kUTTypePNG;
  size_t count = 1;
  CFDictionaryRef options = nullptr;
  CGImageDestinationRef dest = ::CGImageDestinationCreateWithURL(url, type, count, options);
  ::CFRelease(url);

  ::CGImageDestinationAddImage(dest, cgImage, nullptr);

  ::CGImageDestinationFinalize(dest);
  ::CFRelease(dest);
  ::CGImageRelease(cgImage);

  surf->Unlock();

  return;
}

#endif
