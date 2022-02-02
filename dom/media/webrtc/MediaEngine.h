/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIAENGINE_H_
#define MEDIAENGINE_H_

#include "DOMMediaStream.h"
#include "MediaEventSource.h"
#include "MediaTrackGraph.h"
#include "MediaTrackConstraints.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"
#include "mozilla/dom/VideoStreamTrack.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ThreadSafeWeakPtr.h"

namespace mozilla {

namespace dom {
class Blob;
}  // namespace dom

class AllocationHandle;
class MediaDevice;
class MediaEngineSource;

enum MediaSinkEnum {
  Speaker,
  Other,
};

enum { kVideoTrack = 1, kAudioTrack = 2, kTrackCount };

class MediaEngine {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaEngine)
  NS_DECL_OWNINGEVENTTARGET

  void AssertIsOnOwningThread() const { NS_ASSERT_OWNINGTHREAD(MediaEngine); }

  /**
   * Populate an array of sources of the requested type in the nsTArray.
   * Also include devices that are currently unavailable.
   */
  virtual void EnumerateDevices(dom::MediaSourceEnum, MediaSinkEnum,
                                nsTArray<RefPtr<MediaDevice>>*) = 0;

  virtual void Shutdown() = 0;

  virtual RefPtr<MediaEngineSource> CreateSource(
      const MediaDevice* aDevice) = 0;

  virtual MediaEventSource<void>& DeviceListChangeEvent() = 0;
  /**
   * Return true if devices returned from EnumerateDevices are emulated media
   * devices.
   */
  virtual bool IsFake() const = 0;

 protected:
  virtual ~MediaEngine() = default;
};

}  // namespace mozilla

#endif /* MEDIAENGINE_H_ */
