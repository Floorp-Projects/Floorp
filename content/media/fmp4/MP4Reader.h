/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MP4Reader_h_)
#define MP4Reader_h_

#include "MediaDecoderReader.h"
#include "nsAutoPtr.h"
#include "PlatformDecoderModule.h"
#include "mp4_demuxer/mp4_demuxer.h"
#include "MediaTaskQueue.h"

#include <deque>
#include "mozilla/Monitor.h"

namespace mozilla {

namespace dom {
class TimeRanges;
}

typedef std::deque<mp4_demuxer::MP4Sample*> MP4SampleQueue;

class MP4Stream;

class MP4Reader : public MediaDecoderReader
{
public:
  MP4Reader(AbstractMediaDecoder* aDecoder);

  virtual ~MP4Reader();

  virtual nsresult Init(MediaDecoderReader* aCloneDonor) MOZ_OVERRIDE;

  virtual bool DecodeAudioData() MOZ_OVERRIDE;
  virtual bool DecodeVideoFrame(bool &aKeyframeSkip,
                                int64_t aTimeThreshold) MOZ_OVERRIDE;

  virtual bool HasAudio() MOZ_OVERRIDE;
  virtual bool HasVideo() MOZ_OVERRIDE;

  virtual nsresult ReadMetadata(MediaInfo* aInfo,
                                MetadataTags** aTags) MOZ_OVERRIDE;

  virtual nsresult Seek(int64_t aTime,
                        int64_t aStartTime,
                        int64_t aEndTime,
                        int64_t aCurrentTime) MOZ_OVERRIDE;

  virtual bool IsMediaSeekable() MOZ_OVERRIDE;

  virtual nsresult GetBuffered(dom::TimeRanges* aBuffered,
                               int64_t aStartTime) MOZ_OVERRIDE;

private:

  // Destroys all decoder resources.
  void Shutdown();

  // Initializes mLayersBackendType if possible.
  void InitLayersBackendType();

  // Blocks until the demuxer produces an sample of specified type.
  // Returns nullptr on error on EOS. Caller must delete sample.
  mp4_demuxer::MP4Sample* PopSample(mp4_demuxer::TrackType aTrack);

  bool SkipVideoDemuxToNextKeyFrame(int64_t aTimeThreshold, uint32_t& parsed);

  void Output(mp4_demuxer::TrackType aType, MediaData* aSample);
  void InputExhausted(mp4_demuxer::TrackType aTrack);
  void Error(mp4_demuxer::TrackType aTrack);
  bool Decode(mp4_demuxer::TrackType aTrack);
  void Flush(mp4_demuxer::TrackType aTrack);

  nsAutoPtr<mp4_demuxer::MP4Demuxer> mDemuxer;
  nsAutoPtr<PlatformDecoderModule> mPlatform;

  class DecoderCallback : public MediaDataDecoderCallback {
  public:
    DecoderCallback(MP4Reader* aReader,
                    mp4_demuxer::TrackType aType)
      : mReader(aReader)
      , mType(aType)
    {
    }
    virtual void Output(MediaData* aSample) MOZ_OVERRIDE {
      mReader->Output(mType, aSample);
    }
    virtual void InputExhausted() MOZ_OVERRIDE {
      mReader->InputExhausted(mType);
    }
    virtual void Error() MOZ_OVERRIDE {
      mReader->Error(mType);
    }
  private:
    MP4Reader* mReader;
    mp4_demuxer::TrackType mType;
  };

  struct DecoderData {
    DecoderData(const char* aMonitorName,
                uint32_t aDecodeAhead)
      : mMonitor(aMonitorName)
      , mNumSamplesInput(0)
      , mNumSamplesOutput(0)
      , mDecodeAhead(aDecodeAhead)
      , mActive(false)
      , mInputExhausted(false)
      , mError(false)
      , mIsFlushing(false)
    {
    }

    // The platform decoder.
    RefPtr<MediaDataDecoder> mDecoder;
    // TaskQueue on which decoder can choose to decode.
    // Only non-null up until the decoder is created.
    RefPtr<MediaTaskQueue> mTaskQueue;
    // Callback that receives output and error notifications from the decoder.
    nsAutoPtr<DecoderCallback> mCallback;
    // Monitor that protects all non-threadsafe state; the primitives
    // that follow.
    Monitor mMonitor;
    uint64_t mNumSamplesInput;
    uint64_t mNumSamplesOutput;
    uint32_t mDecodeAhead;
    // Whether this stream exists in the media.
    bool mActive;
    bool mInputExhausted;
    bool mError;
    bool mIsFlushing;
  };
  DecoderData mAudio;
  DecoderData mVideo;
  // Queued frame extracted by the demuxer, but not yet sent to the platform
  // decoder.
  nsAutoPtr<mp4_demuxer::MP4Sample> mQueuedVideoSample;

  // The last number of decoded output frames that we've reported to
  // MediaDecoder::NotifyDecoded(). We diff the number of output video
  // frames every time that DecodeVideoData() is called, and report the
  // delta there.
  uint64_t mLastReportedNumDecodedFrames;

  DecoderData& GetDecoderData(mp4_demuxer::TrackType aTrack);
  MP4SampleQueue& SampleQueue(mp4_demuxer::TrackType aTrack);
  MediaDataDecoder* Decoder(mp4_demuxer::TrackType aTrack);

  layers::LayersBackend mLayersBackendType;

};

} // namespace mozilla

#endif
