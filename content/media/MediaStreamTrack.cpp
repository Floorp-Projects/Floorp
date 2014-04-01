/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamTrack.h"

#include "DOMMediaStream.h"
#include "nsIUUIDGenerator.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace dom {

MediaStreamTrack::MediaStreamTrack(DOMMediaStream* aStream, TrackID aTrackID)
  : mStream(aStream), mTrackID(aTrackID), mEnded(false), mEnabled(true)
{
  SetIsDOMBinding();

  memset(&mID, 0, sizeof(mID));

  nsresult rv;
  nsCOMPtr<nsIUUIDGenerator> uuidgen =
    do_GetService("@mozilla.org/uuid-generator;1", &rv);
  if (uuidgen) {
    uuidgen->GenerateUUIDInPlace(&mID);
  }
}

MediaStreamTrack::~MediaStreamTrack()
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(MediaStreamTrack, DOMEventTargetHelper,
                                     mStream)

NS_IMPL_ADDREF_INHERITED(MediaStreamTrack, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MediaStreamTrack, DOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MediaStreamTrack)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

void
MediaStreamTrack::GetId(nsAString& aID)
{
  char chars[NSID_LENGTH];
  mID.ToProvidedString(chars);
  aID = NS_ConvertASCIItoUTF16(chars);
}

void
MediaStreamTrack::SetEnabled(bool aEnabled)
{
  mEnabled = aEnabled;
  mStream->SetTrackEnabled(mTrackID, aEnabled);
}

}
}
