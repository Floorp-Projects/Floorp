/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GStreamerFormatHelper.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsXPCOMStrings.h"
#include "GStreamerLoader.h"

#define ENTRY_FORMAT(entry) entry[0]
#define ENTRY_CAPS(entry) entry[1]

namespace mozilla {

GStreamerFormatHelper* GStreamerFormatHelper::gInstance = nullptr;
bool GStreamerFormatHelper::sLoadOK = false;

GStreamerFormatHelper* GStreamerFormatHelper::Instance() {
  if (!gInstance) {
    if ((sLoadOK = load_gstreamer())) {
      gst_init(nullptr, nullptr);
    }

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
  {"audio/mp4", "audio/x-m4a"},
  {"audio/x-m4a", "audio/x-m4a"},
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
  if (!sLoadOK) {
    return;
  }

  mSupportedContainerCaps = gst_caps_new_empty();
  for (unsigned int i = 0; i < G_N_ELEMENTS(mContainers); i++) {
    const char* capsString = mContainers[i][1];
    GstCaps* caps = gst_caps_from_string(capsString);
    gst_caps_append(mSupportedContainerCaps, caps);
  }

  mSupportedCodecCaps = gst_caps_new_empty();
  for (unsigned int i = 0; i < G_N_ELEMENTS(mCodecs); i++) {
    const char* capsString = mCodecs[i][1];
    GstCaps* caps = gst_caps_from_string(capsString);
    gst_caps_append(mSupportedCodecCaps, caps);
  }
}

GStreamerFormatHelper::~GStreamerFormatHelper() {
  if (!sLoadOK) {
    return;
  }

  gst_caps_unref(mSupportedContainerCaps);
  gst_caps_unref(mSupportedCodecCaps);

  if (mFactories)
    g_list_free(mFactories);
}

bool GStreamerFormatHelper::CanHandleMediaType(const nsACString& aMIMEType,
                                               const nsAString* aCodecs) {
  if (!sLoadOK) {
    return false;
  }

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
  NS_ASSERTION(sLoadOK, "GStreamer library not linked");

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

static gboolean FactoryFilter(GstPluginFeature *aFeature, gpointer)
{
  if (!GST_IS_ELEMENT_FACTORY(aFeature)) {
    return FALSE;
  }

  // TODO _get_klass doesn't exist in 1.0
  const gchar *className =
    gst_element_factory_get_klass(GST_ELEMENT_FACTORY_CAST(aFeature));

  if (!strstr(className, "Decoder") && !strstr(className, "Demux")) {
    return FALSE;
  }

  return gst_plugin_feature_get_rank(aFeature) >= GST_RANK_MARGINAL;
}

/**
 * Returns true if any |aFactory| caps intersect with |aCaps|
 */
static bool SupportsCaps(GstElementFactory *aFactory, GstCaps *aCaps)
{
  for (const GList *iter = gst_element_factory_get_static_pad_templates(aFactory); iter; iter = iter->next) {
    GstStaticPadTemplate *templ = static_cast<GstStaticPadTemplate *>(iter->data);

    if (templ->direction == GST_PAD_SRC) {
      continue;
    }

    GstCaps *caps = gst_static_caps_get(&templ->static_caps);
    if (!caps) {
      continue;
    }

    if (gst_caps_can_intersect(gst_static_caps_get(&templ->static_caps), aCaps)) {
      return true;
    }
  }

  return false;
}

bool GStreamerFormatHelper::HaveElementsToProcessCaps(GstCaps* aCaps) {
  NS_ASSERTION(sLoadOK, "GStreamer library not linked");

  GList* factories = GetFactories();

  /* here aCaps contains [containerCaps, [codecCaps1, [codecCaps2, ...]]] so process
   * caps structures individually as we want one element for _each_
   * structure */
  for (unsigned int i = 0; i < gst_caps_get_size(aCaps); i++) {
    GstStructure* s = gst_caps_get_structure(aCaps, i);
    GstCaps* caps = gst_caps_new_full(gst_structure_copy(s), nullptr);

    bool found = false;
    for (GList *elem = factories; elem; elem = elem->next) {
      if (SupportsCaps(GST_ELEMENT_FACTORY_CAST(elem->data), caps)) {
        found = true;
        break;
      }
    }

    if (!found) {
      return false;
    }

    gst_caps_unref(caps);
  }

  return true;
}

bool GStreamerFormatHelper::CanHandleContainerCaps(GstCaps* aCaps)
{
  NS_ASSERTION(sLoadOK, "GStreamer library not linked");

  return gst_caps_can_intersect(aCaps, mSupportedContainerCaps);
}

bool GStreamerFormatHelper::CanHandleCodecCaps(GstCaps* aCaps)
{
  NS_ASSERTION(sLoadOK, "GStreamer library not linked");

  return gst_caps_can_intersect(aCaps, mSupportedCodecCaps);
}

GList* GStreamerFormatHelper::GetFactories() {
  NS_ASSERTION(sLoadOK, "GStreamer library not linked");

  uint32_t cookie = gst_default_registry_get_feature_list_cookie ();
  if (cookie != mCookie) {
    g_list_free(mFactories);
    mFactories =
      gst_default_registry_feature_filter((GstPluginFeatureFilter)FactoryFilter,
                                          false, nullptr);
    mCookie = cookie;
  }

  return mFactories;
}

} // namespace mozilla
