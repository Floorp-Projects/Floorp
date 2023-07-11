/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFMediaSource.h"

#include <mfapi.h>
#include <mfidl.h>
#include <stdint.h>

#include "MFCDMProxy.h"
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

MFMediaSource::MFMediaSource()
    : mPresentationEnded(false), mIsAudioEnded(false), mIsVideoEnded(false) {
  MOZ_COUNT_CTOR(MFMediaSource);
  LOG("media source created");
}

MFMediaSource::~MFMediaSource() {
  // TODO : notify cdm about the last key id?
  MOZ_COUNT_DTOR(MFMediaSource);
  LOG("media source destroyed");
}

HRESULT MFMediaSource::RuntimeClassInitialize(
    const Maybe<AudioInfo>& aAudio, const Maybe<VideoInfo>& aVideo,
    nsISerialEventTarget* aManagerThread) {
  // On manager thread.
  MutexAutoLock lock(mMutex);

  static uint64_t streamId = 1;

  mTaskQueue = TaskQueue::Create(
      GetMediaThreadPool(MediaThreadType::PLATFORM_DECODER), "MFMediaSource");
  mManagerThread = aManagerThread;
  MOZ_ASSERT(mManagerThread, "manager thread shouldn't be nullptr!");

  if (aAudio) {
    mAudioStream.Attach(
        MFMediaEngineAudioStream::Create(streamId++, *aAudio, this));
    if (!mAudioStream) {
      NS_WARNING("Failed to create audio stream");
      return E_FAIL;
    }
    mAudioStreamEndedListener = mAudioStream->EndedEvent().Connect(
        mManagerThread, this, &MFMediaSource::HandleStreamEnded);
  } else {
    mIsAudioEnded = true;
  }

  if (aVideo) {
    mVideoStream.Attach(
        MFMediaEngineVideoStream::Create(streamId++, *aVideo, this));
    if (!mVideoStream) {
      NS_WARNING("Failed to create video stream");
      return E_FAIL;
    }
    mVideoStreamEndedListener = mVideoStream->EndedEvent().Connect(
        mManagerThread, this, &MFMediaSource::HandleStreamEnded);
  } else {
    mIsVideoEnded = true;
  }

  RETURN_IF_FAILED(wmf::MFCreateEventQueue(&mMediaEventQueue));

  LOG("Initialized a media source");
  return S_OK;
}

IFACEMETHODIMP MFMediaSource::GetCharacteristics(DWORD* aCharacteristics) {
  // This could be run on both mf thread pool and manager thread.
  {
    MutexAutoLock lock(mMutex);
    if (mState == State::Shutdowned) {
      return MF_E_SHUTDOWN;
    }
  }
  // https://docs.microsoft.com/en-us/windows/win32/api/mfidl/ne-mfidl-mfmediasource_characteristics
  *aCharacteristics = MFMEDIASOURCE_CAN_SEEK | MFMEDIASOURCE_CAN_PAUSE;
  return S_OK;
}

IFACEMETHODIMP MFMediaSource::CreatePresentationDescriptor(
    IMFPresentationDescriptor** aPresentationDescriptor) {
  AssertOnMFThreadPool();
  MutexAutoLock lock(mMutex);
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
  MutexAutoLock lock(mMutex);
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

    ComPtr<MFMediaEngineStream> stream;
    if (mAudioStream && mAudioStream->DescriptorId() == streamId) {
      stream = mAudioStream;
    } else if (mVideoStream && mVideoStream->DescriptorId() == streamId) {
      stream = mVideoStream;
    }
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
  if (mAudioStream && mAudioStream->IsSelected()) {
    mIsAudioEnded = false;
  }
  if (mVideoStream && mVideoStream->IsSelected()) {
    mIsVideoEnded = false;
  }
  LOG("Started media source");
  return S_OK;
}

IFACEMETHODIMP MFMediaSource::Stop() {
  AssertOnMFThreadPool();
  MutexAutoLock lock(mMutex);
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
  MutexAutoLock lock(mMutex);
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
  // Could be called on either manager thread or MF thread pool.
  MutexAutoLock lock(mMutex);
  if (mState == State::Shutdowned) {
    return MF_E_SHUTDOWN;
  }

  LOG("Shutdown");
  // After this method is called, all IMFMediaEventQueue methods return
  // MF_E_SHUTDOWN.
  RETURN_IF_FAILED(mMediaEventQueue->Shutdown());
  mState = State::Shutdowned;
  LOG("Shutdowned media source");
  return S_OK;
}

void MFMediaSource::ShutdownTaskQueue() {
  AssertOnManagerThread();
  LOG("ShutdownTaskQueue");
  MutexAutoLock lock(mMutex);
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
  Unused << mTaskQueue->BeginShutdown();
  mTaskQueue = nullptr;
}

IFACEMETHODIMP MFMediaSource::GetEvent(DWORD aFlags, IMFMediaEvent** aEvent) {
  MOZ_ASSERT(mMediaEventQueue);
  return mMediaEventQueue->GetEvent(aFlags, aEvent);
}

IFACEMETHODIMP MFMediaSource::BeginGetEvent(IMFAsyncCallback* aCallback,
                                            IUnknown* aState) {
  MOZ_ASSERT(mMediaEventQueue);
  return mMediaEventQueue->BeginGetEvent(aCallback, aState);
}

IFACEMETHODIMP MFMediaSource::EndGetEvent(IMFAsyncResult* aResult,
                                          IMFMediaEvent** aEvent) {
  MOZ_ASSERT(mMediaEventQueue);
  return mMediaEventQueue->EndGetEvent(aResult, aEvent);
}

IFACEMETHODIMP MFMediaSource::QueueEvent(MediaEventType aType,
                                         REFGUID aExtendedType, HRESULT aStatus,
                                         const PROPVARIANT* aValue) {
  MOZ_ASSERT(mMediaEventQueue);
  LOG("Queued event %s", MediaEventTypeToStr(aType));
  PROFILER_MARKER_TEXT("MFMediaSource::QueueEvent", MEDIA_PLAYBACK, {},
                       nsPrintfCString("%s", MediaEventTypeToStr(aType)));
  RETURN_IF_FAILED(mMediaEventQueue->QueueEventParamVar(aType, aExtendedType,
                                                        aStatus, aValue));
  return S_OK;
}

bool MFMediaSource::IsSeekable() const {
  // TODO : check seekable from info.
  return true;
}

void MFMediaSource::NotifyEndOfStream(TrackInfo::TrackType aType) {
  AssertOnManagerThread();
  MutexAutoLock lock(mMutex);
  if (mState == State::Shutdowned) {
    return;
  }
  if (aType == TrackInfo::TrackType::kAudioTrack) {
    MOZ_ASSERT(mAudioStream);
    mAudioStream->NotifyEndOfStream();
  } else if (aType == TrackInfo::TrackType::kVideoTrack) {
    MOZ_ASSERT(mVideoStream);
    mVideoStream->NotifyEndOfStream();
  }
}

void MFMediaSource::HandleStreamEnded(TrackInfo::TrackType aType) {
  AssertOnManagerThread();
  MutexAutoLock lock(mMutex);
  if (mState == State::Shutdowned) {
    return;
  }
  if (mPresentationEnded) {
    LOG("Presentation is ended already");
    RETURN_VOID_IF_FAILED(
        QueueEvent(MEEndOfPresentation, GUID_NULL, S_OK, nullptr));
    return;
  }

  LOG("Handle %s stream ended", TrackTypeToStr(aType));
  if (aType == TrackInfo::TrackType::kAudioTrack) {
    mIsAudioEnded = true;
  } else if (aType == TrackInfo::TrackType::kVideoTrack) {
    mIsVideoEnded = true;
  } else {
    MOZ_ASSERT_UNREACHABLE("Incorrect track type!");
  }
  mPresentationEnded = mIsAudioEnded && mIsVideoEnded;
  LOG("PresentationEnded=%d, audioEnded=%d, videoEnded=%d",
      !!mPresentationEnded, mIsAudioEnded, mIsVideoEnded);
  PROFILER_MARKER_TEXT(
      " MFMediaSource::HandleStreamEnded", MEDIA_PLAYBACK, {},
      nsPrintfCString("PresentationEnded=%d, audioEnded=%d, videoEnded=%d",
                      !!mPresentationEnded, mIsAudioEnded, mIsVideoEnded));
  if (mPresentationEnded) {
    RETURN_VOID_IF_FAILED(
        QueueEvent(MEEndOfPresentation, GUID_NULL, S_OK, nullptr));
  }
}

void MFMediaSource::SetDCompSurfaceHandle(HANDLE aDCompSurfaceHandle,
                                          gfx::IntSize aDisplay) {
  AssertOnManagerThread();
  MutexAutoLock lock(mMutex);
  if (mVideoStream) {
    mVideoStream->AsVideoStream()->SetDCompSurfaceHandle(aDCompSurfaceHandle,
                                                         aDisplay);
  }
}

IFACEMETHODIMP MFMediaSource::GetService(REFGUID aGuidService, REFIID aRiid,
                                         LPVOID* aResult) {
  if (!IsEqualGUID(aGuidService, MF_RATE_CONTROL_SERVICE)) {
    return MF_E_UNSUPPORTED_SERVICE;
  }
  return QueryInterface(aRiid, aResult);
}

IFACEMETHODIMP MFMediaSource::GetSlowestRate(MFRATE_DIRECTION aDirection,
                                             BOOL aSupportsThinning,
                                             float* aRate) {
  AssertOnMFThreadPool();
  MOZ_ASSERT(aRate);
  *aRate = 0.0f;
  {
    MutexAutoLock lock(mMutex);
    if (mState == State::Shutdowned) {
      return MF_E_SHUTDOWN;
    }
  }
  if (aDirection == MFRATE_REVERSE) {
    return MF_E_REVERSE_UNSUPPORTED;
  }
  return S_OK;
}

IFACEMETHODIMP MFMediaSource::GetFastestRate(MFRATE_DIRECTION aDirection,
                                             BOOL aSupportsThinning,
                                             float* aRate) {
  AssertOnMFThreadPool();
  MOZ_ASSERT(aRate);
  {
    MutexAutoLock lock(mMutex);
    if (mState == State::Shutdowned) {
      *aRate = 0.0f;
      return MF_E_SHUTDOWN;
    }
  }
  if (aDirection == MFRATE_REVERSE) {
    return MF_E_REVERSE_UNSUPPORTED;
  }
  *aRate = 16.0f;
  return S_OK;
}

IFACEMETHODIMP MFMediaSource::IsRateSupported(BOOL aSupportsThinning,
                                              float aNewRate,
                                              float* aSupportedRate) {
  AssertOnMFThreadPool();
  {
    MutexAutoLock lock(mMutex);
    if (mState == State::Shutdowned) {
      return MF_E_SHUTDOWN;
    }
  }

  if (aSupportedRate) {
    *aSupportedRate = 0.0f;
  }

  MFRATE_DIRECTION direction = aNewRate >= 0 ? MFRATE_FORWARD : MFRATE_REVERSE;
  float fastestRate = 0.0f, slowestRate = 0.0f;
  GetFastestRate(direction, aSupportsThinning, &fastestRate);
  GetSlowestRate(direction, aSupportsThinning, &slowestRate);

  if (aSupportsThinning) {
    return MF_E_THINNING_UNSUPPORTED;
  } else if (aNewRate < slowestRate) {
    return MF_E_REVERSE_UNSUPPORTED;
  } else if (aNewRate > fastestRate) {
    return MF_E_UNSUPPORTED_RATE;
  }

  if (aSupportedRate) {
    *aSupportedRate = aNewRate;
  }
  return S_OK;
}

IFACEMETHODIMP MFMediaSource::SetRate(BOOL aSupportsThinning, float aRate) {
  AssertOnMFThreadPool();
  {
    MutexAutoLock lock(mMutex);
    if (mState == State::Shutdowned) {
      return MF_E_SHUTDOWN;
    }
  }

  HRESULT hr = IsRateSupported(aSupportsThinning, aRate, &mPlaybackRate);
  if (FAILED(hr)) {
    LOG("Unsupported playback rate %f, error=%lX", aRate, hr);
    return hr;
  }

  PROPVARIANT varRate;
  varRate.vt = VT_R4;
  varRate.fltVal = mPlaybackRate;
  LOG("Set playback rate %f", mPlaybackRate);
  return QueueEvent(MESourceRateChanged, GUID_NULL, S_OK, &varRate);
}

IFACEMETHODIMP MFMediaSource::GetRate(BOOL* aSupportsThinning, float* aRate) {
  AssertOnMFThreadPool();
  {
    MutexAutoLock lock(mMutex);
    if (mState == State::Shutdowned) {
      return MF_E_SHUTDOWN;
    }
  }
  *aSupportsThinning = FALSE;
  *aRate = mPlaybackRate;
  return S_OK;
}

HRESULT MFMediaSource::GetInputTrustAuthority(DWORD aStreamId, REFIID aRiid,
                                              IUnknown** aITAOut) {
  // TODO : add threading assertion, not sure what thread it would be running on
  // now.
  {
    MutexAutoLock lock(mMutex);
    if (mState == State::Shutdowned) {
      return MF_E_SHUTDOWN;
    }
  }
#ifdef MOZ_WMF_CDM
  if (!mCDMProxy) {
    return MF_E_NOT_PROTECTED;
  }

  // TODO : verify if this aStreamId is really matching our stream id or not.
  ComPtr<MFMediaEngineStream> stream = GetStreamByIndentifier(aStreamId);
  if (!stream) {
    return E_INVALIDARG;
  }

  if (!stream->IsEncrypted()) {
    return MF_E_NOT_PROTECTED;
  }

  RETURN_IF_FAILED(
      mCDMProxy->GetInputTrustAuthority(aStreamId, nullptr, 0, aRiid, aITAOut));
#endif
  return S_OK;
}

MFMediaSource::State MFMediaSource::GetState() const {
  MutexAutoLock lock(mMutex);
  return mState;
}

MFMediaEngineStream* MFMediaSource::GetAudioStream() {
  MutexAutoLock lock(mMutex);
  return mAudioStream.Get();
}
MFMediaEngineStream* MFMediaSource::GetVideoStream() {
  MutexAutoLock lock(mMutex);
  return mVideoStream.Get();
}

MFMediaEngineStream* MFMediaSource::GetStreamByIndentifier(
    DWORD aStreamId) const {
  MutexAutoLock lock(mMutex);
  if (mAudioStream && mAudioStream->DescriptorId() == aStreamId) {
    return mAudioStream.Get();
  }
  if (mVideoStream && mVideoStream->DescriptorId() == aStreamId) {
    return mVideoStream.Get();
  }
  return nullptr;
}

#ifdef MOZ_WMF_CDM
void MFMediaSource::SetCDMProxy(MFCDMProxy* aCDMProxy) {
  AssertOnManagerThread();
  mCDMProxy = aCDMProxy;
  // TODO : ask cdm proxy to refresh trusted input
}
#endif

bool MFMediaSource::IsEncrypted() const {
  MutexAutoLock lock(mMutex);
  return (mAudioStream && mAudioStream->IsEncrypted()) ||
         (mVideoStream && mVideoStream->IsEncrypted());
}

void MFMediaSource::AssertOnManagerThread() const {
  MOZ_ASSERT(mManagerThread->IsOnCurrentThread());
}

void MFMediaSource::AssertOnMFThreadPool() const {
  // We can't really assert the thread id from thread pool, because it would
  // change any time. So we just assert this is not the manager thread, and use
  // the explicit function name to indicate what thread we should run on.
  MOZ_ASSERT(!mManagerThread->IsOnCurrentThread());
}

#undef LOG

}  // namespace mozilla
