/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsError.h"
#include "MediaDecoderStateMachine.h"
#include "AbstractMediaDecoder.h"
#include "MediaResource.h"
#include "GStreamerReader.h"
#include "GStreamerFormatHelper.h"
#include "GStreamerMozVideoBuffer.h"
#include "VideoUtils.h"
#include "mozilla/dom/TimeRanges.h"
#include "mozilla/Preferences.h"

namespace mozilla {

using namespace layers;

// Un-comment to enable logging of seek bisections.
//#define SEEK_LOGGING

#ifdef PR_LOGGING
extern PRLogModuleInfo* gMediaDecoderLog;
#define LOG(type, msg) PR_LOG(gMediaDecoderLog, type, msg)
#else
#define LOG(type, msg)
#endif

extern bool
IsYV12Format(const VideoData::YCbCrBuffer::Plane& aYPlane,
             const VideoData::YCbCrBuffer::Plane& aCbPlane,
             const VideoData::YCbCrBuffer::Plane& aCrPlane);

static const int MAX_CHANNELS = 4;
// Let the demuxer work in pull mode for short files
static const int SHORT_FILE_SIZE = 1024 * 1024;
// The default resource->Read() size when working in push mode
static const int DEFAULT_SOURCE_READ_SIZE = 50 * 1024;

typedef enum {
  GST_PLAY_FLAG_VIDEO         = (1 << 0),
  GST_PLAY_FLAG_AUDIO         = (1 << 1),
  GST_PLAY_FLAG_TEXT          = (1 << 2),
  GST_PLAY_FLAG_VIS           = (1 << 3),
  GST_PLAY_FLAG_SOFT_VOLUME   = (1 << 4),
  GST_PLAY_FLAG_NATIVE_AUDIO  = (1 << 5),
  GST_PLAY_FLAG_NATIVE_VIDEO  = (1 << 6),
  GST_PLAY_FLAG_DOWNLOAD      = (1 << 7),
  GST_PLAY_FLAG_BUFFERING     = (1 << 8),
  GST_PLAY_FLAG_DEINTERLACE   = (1 << 9),
  GST_PLAY_FLAG_SOFT_COLORBALANCE = (1 << 10)
} PlayFlags;

GStreamerReader::GStreamerReader(AbstractMediaDecoder* aDecoder)
  : MediaDecoderReader(aDecoder),
  mPlayBin(nullptr),
  mBus(nullptr),
  mSource(nullptr),
  mVideoSink(nullptr),
  mVideoAppSink(nullptr),
  mAudioSink(nullptr),
  mAudioAppSink(nullptr),
  mFormat(GST_VIDEO_FORMAT_UNKNOWN),
  mVideoSinkBufferCount(0),
  mAudioSinkBufferCount(0),
  mGstThreadsMonitor("media.gst.threads"),
  mReachedEos(false),
  mByteOffset(0),
  mLastReportedByteOffset(0),
  fpsNum(0),
  fpsDen(0)
{
  MOZ_COUNT_CTOR(GStreamerReader);

  mSrcCallbacks.need_data = GStreamerReader::NeedDataCb;
  mSrcCallbacks.enough_data = GStreamerReader::EnoughDataCb;
  mSrcCallbacks.seek_data = GStreamerReader::SeekDataCb;

  mSinkCallbacks.eos = GStreamerReader::EosCb;
  mSinkCallbacks.new_preroll = GStreamerReader::NewPrerollCb;
  mSinkCallbacks.new_buffer = GStreamerReader::NewBufferCb;
  mSinkCallbacks.new_buffer_list = nullptr;

  gst_segment_init(&mVideoSegment, GST_FORMAT_UNDEFINED);
  gst_segment_init(&mAudioSegment, GST_FORMAT_UNDEFINED);
}

GStreamerReader::~GStreamerReader()
{
  MOZ_COUNT_DTOR(GStreamerReader);
  ResetDecode();

  if (mPlayBin) {
    gst_app_src_end_of_stream(mSource);
    if (mSource)
      gst_object_unref(mSource);
    gst_element_set_state(mPlayBin, GST_STATE_NULL);
    gst_object_unref(mPlayBin);
    mPlayBin = nullptr;
    mVideoSink = nullptr;
    mVideoAppSink = nullptr;
    mAudioSink = nullptr;
    mAudioAppSink = nullptr;
    gst_object_unref(mBus);
    mBus = nullptr;
  }
}

nsresult GStreamerReader::Init(MediaDecoderReader* aCloneDonor)
{
  GError* error = nullptr;
  if (!gst_init_check(0, 0, &error)) {
    LOG(PR_LOG_ERROR, ("gst initialization failed: %s", error->message));
    g_error_free(error);
    return NS_ERROR_FAILURE;
  }

  mPlayBin = gst_element_factory_make("playbin2", nullptr);
  if (!mPlayBin) {
    LOG(PR_LOG_ERROR, ("couldn't create playbin2"));
    return NS_ERROR_FAILURE;
  }
  g_object_set(mPlayBin, "buffer-size", 0, nullptr);
  mBus = gst_pipeline_get_bus(GST_PIPELINE(mPlayBin));

  mVideoSink = gst_parse_bin_from_description("capsfilter name=filter ! "
      "appsink name=videosink sync=true max-buffers=1 "
      "caps=video/x-raw-yuv,format=(fourcc)I420"
      , TRUE, nullptr);
  mVideoAppSink = GST_APP_SINK(gst_bin_get_by_name(GST_BIN(mVideoSink),
        "videosink"));
  gst_app_sink_set_callbacks(mVideoAppSink, &mSinkCallbacks,
      (gpointer) this, nullptr);
  GstPad* sinkpad = gst_element_get_pad(GST_ELEMENT(mVideoAppSink), "sink");
  gst_pad_add_event_probe(sinkpad,
      G_CALLBACK(&GStreamerReader::EventProbeCb), this);
  gst_object_unref(sinkpad);
  gst_pad_set_bufferalloc_function(sinkpad, GStreamerReader::AllocateVideoBufferCb);
  gst_pad_set_element_private(sinkpad, this);

  mAudioSink = gst_parse_bin_from_description("capsfilter name=filter ! "
#ifdef MOZ_SAMPLE_TYPE_FLOAT32
        "appsink name=audiosink sync=true caps=audio/x-raw-float,"
#ifdef IS_LITTLE_ENDIAN
        "channels={1,2},width=32,endianness=1234", TRUE, nullptr);
#else
        "channels={1,2},width=32,endianness=4321", TRUE, nullptr);
#endif
#else
        "appsink name=audiosink sync=true caps=audio/x-raw-int,"
#ifdef IS_LITTLE_ENDIAN
        "channels={1,2},width=16,endianness=1234", TRUE, nullptr);
#else
        "channels={1,2},width=16,endianness=4321", TRUE, nullptr);
#endif
#endif
  mAudioAppSink = GST_APP_SINK(gst_bin_get_by_name(GST_BIN(mAudioSink),
                                                   "audiosink"));
  gst_app_sink_set_callbacks(mAudioAppSink, &mSinkCallbacks,
                             (gpointer) this, nullptr);
  sinkpad = gst_element_get_pad(GST_ELEMENT(mAudioAppSink), "sink");
  gst_pad_add_event_probe(sinkpad,
                          G_CALLBACK(&GStreamerReader::EventProbeCb), this);
  gst_object_unref(sinkpad);

  g_object_set(mPlayBin, "uri", "appsrc://",
               "video-sink", mVideoSink,
               "audio-sink", mAudioSink,
               nullptr);

  g_signal_connect(G_OBJECT(mPlayBin), "notify::source",
                   G_CALLBACK(GStreamerReader::PlayBinSourceSetupCb), this);

  return NS_OK;
}

void GStreamerReader::PlayBinSourceSetupCb(GstElement* aPlayBin,
                                           GParamSpec* pspec,
                                           gpointer aUserData)
{
  GstElement *source;
  GStreamerReader* reader = reinterpret_cast<GStreamerReader*>(aUserData);

  g_object_get(aPlayBin, "source", &source, NULL);
  reader->PlayBinSourceSetup(GST_APP_SRC(source));
}

void GStreamerReader::PlayBinSourceSetup(GstAppSrc* aSource)
{
  mSource = GST_APP_SRC(aSource);
  gst_app_src_set_callbacks(mSource, &mSrcCallbacks, (gpointer) this, nullptr);
  MediaResource* resource = mDecoder->GetResource();

  /* do a short read to trigger a network request so that GetLength() below
   * returns something meaningful and not -1
   */
  char buf[512];
  unsigned int size = 0;
  resource->Read(buf, sizeof(buf), &size);
  resource->Seek(SEEK_SET, 0);

  /* now we should have a length */
  int64_t resourceLength = resource->GetLength();
  gst_app_src_set_size(mSource, resourceLength);
  if (resource->IsDataCachedToEndOfResource(0) ||
      (resourceLength != -1 && resourceLength <= SHORT_FILE_SIZE)) {
    /* let the demuxer work in pull mode for local files (or very short files)
     * so that we get optimal seeking accuracy/performance
     */
    LOG(PR_LOG_DEBUG, ("configuring random access, len %lld", resourceLength));
    gst_app_src_set_stream_type(mSource, GST_APP_STREAM_TYPE_RANDOM_ACCESS);
  } else {
    /* make the demuxer work in push mode so that seeking is kept to a minimum
     */
    LOG(PR_LOG_DEBUG, ("configuring push mode, len %lld", resourceLength));
    gst_app_src_set_stream_type(mSource, GST_APP_STREAM_TYPE_SEEKABLE);
  }
}

nsresult GStreamerReader::ReadMetadata(VideoInfo* aInfo,
                                       MetadataTags** aTags)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  nsresult ret = NS_OK;

  /* We do 3 attempts here: decoding audio and video, decoding video only,
   * decoding audio only. This allows us to play streams that have one broken
   * stream but that are otherwise decodeable.
   */
  guint flags[3] = {GST_PLAY_FLAG_VIDEO|GST_PLAY_FLAG_AUDIO,
    static_cast<guint>(~GST_PLAY_FLAG_AUDIO), static_cast<guint>(~GST_PLAY_FLAG_VIDEO)};
  guint default_flags, current_flags;
  g_object_get(mPlayBin, "flags", &default_flags, nullptr);

  GstMessage* message = nullptr;
  for (unsigned int i = 0; i < G_N_ELEMENTS(flags); i++) {
    current_flags = default_flags & flags[i];
    g_object_set(G_OBJECT(mPlayBin), "flags", current_flags, nullptr);

    /* reset filter caps to ANY */
    GstCaps* caps = gst_caps_new_any();
    GstElement* filter = gst_bin_get_by_name(GST_BIN(mAudioSink), "filter");
    g_object_set(filter, "caps", caps, nullptr);
    gst_object_unref(filter);

    filter = gst_bin_get_by_name(GST_BIN(mVideoSink), "filter");
    g_object_set(filter, "caps", caps, nullptr);
    gst_object_unref(filter);
    gst_caps_unref(caps);
    filter = nullptr;

    if (!(current_flags & GST_PLAY_FLAG_AUDIO))
      filter = gst_bin_get_by_name(GST_BIN(mAudioSink), "filter");
    else if (!(current_flags & GST_PLAY_FLAG_VIDEO))
      filter = gst_bin_get_by_name(GST_BIN(mVideoSink), "filter");

    if (filter) {
      /* Little trick: set the target caps to "skip" so that playbin2 fails to
       * find a decoder for the stream we want to skip.
       */
      GstCaps* filterCaps = gst_caps_new_simple ("skip", nullptr);
      g_object_set(filter, "caps", filterCaps, nullptr);
      gst_caps_unref(filterCaps);
      gst_object_unref(filter);
    }

    /* start the pipeline */
    gst_element_set_state(mPlayBin, GST_STATE_PAUSED);

    /* Wait for ASYNC_DONE, which is emitted when the pipeline is built,
     * prerolled and ready to play. Also watch for errors.
     */
    message = gst_bus_timed_pop_filtered(mBus, GST_CLOCK_TIME_NONE,
                 (GstMessageType)(GST_MESSAGE_ASYNC_DONE | GST_MESSAGE_ERROR));
    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR) {
      GError* error;
      gchar* debug;

      gst_message_parse_error(message, &error, &debug);
      LOG(PR_LOG_ERROR, ("read metadata error: %s: %s", error->message,
                         debug));
      g_error_free(error);
      g_free(debug);
      gst_element_set_state(mPlayBin, GST_STATE_NULL);
      gst_message_unref(message);
      ret = NS_ERROR_FAILURE;
    } else {
      gst_message_unref(message);
      ret = NS_OK;
      break;
    }
  }

  if (NS_SUCCEEDED(ret))
    ret = CheckSupportedFormats();

  if (NS_FAILED(ret))
    /* we couldn't get this to play */
    return ret;

  /* FIXME: workaround for a bug in matroskademux. This seek makes matroskademux
   * parse the index */
  if (gst_element_seek_simple(mPlayBin, GST_FORMAT_TIME,
        GST_SEEK_FLAG_FLUSH, 0)) {
    /* after a seek we need to wait again for ASYNC_DONE */
    message = gst_bus_timed_pop_filtered(mBus, GST_CLOCK_TIME_NONE,
       (GstMessageType)(GST_MESSAGE_ASYNC_DONE | GST_MESSAGE_ERROR));
    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR) {
      gst_element_set_state(mPlayBin, GST_STATE_NULL);
      gst_message_unref(message);
      return NS_ERROR_FAILURE;
    }
  }

  /* report the duration */
  gint64 duration;
  GstFormat format = GST_FORMAT_TIME;
  if (gst_element_query_duration(GST_ELEMENT(mPlayBin),
      &format, &duration) && format == GST_FORMAT_TIME) {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    LOG(PR_LOG_DEBUG, ("returning duration %" GST_TIME_FORMAT,
          GST_TIME_ARGS (duration)));
    duration = GST_TIME_AS_USECONDS (duration);
    mDecoder->SetMediaDuration(duration);
  }

  int n_video = 0, n_audio = 0;
  g_object_get(mPlayBin, "n-video", &n_video, "n-audio", &n_audio, nullptr);
  mInfo.mHasVideo = n_video != 0;
  mInfo.mHasAudio = n_audio != 0;

  *aInfo = mInfo;

  *aTags = nullptr;

  /* set the pipeline to PLAYING so that it starts decoding and queueing data in
   * the appsinks */
  gst_element_set_state(mPlayBin, GST_STATE_PLAYING);

  return NS_OK;
}

nsresult GStreamerReader::CheckSupportedFormats()
{
  bool done = false;
  bool unsupported = false;

  GstIterator *it = gst_bin_iterate_recurse(GST_BIN(mPlayBin));
  while (!done) {
    GstElement* element;
    GstIteratorResult res = gst_iterator_next(it, (void **)&element);
    switch(res) {
      case GST_ITERATOR_OK:
      {
        GstElementFactory* factory = gst_element_get_factory(element);
        if (factory) {
          const char* klass = gst_element_factory_get_klass(factory);
          GstPad* pad = gst_element_get_pad(element, "sink");
          if (pad) {
            GstCaps* caps = gst_pad_get_negotiated_caps(pad);

            if (caps) {
              /* check for demuxers but ignore elements like id3demux */
              if (strstr (klass, "Demuxer") && !strstr(klass, "Metadata"))
                unsupported = !GStreamerFormatHelper::Instance()->CanHandleContainerCaps(caps);
              else if (strstr (klass, "Decoder"))
                unsupported = !GStreamerFormatHelper::Instance()->CanHandleCodecCaps(caps);

              gst_caps_unref(caps);
            }
            gst_object_unref(pad);
          }
        }

        gst_object_unref(element);
        done = unsupported;
        break;
      }
      case GST_ITERATOR_RESYNC:
        unsupported = false;
        done = false;
        break;
      case GST_ITERATOR_ERROR:
        done = true;
        break;
      case GST_ITERATOR_DONE:
        done = true;
        break;
    }
  }

  return unsupported ? NS_ERROR_FAILURE : NS_OK;
}

nsresult GStreamerReader::ResetDecode()
{
  nsresult res = NS_OK;

  if (NS_FAILED(MediaDecoderReader::ResetDecode())) {
    res = NS_ERROR_FAILURE;
  }

  mVideoQueue.Reset();
  mAudioQueue.Reset();

  mVideoSinkBufferCount = 0;
  mAudioSinkBufferCount = 0;
  mReachedEos = false;
  mLastReportedByteOffset = 0;
  mByteOffset = 0;

  return res;
}

void GStreamerReader::NotifyBytesConsumed()
{
  NS_ASSERTION(mByteOffset >= mLastReportedByteOffset,
      "current byte offset less than prev offset");
  mDecoder->NotifyBytesConsumed(mByteOffset - mLastReportedByteOffset);
  mLastReportedByteOffset = mByteOffset;
}

bool GStreamerReader::WaitForDecodedData(int* aCounter)
{
  ReentrantMonitorAutoEnter mon(mGstThreadsMonitor);

  /* Report consumed bytes from here as we can't do it from gst threads */
  NotifyBytesConsumed();
  while(*aCounter == 0) {
    if (mReachedEos) {
      return false;
    }
    mon.Wait();
    NotifyBytesConsumed();
  }
  (*aCounter)--;

  return true;
}

bool GStreamerReader::DecodeAudioData()
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  if (!WaitForDecodedData(&mAudioSinkBufferCount)) {
    mAudioQueue.Finish();
    return false;
  }

  GstBuffer* buffer = gst_app_sink_pull_buffer(mAudioAppSink);
  int64_t timestamp = GST_BUFFER_TIMESTAMP(buffer);
  timestamp = gst_segment_to_stream_time(&mAudioSegment,
      GST_FORMAT_TIME, timestamp);
  timestamp = GST_TIME_AS_USECONDS(timestamp);
  int64_t duration = 0;
  if (GST_CLOCK_TIME_IS_VALID(GST_BUFFER_DURATION(buffer)))
    duration = GST_TIME_AS_USECONDS(GST_BUFFER_DURATION(buffer));

  int64_t offset = GST_BUFFER_OFFSET(buffer);
  unsigned int size = GST_BUFFER_SIZE(buffer);
  int32_t frames = (size / sizeof(AudioDataValue)) / mInfo.mAudioChannels;
  ssize_t outSize = static_cast<size_t>(size / sizeof(AudioDataValue));
  nsAutoArrayPtr<AudioDataValue> data(new AudioDataValue[outSize]);
  memcpy(data, GST_BUFFER_DATA(buffer), GST_BUFFER_SIZE(buffer));
  AudioData* audio = new AudioData(offset, timestamp, duration,
      frames, data.forget(), mInfo.mAudioChannels);

  mAudioQueue.Push(audio);
  gst_buffer_unref(buffer);

  return true;
}

bool GStreamerReader::DecodeVideoFrame(bool &aKeyFrameSkip,
                                         int64_t aTimeThreshold)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  GstBuffer* buffer = nullptr;
  int64_t timestamp, nextTimestamp;
  while (true)
  {
    if (!WaitForDecodedData(&mVideoSinkBufferCount)) {
      mVideoQueue.Finish();
      break;
    }
    mDecoder->NotifyDecodedFrames(0, 1);

    buffer = gst_app_sink_pull_buffer(mVideoAppSink);
    bool isKeyframe = !GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DISCONT);
    if ((aKeyFrameSkip && !isKeyframe)) {
      gst_buffer_unref(buffer);
      buffer = nullptr;
      continue;
    }

    timestamp = GST_BUFFER_TIMESTAMP(buffer);
    {
      ReentrantMonitorAutoEnter mon(mGstThreadsMonitor);
      timestamp = gst_segment_to_stream_time(&mVideoSegment,
          GST_FORMAT_TIME, timestamp);
    }
    NS_ASSERTION(GST_CLOCK_TIME_IS_VALID(timestamp),
        "frame has invalid timestamp");
    timestamp = nextTimestamp = GST_TIME_AS_USECONDS(timestamp);
    if (GST_CLOCK_TIME_IS_VALID(GST_BUFFER_DURATION(buffer)))
      nextTimestamp += GST_TIME_AS_USECONDS(GST_BUFFER_DURATION(buffer));
    else if (fpsNum && fpsDen)
      /* add 1-frame duration */
      nextTimestamp += gst_util_uint64_scale(GST_USECOND, fpsNum, fpsDen);

    if (timestamp < aTimeThreshold) {
      LOG(PR_LOG_DEBUG, ("skipping frame %" GST_TIME_FORMAT
            " threshold %" GST_TIME_FORMAT,
            GST_TIME_ARGS(timestamp), GST_TIME_ARGS(aTimeThreshold)));
      gst_buffer_unref(buffer);
      buffer = nullptr;
      continue;
    }

    break;
  }

  if (!buffer)
    /* no more frames */
    return false;

  nsRefPtr<PlanarYCbCrImage> image;
  GstMozVideoBufferData* bufferdata = reinterpret_cast<GstMozVideoBufferData*>
      GST_IS_MOZ_VIDEO_BUFFER(buffer)?gst_moz_video_buffer_get_data(GST_MOZ_VIDEO_BUFFER(buffer)):nullptr;

  if(bufferdata)
    image = bufferdata->mImage;

  if (!image) {
    /* Ugh, upstream is not calling gst_pad_alloc_buffer(). Fallback to
     * allocating a PlanarYCbCrImage backed GstBuffer here and memcpy.
     */
    GstBuffer* tmp = nullptr;
    AllocateVideoBufferFull(nullptr, GST_BUFFER_OFFSET(buffer),
        GST_BUFFER_SIZE(buffer), nullptr, &tmp, image);

    /* copy */
    gst_buffer_copy_metadata(tmp, buffer, (GstBufferCopyFlags)GST_BUFFER_COPY_ALL);
    memcpy(GST_BUFFER_DATA(tmp), GST_BUFFER_DATA(buffer),
        GST_BUFFER_SIZE(tmp));
    gst_buffer_unref(buffer);
    buffer = tmp;
  }

  guint8* data = GST_BUFFER_DATA(buffer);

  int width = mPicture.width;
  int height = mPicture.height;
  GstVideoFormat format = mFormat;

  VideoData::YCbCrBuffer b;
  for(int i = 0; i < 3; i++) {
    b.mPlanes[i].mData = data + gst_video_format_get_component_offset(format, i,
        width, height);
    b.mPlanes[i].mStride = gst_video_format_get_row_stride(format, i, width);
    b.mPlanes[i].mHeight = gst_video_format_get_component_height(format,
        i, height);
    b.mPlanes[i].mWidth = gst_video_format_get_component_width(format,
        i, width);
    b.mPlanes[i].mOffset = 0;
    b.mPlanes[i].mSkip = 0;
  }

  bool isKeyframe = !GST_BUFFER_FLAG_IS_SET(buffer,
      GST_BUFFER_FLAG_DELTA_UNIT);
  /* XXX ? */
  int64_t offset = 0;
  VideoData* video = VideoData::Create(mInfo, image, offset,
                                       timestamp, nextTimestamp, b,
                                       isKeyframe, -1, mPicture);
  mVideoQueue.Push(video);
  gst_buffer_unref(buffer);

  return true;
}

nsresult GStreamerReader::Seek(int64_t aTarget,
                                 int64_t aStartTime,
                                 int64_t aEndTime,
                                 int64_t aCurrentTime)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  gint64 seekPos = aTarget * GST_USECOND;
  LOG(PR_LOG_DEBUG, ("%p About to seek to %" GST_TIME_FORMAT,
        mDecoder, GST_TIME_ARGS(seekPos)));

  if (!gst_element_seek_simple(mPlayBin, GST_FORMAT_TIME,
    static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE), seekPos)) {
    LOG(PR_LOG_ERROR, ("seek failed"));
    return NS_ERROR_FAILURE;
  }
  LOG(PR_LOG_DEBUG, ("seek succeeded"));

  return DecodeToTarget(aTarget);
}

nsresult GStreamerReader::GetBuffered(TimeRanges* aBuffered,
                                      int64_t aStartTime)
{
  if (!mInfo.mHasVideo && !mInfo.mHasAudio) {
    return NS_OK;
  }

  GstFormat format = GST_FORMAT_TIME;
  MediaResource* resource = mDecoder->GetResource();
  nsTArray<MediaByteRange> ranges;
  resource->GetCachedRanges(ranges);

  if (mDecoder->OnStateMachineThread())
    /* Report the position from here while buffering as we can't report it from
     * the gstreamer threads that are actually reading from the resource
     */
    NotifyBytesConsumed();

  if (resource->IsDataCachedToEndOfResource(0)) {
    /* fast path for local or completely cached files */
    gint64 duration = 0;

    duration = QueryDuration();
    double end = (double) duration / GST_MSECOND;
    LOG(PR_LOG_DEBUG, ("complete range [0, %f] for [0, %li]",
          end, resource->GetLength()));
    aBuffered->Add(0, end);
    return NS_OK;
  }

  for(uint32_t index = 0; index < ranges.Length(); index++) {
    int64_t startOffset = ranges[index].mStart;
    int64_t endOffset = ranges[index].mEnd;
    gint64 startTime, endTime;

    if (!gst_element_query_convert(GST_ELEMENT(mPlayBin), GST_FORMAT_BYTES,
      startOffset, &format, &startTime) || format != GST_FORMAT_TIME)
      continue;
    if (!gst_element_query_convert(GST_ELEMENT(mPlayBin), GST_FORMAT_BYTES,
      endOffset, &format, &endTime) || format != GST_FORMAT_TIME)
      continue;

    double start = (double) GST_TIME_AS_USECONDS (startTime) / GST_MSECOND;
    double end = (double) GST_TIME_AS_USECONDS (endTime) / GST_MSECOND;
    LOG(PR_LOG_DEBUG, ("adding range [%f, %f] for [%li %li] size %li",
          start, end, startOffset, endOffset, resource->GetLength()));
    aBuffered->Add(start, end);
  }

  return NS_OK;
}

void GStreamerReader::ReadAndPushData(guint aLength)
{
  MediaResource* resource = mDecoder->GetResource();
  NS_ASSERTION(resource, "Decoder has no media resource");
  nsresult rv = NS_OK;

  GstBuffer* buffer = gst_buffer_new_and_alloc(aLength);
  guint8* data = GST_BUFFER_DATA(buffer);
  uint32_t size = 0, bytesRead = 0;
  while(bytesRead < aLength) {
    rv = resource->Read(reinterpret_cast<char*>(data + bytesRead),
        aLength - bytesRead, &size);
    if (NS_FAILED(rv) || size == 0)
      break;

    bytesRead += size;
  }

  GST_BUFFER_SIZE(buffer) = bytesRead;
  mByteOffset += bytesRead;

  GstFlowReturn ret = gst_app_src_push_buffer(mSource, gst_buffer_ref(buffer));
  if (ret != GST_FLOW_OK) {
    LOG(PR_LOG_ERROR, ("ReadAndPushData push ret %s", gst_flow_get_name(ret)));
  }

  if (GST_BUFFER_SIZE (buffer) < aLength) {
    /* If we read less than what we wanted, we reached the end */
    gst_app_src_end_of_stream(mSource);
  }

  gst_buffer_unref(buffer);
}

int64_t GStreamerReader::QueryDuration()
{
  gint64 duration = 0;
  GstFormat format = GST_FORMAT_TIME;

  if (gst_element_query_duration(GST_ELEMENT(mPlayBin),
      &format, &duration)) {
    if (format == GST_FORMAT_TIME) {
      LOG(PR_LOG_DEBUG, ("pipeline duration %" GST_TIME_FORMAT,
            GST_TIME_ARGS (duration)));
      duration = GST_TIME_AS_USECONDS (duration);
    }
  }

  {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    int64_t media_duration = mDecoder->GetMediaDuration();
    if (media_duration != -1 && media_duration > duration) {
      // We decoded more than the reported duration (which could be estimated)
      LOG(PR_LOG_DEBUG, ("decoded duration > estimated duration"));
      duration = media_duration;
    }
  }

  return duration;
}

void GStreamerReader::NeedDataCb(GstAppSrc* aSrc,
                                 guint aLength,
                                 gpointer aUserData)
{
  GStreamerReader* reader = reinterpret_cast<GStreamerReader*>(aUserData);
  reader->NeedData(aSrc, aLength);
}

void GStreamerReader::NeedData(GstAppSrc* aSrc, guint aLength)
{
  if (aLength == static_cast<guint>(-1))
    aLength = DEFAULT_SOURCE_READ_SIZE;
  ReadAndPushData(aLength);
}

void GStreamerReader::EnoughDataCb(GstAppSrc* aSrc, gpointer aUserData)
{
  GStreamerReader* reader = reinterpret_cast<GStreamerReader*>(aUserData);
  reader->EnoughData(aSrc);
}

void GStreamerReader::EnoughData(GstAppSrc* aSrc)
{
}

gboolean GStreamerReader::SeekDataCb(GstAppSrc* aSrc,
                                     guint64 aOffset,
                                     gpointer aUserData)
{
  GStreamerReader* reader = reinterpret_cast<GStreamerReader*>(aUserData);
  return reader->SeekData(aSrc, aOffset);
}

gboolean GStreamerReader::SeekData(GstAppSrc* aSrc, guint64 aOffset)
{
  ReentrantMonitorAutoEnter mon(mGstThreadsMonitor);
  MediaResource* resource = mDecoder->GetResource();
  int64_t resourceLength = resource->GetLength();

  if (gst_app_src_get_size(mSource) == -1) {
    /* It's possible that we didn't know the length when we initialized mSource
     * but maybe we do now
     */
    gst_app_src_set_size(mSource, resourceLength);
  }

  nsresult rv = NS_ERROR_FAILURE;
  if (aOffset < static_cast<guint64>(resourceLength)) {
    rv = resource->Seek(SEEK_SET, aOffset);
  }

  if (NS_SUCCEEDED(rv)) {
    mByteOffset = mLastReportedByteOffset = aOffset;
  } else {
    LOG(PR_LOG_ERROR, ("seek at %lu failed", aOffset));
  }

  return NS_SUCCEEDED(rv);
}

gboolean GStreamerReader::EventProbeCb(GstPad* aPad,
                                         GstEvent* aEvent,
                                         gpointer aUserData)
{
  GStreamerReader* reader = reinterpret_cast<GStreamerReader*>(aUserData);
  return reader->EventProbe(aPad, aEvent);
}

gboolean GStreamerReader::EventProbe(GstPad* aPad, GstEvent* aEvent)
{
  GstElement* parent = GST_ELEMENT(gst_pad_get_parent(aPad));
  switch(GST_EVENT_TYPE(aEvent)) {
    case GST_EVENT_NEWSEGMENT:
    {
      gboolean update;
      gdouble rate;
      GstFormat format;
      gint64 start, stop, position;
      GstSegment* segment;

      /* Store the segments so we can convert timestamps to stream time, which
       * is what the upper layers sync on.
       */
      ReentrantMonitorAutoEnter mon(mGstThreadsMonitor);
      gst_event_parse_new_segment(aEvent, &update, &rate, &format,
          &start, &stop, &position);
      if (parent == GST_ELEMENT(mVideoAppSink))
        segment = &mVideoSegment;
      else
        segment = &mAudioSegment;
      gst_segment_set_newsegment(segment, update, rate, format,
          start, stop, position);
      break;
    }
    case GST_EVENT_FLUSH_STOP:
      /* Reset on seeks */
      ResetDecode();
      break;
    default:
      break;
  }
  gst_object_unref(parent);

  return TRUE;
}

GstFlowReturn GStreamerReader::AllocateVideoBufferFull(GstPad* aPad,
                                                       guint64 aOffset,
                                                       guint aSize,
                                                       GstCaps* aCaps,
                                                       GstBuffer** aBuf,
                                                       nsRefPtr<PlanarYCbCrImage>& aImage)
{
  /* allocate an image using the container */
  ImageContainer* container = mDecoder->GetImageContainer();
  ImageFormat format = PLANAR_YCBCR;
  PlanarYCbCrImage* img = reinterpret_cast<PlanarYCbCrImage*>(container->CreateImage(&format, 1).get());
  nsRefPtr<PlanarYCbCrImage> image = dont_AddRef(img);

  /* prepare a GstBuffer pointing to the underlying PlanarYCbCrImage buffer */
  GstBuffer* buf = GST_BUFFER(gst_moz_video_buffer_new());
  GST_BUFFER_SIZE(buf) = aSize;
  /* allocate the actual YUV buffer */
  GST_BUFFER_DATA(buf) = image->AllocateAndGetNewBuffer(aSize);

  aImage = image;

  /* create a GstMozVideoBufferData to hold the image */
  GstMozVideoBufferData* bufferdata = new GstMozVideoBufferData(image);

  /* Attach bufferdata to our GstMozVideoBuffer, it will take care to free it */
  gst_moz_video_buffer_set_data(GST_MOZ_VIDEO_BUFFER(buf), bufferdata);

  *aBuf = buf;
  return GST_FLOW_OK;
}

GstFlowReturn GStreamerReader::AllocateVideoBufferCb(GstPad* aPad,
                                                     guint64 aOffset,
                                                     guint aSize,
                                                     GstCaps* aCaps,
                                                     GstBuffer** aBuf)
{
  GStreamerReader* reader = reinterpret_cast<GStreamerReader*>(gst_pad_get_element_private(aPad));
  return reader->AllocateVideoBuffer(aPad, aOffset, aSize, aCaps, aBuf);
}

GstFlowReturn GStreamerReader::AllocateVideoBuffer(GstPad* aPad,
                                                   guint64 aOffset,
                                                   guint aSize,
                                                   GstCaps* aCaps,
                                                   GstBuffer** aBuf)
{
  nsRefPtr<PlanarYCbCrImage> image;
  return AllocateVideoBufferFull(aPad, aOffset, aSize, aCaps, aBuf, image);
}

GstFlowReturn GStreamerReader::NewPrerollCb(GstAppSink* aSink,
                                              gpointer aUserData)
{
  GStreamerReader* reader = reinterpret_cast<GStreamerReader*>(aUserData);

  if (aSink == reader->mVideoAppSink)
    reader->VideoPreroll();
  else
    reader->AudioPreroll();
  return GST_FLOW_OK;
}

void GStreamerReader::AudioPreroll()
{
  /* The first audio buffer has reached the audio sink. Get rate and channels */
  LOG(PR_LOG_DEBUG, ("Audio preroll"));
  GstPad* sinkpad = gst_element_get_pad(GST_ELEMENT(mAudioAppSink), "sink");
  GstCaps* caps = gst_pad_get_negotiated_caps(sinkpad);
  GstStructure* s = gst_caps_get_structure(caps, 0);
  mInfo.mAudioRate = mInfo.mAudioChannels = 0;
  gst_structure_get_int(s, "rate", (gint*) &mInfo.mAudioRate);
  gst_structure_get_int(s, "channels", (gint*) &mInfo.mAudioChannels);
  NS_ASSERTION(mInfo.mAudioRate != 0, ("audio rate is zero"));
  NS_ASSERTION(mInfo.mAudioChannels != 0, ("audio channels is zero"));
  NS_ASSERTION(mInfo.mAudioChannels > 0 && mInfo.mAudioChannels <= MAX_CHANNELS,
      "invalid audio channels number");
  mInfo.mHasAudio = true;
  gst_caps_unref(caps);
  gst_object_unref(sinkpad);
}

void GStreamerReader::VideoPreroll()
{
  /* The first video buffer has reached the video sink. Get width and height */
  LOG(PR_LOG_DEBUG, ("Video preroll"));
  GstPad* sinkpad = gst_element_get_pad(GST_ELEMENT(mVideoAppSink), "sink");
  GstCaps* caps = gst_pad_get_negotiated_caps(sinkpad);
  gst_video_format_parse_caps(caps, &mFormat, &mPicture.width, &mPicture.height);
  GstStructure* structure = gst_caps_get_structure(caps, 0);
  gst_structure_get_fraction(structure, "framerate", &fpsNum, &fpsDen);
  NS_ASSERTION(mPicture.width && mPicture.height, "invalid video resolution");
  mInfo.mDisplay = nsIntSize(mPicture.width, mPicture.height);
  mInfo.mHasVideo = true;
  gst_caps_unref(caps);
  gst_object_unref(sinkpad);
}

GstFlowReturn GStreamerReader::NewBufferCb(GstAppSink* aSink,
                                           gpointer aUserData)
{
  GStreamerReader* reader = reinterpret_cast<GStreamerReader*>(aUserData);

  if (aSink == reader->mVideoAppSink)
    reader->NewVideoBuffer();
  else
    reader->NewAudioBuffer();

  return GST_FLOW_OK;
}

void GStreamerReader::NewVideoBuffer()
{
  ReentrantMonitorAutoEnter mon(mGstThreadsMonitor);
  /* We have a new video buffer queued in the video sink. Increment the counter
   * and notify the decode thread potentially blocked in DecodeVideoFrame
   */
  mDecoder->NotifyDecodedFrames(1, 0);
  mVideoSinkBufferCount++;
  mon.NotifyAll();
}

void GStreamerReader::NewAudioBuffer()
{
  ReentrantMonitorAutoEnter mon(mGstThreadsMonitor);
  /* We have a new audio buffer queued in the audio sink. Increment the counter
   * and notify the decode thread potentially blocked in DecodeAudioData
   */
  mAudioSinkBufferCount++;
  mon.NotifyAll();
}

void GStreamerReader::EosCb(GstAppSink* aSink, gpointer aUserData)
{
  GStreamerReader* reader = reinterpret_cast<GStreamerReader*>(aUserData);
  reader->Eos(aSink);
}

void GStreamerReader::Eos(GstAppSink* aSink)
{
  /* We reached the end of the stream */
  {
    ReentrantMonitorAutoEnter mon(mGstThreadsMonitor);
    /* Potentially unblock DecodeVideoFrame and DecodeAudioData */
    mReachedEos = true;
    mon.NotifyAll();
  }

  {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    /* Potentially unblock the decode thread in ::DecodeLoop */
    mVideoQueue.Finish();
    mAudioQueue.Finish();
    mon.NotifyAll();
  }
}

} // namespace mozilla

