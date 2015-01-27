/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLProgram.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"
#include "WebGLShader.h"
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
 *     `out_index`: <not written>
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
AddActiveInfo(WebGLContext* webgl, GLint elemCount, GLenum elemType, bool isArray,
              const nsACString& baseUserName, const nsACString& baseMappedName,
              std::vector<nsRefPtr<WebGLActiveInfo>>* activeInfoList,
              std::map<nsCString, const WebGLActiveInfo*>* infoLocMap)
{
    nsRefPtr<WebGLActiveInfo> info = new WebGLActiveInfo(webgl, elemCount, elemType,
                                                         isArray, baseUserName,
                                                         baseMappedName);
    activeInfoList->push_back(info);

    infoLocMap->insert(std::make_pair(info->mBaseUserName, info.get()));
}

//#define DUMP_SHADERVAR_MAPPINGS

static TemporaryRef<const webgl::LinkedProgramInfo>
QueryProgramInfo(WebGLProgram* prog, gl::GLContext* gl)
{
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

#ifdef DUMP_SHADERVAR_MAPPINGS
    printf_stderr("maxAttribLenWithNull: %d\n", maxAttribLenWithNull);
    printf_stderr("maxUniformLenWithNull: %d\n", maxUniformLenWithNull);
#endif

    // Attribs

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

        mappedName.SetLength(lengthWithoutNull);

        // Collect ActiveInfos:

        // Attribs can't be arrays, so we can skip some of the mess we have in the Uniform
        // path.
        nsDependentCString userName;
        if (!prog->FindAttribUserNameByMappedName(mappedName, &userName))
            userName.Rebind(mappedName, 0);

#ifdef DUMP_SHADERVAR_MAPPINGS
        printf_stderr("[attrib %i] %s/%s\n", i, mappedName.BeginReading(),
                      userName.BeginReading());
        printf_stderr("    lengthWithoutNull: %d\n", lengthWithoutNull);
#endif

        const bool isArray = false;
        AddActiveInfo(prog->Context(), elemCount, elemType, isArray, userName, mappedName,
                      &info->activeAttribs, &info->attribMap);

        // Collect active locations:
        GLint loc = gl->fGetAttribLocation(prog->mGLName, mappedName.BeginReading());
        if (loc == -1)
            MOZ_CRASH("Active attrib has no location.");

        info->activeAttribLocs.insert(loc);
    }

    // Uniforms

    const bool needsCheckForArrays = true;

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

        nsAutoCString baseMappedName;
        bool isArray;
        size_t arrayIndex;
        if (!ParseName(mappedName, &baseMappedName, &isArray, &arrayIndex))
            MOZ_CRASH("Failed to parse `mappedName` received from driver.");

        // Note that for good drivers, `isArray` should already be correct.
        // However, if FindUniform succeeds, it will be validator-guaranteed correct.

        nsAutoCString baseUserName;
        if (!prog->FindUniformByMappedName(baseMappedName, &baseUserName, &isArray)) {
            baseUserName = baseMappedName;

            if (needsCheckForArrays && !isArray) {
                // By GLES 3, GetUniformLocation("foo[0]") should return -1 if `foo` is
                // not an array. Our current linux Try slaves return the location of `foo`
                // anyways, though.
                std::string mappedName = baseMappedName.BeginReading();
                mappedName += "[0]";

                GLint loc = gl->fGetUniformLocation(prog->mGLName, mappedName.c_str());
                if (loc != -1)
                    isArray = true;
            }
        }

#ifdef DUMP_SHADERVAR_MAPPINGS
        printf_stderr("[uniform %i] %s/%i/%s/%s\n", i, mappedName.BeginReading(),
                      (int)isArray, baseMappedName.BeginReading(),
                      baseUserName.BeginReading());
        printf_stderr("    lengthWithoutNull: %d\n", lengthWithoutNull);
        printf_stderr("    isArray: %d\n", (int)isArray);
#endif

        AddActiveInfo(prog->Context(), elemCount, elemType, isArray, baseUserName,
                      baseMappedName, &info->activeUniforms, &info->uniformMap);
    }

    return info.forget();
}

////////////////////////////////////////////////////////////////////////////////


webgl::LinkedProgramInfo::LinkedProgramInfo(WebGLProgram* aProg)
    : prog(aProg)
    , fragDataMap(nullptr)
{ }

////////////////////////////////////////////////////////////////////////////////
// WebGLProgram

static GLuint
CreateProgram(gl::GLContext* gl)
{
    gl->MakeCurrent();
    return gl->fCreateProgram();
}

WebGLProgram::WebGLProgram(WebGLContext* webgl)
    : WebGLContextBoundObject(webgl)
    , mGLName(CreateProgram(webgl->GL()))
{
    mContext->mPrograms.insertBack(this);
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
        mContext->ErrorInvalidOperation("bindAttribLocation: Can't set the  location of a"
                                        " name that starts with 'gl_'.");
        return;
    }

    NS_LossyConvertUTF16toASCII asciiName(name);

    auto res = mBoundAttribLocs.insert(std::pair<nsCString, GLuint>(asciiName, loc));

    const bool wasInserted = res.second;
    if (!wasInserted) {
        auto itr = res.first;
        itr->second = loc;
    }
}

void
WebGLProgram::DetachShader(WebGLShader* shader)
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
        nsRefPtr<WebGLActiveInfo> ret = WebGLActiveInfo::CreateInvalid(mContext);
        return ret.forget();
    }

    const auto& activeList = mMostRecentLinkInfo->activeAttribs;

    if (index >= activeList.size()) {
        mContext->ErrorInvalidValue("`index` (%i) must be less than %s (%i).",
                                    index, "ACTIVE_ATTRIBS", activeList.size());
        return nullptr;
    }

    nsRefPtr<WebGLActiveInfo> ret =  activeList[index];
    return ret.forget();
}

already_AddRefed<WebGLActiveInfo>
WebGLProgram::GetActiveUniform(GLuint index) const
{
    if (!mMostRecentLinkInfo) {
        nsRefPtr<WebGLActiveInfo> ret = WebGLActiveInfo::CreateInvalid(mContext);
        return ret.forget();
    }

    const auto& activeList = mMostRecentLinkInfo->activeUniforms;

    if (index >= activeList.size()) {
        mContext->ErrorInvalidValue("`index` (%i) must be less than %s (%i).",
                                    index, "ACTIVE_UNIFORMS", activeList.size());
        return nullptr;
    }

    nsRefPtr<WebGLActiveInfo> ret = activeList[index];
    return ret.forget();
}

void
WebGLProgram::GetAttachedShaders(nsTArray<nsRefPtr<WebGLShader>>* const out) const
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

    const WebGLActiveInfo* info;
    if (!LinkInfo()->FindAttrib(userName, &info))
        return -1;

    const nsCString& mappedName = info->mBaseMappedName;

    gl::GLContext* gl = mContext->GL();
    gl->MakeCurrent();

    return gl->fGetAttribLocation(mGLName, mappedName.BeginReading());
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

    const NS_LossyConvertUTF16toASCII userName(userName_wide);

    nsCString mappedName;
    if (!LinkInfo()->FindFragData(userName, &mappedName))
        return -1;

    gl::GLContext* gl = mContext->GL();
    gl->MakeCurrent();

    return gl->fGetFragDataLocation(mGLName, mappedName.BeginReading());
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
            return JS::Int32Value(GetProgramiv(gl, mGLName, pname));
        }
    }


    switch (pname) {
    case LOCAL_GL_ATTACHED_SHADERS:
    case LOCAL_GL_ACTIVE_UNIFORMS:
    case LOCAL_GL_ACTIVE_ATTRIBUTES:
        return JS::Int32Value(GetProgramiv(gl, mGLName, pname));

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
        return JS::BooleanValue(bool(GetProgramiv(gl, mGLName, pname)));

    default:
        mContext->ErrorInvalidEnumInfo("getProgramParameter: `pname`",
                                       pname);
        return JS::NullValue();
    }
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

    nsDependentCString baseUserName;
    bool isArray;
    size_t arrayIndex;
    if (!ParseName(userName, &baseUserName, &isArray, &arrayIndex))
        return nullptr;

    const WebGLActiveInfo* activeInfo;
    if (!LinkInfo()->FindUniform(baseUserName, &activeInfo))
        return nullptr;

    const nsCString& baseMappedName = activeInfo->mBaseMappedName;

    nsAutoCString mappedName(baseMappedName);
    if (isArray) {
        mappedName.AppendLiteral("[");
        mappedName.AppendInt(uint32_t(arrayIndex));
        mappedName.AppendLiteral("]");
    }

    gl::GLContext* gl = mContext->GL();
    gl->MakeCurrent();

    GLint loc = gl->fGetUniformLocation(mGLName, mappedName.BeginReading());
    if (loc == -1)
        return nullptr;

    nsRefPtr<WebGLUniformLocation> locObj = new WebGLUniformLocation(mContext, LinkInfo(),
                                                                     loc, activeInfo);
    return locObj.forget();
}

bool
WebGLProgram::LinkProgram()
{
    mContext->InvalidateBufferFetching(); // we do it early in this function
    // as some of the validation below changes program state

    mLinkLog.Truncate();
    mMostRecentLinkInfo = nullptr;

    if (!mVertShader || !mVertShader->IsCompiled()) {
        mLinkLog.AssignLiteral("Must have a compiled vertex shader attached.");
        mContext->GenerateWarning("linkProgram: %s", mLinkLog.BeginReading());
        return false;
    }

    if (!mFragShader || !mFragShader->IsCompiled()) {
        mLinkLog.AssignLiteral("Must have an compiled fragment shader attached.");
        mContext->GenerateWarning("linkProgram: %s", mLinkLog.BeginReading());
        return false;
    }

    if (!mFragShader->CanLinkTo(mVertShader, &mLinkLog)) {
        mContext->GenerateWarning("linkProgram: %s", mLinkLog.BeginReading());
        return false;
    }

    gl::GLContext* gl = mContext->gl;
    gl->MakeCurrent();

    // Bug 777028: Mesa can't handle more than 16 samplers per program,
    // counting each array entry.
    size_t numSamplerUniforms_upperBound = mVertShader->CalcNumSamplerUniforms() +
                                           mFragShader->CalcNumSamplerUniforms();
    if (gl->WorkAroundDriverBugs() &&
        mContext->mIsMesa &&
        numSamplerUniforms_upperBound > 16)
    {
        mLinkLog.AssignLiteral("Programs with more than 16 samplers are disallowed on"
                               " Mesa drivers to avoid crashing.");
        mContext->GenerateWarning("linkProgram: %s", mLinkLog.BeginReading());
        return false;
    }

    // Bind the attrib locations.
    // This can't be done trivially, because we have to deal with mapped attrib names.
    for (auto itr = mBoundAttribLocs.begin(); itr != mBoundAttribLocs.end(); ++itr) {
        const nsCString& name = itr->first;
        GLuint index = itr->second;

        mVertShader->BindAttribLocation(mGLName, name, index);
    }

    if (LinkAndUpdate())
        return true;

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

    return false;
}

bool
WebGLProgram::UseProgram() const
{
    if (!mMostRecentLinkInfo) {
        mContext->ErrorInvalidOperation("useProgram: Program has not been successfully"
                                        " linked.");
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

bool
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
        return false;

    mMostRecentLinkInfo = QueryProgramInfo(this, gl);

    MOZ_ASSERT(mMostRecentLinkInfo);
    if (!mMostRecentLinkInfo)
        mLinkLog.AssignLiteral("Failed to gather program info.");

    return mMostRecentLinkInfo;
}

bool
WebGLProgram::FindAttribUserNameByMappedName(const nsACString& mappedName,
                                             nsDependentCString* const out_userName) const
{
    if (mVertShader->FindAttribUserNameByMappedName(mappedName, out_userName))
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

////////////////////////////////////////////////////////////////////////////////

JSObject*
WebGLProgram::WrapObject(JSContext* js)
{
    return dom::WebGLProgramBinding::Wrap(js, this);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WebGLProgram, mVertShader, mFragShader)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLProgram, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLProgram, Release)

} // namespace mozilla
