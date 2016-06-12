/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureGarbageBin.h"
#include "GLContext.h"

using namespace mozilla;
using namespace mozilla::gl;

void
TextureGarbageBin::GLContextTeardown()
{
    EmptyGarbage();

    MutexAutoLock lock(mMutex);
    mGL = nullptr;
}

void
TextureGarbageBin::Trash(GLuint tex)
{
    MutexAutoLock lock(mMutex);
    if (!mGL)
        return;

    mGarbageTextures.push(tex);
}

void
TextureGarbageBin::EmptyGarbage()
{
    MutexAutoLock lock(mMutex);
    if (!mGL)
        return;

    MOZ_RELEASE_ASSERT(mGL->IsCurrent(), "GFX: GL context not current.");
    while (!mGarbageTextures.empty()) {
        GLuint tex = mGarbageTextures.top();
        mGarbageTextures.pop();
        mGL->fDeleteTextures(1, &tex);
    }
}
