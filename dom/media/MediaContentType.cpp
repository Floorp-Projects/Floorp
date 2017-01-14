/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaContentType.h"

#include "nsContentTypeParser.h"

namespace mozilla {

size_t
MediaContentType::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  return mExtendedMIMEType.SizeOfExcludingThis(aMallocSizeOf);
}

Maybe<MediaContentType>
MakeMediaContentType(const nsAString& aType)
{
  Maybe<MediaExtendedMIMEType> mime = MakeMediaExtendedMIMEType(aType);
  if (mime) {
    return Some(MediaContentType(Move(*mime)));
  }
  return Nothing();
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
