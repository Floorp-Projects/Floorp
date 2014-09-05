/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CaptureTask.h"
#include "mozilla/dom/ImageCapture.h"
#include "mozilla/dom/ImageCaptureError.h"
#include "mozilla/dom/ImageEncoder.h"
#include "mozilla/dom/VideoStreamTrack.h"
#include "gfxUtils.h"
#include "nsThreadUtils.h"

namespace mozilla {

nsresult
CaptureTask::TaskComplete(already_AddRefed<dom::DOMFile> aBlob, nsresult aRv)
{
  MOZ_ASSERT(NS_IsMainThread());

  DetachStream();

  nsresult rv;
  nsRefPtr<dom::DOMFile> blob(aBlob);
  if (mPrincipalChanged) {
    aRv = NS_ERROR_DOM_SECURITY_ERR;
    IC_LOG("MediaStream principal should not change during TakePhoto().");
  }

  if (NS_SUCCEEDED(aRv)) {
    rv = mImageCapture->PostBlobEvent(blob);
  } else {
    rv = mImageCapture->PostErrorEvent(dom::ImageCaptureError::PHOTO_ERROR, aRv);
  }

  // Ensure ImageCapture dereference on main thread here because the TakePhoto()
  // sequences stopped here.
  mImageCapture = nullptr;

  return rv;
}

void
CaptureTask::AttachStream()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<dom::VideoStreamTrack> track = mImageCapture->GetVideoStreamTrack();

  nsRefPtr<DOMMediaStream> domStream = track->GetStream();
  domStream->AddPrincipalChangeObserver(this);

  nsRefPtr<MediaStream> stream = domStream->GetStream();
  stream->AddListener(this);
}

void
CaptureTask::DetachStream()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<dom::VideoStreamTrack> track = mImageCapture->GetVideoStreamTrack();

  nsRefPtr<DOMMediaStream> domStream = track->GetStream();
  domStream->RemovePrincipalChangeObserver(this);

  nsRefPtr<MediaStream> stream = domStream->GetStream();
  stream->RemoveListener(this);
}

void
CaptureTask::PrincipalChanged(DOMMediaStream* aMediaStream)
{
  MOZ_ASSERT(NS_IsMainThread());
  mPrincipalChanged = true;
}

void
CaptureTask::NotifyQueuedTrackChanges(MediaStreamGraph* aGraph, TrackID aID,
                                      TrackRate aTrackRate,
                                      TrackTicks aTrackOffset,
                                      uint32_t aTrackEvents,
                                      const MediaSegment& aQueuedMedia)
{
  if (mImageGrabbedOrTrackEnd) {
    return;
  }

  if (aTrackEvents == MediaStreamListener::TRACK_EVENT_ENDED) {
    PostTrackEndEvent();
    return;
  }

  // Callback for encoding complete, it calls on main thread.
  class EncodeComplete : public dom::EncodeCompleteCallback
  {
  public:
    explicit EncodeComplete(CaptureTask* aTask) : mTask(aTask) {}

    nsresult ReceiveBlob(already_AddRefed<dom::DOMFile> aBlob) MOZ_OVERRIDE
    {
      nsRefPtr<dom::DOMFile> blob(aBlob);
      mTask->TaskComplete(blob.forget(), NS_OK);
      mTask = nullptr;
      return NS_OK;
    }

  protected:
    nsRefPtr<CaptureTask> mTask;
  };

  if (aQueuedMedia.GetType() == MediaSegment::VIDEO && mTrackID == aID) {
    VideoSegment* video =
      const_cast<VideoSegment*> (static_cast<const VideoSegment*>(&aQueuedMedia));
    VideoSegment::ChunkIterator iter(*video);
    while (!iter.IsEnded()) {
      VideoChunk chunk = *iter;
      // Extract the first valid video frame.
      VideoFrame frame;
      if (!chunk.IsNull()) {
        nsRefPtr<layers::Image> image;
        if (chunk.mFrame.GetForceBlack()) {
          // Create a black image.
          image = VideoFrame::CreateBlackImage(chunk.mFrame.GetIntrinsicSize());
        } else {
          image = chunk.mFrame.GetImage();
        }
        MOZ_ASSERT(image);
        mImageGrabbedOrTrackEnd = true;

        // Encode image.
        nsresult rv;
        nsAutoString type(NS_LITERAL_STRING("image/jpeg"));
        nsAutoString options;
        rv = dom::ImageEncoder::ExtractDataFromLayersImageAsync(
                                  type,
                                  options,
                                  false,
                                  image,
                                  new EncodeComplete(this));
        if (NS_FAILED(rv)) {
          PostTrackEndEvent();
        }
        return;
      }
      iter.Next();
    }
  }
}

void
CaptureTask::NotifyEvent(MediaStreamGraph* aGraph, MediaStreamGraphEvent aEvent)
{
  if (((aEvent == EVENT_FINISHED) || (aEvent == EVENT_REMOVED)) &&
      !mImageGrabbedOrTrackEnd) {
    PostTrackEndEvent();
  }
}

void
CaptureTask::PostTrackEndEvent()
{
  mImageGrabbedOrTrackEnd = true;

  // Got track end or finish event, stop the task.
  class TrackEndRunnable : public nsRunnable
  {
  public:
    explicit TrackEndRunnable(CaptureTask* aTask)
      : mTask(aTask) {}

    NS_IMETHOD Run()
    {
      mTask->TaskComplete(nullptr, NS_ERROR_FAILURE);
      mTask = nullptr;
      return NS_OK;
    }

  protected:
    nsRefPtr<CaptureTask> mTask;
  };

  IC_LOG("Got MediaStream track removed or finished event.");
  NS_DispatchToMainThread(new TrackEndRunnable(this));
}

} // namespace mozilla
