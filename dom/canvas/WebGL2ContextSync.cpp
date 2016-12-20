/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"

#include "GLContext.h"
#include "WebGLSync.h"

namespace mozilla {

// -------------------------------------------------------------------------
// Sync objects

already_AddRefed<WebGLSync>
WebGL2Context::FenceSync(GLenum condition, GLbitfield flags)
{
    if (IsContextLost())
        return nullptr;

    if (condition != LOCAL_GL_SYNC_GPU_COMMANDS_COMPLETE) {
        ErrorInvalidEnum("fenceSync: condition must be SYNC_GPU_COMMANDS_COMPLETE");
        return nullptr;
    }

    if (flags != 0) {
        ErrorInvalidValue("fenceSync: flags must be 0");
        return nullptr;
    }

    MakeContextCurrent();
    RefPtr<WebGLSync> globj = new WebGLSync(this, condition, flags);
    return globj.forget();
}

bool
WebGL2Context::IsSync(const WebGLSync* sync)
{
    if (!ValidateIsObject("isSync", sync))
        return false;

    return true;
}

void
WebGL2Context::DeleteSync(WebGLSync* sync)
{
    if (!ValidateDeleteObject("deleteSync", sync))
        return;

    sync->RequestDelete();
}

GLenum
WebGL2Context::ClientWaitSync(const WebGLSync& sync, GLbitfield flags, GLuint64 timeout)
{
    const char funcName[] = "clientWaitSync";
    if (IsContextLost())
        return LOCAL_GL_WAIT_FAILED;

    if (!ValidateObject(funcName, sync))
        return LOCAL_GL_WAIT_FAILED;

    if (flags != 0 && flags != LOCAL_GL_SYNC_FLUSH_COMMANDS_BIT) {
        ErrorInvalidValue("%s: `flags` must be SYNC_FLUSH_COMMANDS_BIT or 0.", funcName);
        return LOCAL_GL_WAIT_FAILED;
    }

    if (timeout > kMaxClientWaitSyncTimeoutNS) {
        ErrorInvalidOperation("%s: `timeout` must not exceed %s nanoseconds.", funcName,
                              "MAX_CLIENT_WAIT_TIMEOUT_WEBGL");
        return LOCAL_GL_WAIT_FAILED;
    }

    MakeContextCurrent();
    return gl->fClientWaitSync(sync.mGLName, flags, timeout);
}

void
WebGL2Context::WaitSync(const WebGLSync& sync, GLbitfield flags, GLint64 timeout)
{
    const char funcName[] = "waitSync";
    if (IsContextLost())
        return;

    if (!ValidateObject(funcName, sync))
        return;

    if (flags != 0) {
        ErrorInvalidValue("%s: `flags` must be 0.", funcName);
        return;
    }

    if (timeout != -1) {
        ErrorInvalidValue("%s: `timeout` must be TIMEOUT_IGNORED.", funcName);
        return;
    }

    MakeContextCurrent();
    gl->fWaitSync(sync.mGLName, flags, LOCAL_GL_TIMEOUT_IGNORED);
}

void
WebGL2Context::GetSyncParameter(JSContext*, const WebGLSync& sync, GLenum pname,
                                JS::MutableHandleValue retval)
{
    const char funcName[] = "getSyncParameter";
    retval.setNull();
    if (IsContextLost())
        return;

    if (!ValidateObject(funcName, sync))
        return;

    ////

    gl->MakeCurrent();

    GLint result = 0;
    switch (pname) {
    case LOCAL_GL_OBJECT_TYPE:
    case LOCAL_GL_SYNC_STATUS:
    case LOCAL_GL_SYNC_CONDITION:
    case LOCAL_GL_SYNC_FLAGS:
        gl->fGetSynciv(sync.mGLName, pname, 1, nullptr, &result);
        retval.set(JS::Int32Value(result));
        return;

    default:
        ErrorInvalidEnum("%s: Invalid pname 0x%04x", funcName, pname);
        return;
    }
}

} // namespace mozilla
