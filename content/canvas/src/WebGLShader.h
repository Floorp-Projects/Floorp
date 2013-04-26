/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLSHADER_H_
#define WEBGLSHADER_H_

#include "WebGLObjectModel.h"

#include "nsWrapperCache.h"

#include "angle/ShaderLang.h"

#include "mozilla/LinkedList.h"

namespace mozilla {

struct WebGLMappedIdentifier {
    nsCString original, mapped; // ASCII strings
    WebGLMappedIdentifier(const nsACString& o, const nsACString& m) : original(o), mapped(m) {}
};

struct WebGLUniformInfo {
    uint32_t arraySize;
    bool isArray;
    ShDataType type;

    WebGLUniformInfo(uint32_t s = 0, bool a = false, ShDataType t = SH_NONE)
        : arraySize(s), isArray(a), type(t) {}

    int ElementSize() const {
        switch (type) {
            case SH_INT:
            case SH_FLOAT:
            case SH_BOOL:
            case SH_SAMPLER_2D:
            case SH_SAMPLER_CUBE:
                return 1;
            case SH_INT_VEC2:
            case SH_FLOAT_VEC2:
            case SH_BOOL_VEC2:
                return 2;
            case SH_INT_VEC3:
            case SH_FLOAT_VEC3:
            case SH_BOOL_VEC3:
                return 3;
            case SH_INT_VEC4:
            case SH_FLOAT_VEC4:
            case SH_BOOL_VEC4:
            case SH_FLOAT_MAT2:
                return 4;
            case SH_FLOAT_MAT3:
                return 9;
            case SH_FLOAT_MAT4:
                return 16;
            default:
                NS_ABORT(); // should never get here
                return 0;
        }
    }
};

class WebGLShader MOZ_FINAL
    : public nsISupports
    , public WebGLRefCountedObject<WebGLShader>
    , public LinkedListElement<WebGLShader>
    , public WebGLContextBoundObject
    , public nsWrapperCache
{
    friend class WebGLContext;
    friend class WebGLProgram;

public:
    WebGLShader(WebGLContext *context, WebGLenum stype);

    ~WebGLShader() {
        DeleteOnce();
    }

    size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const;

    WebGLuint GLName() { return mGLName; }
    WebGLenum ShaderType() { return mType; }

    void SetSource(const nsAString& src) {
        // XXX do some quick gzip here maybe -- getting this will be very rare
        mSource.Assign(src);
    }

    const nsString& Source() const { return mSource; }

    void SetNeedsTranslation() { mNeedsTranslation = true; }
    bool NeedsTranslation() const { return mNeedsTranslation; }

    void SetCompileStatus (bool status) {
        mCompileStatus = status;
    }

    void Delete();

    bool CompileStatus() const {
        return mCompileStatus;
    }

    void SetTranslationSuccess();

    void SetTranslationFailure(const nsCString& msg) {
        mTranslationLog.Assign(msg); 
    }

    const nsCString& TranslationLog() const { return mTranslationLog; }

    WebGLContext *GetParentObject() const {
        return Context();
    }

    virtual JSObject* WrapObject(JSContext *cx,
                                 JS::Handle<JSObject*> scope) MOZ_OVERRIDE;

    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(WebGLShader)

protected:

    WebGLuint mGLName;
    WebGLenum mType;
    nsString mSource;
    nsCString mTranslationLog; // The translation log should contain only ASCII characters
    bool mNeedsTranslation;
    nsTArray<WebGLMappedIdentifier> mAttributes;
    nsTArray<WebGLMappedIdentifier> mUniforms;
    nsTArray<WebGLUniformInfo> mUniformInfos;
    int mAttribMaxNameLength;
    bool mCompileStatus;
};
} // namespace mozilla

#endif
