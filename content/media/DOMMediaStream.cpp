/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMMediaStream.h"
#include "nsDOMClassInfoID.h"
#include "nsContentUtils.h"

using namespace mozilla;

DOMCI_DATA(MediaStream, DOMMediaStream)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMMediaStream)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMediaStream)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MediaStream)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMMediaStream)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMMediaStream)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DOMMediaStream)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DOMMediaStream)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

// LocalMediaStream currently is the same C++ class as MediaStream;
// they may eventually split
DOMCI_DATA(LocalMediaStream, DOMLocalMediaStream)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMLocalMediaStream)
  NS_INTERFACE_MAP_ENTRY(nsIDOMLocalMediaStream)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMMediaStream, DOMMediaStream)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMLocalMediaStream)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(LocalMediaStream)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMLocalMediaStream)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMLocalMediaStream)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DOMLocalMediaStream)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DOMLocalMediaStream)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END


DOMMediaStream::~DOMMediaStream()
{
  if (mStream) {
    mStream->Destroy();
  }
}

NS_IMETHODIMP
DOMMediaStream::GetCurrentTime(double *aCurrentTime)
{
  *aCurrentTime = mStream ? MediaTimeToSeconds(mStream->GetCurrentTime()) : 0.0;
  return NS_OK;
}

DOMLocalMediaStream::~DOMLocalMediaStream()
{
  if (mStream) {
    // Make sure Listeners of this stream know it's going away
    Stop();
  }
}

NS_IMETHODIMP
DOMLocalMediaStream::Stop()
{
  if (mStream && mStream->AsSourceStream()) {
    mStream->AsSourceStream()->EndAllTrackAndFinish();
  }
  return NS_OK;
}

already_AddRefed<DOMMediaStream>
DOMMediaStream::CreateSourceStream(uint32_t aHintContents)
{
  nsRefPtr<DOMMediaStream> stream = new DOMMediaStream();
  stream->InitSourceStream(aHintContents);
  return stream.forget();
}

already_AddRefed<DOMLocalMediaStream>
DOMLocalMediaStream::CreateSourceStream(uint32_t aHintContents)
{
  nsRefPtr<DOMLocalMediaStream> stream = new DOMLocalMediaStream();
  stream->InitSourceStream(aHintContents);
  return stream.forget();
}

already_AddRefed<DOMMediaStream>
DOMMediaStream::CreateTrackUnionStream(uint32_t aHintContents)
{
  nsRefPtr<DOMMediaStream> stream = new DOMMediaStream();
  stream->InitTrackUnionStream(aHintContents);
  return stream.forget();
}

already_AddRefed<DOMLocalMediaStream>
DOMLocalMediaStream::CreateTrackUnionStream(uint32_t aHintContents)
{
  nsRefPtr<DOMLocalMediaStream> stream = new DOMLocalMediaStream();
  stream->InitTrackUnionStream(aHintContents);
  return stream.forget();
}

bool
DOMMediaStream::CombineWithPrincipal(nsIPrincipal* aPrincipal)
{
  return nsContentUtils::CombineResourcePrincipals(&mPrincipal, aPrincipal);
}
