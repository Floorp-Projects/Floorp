/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <dlfcn.h>
#include <stdio.h>

#include "GStreamerLoader.h"

#define LIBGSTREAMER 0
#define LIBGSTAPP 1
#define LIBGSTVIDEO 2

namespace mozilla {

/*
 * Declare our function pointers using the types from the global gstreamer
 * definitions.
 */
#define GST_FUNC(_, func) typeof(::func)* func;
#define REPLACE_FUNC(func) GST_FUNC(-1, func)
#include "GStreamerFunctionList.h"
#undef GST_FUNC
#undef REPLACE_FUNC

/*
 * Redefinitions of functions that have been defined in the gstreamer headers to
 * stop them calling the gstreamer functions in global scope.
 */
GstBuffer * gst_buffer_ref_impl(GstBuffer *buf);
void gst_buffer_unref_impl(GstBuffer *buf);
void gst_message_unref_impl(GstMessage *msg);

bool
load_gstreamer()
{
#ifdef __APPLE__
  return true;
#endif
  static bool loaded = false;

  if (loaded) {
    return true;
  }

  void *gstreamerLib = NULL;
  guint major = 0;
  guint minor = 0;
  guint micro, nano;

  typedef typeof(::gst_version) VersionFuncType;
  if (VersionFuncType *versionFunc = (VersionFuncType*)dlsym(RTLD_DEFAULT, "gst_version")) {
    versionFunc(&major, &minor, &micro, &nano);
  }

  if (major == GST_VERSION_MAJOR && minor == GST_VERSION_MINOR) {
    gstreamerLib = RTLD_DEFAULT;
  } else {
#ifdef __OpenBSD__
    gstreamerLib = dlopen("libgstreamer-0.10.so", RTLD_NOW | RTLD_LOCAL);
#else
    gstreamerLib = dlopen("libgstreamer-0.10.so.0", RTLD_NOW | RTLD_LOCAL);
#endif
  }

  void *handles[] = {
    gstreamerLib,
#ifdef __OpenBSD__
    dlopen("libgstapp-0.10.so", RTLD_NOW | RTLD_LOCAL),
    dlopen("libgstvideo-0.10.so", RTLD_NOW | RTLD_LOCAL)
#else
    dlopen("libgstapp-0.10.so.0", RTLD_NOW | RTLD_LOCAL),
    dlopen("libgstvideo-0.10.so.0", RTLD_NOW | RTLD_LOCAL)
#endif
  };

  for (size_t i = 0; i < sizeof(handles) / sizeof(handles[0]); i++) {
    if (!handles[i]) {
      goto fail;
    }
  }

#define GST_FUNC(lib, symbol) \
  if (!(symbol = (typeof(symbol))dlsym(handles[lib], #symbol))) { \
    goto fail; \
  }
#define REPLACE_FUNC(symbol) symbol = symbol##_impl;
#include "GStreamerFunctionList.h"
#undef GST_FUNC
#undef REPLACE_FUNC

  loaded = true;
  return true;

fail:

  for (size_t i = 0; i < sizeof(handles) / sizeof(handles[0]); i++) {
    if (handles[i] && handles[i] != RTLD_DEFAULT) {
      dlclose(handles[i]);
    }
  }

  return false;
}

GstBuffer *
gst_buffer_ref_impl(GstBuffer *buf)
{
  return (GstBuffer *)gst_mini_object_ref(GST_MINI_OBJECT_CAST(buf));
}

void
gst_buffer_unref_impl(GstBuffer *buf)
{
  gst_mini_object_unref(GST_MINI_OBJECT_CAST(buf));
}

void
gst_message_unref_impl(GstMessage *msg)
{
  gst_mini_object_unref(GST_MINI_OBJECT_CAST(msg));
}

}
