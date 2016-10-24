/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SKIA_GL_GLUE_H_
#define SKIA_GL_GLUE_H_

#ifdef USE_SKIA_GPU

#include "mozilla/gfx/RefPtrSkia.h"
#include "mozilla/RefPtr.h"

struct GrGLInterface;
class GrContext;

namespace mozilla {
namespace gl {

class GLContext;

class SkiaGLGlue : public GenericAtomicRefCounted
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(SkiaGLGlue)
  explicit SkiaGLGlue(GLContext* context);
  GLContext* GetGLContext() const { return mGLContext.get(); }
  GrContext* GetGrContext() const { return mGrContext.get(); }

protected:
  virtual ~SkiaGLGlue();

private:
  RefPtr<GLContext> mGLContext;
  gfx::RefPtrSkia<GrGLInterface> mGrGLInterface;
  gfx::RefPtrSkia<GrContext> mGrContext;
};

} // namespace gl
} // namespace mozilla

#else // USE_SKIA_GPU

class GrContext;

namespace mozilla {
namespace gl {

class GLContext;

class SkiaGLGlue : public GenericAtomicRefCounted
{
public:
  SkiaGLGlue(GLContext* context);
  GLContext* GetGLContext() const { return nullptr; }
  GrContext* GetGrContext() const { return nullptr; }
};

} // namespace gl
} // namespace mozilla

#endif // USE_SKIA_GPU

#endif // SKIA_GL_GLUE_H_
