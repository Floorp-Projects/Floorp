/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(PDMFactory_h_)
#  define PDMFactory_h_

#  include "DecoderDoctorDiagnostics.h"
#  include "PlatformDecoderModule.h"
#  include "mozilla/StaticMutex.h"

class CDMProxy;

namespace mozilla {

class PDMFactoryImpl;
template <class T>
class StaticAutoPtr;
enum class RemoteDecodeIn;

class PDMFactory final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PDMFactory)

  PDMFactory();

  // To be called in the content process only, used to determine which PDMs are
  // usable in their respective process.
  static already_AddRefed<PDMFactory> PDMFactoryForRdd();
  static already_AddRefed<PDMFactory> PDMFactoryForGpu();

  // Factory method that creates the appropriate PlatformDecoderModule for
  // the platform we're running on. Caller is responsible for deleting this
  // instance. It's expected that there will be multiple
  // PlatformDecoderModules alive at the same time.
  // This is called on the decode task queue.
  already_AddRefed<MediaDataDecoder> CreateDecoder(
      const CreateDecoderParams& aParams);

  bool SupportsMimeType(const nsACString& aMimeType,
                        DecoderDoctorDiagnostics* aDiagnostics) const;
  bool Supports(const SupportDecoderParams& aParams,
                DecoderDoctorDiagnostics* aDiagnostics) const;

  // Creates a PlatformDecoderModule that uses a CDMProxy to decrypt or
  // decrypt-and-decode EME encrypted content. If the CDM only decrypts and
  // does not decode, we create a PDM and use that to create MediaDataDecoders
  // that we use on on aTaskQueue to decode the decrypted stream.
  // This is called on the decode task queue.
  void SetCDMProxy(CDMProxy* aProxy);

  static constexpr int kYUV400 = 0;
  static constexpr int kYUV420 = 1;
  static constexpr int kYUV422 = 2;
  static constexpr int kYUV444 = 3;

  /*
   * All the codecs we support
   */
  enum class MediaCodecs {
    H264,
    VP9,
    VP8,
    AV1,
    Theora,
    AAC,
    MP3,
    Opus,
    Vorbis,
    Flac,
    Wave,

    SENTINEL,
  };

  using MediaCodecsSupported = EnumSet<MediaCodecs>;

  static MediaCodecsSupported Supported();
  static void SetSupported(const MediaCodecsSupported& aSupported);

 private:
  virtual ~PDMFactory() = default;
  // Will set PDM list for the required process.
  // This is used to determine which PDMs are available on the given process
  // from the content process.
  explicit PDMFactory(const RemoteDecodeIn& aProcess);

  void CreatePDMs();
  void CreateNullPDM();
  void CreateGpuPDMs();
  void CreateRddPDMs();
  void CreateContentPDMs();
  void CreateDefaultPDMs();

  template <typename DECODER_MODULE, typename... ARGS>
  bool CreateAndStartupPDM(ARGS&&... aArgs) {
    return StartupPDM(DECODER_MODULE::Create(std::forward<ARGS>(aArgs)...));
  }

  // Startup the provided PDM and add it to our list if successful.
  bool StartupPDM(already_AddRefed<PlatformDecoderModule> aPDM,
                  bool aInsertAtBeginning = false);
  // Returns the first PDM in our list supporting the mimetype.
  already_AddRefed<PlatformDecoderModule> GetDecoderModule(
      const SupportDecoderParams& aParams,
      DecoderDoctorDiagnostics* aDiagnostics) const;

  already_AddRefed<MediaDataDecoder> CreateDecoderWithPDM(
      PlatformDecoderModule* aPDM, const CreateDecoderParams& aParams);

  nsTArray<RefPtr<PlatformDecoderModule>> mCurrentPDMs;
  RefPtr<PlatformDecoderModule> mEMEPDM;
  RefPtr<PlatformDecoderModule> mNullPDM;

  DecoderDoctorDiagnostics::FlagsSet mFailureFlags;

  friend class RemoteVideoDecoderParent;
  static void EnsureInit();
  template <class T>
  friend class StaticAutoPtr;
  static StaticAutoPtr<PDMFactoryImpl> sInstance;
  static StaticMutex sMonitor;
};

// Used for IPDL serialization.
// The 'value' have to be the biggest enum from MediaCodecs.
template <>
struct MaxEnumValue<PDMFactory::MediaCodecs> {
  static constexpr unsigned int value =
      static_cast<unsigned int>(PDMFactory::MediaCodecs::SENTINEL);
};

}  // namespace mozilla

#endif /* PDMFactory_h_ */
