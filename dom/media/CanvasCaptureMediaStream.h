/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CanvasCaptureMediaStream_h_
#define mozilla_dom_CanvasCaptureMediaStream_h_

#include "DOMMediaStream.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "PrincipalHandle.h"

class nsIPrincipal;

namespace mozilla {
class DOMMediaStream;
class SourceMediaTrack;

namespace layers {
class Image;
}  // namespace layers

namespace dom {
class CanvasCaptureMediaStream;
class HTMLCanvasElement;
class OutputStreamFrameListener;

/*
 * The CanvasCaptureMediaStream is a MediaStream subclass that provides a video
 * track containing frames from a canvas. See an architectural overview below.
 *
 * ----------------------------------------------------------------------------
 *     === Main Thread ===              __________________________
 *                                     |                          |
 *                                     | CanvasCaptureMediaStream |
 *                                     |__________________________|
 *                                                  |
 *                                                  | RequestFrame()
 *                                                  v
 *                                       ________________________
 *  ________   FrameCaptureRequested?   |                        |
 * |        | ------------------------> |   OutputStreamDriver   |
 * | Canvas |  SetFrameCapture()        | (FrameCaptureListener) |
 * |________| ------------------------> |________________________|
 *                                                  |
 *                                                  | SetImage() -
 *                                                  | AppendToTrack()
 *                                                  |
 *                                                  v
 *                                      __________________________
 *                                     |                          |
 *                                     |  MTG / SourceMediaTrack  |
 *                                     |__________________________|
 * ----------------------------------------------------------------------------
 */

/*
 * Base class for drivers of the output stream.
 * It is up to each sub class to implement the NewFrame() callback of
 * FrameCaptureListener.
 */
class OutputStreamDriver : public FrameCaptureListener {
 public:
  OutputStreamDriver(SourceMediaTrack* aSourceStream,
                     const PrincipalHandle& aPrincipalHandle);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(OutputStreamDriver);

  /*
   * Called from js' requestFrame() when it wants the next painted frame to be
   * explicitly captured.
   */
  virtual void RequestFrameCapture() = 0;

  /*
   * Sub classes can SetImage() to update the image being appended to the
   * output stream. It will be appended on the next NotifyPull from MTG.
   */
  void SetImage(RefPtr<layers::Image>&& aImage, const TimeStamp& aTime);

  /*
   * Ends the track in mSourceStream when we know there won't be any more images
   * requested for it.
   */
  void EndTrack();

  const RefPtr<SourceMediaTrack> mSourceStream;
  const PrincipalHandle mPrincipalHandle;

 protected:
  virtual ~OutputStreamDriver();
};

class CanvasCaptureMediaStream : public DOMMediaStream {
 public:
  CanvasCaptureMediaStream(nsPIDOMWindowInner* aWindow,
                           HTMLCanvasElement* aCanvas);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(CanvasCaptureMediaStream,
                                           DOMMediaStream)

  nsresult Init(const dom::Optional<double>& aFPS, nsIPrincipal* aPrincipal);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL
  HTMLCanvasElement* Canvas() const { return mCanvas; }
  void RequestFrame();

  dom::FrameCaptureListener* FrameCaptureListener();

  /**
   * Stops capturing for this stream at mCanvas.
   */
  void StopCapture();

  SourceMediaTrack* GetSourceStream() const;

 protected:
  ~CanvasCaptureMediaStream();

 private:
  RefPtr<HTMLCanvasElement> mCanvas;
  RefPtr<OutputStreamDriver> mOutputStreamDriver;
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_CanvasCaptureMediaStream_h_ */
