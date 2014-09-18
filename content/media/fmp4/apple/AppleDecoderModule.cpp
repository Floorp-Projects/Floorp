/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AppleATDecoder.h"
#include "AppleCMLinker.h"
#include "AppleDecoderModule.h"
#include "AppleVDADecoder.h"
#include "AppleVDALinker.h"
#include "AppleVTDecoder.h"
#include "AppleVTLinker.h"
#include "mozilla/Preferences.h"
#include "mozilla/DebugOnly.h"

namespace mozilla {

bool AppleDecoderModule::sInitialized = false;
bool AppleDecoderModule::sIsVTAvailable = false;
bool AppleDecoderModule::sIsVTHWAvailable = false;
bool AppleDecoderModule::sIsVDAAvailable = false;
bool AppleDecoderModule::sForceVDA = false;

AppleDecoderModule::AppleDecoderModule()
{
}

AppleDecoderModule::~AppleDecoderModule()
{
}

/* static */
void
AppleDecoderModule::Init()
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");

  sForceVDA = Preferences::GetBool("media.apple.forcevda", false);

  if (sInitialized) {
    return;
  }

  // dlopen VideoDecodeAcceleration.framework if it's available.
  sIsVDAAvailable = AppleVDALinker::Link();

  // dlopen CoreMedia.framework if it's available.
  bool haveCoreMedia = AppleCMLinker::Link();
  // dlopen VideoToolbox.framework if it's available.
  // We must link both CM and VideoToolbox framework to allow for proper
  // paired Link/Unlink calls
  bool haveVideoToolbox = AppleVTLinker::Link();
  sIsVTAvailable = haveCoreMedia && haveVideoToolbox;

  sIsVTHWAvailable = AppleVTLinker::skPropHWAccel != nullptr;

  if (sIsVDAAvailable) {
    AppleVDALinker::Unlink();
  }
  if (sIsVTAvailable) {
    AppleVTLinker::Unlink();
    AppleCMLinker::Unlink();
  }
  sInitialized = true;
}

class InitTask : public nsRunnable {
public:
  NS_IMETHOD Run() MOZ_OVERRIDE {
    MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");
    AppleDecoderModule::Init();
    return NS_OK;
  }
};

/* static */
nsresult
AppleDecoderModule::CanDecode()
{
  if (!sInitialized) {
    if (NS_IsMainThread()) {
      Init();
    } else {
      nsRefPtr<nsIRunnable> task(new InitTask());
      NS_DispatchToMainThread(task, NS_DISPATCH_SYNC);
    }
  }

  return (sIsVDAAvailable || sIsVTAvailable) ? NS_OK : NS_ERROR_NO_INTERFACE;
}

class LinkTask : public nsRunnable {
public:
  NS_IMETHOD Run() MOZ_OVERRIDE {
    MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");
    MOZ_ASSERT(AppleDecoderModule::sInitialized);
    if (AppleDecoderModule::sIsVDAAvailable) {
      AppleVDALinker::Link();
    }
    if (AppleDecoderModule::sIsVTAvailable) {
      AppleVTLinker::Link();
      AppleCMLinker::Link();
    }
    return NS_OK;
  }
};

nsresult
AppleDecoderModule::Startup()
{
  if (!sIsVDAAvailable && !sIsVTAvailable) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<nsIRunnable> task(new LinkTask());
  NS_DispatchToMainThread(task, NS_DISPATCH_SYNC);

  return NS_OK;
}

class UnlinkTask : public nsRunnable {
public:
  NS_IMETHOD Run() MOZ_OVERRIDE {
    MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");
    MOZ_ASSERT(AppleDecoderModule::sInitialized);
    if (AppleDecoderModule::sIsVDAAvailable) {
      AppleVDALinker::Unlink();
    }
    if (AppleDecoderModule::sIsVTAvailable) {
      AppleVTLinker::Unlink();
      AppleCMLinker::Unlink();
    }
    return NS_OK;
  }
};

nsresult
AppleDecoderModule::Shutdown()
{
  nsRefPtr<nsIRunnable> task(new UnlinkTask());
  NS_DispatchToMainThread(task);
  return NS_OK;
}

already_AddRefed<MediaDataDecoder>
AppleDecoderModule::CreateH264Decoder(const mp4_demuxer::VideoDecoderConfig& aConfig,
                                      layers::LayersBackend aLayersBackend,
                                      layers::ImageContainer* aImageContainer,
                                      MediaTaskQueue* aVideoTaskQueue,
                                      MediaDataDecoderCallback* aCallback)
{
  nsRefPtr<MediaDataDecoder> decoder;

  if (sIsVDAAvailable && (!sIsVTHWAvailable || sForceVDA)) {
    decoder =
      AppleVDADecoder::CreateVDADecoder(aConfig,
                                        aVideoTaskQueue,
                                        aCallback,
                                        aImageContainer);
    if (decoder) {
      return decoder.forget();
    }
  }
  // We fallback here if VDA isn't available, or is available but isn't
  // supported by the current platform.
  if (sIsVTAvailable) {
    decoder =
      new AppleVTDecoder(aConfig, aVideoTaskQueue, aCallback, aImageContainer);
  }
  return decoder.forget();
}

already_AddRefed<MediaDataDecoder>
AppleDecoderModule::CreateAudioDecoder(const mp4_demuxer::AudioDecoderConfig& aConfig,
                                       MediaTaskQueue* aAudioTaskQueue,
                                       MediaDataDecoderCallback* aCallback)
{
  nsRefPtr<MediaDataDecoder> decoder =
    new AppleATDecoder(aConfig, aAudioTaskQueue, aCallback);
  return decoder.forget();
}

bool
AppleDecoderModule::SupportsAudioMimeType(const char* aMimeType)
{
  return !strcmp(aMimeType, "audio/mp4a-latm") || !strcmp(aMimeType, "audio/mpeg");
}

} // namespace mozilla
