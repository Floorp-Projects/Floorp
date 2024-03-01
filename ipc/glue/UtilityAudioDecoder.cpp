/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ProcInfo.h"
#include "mozilla/ipc/UtilityAudioDecoder.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/ipc/UtilityProcessChild.h"

namespace mozilla::ipc {

UtilityActorName GetAudioActorName(const SandboxingKind aSandbox) {
  switch (aSandbox) {
    case GENERIC_UTILITY:
      return UtilityActorName::AudioDecoder_Generic;
#ifdef MOZ_APPLEMEDIA
    case UTILITY_AUDIO_DECODING_APPLE_MEDIA:
      return UtilityActorName::AudioDecoder_AppleMedia;
#endif
#ifdef XP_WIN
    case UTILITY_AUDIO_DECODING_WMF:
      return UtilityActorName::AudioDecoder_WMF;
#endif
#ifdef MOZ_WMF_MEDIA_ENGINE
    case MF_MEDIA_ENGINE_CDM:
      return UtilityActorName::MfMediaEngineCDM;
#endif
    default:
      MOZ_CRASH("Unexpected mSandbox for GetActorName()");
  }
}

nsCString GetChildAudioActorName() {
  RefPtr<ipc::UtilityProcessChild> s = ipc::UtilityProcessChild::Get();
  MOZ_ASSERT(s, "Has UtilityProcessChild");
  return dom::GetEnumString(GetAudioActorName(s->mSandbox));
}

}  // namespace mozilla::ipc
