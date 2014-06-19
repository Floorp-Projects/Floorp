/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMF.h"
#include "WMFDecoder.h"
#include "WMFReader.h"
#include "WMFUtils.h"
#include "MediaDecoderStateMachine.h"
#include "mozilla/Preferences.h"
#include "mozilla/WindowsVersion.h"
#include "nsCharSeparatedTokenizer.h"

#ifdef MOZ_DIRECTSHOW
#include "DirectShowDecoder.h"
#endif

namespace mozilla {

MediaDecoderStateMachine* WMFDecoder::CreateStateMachine()
{
  return new MediaDecoderStateMachine(this, new WMFReader(this));
}

/* static */
bool
WMFDecoder::IsMP3Supported()
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");
  if (!MediaDecoder::IsWMFEnabled()) {
    return false;
  }
  // MP3 works fine in WMF on Windows Vista and Windows 8.
  if (!IsWin7OrLater()) {
    return true;
  }
  // MP3 support is disabled if we're on Windows 7 and no service packs are
  // installed, since it's crashy. Win7 with service packs is not so crashy,
  // so we enable it there. Note we prefer DirectShow for MP3 playback, but
  // we still support MP3 in MP4 via WMF, or MP3 in WMF if DirectShow is
  // disabled.
  return IsWin7SP1OrLater();
}

static bool
IsSupportedH264Codec(const nsAString& aCodec)
{
  // According to the WMF documentation:
  // http://msdn.microsoft.com/en-us/library/windows/desktop/dd797815%28v=vs.85%29.aspx
  // "The Media Foundation H.264 video decoder is a Media Foundation Transform
  // that supports decoding of Baseline, Main, and High profiles, up to level
  // 5.1.". We also report that we can play Extended profile, as there are
  // bitstreams that are Extended compliant that are also Baseline compliant.

  // H.264 codecs parameters have a type defined as avc1.PPCCLL, where
  // PP = profile_idc, CC = constraint_set flags, LL = level_idc.
  // We ignore the constraint_set flags, as it's not clear from the WMF
  // documentation what constraints the WMF H.264 decoder supports.
  // See http://blog.pearce.org.nz/2013/11/what-does-h264avc1-codecs-parameters.html
  // for more details.
  if (aCodec.Length() != strlen("avc1.PPCCLL")) {
    return false;
  }

  // Verify the codec starts with "avc1.".
  const nsAString& sample = Substring(aCodec, 0, 5);
  if (!sample.EqualsASCII("avc1.")) {
    return false;
  }

  // Extract the profile_idc and level_idc. Note: the constraint_set flags
  // are ignored, it's not clear from the WMF documentation if they make a
  // difference.
  nsresult rv = NS_OK;
  const int32_t profile = PromiseFlatString(Substring(aCodec, 5, 2)).ToInteger(&rv, 16);
  NS_ENSURE_SUCCESS(rv, false);

  const int32_t level = PromiseFlatString(Substring(aCodec, 9, 2)).ToInteger(&rv, 16);
  NS_ENSURE_SUCCESS(rv, false);

  return level >= eAVEncH264VLevel1 &&
         level <= eAVEncH264VLevel5_1 &&
         (profile == eAVEncH264VProfile_Base ||
          profile == eAVEncH264VProfile_Main ||
          profile == eAVEncH264VProfile_Extended ||
          profile == eAVEncH264VProfile_High);
}

bool
WMFDecoder::CanPlayType(const nsACString& aType,
                        const nsAString& aCodecs)
{
  if (!MediaDecoder::IsWMFEnabled() ||
      NS_FAILED(LoadDLLs())) {
    return false;
  }

  // Assume that if LoadDLLs() didn't fail, we can playback the types that
  // we know should be supported by Windows Media Foundation.
  if ((aType.EqualsASCII("audio/mpeg") || aType.EqualsASCII("audio/mp3")) &&
      IsMP3Supported()) {
    // Note: We block MP3 playback on Window 7 SP0 since it seems to crash
    // in some circumstances.
    return !aCodecs.Length() || aCodecs.EqualsASCII("mp3");
  }

  // AAC-LC or MP3 in M4A.
  if (aType.EqualsASCII("audio/mp4") || aType.EqualsASCII("audio/x-m4a")) {
    return !aCodecs.Length() ||
           aCodecs.EqualsASCII("mp4a.40.2") ||
           aCodecs.EqualsASCII("mp3");
  }

  if (!aType.EqualsASCII("video/mp4")) {
    return false;
  }

  // H.264 + AAC in MP4. Verify that all the codecs specifed are ones that
  // we expect that we can play.
  nsCharSeparatedTokenizer tokenizer(aCodecs, ',');
  bool expectMoreTokens = false;
  while (tokenizer.hasMoreTokens()) {
    const nsSubstring& token = tokenizer.nextToken();
    expectMoreTokens = tokenizer.separatorAfterCurrentToken();
    if (token.EqualsASCII("mp4a.40.2") || // AAC-LC
        token.EqualsASCII("mp3") ||
        IsSupportedH264Codec(token)) {
      continue;
    }
    return false;
  }
  if (expectMoreTokens) {
    // Last codec name was empty
    return false;
  }
  return true;
}

nsresult
WMFDecoder::LoadDLLs()
{
  return SUCCEEDED(wmf::LoadDLLs()) ? NS_OK : NS_ERROR_FAILURE;
}

void
WMFDecoder::UnloadDLLs()
{
  wmf::UnloadDLLs();
}

/* static */
bool
WMFDecoder::IsEnabled()
{
  // We only use WMF on Windows Vista and up
  return IsVistaOrLater() &&
         Preferences::GetBool("media.windows-media-foundation.enabled");
}

} // namespace mozilla

