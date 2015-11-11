/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GStreamerFormatHelper.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsString.h"
#include "GStreamerLoader.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"

#define ENTRY_FORMAT(entry) entry[0]
#define ENTRY_CAPS(entry) entry[1]

namespace mozilla {

extern LazyLogModule gMediaDecoderLog;
#define LOG(msg, ...) \
    MOZ_LOG(gMediaDecoderLog, LogLevel::Debug, ("GStreamerFormatHelper " msg, ##__VA_ARGS__))

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
  delete gInstance;
  gInstance = nullptr;
}

static char const *const sContainers[][2] = {
  {"video/mp4", "video/quicktime"},
  {"video/x-m4v", "video/quicktime"},
  {"video/quicktime", "video/quicktime"},
  {"audio/mp4", "audio/x-m4a"},
  {"audio/x-m4a", "audio/x-m4a"},
  {"audio/mpeg", "audio/mpeg, mpegversion=(int)1"},
  {"audio/mp3", "audio/mpeg, mpegversion=(int)1"},
};

static char const *const sCodecs[9][2] = {
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

static char const * const sDefaultCodecCaps[][2] = {
  {"video/mp4", "video/x-h264"},
  {"video/x-m4v", "video/x-h264"},
  {"video/quicktime", "video/x-h264"},
  {"audio/mp4", "audio/mpeg, mpegversion=(int)4"},
  {"audio/x-m4a", "audio/mpeg, mpegversion=(int)4"},
  {"audio/mp3", "audio/mpeg, layer=(int)3"},
  {"audio/mpeg", "audio/mpeg, layer=(int)3"}
};

static char const * const sPluginBlockList[] = {
  "flump3dec",
  "h264parse",
};

GStreamerFormatHelper::GStreamerFormatHelper()
  : mFactories(nullptr),
    mCookie(static_cast<uint32_t>(-1))
{
  if (!sLoadOK) {
    return;
  }

  mSupportedContainerCaps = gst_caps_new_empty();
  for (unsigned int i = 0; i < G_N_ELEMENTS(sContainers); i++) {
    const char* capsString = sContainers[i][1];
    GstCaps* caps = gst_caps_from_string(capsString);
    gst_caps_append(mSupportedContainerCaps, caps);
  }

  mSupportedCodecCaps = gst_caps_new_empty();
  for (unsigned int i = 0; i < G_N_ELEMENTS(sCodecs); i++) {
    const char* capsString = sCodecs[i][1];
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

static GstCaps *
GetContainerCapsFromMIMEType(const char *aType) {
  /* convert aMIMEType to gst container caps */
  const char* capsString = nullptr;
  for (uint32_t i = 0; i < G_N_ELEMENTS(sContainers); i++) {
    if (!strcmp(ENTRY_FORMAT(sContainers[i]), aType)) {
      capsString = ENTRY_CAPS(sContainers[i]);
      break;
    }
  }

  if (!capsString) {
    /* we couldn't find any matching caps */
    return nullptr;
  }

  return gst_caps_from_string(capsString);
}

static GstCaps *
GetDefaultCapsFromMIMEType(const char *aType) {
  GstCaps *caps = GetContainerCapsFromMIMEType(aType);

  for (uint32_t i = 0; i < G_N_ELEMENTS(sDefaultCodecCaps); i++) {
    if (!strcmp(sDefaultCodecCaps[i][0], aType)) {
      GstCaps *tmp = gst_caps_from_string(sDefaultCodecCaps[i][1]);

      gst_caps_append(caps, tmp);
      return caps;
    }
  }

  return nullptr;
}

bool GStreamerFormatHelper::CanHandleMediaType(const nsACString& aMIMEType,
                                               const nsAString* aCodecs) {
  if (!sLoadOK) {
    return false;
  }

  const char *type;
  NS_CStringGetData(aMIMEType, &type, nullptr);

  GstCaps *caps;
  if (aCodecs && !aCodecs->IsEmpty()) {
    caps = ConvertFormatsToCaps(type, aCodecs);
  } else {
    // Get a minimal set of codec caps for this MIME type we should support so
    // that we don't overreport MIME types we are able to play.
    caps = GetDefaultCapsFromMIMEType(type);
  }

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

  GstCaps *caps = GetContainerCapsFromMIMEType(aMIMEType);
  if (!caps) {
    return nullptr;
  }

  /* container only */
  if (!aCodecs) {
    return caps;
  }

  nsCharSeparatedTokenizer tokenizer(*aCodecs, ',');
  while (tokenizer.hasMoreTokens()) {
    const nsSubstring& codec = tokenizer.nextToken();
    const char *capsString = nullptr;

    for (i = 0; i < G_N_ELEMENTS(sCodecs); i++) {
      if (codec.EqualsASCII(ENTRY_FORMAT(sCodecs[i]))) {
        capsString = ENTRY_CAPS(sCodecs[i]);
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

/* static */ bool
GStreamerFormatHelper::IsBlockListEnabled()
{
  static bool sBlockListEnabled;
  static bool sBlockListEnabledCached = false;

  if (!sBlockListEnabledCached) {
    Preferences::AddBoolVarCache(&sBlockListEnabled,
                                 "media.gstreamer.enable-blacklist", true);
    sBlockListEnabledCached = true;
  }

  return sBlockListEnabled;
}

/* static */ bool
GStreamerFormatHelper::IsPluginFeatureBlocked(GstPluginFeature *aFeature)
{
  if (!IsBlockListEnabled()) {
    return false;
  }

  const gchar *factoryName =
    gst_plugin_feature_get_name(aFeature);

  for (unsigned int i = 0; i < G_N_ELEMENTS(sPluginBlockList); i++) {
    if (!strcmp(factoryName, sPluginBlockList[i])) {
      LOG("rejecting disabled plugin %s", factoryName);
      return true;
    }
  }

  return false;
}

static gboolean FactoryFilter(GstPluginFeature *aFeature, gpointer)
{
  if (!GST_IS_ELEMENT_FACTORY(aFeature)) {
    return FALSE;
  }

  const gchar *className =
    gst_element_factory_get_klass(GST_ELEMENT_FACTORY_CAST(aFeature));

  // NB: We skip filtering parsers here, because adding them to
  // the list can give false decoder positives to canPlayType().
  if (!strstr(className, "Decoder") && !strstr(className, "Demux")) {
    return FALSE;
  }

  return
    gst_plugin_feature_get_rank(aFeature) >= GST_RANK_MARGINAL &&
    !GStreamerFormatHelper::IsPluginFeatureBlocked(aFeature);
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

    bool supported = gst_caps_can_intersect(caps, aCaps);

    gst_caps_unref(caps);

    if (supported) {
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

    gst_caps_unref(caps);

    if (!found) {
      return false;
    }
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

#if GST_VERSION_MAJOR >= 1
  uint32_t cookie = gst_registry_get_feature_list_cookie(gst_registry_get());
#else
  uint32_t cookie = gst_default_registry_get_feature_list_cookie();
#endif
  if (cookie != mCookie) {
    g_list_free(mFactories);
#if GST_VERSION_MAJOR >= 1
    mFactories =
      gst_registry_feature_filter(gst_registry_get(),
                                  (GstPluginFeatureFilter)FactoryFilter,
                                  false, nullptr);
#else
    mFactories =
      gst_default_registry_feature_filter((GstPluginFeatureFilter)FactoryFilter,
                                          false, nullptr);
#endif
    mCookie = cookie;
  }

  return mFactories;
}

} // namespace mozilla
