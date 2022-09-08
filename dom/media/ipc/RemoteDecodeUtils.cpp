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
#ifdef MOZ_WMF_MEDIA_ENGINE
    case RemoteDecodeIn::UtilityProcess_MFMediaEngineCDM:
      return SandboxingKind::MF_MEDIA_ENGINE_CDM;
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
#ifdef MOZ_WMF_MEDIA_ENGINE
    case SandboxingKind::MF_MEDIA_ENGINE_CDM:
      return RemoteDecodeIn::UtilityProcess_MFMediaEngineCDM;
#endif
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported SandboxingKind");
      return RemoteDecodeIn::Unspecified;
  }
}

RemoteDecodeIn GetRemoteDecodeInFromVideoBridgeSource(
    layers::VideoBridgeSource aSource) {
  switch (aSource) {
    case layers::VideoBridgeSource::RddProcess:
      return RemoteDecodeIn::RddProcess;
    case layers::VideoBridgeSource::GpuProcess:
      return RemoteDecodeIn::GpuProcess;
    case layers::VideoBridgeSource::MFMediaEngineCDMProcess:
      return RemoteDecodeIn::UtilityProcess_MFMediaEngineCDM;
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported VideoBridgeSource");
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
#ifdef MOZ_WMF_MEDIA_ENGINE
    case RemoteDecodeIn::UtilityProcess_MFMediaEngineCDM:
      return "Utility MF Media Engine CDM";
#endif
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported RemoteDecodeIn");
      return "Unknown";
  }
}

}  // namespace mozilla
