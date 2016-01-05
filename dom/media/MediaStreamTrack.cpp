/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamTrack.h"

#include "DOMMediaStream.h"
#include "MediaStreamGraph.h"
#include "nsIUUIDGenerator.h"
#include "nsServiceManagerUtils.h"

#ifdef LOG
#undef LOG
#endif

static PRLogModuleInfo* gMediaStreamTrackLog;
#define LOG(type, msg) MOZ_LOG(gMediaStreamTrackLog, type, msg)

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTING_ADDREF(MediaStreamTrackSource)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MediaStreamTrackSource)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaStreamTrackSource)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END
NS_IMPL_CYCLE_COLLECTION(MediaStreamTrackSource, mPrincipal)

already_AddRefed<Promise>
MediaStreamTrackSource::ApplyConstraints(nsPIDOMWindowInner* aWindow,
                                         const dom::MediaTrackConstraints& aConstraints,
                                         ErrorResult &aRv)
{
  nsCOMPtr<nsIGlobalObject> go = do_QueryInterface(aWindow);
  RefPtr<Promise> promise = Promise::Create(go, aRv);
  MOZ_RELEASE_ASSERT(!aRv.Failed());

  promise->MaybeReject(new MediaStreamError(
    aWindow,
    NS_LITERAL_STRING("OverconstrainedError"),
    NS_LITERAL_STRING(""),
    NS_LITERAL_STRING("")));
  return promise.forget();
}

MediaStreamTrack::MediaStreamTrack(DOMMediaStream* aStream, TrackID aTrackID,
                                   TrackID aInputTrackID, const nsString& aLabel,
                                   MediaStreamTrackSource* aSource)
  : mOwningStream(aStream), mTrackID(aTrackID),
    mInputTrackID(aInputTrackID), mSource(aSource), mLabel(aLabel),
    mEnded(false), mEnabled(true), mRemote(aSource->IsRemote()), mStopped(false)
{

  if (!gMediaStreamTrackLog) {
    gMediaStreamTrackLog = PR_NewLogModule("MediaStreamTrack");
  }

  GetSource().RegisterSink(this);

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

NS_IMPL_CYCLE_COLLECTION_CLASS(MediaStreamTrack)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(MediaStreamTrack,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOwningStream)
  if (tmp->mSource) {
    tmp->mSource->UnregisterSink(tmp);
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSource)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOriginalTrack)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(MediaStreamTrack,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwningStream)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSource)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOriginalTrack)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(MediaStreamTrack, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MediaStreamTrack, DOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MediaStreamTrack)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

nsPIDOMWindowInner*
MediaStreamTrack::GetParentObject() const
{
  MOZ_RELEASE_ASSERT(mOwningStream);
  return mOwningStream->GetParentObject();
}

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
  // XXX Bug 1208371 - This enables/disables the track across clones.
  GetInputStream()->SetTrackEnabled(mTrackID, aEnabled);
}

void
MediaStreamTrack::Stop()
{
  LOG(LogLevel::Info, ("MediaStreamTrack %p Stop()", this));

  if (mStopped) {
    LOG(LogLevel::Warning, ("MediaStreamTrack %p Already stopped", this));
    return;
  }

  if (mRemote) {
    LOG(LogLevel::Warning, ("MediaStreamTrack %p is remote. Can't be stopped.", this));
    return;
  }

  if (!mSource) {
    MOZ_ASSERT(false);
    return;
  }

  mSource->UnregisterSink(this);
  mStopped = true;
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

  nsPIDOMWindowInner* window = mOwningStream->GetParentObject();
  return GetSource().ApplyConstraints(window, aConstraints, aRv);
}

MediaStreamGraph*
MediaStreamTrack::Graph()
{
  return GetOwnedStream()->Graph();
}

void
MediaStreamTrack::PrincipalChanged()
{
  LOG(LogLevel::Info, ("MediaStreamTrack %p Principal changed. Now: "
                       "null=%d, codebase=%d, expanded=%d, system=%d", this,
                       GetPrincipal()->GetIsNullPrincipal(),
                       GetPrincipal()->GetIsCodebasePrincipal(),
                       GetPrincipal()->GetIsExpandedPrincipal(),
                       GetPrincipal()->GetIsSystemPrincipal()));
  for (PrincipalChangeObserver<MediaStreamTrack>* observer
      : mPrincipalChangeObservers) {
    observer->PrincipalChanged(this);
  }
}

bool
MediaStreamTrack::AddPrincipalChangeObserver(
  PrincipalChangeObserver<MediaStreamTrack>* aObserver)
{
  return mPrincipalChangeObservers.AppendElement(aObserver) != nullptr;
}

bool
MediaStreamTrack::RemovePrincipalChangeObserver(
  PrincipalChangeObserver<MediaStreamTrack>* aObserver)
{
  return mPrincipalChangeObservers.RemoveElement(aObserver);
}

already_AddRefed<MediaStreamTrack>
MediaStreamTrack::Clone()
{
  // MediaStreamTracks are currently governed by streams, so we need a dummy
  // DOMMediaStream to own our track clone. The dummy will never see any
  // dynamically created tracks (no input stream) so no need for a SourceGetter.
  RefPtr<DOMMediaStream> newStream =
    new DOMMediaStream(mOwningStream->GetParentObject(), nullptr);

  MediaStreamGraph* graph = Graph();
  newStream->InitOwnedStreamCommon(graph);
  newStream->InitPlaybackStreamCommon(graph);

  return newStream->CloneDOMTrack(*this, mTrackID);
}

DOMMediaStream*
MediaStreamTrack::GetInputDOMStream()
{
  MediaStreamTrack* originalTrack =
    mOriginalTrack ? mOriginalTrack.get() : this;
  MOZ_RELEASE_ASSERT(originalTrack->mOwningStream);
  return originalTrack->mOwningStream;
}

MediaStream*
MediaStreamTrack::GetInputStream()
{
  DOMMediaStream* inputDOMStream = GetInputDOMStream();
  MOZ_RELEASE_ASSERT(inputDOMStream->GetInputStream());
  return inputDOMStream->GetInputStream();
}

ProcessedMediaStream*
MediaStreamTrack::GetOwnedStream()
{
  return mOwningStream->GetOwnedStream();
}

void
MediaStreamTrack::AddListener(MediaStreamTrackListener* aListener)
{
  LOG(LogLevel::Debug, ("MediaStreamTrack %p adding listener %p",
                        this, aListener));

  GetOwnedStream()->AddTrackListener(aListener, mTrackID);
}

void
MediaStreamTrack::RemoveListener(MediaStreamTrackListener* aListener)
{
  LOG(LogLevel::Debug, ("MediaStreamTrack %p removing listener %p",
                        this, aListener));

  GetOwnedStream()->RemoveTrackListener(aListener, mTrackID);
}

} // namespace dom
} // namespace mozilla
