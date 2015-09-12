/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_OTMTMarkerObserver_h_
#define mozilla_OTMTMarkerObserver_h_

#include "mozilla/Mutex.h"

namespace mozilla {
class AbstractTimelineMarker;

class OTMTMarkerReceiver
{
private:
  OTMTMarkerReceiver() = delete;
  OTMTMarkerReceiver(const OTMTMarkerReceiver& aOther) = delete;
  void operator=(const OTMTMarkerReceiver& aOther) = delete;

public:
  explicit OTMTMarkerReceiver(const char* aMutexName)
    : mLock(aMutexName)
  {
  }

  virtual ~OTMTMarkerReceiver() {}
  virtual void AddOTMTMarkerClone(UniquePtr<AbstractTimelineMarker>& aMarker) = 0;

protected:
  Mutex& GetLock() { return mLock; };

private:
  Mutex mLock;
};

} // namespace mozilla

#endif /* mozilla_OTMTMarkerObserver_h_ */
