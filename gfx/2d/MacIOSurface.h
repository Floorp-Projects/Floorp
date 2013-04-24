/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MacIOSurface_h__
#define MacIOSurface_h__
#ifdef XP_MACOSX

#import <OpenGL/OpenGL.h>
#include "2D.h"
#include "mozilla/RefPtr.h"

class gfxASurface;
struct _CGLContextObject;

typedef _CGLContextObject* CGLContextObj;
typedef struct CGContext* CGContextRef;
typedef struct CGImage* CGImageRef;
typedef uint32_t IOSurfaceID;

class MacIOSurface : public mozilla::RefCounted<MacIOSurface> {
public:
  typedef mozilla::gfx::SourceSurface SourceSurface;

  static mozilla::TemporaryRef<MacIOSurface> CreateIOSurface(int aWidth, int aHeight,
                                                             double aContentsScaleFactor = 1.0);
  static void ReleaseIOSurface(MacIOSurface *aIOSurface);
  static mozilla::TemporaryRef<MacIOSurface> LookupSurface(IOSurfaceID aSurfaceID,
                                                           double aContentsScaleFactor = 1.0);

  MacIOSurface(const void *aIOSurfacePtr, double aContentsScaleFactor = 1.0)
    : mIOSurfacePtr(aIOSurfacePtr), mContentsScaleFactor(aContentsScaleFactor) {}
  ~MacIOSurface();
  IOSurfaceID GetIOSurfaceID();
  void *GetBaseAddress();
  // GetWidth() and GetHeight() return values in "display pixels".  A
  // "display pixel" is the smallest fully addressable part of a display.
  // But in HiDPI modes each "display pixel" corresponds to more than one
  // device pixel.  Use GetDevicePixel**() to get device pixels.
  size_t GetWidth();
  size_t GetHeight();
  double GetContentsScaleFactor() { return mContentsScaleFactor; }
  size_t GetDevicePixelWidth();
  size_t GetDevicePixelHeight();
  size_t GetBytesPerRow();
  void Lock();
  void Unlock();
  // We would like to forward declare NSOpenGLContext, but it is an @interface
  // and this file is also used from c++, so we use a void *.
  CGLError CGLTexImageIOSurface2D(void *ctxt,
                                  GLenum internalFormat, GLenum format,
                                  GLenum type, GLuint plane);
  mozilla::TemporaryRef<SourceSurface> GetAsSurface();
  CGContextRef CreateIOSurfaceContext();

  // FIXME This doesn't really belong here
  static CGImageRef CreateImageFromIOSurfaceContext(CGContextRef aContext);
  static mozilla::TemporaryRef<MacIOSurface> IOSurfaceContextGetSurface(CGContextRef aContext,
                                                                        double aContentsScaleFactor = 1.0);

private:
  friend class nsCARenderer;
  const void* mIOSurfacePtr;
  double mContentsScaleFactor;
};

#endif
#endif
