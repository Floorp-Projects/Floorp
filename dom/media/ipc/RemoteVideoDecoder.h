/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_dom_ipc_RemoteVideoDecoder_h
#define include_dom_ipc_RemoteVideoDecoder_h

#include "mozilla/RefPtr.h"
#include "mozilla/DebugOnly.h"
#include "MediaData.h"
#include "PlatformDecoderModule.h"

namespace mozilla {
namespace dom {

class VideoDecoderChild;
class RemoteDecoderModule;

// A MediaDataDecoder implementation that proxies through IPDL
// to a 'real' decoder in the GPU process.
// All requests get forwarded to a VideoDecoderChild instance that
// operates solely on the VideoDecoderManagerChild thread.
class RemoteVideoDecoder : public MediaDataDecoder
{
public:
  friend class RemoteDecoderModule;

  // MediaDataDecoder
  RefPtr<InitPromise> Init() override;
  void Input(MediaRawData* aSample) override;
  void Flush() override;
  void Drain() override;
  void Shutdown() override;

  const char* GetDescriptionName() const override { return "RemoteVideoDecoder"; }

private:
  explicit RemoteVideoDecoder(MediaDataDecoderCallback* aCallback);
  ~RemoteVideoDecoder();

  RefPtr<InitPromise> InitInternal();

  // Only ever written to from the reader task queue (during the constructor and destructor
  // when we can guarantee no other threads are accessing it). Only read from the manager
  // thread.
  RefPtr<VideoDecoderChild> mActor;
#ifdef DEBUG
  MediaDataDecoderCallback* mCallback;
#endif
};

// A PDM implementation that creates RemoteVideoDecoders.
// We currently require a 'wrapped' PDM in order to be able to answer SupportsMimeType
// and DecoderNeedsConversion. Ideally we'd check these over IPDL using the manager
// protocol
class RemoteDecoderModule : public PlatformDecoderModule
{
public:
  explicit RemoteDecoderModule(PlatformDecoderModule* aWrapped)
    : mWrapped(aWrapped)
  {}

  nsresult Startup() override;

  bool SupportsMimeType(const nsACString& aMimeType,
                        DecoderDoctorDiagnostics* aDiagnostics) const override;

  ConversionRequired DecoderNeedsConversion(
    const TrackInfo& aConfig) const override;

  already_AddRefed<MediaDataDecoder> CreateVideoDecoder(
    const CreateDecoderParams& aParams) override;

  already_AddRefed<MediaDataDecoder> CreateAudioDecoder(
    const CreateDecoderParams& aParams) override
  {
    return nullptr;
  }

private:
  RefPtr<PlatformDecoderModule> mWrapped;
};

} // namespace dom
} // namespace mozilla

#endif // include_dom_ipc_RemoteVideoDecoder_h
