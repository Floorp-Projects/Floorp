/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HLSResource.h"
#include "HLSUtils.h"

using namespace mozilla::java;

namespace mozilla {

HLSResourceCallbacksSupport::HLSResourceCallbacksSupport(HLSResource* aResource)
{
  MOZ_ASSERT(aResource);
  mResource = aResource;
}

void
HLSResourceCallbacksSupport::Detach()
{
  MOZ_ASSERT(NS_IsMainThread());
  mResource = nullptr;
}

void
HLSResourceCallbacksSupport::OnDataArrived()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mResource) {
    mResource->onDataAvailable();
  }
}

void
HLSResourceCallbacksSupport::OnError(int aErrorCode)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mResource) {
    mResource->onError(aErrorCode);
  }
}

HLSResource::HLSResource(MediaResourceCallback* aCallback,
                         nsIChannel* aChannel,
                         nsIURI* aURI,
                         const MediaContainerType& aContainerType)
  : mCallback(aCallback)
  , mChannel(aChannel)
  , mURI(aURI)
  , mContainerType(aContainerType)
{
  nsCString spec;
  nsresult rv = aURI->GetSpec(spec);
  (void)rv;
  HLSResourceCallbacksSupport::Init();
  mJavaCallbacks = GeckoHLSResourceWrapper::Callbacks::New();
  mCallbackSupport = new HLSResourceCallbacksSupport(this);
  HLSResourceCallbacksSupport::AttachNative(mJavaCallbacks, mCallbackSupport);
  mHLSResourceWrapper = java::GeckoHLSResourceWrapper::Create(NS_ConvertUTF8toUTF16(spec),
                                                              mJavaCallbacks);
  MOZ_ASSERT(mHLSResourceWrapper);
}

void
HLSResource::onDataAvailable()
{
  MOZ_ASSERT(mCallback);
  HLS_DEBUG("HLSResource", "onDataAvailable");
  mCallback->NotifyDataArrived();
}

void
HLSResource::onError(int aErrorCode)
{
  MOZ_ASSERT(mCallback);
  HLS_DEBUG("HLSResource", "onError(%d)", aErrorCode);
  // Since HLS source should be from the Internet, we treat all resource errors
  // from GeckoHlsPlayer as network errors.
  mCallback->NotifyNetworkError();
}

HLSResource::~HLSResource()
{
  HLS_DEBUG("HLSResource", "~HLSResource()");
  if (mCallbackSupport) {
    mCallbackSupport->Detach();
    mCallbackSupport = nullptr;
  }
  if (mJavaCallbacks) {
    HLSResourceCallbacksSupport::DisposeNative(mJavaCallbacks);
    mJavaCallbacks = nullptr;
  }
  if (mHLSResourceWrapper) {
    mHLSResourceWrapper->Destroy();
    mHLSResourceWrapper = nullptr;
  }
}

} // namespace mozilla
