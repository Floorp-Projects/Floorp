/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MediaDataEncoderProxy_h_)
#  define MediaDataEncoderProxy_h_

#  include "PlatformEncoderModule.h"
#  include "mozilla/Atomics.h"
#  include "mozilla/RefPtr.h"
#  include "nsThreadUtils.h"
#  include "nscore.h"

namespace mozilla {

class MediaDataEncoderProxy final : public MediaDataEncoder {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaDataEncoderProxy, final);

  explicit MediaDataEncoderProxy(
      already_AddRefed<MediaDataEncoder> aProxyEncoder,
      already_AddRefed<nsISerialEventTarget> aProxyThread);

  RefPtr<InitPromise> Init() override;
  RefPtr<EncodePromise> Encode(const MediaData* aSample) override;
  RefPtr<ReconfigurationPromise> Reconfigure(
      const RefPtr<const EncoderConfigurationChangeList>& aConfigurationChanges)
      override;
  RefPtr<EncodePromise> Drain() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  RefPtr<GenericPromise> SetBitrate(uint32_t aBitsPerSec) override;
  bool IsHardwareAccelerated(nsACString& aFailureReason) const override;
  nsCString GetDescriptionName() const override;

 protected:
  virtual ~MediaDataEncoderProxy();

 private:
  // Set on construction and clear on the proxy thread if set.
  RefPtr<MediaDataEncoder> mProxyEncoder;
  const nsCOMPtr<nsISerialEventTarget> mProxyThread;

#  if defined(DEBUG)
  Atomic<bool> mIsShutdown = Atomic<bool>(false);
#  endif
};

}  // namespace mozilla

#endif  // MediaDataEncoderProxy_h_
