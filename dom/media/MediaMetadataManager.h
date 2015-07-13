/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MediaMetadataManager_h__)
#define MediaMetadataManager_h__
#include "VideoUtils.h"
#include "mozilla/LinkedList.h"
#include "AbstractMediaDecoder.h"
#include "nsAutoPtr.h"

namespace mozilla {

  // A struct that contains the metadata of a media, and the time at which those
  // metadata should start to be reported.
  class TimedMetadata : public LinkedListElement<TimedMetadata> {
    public:
      // The time, in microseconds, at which those metadata should be available.
      int64_t mPublishTime;
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
  class MediaMetadataManager
  {
    public:
      ~MediaMetadataManager() {
        TimedMetadata* element;
        while((element = mMetadataQueue.popFirst()) != nullptr) {
          delete element;
        }
      }
      void QueueMetadata(TimedMetadata* aMetadata) {
        mMetadataQueue.insertBack(aMetadata);
      }

      void DispatchMetadataIfNeeded(AbstractMediaDecoder* aDecoder, double aCurrentTime) {
        TimedMetadata* metadata = mMetadataQueue.getFirst();
        while (metadata && aCurrentTime >= static_cast<double>(metadata->mPublishTime) / USECS_PER_S) {
          // Remove all media tracks from the list first.
          nsCOMPtr<nsIRunnable> removeTracksEvent =
            new RemoveMediaTracksEventRunner(aDecoder);
          NS_DispatchToMainThread(removeTracksEvent);

          nsCOMPtr<nsIRunnable> metadataUpdatedEvent =
            new MetadataUpdatedEventRunner(aDecoder,
                                           metadata->mInfo,
                                           metadata->mTags);
          NS_DispatchToMainThread(metadataUpdatedEvent);
          delete mMetadataQueue.popFirst();
          metadata = mMetadataQueue.getFirst();
        }
      }
    protected:
      LinkedList<TimedMetadata> mMetadataQueue;
  };
} // namespace mozilla

#endif
