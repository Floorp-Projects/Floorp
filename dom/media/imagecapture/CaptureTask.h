/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAPTURETASK_H
#define CAPTURETASK_H

#include "DOMMediaStream.h"
#include "MediaStreamGraph.h"

namespace mozilla {

namespace dom {
class Blob;
class ImageCapture;
} // namespace dom

/**
 * CaptureTask retrieves image from MediaStream and encodes the image to jpeg in
 * ImageEncoder. The whole procedures start at AttachStream(), it will add this
 * class into MediaStream and retrieves an image in MediaStreamGraph thread.
 * Once the image is retrieved, it will be sent to ImageEncoder and the encoded
 * blob will be sent out via encoder callback in main thread.
 *
 * CaptureTask holds a reference of ImageCapture to ensure ImageCapture won't be
 * released during the period of the capturing process described above.
 */
class CaptureTask : public MediaStreamListener,
                    public DOMMediaStream::PrincipalChangeObserver
{
public:
  // MediaStreamListener methods.
  virtual void NotifyQueuedTrackChanges(MediaStreamGraph* aGraph, TrackID aID,
                                        StreamTime aTrackOffset,
                                        uint32_t aTrackEvents,
                                        const MediaSegment& aQueuedMedia,
                                        MediaStream* aInputStream,
                                        TrackID aInputTrackID) override;

  virtual void NotifyEvent(MediaStreamGraph* aGraph,
                           MediaStreamGraphEvent aEvent) override;

  // DOMMediaStream::PrincipalChangeObserver method.
  virtual void PrincipalChanged(DOMMediaStream* aMediaStream) override;

  // CaptureTask methods.

  // It is called when aBlob is ready to post back to script in company with
  // aRv == NS_OK. If aRv is not NS_OK, it will post an error event to script.
  //
  // Note:
  //   this function should be called on main thread.
  nsresult TaskComplete(already_AddRefed<dom::Blob> aBlob, nsresult aRv);

  // Add listeners into MediaStream and PrincipalChangeObserver. It should be on
  // main thread only.
  void AttachStream();

  // Remove listeners from MediaStream and PrincipalChangeObserver. It should be
  // on main thread only.
  void DetachStream();

  // CaptureTask should be created on main thread.
  CaptureTask(dom::ImageCapture* aImageCapture, TrackID aTrackID)
    : mImageCapture(aImageCapture)
    , mTrackID(aTrackID)
    , mImageGrabbedOrTrackEnd(false)
    , mPrincipalChanged(false) {}

protected:
  virtual ~CaptureTask() {}

  // Post a runnable on main thread to end this task and call TaskComplete to post
  // error event to script. It is called off-main-thread.
  void PostTrackEndEvent();

  // The ImageCapture associates with this task. This reference count should not
  // change in this class unless it clears this reference after a blob or error
  // event to script.
  nsRefPtr<dom::ImageCapture> mImageCapture;

  TrackID mTrackID;

  // True when an image is retrieved from MediaStreamGraph or MediaStreamGraph
  // sends a track finish, end, or removed event.
  bool mImageGrabbedOrTrackEnd;

  // True when MediaStream principal is changed in the period of taking photo
  // and it causes a NS_ERROR_DOM_SECURITY_ERR error to script.
  bool mPrincipalChanged;
};

} // namespace mozilla

#endif // CAPTURETASK_H
