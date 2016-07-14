/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_ACTIVE_INFO_H_
#define WEBGL_ACTIVE_INFO_H_

#include "GLDefs.h"
#include "mozilla/Attributes.h"
#include "nsCycleCollectionParticipant.h" // NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS
#include "nsISupportsImpl.h" // NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING
#include "nsString.h"
#include "nsWrapperCache.h"

namespace mozilla {

class WebGLContext;

class WebGLActiveInfo final
    : public nsWrapperCache
{
public:
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLActiveInfo)
    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLActiveInfo)

    virtual JSObject* WrapObject(JSContext* js, JS::Handle<JSObject*> givenProto) override;

    WebGLContext* GetParentObject() const {
        return mWebGL;
    }

    WebGLContext* const mWebGL;

    // ActiveInfo state:
    const uint32_t mElemCount; // `size`
    const GLenum mElemType; // `type`
    const nsCString mBaseUserName; // `name`, but ASCII, and without any final "[0]".

    // Not actually part of ActiveInfo:
    const bool mIsArray;
    const uint8_t mElemSize;
    const nsCString mBaseMappedName; // Without any final "[0]".

    bool IsSampler() const;

    WebGLActiveInfo(WebGLContext* webgl, GLint elemCount, GLenum elemType, bool isArray,
                    const nsACString& baseUserName, const nsACString& baseMappedName);

    /* GLES 2.0.25, p33:
     *   This command will return as much information about active
     *   attributes as possible. If no information is available, length will
     *   be set to zero and name will be an empty string. This situation
     *   could arise if GetActiveAttrib is issued after a failed link.
     *
     * It's the same for GetActiveUniform.
     */
    static WebGLActiveInfo* CreateInvalid(WebGLContext* webgl) {
        return new WebGLActiveInfo(webgl);
    }

    // WebIDL attributes
    GLint Size() const {
        return mElemCount;
    }

    GLenum Type() const {
        return mElemType;
    }

    void GetName(nsString& retval) const {
        CopyASCIItoUTF16(mBaseUserName, retval);
        if (mIsArray)
            retval.AppendLiteral("[0]");
    }

private:
    explicit WebGLActiveInfo(WebGLContext* webgl)
        : mWebGL(webgl)
        , mElemCount(0)
        , mElemType(0)
        , mBaseUserName("")
        , mIsArray(false)
        , mElemSize(0)
        , mBaseMappedName("")
    { }

    // Private destructor, to discourage deletion outside of Release():
    ~WebGLActiveInfo() { }
};

//////////

bool IsElemTypeSampler(GLenum elemType);

} // namespace mozilla

#endif // WEBGL_ACTIVE_INFO_H_
