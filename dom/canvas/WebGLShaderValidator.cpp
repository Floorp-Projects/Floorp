/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLShaderValidator.h"

#include "gfxPrefs.h"
#include "GLContext.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/Preferences.h"
#include "MurmurHash3.h"
#include "nsPrintfCString.h"
#include <string>
#include <vector>
#include "WebGLContext.h"

namespace mozilla {
namespace webgl {

uint64_t IdentifierHashFunc(const char* name, size_t len) {
  // NB: we use the x86 function everywhere, even though it's suboptimal perf
  // on x64.  They return different results; not sure if that's a requirement.
  uint64_t hash[2];
  MurmurHash3_x86_128(name, len, 0, hash);
  return hash[0];
}

static ShCompileOptions ChooseValidatorCompileOptions(
    const ShBuiltInResources& resources, const mozilla::gl::GLContext* gl) {
  ShCompileOptions options = SH_VARIABLES | SH_ENFORCE_PACKING_RESTRICTIONS |
                             SH_OBJECT_CODE | SH_INIT_GL_POSITION |
                             SH_INITIALIZE_UNINITIALIZED_LOCALS |
                             SH_INIT_OUTPUT_VARIABLES;

#ifdef XP_MACOSX
  options |= SH_REMOVE_INVARIANT_AND_CENTROID_FOR_ESSL3;
#else
  // We want to do this everywhere, but to do this on Mac, we need
  // to do it only on Mac OSX > 10.6 as this causes the shader
  // compiler in 10.6 to crash
  options |= SH_CLAMP_INDIRECT_ARRAY_BOUNDS;
#endif

  if (gl->WorkAroundDriverBugs()) {
#ifdef XP_MACOSX
    // Work around https://bugs.webkit.org/show_bug.cgi?id=124684,
    // https://chromium.googlesource.com/angle/angle/+/5e70cf9d0b1bb
    options |= SH_UNFOLD_SHORT_CIRCUIT;

    // Work around that Mac drivers handle struct scopes incorrectly.
    options |= SH_REGENERATE_STRUCT_NAMES;
    options |= SH_INIT_OUTPUT_VARIABLES;

    if (gl->Vendor() == gl::GLVendor::Intel) {
      // Work around that Intel drivers on Mac OSX handle for-loop incorrectly.
      options |= SH_ADD_AND_TRUE_TO_LOOP_CONDITION;

      options |= SH_REWRITE_TEXELFETCHOFFSET_TO_TEXELFETCH;
    }
#endif

    if (!gl->IsANGLE() && gl->Vendor() == gl::GLVendor::Intel) {
      // Failures on at least Windows+Intel+OGL on:
      // conformance/glsl/constructors/glsl-construct-mat2.html
      options |= SH_SCALARIZE_VEC_AND_MAT_CONSTRUCTOR_ARGS;
    }
  }

  if (gfxPrefs::WebGLAllANGLEOptions()) {
    options = -1;

    options ^= SH_INTERMEDIATE_TREE;
    options ^= SH_LINE_DIRECTIVES;
    options ^= SH_SOURCE_PATH;

    options ^= SH_LIMIT_EXPRESSION_COMPLEXITY;
    options ^= SH_LIMIT_CALL_STACK_DEPTH;

    options ^= SH_EXPAND_SELECT_HLSL_INTEGER_POW_EXPRESSIONS;
    options ^= SH_HLSL_GET_DIMENSIONS_IGNORES_BASE_LEVEL;

    options ^= SH_DONT_REMOVE_INVARIANT_FOR_FRAGMENT_INPUT;
    options ^= SH_REMOVE_INVARIANT_AND_CENTROID_FOR_ESSL3;
  }

  if (resources.MaxExpressionComplexity > 0) {
    options |= SH_LIMIT_EXPRESSION_COMPLEXITY;
  }
  if (resources.MaxCallStackDepth > 0) {
    options |= SH_LIMIT_CALL_STACK_DEPTH;
  }

  return options;
}

}  // namespace webgl

////////////////////////////////////////

static ShShaderOutput ShaderOutput(gl::GLContext* gl) {
  if (gl->IsGLES()) {
    return SH_ESSL_OUTPUT;
  } else {
    uint32_t version = gl->ShadingLanguageVersion();
    switch (version) {
      case 100:
        return SH_GLSL_COMPATIBILITY_OUTPUT;
      case 120:
        return SH_GLSL_COMPATIBILITY_OUTPUT;
      case 130:
        return SH_GLSL_130_OUTPUT;
      case 140:
        return SH_GLSL_140_OUTPUT;
      case 150:
        return SH_GLSL_150_CORE_OUTPUT;
      case 330:
        return SH_GLSL_330_CORE_OUTPUT;
      case 400:
        return SH_GLSL_400_CORE_OUTPUT;
      case 410:
        return SH_GLSL_410_CORE_OUTPUT;
      case 420:
        return SH_GLSL_420_CORE_OUTPUT;
      case 430:
        return SH_GLSL_430_CORE_OUTPUT;
      case 440:
        return SH_GLSL_440_CORE_OUTPUT;
      default:
        if (version >= 450) {
          // "OpenGL 4.6 is also guaranteed to support all previous versions of
          // the OpenGL Shading Language back to version 1.10."
          return SH_GLSL_450_CORE_OUTPUT;
        }
        gfxCriticalNote << "Unexpected GLSL version: " << version;
    }
  }

  return SH_GLSL_COMPATIBILITY_OUTPUT;
}

webgl::ShaderValidator* WebGLContext::CreateShaderValidator(
    GLenum shaderType) const {
  const auto spec = (IsWebGL2() ? SH_WEBGL2_SPEC : SH_WEBGL_SPEC);
  const auto outputLanguage = ShaderOutput(gl);

  ShBuiltInResources resources;
  memset(&resources, 0, sizeof(resources));
  sh::InitBuiltInResources(&resources);

  resources.HashFunction = webgl::IdentifierHashFunc;

  resources.MaxVertexAttribs = mGLMaxVertexAttribs;
  resources.MaxVertexUniformVectors = mGLMaxVertexUniformVectors;
  resources.MaxVertexTextureImageUnits = mGLMaxVertexTextureImageUnits;
  resources.MaxCombinedTextureImageUnits = mGLMaxCombinedTextureImageUnits;
  resources.MaxTextureImageUnits = mGLMaxFragmentTextureImageUnits;
  resources.MaxFragmentUniformVectors = mGLMaxFragmentUniformVectors;

  resources.MaxVertexOutputVectors = mGLMaxVertexOutputVectors;
  resources.MaxFragmentInputVectors = mGLMaxFragmentInputVectors;
  resources.MaxVaryingVectors = mGLMaxFragmentInputVectors;

  if (IsWebGL2()) {
    resources.MinProgramTexelOffset = mGLMinProgramTexelOffset;
    resources.MaxProgramTexelOffset = mGLMaxProgramTexelOffset;
  }

  const bool hasMRTs =
      (IsWebGL2() || IsExtensionEnabled(WebGLExtensionID::WEBGL_draw_buffers));
  resources.MaxDrawBuffers = (hasMRTs ? mGLMaxDrawBuffers : 1);

  if (IsExtensionEnabled(WebGLExtensionID::EXT_frag_depth))
    resources.EXT_frag_depth = 1;

  if (IsExtensionEnabled(WebGLExtensionID::OES_standard_derivatives))
    resources.OES_standard_derivatives = 1;

  if (IsExtensionEnabled(WebGLExtensionID::WEBGL_draw_buffers))
    resources.EXT_draw_buffers = 1;

  if (IsExtensionEnabled(WebGLExtensionID::EXT_shader_texture_lod))
    resources.EXT_shader_texture_lod = 1;

  // Tell ANGLE to allow highp in frag shaders. (unless disabled)
  // If underlying GLES doesn't have highp in frag shaders, it should complain
  // anyways.
  resources.FragmentPrecisionHigh = mDisableFragHighP ? 0 : 1;

  if (gl->WorkAroundDriverBugs()) {
#ifdef XP_MACOSX
    if (gl->Vendor() == gl::GLVendor::NVIDIA) {
      // Work around bug 890432
      resources.MaxExpressionComplexity = 1000;
    }
#endif
  }

  const auto compileOptions =
      webgl::ChooseValidatorCompileOptions(resources, gl);
  return webgl::ShaderValidator::Create(shaderType, spec, outputLanguage,
                                        resources, compileOptions);
}

////////////////////////////////////////

namespace webgl {

/*static*/
ShaderValidator* ShaderValidator::Create(GLenum shaderType, ShShaderSpec spec,
                                         ShShaderOutput outputLanguage,
                                         const ShBuiltInResources& resources,
                                         ShCompileOptions compileOptions) {
  ShHandle handle =
      sh::ConstructCompiler(shaderType, spec, outputLanguage, &resources);
  MOZ_RELEASE_ASSERT(handle);
  if (!handle) return nullptr;

  return new ShaderValidator(handle, compileOptions,
                             resources.MaxVaryingVectors);
}

ShaderValidator::~ShaderValidator() { sh::Destruct(mHandle); }

bool ShaderValidator::ValidateAndTranslate(const char* source) {
  MOZ_ASSERT(!mHasRun);
  mHasRun = true;

  const char* const parts[] = {source};
  return sh::Compile(mHandle, parts, ArrayLength(parts), mCompileOptions);
}

template <size_t N>
static bool StartsWith(const std::string& haystack, const char (&needle)[N]) {
  return haystack.compare(0, N - 1, needle) == 0;
}

bool ShaderValidator::CanLinkTo(const ShaderValidator* prev,
                                nsCString* const out_log) const {
  if (!prev) {
    nsPrintfCString error("Passed in NULL prev ShaderValidator.");
    *out_log = error;
    return false;
  }

  const auto shaderVersion = sh::GetShaderVersion(mHandle);
  if (sh::GetShaderVersion(prev->mHandle) != shaderVersion) {
    nsPrintfCString error(
        "Vertex shader version %d does not match"
        " fragment shader version %d.",
        sh::GetShaderVersion(prev->mHandle), sh::GetShaderVersion(mHandle));
    *out_log = error;
    return false;
  }

  {
    const std::vector<sh::Uniform>* vertPtr = sh::GetUniforms(prev->mHandle);
    const std::vector<sh::Uniform>* fragPtr = sh::GetUniforms(mHandle);
    if (!vertPtr || !fragPtr) {
      nsPrintfCString error("Could not create uniform list.");
      *out_log = error;
      return false;
    }

    for (auto itrFrag = fragPtr->begin(); itrFrag != fragPtr->end();
         ++itrFrag) {
      for (auto itrVert = vertPtr->begin(); itrVert != vertPtr->end();
           ++itrVert) {
        if (itrVert->name != itrFrag->name) continue;

        if (!itrVert->isSameUniformAtLinkTime(*itrFrag)) {
          nsPrintfCString error(
              "Uniform `%s` is not linkable between"
              " attached shaders.",
              itrFrag->name.c_str());
          *out_log = error;
          return false;
        }

        break;
      }
    }
  }
  {
    const auto vertVars = sh::GetInterfaceBlocks(prev->mHandle);
    const auto fragVars = sh::GetInterfaceBlocks(mHandle);
    if (!vertVars || !fragVars) {
      nsPrintfCString error("Could not create uniform block list.");
      *out_log = error;
      return false;
    }

    for (const auto& fragVar : *fragVars) {
      for (const auto& vertVar : *vertVars) {
        if (vertVar.name != fragVar.name) continue;

        if (!vertVar.isSameInterfaceBlockAtLinkTime(fragVar)) {
          nsPrintfCString error(
              "Interface block `%s` is not linkable between"
              " attached shaders.",
              fragVar.name.c_str());
          *out_log = error;
          return false;
        }

        break;
      }
    }
  }

  const auto& vertVaryings = sh::GetVaryings(prev->mHandle);
  const auto& fragVaryings = sh::GetVaryings(mHandle);
  if (!vertVaryings || !fragVaryings) {
    nsPrintfCString error("Could not create varying list.");
    *out_log = error;
    return false;
  }

  {
    std::vector<sh::ShaderVariable> staticUseVaryingList;

    for (const auto& fragVarying : *fragVaryings) {
      static const char prefix[] = "gl_";
      if (StartsWith(fragVarying.name, prefix)) {
        if (fragVarying.staticUse) {
          staticUseVaryingList.push_back(fragVarying);
        }
        continue;
      }

      bool definedInVertShader = false;
      bool staticVertUse = false;

      for (const auto& vertVarying : *vertVaryings) {
        if (vertVarying.name != fragVarying.name) continue;

        if (!vertVarying.isSameVaryingAtLinkTime(fragVarying, shaderVersion)) {
          nsPrintfCString error(
              "Varying `%s`is not linkable between"
              " attached shaders.",
              fragVarying.name.c_str());
          *out_log = error;
          return false;
        }

        definedInVertShader = true;
        staticVertUse = vertVarying.staticUse;
        break;
      }

      if (!definedInVertShader && fragVarying.staticUse) {
        nsPrintfCString error(
            "Varying `%s` has static-use in the frag"
            " shader, but is undeclared in the vert"
            " shader.",
            fragVarying.name.c_str());
        *out_log = error;
        return false;
      }

      if (staticVertUse && fragVarying.staticUse) {
        staticUseVaryingList.push_back(fragVarying);
      }
    }

    if (!sh::CheckVariablesWithinPackingLimits(mMaxVaryingVectors,
                                               staticUseVaryingList)) {
      *out_log =
          "Statically used varyings do not fit within packing limits. (see"
          " GLSL ES Specification 1.0.17, p111)";
      return false;
    }
  }

  if (shaderVersion == 100) {
    // Enforce ESSL1 invariant linking rules.
    bool isInvariant_Position = false;
    bool isInvariant_PointSize = false;
    bool isInvariant_FragCoord = false;
    bool isInvariant_PointCoord = false;

    for (const auto& varying : *vertVaryings) {
      if (varying.name == "gl_Position") {
        isInvariant_Position = varying.isInvariant;
      } else if (varying.name == "gl_PointSize") {
        isInvariant_PointSize = varying.isInvariant;
      }
    }

    for (const auto& varying : *fragVaryings) {
      if (varying.name == "gl_FragCoord") {
        isInvariant_FragCoord = varying.isInvariant;
      } else if (varying.name == "gl_PointCoord") {
        isInvariant_PointCoord = varying.isInvariant;
      }
    }

    ////

    const auto fnCanBuiltInsLink = [](bool vertIsInvariant,
                                      bool fragIsInvariant) {
      if (vertIsInvariant) return true;

      return !fragIsInvariant;
    };

    if (!fnCanBuiltInsLink(isInvariant_Position, isInvariant_FragCoord)) {
      *out_log =
          "gl_Position must be invariant if gl_FragCoord is. (see GLSL ES"
          " Specification 1.0.17, p39)";
      return false;
    }

    if (!fnCanBuiltInsLink(isInvariant_PointSize, isInvariant_PointCoord)) {
      *out_log =
          "gl_PointSize must be invariant if gl_PointCoord is. (see GLSL ES"
          " Specification 1.0.17, p39)";
      return false;
    }
  }

  return true;
}

// This must handle names like "foo.bar[0]".
bool ShaderValidator::FindUniformByMappedName(const std::string& mappedName,
                                              std::string* const out_userName,
                                              bool* const out_isArray) const {
  const std::vector<sh::Uniform>& uniforms = *sh::GetUniforms(mHandle);
  for (auto itr = uniforms.begin(); itr != uniforms.end(); ++itr) {
    const sh::ShaderVariable* found;
    if (!itr->findInfoByMappedName(mappedName, &found, out_userName)) continue;

    *out_isArray = found->isArray();
    return true;
  }

  const size_t dotPos = mappedName.find(".");

  const std::vector<sh::InterfaceBlock>& interfaces =
      *sh::GetInterfaceBlocks(mHandle);
  for (const auto& interface : interfaces) {
    std::string mappedFieldName;
    const bool hasInstanceName = !interface.instanceName.empty();

    // If the InterfaceBlock has an instanceName, all variables defined
    // within the block are qualified with the block name, as opposed
    // to being placed in the global scope.
    if (hasInstanceName) {
      // If mappedName has no block name prefix, skip
      if (std::string::npos == dotPos) continue;

      // If mappedName has a block name prefix that doesn't match, skip
      const std::string mappedInterfaceBlockName = mappedName.substr(0, dotPos);
      if (interface.mappedName != mappedInterfaceBlockName) continue;

      mappedFieldName = mappedName.substr(dotPos + 1);
    } else {
      mappedFieldName = mappedName;
    }

    for (const auto& field : interface.fields) {
      const sh::ShaderVariable* found;

      if (!field.findInfoByMappedName(mappedFieldName, &found, out_userName))
        continue;

      if (hasInstanceName) {
        // Prepend the user name of the interface that matched
        *out_userName = interface.name + "." + *out_userName;
      }

      *out_isArray = found->isArray();
      return true;
    }
  }

  return false;
}

}  // namespace webgl
}  // namespace mozilla
