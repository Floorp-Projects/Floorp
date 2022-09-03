/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "UtilityProcessImpl.h"

#include "mozilla/ipc/IOThreadChild.h"
#include "mozilla/GeckoArgs.h"

#if defined(OS_WIN) && defined(MOZ_SANDBOX)
#  include "mozilla/sandboxTarget.h"
#  include "WMF.h"
#  include "WMFDecoderModule.h"
#endif

#if defined(XP_OPENBSD) && defined(MOZ_SANDBOX)
#  include "mozilla/SandboxSettings.h"
#endif

namespace mozilla::ipc {

UtilityProcessImpl::~UtilityProcessImpl() = default;

bool UtilityProcessImpl::Init(int aArgc, char* aArgv[]) {
  Maybe<uint64_t> sandboxingKind = geckoargs::sSandboxingKind.Get(aArgc, aArgv);
  if (sandboxingKind.isNothing()) {
    return false;
  }

  if (*sandboxingKind >= SandboxingKind::COUNT) {
    return false;
  }

#if defined(MOZ_SANDBOX) && defined(OS_WIN)
  // We delay load winmm.dll so that its dependencies don't interfere with COM
  // initialization when win32k is locked down. We need to load it before we
  // lower the sandbox in processes where the policy will prevent loading.
  ::LoadLibraryW(L"winmm.dll");

  if (*sandboxingKind == SandboxingKind::UTILITY_AUDIO_DECODING_GENERIC) {
    UtilityAudioDecoderParent::GenericPreloadForSandbox();
  }

  if (*sandboxingKind == SandboxingKind::UTILITY_AUDIO_DECODING_WMF
#  ifdef MOZ_WMF_MEDIA_ENGINE
      || *sandboxingKind == SandboxingKind::MF_MEDIA_ENGINE_CDM
#  endif
  ) {
    UtilityAudioDecoderParent::WMFPreloadForSandbox();
  }

  // Go for it
  mozilla::SandboxTarget::Instance()->StartSandbox();
#elif defined(__OpenBSD__) && defined(MOZ_SANDBOX)
  StartOpenBSDSandbox(GeckoProcessType_Utility,
                      (SandboxingKind)*sandboxingKind);
#endif

  Maybe<const char*> parentBuildID =
      geckoargs::sParentBuildID.Get(aArgc, aArgv);
  if (parentBuildID.isNothing()) {
    return false;
  }

  if (!ProcessChild::InitPrefs(aArgc, aArgv)) {
    return false;
  }

  return mUtility->Init(TakeInitialEndpoint(), nsCString(*parentBuildID),
                        *sandboxingKind);
}

void UtilityProcessImpl::CleanUp() { NS_ShutdownXPCOM(nullptr); }

}  // namespace mozilla::ipc
