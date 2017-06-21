/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
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
} // namespace gfx
namespace gl {
class GLContext;
} // namespace gl
namespace layers {

class TextureHost;
class DataTextureSource;
class Compositor;

// Provided by a HostLayerManager or Compositor for allocating backend-specific
// texture types.
class TextureSourceProvider
{
public:
  NS_INLINE_DECL_REFCOUNTING(TextureSourceProvider)

  virtual already_AddRefed<DataTextureSource>
  CreateDataTextureSource(TextureFlags aFlags = TextureFlags::NO_FLAGS) = 0;

  virtual already_AddRefed<DataTextureSource>
  CreateDataTextureSourceAround(gfx::DataSourceSurface* aSurface) {
    return nullptr;
  }

  virtual already_AddRefed<DataTextureSource>
  CreateDataTextureSourceAroundYCbCr(TextureHost* aTexture) {
    return nullptr;
  }

  virtual TimeStamp GetLastCompositionEndTime() const = 0;

  // Return true if the effect type is supported.
  //
  // By default Compositor implementations should support all effects but in
  // some rare cases it is not possible to support an effect efficiently.
  // This is the case for BasicCompositor with EffectYCbCr.
  virtual bool SupportsEffect(EffectTypes aEffect) { return true; }

  /// Most compositor backends operate asynchronously under the hood. This
  /// means that when a layer stops using a texture it is often desirable to
  /// wait for the end of the next composition before releasing the texture's
  /// ReadLock.
  /// This function provides a convenient way to do this delayed unlocking, if
  /// the texture itself requires it.
  void UnlockAfterComposition(TextureHost* aTexture);

  /// Most compositor backends operate asynchronously under the hood. This
  /// means that when a layer stops using a texture it is often desirable to
  /// wait for the end of the next composition before NotifyNotUsed() call.
  /// This function provides a convenient way to do this delayed NotifyNotUsed()
  /// call, if the texture itself requires it.
  /// See bug 1260611 and bug 1252835
  ///
  /// Returns true if notified, false otherwise.
  virtual bool NotifyNotUsedAfterComposition(TextureHost* aTextureHost);

  // If overridden, make sure to call the base function.
  virtual void Destroy();

  void FlushPendingNotifyNotUsed();

  // If this provider is also a Compositor, return the compositor. Otherwise return
  // null.
  virtual Compositor* AsCompositor() {
    return nullptr;
  }

#ifdef XP_WIN
  // On Windows, if this provides Direct3D textures, it must expose the device.
  virtual ID3D11Device* GetD3D11Device() const {
    return nullptr;
  }
#endif

  // If this provides OpenGL textures, it must expose the GLContext.
  virtual gl::GLContext* GetGLContext() const {
    return nullptr;
  }

  virtual int32_t GetMaxTextureSize() const = 0;

  // Return whether or not this provider is still valid (i.e., is still being
  // used to composite).
  virtual bool IsValid() const = 0;

public:
  class MOZ_STACK_CLASS AutoReadUnlockTextures
  {
  public:
    explicit AutoReadUnlockTextures(TextureSourceProvider* aProvider)
     : mProvider(aProvider)
    {}
    ~AutoReadUnlockTextures() {
      mProvider->ReadUnlockTextures();
    }

  private:
    RefPtr<TextureSourceProvider> mProvider;
  };

protected:
  // Should be called at the end of each composition.
  void ReadUnlockTextures();

  virtual ~TextureSourceProvider();

private:
  // An array of locks that will need to be unlocked after the next composition.
  nsTArray<RefPtr<TextureHost>> mUnlockAfterComposition;

  // An array of TextureHosts that will need to call NotifyNotUsed() after the next composition.
  nsTArray<RefPtr<TextureHost>> mNotifyNotUsedAfterComposition;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_gfx_layers_TextureSourceProvider_h
