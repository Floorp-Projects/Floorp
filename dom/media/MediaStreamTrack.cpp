/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamTrack.h"

#include "DOMMediaStream.h"
#include "nsIUUIDGenerator.h"
#include "nsServiceManagerUtils.h"

#ifdef LOG
#undef LOG
#endif

static PRLogModuleInfo* gMediaStreamTrackLog;
#define LOG(type, msg) MOZ_LOG(gMediaStreamTrackLog, type, msg)

namespace mozilla {
namespace dom {

MediaStreamTrack::MediaStreamTrack(DOMMediaStream* aStream, TrackID aTrackID, const nsString& aLabel)
  : mOwningStream(aStream), mTrackID(aTrackID), mLabel(aLabel), mEnded(false), mEnabled(true)
{

  if (!gMediaStreamTrackLog) {
    gMediaStreamTrackLog = PR_NewLogModule("MediaStreamTrack");
  }

  nsresult rv;
  nsCOMPtr<nsIUUIDGenerator> uuidgen =
    do_GetService("@mozilla.org/uuid-generator;1", &rv);

  nsID uuid;
  memset(&uuid, 0, sizeof(uuid));
  if (uuidgen) {
    uuidgen->GenerateUUIDInPlace(&uuid);
  }

  char chars[NSID_LENGTH];
  uuid.ToProvidedString(chars);
  mID = NS_ConvertASCIItoUTF16(chars);
}

MediaStreamTrack::~MediaStreamTrack()
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(MediaStreamTrack, DOMEventTargetHelper,
                                   mOwningStream, mOriginalTrack)

NS_IMPL_ADDREF_INHERITED(MediaStreamTrack, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MediaStreamTrack, DOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MediaStreamTrack)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

void
MediaStreamTrack::GetId(nsAString& aID) const
{
  aID = mID;
}

void
MediaStreamTrack::SetEnabled(bool aEnabled)
{
  LOG(LogLevel::Info, ("MediaStreamTrack %p %s",
                       this, aEnabled ? "Enabled" : "Disabled"));

  mEnabled = aEnabled;
  mOwningStream->SetTrackEnabled(mTrackID, aEnabled);
}

void
MediaStreamTrack::Stop()
{
  LOG(LogLevel::Info, ("MediaStreamTrack %p Stop()", this));

  mOwningStream->StopTrack(mTrackID);
}

already_AddRefed<Promise>
MediaStreamTrack::ApplyConstraints(const MediaTrackConstraints& aConstraints,
                                   ErrorResult &aRv)
{
  if (MOZ_LOG_TEST(gMediaStreamTrackLog, LogLevel::Info)) {
    nsString str;
    aConstraints.ToJSON(str);

    LOG(LogLevel::Info, ("MediaStreamTrack %p ApplyConstraints() with "
                         "constraints %s", this, NS_ConvertUTF16toUTF8(str).get()));
  }

  return GetStream()->ApplyConstraintsToTrack(mTrackID, aConstraints, aRv);
}

} // namespace dom
} // namespace mozilla
