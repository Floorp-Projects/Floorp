/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLPROGRAM_H_
#define WEBGLPROGRAM_H_

#include "WebGLObjectModel.h"

#include "nsWrapperCache.h"

#include "mozilla/LinkedList.h"
#include "mozilla/CheckedInt.h"

namespace mozilla {

class WebGLShader;
class WebGLUniformInfo;

typedef nsDataHashtable<nsCStringHashKey, nsCString> CStringMap;
typedef nsDataHashtable<nsCStringHashKey, WebGLUniformInfo> CStringToUniformInfoMap;

class WebGLProgram MOZ_FINAL
    : public nsISupports
    , public WebGLRefCountedObject<WebGLProgram>
    , public LinkedListElement<WebGLProgram>
    , public WebGLContextBoundObject
    , public nsWrapperCache
{
public:
    WebGLProgram(WebGLContext *context);

    ~WebGLProgram() {
        DeleteOnce();
    }

    void Delete();

    void DetachShaders() {
        mAttachedShaders.Clear();
    }

    WebGLuint GLName() { return mGLName; }
    const nsTArray<WebGLRefPtr<WebGLShader> >& AttachedShaders() const { return mAttachedShaders; }
    bool LinkStatus() { return mLinkStatus; }
    uint32_t Generation() const { return mGeneration.value(); }
    void SetLinkStatus(bool val) { mLinkStatus = val; }

    bool ContainsShader(WebGLShader *shader) {
        return mAttachedShaders.Contains(shader);
    }

    // return true if the shader wasn't already attached
    bool AttachShader(WebGLShader *shader);

    // return true if the shader was found and removed
    bool DetachShader(WebGLShader *shader);

    bool HasAttachedShaderOfType(GLenum shaderType);

    bool HasBothShaderTypesAttached() {
        return
            HasAttachedShaderOfType(LOCAL_GL_VERTEX_SHADER) &&
            HasAttachedShaderOfType(LOCAL_GL_FRAGMENT_SHADER);
    }

    bool HasBadShaderAttached();

    size_t UpperBoundNumSamplerUniforms();

    bool NextGeneration()
    {
        if (!(mGeneration + 1).isValid())
            return false; // must exit without changing mGeneration
        ++mGeneration;
        return true;
    }

    /* Called only after LinkProgram */
    bool UpdateInfo();

    /* Getters for cached program info */
    bool IsAttribInUse(unsigned i) const { return mAttribsInUse[i]; }

    /* Maps identifier |name| to the mapped identifier |*mappedName|
     * Both are ASCII strings.
     */
    void MapIdentifier(const nsACString& name, nsCString *mappedName);

    /* Un-maps mapped identifier |name| to the original identifier |*reverseMappedName|
     * Both are ASCII strings.
     */
    void ReverseMapIdentifier(const nsACString& name, nsCString *reverseMappedName);

    /* Returns the uniform array size (or 1 if the uniform is not an array) of
     * the uniform with given mapped identifier.
     *
     * Note: the input string |name| is the mapped identifier, not the original identifier.
     */
    WebGLUniformInfo GetUniformInfoForMappedIdentifier(const nsACString& name);

    WebGLContext *GetParentObject() const {
        return Context();
    }

    virtual JSObject* WrapObject(JSContext *cx,
                                 JS::Handle<JSObject*> scope) MOZ_OVERRIDE;

    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(WebGLProgram)

    // public post-link data
    std::map<GLint, nsCString> mActiveAttribMap;

protected:

    WebGLuint mGLName;
    bool mLinkStatus;
    // attached shaders of the program object
    nsTArray<WebGLRefPtr<WebGLShader> > mAttachedShaders;
    CheckedUint32 mGeneration;

    // post-link data
    nsTArray<bool> mAttribsInUse;
    nsAutoPtr<CStringMap> mIdentifierMap, mIdentifierReverseMap;
    nsAutoPtr<CStringToUniformInfoMap> mUniformInfoMap;
    int mAttribMaxNameLength;
};

} // namespace mozilla

#endif
