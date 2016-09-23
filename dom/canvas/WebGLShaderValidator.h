/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_SHADER_VALIDATOR_H_
#define WEBGL_SHADER_VALIDATOR_H_

#include "angle/ShaderLang.h"
#include "GLDefs.h"
#include "nsString.h"
#include <string>

namespace mozilla {
namespace webgl {

class ShaderValidator final
{
    const ShHandle mHandle;
    const int mCompileOptions;
    const int mMaxVaryingVectors;
    bool mHasRun;

public:
    static ShaderValidator* Create(GLenum shaderType, ShShaderSpec spec,
                                   ShShaderOutput outputLanguage,
                                   const ShBuiltInResources& resources,
                                   int compileOptions);

private:
    ShaderValidator(ShHandle handle, int compileOptions, int maxVaryingVectors)
        : mHandle(handle)
        , mCompileOptions(compileOptions)
        , mMaxVaryingVectors(maxVaryingVectors)
        , mHasRun(false)
    { }

public:
    ~ShaderValidator();

    bool ValidateAndTranslate(const char* source);
    void GetInfoLog(nsACString* out) const;
    void GetOutput(nsACString* out) const;
    bool CanLinkTo(const ShaderValidator* prev, nsCString* const out_log) const;
    size_t CalcNumSamplerUniforms() const;
    size_t NumAttributes() const;

    bool FindAttribUserNameByMappedName(const std::string& mappedName,
                                        const std::string** const out_userName) const;

    bool FindAttribMappedNameByUserName(const std::string& userName,
                                        const std::string** const out_mappedName) const;

    bool FindVaryingMappedNameByUserName(const std::string& userName,
                                         const std::string** const out_mappedName) const;

    bool FindVaryingByMappedName(const std::string& mappedName,
                                 std::string* const out_userName,
                                 bool* const out_isArray) const;
    bool FindUniformByMappedName(const std::string& mappedName,
                                 std::string* const out_userName,
                                 bool* const out_isArray) const;
    bool FindUniformBlockByMappedName(const std::string& mappedName,
                                      std::string* const out_userName) const;

    void EnumerateFragOutputs(std::map<nsCString, const nsCString> &out_FragOutputs) const;

    bool ValidateTransformFeedback(const std::vector<nsString>& userNames,
                                   uint32_t maxComponents, nsCString* const out_errorText,
                                   std::vector<std::string>* const out_mappedNames);
};

} // namespace webgl
} // namespace mozilla

#endif // WEBGL_SHADER_VALIDATOR_H_
