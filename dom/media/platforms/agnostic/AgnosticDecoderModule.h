#if !defined(AgnosticDecoderModule_h_)
#define AgnosticDecoderModule_h_

#include "PlatformDecoderModule.h"

namespace mozilla {

class AgnosticDecoderModule : public PlatformDecoderModule {
public:
  AgnosticDecoderModule() = default;
  virtual ~AgnosticDecoderModule() = default;

  bool SupportsMimeType(const nsACString& aMimeType,
                        DecoderDoctorDiagnostics* aDiagnostics) const override;

  ConversionRequired
  DecoderNeedsConversion(const TrackInfo& aConfig) const override
  {
    return ConversionRequired::kNeedNone;
  }

protected:
  // Decode thread.
  already_AddRefed<MediaDataDecoder>
  CreateVideoDecoder(const VideoInfo& aConfig,
                     layers::LayersBackend aLayersBackend,
                     layers::ImageContainer* aImageContainer,
                     TaskQueue* aTaskQueue,
                     MediaDataDecoderCallback* aCallback,
                     DecoderDoctorDiagnostics* aDiagnostics) override;

  // Decode thread.
  already_AddRefed<MediaDataDecoder>
  CreateAudioDecoder(const AudioInfo& aConfig,
                     TaskQueue* aTaskQueue,
                     MediaDataDecoderCallback* aCallback,
                     DecoderDoctorDiagnostics* aDiagnostics) override;
};

} // namespace mozilla

#endif /* AgnosticDecoderModule_h_ */
