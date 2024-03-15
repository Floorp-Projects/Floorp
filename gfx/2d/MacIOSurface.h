/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MacIOSurface_h__
#define MacIOSurface_h__
#ifdef XP_DARWIN
#  include <CoreVideo/CoreVideo.h>
#  include <IOSurface/IOSurfaceRef.h>
#  include <QuartzCore/QuartzCore.h>
#  include <dlfcn.h>

#  include "mozilla/gfx/Types.h"
#  include "CFTypeRefPtr.h"

namespace mozilla {
namespace gl {
class GLContext;
}
}  // namespace mozilla

#  ifdef XP_MACOSX
struct _CGLContextObject;

typedef _CGLContextObject* CGLContextObj;
#  endif
typedef uint32_t IOSurfaceID;

#  ifdef XP_MACOSX
#    import <OpenGL/OpenGL.h>
#  else
#    include "GLTypes.h"
typedef realGLboolean GLboolean;
#    include <OpenGLES/ES2/gl.h>
#  endif

#  include "2D.h"
#  include "mozilla/RefCounted.h"
#  include "mozilla/RefPtr.h"

class MacIOSurface final
    : public mozilla::external::AtomicRefCounted<MacIOSurface> {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(MacIOSurface)
  typedef mozilla::gfx::SourceSurface SourceSurface;
  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef mozilla::gfx::BackendType BackendType;
  typedef mozilla::gfx::IntSize IntSize;
  typedef mozilla::gfx::YUVColorSpace YUVColorSpace;
  typedef mozilla::gfx::ColorSpace2 ColorSpace2;
  typedef mozilla::gfx::TransferFunction TransferFunction;
  typedef mozilla::gfx::ColorRange ColorRange;
  typedef mozilla::gfx::ColorDepth ColorDepth;

  // The usage count of the IOSurface is increased by 1 during the lifetime
  // of the MacIOSurface instance.
  // MacIOSurface holds a reference to the corresponding IOSurface.

  static already_AddRefed<MacIOSurface> CreateIOSurface(int aWidth, int aHeight,
                                                        bool aHasAlpha = true);
  static already_AddRefed<MacIOSurface> CreateNV12OrP010Surface(
      const IntSize& aYSize, const IntSize& aCbCrSize,
      YUVColorSpace aColorSpace, TransferFunction aTransferFunction,
      ColorRange aColorRange, ColorDepth aColorDepth);
  static already_AddRefed<MacIOSurface> CreateYUV422Surface(
      const IntSize& aSize, YUVColorSpace aColorSpace, ColorRange aColorRange);
  static void ReleaseIOSurface(MacIOSurface* aIOSurface);
  static already_AddRefed<MacIOSurface> LookupSurface(
      IOSurfaceID aSurfaceID, bool aHasAlpha = true,
      mozilla::gfx::YUVColorSpace aColorSpace =
          mozilla::gfx::YUVColorSpace::Identity);

  explicit MacIOSurface(CFTypeRefPtr<IOSurfaceRef> aIOSurfaceRef,
                        bool aHasAlpha = true,
                        mozilla::gfx::YUVColorSpace aColorSpace =
                            mozilla::gfx::YUVColorSpace::Identity);

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
  IntSize GetSize(size_t plane = 0) const {
    return IntSize(GetWidth(plane), GetHeight(plane));
  }
  size_t GetDevicePixelWidth(size_t plane = 0) const;
  size_t GetDevicePixelHeight(size_t plane = 0) const;
  size_t GetBytesPerRow(size_t plane = 0) const;
  size_t GetAllocSize() const;
  void Lock(bool aReadOnly = true);
  void Unlock(bool aReadOnly = true);
  bool IsLocked() const { return mIsLocked; }
  void IncrementUseCount();
  void DecrementUseCount();
  bool HasAlpha() const { return mHasAlpha; }
  mozilla::gfx::SurfaceFormat GetFormat() const;
  mozilla::gfx::SurfaceFormat GetReadFormat() const;
  mozilla::gfx::ColorDepth GetColorDepth() const;
  // This would be better suited on MacIOSurfaceImage type, however due to the
  // current data structure, this is not possible as only the IOSurfaceRef is
  // being used across.
  void SetYUVColorSpace(YUVColorSpace aColorSpace) {
    mColorSpace = aColorSpace;
  }
  YUVColorSpace GetYUVColorSpace() const { return mColorSpace; }
  bool IsFullRange() const {
    OSType format = GetPixelFormat();
    return (format == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange ||
            format == kCVPixelFormatType_420YpCbCr10BiPlanarFullRange);
  }
  mozilla::gfx::ColorRange GetColorRange() const {
    if (IsFullRange()) return mozilla::gfx::ColorRange::FULL;
    return mozilla::gfx::ColorRange::LIMITED;
  }

  // Bind this IOSurface to a texture using the most efficient mechanism
  // available on the current platform.
  //
  // Note that on iOS simulator, due to incomplete support for
  // texImageIOSurface, this will only use texImage2D to upload, and cannot be
  // used to read-back the GL texture to an IOSurface.
  bool BindTexImage(mozilla::gl::GLContext* aGL, size_t aPlane,
                    mozilla::gfx::SurfaceFormat* aOutReadFormat = nullptr);

  already_AddRefed<SourceSurface> GetAsSurface();

  // Creates a DrawTarget that wraps the data in the IOSurface. Rendering to
  // this DrawTarget directly manipulates the contents of the IOSurface.
  // Only call when the surface is already locked for writing!
  // The returned DrawTarget must only be used while the surface is still
  // locked.
  // Also, only call this if you're reasonably sure that the DrawTarget of the
  // selected backend supports the IOSurface's SurfaceFormat.
  already_AddRefed<DrawTarget> GetAsDrawTargetLocked(BackendType aBackendType);

  static size_t GetMaxWidth();
  static size_t GetMaxHeight();
  CFTypeRefPtr<IOSurfaceRef> GetIOSurfaceRef() { return mIOSurfaceRef; }

  void SetColorSpace(mozilla::gfx::ColorSpace2) const;

  ColorSpace2 mColorPrimaries = ColorSpace2::UNKNOWN;

 private:
  CFTypeRefPtr<IOSurfaceRef> mIOSurfaceRef;
  const bool mHasAlpha;
  YUVColorSpace mColorSpace = YUVColorSpace::Identity;
  bool mIsLocked = false;
};

#endif
#endif
