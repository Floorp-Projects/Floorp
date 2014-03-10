/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/RefPtr.h"

#ifdef USE_SKIA_GPU

#include "GLContext.h"
#include "skia/GrGLInterface.h"
#include "skia/GrContext.h"

namespace mozilla {
namespace gl {

class SkiaGLGlue : public GenericAtomicRefCounted
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(SkiaGLGlue)
  SkiaGLGlue(GLContext* context);
  GLContext* GetGLContext() const { return mGLContext.get(); }
  GrContext* GetGrContext() const { return mGrContext.get(); }

protected:
  virtual ~SkiaGLGlue() {
    /*
     * These members have inter-dependencies, but do not keep each other alive, so
     * destruction order is very important here: mGrContext uses mGrGLInterface, and
     * through it, uses mGLContext
     */
    mGrContext = nullptr;
    mGrGLInterface = nullptr;
    mGLContext = nullptr;
  }

private:
  RefPtr<GLContext> mGLContext;
  SkRefPtr<GrGLInterface> mGrGLInterface;
  SkRefPtr<GrContext> mGrContext;
};

}
}

#else

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
}
}

#endif