#ifndef WidevineDummyDecoder_h_
#define WidevineDummyDecoder_h_

#include "stddef.h"
#include "content_decryption_module.h"
#include "gmp-api/gmp-video-decode.h"
#include "gmp-api/gmp-video-host.h"
#include "WidevineDecryptor.h"
#include "WidevineVideoFrame.h"


namespace mozilla {

class WidevineDummyDecoder : public GMPVideoDecoder {
public:
  WidevineDummyDecoder();

  void InitDecode(const GMPVideoCodec& aCodecSettings,
                  const uint8_t* aCodecSpecific,
                  uint32_t aCodecSpecificLength,
                  GMPVideoDecoderCallback* aCallback,
                  int32_t aCoreCount) override;

  void Decode(GMPVideoEncodedFrame* aInputFrame,
              bool aMissingFrames,
              const uint8_t* aCodecSpecificInfo,
              uint32_t aCodecSpecificInfoLength,
              int64_t aRenderTimeMs = -1) override;

  void Reset() override;

  void Drain() override;

  void DecodingComplete() override;

private:
  ~WidevineDummyDecoder();

  GMPVideoDecoderCallback* mCallback;
};
}

#endif