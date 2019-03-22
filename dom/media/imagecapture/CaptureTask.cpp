/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CaptureTask.h"
#include "mozilla/dom/ImageCapture.h"
#include "mozilla/dom/ImageCaptureError.h"
#include "mozilla/dom/ImageEncoder.h"
#include "mozilla/dom/MediaStreamTrack.h"
#include "mozilla/dom/VideoStreamTrack.h"
#include "gfxUtils.h"
#include "nsThreadUtils.h"

namespace mozilla {

class CaptureTask::MediaStreamEventListener : public MediaStreamTrackListener {
 public:
  explicit MediaStreamEventListener(CaptureTask* aCaptureTask)
      : mCaptureTask(aCaptureTask){};

  // MediaStreamTrackListener methods.
  void NotifyEnded() override { mCaptureTask->PostTrackEndEvent(); }

 private:
  CaptureTask* mCaptureTask;
};

CaptureTask::CaptureTask(dom::ImageCapture* aImageCapture)
    : mImageCapture(aImageCapture),
      mEventListener(new MediaStreamEventListener(this)),
      mImageGrabbedOrTrackEnd(false),
      mPrincipalChanged(false) {}

nsresult CaptureTask::TaskComplete(already_AddRefed<dom::Blob> aBlob,
                                   nsresult aRv) {
  MOZ_ASSERT(NS_IsMainThread());

  DetachTrack();

  nsresult rv;
  RefPtr<dom::Blob> blob(aBlob);

  // We have to set the parent because the blob has been generated with a valid
  // one.
  if (blob) {
    blob = dom::Blob::Create(mImageCapture->GetParentObject(), blob->Impl());
  }

  if (mPrincipalChanged) {
    aRv = NS_ERROR_DOM_SECURITY_ERR;
    IC_LOG("MediaStream principal should not change during TakePhoto().");
  }

  if (NS_SUCCEEDED(aRv)) {
    rv = mImageCapture->PostBlobEvent(blob);
  } else {
    rv =
        mImageCapture->PostErrorEvent(dom::ImageCaptureError::PHOTO_ERROR, aRv);
  }

  // Ensure ImageCapture dereference on main thread here because the TakePhoto()
  // sequences stopped here.
  mImageCapture = nullptr;

  return rv;
}

void CaptureTask::AttachTrack() {
  MOZ_ASSERT(NS_IsMainThread());

  dom::MediaStreamTrack* track = mImageCapture->GetVideoStreamTrack();
  track->AddPrincipalChangeObserver(this);
  track->AddListener(mEventListener.get());
  track->AddDirectListener(this);
}

void CaptureTask::DetachTrack() {
  MOZ_ASSERT(NS_IsMainThread());

  dom::MediaStreamTrack* track = mImageCapture->GetVideoStreamTrack();
  track->RemovePrincipalChangeObserver(this);
  track->RemoveListener(mEventListener.get());
  track->RemoveDirectListener(this);
}

void CaptureTask::PrincipalChanged(dom::MediaStreamTrack* aMediaStreamTrack) {
  MOZ_ASSERT(NS_IsMainThread());
  mPrincipalChanged = true;
}

void CaptureTask::NotifyRealtimeTrackData(MediaStreamGraph* aGraph,
                                          StreamTime aTrackOffset,
                                          const MediaSegment& aMedia) {
  MOZ_ASSERT(aMedia.GetType() == MediaSegment::VIDEO);
  const VideoSegment& video = static_cast<const VideoSegment&>(aMedia);

  // Callback for encoding complete, it calls on main thread.
  class EncodeComplete : public dom::EncodeCompleteCallback {
   public:
    explicit EncodeComplete(CaptureTask* aTask) : mTask(aTask) {}

    nsresult ReceiveBlob(already_AddRefed<dom::Blob> aBlob) override {
      RefPtr<dom::Blob> blob(aBlob);
      mTask->TaskComplete(blob.forget(), NS_OK);
      mTask = nullptr;
      return NS_OK;
    }

   protected:
    RefPtr<CaptureTask> mTask;
  };

  for (VideoSegment::ConstChunkIterator iter(video); !iter.IsEnded();
       iter.Next()) {
    VideoChunk chunk = *iter;

    // Extract the first valid video frame.
    VideoFrame frame;
    if (chunk.IsNull()) {
      continue;
    }

    RefPtr<layers::Image> image;
    if (chunk.mFrame.GetForceBlack()) {
      // Create a black image.
      image = VideoFrame::CreateBlackImage(chunk.mFrame.GetIntrinsicSize());
    } else {
      image = chunk.mFrame.GetImage();
    }
    if (!image) {
      MOZ_ASSERT(image);
      continue;
    }

    bool wasGrabbed = mImageGrabbedOrTrackEnd.exchange(true);
    if (wasGrabbed) {
      return;
    }

    // Encode image.
    nsresult rv;
    nsAutoString type(NS_LITERAL_STRING("image/jpeg"));
    nsAutoString options;
    rv = dom::ImageEncoder::ExtractDataFromLayersImageAsync(
        type, options, false, image, false, new EncodeComplete(this));
    if (NS_FAILED(rv)) {
      PostTrackEndEvent();
    }
  }
}

void CaptureTask::PostTrackEndEvent() {
  bool wasGrabbed = mImageGrabbedOrTrackEnd.exchange(true);
  if (wasGrabbed) {
    return;
  }

  // Got track end or finish event, stop the task.
  class TrackEndRunnable : public Runnable {
   public:
    explicit TrackEndRunnable(CaptureTask* aTask)
        : mozilla::Runnable("TrackEndRunnable"), mTask(aTask) {}

    NS_IMETHOD Run() override {
      mTask->TaskComplete(nullptr, NS_ERROR_FAILURE);
      mTask = nullptr;
      return NS_OK;
    }

   protected:
    RefPtr<CaptureTask> mTask;
  };

  IC_LOG("Got MediaStream track removed or finished event.");
  nsCOMPtr<nsIRunnable> event = new TrackEndRunnable(this);
  SystemGroup::Dispatch(TaskCategory::Other, event.forget());
}

}  // namespace mozilla
