/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsError.h"
#include "nsMimeTypes.h"
#include "MediaDecoderStateMachine.h"
#include "AbstractMediaDecoder.h"
#include "GStreamerReader.h"
#if GST_VERSION_MAJOR >= 1
#include "GStreamerAllocator.h"
#endif
#include "GStreamerFormatHelper.h"
#include "VideoUtils.h"
#include "mozilla/Endian.h"
#include "mozilla/Preferences.h"
#include "mozilla/unused.h"
#include "GStreamerLoader.h"
#include "gfx2DGlue.h"

namespace mozilla {

using namespace gfx;
using namespace layers;
using namespace media;

// Un-comment to enable logging of seek bisections.
//#define SEEK_LOGGING

extern PRLogModuleInfo* gMediaDecoderLog;
#define LOG(type, msg, ...) \
  MOZ_LOG(gMediaDecoderLog, type, ("GStreamerReader(%p) " msg, this, ##__VA_ARGS__))

#if DEBUG
static const unsigned int MAX_CHANNELS = 4;
#endif
// Let the demuxer work in pull mode for short files. This used to be a micro
// optimization to have more accurate durations for ogg files in mochitests.
// Since as of today we aren't using gstreamer to demux ogg, and having demuxers
// work in pull mode over http makes them slower (since they really assume
// near-zero latency in pull mode) set the constant to 0 for now, which
// effectively disables it.
static const int SHORT_FILE_SIZE = 0;
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
  mMP3FrameParser(aDecoder->GetResource()->GetLength()),
  mDataOffset(0),
  mUseParserDuration(false),
  mLastParserDuration(-1),
#if GST_VERSION_MAJOR >= 1
  mAllocator(nullptr),
  mBufferPool(nullptr),
#endif
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
  mReachedAudioEos(false),
  mReachedVideoEos(false),
#if GST_VERSION_MAJOR >= 1
  mConfigureAlignment(true),
#endif
  fpsNum(0),
  fpsDen(0),
  mResource(aDecoder->GetResource())
{
  MOZ_COUNT_CTOR(GStreamerReader);

  mSrcCallbacks.need_data = GStreamerReader::NeedDataCb;
  mSrcCallbacks.enough_data = GStreamerReader::EnoughDataCb;
  mSrcCallbacks.seek_data = GStreamerReader::SeekDataCb;

  mSinkCallbacks.eos = GStreamerReader::EosCb;
  mSinkCallbacks.new_preroll = GStreamerReader::NewPrerollCb;
#if GST_VERSION_MAJOR >= 1
  mSinkCallbacks.new_sample = GStreamerReader::NewBufferCb;
#else
  mSinkCallbacks.new_buffer = GStreamerReader::NewBufferCb;
  mSinkCallbacks.new_buffer_list = nullptr;
#endif

  gst_segment_init(&mVideoSegment, GST_FORMAT_UNDEFINED);
  gst_segment_init(&mAudioSegment, GST_FORMAT_UNDEFINED);
}

GStreamerReader::~GStreamerReader()
{
  MOZ_COUNT_DTOR(GStreamerReader);
  NS_ASSERTION(!mPlayBin, "No Shutdown() after Init()");
}

nsresult GStreamerReader::Init()
{
  GStreamerFormatHelper::Instance();

#if GST_VERSION_MAJOR >= 1
  mAllocator = static_cast<GstAllocator*>(g_object_new(GST_TYPE_MOZ_GFX_MEMORY_ALLOCATOR, nullptr));
  moz_gfx_memory_allocator_set_reader(mAllocator, this);

  mBufferPool = static_cast<GstBufferPool*>(g_object_new(GST_TYPE_MOZ_GFX_BUFFER_POOL, nullptr));
#endif

#if GST_VERSION_MAJOR >= 1
  mPlayBin = gst_element_factory_make("playbin", nullptr);
#else
  mPlayBin = gst_element_factory_make("playbin2", nullptr);
#endif
  if (!mPlayBin) {
    LOG(LogLevel::Error, "couldn't create playbin");
    return NS_ERROR_FAILURE;
  }
  g_object_set(mPlayBin, "buffer-size", 0, nullptr);
  mBus = gst_pipeline_get_bus(GST_PIPELINE(mPlayBin));

  mVideoSink = gst_parse_bin_from_description("capsfilter name=filter ! "
      "appsink name=videosink sync=false max-buffers=1 "
#if GST_VERSION_MAJOR >= 1
      "caps=video/x-raw,format=I420"
#else
      "caps=video/x-raw-yuv,format=(fourcc)I420"
#endif
      , TRUE, nullptr);
  mVideoAppSink = GST_APP_SINK(gst_bin_get_by_name(GST_BIN(mVideoSink),
        "videosink"));
  mAudioSink = gst_parse_bin_from_description("capsfilter name=filter ! "
        "appsink name=audiosink sync=false max-buffers=1", TRUE, nullptr);
  mAudioAppSink = GST_APP_SINK(gst_bin_get_by_name(GST_BIN(mAudioSink),
                                                   "audiosink"));
  GstCaps* caps = BuildAudioSinkCaps();
  g_object_set(mAudioAppSink, "caps", caps, nullptr);
  gst_caps_unref(caps);

  gst_app_sink_set_callbacks(mVideoAppSink, &mSinkCallbacks,
      (gpointer) this, nullptr);
  gst_app_sink_set_callbacks(mAudioAppSink, &mSinkCallbacks,
                             (gpointer) this, nullptr);
  InstallPadCallbacks();

  g_object_set(mPlayBin, "uri", "appsrc://",
               "video-sink", mVideoSink,
               "audio-sink", mAudioSink,
               nullptr);

  g_signal_connect(G_OBJECT(mPlayBin), "notify::source",
                   G_CALLBACK(GStreamerReader::PlayBinSourceSetupCb), this);
  g_signal_connect(G_OBJECT(mPlayBin), "element-added",
                   G_CALLBACK(GStreamerReader::PlayElementAddedCb), this);

  g_signal_connect(G_OBJECT(mPlayBin), "element-added",
                   G_CALLBACK(GStreamerReader::ElementAddedCb), this);

  return NS_OK;
}

nsRefPtr<ShutdownPromise>
GStreamerReader::Shutdown()
{
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
#if GST_VERSION_MAJOR >= 1
    g_object_unref(mAllocator);
    g_object_unref(mBufferPool);
#endif
  }

  return MediaDecoderReader::Shutdown();
}

GstBusSyncReply
GStreamerReader::ErrorCb(GstBus *aBus, GstMessage *aMessage, gpointer aUserData)
{
  return static_cast<GStreamerReader*>(aUserData)->Error(aBus, aMessage);
}

GstBusSyncReply
GStreamerReader::Error(GstBus *aBus, GstMessage *aMessage)
{
  if (GST_MESSAGE_TYPE(aMessage) == GST_MESSAGE_ERROR) {
    Eos();
  }

  return GST_BUS_PASS;
}

void GStreamerReader::ElementAddedCb(GstBin *aPlayBin,
                                     GstElement *aElement,
                                     gpointer aUserData)
{
  GstElementFactory *factory = gst_element_get_factory(aElement);

  if (!factory)
    return;

  const gchar *name = gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(factory));

  if (name && !strcmp(name, "uridecodebin")) {
    g_signal_connect(G_OBJECT(aElement), "autoplug-sort",
                     G_CALLBACK(GStreamerReader::ElementFilterCb), aUserData);
  }
}

GValueArray *GStreamerReader::ElementFilterCb(GstURIDecodeBin *aBin,
                                              GstPad *aPad,
                                              GstCaps *aCaps,
                                              GValueArray *aFactories,
                                              gpointer aUserData)
{
  return ((GStreamerReader*)aUserData)->ElementFilter(aBin, aPad, aCaps, aFactories);
}

GValueArray *GStreamerReader::ElementFilter(GstURIDecodeBin *aBin,
                                            GstPad *aPad,
                                            GstCaps *aCaps,
                                            GValueArray *aFactories)
{
  GValueArray *filtered = g_value_array_new(aFactories->n_values);

  for (unsigned int i = 0; i < aFactories->n_values; i++) {
    GValue *value = &aFactories->values[i];
    GstPluginFeature *factory = GST_PLUGIN_FEATURE(g_value_peek_pointer(value));

    if (!GStreamerFormatHelper::IsPluginFeatureBlacklisted(factory)) {
      g_value_array_append(filtered, value);
    }
  }

  return filtered;
}

void GStreamerReader::PlayBinSourceSetupCb(GstElement* aPlayBin,
                                           GParamSpec* pspec,
                                           gpointer aUserData)
{
  GstElement *source;
  GStreamerReader* reader = reinterpret_cast<GStreamerReader*>(aUserData);

  g_object_get(aPlayBin, "source", &source, nullptr);
  reader->PlayBinSourceSetup(GST_APP_SRC(source));
}

void GStreamerReader::PlayBinSourceSetup(GstAppSrc* aSource)
{
  mSource = GST_APP_SRC(aSource);
  gst_app_src_set_callbacks(mSource, &mSrcCallbacks, (gpointer) this, nullptr);

  /* do a short read to trigger a network request so that GetLength() below
   * returns something meaningful and not -1
   */
  char buf[512];
  unsigned int size = 0;
  mResource.Read(buf, sizeof(buf), &size);
  mResource.Seek(SEEK_SET, 0);

  /* now we should have a length */
  int64_t resourceLength = GetDataLength();
  gst_app_src_set_size(mSource, resourceLength);
  if (mResource.GetResource()->IsDataCachedToEndOfResource(0) ||
      (resourceLength != -1 && resourceLength <= SHORT_FILE_SIZE)) {
    /* let the demuxer work in pull mode for local files (or very short files)
     * so that we get optimal seeking accuracy/performance
     */
    LOG(LogLevel::Debug, "configuring random access, len %lld", resourceLength);
    gst_app_src_set_stream_type(mSource, GST_APP_STREAM_TYPE_RANDOM_ACCESS);
  } else {
    /* make the demuxer work in push mode so that seeking is kept to a minimum
     */
    LOG(LogLevel::Debug, "configuring push mode, len %lld", resourceLength);
    gst_app_src_set_stream_type(mSource, GST_APP_STREAM_TYPE_SEEKABLE);
  }

  // Set the source MIME type to stop typefind trying every. single. format.
  GstCaps *caps =
    GStreamerFormatHelper::ConvertFormatsToCaps(mDecoder->GetResource()->GetContentType().get(),
                                                nullptr);

  gst_app_src_set_caps(aSource, caps);
  gst_caps_unref(caps);
}

/**
 * If this stream is an MP3, we want to parse the headers to estimate the
 * stream duration.
 */
nsresult GStreamerReader::ParseMP3Headers()
{
  const uint32_t MAX_READ_BYTES = 4096;

  uint64_t offset = 0;
  char bytes[MAX_READ_BYTES];
  uint32_t bytesRead;
  do {
    nsresult rv = mResource.ReadAt(offset, bytes, MAX_READ_BYTES, &bytesRead);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(bytesRead, NS_ERROR_FAILURE);

    mMP3FrameParser.Parse(reinterpret_cast<uint8_t*>(bytes), bytesRead, offset);
    offset += bytesRead;
  } while (!mMP3FrameParser.ParsedHeaders());

  if (mMP3FrameParser.IsMP3()) {
    mLastParserDuration = mMP3FrameParser.GetDuration();
    mDataOffset = mMP3FrameParser.GetMP3Offset();

    // Update GStreamer's stream length in case we found any ID3 headers to
    // ignore.
    gst_app_src_set_size(mSource, GetDataLength());
  }

  return NS_OK;
}

int64_t
GStreamerReader::GetDataLength()
{
  int64_t streamLen = mResource.GetLength();

  if (streamLen < 0) {
    return streamLen;
  }

  return streamLen - mDataOffset;
}

nsresult GStreamerReader::ReadMetadata(MediaInfo* aInfo,
                                       MetadataTags** aTags)
{
  MOZ_ASSERT(OnTaskQueue());
  nsresult ret = NS_OK;

  /*
   * Parse MP3 headers before we kick off the GStreamer pipeline otherwise there
   * might be concurrent stream operations happening on both decoding and gstreamer
   * threads which will screw the GStreamer state machine.
   */
  LOG(LogLevel::Debug, "content-type: %s %s",
      mDecoder->GetResource()->GetContentType().get(),
      mDecoder->GetResource()->GetContentURL().get());
  bool isMP3 = mDecoder->GetResource()->GetContentType().EqualsASCII(AUDIO_MP3);
  if (isMP3) {
    ParseMP3Headers();
  }


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
      GstCaps* filterCaps = gst_caps_new_simple ("skip", nullptr, nullptr);
      g_object_set(filter, "caps", filterCaps, nullptr);
      gst_caps_unref(filterCaps);
      gst_object_unref(filter);
    }

    LOG(LogLevel::Debug, "starting metadata pipeline");
    if (gst_element_set_state(mPlayBin, GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE) {
      LOG(LogLevel::Debug, "metadata pipeline state change failed");
      ret = NS_ERROR_FAILURE;
      continue;
    }

    /* Wait for ASYNC_DONE, which is emitted when the pipeline is built,
     * prerolled and ready to play. Also watch for errors.
     */
    message = gst_bus_timed_pop_filtered(mBus, GST_CLOCK_TIME_NONE,
                 (GstMessageType)(GST_MESSAGE_ASYNC_DONE | GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ASYNC_DONE) {
      LOG(LogLevel::Debug, "read metadata pipeline prerolled");
      gst_message_unref(message);
      ret = NS_OK;
      break;
    } else {
      LOG(LogLevel::Debug, "read metadata pipeline failed to preroll: %s",
            gst_message_type_get_name (GST_MESSAGE_TYPE (message)));

      if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR) {
        GError* error;
        gchar* debug;
        gst_message_parse_error(message, &error, &debug);
        LOG(LogLevel::Error, "read metadata error: %s: %s", error->message, debug);
        g_error_free(error);
        g_free(debug);
      }
      /* Unexpected stream close/EOS or other error. We'll give up if all
       * streams are in error/eos. */
      gst_element_set_state(mPlayBin, GST_STATE_NULL);
      gst_message_unref(message);
      ret = NS_ERROR_FAILURE;
    }
  }

  if (NS_SUCCEEDED(ret))
    ret = CheckSupportedFormats();

  if (NS_FAILED(ret))
    /* we couldn't get this to play */
    return ret;

  /* report the duration */
  gint64 duration;

  if (isMP3 && mMP3FrameParser.IsMP3()) {
    // The MP3FrameParser has reported a duration; use that over the gstreamer
    // reported duration for inter-platform consistency.
    mUseParserDuration = true;
    mLastParserDuration = mMP3FrameParser.GetDuration();
    mInfo.mMetadataDuration.emplace(TimeUnit::FromMicroseconds(mLastParserDuration));
  } else {
    LOG(LogLevel::Debug, "querying duration");
    // Otherwise use the gstreamer duration.
#if GST_VERSION_MAJOR >= 1
    if (gst_element_query_duration(GST_ELEMENT(mPlayBin),
          GST_FORMAT_TIME, &duration)) {
#else
    GstFormat format = GST_FORMAT_TIME;
    if (gst_element_query_duration(GST_ELEMENT(mPlayBin),
      &format, &duration) && format == GST_FORMAT_TIME) {
#endif
      LOG(LogLevel::Debug, "have duration %" GST_TIME_FORMAT, GST_TIME_ARGS(duration));
      duration = GST_TIME_AS_USECONDS (duration);
      mInfo.mMetadataDuration.emplace(TimeUnit::FromMicroseconds(duration));
    }
  }

  int n_video = 0, n_audio = 0;
  g_object_get(mPlayBin, "n-video", &n_video, "n-audio", &n_audio, nullptr);

  if (!n_video) {
    mInfo.mVideo = VideoInfo();
  }
  if (!n_audio) {
    mInfo.mAudio = AudioInfo();
  }
  *aInfo = mInfo;

  *aTags = nullptr;

  // Watch the pipeline for fatal errors
#if GST_VERSION_MAJOR >= 1
  gst_bus_set_sync_handler(mBus, GStreamerReader::ErrorCb, this, nullptr);
#else
  gst_bus_set_sync_handler(mBus, GStreamerReader::ErrorCb, this);
#endif

  /* set the pipeline to PLAYING so that it starts decoding and queueing data in
   * the appsinks */
  gst_element_set_state(mPlayBin, GST_STATE_PLAYING);

  return NS_OK;
}

bool
GStreamerReader::IsMediaSeekable()
{
  if (mUseParserDuration) {
    return true;
  }

  gint64 duration;
#if GST_VERSION_MAJOR >= 1
  if (gst_element_query_duration(GST_ELEMENT(mPlayBin), GST_FORMAT_TIME,
                                 &duration)) {
#else
  GstFormat format = GST_FORMAT_TIME;
  if (gst_element_query_duration(GST_ELEMENT(mPlayBin), &format, &duration) &&
      format == GST_FORMAT_TIME) {
#endif
    return true;
  }

  return false;
}

nsresult GStreamerReader::CheckSupportedFormats()
{
  bool done = false;
  bool unsupported = false;

  GstIterator* it = gst_bin_iterate_recurse(GST_BIN(mPlayBin));
  while (!done) {
    GstIteratorResult res;
    GstElement* element;

#if GST_VERSION_MAJOR >= 1
    GValue value = {0,};
    res = gst_iterator_next(it, &value);
#else
    res = gst_iterator_next(it, (void **) &element);
#endif
    switch(res) {
      case GST_ITERATOR_OK:
      {
#if GST_VERSION_MAJOR >= 1
        element = GST_ELEMENT (g_value_get_object (&value));
#endif
        GstElementFactory* factory = gst_element_get_factory(element);
        if (factory) {
          const char* klass = gst_element_factory_get_klass(factory);
          GstPad* pad = gst_element_get_static_pad(element, "sink");
          if (pad) {
            GstCaps* caps;

#if GST_VERSION_MAJOR >= 1
            caps = gst_pad_get_current_caps(pad);
#else
            caps = gst_pad_get_negotiated_caps(pad);
#endif

            if (caps) {
              /* check for demuxers but ignore elements like id3demux */
              if (strstr (klass, "Demuxer") && !strstr(klass, "Metadata"))
                unsupported = !GStreamerFormatHelper::Instance()->CanHandleContainerCaps(caps);
              else if (strstr (klass, "Decoder") && !strstr(klass, "Generic"))
                unsupported = !GStreamerFormatHelper::Instance()->CanHandleCodecCaps(caps);

              gst_caps_unref(caps);
            }
            gst_object_unref(pad);
          }
        }

#if GST_VERSION_MAJOR >= 1
        g_value_unset (&value);
#else
        gst_object_unref(element);
#endif
        done = unsupported;
        break;
      }
      case GST_ITERATOR_RESYNC:
        unsupported = false;
        break;
      case GST_ITERATOR_ERROR:
        done = true;
        break;
      case GST_ITERATOR_DONE:
        done = true;
        break;
    }
  }

  gst_iterator_free(it);

  return unsupported ? NS_ERROR_FAILURE : NS_OK;
}

nsresult GStreamerReader::ResetDecode()
{
  nsresult res = NS_OK;

  LOG(LogLevel::Debug, "reset decode");

  if (NS_FAILED(MediaDecoderReader::ResetDecode())) {
    res = NS_ERROR_FAILURE;
  }

  mVideoQueue.Reset();
  mAudioQueue.Reset();

  mVideoSinkBufferCount = 0;
  mAudioSinkBufferCount = 0;
  mReachedAudioEos = false;
  mReachedVideoEos = false;
#if GST_VERSION_MAJOR >= 1
  mConfigureAlignment = true;
#endif

  LOG(LogLevel::Debug, "reset decode done");

  return res;
}

bool GStreamerReader::DecodeAudioData()
{
  MOZ_ASSERT(OnTaskQueue());

  GstBuffer *buffer = nullptr;

  {
    ReentrantMonitorAutoEnter mon(mGstThreadsMonitor);

    if (mReachedAudioEos && !mAudioSinkBufferCount) {
      return false;
    }

    /* Wait something to be decoded before return or continue */
    if (!mAudioSinkBufferCount) {
      if(!mVideoSinkBufferCount) {
        /* We have nothing decoded so it makes no sense to return to the state machine
         * as it will call us back immediately, we'll return again and so on, wasting
         * CPU cycles for no job done. So, block here until there is either video or
         * audio data available
        */
        mon.Wait();
        if (!mAudioSinkBufferCount) {
          /* There is still no audio data available, so either there is video data or
           * something else has happened (Eos, etc...). Return to the state machine
           * to process it.
           */
          return true;
        }
      }
      else {
        return true;
      }
    }

#if GST_VERSION_MAJOR >= 1
    GstSample *sample = gst_app_sink_pull_sample(mAudioAppSink);
    buffer = gst_buffer_ref(gst_sample_get_buffer(sample));
    gst_sample_unref(sample);
#else
    buffer = gst_app_sink_pull_buffer(mAudioAppSink);
#endif

    mAudioSinkBufferCount--;
  }

  int64_t timestamp = GST_BUFFER_TIMESTAMP(buffer);
  {
    ReentrantMonitorAutoEnter mon(mGstThreadsMonitor);
    timestamp = gst_segment_to_stream_time(&mAudioSegment,
                                           GST_FORMAT_TIME, timestamp);
  }
  timestamp = GST_TIME_AS_USECONDS(timestamp);

  int64_t offset = GST_BUFFER_OFFSET(buffer);
  guint8* data;
#if GST_VERSION_MAJOR >= 1
  GstMapInfo info;
  gst_buffer_map(buffer, &info, GST_MAP_READ);
  unsigned int size = info.size;
  data = info.data;
#else
  unsigned int size = GST_BUFFER_SIZE(buffer);
  data = GST_BUFFER_DATA(buffer);
#endif
  int32_t frames = (size / sizeof(AudioDataValue)) / mInfo.mAudio.mChannels;

  typedef AudioCompactor::NativeCopy GstCopy;
  mAudioCompactor.Push(offset,
                       timestamp,
                       mInfo.mAudio.mRate,
                       frames,
                       mInfo.mAudio.mChannels,
                       GstCopy(data,
                               size,
                               mInfo.mAudio.mChannels));
#if GST_VERSION_MAJOR >= 1
  gst_buffer_unmap(buffer, &info);
#endif

  gst_buffer_unref(buffer);

  return true;
}

bool GStreamerReader::DecodeVideoFrame(bool &aKeyFrameSkip,
                                       int64_t aTimeThreshold)
{
  MOZ_ASSERT(OnTaskQueue());

  GstBuffer *buffer = nullptr;

  {
    ReentrantMonitorAutoEnter mon(mGstThreadsMonitor);

    if (mReachedVideoEos && !mVideoSinkBufferCount) {
      return false;
    }

    /* Wait something to be decoded before return or continue */
    if (!mVideoSinkBufferCount) {
      if (!mAudioSinkBufferCount) {
        /* We have nothing decoded so it makes no sense to return to the state machine
         * as it will call us back immediately, we'll return again and so on, wasting
         * CPU cycles for no job done. So, block here until there is either video or
         * audio data available
        */
        mon.Wait();
        if (!mVideoSinkBufferCount) {
          /* There is still no video data available, so either there is audio data or
           * something else has happened (Eos, etc...). Return to the state machine
           * to process it
           */
          return true;
        }
      }
      else {
        return true;
      }
    }

    mDecoder->NotifyDecodedFrames(0, 1, 0);

#if GST_VERSION_MAJOR >= 1
    GstSample *sample = gst_app_sink_pull_sample(mVideoAppSink);
    buffer = gst_buffer_ref(gst_sample_get_buffer(sample));
    gst_sample_unref(sample);
#else
    buffer = gst_app_sink_pull_buffer(mVideoAppSink);
#endif
    mVideoSinkBufferCount--;
  }

  bool isKeyframe = !GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT);
  if ((aKeyFrameSkip && !isKeyframe)) {
    mDecoder->NotifyDecodedFrames(0, 0, 1);
    gst_buffer_unref(buffer);
    return true;
  }

  int64_t timestamp = GST_BUFFER_TIMESTAMP(buffer);
  {
    ReentrantMonitorAutoEnter mon(mGstThreadsMonitor);
    timestamp = gst_segment_to_stream_time(&mVideoSegment,
                                           GST_FORMAT_TIME, timestamp);
  }
  NS_ASSERTION(GST_CLOCK_TIME_IS_VALID(timestamp),
               "frame has invalid timestamp");

  timestamp = GST_TIME_AS_USECONDS(timestamp);
  int64_t duration = 0;
  if (GST_CLOCK_TIME_IS_VALID(GST_BUFFER_DURATION(buffer)))
    duration = GST_TIME_AS_USECONDS(GST_BUFFER_DURATION(buffer));
  else if (fpsNum && fpsDen)
    /* add 1-frame duration */
    duration = gst_util_uint64_scale(GST_USECOND, fpsDen, fpsNum);

  if (timestamp < aTimeThreshold) {
    LOG(LogLevel::Debug, "skipping frame %" GST_TIME_FORMAT
                      " threshold %" GST_TIME_FORMAT,
                      GST_TIME_ARGS(timestamp * 1000),
                      GST_TIME_ARGS(aTimeThreshold * 1000));
    gst_buffer_unref(buffer);
    return true;
  }

  if (!buffer)
    /* no more frames */
    return true;

#if GST_VERSION_MAJOR >= 1
  if (mConfigureAlignment && buffer->pool) {
    GstStructure *config = gst_buffer_pool_get_config(buffer->pool);
    GstVideoAlignment align;
    if (gst_buffer_pool_config_get_video_alignment(config, &align))
      gst_video_info_align(&mVideoInfo, &align);
    gst_structure_free(config);
    mConfigureAlignment = false;
  }
#endif

  nsRefPtr<PlanarYCbCrImage> image = GetImageFromBuffer(buffer);
  if (!image) {
    /* Ugh, upstream is not calling gst_pad_alloc_buffer(). Fallback to
     * allocating a PlanarYCbCrImage backed GstBuffer here and memcpy.
     */
    GstBuffer* tmp = nullptr;
    CopyIntoImageBuffer(buffer, &tmp, image);
    gst_buffer_unref(buffer);
    buffer = tmp;
  }

  int64_t offset = mResource.Tell(); // Estimate location in media.
  nsRefPtr<VideoData> video = VideoData::CreateFromImage(mInfo.mVideo,
                                                         mDecoder->GetImageContainer(),
                                                         offset, timestamp, duration,
                                                         static_cast<Image*>(image.get()),
                                                         isKeyframe, -1, mPicture);
  mVideoQueue.Push(video);

  gst_buffer_unref(buffer);

  return true;
}

nsRefPtr<MediaDecoderReader::SeekPromise>
GStreamerReader::Seek(int64_t aTarget, int64_t aEndTime)
{
  MOZ_ASSERT(OnTaskQueue());

  gint64 seekPos = aTarget * GST_USECOND;
  LOG(LogLevel::Debug, "%p About to seek to %" GST_TIME_FORMAT,
        mDecoder, GST_TIME_ARGS(seekPos));

  int flags = GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT;
  if (!gst_element_seek_simple(mPlayBin,
                               GST_FORMAT_TIME,
                               static_cast<GstSeekFlags>(flags),
                               seekPos)) {
    LOG(LogLevel::Error, "seek failed");
    return SeekPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }
  LOG(LogLevel::Debug, "seek succeeded");
  GstMessage* message = gst_bus_timed_pop_filtered(mBus, GST_CLOCK_TIME_NONE,
               (GstMessageType)(GST_MESSAGE_ASYNC_DONE | GST_MESSAGE_ERROR));
  gst_message_unref(message);
  LOG(LogLevel::Debug, "seek completed");

  return SeekPromise::CreateAndResolve(aTarget, __func__);
}

media::TimeIntervals GStreamerReader::GetBuffered()
{
  MOZ_ASSERT(OnTaskQueue());
  if (!HaveStartTime()) {
    return media::TimeIntervals();
  }
  media::TimeIntervals buffered;
  if (!mInfo.HasValidMedia()) {
    return buffered;
  }

#if GST_VERSION_MAJOR == 0
  GstFormat format = GST_FORMAT_TIME;
#endif
  AutoPinned<MediaResource> resource(mDecoder->GetResource());
  nsTArray<MediaByteRange> ranges;
  resource->GetCachedRanges(ranges);

  if (resource->IsDataCachedToEndOfResource(0)) {
    /* fast path for local or completely cached files */
    gint64 duration =
       mDuration.Ref().refOr(media::TimeUnit::FromMicroseconds(0)).ToMicroseconds();
    LOG(LogLevel::Debug, "complete range [0, %f] for [0, %li]",
        (double) duration / GST_MSECOND, GetDataLength());
    buffered +=
      media::TimeInterval(media::TimeUnit::FromMicroseconds(0),
                          media::TimeUnit::FromMicroseconds(duration));
    return buffered;
  }

  for(uint32_t index = 0; index < ranges.Length(); index++) {
    int64_t startOffset = ranges[index].mStart;
    int64_t endOffset = ranges[index].mEnd;
    gint64 startTime, endTime, duration;
    bool haveDuration = false;

#if GST_VERSION_MAJOR >= 1
    if (!gst_element_query_convert(GST_ELEMENT(mPlayBin), GST_FORMAT_BYTES,
      startOffset, GST_FORMAT_TIME, &startTime))
      continue;
    if (!gst_element_query_convert(GST_ELEMENT(mPlayBin), GST_FORMAT_BYTES,
      endOffset, GST_FORMAT_TIME, &endTime))
      continue;
    if (gst_element_query_duration(GST_ELEMENT(mPlayBin),
      GST_FORMAT_TIME, &duration)) {
      haveDuration = true;
    }
#else
    if (!gst_element_query_convert(GST_ELEMENT(mPlayBin), GST_FORMAT_BYTES,
      startOffset, &format, &startTime) || format != GST_FORMAT_TIME)
      continue;
    if (!gst_element_query_convert(GST_ELEMENT(mPlayBin), GST_FORMAT_BYTES,
      endOffset, &format, &endTime) || format != GST_FORMAT_TIME)
      continue;
    if (gst_element_query_duration(GST_ELEMENT(mPlayBin),
      &format, &duration) && format == GST_FORMAT_TIME) {
      haveDuration = true;
    }
#endif
    // Check that the estimated time doesn't go beyond known duration
    // as this indicates a buggy gst plugin.
    if (haveDuration && endTime > duration) {
      LOG(LogLevel::Debug, "Have duration %" GST_TIME_FORMAT "contradicting endTime %" GST_TIME_FORMAT,
          GST_TIME_ARGS(duration), GST_TIME_ARGS(endTime));
      endTime = std::min(endTime, duration);
    }

    LOG(LogLevel::Debug, "adding range [%f, %f] for [%li %li] size %li",
        (double) GST_TIME_AS_USECONDS (startTime) / GST_MSECOND,
        (double) GST_TIME_AS_USECONDS (endTime) / GST_MSECOND,
        startOffset, endOffset, GetDataLength());
    buffered +=
      media::TimeInterval(media::TimeUnit::FromMicroseconds(GST_TIME_AS_USECONDS(startTime)),
                          media::TimeUnit::FromMicroseconds(GST_TIME_AS_USECONDS(endTime)));
  }

  return buffered;
}

void GStreamerReader::ReadAndPushData(guint aLength)
{
  int64_t offset1 = mResource.Tell();
  unused << offset1;
  nsresult rv = NS_OK;

  GstBuffer* buffer = gst_buffer_new_and_alloc(aLength);
#if GST_VERSION_MAJOR >= 1
  GstMapInfo info;
  gst_buffer_map(buffer, &info, GST_MAP_WRITE);
  guint8 *data = info.data;
#else
  guint8* data = GST_BUFFER_DATA(buffer);
#endif
  uint32_t size = 0, bytesRead = 0;
  while(bytesRead < aLength) {
    rv = mResource.Read(reinterpret_cast<char*>(data + bytesRead),
                        aLength - bytesRead, &size);
    if (NS_FAILED(rv) || size == 0)
      break;

    bytesRead += size;
  }

  int64_t offset2 = mResource.Tell();
  unused << offset2;

#if GST_VERSION_MAJOR >= 1
  gst_buffer_unmap(buffer, &info);
  gst_buffer_set_size(buffer, bytesRead);
#else
  GST_BUFFER_SIZE(buffer) = bytesRead;
#endif

  GstFlowReturn ret = gst_app_src_push_buffer(mSource, gst_buffer_ref(buffer));
  if (ret != GST_FLOW_OK) {
    LOG(LogLevel::Error, "ReadAndPushData push ret %s(%d)", gst_flow_get_name(ret), ret);
  }

  if (NS_FAILED(rv)) {
    /* Terminate the stream if there is an error in reading */
    LOG(LogLevel::Error, "ReadAndPushData read error, rv=%x", rv);
    gst_app_src_end_of_stream(mSource);
  } else if (bytesRead < aLength) {
    /* If we read less than what we wanted, we reached the end */
    LOG(LogLevel::Warning, "ReadAndPushData read underflow, "
        "bytesRead=%u, aLength=%u, offset(%lld,%lld)",
        bytesRead, aLength, offset1, offset2);
    gst_app_src_end_of_stream(mSource);
  }

  gst_buffer_unref(buffer);

  /* Ensure offset change is consistent in this function.
   * If there are other stream operations on another thread at the same time,
   * it will disturb the GStreamer state machine.
   */
  MOZ_ASSERT(offset1 + bytesRead == offset2);
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
  aOffset += mDataOffset;

  ReentrantMonitorAutoEnter mon(mGstThreadsMonitor);
  int64_t resourceLength = mResource.GetLength();

  if (gst_app_src_get_size(mSource) == -1) {
    /* It's possible that we didn't know the length when we initialized mSource
     * but maybe we do now
     */
    gst_app_src_set_size(mSource, GetDataLength());
  }

  nsresult rv = NS_ERROR_FAILURE;
  if (aOffset < static_cast<guint64>(resourceLength)) {
    rv = mResource.Seek(SEEK_SET, aOffset);
  }

  return NS_SUCCEEDED(rv);
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
  LOG(LogLevel::Debug, "Audio preroll");
  GstPad* sinkpad = gst_element_get_static_pad(GST_ELEMENT(mAudioAppSink), "sink");
#if GST_VERSION_MAJOR >= 1
  GstCaps *caps = gst_pad_get_current_caps(sinkpad);
#else
  GstCaps* caps = gst_pad_get_negotiated_caps(sinkpad);
#endif
  GstStructure* s = gst_caps_get_structure(caps, 0);
  mInfo.mAudio.mRate = mInfo.mAudio.mChannels = 0;
  gst_structure_get_int(s, "rate", (gint*) &mInfo.mAudio.mRate);
  gst_structure_get_int(s, "channels", (gint*) &mInfo.mAudio.mChannels);
  NS_ASSERTION(mInfo.mAudio.mRate != 0, ("audio rate is zero"));
  NS_ASSERTION(mInfo.mAudio.mChannels != 0, ("audio channels is zero"));
  NS_ASSERTION(mInfo.mAudio.mChannels > 0 && mInfo.mAudio.mChannels <= MAX_CHANNELS,
      "invalid audio channels number");
  gst_caps_unref(caps);
  gst_object_unref(sinkpad);
}

void GStreamerReader::VideoPreroll()
{
  /* The first video buffer has reached the video sink. Get width and height */
  LOG(LogLevel::Debug, "Video preroll");
  GstPad* sinkpad = gst_element_get_static_pad(GST_ELEMENT(mVideoAppSink), "sink");
  int PARNumerator, PARDenominator;
#if GST_VERSION_MAJOR >= 1
  GstCaps* caps = gst_pad_get_current_caps(sinkpad);
  memset (&mVideoInfo, 0, sizeof (mVideoInfo));
  gst_video_info_from_caps(&mVideoInfo, caps);
  mFormat = mVideoInfo.finfo->format;
  mPicture.width = mVideoInfo.width;
  mPicture.height = mVideoInfo.height;
  PARNumerator = GST_VIDEO_INFO_PAR_N(&mVideoInfo);
  PARDenominator = GST_VIDEO_INFO_PAR_D(&mVideoInfo);
#else
  GstCaps* caps = gst_pad_get_negotiated_caps(sinkpad);
  gst_video_format_parse_caps(caps, &mFormat, &mPicture.width, &mPicture.height);
  if (!gst_video_parse_caps_pixel_aspect_ratio(caps, &PARNumerator, &PARDenominator)) {
    PARNumerator = 1;
    PARDenominator = 1;
  }
#endif
  NS_ASSERTION(mPicture.width && mPicture.height, "invalid video resolution");

  // Calculate display size according to pixel aspect ratio.
  nsIntRect pictureRect(0, 0, mPicture.width, mPicture.height);
  nsIntSize frameSize = nsIntSize(mPicture.width, mPicture.height);
  nsIntSize displaySize = nsIntSize(mPicture.width, mPicture.height);
  ScaleDisplayByAspectRatio(displaySize, float(PARNumerator) / float(PARDenominator));

  // If video frame size is overflow, stop playing.
  if (IsValidVideoRegion(frameSize, pictureRect, displaySize)) {
    GstStructure* structure = gst_caps_get_structure(caps, 0);
    gst_structure_get_fraction(structure, "framerate", &fpsNum, &fpsDen);
    mInfo.mVideo.mDisplay = displaySize;
  } else {
    LOG(LogLevel::Debug, "invalid video region");
    Eos();
  }
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

  mDecoder->NotifyDecodedFrames(1, 0, 0);
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
    if (aSink == mVideoAppSink) {
      mReachedVideoEos = true;
    } else if (aSink == mAudioAppSink) {
      mReachedAudioEos = true;
    } else {
      // Assume this is an error causing an EOS.
      mReachedAudioEos = true;
      mReachedVideoEos = true;
    }
    mon.NotifyAll();
  }
}

/**
 * This callback is called while the pipeline is automatically built, after a
 * new element has been added to the pipeline. We use it to find the
 * uridecodebin instance used by playbin and connect to it to apply our
 * blacklist.
 */
void
GStreamerReader::PlayElementAddedCb(GstBin *aBin, GstElement *aElement,
                                    gpointer *aUserData)
{
  const static char sUriDecodeBinPrefix[] = "uridecodebin";
  gchar *name = gst_element_get_name(aElement);

  // Attach this callback to uridecodebin, child of playbin.
  if (!strncmp(name, sUriDecodeBinPrefix, sizeof(sUriDecodeBinPrefix) - 1)) {
    g_signal_connect(G_OBJECT(aElement), "autoplug-sort",
                     G_CALLBACK(GStreamerReader::AutoplugSortCb), aUserData);
  }

  g_free(name);
}

bool
GStreamerReader::ShouldAutoplugFactory(GstElementFactory* aFactory, GstCaps* aCaps)
{
  bool autoplug;
  const gchar *klass = gst_element_factory_get_klass(aFactory);
  if (strstr(klass, "Demuxer") && !strstr(klass, "Metadata")) {
    autoplug = GStreamerFormatHelper::Instance()->CanHandleContainerCaps(aCaps);
  } else if (strstr(klass, "Decoder") && !strstr(klass, "Generic")) {
    autoplug = GStreamerFormatHelper::Instance()->CanHandleCodecCaps(aCaps);
  } else {
    /* we only filter demuxers and decoders, let everything else be autoplugged */
    autoplug = true;
  }

  return autoplug;
}

/**
 * This is called by uridecodebin (running inside playbin), after it has found
 * candidate factories to continue decoding the stream. We apply the blacklist
 * here, disallowing known-crashy plugins.
 */
GValueArray*
GStreamerReader::AutoplugSortCb(GstElement* aElement, GstPad* aPad,
                                GstCaps* aCaps, GValueArray* aFactories)
{
  if (!aFactories->n_values) {
    return nullptr;
  }

  /* aFactories[0] is the element factory that is going to be used to
   * create the next element needed to demux or decode the stream.
   */
  GstElementFactory *factory = (GstElementFactory*) g_value_get_object(g_value_array_get_nth(aFactories, 0));
  if (!ShouldAutoplugFactory(factory, aCaps)) {
    /* We don't support this factory. Return an empty array to signal that we
     * don't want to continue decoding this (sub)stream.
     */
    return g_value_array_new(0);
  }

  /* nullptr means that we're ok with the candidates and don't need to apply any
   * sorting/filtering.
   */
  return nullptr;
}

/**
 * If this is an MP3 stream, pass any new data we get to the MP3 frame parser
 * for duration estimation.
 */
void GStreamerReader::NotifyDataArrivedInternal(uint32_t aLength,
                                                int64_t aOffset)
{
  MOZ_ASSERT(OnTaskQueue());
  if (HasVideo()) {
    return;
  }
  if (!mMP3FrameParser.NeedsData()) {
    return;
  }

  IntervalSet<int64_t> intervals = mFilter.NotifyDataArrived(aLength, aOffset);
  for (const auto& interval : intervals) {
    nsRefPtr<MediaByteBuffer> bytes =
      mResource.MediaReadAt(interval.mStart, interval.Length());
    NS_ENSURE_TRUE_VOID(bytes);
    mMP3FrameParser.Parse(bytes->Elements(), interval.Length(), interval.mStart);
    if (!mMP3FrameParser.IsMP3()) {
      return;
    }

    int64_t duration = mMP3FrameParser.GetDuration();
    if (duration != mLastParserDuration && mUseParserDuration) {
      MOZ_ASSERT(mDecoder);
      mLastParserDuration = duration;
      mDecoder->DispatchUpdateEstimatedMediaDuration(mLastParserDuration);
    }
  }
}

#if GST_VERSION_MAJOR >= 1
GstCaps* GStreamerReader::BuildAudioSinkCaps()
{
  GstCaps* caps = gst_caps_from_string("audio/x-raw, channels={1,2}");
  const char* format;
#ifdef MOZ_SAMPLE_TYPE_FLOAT32
#if MOZ_LITTLE_ENDIAN
  format = "F32LE";
#else
  format = "F32BE";
#endif
#else /* !MOZ_SAMPLE_TYPE_FLOAT32 */
#if MOZ_LITTLE_ENDIAN
  format = "S16LE";
#else
  format = "S16BE";
#endif
#endif
  gst_caps_set_simple(caps, "format", G_TYPE_STRING, format, nullptr);

  return caps;
}

void GStreamerReader::InstallPadCallbacks()
{
  GstPad* sinkpad = gst_element_get_static_pad(GST_ELEMENT(mVideoAppSink), "sink");

  gst_pad_add_probe(sinkpad,
      (GstPadProbeType) (GST_PAD_PROBE_TYPE_SCHEDULING |
        GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM |
        GST_PAD_PROBE_TYPE_EVENT_UPSTREAM |
        GST_PAD_PROBE_TYPE_EVENT_FLUSH),
      &GStreamerReader::EventProbeCb, this, nullptr);
  gst_pad_add_probe(sinkpad, GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM,
      GStreamerReader::QueryProbeCb, nullptr, nullptr);

  gst_pad_set_element_private(sinkpad, this);
  gst_object_unref(sinkpad);

  sinkpad = gst_element_get_static_pad(GST_ELEMENT(mAudioAppSink), "sink");
  gst_pad_add_probe(sinkpad,
      (GstPadProbeType) (GST_PAD_PROBE_TYPE_SCHEDULING |
        GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM |
        GST_PAD_PROBE_TYPE_EVENT_UPSTREAM |
        GST_PAD_PROBE_TYPE_EVENT_FLUSH),
      &GStreamerReader::EventProbeCb, this, nullptr);
  gst_object_unref(sinkpad);
}

GstPadProbeReturn GStreamerReader::EventProbeCb(GstPad *aPad,
                                                GstPadProbeInfo *aInfo,
                                                gpointer aUserData)
{
  GStreamerReader *reader = (GStreamerReader *) aUserData;
  GstEvent *aEvent = (GstEvent *)aInfo->data;
  return reader->EventProbe(aPad, aEvent);
}

GstPadProbeReturn GStreamerReader::EventProbe(GstPad *aPad, GstEvent *aEvent)
{
  GstElement* parent = GST_ELEMENT(gst_pad_get_parent(aPad));

  LOG(LogLevel::Debug, "event probe %s", GST_EVENT_TYPE_NAME (aEvent));

  switch(GST_EVENT_TYPE(aEvent)) {
    case GST_EVENT_SEGMENT:
    {
      const GstSegment *newSegment;
      GstSegment* segment;

      /* Store the segments so we can convert timestamps to stream time, which
       * is what the upper layers sync on.
       */
      ReentrantMonitorAutoEnter mon(mGstThreadsMonitor);
#if GST_VERSION_MINOR <= 1 && GST_VERSION_MICRO < 1
      ResetDecode();
#endif
      gst_event_parse_segment(aEvent, &newSegment);
      if (parent == GST_ELEMENT(mVideoAppSink))
        segment = &mVideoSegment;
      else
        segment = &mAudioSegment;
      gst_segment_copy_into (newSegment, segment);
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

  return GST_PAD_PROBE_OK;
}

GstPadProbeReturn GStreamerReader::QueryProbeCb(GstPad* aPad, GstPadProbeInfo* aInfo, gpointer aUserData)
{
  GStreamerReader* reader = reinterpret_cast<GStreamerReader*>(gst_pad_get_element_private(aPad));
  return reader->QueryProbe(aPad, aInfo, aUserData);
}

GstPadProbeReturn GStreamerReader::QueryProbe(GstPad* aPad, GstPadProbeInfo* aInfo, gpointer aUserData)
{
  GstQuery *query = gst_pad_probe_info_get_query(aInfo);
  GstPadProbeReturn ret = GST_PAD_PROBE_OK;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_ALLOCATION:
      GstCaps *caps;
      GstVideoInfo info;
      gboolean need_pool;

      gst_query_parse_allocation(query, &caps, &need_pool);
      gst_video_info_init(&info);
      gst_video_info_from_caps(&info, caps);
      gst_query_add_allocation_param(query, mAllocator, nullptr);
      gst_query_add_allocation_pool(query, mBufferPool, info.size, 0, 0);
      break;
    default:
      break;
  }

  return ret;
}

void GStreamerReader::ImageDataFromVideoFrame(GstVideoFrame *aFrame,
                                              PlanarYCbCrImage::Data *aData)
{
  NS_ASSERTION(GST_VIDEO_INFO_IS_YUV(&mVideoInfo),
               "Non-YUV video frame formats not supported");
  NS_ASSERTION(GST_VIDEO_FRAME_N_COMPONENTS(aFrame) == 3,
               "Unsupported number of components in video frame");

  aData->mPicX = aData->mPicY = 0;
  aData->mPicSize = gfx::IntSize(mPicture.width, mPicture.height);
  aData->mStereoMode = StereoMode::MONO;

  aData->mYChannel = GST_VIDEO_FRAME_COMP_DATA(aFrame, 0);
  aData->mYStride = GST_VIDEO_FRAME_COMP_STRIDE(aFrame, 0);
  aData->mYSize = gfx::IntSize(GST_VIDEO_FRAME_COMP_WIDTH(aFrame, 0),
                          GST_VIDEO_FRAME_COMP_HEIGHT(aFrame, 0));
  aData->mYSkip = GST_VIDEO_FRAME_COMP_PSTRIDE(aFrame, 0) - 1;
  aData->mCbCrStride = GST_VIDEO_FRAME_COMP_STRIDE(aFrame, 1);
  aData->mCbCrSize = gfx::IntSize(GST_VIDEO_FRAME_COMP_WIDTH(aFrame, 1),
                             GST_VIDEO_FRAME_COMP_HEIGHT(aFrame, 1));
  aData->mCbChannel = GST_VIDEO_FRAME_COMP_DATA(aFrame, 1);
  aData->mCrChannel = GST_VIDEO_FRAME_COMP_DATA(aFrame, 2);
  aData->mCbSkip = GST_VIDEO_FRAME_COMP_PSTRIDE(aFrame, 1) - 1;
  aData->mCrSkip = GST_VIDEO_FRAME_COMP_PSTRIDE(aFrame, 2) - 1;
}

nsRefPtr<PlanarYCbCrImage> GStreamerReader::GetImageFromBuffer(GstBuffer* aBuffer)
{
  nsRefPtr<PlanarYCbCrImage> image = nullptr;

  if (gst_buffer_n_memory(aBuffer) == 1) {
    GstMemory* mem = gst_buffer_peek_memory(aBuffer, 0);
    if (GST_IS_MOZ_GFX_MEMORY_ALLOCATOR(mem->allocator)) {
      image = moz_gfx_memory_get_image(mem);

      GstVideoFrame frame;
      gst_video_frame_map(&frame, &mVideoInfo, aBuffer, GST_MAP_READ);
      PlanarYCbCrImage::Data data;
      ImageDataFromVideoFrame(&frame, &data);
      image->SetDataNoCopy(data);
      gst_video_frame_unmap(&frame);
    }
  }

  return image;
}

void GStreamerReader::CopyIntoImageBuffer(GstBuffer* aBuffer,
                                          GstBuffer** aOutBuffer,
                                          nsRefPtr<PlanarYCbCrImage> &image)
{
  *aOutBuffer = gst_buffer_new_allocate(mAllocator, gst_buffer_get_size(aBuffer), nullptr);
  GstMemory *mem = gst_buffer_peek_memory(*aOutBuffer, 0);
  GstMapInfo map_info;
  gst_memory_map(mem, &map_info, GST_MAP_WRITE);
  gst_buffer_extract(aBuffer, 0, map_info.data, gst_buffer_get_size(aBuffer));
  gst_memory_unmap(mem, &map_info);

  /* create a new gst buffer with the newly created memory and copy the
   * metadata over from the incoming buffer */
  gst_buffer_copy_into(*aOutBuffer, aBuffer,
      (GstBufferCopyFlags)(GST_BUFFER_COPY_METADATA), 0, -1);
  image = GetImageFromBuffer(*aOutBuffer);
}
#endif

} // namespace mozilla

