/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteTrackSource.h"

#include "MediaStreamError.h"
#include "MediaTrackGraph.h"
#include "RTCRtpReceiver.h"

namespace mozilla {

NS_IMPL_ADDREF_INHERITED(RemoteTrackSource, dom::MediaStreamTrackSource)
NS_IMPL_RELEASE_INHERITED(RemoteTrackSource, dom::MediaStreamTrackSource)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(RemoteTrackSource)
NS_INTERFACE_MAP_END_INHERITING(dom::MediaStreamTrackSource)
NS_IMPL_CYCLE_COLLECTION_CLASS(RemoteTrackSource)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(RemoteTrackSource,
                                                dom::MediaStreamTrackSource)
  tmp->Destroy();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mReceiver)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(RemoteTrackSource,
                                                  dom::MediaStreamTrackSource)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mReceiver)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

RemoteTrackSource::RemoteTrackSource(SourceMediaTrack* aStream,
                                     dom::RTCRtpReceiver* aReceiver,
                                     nsIPrincipal* aPrincipal,
                                     const nsString& aLabel,
                                     TrackingId aTrackingId)
    : dom::MediaStreamTrackSource(aPrincipal, aLabel, std::move(aTrackingId)),
      mStream(aStream),
      mReceiver(aReceiver) {}

RemoteTrackSource::~RemoteTrackSource() { Destroy(); }

void RemoteTrackSource::Destroy() {
  if (mStream) {
    MOZ_ASSERT(!mStream->IsDestroyed());
    mStream->End();
    mStream->Destroy();
    mStream = nullptr;

    GetMainThreadSerialEventTarget()->Dispatch(NewRunnableMethod(
        "RemoteTrackSource::ForceEnded", this, &RemoteTrackSource::ForceEnded));
  }
}

auto RemoteTrackSource::ApplyConstraints(
    const dom::MediaTrackConstraints& aConstraints, dom::CallerType aCallerType)
    -> RefPtr<ApplyConstraintsPromise> {
  return ApplyConstraintsPromise::CreateAndReject(
      MakeRefPtr<MediaMgrError>(
          dom::MediaStreamError::Name::OverconstrainedError, ""),
      __func__);
}

void RemoteTrackSource::SetPrincipal(nsIPrincipal* aPrincipal) {
  mPrincipal = aPrincipal;
  PrincipalChanged();
}

void RemoteTrackSource::SetMuted(bool aMuted) { MutedChanged(aMuted); }

void RemoteTrackSource::ForceEnded() { OverrideEnded(); }

SourceMediaTrack* RemoteTrackSource::Stream() const { return mStream; }

}  // namespace mozilla
