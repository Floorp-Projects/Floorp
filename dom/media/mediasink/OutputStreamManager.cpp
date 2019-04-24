/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OutputStreamManager.h"

#include "DOMMediaStream.h"
#include "MediaStreamGraph.h"
#include "mozilla/dom/MediaStreamTrack.h"
#include "nsContentUtils.h"

namespace mozilla {

#define LOG(level, msg, ...) \
  MOZ_LOG(gMediaDecoderLog, level, (msg, ##__VA_ARGS__))

class DecodedStreamTrackSource : public dom::MediaStreamTrackSource {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DecodedStreamTrackSource,
                                           dom::MediaStreamTrackSource)

  explicit DecodedStreamTrackSource(OutputStreamManager* aManager,
                                    OutputStreamData* aData, TrackID aTrackID,
                                    nsIPrincipal* aPrincipal,
                                    CORSMode aCORSMode,
                                    AbstractThread* aAbstractMainThread)
      : dom::MediaStreamTrackSource(aPrincipal, nsString()),
        mCORSMode(aCORSMode) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  dom::MediaSourceEnum GetMediaSource() const override {
    return dom::MediaSourceEnum::Other;
  }

  CORSMode GetCORSMode() const override {
    MOZ_ASSERT(NS_IsMainThread());
    return mCORSMode;
  }

  void Stop() override {
    MOZ_ASSERT(NS_IsMainThread());

    // We don't notify the source that a track was stopped since it will keep
    // producing tracks until the element ends. The decoder also needs the
    // tracks it created to be live at the source since the decoder's clock is
    // based on MediaStreams during capture.
  }

  void Disable() override {}

  void Enable() override {}

  void SetPrincipal(nsIPrincipal* aPrincipal) {
    MOZ_ASSERT(NS_IsMainThread());
    mPrincipal = aPrincipal;
    PrincipalChanged();
  }

 protected:
  virtual ~DecodedStreamTrackSource() { MOZ_ASSERT(NS_IsMainThread()); }

  const CORSMode mCORSMode;
};

NS_IMPL_ADDREF_INHERITED(DecodedStreamTrackSource, dom::MediaStreamTrackSource)
NS_IMPL_RELEASE_INHERITED(DecodedStreamTrackSource, dom::MediaStreamTrackSource)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DecodedStreamTrackSource)
NS_INTERFACE_MAP_END_INHERITING(dom::MediaStreamTrackSource)
NS_IMPL_CYCLE_COLLECTION_INHERITED(DecodedStreamTrackSource,
                                   dom::MediaStreamTrackSource)

OutputStreamData::OutputStreamData(OutputStreamManager* aManager,
                                   AbstractThread* aAbstractMainThread,
                                   DOMMediaStream* aDOMStream)
    : mManager(aManager),
      mAbstractMainThread(aAbstractMainThread),
      mDOMStream(aDOMStream),
      mInputStream(mDOMStream->GetInputStream()->AsProcessedStream()),
      mPort(mInputStream->AllocateInputPort(mManager->mSourceStream)) {
  MOZ_ASSERT(NS_IsMainThread());
}

OutputStreamData::~OutputStreamData() {
  MOZ_ASSERT(NS_IsMainThread());

  // During cycle collection, MediaStream can be destroyed and send
  // its Destroy message before this decoder is destroyed. So we have to
  // be careful not to send any messages after the Destroy().
  if (mInputStream->IsDestroyed()) {
    return;
  }

  // Disconnect any existing port.
  if (mPort) {
    mPort->Destroy();
  }
}

void OutputStreamData::AddTrack(TrackID aTrackID, MediaSegment::Type aType,
                                nsIPrincipal* aPrincipal, CORSMode aCORSMode,
                                bool aAsyncAddTrack) {
  MOZ_ASSERT(NS_IsMainThread());

  LOG(LogLevel::Debug, "Adding output %s track with id %d to MediaStream %p%s",
      aType == MediaSegment::AUDIO ? "audio" : "video", aTrackID,
      mDOMStream.get(), aAsyncAddTrack ? " (async)" : "");

  RefPtr<dom::MediaStreamTrackSource> source = new DecodedStreamTrackSource(
      mManager, this, aTrackID, aPrincipal, aCORSMode, mAbstractMainThread);
  RefPtr<dom::MediaStreamTrack> track =
      mDOMStream->CreateDOMTrack(aTrackID, aType, source);
  mTracks.AppendElement(track);
  if (aAsyncAddTrack) {
    GetMainThreadEventTarget()->Dispatch(
        NewRunnableMethod<RefPtr<dom::MediaStreamTrack>>(
            "DOMMediaStream::AddTrackInternal", mDOMStream,
            &DOMMediaStream::AddTrackInternal, track));
  } else {
    mDOMStream->AddTrackInternal(track);
  }
}

void OutputStreamData::RemoveTrack(TrackID aTrackID) {
  MOZ_ASSERT(NS_IsMainThread());

  LOG(LogLevel::Debug, "Removing output track with id %d from MediaStream %p",
      aTrackID, mDOMStream.get());

  RefPtr<dom::MediaStreamTrack> track =
      mDOMStream->FindOwnedDOMTrack(mInputStream, aTrackID);
  MOZ_DIAGNOSTIC_ASSERT(track);
  mTracks.RemoveElement(track);
  GetMainThreadEventTarget()->Dispatch(
      NewRunnableMethod("MediaStreamTrack::OverrideEnded", track,
                        &dom::MediaStreamTrack::OverrideEnded));
}

void OutputStreamData::SetPrincipal(nsIPrincipal* aPrincipal) {
  for (const RefPtr<dom::MediaStreamTrack>& track : mTracks) {
    DecodedStreamTrackSource& source =
        static_cast<DecodedStreamTrackSource&>(track->GetSource());
    source.SetPrincipal(aPrincipal);
  }
}

OutputStreamManager::OutputStreamManager(SourceMediaStream* aSourceStream,
                                         TrackID aNextTrackID,
                                         nsIPrincipal* aPrincipal,
                                         CORSMode aCORSMode,
                                         AbstractThread* aAbstractMainThread)
    : mSourceStream(aSourceStream),
      mAbstractMainThread(aAbstractMainThread),
      mPrincipalHandle(
          aAbstractMainThread,
          aPrincipal ? MakePrincipalHandle(aPrincipal) : PRINCIPAL_HANDLE_NONE,
          "OutputStreamManager::mPrincipalHandle (Canonical)"),
      mPrincipal(aPrincipal),
      mCORSMode(aCORSMode),
      mNextTrackID(aNextTrackID),
      mPlaying(true)  // mSourceStream always starts non-suspended
{
  MOZ_ASSERT(NS_IsMainThread());
}

void OutputStreamManager::Add(DOMMediaStream* aDOMStream) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mSourceStream->IsDestroyed());
  // All streams must belong to the same graph.
  MOZ_ASSERT(mSourceStream->Graph() == aDOMStream->GetInputStream()->Graph());

  LOG(LogLevel::Info, "Adding MediaStream %p", aDOMStream);

  OutputStreamData* p = mStreams
                            .AppendElement(new OutputStreamData(
                                this, mAbstractMainThread, aDOMStream))
                            ->get();
  for (const Pair<TrackID, MediaSegment::Type>& pair : mLiveTracks) {
    p->AddTrack(pair.first(), pair.second(), mPrincipal, mCORSMode, false);
  }
}

void OutputStreamManager::Remove(DOMMediaStream* aDOMStream) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mSourceStream->IsDestroyed());

  LOG(LogLevel::Info, "Removing MediaStream %p", aDOMStream);

  mStreams.ApplyIf(
      aDOMStream, 0, StreamComparator(),
      [&](const UniquePtr<OutputStreamData>& aData) {
        for (const Pair<TrackID, MediaSegment::Type>& pair : mLiveTracks) {
          aData->RemoveTrack(pair.first());
        }
      },
      []() { MOZ_ASSERT_UNREACHABLE("Didn't exist"); });
  DebugOnly<bool> rv = mStreams.RemoveElement(aDOMStream, StreamComparator());
  MOZ_ASSERT(rv);
}

bool OutputStreamManager::HasTrackType(MediaSegment::Type aType) {
  MOZ_ASSERT(NS_IsMainThread());

  return mLiveTracks.Contains(aType, TrackTypeComparator());
}

bool OutputStreamManager::HasTracks(TrackID aAudioTrack, TrackID aVideoTrack) {
  MOZ_ASSERT(NS_IsMainThread());

  size_t nrExpectedTracks = 0;
  bool asExpected = true;
  if (IsTrackIDExplicit(aAudioTrack)) {
    Unused << ++nrExpectedTracks;
    asExpected = asExpected && mLiveTracks.Contains(
                                   MakePair(aAudioTrack, MediaSegment::AUDIO),
                                   TrackComparator());
  }
  if (IsTrackIDExplicit(aVideoTrack)) {
    Unused << ++nrExpectedTracks;
    asExpected = asExpected && mLiveTracks.Contains(
                                   MakePair(aVideoTrack, MediaSegment::VIDEO),
                                   TrackComparator());
  }
  asExpected = asExpected && mLiveTracks.Length() == nrExpectedTracks;
  return asExpected;
}

size_t OutputStreamManager::NumberOfTracks() {
  MOZ_ASSERT(NS_IsMainThread());
  return mLiveTracks.Length();
}

void OutputStreamManager::AddTrack(MediaSegment::Type aType) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mSourceStream->IsDestroyed());
  MOZ_ASSERT(!HasTrackType(aType),
             "Cannot have two tracks of the same type at the same time");

  TrackID id = mNextTrackID++;

  LOG(LogLevel::Info, "Adding %s track with id %d",
      aType == MediaSegment::AUDIO ? "audio" : "video", id);

  mLiveTracks.AppendElement(MakePair(id, aType));
  for (const auto& data : mStreams) {
    data->AddTrack(id, aType, mPrincipal, mCORSMode, true);
  }
}

void OutputStreamManager::RemoveTrack(TrackID aTrackID) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mSourceStream->IsDestroyed());
  LOG(LogLevel::Info, "Removing track with id %d", aTrackID);
  DebugOnly<bool> rv = mLiveTracks.RemoveElement(aTrackID, TrackIDComparator());
  MOZ_ASSERT(rv);
  for (const auto& data : mStreams) {
    data->RemoveTrack(aTrackID);
  }
}

void OutputStreamManager::RemoveTracks() {
  MOZ_ASSERT(NS_IsMainThread());
  nsTArray<Pair<TrackID, MediaSegment::Type>> liveTracks(mLiveTracks);
  for (const auto& pair : liveTracks) {
    RemoveTrack(pair.first());
  }
}

void OutputStreamManager::Disconnect() {
  MOZ_ASSERT(NS_IsMainThread());
  RemoveTracks();
  MOZ_ASSERT(mLiveTracks.IsEmpty());
  nsTArray<RefPtr<DOMMediaStream>> domStreams(mStreams.Length());
  for (const auto& data : mStreams) {
    domStreams.AppendElement(data->mDOMStream);
  }
  for (auto& domStream : domStreams) {
    Remove(domStream);
  }
  MOZ_ASSERT(mStreams.IsEmpty());
  if (!mSourceStream->IsDestroyed()) {
    mSourceStream->Destroy();
  }
}

AbstractCanonical<PrincipalHandle>*
OutputStreamManager::CanonicalPrincipalHandle() {
  return &mPrincipalHandle;
}

void OutputStreamManager::SetPrincipal(nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIPrincipal> principal = mPrincipal;
  if (nsContentUtils::CombineResourcePrincipals(&principal, aPrincipal)) {
    mPrincipal = principal;
    for (const UniquePtr<OutputStreamData>& data : mStreams) {
      data->SetPrincipal(mPrincipal);
    }
    mPrincipalHandle = MakePrincipalHandle(principal);
  }
}

TrackID OutputStreamManager::NextTrackID() const {
  MOZ_ASSERT(NS_IsMainThread());
  return mNextTrackID;
}

TrackID OutputStreamManager::GetLiveTrackIDFor(MediaSegment::Type aType) const {
  MOZ_ASSERT(NS_IsMainThread());
  for (const auto& pair : mLiveTracks) {
    if (pair.second() == aType) {
      return pair.first();
    }
  }
  return TRACK_NONE;
}

void OutputStreamManager::SetPlaying(bool aPlaying) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mPlaying == aPlaying) {
    return;
  }

  mPlaying = aPlaying;
  if (mPlaying) {
    mSourceStream->Resume();
  } else {
    mSourceStream->Suspend();
  }
}

#undef LOG

}  // namespace mozilla
