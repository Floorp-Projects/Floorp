/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(GStreamerFormatHelper_h_)
#define GStreamerFormatHelper_h_

#include <gst/gst.h>
#include <mozilla/Types.h>
#include "nsXPCOMStrings.h"

namespace mozilla {

class GStreamerFormatHelper {
  /* This class can be used to query the GStreamer registry for the required
   * demuxers/decoders from nsHTMLMediaElement::CanPlayType.
   * It implements looking at the GstRegistry to check if elements to
   * demux/decode the formats passed to CanPlayType() are actually installed.
   */
  public:
    static GStreamerFormatHelper* Instance();
    ~GStreamerFormatHelper();

    bool CanHandleMediaType(const nsACString& aMIMEType,
                            const nsAString* aCodecs);

    bool CanHandleContainerCaps(GstCaps* aCaps);
    bool CanHandleCodecCaps(GstCaps* aCaps);

   static void Shutdown();

  private:
    GStreamerFormatHelper();
    GstCaps* ConvertFormatsToCaps(const char* aMIMEType,
                                  const nsAString* aCodecs);
    char* const *CodecListFromCaps(GstCaps* aCaps);
    bool HaveElementsToProcessCaps(GstCaps* aCaps);
    GList* GetFactories();

    static GStreamerFormatHelper* gInstance;

    /* table to convert from container MIME types to GStreamer caps */
    static char const *const mContainers[6][2];

    /* table to convert from codec MIME types to GStreamer caps */
    static char const *const mCodecs[9][2];

    /*
     * True iff we were able to find the proper GStreamer libs and the functions
     * we need.
     */
    static bool sLoadOK;

    /* whitelist of supported container/codec gst caps */
    GstCaps* mSupportedContainerCaps;
    GstCaps* mSupportedCodecCaps;

    /* list of GStreamer element factories
     * Element factories are the basic types retrieved from the GStreamer
     * registry, they describe all plugins and elements that GStreamer can
     * create.
     * This means that element factories are useful for automated element
     * instancing, such as what autopluggers do,
     * and for creating lists of available elements. */
    GList* mFactories;

    /* Storage for the default registrys feature list cookie.
     * It changes every time a feature is added to or removed from the
     * GStreamer registry. */
    uint32_t mCookie;
};

} //namespace mozilla

#endif
