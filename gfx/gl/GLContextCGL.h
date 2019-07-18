/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLCONTEXTCGL_H_
#define GLCONTEXTCGL_H_

#include "GLContext.h"

#include "OpenGL/OpenGL.h"
#include <IOSurface/IOSurface.h>

#ifdef __OBJC__
#  include <AppKit/NSOpenGL.h>
#else
typedef void NSOpenGLContext;
#endif

#include <unordered_map>

#include "mozilla/HashFunctions.h"
#include "mozilla/UniquePtr.h"

class nsIWidget;

namespace mozilla {
namespace gl {

class MozFramebuffer;

class GLContextCGL : public GLContext {
  friend class GLContextProviderCGL;

  NSOpenGLContext* mContext;
  GLuint mDefaultFramebuffer = 0;

  struct IOSurfaceRefHasher {
    std::size_t operator()(const IOSurfaceRef& aSurface) const {
      return HashGeneric(reinterpret_cast<uintptr_t>(aSurface));
    }
  };

  std::unordered_map<IOSurfaceRef, UniquePtr<MozFramebuffer>,
                     IOSurfaceRefHasher>
      mRegisteredIOSurfaceFramebuffers;

 protected:
  virtual void OnMarkDestroyed() override;

 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(GLContextCGL, override)
  GLContextCGL(CreateContextFlags flags, const SurfaceCaps& caps,
               NSOpenGLContext* context, bool isOffscreen);

  ~GLContextCGL();

  virtual GLContextType GetContextType() const override {
    return GLContextType::CGL;
  }

  static GLContextCGL* Cast(GLContext* gl) {
    MOZ_ASSERT(gl->GetContextType() == GLContextType::CGL);
    return static_cast<GLContextCGL*>(gl);
  }

  NSOpenGLContext* GetNSOpenGLContext() const { return mContext; }
  CGLContextObj GetCGLContext() const;

  virtual bool MakeCurrentImpl() const override;

  virtual bool IsCurrentImpl() const override;

  virtual GLenum GetPreferredARGB32Format() const override;

  virtual bool IsDoubleBuffered() const override;

  virtual bool SwapBuffers() override;

  virtual void GetWSIInfo(nsCString* const out) const override;

  Maybe<SymbolLoader> GetSymbolLoader() const override;

  virtual GLuint GetDefaultFramebuffer() override;

  void RegisterIOSurface(IOSurfaceRef aSurface);
  void UnregisterIOSurface(IOSurfaceRef aSurface);
  void UseRegisteredIOSurfaceForDefaultFramebuffer(IOSurfaceRef aSurface);
};

}  // namespace gl
}  // namespace mozilla

#endif  // GLCONTEXTCGL_H_
