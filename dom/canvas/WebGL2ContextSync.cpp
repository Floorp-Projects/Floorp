/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"
#include "GLContext.h"

using namespace mozilla;
using namespace mozilla::dom;

// -------------------------------------------------------------------------
// Sync objects

already_AddRefed<WebGLSync>
WebGL2Context::FenceSync(GLenum condition, GLbitfield flags)
{
    MOZ_CRASH("Not Implemented.");
    return nullptr;
}

bool
WebGL2Context::IsSync(WebGLSync* sync)
{
    MOZ_CRASH("Not Implemented.");
    return false;
}

void
WebGL2Context::DeleteSync(WebGLSync* sync)
{
    MOZ_CRASH("Not Implemented.");
}

GLenum
WebGL2Context::ClientWaitSync(WebGLSync* sync, GLbitfield flags, GLuint64 timeout)
{
    MOZ_CRASH("Not Implemented.");
    return LOCAL_GL_FALSE;
}

void
WebGL2Context::WaitSync(WebGLSync* sync, GLbitfield flags, GLuint64 timeout)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::GetSyncParameter(JSContext*, WebGLSync* sync, GLenum pname, JS::MutableHandleValue retval)
{
    MOZ_CRASH("Not Implemented.");
}
