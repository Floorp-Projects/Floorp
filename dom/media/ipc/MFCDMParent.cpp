/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFCDMParent.h"

#include <mfmediaengine.h>
#define INITGUID          // Enable DEFINE_PROPERTYKEY()
#include <propkeydef.h>   // For DEFINE_PROPERTYKEY() definition
#include <propvarutil.h>  // For InitPropVariantFrom*()

#include "mozilla/dom/KeySystemNames.h"
#include "mozilla/EMEUtils.h"
#include "mozilla/KeySystemConfig.h"
#include "RemoteDecodeUtils.h"       // For GetCurrentSandboxingKind()
#include "SpecialSystemDirectory.h"  // For temp dir

namespace mozilla {

// See
// https://source.chromium.org/chromium/chromium/src/+/main:media/cdm/win/media_foundation_cdm_util.cc;l=26-40;drc=503535015a7b373cc6185c69c991e01fda5da571
#ifndef EME_CONTENTDECRYPTIONMODULE_ORIGIN_ID
DEFINE_PROPERTYKEY(EME_CONTENTDECRYPTIONMODULE_ORIGIN_ID, 0x1218a3e2, 0xcfb0,
                   0x4c98, 0x90, 0xe5, 0x5f, 0x58, 0x18, 0xd4, 0xb6, 0x7e,
                   PID_FIRST_USABLE);
#endif

#define MFCDM_PARENT_LOG(msg, ...) \
  EME_LOG("MFCDMParent[%p]@%s: " msg, this, __func__, ##__VA_ARGS__)
#define MFCDM_PARENT_SLOG(msg, ...) \
  EME_LOG("MFCDMParent@%s: " msg, __func__, ##__VA_ARGS__)

#define MFCDM_RETURN_IF_FAILED(x)                       \
  do {                                                  \
    HRESULT rv = x;                                     \
    if (MOZ_UNLIKELY(FAILED(rv))) {                     \
      MFCDM_PARENT_SLOG("(" #x ") failed, rv=%lx", rv); \
      return rv;                                        \
    }                                                   \
  } while (false)

#define MFCDM_REJECT_IF(pred, rv)                            \
  do {                                                       \
    if (MOZ_UNLIKELY(pred)) {                                \
      MFCDM_PARENT_LOG("reject for [" #pred "], rv=%x", rv); \
      aResolver(rv);                                         \
      return IPC_OK();                                       \
    }                                                        \
  } while (false)

#define MFCDM_REJECT_IF_FAILED(op, rv)                             \
  do {                                                             \
    HRESULT hr = op;                                               \
    if (MOZ_UNLIKELY(FAILED(hr))) {                                \
      MFCDM_PARENT_LOG("(" #op ") failed(hr=%lx), rv=%x", hr, rv); \
      aResolver(rv);                                               \
      return IPC_OK();                                             \
    }                                                              \
  } while (false)

MFCDMParent::MFCDMParent(const nsAString& aKeySystem,
                         RemoteDecoderManagerParent* aManager,
                         nsISerialEventTarget* aManagerThread)
    : mKeySystem(aKeySystem),
      mManager(aManager),
      mManagerThread(aManagerThread),
      mId(sNextId++) {
  // TODO: check Widevine too when it's ready.
  MOZ_ASSERT(aKeySystem.EqualsLiteral(kPlayReadyKeySystemName));
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(aManagerThread);
  MOZ_ASSERT(XRE_IsUtilityProcess());
  MOZ_ASSERT(GetCurrentSandboxingKind() ==
             ipc::SandboxingKind::MF_MEDIA_ENGINE_CDM);

  mIPDLSelfRef = this;
  LoadFactory();
  Register();
}

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
  static nsTArray<std::pair<CryptoScheme, nsString>> kSchemes = {
      std::pair<CryptoScheme, nsString>(
          CryptoScheme::Cenc, u"encryption-type=cenc,encryption-iv-size=8"),
      std::pair<CryptoScheme, nsString>(
          CryptoScheme::Cbcs, u"encryption-type=cbcs,encryption-iv-size=16")};
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

// RAIIized PROPVARIANT. See
// third_party/libwebrtc/modules/audio_device/win/core_audio_utility_win.h
class AutoPropVar {
 public:
  AutoPropVar() { PropVariantInit(&mVar); }

  ~AutoPropVar() { Reset(); }

  AutoPropVar(const AutoPropVar&) = delete;
  AutoPropVar& operator=(const AutoPropVar&) = delete;
  bool operator==(const AutoPropVar&) const = delete;
  bool operator!=(const AutoPropVar&) const = delete;

  // Returns a pointer to the underlying PROPVARIANT for use as an out param
  // in a function call.
  PROPVARIANT* Receive() {
    MOZ_ASSERT(mVar.vt == VT_EMPTY);
    return &mVar;
  }

  // Clears the instance to prepare it for re-use (e.g., via Receive).
  void Reset() {
    if (mVar.vt != VT_EMPTY) {
      HRESULT hr = PropVariantClear(&mVar);
      MOZ_ASSERT(SUCCEEDED(hr));
      Unused << hr;
    }
  }

  const PROPVARIANT& get() const { return mVar; }
  const PROPVARIANT* ptr() const { return &mVar; }

 private:
  PROPVARIANT mVar;
};

static MF_MEDIAKEYS_REQUIREMENT ToMFRequirement(
    const KeySystemConfig::Requirement aRequirement) {
  switch (aRequirement) {
    case KeySystemConfig::Requirement::NotAllowed:
      return MF_MEDIAKEYS_REQUIREMENT_NOT_ALLOWED;
    case KeySystemConfig::Requirement::Optional:
      return MF_MEDIAKEYS_REQUIREMENT_OPTIONAL;
    case KeySystemConfig::Requirement::Required:
      return MF_MEDIAKEYS_REQUIREMENT_REQUIRED;
  }
};

static HRESULT BuildCDMAccessConfig(
    const MFCDMInitParamsIPDL& aParams,
    Microsoft::WRL::ComPtr<IPropertyStore>& aConfig) {
  Microsoft::WRL::ComPtr<IPropertyStore>
      mksc;  // EME MediaKeySystemConfiguration
  MFCDM_RETURN_IF_FAILED(PSCreateMemoryPropertyStore(IID_PPV_ARGS(&mksc)));

  // Empty 'audioCapabilities'.
  AutoPropVar audioCapabilities;
  PROPVARIANT* var = audioCapabilities.Receive();
  var->vt = VT_VARIANT | VT_VECTOR;
  var->capropvar.cElems = 0;
  MFCDM_RETURN_IF_FAILED(
      mksc->SetValue(MF_EME_AUDIOCAPABILITIES, audioCapabilities.get()));

  // 'videoCapabilites'.
  Microsoft::WRL::ComPtr<IPropertyStore>
      mksmc;  // EME MediaKeySystemMediaCapability
  MFCDM_RETURN_IF_FAILED(PSCreateMemoryPropertyStore(IID_PPV_ARGS(&mksmc)));
  AutoPropVar robustness;
  if (aParams.hwSecure()) {
    var = robustness.Receive();
    var->vt = VT_BSTR;
    var->bstrVal = SysAllocString(L"HW_SECURE_ALL");
    MFCDM_RETURN_IF_FAILED(
        mksmc->SetValue(MF_EME_ROBUSTNESS, robustness.get()));
  }
  // Store mksmc in a PROPVARIANT.
  AutoPropVar videoCapability;
  var = videoCapability.Receive();
  var->vt = VT_UNKNOWN;
  var->punkVal = mksmc.Detach();
  // Insert element.
  AutoPropVar videoCapabilities;
  var = videoCapabilities.Receive();
  var->vt = VT_VARIANT | VT_VECTOR;
  var->capropvar.cElems = 1;
  var->capropvar.pElems =
      reinterpret_cast<PROPVARIANT*>(CoTaskMemAlloc(sizeof(PROPVARIANT)));
  PropVariantCopy(var->capropvar.pElems, videoCapability.ptr());
  MFCDM_RETURN_IF_FAILED(
      mksc->SetValue(MF_EME_VIDEOCAPABILITIES, videoCapabilities.get()));

  AutoPropVar persistState;
  InitPropVariantFromUInt32(ToMFRequirement(aParams.persistentState()),
                            persistState.Receive());
  MFCDM_RETURN_IF_FAILED(
      mksc->SetValue(MF_EME_PERSISTEDSTATE, persistState.get()));

  AutoPropVar distinctiveID;
  InitPropVariantFromUInt32(ToMFRequirement(aParams.distinctiveID()),
                            distinctiveID.Receive());
  MFCDM_RETURN_IF_FAILED(
      mksc->SetValue(MF_EME_DISTINCTIVEID, distinctiveID.get()));

  aConfig.Swap(mksc);
  return S_OK;
}

static HRESULT BuildCDMProperties(
    const nsString& aOrigin, Microsoft::WRL::ComPtr<IPropertyStore>& aProps) {
  MOZ_ASSERT(!aOrigin.IsEmpty());

  Microsoft::WRL::ComPtr<IPropertyStore> props;
  MFCDM_RETURN_IF_FAILED(PSCreateMemoryPropertyStore(IID_PPV_ARGS(&props)));

  AutoPropVar origin;
  MFCDM_RETURN_IF_FAILED(
      InitPropVariantFromString(aOrigin.get(), origin.Receive()));
  MFCDM_RETURN_IF_FAILED(
      props->SetValue(EME_CONTENTDECRYPTIONMODULE_ORIGIN_ID, origin.get()));

  // TODO: support client token?

  // TODO: CDM store path per profile?
  nsCOMPtr<nsIFile> dir;
  if (NS_FAILED(GetSpecialSystemDirectory(OS_TemporaryDirectory,
                                          getter_AddRefs(dir)))) {
    return E_ACCESSDENIED;
  }
  if (NS_FAILED(dir->AppendNative(nsDependentCString("mfcdm")))) {
    return E_ACCESSDENIED;
  }
  nsresult rv = dir->Create(nsIFile::DIRECTORY_TYPE, 0700);
  if (rv != NS_ERROR_FILE_ALREADY_EXISTS && NS_FAILED(rv)) {
    return E_ACCESSDENIED;
  }
  nsAutoString cdmStorePath;
  if (NS_FAILED(dir->GetPath(cdmStorePath))) {
    return E_ACCESSDENIED;
  }

  AutoPropVar path;
  MFCDM_RETURN_IF_FAILED(
      InitPropVariantFromString(cdmStorePath.get(), path.Receive()));
  MFCDM_RETURN_IF_FAILED(
      props->SetValue(MF_CONTENTDECRYPTIONMODULE_STOREPATH, path.get()));

  aProps.Swap(props);
  return S_OK;
}

mozilla::ipc::IPCResult MFCDMParent::RecvInit(
    const MFCDMInitParamsIPDL& aParams, InitResolver&& aResolver) {
  // Get access object to CDM.
  Microsoft::WRL::ComPtr<IPropertyStore> accessConfig;
  MFCDM_REJECT_IF_FAILED(BuildCDMAccessConfig(aParams, accessConfig),
                         NS_ERROR_FAILURE);

  AutoTArray<IPropertyStore*, 1> configs = {accessConfig.Get()};
  Microsoft::WRL::ComPtr<IMFContentDecryptionModuleAccess> cdmAccess;
  MFCDM_REJECT_IF_FAILED(
      mFactory->CreateContentDecryptionModuleAccess(
          mKeySystem.get(), configs.Elements(), configs.Length(), &cdmAccess),
      NS_ERROR_FAILURE);
  // Get CDM.
  Microsoft::WRL::ComPtr<IPropertyStore> cdmProps;
  MFCDM_REJECT_IF_FAILED(BuildCDMProperties(aParams.origin(), cdmProps),
                         NS_ERROR_FAILURE);
  Microsoft::WRL::ComPtr<IMFContentDecryptionModule> cdm;
  MFCDM_REJECT_IF_FAILED(
      cdmAccess->CreateContentDecryptionModule(cdmProps.Get(), &cdm),
      NS_ERROR_FAILURE);

  mCDM.Swap(cdm);
  aResolver(MFCDMInitIPDL{mId});
  return IPC_OK();
}

#undef MFCDM_REJECT_IF_FAILED
#undef MFCDM_REJECT_IF
#undef MFCDM_RETURN_IF_FAILED
#undef MFCDM_PARENT_SLOG
#undef MFCDM_PARENT_LOG

}  // namespace mozilla
