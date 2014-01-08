/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLCONTEXTCGL_H_
#define GLCONTEXTCGL_H_

#include "GLContext.h"

#include "OpenGL/OpenGL.h"

#ifdef __OBJC__
#include <AppKit/NSOpenGL.h>
#else
typedef void NSOpenGLContext;
#endif

namespace mozilla {
namespace gl {

class GLContextCGL : public GLContext
{
    friend class GLContextProviderCGL;

    NSOpenGLContext *mContext;

public:
    GLContextCGL(const SurfaceCaps& caps,
                 GLContext *shareContext,
                 NSOpenGLContext *context,
                 bool isOffscreen = false);

    ~GLContextCGL();

    virtual GLContextType GetContextType() MOZ_OVERRIDE { return ContextTypeCGL; }

    static GLContextCGL* Cast(GLContext* gl) {
        MOZ_ASSERT(gl->GetContextType() == ContextTypeCGL);
        return static_cast<GLContextCGL*>(gl);
    }

    bool Init();

    NSOpenGLContext* GetNSOpenGLContext() const { return mContext; }
    CGLContextObj GetCGLContext() const;

    bool MakeCurrentImpl(bool aForce = false);

    virtual bool IsCurrent();

    virtual GLenum GetPreferredARGB32Format() MOZ_OVERRIDE;

    bool SetupLookupFunction();

    bool IsDoubleBuffered();

    bool SupportsRobustness();

    bool SwapBuffers();

    bool ResizeOffscreen(const gfx::IntSize& aNewSize);
};

}
}

#endif // GLCONTEXTCGL_H_
