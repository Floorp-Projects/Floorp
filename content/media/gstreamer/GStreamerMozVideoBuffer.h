/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __GST_MOZ_VIDEO_BUFFER_H__
#define __GST_MOZ_VIDEO_BUFFER_H__

#include <gst/gst.h>
#include "GStreamerLoader.h"
#include "MediaDecoderReader.h"

namespace mozilla {

#define GST_TYPE_MOZ_VIDEO_BUFFER_DATA            (gst_moz_video_buffer_data_get_type())
#define GST_IS_MOZ_VIDEO_BUFFER_DATA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_MOZ_VIDEO_BUFFER_DATA))

#define GST_TYPE_MOZ_VIDEO_BUFFER            (gst_moz_video_buffer_get_type())
#define GST_IS_MOZ_VIDEO_BUFFER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_MOZ_VIDEO_BUFFER))
#define GST_IS_MOZ_VIDEO_BUFFER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_MOZ_VIDEO_BUFFER))
#define GST_MOZ_VIDEO_BUFFER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_MOZ_VIDEO_BUFFER, GstMozVideoBuffer))

typedef struct _GstMozVideoBuffer GstMozVideoBuffer;
typedef struct _GstMozVideoBufferClass GstMozVideoBufferClass;

class GstMozVideoBufferData;

struct _GstMozVideoBuffer {
  GstBuffer buffer;
  GstMozVideoBufferData* data;
};

struct _GstMozVideoBufferClass {
  GstBufferClass  buffer_class;
};

GType gst_moz_video_buffer_get_type(void);
GstMozVideoBuffer* gst_moz_video_buffer_new(void);
void gst_moz_video_buffer_set_data(GstMozVideoBuffer* buf, GstMozVideoBufferData* data);
GstMozVideoBufferData* gst_moz_video_buffer_get_data(const GstMozVideoBuffer* buf);

class GstMozVideoBufferData {
  public:
    GstMozVideoBufferData(layers::PlanarYCbCrImage* aImage) : mImage(aImage) {}

    static void* Copy(void* aData) {
      return new GstMozVideoBufferData(reinterpret_cast<GstMozVideoBufferData*>(aData)->mImage);
    }

    static void Free(void* aData) {
      delete reinterpret_cast<GstMozVideoBufferData*>(aData);
    }

    nsRefPtr<layers::PlanarYCbCrImage> mImage;
};

GType gst_moz_video_buffer_data_get_type (void);

} // namespace mozilla

#endif /* __GST_MOZ_VIDEO_BUFFER_H__ */

