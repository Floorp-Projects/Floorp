/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MP4METADATA_H_
#define MP4METADATA_H_

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

class IndiceWrapper {
public:
  virtual size_t Length() const = 0;

  // TODO: Index::Indice is from stagefright, we should use another struct once
  //       stagefrigth is removed.
  virtual bool GetIndice(size_t aIndex, Index::Indice& aIndice) const = 0;

  virtual ~IndiceWrapper() {}
};

struct FreeMP4Parser { void operator()(mp4parse_parser* aPtr) { mp4parse_free(aPtr); } };

// Wrap an Stream to remember the read offset.
class StreamAdaptor {
public:
  explicit StreamAdaptor(ByteStream* aSource)
    : mSource(aSource)
    , mOffset(0)
  {
  }

  ~StreamAdaptor() {}

  bool Read(uint8_t* buffer, uintptr_t size, size_t* bytes_read);

private:
  ByteStream* mSource;
  CheckedInt<size_t> mOffset;
};

class MP4Metadata
{
public:
  explicit MP4Metadata(ByteStream* aSource);
  ~MP4Metadata();

  // Simple template class containing a MediaResult and another type.
  template <typename T>
  class ResultAndType
  {
  public:
    template <typename M2, typename T2>
    ResultAndType(M2&& aM, T2&& aT)
      : mResult(Forward<M2>(aM)), mT(Forward<T2>(aT))
    {
    }
    ResultAndType(const ResultAndType&) = default;
    ResultAndType& operator=(const ResultAndType&) = default;
    ResultAndType(ResultAndType&&) = default;
    ResultAndType& operator=(ResultAndType&&) = default;

    mozilla::MediaResult& Result() { return mResult; }
    T& Ref() { return mT; }

  private:
    mozilla::MediaResult mResult;
    typename mozilla::Decay<T>::Type mT;
  };

  using ResultAndByteBuffer = ResultAndType<RefPtr<mozilla::MediaByteBuffer>>;
  static ResultAndByteBuffer Metadata(ByteStream* aSource);

  static constexpr uint32_t NumberTracksError() { return UINT32_MAX; }
  using ResultAndTrackCount = ResultAndType<uint32_t>;
  ResultAndTrackCount GetNumberTracks(mozilla::TrackInfo::TrackType aType) const;

  using ResultAndTrackInfo =
    ResultAndType<mozilla::UniquePtr<mozilla::TrackInfo>>;
  ResultAndTrackInfo GetTrackInfo(mozilla::TrackInfo::TrackType aType,
                                  size_t aTrackNumber) const;

  bool CanSeek() const;

  using ResultAndCryptoFile = ResultAndType<const CryptoFile*>;
  ResultAndCryptoFile Crypto() const;

  using ResultAndIndice = ResultAndType<mozilla::UniquePtr<IndiceWrapper>>;
  ResultAndIndice GetTrackIndice(mozilla::TrackID aTrackID);

  nsresult Parse();

private:
  void UpdateCrypto();
  Maybe<uint32_t> TrackTypeToGlobalTrackIndex(mozilla::TrackInfo::TrackType aType, size_t aTrackNumber) const;

  CryptoFile mCrypto;
  RefPtr<ByteStream> mSource;
  StreamAdaptor mSourceAdaptor;
  mozilla::UniquePtr<mp4parse_parser, FreeMP4Parser> mParser;
};

} // namespace mozilla

#endif // MP4METADATA_H_
