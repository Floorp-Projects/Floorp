/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_WEBRTCCALLWRAPPER_H_
#define DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_WEBRTCCALLWRAPPER_H_

#include <set>

#include "domstubs.h"
#include "jsapi/RTCStatsReport.h"
#include "nsISupportsImpl.h"
#include "SystemTime.h"

// libwebrtc includes
#include "api/video/builtin_video_bitrate_allocator_factory.h"
#include "call/call.h"

namespace mozilla {
class AbstractThread;
class MediaSessionConduit;
class SharedWebrtcState;

namespace media {
class ShutdownBlockingTicket;
}

// Wrap the webrtc.org Call class adding mozilla add/ref support.
class WebrtcCallWrapper {
 public:
  typedef webrtc::Call::Config Config;

  static RefPtr<WebrtcCallWrapper> Create(
      const dom::RTCStatsTimestampMaker& aTimestampMaker,
      UniquePtr<media::ShutdownBlockingTicket> aShutdownTicket,
      const RefPtr<SharedWebrtcState>& aSharedState);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebrtcCallWrapper)

  // Don't allow copying/assigning.
  WebrtcCallWrapper(const WebrtcCallWrapper&) = delete;
  void operator=(const WebrtcCallWrapper&) = delete;

  void SetCall(UniquePtr<webrtc::Call> aCall);

  webrtc::Call* Call() const;

  void UnsetRemoteSSRC(uint32_t aSsrc);

  // Idempotent.
  void RegisterConduit(MediaSessionConduit* conduit);

  // Idempotent.
  void UnregisterConduit(MediaSessionConduit* conduit);

  // Allow destroying the Call instance on the Call worker thread.
  //
  // Note that shutdown is blocked until the Call instance is destroyed.
  //
  // This CallWrapper is designed to be sharable, and is held by several objects
  // that are cycle-collectable. TaskQueueWrapper that the Call instances use
  // for worker threads are based off SharedThreadPools, and will block
  // xpcom-shutdown-threads until destroyed. The Call instance however will hold
  // on to its worker threads until destruction.
  //
  // If the last ref to this CallWrapper is held to cycle collector shutdown we
  // end up in a deadlock where cycle collector shutdown is required to destroy
  // the SharedThreadPool that is blocking xpcom-shutdown-threads from finishing
  // and triggering cycle collector shutdown.
  //
  // It would be nice to have the invariant where this class is immutable to the
  // degree that mCall is const, but given the above that is not possible.
  void Destroy();

  const dom::RTCStatsTimestampMaker& GetTimestampMaker() const;

 protected:
  virtual ~WebrtcCallWrapper();

  WebrtcCallWrapper(RefPtr<SharedWebrtcState> aSharedState,
                    UniquePtr<webrtc::VideoBitrateAllocatorFactory>
                        aVideoBitrateAllocatorFactory,
                    UniquePtr<webrtc::RtcEventLog> aEventLog,
                    UniquePtr<webrtc::TaskQueueFactory> aTaskQueueFactory,
                    const dom::RTCStatsTimestampMaker& aTimestampMaker,
                    UniquePtr<media::ShutdownBlockingTicket> aShutdownTicket);

  const RefPtr<SharedWebrtcState> mSharedState;

  // Allows conduits to know about one another, to avoid remote SSRC
  // collisions.
  std::set<MediaSessionConduit*> mConduits;
  RTCStatsTimestampMakerRealtimeClock mClock;
  UniquePtr<media::ShutdownBlockingTicket> mShutdownTicket;

 public:
  const RefPtr<AbstractThread> mCallThread;
  const RefPtr<webrtc::AudioDecoderFactory> mAudioDecoderFactory;
  const UniquePtr<webrtc::VideoBitrateAllocatorFactory>
      mVideoBitrateAllocatorFactory;
  const UniquePtr<webrtc::RtcEventLog> mEventLog;
  const UniquePtr<webrtc::TaskQueueFactory> mTaskQueueFactory;

 protected:
  // Call worker thread only.
  UniquePtr<webrtc::Call> mCall;
};

}  // namespace mozilla

#endif
