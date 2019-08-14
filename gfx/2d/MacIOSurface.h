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
typedef IOSurfacePtr (*IOSurfaceCreateFunc)(CFDictionaryRef properties);
typedef IOSurfacePtr (*IOSurfaceLookupFunc)(uint32_t io_surface_id);
typedef IOSurfaceID (*IOSurfaceGetIDFunc)(IOSurfacePtr io_surface);
typedef void (*IOSurfaceVoidFunc)(IOSurfacePtr io_surface);
typedef IOReturn (*IOSurfaceLockFunc)(IOSurfacePtr io_surface, uint32_t options,
                                      uint32_t* seed);
typedef IOReturn (*IOSurfaceUnlockFunc)(IOSurfacePtr io_surface,
                                        uint32_t options, uint32_t* seed);
typedef void* (*IOSurfaceGetBaseAddressFunc)(IOSurfacePtr io_surface);
typedef void* (*IOSurfaceGetBaseAddressOfPlaneFunc)(IOSurfacePtr io_surface,
                                                    size_t planeIndex);
typedef size_t (*IOSurfaceSizeTFunc)(IOSurfacePtr io_surface);
typedef size_t (*IOSurfaceSizePlaneTFunc)(IOSurfacePtr io_surface,
                                          size_t plane);
typedef size_t (*IOSurfaceGetPropertyMaximumFunc)(CFStringRef property);
typedef IOSurfacePtr (*CVPixelBufferGetIOSurfaceFunc)(
    CVPixelBufferRef pixelBuffer);

typedef OSType (*IOSurfacePixelFormatFunc)(IOSurfacePtr io_surface);

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

class MacIOSurfaceLib {
 public:
  MacIOSurfaceLib() = delete;
  static void* sIOSurfaceFramework;
  static void* sCoreVideoFramework;
  static bool isLoaded;
  static IOSurfaceCreateFunc sCreate;
  static IOSurfaceGetIDFunc sGetID;
  static IOSurfaceLookupFunc sLookup;
  static IOSurfaceGetBaseAddressFunc sGetBaseAddress;
  static IOSurfaceGetBaseAddressOfPlaneFunc sGetBaseAddressOfPlane;
  static IOSurfaceSizeTFunc sPlaneCount;
  static IOSurfaceLockFunc sLock;
  static IOSurfaceUnlockFunc sUnlock;
  static IOSurfaceVoidFunc sIncrementUseCount;
  static IOSurfaceVoidFunc sDecrementUseCount;
  static IOSurfaceSizePlaneTFunc sWidth;
  static IOSurfaceSizePlaneTFunc sHeight;
  static IOSurfaceSizePlaneTFunc sBytesPerRow;
  static IOSurfaceGetPropertyMaximumFunc sGetPropertyMaximum;
  static CVPixelBufferGetIOSurfaceFunc sCVPixelBufferGetIOSurface;
  static IOSurfacePixelFormatFunc sPixelFormat;
  static CFStringRef kPropWidth;
  static CFStringRef kPropHeight;
  static CFStringRef kPropBytesPerElem;
  static CFStringRef kPropBytesPerRow;
  static CFStringRef kPropIsGlobal;

  static bool isInit();
  static CFStringRef GetIOConst(const char* symbole);
  static IOSurfacePtr IOSurfaceCreate(CFDictionaryRef properties);
  static IOSurfacePtr IOSurfaceLookup(IOSurfaceID aIOSurfaceID);
  static IOSurfaceID IOSurfaceGetID(IOSurfacePtr aIOSurfacePtr);
  static void* IOSurfaceGetBaseAddress(IOSurfacePtr aIOSurfacePtr);
  static void* IOSurfaceGetBaseAddressOfPlane(IOSurfacePtr aIOSurfacePtr,
                                              size_t aPlaneIndex);
  static size_t IOSurfaceGetPlaneCount(IOSurfacePtr aIOSurfacePtr);
  static size_t IOSurfaceGetWidth(IOSurfacePtr aIOSurfacePtr, size_t plane);
  static size_t IOSurfaceGetHeight(IOSurfacePtr aIOSurfacePtr, size_t plane);
  static size_t IOSurfaceGetBytesPerRow(IOSurfacePtr aIOSurfacePtr,
                                        size_t plane);
  static size_t IOSurfaceGetPropertyMaximum(CFStringRef property);
  static IOReturn IOSurfaceLock(IOSurfacePtr aIOSurfacePtr, uint32_t options,
                                uint32_t* seed);
  static IOReturn IOSurfaceUnlock(IOSurfacePtr aIOSurfacePtr, uint32_t options,
                                  uint32_t* seed);
  static void IOSurfaceIncrementUseCount(IOSurfacePtr aIOSurfacePtr);
  static void IOSurfaceDecrementUseCount(IOSurfacePtr aIOSurfacePtr);
  static IOSurfacePtr CVPixelBufferGetIOSurface(CVPixelBufferRef apixelBuffer);
  static OSType IOSurfaceGetPixelFormat(IOSurfacePtr aIOSurfacePtr);
  static void LoadLibrary();
  static void CloseLibrary();

  // Static deconstructor
  static class LibraryUnloader {
   public:
    ~LibraryUnloader() { CloseLibrary(); }
  } sLibraryUnloader;
};

#endif
#endif
