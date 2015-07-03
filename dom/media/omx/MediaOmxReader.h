/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(MediaOmxReader_h_)
#define MediaOmxReader_h_

#include "MediaOmxCommonReader.h"
#include "MediaResource.h"
#include "MediaDecoderReader.h"
#include "nsMimeTypes.h"
#include "MP3FrameParser.h"
#include "nsRect.h"

#include <ui/GraphicBuffer.h>
#include <stagefright/MediaSource.h>

namespace android {
class OmxDecoder;
class MOZ_EXPORT MediaExtractor;
}

namespace mozilla {

class AbstractMediaDecoder;

class MediaOmxReader : public MediaOmxCommonReader
{
  typedef MediaOmxCommonReader::MediaResourcePromise MediaResourcePromise;

  // This mutex is held when accessing the mIsShutdown variable, which is
  // modified on the decode task queue and read on main and IO threads.
  Mutex mShutdownMutex;
  nsCString mType;
  bool mHasVideo;
  bool mHasAudio;
  nsIntRect mPicture;
  nsIntSize mInitialFrame;
  int64_t mVideoSeekTimeUs;
  int64_t mAudioSeekTimeUs;
  int64_t mLastParserDuration;
  int32_t mSkipCount;
  // If mIsShutdown is false, and mShutdownMutex is held, then
  // AbstractMediaDecoder::mDecoder will be non-null.
  bool mIsShutdown;
  MediaPromiseHolder<MediaDecoderReader::MetadataPromise> mMetadataPromise;
  MediaPromiseRequestHolder<MediaResourcePromise> mMediaResourceRequest;

  MediaPromiseHolder<MediaDecoderReader::SeekPromise> mSeekPromise;
  MediaPromiseRequestHolder<MediaDecoderReader::VideoDataPromise> mSeekRequest;
protected:
  android::sp<android::OmxDecoder> mOmxDecoder;
  android::sp<android::MediaExtractor> mExtractor;
  MP3FrameParser mMP3FrameParser;

  // Called by ReadMetadata() during MediaDecoderStateMachine::DecodeMetadata()
  // on decode thread. It create and initialize the OMX decoder including
  // setting up custom extractor. The extractor provide the essential
  // information used for creating OMX decoder such as video/audio codec.
  virtual nsresult InitOmxDecoder();

  // Called inside DecodeVideoFrame, DecodeAudioData, ReadMetadata and Seek
  // to activate the decoder automatically.
  virtual void EnsureActive();

  virtual void HandleResourceAllocated();

public:
  MediaOmxReader(AbstractMediaDecoder* aDecoder);
  ~MediaOmxReader();

  virtual nsresult Init(MediaDecoderReader* aCloneDonor);

protected:
  virtual void NotifyDataArrivedInternal(uint32_t aLength, int64_t aOffset) override;
public:

  virtual nsresult ResetDecode()
  {
    mSeekRequest.DisconnectIfExists();
    mSeekPromise.RejectIfExists(NS_OK, __func__);
    return MediaDecoderReader::ResetDecode();
  }

  virtual bool DecodeAudioData();
  virtual bool DecodeVideoFrame(bool &aKeyframeSkip,
                                int64_t aTimeThreshold);

  virtual bool HasAudio()
  {
    return mHasAudio;
  }

  virtual bool HasVideo()
  {
    return mHasVideo;
  }

  virtual void ReleaseMediaResources();

  virtual nsRefPtr<MediaDecoderReader::MetadataPromise> AsyncReadMetadata() override;

  virtual nsRefPtr<SeekPromise>
  Seek(int64_t aTime, int64_t aEndTime) override;

  virtual bool IsMediaSeekable() override;

  virtual void SetIdle() override;

  virtual nsRefPtr<ShutdownPromise> Shutdown() override;

  android::sp<android::MediaSource> GetAudioOffloadTrack();

  // This method is intended only for private use but public only for
  // MediaPromise::InvokeCallbackMethod().
  void ReleaseDecoder();

private:
  class ProcessCachedDataTask;
  class NotifyDataArrivedRunnable;

  bool IsShutdown() {
    MutexAutoLock lock(mShutdownMutex);
    return mIsShutdown;
  }

  int64_t ProcessCachedData(int64_t aOffset);

  already_AddRefed<AbstractMediaDecoder> SafeGetDecoder();
};

} // namespace mozilla

#endif
