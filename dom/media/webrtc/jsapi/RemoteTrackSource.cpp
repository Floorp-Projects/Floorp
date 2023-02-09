/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteTrackSource.h"

#include "MediaStreamError.h"
#include "MediaTrackGraph.h"

namespace mozilla {

RemoteTrackSource::RemoteTrackSource(SourceMediaTrack* aStream,
                                     nsIPrincipal* aPrincipal,
                                     const nsString& aLabel,
                                     TrackingId aTrackingId)
    : dom::MediaStreamTrackSource(aPrincipal, aLabel, std::move(aTrackingId)),
      mStream(aStream) {}

RemoteTrackSource::~RemoteTrackSource() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mStream->IsDestroyed());
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

}  // namespace mozilla
