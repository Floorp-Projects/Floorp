/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(nsMediaPluginHost_h_)
#define nsMediaPluginHost_h_

#include "nsTArray.h"
#include "MediaResource.h"
#include "MPAPI.h"

class nsMediaPluginReader;

class nsMediaPluginHost {
  nsTArray<MPAPI::Manifest *> mPlugins;
  MPAPI::Manifest *FindPlugin(const nsACString& aMimeType);
  void TryLoad(const char *name);
public:
  nsMediaPluginHost();
  ~nsMediaPluginHost();

  static void Shutdown();

  bool FindDecoder(const nsACString& aMimeType, const char* const** aCodecs);
  MPAPI::Decoder *CreateDecoder(mozilla::MediaResource *aResource, const nsACString& aMimeType);
  void DestroyDecoder(MPAPI::Decoder *aDecoder);
};

nsMediaPluginHost *GetMediaPluginHost();

#endif
