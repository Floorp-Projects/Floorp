/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_decoders_nsAVIFDecoder_h
#define mozilla_image_decoders_nsAVIFDecoder_h

#include "Decoder.h"
#include "mozilla/gfx/Types.h"
#include "MP4Metadata.h"
#include "mp4parse.h"
#include "SampleIterator.h"
#include "SurfacePipe.h"

#include "aom/aom_decoder.h"
#include "dav1d/dav1d.h"

#include "mozilla/Telemetry.h"

namespace mozilla {
namespace image {
class RasterImage;
class AVIFDecoderStream;
class AVIFParser;
class AVIFDecoderInterface;

class nsAVIFDecoder final : public Decoder {
 public:
  virtual ~nsAVIFDecoder();

  DecoderType GetType() const override { return DecoderType::AVIF; }

 protected:
  LexerResult DoDecode(SourceBufferIterator& aIterator,
                       IResumable* aOnResume) override;
  Maybe<Telemetry::HistogramID> SpeedHistogram() const override;

 private:
  friend class DecoderFactory;
  friend class AVIFDecoderInterface;
  friend class AVIFParser;

  // Decoders should only be instantiated via DecoderFactory.
  explicit nsAVIFDecoder(RasterImage* aImage);

  static intptr_t ReadSource(uint8_t* aDestBuf, uintptr_t aDestBufSize,
                             void* aUserData);

  typedef int Dav1dResult;
  enum class NonAOMCodecError { NoFrame, SizeOverflow };
  typedef Variant<aom_codec_err_t, NonAOMCodecError> AOMResult;
  enum class NonDecoderResult {
    NeedMoreData,
    OutputAvailable,
    Complete,
    SizeOverflow,
    OutOfMemory,
    PipeInitError,
    WriteBufferError,
    AlphaYSizeMismatch,
    AlphaYColorDepthMismatch,
    MetadataImageSizeMismatch,
    InvalidCICP,
    NoSamples,
  };
  using DecodeResult =
      Variant<Mp4parseStatus, NonDecoderResult, Dav1dResult, AOMResult>;
  Mp4parseStatus CreateParser();
  DecodeResult CreateDecoder();
  DecodeResult Decode(SourceBufferIterator& aIterator, IResumable* aOnResume);

  static bool IsDecodeSuccess(const DecodeResult& aResult);

  void RecordDecodeResultTelemetry(const DecodeResult& aResult);

  Vector<uint8_t> mBufferedData;
  UniquePtr<AVIFDecoderStream> mBufferStream;

  /// Pointer to the next place to read from mBufferedData
  const uint8_t* mReadCursor = nullptr;

  UniquePtr<AVIFParser> mParser = nullptr;
  UniquePtr<AVIFDecoderInterface> mDecoder = nullptr;

  bool mIsAnimated = false;
  bool mHasAlpha = false;
};

class AVIFDecoderStream : public ByteStream {
 public:
  explicit AVIFDecoderStream(Vector<uint8_t>* aBuffer) { mBuffer = aBuffer; }

  virtual bool ReadAt(int64_t offset, void* data, size_t size,
                      size_t* bytes_read) override;
  virtual bool CachedReadAt(int64_t offset, void* data, size_t size,
                            size_t* bytes_read) override {
    return ReadAt(offset, data, size, bytes_read);
  };
  virtual bool Length(int64_t* size) override;
  virtual const uint8_t* GetContiguousAccess(int64_t aOffset,
                                             size_t aSize) override;

 private:
  Vector<uint8_t>* mBuffer;
};

struct AVIFImage {
  uint32_t mFrameNum = 0;
  FrameTimeout mDuration = FrameTimeout::Zero();
  RefPtr<MediaRawData> mColorImage = nullptr;
  RefPtr<MediaRawData> mAlphaImage = nullptr;
};

class AVIFParser {
 public:
  static Mp4parseStatus Create(const Mp4parseIo* aIo, ByteStream* aBuffer,
                               UniquePtr<AVIFParser>& aParserOut);

  ~AVIFParser();

  const Mp4parseAvifInfo& GetInfo() const { return mInfo; }

  nsAVIFDecoder::DecodeResult GetImage(AVIFImage& aImage);

  bool IsAnimated() const;

 private:
  explicit AVIFParser(const Mp4parseIo* aIo);

  Mp4parseStatus Init(ByteStream* aBuffer);

  struct FreeAvifParser {
    void operator()(Mp4parseAvifParser* aPtr) { mp4parse_avif_free(aPtr); }
  };

  const Mp4parseIo* mIo;
  UniquePtr<Mp4parseAvifParser, FreeAvifParser> mParser = nullptr;
  Mp4parseAvifInfo mInfo = {};

  UniquePtr<SampleIterator> mColorSampleIter = nullptr;
  UniquePtr<SampleIterator> mAlphaSampleIter = nullptr;
  uint32_t mFrameNum = 0;
};

struct AVIFDecodedData;

// An interface to do decode and get the decoded data
class AVIFDecoderInterface {
 public:
  using Dav1dResult = nsAVIFDecoder::Dav1dResult;
  using NonAOMCodecError = nsAVIFDecoder::NonAOMCodecError;
  using AOMResult = nsAVIFDecoder::AOMResult;
  using NonDecoderResult = nsAVIFDecoder::NonDecoderResult;
  using DecodeResult = nsAVIFDecoder::DecodeResult;

  virtual ~AVIFDecoderInterface();

  // Set the mDecodedData if Decode() succeeds
  virtual DecodeResult Decode(bool aIsMetadataDecode,
                              const Mp4parseAvifInfo& aAVIFInfo,
                              const AVIFImage& aSamples) = 0;
  // Must be called only once after Decode() succeeds
  UniquePtr<AVIFDecodedData> GetDecodedData() {
    MOZ_ASSERT(mDecodedData);
    return std::move(mDecodedData);
  }

 protected:
  explicit AVIFDecoderInterface() = default;

  inline static bool IsDecodeSuccess(const DecodeResult& aResult) {
    return nsAVIFDecoder::IsDecodeSuccess(aResult);
  }

  // The mDecodedData is valid after Decode() succeeds
  UniquePtr<AVIFDecodedData> mDecodedData;
};

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_decoders_nsAVIFDecoder_h
