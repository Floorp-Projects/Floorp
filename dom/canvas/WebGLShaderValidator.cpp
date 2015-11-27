/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLShaderValidator.h"

#include "angle/ShaderLang.h"
#include "gfxPrefs.h"
#include "GLContext.h"
#include "mozilla/Preferences.h"
#include "MurmurHash3.h"
#include "nsPrintfCString.h"
#include "nsTArray.h"
#include <string>
#include <vector>
#include "WebGLContext.h"

namespace mozilla {
namespace webgl {

uint64_t
IdentifierHashFunc(const char* name, size_t len)
{
    // NB: we use the x86 function everywhere, even though it's suboptimal perf
    // on x64.  They return different results; not sure if that's a requirement.
    uint64_t hash[2];
    MurmurHash3_x86_128(name, len, 0, hash);
    return hash[0];
}

static int
ChooseValidatorCompileOptions(const ShBuiltInResources& resources,
                              const mozilla::gl::GLContext* gl)
{
    int options = SH_VARIABLES |
                  SH_ENFORCE_PACKING_RESTRICTIONS |
                  SH_INIT_VARYINGS_WITHOUT_STATIC_USE |
                  SH_OBJECT_CODE |
                  SH_LIMIT_CALL_STACK_DEPTH |
                  SH_INIT_GL_POSITION;

    if (resources.MaxExpressionComplexity > 0) {
        options |= SH_LIMIT_EXPRESSION_COMPLEXITY;
    }

    if (gfxPrefs::WebGLAllANGLEOptions()) {
        return options |
               SH_VALIDATE_LOOP_INDEXING |
               SH_UNROLL_FOR_LOOP_WITH_INTEGER_INDEX |
               SH_UNROLL_FOR_LOOP_WITH_SAMPLER_ARRAY_INDEX |
               SH_EMULATE_BUILT_IN_FUNCTIONS |
               SH_CLAMP_INDIRECT_ARRAY_BOUNDS |
               SH_UNFOLD_SHORT_CIRCUIT |
               SH_SCALARIZE_VEC_AND_MAT_CONSTRUCTOR_ARGS |
               SH_REGENERATE_STRUCT_NAMES;
    }

#ifndef XP_MACOSX
    // We want to do this everywhere, but to do this on Mac, we need
    // to do it only on Mac OSX > 10.6 as this causes the shader
    // compiler in 10.6 to crash
    options |= SH_CLAMP_INDIRECT_ARRAY_BOUNDS;
#endif

#ifdef XP_MACOSX
    if (gl->WorkAroundDriverBugs()) {
        // Work around https://bugs.webkit.org/show_bug.cgi?id=124684,
        // https://chromium.googlesource.com/angle/angle/+/5e70cf9d0b1bb
        options |= SH_UNFOLD_SHORT_CIRCUIT;

        // Work around bug 665578 and bug 769810
        if (gl->Vendor() == gl::GLVendor::ATI) {
            options |= SH_EMULATE_BUILT_IN_FUNCTIONS;
        }

        // Work around bug 735560
        if (gl->Vendor() == gl::GLVendor::Intel) {
            options |= SH_EMULATE_BUILT_IN_FUNCTIONS;
        }

        // Work around bug 636926
        if (gl->Vendor() == gl::GLVendor::NVIDIA) {
            options |= SH_UNROLL_FOR_LOOP_WITH_SAMPLER_ARRAY_INDEX;
        }
    }
#endif

    return options;
}

} // namespace webgl

////////////////////////////////////////

static ShShaderOutput
ShaderOutput(gl::GLContext* gl)
{
    if (gl->IsGLES()) {
        return SH_ESSL_OUTPUT;
    } else {
        uint32_t version = gl->ShadingLanguageVersion();
        switch (version) {
        case 100: return SH_GLSL_COMPATIBILITY_OUTPUT;
        case 120: return SH_GLSL_COMPATIBILITY_OUTPUT;
        case 130: return SH_GLSL_130_OUTPUT;
        case 140: return SH_GLSL_140_OUTPUT;
        case 150: return SH_GLSL_150_CORE_OUTPUT;
        case 330: return SH_GLSL_330_CORE_OUTPUT;
        case 400: return SH_GLSL_400_CORE_OUTPUT;
        case 410: return SH_GLSL_410_CORE_OUTPUT;
        case 420: return SH_GLSL_420_CORE_OUTPUT;
        case 430: return SH_GLSL_430_CORE_OUTPUT;
        case 440: return SH_GLSL_440_CORE_OUTPUT;
        case 450: return SH_GLSL_450_CORE_OUTPUT;
        default:
            MOZ_CRASH("Unexpected GLSL version.");
        }
    }

    return SH_GLSL_OUTPUT;
}

webgl::ShaderValidator*
WebGLContext::CreateShaderValidator(GLenum shaderType) const
{
    if (mBypassShaderValidation)
        return nullptr;

    ShShaderSpec spec = IsWebGL2() ? SH_WEBGL2_SPEC : SH_WEBGL_SPEC;
    ShShaderOutput outputLanguage = ShaderOutput(gl);
    ShBuiltInResources resources;
    memset(&resources, 0, sizeof(resources));
    ShInitBuiltInResources(&resources);

    resources.HashFunction = webgl::IdentifierHashFunc;

    resources.MaxVertexAttribs = mGLMaxVertexAttribs;
    resources.MaxVertexUniformVectors = mGLMaxVertexUniformVectors;
    resources.MaxVaryingVectors = mGLMaxVaryingVectors;
    resources.MaxVertexTextureImageUnits = mGLMaxVertexTextureImageUnits;
    resources.MaxCombinedTextureImageUnits = mGLMaxTextureUnits;
    resources.MaxTextureImageUnits = mGLMaxTextureImageUnits;
    resources.MaxFragmentUniformVectors = mGLMaxFragmentUniformVectors;
    resources.MaxDrawBuffers = mGLMaxDrawBuffers;

    if (IsWebGL2() || IsExtensionEnabled(WebGLExtensionID::EXT_frag_depth))
        resources.EXT_frag_depth = 1;

    if (IsWebGL2() || IsExtensionEnabled(WebGLExtensionID::OES_standard_derivatives))
        resources.OES_standard_derivatives = 1;

    if (IsWebGL2() || IsExtensionEnabled(WebGLExtensionID::WEBGL_draw_buffers))
        resources.EXT_draw_buffers = 1;

    if (IsWebGL2() || IsExtensionEnabled(WebGLExtensionID::EXT_shader_texture_lod))
        resources.EXT_shader_texture_lod = 1;

    // Tell ANGLE to allow highp in frag shaders. (unless disabled)
    // If underlying GLES doesn't have highp in frag shaders, it should complain anyways.
    resources.FragmentPrecisionHigh = mDisableFragHighP ? 0 : 1;

    if (gl->WorkAroundDriverBugs()) {
#ifdef XP_MACOSX
        if (gl->Vendor() == gl::GLVendor::NVIDIA) {
            // Work around bug 890432
            resources.MaxExpressionComplexity = 1000;
        }
#endif
    }

    int compileOptions = webgl::ChooseValidatorCompileOptions(resources, gl);

    return webgl::ShaderValidator::Create(shaderType, spec, outputLanguage, resources,
                                          compileOptions);
}

////////////////////////////////////////

namespace webgl {

/*static*/ ShaderValidator*
ShaderValidator::Create(GLenum shaderType, ShShaderSpec spec,
                        ShShaderOutput outputLanguage,
                        const ShBuiltInResources& resources, int compileOptions)
{
    ShHandle handle = ShConstructCompiler(shaderType, spec, outputLanguage, &resources);
    if (!handle)
        return nullptr;

    return new ShaderValidator(handle, compileOptions, resources.MaxVaryingVectors);
}

ShaderValidator::~ShaderValidator()
{
    ShDestruct(mHandle);
}

bool
ShaderValidator::ValidateAndTranslate(const char* source)
{
    MOZ_ASSERT(!mHasRun);
    mHasRun = true;

    const char* const parts[] = {
        source
    };
    return ShCompile(mHandle, parts, ArrayLength(parts), mCompileOptions);
}

void
ShaderValidator::GetInfoLog(nsACString* out) const
{
    MOZ_ASSERT(mHasRun);

    const std::string &log = ShGetInfoLog(mHandle);
    out->Assign(log.data(), log.length());
}

void
ShaderValidator::GetOutput(nsACString* out) const
{
    MOZ_ASSERT(mHasRun);

    const std::string &output = ShGetObjectCode(mHandle);
    out->Assign(output.data(), output.length());
}

template<size_t N>
static bool
StartsWith(const std::string& haystack, const char (&needle)[N])
{
    return haystack.compare(0, N - 1, needle) == 0;
}

bool
ShaderValidator::CanLinkTo(const ShaderValidator* prev, nsCString* const out_log) const
{
    if (!prev) {
        nsPrintfCString error("Passed in NULL prev ShaderValidator.");
        *out_log = error;
        return false;
    }

    {
        const std::vector<sh::Uniform>* vertPtr = ShGetUniforms(prev->mHandle);
        const std::vector<sh::Uniform>* fragPtr = ShGetUniforms(mHandle);
        if (!vertPtr || !fragPtr) {
            nsPrintfCString error("Could not create uniform list.");
            *out_log = error;
            return false;
        }

        for (auto itrFrag = fragPtr->begin(); itrFrag != fragPtr->end(); ++itrFrag) {
            for (auto itrVert = vertPtr->begin(); itrVert != vertPtr->end(); ++itrVert) {
                if (itrVert->name != itrFrag->name)
                    continue;

                if (!itrVert->isSameUniformAtLinkTime(*itrFrag)) {
                    nsPrintfCString error("Uniform `%s`is not linkable between"
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
        const std::vector<sh::Varying>* vertPtr = ShGetVaryings(prev->mHandle);
        const std::vector<sh::Varying>* fragPtr = ShGetVaryings(mHandle);
        if (!vertPtr || !fragPtr) {
            nsPrintfCString error("Could not create varying list.");
            *out_log = error;
            return false;
        }

        nsTArray<ShVariableInfo> staticUseVaryingList;

        for (auto itrFrag = fragPtr->begin(); itrFrag != fragPtr->end(); ++itrFrag) {
            const ShVariableInfo varInfo = { itrFrag->type,
                                             (int)itrFrag->elementCount() };

            static const char prefix[] = "gl_";
            if (StartsWith(itrFrag->name, prefix)) {
                if (itrFrag->staticUse)
                    staticUseVaryingList.AppendElement(varInfo);

                continue;
            }

            bool definedInVertShader = false;
            bool staticVertUse = false;

            for (auto itrVert = vertPtr->begin(); itrVert != vertPtr->end(); ++itrVert) {
                if (itrVert->name != itrFrag->name)
                    continue;

                if (!itrVert->isSameVaryingAtLinkTime(*itrFrag)) {
                    nsPrintfCString error("Varying `%s`is not linkable between"
                                          " attached shaders.",
                                          itrFrag->name.c_str());
                    *out_log = error;
                    return false;
                }

                definedInVertShader = true;
                staticVertUse = itrVert->staticUse;
                break;
            }

            if (!definedInVertShader && itrFrag->staticUse) {
                nsPrintfCString error("Varying `%s` has static-use in the frag"
                                      " shader, but is undeclared in the vert"
                                      " shader.", itrFrag->name.c_str());
                *out_log = error;
                return false;
            }

            if (staticVertUse && itrFrag->staticUse)
                staticUseVaryingList.AppendElement(varInfo);
        }

        if (!ShCheckVariablesWithinPackingLimits(mMaxVaryingVectors,
                                                 staticUseVaryingList.Elements(),
                                                 staticUseVaryingList.Length()))
        {
            *out_log = "Statically used varyings do not fit within packing limits. (see"
                       " GLSL ES Specification 1.0.17, p111)";
            return false;
        }
    }

    return true;
}

size_t
ShaderValidator::CalcNumSamplerUniforms() const
{
    size_t accum = 0;

    const std::vector<sh::Uniform>& uniforms = *ShGetUniforms(mHandle);

    for (auto itr = uniforms.begin(); itr != uniforms.end(); ++itr) {
        GLenum type = itr->type;
        if (type == LOCAL_GL_SAMPLER_2D ||
            type == LOCAL_GL_SAMPLER_CUBE)
        {
            accum += itr->arraySize;
        }
    }

    return accum;
}

size_t
ShaderValidator::NumAttributes() const
{
  return ShGetAttributes(mHandle)->size();
}

// Attribs cannot be structs or arrays, and neither can vertex inputs in ES3.
// Therefore, attrib names are always simple.
bool
ShaderValidator::FindAttribUserNameByMappedName(const std::string& mappedName,
                                                const std::string** const out_userName) const
{
    const std::vector<sh::Attribute>& attribs = *ShGetAttributes(mHandle);
    for (auto itr = attribs.begin(); itr != attribs.end(); ++itr) {
        if (itr->mappedName == mappedName) {
            *out_userName = &(itr->name);
            return true;
        }
    }

    return false;
}

bool
ShaderValidator::FindAttribMappedNameByUserName(const std::string& userName,
                                                const std::string** const out_mappedName) const
{
    const std::vector<sh::Attribute>& attribs = *ShGetAttributes(mHandle);
    for (auto itr = attribs.begin(); itr != attribs.end(); ++itr) {
        if (itr->name == userName) {
            *out_mappedName = &(itr->mappedName);
            return true;
        }
    }

    return false;
}

bool
ShaderValidator::FindVaryingByMappedName(const std::string& mappedName,
                                         std::string* const out_userName,
                                         bool* const out_isArray) const
{
    const std::vector<sh::Varying>& varyings = *ShGetVaryings(mHandle);
    for (auto itr = varyings.begin(); itr != varyings.end(); ++itr) {
        const sh::ShaderVariable* found;
        if (!itr->findInfoByMappedName(mappedName, &found, out_userName))
            continue;

        *out_isArray = found->isArray();
        return true;
    }

    return false;
}

bool
ShaderValidator::FindVaryingMappedNameByUserName(const std::string& userName,
                                                 const std::string** const out_mappedName) const
{
    const std::vector<sh::Varying>& attribs = *ShGetVaryings(mHandle);
    for (auto itr = attribs.begin(); itr != attribs.end(); ++itr) {
        if (itr->name == userName) {
            *out_mappedName = &(itr->mappedName);
            return true;
        }
    }

    return false;
}
// This must handle names like "foo.bar[0]".
bool
ShaderValidator::FindUniformByMappedName(const std::string& mappedName,
                                         std::string* const out_userName,
                                         bool* const out_isArray) const
{
    const std::vector<sh::Uniform>& uniforms = *ShGetUniforms(mHandle);
    for (auto itr = uniforms.begin(); itr != uniforms.end(); ++itr) {
        const sh::ShaderVariable* found;
        if (!itr->findInfoByMappedName(mappedName, &found, out_userName))
            continue;

        *out_isArray = found->isArray();
        return true;
    }

    const std::vector<sh::InterfaceBlock>& interfaces = *ShGetInterfaceBlocks(mHandle);
    for (const auto& interface : interfaces) {
        for (const auto& field : interface.fields) {
            const sh::ShaderVariable* found;

            if (!field.findInfoByMappedName(mappedName, &found, out_userName))
                continue;

            *out_isArray = found->isArray();
            return true;
        }
    }

    return false;
}

bool
ShaderValidator::FindUniformBlockByMappedName(const std::string& mappedName,
                                              std::string* const out_userName) const
{
    const std::vector<sh::InterfaceBlock>& interfaces = *ShGetInterfaceBlocks(mHandle);
    for (const auto& interface : interfaces) {
        if (mappedName == interface.mappedName) {
            *out_userName = interface.name;
            return true;
        }
    }

    return false;
}

} // namespace webgl
} // namespace mozilla
