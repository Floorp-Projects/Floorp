/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaContentType_h_
#define MediaContentType_h_

#include "MediaMIMETypes.h"
#include "mozilla/Maybe.h"
#include "nsString.h"

namespace mozilla {

// Class containing media type information for containers.
class MediaContentType
{
public:
  explicit MediaContentType(const MediaMIMEType& aType)
    : mExtendedMIMEType(aType)
  {}
  explicit MediaContentType(MediaMIMEType&& aType)
    : mExtendedMIMEType(Move(aType))
  {}
  explicit MediaContentType(const MediaExtendedMIMEType& aType)
    : mExtendedMIMEType(aType)
  {
  }
  explicit MediaContentType(MediaExtendedMIMEType&& aType)
    : mExtendedMIMEType(Move(aType))
  {
  }

  const MediaMIMEType& Type() const { return mExtendedMIMEType.Type(); }
  const MediaExtendedMIMEType& ExtendedType() const { return mExtendedMIMEType; }

  // Original string. Note that "type/subtype" may not be lowercase,
  // use Type().AsString() instead to get the normalized "type/subtype".
  const nsACString& OriginalString() const { return mExtendedMIMEType.OriginalString(); }

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  MediaExtendedMIMEType mExtendedMIMEType;
};

Maybe<MediaContentType> MakeMediaContentType(const nsAString& aType);
Maybe<MediaContentType> MakeMediaContentType(const nsACString& aType);
Maybe<MediaContentType> MakeMediaContentType(const char* aType);

} // namespace mozilla

#endif // MediaContentType_h_
