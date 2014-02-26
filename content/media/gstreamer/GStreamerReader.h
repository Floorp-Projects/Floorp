/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(GStreamerReader_h_)
#define GStreamerReader_h_

#include <map>

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
// This include trips -Wreserved-user-defined-literal on clang. Ignoring it
// trips -Wpragmas on GCC (unknown warning), but ignoring that trips
// -Wunknown-pragmas on clang (unknown pragma).
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wreserved-user-defined-literal"
#include <gst/video/video.h>
#pragma GCC diagnostic pop

#include "MediaDecoderReader.h"
#include "MP3FrameParser.h"
#include "ImageContainer.h"
#include "nsRect.h"

namespace mozilla {

namespace dom {
class TimeRanges;
}

class AbstractMediaDecoder;

class GStreamerReader : public MediaDecoderReader
{
  typedef gfx::IntRect IntRect;

public:
  GStreamerReader(AbstractMediaDecoder* aDecoder);
  virtual ~GStreamerReader();

  virtual nsresult Init(MediaDecoderReader* aCloneDonor);
  virtual nsresult ResetDecode();
  virtual bool DecodeAudioData();
  virtual bool DecodeVideoFrame(bool &aKeyframeSkip,
                                int64_t aTimeThreshold);
  virtual nsresult ReadMetadata(MediaInfo* aInfo,
                                MetadataTags** aTags);
  virtual nsresult Seek(int64_t aTime,
                        int64_t aStartTime,
                        int64_t aEndTime,
                        int64_t aCurrentTime);
  virtual nsresult GetBuffered(dom::TimeRanges* aBuffered, int64_t aStartTime);

  virtual void NotifyDataArrived(const char *aBuffer,
                                 uint32_t aLength,
                                 int64_t aOffset) MOZ_OVERRIDE;

  virtual bool HasAudio() {
    return mInfo.HasAudio();
  }

  virtual bool HasVideo() {
    return mInfo.HasVideo();
  }

  layers::ImageContainer* GetImageContainer() { return mDecoder->GetImageContainer(); }

private:

  void ReadAndPushData(guint aLength);
  nsRefPtr<layers::PlanarYCbCrImage> GetImageFromBuffer(GstBuffer* aBuffer);
  void CopyIntoImageBuffer(GstBuffer *aBuffer, GstBuffer** aOutBuffer, nsRefPtr<layers::PlanarYCbCrImage> &image);
  GstCaps* BuildAudioSinkCaps();
  void InstallPadCallbacks();

#if GST_VERSION_MAJOR >= 1
  void ImageDataFromVideoFrame(GstVideoFrame *aFrame, layers::PlanarYCbCrImage::Data *aData);
#endif

  /* Called once the pipeline is setup to check that the stream only contains
   * supported formats
   */
  nsresult CheckSupportedFormats();

  /* Gst callbacks */

  static GstBusSyncReply ErrorCb(GstBus *aBus, GstMessage *aMessage, gpointer aUserData);
  GstBusSyncReply Error(GstBus *aBus, GstMessage *aMessage);

  /* Called on the source-setup signal emitted by playbin. Used to
   * configure appsrc .
   */
  static void PlayBinSourceSetupCb(GstElement* aPlayBin,
                                   GParamSpec* pspec,
                                   gpointer aUserData);
  void PlayBinSourceSetup(GstAppSrc* aSource);

  /* Called from appsrc when we need to read more data from the resource */
  static void NeedDataCb(GstAppSrc* aSrc, guint aLength, gpointer aUserData);
  void NeedData(GstAppSrc* aSrc, guint aLength);

  /* Called when appsrc has enough data and we can stop reading */
  static void EnoughDataCb(GstAppSrc* aSrc, gpointer aUserData);
  void EnoughData(GstAppSrc* aSrc);

  /* Called when a seek is issued on the pipeline */
  static gboolean SeekDataCb(GstAppSrc* aSrc,
                             guint64 aOffset,
                             gpointer aUserData);
  gboolean SeekData(GstAppSrc* aSrc, guint64 aOffset);

  /* Called when events reach the sinks. See inline comments */
#if GST_VERSION_MAJOR == 1
  static GstPadProbeReturn EventProbeCb(GstPad *aPad, GstPadProbeInfo *aInfo, gpointer aUserData);
  GstPadProbeReturn EventProbe(GstPad *aPad, GstEvent *aEvent);
#else
  static gboolean EventProbeCb(GstPad* aPad, GstEvent* aEvent, gpointer aUserData);
  gboolean EventProbe(GstPad* aPad, GstEvent* aEvent);
#endif

  /* Called when the video part of the pipeline allocates buffers. Used to
   * provide PlanarYCbCrImage backed GstBuffers to the pipeline so that a memory
   * copy can be avoided when handling YUV buffers from the pipeline to the gfx
   * side.
   */
#if GST_VERSION_MAJOR == 1
  static GstPadProbeReturn QueryProbeCb(GstPad *aPad, GstPadProbeInfo *aInfo, gpointer aUserData);
  GstPadProbeReturn QueryProbe(GstPad *aPad, GstPadProbeInfo *aInfo, gpointer aUserData);
#else
  static GstFlowReturn AllocateVideoBufferCb(GstPad* aPad, guint64 aOffset, guint aSize,
                                             GstCaps* aCaps, GstBuffer** aBuf);
  GstFlowReturn AllocateVideoBufferFull(GstPad* aPad, guint64 aOffset, guint aSize,
                                     GstCaps* aCaps, GstBuffer** aBuf, nsRefPtr<layers::PlanarYCbCrImage>& aImage);
  GstFlowReturn AllocateVideoBuffer(GstPad* aPad, guint64 aOffset, guint aSize,
                                     GstCaps* aCaps, GstBuffer** aBuf);
#endif


  /* Called when the pipeline is prerolled, that is when at start or after a
   * seek, the first audio and video buffers are queued in the sinks.
   */
  static GstFlowReturn NewPrerollCb(GstAppSink* aSink, gpointer aUserData);
  void VideoPreroll();
  void AudioPreroll();

  /* Called when buffers reach the sinks */
  static GstFlowReturn NewBufferCb(GstAppSink* aSink, gpointer aUserData);
  void NewVideoBuffer();
  void NewAudioBuffer();

  /* Called at end of stream, when decoding has finished */
  static void EosCb(GstAppSink* aSink, gpointer aUserData);
  /* Notifies that a sink will no longer receive any more data. If nullptr
   * is passed to this, we'll assume all streams have reached EOS (for example
   * an error has occurred). */
  void Eos(GstAppSink* aSink = nullptr);

  /* Called when an element is added inside playbin. We use it to find the
   * decodebin instance.
   */
  static void PlayElementAddedCb(GstBin *aBin, GstElement *aElement,
                                 gpointer *aUserData);

  /* Called during decoding, to decide whether a (sub)stream should be decoded or
   * ignored */
  static bool ShouldAutoplugFactory(GstElementFactory* aFactory, GstCaps* aCaps);

  /* Called by decodebin during autoplugging. We use it to apply our
   * container/codec whitelist.
   */
  static GValueArray* AutoplugSortCb(GstElement* aElement,
                                     GstPad* aPad, GstCaps* aCaps,
                                     GValueArray* aFactories);

  // Try to find MP3 headers in this stream using our MP3 frame parser.
  nsresult ParseMP3Headers();

  // Use our own MP3 parser here, largely for consistency with other platforms.
  MP3FrameParser mMP3FrameParser;

  // We want to be able to decide in |ReadMetadata| whether or not we use the
  // duration from the MP3 frame parser, as this backend supports more than just
  // MP3. But |NotifyDataArrived| can update the duration and is often called
  // _before_ |ReadMetadata|. This flag stops the former from using the parser
  // duration until we are sure we want to.
  bool mUseParserDuration;
  int64_t mLastParserDuration;

#if GST_VERSION_MAJOR >= 1
  GstAllocator *mAllocator;
  GstBufferPool *mBufferPool;
  GstVideoInfo mVideoInfo;
#endif
  GstElement* mPlayBin;
  GstBus* mBus;
  GstAppSrc* mSource;
  /* video sink bin */
  GstElement* mVideoSink;
  /* the actual video app sink */
  GstAppSink* mVideoAppSink;
  /* audio sink bin */
  GstElement* mAudioSink;
  /* the actual audio app sink */
  GstAppSink* mAudioAppSink;
  GstVideoFormat mFormat;
  IntRect mPicture;
  int mVideoSinkBufferCount;
  int mAudioSinkBufferCount;
  GstAppSrcCallbacks mSrcCallbacks;
  GstAppSinkCallbacks mSinkCallbacks;
  /* monitor used to synchronize access to shared state between gstreamer
   * threads and other gecko threads */
  ReentrantMonitor mGstThreadsMonitor;
  /* video and audio segments we use to convert absolute timestamps to [0,
   * stream_duration]. They're set when the pipeline is started or after a seek.
   * Concurrent access guarded with mGstThreadsMonitor.
   */
  GstSegment mVideoSegment;
  GstSegment mAudioSegment;
  /* bool used to signal when gst has detected the end of stream and
   * DecodeAudioData and DecodeVideoFrame should not expect any more data
   */
  bool mReachedAudioEos;
  bool mReachedVideoEos;
#if GST_VERSION_MAJOR >= 1
  bool mConfigureAlignment;
#endif
  int fpsNum;
  int fpsDen;
};

} // namespace mozilla

#endif
