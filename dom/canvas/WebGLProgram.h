/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_PROGRAM_H_
#define WEBGL_PROGRAM_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "mozilla/LinkedList.h"
#include "mozilla/RefPtr.h"
#include "mozilla/WeakPtr.h"
#include "nsString.h"
#include "nsWrapperCache.h"

#include "CacheInvalidator.h"
#include "WebGLActiveInfo.h"
#include "WebGLContext.h"
#include "WebGLObjectModel.h"

namespace mozilla {
class ErrorResult;
class WebGLActiveInfo;
class WebGLContext;
class WebGLProgram;
class WebGLShader;
class WebGLUniformLocation;

namespace dom {
template <typename>
struct Nullable;
class OwningUnsignedLongOrUint32ArrayOrBoolean;
template <typename>
class Sequence;
}  // namespace dom

namespace webgl {

enum class TextureBaseType : uint8_t;

struct AttribInfo final {
  const WebGLActiveInfo mActiveInfo;
  const GLint mLoc;  // -1 for active built-ins
};

struct UniformInfo final {
  typedef decltype(WebGLContext::mBound2DTextures) TexListT;

  const WebGLActiveInfo mActiveInfo;
  const TexListT* const mSamplerTexList;
  const webgl::TextureBaseType mTexBaseType;
  const bool mIsShadowSampler;

  std::vector<uint32_t> mSamplerValues;

 protected:
  static const TexListT* GetTexList(const WebGLContext* aWebGL,
                                    WebGLActiveInfo* activeInfo);

 public:
  explicit UniformInfo(const WebGLContext* aWebGL, WebGLActiveInfo& activeInfo);
};

struct UniformBlockInfo final {
  const nsCString mUserName;
  const nsCString mMappedName;
  const uint32_t mDataSize;

  const IndexedBufferBinding* mBinding;

  UniformBlockInfo(WebGLContext* webgl, const nsACString& userName,
                   const nsACString& mappedName, uint32_t dataSize)
      : mUserName(userName),
        mMappedName(mappedName),
        mDataSize(dataSize),
        mBinding(&webgl->mIndexedUniformBufferBindings[0]) {}
};

struct FragOutputInfo final {
  const uint8_t loc;
  const nsCString userName;
  const nsCString mappedName;
  const TextureBaseType baseType;
};

struct CachedDrawFetchLimits final {
  uint64_t maxVerts = 0;
  uint64_t maxInstances = 0;
  std::vector<BufferAndIndex> usedBuffers;
};

struct LinkedProgramInfo final : public RefCounted<LinkedProgramInfo>,
                                 public SupportsWeakPtr<LinkedProgramInfo>,
                                 public CacheInvalidator {
  friend class mozilla::WebGLProgram;

  MOZ_DECLARE_REFCOUNTED_TYPENAME(LinkedProgramInfo)
  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(LinkedProgramInfo)

  //////

  WebGLProgram* const prog;
  const GLenum transformFeedbackBufferMode;

  std::vector<AttribInfo> attribs;
  std::vector<UniformInfo*> uniforms;            // Owns its contents.
  std::vector<UniformBlockInfo*> uniformBlocks;  // Owns its contents.
  std::vector<WebGLActiveInfo> transformFeedbackVaryings;
  std::unordered_map<uint8_t, const FragOutputInfo> fragOutputs;
  uint8_t zLayerCount = 1;

  // Needed for draw call validation.
  std::vector<UniformInfo*> uniformSamplers;

  mutable std::vector<size_t> componentsPerTFVert;

  bool attrib0Active;

  //////

  mutable CacheWeakMap<const WebGLVertexArray*, CachedDrawFetchLimits>
      mDrawFetchCache;

  const CachedDrawFetchLimits* GetDrawFetchLimits() const;

  //////

  explicit LinkedProgramInfo(WebGLProgram* prog);
  ~LinkedProgramInfo();

  bool FindAttrib(const nsCString& userName,
                  const AttribInfo** const out_info) const;
  bool FindUniform(const nsCString& userName, nsCString* const out_mappedName,
                   size_t* const out_arrayIndex,
                   UniformInfo** const out_info) const;
};

}  // namespace webgl

class WebGLProgram final : public WebGLRefCountedObject<WebGLProgram>,
                           public LinkedListElement<WebGLProgram> {
  friend class WebGLTransformFeedback;
  friend struct webgl::LinkedProgramInfo;

 public:
  NS_INLINE_DECL_REFCOUNTING(WebGLProgram)

  explicit WebGLProgram(WebGLContext* webgl);

  void Delete();

  // GL funcs
  void AttachShader(WebGLShader* shader);
  void BindAttribLocation(GLuint index, const nsAString& name);
  void DetachShader(const WebGLShader* shader);
  Maybe<WebGLActiveInfo> GetActiveAttrib(GLuint index) const;
  Maybe<WebGLActiveInfo> GetActiveUniform(GLuint index) const;
  MaybeAttachedShaders GetAttachedShaders() const;
  GLint GetAttribLocation(const nsAString& name) const;
  GLint GetFragDataLocation(const nsAString& name) const;
  nsString GetProgramInfoLog() const;
  MaybeWebGLVariant GetProgramParameter(GLenum pname) const;
  GLuint GetUniformBlockIndex(const nsAString& name) const;
  nsString GetActiveUniformBlockName(GLuint uniformBlockIndex) const;
  MaybeWebGLVariant GetActiveUniformBlockParam(GLuint uniformBlockIndex,
                                               GLenum pname) const;
  MaybeWebGLVariant GetActiveUniformBlockActiveUniforms(
      GLuint uniformBlockIndex) const;
  already_AddRefed<WebGLUniformLocation> GetUniformLocation(
      const nsAString& name) const;
  MaybeWebGLVariant GetUniformIndices(
      const nsTArray<nsString>& uniformNames) const;
  void UniformBlockBinding(GLuint uniformBlockIndex,
                           GLuint uniformBlockBinding) const;

  void LinkProgram();
  bool UseProgram() const;
  void ValidateProgram() const;

  ////////////////

  bool FindAttribUserNameByMappedName(const nsACString& mappedName,
                                      nsCString* const out_userName) const;
  bool FindVaryingByMappedName(const nsACString& mappedName,
                               nsCString* const out_userName,
                               bool* const out_isArray) const;
  bool FindUniformByMappedName(const nsACString& mappedName,
                               nsCString* const out_userName,
                               bool* const out_isArray) const;
  bool UnmapUniformBlockName(const nsCString& mappedName,
                             nsCString* const out_userName) const;

  void TransformFeedbackVaryings(const nsTArray<nsString>& varyings,
                                 GLenum bufferMode);
  Maybe<WebGLActiveInfo> GetTransformFeedbackVarying(GLuint index) const;

  void EnumerateFragOutputs(
      std::map<nsCString, const nsCString>& out_FragOutputs) const;

  bool IsLinked() const { return mMostRecentLinkInfo; }

  const webgl::LinkedProgramInfo* LinkInfo() const {
    return mMostRecentLinkInfo.get();
  }

  const auto& VertShader() const { return mVertShader; }
  const auto& FragShader() const { return mFragShader; }

 private:
  ~WebGLProgram();

  void LinkAndUpdate();
  bool ValidateForLink();
  bool ValidateAfterTentativeLink(nsCString* const out_linkLog) const;

 public:
  const GLuint mGLName;

 private:
  WebGLRefPtr<WebGLShader> mVertShader;
  WebGLRefPtr<WebGLShader> mFragShader;
  size_t mNumActiveTFOs;

  std::map<std::string, GLuint> mNextLink_BoundAttribLocs;

  std::vector<nsString> mNextLink_TransformFeedbackVaryings;
  GLenum mNextLink_TransformFeedbackBufferMode;

  nsCString mLinkLog;
  RefPtr<const webgl::LinkedProgramInfo> mMostRecentLinkInfo;
};

}  // namespace mozilla

#endif  // WEBGL_PROGRAM_H_
