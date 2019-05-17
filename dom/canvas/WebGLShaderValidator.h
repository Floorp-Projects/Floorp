/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_SHADER_VALIDATOR_H_
#define WEBGL_SHADER_VALIDATOR_H_

#include <string>

#include "GLDefs.h"
#include "GLSLANG/ShaderLang.h"
#include "nsString.h"

namespace mozilla {
namespace webgl {

class ShaderValidator final {
public:
  const ShHandle mHandle;
private:
  const ShCompileOptions mCompileOptions;
  const int mMaxVaryingVectors;
  bool mHasRun;

 public:
  static ShaderValidator* Create(GLenum shaderType, ShShaderSpec spec,
                                 ShShaderOutput outputLanguage,
                                 const ShBuiltInResources& resources,
                                 ShCompileOptions compileOptions);

 private:
  ShaderValidator(ShHandle handle, ShCompileOptions compileOptions,
                  int maxVaryingVectors)
      : mHandle(handle),
        mCompileOptions(compileOptions),
        mMaxVaryingVectors(maxVaryingVectors),
        mHasRun(false) {}

 public:
  ~ShaderValidator();

  bool ValidateAndTranslate(const char* source);
  bool CanLinkTo(const ShaderValidator* prev, nsCString* const out_log) const;

  bool FindUniformByMappedName(const std::string& mappedName,
                               std::string* const out_userName,
                               bool* const out_isArray) const;

  bool ValidateTransformFeedback(
      const std::vector<nsString>& userNames, uint32_t maxComponents,
      nsCString* const out_errorText,
      std::vector<std::string>* const out_mappedNames);
};

}  // namespace webgl
}  // namespace mozilla

#endif  // WEBGL_SHADER_VALIDATOR_H_
