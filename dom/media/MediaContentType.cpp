/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaContentType.h"

#include "nsContentTypeParser.h"

namespace mozilla {

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

bool
MediaContentType::Populate(const nsAString& aType)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsContentTypeParser parser(aType);
  nsAutoString mime;
  nsresult rv = parser.GetType(mime);
  if (!NS_SUCCEEDED(rv) || mime.IsEmpty()) {
    return false;
  }

  mMIMEType = NS_ConvertUTF16toUTF8(mime);

  rv = parser.GetParameter("codecs", mCodecs);
  mHaveCodecs = NS_SUCCEEDED(rv);

  mWidth = GetParameterAsNumber(parser, "width", -1);
  mHeight = GetParameterAsNumber(parser, "height", -1);
  mFramerate = GetParameterAsNumber(parser, "framerate", -1);
  mBitrate = GetParameterAsNumber(parser, "bitrate", -1);

  return true;
}

Maybe<MediaContentType>
MakeMediaContentType(const nsAString& aType)
{
  Maybe<MediaContentType> type{Some(MediaContentType{})};
  if (!type->Populate(aType)) {
    type.reset();
  }
  return type;
}

Maybe<MediaContentType>
MakeMediaContentType(const nsACString& aType)
{
  return MakeMediaContentType(NS_ConvertUTF8toUTF16(aType));
}

Maybe<MediaContentType>
MakeMediaContentType(const char* aType)
{
  if (!aType) {
    return Nothing();
  }
  return MakeMediaContentType(nsDependentCString(aType));
}

} // namespace mozilla
