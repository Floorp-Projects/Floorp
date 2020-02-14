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

namespace layout {
class PVsyncChild;
}

namespace gfx {
class PVsyncBridgeParent;

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

    void AddCompositorVsyncDispatcher(
        CompositorVsyncDispatcher* aCompositorVsyncDispatcher);
    void RemoveCompositorVsyncDispatcher(
        CompositorVsyncDispatcher* aCompositorVsyncDispatcher);
    void MoveListenersToNewSource(VsyncSource::Display& aNewDisplay);
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
    nsTArray<RefPtr<CompositorVsyncDispatcher>> mCompositorVsyncDispatchers;
    RefPtr<RefreshTimerVsyncDispatcher> mRefreshTimerVsyncDispatcher;
    VsyncId mVsyncId;
  };

  void AddCompositorVsyncDispatcher(
      CompositorVsyncDispatcher* aCompositorVsyncDispatcher);
  void RemoveCompositorVsyncDispatcher(
      CompositorVsyncDispatcher* aCompositorVsyncDispatcher);

  void MoveListenersToNewSource(VsyncSource* aNewSource);

  RefPtr<RefreshTimerVsyncDispatcher> GetRefreshTimerVsyncDispatcher();
  virtual Display& GetGlobalDisplay() = 0;  // Works across all displays
  void Shutdown();

 protected:
  virtual ~VsyncSource() = default;
};

}  // namespace gfx

namespace recordreplay {
namespace child {
void NotifyVsyncObserver();
}
}  // namespace recordreplay

struct VsyncEvent {
  VsyncId mId;
  TimeStamp mTime;

 private:
  VsyncEvent(const VsyncId& aId, const TimeStamp& aTime)
      : mId(aId), mTime(aTime) {}
  VsyncEvent() {}
  friend class gfx::VsyncSource::Display;
  friend class gfx::PVsyncBridgeParent;
  friend class layout::PVsyncChild;
  friend void recordreplay::child::NotifyVsyncObserver();
};

}  // namespace mozilla

#endif /* GFX_VSYNCSOURCE_H */
