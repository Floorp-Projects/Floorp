/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HLSResource_h_
#define HLSResource_h_

#include "GeneratedJNINatives.h"
#include "GeneratedJNIWrappers.h"
#include "HLSUtils.h"
#include "nsContentUtils.h"

using namespace mozilla::java;

namespace mozilla {

class HLSDecoder;
class HLSResource;

class HLSResourceCallbacksSupport
  : public GeckoHLSResourceWrapper::Callbacks::Natives<HLSResourceCallbacksSupport>
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(HLSResourceCallbacksSupport)
public:
  typedef GeckoHLSResourceWrapper::Callbacks::Natives<HLSResourceCallbacksSupport> NativeCallbacks;
  using NativeCallbacks::DisposeNative;
  using NativeCallbacks::AttachNative;

  HLSResourceCallbacksSupport(HLSResource* aResource);
  void Detach();
  void OnDataArrived();
  void OnError(int aErrorCode);

private:
  ~HLSResourceCallbacksSupport() {}
  HLSResource* mResource;
};

class HLSResource final
{
public:
  HLSResource(HLSDecoder* aDecoder, nsIChannel* aChannel, nsIURI* aURI);
  ~HLSResource();
  void Suspend();
  void Resume();

  already_AddRefed<nsIPrincipal> GetCurrentPrincipal()
  {
    NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

    nsCOMPtr<nsIPrincipal> principal;
    nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
    if (!secMan || !mChannel)
      return nullptr;
    secMan->GetChannelResultPrincipal(mChannel, getter_AddRefs(principal));
    return principal.forget();
  }

  java::GeckoHLSResourceWrapper::GlobalRef GetResourceWrapper() {
    return mHLSResourceWrapper;
  }

  void Detach() { mDecoder = nullptr; }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
  {
    // TODO: track JAVA wrappers.
    return 0;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

private:
  friend class HLSResourceCallbacksSupport;

  void onDataAvailable();
  void onError(int aErrorCode);

  HLSDecoder* mDecoder;
  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsIURI> mURI;
  java::GeckoHLSResourceWrapper::GlobalRef mHLSResourceWrapper;
  java::GeckoHLSResourceWrapper::Callbacks::GlobalRef mJavaCallbacks;
  RefPtr<HLSResourceCallbacksSupport> mCallbackSupport;
};

} // namespace mozilla
#endif /* HLSResource_h_ */
