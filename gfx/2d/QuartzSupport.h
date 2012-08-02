/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCoreAnimationSupport_h__
#define nsCoreAnimationSupport_h__
#ifdef XP_MACOSX

#import <OpenGL/OpenGL.h>
#import "ApplicationServices/ApplicationServices.h"
#include "gfxTypes.h"
#include "mozilla/RefPtr.h"
#include "mozilla/gfx/MacIOSurface.h"

// Get the system color space.
CGColorSpaceRef THEBES_API CreateSystemColorSpace();

// Manages a CARenderer
struct _CGLPBufferObject;
struct _CGLContextObject;

enum AllowOfflineRendererEnum { ALLOW_OFFLINE_RENDERER, DISALLOW_OFFLINE_RENDERER };

class nsCARenderer : public mozilla::RefCounted<nsCARenderer> {
public:
  nsCARenderer() : mCARenderer(nsnull), mFBOTexture(0), mOpenGLContext(nsnull),
                   mCGImage(nsnull), mCGData(nsnull), mIOSurface(nsnull), mFBO(0),
                   mIOTexture(0),
                   mUnsupportedWidth(UINT32_MAX), mUnsupportedHeight(UINT32_MAX),
                   mAllowOfflineRenderer(DISALLOW_OFFLINE_RENDERER) {}
  ~nsCARenderer();
  nsresult SetupRenderer(void* aCALayer, int aWidth, int aHeight,
                         AllowOfflineRendererEnum aAllowOfflineRenderer);
  nsresult Render(int aWidth, int aHeight, CGImageRef *aOutCAImage);
  bool isInit() { return mCARenderer != nsnull; }
  /*
   * Render the CALayer to an IOSurface. If no IOSurface
   * is attached then an internal pixel buffer will be
   * used.
   */
  void AttachIOSurface(mozilla::RefPtr<MacIOSurface> aSurface);
  IOSurfaceID GetIOSurfaceID();
  static nsresult DrawSurfaceToCGContext(CGContextRef aContext,
                                         MacIOSurface *surf,
                                         CGColorSpaceRef aColorSpace,
                                         int aX, int aY,
                                         size_t aWidth, size_t aHeight);

  // Remove & Add the layer without destroying
  // the renderer for fast back buffer swapping.
  void DettachCALayer();
  void AttachCALayer(void *aCALayer);
#ifdef DEBUG
  static void SaveToDisk(MacIOSurface *surf);
#endif
private:
  void SetBounds(int aWidth, int aHeight);
  void SetViewport(int aWidth, int aHeight);
  void Destroy();

  void *mCARenderer;
  GLuint                    mFBOTexture;
  _CGLContextObject        *mOpenGLContext;
  CGImageRef                mCGImage;
  void                     *mCGData;
  mozilla::RefPtr<MacIOSurface> mIOSurface;
  uint32_t                  mFBO;
  uint32_t                  mIOTexture;
  uint32_t                  mUnsupportedWidth;
  uint32_t                  mUnsupportedHeight;
  AllowOfflineRendererEnum  mAllowOfflineRenderer;
};

enum CGContextType {
  CG_CONTEXT_TYPE_UNKNOWN = 0,
  // These are found by inspection, it's possible they could be changed
  CG_CONTEXT_TYPE_BITMAP = 4,
  CG_CONTEXT_TYPE_IOSURFACE = 8
};

CGContextType GetContextType(CGContextRef ref);

#endif // XP_MACOSX
#endif // nsCoreAnimationSupport_h__

