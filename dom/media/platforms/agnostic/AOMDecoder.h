/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(AOMDecoder_h_)
#  define AOMDecoder_h_

#  include <stdint.h>

#  include "PerformanceRecorder.h"
#  include "PlatformDecoderModule.h"
#  include <aom/aom_decoder.h>
#  include "mozilla/Span.h"
#  include "VideoUtils.h"

namespace mozilla {

DDLoggedTypeDeclNameAndBase(AOMDecoder, MediaDataDecoder);

class AOMDecoder final : public MediaDataDecoder,
                         public DecoderDoctorLifeLogger<AOMDecoder> {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AOMDecoder, final);

  explicit AOMDecoder(const CreateDecoderParams& aParams);

  RefPtr<InitPromise> Init() override;
  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;
  RefPtr<DecodePromise> Drain() override;
  RefPtr<FlushPromise> Flush() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  nsCString GetDescriptionName() const override {
    return "av1 libaom video decoder"_ns;
  }
  nsCString GetCodecName() const override { return "av1"_ns; }

  // Return true if aMimeType is a one of the strings used
  // by our demuxers to identify AV1 streams.
  static bool IsAV1(const nsACString& aMimeType);

  // Return true if a sample is a keyframe.
  static bool IsKeyframe(Span<const uint8_t> aBuffer);

  // Return the frame dimensions for a sample.
  static gfx::IntSize GetFrameSize(Span<const uint8_t> aBuffer);

  // obu_type defined at:
  // https://aomediacodec.github.io/av1-spec/av1-spec.pdf#page=123
  enum class OBUType : uint8_t {
    Reserved = 0,
    SequenceHeader = 1,
    TemporalDelimiter = 2,
    FrameHeader = 3,
    TileGroup = 4,
    Metadata = 5,
    Frame = 6,
    RedundantFrameHeader = 7,
    TileList = 8,
    Padding = 15
  };

  struct OBUInfo {
    OBUType mType = OBUType::Reserved;
    bool mExtensionFlag = false;
    Span<const uint8_t> mContents;

    bool IsValid() const {
      switch (mType) {
        case OBUType::SequenceHeader:
        case OBUType::TemporalDelimiter:
        case OBUType::FrameHeader:
        case OBUType::TileGroup:
        case OBUType::Metadata:
        case OBUType::Frame:
        case OBUType::RedundantFrameHeader:
        case OBUType::TileList:
        case OBUType::Padding:
          return true;
        default:
          return false;
      }
    }
  };

  struct OBUIterator {
   public:
    explicit OBUIterator(const Span<const uint8_t>& aData)
        : mData(aData), mPosition(0), mGoNext(true), mResult(NS_OK) {}
    bool HasNext() {
      UpdateNext();
      return !mGoNext;
    }
    OBUInfo Next() {
      UpdateNext();
      mGoNext = true;
      return mCurrent;
    }
    MediaResult GetResult() const { return mResult; }

   private:
    const Span<const uint8_t>& mData;
    size_t mPosition;
    OBUInfo mCurrent;
    bool mGoNext;
    MediaResult mResult;

    // Used to fill mCurrent with the next OBU in the iterator.
    // mGoNext must be set to false if the next OBU is retrieved,
    // otherwise it will be true so that HasNext() returns false.
    // When an invalid OBU is read, the iterator will finish and
    // mCurrent will be reset to default OBUInfo().
    void UpdateNext();
  };

  // Create an iterator to parse Open Bitstream Units from a buffer.
  static OBUIterator ReadOBUs(const Span<const uint8_t>& aData);
  // Writes an Open Bitstream Unit header type and the contained subheader.
  // Extension flag is set to 0 and size field is always present.
  static already_AddRefed<MediaByteBuffer> CreateOBU(
      const OBUType aType, const Span<const uint8_t>& aContents);

  // chroma_sample_position defined at:
  // https://aomediacodec.github.io/av1-spec/av1-spec.pdf#page=131
  enum class ChromaSamplePosition : uint8_t {
    Unknown = 0,
    Vertical = 1,
    Colocated = 2,
    Reserved = 3
  };

  struct OperatingPoint {
    // https://aomediacodec.github.io/av1-spec/av1-spec.pdf#page=125
    // operating_point_idc[ i ]: A set of bitwise flags determining
    // the temporal and spatial layers to decode.
    // A value of 0 indicates that scalability is not being used.
    uint16_t mLayers = 0;
    // https://aomediacodec.github.io/av1-spec/av1-spec.pdf#page=650
    // See A.3: Levels for a definition of the available levels.
    uint8_t mLevel = 0;
    // https://aomediacodec.github.io/av1-spec/av1-spec.pdf#page=126
    // seq_tier[ i ]: The tier for the selected operating point.
    uint8_t mTier = 0;

    bool operator==(const OperatingPoint& aOther) const {
      return mLayers == aOther.mLayers && mLevel == aOther.mLevel &&
             mTier == aOther.mTier;
    }
    bool operator!=(const OperatingPoint& aOther) const {
      return !(*this == aOther);
    }
  };

  struct AV1SequenceInfo {
    AV1SequenceInfo() = default;

    AV1SequenceInfo(const AV1SequenceInfo& aOther) { *this = aOther; }

    // Profiles, levels and tiers defined at:
    // https://aomediacodec.github.io/av1-spec/av1-spec.pdf#page=650
    uint8_t mProfile = 0;

    // choose_operating_point( ) defines that the operating points are
    // specified in order of preference by the encoder. Higher operating
    // points indices in the header will allow a tradeoff of quality for
    // performance, dropping some data from the decoding process.
    // Normally we are only interested in the first operating point.
    // See: https://aomediacodec.github.io/av1-spec/av1-spec.pdf#page=126
    nsTArray<OperatingPoint> mOperatingPoints = nsTArray<OperatingPoint>(1);

    gfx::IntSize mImage = {0, 0};

    // Color configs explained at:
    // https://aomediacodec.github.io/av1-spec/av1-spec.pdf#page=129
    uint8_t mBitDepth = 8;
    bool mMonochrome = false;
    bool mSubsamplingX = true;
    bool mSubsamplingY = true;
    ChromaSamplePosition mChromaSamplePosition = ChromaSamplePosition::Unknown;

    VideoColorSpace mColorSpace;

    gfx::ColorDepth ColorDepth() const {
      return gfx::ColorDepthForBitDepth(mBitDepth);
    }

    bool operator==(const AV1SequenceInfo& aOther) const {
      if (mProfile != aOther.mProfile || mImage != aOther.mImage ||
          mBitDepth != aOther.mBitDepth || mMonochrome != aOther.mMonochrome ||
          mSubsamplingX != aOther.mSubsamplingX ||
          mSubsamplingY != aOther.mSubsamplingY ||
          mChromaSamplePosition != aOther.mChromaSamplePosition ||
          mColorSpace != aOther.mColorSpace) {
        return false;
      }

      size_t opCount = mOperatingPoints.Length();
      if (opCount != aOther.mOperatingPoints.Length()) {
        return false;
      }
      for (size_t i = 0; i < opCount; i++) {
        if (mOperatingPoints[i] != aOther.mOperatingPoints[i]) {
          return false;
        }
      }

      return true;
    }
    bool operator!=(const AV1SequenceInfo& aOther) const {
      return !(*this == aOther);
    }
    AV1SequenceInfo& operator=(const AV1SequenceInfo& aOther) {
      mProfile = aOther.mProfile;

      size_t opCount = aOther.mOperatingPoints.Length();
      mOperatingPoints.ClearAndRetainStorage();
      mOperatingPoints.SetCapacity(opCount);
      for (size_t i = 0; i < opCount; i++) {
        mOperatingPoints.AppendElement(aOther.mOperatingPoints[i]);
      }

      mImage = aOther.mImage;
      mBitDepth = aOther.mBitDepth;
      mMonochrome = aOther.mMonochrome;
      mSubsamplingX = aOther.mSubsamplingX;
      mSubsamplingY = aOther.mSubsamplingY;
      mChromaSamplePosition = aOther.mChromaSamplePosition;
      mColorSpace = aOther.mColorSpace;
      return *this;
    }
  };

  // Get a sequence header's info from a sample.
  // Returns a MediaResult with codes:
  //    NS_OK: Sequence header was successfully found and read.
  //    NS_ERROR_DOM_MEDIA_WAITING_FOR_DATA: Sequence header was not present.
  //    Other errors will indicate that the data was corrupt.
  static MediaResult ReadSequenceHeaderInfo(const Span<const uint8_t>& aSample,
                                            AV1SequenceInfo& aDestInfo);
  // Writes a sequence header OBU to the buffer.
  static already_AddRefed<MediaByteBuffer> CreateSequenceHeader(
      const AV1SequenceInfo& aInfo, nsresult& aResult);

  // Reads the raw data of an ISOBMFF-compatible av1 configuration box (av1C),
  // including any included sequence header.
  static void TryReadAV1CBox(const MediaByteBuffer* aBox,
                             AV1SequenceInfo& aDestInfo,
                             MediaResult& aSeqHdrResult);
  // Reads the raw data of an ISOBMFF-compatible av1 configuration box (av1C),
  // including any included sequence header.
  // This function should only be called for av1C boxes made by WriteAV1CBox, as
  // it will assert that the box and its contained OBUs are not corrupted.
  static void ReadAV1CBox(const MediaByteBuffer* aBox,
                          AV1SequenceInfo& aDestInfo, bool& aHadSeqHdr) {
    MediaResult seqHdrResult;
    TryReadAV1CBox(aBox, aDestInfo, seqHdrResult);
    nsresult code = seqHdrResult.Code();
    MOZ_ASSERT(code == NS_OK || code == NS_ERROR_DOM_MEDIA_WAITING_FOR_DATA);
    aHadSeqHdr = code == NS_OK;
  }
  // Writes an ISOBMFF-compatible av1 configuration box (av1C) to the buffer.
  static void WriteAV1CBox(const AV1SequenceInfo& aInfo,
                           MediaByteBuffer* aDestBox, bool& aHasSeqHdr);

  // Create sequence info from a MIME codecs string.
  static Maybe<AV1SequenceInfo> CreateSequenceInfoFromCodecs(
      const nsAString& aCodec);
  static bool SetVideoInfo(VideoInfo* aDestInfo, const nsAString& aCodec);

 private:
  ~AOMDecoder();
  RefPtr<DecodePromise> ProcessDecode(MediaRawData* aSample);

  const RefPtr<layers::ImageContainer> mImageContainer;
  const RefPtr<TaskQueue> mTaskQueue;

  // AOM decoder state
  aom_codec_ctx_t mCodec;

  const VideoInfo mInfo;
  const Maybe<TrackingId> mTrackingId;
  PerformanceRecorderMulti<DecodeStage> mPerformanceRecorder;
};

}  // namespace mozilla

#endif  // AOMDecoder_h_
