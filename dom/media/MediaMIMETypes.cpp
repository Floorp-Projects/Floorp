/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaMIMETypes.h"

#include "nsContentTypeParser.h"

namespace mozilla {

template <int N>
static bool
StartsWith(const nsACString& string, const char (&prefix)[N])
{
    if (N - 1 > string.Length()) {
      return false;
    }
    return memcmp(string.Data(), prefix, N - 1) == 0;
}

bool
MediaMIMEType::HasApplicationMajorType() const
{
  return StartsWith(mMIMEType, "application/");
}

bool
MediaMIMEType::HasAudioMajorType() const
{
  return StartsWith(mMIMEType, "audio/");
}

bool
MediaMIMEType::HasVideoMajorType() const
{
  return StartsWith(mMIMEType, "video/");
}

size_t
MediaMIMEType::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  return mMIMEType.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
}

MediaMIMEType::MediaMIMEType(const nsACString& aType)
  : mMIMEType(aType)
{
}

Maybe<MediaMIMEType>
MakeMediaMIMEType(const nsAString& aType)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsContentTypeParser parser(aType);
  nsAutoString mime;
  nsresult rv = parser.GetType(mime);
  if (!NS_SUCCEEDED(rv) || mime.IsEmpty()) {
    return Nothing();
  }

  NS_ConvertUTF16toUTF8 mime8{mime};
  if (!IsMediaMIMEType(mime8)) {
    return Nothing();
  }

  return Some(MediaMIMEType(mime8));
}

Maybe<MediaMIMEType>
MakeMediaMIMEType(const nsACString& aType)
{
  return MakeMediaMIMEType(NS_ConvertUTF8toUTF16(aType));
}

Maybe<MediaMIMEType>
MakeMediaMIMEType(const char* aType)
{
  if (!aType) {
    return Nothing();
  }
  return MakeMediaMIMEType(nsDependentCString(aType));
}

bool
MediaCodecs::Contains(const nsAString& aCodec) const
{
  for (const auto& myCodec : Range()) {
    if (myCodec == aCodec) {
      return true;
    }
  }
  return false;
}

bool
MediaCodecs::ContainsAll(const MediaCodecs& aCodecs) const
{
  const auto& codecsToTest = aCodecs.Range();
  for (const auto& codecToTest : codecsToTest) {
    if (!Contains(codecToTest)) {
      return false;
    }
  }
  return true;
}

size_t
MediaCodecs::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  return mCodecs.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
}


static int32_t
GetParameterAsNumber(const nsContentTypeParser& aParser,
                     const char* aParameter,
                     const int32_t aErrorReturn)
{
  nsAutoString parameterString;
  nsresult rv = aParser.GetParameter(aParameter, parameterString);
  if (NS_FAILED_impl(rv)) {
    return aErrorReturn;
  }
  int32_t number = parameterString.ToInteger(&rv);
  if (MOZ_UNLIKELY(NS_FAILED_impl(rv))) {
    return aErrorReturn;
  }
  return number;
}

MediaExtendedMIMEType::MediaExtendedMIMEType(const nsACString& aMIMEType,
                                             bool aHaveCodecs,
                                             const nsAString& aCodecs,
                                             int32_t aWidth, int32_t aHeight,
                                             int32_t aFramerate, int32_t aBitrate)
  : mMIMEType(aMIMEType)
  , mHaveCodecs(aHaveCodecs)
  , mCodecs(aCodecs)
  , mWidth(aWidth)
  , mHeight(aHeight)
  , mFramerate(aFramerate)
  , mBitrate(aBitrate)
{
}

MediaExtendedMIMEType::MediaExtendedMIMEType(const MediaMIMEType& aType)
  : mMIMEType(aType)
{
}

MediaExtendedMIMEType::MediaExtendedMIMEType(MediaMIMEType&& aType)
  : mMIMEType(Move(aType))
{
}

Maybe<MediaExtendedMIMEType>
MakeMediaExtendedMIMEType(const nsAString& aType)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsContentTypeParser parser(aType);
  nsAutoString mime;
  nsresult rv = parser.GetType(mime);
  if (!NS_SUCCEEDED(rv) || mime.IsEmpty()) {
    return Nothing();
  }

  NS_ConvertUTF16toUTF8 mime8{mime};
  if (!IsMediaMIMEType(mime8)) {
    return Nothing();
  }

  nsAutoString codecs;
  rv = parser.GetParameter("codecs", codecs);
  bool haveCodecs = NS_SUCCEEDED(rv);

  int32_t width = GetParameterAsNumber(parser, "width", -1);
  int32_t height = GetParameterAsNumber(parser, "height", -1);
  int32_t framerate = GetParameterAsNumber(parser, "framerate", -1);
  int32_t bitrate = GetParameterAsNumber(parser, "bitrate", -1);

  return Some(MediaExtendedMIMEType(mime8,
                                    haveCodecs, codecs,
                                    width, height,
                                    framerate, bitrate));
}

size_t
MediaExtendedMIMEType::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  return mMIMEType.SizeOfExcludingThis(aMallocSizeOf)
         + mCodecs.SizeOfExcludingThis(aMallocSizeOf);
}

Maybe<MediaExtendedMIMEType>
MakeMediaExtendedMIMEType(const nsACString& aType)
{
  return MakeMediaExtendedMIMEType(NS_ConvertUTF8toUTF16(aType));
}

Maybe<MediaExtendedMIMEType>
MakeMediaExtendedMIMEType(const char* aType)
{
  if (!aType) {
    return Nothing();
  }
  return MakeMediaExtendedMIMEType(nsDependentCString(aType));
}

} // namespace mozilla
