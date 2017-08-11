/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLCONTEXTEAGL_H_
#define GLCONTEXTEAGL_H_

#include "GLContext.h"

#include <CoreGraphics/CoreGraphics.h>
#include <OpenGLES/EAGL.h>

namespace mozilla {
namespace gl {

class GLContextEAGL : public GLContext
{
    friend class GLContextProviderEAGL;

    EAGLContext* const mContext;

public:
    MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(GLContextEAGL, override)
    GLContextEAGL(CreateContextFlags flags, const SurfaceCaps& caps, EAGLContext* context,
                  GLContext* sharedContext, bool isOffscreen, ContextProfile profile);

    ~GLContextEAGL();

    virtual GLContextType GetContextType() const override {
        return GLContextType::EAGL;
    }

    static GLContextEAGL* Cast(GLContext* gl) {
        MOZ_ASSERT(gl->GetContextType() == GLContextType::EAGL);
        return static_cast<GLContextEAGL*>(gl);
    }

    bool Init() override;

    bool AttachToWindow(nsIWidget* aWidget);

    EAGLContext* GetEAGLContext() const { return mContext; }

    virtual bool MakeCurrentImpl(bool aForce) const override;

    virtual bool IsCurrent() const override;

    virtual bool SetupLookupFunction() override;

    virtual bool IsDoubleBuffered() const override;

    virtual bool SwapBuffers() override;

    virtual void GetWSIInfo(nsCString* const out) const override;

    virtual GLuint GetDefaultFramebuffer() override {
        return mBackbufferFB;
    }

    virtual bool RenewSurface(nsIWidget* aWidget) override {
        // FIXME: should use the passed widget instead of the existing one.
        return RecreateRB();
    }

private:
    GLuint mBackbufferRB;
    GLuint mBackbufferFB;

    void* mLayer;

    bool RecreateRB();
};

} // namespace gl
} // namespace mozilla

#endif // GLCONTEXTEAGL_H_
