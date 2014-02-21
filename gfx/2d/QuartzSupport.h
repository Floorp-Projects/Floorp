/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCoreAnimationSupport_h__
#define nsCoreAnimationSupport_h__
#ifdef XP_MACOSX

#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>
#import "ApplicationServices/ApplicationServices.h"
#include "gfxTypes.h"
#include "mozilla/RefPtr.h"
#include "mozilla/gfx/MacIOSurface.h"
#include "nsError.h"

// Get the system color space.
CGColorSpaceRef CreateSystemColorSpace();

// Manages a CARenderer
struct _CGLPBufferObject;
struct _CGLContextObject;

enum AllowOfflineRendererEnum { ALLOW_OFFLINE_RENDERER, DISALLOW_OFFLINE_RENDERER };

class nsCARenderer : public mozilla::RefCounted<nsCARenderer> {
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(nsCARenderer)
  nsCARenderer() : mCARenderer(nullptr), mWrapperCALayer(nullptr), mFBOTexture(0),
                   mOpenGLContext(nullptr), mCGImage(nullptr), mCGData(nullptr),
                   mIOSurface(nullptr), mFBO(0), mIOTexture(0),
                   mUnsupportedWidth(UINT32_MAX), mUnsupportedHeight(UINT32_MAX),
                   mAllowOfflineRenderer(DISALLOW_OFFLINE_RENDERER),
                   mContentsScaleFactor(1.0) {}
  ~nsCARenderer();
  // aWidth and aHeight are in "display pixels".  A "display pixel" is the
  // smallest fully addressable part of a display.  But in HiDPI modes each
  // "display pixel" corresponds to more than one device pixel.  Multiply
  // display pixels by aContentsScaleFactor to get device pixels.
  nsresult SetupRenderer(void* aCALayer, int aWidth, int aHeight,
                         double aContentsScaleFactor,
                         AllowOfflineRendererEnum aAllowOfflineRenderer);
  // aWidth and aHeight are in "display pixels".  Multiply by
  // aContentsScaleFactor to get device pixels.
  nsresult Render(int aWidth, int aHeight,
                  double aContentsScaleFactor,
                  CGImageRef *aOutCAImage);
  bool isInit() { return mCARenderer != nullptr; }
  /*
   * Render the CALayer to an IOSurface. If no IOSurface
   * is attached then an internal pixel buffer will be
   * used.
   */
  void AttachIOSurface(mozilla::RefPtr<MacIOSurface> aSurface);
  IOSurfaceID GetIOSurfaceID();
  // aX, aY, aWidth and aHeight are in "display pixels".  Multiply by
  // surf->GetContentsScaleFactor() to get device pixels.
  static nsresult DrawSurfaceToCGContext(CGContextRef aContext,
                                         MacIOSurface *surf,
                                         CGColorSpaceRef aColorSpace,
                                         int aX, int aY,
                                         size_t aWidth, size_t aHeight);

  // Remove & Add the layer without destroying
  // the renderer for fast back buffer swapping.
  void DetachCALayer();
  void AttachCALayer(void *aCALayer);
#ifdef DEBUG
  static void SaveToDisk(MacIOSurface *surf);
#endif
private:
  // aWidth and aHeight are in "display pixels".  Multiply by
  // mContentsScaleFactor to get device pixels.
  void SetBounds(int aWidth, int aHeight);
  // aWidth and aHeight are in "display pixels".  Multiply by
  // mContentsScaleFactor to get device pixels.
  void SetViewport(int aWidth, int aHeight);
  void Destroy();

  void *mCARenderer;
  void *mWrapperCALayer;
  GLuint                    mFBOTexture;
  _CGLContextObject        *mOpenGLContext;
  CGImageRef                mCGImage;
  void                     *mCGData;
  mozilla::RefPtr<MacIOSurface> mIOSurface;
  uint32_t                  mFBO;
  uint32_t                  mIOTexture;
  int                       mUnsupportedWidth;
  int                       mUnsupportedHeight;
  AllowOfflineRendererEnum  mAllowOfflineRenderer;
  double                    mContentsScaleFactor;
};

#endif // XP_MACOSX
#endif // nsCoreAnimationSupport_h__

