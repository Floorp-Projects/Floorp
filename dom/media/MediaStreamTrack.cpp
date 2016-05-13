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

NS_IMPL_CYCLE_COLLECTION_CLASS(MediaStreamTrackSource)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(MediaStreamTrackSource)
  tmp->Destroy();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPrincipal)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(MediaStreamTrackSource)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPrincipal)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

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

/**
 * PrincipalHandleListener monitors changes in PrincipalHandle of the media flowing
 * through the MediaStreamGraph.
 *
 * When the main thread principal for a MediaStreamTrack changes, its principal
 * will be set to the combination of the previous principal and the new one.
 *
 * As a PrincipalHandle change later happens on the MediaStreamGraph thread, we will
 * be notified. If the latest principal on main thread matches the PrincipalHandle
 * we just saw on MSG thread, we will set the track's principal to the new one.
 *
 * We know at this point that the old principal has been flushed out and data
 * under it cannot leak to consumers.
 *
 * In case of multiple changes to the main thread state, the track's principal
 * will be a combination of its old principal and all the new ones until the
 * latest main thread principal matches the PrincipalHandle on the MSG thread.
 */
class MediaStreamTrack::PrincipalHandleListener : public MediaStreamTrackListener
{
public:
  explicit PrincipalHandleListener(MediaStreamTrack* aTrack)
    : mTrack(aTrack) {}

  void Forget()
  {
    MOZ_ASSERT(NS_IsMainThread());
    mTrack = nullptr;
  }

  void DoNotifyPrincipalHandleChanged(const PrincipalHandle& aNewPrincipalHandle)
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (!mTrack) {
      return;
    }

    mTrack->NotifyPrincipalHandleChanged(aNewPrincipalHandle);
  }

  void NotifyPrincipalHandleChanged(MediaStreamGraph* aGraph,
                                    const PrincipalHandle& aNewPrincipalHandle) override
  {
    nsCOMPtr<nsIRunnable> runnable =
      NewRunnableMethod<StoreCopyPassByConstLRef<PrincipalHandle>>(
        this, &PrincipalHandleListener::DoNotifyPrincipalHandleChanged, aNewPrincipalHandle);
    aGraph->DispatchToMainThreadAfterStreamStateUpdate(runnable.forget());
  }

protected:
  // These fields may only be accessed on the main thread
  MediaStreamTrack* mTrack;
};

MediaStreamTrack::MediaStreamTrack(DOMMediaStream* aStream, TrackID aTrackID,
                                   TrackID aInputTrackID,
                                   MediaStreamTrackSource* aSource)
  : mOwningStream(aStream), mTrackID(aTrackID),
    mInputTrackID(aInputTrackID), mSource(aSource),
    mPrincipal(aSource->GetPrincipal()),
    mEnded(false), mEnabled(true), mRemote(aSource->IsRemote()), mStopped(false)
{

  if (!gMediaStreamTrackLog) {
    gMediaStreamTrackLog = PR_NewLogModule("MediaStreamTrack");
  }

  GetSource().RegisterSink(this);

  mPrincipalHandleListener = new PrincipalHandleListener(this);
  AddListener(mPrincipalHandleListener);

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
  Destroy();
}

void
MediaStreamTrack::Destroy()
{
  if (mSource) {
    mSource->UnregisterSink(this);
  }
  if (mPrincipalHandleListener) {
    if (GetOwnedStream()) {
      RemoveListener(mPrincipalHandleListener);
    }
    mPrincipalHandleListener->Forget();
    mPrincipalHandleListener = nullptr;
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(MediaStreamTrack)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(MediaStreamTrack,
                                                DOMEventTargetHelper)
  tmp->Destroy();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOwningStream)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSource)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOriginalTrack)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPrincipal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPendingPrincipal)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(MediaStreamTrack,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwningStream)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSource)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOriginalTrack)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPrincipal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPendingPrincipal)
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
  GetOwnedStream()->SetTrackEnabled(mTrackID, aEnabled);
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

  MOZ_ASSERT(mOwningStream, "Every MediaStreamTrack needs an owning DOMMediaStream");
  DOMMediaStream::TrackPort* port = mOwningStream->FindOwnedTrackPort(*this);
  MOZ_ASSERT(port, "A MediaStreamTrack must exist in its owning DOMMediaStream");
  RefPtr<Pledge<bool>> p = port->BlockSourceTrackId(mInputTrackID);
  Unused << p;

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

MediaStreamGraphImpl*
MediaStreamTrack::GraphImpl()
{
  return GetOwnedStream()->GraphImpl();
}

void
MediaStreamTrack::SetPrincipal(nsIPrincipal* aPrincipal)
{
  if (aPrincipal == mPrincipal) {
    return;
  }
  mPrincipal = aPrincipal;

  LOG(LogLevel::Info, ("MediaStreamTrack %p principal changed to %p. Now: "
                       "null=%d, codebase=%d, expanded=%d, system=%d",
                       this, mPrincipal.get(),
                       mPrincipal->GetIsNullPrincipal(),
                       mPrincipal->GetIsCodebasePrincipal(),
                       mPrincipal->GetIsExpandedPrincipal(),
                       mPrincipal->GetIsSystemPrincipal()));
  for (PrincipalChangeObserver<MediaStreamTrack>* observer
      : mPrincipalChangeObservers) {
    observer->PrincipalChanged(this);
  }
}

void
MediaStreamTrack::PrincipalChanged()
{
  mPendingPrincipal = GetSource().GetPrincipal();
  nsCOMPtr<nsIPrincipal> newPrincipal = mPrincipal;
  LOG(LogLevel::Info, ("MediaStreamTrack %p Principal changed on main thread "
                       "to %p (pending). Combining with existing principal %p.",
                       this, mPendingPrincipal.get(), mPrincipal.get()));
  if (nsContentUtils::CombineResourcePrincipals(&newPrincipal,
                                                mPendingPrincipal)) {
    SetPrincipal(newPrincipal);
  }
}

void
MediaStreamTrack::NotifyPrincipalHandleChanged(const PrincipalHandle& aNewPrincipalHandle)
{
  PrincipalHandle handle(aNewPrincipalHandle);
  LOG(LogLevel::Info, ("MediaStreamTrack %p principalHandle changed on "
                       "MediaStreamGraph thread to %p. Current principal: %p, "
                       "pending: %p",
                       this, GetPrincipalFromHandle(handle),
                       mPrincipal.get(), mPendingPrincipal.get()));
  if (PrincipalHandleMatches(handle, mPendingPrincipal)) {
    SetPrincipal(mPendingPrincipal);
    mPendingPrincipal = nullptr;
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

void
MediaStreamTrack::NotifyEndedImpl()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mEnded) {
    return;
  }

  LOG(LogLevel::Info, ("MediaStreamTrack %p ended", this));

  DispatchTrustedEvent(NS_LITERAL_STRING("ended"));

  mEnded = true;
}

void
MediaStreamTrack::NotifyEnded()
{
  NS_DispatchToMainThread(NewRunnableMethod(this, &MediaStreamTrack::NotifyEndedImpl));
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

void
MediaStreamTrack::AddDirectListener(MediaStreamTrackDirectListener *aListener)
{
  LOG(LogLevel::Debug, ("MediaStreamTrack %p (%s) adding direct listener %p to "
                        "stream %p, track %d",
                        this, AsAudioStreamTrack() ? "audio" : "video",
                        aListener, GetOwnedStream(), mTrackID));

  GetOwnedStream()->AddDirectTrackListener(aListener, mTrackID);
}

void
MediaStreamTrack::RemoveDirectListener(MediaStreamTrackDirectListener *aListener)
{
  LOG(LogLevel::Debug, ("MediaStreamTrack %p removing direct listener %p from stream %p",
                        this, aListener, GetOwnedStream()));

  GetOwnedStream()->RemoveDirectTrackListener(aListener, mTrackID);
}

already_AddRefed<MediaInputPort>
MediaStreamTrack::ForwardTrackContentsTo(ProcessedMediaStream* aStream)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(aStream);
  RefPtr<MediaInputPort> port =
    aStream->AllocateInputPort(GetOwnedStream(), mTrackID);
  return port.forget();
}

bool
MediaStreamTrack::IsForwardedThrough(MediaInputPort* aPort)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPort);
  if (!aPort) {
    return false;
  }
  return aPort->GetSource() == GetOwnedStream() &&
         aPort->PassTrackThrough(mTrackID);
}

} // namespace dom
} // namespace mozilla
