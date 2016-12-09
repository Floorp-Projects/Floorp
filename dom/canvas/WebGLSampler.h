/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_SAMPLER_H_
#define WEBGL_SAMPLER_H_

#include "mozilla/LinkedList.h"
#include "nsWrapperCache.h"
#include "WebGLObjectModel.h"
#include "WebGLStrongTypes.h"

namespace mozilla {

class WebGLSampler final
    : public nsWrapperCache
    , public WebGLRefCountedObject<WebGLSampler>
    , public LinkedListElement<WebGLSampler>
{
    friend class WebGLContext2;
    friend class WebGLTexture;

public:
    WebGLSampler(WebGLContext* webgl, GLuint sampler);

    const GLuint mGLName;

    void Delete();
    WebGLContext* GetParentObject() const;

    virtual JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto) override;

    void SamplerParameter(const char* funcName, GLenum pname, GLint paramInt);

private:
    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLSampler)
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLSampler)

    TexMinFilter mMinFilter;
    TexMagFilter mMagFilter;
    TexWrap mWrapS;
    TexWrap mWrapT;
    TexWrap mWrapR;
    GLint mMinLod;
    GLint mMaxLod;
    TexCompareMode mCompareMode;
    TexCompareFunc mCompareFunc;

private:
    ~WebGLSampler();
};

} // namespace mozilla

#endif // WEBGL_SAMPLER_H_
