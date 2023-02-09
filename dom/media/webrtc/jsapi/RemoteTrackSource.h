/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_WEBRTC_JSAPI_REMOTETRACKSOURCE_H_
#define DOM_MEDIA_WEBRTC_JSAPI_REMOTETRACKSOURCE_H_

#include "MediaStreamTrack.h"

namespace mozilla {

namespace dom {
class RTCRtpReceiver;
}

class SourceMediaTrack;

class RemoteTrackSource : public dom::MediaStreamTrackSource {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(RemoteTrackSource,
                                           dom::MediaStreamTrackSource)

  RemoteTrackSource(SourceMediaTrack* aStream, dom::RTCRtpReceiver* aReceiver,
                    nsIPrincipal* aPrincipal, const nsString& aLabel,
                    TrackingId aTrackingId);

  void Destroy() override;

  dom::MediaSourceEnum GetMediaSource() const override {
    return dom::MediaSourceEnum::Other;
  }

  RefPtr<ApplyConstraintsPromise> ApplyConstraints(
      const dom::MediaTrackConstraints& aConstraints,
      dom::CallerType aCallerType) override;

  void Stop() override {
    // XXX (Bug 1314270): Implement rejection logic if necessary when we have
    //                    clarity in the spec.
  }

  void Disable() override {}

  void Enable() override {}

  void SetPrincipal(nsIPrincipal* aPrincipal);
  void SetMuted(bool aMuted);
  void ForceEnded();

  SourceMediaTrack* Stream() const;

 private:
  virtual ~RemoteTrackSource();

  RefPtr<SourceMediaTrack> mStream;
  RefPtr<dom::RTCRtpReceiver> mReceiver;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_WEBRTC_JSAPI_REMOTETRACKSOURCE_H_
