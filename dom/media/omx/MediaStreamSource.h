/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIA_STREAM_SOURCE_H
#define MEDIA_STREAM_SOURCE_H

#include <stdint.h>

#include <stagefright/DataSource.h>
#include <stagefright/MediaSource.h>

#include "MediaResource.h"
#include "nsAutoPtr.h"

namespace android {

// MediaStreamSource is a DataSource that reads from a MPAPI media stream.
class MediaStreamSource : public DataSource {
  typedef mozilla::MediaResource MediaResource;
  typedef mozilla::MediaResourceIndex MediaResourceIndex;

  Mutex mLock;
  MediaResourceIndex mResource;
public:
  MediaStreamSource(MediaResource* aResource);

  status_t initCheck() const override;
  ssize_t readAt(off64_t offset, void *data, size_t size) override;
  status_t getSize(off64_t *size) override;
  uint32_t flags() override {
    return kWantsPrefetching;
  }

  int64_t Tell();

  // Apparently unused.
  virtual ssize_t readAt(off_t offset, void *data, size_t size) {
    return readAt(static_cast<off64_t>(offset), data, size);
  }
  virtual status_t getSize(off_t *size) {
    off64_t size64;
    status_t status = getSize(&size64);
    *size = size64;
    return status;
  }

  virtual ~MediaStreamSource();

private:
  MediaStreamSource(const MediaStreamSource &);
  MediaStreamSource &operator=(const MediaStreamSource &);
};

} // namespace android

#endif // MEDIA_STREAM_SOURCE_H
