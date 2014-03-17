/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(MediaPluginHost_h_)
#define MediaPluginHost_h_

#include "nsTArray.h"
#include "MediaResource.h"
#include "MPAPI.h"
#include "MediaResourceServer.h"

namespace mozilla {

class MediaPluginReader;

class MediaPluginHost {
  nsRefPtr<MediaResourceServer> mResourceServer;
  nsTArray<MPAPI::Manifest *> mPlugins;

  MPAPI::Manifest *FindPlugin(const nsACString& aMimeType);
public:
  MediaPluginHost();
  ~MediaPluginHost();

  static void Shutdown();

  bool FindDecoder(const nsACString& aMimeType, const char* const** aCodecs);
  MPAPI::Decoder *CreateDecoder(mozilla::MediaResource *aResource, const nsACString& aMimeType);
  void DestroyDecoder(MPAPI::Decoder *aDecoder);
};

MediaPluginHost *GetMediaPluginHost();

} // namespace mozilla

#endif
