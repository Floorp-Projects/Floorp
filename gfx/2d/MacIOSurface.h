/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MacIOSurface_h__
#define MacIOSurface_h__
#ifdef XP_MACOSX
#include <QuartzCore/QuartzCore.h>
#include <dlfcn.h>

typedef CFTypeRef IOSurfacePtr;
typedef IOSurfacePtr (*IOSurfaceCreateFunc) (CFDictionaryRef properties);
typedef IOSurfacePtr (*IOSurfaceLookupFunc) (uint32_t io_surface_id);
typedef IOSurfaceID (*IOSurfaceGetIDFunc)(IOSurfacePtr io_surface);
typedef void (*IOSurfaceVoidFunc)(IOSurfacePtr io_surface);
typedef IOReturn (*IOSurfaceLockFunc)(IOSurfacePtr io_surface, uint32_t options,
                                      uint32_t *seed);
typedef IOReturn (*IOSurfaceUnlockFunc)(IOSurfacePtr io_surface,
                                        uint32_t options, uint32_t *seed);
typedef void* (*IOSurfaceGetBaseAddressFunc)(IOSurfacePtr io_surface);
typedef void* (*IOSurfaceGetBaseAddressOfPlaneFunc)(IOSurfacePtr io_surface,
                                                    size_t planeIndex);
typedef size_t (*IOSurfaceSizeTFunc)(IOSurfacePtr io_surface);
typedef size_t (*IOSurfaceSizePlaneTFunc)(IOSurfacePtr io_surface, size_t plane);
typedef size_t (*IOSurfaceGetPropertyMaximumFunc) (CFStringRef property);
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

typedef IOSurfacePtr (*CVPixelBufferGetIOSurfaceFunc)(
  CVPixelBufferRef pixelBuffer);

typedef OSType (*IOSurfacePixelFormatFunc)(IOSurfacePtr io_surface);

#import <OpenGL/OpenGL.h>
#include "2D.h"
#include "mozilla/RefPtr.h"

struct _CGLContextObject;

typedef _CGLContextObject* CGLContextObj;
typedef struct CGContext* CGContextRef;
typedef struct CGImage* CGImageRef;
typedef uint32_t IOSurfaceID;

enum CGContextType {
  CG_CONTEXT_TYPE_UNKNOWN = 0,
  // These are found by inspection, it's possible they could be changed
  CG_CONTEXT_TYPE_BITMAP = 4,
  CG_CONTEXT_TYPE_IOSURFACE = 8
};

CGContextType GetContextType(CGContextRef ref);

class MacIOSurface : public mozilla::external::AtomicRefCounted<MacIOSurface> {
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(MacIOSurface)
  typedef mozilla::gfx::SourceSurface SourceSurface;

  // The usage count of the IOSurface is increased by 1 during the lifetime
  // of the MacIOSurface instance.
  // MacIOSurface holds a reference to the corresponding IOSurface.

  static already_AddRefed<MacIOSurface> CreateIOSurface(int aWidth, int aHeight,
                                                             double aContentsScaleFactor = 1.0,
                                                             bool aHasAlpha = true);
  static void ReleaseIOSurface(MacIOSurface *aIOSurface);
  static already_AddRefed<MacIOSurface> LookupSurface(IOSurfaceID aSurfaceID,
                                                           double aContentsScaleFactor = 1.0,
                                                           bool aHasAlpha = true);

  explicit MacIOSurface(const void *aIOSurfacePtr,
                        double aContentsScaleFactor = 1.0,
                        bool aHasAlpha = true);
  virtual ~MacIOSurface();
  IOSurfaceID GetIOSurfaceID();
  void *GetBaseAddress();
  void *GetBaseAddressOfPlane(size_t planeIndex);
  size_t GetPlaneCount();
  OSType GetPixelFormat();
  // GetWidth() and GetHeight() return values in "display pixels".  A
  // "display pixel" is the smallest fully addressable part of a display.
  // But in HiDPI modes each "display pixel" corresponds to more than one
  // device pixel.  Use GetDevicePixel**() to get device pixels.
  size_t GetWidth(size_t plane = 0);
  size_t GetHeight(size_t plane = 0);
  double GetContentsScaleFactor() { return mContentsScaleFactor; }
  size_t GetDevicePixelWidth(size_t plane = 0);
  size_t GetDevicePixelHeight(size_t plane = 0);
  size_t GetBytesPerRow(size_t plane = 0);
  void Lock();
  void Unlock();
  void IncrementUseCount();
  void DecrementUseCount();
  bool HasAlpha() { return mHasAlpha; }
  mozilla::gfx::SurfaceFormat GetFormat();
  // We would like to forward declare NSOpenGLContext, but it is an @interface
  // and this file is also used from c++, so we use a void *.
  CGLError CGLTexImageIOSurface2D(CGLContextObj ctxt, size_t plane = 0);
  already_AddRefed<SourceSurface> GetAsSurface();
  CGContextRef CreateIOSurfaceContext();

  // FIXME This doesn't really belong here
  static CGImageRef CreateImageFromIOSurfaceContext(CGContextRef aContext);
  static already_AddRefed<MacIOSurface> IOSurfaceContextGetSurface(CGContextRef aContext,
                                                                        double aContentsScaleFactor = 1.0,
                                                                        bool aHasAlpha = true);
  static size_t GetMaxWidth();
  static size_t GetMaxHeight();

private:
  friend class nsCARenderer;
  const void* mIOSurfacePtr;
  double mContentsScaleFactor;
  bool mHasAlpha;
};

class MacIOSurfaceLib: public MacIOSurface {
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(MacIOSurfaceLib)
  static void                        *sIOSurfaceFramework;
  static void                        *sOpenGLFramework;
  static void                        *sCoreGraphicsFramework;
  static void                        *sCoreVideoFramework;
  static bool                         isLoaded;
  static IOSurfaceCreateFunc          sCreate;
  static IOSurfaceGetIDFunc           sGetID;
  static IOSurfaceLookupFunc          sLookup;
  static IOSurfaceGetBaseAddressFunc  sGetBaseAddress;
  static IOSurfaceGetBaseAddressOfPlaneFunc sGetBaseAddressOfPlane;
  static IOSurfaceSizeTFunc           sPlaneCount;
  static IOSurfaceLockFunc            sLock;
  static IOSurfaceUnlockFunc          sUnlock;
  static IOSurfaceVoidFunc            sIncrementUseCount;
  static IOSurfaceVoidFunc            sDecrementUseCount;
  static IOSurfaceSizePlaneTFunc      sWidth;
  static IOSurfaceSizePlaneTFunc      sHeight;
  static IOSurfaceSizePlaneTFunc      sBytesPerRow;
  static IOSurfaceGetPropertyMaximumFunc  sGetPropertyMaximum;
  static CGLTexImageIOSurface2DFunc   sTexImage;
  static IOSurfaceContextCreateFunc   sIOSurfaceContextCreate;
  static IOSurfaceContextCreateImageFunc  sIOSurfaceContextCreateImage;
  static IOSurfaceContextGetSurfaceFunc   sIOSurfaceContextGetSurface;
  static CVPixelBufferGetIOSurfaceFunc    sCVPixelBufferGetIOSurface;
  static IOSurfacePixelFormatFunc     sPixelFormat;
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
  static void*        IOSurfaceGetBaseAddress(IOSurfacePtr aIOSurfacePtr);
  static void*        IOSurfaceGetBaseAddressOfPlane(IOSurfacePtr aIOSurfacePtr,
                                                     size_t aPlaneIndex);
  static size_t       IOSurfaceGetPlaneCount(IOSurfacePtr aIOSurfacePtr);
  static size_t       IOSurfaceGetWidth(IOSurfacePtr aIOSurfacePtr, size_t plane);
  static size_t       IOSurfaceGetHeight(IOSurfacePtr aIOSurfacePtr, size_t plane);
  static size_t       IOSurfaceGetBytesPerRow(IOSurfacePtr aIOSurfacePtr, size_t plane);
  static size_t       IOSurfaceGetPropertyMaximum(CFStringRef property);
  static IOReturn     IOSurfaceLock(IOSurfacePtr aIOSurfacePtr,
                                    uint32_t options, uint32_t *seed);
  static IOReturn     IOSurfaceUnlock(IOSurfacePtr aIOSurfacePtr,
                                      uint32_t options, uint32_t *seed);
  static void         IOSurfaceIncrementUseCount(IOSurfacePtr aIOSurfacePtr);
  static void         IOSurfaceDecrementUseCount(IOSurfacePtr aIOSurfacePtr);
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
  static IOSurfacePtr CVPixelBufferGetIOSurface(CVPixelBufferRef apixelBuffer);
  static OSType       IOSurfaceGetPixelFormat(IOSurfacePtr aIOSurfacePtr);
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

#endif
#endif
