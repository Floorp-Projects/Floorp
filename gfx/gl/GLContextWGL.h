/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLCONTEXTWGL_H_
#define GLCONTEXTWGL_H_

#include "GLContext.h"
#include "WGLLibrary.h"

namespace mozilla {
namespace gl {

class GLContextWGL : public GLContext
{
public:
    MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(GLContextWGL)
    // From Window: (possibly for offscreen!)
    GLContextWGL(const SurfaceCaps& caps,
                 GLContext* sharedContext,
                 bool isOffscreen,
                 HDC aDC,
                 HGLRC aContext,
                 HWND aWindow = nullptr);

    // From PBuffer
    GLContextWGL(const SurfaceCaps& caps,
                 GLContext* sharedContext,
                 bool isOffscreen,
                 HANDLE aPbuffer,
                 HDC aDC,
                 HGLRC aContext,
                 int aPixelFormat);

    ~GLContextWGL();

    virtual GLContextType GetContextType() const MOZ_OVERRIDE { return GLContextType::WGL; }

    static GLContextWGL* Cast(GLContext* gl) {
        MOZ_ASSERT(gl->GetContextType() == GLContextType::WGL);
        return static_cast<GLContextWGL*>(gl);
    }

    bool Init();

    virtual bool MakeCurrentImpl(bool aForce) MOZ_OVERRIDE;

    virtual bool IsCurrent() MOZ_OVERRIDE;

    void SetIsDoubleBuffered(bool aIsDB);

    virtual bool IsDoubleBuffered() const MOZ_OVERRIDE;

    virtual bool SupportsRobustness() const MOZ_OVERRIDE;

    virtual bool SwapBuffers() MOZ_OVERRIDE;

    virtual bool SetupLookupFunction() MOZ_OVERRIDE;

    HGLRC Context() { return mContext; }

protected:
    friend class GLContextProviderWGL;

    HDC mDC;
    HGLRC mContext;
    HWND mWnd;
    HANDLE mPBuffer;
    int mPixelFormat;
    bool mIsDoubleBuffered;
};

}
}

#endif // GLCONTEXTWGL_H_
