/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaContentType.h"

#include "nsContentTypeParser.h"

namespace mozilla {

MediaContentType::MediaContentType(const nsAString& aType)
{
  Populate(aType);
}

MediaContentType::MediaContentType(const nsACString& aType)
{
  NS_ConvertUTF8toUTF16 typeUTF16(aType);
  Populate(typeUTF16);
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

void
MediaContentType::Populate(const nsAString& aType)
{
  nsContentTypeParser parser(aType);
  nsAutoString mime;
  nsresult rv = parser.GetType(mime);
  if (NS_SUCCEEDED(rv)) {
    mMIMEType = NS_ConvertUTF16toUTF8(mime);
  }

  rv = parser.GetParameter("codecs", mCodecs);
  mHaveCodecs = NS_SUCCEEDED(rv);

  mWidth = GetParameterAsNumber(parser, "width", -1);
  mHeight = GetParameterAsNumber(parser, "height", -1);
  mFramerate = GetParameterAsNumber(parser, "framerate", -1);
  mBitrate = GetParameterAsNumber(parser, "bitrate", -1);
}

} // namespace mozilla
