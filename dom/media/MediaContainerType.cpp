/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaContainerType.h"

namespace mozilla {

size_t
MediaContainerType::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  return mExtendedMIMEType.SizeOfExcludingThis(aMallocSizeOf);
}

Maybe<MediaContainerType>
MakeMediaContainerType(const nsAString& aType)
{
  Maybe<MediaExtendedMIMEType> mime = MakeMediaExtendedMIMEType(aType);
  if (mime) {
    return Some(MediaContainerType(std::move(*mime)));
  }
  return Nothing();
}

Maybe<MediaContainerType>
MakeMediaContainerType(const nsACString& aType)
{
  return MakeMediaContainerType(NS_ConvertUTF8toUTF16(aType));
}

Maybe<MediaContainerType>
MakeMediaContainerType(const char* aType)
{
  if (!aType) {
    return Nothing();
  }
  return MakeMediaContainerType(nsDependentCString(aType));
}

} // namespace mozilla
