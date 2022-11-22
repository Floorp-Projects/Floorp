/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VSYNCSOURCE_H
#define GFX_VSYNCSOURCE_H

#include "nsTArray.h"
#include "mozilla/DataMutex.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Maybe.h"
#include "mozilla/Mutex.h"
#include "mozilla/TimeStamp.h"
#include "nsISupportsImpl.h"
#include "mozilla/layers/LayersTypes.h"

namespace mozilla {
class VsyncDispatcher;
class VsyncObserver;
struct VsyncEvent;

class VsyncIdType {};
typedef layers::BaseTransactionId<VsyncIdType> VsyncId;

namespace gfx {

// Controls how and when to enable/disable vsync. Lives as long as the
// gfxPlatform does on the parent process
class VsyncSource {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VsyncSource)

  typedef mozilla::VsyncDispatcher VsyncDispatcher;

 public:
  VsyncSource();

  // Notified when this display's vsync callback occurs, on the vsync thread
  // Different platforms give different aVsyncTimestamp values.
  // macOS: TimeStamp::Now() or the output time of the previous vsync
  //        callback, whichever is older.
  // Windows: It's messy, see gfxWindowsPlatform.
  // Android: TODO
  //
  // @param aVsyncTimestamp  The time of the Vsync that just occured. Needs to
  //     be at or before the time of the NotifyVsync call.
  // @param aOutputTimestamp  The estimated timestamp at which drawing will
  //     appear on the screen, if the drawing happens within a certain
  //     (unknown) budget. Useful for Audio/Video sync. On platforms where
  //     this timestamp is provided by the system (macOS), it is a much more
  //     stable and consistent timing source than the time at which the vsync
  //     callback is called.
  virtual void NotifyVsync(const TimeStamp& aVsyncTimestamp,
                           const TimeStamp& aOutputTimestamp);

  // Can be called on any thread.
  // Adding the same dispatcher multiple times will increment a count.
  // This means that the sequence "Add, Add, Remove" has the same behavior as
  // "Add, Remove, Add".
  void AddVsyncDispatcher(VsyncDispatcher* aDispatcher);
  void RemoveVsyncDispatcher(VsyncDispatcher* aDispatcher);

  virtual TimeDuration GetVsyncRate();

  // These should all only be called on the main thread
  virtual void EnableVsync() = 0;
  virtual void DisableVsync() = 0;
  virtual bool IsVsyncEnabled() = 0;
  virtual void Shutdown() = 0;

  // Returns the rate of the fastest enabled VsyncSource or Nothing().
  static Maybe<TimeDuration> GetFastestVsyncRate();

 protected:
  virtual ~VsyncSource();

 private:
  // Can be called on any thread
  void UpdateVsyncStatus();

  struct DispatcherRefWithCount {
    // The dispatcher.
    RefPtr<VsyncDispatcher> mDispatcher;
    // The number of add calls minus the number of remove calls for this
    // dispatcher. Should always be > 0 as long as this dispatcher is in
    // mDispatchers.
    size_t mCount = 0;
  };

  struct State {
    // The set of VsyncDispatchers which are registered with this source.
    // At the moment, the length of this array is always zero or one.
    // The ability to register multiple dispatchers is not used yet; it is
    // intended for when we have one dispatcher per widget.
    nsTArray<DispatcherRefWithCount> mDispatchers;

    // The vsync ID which we used for the last vsync event.
    VsyncId mVsyncId;
  };

  DataMutex<State> mState;
};

}  // namespace gfx

struct VsyncEvent {
  VsyncId mId;
  TimeStamp mTime;
  TimeStamp mOutputTime;  // estimate

  VsyncEvent(const VsyncId& aId, const TimeStamp& aVsyncTime,
             const TimeStamp& aOutputTime)
      : mId(aId), mTime(aVsyncTime), mOutputTime(aOutputTime) {}
  VsyncEvent() = default;
};

}  // namespace mozilla

#endif /* GFX_VSYNCSOURCE_H */
