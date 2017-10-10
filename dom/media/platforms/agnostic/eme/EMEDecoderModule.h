/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(EMEDecoderModule_h_)
#define EMEDecoderModule_h_

#include "MediaDataDecoderProxy.h"
#include "PlatformDecoderModule.h"
#include "PlatformDecoderModule.h"
#include "SamplesWaitingForKey.h"

namespace mozilla {

class CDMProxy;
class PDMFactory;

class EMEDecoderModule : public PlatformDecoderModule
{
public:
  EMEDecoderModule(CDMProxy* aProxy, PDMFactory* aPDM);

protected:
  // Decode thread.
  already_AddRefed<MediaDataDecoder>
  CreateVideoDecoder(const CreateDecoderParams& aParams) override;

  // Decode thread.
  already_AddRefed<MediaDataDecoder>
  CreateAudioDecoder(const CreateDecoderParams& aParams) override;

  bool
  SupportsMimeType(const nsACString &aMimeType,
                   DecoderDoctorDiagnostics *aDiagnostics) const override;

private:
  virtual ~EMEDecoderModule();
  RefPtr<CDMProxy> mProxy;
  // Will be null if CDM has decoding capability.
  RefPtr<PDMFactory> mPDM;
};

DDLoggedTypeDeclNameAndBase(EMEMediaDataDecoderProxy, MediaDataDecoderProxy);

class EMEMediaDataDecoderProxy
  : public MediaDataDecoderProxy
  , public DecoderDoctorLifeLogger<EMEMediaDataDecoderProxy>
{
public:
  EMEMediaDataDecoderProxy(
    already_AddRefed<AbstractThread> aProxyThread, CDMProxy* aProxy,
    const CreateDecoderParams& aParams);
  EMEMediaDataDecoderProxy(const CreateDecoderParams& aParams,
                           already_AddRefed<MediaDataDecoder> aProxyDecoder,
                           CDMProxy* aProxy);

  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;
  RefPtr<FlushPromise> Flush() override;
  RefPtr<ShutdownPromise> Shutdown() override;

private:
  RefPtr<TaskQueue> mTaskQueue;
  RefPtr<SamplesWaitingForKey> mSamplesWaitingForKey;
  MozPromiseRequestHolder<SamplesWaitingForKey::WaitForKeyPromise> mKeyRequest;
  MozPromiseHolder<DecodePromise> mDecodePromise;
  MozPromiseRequestHolder<DecodePromise> mDecodeRequest;
  RefPtr<CDMProxy> mProxy;
};

} // namespace mozilla

#endif // EMEDecoderModule_h_
