/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContext.h"
#include "WebGL2Context.h"
#include "WebGLContextUtils.h"
#include "WebGLTexture.h"

namespace mozilla {

void
WebGL2Context::TexStorage(const char* funcName, uint8_t funcDims, GLenum rawTarget,
                          GLsizei levels, GLenum internalFormat, GLsizei width,
                          GLsizei height, GLsizei depth)
{
    TexTarget target;
    WebGLTexture* tex;
    if (!ValidateTexTarget(this, funcName, funcDims, rawTarget, &target, &tex))
        return;

    tex->TexStorage(funcName, target, levels, internalFormat, width, height, depth);
}

////////////////////

/*virtual*/ bool
WebGL2Context::IsTexParamValid(GLenum pname) const
{
    switch (pname) {
    case LOCAL_GL_TEXTURE_BASE_LEVEL:
    case LOCAL_GL_TEXTURE_COMPARE_FUNC:
    case LOCAL_GL_TEXTURE_COMPARE_MODE:
    case LOCAL_GL_TEXTURE_IMMUTABLE_FORMAT:
    case LOCAL_GL_TEXTURE_IMMUTABLE_LEVELS:
    case LOCAL_GL_TEXTURE_MAX_LEVEL:
    case LOCAL_GL_TEXTURE_WRAP_R:
    case LOCAL_GL_TEXTURE_MAX_LOD:
    case LOCAL_GL_TEXTURE_MIN_LOD:
        return true;

    default:
        return WebGLContext::IsTexParamValid(pname);
    }
}

} // namespace mozilla
