/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIOSurface_h__
#define nsIOSurface_h__
#ifdef XP_MACOSX

#import <OpenGL/OpenGL.h>

class gfxASurface;
struct _CGLContextObject;

typedef _CGLContextObject* CGLContextObj;
typedef uint32_t IOSurfaceID;

class THEBES_API nsIOSurface {
    NS_INLINE_DECL_REFCOUNTING(nsIOSurface)
public:
  static already_AddRefed<nsIOSurface> CreateIOSurface(int aWidth, int aHeight);
  static void ReleaseIOSurface(nsIOSurface *aIOSurface);
  static already_AddRefed<nsIOSurface> LookupSurface(IOSurfaceID aSurfaceID);

  nsIOSurface(const void *aIOSurfacePtr) : mIOSurfacePtr(aIOSurfacePtr) {}
  ~nsIOSurface();
  IOSurfaceID GetIOSurfaceID();
  void *GetBaseAddress();
  size_t GetWidth();
  size_t GetHeight();
  size_t GetBytesPerRow();
  void Lock();
  void Unlock();
  // We would like to forward declare NSOpenGLContext, but it is an @interface
  // and this file is also used from c++, so we use a void *.
  CGLError CGLTexImageIOSurface2D(void *ctxt,
                                  GLenum internalFormat, GLenum format,
                                  GLenum type, GLuint plane);
  already_AddRefed<gfxASurface> GetAsSurface();
private:
  friend class nsCARenderer;
  const void* mIOSurfacePtr;
};

#endif
#endif
