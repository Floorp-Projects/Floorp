/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PlatformDecoderModule.h"
#ifdef XP_WIN
#include "WMFDecoderModule.h"
#endif
#ifdef MOZ_FFMPEG
#include "FFmpegRuntimeLinker.h"
#endif
#include "mozilla/Preferences.h"
#include "EMEDecoderModule.h"
#include "mozilla/CDMProxy.h"
#include "SharedThreadPool.h"
#include "MediaTaskQueue.h"

namespace mozilla {

extern PlatformDecoderModule* CreateBlankDecoderModule();

bool PlatformDecoderModule::sUseBlankDecoder = false;
bool PlatformDecoderModule::sFFmpegDecoderEnabled = false;

/* static */
void
PlatformDecoderModule::Init()
{
  MOZ_ASSERT(NS_IsMainThread());
  static bool alreadyInitialized = false;
  if (alreadyInitialized) {
    return;
  }
  alreadyInitialized = true;

  Preferences::AddBoolVarCache(&sUseBlankDecoder,
                               "media.fragmented-mp4.use-blank-decoder");
  Preferences::AddBoolVarCache(&sFFmpegDecoderEnabled,
                               "media.fragmented-mp4.ffmpeg.enabled", false);
#ifdef XP_WIN
  WMFDecoderModule::Init();
#endif
}

class CreateTaskQueueTask : public nsRunnable {
public:
  NS_IMETHOD Run() {
    MOZ_ASSERT(NS_IsMainThread());
    mTaskQueue = new MediaTaskQueue(GetMediaDecodeThreadPool());
    return NS_OK;
  }
  nsRefPtr<MediaTaskQueue> mTaskQueue;
};

static already_AddRefed<MediaTaskQueue>
CreateTaskQueue()
{
  // We must create the MediaTaskQueue/SharedThreadPool on the main thread.
  nsRefPtr<CreateTaskQueueTask> t(new CreateTaskQueueTask());
  nsresult rv = NS_DispatchToMainThread(t, NS_DISPATCH_SYNC);
  NS_ENSURE_SUCCESS(rv, nullptr);
  return t->mTaskQueue.forget();
}

/* static */
PlatformDecoderModule*
PlatformDecoderModule::CreateCDMWrapper(CDMProxy* aProxy,
                                        bool aHasAudio,
                                        bool aHasVideo,
                                        MediaTaskQueue* aTaskQueue)
{
  bool cdmDecodesAudio;
  bool cdmDecodesVideo;
  {
    CDMCaps::AutoLock caps(aProxy->Capabilites());
    cdmDecodesAudio = caps.CanDecryptAndDecodeAudio();
    cdmDecodesVideo = caps.CanDecryptAndDecodeVideo();
  }

  nsAutoPtr<PlatformDecoderModule> pdm;
  if ((!cdmDecodesAudio && aHasAudio) || (!cdmDecodesVideo && aHasVideo)) {
    // The CDM itself can't decode. We need to wrap a PDM to decode the
    // decrypted output of the CDM.
    pdm = Create();
    if (!pdm) {
      return nullptr;
    }
  } else {
    NS_WARNING("CDM that decodes not yet supported!");
    return nullptr;
  }

  return new EMEDecoderModule(aProxy,
                              pdm.forget(),
                              cdmDecodesAudio,
                              cdmDecodesVideo,
                              CreateTaskQueue());
}

/* static */
PlatformDecoderModule*
PlatformDecoderModule::Create()
{
  if (sUseBlankDecoder) {
    return CreateBlankDecoderModule();
  }
#ifdef XP_WIN
  nsAutoPtr<WMFDecoderModule> m(new WMFDecoderModule());
  if (NS_SUCCEEDED(m->Startup())) {
    return m.forget();
  }
#endif
#ifdef MOZ_FFMPEG
  if (sFFmpegDecoderEnabled) {
    return FFmpegRuntimeLinker::CreateDecoderModule();
  }
#endif
  return nullptr;
}

} // namespace mozilla
