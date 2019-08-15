/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MacIOSurface_h__
#define MacIOSurface_h__
#ifdef XP_DARWIN
#  include <CoreVideo/CoreVideo.h>
#  include <QuartzCore/QuartzCore.h>
#  include <dlfcn.h>

#  include "mozilla/gfx/Types.h"

namespace mozilla {
namespace gl {
class GLContext;
}
}  // namespace mozilla

struct _CGLContextObject;

typedef _CGLContextObject* CGLContextObj;
typedef uint32_t IOSurfaceID;

#  ifdef XP_IOS
typedef kern_return_t IOReturn;
typedef int CGLError;
#  endif

typedef CFTypeRef IOSurfacePtr;

#  ifdef XP_MACOSX
#    import <OpenGL/OpenGL.h>
#  else
#    import <OpenGLES/ES2/gl.h>
#  endif

#  include "2D.h"
#  include "mozilla/RefCounted.h"
#  include "mozilla/RefPtr.h"

class MacIOSurface final
    : public mozilla::external::AtomicRefCounted<MacIOSurface> {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(MacIOSurface)
  typedef mozilla::gfx::SourceSurface SourceSurface;

  // The usage count of the IOSurface is increased by 1 during the lifetime
  // of the MacIOSurface instance.
  // MacIOSurface holds a reference to the corresponding IOSurface.

  static already_AddRefed<MacIOSurface> CreateIOSurface(
      int aWidth, int aHeight, double aContentsScaleFactor = 1.0,
      bool aHasAlpha = true);
  static void ReleaseIOSurface(MacIOSurface* aIOSurface);
  static already_AddRefed<MacIOSurface> LookupSurface(
      IOSurfaceID aSurfaceID, double aContentsScaleFactor = 1.0,
      bool aHasAlpha = true,
      mozilla::gfx::YUVColorSpace aColorSpace =
          mozilla::gfx::YUVColorSpace::UNKNOWN);

  explicit MacIOSurface(IOSurfacePtr aIOSurfacePtr,
                        double aContentsScaleFactor = 1.0,
                        bool aHasAlpha = true,
                        mozilla::gfx::YUVColorSpace aColorSpace =
                            mozilla::gfx::YUVColorSpace::UNKNOWN);
  ~MacIOSurface();
  IOSurfaceID GetIOSurfaceID() const;
  void* GetBaseAddress() const;
  void* GetBaseAddressOfPlane(size_t planeIndex) const;
  size_t GetPlaneCount() const;
  OSType GetPixelFormat() const;
  // GetWidth() and GetHeight() return values in "display pixels".  A
  // "display pixel" is the smallest fully addressable part of a display.
  // But in HiDPI modes each "display pixel" corresponds to more than one
  // device pixel.  Use GetDevicePixel**() to get device pixels.
  size_t GetWidth(size_t plane = 0) const;
  size_t GetHeight(size_t plane = 0) const;
  double GetContentsScaleFactor() const { return mContentsScaleFactor; }
  size_t GetDevicePixelWidth(size_t plane = 0) const;
  size_t GetDevicePixelHeight(size_t plane = 0) const;
  size_t GetBytesPerRow(size_t plane = 0) const;
  void Lock(bool aReadOnly = true);
  void Unlock(bool aReadOnly = true);
  void IncrementUseCount();
  void DecrementUseCount();
  bool HasAlpha() const { return mHasAlpha; }
  mozilla::gfx::SurfaceFormat GetFormat() const;
  mozilla::gfx::SurfaceFormat GetReadFormat() const;
  // This would be better suited on MacIOSurfaceImage type, however due to the
  // current data structure, this is not possible as only the IOSurfacePtr is
  // being used across.
  void SetYUVColorSpace(mozilla::gfx::YUVColorSpace aColorSpace) {
    mColorSpace = aColorSpace;
  }
  mozilla::gfx::YUVColorSpace GetYUVColorSpace() const { return mColorSpace; }
  bool IsFullRange() const {
    return GetPixelFormat() == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
  }

  // We would like to forward declare NSOpenGLContext, but it is an @interface
  // and this file is also used from c++, so we use a void *.
  CGLError CGLTexImageIOSurface2D(
      mozilla::gl::GLContext* aGL, CGLContextObj ctxt, size_t plane,
      mozilla::gfx::SurfaceFormat* aOutReadFormat = nullptr);
  CGLError CGLTexImageIOSurface2D(CGLContextObj ctxt, GLenum target,
                                  GLenum internalFormat, GLsizei width,
                                  GLsizei height, GLenum format, GLenum type,
                                  GLuint plane) const;
  already_AddRefed<SourceSurface> GetAsSurface();

  static size_t GetMaxWidth();
  static size_t GetMaxHeight();
  const void* GetIOSurfacePtr() { return mIOSurfacePtr; }

 private:
  friend class nsCARenderer;
  const IOSurfacePtr mIOSurfacePtr;
  double mContentsScaleFactor;
  bool mHasAlpha;
  mozilla::gfx::YUVColorSpace mColorSpace =
      mozilla::gfx::YUVColorSpace::UNKNOWN;
};

#endif
#endif
