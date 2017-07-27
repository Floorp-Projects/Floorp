/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MP4Stream.h"

namespace mozilla {

MP4Stream::MP4Stream(MediaResource* aResource)
  : mResource(aResource)
{
  MOZ_COUNT_CTOR(MP4Stream);
  MOZ_ASSERT(aResource);
}

MP4Stream::~MP4Stream()
{
  MOZ_COUNT_DTOR(MP4Stream);
}

bool
MP4Stream::ReadAt(int64_t aOffset, void* aBuffer, size_t aCount,
                  size_t* aBytesRead)
{
  return CachedReadAt(aOffset, aBuffer, aCount, aBytesRead);
}

bool
MP4Stream::CachedReadAt(int64_t aOffset, void* aBuffer, size_t aCount,
                        size_t* aBytesRead)
{
  nsresult rv =
    mResource.GetResource()->ReadFromCache(reinterpret_cast<char*>(aBuffer),
                                           aOffset, aCount);
  if (NS_FAILED(rv)) {
    *aBytesRead = 0;
    return false;
  }
  *aBytesRead = aCount;
  return true;
}

bool
MP4Stream::Length(int64_t* aSize)
{
  if (mResource.GetLength() < 0)
    return false;
  *aSize = mResource.GetLength();
  return true;
}

} // namespace mozilla
