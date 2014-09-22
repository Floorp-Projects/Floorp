/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLUNIFORMINFO_H_
#define WEBGLUNIFORMINFO_H_

#include "GLDefs.h"
#include "angle/ShaderLang.h"

namespace mozilla {

struct WebGLUniformInfo {
    uint32_t arraySize;
    bool isArray;
    sh::GLenum type;

    explicit WebGLUniformInfo(uint32_t s = 0, bool a = false, sh::GLenum t = LOCAL_GL_NONE)
        : arraySize(s), isArray(a), type(t) {}

    int ElementSize() const {
        switch (type) {
            case LOCAL_GL_INT:
            case LOCAL_GL_FLOAT:
            case LOCAL_GL_BOOL:
            case LOCAL_GL_SAMPLER_2D:
            case LOCAL_GL_SAMPLER_CUBE:
                return 1;
            case LOCAL_GL_INT_VEC2:
            case LOCAL_GL_FLOAT_VEC2:
            case LOCAL_GL_BOOL_VEC2:
                return 2;
            case LOCAL_GL_INT_VEC3:
            case LOCAL_GL_FLOAT_VEC3:
            case LOCAL_GL_BOOL_VEC3:
                return 3;
            case LOCAL_GL_INT_VEC4:
            case LOCAL_GL_FLOAT_VEC4:
            case LOCAL_GL_BOOL_VEC4:
            case LOCAL_GL_FLOAT_MAT2:
                return 4;
            case LOCAL_GL_FLOAT_MAT3:
                return 9;
            case LOCAL_GL_FLOAT_MAT4:
                return 16;
            default:
                MOZ_ASSERT(false); // should never get here
                return 0;
        }
    }
};

} // namespace mozilla

#endif
