/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_SHADER_H_
#define WEBGL_SHADER_H_

#include <map>
#include <string>
#include <vector>

#include "GLDefs.h"
#include "mozilla/LinkedList.h"
#include "mozilla/MemoryReporting.h"
#include "nsString.h"
#include "nsWrapperCache.h"

#include "WebGLObjectModel.h"

namespace mozilla {

namespace webgl {
class ShaderValidator;
} // namespace webgl

class WebGLShader final
    : public nsWrapperCache
    , public WebGLRefCountedObject<WebGLShader>
    , public LinkedListElement<WebGLShader>
{
    friend class WebGLContext;
    friend class WebGLProgram;

public:
    WebGLShader(WebGLContext* webgl, GLenum type);

protected:
    ~WebGLShader();

public:
    // GL funcs
    void CompileShader();
    JS::Value GetShaderParameter(GLenum pname) const;
    void GetShaderInfoLog(nsAString* out) const;
    void GetShaderSource(nsAString* out) const;
    void GetShaderTranslatedSource(nsAString* out) const;
    void ShaderSource(const nsAString& source);

    // Util funcs
    bool CanLinkTo(const WebGLShader* prev, nsCString* const out_log) const;
    size_t CalcNumSamplerUniforms() const;
    size_t NumAttributes() const;
    bool FindAttribUserNameByMappedName(const nsACString& mappedName,
                                        nsDependentCString* const out_userName) const;
    bool FindVaryingByMappedName(const nsACString& mappedName,
                                 nsCString* const out_userName,
                                 bool* const out_isArray) const;
    bool FindUniformByMappedName(const nsACString& mappedName,
                                 nsCString* const out_userName,
                                 bool* const out_isArray) const;
    bool FindUniformBlockByMappedName(const nsACString& mappedName,
                                      nsCString* const out_userName,
                                      bool* const out_isArray) const;

    void EnumerateFragOutputs(std::map<nsCString, const nsCString> &out_FragOutputs) const;

    bool IsCompiled() const {
        return mTranslationSuccessful && mCompilationSuccessful;
    }

private:
    void BindAttribLocation(GLuint prog, const nsCString& userName, GLuint index) const;
    void MapTransformFeedbackVaryings(const std::vector<nsString>& varyings,
                                      std::vector<std::string>* out_mappedVaryings) const;

public:
    // Other funcs
    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
    void Delete();

    WebGLContext* GetParentObject() const { return mContext; }

    virtual JSObject* WrapObject(JSContext* js, JS::Handle<JSObject*> givenProto) override;

    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLShader)
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLShader)

public:
    const GLuint mGLName;
    const GLenum mType;
protected:
    nsString mSource;
    nsCString mCleanSource;

    UniquePtr<webgl::ShaderValidator> mValidator;
    nsCString mValidationLog;
    bool mTranslationSuccessful;
    nsCString mTranslatedSource;

    bool mCompilationSuccessful;
    nsCString mCompilationLog;
};

} // namespace mozilla

#endif // WEBGL_SHADER_H_
