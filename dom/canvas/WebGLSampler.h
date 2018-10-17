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
#include "WebGLTexture.h"

namespace mozilla {

class WebGLSampler final
    : public nsWrapperCache
    , public WebGLRefCountedObject<WebGLSampler>
    , public LinkedListElement<WebGLSampler>
    , public CacheInvalidator
{
    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLSampler)
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLSampler)

public:
    const GLuint mGLName;
private:
    webgl::SamplingState mState;

public:
    explicit WebGLSampler(WebGLContext* webgl);
private:
    ~WebGLSampler();

public:
    void Delete();
    WebGLContext* GetParentObject() const;

    virtual JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto) override;

    void SamplerParameter(GLenum pname, const FloatOrInt& param);

    const auto& State() const { return mState; }
};

} // namespace mozilla

#endif // WEBGL_SAMPLER_H_
