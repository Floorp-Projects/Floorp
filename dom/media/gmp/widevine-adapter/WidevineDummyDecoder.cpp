#include "WidevineDummyDecoder.h"
#include "WidevineUtils.h"

using namespace cdm;

namespace mozilla {
WidevineDummyDecoder::WidevineDummyDecoder()
{
  CDM_LOG("WidevineDummyDecoder created");
}

void WidevineDummyDecoder::InitDecode(const GMPVideoCodec & aCodecSettings,
                                      const uint8_t * aCodecSpecific,
                                      uint32_t aCodecSpecificLength,
                                      GMPVideoDecoderCallback * aCallback,
                                      int32_t aCoreCount)
{
  CDM_LOG("WidevineDummyDecoder::InitDecode");

  mCallback = aCallback;
  mCallback->Error(GMPErr::GMPNotImplementedErr);
}

void WidevineDummyDecoder::Decode(GMPVideoEncodedFrame * aInputFrame,
                                  bool aMissingFrames,
                                  const uint8_t * aCodecSpecificInfo,
                                  uint32_t aCodecSpecificInfoLength,
                                  int64_t aRenderTimeMs)
{
  CDM_LOG("WidevineDummyDecoder::Decode");
  mCallback->Error(GMPErr::GMPNotImplementedErr);
}

void WidevineDummyDecoder::Reset()
{
  CDM_LOG("WidevineDummyDecoder::Reset");
  mCallback->Error(GMPErr::GMPNotImplementedErr);
}

void WidevineDummyDecoder::Drain()
{
  CDM_LOG("WidevineDummyDecoder::Drain");
  mCallback->Error(GMPErr::GMPNotImplementedErr);
}

void WidevineDummyDecoder::DecodingComplete()
{
  CDM_LOG("WidevineDummyDecoder::DecodingComplete");

  mCallback = nullptr;
  delete this;
}

WidevineDummyDecoder::~WidevineDummyDecoder() {
  CDM_LOG("WidevineDummyDecoder destroyed");
}
}