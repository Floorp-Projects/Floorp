/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MP4METADATA_H_
#define MP4METADATA_H_

#include <type_traits>

#include "mozilla/TypeTraits.h"
#include "mozilla/UniquePtr.h"
#include "DecoderData.h"
#include "Index.h"
#include "MediaData.h"
#include "MediaInfo.h"
#include "MediaResult.h"
#include "ByteStream.h"
#include "mp4parse.h"

namespace mozilla {

DDLoggedTypeDeclName(MP4Metadata);

// The memory owner in mIndice.indices is rust mp4 parser, so lifetime of this
// class SHOULD NOT longer than rust parser.
class IndiceWrapper {
 public:
  size_t Length() const;

  bool GetIndice(size_t aIndex, Index::Indice& aIndice) const;

  explicit IndiceWrapper(Mp4parseByteData& aRustIndice);

 protected:
  Mp4parseByteData mIndice;
};

struct FreeMP4Parser {
  void operator()(Mp4parseParser* aPtr) { mp4parse_free(aPtr); }
};

// Wrap an Stream to remember the read offset.
class StreamAdaptor {
 public:
  explicit StreamAdaptor(ByteStream* aSource) : mSource(aSource), mOffset(0) {}

  ~StreamAdaptor() = default;

  bool Read(uint8_t* buffer, uintptr_t size, size_t* bytes_read);

 private:
  ByteStream* mSource;
  CheckedInt<size_t> mOffset;
};

class MP4Metadata : public DecoderDoctorLifeLogger<MP4Metadata> {
 public:
  explicit MP4Metadata(ByteStream* aSource);
  ~MP4Metadata();

  // Simple template class containing a MediaResult and another type.
  template <typename T>
  class ResultAndType {
   public:
    template <typename M2, typename T2>
    ResultAndType(M2&& aM, T2&& aT)
        : mResult(std::forward<M2>(aM)), mT(std::forward<T2>(aT)) {}
    ResultAndType(const ResultAndType&) = default;
    ResultAndType& operator=(const ResultAndType&) = default;
    ResultAndType(ResultAndType&&) = default;
    ResultAndType& operator=(ResultAndType&&) = default;

    mozilla::MediaResult& Result() { return mResult; }
    T& Ref() { return mT; }

   private:
    mozilla::MediaResult mResult;
    std::decay_t<T> mT;
  };

  using ResultAndByteBuffer = ResultAndType<RefPtr<mozilla::MediaByteBuffer>>;
  static ResultAndByteBuffer Metadata(ByteStream* aSource);

  static constexpr uint32_t NumberTracksError() { return UINT32_MAX; }
  using ResultAndTrackCount = ResultAndType<uint32_t>;
  ResultAndTrackCount GetNumberTracks(
      mozilla::TrackInfo::TrackType aType) const;

  using ResultAndTrackInfo =
      ResultAndType<mozilla::UniquePtr<mozilla::TrackInfo>>;
  ResultAndTrackInfo GetTrackInfo(mozilla::TrackInfo::TrackType aType,
                                  size_t aTrackNumber) const;

  bool CanSeek() const;

  using ResultAndCryptoFile = ResultAndType<const CryptoFile*>;
  ResultAndCryptoFile Crypto() const;

  using ResultAndIndice = ResultAndType<mozilla::UniquePtr<IndiceWrapper>>;
  ResultAndIndice GetTrackIndice(uint32_t aTrackId);

  nsresult Parse();

 private:
  void UpdateCrypto();
  Maybe<uint32_t> TrackTypeToGlobalTrackIndex(
      mozilla::TrackInfo::TrackType aType, size_t aTrackNumber) const;

  CryptoFile mCrypto;
  RefPtr<ByteStream> mSource;
  StreamAdaptor mSourceAdaptor;
  mozilla::UniquePtr<Mp4parseParser, FreeMP4Parser> mParser;
};

}  // namespace mozilla

#endif  // MP4METADATA_H_
