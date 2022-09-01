/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteDecodeUtils.h"
#include "mozilla/ipc/UtilityProcessChild.h"

namespace mozilla {

using SandboxingKind = ipc::SandboxingKind;

SandboxingKind GetCurrentSandboxingKind() {
  MOZ_ASSERT(XRE_IsUtilityProcess());
  return ipc::UtilityProcessChild::GetSingleton()->mSandbox;
}

SandboxingKind GetSandboxingKindFromLocation(RemoteDecodeIn aLocation) {
  switch (aLocation) {
    case RemoteDecodeIn::UtilityProcess_Generic:
      return SandboxingKind::UTILITY_AUDIO_DECODING_GENERIC;
#ifdef MOZ_APPLEMEDIA
    case RemoteDecodeIn::UtilityProcess_AppleMedia:
      return SandboxingKind::UTILITY_AUDIO_DECODING_APPLE_MEDIA;
      break;
#endif
#ifdef XP_WIN
    case RemoteDecodeIn::UtilityProcess_WMF:
      return SandboxingKind::UTILITY_AUDIO_DECODING_WMF;
#endif
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported RemoteDecodeIn");
      return SandboxingKind::COUNT;
  }
}

RemoteDecodeIn GetRemoteDecodeInFromKind(SandboxingKind aKind) {
  switch (aKind) {
    case SandboxingKind::UTILITY_AUDIO_DECODING_GENERIC:
      return RemoteDecodeIn::UtilityProcess_Generic;
#ifdef MOZ_APPLEMEDIA
    case SandboxingKind::UTILITY_AUDIO_DECODING_APPLE_MEDIA:
      return RemoteDecodeIn::UtilityProcess_AppleMedia;
#endif
#ifdef XP_WIN
    case SandboxingKind::UTILITY_AUDIO_DECODING_WMF:
      return RemoteDecodeIn::UtilityProcess_WMF;
#endif
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported SandboxingKind");
      return RemoteDecodeIn::Unspecified;
  }
}

const char* RemoteDecodeInToStr(RemoteDecodeIn aLocation) {
  switch (aLocation) {
    case RemoteDecodeIn::RddProcess:
      return "RDD";
    case RemoteDecodeIn::GpuProcess:
      return "GPU";
    case RemoteDecodeIn::UtilityProcess_Generic:
      return "Utility Generic";
#ifdef MOZ_APPLEMEDIA
    case RemoteDecodeIn::UtilityProcess_AppleMedia:
      return "Utility AppleMedia";
#endif
#ifdef XP_WIN
    case RemoteDecodeIn::UtilityProcess_WMF:
      return "Utility WMF";
#endif
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported RemoteDecodeIn");
      return "Unknown";
  }
}

}  // namespace mozilla
