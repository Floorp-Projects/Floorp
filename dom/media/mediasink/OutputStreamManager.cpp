/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OutputStreamManager.h"

#include "DOMMediaStream.h"
#include "../MediaTrackGraph.h"
#include "mozilla/dom/MediaStreamTrack.h"
#include "mozilla/dom/AudioStreamTrack.h"
#include "mozilla/dom/VideoStreamTrack.h"
#include "nsContentUtils.h"

namespace mozilla {

#define LOG(level, msg, ...) \
  MOZ_LOG(gMediaDecoderLog, level, (msg, ##__VA_ARGS__))

class DecodedStreamTrackSource : public dom::MediaStreamTrackSource {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DecodedStreamTrackSource,
                                           dom::MediaStreamTrackSource)

  explicit DecodedStreamTrackSource(SourceMediaTrack* aSourceStream,
                                    nsIPrincipal* aPrincipal)
      : dom::MediaStreamTrackSource(aPrincipal, nsString()),
        mTrack(aSourceStream->Graph()->CreateForwardedInputTrack(
            aSourceStream->mType)),
        mPort(mTrack->AllocateInputPort(aSourceStream)) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  dom::MediaSourceEnum GetMediaSource() const override {
    return dom::MediaSourceEnum::Other;
  }

  void Stop() override {
    MOZ_ASSERT(NS_IsMainThread());

    // We don't notify the source that a track was stopped since it will keep
    // producing tracks until the element ends. The decoder also needs the
    // tracks it created to be live at the source since the decoder's clock is
    // based on MediaStreams during capture. We do however, disconnect this
    // track's underlying track.
    if (!mTrack->IsDestroyed()) {
      mTrack->Destroy();
      mPort->Destroy();
    }
  }

  void Disable() override {}

  void Enable() override {}

  void SetPrincipal(nsIPrincipal* aPrincipal) {
    MOZ_ASSERT(NS_IsMainThread());
    mPrincipal = aPrincipal;
    PrincipalChanged();
  }

  void ForceEnded() { OverrideEnded(); }

  const RefPtr<ProcessedMediaTrack> mTrack;
  const RefPtr<MediaInputPort> mPort;

 protected:
  virtual ~DecodedStreamTrackSource() {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mTrack->IsDestroyed());
  }
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
      mDOMStream(aDOMStream) {
  MOZ_ASSERT(NS_IsMainThread());
}

OutputStreamData::~OutputStreamData() = default;

void OutputStreamData::AddTrack(SourceMediaTrack* aTrack,
                                MediaSegment::Type aType,
                                nsIPrincipal* aPrincipal, bool aAsyncAddTrack) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(mDOMStream);

  LOG(LogLevel::Debug,
      "Adding output %s track sourced from track %p to MediaStream %p%s",
      aType == MediaSegment::AUDIO ? "audio" : "video", aTrack,
      mDOMStream.get(), aAsyncAddTrack ? " (async)" : "");

  auto source = MakeRefPtr<DecodedStreamTrackSource>(aTrack, aPrincipal);
  RefPtr<dom::MediaStreamTrack> track;
  if (aType == MediaSegment::AUDIO) {
    track = new dom::AudioStreamTrack(mDOMStream->GetParentObject(),
                                      source->mTrack, source);
  } else {
    MOZ_ASSERT(aType == MediaSegment::VIDEO);
    track = new dom::VideoStreamTrack(mDOMStream->GetParentObject(),
                                      source->mTrack, source);
  }
  mTracks.AppendElement(track.get());
  if (aAsyncAddTrack) {
    GetMainThreadEventTarget()->Dispatch(
        NewRunnableMethod<RefPtr<dom::MediaStreamTrack>>(
            "DOMMediaStream::AddTrackInternal", mDOMStream.get(),
            &DOMMediaStream::AddTrackInternal, track));
  } else {
    mDOMStream->AddTrackInternal(track);
  }
}

void OutputStreamData::RemoveTrack(SourceMediaTrack* aTrack) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(mDOMStream);

  LOG(LogLevel::Debug,
      "Removing output track sourced by track %p from MediaStream %p", aTrack,
      mDOMStream.get());

  for (const auto& t : nsTArray<WeakPtr<dom::MediaStreamTrack>>(mTracks)) {
    mTracks.RemoveElement(t);
    if (!t || t->Ended()) {
      continue;
    }
    DecodedStreamTrackSource& source =
        static_cast<DecodedStreamTrackSource&>(t->GetSource());
    GetMainThreadEventTarget()->Dispatch(
        NewRunnableMethod("DecodedStreamTrackSource::ForceEnded", &source,
                          &DecodedStreamTrackSource::ForceEnded));
  }
}

void OutputStreamData::SetPrincipal(nsIPrincipal* aPrincipal) {
  MOZ_DIAGNOSTIC_ASSERT(mDOMStream);
  for (const WeakPtr<dom::MediaStreamTrack>& track : mTracks) {
    if (!track || track->Ended()) {
      continue;
    }
    DecodedStreamTrackSource& source =
        static_cast<DecodedStreamTrackSource&>(track->GetSource());
    source.SetPrincipal(aPrincipal);
  }
}

OutputStreamManager::OutputStreamManager(SharedDummyTrack* aDummyStream,
                                         nsIPrincipal* aPrincipal,
                                         AbstractThread* aAbstractMainThread)
    : mAbstractMainThread(aAbstractMainThread),
      mDummyStream(aDummyStream),
      mPrincipalHandle(
          aAbstractMainThread,
          aPrincipal ? MakePrincipalHandle(aPrincipal) : PRINCIPAL_HANDLE_NONE,
          "OutputStreamManager::mPrincipalHandle (Canonical)") {
  MOZ_ASSERT(NS_IsMainThread());
}

void OutputStreamManager::Add(DOMMediaStream* aDOMStream) {
  MOZ_ASSERT(NS_IsMainThread());

  LOG(LogLevel::Info, "Adding MediaStream %p", aDOMStream);

  OutputStreamData* p = mStreams
                            .AppendElement(new OutputStreamData(
                                this, mAbstractMainThread, aDOMStream))
                            ->get();
  for (const auto& lt : mLiveTracks) {
    p->AddTrack(lt->mSourceTrack, lt->mType, mPrincipalHandle.Ref(), false);
  }
}

void OutputStreamManager::Remove(DOMMediaStream* aDOMStream) {
  MOZ_ASSERT(NS_IsMainThread());

  LOG(LogLevel::Info, "Removing MediaStream %p", aDOMStream);

  AutoRemoveDestroyedStreams();
  mStreams.ApplyIf(
      aDOMStream, 0, StreamComparator(),
      [&](const UniquePtr<OutputStreamData>& aData) {
        for (const auto& lt : mLiveTracks) {
          aData->RemoveTrack(lt->mSourceTrack);
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

bool OutputStreamManager::HasTracks(SourceMediaTrack* aAudioStream,
                                    SourceMediaTrack* aVideoStream) {
  MOZ_ASSERT(NS_IsMainThread());

  size_t nrExpectedTracks = 0;
  bool asExpected = true;
  if (aAudioStream) {
    Unused << ++nrExpectedTracks;
    asExpected = asExpected && mLiveTracks.Contains(
                                   MakePair(aAudioStream, MediaSegment::AUDIO),
                                   TrackComparator());
  }
  if (aVideoStream) {
    Unused << ++nrExpectedTracks;
    asExpected = asExpected && mLiveTracks.Contains(
                                   MakePair(aVideoStream, MediaSegment::VIDEO),
                                   TrackComparator());
  }
  asExpected = asExpected && mLiveTracks.Length() == nrExpectedTracks;
  return asExpected;
}

SourceMediaTrack* OutputStreamManager::GetPrecreatedTrackOfType(
    MediaSegment::Type aType) const {
  auto i = mLiveTracks.IndexOf(aType, 0, PrecreatedTrackTypeComparator());
  return i == nsTArray<UniquePtr<LiveTrack>>::NoIndex
             ? nullptr
             : mLiveTracks[i]->mSourceTrack.get();
}

size_t OutputStreamManager::NumberOfTracks() {
  MOZ_ASSERT(NS_IsMainThread());
  return mLiveTracks.Length();
}

already_AddRefed<SourceMediaTrack> OutputStreamManager::AddTrack(
    MediaSegment::Type aType) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!HasTrackType(aType),
             "Cannot have two tracks of the same type at the same time");

  RefPtr<SourceMediaTrack> track =
      mDummyStream->mTrack->Graph()->CreateSourceTrack(aType);
  if (!mPlaying) {
    track->Suspend();
  }

  LOG(LogLevel::Info, "Adding %s track sourced by track %p",
      aType == MediaSegment::AUDIO ? "audio" : "video", track.get());

  mLiveTracks.AppendElement(MakeUnique<LiveTrack>(track, aType));
  AutoRemoveDestroyedStreams();
  for (const auto& data : mStreams) {
    data->AddTrack(track, aType, mPrincipalHandle.Ref(), true);
  }

  return track.forget();
}

OutputStreamManager::LiveTrack::LiveTrack(SourceMediaTrack* aSourceTrack,
                                          MediaSegment::Type aType)
    : mSourceTrack(aSourceTrack), mType(aType) {}

OutputStreamManager::LiveTrack::~LiveTrack() { mSourceTrack->Destroy(); }

void OutputStreamManager::AutoRemoveDestroyedStreams() {
  MOZ_ASSERT(NS_IsMainThread());
  for (size_t i = mStreams.Length(); i > 0; --i) {
    const auto& data = mStreams[i - 1];
    if (!data->mDOMStream) {
      // If the mDOMStream WeakPtr is now null, mDOMStream has been destructed.
      mStreams.RemoveElementAt(i - 1);
    }
  }
}

void OutputStreamManager::RemoveTrack(SourceMediaTrack* aTrack) {
  MOZ_ASSERT(NS_IsMainThread());
  LOG(LogLevel::Info, "Removing track with source track %p", aTrack);
  DebugOnly<bool> rv =
      mLiveTracks.RemoveElement(aTrack, TrackStreamComparator());
  MOZ_ASSERT(rv);
  AutoRemoveDestroyedStreams();
  for (const auto& data : mStreams) {
    data->RemoveTrack(aTrack);
  }
}

void OutputStreamManager::RemoveTracks() {
  MOZ_ASSERT(NS_IsMainThread());
  for (size_t i = mLiveTracks.Length(); i > 0; --i) {
    RemoveTrack(mLiveTracks[i - 1]->mSourceTrack);
  }
}

void OutputStreamManager::Disconnect() {
  MOZ_ASSERT(NS_IsMainThread());
  RemoveTracks();
  MOZ_ASSERT(mLiveTracks.IsEmpty());
  AutoRemoveDestroyedStreams();
  nsTArray<RefPtr<DOMMediaStream>> domStreams(mStreams.Length());
  for (const auto& data : mStreams) {
    domStreams.AppendElement(data->mDOMStream);
  }
  for (auto& domStream : domStreams) {
    Remove(domStream);
  }
  MOZ_ASSERT(mStreams.IsEmpty());
}

AbstractCanonical<PrincipalHandle>*
OutputStreamManager::CanonicalPrincipalHandle() {
  return &mPrincipalHandle;
}

void OutputStreamManager::SetPrincipal(nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIPrincipal> principal = GetPrincipalFromHandle(mPrincipalHandle);
  if (nsContentUtils::CombineResourcePrincipals(&principal, aPrincipal)) {
    AutoRemoveDestroyedStreams();
    for (const UniquePtr<OutputStreamData>& data : mStreams) {
      data->SetPrincipal(principal);
    }
    mPrincipalHandle = MakePrincipalHandle(principal);
  }
}

void OutputStreamManager::SetPlaying(bool aPlaying) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mPlaying == aPlaying) {
    return;
  }

  mPlaying = aPlaying;
  for (auto& lt : mLiveTracks) {
    if (mPlaying) {
      lt->mSourceTrack->Resume();
      lt->mEverPlayed = true;
    } else {
      lt->mSourceTrack->Suspend();
    }
  }
}

OutputStreamManager::~OutputStreamManager() = default;

#undef LOG

}  // namespace mozilla
