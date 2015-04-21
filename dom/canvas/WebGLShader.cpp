/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLShader.h"

#include "angle/ShaderLang.h"
#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "mozilla/MemoryReporting.h"
#include "nsPrintfCString.h"
#include "nsString.h"
#include "WebGLContext.h"
#include "WebGLObjectModel.h"
#include "WebGLShaderValidator.h"
#include "WebGLValidateStrings.h"

namespace mozilla {

// On success, writes to out_validator and out_translatedSource.
// On failure, writes to out_translationLog.
static bool
Translate(const nsACString& source, webgl::ShaderValidator* validator,
          nsACString* const out_translationLog, nsACString* const out_translatedSource)
{
    if (!validator->ValidateAndTranslate(source.BeginReading())) {
        validator->GetInfoLog(out_translationLog);
        return false;
    }

    // Success
    validator->GetOutput(out_translatedSource);
    return true;
}

template<size_t N>
static bool
SubstringStartsWith(const std::string& testStr, size_t offset, const char (& refStr)[N])
{
    for (size_t i = 0; i < N-1; i++) {
        if (testStr[offset + i] != refStr[i])
            return false;
    }
    return true;
}

/* On success, writes to out_translatedSource.
 * On failure, writes to out_translationLog.
 *
 * Requirements:
 *   #version is either omitted, `#version 100`, or `version 300 es`.
 */
static bool
TranslateWithoutValidation(const nsACString& sourceNS, bool isWebGL2,
                           nsACString* const out_translationLog,
                           nsACString* const out_translatedSource)
{
    std::string source = sourceNS.BeginReading();

    size_t versionStrStart = source.find("#version");
    size_t versionStrLen;
    uint32_t glesslVersion;

    if (versionStrStart != std::string::npos) {
        static const char versionStr100[] = "#version 100\n";
        static const char versionStr300es[] = "#version 300 es\n";

        if (isWebGL2 && SubstringStartsWith(source, versionStrStart, versionStr300es)) {
            glesslVersion = 300;
            versionStrLen = strlen(versionStr300es);

        } else if (SubstringStartsWith(source, versionStrStart, versionStr100)) {
            glesslVersion = 100;
            versionStrLen = strlen(versionStr100);

        } else {
            nsPrintfCString error("#version, if declared, must be %s.",
                                  isWebGL2 ? "`100` or `300 es`"
                                           : "`100`");
            *out_translationLog = error;
            return false;
        }
    } else {
        versionStrStart = 0;
        versionStrLen = 0;
        glesslVersion = 100;
    }

    std::string reversionedSource = source;
    reversionedSource.erase(versionStrStart, versionStrLen);

    switch (glesslVersion) {
    case 100:
        break;
    case 300:
        reversionedSource.insert(versionStrStart, "#version 330\n");
        break;
    default:
        MOZ_CRASH("Bad `glesslVersion`.");
    }

    out_translatedSource->Assign(reversionedSource.c_str(),
                                 reversionedSource.length());
    return true;
}

static void
GetCompilationStatusAndLog(gl::GLContext* gl, GLuint shader, bool* const out_success,
                           nsACString* const out_log)
{
    GLint compileStatus = LOCAL_GL_FALSE;
    gl->fGetShaderiv(shader, LOCAL_GL_COMPILE_STATUS, &compileStatus);

    // It's simpler if we always get the log.
    GLint lenWithNull = 0;
    gl->fGetShaderiv(shader, LOCAL_GL_INFO_LOG_LENGTH, &lenWithNull);

    if (lenWithNull > 1) {
        // SetLength takes the length without the null.
        out_log->SetLength(lenWithNull - 1);
        gl->fGetShaderInfoLog(shader, lenWithNull, nullptr, out_log->BeginWriting());
    } else {
        out_log->SetLength(0);
    }

    *out_success = (compileStatus == LOCAL_GL_TRUE);
}

////////////////////////////////////////////////////////////////////////////////

static GLuint
CreateShader(gl::GLContext* gl, GLenum type)
{
    gl->MakeCurrent();
    return gl->fCreateShader(type);
}

WebGLShader::WebGLShader(WebGLContext* webgl, GLenum type)
    : WebGLContextBoundObject(webgl)
    , mGLName(CreateShader(webgl->GL(), type))
    , mType(type)
{
    mContext->mShaders.insertBack(this);
}

WebGLShader::~WebGLShader()
{
    DeleteOnce();
}

void
WebGLShader::ShaderSource(const nsAString& source)
{
    StripComments stripComments(source);
    const nsAString& cleanSource = Substring(stripComments.result().Elements(),
                                             stripComments.length());
    if (!ValidateGLSLString(cleanSource, mContext, "shaderSource"))
        return;

    // We checked that the source stripped of comments is in the
    // 7-bit ASCII range, so we can skip the NS_IsAscii() check.
    NS_LossyConvertUTF16toASCII sourceCString(cleanSource);

    if (mContext->gl->WorkAroundDriverBugs()) {
        const size_t maxSourceLength = 0x3ffff;
        if (sourceCString.Length() > maxSourceLength) {
            mContext->ErrorInvalidValue("shaderSource: Source has more than %d"
                                        " characters. (Driver workaround)",
                                        maxSourceLength);
            return;
        }
    }

    // HACK - dump shader source
    {
/*
        printf_stderr("//-*- glsl -*-\n");
        // Wow - Roll Your Own For Each Lines because printf_stderr has a hard-coded internal size, so long strings are truncated.
        const nsString& src = shader->Source();
        int32_t start = 0;
        int32_t end = src.Find("\n", false, start, -1);
        while (end > -1) {
            printf_stderr("%s\n", NS_ConvertUTF16toUTF8(nsDependentSubstring(src, start, end - start)).get());
            start = end + 1;
            end = src.Find("\n", false, start, -1);
        }
        printf_stderr("//\n");
*/
    }
    // HACK

    mSource = source;
    mCleanSource = sourceCString;
}

void
WebGLShader::CompileShader()
{
    mValidator = nullptr;
    mTranslationSuccessful = false;
    mCompilationSuccessful = false;

    gl::GLContext* gl = mContext->gl;

    mValidator.reset(mContext->CreateShaderValidator(mType));

    bool success;
    if (mValidator) {
        success = Translate(mCleanSource, mValidator.get(), &mValidationLog,
                            &mTranslatedSource);
    } else {
        success = TranslateWithoutValidation(mCleanSource, mContext->IsWebGL2(),
                                             &mValidationLog, &mTranslatedSource);
    }

    if (!success)
        return;

    mTranslationSuccessful = true;

    gl->MakeCurrent();

    const char* const parts[] = {
        mTranslatedSource.BeginReading()
    };
    gl->fShaderSource(mGLName, ArrayLength(parts), parts, nullptr);

    gl->fCompileShader(mGLName);

    GetCompilationStatusAndLog(gl, mGLName, &mCompilationSuccessful, &mCompilationLog);
}

void
WebGLShader::GetShaderInfoLog(nsAString* out) const
{
    const nsCString& log = !mTranslationSuccessful ? mValidationLog
                                                   : mCompilationLog;
    CopyASCIItoUTF16(log, *out);
}

JS::Value
WebGLShader::GetShaderParameter(GLenum pname) const
{
    switch (pname) {
    case LOCAL_GL_SHADER_TYPE:
        return JS::NumberValue(mType);

    case LOCAL_GL_DELETE_STATUS:
        return JS::BooleanValue(IsDeleteRequested());

    case LOCAL_GL_COMPILE_STATUS:
        return JS::BooleanValue(mCompilationSuccessful);

    default:
        mContext->ErrorInvalidEnumInfo("getShaderParameter: `pname`", pname);
        return JS::NullValue();
    }
}

void
WebGLShader::GetShaderSource(nsAString* out) const
{
    out->SetIsVoid(false);
    *out = mSource;
}

void
WebGLShader::GetShaderTranslatedSource(nsAString* out) const
{
    if (!mCompilationSuccessful) {
        mContext->ErrorInvalidOperation("getShaderTranslatedSource: Shader has"
                                        " not been successfully compiled.");
        return;
    }

    out->SetIsVoid(false);
    CopyASCIItoUTF16(mTranslatedSource, *out);
}

////////////////////////////////////////////////////////////////////////////////

bool
WebGLShader::CanLinkTo(const WebGLShader* prev, nsCString* const out_log) const
{
    if (!mValidator)
        return true;

    return mValidator->CanLinkTo(prev->mValidator.get(), out_log);
}

size_t
WebGLShader::CalcNumSamplerUniforms() const
{
    if (mValidator)
        return mValidator->CalcNumSamplerUniforms();

    // TODO
    return 0;
}

void
WebGLShader::BindAttribLocation(GLuint prog, const nsCString& userName,
                                GLuint index) const
{
    std::string userNameStr(userName.BeginReading());

    const std::string* mappedNameStr = &userNameStr;
    if (mValidator)
        mValidator->FindAttribMappedNameByUserName(userNameStr, &mappedNameStr);

    mContext->gl->fBindAttribLocation(prog, index, mappedNameStr->c_str());
}

bool
WebGLShader::FindAttribUserNameByMappedName(const nsACString& mappedName,
                                            nsDependentCString* const out_userName) const
{
    if (!mValidator)
        return false;

    const std::string mappedNameStr(mappedName.BeginReading());
    const std::string* userNameStr;
    if (!mValidator->FindAttribUserNameByMappedName(mappedNameStr, &userNameStr))
        return false;

    out_userName->Rebind(userNameStr->c_str());
    return true;
}

bool
WebGLShader::FindUniformByMappedName(const nsACString& mappedName,
                                     nsCString* const out_userName,
                                     bool* const out_isArray) const
{
    if (!mValidator)
        return false;

    const std::string mappedNameStr(mappedName.BeginReading(), mappedName.Length());
    std::string userNameStr;
    if (!mValidator->FindUniformByMappedName(mappedNameStr, &userNameStr, out_isArray))
        return false;

    *out_userName = userNameStr.c_str();
    return true;
}

bool
WebGLShader::FindUniformBlockByMappedName(const nsACString& mappedName,
                                          nsCString* const out_userName,
                                          bool* const out_isArray) const
{
    // TODO: Extract block information from shader validator.
    return false;
}

////////////////////////////////////////////////////////////////////////////////
// Boilerplate

JSObject*
WebGLShader::WrapObject(JSContext* js, JS::Handle<JSObject*> aGivenProto)
{
    return dom::WebGLShaderBinding::Wrap(js, this, aGivenProto);
}

size_t
WebGLShader::SizeOfIncludingThis(MallocSizeOf mallocSizeOf) const
{
    size_t validatorSize = mValidator ? mallocSizeOf(mValidator.get())
                                      : 0;
    return mallocSizeOf(this) +
           mSource.SizeOfExcludingThisIfUnshared(mallocSizeOf) +
           mCleanSource.SizeOfExcludingThisIfUnshared(mallocSizeOf) +
           validatorSize +
           mValidationLog.SizeOfExcludingThisIfUnshared(mallocSizeOf) +
           mTranslatedSource.SizeOfExcludingThisIfUnshared(mallocSizeOf) +
           mCompilationLog.SizeOfExcludingThisIfUnshared(mallocSizeOf);
}

void
WebGLShader::Delete()
{
    gl::GLContext* gl = mContext->GL();

    gl->MakeCurrent();
    gl->fDeleteShader(mGLName);

    LinkedListElement<WebGLShader>::removeFrom(mContext->mShaders);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WebGLShader)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLShader, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLShader, Release)

} // namespace mozilla
