/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_SHADER_H_
#define WEBGL_SHADER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "GLDefs.h"
#include "mozilla/LinkedList.h"
#include "mozilla/MemoryReporting.h"
#include "nsString.h"
#include "nsWrapperCache.h"

#include "WebGLObjectModel.h"

namespace mozilla {

namespace webgl {
class ShaderValidatorResults;
}  // namespace webgl

class WebGLShader final : public WebGLRefCountedObject<WebGLShader>,
                          public LinkedListElement<WebGLShader> {
  friend class WebGLContext;
  friend class WebGLProgram;

 public:
  WebGLShader(WebGLContext* webgl, GLenum type);

 protected:
  ~WebGLShader();

 public:
  // GL funcs
  void CompileShader();
  MaybeWebGLVariant GetShaderParameter(GLenum pname) const;
  nsString GetShaderInfoLog() const;
  nsString GetShaderSource() const;
  nsString GetShaderTranslatedSource() const;
  void ShaderSource(const nsAString& source);

  // Util funcs
  size_t CalcNumSamplerUniforms() const;
  size_t NumAttributes() const;
  bool FindAttribUserNameByMappedName(const nsACString& mappedName,
                                      nsCString* const out_userName) const;
  bool FindVaryingByMappedName(const nsACString& mappedName,
                               nsCString* const out_userName,
                               bool* const out_isArray) const;
  bool FindUniformByMappedName(const nsACString& mappedName,
                               nsCString* const out_userName,
                               bool* const out_isArray) const;
  bool UnmapUniformBlockName(const nsACString& baseMappedName,
                             nsCString* const out_baseUserName) const;

  bool IsCompiled() const { return mCompilationSuccessful; }
  const auto& CompileResults() const { return mCompileResults; }

 private:
  void BindAttribLocation(GLuint prog, const std::string& userName,
                          GLuint index) const;
  void MapTransformFeedbackVaryings(
      const std::vector<nsString>& varyings,
      std::vector<std::string>* out_mappedVaryings) const;

 public:
  // Other funcs
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
  void Delete();

  NS_INLINE_DECL_REFCOUNTING(WebGLShader)

 public:
  const GLuint mGLName;
  const GLenum mType;

 protected:
  nsString mSource;
  nsCString mCleanSource;

  std::unique_ptr<const webgl::ShaderValidatorResults>
      mCompileResults;  // Never null.
  bool mCompilationSuccessful = false;
  nsCString mCompilationLog;
};

}  // namespace mozilla

#endif  // WEBGL_SHADER_H_
