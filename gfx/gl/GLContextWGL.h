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
    MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(GLContextWGL, override)
    // From Window: (possibly for offscreen!)
    GLContextWGL(CreateContextFlags flags,
                 const SurfaceCaps& caps,
                 bool isOffscreen,
                 HDC aDC,
                 HGLRC aContext,
                 HWND aWindow = nullptr);

    // From PBuffer
    GLContextWGL(CreateContextFlags flags,
                 const SurfaceCaps& caps,
                 bool isOffscreen,
                 HANDLE aPbuffer,
                 HDC aDC,
                 HGLRC aContext,
                 int aPixelFormat);

    ~GLContextWGL();

    virtual GLContextType GetContextType() const override { return GLContextType::WGL; }

    static GLContextWGL* Cast(GLContext* gl) {
        MOZ_ASSERT(gl->GetContextType() == GLContextType::WGL);
        return static_cast<GLContextWGL*>(gl);
    }

    bool Init() override;

    virtual bool MakeCurrentImpl(bool aForce) override;

    virtual bool IsCurrent() override;

    void SetIsDoubleBuffered(bool aIsDB);

    virtual bool IsDoubleBuffered() const override;

    virtual bool SupportsRobustness() const override;

    virtual bool SwapBuffers() override;

    virtual bool SetupLookupFunction() override;

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
