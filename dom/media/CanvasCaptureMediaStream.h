/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CanvasCaptureMediaStream_h_
#define mozilla_dom_CanvasCaptureMediaStream_h_

namespace mozilla {
class DOMMediaStream;

namespace dom {
class HTMLCanvasElement;

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
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_CanvasCaptureMediaStream_h_ */
