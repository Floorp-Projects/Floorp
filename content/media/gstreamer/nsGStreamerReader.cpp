/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsError.h"
#include "nsBuiltinDecoderStateMachine.h"
#include "nsBuiltinDecoder.h"
#include "MediaResource.h"
#include "nsGStreamerReader.h"
#include "VideoUtils.h"
#include "nsTimeRanges.h"
#include "mozilla/Preferences.h"

using namespace mozilla;
using namespace mozilla::layers;

// Un-comment to enable logging of seek bisections.
//#define SEEK_LOGGING

#ifdef PR_LOGGING
extern PRLogModuleInfo* gBuiltinDecoderLog;
#define LOG(type, msg) PR_LOG(gBuiltinDecoderLog, type, msg)
#else
#define LOG(type, msg)
#endif

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

nsGStreamerReader::nsGStreamerReader(nsBuiltinDecoder* aDecoder)
  : nsBuiltinDecoderReader(aDecoder),
  mPlayBin(NULL),
  mBus(NULL),
  mSource(NULL),
  mVideoSink(NULL),
  mVideoAppSink(NULL),
  mAudioSink(NULL),
  mAudioAppSink(NULL),
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
  MOZ_COUNT_CTOR(nsGStreamerReader);

  mSrcCallbacks.need_data = nsGStreamerReader::NeedDataCb;
  mSrcCallbacks.enough_data = nsGStreamerReader::EnoughDataCb;
  mSrcCallbacks.seek_data = nsGStreamerReader::SeekDataCb;

  mSinkCallbacks.eos = nsGStreamerReader::EosCb;
  mSinkCallbacks.new_preroll = nsGStreamerReader::NewPrerollCb;
  mSinkCallbacks.new_buffer = nsGStreamerReader::NewBufferCb;
  mSinkCallbacks.new_buffer_list = NULL;

  gst_segment_init(&mVideoSegment, GST_FORMAT_UNDEFINED);
  gst_segment_init(&mAudioSegment, GST_FORMAT_UNDEFINED);
}

nsGStreamerReader::~nsGStreamerReader()
{
  MOZ_COUNT_DTOR(nsGStreamerReader);
  ResetDecode();

  if (mPlayBin) {
    gst_app_src_end_of_stream(mSource);
    gst_element_set_state(mPlayBin, GST_STATE_NULL);
    gst_object_unref(mPlayBin);
    mPlayBin = NULL;
    mVideoSink = NULL;
    mVideoAppSink = NULL;
    mAudioSink = NULL;
    mAudioAppSink = NULL;
    gst_object_unref(mBus);
    mBus = NULL;
  }
}

nsresult nsGStreamerReader::Init(nsBuiltinDecoderReader* aCloneDonor)
{
  GError *error = NULL;
  if (!gst_init_check(0, 0, &error)) {
    LOG(PR_LOG_ERROR, ("gst initialization failed: %s", error->message));
    g_error_free(error);
    return NS_ERROR_FAILURE;
  }

  mPlayBin = gst_element_factory_make("playbin2", NULL);
  if (mPlayBin == NULL) {
    LOG(PR_LOG_ERROR, ("couldn't create playbin2"));
    return NS_ERROR_FAILURE;
  }
  g_object_set(mPlayBin, "buffer-size", 0, NULL);
  mBus = gst_pipeline_get_bus(GST_PIPELINE(mPlayBin));

  mVideoSink = gst_parse_bin_from_description("capsfilter name=filter ! "
      "appsink name=videosink sync=true max-buffers=1 "
      "caps=video/x-raw-yuv,format=(fourcc)I420"
      , TRUE, NULL);
  mVideoAppSink = GST_APP_SINK(gst_bin_get_by_name(GST_BIN(mVideoSink),
        "videosink"));
  gst_app_sink_set_callbacks(mVideoAppSink, &mSinkCallbacks,
      (gpointer) this, NULL);
  GstPad *sinkpad = gst_element_get_pad(GST_ELEMENT(mVideoAppSink), "sink");
  gst_pad_add_event_probe(sinkpad,
      G_CALLBACK(&nsGStreamerReader::EventProbeCb), this);
  gst_object_unref(sinkpad);

  mAudioSink = gst_parse_bin_from_description("capsfilter name=filter ! "
        "appsink name=audiosink sync=true caps=audio/x-raw-float,"
        "channels={1,2},rate=44100,width=32,endianness=1234", TRUE, NULL);
  mAudioAppSink = GST_APP_SINK(gst_bin_get_by_name(GST_BIN(mAudioSink),
        "audiosink"));
  gst_app_sink_set_callbacks(mAudioAppSink, &mSinkCallbacks,
      (gpointer) this, NULL);
  sinkpad = gst_element_get_pad(GST_ELEMENT(mAudioAppSink), "sink");
  gst_pad_add_event_probe(sinkpad,
      G_CALLBACK(&nsGStreamerReader::EventProbeCb), this);
  gst_object_unref(sinkpad);

  g_object_set(mPlayBin, "uri", "appsrc://",
      "video-sink", mVideoSink,
      "audio-sink", mAudioSink,
      NULL);

  g_object_connect(mPlayBin, "signal::source-setup",
      nsGStreamerReader::PlayBinSourceSetupCb, this, NULL);

  return NS_OK;
}

void nsGStreamerReader::PlayBinSourceSetupCb(GstElement *aPlayBin,
                                             GstElement *aSource,
                                             gpointer aUserData)
{
  nsGStreamerReader *reader = reinterpret_cast<nsGStreamerReader*>(aUserData);
  reader->PlayBinSourceSetup(GST_APP_SRC(aSource));
}

void nsGStreamerReader::PlayBinSourceSetup(GstAppSrc *aSource)
{
  mSource = GST_APP_SRC(aSource);
  gst_app_src_set_callbacks(mSource, &mSrcCallbacks, (gpointer) this, NULL);
  MediaResource* resource = mDecoder->GetResource();
  PRInt64 len = resource->GetLength();
  gst_app_src_set_size(mSource, len);
  if (resource->IsDataCachedToEndOfResource(0) ||
      (len != -1 && len <= SHORT_FILE_SIZE)) {
    /* let the demuxer work in pull mode for local files (or very short files)
     * so that we get optimal seeking accuracy/performance
     */
    LOG(PR_LOG_ERROR, ("configuring random access"));
    gst_app_src_set_stream_type(mSource, GST_APP_STREAM_TYPE_RANDOM_ACCESS);
  } else {
    /* make the demuxer work in push mode so that seeking is kept to a minimum
     */
    gst_app_src_set_stream_type(mSource, GST_APP_STREAM_TYPE_SEEKABLE);
  }
}

nsresult nsGStreamerReader::ReadMetadata(nsVideoInfo* aInfo)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  nsresult ret = NS_OK;

  /* We do 3 attempts here: decoding audio and video, decoding video only,
   * decoding audio only. This allows us to play streams that have one broken
   * stream but that are otherwise decodeable.
   */
  guint flags[3] = {GST_PLAY_FLAG_VIDEO|GST_PLAY_FLAG_AUDIO,
    ~GST_PLAY_FLAG_AUDIO, ~GST_PLAY_FLAG_VIDEO};
  guint default_flags, current_flags;
  g_object_get(mPlayBin, "flags", &default_flags, NULL);

  GstMessage *message = NULL;
  for (int i=0; i < G_N_ELEMENTS(flags); i++) {
    current_flags = default_flags & flags[i];
    g_object_set(G_OBJECT(mPlayBin), "flags", current_flags, NULL);

    /* reset filter caps to ANY */
    GstCaps *caps = gst_caps_new_any();
    GstElement *filter = gst_bin_get_by_name(GST_BIN(mAudioSink), "filter");
    g_object_set(filter, "caps", caps, NULL);
    gst_object_unref(filter);

    filter = gst_bin_get_by_name(GST_BIN(mVideoSink), "filter");
    g_object_set(filter, "caps", caps, NULL);
    gst_object_unref(filter);
    gst_caps_unref(caps);
    filter = NULL;

    if (!(current_flags & GST_PLAY_FLAG_AUDIO))
      filter = gst_bin_get_by_name(GST_BIN(mAudioSink), "filter");
    else if (!(current_flags & GST_PLAY_FLAG_VIDEO))
      filter = gst_bin_get_by_name(GST_BIN(mVideoSink), "filter");

    if (filter) {
      /* Little trick: set the target caps to "skip" so that playbin2 fails to
       * find a decoder for the stream we want to skip.
       */
      GstCaps *filterCaps = gst_caps_new_simple ("skip", NULL);
      g_object_set(filter, "caps", filterCaps, NULL);
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
      GError *error;
      gchar *debug;

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
    mDecoder->GetStateMachine()->SetDuration(duration);
  }

  int n_video = 0, n_audio = 0;
  g_object_get(mPlayBin, "n-video", &n_video, "n-audio", &n_audio, NULL);
  mInfo.mHasVideo = n_video != 0;
  mInfo.mHasAudio = n_audio != 0;

  *aInfo = mInfo;

  /* set the pipeline to PLAYING so that it starts decoding and queueing data in
   * the appsinks */
  gst_element_set_state(mPlayBin, GST_STATE_PLAYING);

  return NS_OK;
}

nsresult nsGStreamerReader::ResetDecode()
{
  nsresult res = NS_OK;

  if (NS_FAILED(nsBuiltinDecoderReader::ResetDecode())) {
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

void nsGStreamerReader::NotifyBytesConsumed()
{
  NS_ASSERTION(mByteOffset >= mLastReportedByteOffset,
      "current byte offset less than prev offset");
  mDecoder->NotifyBytesConsumed(mByteOffset - mLastReportedByteOffset);
  mLastReportedByteOffset = mByteOffset;
}

bool nsGStreamerReader::WaitForDecodedData(int *aCounter)
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

bool nsGStreamerReader::DecodeAudioData()
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  if (!WaitForDecodedData(&mAudioSinkBufferCount)) {
    mAudioQueue.Finish();
    return false;
  }

  GstBuffer *buffer = gst_app_sink_pull_buffer(mAudioAppSink);
  PRInt64 timestamp = GST_BUFFER_TIMESTAMP(buffer);
  timestamp = gst_segment_to_stream_time(&mAudioSegment,
      GST_FORMAT_TIME, timestamp);
  timestamp = GST_TIME_AS_USECONDS(timestamp);
  PRInt64 duration = 0;
  if (GST_CLOCK_TIME_IS_VALID(GST_BUFFER_DURATION(buffer)))
    duration = GST_TIME_AS_USECONDS(GST_BUFFER_DURATION(buffer));

  PRInt64 offset = GST_BUFFER_OFFSET(buffer);
  unsigned int size = GST_BUFFER_SIZE(buffer);
  PRInt32 frames = (size / sizeof(AudioDataValue)) / mInfo.mAudioChannels;
  ssize_t outSize = static_cast<size_t>(size / sizeof(AudioDataValue));
  nsAutoArrayPtr<AudioDataValue> data(new AudioDataValue[outSize]);
  memcpy(data, GST_BUFFER_DATA(buffer), GST_BUFFER_SIZE(buffer));
  AudioData *audio = new AudioData(offset, timestamp, duration,
      frames, data.forget(), mInfo.mAudioChannels);

  mAudioQueue.Push(audio);
  gst_buffer_unref(buffer);

  if (mAudioQueue.GetSize() < 2) {
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(mDecoder, &nsBuiltinDecoder::NextFrameAvailable);
    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
  }

  return true;
}

bool nsGStreamerReader::DecodeVideoFrame(bool &aKeyFrameSkip,
                                         PRInt64 aTimeThreshold)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  GstBuffer *buffer = NULL;
  PRInt64 timestamp, nextTimestamp;
  while (true)
  {
    if (!WaitForDecodedData(&mVideoSinkBufferCount)) {
      mVideoQueue.Finish();
      break;
    }
    mDecoder->GetFrameStatistics().NotifyDecodedFrames(0, 1);

    buffer = gst_app_sink_pull_buffer(mVideoAppSink);
    bool isKeyframe = !GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DISCONT);
    if ((aKeyFrameSkip && !isKeyframe)) {
      gst_buffer_unref(buffer);
      buffer = NULL;
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
      buffer = NULL;
      continue;
    }

    break;
  }

  if (buffer == NULL)
    /* no more frames */
    return false;

  guint8 *data = GST_BUFFER_DATA(buffer);

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
  PRInt64 offset = 0;
  VideoData *video = VideoData::Create(mInfo,
                                       mDecoder->GetImageContainer(),
                                       offset,
                                       timestamp,
                                       nextTimestamp,
                                       b,
                                       isKeyframe,
                                       -1,
                                       mPicture);
  mVideoQueue.Push(video);
  gst_buffer_unref(buffer);

  if (mVideoQueue.GetSize() < 2) {
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(mDecoder, &nsBuiltinDecoder::NextFrameAvailable);
    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
  }

  return true;
}

nsresult nsGStreamerReader::Seek(PRInt64 aTarget,
                                 PRInt64 aStartTime,
                                 PRInt64 aEndTime,
                                 PRInt64 aCurrentTime)
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

nsresult nsGStreamerReader::GetBuffered(nsTimeRanges* aBuffered,
                                        PRInt64 aStartTime)
{
  GstFormat format = GST_FORMAT_TIME;
  MediaResource* resource = mDecoder->GetResource();
  gint64 resourceLength = resource->GetLength();
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
    GstFormat format = GST_FORMAT_TIME;

    duration = QueryDuration();
    double end = (double) duration / GST_MSECOND;
    LOG(PR_LOG_DEBUG, ("complete range [0, %f] for [0, %li]",
          end, resourceLength));
    aBuffered->Add(0, end);
    return NS_OK;
  }

  for(PRUint32 index = 0; index < ranges.Length(); index++) {
    PRInt64 startOffset = ranges[index].mStart;
    PRInt64 endOffset = ranges[index].mEnd;
    gint64 startTime, endTime;

    if (!gst_element_query_convert(GST_ELEMENT(mPlayBin), GST_FORMAT_BYTES,
      startOffset, &format, &startTime) || format != GST_FORMAT_TIME)
      continue;
    if (!gst_element_query_convert(GST_ELEMENT(mPlayBin), GST_FORMAT_BYTES,
      endOffset, &format, &endTime) || format != GST_FORMAT_TIME)
      continue;

    double start = start = (double) GST_TIME_AS_USECONDS (startTime) / GST_MSECOND;
    double end = (double) GST_TIME_AS_USECONDS (endTime) / GST_MSECOND;
    LOG(PR_LOG_DEBUG, ("adding range [%f, %f] for [%li %li] size %li",
          start, end, startOffset, endOffset, resourceLength));
    aBuffered->Add(start, end);
  }

  return NS_OK;
}

void nsGStreamerReader::ReadAndPushData(guint aLength)
{
  MediaResource* resource = mDecoder->GetResource();
  NS_ASSERTION(resource, "Decoder has no media resource");
  nsresult rv = NS_OK;

  GstBuffer *buffer = gst_buffer_new_and_alloc(aLength);
  guint8 *data = GST_BUFFER_DATA(buffer);
  PRUint32 size = 0, bytesRead = 0;
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
  if (ret != GST_FLOW_OK)
    LOG(PR_LOG_ERROR, ("ReadAndPushData push ret %s", gst_flow_get_name(ret)));

  if (GST_BUFFER_SIZE (buffer) < aLength)
    /* If we read less than what we wanted, we reached the end */
    gst_app_src_end_of_stream(mSource);

  gst_buffer_unref(buffer);
}

PRInt64 nsGStreamerReader::QueryDuration()
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

  if (mDecoder->mDuration != -1 &&
      mDecoder->mDuration > duration) {
    /* We decoded more than the reported duration (which could be estimated) */
    LOG(PR_LOG_DEBUG, ("mDuration > duration"));
    duration = mDecoder->mDuration;
  }

  return duration;
}

void nsGStreamerReader::NeedDataCb(GstAppSrc *aSrc,
                                   guint aLength,
                                   gpointer aUserData)
{
  nsGStreamerReader *reader = (nsGStreamerReader *) aUserData;
  reader->NeedData(aSrc, aLength);
}

void nsGStreamerReader::NeedData(GstAppSrc *aSrc, guint aLength)
{
  if (aLength == -1)
    aLength = DEFAULT_SOURCE_READ_SIZE;
  ReadAndPushData(aLength);
}

void nsGStreamerReader::EnoughDataCb(GstAppSrc *aSrc, gpointer aUserData)
{
  nsGStreamerReader *reader = (nsGStreamerReader *) aUserData;
  reader->EnoughData(aSrc);
}

void nsGStreamerReader::EnoughData(GstAppSrc *aSrc)
{
}

gboolean nsGStreamerReader::SeekDataCb(GstAppSrc *aSrc,
                                       guint64 aOffset,
                                       gpointer aUserData)
{
  nsGStreamerReader *reader = (nsGStreamerReader *) aUserData;
  return reader->SeekData(aSrc, aOffset);
}

gboolean nsGStreamerReader::SeekData(GstAppSrc *aSrc, guint64 aOffset)
{
  ReentrantMonitorAutoEnter mon(mGstThreadsMonitor);
  MediaResource* resource = mDecoder->GetResource();

  if (gst_app_src_get_size(mSource) == -1)
    /* It's possible that we didn't know the length when we initialized mSource
     * but maybe we do now
     */
    gst_app_src_set_size(mSource, resource->GetLength());

  nsresult rv = NS_ERROR_FAILURE;
  if (aOffset < resource->GetLength())
    rv = resource->Seek(SEEK_SET, aOffset);

  if (NS_SUCCEEDED(rv))
    mByteOffset = mLastReportedByteOffset = aOffset;
  else
    LOG(PR_LOG_ERROR, ("seek at %lu failed", aOffset));

  return NS_SUCCEEDED(rv);
}

gboolean nsGStreamerReader::EventProbeCb(GstPad *aPad,
                                         GstEvent *aEvent,
                                         gpointer aUserData)
{
  nsGStreamerReader *reader = (nsGStreamerReader *) aUserData;
  return reader->EventProbe(aPad, aEvent);
}

gboolean nsGStreamerReader::EventProbe(GstPad *aPad, GstEvent *aEvent)
{
  GstElement *parent = GST_ELEMENT(gst_pad_get_parent(aPad));
  switch(GST_EVENT_TYPE(aEvent)) {
    case GST_EVENT_NEWSEGMENT:
    {
      gboolean update;
      gdouble rate;
      GstFormat format;
      gint64 start, stop, position;
      GstSegment *segment;

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

GstFlowReturn nsGStreamerReader::NewPrerollCb(GstAppSink *aSink,
                                              gpointer aUserData)
{
  nsGStreamerReader *reader = (nsGStreamerReader *) aUserData;

  if (aSink == reader->mVideoAppSink)
    reader->VideoPreroll();
  else
    reader->AudioPreroll();
  return GST_FLOW_OK;
}

void nsGStreamerReader::AudioPreroll()
{
  /* The first audio buffer has reached the audio sink. Get rate and channels */
  LOG(PR_LOG_DEBUG, ("Audio preroll"));
  GstPad *sinkpad = gst_element_get_pad(GST_ELEMENT(mAudioAppSink), "sink");
  GstCaps *caps = gst_pad_get_negotiated_caps(sinkpad);
  GstStructure *s = gst_caps_get_structure(caps, 0);
  mInfo.mAudioRate = mInfo.mAudioChannels = 0;
  gst_structure_get_int(s, "rate", (gint *) &mInfo.mAudioRate);
  gst_structure_get_int(s, "channels", (gint *) &mInfo.mAudioChannels);
  NS_ASSERTION(mInfo.mAudioRate != 0, ("audio rate is zero"));
  NS_ASSERTION(mInfo.mAudioChannels != 0, ("audio channels is zero"));
  NS_ASSERTION(mInfo.mAudioChannels > 0 && mInfo.mAudioChannels <= MAX_CHANNELS,
      "invalid audio channels number");
  mInfo.mHasAudio = true;
  gst_caps_unref(caps);
  gst_object_unref(sinkpad);
}

void nsGStreamerReader::VideoPreroll()
{
  /* The first video buffer has reached the video sink. Get width and height */
  LOG(PR_LOG_DEBUG, ("Video preroll"));
  GstPad *sinkpad = gst_element_get_pad(GST_ELEMENT(mVideoAppSink), "sink");
  GstCaps *caps = gst_pad_get_negotiated_caps(sinkpad);
  gst_video_format_parse_caps(caps, &mFormat, &mPicture.width, &mPicture.height);
  GstStructure *structure = gst_caps_get_structure(caps, 0);
  gst_structure_get_fraction(structure, "framerate", &fpsNum, &fpsDen);
  NS_ASSERTION(mPicture.width && mPicture.height, "invalid video resolution");
  mInfo.mDisplay = nsIntSize(mPicture.width, mPicture.height);
  mInfo.mHasVideo = true;
  gst_caps_unref(caps);
  gst_object_unref(sinkpad);
}

GstFlowReturn nsGStreamerReader::NewBufferCb(GstAppSink *aSink,
                                             gpointer aUserData)
{
  nsGStreamerReader *reader = (nsGStreamerReader *) aUserData;

  if (aSink == reader->mVideoAppSink)
    reader->NewVideoBuffer();
  else
    reader->NewAudioBuffer();

  return GST_FLOW_OK;
}

void nsGStreamerReader::NewVideoBuffer()
{
  ReentrantMonitorAutoEnter mon(mGstThreadsMonitor);
  /* We have a new video buffer queued in the video sink. Increment the counter
   * and notify the decode thread potentially blocked in DecodeVideoFrame
   */
  mDecoder->GetFrameStatistics().NotifyDecodedFrames(1, 0);
  mVideoSinkBufferCount++;
  mon.NotifyAll();
}

void nsGStreamerReader::NewAudioBuffer()
{
  ReentrantMonitorAutoEnter mon(mGstThreadsMonitor);
  /* We have a new audio buffer queued in the audio sink. Increment the counter
   * and notify the decode thread potentially blocked in DecodeAudioData
   */
  mAudioSinkBufferCount++;
  mon.NotifyAll();
}

void nsGStreamerReader::EosCb(GstAppSink *aSink, gpointer aUserData)
{
  nsGStreamerReader *reader = (nsGStreamerReader *) aUserData;
  reader->Eos(aSink);
}

void nsGStreamerReader::Eos(GstAppSink *aSink)
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
