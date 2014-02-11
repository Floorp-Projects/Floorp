/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GStreamerLoader_h_
#define GStreamerLoader_h_

#include <gst/gst.h>
#include <gst/gstbuffer.h>
#include <gst/gstelementfactory.h>
#include <gst/gststructure.h>
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

#if GST_VERSION_MAJOR == 1
#include <gst/video/gstvideometa.h>
#include <gst/video/gstvideopool.h>
#endif

namespace mozilla {

/*
 * dlopens the required libraries and dlsyms the functions we need.
 * Returns true on success, false otherwise.
 */
bool load_gstreamer();

/*
 * Declare our extern function pointers using the types from the global
 * gstreamer definitions.
 */
#define GST_FUNC(_, func) extern typeof(::func)* func;
#define REPLACE_FUNC(func) GST_FUNC(-1, func)
#include "GStreamerFunctionList.h"
#undef GST_FUNC
#undef REPLACE_FUNC

}

#undef GST_CAPS_ANY
#define GST_CAPS_ANY (*_gst_caps_any)

#endif // GStreamerLoader_h_
