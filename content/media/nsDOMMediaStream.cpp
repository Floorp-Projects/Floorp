/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

already_AddRefed<nsDOMMediaStream>
nsDOMMediaStream::CreateInputStream()
{
  nsRefPtr<nsDOMMediaStream> stream = new nsDOMMediaStream();
  MediaStreamGraph* gm = MediaStreamGraph::GetInstance();
  stream->mStream = gm->CreateInputStream(stream);
  return stream.forget();
}

bool
nsDOMMediaStream::CombineWithPrincipal(nsIPrincipal* aPrincipal)
{
  return nsContentUtils::CombineResourcePrincipals(&mPrincipal, aPrincipal);
}
