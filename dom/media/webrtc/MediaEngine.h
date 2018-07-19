/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIAENGINE_H_
#define MEDIAENGINE_H_

#include "DOMMediaStream.h"
#include "MediaStreamGraph.h"
#include "MediaTrackConstraints.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"
#include "mozilla/dom/VideoStreamTrack.h"
#include "mozilla/media/DeviceChangeCallback.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ThreadSafeWeakPtr.h"

namespace mozilla {

namespace dom {
class Blob;
} // namespace dom

class AllocationHandle;
class MediaEngineSource;

enum {
  kVideoTrack = 1,
  kAudioTrack = 2,
  kTrackCount
};

class MediaEngine : public DeviceChangeCallback
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaEngine)
  NS_DECL_OWNINGTHREAD

  void AssertIsOnOwningThread() const
  {
    NS_ASSERT_OWNINGTHREAD(MediaEngine);
  }

  /**
   * Populate an array of sources of the requested type in the nsTArray.
   * Also include devices that are currently unavailable.
   */
  virtual void EnumerateDevices(uint64_t aWindowId,
                                dom::MediaSourceEnum,
                                nsTArray<RefPtr<MediaEngineSource>>*) = 0;

  virtual void ReleaseResourcesForWindow(uint64_t aWindowId) = 0;
  virtual void Shutdown() = 0;

  virtual void SetFakeDeviceChangeEvents() {}

protected:
  virtual ~MediaEngine() {}
};

} // namespace mozilla

#endif /* MEDIAENGINE_H_ */
