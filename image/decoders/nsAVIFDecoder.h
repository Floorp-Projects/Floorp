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

#include <aom/aom_decoder.h>
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
    RenderSizeMismatch,
    FrameSizeChanged,
    InvalidCICP,
    NoSamples,
  };
  using DecodeResult =
      Variant<Mp4parseStatus, NonDecoderResult, Dav1dResult, AOMResult>;
  Mp4parseStatus CreateParser();
  DecodeResult CreateDecoder();
  DecodeResult DoDecodeInternal(SourceBufferIterator& aIterator,
                                IResumable* aOnResume);

  static bool IsDecodeSuccess(const DecodeResult& aResult);

  void RecordDecodeResultTelemetry(const DecodeResult& aResult);

  Vector<uint8_t> mBufferedData;
  RefPtr<AVIFDecoderStream> mBufferStream;

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
                               UniquePtr<AVIFParser>& aParserOut,
                               bool aAllowSequences, bool aAnimateAVIFMajor);

  ~AVIFParser();

  const Mp4parseAvifInfo& GetInfo() const { return mInfo; }

  nsAVIFDecoder::DecodeResult GetImage(AVIFImage& aImage);

  bool IsAnimated() const;

 private:
  explicit AVIFParser(const Mp4parseIo* aIo);

  Mp4parseStatus Init(ByteStream* aBuffer, bool aAllowSequences,
                      bool aAnimateAVIFMajor);

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

struct Dav1dPictureUnref {
  void operator()(Dav1dPicture* aPtr) {
    dav1d_picture_unref(aPtr);
    delete aPtr;
  }
};

using OwnedDav1dPicture = UniquePtr<Dav1dPicture, Dav1dPictureUnref>;

class OwnedAOMImage {
 public:
  ~OwnedAOMImage();

  static OwnedAOMImage* CopyFrom(aom_image_t* aImage, bool aIsAlpha);

  aom_image_t* GetImage() { return mImage.isSome() ? mImage.ptr() : nullptr; }

 private:
  OwnedAOMImage();

  bool CloneFrom(aom_image_t* aImage, bool aIsAlpha);

  // The mImage's planes are referenced to mBuffer
  Maybe<aom_image_t> mImage;
  UniquePtr<uint8_t[]> mBuffer;
};

struct AVIFDecodedData : layers::PlanarYCbCrData {
 public:
  Maybe<OrientedIntSize> mRenderSize = Nothing();
  gfx::CICP::ColourPrimaries mColourPrimaries = gfx::CICP::CP_UNSPECIFIED;
  gfx::CICP::TransferCharacteristics mTransferCharacteristics =
      gfx::CICP::TC_UNSPECIFIED;
  gfx::CICP::MatrixCoefficients mMatrixCoefficients = gfx::CICP::MC_UNSPECIFIED;

  OwnedDav1dPicture mColorDav1d;
  OwnedDav1dPicture mAlphaDav1d;
  UniquePtr<OwnedAOMImage> mColorAOM;
  UniquePtr<OwnedAOMImage> mAlphaAOM;

  // CICP values (either from the BMFF container or the AV1 sequence header) are
  // used to create the colorspace transform. CICP::MatrixCoefficients is only
  // stored for the sake of telemetry, since the relevant information for YUV ->
  // RGB conversion is stored in mYUVColorSpace.
  //
  // There are three potential sources of color information for an AVIF:
  // 1. ICC profile via a ColourInformationBox (colr) defined in [ISOBMFF]
  //    § 12.1.5 "Colour information" and [MIAF] § 7.3.6.4 "Colour information
  //    property"
  // 2. NCLX (AKA CICP see [ITU-T H.273]) values in the same
  // ColourInformationBox
  //    which can have an ICC profile or NCLX values, not both).
  // 3. NCLX values in the AV1 bitstream
  //
  // The 'colr' box is optional, but there are always CICP values in the AV1
  // bitstream, so it is possible to have both. Per ISOBMFF § 12.1.5.1
  // > If colour information is supplied in both this box, and also in the
  // > video bitstream, this box takes precedence, and over-rides the
  // > information in the bitstream.
  //
  // If present, the ICC profile takes precedence over CICP values, but only
  // specifies the color space, not the matrix coefficients necessary to convert
  // YCbCr data (as most AVIF are encoded) to RGB. The matrix coefficients are
  // always derived from the CICP values for matrix_coefficients (and
  // potentially colour_primaries, but in that case only the CICP values for
  // colour_primaries will be used, not anything harvested from the ICC
  // profile).
  //
  // If there is no ICC profile, the color space transform will be based on the
  // CICP values either from the 'colr' box, or if absent/unspecified, the
  // decoded AV1 sequence header.
  //
  // For values that are 2 (meaning unspecified) after trying both, the
  // fallback values are:
  // - CP:  1 (BT.709/sRGB)
  // - TC: 13 (sRGB)
  // - MC:  6 (BT.601)
  // - Range: Full
  //
  // Additional details here:
  // <https://github.com/AOMediaCodec/libavif/wiki/CICP#unspecified>. Note
  // that this contradicts the current version of [MIAF] § 7.3.6.4 which
  // specifies MC=1 (BT.709). This is revised in [MIAF DAMD2] and confirmed by
  // <https://github.com/AOMediaCodec/av1-avif/issues/77#issuecomment-676526097>
  //
  // The precedence for applying the various values and defaults in the event
  // no valid values are found are managed by the following functions.
  //
  // References:
  // [ISOBMFF]: ISO/IEC 14496-12:2020 <https://www.iso.org/standard/74428.html>
  // [MIAF]: ISO/IEC 23000-22:2019 <https://www.iso.org/standard/74417.html>
  // [MIAF DAMD2]: ISO/IEC 23000-22:2019/FDAmd 2
  // <https://www.iso.org/standard/81634.html>
  // [ITU-T H.273]: Rec. ITU-T H.273 (12/2016)
  //     <https://www.itu.int/rec/T-REC-H.273-201612-I/en>
  void SetCicpValues(
      const Mp4parseNclxColourInformation* aNclx,
      const gfx::CICP::ColourPrimaries aAv1ColourPrimaries,
      const gfx::CICP::TransferCharacteristics aAv1TransferCharacteristics,
      const gfx::CICP::MatrixCoefficients aAv1MatrixCoefficients);
};

// An interface to do decode and get the decoded data
class AVIFDecoderInterface {
 public:
  using Dav1dResult = nsAVIFDecoder::Dav1dResult;
  using NonAOMCodecError = nsAVIFDecoder::NonAOMCodecError;
  using AOMResult = nsAVIFDecoder::AOMResult;
  using NonDecoderResult = nsAVIFDecoder::NonDecoderResult;
  using DecodeResult = nsAVIFDecoder::DecodeResult;

  virtual ~AVIFDecoderInterface() = default;

  // Set the mDecodedData if Decode() succeeds
  virtual DecodeResult Decode(bool aShouldSendTelemetry,
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
