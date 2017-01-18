#include "WidevineDummyDecoder.h"
#include "WidevineUtils.h"

using namespace cdm;

namespace mozilla {
WidevineDummyDecoder::WidevineDummyDecoder()
{
  Log("WidevineDummyDecoder created");
}

void WidevineDummyDecoder::InitDecode(const GMPVideoCodec & aCodecSettings,
                                      const uint8_t * aCodecSpecific,
                                      uint32_t aCodecSpecificLength,
                                      GMPVideoDecoderCallback * aCallback,
                                      int32_t aCoreCount)
{
  Log("WidevineDummyDecoder::InitDecode");

  mCallback = aCallback;
  mCallback->Error(GMPErr::GMPNotImplementedErr);
}

void WidevineDummyDecoder::Decode(GMPVideoEncodedFrame * aInputFrame,
                                  bool aMissingFrames,
                                  const uint8_t * aCodecSpecificInfo,
                                  uint32_t aCodecSpecificInfoLength,
                                  int64_t aRenderTimeMs)
{
  Log("WidevineDummyDecoder::Decode");
  mCallback->Error(GMPErr::GMPNotImplementedErr);
}

void WidevineDummyDecoder::Reset()
{
  Log("WidevineDummyDecoder::Reset");
  mCallback->Error(GMPErr::GMPNotImplementedErr);
}

void WidevineDummyDecoder::Drain()
{
  Log("WidevineDummyDecoder::Drain");
  mCallback->Error(GMPErr::GMPNotImplementedErr);
}

void WidevineDummyDecoder::DecodingComplete()
{
  Log("WidevineDummyDecoder::DecodingComplete");

  mCallback = nullptr;
  delete this;
}

WidevineDummyDecoder::~WidevineDummyDecoder() {
  Log("WidevineDummyDecoder destroyed");
}
}