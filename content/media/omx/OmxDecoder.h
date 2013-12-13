#include <stagefright/foundation/ABase.h>
#include <stagefright/foundation/AHandlerReflector.h>
#include <stagefright/foundation/ALooper.h>
#include <stagefright/MediaSource.h>
#include <stagefright/DataSource.h>
#include <stagefright/MediaSource.h>
#include <utils/RefBase.h>
#include <stagefright/MediaExtractor.h>

#include "GonkNativeWindow.h"
#include "GonkNativeWindowClient.h"
#include "GrallocImages.h"
#include "MP3FrameParser.h"
#include "MPAPI.h"
#include "MediaResource.h"
#include "AbstractMediaDecoder.h"
#include "OMXCodecProxy.h"

namespace android {
class OmxDecoder;
};

namespace mozilla {
namespace layers {

class VideoGraphicBuffer : public GraphicBufferLocked {
  // XXX change this to an actual smart pointer at some point
  android::MediaBuffer *mMediaBuffer;
  android::wp<android::OmxDecoder> mOmxDecoder;
  public:
    VideoGraphicBuffer(const android::wp<android::OmxDecoder> aOmxDecoder,
                       android::MediaBuffer *aBuffer,
                       SurfaceDescriptor& aDescriptor);
    ~VideoGraphicBuffer();
    void Unlock();
};

}
}

namespace android {

// MediaStreamSource is a DataSource that reads from a MPAPI media stream.
class MediaStreamSource : public DataSource {
  typedef mozilla::MediaResource MediaResource;
  typedef mozilla::AbstractMediaDecoder AbstractMediaDecoder;

  Mutex mLock;
  nsRefPtr<MediaResource> mResource;
  AbstractMediaDecoder *mDecoder;
public:
  MediaStreamSource(MediaResource* aResource,
                    AbstractMediaDecoder *aDecoder);

  virtual status_t initCheck() const;
  virtual ssize_t readAt(off64_t offset, void *data, size_t size);
  virtual ssize_t readAt(off_t offset, void *data, size_t size) {
    return readAt(static_cast<off64_t>(offset), data, size);
  }
  virtual status_t getSize(off_t *size) {
    off64_t size64;
    status_t status = getSize(&size64);
    *size = size64;
    return status;
  }
  virtual status_t getSize(off64_t *size);
  virtual uint32_t flags() {
    return kWantsPrefetching;
  }

  virtual ~MediaStreamSource();

private:
  MediaStreamSource(const MediaStreamSource &);
  MediaStreamSource &operator=(const MediaStreamSource &);
};

class OmxDecoder : public OMXCodecProxy::EventListener {
  typedef MPAPI::AudioFrame AudioFrame;
  typedef MPAPI::VideoFrame VideoFrame;
  typedef mozilla::MP3FrameParser MP3FrameParser;
  typedef mozilla::MediaResource MediaResource;
  typedef mozilla::AbstractMediaDecoder AbstractMediaDecoder;

  enum {
    kPreferSoftwareCodecs = 1,
    kSoftwareCodecsOnly = 8,
    kHardwareCodecsOnly = 16,
  };

  enum {
    kNotifyPostReleaseVideoBuffer = 'noti',
    kNotifyStatusChanged = 'stat'
  };

  AbstractMediaDecoder *mDecoder;
  nsRefPtr<MediaResource> mResource;
  sp<GonkNativeWindow> mNativeWindow;
  sp<GonkNativeWindowClient> mNativeWindowClient;
  sp<MediaSource> mVideoTrack;
  sp<OMXCodecProxy> mVideoSource;
  sp<MediaSource> mAudioTrack;
  sp<MediaSource> mAudioSource;
  int32_t mVideoWidth;
  int32_t mVideoHeight;
  int32_t mVideoColorFormat;
  int32_t mVideoStride;
  int32_t mVideoSliceHeight;
  int32_t mVideoRotation;
  int32_t mAudioChannels;
  int32_t mAudioSampleRate;
  int64_t mDurationUs;
  VideoFrame mVideoFrame;
  AudioFrame mAudioFrame;
  MP3FrameParser mMP3FrameParser;
  bool mIsMp3;

  // Lifetime of these should be handled by OMXCodec, as long as we release
  //   them after use: see ReleaseVideoBuffer(), ReleaseAudioBuffer()
  MediaBuffer *mVideoBuffer;
  MediaBuffer *mAudioBuffer;

  // Hold video's MediaBuffers that are released during video seeking.
  // The holded MediaBuffers are released soon after seek completion.
  // OMXCodec does not accept MediaBuffer during seeking. If MediaBuffer is
  //  returned to OMXCodec during seeking, OMXCodec calls assert.
  Vector<MediaBuffer *> mPendingVideoBuffers;
  // The lock protects mPendingVideoBuffers.
  Mutex mPendingVideoBuffersLock;

  // Show if OMXCodec is seeking.
  bool mIsVideoSeeking;
  // The lock protects video MediaBuffer release()'s pending operations called
  //  from multiple threads. The pending operations happen only during video
  //  seeking. Holding mSeekLock long time could affect to video rendering.
  // Holding time should be minimum.
  Mutex mSeekLock;

  // ALooper is a message loop used in stagefright.
  // It creates a thread for messages and handles messages in the thread.
  // ALooper is a clone of Looper in android Java.
  // http://developer.android.com/reference/android/os/Looper.html
  sp<ALooper> mLooper;
  // deliver a message to a wrapped object(OmxDecoder).
  // AHandlerReflector is similar to Handler in android Java.
  // http://developer.android.com/reference/android/os/Handler.html
  sp<AHandlerReflector<OmxDecoder> > mReflector;

  // 'true' if a read from the audio stream was done while reading the metadata
  bool mAudioMetadataRead;

  void ReleaseVideoBuffer();
  void ReleaseAudioBuffer();
  // Call with mSeekLock held.
  void ReleaseAllPendingVideoBuffersLocked();

  void PlanarYUV420Frame(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame);
  void CbYCrYFrame(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame);
  void SemiPlanarYUV420Frame(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame);
  void SemiPlanarYVU420Frame(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame);
  bool ToVideoFrame(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame);
  bool ToAudioFrame(AudioFrame *aFrame, int64_t aTimeUs, void *aData, size_t aDataOffset, size_t aSize,
                    int32_t aAudioChannels, int32_t aAudioSampleRate);

  //True if decoder is in a paused state
  bool mAudioPaused;
  bool mVideoPaused;

public:
  OmxDecoder(MediaResource *aResource, AbstractMediaDecoder *aDecoder);
  ~OmxDecoder();

  // MediaResourceManagerClient::EventListener
  virtual void statusChanged();

  // The MediaExtractor provides essential information for creating OMXCodec
  // instance. Such as video/audio codec, we can retrieve them through the
  // MediaExtractor::getTrackMetaData().
  // In general cases, the extractor is created by a sp<DataSource> which
  // connect to a MediaResource like ChannelMediaResource.
  // Data is read from the MediaResource to create a suitable extractor which
  // extracts data from a container.
  // Note: RTSP requires a custom extractor because it doesn't have a container.
  bool Init(sp<MediaExtractor>& extractor);

  bool TryLoad();
  bool IsDormantNeeded();
  bool IsWaitingMediaResources();
  bool AllocateMediaResources();
  void ReleaseMediaResources();
  bool SetVideoFormat();
  bool SetAudioFormat();

  void ReleaseDecoder();

  bool NotifyDataArrived(const char* aBuffer, uint32_t aLength, int64_t aOffset);

  void GetDuration(int64_t *durationUs) {
    *durationUs = mDurationUs;
  }

  void GetVideoParameters(int32_t *width, int32_t *height) {
    *width = mVideoWidth;
    *height = mVideoHeight;
  }

  void GetAudioParameters(int32_t *numChannels, int32_t *sampleRate) {
    *numChannels = mAudioChannels;
    *sampleRate = mAudioSampleRate;
  }

  bool HasVideo() {
    return mVideoSource != nullptr;
  }

  bool HasAudio() {
    return mAudioSource != nullptr;
  }

  bool ReadVideo(VideoFrame *aFrame, int64_t aSeekTimeUs,
                 bool aKeyframeSkip = false,
                 bool aDoSeek = false);
  bool ReadAudio(AudioFrame *aFrame, int64_t aSeekTimeUs);

  MediaResource *GetResource() {
    return mResource;
  }

  //Change decoder into a playing state
  nsresult Play();

  //Change decoder into a paused state
  void Pause();

  // Post kNotifyPostReleaseVideoBuffer message to OmxDecoder via ALooper.
  void PostReleaseVideoBuffer(MediaBuffer *aBuffer);
  // Receive a message from AHandlerReflector.
  // Called on ALooper thread.
  void onMessageReceived(const sp<AMessage> &msg);

  int64_t ProcessCachedData(int64_t aOffset, bool aWaitForCompletion);
};

}

