/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VSYNCSOURCE_H
#define GFX_VSYNCSOURCE_H

#include "nsTArray.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Mutex.h"
#include "mozilla/TimeStamp.h"
#include "nsISupportsImpl.h"
#include "mozilla/layers/LayersTypes.h"

namespace mozilla {
class RefreshTimerVsyncDispatcher;
class CompositorVsyncDispatcher;

class VsyncIdType {};
typedef layers::BaseTransactionId<VsyncIdType> VsyncId;

namespace gfx {

// Controls how and when to enable/disable vsync. Lives as long as the
// gfxPlatform does on the parent process
class VsyncSource {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VsyncSource)

  typedef mozilla::RefreshTimerVsyncDispatcher RefreshTimerVsyncDispatcher;
  typedef mozilla::CompositorVsyncDispatcher CompositorVsyncDispatcher;

 public:
  // Controls vsync unique to each display and unique on each platform
  class Display {
   public:
    Display();
    virtual ~Display();

    // Notified when this display's vsync occurs, on the vsync thread
    // The aVsyncTimestamp should normalize to the Vsync time that just occured
    // However, different platforms give different vsync notification times.
    // OSX - The vsync timestamp of the upcoming frame, in the future
    // Windows: It's messy, see gfxWindowsPlatform.
    // Android: TODO
    // All platforms should normalize to the vsync that just occured.
    // Large parts of Gecko assume TimeStamps should not be in the future such
    // as animations
    virtual void NotifyVsync(TimeStamp aVsyncTimestamp);

    RefPtr<RefreshTimerVsyncDispatcher> GetRefreshTimerVsyncDispatcher();

    void RegisterCompositorVsyncDispatcher(
        CompositorVsyncDispatcher* aCompositorVsyncDispatcher);
    void DeregisterCompositorVsyncDispatcher(
        CompositorVsyncDispatcher* aCompositorVsyncDispatcher);
    void EnableCompositorVsyncDispatcher(
        CompositorVsyncDispatcher* aCompositorVsyncDispatcher);
    void DisableCompositorVsyncDispatcher(
        CompositorVsyncDispatcher* aCompositorVsyncDispatcher);
    void MoveListenersToNewSource(const RefPtr<VsyncSource>& aNewSource);
    void NotifyRefreshTimerVsyncStatus(bool aEnable);
    virtual TimeDuration GetVsyncRate();

    // These should all only be called on the main thread
    virtual void EnableVsync() = 0;
    virtual void DisableVsync() = 0;
    virtual bool IsVsyncEnabled() = 0;
    virtual void Shutdown() = 0;

   private:
    void UpdateVsyncStatus();

    Mutex mDispatcherLock;
    bool mRefreshTimerNeedsVsync;
    nsTArray<RefPtr<CompositorVsyncDispatcher>>
        mEnabledCompositorVsyncDispatchers;
    nsTArray<RefPtr<CompositorVsyncDispatcher>>
        mRegisteredCompositorVsyncDispatchers;
    RefPtr<RefreshTimerVsyncDispatcher> mRefreshTimerVsyncDispatcher;
    VsyncId mVsyncId;
  };

  void EnableCompositorVsyncDispatcher(
      CompositorVsyncDispatcher* aCompositorVsyncDispatcher);
  void DisableCompositorVsyncDispatcher(
      CompositorVsyncDispatcher* aCompositorVsyncDispatcher);
  void RegisterCompositorVsyncDispatcher(
      CompositorVsyncDispatcher* aCompositorVsyncDispatcher);
  void DeregisterCompositorVsyncDispatcher(
      CompositorVsyncDispatcher* aCompositorVsyncDispatcher);

  void MoveListenersToNewSource(const RefPtr<VsyncSource>& aNewSource);

  RefPtr<RefreshTimerVsyncDispatcher> GetRefreshTimerVsyncDispatcher();
  virtual Display& GetGlobalDisplay() = 0;  // Works across all displays
  void Shutdown();

 protected:
  virtual ~VsyncSource() = default;
};

}  // namespace gfx

struct VsyncEvent {
  VsyncId mId;
  TimeStamp mTime;

  VsyncEvent(const VsyncId& aId, const TimeStamp& aTime)
      : mId(aId), mTime(aTime) {}
  VsyncEvent() = default;
};

}  // namespace mozilla

#endif /* GFX_VSYNCSOURCE_H */
