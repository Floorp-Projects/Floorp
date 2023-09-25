/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_CanvasChild_h
#define mozilla_layers_CanvasChild_h

#include "mozilla/gfx/RecordedEvent.h"
#include "mozilla/ipc/CrossProcessSemaphore.h"
#include "mozilla/layers/PCanvasChild.h"
#include "mozilla/layers/SourceSurfaceSharedData.h"
#include "mozilla/WeakPtr.h"
#include "nsRefPtrHashtable.h"
#include "nsTArray.h"

namespace mozilla {

namespace gfx {
class SourceSurface;
}

namespace layers {
class CanvasDrawEventRecorder;

class CanvasChild final : public PCanvasChild, public SupportsWeakPtr {
 public:
  NS_INLINE_DECL_REFCOUNTING(CanvasChild)

  CanvasChild();

  /**
   * @returns true if remote canvas has been deactivated due to failure.
   */
  static bool Deactivated() { return mDeactivated; }

  /**
   * Release resources until they are next required.
   */
  static void ClearCachedResources();

  ipc::IPCResult RecvNotifyDeviceChanged();

  ipc::IPCResult RecvDeactivate();

  /**
   * Ensures that the DrawEventRecorder has been created.
   *
   * @params aTextureType the TextureType to create in the CanvasTranslator.
   */
  void EnsureRecorder(TextureType aTextureType);

  /**
   * Send a messsage to our CanvasParent to resume translation.
   */
  void ResumeTranslation();

  /**
   * Clean up IPDL actor.
   */
  void Destroy();

  /**
   * Called when a RecordedTextureData is write locked.
   */
  void OnTextureWriteLock();

  /**
   * Called when a RecordedTextureData is forwarded to the compositor.
   */
  void OnTextureForwarded();

  /**
   * @returns true if we should be caching data surfaces in the GPU process.
   */
  bool ShouldCacheDataSurface() const {
    return mTransactionsSinceGetDataSurface < kCacheDataSurfaceThreshold;
  }

  /**
   * Ensures that we have sent a begin transaction event, since the last
   * end transaction.
   * @returns false on failure to begin transaction
   */
  bool EnsureBeginTransaction();

  /**
   * Send an end transaction event to indicate the end of events for this frame.
   */
  void EndTransaction();

  /**
   * @returns true if the canvas IPC classes have not been used for some time
   *          and can be cleaned up.
   */
  bool ShouldBeCleanedUp() const;

  /**
   * Create a DrawTargetRecording for a canvas texture.
   * @param aSize size for the DrawTarget
   * @param aFormat SurfaceFormat for the DrawTarget
   * @returns newly created DrawTargetRecording
   */
  already_AddRefed<gfx::DrawTarget> CreateDrawTarget(
      gfx::IntSize aSize, gfx::SurfaceFormat aFormat);

  /**
   * Record an event for processing by the CanvasParent's CanvasTranslator.
   * @param aEvent the event to record
   */
  void RecordEvent(const gfx::RecordedEvent& aEvent);

  /**
   * Wrap the given surface, so that we can provide a DataSourceSurface if
   * required.
   * @param aSurface the SourceSurface to wrap
   * @returns a SourceSurface that can provide a DataSourceSurface if required
   */
  already_AddRefed<gfx::SourceSurface> WrapSurface(
      const RefPtr<gfx::SourceSurface>& aSurface);

  /**
   * Get DataSourceSurface from the translated equivalent version of aSurface in
   * the GPU process.
   * @param aSurface the SourceSurface in this process for which we need a
   *                 DataSourceSurface
   * @returns a DataSourceSurface created from data for aSurface retrieve from
   *          GPU process
   */
  already_AddRefed<gfx::DataSourceSurface> GetDataSurface(
      const gfx::SourceSurface* aSurface);

 protected:
  void ActorDestroy(ActorDestroyReason aWhy) final;

 private:
  DISALLOW_COPY_AND_ASSIGN(CanvasChild);

  ~CanvasChild() final;

  static const uint32_t kCacheDataSurfaceThreshold = 10;

  static bool mDeactivated;
  static bool mInForeground;

  RefPtr<CanvasDrawEventRecorder> mRecorder;
  TextureType mTextureType = TextureType::Unknown;
  uint32_t mLastWriteLockCheckpoint = 0;
  uint32_t mTransactionsSinceGetDataSurface = kCacheDataSurfaceThreshold;
  std::vector<RefPtr<gfx::SourceSurface>> mLastTransactionExternalSurfaces;
  bool mIsInTransaction = false;
  bool mHasOutstandingWriteLock = false;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_CanvasChild_h
