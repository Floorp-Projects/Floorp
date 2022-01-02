/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_gfx_layers_TextureSourceProvider_h
#define mozilla_gfx_layers_TextureSourceProvider_h

#include "nsISupportsImpl.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/layers/CompositorTypes.h"
#include "nsTArray.h"

struct ID3D11Device;

namespace mozilla {
namespace gfx {
class DataSourceSurface;
}  // namespace gfx
namespace gl {
class GLContext;
}  // namespace gl
namespace layers {

class TextureHost;
class DataTextureSource;
class Compositor;
class CompositorOGL;

// Provided by a HostLayerManager or Compositor for allocating backend-specific
// texture types.
class TextureSourceProvider {
 public:
  NS_INLINE_DECL_REFCOUNTING(TextureSourceProvider)

  virtual already_AddRefed<DataTextureSource> CreateDataTextureSource(
      TextureFlags aFlags = TextureFlags::NO_FLAGS) = 0;

  virtual TimeStamp GetLastCompositionEndTime() const = 0;

  virtual void TryUnlockTextures() {}

  // If overridden, make sure to call the base function.
  virtual void Destroy();

  // If this provider is also a Compositor, return the compositor. Otherwise
  // return null.
  virtual Compositor* AsCompositor() { return nullptr; }

  // If this provider is also a CompositorOGL, return the compositor. Otherwise
  // return nullptr.
  virtual CompositorOGL* AsCompositorOGL() { return nullptr; }

#ifdef XP_WIN
  // On Windows, if this provides Direct3D textures, it must expose the device.
  virtual ID3D11Device* GetD3D11Device() const { return nullptr; }
#endif

  // If this provides OpenGL textures, it must expose the GLContext.
  virtual gl::GLContext* GetGLContext() const { return nullptr; }

  virtual int32_t GetMaxTextureSize() const = 0;

  // Return whether or not this provider is still valid (i.e., is still being
  // used to composite).
  virtual bool IsValid() const = 0;

 protected:
  virtual ~TextureSourceProvider();
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_gfx_layers_TextureSourceProvider_h
