/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HLSDecoder_h_
#define HLSDecoder_h_

#include "MediaDecoder.h"
#include "mozilla/java/GeckoHLSResourceWrapperWrappers.h"

namespace mozilla {

class HLSResourceCallbacksSupport;

class HLSDecoder final : public MediaDecoder {
 public:
  static RefPtr<HLSDecoder> Create(MediaDecoderInit& aInit);

  // Returns true if the HLS backend is pref'ed on.
  static bool IsEnabled();

  // Returns true if aContainerType is an HLS type that we think we can render
  // with the a platform decoder backend.
  // If provided, codecs are checked for support.
  static bool IsSupportedType(const MediaContainerType& aContainerType);

  nsresult Load(nsIChannel* aChannel);

  // MediaDecoder interface.
  void Play() override;

  void Pause() override;

  void AddSizeOfResources(ResourceSizes* aSizes) override;
  already_AddRefed<nsIPrincipal> GetCurrentPrincipal() override;
  bool HadCrossOriginRedirects() override;
  bool IsTransportSeekable() override { return true; }
  void Suspend() override;
  void Resume() override;
  void Shutdown() override;

  // Called as data arrives on the underlying HLS player. Main thread only.
  void NotifyDataArrived();

  // Called when Exoplayer start to load media. Main thread only.
  void NotifyLoad(nsCString aMediaUrl);

 private:
  friend class HLSResourceCallbacksSupport;

  explicit HLSDecoder(MediaDecoderInit& aInit);
  ~HLSDecoder();
  MediaDecoderStateMachineBase* CreateStateMachine();

  bool CanPlayThroughImpl() final {
    // TODO: We don't know how to estimate 'canplaythrough' for this decoder.
    // For now we just return true for 'autoplay' can work.
    return true;
  }

  void UpdateCurrentPrincipal(nsCString aMediaUrl);
  already_AddRefed<nsIPrincipal> GetContentPrincipal(nsCString aMediaUrl);

  static size_t sAllocatedInstances;  // Access only in the main thread.

  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsIURI> mURI;
  java::GeckoHLSResourceWrapper::GlobalRef mHLSResourceWrapper;
  java::GeckoHLSResourceWrapper::Callbacks::GlobalRef mJavaCallbacks;
  RefPtr<HLSResourceCallbacksSupport> mCallbackSupport;
  nsCOMPtr<nsIPrincipal> mContentPrincipal;
};

}  // namespace mozilla

#endif /* HLSDecoder_h_ */
