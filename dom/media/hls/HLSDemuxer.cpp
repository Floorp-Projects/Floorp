/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HLSDemuxer.h"

#include <algorithm>
#include <limits>
#include <stdint.h>

#include "HLSUtils.h"
#include "MediaCodec.h"
#include "mozilla/Unused.h"
#include "nsPrintfCString.h"

using namespace mozilla::java;

namespace mozilla {

static Atomic<uint32_t> sStreamSourceID(0u);

typedef TrackInfo::TrackType TrackType;
using media::TimeUnit;
using media::TimeIntervals;
using media::TimeInterval;

static
VideoInfo::Rotation getVideoInfoRotation(int aRotation)
{
  switch (aRotation) {
    case 0:
      return VideoInfo::Rotation::kDegree_0;
    case 90:
      return VideoInfo::Rotation::kDegree_90;
    case 180:
      return VideoInfo::Rotation::kDegree_180;
    case 270:
      return VideoInfo::Rotation::kDegree_270;
    default:
      return VideoInfo::Rotation::kDegree_0;
  }
}

static
mozilla::StereoMode getStereoMode(int aMode)
{
  switch (aMode) {
    case 0:
      return mozilla::StereoMode::MONO;
    case 1:
      return mozilla::StereoMode::TOP_BOTTOM;
    case 2:
      return mozilla::StereoMode::LEFT_RIGHT;
    default:
      return mozilla::StereoMode::MONO;
  }
}

// HLSDemuxerCallbacksSupport is a native implemented callback class for
// Callbacks in GeckoHLSDemuxerWrapper.java.
// The callback functions will be invoked by JAVA-side thread.
// Should dispatch the task to the demuxer's task queue.
// We ensure the callback will never be invoked after
// HLSDemuxerCallbacksSupport::DisposeNative has been called in ~HLSDemuxer.
class HLSDemuxer::HLSDemuxerCallbacksSupport
 : public GeckoHLSDemuxerWrapper::Callbacks::Natives<HLSDemuxerCallbacksSupport>
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(HLSDemuxerCallbacksSupport)
public:
  typedef GeckoHLSDemuxerWrapper::Callbacks::Natives<HLSDemuxerCallbacksSupport> NativeCallbacks;
  using NativeCallbacks::DisposeNative;
  using NativeCallbacks::AttachNative;

  HLSDemuxerCallbacksSupport(HLSDemuxer* aDemuxer)
    : mMutex("HLSDemuxerCallbacksSupport")
    , mDemuxer(aDemuxer)
  {
    MOZ_ASSERT(mDemuxer);
  }

  void OnInitialized(bool aHasAudio, bool aHasVideo)
  {
    HLS_DEBUG("HLSDemuxerCallbacksSupport",
              "OnInitialized");
    MutexAutoLock lock(mMutex);
    if (!mDemuxer) { return; }
    RefPtr<HLSDemuxerCallbacksSupport> self = this;
    mDemuxer->GetTaskQueue()->Dispatch(NS_NewRunnableFunction(
     "HLSDemuxer::HLSDemuxerCallbacksSupport::OnInitialized",
     [=] () {
       MutexAutoLock lock(self->mMutex);
       if (self->mDemuxer) {
         self->mDemuxer->OnInitialized(aHasAudio, aHasVideo);
       }
     }));
  }

  // TODO: Handle the unexpected error signal from the java implementation
  // in bug 1368904.
  void OnError(int aErrorCode)
  {
    HLS_DEBUG("HLSDemuxerCallbacksSupport",
              "Got error(%d) from java side",
              aErrorCode);
  }
  void Detach()
  {
    MutexAutoLock lock(mMutex);
    mDemuxer = nullptr;
  }

  Mutex mMutex;
private:
  ~HLSDemuxerCallbacksSupport() { }
  HLSDemuxer* mDemuxer;

};

HLSDemuxer::HLSDemuxer(int aPlayerId)
  : mTaskQueue(new AutoTaskQueue(GetMediaThreadPool(MediaThreadType::PLAYBACK),
                                 /* aSupportsTailDispatch = */ false))
  , mMutex("HLSDemuxer")
{
  MOZ_ASSERT(NS_IsMainThread());
  HLSDemuxerCallbacksSupport::Init();
  mJavaCallbacks = GeckoHLSDemuxerWrapper::Callbacks::New();
  MOZ_ASSERT(mJavaCallbacks);

  mCallbackSupport = new HLSDemuxerCallbacksSupport(this);
  HLSDemuxerCallbacksSupport::AttachNative(mJavaCallbacks,
                                           mCallbackSupport);

  mHLSDemuxerWrapper = GeckoHLSDemuxerWrapper::Create(aPlayerId, mJavaCallbacks);
  MOZ_ASSERT(mHLSDemuxerWrapper);
}

void
HLSDemuxer::OnInitialized(bool aHasAudio, bool aHasVideo)
{
  MOZ_ASSERT(OnTaskQueue());

  if (aHasAudio) {
    UpdateAudioInfo(0);
  }
  if (aHasVideo) {
    UpdateVideoInfo(0);
  }

  mInitPromise.ResolveIfExists(NS_OK, __func__);
}

RefPtr<HLSDemuxer::InitPromise>
HLSDemuxer::Init()
{
  RefPtr<HLSDemuxer> self = this;
  return InvokeAsync(GetTaskQueue(), __func__,
    [self](){
      RefPtr<InitPromise> p = self->mInitPromise.Ensure(__func__);
      return p;
    });
}

void HLSDemuxer::NotifyDataArrived()
{
  HLS_DEBUG("HLSDemuxer", "NotifyDataArrived");
}

bool
HLSDemuxer::HasTrackType(TrackType aType) const
{
  MutexAutoLock lock(mMutex);
  HLS_DEBUG("HLSDemuxer", "HasTrackType(%d)", aType);
  switch (aType) {
    case TrackType::kAudioTrack:
      return mInfo.HasAudio();
    case TrackType::kVideoTrack:
      return mInfo.HasVideo();
    default:
      return false;
  }
}

uint32_t
HLSDemuxer::GetNumberTracks(TrackType aType) const
{
  switch (aType) {
    case TrackType::kAudioTrack:
      return mHLSDemuxerWrapper->GetNumberOfTracks(TrackType::kAudioTrack);
    case TrackType::kVideoTrack:
      return mHLSDemuxerWrapper->GetNumberOfTracks(TrackType::kVideoTrack);
    default:
      return 0;
  }
}

already_AddRefed<MediaTrackDemuxer>
HLSDemuxer::GetTrackDemuxer(TrackType aType, uint32_t aTrackNumber)
{
  RefPtr<HLSTrackDemuxer> e = new HLSTrackDemuxer(this, aType);
  mDemuxers.AppendElement(e);
  return e.forget();
}

bool
HLSDemuxer::IsSeekable() const
{
  return !mHLSDemuxerWrapper->IsLiveStream();
}

UniquePtr<EncryptionInfo>
HLSDemuxer::GetCrypto()
{
  // TODO: Currently, our HLS implementation doesn't support encrypted content.
  // Return null at this stage.
  return nullptr;
}

TrackInfo*
HLSDemuxer::GetTrackInfo(TrackType aTrack)
{
  MutexAutoLock lock(mMutex);
  switch (aTrack) {
    case TrackType::kAudioTrack: {
      return &mInfo.mAudio;
    }
    case TrackType::kVideoTrack: {
      return &mInfo.mVideo;
    }
    default:
      return nullptr;
  }
}

TimeUnit
HLSDemuxer::GetNextKeyFrameTime()
{
  MOZ_ASSERT(mHLSDemuxerWrapper);
  return TimeUnit::FromMicroseconds(mHLSDemuxerWrapper->GetNextKeyFrameTime());
}

void
HLSDemuxer::UpdateAudioInfo(int index)
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(mHLSDemuxerWrapper);
  HLS_DEBUG("HLSDemuxer", "UpdateAudioInfo (%d)", index);
  MutexAutoLock lock(mMutex);
  jni::Object::LocalRef infoObj = mHLSDemuxerWrapper->GetAudioInfo(index);
  if (infoObj) {
    java::GeckoAudioInfo::LocalRef audioInfo(Move(infoObj));
    mInfo.mAudio.mRate = audioInfo->Rate();
    mInfo.mAudio.mChannels = audioInfo->Channels();
    mInfo.mAudio.mProfile = audioInfo->Profile();
    mInfo.mAudio.mBitDepth = audioInfo->BitDepth();
    mInfo.mAudio.mMimeType = NS_ConvertUTF16toUTF8(audioInfo->MimeType()->ToString());
    mInfo.mAudio.mDuration = TimeUnit::FromMicroseconds(audioInfo->Duration());
    auto&& csd = audioInfo->CodecSpecificData()->GetElements();
    mInfo.mAudio.mCodecSpecificConfig->Clear();
    mInfo.mAudio.mCodecSpecificConfig->AppendElements(reinterpret_cast<uint8_t*>(&csd[0]),
                                                      csd.Length());
  }
}

void
HLSDemuxer::UpdateVideoInfo(int index)
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(mHLSDemuxerWrapper);
  MutexAutoLock lock(mMutex);
  jni::Object::LocalRef infoObj = mHLSDemuxerWrapper->GetVideoInfo(index);
  if (infoObj) {
    java::GeckoVideoInfo::LocalRef videoInfo(Move(infoObj));
    mInfo.mVideo.mStereoMode = getStereoMode(videoInfo->StereoMode());
    mInfo.mVideo.mRotation = getVideoInfoRotation(videoInfo->Rotation());
    mInfo.mVideo.mImage.width = videoInfo->DisplayWidth();
    mInfo.mVideo.mImage.height = videoInfo->DisplayHeight();
    mInfo.mVideo.mDisplay.width = videoInfo->PictureWidth();
    mInfo.mVideo.mDisplay.height = videoInfo->PictureHeight();
    mInfo.mVideo.mMimeType = NS_ConvertUTF16toUTF8(videoInfo->MimeType()->ToString());
    mInfo.mVideo.mDuration = TimeUnit::FromMicroseconds(videoInfo->Duration());
    HLS_DEBUG("HLSDemuxer", "UpdateVideoInfo (%d) / I(%dx%d) / D(%dx%d)",
     index, mInfo.mVideo.mImage.width, mInfo.mVideo.mImage.height,
     mInfo.mVideo.mDisplay.width, mInfo.mVideo.mDisplay.height);
  }
}

bool
HLSDemuxer::OnTaskQueue() const
{
  return mTaskQueue->IsCurrentThreadIn();
}

HLSDemuxer::~HLSDemuxer()
{
  HLS_DEBUG("HLSDemuxer", "~HLSDemuxer()");
  mCallbackSupport->Detach();
  if (mJavaCallbacks) {
    HLSDemuxerCallbacksSupport::DisposeNative(mJavaCallbacks);
    mJavaCallbacks = nullptr;
  }
  if (mHLSDemuxerWrapper) {
    mHLSDemuxerWrapper->Destroy();
    mHLSDemuxerWrapper = nullptr;
  }
  mInitPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
}

HLSTrackDemuxer::HLSTrackDemuxer(HLSDemuxer* aParent, TrackInfo::TrackType aType)
  : mParent(aParent)
  , mType(aType)
{
}

UniquePtr<TrackInfo>
HLSTrackDemuxer::GetInfo() const
{
  return mParent->GetTrackInfo(mType)->Clone();
}

RefPtr<HLSTrackDemuxer::SeekPromise>
HLSTrackDemuxer::Seek(const TimeUnit& aTime)
{
  MOZ_ASSERT(mParent, "Called after BreackCycle()");
  return InvokeAsync<TimeUnit&&>(mParent->GetTaskQueue(),
                                 this,
                                 __func__,
                                 &HLSTrackDemuxer::DoSeek,
                                 aTime);
}

RefPtr<HLSTrackDemuxer::SeekPromise>
HLSTrackDemuxer::DoSeek(const TimeUnit& aTime)
{
  MOZ_ASSERT(mParent, "Called after BreackCycle()");
  MOZ_ASSERT(mParent->OnTaskQueue());
  mQueuedSample = nullptr;
  int64_t seekTimeUs = aTime.ToMicroseconds();
  bool result = mParent->mHLSDemuxerWrapper->Seek(seekTimeUs);
  if (!result) {
    return SeekPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_WAITING_FOR_DATA,
                                        __func__);
  }
  TimeUnit seekTime = TimeUnit::FromMicroseconds(seekTimeUs);
  return SeekPromise::CreateAndResolve(seekTime, __func__);
}

RefPtr<HLSTrackDemuxer::SamplesPromise>
HLSTrackDemuxer::GetSamples(int32_t aNumSamples)
{
  MOZ_ASSERT(mParent, "Called after BreackCycle()");
  return InvokeAsync(mParent->GetTaskQueue(), this, __func__,
                     &HLSTrackDemuxer::DoGetSamples, aNumSamples);
}

RefPtr<HLSTrackDemuxer::SamplesPromise>
HLSTrackDemuxer::DoGetSamples(int32_t aNumSamples)
{
  MOZ_ASSERT(mParent, "Called after BreackCycle()");
  MOZ_ASSERT(mParent->OnTaskQueue());
  if (!aNumSamples) {
    return SamplesPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_DEMUXER_ERR,
                                           __func__);
  }
  RefPtr<SamplesHolder> samples = new SamplesHolder;
  if (mQueuedSample) {
    if (mQueuedSample->mEOS) {
      return SamplesPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_END_OF_STREAM,
                                             __func__);
    }
    MOZ_ASSERT(mQueuedSample->mKeyframe,
               "mQueuedSample must be a keyframe");
    samples->mSamples.AppendElement(mQueuedSample);
    mQueuedSample = nullptr;
    aNumSamples--;
  }
  if (aNumSamples == 0) {
    // Return the queued sample.
    return SamplesPromise::CreateAndResolve(samples, __func__);
  }
  mozilla::jni::ObjectArray::LocalRef demuxedSamples =
    (mType == TrackInfo::kAudioTrack)
    ? mParent->mHLSDemuxerWrapper->GetSamples(TrackInfo::kAudioTrack, aNumSamples)
    : mParent->mHLSDemuxerWrapper->GetSamples(TrackInfo::kVideoTrack, aNumSamples);
  nsTArray<jni::Object::LocalRef> sampleObjectArray(demuxedSamples->GetElements());

  if (sampleObjectArray.IsEmpty()) {
    return SamplesPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_WAITING_FOR_DATA, __func__);
  }

  for (auto&& demuxedSample : sampleObjectArray) {
    java::GeckoHLSSample::LocalRef sample(Move(demuxedSample));
    if (sample->IsEOS()) {
      HLS_DEBUG("HLSTrackDemuxer", "Met BUFFER_FLAG_END_OF_STREAM.");
      if (samples->mSamples.IsEmpty()) {
        return SamplesPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_END_OF_STREAM,
                                               __func__);
      }
      mQueuedSample = new MediaRawData();
      mQueuedSample->mEOS = true;
      break;
    }
    RefPtr<MediaRawData> mrd = ConvertToMediaRawData(sample);
    if (!mrd) {
      return SamplesPromise::CreateAndReject(NS_ERROR_OUT_OF_MEMORY, __func__);
    }
    samples->mSamples.AppendElement(mrd);
  }
  if (mType == TrackInfo::kVideoTrack &&
      (mNextKeyframeTime.isNothing() ||
       samples->mSamples.LastElement()->mTime >= mNextKeyframeTime.value())) {
    // Only need to find NextKeyFrame for Video
    UpdateNextKeyFrameTime();
  }

  return SamplesPromise::CreateAndResolve(samples, __func__);
}


CryptoSample
HLSTrackDemuxer::ExtractCryptoSample(size_t aSampleSize,
                                     java::sdk::CryptoInfo::LocalRef aCryptoInfo)
{
  if (!aCryptoInfo) {
    return CryptoSample{};
  }
  // Extract Crypto information
  CryptoSample crypto;
  char const* msg = "";
  do {
    HLS_DEBUG("HLSTrackDemuxer", "Sample has Crypto Info");
    crypto.mValid = true;
    int32_t mode = 0;
    if (NS_FAILED(aCryptoInfo->Mode(&mode))) {
      msg = "Error when extracting encryption mode.";
      break;
    }
    crypto.mMode = mode;
    mozilla::jni::ByteArray::LocalRef ivData;
    if (NS_FAILED(aCryptoInfo->Iv(&ivData))) {
      msg = "Error when extracting encryption IV.";
      break;
    }
    // Data in mIV is uint8_t and jbyte is signed char
    auto&& ivArr= ivData->GetElements();
    crypto.mIV.AppendElements(reinterpret_cast<uint8_t*>(&ivArr[0]),
                              ivArr.Length());
    crypto.mIVSize = ivArr.Length();
    mozilla::jni::ByteArray::LocalRef keyData;
    if (NS_FAILED(aCryptoInfo->Key(&keyData))) {
      msg = "Error when extracting encryption key.";
      break;
    }
    auto&& keyArr = keyData->GetElements();
    // Data in mKeyId is uint8_t and jbyte is signed char
    crypto.mKeyId.AppendElements(reinterpret_cast<uint8_t*>(&keyArr[0]),
                                 keyArr.Length());

    mozilla::jni::IntArray::LocalRef clearData;
    if (NS_FAILED(aCryptoInfo->NumBytesOfClearData(&clearData))) {
      msg = "Error when extracting clear data.";
      break;
    }
    // Data in mPlainSizes is uint16_t, NumBytesOfClearData is int32_t
    // , so need a for loop to copy
    for (const auto& b : clearData->GetElements()) {
      crypto.mPlainSizes.AppendElement(b);
    }

    mozilla::jni::IntArray::LocalRef encryptedData;
    if (NS_FAILED(aCryptoInfo->NumBytesOfEncryptedData(&encryptedData))) {
      msg = "Error when extracting encrypted data.";
      break;
    }
    auto&& encryptedArr = encryptedData->GetElements();
    // Data in mEncryptedSizes is uint32_t, NumBytesOfEncryptedData is int32_t
    crypto.mEncryptedSizes.AppendElements(reinterpret_cast<uint32_t*>(&encryptedArr[0]),
                                          encryptedArr.Length());
    int subSamplesNum = 0;
    if (NS_FAILED(aCryptoInfo->NumSubSamples(&subSamplesNum))) {
      msg = "Error when extracting subsamples.";
      break;
    }
    crypto.mPlainSizes[0] -= (aSampleSize - subSamplesNum);

    return crypto;
  } while (false);

  HLS_DEBUG("HLSTrackDemuxer",
            "%s", msg);
  return CryptoSample{};
}

RefPtr<MediaRawData>
HLSTrackDemuxer::ConvertToMediaRawData(java::GeckoHLSSample::LocalRef aSample)
{
  java::sdk::BufferInfo::LocalRef info = aSample->Info();
  // Currently extract PTS, Size and Data without Crypto information.
  // Transform java Sample into MediaRawData
  RefPtr<MediaRawData> mrd = new MediaRawData();
  int64_t presentationTimeUs = 0;
  bool ok = NS_SUCCEEDED(info->PresentationTimeUs(&presentationTimeUs));
  mrd->mTime = TimeUnit::FromMicroseconds(presentationTimeUs);
  mrd->mTimecode = TimeUnit::FromMicroseconds(presentationTimeUs);
  mrd->mKeyframe = aSample->IsKeyFrame();
  mrd->mDuration = (mType == TrackInfo::kVideoTrack)
                   ? TimeUnit::FromMicroseconds(aSample->Duration())
                   : TimeUnit::Zero();

  int32_t size = 0;
  ok &= NS_SUCCEEDED(info->Size(&size));
  if (!ok) {
    HLS_DEBUG("HLSTrackDemuxer", "Error occurred during extraction from Sample java object.");
    return nullptr;
  }

  // Update streamSouceID & videoInfo for MFR.
  if (mType == TrackInfo::kVideoTrack) {
    auto sampleFormatIndex = aSample->FormatIndex();
    if (mLastFormatIndex != sampleFormatIndex) {
      mLastFormatIndex = sampleFormatIndex;
      mParent->UpdateVideoInfo(mLastFormatIndex);
      MutexAutoLock lock(mParent->mMutex);
      mrd->mTrackInfo = new TrackInfoSharedPtr(mParent->mInfo.mVideo, ++sStreamSourceID);
    }
  }

  // Write payload into MediaRawData
  UniquePtr<MediaRawDataWriter> writer(mrd->CreateWriter());
  if (!writer->SetSize(size)) {
    HLS_DEBUG("HLSTrackDemuxer", "Exit failed to allocate media buffer");
    return nullptr;
  }
  jni::ByteBuffer::LocalRef dest =
    jni::ByteBuffer::New(writer->Data(), writer->Size());
  aSample->WriteToByteBuffer(dest);

  writer->mCrypto = ExtractCryptoSample(writer->Size(),
                                        aSample->CryptoInfo());
  return mrd;
}

void
HLSTrackDemuxer::Reset()
{
  MOZ_ASSERT(mParent, "Called after BreackCycle()");
  mQueuedSample = nullptr;
}

void
HLSTrackDemuxer::UpdateNextKeyFrameTime()
{
  MOZ_ASSERT(mParent, "Called after BreackCycle()");
  TimeUnit nextKeyFrameTime = mParent->GetNextKeyFrameTime();
  if (nextKeyFrameTime != mNextKeyframeTime.refOr(TimeUnit::FromInfinity())) {
    HLS_DEBUG("HLSTrackDemuxer", "Update mNextKeyframeTime to %" PRId64 , nextKeyFrameTime.ToMicroseconds());
    mNextKeyframeTime = Some(nextKeyFrameTime);
  }
}

nsresult
HLSTrackDemuxer::GetNextRandomAccessPoint(TimeUnit* aTime)
{
  if (mNextKeyframeTime.isNothing()) {
    // There's no next key frame.
    *aTime = TimeUnit::FromInfinity();
  } else {
    *aTime = mNextKeyframeTime.value();
  }
  return NS_OK;
}

RefPtr<HLSTrackDemuxer::SkipAccessPointPromise>
HLSTrackDemuxer::SkipToNextRandomAccessPoint(
  const TimeUnit& aTimeThreshold)
{
  return InvokeAsync(
           mParent->GetTaskQueue(), this, __func__,
           &HLSTrackDemuxer::DoSkipToNextRandomAccessPoint,
           aTimeThreshold);
}

RefPtr<HLSTrackDemuxer::SkipAccessPointPromise>
HLSTrackDemuxer::DoSkipToNextRandomAccessPoint(
  const TimeUnit& aTimeThreshold)
{
  MOZ_ASSERT(mParent, "Called after BreackCycle()");
  MOZ_ASSERT(mParent->OnTaskQueue());
  mQueuedSample = nullptr;
  uint32_t parsed = 0;
  bool found = false;
  MediaResult result = NS_ERROR_DOM_MEDIA_END_OF_STREAM;
  do {
    mozilla::jni::ObjectArray::LocalRef demuxedSamples =
      mParent->mHLSDemuxerWrapper->GetSamples(mType, 1);
    nsTArray<jni::Object::LocalRef> sampleObjectArray(demuxedSamples->GetElements());
    if (sampleObjectArray.IsEmpty()) {
      result = NS_ERROR_DOM_MEDIA_WAITING_FOR_DATA;
      break;
    }
    parsed++;
    java::GeckoHLSSample::LocalRef sample(Move(sampleObjectArray[0]));
    if (sample->IsEOS()) {
      result = NS_ERROR_DOM_MEDIA_END_OF_STREAM;
      break;
    }
    if (sample->IsKeyFrame()) {
      java::sdk::BufferInfo::LocalRef info = sample->Info();
      int64_t presentationTimeUs = 0;
      bool ok = NS_SUCCEEDED(info->PresentationTimeUs(&presentationTimeUs));
      if (ok && TimeUnit::FromMicroseconds(presentationTimeUs) >= aTimeThreshold) {
        found = true;
        mQueuedSample = ConvertToMediaRawData(sample);
        break;
      }
    }
  } while(true);

  if (!found) {
    return SkipAccessPointPromise::CreateAndReject(
      SkipFailureHolder(result, parsed),
      __func__);
  }
  return SkipAccessPointPromise::CreateAndResolve(parsed, __func__);
}

TimeIntervals
HLSTrackDemuxer::GetBuffered()
{
  MOZ_ASSERT(mParent, "Called after BreackCycle()");
  int64_t bufferedTime = mParent->mHLSDemuxerWrapper->GetBuffered(); //us
  return TimeIntervals(TimeInterval(TimeUnit(),
                                    TimeUnit::FromMicroseconds(bufferedTime)));
}

void
HLSTrackDemuxer::BreakCycles()
{
  RefPtr<HLSTrackDemuxer> self = this;
  nsCOMPtr<nsIRunnable> task =
    NS_NewRunnableFunction("HLSTrackDemuxer::BreakCycles",
    [self]() {
      self->mParent = nullptr;
    } );
  mParent->GetTaskQueue()->Dispatch(task.forget());
}

HLSTrackDemuxer::~HLSTrackDemuxer()
{
}

} // namespace mozilla
