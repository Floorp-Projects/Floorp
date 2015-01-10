/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_SHADER_H_
#define WEBGL_SHADER_H_

#include "mozilla/LinkedList.h"
#include "mozilla/MemoryReporting.h"
#include "nsWrapperCache.h"
#include "WebGLObjectModel.h"
#include "WebGLUniformInfo.h"

namespace mozilla {

struct WebGLMappedIdentifier
{
    // ASCII strings
    nsCString original;
    nsCString mapped;

    WebGLMappedIdentifier(const nsACString& o, const nsACString& m)
        : original(o)
        , mapped(m)
    {}
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
    WebGLShader(WebGLContext* webgl, GLenum type);

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

    GLuint GLName() { return mGLName; }
    sh::GLenum ShaderType() { return mType; }

    void SetSource(const nsAString& src) {
        // TODO: Do some quick gzip here maybe? Getting this will be very rare,
        // and we have to keep it forever.
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

    WebGLContext* GetParentObject() const {
        return Context();
    }

    virtual JSObject* WrapObject(JSContext* cx) MOZ_OVERRIDE;

    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLShader)
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLShader)

protected:
    ~WebGLShader() {
        DeleteOnce();
    }

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

#endif // WEBGL_SHADER_H_
