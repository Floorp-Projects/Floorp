/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFMediaSource.h"

#include <mfapi.h>
#include <mfidl.h>
#include <stdint.h>

#include "MFMediaEngineAudioStream.h"
#include "MFMediaEngineUtils.h"
#include "MFMediaEngineVideoStream.h"
#include "VideoUtils.h"
#include "WMF.h"
#include "mozilla/Atomics.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/TaskQueue.h"

namespace mozilla {

#define LOG(msg, ...)                         \
  MOZ_LOG(gMFMediaEngineLog, LogLevel::Debug, \
          ("MFMediaSource=%p, " msg, this, ##__VA_ARGS__))

using Microsoft::WRL::ComPtr;

MFMediaSource::MFMediaSource() : mPresentationEnded(false) {}

HRESULT MFMediaSource::RuntimeClassInitialize(const Maybe<AudioInfo>& aAudio,
                                              const Maybe<VideoInfo>& aVideo) {
  static uint64_t streamId = 1;

  mTaskQueue = TaskQueue::Create(
      GetMediaThreadPool(MediaThreadType::PLATFORM_DECODER), "MFMediaSource");
  if (aAudio) {
    mAudioStream = MFMediaEngineAudioStream::Create(streamId++, *aAudio, this);
    if (!mAudioStream) {
      NS_WARNING("Failed to create audio stream");
      return E_FAIL;
    }
    mAudioStreamEndedListener = mAudioStream->EndedEvent().Connect(
        mTaskQueue, this, &MFMediaSource::HandleStreamEnded);
  }

  // TODO : This is for testing. Remove this pref after finishing the video
  // output implementation. Our first step is to make audio playback work.
  if (StaticPrefs::media_wmf_media_engine_video_output_enabled()) {
    if (aVideo) {
      mVideoStream =
          MFMediaEngineVideoStream::Create(streamId++, *aVideo, this);
      if (!mVideoStream) {
        NS_WARNING("Failed to create video stream");
      return E_FAIL;
      }
      mVideoStreamEndedListener = mVideoStream->EndedEvent().Connect(
          mTaskQueue, this, &MFMediaSource::HandleStreamEnded);
    }
  }

  RETURN_IF_FAILED(wmf::MFCreateEventQueue(&mMediaEventQueue));

  LOG("Initialized a media source");
  return S_OK;
}

IFACEMETHODIMP MFMediaSource::GetCharacteristics(DWORD* aCharacteristics) {
  AssertOnMFThreadPool();
  if (mState == State::Shutdowned) {
    return MF_E_SHUTDOWN;
  }

  LOG("GetCharacteristics");
  // https://docs.microsoft.com/en-us/windows/win32/api/mfidl/ne-mfidl-mfmediasource_characteristics
  *aCharacteristics = MFMEDIASOURCE_CAN_SEEK | MFMEDIASOURCE_CAN_PAUSE;
  return S_OK;
}

IFACEMETHODIMP MFMediaSource::CreatePresentationDescriptor(
    IMFPresentationDescriptor** aPresentationDescriptor) {
  AssertOnMFThreadPool();
  if (mState == State::Shutdowned) {
    return MF_E_SHUTDOWN;
  }

  LOG("CreatePresentationDescriptor");
  // See steps of creating the presentation descriptor
  // https://docs.microsoft.com/en-us/windows/win32/medfound/writing-a-custom-media-source#creating-the-presentation-descriptor
  ComPtr<IMFPresentationDescriptor> presentationDescriptor;
  nsTArray<ComPtr<IMFStreamDescriptor>> streamDescriptors;

  DWORD audioDescriptorId = 0, videoDescriptorId = 0;
  if (mAudioStream) {
    ComPtr<IMFStreamDescriptor>* descriptor = streamDescriptors.AppendElement();
    RETURN_IF_FAILED(
        mAudioStream->GetStreamDescriptor(descriptor->GetAddressOf()));
    audioDescriptorId = mAudioStream->DescriptorId();
  }

  if (mVideoStream) {
    ComPtr<IMFStreamDescriptor>* descriptor = streamDescriptors.AppendElement();
    RETURN_IF_FAILED(
        mVideoStream->GetStreamDescriptor(descriptor->GetAddressOf()));
    videoDescriptorId = mVideoStream->DescriptorId();
  }

  const DWORD descCount = static_cast<DWORD>(streamDescriptors.Length());
  MOZ_ASSERT(descCount <= 2);
  RETURN_IF_FAILED(wmf::MFCreatePresentationDescriptor(
      descCount,
      reinterpret_cast<IMFStreamDescriptor**>(streamDescriptors.Elements()),
      &presentationDescriptor));

  // Select default streams for the presentation descriptor.
  for (DWORD idx = 0; idx < descCount; idx++) {
    ComPtr<IMFStreamDescriptor> streamDescriptor;
    BOOL selected;
    RETURN_IF_FAILED(presentationDescriptor->GetStreamDescriptorByIndex(
        idx, &selected, &streamDescriptor));
    if (selected) {
      continue;
    }
    RETURN_IF_FAILED(presentationDescriptor->SelectStream(idx));
    DWORD streamId;
    streamDescriptor->GetStreamIdentifier(&streamId);
    LOG("  Select stream (id=%lu)", streamId);
  }

  LOG("Created a presentation descriptor (a=%lu,v=%lu)", audioDescriptorId,
      videoDescriptorId);
  *aPresentationDescriptor = presentationDescriptor.Detach();
  return S_OK;
}

IFACEMETHODIMP MFMediaSource::Start(
    IMFPresentationDescriptor* aPresentationDescriptor,
    const GUID* aGuidTimeFormat, const PROPVARIANT* aStartPosition) {
  AssertOnMFThreadPool();
  if (mState == State::Shutdowned) {
    return MF_E_SHUTDOWN;
  }

  // See detailed steps in following documents.
  // https://docs.microsoft.com/en-us/windows/win32/api/mfidl/nf-mfidl-imfmediasource-start
  // https://docs.microsoft.com/en-us/windows/win32/medfound/writing-a-custom-media-source#starting-the-media-source

  // A call to Start results in a seek if the previous state was started or
  // paused, and the new starting position is not VT_EMPTY.
  const bool isSeeking =
      IsSeekable() && ((mState == State::Started || mState == State::Paused) &&
                       aStartPosition->vt != VT_EMPTY);
  nsAutoCString startPosition;
  if (aStartPosition->vt == VT_I8) {
    startPosition.AppendInt(aStartPosition->hVal.QuadPart);
  } else if (aStartPosition->vt == VT_EMPTY) {
    startPosition.AppendLiteral("empty");
  }
  LOG("Start, start position=%s, isSeeking=%d", startPosition.get(), isSeeking);

  // Ask IMFMediaStream to send stream events.
  DWORD streamDescCount = 0;
  RETURN_IF_FAILED(
      aPresentationDescriptor->GetStreamDescriptorCount(&streamDescCount));

  // TODO : should event orders be exactly same as msdn's order?
  for (DWORD idx = 0; idx < streamDescCount; idx++) {
    ComPtr<IMFStreamDescriptor> streamDescriptor;
    BOOL selected;
    RETURN_IF_FAILED(aPresentationDescriptor->GetStreamDescriptorByIndex(
        idx, &selected, &streamDescriptor));

    DWORD streamId;
    RETURN_IF_FAILED(streamDescriptor->GetStreamIdentifier(&streamId));

    ComPtr<MFMediaEngineStream> stream = GetStreamByDescriptorId(streamId);
    NS_ENSURE_TRUE(stream, MF_E_INVALIDREQUEST);

    if (selected) {
      RETURN_IF_FAILED(mMediaEventQueue->QueueEventParamUnk(
          stream->IsSelected() ? MEUpdatedStream : MENewStream, GUID_NULL, S_OK,
          stream.Get()));
      // Need to select stream first before doing other operations.
      stream->SetSelected(true);
      if (isSeeking) {
        RETURN_IF_FAILED(stream->Seek(aStartPosition));
      } else {
        RETURN_IF_FAILED(stream->Start(aStartPosition));
      }
    } else {
      stream->SetSelected(false);
    }
  }

  // Send source event.
  RETURN_IF_FAILED(QueueEvent(isSeeking ? MESourceSeeked : MESourceStarted,
                              GUID_NULL, S_OK, aStartPosition));
  mState = State::Started;
  mPresentationEnded = false;
  LOG("Started media source");
  return S_OK;
}

IFACEMETHODIMP MFMediaSource::Stop() {
  AssertOnMFThreadPool();
  if (mState == State::Shutdowned) {
    return MF_E_SHUTDOWN;
  }

  LOG("Stop");
  RETURN_IF_FAILED(QueueEvent(MESourceStopped, GUID_NULL, S_OK, nullptr));
  if (mAudioStream) {
    RETURN_IF_FAILED(mAudioStream->Stop());
  }
  if (mVideoStream) {
    RETURN_IF_FAILED(mVideoStream->Stop());
  }

  mState = State::Stopped;
  LOG("Stopped media source");
  return S_OK;
}

IFACEMETHODIMP MFMediaSource::Pause() {
  AssertOnMFThreadPool();
  if (mState == State::Shutdowned) {
    return MF_E_SHUTDOWN;
  }
  if (mState != State::Started) {
    return MF_E_INVALID_STATE_TRANSITION;
  }

  LOG("Pause");
  RETURN_IF_FAILED(QueueEvent(MESourcePaused, GUID_NULL, S_OK, nullptr));
  if (mAudioStream) {
    RETURN_IF_FAILED(mAudioStream->Pause());
  }
  if (mVideoStream) {
    RETURN_IF_FAILED(mVideoStream->Pause());
  }

  mState = State::Paused;
  LOG("Paused media source");
  return S_OK;
}

IFACEMETHODIMP MFMediaSource::Shutdown() {
  AssertOnMFThreadPool();
  if (mState == State::Shutdowned) {
    return MF_E_SHUTDOWN;
  }

  LOG("Shutdown");
  // After this method is called, all IMFMediaEventQueue methods return
  // MF_E_SHUTDOWN.
  RETURN_IF_FAILED(mMediaEventQueue->Shutdown());
  if (mAudioStream) {
    mAudioStream->Shutdown();
    mAudioStream = nullptr;
    mAudioStreamEndedListener.DisconnectIfExists();
  }
  if (mVideoStream) {
    mVideoStream->Shutdown();
    mVideoStream = nullptr;
    mVideoStreamEndedListener.DisconnectIfExists();
  }

  mState = State::Shutdowned;
  Unused << mTaskQueue->BeginShutdown();
  LOG("Shutdowned media source");
  return S_OK;
}

IFACEMETHODIMP MFMediaSource::GetEvent(DWORD aFlags, IMFMediaEvent** aEvent) {
  MOZ_ASSERT(mMediaEventQueue);
  LOG("GetEvent");
  return mMediaEventQueue->GetEvent(aFlags, aEvent);
}

IFACEMETHODIMP MFMediaSource::BeginGetEvent(IMFAsyncCallback* aCallback,
                                            IUnknown* aState) {
  MOZ_ASSERT(mMediaEventQueue);
  LOG("BeginGetEvent");
  return mMediaEventQueue->BeginGetEvent(aCallback, aState);
}

IFACEMETHODIMP MFMediaSource::EndGetEvent(IMFAsyncResult* aResult,
                                          IMFMediaEvent** aEvent) {
  MOZ_ASSERT(mMediaEventQueue);
  LOG("EndGetEvent");
  return mMediaEventQueue->EndGetEvent(aResult, aEvent);
}

IFACEMETHODIMP MFMediaSource::QueueEvent(MediaEventType aType,
                                         REFGUID aExtendedType, HRESULT aStatus,
                                         const PROPVARIANT* aValue) {
  MOZ_ASSERT(mMediaEventQueue);
  RETURN_IF_FAILED(mMediaEventQueue->QueueEventParamVar(aType, aExtendedType,
                                                        aStatus, aValue));
  LOG("Queued event %s", MediaEventTypeToStr(aType));
  return S_OK;
}

bool MFMediaSource::IsSeekable() const {
  // TODO : check seekable from info.
  return true;
}

MFMediaEngineStream* MFMediaSource::GetStreamByDescriptorId(DWORD aId) const {
  if (mAudioStream && mAudioStream->DescriptorId() == aId) {
    return mAudioStream.Get();
  }
  if (mVideoStream && mVideoStream->DescriptorId() == aId) {
    return mVideoStream.Get();
  }
  NS_WARNING("Invalid stream descriptor Id!");
  return nullptr;
}

void MFMediaSource::NotifyEndOfStreamInternal(TrackInfo::TrackType aType) {
  AssertOnTaskQueue();
  if (aType == TrackInfo::TrackType::kAudioTrack) {
    MOZ_ASSERT(mAudioStream);
    mAudioStream->NotifyEndOfStream();
  } else if (aType == TrackInfo::TrackType::kVideoTrack) {
    if (StaticPrefs::media_wmf_media_engine_video_output_enabled()) {
      MOZ_ASSERT(mVideoStream);
      mVideoStream->NotifyEndOfStream();
    }
  }
}

void MFMediaSource::HandleStreamEnded(TrackInfo::TrackType aType) {
  AssertOnTaskQueue();
  LOG("Handle %s stream ended", TrackTypeToStr(aType));
  if (mPresentationEnded) {
    return;
  }

  const bool audioEnded = !mAudioStream || mAudioStream->IsEnded();
  const bool videoEnded = !mVideoStream || mVideoStream->IsEnded();
  mPresentationEnded = audioEnded && videoEnded;
  LOG("PresentationEnded=%d, audioEnded=%d, videoEnded=%d",
      !!mPresentationEnded, audioEnded, videoEnded);
  if (mPresentationEnded) {
    RETURN_VOID_IF_FAILED(
        QueueEvent(MEEndOfPresentation, GUID_NULL, S_OK, nullptr));
  }
}

void MFMediaSource::AssertOnTaskQueue() const {
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
}

void MFMediaSource::AssertOnMFThreadPool() const {
  // We can't really assert the thread id from thread pool, because it would
  // change any time. So we just assert this is not the task queue, and use the
  // explicit function name to indicate what thread we should run on.
  MOZ_ASSERT(!mTaskQueue->IsCurrentThreadIn());
}

#undef LOG

}  // namespace mozilla
