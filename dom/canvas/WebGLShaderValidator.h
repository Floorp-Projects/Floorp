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

class ShaderValidator MOZ_FINAL
{
    const ShHandle mHandle;
    const int mCompileOptions;
    bool mHasRun;

public:
    static ShaderValidator* Create(GLenum shaderType, ShShaderSpec spec,
                                   ShShaderOutput outputLanguage,
                                   const ShBuiltInResources& resources,
                                   int compileOptions);

private:
    ShaderValidator(ShHandle handle, int compileOptions)
        : mHandle(handle)
        , mCompileOptions(compileOptions)
        , mHasRun(false)
    { }

public:
    ~ShaderValidator();

    bool ValidateAndTranslate(const char* source);
    void GetInfoLog(nsACString* out) const;
    void GetOutput(nsACString* out) const;
    bool CanLinkTo(const ShaderValidator* prev, nsCString* const out_log) const;
    size_t CalcNumSamplerUniforms() const;

    bool FindAttribUserNameByMappedName(const std::string& mappedName,
                                        const std::string** const out_userName) const;

    bool FindAttribMappedNameByUserName(const std::string& userName,
                                        const std::string** const out_mappedName) const;

    bool FindUniformByMappedName(const std::string& mappedName,
                                 std::string* const out_userName,
                                 bool* const out_isArray) const;
};

} // namespace webgl
} // namespace mozilla

#endif // WEBGL_SHADER_VALIDATOR_H_
