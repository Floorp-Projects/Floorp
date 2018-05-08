//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Data.cpp: Container class for all GL relevant state, caps and objects

#include "libANGLE/ContextState.h"

#include "libANGLE/Framebuffer.h"
#include "libANGLE/ResourceManager.h"

namespace gl
{

namespace
{

template <typename T>
using ContextStateMember = T *(ContextState::*);

template <typename T>
T *AllocateOrGetSharedResourceManager(const ContextState *shareContextState,
                                      ContextStateMember<T> member)
{
    if (shareContextState)
    {
        T *resourceManager = (*shareContextState).*member;
        resourceManager->addRef();
        return resourceManager;
    }
    else
    {
        return new T();
    }
}

TextureManager *AllocateOrGetSharedTextureManager(const ContextState *shareContextState,
                                                  TextureManager *shareTextures,
                                                  ContextStateMember<TextureManager> member)
{
    if (shareContextState)
    {
        TextureManager *textureManager = (*shareContextState).*member;
        ASSERT(shareTextures == nullptr || textureManager == shareTextures);
        textureManager->addRef();
        return textureManager;
    }
    else if (shareTextures)
    {
        TextureManager *textureManager = shareTextures;
        textureManager->addRef();
        return textureManager;
    }
    else
    {
        return new TextureManager();
    }
}

}  // anonymous namespace

ContextState::ContextState(ContextID contextIn,
                           const ContextState *shareContextState,
                           TextureManager *shareTextures,
                           const Version &clientVersion,
                           State *stateIn,
                           const Caps &capsIn,
                           const TextureCapsMap &textureCapsIn,
                           const Extensions &extensionsIn,
                           const Limitations &limitationsIn)
    : mClientVersion(clientVersion),
      mContext(contextIn),
      mState(stateIn),
      mCaps(capsIn),
      mTextureCaps(textureCapsIn),
      mExtensions(extensionsIn),
      mLimitations(limitationsIn),
      mBuffers(AllocateOrGetSharedResourceManager(shareContextState, &ContextState::mBuffers)),
      mShaderPrograms(
          AllocateOrGetSharedResourceManager(shareContextState, &ContextState::mShaderPrograms)),
      mTextures(AllocateOrGetSharedTextureManager(shareContextState,
                                                  shareTextures,
                                                  &ContextState::mTextures)),
      mRenderbuffers(
          AllocateOrGetSharedResourceManager(shareContextState, &ContextState::mRenderbuffers)),
      mSamplers(AllocateOrGetSharedResourceManager(shareContextState, &ContextState::mSamplers)),
      mSyncs(AllocateOrGetSharedResourceManager(shareContextState, &ContextState::mSyncs)),
      mPaths(AllocateOrGetSharedResourceManager(shareContextState, &ContextState::mPaths)),
      mFramebuffers(new FramebufferManager()),
      mPipelines(new ProgramPipelineManager())
{
}

ContextState::~ContextState()
{
    // Handles are released by the Context.
}

bool ContextState::isWebGL() const
{
    return mExtensions.webglCompatibility;
}

bool ContextState::isWebGL1() const
{
    return (isWebGL() && mClientVersion.major == 2);
}

const TextureCaps &ContextState::getTextureCap(GLenum internalFormat) const
{
    return mTextureCaps.get(internalFormat);
}

}  // namespace gl
