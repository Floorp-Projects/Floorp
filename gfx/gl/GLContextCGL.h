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

    NSOpenGLContext* mContext;

public:
    MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(GLContextCGL, override)
    GLContextCGL(CreateContextFlags flags, const SurfaceCaps& caps,
                 NSOpenGLContext* context, bool isOffscreen);

    ~GLContextCGL();

    virtual GLContextType GetContextType() const override { return GLContextType::CGL; }

    static GLContextCGL* Cast(GLContext* gl) {
        MOZ_ASSERT(gl->GetContextType() == GLContextType::CGL);
        return static_cast<GLContextCGL*>(gl);
    }

    bool Init() override;

    NSOpenGLContext* GetNSOpenGLContext() const { return mContext; }
    CGLContextObj GetCGLContext() const;

    virtual bool MakeCurrentImpl(bool aForce) const override;

    virtual bool IsCurrent() const override;

    virtual GLenum GetPreferredARGB32Format() const override;

    virtual bool SetupLookupFunction() override;

    virtual bool IsDoubleBuffered() const override;

    virtual bool SwapBuffers() override;

    virtual void GetWSIInfo(nsCString* const out) const override;
};

} // namespace gl
} // namespace mozilla

#endif // GLCONTEXTCGL_H_
