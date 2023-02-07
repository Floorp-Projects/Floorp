/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFCDMParent.h"

#include <combaseapi.h>
#include <intsafe.h>
#include <mfmediaengine.h>
#include <propsys.h>
#include <wtypes.h>

#include "mozilla/EMEUtils.h"
#include "mozilla/KeySystemConfig.h"
#include "WMFUtils.h"

namespace mozilla {

#define MFCDM_PARENT_LOG(msg, ...) \
  EME_LOG("MFCDMParent[%p]@%s: " msg, this, __func__, ##__VA_ARGS__)
#define MFCDM_PARENT_SLOG(msg, ...) \
  EME_LOG("MFCDMParent@%s: " msg, __func__, ##__VA_ARGS__)

#define MFCDM_RETURN_IF_FAILED(x)                      \
  do {                                                 \
    HRESULT rv = x;                                    \
    if (MOZ_UNLIKELY(FAILED(rv))) {                    \
      MFCDM_PARENT_LOG("(" #x ") failed, rv=%lx", rv); \
      return rv;                                       \
    }                                                  \
  } while (false)

#define MFCDM_REJECT_IF(pred, rv)                            \
  do {                                                       \
    if (MOZ_UNLIKELY(pred)) {                                \
      MFCDM_PARENT_LOG("reject for [" #pred "], rv=%x", rv); \
      aResolver(rv);                                         \
      return IPC_OK();                                       \
    }                                                        \
  } while (false)

HRESULT MFCDMParent::LoadFactory() {
  Microsoft::WRL::ComPtr<IMFMediaEngineClassFactory4> clsFactory;
  MFCDM_RETURN_IF_FAILED(CoCreateInstance(CLSID_MFMediaEngineClassFactory,
                                          nullptr, CLSCTX_INPROC_SERVER,
                                          IID_PPV_ARGS(&clsFactory)));

  Microsoft::WRL::ComPtr<IMFContentDecryptionModuleFactory> cdmFactory;
  MFCDM_RETURN_IF_FAILED(clsFactory->CreateContentDecryptionModuleFactory(
      mKeySystem.get(), IID_PPV_ARGS(&cdmFactory)));

  mFactory.Swap(cdmFactory);
  return S_OK;
}

// Use IMFContentDecryptionModuleFactory::IsTypeSupported() to get DRM
// capabilities. It appears to be the same as
// Windows.Media.Protection.ProtectionCapabilities.IsTypeSupported(). See
// https://learn.microsoft.com/en-us/uwp/api/windows.media.protection.protectioncapabilities.istypesupported?view=winrt-19041
static bool FactorySupports(
    Microsoft::WRL::ComPtr<IMFContentDecryptionModuleFactory>& aFactory,
    const nsString& aKeySystem,
    const KeySystemConfig::EMECodecString& aVideoCodec,
    const KeySystemConfig::EMECodecString& aAudioCodec =
        KeySystemConfig::EMECodecString(""),
    const nsString& aAdditionalFeature = nsString(u"")) {
  // MP4 is the only container supported.
  nsString mimeType(u"video/mp4;codecs=\"");
  MOZ_ASSERT(!aVideoCodec.IsEmpty());
  mimeType.AppendASCII(aVideoCodec);
  if (!aAudioCodec.IsEmpty()) {
    mimeType.AppendLiteral(u",");
    mimeType.AppendASCII(aAudioCodec);
  }
  // These features are required to call IsTypeSupported(). We only care about
  // codec and encryption scheme so hardcode the rest.
  mimeType.AppendLiteral(
      u"\";features=\"decode-bpp=8,"
      "decode-res-x=1920,decode-res-y=1080,"
      "decode-bitrate=10000000,decode-fps=30,");
  if (!aAdditionalFeature.IsEmpty()) {
    MOZ_ASSERT(aAdditionalFeature.Last() == u',');
    mimeType.Append(aAdditionalFeature);
  }
  // TODO: add HW robustness setting too.
  mimeType.AppendLiteral(u"encryption-robustness=SW_SECURE_DECODE\"");
  return aFactory->IsTypeSupported(aKeySystem.get(), mimeType.get());
}

mozilla::ipc::IPCResult MFCDMParent::RecvGetCapabilities(
    GetCapabilitiesResolver&& aResolver) {
  MFCDM_REJECT_IF(!mFactory, NS_ERROR_DOM_NOT_SUPPORTED_ERR);

  MFCDMCapabilitiesIPDL capabilities;

  // TODO: check HW CDM creation

  static nsTArray<KeySystemConfig::EMECodecString> kVideoCodecs({
      KeySystemConfig::EME_CODEC_H264,
      KeySystemConfig::EME_CODEC_VP8,
      KeySystemConfig::EME_CODEC_VP9,
  });
  // Remember supported video codecs.
  // It will be used when collecting audio codec and encryption scheme support.
  nsTArray<KeySystemConfig::EMECodecString> supportedVideoCodecs;
  for (auto& codec : kVideoCodecs) {
    if (FactorySupports(mFactory, mKeySystem, codec)) {
      MFCDMMediaCapability* c =
          capabilities.videoCapabilities().AppendElement();
      c->contentType() = NS_ConvertUTF8toUTF16(codec);
      MFCDM_PARENT_LOG("%s: +video:%s", __func__, codec.get());
      supportedVideoCodecs.AppendElement(codec);
    }
  }
  if (supportedVideoCodecs.IsEmpty()) {
    // Return empty capabilities when no video codec is supported.
    aResolver(std::move(capabilities));
    return IPC_OK();
  }

  static nsTArray<KeySystemConfig::EMECodecString> kAudioCodecs({
      KeySystemConfig::EME_CODEC_AAC,
      KeySystemConfig::EME_CODEC_FLAC,
      KeySystemConfig::EME_CODEC_OPUS,
      KeySystemConfig::EME_CODEC_VORBIS,
  });
  for (auto& codec : kAudioCodecs) {
    if (FactorySupports(mFactory, mKeySystem, supportedVideoCodecs[0], codec)) {
      MFCDMMediaCapability* c =
          capabilities.audioCapabilities().AppendElement();
      c->contentType() = NS_ConvertUTF8toUTF16(codec);
      MFCDM_PARENT_LOG("%s: +audio:%s", __func__, codec.get());
    }
  }

  // Collect schemes supported by all video codecs.
  static nsTArray<std::pair<CryptoScheme, nsDependentString>> kSchemes = {
      std::pair<CryptoScheme, nsDependentString>(
          CryptoScheme::Cenc, u"encryption-type=cenc,encryption-iv-size=8,"),
      std::pair<CryptoScheme, nsDependentString>(
          CryptoScheme::Cbcs, u"encryption-type=cbcs,encryption-iv-size=16,")};
  for (auto& scheme : kSchemes) {
    bool ok = true;
    for (auto& codec : supportedVideoCodecs) {
      ok &= FactorySupports(mFactory, mKeySystem, codec,
                            KeySystemConfig::EMECodecString(""),
                            scheme.second /* additional feature */);
      if (!ok) {
        break;
      }
    }
    if (ok) {
      capabilities.encryptionSchemes().AppendElement(scheme.first);
      MFCDM_PARENT_LOG("%s: +scheme:%s", __func__,
                       scheme.first == CryptoScheme::Cenc ? "cenc" : "cbcs");
    }
  }

  // TODO: don't hardcode
  capabilities.initDataTypes().AppendElement(u"keyids");
  capabilities.initDataTypes().AppendElement(u"cenc");
  capabilities.sessionTypes().AppendElement(
      KeySystemConfig::SessionType::Temporary);
  capabilities.sessionTypes().AppendElement(
      KeySystemConfig::SessionType::PersistentLicense);
  // WMF CDMs usually require these. See
  // https://source.chromium.org/chromium/chromium/src/+/main:media/cdm/win/media_foundation_cdm_factory.cc;l=69-73;drc=b3ca5c09fa0aa07b7f9921501f75e43d80f3ba48
  capabilities.persistentState() = KeySystemConfig::Requirement::Required;
  capabilities.distinctiveID() = KeySystemConfig::Requirement::Required;

  capabilities.keySystem() = mKeySystem;

  aResolver(std::move(capabilities));
  return IPC_OK();
}

#undef MFCDM_REJECT_IF
#undef MFCDM_RETURN_IF_FAILED
#undef MFCDM_PARENT_SLOG
#undef MFCDM_PARENT_LOG

}  // namespace mozilla
