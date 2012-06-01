/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/TimeStamp.h"
#include "nsTimeRanges.h"
#include "MediaResource.h"
#include "nsHTMLMediaElement.h"
#include "nsMediaPluginHost.h"
#include "nsXPCOMStrings.h"
#include "nsISeekableStream.h"
#include "pratom.h"
#include "nsMediaPluginReader.h"

#include "MPAPI.h"

using namespace MPAPI;
using namespace mozilla;

static MediaResource *GetResource(Decoder *aDecoder)
{
  return reinterpret_cast<MediaResource *>(aDecoder->mResource);
}

static bool Read(Decoder *aDecoder, char *aBuffer, int64_t aOffset, uint32_t aCount, uint32_t* aBytes)
{
  MediaResource *resource = GetResource(aDecoder);
  if (aOffset != resource->Tell()) {
    nsresult rv = resource->Seek(nsISeekableStream::NS_SEEK_SET, aOffset);
    if (NS_FAILED(rv)) {
      return false;
    }
  }
  nsresult rv = resource->Read(aBuffer, aCount, aBytes);
  if (NS_FAILED(rv)) {
    return false;
  }
  return true;
}

static uint64_t GetLength(Decoder *aDecoder)
{
  return GetResource(aDecoder)->GetLength();
}

static void SetMetaDataReadMode(Decoder *aDecoder)
{
  GetResource(aDecoder)->SetReadMode(nsMediaCacheStream::MODE_METADATA);
}

static void SetPlaybackReadMode(Decoder *aDecoder)
{
  GetResource(aDecoder)->SetReadMode(nsMediaCacheStream::MODE_PLAYBACK);
}

static PluginHost sPluginHost = {
  Read,
  GetLength,
  SetMetaDataReadMode,
  SetPlaybackReadMode
};

void nsMediaPluginHost::TryLoad(const char *name)
{
  PRLibrary *lib = PR_LoadLibrary(name);
  if (lib) {
    Manifest *manifest = static_cast<Manifest *>(PR_FindSymbol(lib, "MPAPI_MANIFEST"));
    if (manifest)
      mPlugins.AppendElement(manifest);
  }
}

nsMediaPluginHost::nsMediaPluginHost() {
  MOZ_COUNT_CTOR(nsMediaPluginHost);
#ifdef ANDROID
  TryLoad("libomxplugin.so");
#endif
}

nsMediaPluginHost::~nsMediaPluginHost() {
  MOZ_COUNT_DTOR(nsMediaPluginHost);
}

bool nsMediaPluginHost::FindDecoder(const nsACString& aMimeType, const char* const** aCodecs)
{
  const char *chars;
  size_t len = NS_CStringGetData(aMimeType, &chars, nsnull);
  for (size_t n = 0; n < mPlugins.Length(); ++n) {
    Manifest *plugin = mPlugins[n];
    const char* const *codecs;
    if (plugin->CanDecode(chars, len, &codecs)) {
      if (aCodecs)
        *aCodecs = codecs;
      return true;
    }
  }
  return false;
}

Decoder::Decoder() :
  mResource(NULL), mPrivate(NULL)
{
}

MPAPI::Decoder *nsMediaPluginHost::CreateDecoder(MediaResource *aResource, const nsACString& aMimeType)
{
  const char *chars;
  size_t len = NS_CStringGetData(aMimeType, &chars, nsnull);

  Decoder *decoder = new Decoder();
  if (!decoder) {
    return nsnull;
  }
  decoder->mResource = aResource;

  for (size_t n = 0; n < mPlugins.Length(); ++n) {
    Manifest *plugin = mPlugins[n];
    const char* const *codecs;
    if (!plugin->CanDecode(chars, len, &codecs)) {
      continue;
    }
    if (plugin->CreateDecoder(&sPluginHost, decoder, chars, len)) {
      return decoder;
    }
  }

  return nsnull;
}

void nsMediaPluginHost::DestroyDecoder(Decoder *aDecoder)
{
  aDecoder->DestroyDecoder(aDecoder);
  delete aDecoder;
}

nsMediaPluginHost *sMediaPluginHost = nsnull;
nsMediaPluginHost *GetMediaPluginHost()
{
  if (!sMediaPluginHost) {
    sMediaPluginHost = new nsMediaPluginHost();
  }
  return sMediaPluginHost;
}

void nsMediaPluginHost::Shutdown()
{
  if (sMediaPluginHost) {
    delete sMediaPluginHost;
    sMediaPluginHost = nsnull;
  }
}


