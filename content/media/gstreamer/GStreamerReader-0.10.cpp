#include "nsError.h"
#include "MediaDecoderStateMachine.h"
#include "AbstractMediaDecoder.h"
#include "MediaResource.h"
#include "GStreamerReader.h"
#include "GStreamerMozVideoBuffer.h"
#include "GStreamerFormatHelper.h"
#include "VideoUtils.h"
#include "mozilla/dom/TimeRanges.h"
#include "mozilla/Preferences.h"

using namespace mozilla;
using mozilla::layers::PlanarYCbCrImage;
using mozilla::layers::ImageContainer;

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

GstFlowReturn GStreamerReader::AllocateVideoBufferFull(GstPad* aPad,
                                                       guint64 aOffset,
                                                       guint aSize,
                                                       GstCaps* aCaps,
                                                       GstBuffer** aBuf,
                                                       nsRefPtr<PlanarYCbCrImage>& aImage)
{
  /* allocate an image using the container */
  ImageContainer* container = mDecoder->GetImageContainer();
  if (container == nullptr) {
    return GST_FLOW_ERROR;
  }
  PlanarYCbCrImage* img = reinterpret_cast<PlanarYCbCrImage*>(container->CreateImage(ImageFormat::PLANAR_YCBCR).get());
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

gboolean GStreamerReader::EventProbeCb(GstPad* aPad,
                                         GstEvent* aEvent,
                                         gpointer aUserData)
{
  GStreamerReader* reader = reinterpret_cast<GStreamerReader*>(aUserData);
  return reader->EventProbe(aPad, aEvent);
}

nsRefPtr<PlanarYCbCrImage> GStreamerReader::GetImageFromBuffer(GstBuffer* aBuffer)
{
  if (!GST_IS_MOZ_VIDEO_BUFFER (aBuffer))
    return nullptr;

  nsRefPtr<PlanarYCbCrImage> image;
  GstMozVideoBufferData* bufferdata = reinterpret_cast<GstMozVideoBufferData*>(gst_moz_video_buffer_get_data(GST_MOZ_VIDEO_BUFFER(aBuffer)));
  image = bufferdata->mImage;

  PlanarYCbCrImage::Data data;
  data.mPicX = data.mPicY = 0;
  data.mPicSize = gfx::IntSize(mPicture.width, mPicture.height);
  data.mStereoMode = StereoMode::MONO;

  data.mYChannel = GST_BUFFER_DATA(aBuffer);
  data.mYStride = gst_video_format_get_row_stride(mFormat, 0, mPicture.width);
  data.mYSize = gfx::IntSize(data.mYStride,
      gst_video_format_get_component_height(mFormat, 0, mPicture.height));
  data.mYSkip = 0;
  data.mCbCrStride = gst_video_format_get_row_stride(mFormat, 1, mPicture.width);
  data.mCbCrSize = gfx::IntSize(data.mCbCrStride,
      gst_video_format_get_component_height(mFormat, 1, mPicture.height));
  data.mCbChannel = data.mYChannel + gst_video_format_get_component_offset(mFormat, 1,
      mPicture.width, mPicture.height);
  data.mCrChannel = data.mYChannel + gst_video_format_get_component_offset(mFormat, 2,
      mPicture.width, mPicture.height);
  data.mCbSkip = 0;
  data.mCrSkip = 0;

  image->SetDataNoCopy(data);

  return image;
}

void GStreamerReader::CopyIntoImageBuffer(GstBuffer* aBuffer,
                                          GstBuffer** aOutBuffer,
                                          nsRefPtr<PlanarYCbCrImage> &aImage)
{
  AllocateVideoBufferFull(nullptr, GST_BUFFER_OFFSET(aBuffer),
      GST_BUFFER_SIZE(aBuffer), nullptr, aOutBuffer, aImage);

  gst_buffer_copy_metadata(*aOutBuffer, aBuffer, (GstBufferCopyFlags)GST_BUFFER_COPY_ALL);
  memcpy(GST_BUFFER_DATA(*aOutBuffer), GST_BUFFER_DATA(aBuffer), GST_BUFFER_SIZE(*aOutBuffer));

  aImage = GetImageFromBuffer(*aOutBuffer);
}

GstCaps* GStreamerReader::BuildAudioSinkCaps()
{
  GstCaps* caps;
#ifdef IS_LITTLE_ENDIAN
  int endianness = 1234;
#else
  int endianness = 4321;
#endif
  gint width;
#ifdef MOZ_SAMPLE_TYPE_FLOAT32
  caps = gst_caps_from_string("audio/x-raw-float, channels={1,2}");
  width = 32;
#else /* !MOZ_SAMPLE_TYPE_FLOAT32 */
  caps = gst_caps_from_string("audio/x-raw-int, channels={1,2}");
  width = 16;
#endif
  gst_caps_set_simple(caps,
      "width", G_TYPE_INT, width,
      "endianness", G_TYPE_INT, endianness,
      NULL);

  return caps;
}

void GStreamerReader::InstallPadCallbacks()
{
  GstPad* sinkpad = gst_element_get_static_pad(GST_ELEMENT(mVideoAppSink), "sink");
  gst_pad_add_event_probe(sinkpad,
                          G_CALLBACK(&GStreamerReader::EventProbeCb), this);

  gst_pad_set_bufferalloc_function(sinkpad, GStreamerReader::AllocateVideoBufferCb);
  gst_pad_set_element_private(sinkpad, this);
  gst_object_unref(sinkpad);

  sinkpad = gst_element_get_static_pad(GST_ELEMENT(mAudioAppSink), "sink");
  gst_pad_add_event_probe(sinkpad,
                          G_CALLBACK(&GStreamerReader::EventProbeCb), this);
  gst_object_unref(sinkpad);
}
