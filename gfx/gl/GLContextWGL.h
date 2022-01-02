/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLCONTEXTWGL_H_
#define GLCONTEXTWGL_H_

#include "GLContext.h"
#include "WGLLibrary.h"

namespace mozilla {
namespace gl {

class GLContextWGL final : public GLContext {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(GLContextWGL, override)
  // From Window: (possibly for offscreen!)
  GLContextWGL(const GLContextDesc&, HDC aDC, HGLRC aContext,
               HWND aWindow = nullptr);

  // From PBuffer
  GLContextWGL(const GLContextDesc&, HANDLE aPbuffer, HDC aDC, HGLRC aContext,
               int aPixelFormat);

  ~GLContextWGL();

  virtual GLContextType GetContextType() const override {
    return GLContextType::WGL;
  }

  virtual bool MakeCurrentImpl() const override;
  virtual bool IsCurrentImpl() const override;
  virtual bool IsDoubleBuffered() const override { return mIsDoubleBuffered; }
  virtual bool SwapBuffers() override;
  virtual void GetWSIInfo(nsCString* const out) const override;

  Maybe<SymbolLoader> GetSymbolLoader() const override {
    return Some(sWGLLib.GetSymbolLoader());
  }

  HGLRC Context() { return mContext; }

 protected:
  friend class GLContextProviderWGL;

  HDC mDC;
  HGLRC mContext;
  HWND mWnd;
  HANDLE mPBuffer;
  int mPixelFormat;

 public:
  bool mIsDoubleBuffered = false;
};

}  // namespace gl
}  // namespace mozilla

#endif  // GLCONTEXTWGL_H_
