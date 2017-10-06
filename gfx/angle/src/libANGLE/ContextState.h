//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ContextState: Container class for all GL context state, caps and objects.

#ifndef LIBANGLE_CONTEXTSTATE_H_
#define LIBANGLE_CONTEXTSTATE_H_

#include "common/angleutils.h"
#include "libANGLE/State.h"
#include "libANGLE/Version.h"

namespace gl
{
class ValidationContext;
class ContextState;

static constexpr Version ES_2_0 = Version(2, 0);
static constexpr Version ES_3_0 = Version(3, 0);
static constexpr Version ES_3_1 = Version(3, 1);

class ContextState final : public angle::NonCopyable
{
  public:
    ContextState(uintptr_t context,
                 const Version &clientVersion,
                 State *state,
                 const Caps &caps,
                 const TextureCapsMap &textureCaps,
                 const Extensions &extensions,
                 const ResourceManager *resourceManager,
                 const Limitations &limitations,
                 const ResourceMap<Framebuffer> &framebufferMap);
    ~ContextState();

    uintptr_t getContext() const { return mContext; }
    GLint getClientMajorVersion() const { return mClientVersion.major; }
    GLint getClientMinorVersion() const { return mClientVersion.minor; }
    const Version &getClientVersion() const { return mClientVersion; }
    const State &getState() const { return *mState; }
    const Caps &getCaps() const { return mCaps; }
    const TextureCapsMap &getTextureCaps() const { return mTextureCaps; }
    const Extensions &getExtensions() const { return mExtensions; }
    const ResourceManager &getResourceManager() const { return *mResourceManager; }
    const Limitations &getLimitations() const { return mLimitations; }

    const TextureCaps &getTextureCap(GLenum internalFormat) const;

  private:
    friend class Context;
    friend class ValidationContext;

    Version mClientVersion;
    uintptr_t mContext;
    State *mState;
    const Caps &mCaps;
    const TextureCapsMap &mTextureCaps;
    const Extensions &mExtensions;
    const ResourceManager *mResourceManager;
    const Limitations &mLimitations;
    const ResourceMap<Framebuffer> &mFramebufferMap;
};

class ValidationContext : angle::NonCopyable
{
  public:
    ValidationContext(const Version &clientVersion,
                      State *state,
                      const Caps &caps,
                      const TextureCapsMap &textureCaps,
                      const Extensions &extensions,
                      const ResourceManager *resourceManager,
                      const Limitations &limitations,
                      const ResourceMap<Framebuffer> &framebufferMap,
                      bool skipValidation);
    virtual ~ValidationContext() {}

    virtual void handleError(const Error &error) = 0;

    const ContextState &getContextState() const { return mState; }
    GLint getClientMajorVersion() const { return mState.getClientMajorVersion(); }
    GLint getClientMinorVersion() const { return mState.getClientMinorVersion(); }
    const Version &getClientVersion() const { return mState.getClientVersion(); }
    const State &getGLState() const { return mState.getState(); }
    const Caps &getCaps() const { return mState.getCaps(); }
    const TextureCapsMap &getTextureCaps() const { return mState.getTextureCaps(); }
    const Extensions &getExtensions() const { return mState.getExtensions(); }
    const Limitations &getLimitations() const { return mState.getLimitations(); }
    bool skipValidation() const { return mSkipValidation; }

    // Specific methods needed for validation.
    bool getQueryParameterInfo(GLenum pname, GLenum *type, unsigned int *numParams);
    bool getIndexedQueryParameterInfo(GLenum target, GLenum *type, unsigned int *numParams);

    Program *getProgram(GLuint handle) const;
    Shader *getShader(GLuint handle) const;

    bool isTextureGenerated(GLuint texture) const;
    bool isBufferGenerated(GLuint buffer) const;
    bool isRenderbufferGenerated(GLuint renderbuffer) const;
    bool isFramebufferGenerated(GLuint framebuffer) const;

  protected:
    ContextState mState;
    bool mSkipValidation;
};
}  // namespace gl

#endif  // LIBANGLE_CONTEXTSTATE_H_
