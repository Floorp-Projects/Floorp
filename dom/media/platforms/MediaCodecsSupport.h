/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef DOM_MEDIA_PLATFORMS_MEDIACODECSSUPPORT_H_
#define DOM_MEDIA_PLATFORMS_MEDIACODECSSUPPORT_H_
#include <array>

#include "mozilla/Atomics.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/EnumSet.h"
#include "mozilla/StaticMutex.h"
#include "nsString.h"
#include "nsTHashMap.h"
#include "nsThreadUtils.h"

namespace mozilla::media {
// List of codecs we support, used in the below macros
// to generate MediaCodec and MediaCodecSupports enums.
#define CODEC_LIST \
  X(H264)          \
  X(VP8)           \
  X(VP9)           \
  X(AV1)           \
  X(HEVC)          \
  X(Theora)        \
  X(AAC)           \
  X(FLAC)          \
  X(MP3)           \
  X(Opus)          \
  X(Vorbis)        \
  X(Wave)

// Generate MediaCodec enum with the names of each codec we support.
// Example: MediaCodec::H264
enum class MediaCodec : int {
#define X(name) name,
  CODEC_LIST
#undef X
      SENTINEL
};
using MediaCodecSet = EnumSet<MediaCodec, uint64_t>;

// Helper macros used to create codec-specific SW/HW decode enums below.
#define SW_DECODE(codec) codec##SoftwareDecode
#define HW_DECODE(codec) codec##HardwareDecode

// For codec which we can do hardware decoding once user installs the free
// platform extension, eg. AV1 on Windows
#define LACK_HW_EXTENSION(codec) codec##LackOfExtension

// Generate the MediaCodecsSupport enum, containing
// codec-specific SW/HW decode/encode information.
// Entries for HW audio decode/encode should never be set as we
// don't support HW audio decode/encode, but they are included
// for debug purposes / check for erroneous PDM return values.
// Example: MediaCodecsSupport::AACSoftwareDecode
enum class MediaCodecsSupport : int {
#define X(name) SW_DECODE(name), HW_DECODE(name), LACK_HW_EXTENSION(name),
  CODEC_LIST
#undef X
      SENTINEL
};
#undef SW_DECODE
#undef HW_DECODE
#undef CODEC_LIST  // end of macros!

// Enumset containing per-codec SW/HW support
using MediaCodecsSupported = EnumSet<MediaCodecsSupport, uint64_t>;

// Codec-agnostic SW/HW decode support information.
enum class DecodeSupport : int {
  SoftwareDecode,
  HardwareDecode,
  UnsureDueToLackOfExtension,
};
using DecodeSupportSet = EnumSet<DecodeSupport, uint64_t>;

// CodecDefinition stores information needed to convert / index
// codec support information between types. See: GetAllCodecDefinitions()
struct CodecDefinition {
  MediaCodec codec = MediaCodec::SENTINEL;
  const char* commonName = "Undefined codec name";
  const char* mimeTypeString = "Undefined MIME type string";
  MediaCodecsSupport swDecodeSupport = MediaCodecsSupport::SENTINEL;
  MediaCodecsSupport hwDecodeSupport = MediaCodecsSupport::SENTINEL;
  MediaCodecsSupport lackOfHWExtenstion = MediaCodecsSupport::SENTINEL;
};

// Singleton class used to collect, manage, and report codec support data.
class MCSInfo final {
 public:
  // Add codec support information to our aggregated list of supported codecs.
  // Incoming support info is merged with the current support info.
  // This is because different PDMs may report different codec support
  // information, so merging their results allows us to maintain a
  // cumulative support list without overwriting any existing data.
  static void AddSupport(const MediaCodecsSupported& aSupport);

  // Return a cumulative list of codec support information.
  // Each call to AddSupport adds to or updates this list.
  // This support information can be used to create user-readable strings
  // to report codec support information in about:support.
  static MediaCodecsSupported GetSupport();

  // Reset codec support information saved from calls to AddSupport().
  static void ResetSupport();

  // Query a MediaCodecsSupported EnumSet for codec-specific SW/HW support enums
  // and return general support information as stored in a DecodeSupportSet.
  //
  // Example input:
  //
  //   aCodec: MediaCodec::H264
  //   aDecode: MediaCodecsSupport {
  //     MediaCodecsSupport::AACSoftwareDecode
  //     MediaCodecsSupport::H264HardwareDecode,
  //     MediaCodecsSupport::H264SoftwareDecode,
  //     MediaCodecsSupport::VP8SoftwareDecode,
  //   }
  //
  // Example output:
  //
  //   DecodeSupportSet {
  //     DecodeSupport::SoftwareDecode,
  //     DecodeSupport::HardwareDecode
  //   }
  //
  static DecodeSupportSet GetDecodeSupportSet(
      const MediaCodec& aCodec, const MediaCodecsSupported& aSupported);

  // Return codec-specific SW/HW support enums for a given codec.
  // The DecodeSupportSet argument is used which codec-specific SW/HW
  // support values are returned, if any.
  //
  // Example input:
  //   aCodec: MediaCodec::VP8
  //   aSupportSet: DecodeSupportSet {DecodeSupport::SoftwareDecode}
  //
  // Example output:
  //   MediaCodecsSupported {MediaCodecsSupport::VP8SoftwareDecode}
  //
  static MediaCodecsSupported GetDecodeMediaCodecsSupported(
      const MediaCodec& aCodec, const DecodeSupportSet& aSupportSet);

  // Generate a plaintext description for the SW/HW support information
  // contained in a MediaCodecsSupported EnumSet.
  //
  // Example input:
  //   MediaCodecsSupported {
  //     MediaCodecsSupport::H264SoftwareDecode,
  //     MediaCodecsSupport::H264HardwareDecode,
  //     MediaCodecsSupport::VP8SoftwareDecode
  //   }
  //
  // Example output (returned via argument aCodecString)
  //
  //   "SW H264 decode\n
  //   HW H264 decode\n
  //   SW VP8 decode"_ns
  //
  static void GetMediaCodecsSupportedString(
      nsCString& aSupportString, const MediaCodecsSupported& aSupportedCodecs);

  // Returns a MediaCodec enum representing the given MIME type string.
  //
  // Example input:
  //   "audio/flac"_ns
  //
  // Example output:
  //   MediaCodec::FLAC
  //
  static MediaCodec GetMediaCodecFromMimeType(const nsACString& aMimeType);

  // Returns array of hardcoded codec definitions.
  static std::array<CodecDefinition, 13> GetAllCodecDefinitions();

  // Parses an array of MIME type strings and returns a MediaCodecSet.
  static MediaCodecSet GetMediaCodecSetFromMimeTypes(
      const nsTArray<nsCString>& aCodecStrings);

  // Returns a MediaCodecsSupport enum corresponding to the provided
  // codec type and decode support level requested.
  static MediaCodecsSupport GetMediaCodecsSupportEnum(
      const MediaCodec& aCodec, const DecodeSupport& aSupport);

  // Returns true if SW/HW decode enum for a given codec is present in the args.
  static bool SupportsSoftwareDecode(
      const MediaCodecsSupported& aSupportedCodecs, const MediaCodec& aCodec);
  static bool SupportsHardwareDecode(
      const MediaCodecsSupported& aSupportedCodecs, const MediaCodec& aCodec);

  MCSInfo(MCSInfo const&) = delete;
  void operator=(MCSInfo const&) = delete;
  ~MCSInfo() = default;

 private:
  MCSInfo();
  static MCSInfo* GetInstance();

  // Returns a codec definition by MIME type name ("media/vp9")
  // or "common" name ("VP9")
  static CodecDefinition GetCodecDefinition(const MediaCodec& aCodec);

  UniquePtr<nsTHashMap<MediaCodecsSupport, CodecDefinition>> mHashTableMCS;
  UniquePtr<nsTHashMap<const char*, CodecDefinition>> mHashTableString;
  UniquePtr<nsTHashMap<MediaCodec, CodecDefinition>> mHashTableCodec;
  MediaCodecsSupported mSupport;
};
}  // namespace mozilla::media

namespace mozilla {
// Used for IPDL serialization.
// The 'value' has to be the biggest enum from MediaCodecsSupport.
template <typename T>
struct MaxEnumValue;
template <>
struct MaxEnumValue<media::MediaCodecsSupport> {
  static constexpr unsigned int value =
      static_cast<unsigned int>(media::MediaCodecsSupport::SENTINEL);
};
}  // namespace mozilla

#endif /* MediaCodecsSupport_h_ */
