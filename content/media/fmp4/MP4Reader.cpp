/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MP4Reader.h"
#include "MediaResource.h"
#include "mp4_demuxer/mp4_demuxer.h"
#include "mp4_demuxer/Streams.h"
#include "nsSize.h"
#include "VideoUtils.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "ImageContainer.h"
#include "Layers.h"
#include "SharedThreadPool.h"
#include "mozilla/Preferences.h"

using mozilla::layers::Image;
using mozilla::layers::LayerManager;
using mozilla::layers::LayersBackend;

#ifdef PR_LOGGING
PRLogModuleInfo* GetDemuxerLog() {
  static PRLogModuleInfo* log = nullptr;
  if (!log) {
    log = PR_NewLogModule("MP4Demuxer");
  }
  return log;
}
#define LOG(...) PR_LOG(GetDemuxerLog(), PR_LOG_DEBUG, (__VA_ARGS__))
#else
#define LOG(...)
#endif

using namespace mp4_demuxer;

namespace mozilla {

// Uncomment to enable verbose per-sample logging.
//#define LOG_SAMPLE_DECODE 1

class MP4Stream : public mp4_demuxer::Stream {
public:

  MP4Stream(MediaResource* aResource)
    : mResource(aResource)
  {
    MOZ_COUNT_CTOR(MP4Stream);
    MOZ_ASSERT(aResource);
  }
  virtual ~MP4Stream() {
    MOZ_COUNT_DTOR(MP4Stream);
  }

  virtual bool ReadAt(int64_t aOffset,
                      uint8_t* aBuffer,
                      uint32_t aCount,
                      uint32_t* aBytesRead) MOZ_OVERRIDE {
    uint32_t sum = 0;
    do {
      uint32_t offset = aOffset + sum;
      char* buffer = reinterpret_cast<char*>(aBuffer + sum);
      uint32_t toRead = aCount - sum;
      uint32_t bytesRead = 0;
      nsresult rv = mResource->ReadAt(offset, buffer, toRead, &bytesRead);
      if (NS_FAILED(rv)) {
        return false;
      }
      sum += bytesRead;
    } while (sum < aCount);
    *aBytesRead = sum;
    return true;
  }

  virtual int64_t Length() const MOZ_OVERRIDE {
    return mResource->GetLength();
  }

private:
  RefPtr<MediaResource> mResource;
};

MP4Reader::MP4Reader(AbstractMediaDecoder* aDecoder)
  : MediaDecoderReader(aDecoder)
  , mAudio("MP4 audio decoder data", Preferences::GetUint("media.mp4-audio-decode-ahead", 2))
  , mVideo("MP4 video decoder data", Preferences::GetUint("media.mp4-video-decode-ahead", 2))
  , mLastReportedNumDecodedFrames(0)
  , mLayersBackendType(layers::LayersBackend::LAYERS_NONE)
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");
  MOZ_COUNT_CTOR(MP4Reader);
}

MP4Reader::~MP4Reader()
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");
  MOZ_COUNT_DTOR(MP4Reader);
  Shutdown();
}

void
MP4Reader::Shutdown()
{
  if (mAudio.mDecoder) {
    Flush(kAudio);
    mAudio.mDecoder->Shutdown();
    mAudio.mDecoder = nullptr;
  }
  if (mAudio.mTaskQueue) {
    mAudio.mTaskQueue->Shutdown();
    mAudio.mTaskQueue = nullptr;
  }
  if (mVideo.mDecoder) {
    Flush(kVideo);
    mVideo.mDecoder->Shutdown();
    mVideo.mDecoder = nullptr;
  }
  if (mVideo.mTaskQueue) {
    mVideo.mTaskQueue->Shutdown();
    mVideo.mTaskQueue = nullptr;
  }
}

void
MP4Reader::InitLayersBackendType()
{
  if (!IsVideoContentType(mDecoder->GetResource()->GetContentType())) {
    // Not playing video, we don't care about the layers backend type.
    return;
  }
  // Extract the layer manager backend type so that platform decoders
  // can determine whether it's worthwhile using hardware accelerated
  // video decoding.
  MediaDecoderOwner* owner = mDecoder->GetOwner();
  if (!owner) {
    NS_WARNING("MP4Reader without a decoder owner, can't get HWAccel");
    return;
  }

  dom::HTMLMediaElement* element = owner->GetMediaElement();
  NS_ENSURE_TRUE_VOID(element);

  nsRefPtr<LayerManager> layerManager =
    nsContentUtils::LayerManagerForDocument(element->OwnerDoc());
  NS_ENSURE_TRUE_VOID(layerManager);

  mLayersBackendType = layerManager->GetCompositorBackendType();
}

nsresult
MP4Reader::Init(MediaDecoderReader* aCloneDonor)
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");
  PlatformDecoderModule::Init();
  mMP4Stream = new MP4Stream(mDecoder->GetResource());
  mDemuxer = new MP4Demuxer(mMP4Stream);

  InitLayersBackendType();

  mAudio.mTaskQueue = new MediaTaskQueue(
    SharedThreadPool::Get(NS_LITERAL_CSTRING("MP4 Audio Decode")));
  NS_ENSURE_TRUE(mAudio.mTaskQueue, NS_ERROR_FAILURE);

  mVideo.mTaskQueue = new MediaTaskQueue(
    SharedThreadPool::Get(NS_LITERAL_CSTRING("MP4 Video Decode")));
  NS_ENSURE_TRUE(mVideo.mTaskQueue, NS_ERROR_FAILURE);

  return NS_OK;
}

nsresult
MP4Reader::ReadMetadata(MediaInfo* aInfo,
                        MetadataTags** aTags)
{
  bool ok = mDemuxer->Init();
  NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);

  const AudioDecoderConfig& audio = mDemuxer->AudioConfig();
  mInfo.mAudio.mHasAudio = mAudio.mActive = mDemuxer->HasAudio() &&
                                            audio.IsValidConfig();
  // If we have audio, we *only* allow AAC to be decoded.
  if (HasAudio() && audio.codec() != kCodecAAC) {
    return NS_ERROR_FAILURE;
  }

  const VideoDecoderConfig& video = mDemuxer->VideoConfig();
  mInfo.mVideo.mHasVideo = mVideo.mActive = mDemuxer->HasVideo() &&
                                            video.IsValidConfig();
  // If we have video, we *only* allow H.264 to be decoded.
  if (HasVideo() && video.codec() != kCodecH264) {
    return NS_ERROR_FAILURE;
  }

  mPlatform = PlatformDecoderModule::Create();
  NS_ENSURE_TRUE(mPlatform, NS_ERROR_FAILURE);

  if (HasAudio()) {
    mInfo.mAudio.mRate = audio.samples_per_second();
    mInfo.mAudio.mChannels = ChannelLayoutToChannelCount(audio.channel_layout());
    mAudio.mCallback = new DecoderCallback(this, kAudio);
    mAudio.mDecoder = mPlatform->CreateAACDecoder(audio,
                                                  mAudio.mTaskQueue,
                                                  mAudio.mCallback);
    NS_ENSURE_TRUE(mAudio.mDecoder != nullptr, NS_ERROR_FAILURE);
    nsresult rv = mAudio.mDecoder->Init();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (HasVideo()) {
    IntSize sz = video.natural_size();
    mInfo.mVideo.mDisplay = nsIntSize(sz.width(), sz.height());
    mVideo.mCallback = new  DecoderCallback(this, kVideo);
    mVideo.mDecoder = mPlatform->CreateH264Decoder(video,
                                                   mLayersBackendType,
                                                   mDecoder->GetImageContainer(),
                                                   mVideo.mTaskQueue,
                                                   mVideo.mCallback);
    NS_ENSURE_TRUE(mVideo.mDecoder != nullptr, NS_ERROR_FAILURE);
    nsresult rv = mVideo.mDecoder->Init();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Get the duration, and report it to the decoder if we have it.
  Microseconds duration = mDemuxer->Duration();
  if (duration != -1) {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    mDecoder->SetMediaDuration(duration);
  }
  // We can seek if we get a duration *and* the reader reports that it's
  // seekable.
  if (!mDemuxer->CanSeek()) {
    mDecoder->SetMediaSeekable(false);
  }

  *aInfo = mInfo;
  *aTags = nullptr;

  return NS_OK;
}

bool
MP4Reader::HasAudio()
{
  return mAudio.mActive;
}

bool
MP4Reader::HasVideo()
{
  return mVideo.mActive;
}

MP4Reader::DecoderData&
MP4Reader::GetDecoderData(TrackType aTrack)
{
  MOZ_ASSERT(aTrack == kAudio || aTrack == kVideo);
  return (aTrack == kAudio) ? mAudio : mVideo;
}

MP4SampleQueue&
MP4Reader::SampleQueue(TrackType aTrack)
{
  return GetDecoderData(aTrack).mDemuxedSamples;
}

MediaDataDecoder*
MP4Reader::Decoder(mp4_demuxer::TrackType aTrack)
{
  return GetDecoderData(aTrack).mDecoder;
}

MP4Sample*
MP4Reader::PopSample(TrackType aTrack)
{
  // Unfortunately the demuxer outputs in the order samples appear in the
  // media, not on a per stream basis. We cache the samples we get from
  // streams other than the one we want.
  MP4SampleQueue& sampleQueue = SampleQueue(aTrack);
  while (sampleQueue.empty()) {
    nsAutoPtr<MP4Sample> sample;
    bool eos = false;
    bool ok = mDemuxer->Demux(&sample, &eos);
    if (!ok || eos) {
      MOZ_ASSERT(!sample);
      return nullptr;
    }
    MOZ_ASSERT(sample);
    MP4Sample* s = sample.forget();
    SampleQueue(s->type).push_back(s);
  }
  MOZ_ASSERT(!sampleQueue.empty());
  MP4Sample* sample = sampleQueue.front();
  sampleQueue.pop_front();
  return sample;
}

// How async decoding works:
//
// When MP4Reader::Decode() is called:
// * Lock the DecoderData. We assume the state machine wants
//   output from the decoder (in future, we'll assume decoder wants input
//   when the output MediaQueue isn't "full").
// * Cache the value of mNumSamplesOutput, as prevFramesOutput.
// * While we've not output data (mNumSamplesOutput != prevNumFramesOutput)
//   and while we still require input, we demux and input data in the reader.
//   We assume we require input if
//   ((mNumSamplesInput - mNumSamplesOutput) < sDecodeAheadMargin) or
//   mInputExhausted is true. Before we send input, we reset mInputExhausted
//   and increment mNumFrameInput, and drop the lock on DecoderData.
// * Once we no longer require input, we wait on the DecoderData
//   lock for output, or for the input exhausted callback. If we receive the
//   input exhausted callback, we go back and input more data.
// * When our output callback is called, we take the DecoderData lock and
//   increment mNumSamplesOutput. We notify the DecoderData lock. This will
//   awaken the Decode thread, and unblock it, and it will return.
bool
MP4Reader::Decode(TrackType aTrack)
{
  DecoderData& data = GetDecoderData(aTrack);
  MOZ_ASSERT(data.mDecoder);

  data.mMonitor.Lock();
  uint64_t prevNumFramesOutput = data.mNumSamplesOutput;
  while (prevNumFramesOutput == data.mNumSamplesOutput) {
    data.mMonitor.AssertCurrentThreadOwns();
    if (data.mError) {
      // Decode error!
      data.mMonitor.Unlock();
      return false;
    }
    // Send input to the decoder, if we need to. We assume the decoder
    // needs input if it's told us it's out of input, or we're beneath the
    // "low water mark" for the amount of input we've sent it vs the amount
    // out output we've received. We always try to keep the decoder busy if
    // possible, so we try to maintain at least a few input samples ahead,
    // if we need output.
    while (prevNumFramesOutput == data.mNumSamplesOutput &&
           (data.mInputExhausted ||
           (data.mNumSamplesInput - data.mNumSamplesOutput) < data.mDecodeAhead)) {
      data.mMonitor.AssertCurrentThreadOwns();
      data.mMonitor.Unlock();
      nsAutoPtr<MP4Sample> compressed(PopSample(aTrack));
      if (!compressed) {
        // EOS, or error. Let the state machine know there are no more
        // frames coming.
        return false;
      }
      data.mMonitor.Lock();
      data.mInputExhausted = false;
      data.mNumSamplesInput++;
      data.mMonitor.Unlock();
      if (NS_FAILED(data.mDecoder->Input(compressed))) {
        return false;
      }
      // If Input() failed, we let the auto pointer delete |compressed|.
      // Otherwise, we assume the decoder will delete it when it's finished
      // with it.
      compressed.forget();
      data.mMonitor.Lock();
    }
    data.mMonitor.AssertCurrentThreadOwns();
    while (!data.mError &&
           prevNumFramesOutput == data.mNumSamplesOutput &&
           !data.mInputExhausted ) {
      data.mMonitor.Wait();
    }
  }
  data.mMonitor.AssertCurrentThreadOwns();
  data.mMonitor.Unlock();
  return true;
}

#ifdef LOG_SAMPLE_DECODE
static const char*
TrackTypeToStr(TrackType aTrack)
{
  MOZ_ASSERT(aTrack == kAudio || aTrack == kVideo);
  switch (aTrack) {
    case kAudio: return "Audio";
    case kVideo: return "Video";
    default: return "Unknown";
  }
}
#endif

void
MP4Reader::Output(mp4_demuxer::TrackType aTrack, MediaData* aSample)
{
#ifdef LOG_SAMPLE_DECODE
  LOG("Decoded %s sample time=%lld dur=%lld",
      TrackTypeToStr(aTrack), aSample->mTime, aSample->mDuration);
#endif

  DecoderData& data = GetDecoderData(aTrack);
  // Don't accept output while we're flushing.
  MonitorAutoLock mon(data.mMonitor);
  if (data.mIsFlushing) {
    mon.NotifyAll();
    return;
  }

  switch (aTrack) {
    case kAudio: {
      MOZ_ASSERT(aSample->mType == MediaData::AUDIO_SAMPLES);
      AudioQueue().Push(static_cast<AudioData*>(aSample));
      break;
    }
    case kVideo: {
      MOZ_ASSERT(aSample->mType == MediaData::VIDEO_FRAME);
      VideoQueue().Push(static_cast<VideoData*>(aSample));
      break;
    }
    default:
      break;
  }

  data.mNumSamplesOutput++;
  mon.NotifyAll();
}

void
MP4Reader::InputExhausted(mp4_demuxer::TrackType aTrack)
{
  DecoderData& data = GetDecoderData(aTrack);
  MonitorAutoLock mon(data.mMonitor);
  data.mInputExhausted = true;
  mon.NotifyAll();
}

void
MP4Reader::Error(mp4_demuxer::TrackType aTrack)
{
  DecoderData& data = GetDecoderData(aTrack);
  MonitorAutoLock mon(data.mMonitor);
  data.mError = true;
  mon.NotifyAll();
}

bool
MP4Reader::DecodeAudioData()
{
  MOZ_ASSERT(HasAudio() && mPlatform && mAudio.mDecoder);
  return Decode(kAudio);
}

void
MP4Reader::Flush(mp4_demuxer::TrackType aTrack)
{
  DecoderData& data = GetDecoderData(aTrack);
  if (!data.mDecoder) {
    return;
  }
  // Purge the current decoder's state.
  // Set a flag so that we ignore all output while we call
  // MediaDataDecoder::Flush().
  {
    data.mIsFlushing = true;
    MonitorAutoLock mon(data.mMonitor);
  }
  data.mDecoder->Flush();
  {
    data.mIsFlushing = false;
    MonitorAutoLock mon(data.mMonitor);
  }
}

bool
MP4Reader::SkipVideoDemuxToNextKeyFrame(int64_t aTimeThreshold, uint32_t& parsed)
{
  MOZ_ASSERT(mVideo.mDecoder);

  Flush(kVideo);

  // Loop until we reach the next keyframe after the threshold.
  while (true) {
    nsAutoPtr<MP4Sample> compressed(PopSample(kVideo));
    if (!compressed) {
      // EOS, or error. Let the state machine know.
      return false;
    }
    parsed++;
    if (!compressed->is_sync_point ||
        compressed->composition_timestamp < aTimeThreshold) {
      continue;
    }
    mVideo.mDemuxedSamples.push_front(compressed.forget());
    break;
  }

  return true;
}

bool
MP4Reader::DecodeVideoFrame(bool &aKeyframeSkip,
                            int64_t aTimeThreshold)
{
  // Record number of frames decoded and parsed. Automatically update the
  // stats counters using the AutoNotifyDecoded stack-based class.
  uint32_t parsed = 0, decoded = 0;
  AbstractMediaDecoder::AutoNotifyDecoded autoNotify(mDecoder, parsed, decoded);

  MOZ_ASSERT(HasVideo() && mPlatform && mVideo.mDecoder);

  if (aKeyframeSkip) {
    bool ok = SkipVideoDemuxToNextKeyFrame(aTimeThreshold, parsed);
    if (!ok) {
      NS_WARNING("Failed to skip demux up to next keyframe");
      return false;
    }
    aKeyframeSkip = false;
    nsresult rv = mVideo.mDecoder->Flush();
    NS_ENSURE_SUCCESS(rv, false);
  }

  bool rv = Decode(kVideo);
  {
    // Report the number of "decoded" frames as the difference in the
    // mNumSamplesOutput field since the last time we were called.
    MonitorAutoLock mon(mVideo.mMonitor);
    uint64_t delta = mVideo.mNumSamplesOutput - mLastReportedNumDecodedFrames;
    decoded = static_cast<uint32_t>(delta);
    mLastReportedNumDecodedFrames = mVideo.mNumSamplesOutput;
  }
  return rv;
}

nsresult
MP4Reader::Seek(int64_t aTime,
                int64_t aStartTime,
                int64_t aEndTime,
                int64_t aCurrentTime)
{
  if (!mDemuxer->CanSeek()) {
    return NS_ERROR_FAILURE;
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

} // namespace mozilla
