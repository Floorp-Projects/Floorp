/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MP4_STREAM_H_
#define MP4_STREAM_H_

#include "mp4_demuxer/Stream.h"

#include "MediaResource.h"

namespace mozilla {

class MP4Stream : public mp4_demuxer::Stream {
public:
  explicit MP4Stream(MediaResource* aResource);
  virtual ~MP4Stream();
  bool ReadAt(int64_t aOffset, void* aBuffer, size_t aCount,
              size_t* aBytesRead) override;
  bool CachedReadAt(int64_t aOffset, void* aBuffer, size_t aCount,
                    size_t* aBytesRead) override;
  bool Length(int64_t* aSize) override;

private:
  MediaResourceIndex mResource;
};

} // namespace mozilla

#endif
