/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL1Context.h"

namespace mozilla {

// -------------------------------------------------------------------------
// Buffer objects

/** Target validation for BindBuffer, etc */
bool
WebGL1Context::ValidateBufferTarget(GLenum target, const char* info)
{
    switch (target) {
    case LOCAL_GL_ARRAY_BUFFER:
    case LOCAL_GL_ELEMENT_ARRAY_BUFFER:
        return true;

    default:
        ErrorInvalidEnumInfo(info, target);
        return false;
    }
}

bool
WebGL1Context::ValidateBufferIndexedTarget(GLenum target, const char* info)
{
    ErrorInvalidEnumInfo(info, target);
    return false;
}

bool
WebGL1Context::ValidateBufferUsageEnum(GLenum usage, const char* info)
{
    switch (usage) {
    case LOCAL_GL_STREAM_DRAW:
    case LOCAL_GL_STATIC_DRAW:
    case LOCAL_GL_DYNAMIC_DRAW:
        return true;
    default:
        break;
    }

    ErrorInvalidEnumInfo(info, usage);
    return false;
}

} // namespace mozilla
