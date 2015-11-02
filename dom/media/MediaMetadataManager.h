/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MediaMetadataManager_h__)
#define MediaMetadataManager_h__

#include "mozilla/AbstractThread.h"
#include "mozilla/LinkedList.h"

#include "nsAutoPtr.h"
#include "AbstractMediaDecoder.h"
#include "MediaEventSource.h"
#include "TimeUnits.h"
#include "VideoUtils.h"

namespace mozilla {

class TimedMetadata;
typedef MediaEventProducerExc<TimedMetadata> TimedMetadataEventProducer;
typedef MediaEventSourceExc<TimedMetadata> TimedMetadataEventSource;

// A struct that contains the metadata of a media, and the time at which those
// metadata should start to be reported.
class TimedMetadata : public LinkedListElement<TimedMetadata> {
public:
  TimedMetadata(const media::TimeUnit& aPublishTime,
                nsAutoPtr<MetadataTags>&& aTags,
                nsAutoPtr<MediaInfo>&& aInfo)
    : mPublishTime(aPublishTime)
    , mTags(Move(aTags))
    , mInfo(Move(aInfo)) {}

  // Define our move constructor because we don't want to move the members of
  // LinkedListElement to change the list.
  TimedMetadata(TimedMetadata&& aOther)
    : mPublishTime(aOther.mPublishTime)
    , mTags(Move(aOther.mTags))
    , mInfo(Move(aOther.mInfo)) {}

  // The time, in microseconds, at which those metadata should be available.
  media::TimeUnit mPublishTime;
  // The metadata. The ownership is transfered to the element when dispatching to
  // the main threads.
  nsAutoPtr<MetadataTags> mTags;
  // The media info, including the info of audio tracks and video tracks.
  // The ownership is transfered to MediaDecoder when dispatching to the
  // main thread.
  nsAutoPtr<MediaInfo> mInfo;
};

// This class encapsulate the logic to give the metadata from the reader to
// the content, at the right time.
class MediaMetadataManager {
public:
  ~MediaMetadataManager() {
    TimedMetadata* element;
    while((element = mMetadataQueue.popFirst()) != nullptr) {
      delete element;
    }
  }

  // Connect to an event source to receive TimedMetadata events.
  void Connect(TimedMetadataEventSource& aEvent, AbstractThread* aThread) {
    mListener = aEvent.Connect(
      aThread, this, &MediaMetadataManager::OnMetadataQueued);
  }

  // Stop receiving TimedMetadata events.
  void Disconnect() {
    mListener.Disconnect();
  }

  // Return an event source through which we will send TimedMetadata events
  // when playback position reaches the publish time.
  TimedMetadataEventSource& TimedMetadataEvent() {
    return mTimedMetadataEvent;
  }

  void DispatchMetadataIfNeeded(const media::TimeUnit& aCurrentTime) {
    TimedMetadata* metadata = mMetadataQueue.getFirst();
    while (metadata && aCurrentTime >= metadata->mPublishTime) {
      // Our listener will figure out what to do with TimedMetadata.
      mTimedMetadataEvent.Notify(Move(*metadata));
      delete mMetadataQueue.popFirst();
      metadata = mMetadataQueue.getFirst();
    }
  }

protected:
  void OnMetadataQueued(TimedMetadata&& aMetadata) {
    mMetadataQueue.insertBack(new TimedMetadata(Move(aMetadata)));
  }

  LinkedList<TimedMetadata> mMetadataQueue;
  MediaEventListener mListener;
  TimedMetadataEventProducer mTimedMetadataEvent;
};

} // namespace mozilla

#endif
