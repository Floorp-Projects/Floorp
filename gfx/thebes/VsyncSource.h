/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

namespace mozilla {
class RefreshTimerVsyncDispatcher;
class CompositorVsyncDispatcher;

namespace gfx {

// Controls how and when to enable/disable vsync. Lives as long as the
// gfxPlatform does on the parent process
class VsyncSource
{
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
      // Large parts of Gecko assume TimeStamps should not be in the future such as animations
      virtual void NotifyVsync(TimeStamp aVsyncTimestamp);

      RefPtr<RefreshTimerVsyncDispatcher> GetRefreshTimerVsyncDispatcher();

      void AddCompositorVsyncDispatcher(CompositorVsyncDispatcher* aCompositorVsyncDispatcher);
      void RemoveCompositorVsyncDispatcher(CompositorVsyncDispatcher* aCompositorVsyncDispatcher);
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
  };

  void AddCompositorVsyncDispatcher(CompositorVsyncDispatcher* aCompositorVsyncDispatcher);
  void RemoveCompositorVsyncDispatcher(CompositorVsyncDispatcher* aCompositorVsyncDispatcher);

  RefPtr<RefreshTimerVsyncDispatcher> GetRefreshTimerVsyncDispatcher();
  virtual Display& GetGlobalDisplay() = 0; // Works across all displays
  void Shutdown();

protected:
  virtual ~VsyncSource() {}
};

} // namespace gfx
} // namespace mozilla

#endif /* GFX_VSYNCSOURCE_H */
