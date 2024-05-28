/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_EME_KEYSYSTEMCONFIG_H_
#define DOM_MEDIA_EME_KEYSYSTEMCONFIG_H_

#include "MediaData.h"
#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/MozPromise.h"
#include "mozilla/dom/MediaKeysBinding.h"
#include "mozilla/dom/MediaKeySystemAccessBinding.h"

namespace mozilla {

struct KeySystemConfigRequest;

struct KeySystemConfig {
 public:
  using SupportedConfigsPromise =
      MozPromise<nsTArray<KeySystemConfig>, bool /* aIgnored */,
                 /* IsExclusive = */ true>;
  using KeySystemConfigPromise =
      MozPromise<dom::MediaKeySystemConfiguration, bool /* aIgnored */,
                 /* IsExclusive = */ true>;

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
  static constexpr auto EME_CODEC_AV1 = "av1"_ns;
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

    // True if a codec and scheme pair can be decrypted and decoded
    bool DecryptsAndDecodes(
        const EMECodecString& aCodec,
        const Maybe<CryptoScheme>& aScheme = Nothing()) const {
      return CheckCodecAndSchemePair(mCodecsDecoded, aCodec, aScheme);
    }

    // True if a codec and scheme pair can be decrypted
    bool Decrypts(const EMECodecString& aCodec,
                  const Maybe<CryptoScheme>& aScheme = Nothing()) const {
      return CheckCodecAndSchemePair(mCodecsDecrypted, aCodec, aScheme);
    }

    void SetCanDecryptAndDecode(
        const EMECodecString& aCodec,
        const Maybe<CryptoSchemeSet>& aSchemes = Nothing{}) {
      // Can't both decrypt and decrypt-and-decode a codec.
      MOZ_ASSERT(!ContainsDecryptedOnlyCodec(aCodec));
      // Prevent duplicates.
      MOZ_ASSERT(!ContainsDecryptedAndDecodedCodec(aCodec));

      mCodecsDecoded.AppendElement(CodecSchemePair{aCodec, aSchemes});
    }

    void SetCanDecrypt(const EMECodecString& aCodec,
                       const Maybe<CryptoSchemeSet>& aSchemes = Nothing{}) {
      // Prevent duplicates.
      MOZ_ASSERT(!ContainsDecryptedOnlyCodec(aCodec));
      // Can't both decrypt and decrypt-and-decode a codec.
      MOZ_ASSERT(!ContainsDecryptedAndDecodedCodec(aCodec));
      mCodecsDecrypted.AppendElement(CodecSchemePair{aCodec, aSchemes});
    }

    EMECodecString GetDebugInfo() const {
      EMECodecString info;
      info.AppendLiteral("decoding-and-decrypting:[");
      for (size_t idx = 0; idx < mCodecsDecoded.Length(); idx++) {
        const auto& cur = mCodecsDecoded[idx];
        info.Append(cur.first);
        if (cur.second) {
          info.AppendLiteral("(");
          info.Append(CryptoSchemeSetToString(*cur.second));
          info.AppendLiteral(")");
        } else {
          info.AppendLiteral("(all)");
        }
        if (idx + 1 < mCodecsDecoded.Length()) {
          info.AppendLiteral(",");
        }
      }
      info.AppendLiteral("],");
      info.AppendLiteral("decrypting-only:[");
      for (size_t idx = 0; idx < mCodecsDecrypted.Length(); idx++) {
        const auto& cur = mCodecsDecrypted[idx];
        info.Append(cur.first);
        if (cur.second) {
          info.AppendLiteral("(");
          info.Append(CryptoSchemeSetToString(*cur.second));
          info.AppendLiteral(")");
        } else {
          info.AppendLiteral("(all)");
        }
        if (idx + 1 < mCodecsDecrypted.Length()) {
          info.AppendLiteral(",");
        }
      }
      info.AppendLiteral("]");
      return info;
    }

   private:
    using CodecSchemePair = std::pair<EMECodecString, Maybe<CryptoSchemeSet>>;
    // These two arrays are exclusive, the codec in one array can't appear on
    // another array. If CryptoSchemeSet is nothing, that means the codec has
    // support for all schemes, which is our default. Setting CryptoSchemeSet
    // explicitly can restrict avaiable schemes for a codec.
    nsTArray<CodecSchemePair> mCodecsDecoded;
    nsTArray<CodecSchemePair> mCodecsDecrypted;

    bool ContainsDecryptedOnlyCodec(const EMECodecString& aCodec) const {
      return std::any_of(
          mCodecsDecrypted.begin(), mCodecsDecrypted.end(),
          [&](const auto& aPair) { return aPair.first.Equals(aCodec); });
    }
    bool ContainsDecryptedAndDecodedCodec(const EMECodecString& aCodec) const {
      return std::any_of(
          mCodecsDecoded.begin(), mCodecsDecoded.end(),
          [&](const auto& aPair) { return aPair.first.Equals(aCodec); });
    }
    bool CheckCodecAndSchemePair(const nsTArray<CodecSchemePair>& aArray,
                                 const EMECodecString& aCodec,
                                 const Maybe<CryptoScheme>& aScheme) const {
      return std::any_of(aArray.begin(), aArray.end(), [&](const auto& aPair) {
        if (!aPair.first.Equals(aCodec)) {
          return false;
        }
        // No scheme is specified, which means accepting all schemes.
        if (!aPair.second || !aScheme) {
          return true;
        }
        return aPair.second->contains(*aScheme);
      });
    }
  };

  // Return true if given key system is supported on the current device.
  static bool Supports(const nsAString& aKeySystem);

  enum class DecryptionInfo : uint8_t {
    Software,
    Hardware,
  };
  static RefPtr<SupportedConfigsPromise> CreateKeySystemConfigs(
      const nsTArray<KeySystemConfigRequest>& aRequests);
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
    mMP4 = aOther.mMP4;
    mWebM = aOther.mWebM;
    return *this;
  }
  KeySystemConfig(KeySystemConfig&&) = default;
  KeySystemConfig& operator=(KeySystemConfig&&) = default;

  nsString GetDebugInfo() const;

  nsString mKeySystem;
  nsTArray<nsString> mInitDataTypes;
  Requirement mPersistentState = Requirement::NotAllowed;
  Requirement mDistinctiveIdentifier = Requirement::NotAllowed;
  nsTArray<SessionType> mSessionTypes;
  nsTArray<nsString> mVideoRobustness;
  nsTArray<nsString> mAudioRobustness;
  ContainerSupport mMP4;
  ContainerSupport mWebM;
  bool mIsHDCP22Compatible = false;

 private:
  static void CreateClearKeyKeySystemConfigs(
      const KeySystemConfigRequest& aRequest,
      nsTArray<KeySystemConfig>& aOutConfigs);
  static void CreateWivineL3KeySystemConfigs(
      const KeySystemConfigRequest& aRequest,
      nsTArray<KeySystemConfig>& aOutConfigs);
};

struct KeySystemConfigRequest final {
  KeySystemConfigRequest(const nsAString& aKeySystem,
                         KeySystemConfig::DecryptionInfo aDecryption,
                         bool aIsPrivateBrowsing)
      : mKeySystem(aKeySystem),
        mDecryption(aDecryption),
        mIsPrivateBrowsing(aIsPrivateBrowsing) {}
  const nsString mKeySystem;
  const KeySystemConfig::DecryptionInfo mDecryption;
  const bool mIsPrivateBrowsing;
};

KeySystemConfig::SessionType ConvertToKeySystemConfigSessionType(
    dom::MediaKeySessionType aType);
const char* SessionTypeToStr(KeySystemConfig::SessionType aType);
const char* RequirementToStr(KeySystemConfig::Requirement aRequirement);

}  // namespace mozilla

#endif  // DOM_MEDIA_EME_KEYSYSTEMCONFIG_H_
