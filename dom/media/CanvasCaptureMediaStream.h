/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CanvasCaptureMediaStream_h_
#define mozilla_dom_CanvasCaptureMediaStream_h_

#include "DOMMediaStream.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "StreamTracks.h"

class nsIPrincipal;

namespace mozilla {
class DOMMediaStream;
class MediaStreamListener;
class SourceMediaStream;

namespace layers {
class Image;
} // namespace layers

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
 *                                                  | SetImage()
 *                                                  v
 *                                         ___________________
 *                                        |   StreamListener  |
 * ---------------------------------------| (All image access |----------------
 *     === MediaStreamGraph Thread ===    |   Mutex Guarded)  |
 *                                        |___________________|
 *                                              ^       |
 *                                 NotifyPull() |       | AppendToTrack()
 *                                              |       v
 *                                      ___________________________
 *                                     |                           |
 *                                     |  MSG / SourceMediaStream  |
 *                                     |___________________________|
 * ----------------------------------------------------------------------------
 */

/*
 * Base class for drivers of the output stream.
 * It is up to each sub class to implement the NewFrame() callback of
 * FrameCaptureListener.
 */
class OutputStreamDriver : public FrameCaptureListener
{
public:
  OutputStreamDriver(SourceMediaStream* aSourceStream,
                     const TrackID& aTrackId,
                     const PrincipalHandle& aPrincipalHandle);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(OutputStreamDriver);

  /*
   * Sub classes can SetImage() to update the image being appended to the
   * output stream. It will be appended on the next NotifyPull from MSG.
   */
  void SetImage(const RefPtr<layers::Image>& aImage, const TimeStamp& aTime);

  /*
   * Makes sure any internal resources this driver is holding that may create
   * reference cycles are released.
   */
  virtual void Forget() {}

protected:
  virtual ~OutputStreamDriver();
  class StreamListener;

private:
  RefPtr<SourceMediaStream> mSourceStream;
  RefPtr<StreamListener> mStreamListener;
};

class CanvasCaptureMediaStream : public DOMMediaStream
{
public:
  CanvasCaptureMediaStream(nsPIDOMWindowInner* aWindow, HTMLCanvasElement* aCanvas);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(CanvasCaptureMediaStream, DOMMediaStream)

  nsresult Init(const dom::Optional<double>& aFPS, const TrackID& aTrackId,
                nsIPrincipal* aPrincipal);

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL
  HTMLCanvasElement* Canvas() const { return mCanvas; }
  void RequestFrame();

  dom::FrameCaptureListener* FrameCaptureListener();

  /**
   * Stops capturing for this stream at mCanvas.
   */
  void StopCapture();

  /**
   * Create a CanvasCaptureMediaStream whose underlying stream is a SourceMediaStream.
   */
  static already_AddRefed<CanvasCaptureMediaStream>
  CreateSourceStream(nsPIDOMWindowInner* aWindow,
                     HTMLCanvasElement* aCanvas);

protected:
  ~CanvasCaptureMediaStream();

private:
  RefPtr<HTMLCanvasElement> mCanvas;
  RefPtr<OutputStreamDriver> mOutputStreamDriver;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_CanvasCaptureMediaStream_h_ */
