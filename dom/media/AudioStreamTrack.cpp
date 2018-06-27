/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioStreamTrack.h"

#include "nsContentUtils.h"

#include "mozilla/dom/AudioStreamTrackBinding.h"

namespace mozilla {
namespace dom {

JSObject*
AudioStreamTrack::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return AudioStreamTrack_Binding::Wrap(aCx, this, aGivenProto);
}

void
AudioStreamTrack::GetLabel(nsAString& aLabel, CallerType aCallerType)
{
  if (nsContentUtils::ResistFingerprinting(aCallerType)) {
    aLabel.AssignLiteral("Internal Microphone");
    return;
  }
  MediaStreamTrack::GetLabel(aLabel, aCallerType);
}

} // namespace dom
} // namespace mozilla
