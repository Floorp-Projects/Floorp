/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_EME_KEYSYSTEMCONFIG_H_
#define DOM_MEDIA_EME_KEYSYSTEMCONFIG_H_

#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/dom/MediaKeysBinding.h"

namespace mozilla {

struct KeySystemConfig {
 public:
  // EME MediaKeysRequirement:
  // https://www.w3.org/TR/encrypted-media/#dom-mediakeysrequirement
  enum class Requirement {
    Required = 1,
    Optional = 2,
    NotAllowed = 3,
  };

  // EME MediaKeySessionType:
  // https://www.w3.org/TR/encrypted-media/#dom-mediakeysessiontype
  enum class SessionType {
    Temporary = 1,
    PersistentLicense = 2,
  };

  using EMECodecString = nsCString;
  static constexpr auto EME_CODEC_AAC = "aac"_ns;
  static constexpr auto EME_CODEC_OPUS = "opus"_ns;
  static constexpr auto EME_CODEC_VORBIS = "vorbis"_ns;
  static constexpr auto EME_CODEC_FLAC = "flac"_ns;
  static constexpr auto EME_CODEC_H264 = "h264"_ns;
  static constexpr auto EME_CODEC_VP8 = "vp8"_ns;
  static constexpr auto EME_CODEC_VP9 = "vp9"_ns;
  static constexpr auto EME_CODEC_HEVC = "hevc"_ns;

  using EMEEncryptionSchemeString = nsCString;
  static constexpr auto EME_ENCRYPTION_SCHEME_CENC = "cenc"_ns;
  static constexpr auto EME_ENCRYPTION_SCHEME_CBCS = "cbcs"_ns;

  // A codec can be decrypted-and-decoded by the CDM, or only decrypted
  // by the CDM and decoded by Gecko. Not both.
  struct ContainerSupport {
    ContainerSupport() = default;
    ~ContainerSupport() = default;
    ContainerSupport(const ContainerSupport& aOther) {
      mCodecsDecoded = aOther.mCodecsDecoded.Clone();
      mCodecsDecrypted = aOther.mCodecsDecrypted.Clone();
    }
    ContainerSupport& operator=(const ContainerSupport& aOther) {
      if (this == &aOther) {
        return *this;
      }
      mCodecsDecoded = aOther.mCodecsDecoded.Clone();
      mCodecsDecrypted = aOther.mCodecsDecrypted.Clone();
      return *this;
    }
    ContainerSupport(ContainerSupport&& aOther) = default;
    ContainerSupport& operator=(ContainerSupport&&) = default;

    bool IsSupported() const {
      return !mCodecsDecoded.IsEmpty() || !mCodecsDecrypted.IsEmpty();
    }

    // CDM decrypts and decodes using a DRM robust decoder, and passes decoded
    // samples back to Gecko for rendering.
    bool DecryptsAndDecodes(const EMECodecString& aCodec) const {
      return mCodecsDecoded.Contains(aCodec);
    }

    // CDM decrypts and passes the decrypted samples back to Gecko for decoding.
    bool Decrypts(const EMECodecString& aCodec) const {
      return mCodecsDecrypted.Contains(aCodec);
    }

    void SetCanDecryptAndDecode(const EMECodecString& aCodec) {
      // Can't both decrypt and decrypt-and-decode a codec.
      MOZ_ASSERT(!Decrypts(aCodec));
      // Prevent duplicates.
      MOZ_ASSERT(!DecryptsAndDecodes(aCodec));
      mCodecsDecoded.AppendElement(aCodec);
    }

    void SetCanDecrypt(const EMECodecString& aCodec) {
      // Prevent duplicates.
      MOZ_ASSERT(!Decrypts(aCodec));
      // Can't both decrypt and decrypt-and-decode a codec.
      MOZ_ASSERT(!DecryptsAndDecodes(aCodec));
      mCodecsDecrypted.AppendElement(aCodec);
    }

    EMECodecString GetDebugInfo() const {
      EMECodecString info;
      info.AppendLiteral("decoding-and-decrypting:[");
      for (size_t idx = 0; idx < mCodecsDecoded.Length(); idx++) {
        info.Append(mCodecsDecoded[idx]);
        if (idx + 1 < mCodecsDecoded.Length()) {
          info.AppendLiteral(",");
        }
      }
      info.AppendLiteral("],");
      info.AppendLiteral("decrypting-only:[");
      for (size_t idx = 0; idx < mCodecsDecrypted.Length(); idx++) {
        info.Append(mCodecsDecrypted[idx]);
        if (idx + 1 < mCodecsDecrypted.Length()) {
          info.AppendLiteral(",");
        }
      }
      info.AppendLiteral("]");
      return info;
    }

   private:
    nsTArray<EMECodecString> mCodecsDecoded;
    nsTArray<EMECodecString> mCodecsDecrypted;
  };

  // Return true if given key system is supported on the current device.
  static bool Supports(const nsAString& aKeySystem);
  static bool CreateKeySystemConfigs(const nsAString& aKeySystem,
                                     nsTArray<KeySystemConfig>& aOutConfigs);
  static void GetGMPKeySystemConfigs(dom::Promise* aPromise);

  KeySystemConfig() = default;
  ~KeySystemConfig() = default;
  explicit KeySystemConfig(const KeySystemConfig& aOther) {
    mKeySystem = aOther.mKeySystem;
    mInitDataTypes = aOther.mInitDataTypes.Clone();
    mPersistentState = aOther.mPersistentState;
    mDistinctiveIdentifier = aOther.mDistinctiveIdentifier;
    mSessionTypes = aOther.mSessionTypes.Clone();
    mVideoRobustness = aOther.mVideoRobustness.Clone();
    mAudioRobustness = aOther.mAudioRobustness.Clone();
    mEncryptionSchemes = aOther.mEncryptionSchemes.Clone();
    mMP4 = aOther.mMP4;
    mWebM = aOther.mWebM;
  }
  KeySystemConfig& operator=(const KeySystemConfig& aOther) {
    if (this == &aOther) {
      return *this;
    }
    mKeySystem = aOther.mKeySystem;
    mInitDataTypes = aOther.mInitDataTypes.Clone();
    mPersistentState = aOther.mPersistentState;
    mDistinctiveIdentifier = aOther.mDistinctiveIdentifier;
    mSessionTypes = aOther.mSessionTypes.Clone();
    mVideoRobustness = aOther.mVideoRobustness.Clone();
    mAudioRobustness = aOther.mAudioRobustness.Clone();
    mEncryptionSchemes = aOther.mEncryptionSchemes.Clone();
    mMP4 = aOther.mMP4;
    mWebM = aOther.mWebM;
    return *this;
  }
  KeySystemConfig(KeySystemConfig&&) = default;
  KeySystemConfig& operator=(KeySystemConfig&&) = default;

  nsString GetDebugInfo() const;

  // Return true if the given key system is equal to `mKeySystem`, or it can be
  // mapped to the same key system
  bool IsSameKeySystem(const nsAString& aKeySystem) const;

  nsString mKeySystem;
  nsTArray<nsString> mInitDataTypes;
  Requirement mPersistentState = Requirement::NotAllowed;
  Requirement mDistinctiveIdentifier = Requirement::NotAllowed;
  nsTArray<SessionType> mSessionTypes;
  nsTArray<nsString> mVideoRobustness;
  nsTArray<nsString> mAudioRobustness;
  nsTArray<nsString> mEncryptionSchemes;
  ContainerSupport mMP4;
  ContainerSupport mWebM;
};

KeySystemConfig::SessionType ConvertToKeySystemConfigSessionType(
    dom::MediaKeySessionType aType);
const char* SessionTypeToStr(KeySystemConfig::SessionType aType);
const char* RequirementToStr(KeySystemConfig::Requirement aRequirement);

}  // namespace mozilla

#endif  // DOM_MEDIA_EME_KEYSYSTEMCONFIG_H_
