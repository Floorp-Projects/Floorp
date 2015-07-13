/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CanvasCaptureMediaStream_h_
#define mozilla_dom_CanvasCaptureMediaStream_h_

#include "DOMMediaStream.h"
#include "StreamBuffer.h"

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

class OutputStreamDriver
{
public:
  OutputStreamDriver(CanvasCaptureMediaStream* aDOMStream,
                     const TrackID& aTrackId);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(OutputStreamDriver);

  nsresult Start();

  virtual void ForgetDOMStream();

  virtual void RequestFrame() { }

  CanvasCaptureMediaStream* DOMStream() const { return mDOMStream; }

protected:
  virtual ~OutputStreamDriver();
  class StreamListener;

  /*
   * Appends mImage to video track for the desired duration.
   */
  void AppendToTrack(StreamTime aDuration);
  void NotifyPull(StreamTime aDesiredTime);

  /*
   * Sub classes can SetImage() to update the image being appended to the
   * output stream. It will be appended on the next NotifyPull from MSG.
   */
  void SetImage(layers::Image* aImage);

  /*
   * Called in main thread stable state to initialize sub classes.
   */
  virtual void StartInternal() = 0;

private:
  // This is a raw pointer to avoid a reference cycle between OutputStreamDriver
  // and CanvasCaptureMediaStream. ForgetDOMStream() will be called by
  // ~CanvasCaptureMediaStream() to make sure we don't do anything illegal.
  CanvasCaptureMediaStream* mDOMStream;
  nsRefPtr<SourceMediaStream> mSourceStream;
  bool mStarted;
  nsRefPtr<StreamListener> mStreamListener;
  const TrackID mTrackId;

  // The below members are protected by mMutex.
  Mutex mMutex;
  nsRefPtr<layers::Image> mImage;
};

class CanvasCaptureMediaStream: public DOMMediaStream
{
public:
  explicit CanvasCaptureMediaStream(HTMLCanvasElement* aCanvas);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(CanvasCaptureMediaStream, DOMMediaStream)

  nsresult Init(const dom::Optional<double>& aFPS, const TrackID& aTrackId);

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL
  HTMLCanvasElement* Canvas() const { return mCanvas; }
  void RequestFrame();

  /**
   * Create a CanvasCaptureMediaStream whose underlying stream is a SourceMediaStream.
   */
  static already_AddRefed<CanvasCaptureMediaStream>
  CreateSourceStream(nsIDOMWindow* aWindow,
                     HTMLCanvasElement* aCanvas);

protected:
  ~CanvasCaptureMediaStream();

private:
  nsRefPtr<HTMLCanvasElement> mCanvas;
  nsRefPtr<OutputStreamDriver> mOutputStreamDriver;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_CanvasCaptureMediaStream_h_ */
