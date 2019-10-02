/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAPTURETASK_H
#define CAPTURETASK_H

#include "MediaStreamGraph.h"
#include "MediaStreamListener.h"
#include "PrincipalChangeObserver.h"

namespace mozilla {

namespace dom {
class Blob;
class ImageCapture;
class MediaStreamTrack;
}  // namespace dom

/**
 * CaptureTask retrieves image from MediaStream and encodes the image to jpeg in
 * ImageEncoder. The whole procedures start at AttachTrack(), it will add this
 * class into MediaStream and retrieves an image in MediaStreamGraph thread.
 * Once the image is retrieved, it will be sent to ImageEncoder and the encoded
 * blob will be sent out via encoder callback in main thread.
 *
 * CaptureTask holds a reference of ImageCapture to ensure ImageCapture won't be
 * released during the period of the capturing process described above.
 */
class CaptureTask : public DirectMediaStreamTrackListener,
                    public dom::PrincipalChangeObserver<dom::MediaStreamTrack> {
 public:
  class MediaStreamEventListener;

  // DirectMediaStreamTrackListener methods
  void NotifyRealtimeTrackData(MediaStreamGraph* aGraph,
                               StreamTime aTrackOffset,
                               const MediaSegment& aMedia) override;

  // PrincipalChangeObserver<MediaStreamTrack> methods
  void PrincipalChanged(dom::MediaStreamTrack* aMediaStreamTrack) override;

  // CaptureTask methods.

  // It is called when aBlob is ready to post back to script in company with
  // aRv == NS_OK. If aRv is not NS_OK, it will post an error event to script.
  //
  // Note:
  //   this function should be called on main thread.
  nsresult TaskComplete(already_AddRefed<dom::Blob> aBlob, nsresult aRv);

  // Add listeners into MediaStreamTrack and PrincipalChangeObserver.
  // It should be on main thread only.
  void AttachTrack();

  // Remove listeners from MediaStreamTrack and PrincipalChangeObserver.
  // It should be on main thread only.
  void DetachTrack();

  // CaptureTask should be created on main thread.
  explicit CaptureTask(dom::ImageCapture* aImageCapture);

 protected:
  virtual ~CaptureTask() {}

  // Post a runnable on main thread to end this task and call TaskComplete to
  // post error event to script. It is called off-main-thread.
  void PostTrackEndEvent();

  // The ImageCapture associates with this task. This reference count should not
  // change in this class unless it clears this reference after a blob or error
  // event to script.
  RefPtr<dom::ImageCapture> mImageCapture;

  RefPtr<MediaStreamEventListener> mEventListener;

  // True when an image is retrieved from the video track, or MediaStreamGraph
  // sends a track finish, end, or removed event. Any thread.
  Atomic<bool> mImageGrabbedOrTrackEnd;

  // True after MediaStreamTrack principal changes while waiting for a photo
  // to finish and we should raise a security error.
  bool mPrincipalChanged;
};

}  // namespace mozilla

#endif  // CAPTURETASK_H
