/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLCONTEXTEAGL_H_
#define GLCONTEXTEAGL_H_

#include "GLContext.h"

#include <CoreGraphics/CoreGraphics.h>
#include <OpenGLES/EAGL.h>

namespace mozilla {
namespace gl {

class GLContextEAGL : public GLContext {
  friend class GLContextProviderEAGL;

  EAGLContext* const mContext;

 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(GLContextEAGL, override)
  GLContextEAGL(const GLContextDesc&, EAGLContext* context,
                GLContext* sharedContext, ContextProfile profile);

  ~GLContextEAGL();

  virtual GLContextType GetContextType() const override {
    return GLContextType::EAGL;
  }

  static GLContextEAGL* Cast(GLContext* gl) {
    MOZ_ASSERT(gl->GetContextType() == GLContextType::EAGL);
    return static_cast<GLContextEAGL*>(gl);
  }

  bool AttachToWindow(nsIWidget* aWidget);

  EAGLContext* GetEAGLContext() const { return mContext; }

  virtual bool MakeCurrentImpl() const override;

  virtual bool IsCurrentImpl() const override;

  Maybe<SymbolLoader> GetSymbolLoader() const override;

  virtual bool IsDoubleBuffered() const override;

  virtual bool SwapBuffers() override;

  virtual void GetWSIInfo(nsCString* const out) const override;

  virtual GLuint GetDefaultFramebuffer() override { return mBackbufferFB; }

  virtual bool RenewSurface(widget::CompositorWidget*) override {
    // FIXME: should use the passed widget instead of the existing one.
    return RecreateRB();
  }

 private:
  GLuint mBackbufferRB = 0;
  GLuint mBackbufferFB = 0;

  void* mLayer = nullptr;

  bool RecreateRB();
};

}  // namespace gl
}  // namespace mozilla

#endif  // GLCONTEXTEAGL_H_
