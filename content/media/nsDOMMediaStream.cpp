/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMMediaStream.h"
#include "nsDOMClassInfoID.h"
#include "nsContentUtils.h"

using namespace mozilla;

DOMCI_DATA(MediaStream, nsDOMMediaStream)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMMediaStream)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMediaStream)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MediaStream)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMMediaStream)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMMediaStream)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMMediaStream)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsDOMMediaStream)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsDOMMediaStream)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

// LocalMediaStream currently is the same C++ class as MediaStream;
// they may eventually split
DOMCI_DATA(LocalMediaStream, nsDOMLocalMediaStream)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMLocalMediaStream)
  NS_INTERFACE_MAP_ENTRY(nsIDOMLocalMediaStream)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMMediaStream, nsDOMMediaStream)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMLocalMediaStream)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(LocalMediaStream)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMLocalMediaStream)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMLocalMediaStream)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMLocalMediaStream)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsDOMLocalMediaStream)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsDOMLocalMediaStream)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END


nsDOMMediaStream::~nsDOMMediaStream()
{
  if (mStream) {
    mStream->Destroy();
  }
}

NS_IMETHODIMP
nsDOMMediaStream::GetCurrentTime(double *aCurrentTime)
{
  *aCurrentTime = mStream ? MediaTimeToSeconds(mStream->GetCurrentTime()) : 0.0;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMLocalMediaStream::Stop()
{
  if (mStream && mStream->AsSourceStream()) {
    mStream->AsSourceStream()->EndAllTrackAndFinish();
  }
  return NS_OK;
}

already_AddRefed<nsDOMMediaStream>
nsDOMMediaStream::CreateSourceStream(uint32_t aHintContents)
{
  nsRefPtr<nsDOMMediaStream> stream = new nsDOMMediaStream();
  stream->SetHintContents(aHintContents);
  MediaStreamGraph* gm = MediaStreamGraph::GetInstance();
  stream->mStream = gm->CreateSourceStream(stream);
  return stream.forget();
}

already_AddRefed<nsDOMLocalMediaStream>
nsDOMLocalMediaStream::CreateSourceStream(uint32_t aHintContents)
{
  nsRefPtr<nsDOMLocalMediaStream> stream = new nsDOMLocalMediaStream();
  stream->SetHintContents(aHintContents);
  MediaStreamGraph* gm = MediaStreamGraph::GetInstance();
  stream->mStream = gm->CreateSourceStream(stream);
  return stream.forget();
}

already_AddRefed<nsDOMMediaStream>
nsDOMMediaStream::CreateTrackUnionStream()
{
  nsRefPtr<nsDOMMediaStream> stream = new nsDOMMediaStream();
  MediaStreamGraph* gm = MediaStreamGraph::GetInstance();
  stream->mStream = gm->CreateTrackUnionStream(stream);
  return stream.forget();
}

already_AddRefed<nsDOMLocalMediaStream>
nsDOMLocalMediaStream::CreateTrackUnionStream()
{
  nsRefPtr<nsDOMLocalMediaStream> stream = new nsDOMLocalMediaStream();
  MediaStreamGraph* gm = MediaStreamGraph::GetInstance();
  stream->mStream = gm->CreateTrackUnionStream(stream);
  return stream.forget();
}

bool
nsDOMMediaStream::CombineWithPrincipal(nsIPrincipal* aPrincipal)
{
  return nsContentUtils::CombineResourcePrincipals(&mPrincipal, aPrincipal);
}
