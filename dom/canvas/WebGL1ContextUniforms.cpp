/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL1Context.h"

namespace mozilla {

bool
WebGL1Context::ValidateAttribPointerType(bool /*integerMode*/, GLenum type,
                                         uint32_t* out_alignment, const char* info)
{
    MOZ_ASSERT(out_alignment);
    if (!out_alignment)
        return false;

    switch (type) {
    case LOCAL_GL_BYTE:
    case LOCAL_GL_UNSIGNED_BYTE:
        *out_alignment = 1;
        return true;

    case LOCAL_GL_SHORT:
    case LOCAL_GL_UNSIGNED_SHORT:
        *out_alignment = 2;
        return true;
        // XXX case LOCAL_GL_FIXED:
    case LOCAL_GL_FLOAT:
        *out_alignment = 4;
        return true;
    }

    ErrorInvalidEnumInfo(info, type);
    return false;
}

bool
WebGL1Context::ValidateUniformMatrixTranspose(bool transpose, const char* info)
{
    if (transpose) {
        ErrorInvalidValue("%s: transpose must be FALSE as per the "
                          "OpenGL ES 2.0 spec", info);
        return false;
    }

    return true;
}

} // namespace mozilla
