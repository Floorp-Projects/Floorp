/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VBOArena.h"
#include "GLContext.h"

using namespace mozilla::gl;

GLuint VBOArena::Allocate(GLContext *aGLContext)
{
    if (!mAvailableVBOs.size()) {
        GLuint vbo;
        aGLContext->fGenBuffers(1, &vbo);
        mAllocatedVBOs.push_back(vbo);
        return vbo;
    }
    GLuint vbo = mAvailableVBOs.back();
    mAvailableVBOs.pop_back();
    return vbo;
}

void VBOArena::Reset()
{
    mAvailableVBOs = mAllocatedVBOs;
}

void VBOArena::Flush(GLContext *aGLContext)
{
    if (mAvailableVBOs.size()) {
#ifdef DEBUG
        printf_stderr("VBOArena::Flush for %u VBOs\n", mAvailableVBOs.size());
#endif
        aGLContext->fDeleteBuffers(mAvailableVBOs.size(), &mAvailableVBOs[0]);
    }
}
