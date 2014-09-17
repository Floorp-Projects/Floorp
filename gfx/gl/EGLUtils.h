/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EGLUTILS_H_
#define EGLUTILS_H_

#include "GLContextTypes.h"
#include "GLTypes.h"
#include "mozilla/Assertions.h"

namespace mozilla {
namespace gl {

class GLLibraryEGL;

bool DoesEGLContextSupportSharingWithEGLImage(GLContext* gl);
EGLImage CreateEGLImage(GLContext* gl, GLuint tex);

////////////////////////////////////////////////////////////////////////
// EGLImageWrapper

class EGLImageWrapper
{
public:
    static EGLImageWrapper* Create(GLContext* gl, GLuint tex);

private:
    GLLibraryEGL& mLibrary;
    const EGLDisplay mDisplay;
public:
    const EGLImage mImage;
private:
    EGLSync mSync;

    EGLImageWrapper(GLLibraryEGL& library,
                    EGLDisplay display,
                    EGLImage image)
        : mLibrary(library)
        , mDisplay(display)
        , mImage(image)
        , mSync(0)
    {
        MOZ_ASSERT(mImage);
    }

public:
    ~EGLImageWrapper();

    // Insert a sync point on the given context, which should be the current active
    // context.
    bool FenceSync(GLContext* gl);

    bool ClientWaitSync();
};

} // namespace gl
} // namespace mozilla

#endif
