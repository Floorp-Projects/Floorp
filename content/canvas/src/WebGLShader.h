/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLSHADER_H_
#define WEBGLSHADER_H_

#include "WebGLObjectModel.h"
#include "WebGLUniformInfo.h"

#include "nsWrapperCache.h"

#include "angle/ShaderLang.h"

#include "mozilla/LinkedList.h"
#include "mozilla/MemoryReporting.h"

namespace mozilla {

struct WebGLMappedIdentifier {
    nsCString original, mapped; // ASCII strings
    WebGLMappedIdentifier(const nsACString& o, const nsACString& m) : original(o), mapped(m) {}
};

class WebGLShader MOZ_FINAL
    : public nsWrapperCache
    , public WebGLRefCountedObject<WebGLShader>
    , public LinkedListElement<WebGLShader>
    , public WebGLContextBoundObject
{
    friend class WebGLContext;
    friend class WebGLProgram;

public:
    WebGLShader(WebGLContext *context, GLenum stype);

    ~WebGLShader() {
        DeleteOnce();
    }

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

    GLuint GLName() { return mGLName; }
    GLenum ShaderType() { return mType; }

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

    const nsString& TranslatedSource() const { return mTranslatedSource; }

    WebGLContext *GetParentObject() const {
        return Context();
    }

    virtual JSObject* WrapObject(JSContext *cx) MOZ_OVERRIDE;

    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLShader)
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLShader)

protected:

    GLuint mGLName;
    GLenum mType;
    nsString mSource;
    nsString mTranslatedSource;
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
