/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VSYNCSOURCE_H
#define GFX_VSYNCSOURCE_H

#include "nsTArray.h"
#include "mozilla/nsRefPtr.h"
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
      // b2g - The vsync timestamp of the previous frame that was just displayed
      // OSX - The vsync timestamp of the upcoming frame, in the future
      // TODO: Windows / Linux. DOCUMENT THIS WHEN IMPLEMENTING ON THOSE PLATFORMS
      // Android: TODO
      // All platforms should normalize to the vsync that just occured.
      // Large parts of Gecko assume TimeStamps should not be in the future such as animations
      virtual void NotifyVsync(TimeStamp aVsyncTimestamp);

      nsRefPtr<RefreshTimerVsyncDispatcher> GetRefreshTimerVsyncDispatcher();

      void AddCompositorVsyncDispatcher(CompositorVsyncDispatcher* aCompositorVsyncDispatcher);
      void RemoveCompositorVsyncDispatcher(CompositorVsyncDispatcher* aCompositorVsyncDispatcher);
      void NotifyRefreshTimerVsyncStatus(bool aEnable);

      // These should all only be called on the main thread
      virtual void EnableVsync() = 0;
      virtual void DisableVsync() = 0;
      virtual bool IsVsyncEnabled() = 0;

    private:
      void UpdateVsyncStatus();

      Mutex mDispatcherLock;
      bool mRefreshTimerNeedsVsync;
      nsTArray<nsRefPtr<CompositorVsyncDispatcher>> mCompositorVsyncDispatchers;
      nsRefPtr<RefreshTimerVsyncDispatcher> mRefreshTimerVsyncDispatcher;
  };

  void AddCompositorVsyncDispatcher(CompositorVsyncDispatcher* aCompositorVsyncDispatcher);
  void RemoveCompositorVsyncDispatcher(CompositorVsyncDispatcher* aCompositorVsyncDispatcher);

  nsRefPtr<RefreshTimerVsyncDispatcher> GetRefreshTimerVsyncDispatcher();
  virtual Display& GetGlobalDisplay() = 0; // Works across all displays

protected:
  virtual ~VsyncSource() {}
};

} // namespace gfx
} // namespace mozilla

#endif /* GFX_VSYNCSOURCE_H */
