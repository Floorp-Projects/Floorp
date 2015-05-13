/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasCaptureMediaStream.h"
#include "DOMMediaStream.h"
#include "ImageContainer.h"
#include "MediaStreamGraph.h"
#include "mozilla/Mutex.h"
#include "mozilla/dom/CanvasCaptureMediaStreamBinding.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "nsIAppShell.h"
#include "nsWidgetsCID.h"

static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

using namespace mozilla::layers;
using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(CanvasCaptureMediaStream, DOMMediaStream,
                                   mCanvas)

NS_IMPL_ADDREF_INHERITED(CanvasCaptureMediaStream, DOMMediaStream)
NS_IMPL_RELEASE_INHERITED(CanvasCaptureMediaStream, DOMMediaStream)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(CanvasCaptureMediaStream)
NS_INTERFACE_MAP_END_INHERITING(DOMMediaStream)

CanvasCaptureMediaStream::CanvasCaptureMediaStream(HTMLCanvasElement* aCanvas)
  : mCanvas(aCanvas)
{
}

CanvasCaptureMediaStream::~CanvasCaptureMediaStream()
{
}

JSObject*
CanvasCaptureMediaStream::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return dom::CanvasCaptureMediaStreamBinding::Wrap(aCx, this, aGivenProto);
}

void
CanvasCaptureMediaStream::RequestFrame()
{
}

nsresult
CanvasCaptureMediaStream::Init(const dom::Optional<double>& aFPS,
                               const TrackID& aTrackId)
{
  if (!aFPS.WasPassed()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  } else if (aFPS.Value() < 0) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

already_AddRefed<CanvasCaptureMediaStream>
CanvasCaptureMediaStream::CreateSourceStream(nsIDOMWindow* aWindow,
                                             HTMLCanvasElement* aCanvas)
{
  nsRefPtr<CanvasCaptureMediaStream> stream = new CanvasCaptureMediaStream(aCanvas);
  stream->InitSourceStream(aWindow);
  return stream.forget();
}

} // namespace dom
} // namespace mozilla

