/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamSource.h"

#include <utils/threads.h>

#include "nsISeekableStream.h"

namespace android {

MediaStreamSource::MediaStreamSource(MediaResource *aResource)
  : mResource(aResource)
{
}

MediaStreamSource::~MediaStreamSource()
{
}

status_t MediaStreamSource::initCheck() const
{
  return OK;
}

ssize_t MediaStreamSource::readAt(off64_t offset, void *data, size_t size)
{
  char *ptr = static_cast<char *>(data);
  size_t todo = size;
  while (todo > 0) {
    Mutex::Autolock autoLock(mLock);
    uint32_t bytesRead;
    if ((offset != mResource->Tell() &&
         NS_FAILED(mResource->Seek(nsISeekableStream::NS_SEEK_SET, offset))) ||
        NS_FAILED(mResource->Read(ptr, todo, &bytesRead))) {
      return ERROR_IO;
    }

    if (bytesRead == 0) {
      return size - todo;
    }

    offset += bytesRead;
    todo -= bytesRead;
    ptr += bytesRead;
  }
  return size;
}

status_t MediaStreamSource::getSize(off64_t *size)
{
  uint64_t length = mResource->GetLength();
  if (length == static_cast<uint64_t>(-1))
    return ERROR_UNSUPPORTED;

  *size = length;

  return OK;
}

} // namespace android
