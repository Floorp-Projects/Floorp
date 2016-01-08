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

struct UniformBlockInfo final
    : public RefCounted<UniformBlockInfo>
{
    MOZ_DECLARE_REFCOUNTED_TYPENAME(UniformBlockInfo);

    const nsCString mBaseUserName;
    const nsCString mBaseMappedName;

    UniformBlockInfo(const nsACString& baseUserName,
                     const nsACString& baseMappedName)
        : mBaseUserName(baseUserName)
        , mBaseMappedName(baseMappedName)
    {}
};

struct LinkedProgramInfo final
    : public RefCounted<LinkedProgramInfo>
    , public SupportsWeakPtr<LinkedProgramInfo>
{
    MOZ_DECLARE_REFCOUNTED_TYPENAME(LinkedProgramInfo)
    MOZ_DECLARE_WEAKREFERENCE_TYPENAME(LinkedProgramInfo)

    WebGLProgram* const prog;
    std::vector<RefPtr<WebGLActiveInfo>> activeAttribs;
    std::vector<RefPtr<WebGLActiveInfo>> activeUniforms;
    std::vector<RefPtr<WebGLActiveInfo>> transformFeedbackVaryings;

    // Needed for Get{Attrib,Uniform}Location. The keys for these are non-mapped
    // user-facing `GLActiveInfo::name`s, without any final "[0]".
    std::map<nsCString, const WebGLActiveInfo*> attribMap;
    std::map<nsCString, const WebGLActiveInfo*> uniformMap;
    std::map<nsCString, const WebGLActiveInfo*> transformFeedbackVaryingsMap;
    std::map<nsCString, const nsCString>* fragDataMap;

    std::vector<RefPtr<UniformBlockInfo>> uniformBlocks;

    // Needed for draw call validation.
    std::set<GLuint> activeAttribLocs;

    explicit LinkedProgramInfo(WebGLProgram* prog);

    bool FindAttrib(const nsCString& baseUserName,
                    const WebGLActiveInfo** const out_activeInfo) const
    {
        auto itr = attribMap.find(baseUserName);
        if (itr == attribMap.end())
            return false;

        *out_activeInfo = itr->second;
        return true;
    }

    bool FindUniform(const nsCString& baseUserName,
                     const WebGLActiveInfo** const out_activeInfo) const
    {
        auto itr = uniformMap.find(baseUserName);
        if (itr == uniformMap.end())
            return false;

        *out_activeInfo = itr->second;
        return true;
    }

    bool FindUniformBlock(const nsCString& baseUserName,
                          RefPtr<const UniformBlockInfo>* const out_info) const
    {
        const size_t count = uniformBlocks.size();
        for (size_t i = 0; i < count; i++) {
            if (baseUserName == uniformBlocks[i]->mBaseUserName) {
                *out_info = uniformBlocks[i].get();
                return true;
            }
        }

        return false;
    }

    bool FindFragData(const nsCString& baseUserName,
                      nsCString* const out_baseMappedName) const
    {
        if (!fragDataMap) {
            *out_baseMappedName = baseUserName;
            return true;
        }

        MOZ_CRASH("Not implemented.");
    }

    bool HasActiveAttrib(GLuint loc) const {
        auto itr = activeAttribLocs.find(loc);
        return itr != activeAttribLocs.end();
    }
};

} // namespace webgl

class WebGLProgram final
    : public nsWrapperCache
    , public WebGLRefCountedObject<WebGLProgram>
    , public LinkedListElement<WebGLProgram>
    , public WebGLContextBoundObject
{
public:
    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLProgram)
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLProgram)

    explicit WebGLProgram(WebGLContext* webgl);

    void Delete();

    // GL funcs
    void AttachShader(WebGLShader* shader);
    void BindAttribLocation(GLuint index, const nsAString& name);
    void DetachShader(WebGLShader* shader);
    already_AddRefed<WebGLActiveInfo> GetActiveAttrib(GLuint index) const;
    already_AddRefed<WebGLActiveInfo> GetActiveUniform(GLuint index) const;
    void GetAttachedShaders(nsTArray<RefPtr<WebGLShader>>* const out) const;
    GLint GetAttribLocation(const nsAString& name) const;
    GLint GetFragDataLocation(const nsAString& name) const;
    void GetProgramInfoLog(nsAString* const out) const;
    JS::Value GetProgramParameter(GLenum pname) const;
    GLuint GetUniformBlockIndex(const nsAString& name) const;
    void GetActiveUniformBlockName(GLuint uniformBlockIndex, nsAString& name) const;
    void GetActiveUniformBlockParam(GLuint uniformBlockIndex, GLenum pname,
                                    dom::Nullable<dom::OwningUnsignedLongOrUint32ArrayOrBoolean>& retval) const;
    void GetActiveUniformBlockActiveUniforms(JSContext* cx, GLuint uniformBlockIndex,
                                             dom::Nullable<dom::OwningUnsignedLongOrUint32ArrayOrBoolean>& retval,
                                             ErrorResult& rv) const;
    already_AddRefed<WebGLUniformLocation> GetUniformLocation(const nsAString& name) const;
    void GetUniformIndices(const dom::Sequence<nsString>& uniformNames,
                           dom::Nullable< nsTArray<GLuint> >& retval) const;
    void UniformBlockBinding(GLuint uniformBlockIndex, GLuint uniformBlockBinding) const;

    bool LinkProgram();
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
    already_AddRefed<WebGLActiveInfo> GetTransformFeedbackVarying(GLuint index);

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

    bool LinkAndUpdate();

public:
    const GLuint mGLName;

private:
    WebGLRefPtr<WebGLShader> mVertShader;
    WebGLRefPtr<WebGLShader> mFragShader;
    std::map<nsCString, GLuint> mBoundAttribLocs;
    std::vector<nsCString> mTransformFeedbackVaryings;
    GLenum mTransformFeedbackBufferMode;
    nsCString mLinkLog;
    RefPtr<const webgl::LinkedProgramInfo> mMostRecentLinkInfo;
    // Storage for transform feedback varyings before link.
    // (Work around for bug seen on nVidia drivers.)
    std::vector<std::string> mTempMappedVaryings;
};

} // namespace mozilla

#endif // WEBGL_PROGRAM_H_
