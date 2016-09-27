/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_PersistentBUFFERPROVIDER_H
#define MOZILLA_GFX_PersistentBUFFERPROVIDER_H

#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/RefPtr.h"             // for RefPtr, already_AddRefed, etc
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/layers/TextureForwarder.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/Vector.h"

namespace mozilla {

namespace gfx {
  class SourceSurface;
  class DrawTarget;
}

namespace layers {

class CopyableCanvasLayer;

/**
 * A PersistentBufferProvider is for users which require the temporary use of
 * a DrawTarget to draw into. When they're done drawing they return the
 * DrawTarget, when they later need to continue drawing they get a DrawTarget
 * from the provider again, the provider will guarantee the contents of the
 * previously returned DrawTarget is persisted into the one newly returned.
 */
class PersistentBufferProvider : public RefCounted<PersistentBufferProvider>
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(PersistentBufferProvider)

  virtual ~PersistentBufferProvider() { }

  virtual LayersBackend GetType() { return LayersBackend::LAYERS_NONE; }

  /**
   * Get a DrawTarget from the PersistentBufferProvider.
   *
   * \param aPersistedRect This indicates the area of the DrawTarget that needs
   *                       to have remained the same since the call to
   *                       ReturnDrawTarget.
   */
  virtual already_AddRefed<gfx::DrawTarget> BorrowDrawTarget(const gfx::IntRect& aPersistedRect) = 0;

  /**
   * Return a DrawTarget to the PersistentBufferProvider and indicate the
   * contents of this DrawTarget is to be considered current by the
   * BufferProvider. The caller should forget any references to the DrawTarget.
   */
  virtual bool ReturnDrawTarget(already_AddRefed<gfx::DrawTarget> aDT) = 0;

  virtual already_AddRefed<gfx::SourceSurface> BorrowSnapshot() = 0;

  virtual void ReturnSnapshot(already_AddRefed<gfx::SourceSurface> aSnapshot) = 0;

  virtual TextureClient* GetTextureClient() { return nullptr; }

  virtual void OnShutdown() {}

  virtual bool SetForwarder(KnowsCompositor* aFwd) { return true; }

  /**
   * Return true if this provider preserves the drawing state (clips, transforms,
   * etc.) across frames. In practice this means users of the provider can skip
   * popping all of the clips at the end of the frames and pushing them back at
   * the beginning of the following frames, which can be costly (cf. bug 1294351).
   */
  virtual bool PreservesDrawingState() const = 0;
};


class PersistentBufferProviderBasic : public PersistentBufferProvider
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(PersistentBufferProviderBasic, override)

  static already_AddRefed<PersistentBufferProviderBasic>
  Create(gfx::IntSize aSize, gfx::SurfaceFormat aFormat, gfx::BackendType aBackend);

  explicit PersistentBufferProviderBasic(gfx::DrawTarget* aTarget);

  virtual LayersBackend GetType() override { return LayersBackend::LAYERS_BASIC; }

  virtual already_AddRefed<gfx::DrawTarget> BorrowDrawTarget(const gfx::IntRect& aPersistedRect) override;

  virtual bool ReturnDrawTarget(already_AddRefed<gfx::DrawTarget> aDT) override;

  virtual already_AddRefed<gfx::SourceSurface> BorrowSnapshot() override;

  virtual void ReturnSnapshot(already_AddRefed<gfx::SourceSurface> aSnapshot) override;

  virtual bool PreservesDrawingState() const override { return true; }
private:
  ~PersistentBufferProviderBasic();

  RefPtr<gfx::DrawTarget> mDrawTarget;
  RefPtr<gfx::SourceSurface> mSnapshot;
};


/**
 * Provides access to a buffer which can be sent to the compositor without
 * requiring a copy.
 */
class PersistentBufferProviderShared : public PersistentBufferProvider
                                     , public ActiveResource
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(PersistentBufferProviderShared, override)

  static already_AddRefed<PersistentBufferProviderShared>
  Create(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
         KnowsCompositor* aFwd);

  virtual LayersBackend GetType() override { return LayersBackend::LAYERS_CLIENT; }

  virtual already_AddRefed<gfx::DrawTarget> BorrowDrawTarget(const gfx::IntRect& aPersistedRect) override;

  virtual bool ReturnDrawTarget(already_AddRefed<gfx::DrawTarget> aDT) override;

  virtual already_AddRefed<gfx::SourceSurface> BorrowSnapshot() override;

  virtual void ReturnSnapshot(already_AddRefed<gfx::SourceSurface> aSnapshot) override;

  virtual TextureClient* GetTextureClient() override;

  virtual void NotifyInactive() override;

  virtual void OnShutdown() override { Destroy(); }

  virtual bool SetForwarder(KnowsCompositor* aFwd) override;

  virtual bool PreservesDrawingState() const override { return false; }
protected:
  PersistentBufferProviderShared(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                                 KnowsCompositor* aFwd,
                                 RefPtr<TextureClient>& aTexture);

  ~PersistentBufferProviderShared();

  TextureClient* GetTexture(Maybe<uint32_t> aIndex);
  bool CheckIndex(uint32_t aIndex) { return aIndex < mTextures.length(); }

  void Destroy();

  gfx::IntSize mSize;
  gfx::SurfaceFormat mFormat;
  RefPtr<KnowsCompositor> mFwd;
  Vector<RefPtr<TextureClient>, 4> mTextures;
  // Offset of the texture in mTextures that the canvas uses.
  Maybe<uint32_t> mBack;
  // Offset of the texture in mTextures that is presented to the compositor.
  Maybe<uint32_t> mFront;

  RefPtr<gfx::DrawTarget> mDrawTarget;
  RefPtr<gfx::SourceSurface > mSnapshot;
};

struct AutoReturnSnapshot
{
  PersistentBufferProvider* mBufferProvider;
  RefPtr<gfx::SourceSurface>* mSnapshot;

  explicit AutoReturnSnapshot(PersistentBufferProvider* aProvider = nullptr)
  : mBufferProvider(aProvider)
  , mSnapshot(nullptr)
  {}

  ~AutoReturnSnapshot()
  {
    if (mBufferProvider) {
      mBufferProvider->ReturnSnapshot(mSnapshot ? mSnapshot->forget() : nullptr);
    }
  }
};

} // namespace layers
} // namespace mozilla

#endif
