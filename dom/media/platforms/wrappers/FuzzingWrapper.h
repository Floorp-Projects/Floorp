/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(FuzzingWrapper_h_)
#define FuzzingWrapper_h_

#include "PlatformDecoderModule.h"

namespace mozilla {

class DecoderCallbackFuzzingWrapper : public MediaDataDecoderCallback
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DecoderCallbackFuzzingWrapper)

  explicit DecoderCallbackFuzzingWrapper(MediaDataDecoderCallback* aCallback);

private:
  virtual ~DecoderCallbackFuzzingWrapper();

  // MediaDataDecoderCallback implementation.
  void Output(MediaData* aData) override;
  void Error() override;
  void InputExhausted() override;
  void DrainComplete() override;
  void ReleaseMediaResources() override;
  bool OnReaderTaskQueue() override;

  MediaDataDecoderCallback* mCallback;
};

class DecoderFuzzingWrapper : public MediaDataDecoder
{
public:
  DecoderFuzzingWrapper(already_AddRefed<MediaDataDecoder> aDecoder,
                        already_AddRefed<DecoderCallbackFuzzingWrapper> aCallbackWrapper);
  virtual ~DecoderFuzzingWrapper();

private:
  // MediaDataDecoder implementation.
  nsRefPtr<InitPromise> Init() override;
  nsresult Input(MediaRawData* aSample) override;
  nsresult Flush() override;
  nsresult Drain() override;
  nsresult Shutdown() override;
  bool IsHardwareAccelerated(nsACString& aFailureReason) const override;
  nsresult ConfigurationChanged(const TrackInfo& aConfig) override;

  nsRefPtr<MediaDataDecoder> mDecoder;
  nsRefPtr<DecoderCallbackFuzzingWrapper> mCallbackWrapper;
};

} // namespace mozilla

#endif
