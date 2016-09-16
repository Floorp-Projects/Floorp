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

namespace gl
{
class ValidationContext;

class ContextState final : public angle::NonCopyable
{
  public:
    ContextState(uintptr_t context,
                 GLint clientVersion,
                 State *state,
                 const Caps &caps,
                 const TextureCapsMap &textureCaps,
                 const Extensions &extensions,
                 const ResourceManager *resourceManager,
                 const Limitations &limitations);
    ~ContextState();

    uintptr_t getContext() const { return mContext; }
    GLint getClientVersion() const { return mClientVersion; }
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

    uintptr_t mContext;
    GLint mClientVersion;
    State *mState;
    const Caps &mCaps;
    const TextureCapsMap &mTextureCaps;
    const Extensions &mExtensions;
    const ResourceManager *mResourceManager;
    const Limitations &mLimitations;
};

class ValidationContext : angle::NonCopyable
{
  public:
    ValidationContext(GLint clientVersion,
                      State *state,
                      const Caps &caps,
                      const TextureCapsMap &textureCaps,
                      const Extensions &extensions,
                      const ResourceManager *resourceManager,
                      const Limitations &limitations,
                      bool skipValidation);
    virtual ~ValidationContext() {}

    virtual void handleError(const Error &error) = 0;

    const ContextState &getContextState() const { return mState; }
    int getClientVersion() const { return mState.getClientVersion(); }
    const State &getGLState() const { return mState.getState(); }
    const Caps &getCaps() const { return mState.getCaps(); }
    const TextureCapsMap &getTextureCaps() const { return mState.getTextureCaps(); }
    const Extensions &getExtensions() const { return mState.getExtensions(); }
    const Limitations &getLimitations() const { return mState.getLimitations(); }
    bool skipValidation() const { return mSkipValidation; }

    // Specific methods needed for validation.
    bool getQueryParameterInfo(GLenum pname, GLenum *type, unsigned int *numParams);
    bool getIndexedQueryParameterInfo(GLenum target, GLenum *type, unsigned int *numParams);

  protected:
    ContextState mState;
    bool mSkipValidation;
};
}  // namespace gl

#endif  // LIBANGLE_CONTEXTSTATE_H_
