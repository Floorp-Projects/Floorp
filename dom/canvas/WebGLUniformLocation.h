/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_UNIFORM_LOCATION_H_
#define WEBGL_UNIFORM_LOCATION_H_

#include "GLDefs.h"
#include "mozilla/WeakPtr.h"
#include "nsCycleCollectionParticipant.h" // NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS
#include "nsISupportsImpl.h" // NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING
#include "nsWrapperCache.h"

#include "WebGLObjectModel.h"

struct JSContext;

namespace mozilla {
class WebGLActiveInfo;
class WebGLContext;
class WebGLProgram;

namespace webgl {
struct LinkedProgramInfo;
} // namespace webgl

class WebGLUniformLocation final
    : public nsWrapperCache
    , public WebGLContextBoundObject
{
public:
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLUniformLocation)
    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLUniformLocation)

    virtual JSObject* WrapObject(JSContext* js, JS::Handle<JSObject*> givenProto) override;

    WebGLContext* GetParentObject() const {
        return mContext;
    }

    const WeakPtr<const webgl::LinkedProgramInfo> mLinkInfo;
    const GLuint mLoc;
    const size_t mArrayIndex;
    const WebGLActiveInfo* const mActiveInfo;

    WebGLUniformLocation(WebGLContext* webgl, const webgl::LinkedProgramInfo* linkInfo,
                         GLuint loc, size_t arrayIndex, const WebGLActiveInfo* activeInfo);

    bool ValidateForProgram(WebGLProgram* prog, WebGLContext* webgl,
                            const char* funcName) const;
    bool ValidateSamplerSetter(GLint value, WebGLContext* webgl,
                               const char* funcName) const;
    bool ValidateSizeAndType(uint8_t setterElemSize, GLenum setterType,
                             WebGLContext* webgl, const char* funcName) const;
    bool ValidateArrayLength(uint8_t setterElemSize, size_t setterArraySize,
                             WebGLContext* webgl, const char* funcName) const;

    JS::Value GetUniform(JSContext* js, WebGLContext* webgl) const;

    // Needed for certain helper functions like ValidateObject.
    // `WebGLUniformLocation`s can't be 'Deleted' in the WebGL sense.
    bool IsDeleted() const { return false; }

protected:
    ~WebGLUniformLocation();
};

} // namespace mozilla

#endif // WEBGL_UNIFORM_LOCATION_H_
