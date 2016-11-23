/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include "WebGLContext.h"
#include "WebGLObjectModel.h"

namespace mozilla {
class ErrorResult;
class WebGLActiveInfo;
class WebGLProgram;
class WebGLShader;
class WebGLUniformLocation;

namespace dom {
template<typename> struct Nullable;
class OwningUnsignedLongOrUint32ArrayOrBoolean;
template<typename> class Sequence;
} // namespace dom

namespace webgl {

struct AttribInfo final
{
    const RefPtr<WebGLActiveInfo> mActiveInfo;
    uint32_t mLoc;
};

struct UniformInfo final
{
    typedef decltype(WebGLContext::mBound2DTextures) TexListT;

    const RefPtr<WebGLActiveInfo> mActiveInfo;
    const TexListT* const mSamplerTexList;
    std::vector<uint32_t> mSamplerValues;

protected:
    static const TexListT*
    GetTexList(WebGLActiveInfo* activeInfo);

public:
    explicit UniformInfo(WebGLActiveInfo* activeInfo);
};

struct UniformBlockInfo final
{
    const nsCString mBaseUserName;
    const nsCString mBaseMappedName;
    const uint32_t mDataSize;

    const IndexedBufferBinding* mBinding;

    UniformBlockInfo(WebGLContext* webgl, const nsACString& baseUserName,
                     const nsACString& baseMappedName, uint32_t dataSize)
        : mBaseUserName(baseUserName)
        , mBaseMappedName(baseMappedName)
        , mDataSize(dataSize)
        , mBinding(&webgl->mIndexedUniformBufferBindings[0])
    { }
};

struct LinkedProgramInfo final
    : public RefCounted<LinkedProgramInfo>
    , public SupportsWeakPtr<LinkedProgramInfo>
{
    friend class WebGLProgram;

    MOZ_DECLARE_REFCOUNTED_TYPENAME(LinkedProgramInfo)
    MOZ_DECLARE_WEAKREFERENCE_TYPENAME(LinkedProgramInfo)

    //////

    WebGLProgram* const prog;

    std::vector<AttribInfo> attribs;
    std::vector<UniformInfo*> uniforms; // Owns its contents.
    std::vector<UniformBlockInfo*> uniformBlocks; // Owns its contents.
    std::vector<RefPtr<WebGLActiveInfo>> transformFeedbackVaryings;

    // Needed for draw call validation.
    std::vector<UniformInfo*> uniformSamplers;

    mutable std::vector<size_t> componentsPerTFVert;

    //////

    // The maps for the frag data names to the translated names.
    std::map<nsCString, const nsCString> fragDataMap;

    explicit LinkedProgramInfo(WebGLProgram* prog);
    ~LinkedProgramInfo();

    bool FindAttrib(const nsCString& baseUserName, const AttribInfo** const out) const;
    bool FindUniform(const nsCString& baseUserName, UniformInfo** const out) const;
    bool FindUniformBlock(const nsCString& baseUserName,
                          const UniformBlockInfo** const out) const;

    bool
    FindFragData(const nsCString& baseUserName,
                 nsCString* const out_baseMappedName) const
    {
        const auto itr = fragDataMap.find(baseUserName);
        if (itr == fragDataMap.end()) {
            return false;
        }

        *out_baseMappedName = itr->second;
        return true;
    }
};

} // namespace webgl

class WebGLProgram final
    : public nsWrapperCache
    , public WebGLRefCountedObject<WebGLProgram>
    , public LinkedListElement<WebGLProgram>
    , public WebGLContextBoundObject
{
    friend class WebGLTransformFeedback;

public:
    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLProgram)
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLProgram)

    explicit WebGLProgram(WebGLContext* webgl);

    void Delete();

    // GL funcs
    void AttachShader(WebGLShader* shader);
    void BindAttribLocation(GLuint index, const nsAString& name);
    void DetachShader(const WebGLShader* shader);
    already_AddRefed<WebGLActiveInfo> GetActiveAttrib(GLuint index) const;
    already_AddRefed<WebGLActiveInfo> GetActiveUniform(GLuint index) const;
    void GetAttachedShaders(nsTArray<RefPtr<WebGLShader>>* const out) const;
    GLint GetAttribLocation(const nsAString& name) const;
    GLint GetFragDataLocation(const nsAString& name) const;
    void GetProgramInfoLog(nsAString* const out) const;
    JS::Value GetProgramParameter(GLenum pname) const;
    GLuint GetUniformBlockIndex(const nsAString& name) const;
    void GetActiveUniformBlockName(GLuint uniformBlockIndex, nsAString& name) const;
    JS::Value GetActiveUniformBlockParam(GLuint uniformBlockIndex, GLenum pname) const;
    JS::Value GetActiveUniformBlockActiveUniforms(JSContext* cx, GLuint uniformBlockIndex,
                                                  ErrorResult* const out_error) const;
    already_AddRefed<WebGLUniformLocation> GetUniformLocation(const nsAString& name) const;
    void GetUniformIndices(const dom::Sequence<nsString>& uniformNames,
                           dom::Nullable< nsTArray<GLuint> >& retval) const;
    void UniformBlockBinding(GLuint uniformBlockIndex, GLuint uniformBlockBinding) const;

    void LinkProgram();
    bool UseProgram() const;
    void ValidateProgram() const;

    ////////////////

    bool FindAttribUserNameByMappedName(const nsACString& mappedName,
                                        nsDependentCString* const out_userName) const;
    bool FindVaryingByMappedName(const nsACString& mappedName,
                                 nsCString* const out_userName,
                                 bool* const out_isArray) const;
    bool FindUniformByMappedName(const nsACString& mappedName,
                                 nsCString* const out_userName,
                                 bool* const out_isArray) const;
    bool FindUniformBlockByMappedName(const nsACString& mappedName,
                                      nsCString* const out_userName,
                                      bool* const out_isArray) const;

    void TransformFeedbackVaryings(const dom::Sequence<nsString>& varyings,
                                   GLenum bufferMode);
    already_AddRefed<WebGLActiveInfo> GetTransformFeedbackVarying(GLuint index) const;

    void EnumerateFragOutputs(std::map<nsCString, const nsCString> &out_FragOutputs) const;

    bool IsLinked() const { return mMostRecentLinkInfo; }

    const webgl::LinkedProgramInfo* LinkInfo() const {
        return mMostRecentLinkInfo.get();
    }

    WebGLContext* GetParentObject() const {
        return mContext;
    }

    virtual JSObject* WrapObject(JSContext* js, JS::Handle<JSObject*> givenProto) override;

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

    std::map<nsCString, GLuint> mNextLink_BoundAttribLocs;

    std::vector<nsString> mNextLink_TransformFeedbackVaryings;
    GLenum mNextLink_TransformFeedbackBufferMode;

    nsCString mLinkLog;
    RefPtr<const webgl::LinkedProgramInfo> mMostRecentLinkInfo;
};

} // namespace mozilla

#endif // WEBGL_PROGRAM_H_
