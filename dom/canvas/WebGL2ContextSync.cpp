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
WebGL2Context::IsSync(WebGLSync* sync)
{
   if (IsContextLost())
       return false;

   return ValidateObjectAllowDeleted("isSync", sync) && !sync->IsDeleted();
}

void
WebGL2Context::DeleteSync(WebGLSync* sync)
{
    if (IsContextLost())
        return;

    if (!ValidateObjectAllowDeletedOrNull("deleteSync", sync))
        return;

    if (!sync || sync->IsDeleted())
        return;

    sync->RequestDelete();
}

GLenum
WebGL2Context::ClientWaitSync(WebGLSync* sync, GLbitfield flags, GLuint64 timeout)
{
    if (IsContextLost())
        return LOCAL_GL_WAIT_FAILED;

    if (!sync || sync->IsDeleted()) {
        ErrorInvalidValue("clientWaitSync: sync is not a sync object.");
        return LOCAL_GL_WAIT_FAILED;
    }

    if (flags != 0 && flags != LOCAL_GL_SYNC_FLUSH_COMMANDS_BIT) {
        ErrorInvalidValue("clientWaitSync: flag must be SYNC_FLUSH_COMMANDS_BIT or 0");
        return LOCAL_GL_WAIT_FAILED;
    }

    MakeContextCurrent();
    return gl->fClientWaitSync(sync->mGLName, flags, timeout);
}

void
WebGL2Context::WaitSync(WebGLSync* sync, GLbitfield flags, GLuint64 timeout)
{
    if (IsContextLost())
        return;

    if (!sync || sync->IsDeleted()) {
        ErrorInvalidValue("waitSync: sync is not a sync object.");
        return;
    }

    if (flags != 0) {
        ErrorInvalidValue("waitSync: flags must be 0");
        return;
    }

    if (timeout != LOCAL_GL_TIMEOUT_IGNORED) {
        ErrorInvalidValue("waitSync: timeout must be TIMEOUT_IGNORED");
        return;
    }

    MakeContextCurrent();
    gl->fWaitSync(sync->mGLName, flags, timeout);
}

void
WebGL2Context::GetSyncParameter(JSContext*, WebGLSync* sync, GLenum pname, JS::MutableHandleValue retval)
{
    if (IsContextLost())
        return;

    if (!sync || sync->IsDeleted()) {
        ErrorInvalidValue("getSyncParameter: sync is not a sync object.");
        return;
    }

    retval.set(JS::NullValue());

    GLint result = 0;
    switch (pname) {
    case LOCAL_GL_OBJECT_TYPE:
    case LOCAL_GL_SYNC_STATUS:
    case LOCAL_GL_SYNC_CONDITION:
    case LOCAL_GL_SYNC_FLAGS:
        MakeContextCurrent();
        gl->fGetSynciv(sync->mGLName, pname, 1, nullptr, &result);
        retval.set(JS::Int32Value(result));
        return;
    }

    ErrorInvalidEnum("getSyncParameter: Invalid pname 0x%04x", pname);
}

} // namespace mozilla
