//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ContextState: Container class for all GL context state, caps and objects.

#ifndef LIBANGLE_CONTEXTSTATE_H_
#define LIBANGLE_CONTEXTSTATE_H_

#include "common/MemoryBuffer.h"
#include "common/angleutils.h"
#include "libANGLE/State.h"
#include "libANGLE/Version.h"
#include "libANGLE/params.h"

namespace gl
{
class BufferManager;
class ContextState;
class FramebufferManager;
class PathManager;
class ProgramPipelineManager;
class RenderbufferManager;
class SamplerManager;
class ShaderProgramManager;
class SyncManager;
class TextureManager;

static constexpr Version ES_2_0 = Version(2, 0);
static constexpr Version ES_3_0 = Version(3, 0);
static constexpr Version ES_3_1 = Version(3, 1);

using ContextID = uintptr_t;

class ContextState final : angle::NonCopyable
{
  public:
    ContextState(ContextID context,
                 const ContextState *shareContextState,
                 TextureManager *shareTextures,
                 const Version &clientVersion,
                 State *state,
                 const Caps &caps,
                 const TextureCapsMap &textureCaps,
                 const Extensions &extensions,
                 const Limitations &limitations);
    ~ContextState();

    ContextID getContextID() const { return mContext; }
    GLint getClientMajorVersion() const { return mClientVersion.major; }
    GLint getClientMinorVersion() const { return mClientVersion.minor; }
    const Version &getClientVersion() const { return mClientVersion; }
    const State &getState() const { return *mState; }
    const Caps &getCaps() const { return mCaps; }
    const TextureCapsMap &getTextureCaps() const { return mTextureCaps; }
    const Extensions &getExtensions() const { return mExtensions; }
    const Limitations &getLimitations() const { return mLimitations; }

    const TextureCaps &getTextureCap(GLenum internalFormat) const;

    bool isWebGL() const;
    bool isWebGL1() const;

  private:
    friend class Context;

    Version mClientVersion;
    ContextID mContext;
    State *mState;
    const Caps &mCaps;
    const TextureCapsMap &mTextureCaps;
    const Extensions &mExtensions;
    const Limitations &mLimitations;

    BufferManager *mBuffers;
    ShaderProgramManager *mShaderPrograms;
    TextureManager *mTextures;
    RenderbufferManager *mRenderbuffers;
    SamplerManager *mSamplers;
    SyncManager *mSyncs;
    PathManager *mPaths;
    FramebufferManager *mFramebuffers;
    ProgramPipelineManager *mPipelines;
};

}  // namespace gl

#endif  // LIBANGLE_CONTEXTSTATE_H_
