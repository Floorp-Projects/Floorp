/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLShader.h"

#include "angle/ShaderLang.h"
#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "mozilla/MemoryReporting.h"
#include "WebGLContext.h"
#include "WebGLObjectModel.h"

namespace mozilla {

JSObject*
WebGLShader::WrapObject(JSContext* cx) {
    return dom::WebGLShaderBinding::Wrap(cx, this);
}

WebGLShader::WebGLShader(WebGLContext* webgl, GLenum type)
    : WebGLContextBoundObject(webgl)
    , mType(type)
    , mNeedsTranslation(true)
    , mAttribMaxNameLength(0)
    , mCompileStatus(false)
{
    mContext->MakeContextCurrent();
    mGLName = mContext->gl->fCreateShader(mType);
    mContext->mShaders.insertBack(this);
}

void
WebGLShader::Delete()
{
    mSource.Truncate();
    mTranslationLog.Truncate();
    mContext->MakeContextCurrent();
    mContext->gl->fDeleteShader(mGLName);
    LinkedListElement<WebGLShader>::removeFrom(mContext->mShaders);
}

size_t
WebGLShader::SizeOfIncludingThis(MallocSizeOf mallocSizeOf) const
{
    return mallocSizeOf(this) +
           mSource.SizeOfExcludingThisIfUnshared(mallocSizeOf) +
           mTranslationLog.SizeOfExcludingThisIfUnshared(mallocSizeOf);
}

void
WebGLShader::SetTranslationSuccess()
{
    mTranslationLog.SetIsVoid(true);
    mNeedsTranslation = false;
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WebGLShader)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLShader, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLShader, Release)

} // namespace mozilla
