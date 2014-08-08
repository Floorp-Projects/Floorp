/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TEXTUREGARBAGEBIN_H_
#define TEXTUREGARBAGEBIN_H_

#include <stack>

#include "mozilla/Mutex.h"
#include "nsISupportsImpl.h"

#include "GLContextTypes.h"

namespace mozilla {
namespace gl {

class TextureGarbageBin MOZ_FINAL {
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TextureGarbageBin)

private:
    // Private destructor, to discourage deletion outside of Release():
    ~TextureGarbageBin()
    {
    }

    GLContext* mGL;
    Mutex mMutex;
    std::stack<GLuint> mGarbageTextures;

public:
    explicit TextureGarbageBin(GLContext* gl)
        : mGL(gl)
        , mMutex("TextureGarbageBin mutex")
    {}

    void GLContextTeardown();
    void Trash(GLuint tex);
    void EmptyGarbage();
};

}
}

#endif // TEXTUREGARBAGEBIN_H_
