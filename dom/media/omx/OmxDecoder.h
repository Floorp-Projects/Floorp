#include <set>
#include <stagefright/foundation/ABase.h>
#include <stagefright/foundation/AHandlerReflector.h>
#include <stagefright/foundation/ALooper.h>
#include <utils/RefBase.h>
#include <stagefright/MediaExtractor.h>

#include "GonkNativeWindow.h"
#include "GonkNativeWindowClient.h"
#include "mozilla/layers/FenceUtils.h"
#include "MP3FrameParser.h"
#include "MPAPI.h"
#include "MediaOmxCommonReader.h"
#include "AbstractMediaDecoder.h"
#include "OMXCodecProxy.h"

namespace android {
class OmxDecoder;
};

namespace android {

class OmxDecoder : public RefBase {
  typedef MPAPI::AudioFrame AudioFrame;
  typedef MPAPI::VideoFrame VideoFrame;
  typedef mozilla::MP3FrameParser MP3FrameParser;
  typedef mozilla::MediaResource MediaResource;
  typedef mozilla::AbstractMediaDecoder AbstractMediaDecoder;
  typedef mozilla::layers::FenceHandle FenceHandle;
  typedef mozilla::layers::TextureClient TextureClient;
  typedef mozilla::MediaOmxCommonReader::MediaResourcePromise MediaResourcePromise;

  enum {
    kPreferSoftwareCodecs = 1,
    kSoftwareCodecsOnly = 8,
    kHardwareCodecsOnly = 16,
  };

  enum {
    kNotifyPostReleaseVideoBuffer = 'noti',
  };

  AbstractMediaDecoder *mDecoder;
  sp<GonkNativeWindow> mNativeWindow;
  sp<GonkNativeWindowClient> mNativeWindowClient;
  sp<MediaSource> mVideoTrack;
  sp<OMXCodecProxy> mVideoSource;
  sp<MediaSource> mAudioOffloadTrack;
  sp<MediaSource> mAudioTrack;
  sp<MediaSource> mAudioSource;
  int32_t mDisplayWidth;
  int32_t mDisplayHeight;
  int32_t mVideoWidth;
  int32_t mVideoHeight;
  int32_t mVideoColorFormat;
  int32_t mVideoStride;
  int32_t mVideoSliceHeight;
  int32_t mVideoRotation;
  int32_t mAudioChannels;
  int32_t mAudioSampleRate;
  int64_t mDurationUs;
  int64_t mLastSeekTime;

  VideoFrame mVideoFrame;
  AudioFrame mAudioFrame;
  MP3FrameParser mMP3FrameParser;
  bool mIsMp3;

  // Lifetime of these should be handled by OMXCodec, as long as we release
  //   them after use: see ReleaseVideoBuffer(), ReleaseAudioBuffer()
  MediaBuffer *mVideoBuffer;
  MediaBuffer *mAudioBuffer;

  struct BufferItem {
    BufferItem()
     : mMediaBuffer(nullptr)
    {
    }
    BufferItem(MediaBuffer* aMediaBuffer, const FenceHandle& aReleaseFenceHandle)
     : mMediaBuffer(aMediaBuffer)
     , mReleaseFenceHandle(aReleaseFenceHandle) {
    }

    MediaBuffer* mMediaBuffer;
    // a fence will signal when the current buffer is no longer being read.
    FenceHandle mReleaseFenceHandle;
  };

  // Hold video's MediaBuffers that are released during video seeking.
  // The holded MediaBuffers are released soon after seek completion.
  // OMXCodec does not accept MediaBuffer during seeking. If MediaBuffer is
  //  returned to OMXCodec during seeking, OMXCodec calls assert.
  Vector<BufferItem> mPendingVideoBuffers;

  // Hold TextureClients that are waiting to be recycled.
  std::set<TextureClient*> mPendingRecycleTexutreClients;

  // The lock protects mPendingVideoBuffers and mPendingRecycleTexutreClients.
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

  RefPtr<mozilla::TaskQueue> mTaskQueue;

  mozilla::MozPromiseRequestHolder<OMXCodecProxy::CodecPromise> mVideoCodecRequest;
  mozilla::MozPromiseHolder<MediaResourcePromise> mMediaResourcePromise;

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

  mozilla::TaskQueue* OwnerThread() const
  {
    return mTaskQueue;
  }

public:
  explicit OmxDecoder(AbstractMediaDecoder *aDecoder, mozilla::TaskQueue* aTaskQueue);
  ~OmxDecoder();

  // The MediaExtractor provides essential information for creating OMXCodec
  // instance. Such as video/audio codec, we can retrieve them through the
  // MediaExtractor::getTrackMetaData().
  // In general cases, the extractor is created by a sp<DataSource> which
  // connect to a MediaResource like ChannelMediaResource.
  // Data is read from the MediaResource to create a suitable extractor which
  // extracts data from a container.
  // Note: RTSP requires a custom extractor because it doesn't have a container.
  bool Init(sp<MediaExtractor>& extractor);

  // Called after resources(video/audio codec) are allocated, set the
  // mDurationUs and video/audio metadata.
  bool EnsureMetadata();

  RefPtr<MediaResourcePromise> AllocateMediaResources();
  void ReleaseMediaResources();
  bool SetVideoFormat();
  bool SetAudioFormat();

  void ReleaseDecoder();

  void GetDuration(int64_t *durationUs) {
    *durationUs = mDurationUs;
  }

  void GetVideoParameters(int32_t* aDisplayWidth, int32_t* aDisplayHeight,
                          int32_t* aWidth, int32_t* aHeight) {
    *aDisplayWidth = mDisplayWidth;
    *aDisplayHeight = mDisplayHeight;
    *aWidth = mVideoWidth;
    *aHeight = mVideoHeight;
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

  //Change decoder into a playing state
  nsresult Play();

  //Change decoder into a paused state
  void Pause();

  // Post kNotifyPostReleaseVideoBuffer message to OmxDecoder via ALooper.
  void PostReleaseVideoBuffer(MediaBuffer *aBuffer, const FenceHandle& aReleaseFenceHandle);
  // Receive a message from AHandlerReflector.
  // Called on ALooper thread.
  void onMessageReceived(const sp<AMessage> &msg);

  sp<MediaSource> GetAudioOffloadTrack() { return mAudioOffloadTrack; }

  void RecycleCallbackImp(TextureClient* aClient);

  static void RecycleCallback(TextureClient* aClient, void* aClosure);
};

}

