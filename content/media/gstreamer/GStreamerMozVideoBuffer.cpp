/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>
#include "GStreamerReader.h"
#include "GStreamerMozVideoBuffer.h"

namespace mozilla {

static GstMozVideoBuffer *gst_moz_video_buffer_copy(GstMozVideoBuffer* self);
static void gst_moz_video_buffer_finalize(GstMozVideoBuffer* self);

G_DEFINE_TYPE(GstMozVideoBuffer, gst_moz_video_buffer, GST_TYPE_BUFFER);

static void
gst_moz_video_buffer_class_init(GstMozVideoBufferClass* klass)
{
  g_return_if_fail(GST_IS_MOZ_VIDEO_BUFFER_CLASS(klass));

  GstMiniObjectClass *mo_class = GST_MINI_OBJECT_CLASS(klass);

  mo_class->copy =(GstMiniObjectCopyFunction)gst_moz_video_buffer_copy;
  mo_class->finalize =(GstMiniObjectFinalizeFunction)gst_moz_video_buffer_finalize;
}

static void
gst_moz_video_buffer_init(GstMozVideoBuffer* self)
{
  g_return_if_fail(GST_IS_MOZ_VIDEO_BUFFER(self));
}

static void
gst_moz_video_buffer_finalize(GstMozVideoBuffer* self)
{
  g_return_if_fail(GST_IS_MOZ_VIDEO_BUFFER(self));

  if(self->data)
    g_boxed_free(GST_TYPE_MOZ_VIDEO_BUFFER_DATA, self->data);

  GST_MINI_OBJECT_CLASS(gst_moz_video_buffer_parent_class)->finalize(GST_MINI_OBJECT(self));
}

static GstMozVideoBuffer*
gst_moz_video_buffer_copy(GstMozVideoBuffer* self)
{
  GstMozVideoBuffer* copy;

  g_return_val_if_fail(GST_IS_MOZ_VIDEO_BUFFER(self), NULL);

  copy = gst_moz_video_buffer_new();

  /* we simply copy everything from our parent */
  GST_BUFFER_DATA(GST_BUFFER_CAST(copy)) =
      (guint8*)g_memdup(GST_BUFFER_DATA(GST_BUFFER_CAST(self)), GST_BUFFER_SIZE(GST_BUFFER_CAST(self)));

  /* make sure it gets freed(even if the parent is subclassed, we return a
     normal buffer) */
  GST_BUFFER_MALLOCDATA(GST_BUFFER_CAST(copy)) = GST_BUFFER_DATA(GST_BUFFER_CAST(copy));
  GST_BUFFER_SIZE(GST_BUFFER_CAST(copy)) = GST_BUFFER_SIZE(GST_BUFFER_CAST(self));

  /* copy metadata */
  gst_buffer_copy_metadata(GST_BUFFER_CAST(copy),
                           GST_BUFFER_CAST(self),
                           (GstBufferCopyFlags)GST_BUFFER_COPY_ALL);
  /* copy videobuffer */
  if(self->data)
    copy->data = (GstMozVideoBufferData*)g_boxed_copy(GST_TYPE_MOZ_VIDEO_BUFFER_DATA, self->data);

  return copy;
}

GstMozVideoBuffer*
gst_moz_video_buffer_new(void)
{
  GstMozVideoBuffer *self;

  self =(GstMozVideoBuffer*)gst_mini_object_new(GST_TYPE_MOZ_VIDEO_BUFFER);
  self->data = nullptr;

  return self;
}

void
gst_moz_video_buffer_set_data(GstMozVideoBuffer* self, GstMozVideoBufferData* data)
{
  g_return_if_fail(GST_IS_MOZ_VIDEO_BUFFER(self));

  self->data = data;
}

GstMozVideoBufferData*
gst_moz_video_buffer_get_data(const GstMozVideoBuffer* self)
{
  g_return_val_if_fail(GST_IS_MOZ_VIDEO_BUFFER(self), NULL);

  return self->data;
}

GType
gst_moz_video_buffer_data_get_type(void)
{
  static volatile gsize g_define_type_id__volatile = 0;

  if(g_once_init_enter(&g_define_type_id__volatile)) {
    GType g_define_type_id =
        g_boxed_type_register_static(g_intern_static_string("GstMozVideoBufferData"),
                                     (GBoxedCopyFunc)GstMozVideoBufferData::Copy,
                                     (GBoxedFreeFunc)GstMozVideoBufferData::Free);
    g_once_init_leave(&g_define_type_id__volatile, g_define_type_id);
  }

  return g_define_type_id__volatile;
}

}
