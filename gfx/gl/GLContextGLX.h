/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLCONTEXTGLX_H_
#define GLCONTEXTGLX_H_

#include "GLContext.h"
#include "GLXLibrary.h"

namespace mozilla {
namespace gl {

class GLContextGLX : public GLContext
{
public:
    MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(GLContextGLX)
    static already_AddRefed<GLContextGLX>
    CreateGLContext(const SurfaceCaps& caps,
                    GLContextGLX* shareContext,
                    bool isOffscreen,
                    Display* display,
                    GLXDrawable drawable,
                    GLXFBConfig cfg,
                    bool deleteDrawable,
                    gfxXlibSurface* pixmap = nullptr);

    ~GLContextGLX();

    virtual GLContextType GetContextType() const MOZ_OVERRIDE { return GLContextType::GLX; }

    static GLContextGLX* Cast(GLContext* gl) {
        MOZ_ASSERT(gl->GetContextType() == GLContextType::GLX);
        return static_cast<GLContextGLX*>(gl);
    }

    bool Init() MOZ_OVERRIDE;

    virtual bool MakeCurrentImpl(bool aForce) MOZ_OVERRIDE;

    virtual bool IsCurrent() MOZ_OVERRIDE;

    virtual bool SetupLookupFunction() MOZ_OVERRIDE;

    virtual bool IsDoubleBuffered() const MOZ_OVERRIDE;

    virtual bool SupportsRobustness() const MOZ_OVERRIDE;

    virtual bool SwapBuffers() MOZ_OVERRIDE;

private:
    friend class GLContextProviderGLX;

    GLContextGLX(const SurfaceCaps& caps,
                 GLContext* shareContext,
                 bool isOffscreen,
                 Display *aDisplay,
                 GLXDrawable aDrawable,
                 GLXContext aContext,
                 bool aDeleteDrawable,
                 bool aDoubleBuffered,
                 gfxXlibSurface *aPixmap);

    GLXContext mContext;
    Display *mDisplay;
    GLXDrawable mDrawable;
    bool mDeleteDrawable;
    bool mDoubleBuffered;

    GLXLibrary* mGLX;

    nsRefPtr<gfxXlibSurface> mPixmap;
    bool mOwnsContext;
};

}
}

#endif // GLCONTEXTGLX_H_
