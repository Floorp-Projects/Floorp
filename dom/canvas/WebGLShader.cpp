/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLShader.h"

#include "GLSLANG/ShaderLang.h"
#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "mozilla/MemoryReporting.h"
#include "nsPrintfCString.h"
#include "nsString.h"
#include "prenv.h"
#include "WebGLContext.h"
#include "WebGLObjectModel.h"
#include "WebGLShaderValidator.h"
#include "WebGLValidateStrings.h"

namespace mozilla {

static void PrintLongString(const char* const begin, const size_t len) {
  // Wow - Roll Your Own Foreach-Lines because printf_stderr has a hard-coded
  // internal size, so long strings are truncated.

  const size_t chunkSize = 1000;
  const UniqueBuffer buf(moz_xmalloc(chunkSize + 1));  // +1 for null-term
  const auto bufBegin = (char*)buf.get();
  bufBegin[chunkSize] = '\0';

  auto chunkBegin = begin;
  const auto end = begin + len;
  while (chunkBegin + chunkSize < end) {
    memcpy(bufBegin, chunkBegin, chunkSize);
    printf_stderr("%s", bufBegin);
    chunkBegin += chunkSize;
  }
  printf_stderr("%s", chunkBegin);
}

template <size_t N>
static bool SubstringStartsWith(const std::string& testStr, size_t offset,
                                const char (&refStr)[N]) {
  for (size_t i = 0; i < N - 1; i++) {
    if (testStr[offset + i] != refStr[i]) return false;
  }
  return true;
}

static void GetCompilationStatusAndLog(gl::GLContext* gl, GLuint shader,
                                       bool* const out_success,
                                       nsACString* const out_log) {
  GLint compileStatus = LOCAL_GL_FALSE;
  gl->fGetShaderiv(shader, LOCAL_GL_COMPILE_STATUS, &compileStatus);

  // It's simpler if we always get the log.
  GLint lenWithNull = 0;
  gl->fGetShaderiv(shader, LOCAL_GL_INFO_LOG_LENGTH, &lenWithNull);

  if (lenWithNull > 1) {
    // SetLength takes the length without the null.
    out_log->SetLength(lenWithNull - 1);
    gl->fGetShaderInfoLog(shader, lenWithNull, nullptr,
                          out_log->BeginWriting());
  } else {
    out_log->SetLength(0);
  }

  *out_success = (compileStatus == LOCAL_GL_TRUE);
}

////////////////////////////////////////////////////////////////////////////////

WebGLShader::WebGLShader(WebGLContext* webgl, GLenum type)
    : WebGLRefCountedObject(webgl),
      mGLName(webgl->gl->fCreateShader(type)),
      mType(type) {
  mCompileResults = std::make_unique<webgl::ShaderValidatorResults>();
  mContext->mShaders.insertBack(this);
}

WebGLShader::~WebGLShader() { DeleteOnce(); }

void WebGLShader::ShaderSource(const nsAString& source) {
  nsString sourceWithoutComments;
  if (!TruncateComments(source, &sourceWithoutComments)) {
    mContext->ErrorOutOfMemory("Failed to alloc for empting comment contents.");
    return;
  }

  if (!ValidateGLSLPreprocString(mContext, sourceWithoutComments)) return;

  // We checked that the source stripped of comments is in the
  // 7-bit ASCII range, so we can skip the NS_IsAscii() check.
  const NS_LossyConvertUTF16toASCII cleanSource(sourceWithoutComments);

  mSource = source;
  mCleanSource = cleanSource;
}

void WebGLShader::CompileShader() {
  mCompilationSuccessful = false;

  gl::GLContext* gl = mContext->gl;

  static const bool kDumpShaders = PR_GetEnv("MOZ_WEBGL_DUMP_SHADERS");
  if (MOZ_UNLIKELY(kDumpShaders)) {
    printf_stderr("==== begin MOZ_WEBGL_DUMP_SHADERS ====\n");
    PrintLongString(mCleanSource.BeginReading(), mCleanSource.Length());
  }

  {
    const auto validator = mContext->CreateShaderValidator(mType);
    MOZ_ASSERT(validator);

    mCompileResults =
        validator->ValidateAndTranslate(mCleanSource.BeginReading());
  }

  mCompilationLog = mCompileResults->mInfoLog.c_str();
  const auto& success = mCompileResults->mValid;

  if (MOZ_UNLIKELY(kDumpShaders)) {
    printf_stderr("\n==== \\/ \\/ \\/ ====\n");
    if (success) {
      const auto& translated = mCompileResults->mObjectCode;
      PrintLongString(translated.data(), translated.length());
    } else {
      printf_stderr("Validation failed:\n%s",
                    mCompileResults->mInfoLog.c_str());
    }
    printf_stderr("\n==== end ====\n");
  }

  if (!success) return;

  const std::array<const char*, 1> parts = {
      mCompileResults->mObjectCode.c_str()};
  gl->fShaderSource(mGLName, parts.size(), parts.data(), nullptr);

  gl->fCompileShader(mGLName);

  GetCompilationStatusAndLog(gl, mGLName, &mCompilationSuccessful,
                             &mCompilationLog);
}

void WebGLShader::GetShaderInfoLog(nsAString* const out) const {
  CopyASCIItoUTF16(mCompilationLog, *out);
}

MaybeWebGLVariant WebGLShader::GetShaderParameter(GLenum pname) const {
  switch (pname) {
    case LOCAL_GL_SHADER_TYPE:
      return AsSomeVariant(mType);

    case LOCAL_GL_DELETE_STATUS:
      return AsSomeVariant(IsDeleteRequested());

    case LOCAL_GL_COMPILE_STATUS:
      return AsSomeVariant(mCompilationSuccessful);

    default:
      mContext->ErrorInvalidEnumInfo("getShaderParameter: `pname`", pname);
      return Nothing();
  }
}

nsString WebGLShader::GetShaderSource() const { return mSource; }

void WebGLShader::GetShaderTranslatedSource(nsAString* out) const {
  out->SetIsVoid(false);
  const auto& wrapper =
      nsDependentCString(mCompileResults->mObjectCode.c_str());
  CopyASCIItoUTF16(wrapper, *out);
}

////////////////////////////////////////////////////////////////////////////////

size_t WebGLShader::CalcNumSamplerUniforms() const {
  size_t accum = 0;
  for (const auto& cur : mCompileResults->mUniforms) {
    const auto& type = cur.type;
    if (type == LOCAL_GL_SAMPLER_2D || type == LOCAL_GL_SAMPLER_CUBE) {
      accum += cur.getArraySizeProduct();
    }
  }
  return accum;
}

size_t WebGLShader::NumAttributes() const {
  return mCompileResults->mAttributes.size();
}

void WebGLShader::BindAttribLocation(GLuint prog, const std::string& userName,
                                     GLuint index) const {
  for (const auto& attrib : mCompileResults->mAttributes) {
    if (attrib.name == userName) {
      mContext->gl->fBindAttribLocation(prog, index, attrib.mappedName.c_str());
      return;
    }
  }
}

bool WebGLShader::FindAttribUserNameByMappedName(
    const nsACString& mappedName, nsCString* const out_userName) const {
  const std::string mappedNameStr(mappedName.BeginReading());

  for (const auto& cur : mCompileResults->mAttributes) {
    if (cur.mappedName == mappedNameStr) {
      *out_userName = cur.name.c_str();
      return true;
    }
  }
  return false;
}

bool WebGLShader::FindVaryingByMappedName(const nsACString& mappedName,
                                          nsCString* const out_userName,
                                          bool* const out_isArray) const {
  const std::string mappedNameStr(mappedName.BeginReading());

  for (const auto& cur : mCompileResults->mVaryings) {
    const sh::ShaderVariable* found;
    std::string userName;
    if (!cur.findInfoByMappedName(mappedNameStr, &found, &userName)) continue;

    *out_userName = userName.c_str();
    *out_isArray = found->isArray();
    return true;
  }

  return false;
}

bool WebGLShader::FindUniformByMappedName(const nsACString& mappedName,
                                          nsCString* const out_userName,
                                          bool* const out_isArray) const {
  const std::string mappedNameStr(mappedName.BeginReading(),
                                  mappedName.Length());
  std::string userNameStr;
  if (!mCompileResults->FindUniformByMappedName(mappedNameStr, &userNameStr,
                                                out_isArray))
    return false;

  *out_userName = userNameStr.c_str();
  return true;
}

bool WebGLShader::UnmapUniformBlockName(
    const nsACString& baseMappedName, nsCString* const out_baseUserName) const {
  for (const auto& interface : mCompileResults->mInterfaceBlocks) {
    const nsDependentCString interfaceMappedName(interface.mappedName.data(),
                                                 interface.mappedName.size());
    if (baseMappedName == interfaceMappedName) {
      *out_baseUserName = interface.name.data();
      return true;
    }
  }

  return false;
}

void WebGLShader::MapTransformFeedbackVaryings(
    const std::vector<nsString>& varyings,
    std::vector<std::string>* out_mappedVaryings) const {
  MOZ_ASSERT(mType == LOCAL_GL_VERTEX_SHADER);
  MOZ_ASSERT(out_mappedVaryings);

  out_mappedVaryings->clear();
  out_mappedVaryings->reserve(varyings.size());

  const auto& shaderVaryings = mCompileResults->mVaryings;

  for (const auto& wideUserName : varyings) {
    const NS_LossyConvertUTF16toASCII mozUserName(
        wideUserName);  // Don't validate here.
    const std::string userName(mozUserName.BeginReading(),
                               mozUserName.Length());
    const auto* mappedName = &userName;
    for (const auto& shaderVarying : shaderVaryings) {
      if (shaderVarying.name == userName) {
        mappedName = &shaderVarying.mappedName;
        break;
      }
    }
    out_mappedVaryings->push_back(*mappedName);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Boilerplate

size_t WebGLShader::SizeOfIncludingThis(MallocSizeOf mallocSizeOf) const {
  return mallocSizeOf(this) +
         mSource.SizeOfExcludingThisIfUnshared(mallocSizeOf) +
         mCleanSource.SizeOfExcludingThisIfUnshared(mallocSizeOf) +
         mCompileResults->SizeOfIncludingThis(mallocSizeOf) +
         mCompilationLog.SizeOfExcludingThisIfUnshared(mallocSizeOf);
}

void WebGLShader::Delete() {
  gl::GLContext* gl = mContext->GL();

  gl->fDeleteShader(mGLName);

  LinkedListElement<WebGLShader>::removeFrom(mContext->mShaders);
}

}  // namespace mozilla
