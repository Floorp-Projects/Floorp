/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
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

class GLContextGLX : public GLContext
{
public:
    MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(GLContextGLX, override)
    static already_AddRefed<GLContextGLX>
    CreateGLContext(CreateContextFlags flags,
                    const SurfaceCaps& caps,
                    bool isOffscreen,
                    Display* display,
                    GLXDrawable drawable,
                    GLXFBConfig cfg,
                    bool deleteDrawable,
                    gfxXlibSurface* pixmap);

    // Finds a GLXFBConfig compatible with the provided window.
    static bool
    FindFBConfigForWindow(Display* display, int screen, Window window,
                          ScopedXFree<GLXFBConfig>* const out_scopedConfigArr,
                          GLXFBConfig* const out_config, int* const out_visid,
                          bool aWebRender);

    ~GLContextGLX();

    virtual GLContextType GetContextType() const override { return GLContextType::GLX; }

    static GLContextGLX* Cast(GLContext* gl) {
        MOZ_ASSERT(gl->GetContextType() == GLContextType::GLX);
        return static_cast<GLContextGLX*>(gl);
    }

    bool Init() override;

    virtual bool MakeCurrentImpl() const override;

    virtual bool IsCurrentImpl() const override;

    virtual bool SetupLookupFunction() override;

    virtual bool IsDoubleBuffered() const override;

    virtual bool SwapBuffers() override;

    virtual void GetWSIInfo(nsCString* const out) const override;

    // Overrides the current GLXDrawable backing the context and makes the
    // context current.
    bool OverrideDrawable(GLXDrawable drawable);

    // Undoes the effect of a drawable override.
    bool RestoreDrawable();

private:
    friend class GLContextProviderGLX;

    GLContextGLX(CreateContextFlags flags,
                 const SurfaceCaps& caps,
                 bool isOffscreen,
                 Display* aDisplay,
                 GLXDrawable aDrawable,
                 GLXContext aContext,
                 bool aDeleteDrawable,
                 bool aDoubleBuffered,
                 gfxXlibSurface* aPixmap);

    GLXContext mContext;
    Display* mDisplay;
    GLXDrawable mDrawable;
    bool mDeleteDrawable;
    bool mDoubleBuffered;

    GLXLibrary* mGLX;

    RefPtr<gfxXlibSurface> mPixmap;
    bool mOwnsContext;
};

}
}

#endif // GLCONTEXTGLX_H_
