/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_PersistentBUFFERPROVIDER_H
#define MOZILLA_GFX_PersistentBUFFERPROVIDER_H

#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/RefPtr.h"             // for RefPtr, already_AddRefed, etc
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/gfx/Types.h"

namespace mozilla {
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
   *                       ReturnAndUseDT.
   */
  virtual gfx::DrawTarget* GetDT(const gfx::IntRect& aPersistedRect) = 0;
  /**
   * Return a DrawTarget to the PersistentBufferProvider and indicate the
   * contents of this DrawTarget is to be considered current by the
   * BufferProvider
   */
  virtual bool ReturnAndUseDT(gfx::DrawTarget* aDT) = 0;

  virtual already_AddRefed<gfx::SourceSurface> GetSnapshot() = 0;
protected:
};

class PersistentBufferProviderBasic : public PersistentBufferProvider
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(PersistentBufferProviderBasic)

  PersistentBufferProviderBasic(LayerManager* aManager, gfx::IntSize aSize,
                                gfx::SurfaceFormat aFormat, gfx::BackendType aBackend);

  bool IsValid() { return !!mDrawTarget; }
  virtual LayersBackend GetType() { return LayersBackend::LAYERS_BASIC; }
  gfx::DrawTarget* GetDT(const gfx::IntRect& aPersistedRect) { return mDrawTarget; }
  bool ReturnAndUseDT(gfx::DrawTarget* aDT) { MOZ_ASSERT(mDrawTarget == aDT); return true; }
  virtual already_AddRefed<gfx::SourceSurface> GetSnapshot() { return mDrawTarget->Snapshot(); }
private:
  RefPtr<gfx::DrawTarget> mDrawTarget;
};

} // namespace layers
} // namespace mozilla

#endif
