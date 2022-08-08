/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLCONTEXTGLX_H_
#define GLCONTEXTGLX_H_

#include "GLContext.h"
#include "GLXLibrary.h"
#include "mozilla/X11Util.h"

namespace mozilla {
namespace gl {

class GLContextGLX : public GLContext {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(GLContextGLX, override)
  static already_AddRefed<GLContextGLX> CreateGLContext(
      const GLContextDesc&, std::shared_ptr<gfx::XlibDisplay> display,
      GLXDrawable drawable, GLXFBConfig cfg, Drawable ownedPixmap = X11None);

  static bool FindVisual(Display* display, int screen, int* const out_visualId);

  // Finds a GLXFBConfig compatible with the provided window.
  static bool FindFBConfigForWindow(
      Display* display, int screen, Window window,
      ScopedXFree<GLXFBConfig>* const out_scopedConfigArr,
      GLXFBConfig* const out_config, int* const out_visid, bool aWebRender);

  virtual ~GLContextGLX();

  GLContextType GetContextType() const override { return GLContextType::GLX; }

  static GLContextGLX* Cast(GLContext* gl) {
    MOZ_ASSERT(gl->GetContextType() == GLContextType::GLX);
    return static_cast<GLContextGLX*>(gl);
  }

  bool Init() override;

  bool MakeCurrentImpl() const override;

  bool IsCurrentImpl() const override;

  Maybe<SymbolLoader> GetSymbolLoader() const override;

  bool IsDoubleBuffered() const override;

  bool SwapBuffers() override;

  GLint GetBufferAge() const override;

  void GetWSIInfo(nsCString* const out) const override;

  // Overrides the current GLXDrawable backing the context and makes the
  // context current.
  bool OverrideDrawable(GLXDrawable drawable);

  // Undoes the effect of a drawable override.
  bool RestoreDrawable();

 private:
  friend class GLContextProviderGLX;

  GLContextGLX(const GLContextDesc&, std::shared_ptr<gfx::XlibDisplay> aDisplay,
               GLXDrawable aDrawable, GLXContext aContext, bool aDoubleBuffered,
               Drawable aOwnedPixmap = X11None);

  const GLXContext mContext;
  const std::shared_ptr<gfx::XlibDisplay> mDisplay;
  const GLXDrawable mDrawable;
  // The X pixmap associated with the GLX pixmap. If this is provided, then this
  // class assumes responsibility for freeing both. Otherwise, the user of this
  // class is responsibility for freeing the drawables.
  const Drawable mOwnedPixmap;
  const bool mDoubleBuffered;

  GLXLibrary* const mGLX;

  const bool mOwnsContext = true;
};

}  // namespace gl
}  // namespace mozilla

#endif  // GLCONTEXTGLX_H_
