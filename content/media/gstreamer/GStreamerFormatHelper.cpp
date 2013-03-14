/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GStreamerFormatHelper.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsXPCOMStrings.h"

#define ENTRY_FORMAT(entry) entry[0]
#define ENTRY_CAPS(entry) entry[1]

GStreamerFormatHelper* GStreamerFormatHelper::gInstance = nullptr;

GStreamerFormatHelper* GStreamerFormatHelper::Instance() {
  if (!gInstance) {
    gst_init(nullptr, nullptr);
    gInstance = new GStreamerFormatHelper();
  }

  return gInstance;
}

void GStreamerFormatHelper::Shutdown() {
  if (gInstance) {
    delete gInstance;
    gInstance = nullptr;
  }
}

char const *const GStreamerFormatHelper::mContainers[6][2] = {
  {"video/mp4", "video/quicktime"},
  {"video/quicktime", "video/quicktime"},
  {"audio/mp4", "audio/mpeg, mpegversion=(int)4"},
  {"audio/x-m4a", "audio/mpeg, mpegversion=(int)4"},
  {"audio/mpeg", "audio/mpeg, mpegversion=(int)1"},
  {"audio/mp3", "audio/mpeg, mpegversion=(int)1"},
};

char const *const GStreamerFormatHelper::mCodecs[9][2] = {
  {"avc1.42E01E", "video/x-h264"},
  {"avc1.42001E", "video/x-h264"},
  {"avc1.58A01E", "video/x-h264"},
  {"avc1.4D401E", "video/x-h264"},
  {"avc1.64001E", "video/x-h264"},
  {"avc1.64001F", "video/x-h264"},
  {"mp4v.20.3", "video/3gpp"},
  {"mp4a.40.2", "audio/mpeg, mpegversion=(int)4"},
  {"mp3", "audio/mpeg, mpegversion=(int)1"},
};

GStreamerFormatHelper::GStreamerFormatHelper()
  : mFactories(nullptr),
    mCookie(static_cast<uint32_t>(-1))
{
}

GStreamerFormatHelper::~GStreamerFormatHelper() {
  if (mFactories)
    g_list_free(mFactories);
}

bool GStreamerFormatHelper::CanHandleMediaType(const nsACString& aMIMEType,
                                               const nsAString* aCodecs) {
  const char *type;
  NS_CStringGetData(aMIMEType, &type, NULL);

  GstCaps* caps = ConvertFormatsToCaps(type, aCodecs);
  if (!caps) {
    return false;
  }

  bool ret = HaveElementsToProcessCaps(caps);
  gst_caps_unref(caps);

  return ret;
}

GstCaps* GStreamerFormatHelper::ConvertFormatsToCaps(const char* aMIMEType,
                                                     const nsAString* aCodecs) {
  unsigned int i;

  /* convert aMIMEType to gst container caps */
  const char* capsString = nullptr;
  for (i = 0; i < G_N_ELEMENTS(mContainers); i++) {
    if (!strcmp(ENTRY_FORMAT(mContainers[i]), aMIMEType)) {
      capsString = ENTRY_CAPS(mContainers[i]);
      break;
    }
  }

  if (!capsString) {
    /* we couldn't find any matching caps */
    return nullptr;
  }

  GstCaps* caps = gst_caps_from_string(capsString);
  /* container only */
  if (!aCodecs) {
    return caps;
  }

  nsCharSeparatedTokenizer tokenizer(*aCodecs, ',');
  while (tokenizer.hasMoreTokens()) {
    const nsSubstring& codec = tokenizer.nextToken();
    capsString = nullptr;

    for (i = 0; i < G_N_ELEMENTS(mCodecs); i++) {
      if (codec.EqualsASCII(ENTRY_FORMAT(mCodecs[i]))) {
        capsString = ENTRY_CAPS(mCodecs[i]);
        break;
      }
    }

    if (!capsString) {
      gst_caps_unref(caps);
      return nullptr;
    }

    GstCaps* tmp = gst_caps_from_string(capsString);
    /* appends and frees tmp */
    gst_caps_append(caps, tmp);
  }

  return caps;
}

bool GStreamerFormatHelper::HaveElementsToProcessCaps(GstCaps* aCaps) {

  GList* factories = GetFactories();

  GList* list;
  /* here aCaps contains [containerCaps, [codecCaps1, [codecCaps2, ...]]] so process
   * caps structures individually as we want one element for _each_
   * structure */
  for (unsigned int i = 0; i < gst_caps_get_size(aCaps); i++) {
    GstStructure* s = gst_caps_get_structure(aCaps, i);
    GstCaps* caps = gst_caps_new_full(gst_structure_copy(s), nullptr);
    list = gst_element_factory_list_filter (factories, caps, GST_PAD_SINK, FALSE);
    gst_caps_unref(caps);
    if (!list) {
      return false;
    }
    g_list_free(list);
  }

  return true;
}

GList* GStreamerFormatHelper::GetFactories() {
  uint32_t cookie = gst_default_registry_get_feature_list_cookie ();
  if (cookie != mCookie) {
    g_list_free(mFactories);
    mFactories = gst_element_factory_list_get_elements
        (GST_ELEMENT_FACTORY_TYPE_DEMUXER | GST_ELEMENT_FACTORY_TYPE_DECODER,
         GST_RANK_MARGINAL);
    mCookie = cookie;
  }

  return mFactories;
}
