/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaContainerType_h_
#define MediaContainerType_h_

#include "MediaMIMETypes.h"
#include "mozilla/Maybe.h"
#include "nsString.h"

namespace mozilla {

// Class containing media type information for containers.
class MediaContainerType
{
public:
  explicit MediaContainerType(const MediaMIMEType& aType)
    : mExtendedMIMEType(aType)
  {}
  explicit MediaContainerType(MediaMIMEType&& aType)
    : mExtendedMIMEType(std::move(aType))
  {}
  explicit MediaContainerType(const MediaExtendedMIMEType& aType)
    : mExtendedMIMEType(aType)
  {
  }
  explicit MediaContainerType(MediaExtendedMIMEType&& aType)
    : mExtendedMIMEType(std::move(aType))
  {
  }

  const MediaMIMEType& Type() const { return mExtendedMIMEType.Type(); }
  const MediaExtendedMIMEType& ExtendedType() const { return mExtendedMIMEType; }

  // Original string. Note that "type/subtype" may not be lowercase,
  // use Type().AsString() instead to get the normalized "type/subtype".
  const nsCString& OriginalString() const { return mExtendedMIMEType.OriginalString(); }

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  MediaExtendedMIMEType mExtendedMIMEType;
};

Maybe<MediaContainerType> MakeMediaContainerType(const nsAString& aType);
Maybe<MediaContainerType> MakeMediaContainerType(const nsACString& aType);
Maybe<MediaContainerType> MakeMediaContainerType(const char* aType);

} // namespace mozilla

#endif // MediaContainerType_h_
