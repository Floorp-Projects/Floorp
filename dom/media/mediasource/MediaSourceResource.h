/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_MEDIASOURCERESOURCE_H_
#define MOZILLA_MEDIASOURCERESOURCE_H_

#include "mozilla/Monitor.h"

namespace mozilla {

class MediaSourceResource final
{
public:
  MediaSourceResource()
    : mMonitor("MediaSourceResource")
    , mEnded(false)
  {}

  void SetEnded(bool aEnded)
  {
    MonitorAutoLock mon(mMonitor);
    mEnded = aEnded;
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
  {
    // TODO: track source buffers appended to MediaSource.
    return 0;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

private:
  Monitor mMonitor;
  bool mEnded; // protected by mMonitor
};

} // namespace mozilla

#endif /* MOZILLA_MEDIASOURCERESOURCE_H_ */
