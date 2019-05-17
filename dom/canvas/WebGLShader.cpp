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

// On success, writes to out_validator and out_translatedSource.
// On failure, writes to out_translationLog.
static bool Translate(const nsACString& source,
                      webgl::ShaderValidator* validator,
                      nsACString* const out_translationLog,
                      nsACString* const out_translatedSource) {
  if (!validator->ValidateAndTranslate(source.BeginReading())) {
    const std::string& log = sh::GetInfoLog(validator->mHandle);
    out_translationLog->Assign(log.data(), log.length());
    return false;
  }

  // Success
  const std::string& output = sh::GetObjectCode(validator->mHandle);
  out_translatedSource->Assign(output.data(), output.length());

  return true;
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
      mType(type),
      mTranslationSuccessful(false),
      mCompilationSuccessful(false) {
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
  mValidator = nullptr;
  mTranslationSuccessful = false;
  mCompilationSuccessful = false;

  gl::GLContext* gl = mContext->gl;

  mValidator.reset(mContext->CreateShaderValidator(mType));
  MOZ_ASSERT(mValidator);

  static const bool kDumpShaders = PR_GetEnv("MOZ_WEBGL_DUMP_SHADERS");
  if (MOZ_UNLIKELY(kDumpShaders)) {
    printf_stderr("==== begin MOZ_WEBGL_DUMP_SHADERS ====\n");
    PrintLongString(mCleanSource.BeginReading(), mCleanSource.Length());
  }

  const bool success = Translate(mCleanSource, mValidator.get(), &mValidationLog,
                        &mTranslatedSource);

  if (MOZ_UNLIKELY(kDumpShaders)) {
    printf_stderr("\n==== \\/ \\/ \\/ ====\n");
    if (success) {
      PrintLongString(mTranslatedSource.BeginReading(),
                      mTranslatedSource.Length());
    } else {
      printf_stderr("Validation failed:\n%s", mValidationLog.BeginReading());
    }
    printf_stderr("\n==== end ====\n");
  }

  if (!success) return;

  mTranslationSuccessful = true;

  const char* const parts[] = {mTranslatedSource.BeginReading()};
  gl->fShaderSource(mGLName, ArrayLength(parts), parts, nullptr);

  gl->fCompileShader(mGLName);

  GetCompilationStatusAndLog(gl, mGLName, &mCompilationSuccessful,
                             &mCompilationLog);
}

void WebGLShader::GetShaderInfoLog(nsAString* out) const {
  const nsCString& log =
      !mTranslationSuccessful ? mValidationLog : mCompilationLog;
  CopyASCIItoUTF16(log, *out);
}

JS::Value WebGLShader::GetShaderParameter(GLenum pname) const {
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

void WebGLShader::GetShaderSource(nsAString* out) const {
  out->SetIsVoid(false);
  *out = mSource;
}

void WebGLShader::GetShaderTranslatedSource(nsAString* out) const {
  out->SetIsVoid(false);
  CopyASCIItoUTF16(mTranslatedSource, *out);
}

////////////////////////////////////////////////////////////////////////////////

bool WebGLShader::CanLinkTo(const WebGLShader* prev,
                            nsCString* const out_log) const {
  return mValidator->CanLinkTo(prev->mValidator.get(), out_log);
}

size_t WebGLShader::CalcNumSamplerUniforms() const {
  const auto& uniforms = *sh::GetUniforms(mValidator->mHandle);
  size_t accum = 0;
  for (const auto& cur : uniforms) {
    const auto& type = cur.type;
    if (type == LOCAL_GL_SAMPLER_2D || type == LOCAL_GL_SAMPLER_CUBE) {
      accum += cur.getArraySizeProduct();
    }
  }
  return accum;
}

size_t WebGLShader::NumAttributes() const {
  return sh::GetAttributes(mValidator->mHandle)->size();
}

void WebGLShader::BindAttribLocation(GLuint prog, const std::string& userName,
                                     GLuint index) const {
  const auto& attribs = *sh::GetAttributes(mValidator->mHandle);
  for (const auto& attrib : attribs) {
    if (attrib.name == userName) {
      mContext->gl->fBindAttribLocation(prog, index, attrib.mappedName.c_str());
      return;
    }
  }
}

bool WebGLShader::FindAttribUserNameByMappedName(
    const nsACString& mappedName, nsCString* const out_userName) const {
  const std::string mappedNameStr(mappedName.BeginReading());

  const auto& attribs = *sh::GetAttributes(mValidator->mHandle);
  for (const auto& cur : attribs) {
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

  const auto& varyings = *sh::GetVaryings(mValidator->mHandle);
  for (const auto& cur : varyings) {
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
  if (!mValidator->FindUniformByMappedName(mappedNameStr, &userNameStr,
                                           out_isArray))
    return false;

  *out_userName = userNameStr.c_str();
  return true;
}

bool WebGLShader::UnmapUniformBlockName(
    const nsACString& baseMappedName, nsCString* const out_baseUserName) const {
  const auto& interfaces = *sh::GetInterfaceBlocks(mValidator->mHandle);
  for (const auto& interface : interfaces) {
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

  const auto& shaderVaryings = *sh::GetVaryings(mValidator->mHandle);

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

JSObject* WebGLShader::WrapObject(JSContext* js,
                                  JS::Handle<JSObject*> givenProto) {
  return dom::WebGLShader_Binding::Wrap(js, this, givenProto);
}

size_t WebGLShader::SizeOfIncludingThis(MallocSizeOf mallocSizeOf) const {
  size_t validatorSize = mValidator ? mallocSizeOf(mValidator.get()) : 0;
  return mallocSizeOf(this) +
         mSource.SizeOfExcludingThisIfUnshared(mallocSizeOf) +
         mCleanSource.SizeOfExcludingThisIfUnshared(mallocSizeOf) +
         validatorSize +
         mValidationLog.SizeOfExcludingThisIfUnshared(mallocSizeOf) +
         mTranslatedSource.SizeOfExcludingThisIfUnshared(mallocSizeOf) +
         mCompilationLog.SizeOfExcludingThisIfUnshared(mallocSizeOf);
}

void WebGLShader::Delete() {
  gl::GLContext* gl = mContext->GL();

  gl->fDeleteShader(mGLName);

  LinkedListElement<WebGLShader>::removeFrom(mContext->mShaders);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WebGLShader)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLShader, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLShader, Release)

}  // namespace mozilla
