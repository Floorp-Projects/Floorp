/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLProgram.h"

#include "GLContext.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/dom/WebGL2RenderingContextBinding.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "mozilla/RefPtr.h"
#include "nsPrintfCString.h"
#include "WebGLActiveInfo.h"
#include "WebGLContext.h"
#include "WebGLShader.h"
#include "WebGLTransformFeedback.h"
#include "WebGLUniformLocation.h"
#include "WebGLValidateStrings.h"

namespace mozilla {

/* If `name`: "foo[3]"
 * Then returns true, with
 *     `out_baseName`: "foo"
 *     `out_isArray`: true
 *     `out_index`: 3
 *
 * If `name`: "foo"
 * Then returns true, with
 *     `out_baseName`: "foo"
 *     `out_isArray`: false
 *     `out_index`: 0
 */
static bool
ParseName(const nsCString& name, nsCString* const out_baseName,
          bool* const out_isArray, size_t* const out_arrayIndex)
{
    int32_t indexEnd = name.RFind("]");
    if (indexEnd == -1 ||
        (uint32_t)indexEnd != name.Length() - 1)
    {
        *out_baseName = name;
        *out_isArray = false;
        *out_arrayIndex = 0;
        return true;
    }

    int32_t indexOpenBracket = name.RFind("[");
    if (indexOpenBracket == -1)
        return false;

    uint32_t indexStart = indexOpenBracket + 1;
    uint32_t indexLen = indexEnd - indexStart;
    if (indexLen == 0)
        return false;

    const nsAutoCString indexStr(Substring(name, indexStart, indexLen));

    nsresult errorcode;
    int32_t indexNum = indexStr.ToInteger(&errorcode);
    if (NS_FAILED(errorcode))
        return false;

    if (indexNum < 0)
        return false;

    *out_baseName = StringHead(name, indexOpenBracket);
    *out_isArray = true;
    *out_arrayIndex = indexNum;
    return true;
}

static void
AssembleName(const nsCString& baseName, bool isArray, size_t arrayIndex,
             nsCString* const out_name)
{
    *out_name = baseName;
    if (isArray) {
        out_name->Append('[');
        out_name->AppendInt(uint64_t(arrayIndex));
        out_name->Append(']');
    }
}

////

static GLenum
AttribBaseType(GLenum attribType)
{
    switch (attribType) {
    case LOCAL_GL_FLOAT:
    case LOCAL_GL_FLOAT_VEC2:
    case LOCAL_GL_FLOAT_VEC3:
    case LOCAL_GL_FLOAT_VEC4:

    case LOCAL_GL_FLOAT_MAT2:
    case LOCAL_GL_FLOAT_MAT2x3:
    case LOCAL_GL_FLOAT_MAT2x4:

    case LOCAL_GL_FLOAT_MAT3x2:
    case LOCAL_GL_FLOAT_MAT3:
    case LOCAL_GL_FLOAT_MAT3x4:

    case LOCAL_GL_FLOAT_MAT4x2:
    case LOCAL_GL_FLOAT_MAT4x3:
    case LOCAL_GL_FLOAT_MAT4:
        return LOCAL_GL_FLOAT;

    case LOCAL_GL_INT:
    case LOCAL_GL_INT_VEC2:
    case LOCAL_GL_INT_VEC3:
    case LOCAL_GL_INT_VEC4:
        return LOCAL_GL_INT;

    case LOCAL_GL_UNSIGNED_INT:
    case LOCAL_GL_UNSIGNED_INT_VEC2:
    case LOCAL_GL_UNSIGNED_INT_VEC3:
    case LOCAL_GL_UNSIGNED_INT_VEC4:
        return LOCAL_GL_UNSIGNED_INT;

    default:
        MOZ_ASSERT(false, "unexpected attrib elemType");
        return 0;
    }
}

////

/*static*/ const webgl::UniformInfo::TexListT*
webgl::UniformInfo::GetTexList(WebGLActiveInfo* activeInfo)
{
    const auto& webgl = activeInfo->mWebGL;

    switch (activeInfo->mElemType) {
    case LOCAL_GL_SAMPLER_2D:
    case LOCAL_GL_SAMPLER_2D_SHADOW:
    case LOCAL_GL_INT_SAMPLER_2D:
    case LOCAL_GL_UNSIGNED_INT_SAMPLER_2D:
        return &webgl->mBound2DTextures;

    case LOCAL_GL_SAMPLER_CUBE:
    case LOCAL_GL_SAMPLER_CUBE_SHADOW:
    case LOCAL_GL_INT_SAMPLER_CUBE:
    case LOCAL_GL_UNSIGNED_INT_SAMPLER_CUBE:
        return &webgl->mBoundCubeMapTextures;

    case LOCAL_GL_SAMPLER_3D:
    case LOCAL_GL_INT_SAMPLER_3D:
    case LOCAL_GL_UNSIGNED_INT_SAMPLER_3D:
        return &webgl->mBound3DTextures;

    case LOCAL_GL_SAMPLER_2D_ARRAY:
    case LOCAL_GL_SAMPLER_2D_ARRAY_SHADOW:
    case LOCAL_GL_INT_SAMPLER_2D_ARRAY:
    case LOCAL_GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
        return &webgl->mBound2DArrayTextures;

    default:
        return nullptr;
    }
}

webgl::UniformInfo::UniformInfo(WebGLActiveInfo* activeInfo)
    : mActiveInfo(activeInfo)
    , mSamplerTexList(GetTexList(activeInfo))
{
    if (mSamplerTexList) {
        mSamplerValues.assign(mActiveInfo->mElemCount, 0);
    }
}

//////////

//#define DUMP_SHADERVAR_MAPPINGS

static already_AddRefed<const webgl::LinkedProgramInfo>
QueryProgramInfo(WebGLProgram* prog, gl::GLContext* gl)
{
    WebGLContext* const webgl = prog->mContext;

    RefPtr<webgl::LinkedProgramInfo> info(new webgl::LinkedProgramInfo(prog));

    GLuint maxAttribLenWithNull = 0;
    gl->fGetProgramiv(prog->mGLName, LOCAL_GL_ACTIVE_ATTRIBUTE_MAX_LENGTH,
                      (GLint*)&maxAttribLenWithNull);
    if (maxAttribLenWithNull < 1)
        maxAttribLenWithNull = 1;

    GLuint maxUniformLenWithNull = 0;
    gl->fGetProgramiv(prog->mGLName, LOCAL_GL_ACTIVE_UNIFORM_MAX_LENGTH,
                      (GLint*)&maxUniformLenWithNull);
    if (maxUniformLenWithNull < 1)
        maxUniformLenWithNull = 1;

    GLuint maxUniformBlockLenWithNull = 0;
    if (gl->IsSupported(gl::GLFeature::uniform_buffer_object)) {
        gl->fGetProgramiv(prog->mGLName, LOCAL_GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH,
                          (GLint*)&maxUniformBlockLenWithNull);
        if (maxUniformBlockLenWithNull < 1)
            maxUniformBlockLenWithNull = 1;
    }

    GLuint maxTransformFeedbackVaryingLenWithNull = 0;
    if (gl->IsSupported(gl::GLFeature::transform_feedback2)) {
        gl->fGetProgramiv(prog->mGLName, LOCAL_GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH,
                          (GLint*)&maxTransformFeedbackVaryingLenWithNull);
        if (maxTransformFeedbackVaryingLenWithNull < 1)
            maxTransformFeedbackVaryingLenWithNull = 1;
    }

    // Attribs (can't be arrays)

    GLuint numActiveAttribs = 0;
    gl->fGetProgramiv(prog->mGLName, LOCAL_GL_ACTIVE_ATTRIBUTES,
                      (GLint*)&numActiveAttribs);

    for (GLuint i = 0; i < numActiveAttribs; i++) {
        nsAutoCString mappedName;
        mappedName.SetLength(maxAttribLenWithNull - 1);

        GLsizei lengthWithoutNull = 0;
        GLint elemCount = 0; // `size`
        GLenum elemType = 0; // `type`
        gl->fGetActiveAttrib(prog->mGLName, i, mappedName.Length()+1, &lengthWithoutNull,
                             &elemCount, &elemType, mappedName.BeginWriting());
        GLenum error = gl->fGetError();
        if (error != LOCAL_GL_NO_ERROR) {
            gfxCriticalNote << "Failed to do glGetActiveAttrib: " << error;
        }

        mappedName.SetLength(lengthWithoutNull);

        ////

        nsCString userName;
        if (!prog->FindAttribUserNameByMappedName(mappedName, &userName)) {
            userName = mappedName;
        }

        ///////

        GLint loc = gl->fGetAttribLocation(prog->mGLName,
                                           mappedName.BeginReading());
        if (gl->WorkAroundDriverBugs() &&
            mappedName.EqualsIgnoreCase("gl_", 3))
        {
            // Bug 1328559: Appears problematic on ANGLE and OSX, but not Linux or Win+GL.
            loc = -1;
        }
#ifdef DUMP_SHADERVAR_MAPPINGS
        printf_stderr("[attrib %u/%u] @%i %s->%s\n", i, numActiveAttribs, loc,
                      userName.BeginReading(), mappedName.BeginReading());
#endif
        MOZ_ASSERT_IF(mappedName.EqualsIgnoreCase("gl_", 3), loc == -1);

        ///////

        const bool isArray = false;
        const RefPtr<WebGLActiveInfo> activeInfo = new WebGLActiveInfo(webgl, elemCount,
                                                                       elemType, isArray,
                                                                       userName,
                                                                       mappedName);
        const GLenum baseType = AttribBaseType(elemType);
        const webgl::AttribInfo attrib = {activeInfo, loc, baseType};
        info->attribs.push_back(attrib);
    }

    // Uniforms (can be basically anything)

    const bool needsCheckForArrays = gl->WorkAroundDriverBugs();

    GLuint numActiveUniforms = 0;
    gl->fGetProgramiv(prog->mGLName, LOCAL_GL_ACTIVE_UNIFORMS,
                      (GLint*)&numActiveUniforms);

    for (GLuint i = 0; i < numActiveUniforms; i++) {
        nsAutoCString mappedName;
        mappedName.SetLength(maxUniformLenWithNull - 1);

        GLsizei lengthWithoutNull = 0;
        GLint elemCount = 0; // `size`
        GLenum elemType = 0; // `type`
        gl->fGetActiveUniform(prog->mGLName, i, mappedName.Length()+1, &lengthWithoutNull,
                              &elemCount, &elemType, mappedName.BeginWriting());

        mappedName.SetLength(lengthWithoutNull);

        ///////

        nsAutoCString baseMappedName;
        bool isArray;
        size_t arrayIndex;
        if (!ParseName(mappedName, &baseMappedName, &isArray, &arrayIndex))
            MOZ_CRASH("GFX: Failed to parse `mappedName` received from driver.");

        // Note that for good drivers, `isArray` should already be correct.
        // However, if FindUniform succeeds, it will be validator-guaranteed correct.

        ///////

        nsAutoCString baseUserName;
        if (!prog->FindUniformByMappedName(baseMappedName, &baseUserName, &isArray)) {
            // Validator likely missing.
            baseUserName = baseMappedName;

            if (needsCheckForArrays && !isArray) {
                // By GLES 3, GetUniformLocation("foo[0]") should return -1 if `foo` is
                // not an array. Our current linux Try slaves return the location of `foo`
                // anyways, though.
                std::string mappedNameStr = baseMappedName.BeginReading();
                mappedNameStr += "[0]";

                GLint loc = gl->fGetUniformLocation(prog->mGLName, mappedNameStr.c_str());
                if (loc != -1)
                    isArray = true;
            }
        }

        ///////

#ifdef DUMP_SHADERVAR_MAPPINGS
        printf_stderr("[uniform %u/%u] %s->%s\n", i, numActiveUniforms,
                      baseUserName.BeginReading(), mappedName.BeginReading());
#endif

        ///////

        const RefPtr<WebGLActiveInfo> activeInfo = new WebGLActiveInfo(webgl, elemCount,
                                                                       elemType, isArray,
                                                                       baseUserName,
                                                                       baseMappedName);

        auto* uniform = new webgl::UniformInfo(activeInfo);
        info->uniforms.push_back(uniform);

        if (uniform->mSamplerTexList) {
            info->uniformSamplers.push_back(uniform);
        }
    }

    // Uniform Blocks (can be arrays, but can't contain sampler types)

    if (gl->IsSupported(gl::GLFeature::uniform_buffer_object)) {
        GLuint numActiveUniformBlocks = 0;
        gl->fGetProgramiv(prog->mGLName, LOCAL_GL_ACTIVE_UNIFORM_BLOCKS,
                          (GLint*)&numActiveUniformBlocks);

        for (GLuint i = 0; i < numActiveUniformBlocks; i++) {
            nsAutoCString mappedName;
            mappedName.SetLength(maxUniformBlockLenWithNull - 1);

            GLint lengthWithoutNull;
            gl->fGetActiveUniformBlockiv(prog->mGLName, i, LOCAL_GL_UNIFORM_BLOCK_NAME_LENGTH, &lengthWithoutNull);
            gl->fGetActiveUniformBlockName(prog->mGLName, i, maxUniformBlockLenWithNull, &lengthWithoutNull, mappedName.BeginWriting());
            mappedName.SetLength(lengthWithoutNull);

            ////

            nsCString userName;
            if (!prog->UnmapUniformBlockName(mappedName, &userName))
                continue;

#ifdef DUMP_SHADERVAR_MAPPINGS
            printf_stderr("[uniform block %u/%u] %s->%s\n", i, numActiveUniformBlocks,
                          userName.BeginReading(), mappedName.BeginReading());
#endif

            ////

            GLuint dataSize = 0;
            gl->fGetActiveUniformBlockiv(prog->mGLName, i,
                                         LOCAL_GL_UNIFORM_BLOCK_DATA_SIZE,
                                         (GLint*)&dataSize);


            auto* block = new webgl::UniformBlockInfo(webgl, userName, mappedName,
                                                      dataSize);
            info->uniformBlocks.push_back(block);
        }
    }

    // Transform feedback varyings (can be arrays)

    if (gl->IsSupported(gl::GLFeature::transform_feedback2)) {
        GLuint numTransformFeedbackVaryings = 0;
        gl->fGetProgramiv(prog->mGLName, LOCAL_GL_TRANSFORM_FEEDBACK_VARYINGS,
                          (GLint*)&numTransformFeedbackVaryings);

        for (GLuint i = 0; i < numTransformFeedbackVaryings; i++) {
            nsAutoCString mappedName;
            mappedName.SetLength(maxTransformFeedbackVaryingLenWithNull - 1);

            GLint lengthWithoutNull;
            GLsizei elemCount;
            GLenum elemType;
            gl->fGetTransformFeedbackVarying(prog->mGLName, i,
                                             maxTransformFeedbackVaryingLenWithNull,
                                             &lengthWithoutNull, &elemCount, &elemType,
                                             mappedName.BeginWriting());
            mappedName.SetLength(lengthWithoutNull);

            ////

            nsAutoCString baseMappedName;
            bool isArray;
            size_t arrayIndex;
            if (!ParseName(mappedName, &baseMappedName, &isArray, &arrayIndex))
                MOZ_CRASH("GFX: Failed to parse `mappedName` received from driver.");

            nsAutoCString baseUserName;
            if (!prog->FindVaryingByMappedName(mappedName, &baseUserName, &isArray)) {
                baseUserName = baseMappedName;
            }

            ////

#ifdef DUMP_SHADERVAR_MAPPINGS
            printf_stderr("[transform feedback varying %u/%u] %s->%s\n", i,
                          numTransformFeedbackVaryings, baseUserName.BeginReading(),
                          mappedName.BeginReading());
#endif

            const RefPtr<WebGLActiveInfo> activeInfo = new WebGLActiveInfo(webgl,
                                                                           elemCount,
                                                                           elemType,
                                                                           isArray,
                                                                           baseUserName,
                                                                           mappedName);
            info->transformFeedbackVaryings.push_back(activeInfo);
        }
    }

    // Frag outputs

    prog->EnumerateFragOutputs(info->fragDataMap);

    return info.forget();
}

////////////////////////////////////////////////////////////////////////////////

webgl::LinkedProgramInfo::LinkedProgramInfo(WebGLProgram* prog)
    : prog(prog)
    , transformFeedbackBufferMode(prog->mNextLink_TransformFeedbackBufferMode)
{ }

webgl::LinkedProgramInfo::~LinkedProgramInfo()
{
    for (auto& cur : uniforms) {
        delete cur;
    }
    for (auto& cur : uniformBlocks) {
        delete cur;
    }
}

////////////////////////////////////////////////////////////////////////////////
// WebGLProgram

static GLuint
CreateProgram(gl::GLContext* gl)
{
    gl->MakeCurrent();
    return gl->fCreateProgram();
}

WebGLProgram::WebGLProgram(WebGLContext* webgl)
    : WebGLRefCountedObject(webgl)
    , mGLName(CreateProgram(webgl->GL()))
    , mNumActiveTFOs(0)
    , mNextLink_TransformFeedbackBufferMode(LOCAL_GL_INTERLEAVED_ATTRIBS)
{
    mContext->mPrograms.insertBack(this);
}

WebGLProgram::~WebGLProgram()
{
    DeleteOnce();
}

void
WebGLProgram::Delete()
{
    gl::GLContext* gl = mContext->GL();

    gl->MakeCurrent();
    gl->fDeleteProgram(mGLName);

    mVertShader = nullptr;
    mFragShader = nullptr;

    mMostRecentLinkInfo = nullptr;

    LinkedListElement<WebGLProgram>::removeFrom(mContext->mPrograms);
}

////////////////////////////////////////////////////////////////////////////////
// GL funcs

void
WebGLProgram::AttachShader(WebGLShader* shader)
{
    WebGLRefPtr<WebGLShader>* shaderSlot;
    switch (shader->mType) {
    case LOCAL_GL_VERTEX_SHADER:
        shaderSlot = &mVertShader;
        break;
    case LOCAL_GL_FRAGMENT_SHADER:
        shaderSlot = &mFragShader;
        break;
    default:
        mContext->ErrorInvalidOperation("attachShader: Bad type for shader.");
        return;
    }

    if (*shaderSlot) {
        if (shader == *shaderSlot) {
            mContext->ErrorInvalidOperation("attachShader: `shader` is already attached.");
        } else {
            mContext->ErrorInvalidOperation("attachShader: Only one of each type of"
                                            " shader may be attached to a program.");
        }
        return;
    }

    *shaderSlot = shader;

    mContext->MakeContextCurrent();
    mContext->gl->fAttachShader(mGLName, shader->mGLName);
}

void
WebGLProgram::BindAttribLocation(GLuint loc, const nsAString& name)
{
    if (!ValidateGLSLVariableName(name, mContext, "bindAttribLocation"))
        return;

    if (loc >= mContext->MaxVertexAttribs()) {
        mContext->ErrorInvalidValue("bindAttribLocation: `location` must be less than"
                                    " MAX_VERTEX_ATTRIBS.");
        return;
    }

    if (StringBeginsWith(name, NS_LITERAL_STRING("gl_"))) {
        mContext->ErrorInvalidOperation("bindAttribLocation: Can't set the location of a"
                                        " name that starts with 'gl_'.");
        return;
    }

    NS_LossyConvertUTF16toASCII asciiName(name);

    auto res = mNextLink_BoundAttribLocs.insert({asciiName, loc});

    const bool wasInserted = res.second;
    if (!wasInserted) {
        auto itr = res.first;
        itr->second = loc;
    }
}

void
WebGLProgram::DetachShader(const WebGLShader* shader)
{
    MOZ_ASSERT(shader);

    WebGLRefPtr<WebGLShader>* shaderSlot;
    switch (shader->mType) {
    case LOCAL_GL_VERTEX_SHADER:
        shaderSlot = &mVertShader;
        break;
    case LOCAL_GL_FRAGMENT_SHADER:
        shaderSlot = &mFragShader;
        break;
    default:
        mContext->ErrorInvalidOperation("attachShader: Bad type for shader.");
        return;
    }

    if (*shaderSlot != shader) {
        mContext->ErrorInvalidOperation("detachShader: `shader` is not attached.");
        return;
    }

    *shaderSlot = nullptr;

    mContext->MakeContextCurrent();
    mContext->gl->fDetachShader(mGLName, shader->mGLName);
}

already_AddRefed<WebGLActiveInfo>
WebGLProgram::GetActiveAttrib(GLuint index) const
{
    if (!mMostRecentLinkInfo) {
        RefPtr<WebGLActiveInfo> ret = WebGLActiveInfo::CreateInvalid(mContext);
        return ret.forget();
    }

    const auto& attribs = mMostRecentLinkInfo->attribs;

    if (index >= attribs.size()) {
        mContext->ErrorInvalidValue("`index` (%i) must be less than %s (%zu).",
                                    index, "ACTIVE_ATTRIBS", attribs.size());
        return nullptr;
    }

    RefPtr<WebGLActiveInfo> ret = attribs[index].mActiveInfo;
    return ret.forget();
}

already_AddRefed<WebGLActiveInfo>
WebGLProgram::GetActiveUniform(GLuint index) const
{
    if (!mMostRecentLinkInfo) {
        // According to the spec, this can return null.
        RefPtr<WebGLActiveInfo> ret = WebGLActiveInfo::CreateInvalid(mContext);
        return ret.forget();
    }

    const auto& uniforms = mMostRecentLinkInfo->uniforms;

    if (index >= uniforms.size()) {
        mContext->ErrorInvalidValue("`index` (%i) must be less than %s (%zu).",
                                    index, "ACTIVE_UNIFORMS", uniforms.size());
        return nullptr;
    }

    RefPtr<WebGLActiveInfo> ret = uniforms[index]->mActiveInfo;
    return ret.forget();
}

void
WebGLProgram::GetAttachedShaders(nsTArray<RefPtr<WebGLShader>>* const out) const
{
    out->TruncateLength(0);

    if (mVertShader)
        out->AppendElement(mVertShader);

    if (mFragShader)
        out->AppendElement(mFragShader);
}

GLint
WebGLProgram::GetAttribLocation(const nsAString& userName_wide) const
{
    if (!ValidateGLSLVariableName(userName_wide, mContext, "getAttribLocation"))
        return -1;

    if (!IsLinked()) {
        mContext->ErrorInvalidOperation("getAttribLocation: `program` must be linked.");
        return -1;
    }

    const NS_LossyConvertUTF16toASCII userName(userName_wide);

    const webgl::AttribInfo* info;
    if (!LinkInfo()->FindAttrib(userName, &info))
        return -1;

    return GLint(info->mLoc);
}

static GLint
GetFragDataByUserName(const WebGLProgram* prog,
                      const nsCString& userName)
{
    nsCString mappedName;
    if (!prog->LinkInfo()->MapFragDataName(userName, &mappedName))
        return -1;

    return prog->mContext->gl->fGetFragDataLocation(prog->mGLName, mappedName.BeginReading());
}

GLint
WebGLProgram::GetFragDataLocation(const nsAString& userName_wide) const
{
    if (!ValidateGLSLVariableName(userName_wide, mContext, "getFragDataLocation"))
        return -1;

    if (!IsLinked()) {
        mContext->ErrorInvalidOperation("getFragDataLocation: `program` must be linked.");
        return -1;
    }


    const auto& gl = mContext->gl;
    gl->MakeCurrent();

    const NS_LossyConvertUTF16toASCII userName(userName_wide);
#ifdef XP_MACOSX
    if (gl->WorkAroundDriverBugs()) {
        // OSX doesn't return locs for indexed names, just the base names.
        // Indicated by failure in: conformance2/programs/gl-get-frag-data-location.html
        bool isArray;
        size_t arrayIndex;
        nsCString baseUserName;
        if (!ParseName(userName, &baseUserName, &isArray, &arrayIndex))
            return -1;

        if (arrayIndex >= mContext->mImplMaxDrawBuffers)
            return -1;

        const auto baseLoc = GetFragDataByUserName(this, baseUserName);
        const auto loc = baseLoc + GLint(arrayIndex);
        return loc;
    }
#endif
    return GetFragDataByUserName(this, userName);
}

void
WebGLProgram::GetProgramInfoLog(nsAString* const out) const
{
    CopyASCIItoUTF16(mLinkLog, *out);
}

static GLint
GetProgramiv(gl::GLContext* gl, GLuint program, GLenum pname)
{
    GLint ret = 0;
    gl->fGetProgramiv(program, pname, &ret);
    return ret;
}

JS::Value
WebGLProgram::GetProgramParameter(GLenum pname) const
{
    gl::GLContext* gl = mContext->gl;
    gl->MakeCurrent();

    if (mContext->IsWebGL2()) {
        switch (pname) {
        case LOCAL_GL_ACTIVE_UNIFORM_BLOCKS:
            if (!IsLinked())
                return JS::NumberValue(0);
            return JS::NumberValue(LinkInfo()->uniformBlocks.size());

        case LOCAL_GL_TRANSFORM_FEEDBACK_VARYINGS:
            if (!IsLinked())
                return JS::NumberValue(0);
            return JS::NumberValue(LinkInfo()->transformFeedbackVaryings.size());

        case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER_MODE:
            if (!IsLinked())
                return JS::NumberValue(LOCAL_GL_INTERLEAVED_ATTRIBS);
            return JS::NumberValue(LinkInfo()->transformFeedbackBufferMode);
       }
    }

    switch (pname) {
    case LOCAL_GL_ATTACHED_SHADERS:
        return JS::NumberValue( int(bool(mVertShader.get())) + int(bool(mFragShader)) );

    case LOCAL_GL_ACTIVE_UNIFORMS:
        if (!IsLinked())
            return JS::NumberValue(0);
        return JS::NumberValue(LinkInfo()->uniforms.size());

    case LOCAL_GL_ACTIVE_ATTRIBUTES:
        if (!IsLinked())
            return JS::NumberValue(0);
        return JS::NumberValue(LinkInfo()->attribs.size());

    case LOCAL_GL_DELETE_STATUS:
        return JS::BooleanValue(IsDeleteRequested());

    case LOCAL_GL_LINK_STATUS:
        return JS::BooleanValue(IsLinked());

    case LOCAL_GL_VALIDATE_STATUS:
#ifdef XP_MACOSX
        // See comment in ValidateProgram.
        if (gl->WorkAroundDriverBugs())
            return JS::BooleanValue(true);
#endif
        // Todo: Implement this in our code.
        return JS::BooleanValue(bool(GetProgramiv(gl, mGLName, pname)));

    default:
        mContext->ErrorInvalidEnumInfo("getProgramParameter: `pname`",
                                       pname);
        return JS::NullValue();
    }
}

GLuint
WebGLProgram::GetUniformBlockIndex(const nsAString& userName_wide) const
{
    if (!ValidateGLSLVariableName(userName_wide, mContext, "getUniformBlockIndex"))
        return LOCAL_GL_INVALID_INDEX;

    if (!IsLinked()) {
        mContext->ErrorInvalidOperation("getUniformBlockIndex: `program` must be linked.");
        return LOCAL_GL_INVALID_INDEX;
    }

    const NS_LossyConvertUTF16toASCII userName(userName_wide);

    const webgl::UniformBlockInfo* info = nullptr;
    for (const auto& cur : LinkInfo()->uniformBlocks) {
        if (cur->mUserName == userName) {
            info = cur;
            break;
        }
    }
    if (!info)
        return LOCAL_GL_INVALID_INDEX;

    const auto& mappedName = info->mMappedName;

    gl::GLContext* gl = mContext->GL();
    gl->MakeCurrent();
    return gl->fGetUniformBlockIndex(mGLName, mappedName.BeginReading());
}

void
WebGLProgram::GetActiveUniformBlockName(GLuint uniformBlockIndex, nsAString& retval) const
{
    if (!IsLinked()) {
        mContext->ErrorInvalidOperation("getActiveUniformBlockName: `program` must be linked.");
        return;
    }

    const webgl::LinkedProgramInfo* linkInfo = LinkInfo();
    GLuint uniformBlockCount = (GLuint) linkInfo->uniformBlocks.size();
    if (uniformBlockIndex >= uniformBlockCount) {
        mContext->ErrorInvalidValue("getActiveUniformBlockName: index %u invalid.", uniformBlockIndex);
        return;
    }

    const auto& blockInfo = linkInfo->uniformBlocks[uniformBlockIndex];
    retval.Assign(NS_ConvertASCIItoUTF16(blockInfo->mUserName));
}

JS::Value
WebGLProgram::GetActiveUniformBlockParam(GLuint uniformBlockIndex, GLenum pname) const
{
    if (!IsLinked()) {
        mContext->ErrorInvalidOperation("getActiveUniformBlockParameter: `program` must be linked.");
        return JS::NullValue();
    }

    const webgl::LinkedProgramInfo* linkInfo = LinkInfo();
    GLuint uniformBlockCount = (GLuint)linkInfo->uniformBlocks.size();
    if (uniformBlockIndex >= uniformBlockCount) {
        mContext->ErrorInvalidValue("getActiveUniformBlockParameter: index %u invalid.", uniformBlockIndex);
        return JS::NullValue();
    }

    gl::GLContext* gl = mContext->GL();
    GLint param = 0;

    switch (pname) {
    case LOCAL_GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER:
    case LOCAL_GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER:
        gl->fGetActiveUniformBlockiv(mGLName, uniformBlockIndex, pname, &param);
        return JS::BooleanValue(bool(param));

    case LOCAL_GL_UNIFORM_BLOCK_BINDING:
    case LOCAL_GL_UNIFORM_BLOCK_DATA_SIZE:
    case LOCAL_GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS:
        gl->fGetActiveUniformBlockiv(mGLName, uniformBlockIndex, pname, &param);
        return JS::NumberValue(param);

    default:
        MOZ_CRASH("bad `pname`.");
    }
}

JS::Value
WebGLProgram::GetActiveUniformBlockActiveUniforms(JSContext* cx, GLuint uniformBlockIndex,
                                                  ErrorResult* const out_error) const
{
    const char funcName[] = "getActiveUniformBlockParameter";
    if (!IsLinked()) {
        mContext->ErrorInvalidOperation("%s: `program` must be linked.", funcName);
        return JS::NullValue();
    }

    const webgl::LinkedProgramInfo* linkInfo = LinkInfo();
    GLuint uniformBlockCount = (GLuint)linkInfo->uniformBlocks.size();
    if (uniformBlockIndex >= uniformBlockCount) {
        mContext->ErrorInvalidValue("%s: Index %u invalid.", funcName, uniformBlockIndex);
        return JS::NullValue();
    }

    gl::GLContext* gl = mContext->GL();
    GLint activeUniformCount = 0;
    gl->fGetActiveUniformBlockiv(mGLName, uniformBlockIndex,
                                 LOCAL_GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS,
                                 &activeUniformCount);
    JS::RootedObject obj(cx, dom::Uint32Array::Create(cx, mContext, activeUniformCount,
                                                      nullptr));
    if (!obj) {
        *out_error = NS_ERROR_OUT_OF_MEMORY;
        return JS::NullValue();
    }

    dom::Uint32Array result;
    DebugOnly<bool> inited = result.Init(obj);
    MOZ_ASSERT(inited);
    result.ComputeLengthAndData();
    gl->fGetActiveUniformBlockiv(mGLName, uniformBlockIndex,
                                 LOCAL_GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES,
                                 (GLint*)result.Data());

    return JS::ObjectValue(*obj);
}

already_AddRefed<WebGLUniformLocation>
WebGLProgram::GetUniformLocation(const nsAString& userName_wide) const
{
    if (!ValidateGLSLVariableName(userName_wide, mContext, "getUniformLocation"))
        return nullptr;

    if (!IsLinked()) {
        mContext->ErrorInvalidOperation("getUniformLocation: `program` must be linked.");
        return nullptr;
    }

    const NS_LossyConvertUTF16toASCII userName(userName_wide);

    // GLES 2.0.25, Section 2.10, p35
    // If the the uniform location is an array, then the location of the first
    // element of that array can be retrieved by either using the name of the
    // uniform array, or the name of the uniform array appended with "[0]".
    nsCString mappedName;
    size_t arrayIndex;
    webgl::UniformInfo* info;
    if (!LinkInfo()->FindUniform(userName, &mappedName, &arrayIndex, &info))
        return nullptr;

    gl::GLContext* gl = mContext->GL();
    gl->MakeCurrent();

    GLint loc = gl->fGetUniformLocation(mGLName, mappedName.BeginReading());
    if (loc == -1)
        return nullptr;

    RefPtr<WebGLUniformLocation> locObj = new WebGLUniformLocation(mContext, LinkInfo(),
                                                                   info, loc, arrayIndex);
    return locObj.forget();
}

void
WebGLProgram::GetUniformIndices(const dom::Sequence<nsString>& uniformNames,
                                dom::Nullable< nsTArray<GLuint> >& retval) const
{
    const char funcName[] = "getUniformIndices";
    if (!IsLinked()) {
        mContext->ErrorInvalidOperation("%s: `program` must be linked.", funcName);
        return;
    }

    size_t count = uniformNames.Length();
    nsTArray<GLuint>& arr = retval.SetValue();

    gl::GLContext* gl = mContext->GL();
    gl->MakeCurrent();

    for (size_t i = 0; i < count; i++) {
        const NS_LossyConvertUTF16toASCII userName(uniformNames[i]);

        nsCString mappedName;
        size_t arrayIndex;
        webgl::UniformInfo* info;
        if (!LinkInfo()->FindUniform(userName, &mappedName, &arrayIndex, &info)) {
            arr.AppendElement(LOCAL_GL_INVALID_INDEX);
            continue;
        }

        const GLchar* const mappedNameBegin = mappedName.get();

        GLuint index = LOCAL_GL_INVALID_INDEX;
        gl->fGetUniformIndices(mGLName, 1, &mappedNameBegin, &index);
        arr.AppendElement(index);
    }
}

void
WebGLProgram::UniformBlockBinding(GLuint uniformBlockIndex,
                                  GLuint uniformBlockBinding) const
{
    const char funcName[] = "getActiveUniformBlockName";
    if (!IsLinked()) {
        mContext->ErrorInvalidOperation("%s: `program` must be linked.", funcName);
        return;
    }

    const auto& uniformBlocks = LinkInfo()->uniformBlocks;
    if (uniformBlockIndex >= uniformBlocks.size()) {
        mContext->ErrorInvalidValue("%s: Index %u invalid.", funcName, uniformBlockIndex);
        return;
    }
    const auto& uniformBlock = uniformBlocks[uniformBlockIndex];

    const auto& indexedBindings = mContext->mIndexedUniformBufferBindings;
    if (uniformBlockBinding >= indexedBindings.size()) {
        mContext->ErrorInvalidValue("%s: Binding %u invalid.", funcName,
                                    uniformBlockBinding);
        return;
    }
    const auto& indexedBinding = indexedBindings[uniformBlockBinding];

    ////

    gl::GLContext* gl = mContext->GL();
    gl->MakeCurrent();
    gl->fUniformBlockBinding(mGLName, uniformBlockIndex, uniformBlockBinding);

    ////

    uniformBlock->mBinding = &indexedBinding;
}

bool
WebGLProgram::ValidateForLink()
{
    if (!mVertShader || !mVertShader->IsCompiled()) {
        mLinkLog.AssignLiteral("Must have a compiled vertex shader attached.");
        return false;
    }

    if (!mFragShader || !mFragShader->IsCompiled()) {
        mLinkLog.AssignLiteral("Must have an compiled fragment shader attached.");
        return false;
    }

    if (!mFragShader->CanLinkTo(mVertShader, &mLinkLog))
        return false;

    const auto& gl = mContext->gl;

    if (gl->WorkAroundDriverBugs() &&
        mContext->mIsMesa)
    {
        // Bug 777028: Mesa can't handle more than 16 samplers per program,
        // counting each array entry.
        size_t numSamplerUniforms_upperBound = mVertShader->CalcNumSamplerUniforms() +
                                               mFragShader->CalcNumSamplerUniforms();
        if (numSamplerUniforms_upperBound > 16) {
            mLinkLog.AssignLiteral("Programs with more than 16 samplers are disallowed on"
                                   " Mesa drivers to avoid crashing.");
            return false;
        }

        // Bug 1203135: Mesa crashes internally if we exceed the reported maximum attribute count.
        if (mVertShader->NumAttributes() > mContext->MaxVertexAttribs()) {
            mLinkLog.AssignLiteral("Number of attributes exceeds Mesa's reported max"
                                   " attribute count.");
            return false;
        }
    }

    return true;
}

void
WebGLProgram::LinkProgram()
{
    const char funcName[] = "linkProgram";

    if (mNumActiveTFOs) {
        mContext->ErrorInvalidOperation("%s: Program is in-use by one or more active"
                                        " transform feedback objects.",
                                        funcName);
        return;
    }

    mContext->MakeContextCurrent();
    mContext->InvalidateBufferFetching(); // we do it early in this function
    // as some of the validation changes program state

    mLinkLog.Truncate();
    mMostRecentLinkInfo = nullptr;

    if (!ValidateForLink()) {
        mContext->GenerateWarning("%s: %s", funcName, mLinkLog.BeginReading());
        return;
    }

    // Bind the attrib locations.
    // This can't be done trivially, because we have to deal with mapped attrib names.
    for (const auto& pair : mNextLink_BoundAttribLocs) {
        const auto& name = pair.first;
        const auto& index = pair.second;

        mVertShader->BindAttribLocation(mGLName, name, index);
    }

    // Storage for transform feedback varyings before link.
    // (Work around for bug seen on nVidia drivers.)
    std::vector<std::string> scopedMappedTFVaryings;

    if (mContext->IsWebGL2()) {
        mVertShader->MapTransformFeedbackVaryings(mNextLink_TransformFeedbackVaryings,
                                                  &scopedMappedTFVaryings);

        std::vector<const char*> driverVaryings;
        driverVaryings.reserve(scopedMappedTFVaryings.size());
        for (const auto& cur : scopedMappedTFVaryings) {
            driverVaryings.push_back(cur.c_str());
        }

        mContext->gl->fTransformFeedbackVaryings(mGLName, driverVaryings.size(),
                                                 driverVaryings.data(),
                                                 mNextLink_TransformFeedbackBufferMode);
    }

    LinkAndUpdate();

    if (mMostRecentLinkInfo) {
        nsCString postLinkLog;
        if (ValidateAfterTentativeLink(&postLinkLog))
            return;

        mMostRecentLinkInfo = nullptr;
        mLinkLog = postLinkLog;
    }

    // Failed link.
    if (mContext->ShouldGenerateWarnings()) {
        // report shader/program infoLogs as warnings.
        // note that shader compilation errors can be deferred to linkProgram,
        // which is why we can't do anything in compileShader. In practice we could
        // report in compileShader the translation errors generated by ANGLE,
        // but it seems saner to keep a single way of obtaining shader infologs.
        if (!mLinkLog.IsEmpty()) {
            mContext->GenerateWarning("linkProgram: Failed to link, leaving the following"
                                      " log:\n%s\n",
                                      mLinkLog.BeginReading());
        }
    }
}

static uint8_t
NumUsedLocationsByElemType(GLenum elemType)
{
    // GLES 3.0.4 p55

    switch (elemType) {
    case LOCAL_GL_FLOAT_MAT2:
    case LOCAL_GL_FLOAT_MAT2x3:
    case LOCAL_GL_FLOAT_MAT2x4:
        return 2;

    case LOCAL_GL_FLOAT_MAT3x2:
    case LOCAL_GL_FLOAT_MAT3:
    case LOCAL_GL_FLOAT_MAT3x4:
        return 3;

    case LOCAL_GL_FLOAT_MAT4x2:
    case LOCAL_GL_FLOAT_MAT4x3:
    case LOCAL_GL_FLOAT_MAT4:
        return 4;

    default:
        return 1;
    }
}

static uint8_t
NumComponents(GLenum elemType)
{
    switch (elemType) {
    case LOCAL_GL_FLOAT:
    case LOCAL_GL_INT:
    case LOCAL_GL_UNSIGNED_INT:
    case LOCAL_GL_BOOL:
        return 1;

    case LOCAL_GL_FLOAT_VEC2:
    case LOCAL_GL_INT_VEC2:
    case LOCAL_GL_UNSIGNED_INT_VEC2:
    case LOCAL_GL_BOOL_VEC2:
        return 2;

    case LOCAL_GL_FLOAT_VEC3:
    case LOCAL_GL_INT_VEC3:
    case LOCAL_GL_UNSIGNED_INT_VEC3:
    case LOCAL_GL_BOOL_VEC3:
        return 3;

    case LOCAL_GL_FLOAT_VEC4:
    case LOCAL_GL_INT_VEC4:
    case LOCAL_GL_UNSIGNED_INT_VEC4:
    case LOCAL_GL_BOOL_VEC4:
    case LOCAL_GL_FLOAT_MAT2:
        return 4;

    case LOCAL_GL_FLOAT_MAT2x3:
    case LOCAL_GL_FLOAT_MAT3x2:
        return 6;

    case LOCAL_GL_FLOAT_MAT2x4:
    case LOCAL_GL_FLOAT_MAT4x2:
        return 8;

    case LOCAL_GL_FLOAT_MAT3:
        return 9;

    case LOCAL_GL_FLOAT_MAT3x4:
    case LOCAL_GL_FLOAT_MAT4x3:
        return 12;

    case LOCAL_GL_FLOAT_MAT4:
        return 16;

    default:
        MOZ_CRASH("`elemType`");
    }
}

bool
WebGLProgram::ValidateAfterTentativeLink(nsCString* const out_linkLog) const
{
    const auto& linkInfo = mMostRecentLinkInfo;
    const auto& gl = mContext->gl;

    // Check if the attrib name conflicting to uniform name
    for (const auto& attrib : linkInfo->attribs) {
        const auto& attribName = attrib.mActiveInfo->mBaseUserName;

        for (const auto& uniform : linkInfo->uniforms) {
            const auto& uniformName = uniform->mActiveInfo->mBaseUserName;
            if (attribName == uniformName) {
                *out_linkLog = nsPrintfCString("Attrib name conflicts with uniform name:"
                                               " %s",
                                               attribName.BeginReading());
                return false;
            }
        }
    }

    std::map<uint32_t, const webgl::AttribInfo*> attribsByLoc;
    for (const auto& attrib : linkInfo->attribs) {
        if (attrib.mLoc == -1)
            continue;

        const auto& elemType = attrib.mActiveInfo->mElemType;
        const auto numUsedLocs = NumUsedLocationsByElemType(elemType);
        for (uint32_t i = 0; i < numUsedLocs; i++) {
            const uint32_t usedLoc = attrib.mLoc + i;

            const auto res = attribsByLoc.insert({usedLoc, &attrib});
            const bool& didInsert = res.second;
            if (!didInsert) {
                const auto& aliasingName = attrib.mActiveInfo->mBaseUserName;
                const auto& itrExisting = res.first;
                const auto& existingInfo = itrExisting->second;
                const auto& existingName = existingInfo->mActiveInfo->mBaseUserName;
                *out_linkLog = nsPrintfCString("Attrib \"%s\" aliases locations used by"
                                               " attrib \"%s\".",
                                               aliasingName.BeginReading(),
                                               existingName.BeginReading());
                return false;
            }
        }
    }

    // Forbid:
    // * Unrecognized varying name
    // * Duplicate varying name
    // * Too many components for specified buffer mode
    if (mNextLink_TransformFeedbackVaryings.size()) {
        GLuint maxComponentsPerIndex = 0;
        switch (mNextLink_TransformFeedbackBufferMode) {
        case LOCAL_GL_INTERLEAVED_ATTRIBS:
            gl->GetUIntegerv(LOCAL_GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS,
                             &maxComponentsPerIndex);
            break;

        case LOCAL_GL_SEPARATE_ATTRIBS:
            gl->GetUIntegerv(LOCAL_GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS,
                             &maxComponentsPerIndex);
            break;

        default:
            MOZ_CRASH("`bufferMode`");
        }

        std::vector<size_t> componentsPerVert;
        std::set<const WebGLActiveInfo*> alreadyUsed;
        for (const auto& wideUserName : mNextLink_TransformFeedbackVaryings) {
            if (!componentsPerVert.size() ||
                mNextLink_TransformFeedbackBufferMode == LOCAL_GL_SEPARATE_ATTRIBS)
            {
                componentsPerVert.push_back(0);
            }

            ////

            const WebGLActiveInfo* curInfo = nullptr;
            for (const auto& info : linkInfo->transformFeedbackVaryings) {
                const NS_ConvertASCIItoUTF16 info_wideUserName(info->mBaseUserName);
                if (info_wideUserName == wideUserName) {
                    curInfo = info.get();
                    break;
                }
            }

            if (!curInfo) {
                const NS_LossyConvertUTF16toASCII asciiUserName(wideUserName);
                *out_linkLog = nsPrintfCString("Transform feedback varying \"%s\" not"
                                               " found.",
                                               asciiUserName.BeginReading());
                return false;
            }

            const auto insertResPair = alreadyUsed.insert(curInfo);
            const auto& didInsert = insertResPair.second;
            if (!didInsert) {
                const NS_LossyConvertUTF16toASCII asciiUserName(wideUserName);
                *out_linkLog = nsPrintfCString("Transform feedback varying \"%s\""
                                               " specified twice.",
                                               asciiUserName.BeginReading());
                return false;
            }

            ////

            size_t varyingComponents = NumComponents(curInfo->mElemType);
            varyingComponents *= curInfo->mElemCount;

            auto& totalComponentsForIndex = *(componentsPerVert.rbegin());
            totalComponentsForIndex += varyingComponents;

            if (totalComponentsForIndex > maxComponentsPerIndex) {
                const NS_LossyConvertUTF16toASCII asciiUserName(wideUserName);
                *out_linkLog = nsPrintfCString("Transform feedback varying \"%s\""
                                               " pushed `componentsForIndex` over the"
                                               " limit of %u.",
                                               asciiUserName.BeginReading(),
                                               maxComponentsPerIndex);
                return false;
            }
        }

        linkInfo->componentsPerTFVert.swap(componentsPerVert);
    }

    return true;
}

bool
WebGLProgram::UseProgram() const
{
    const char funcName[] = "useProgram";

    if (!mMostRecentLinkInfo) {
        mContext->ErrorInvalidOperation("%s: Program has not been successfully linked.",
                                        funcName);
        return false;
    }

    if (mContext->mBoundTransformFeedback &&
        mContext->mBoundTransformFeedback->mIsActive &&
        !mContext->mBoundTransformFeedback->mIsPaused)
    {
        mContext->ErrorInvalidOperation("%s: Transform feedback active and not paused.",
                                        funcName);
        return false;
    }

    mContext->MakeContextCurrent();

    mContext->InvalidateBufferFetching();

    mContext->gl->fUseProgram(mGLName);
    return true;
}

void
WebGLProgram::ValidateProgram() const
{
    mContext->MakeContextCurrent();
    gl::GLContext* gl = mContext->gl;

#ifdef XP_MACOSX
    // See bug 593867 for NVIDIA and bug 657201 for ATI. The latter is confirmed
    // with Mac OS 10.6.7.
    if (gl->WorkAroundDriverBugs()) {
        mContext->GenerateWarning("validateProgram: Implemented as a no-op on"
                                  " Mac to work around crashes.");
        return;
    }
#endif

    gl->fValidateProgram(mGLName);
}


////////////////////////////////////////////////////////////////////////////////

void
WebGLProgram::LinkAndUpdate()
{
    mMostRecentLinkInfo = nullptr;

    gl::GLContext* gl = mContext->gl;
    gl->fLinkProgram(mGLName);

    // Grab the program log.
    GLuint logLenWithNull = 0;
    gl->fGetProgramiv(mGLName, LOCAL_GL_INFO_LOG_LENGTH, (GLint*)&logLenWithNull);
    if (logLenWithNull > 1) {
        mLinkLog.SetLength(logLenWithNull - 1);
        gl->fGetProgramInfoLog(mGLName, logLenWithNull, nullptr, mLinkLog.BeginWriting());
    } else {
        mLinkLog.SetLength(0);
    }

    GLint ok = 0;
    gl->fGetProgramiv(mGLName, LOCAL_GL_LINK_STATUS, &ok);
    if (!ok)
        return;

    mMostRecentLinkInfo = QueryProgramInfo(this, gl);
    MOZ_RELEASE_ASSERT(mMostRecentLinkInfo, "GFX: most recent link info not set.");
}

bool
WebGLProgram::FindAttribUserNameByMappedName(const nsACString& mappedName,
                                             nsCString* const out_userName) const
{
    if (mVertShader->FindAttribUserNameByMappedName(mappedName, out_userName))
        return true;

    return false;
}

bool
WebGLProgram::FindVaryingByMappedName(const nsACString& mappedName,
                                              nsCString* const out_userName,
                                              bool* const out_isArray) const
{
    if (mVertShader->FindVaryingByMappedName(mappedName, out_userName, out_isArray))
        return true;

    return false;
}


bool
WebGLProgram::FindUniformByMappedName(const nsACString& mappedName,
                                      nsCString* const out_userName,
                                      bool* const out_isArray) const
{
    if (mVertShader->FindUniformByMappedName(mappedName, out_userName, out_isArray))
        return true;

    if (mFragShader->FindUniformByMappedName(mappedName, out_userName, out_isArray))
        return true;

    return false;
}

void
WebGLProgram::TransformFeedbackVaryings(const dom::Sequence<nsString>& varyings,
                                        GLenum bufferMode)
{
    const char funcName[] = "transformFeedbackVaryings";

    const auto& gl = mContext->gl;
    gl->MakeCurrent();

    switch (bufferMode) {
    case LOCAL_GL_INTERLEAVED_ATTRIBS:
        break;

    case LOCAL_GL_SEPARATE_ATTRIBS:
        {
            GLuint maxAttribs = 0;
            gl->GetUIntegerv(LOCAL_GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS,
                             &maxAttribs);
            if (varyings.Length() > maxAttribs) {
                mContext->ErrorInvalidValue("%s: Length of `varyings` exceeds %s.",
                                            funcName,
                                            "TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS");
                return;
            }
        }
        break;

    default:
        mContext->ErrorInvalidEnum("%s: Bad `bufferMode`: 0x%04x.", funcName, bufferMode);
        return;
    }

    ////

    mNextLink_TransformFeedbackVaryings.assign(varyings.Elements(),
                                               varyings.Elements() + varyings.Length());
    mNextLink_TransformFeedbackBufferMode = bufferMode;
}

already_AddRefed<WebGLActiveInfo>
WebGLProgram::GetTransformFeedbackVarying(GLuint index) const
{
    // No docs in the WebGL 2 spec for this function. Taking the language for
    // getActiveAttrib, which states that the function returns null on any error.
    if (!IsLinked()) {
        mContext->ErrorInvalidOperation("getTransformFeedbackVarying: `program` must be "
                                        "linked.");
        return nullptr;
    }

    if (index >= LinkInfo()->transformFeedbackVaryings.size()) {
        mContext->ErrorInvalidValue("getTransformFeedbackVarying: `index` is greater or "
                                    "equal to TRANSFORM_FEEDBACK_VARYINGS.");
        return nullptr;
    }

    RefPtr<WebGLActiveInfo> ret = LinkInfo()->transformFeedbackVaryings[index];
    return ret.forget();
}

bool
WebGLProgram::UnmapUniformBlockName(const nsCString& mappedName,
                                    nsCString* const out_userName) const
{
    nsCString baseMappedName;
    bool isArray;
    size_t arrayIndex;
    if (!ParseName(mappedName, &baseMappedName, &isArray, &arrayIndex))
        return false;

    nsCString baseUserName;
    if (!mVertShader->UnmapUniformBlockName(baseMappedName, &baseUserName) &&
        !mFragShader->UnmapUniformBlockName(baseMappedName, &baseUserName))
    {
        return false;
    }

    AssembleName(baseUserName, isArray, arrayIndex, out_userName);
    return true;
}

void
WebGLProgram::EnumerateFragOutputs(std::map<nsCString, const nsCString> &out_FragOutputs) const
{
    MOZ_ASSERT(mFragShader);

    mFragShader->EnumerateFragOutputs(out_FragOutputs);
}

////////////////////////////////////////////////////////////////////////////////

bool
IsBaseName(const nsCString& name)
{
    if (!name.Length())
        return true;

    return name[name.Length() - 1] != ']'; // Doesn't end in ']'.
}

bool
webgl::LinkedProgramInfo::FindAttrib(const nsCString& userName,
                                     const webgl::AttribInfo** const out) const
{
    // VS inputs cannot be arrays or structures.
    // `userName` is thus always `baseUserName`.
    for (const auto& attrib : attribs) {
        if (attrib.mActiveInfo->mBaseUserName == userName) {
            *out = &attrib;
            return true;
        }
    }

    return false;
}

bool
webgl::LinkedProgramInfo::FindUniform(const nsCString& userName,
                                      nsCString* const out_mappedName,
                                      size_t* const out_arrayIndex,
                                      webgl::UniformInfo** const out_info) const
{
    nsCString baseUserName;
    bool isArray;
    size_t arrayIndex;
    if (!ParseName(userName, &baseUserName, &isArray, &arrayIndex))
        return false;

    webgl::UniformInfo* info = nullptr;
    for (const auto& uniform : uniforms) {
        if (uniform->mActiveInfo->mBaseUserName == baseUserName) {
            info = uniform;
            break;
        }
    }
    if (!info)
        return false;

    const auto& baseMappedName = info->mActiveInfo->mBaseMappedName;
    AssembleName(baseMappedName, isArray, arrayIndex, out_mappedName);

    *out_arrayIndex = arrayIndex;
    *out_info = info;
    return true;
}

bool
webgl::LinkedProgramInfo::MapFragDataName(const nsCString& userName,
                                          nsCString* const out_mappedName) const
{
    // FS outputs can be arrays, but not structures.

    if (!fragDataMap.size()) {
        // No mappings map from validation, so just forward it.
        *out_mappedName = userName;
        return true;
    }

    nsCString baseUserName;
    bool isArray;
    size_t arrayIndex;
    if (!ParseName(userName, &baseUserName, &isArray, &arrayIndex))
        return false;

    const auto itr = fragDataMap.find(baseUserName);
    if (itr == fragDataMap.end())
        return false;

    const auto& baseMappedName = itr->second;
    AssembleName(baseMappedName, isArray, arrayIndex, out_mappedName);
    return true;
}

////////////////////////////////////////////////////////////////////////////////

JSObject*
WebGLProgram::WrapObject(JSContext* js, JS::Handle<JSObject*> givenProto)
{
    return dom::WebGLProgramBinding::Wrap(js, this, givenProto);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WebGLProgram, mVertShader, mFragShader)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLProgram, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLProgram, Release)

} // namespace mozilla
