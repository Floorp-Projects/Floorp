/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_SHADER_VALIDATOR_H_
#define WEBGL_SHADER_VALIDATOR_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "GLDefs.h"
#include "GLSLANG/ShaderLang.h"
#include "nsString.h"

namespace mozilla::webgl {

class ShaderValidatorResults final {
 public:
  std::string mInfoLog;
  bool mValid = false;

  std::string mObjectCode;
  int mShaderVersion = 0;
  int mVertexShaderNumViews = 0;

  std::vector<sh::Attribute> mAttributes;
  std::vector<sh::InterfaceBlock> mInterfaceBlocks;
  std::vector<sh::OutputVariable> mOutputVariables;
  std::vector<sh::Uniform> mUniforms;
  std::vector<sh::Varying> mVaryings;

  std::unordered_map<std::string, std::string> mNameMap;

  int mMaxVaryingVectors = 0;

  bool mNeeds_webgl_gl_VertexID_Offset = false;

  bool CanLinkTo(const ShaderValidatorResults& vert,
                 nsCString* const out_log) const;
  size_t SizeOfIncludingThis(MallocSizeOf) const;
};

class ShaderValidator final {
 public:
  const ShHandle mHandle;

 private:
  const ShCompileOptions mCompileOptions;
  const int mMaxVaryingVectors;

 public:
  bool mIfNeeded_webgl_gl_VertexID_Offset = false;

  static std::unique_ptr<ShaderValidator> Create(
      GLenum shaderType, ShShaderSpec spec, ShShaderOutput outputLanguage,
      const ShBuiltInResources& resources, ShCompileOptions compileOptions);

 private:
  ShaderValidator(ShHandle handle, ShCompileOptions compileOptions,
                  int maxVaryingVectors)
      : mHandle(handle),
        mCompileOptions(compileOptions),
        mMaxVaryingVectors(maxVaryingVectors) {}

 public:
  ~ShaderValidator();

  std::unique_ptr<const ShaderValidatorResults> ValidateAndTranslate(
      const char*) const;
};

}  // namespace mozilla::webgl

#endif  // WEBGL_SHADER_VALIDATOR_H_
