/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_MarkersStorage_h_
#define mozilla_MarkersStorage_h_

#include "TimelineMarkerEnums.h" // for MarkerReleaseRequest
#include "mozilla/Mutex.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/LinkedList.h"
#include "nsTArray.h"

namespace mozilla {
class AbstractTimelineMarker;

namespace dom {
struct ProfileTimelineMarker;
}

class MarkersStorage : public LinkedListElement<MarkersStorage>
{
private:
  MarkersStorage() = delete;
  MarkersStorage(const MarkersStorage& aOther) = delete;
  void operator=(const MarkersStorage& aOther) = delete;

public:
  explicit MarkersStorage(const char* aMutexName);
  virtual ~MarkersStorage();

  virtual void AddMarker(UniquePtr<AbstractTimelineMarker>&& aMarker) = 0;
  virtual void AddOTMTMarker(UniquePtr<AbstractTimelineMarker>&& aMarker) = 0;
  virtual void ClearMarkers() = 0;
  virtual void PopMarkers(JSContext* aCx, nsTArray<dom::ProfileTimelineMarker>& aStore) = 0;

protected:
  Mutex& GetLock();

private:
  Mutex mLock;
};

} // namespace mozilla

#endif /* mozilla_MarkersStorage_h_ */
