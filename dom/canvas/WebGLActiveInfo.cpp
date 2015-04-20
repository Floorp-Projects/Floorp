/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLActiveInfo.h"

#include "mozilla/dom/WebGLRenderingContextBinding.h"

namespace mozilla {

static uint8_t
ElemSizeFromType(GLenum elemType)
{
    switch (elemType) {
    case LOCAL_GL_BOOL:
    case LOCAL_GL_FLOAT:
    case LOCAL_GL_INT:
    case LOCAL_GL_UNSIGNED_INT:
    case LOCAL_GL_SAMPLER_2D:
    case LOCAL_GL_SAMPLER_3D:
    case LOCAL_GL_SAMPLER_CUBE:
    case LOCAL_GL_SAMPLER_2D_SHADOW:
    case LOCAL_GL_SAMPLER_2D_ARRAY:
    case LOCAL_GL_SAMPLER_2D_ARRAY_SHADOW:
    case LOCAL_GL_SAMPLER_CUBE_SHADOW:
    case LOCAL_GL_INT_SAMPLER_2D:
    case LOCAL_GL_INT_SAMPLER_3D:
    case LOCAL_GL_INT_SAMPLER_CUBE:
    case LOCAL_GL_INT_SAMPLER_2D_ARRAY:
    case LOCAL_GL_UNSIGNED_INT_SAMPLER_2D:
    case LOCAL_GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
    case LOCAL_GL_UNSIGNED_INT_SAMPLER_3D:
    case LOCAL_GL_UNSIGNED_INT_SAMPLER_CUBE:
        return 1;

    case LOCAL_GL_BOOL_VEC2:
    case LOCAL_GL_FLOAT_VEC2:
    case LOCAL_GL_INT_VEC2:
    case LOCAL_GL_UNSIGNED_INT_VEC2:
        return 2;

    case LOCAL_GL_BOOL_VEC3:
    case LOCAL_GL_FLOAT_VEC3:
    case LOCAL_GL_INT_VEC3:
    case LOCAL_GL_UNSIGNED_INT_VEC3:
        return 3;

    case LOCAL_GL_BOOL_VEC4:
    case LOCAL_GL_FLOAT_VEC4:
    case LOCAL_GL_INT_VEC4:
    case LOCAL_GL_UNSIGNED_INT_VEC4:
    case LOCAL_GL_FLOAT_MAT2:
        return 4;

    case LOCAL_GL_FLOAT_MAT2x3:
    case LOCAL_GL_FLOAT_MAT3x2:
        return 6;

    case LOCAL_GL_FLOAT_MAT2x4:
    case LOCAL_GL_FLOAT_MAT4x2:
        return 8;

    case LOCAL_GL_FLOAT_MAT3:
        return 9;

    case LOCAL_GL_FLOAT_MAT3x4:
    case LOCAL_GL_FLOAT_MAT4x3:
        return 12;

    case LOCAL_GL_FLOAT_MAT4:
        return 16;

    default:
        MOZ_CRASH("Bad `elemType`.");
    }
}

WebGLActiveInfo::WebGLActiveInfo(WebGLContext* webgl, GLint elemCount, GLenum elemType,
                                 bool isArray, const nsACString& baseUserName,
                                 const nsACString& baseMappedName)
    : mWebGL(webgl)
    , mElemCount(elemCount)
    , mElemType(elemType)
    , mBaseUserName(baseUserName)
    , mIsArray(isArray)
    , mElemSize(ElemSizeFromType(elemType))
    , mBaseMappedName(baseMappedName)
{ }

////////////////////////////////////////////////////////////////////////////////

JSObject*
WebGLActiveInfo::WrapObject(JSContext* js, JS::Handle<JSObject*> aGivenProto)
{
    return dom::WebGLActiveInfoBinding::Wrap(js, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WebGLActiveInfo)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLActiveInfo, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLActiveInfo, Release)

} // namespace mozilla
