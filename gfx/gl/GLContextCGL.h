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
    MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(GLContextCGL)
    GLContextCGL(const SurfaceCaps& caps,
                 GLContext *shareContext,
                 NSOpenGLContext *context,
                 bool isOffscreen = false);

    ~GLContextCGL();

    virtual GLContextType GetContextType() const MOZ_OVERRIDE { return GLContextType::CGL; }

    static GLContextCGL* Cast(GLContext* gl) {
        MOZ_ASSERT(gl->GetContextType() == GLContextType::CGL);
        return static_cast<GLContextCGL*>(gl);
    }

    bool Init();

    NSOpenGLContext* GetNSOpenGLContext() const { return mContext; }
    CGLContextObj GetCGLContext() const;

    virtual bool MakeCurrentImpl(bool aForce) MOZ_OVERRIDE;

    virtual bool IsCurrent() MOZ_OVERRIDE;

    virtual GLenum GetPreferredARGB32Format() const MOZ_OVERRIDE;

    virtual bool SetupLookupFunction() MOZ_OVERRIDE;

    virtual bool IsDoubleBuffered() const MOZ_OVERRIDE;

    virtual bool SupportsRobustness() const MOZ_OVERRIDE;

    virtual bool SwapBuffers() MOZ_OVERRIDE;
};

}
}

#endif // GLCONTEXTCGL_H_
