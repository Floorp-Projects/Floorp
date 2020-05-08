/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "MediaData.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/Tuple.h"
#include "BufferStream.h"
#include "MP4Metadata.h"
#include "MoofParser.h"
#include "TelemetryFixture.h"
#include "TelemetryTestHelpers.h"

class TestStream;
namespace mozilla {
DDLoggedTypeNameAndBase(::TestStream, ByteStream);
}  // namespace mozilla

using namespace mozilla;

static const uint32_t E = MP4Metadata::NumberTracksError();

class TestStream : public ByteStream,
                   public DecoderDoctorLifeLogger<TestStream> {
 public:
  TestStream(const uint8_t* aBuffer, size_t aSize)
      : mHighestSuccessfulEndOffset(0), mBuffer(aBuffer), mSize(aSize) {}
  bool ReadAt(int64_t aOffset, void* aData, size_t aLength,
              size_t* aBytesRead) override {
    if (aOffset < 0 || aOffset > static_cast<int64_t>(mSize)) {
      return false;
    }
    // After the test, 0 <= aOffset <= mSize <= SIZE_MAX, so it's safe to cast
    // to size_t.
    size_t offset = static_cast<size_t>(aOffset);
    // Don't read past the end (but it's not an error to try).
    if (aLength > mSize - offset) {
      aLength = mSize - offset;
    }
    // Now, 0 <= offset <= offset + aLength <= mSize <= SIZE_MAX.
    *aBytesRead = aLength;
    memcpy(aData, mBuffer + offset, aLength);
    if (mHighestSuccessfulEndOffset < offset + aLength) {
      mHighestSuccessfulEndOffset = offset + aLength;
    }
    return true;
  }
  bool CachedReadAt(int64_t aOffset, void* aData, size_t aLength,
                    size_t* aBytesRead) override {
    return ReadAt(aOffset, aData, aLength, aBytesRead);
  }
  bool Length(int64_t* aLength) override {
    *aLength = mSize;
    return true;
  }
  void DiscardBefore(int64_t aOffset) override {}

  // Offset past the last character ever read. 0 when nothing read yet.
  size_t mHighestSuccessfulEndOffset;

 protected:
  virtual ~TestStream() = default;

  const uint8_t* mBuffer;
  size_t mSize;
};

TEST(MP4Metadata, EmptyStream)
{
  RefPtr<ByteStream> stream = new TestStream(nullptr, 0);

  MP4Metadata::ResultAndByteBuffer metadataBuffer =
      MP4Metadata::Metadata(stream);
  EXPECT_TRUE(NS_OK != metadataBuffer.Result());
  EXPECT_FALSE(static_cast<bool>(metadataBuffer.Ref()));

  MP4Metadata metadata(stream);
  EXPECT_TRUE(0u ==
                  metadata.GetNumberTracks(TrackInfo::kUndefinedTrack).Ref() ||
              E == metadata.GetNumberTracks(TrackInfo::kUndefinedTrack).Ref());
  EXPECT_TRUE(0u == metadata.GetNumberTracks(TrackInfo::kAudioTrack).Ref() ||
              E == metadata.GetNumberTracks(TrackInfo::kAudioTrack).Ref());
  EXPECT_TRUE(0u == metadata.GetNumberTracks(TrackInfo::kVideoTrack).Ref() ||
              E == metadata.GetNumberTracks(TrackInfo::kVideoTrack).Ref());
  EXPECT_TRUE(0u == metadata.GetNumberTracks(TrackInfo::kTextTrack).Ref() ||
              E == metadata.GetNumberTracks(TrackInfo::kTextTrack).Ref());
  EXPECT_FALSE(metadata.GetTrackInfo(TrackInfo::kAudioTrack, 0).Ref());
  EXPECT_FALSE(metadata.GetTrackInfo(TrackInfo::kVideoTrack, 0).Ref());
  EXPECT_FALSE(metadata.GetTrackInfo(TrackInfo::kTextTrack, 0).Ref());
  // We can seek anywhere in any MPEG4.
  EXPECT_TRUE(metadata.CanSeek());
  EXPECT_FALSE(metadata.Crypto().Ref()->valid);
}

TEST(MoofParser, EmptyStream)
{
  RefPtr<ByteStream> stream = new TestStream(nullptr, 0);

  MoofParser parser(stream, AsVariant(ParseAllTracks{}), false);
  EXPECT_EQ(0u, parser.mOffset);
  EXPECT_TRUE(parser.ReachedEnd());

  MediaByteRangeSet byteRanges;
  EXPECT_FALSE(parser.RebuildFragmentedIndex(byteRanges));

  EXPECT_TRUE(parser.GetCompositionRange(byteRanges).IsNull());
  EXPECT_TRUE(parser.mInitRange.IsEmpty());
  EXPECT_EQ(0u, parser.mOffset);
  EXPECT_TRUE(parser.ReachedEnd());
  RefPtr<MediaByteBuffer> metadataBuffer = parser.Metadata();
  EXPECT_FALSE(metadataBuffer);
  EXPECT_TRUE(parser.FirstCompleteMediaSegment().IsEmpty());
  EXPECT_TRUE(parser.FirstCompleteMediaHeader().IsEmpty());
}

nsTArray<uint8_t> ReadTestFile(const char* aFilename) {
  if (!aFilename) {
    return {};
  }
  FILE* f = fopen(aFilename, "rb");
  if (!f) {
    return {};
  }

  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return {};
  }
  long position = ftell(f);
  // I know EOF==-1, so this test is made obsolete by '<0', but I don't want
  // the code to rely on that.
  if (position == 0 || position == EOF || position < 0) {
    fclose(f);
    return {};
  }
  if (fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    return {};
  }

  size_t len = static_cast<size_t>(position);
  nsTArray<uint8_t> buffer(len);
  buffer.SetLength(len);
  size_t read = fread(buffer.Elements(), 1, len, f);
  fclose(f);
  if (read != len) {
    return {};
  }

  return buffer;
}

struct TestFileData {
  const char* mFilename;
  bool mParseResult;
  uint32_t mNumberVideoTracks;
  bool mHasVideoIndice;
  int64_t mVideoDuration;  // For first video track, -1 if N/A.
  int32_t mWidth;
  int32_t mHeight;
  uint32_t mNumberAudioTracks;
  int64_t mAudioDuration;  // For first audio track, -1 if N/A.
  bool mHasCrypto;  // Note, MP4Metadata only considers pssh box for crypto.
  uint64_t mMoofReachedOffset;  // or 0 for the end.
  bool mValidMoof;
  bool mHeader;
  int8_t mAudioProfile;
};

static const TestFileData testFiles[] = {
    // filename parses? #V hasVideoIndex vDur w h #A aDur hasCrypto? moofOffset
    // validMoof? hasHeader? audio_profile
    {"test_case_1156505.mp4", false, 0, false, -1, 0, 0, 0, -1, false, 152,
     false, false, 0},  // invalid ''trak box
    {"test_case_1181213.mp4", true, 1, true, 416666, 320, 240, 1, 477460, true,
     0, false, false, 2},
    {"test_case_1181215.mp4", true, 0, false, -1, 0, 0, 0, -1, false, 0, false,
     false, 0},
    {"test_case_1181223.mp4", false, 0, false, 416666, 320, 240, 0, -1, false,
     0, false, false, 0},
    {"test_case_1181719.mp4", false, 0, false, -1, 0, 0, 0, -1, false, 0, false,
     false, 0},
    {"test_case_1185230.mp4", true, 2, true, 416666, 320, 240, 2, 5, false, 0,
     false, false, 2},
    {"test_case_1187067.mp4", true, 1, true, 80000, 160, 90, 0, -1, false, 0,
     false, false, 0},
    {"test_case_1200326.mp4", false, 0, false, -1, 0, 0, 0, -1, false, 0, false,
     false, 0},
    {"test_case_1204580.mp4", true, 1, true, 502500, 320, 180, 0, -1, false, 0,
     false, false, 0},
    {"test_case_1216748.mp4", false, 0, false, -1, 0, 0, 0, -1, false, 152,
     false, false, 0},  // invalid 'trak' box
    {"test_case_1296473.mp4", false, 0, false, -1, 0, 0, 0, -1, false, 0, false,
     false, 0},
    {"test_case_1296532.mp4", true, 1, true, 5589333, 560, 320, 1, 5589333,
     true, 0, true, true, 2},
    {"test_case_1301065.mp4", true, 0, false, -1, 0, 0, 1, 100079991719000000,
     false, 0, false, false, 2},
    {"test_case_1301065-u32max.mp4", true, 0, false, -1, 0, 0, 1, 97391548639,
     false, 0, false, false, 2},
    {"test_case_1301065-max-ez.mp4", true, 0, false, -1, 0, 0, 1,
     209146758205306, false, 0, false, false, 2},
    {"test_case_1301065-harder.mp4", true, 0, false, -1, 0, 0, 1,
     209146758205328, false, 0, false, false, 2},
    {"test_case_1301065-max-ok.mp4", true, 0, false, -1, 0, 0, 1,
     9223372036854775804, false, 0, false, false, 2},
    // The duration is overflow for int64_t in TestFileData, parser uses
    // uint64_t so
    // this file is ignore.
    //{ "test_case_1301065-overfl.mp4", 0, -1,   0,   0, 1, 9223372036854775827,
    //                                                          false,   0,
    //                                                          false, false, 2
    //                                                          },
    {"test_case_1301065-i64max.mp4", true, 0, false, -1, 0, 0, 0, -1, false, 0,
     false, false, 0},
    {"test_case_1301065-i64min.mp4", true, 0, false, -1, 0, 0, 0, -1, false, 0,
     false, false, 0},
    {"test_case_1301065-u64max.mp4", true, 0, false, -1, 0, 0, 1, 0, false, 0,
     false, false, 2},
    {"test_case_1329061.mov", false, 0, false, -1, 0, 0, 1, 234567981, false, 0,
     false, false, 2},
    {"test_case_1351094.mp4", true, 0, false, -1, 0, 0, 0, -1, false, 0, true,
     true, 0},
    {"test_case_1389299.mp4", true, 1, true, 5589333, 560, 320, 1, 5589333,
     true, 0, true, true, 2},

    {"test_case_1389527.mp4", true, 1, false, 5005000, 80, 128, 1, 4992000,
     false, 0, false, false, 2},
    {"test_case_1395244.mp4", true, 1, true, 416666, 320, 240, 1, 477460, false,
     0, false, false, 2},
    {"test_case_1388991.mp4", true, 0, false, -1, 0, 0, 1, 30000181, false, 0,
     false, false, 2},
    {"test_case_1380468.mp4", false, 0, false, 0, 0, 0, 0, 0, false, 0, false,
     false, 0},
    {"test_case_1410565.mp4", false, 0, false, 0, 0, 0, 0, 0, false, 955100,
     true, true, 2},  // negative 'timescale'
    {"test_case_1513651-2-sample-description-entries.mp4", true, 1, true,
     9843344, 400, 300, 0, -1, true, 0, false, false, 0},
    {"test_case_1519617-cenc-init-with-track_id-0.mp4", true, 1, true, 0, 1272,
     530, 0, -1, false, 0, false, false,
     0},  // Uses bad track id 0 and has a sinf but no pssh
    {"test_case_1519617-track2-trafs-removed.mp4", true, 1, true, 10032000, 400,
     300, 1, 10032000, false, 0, true, true, 2},
    {"test_case_1519617-video-has-track_id-0.mp4", true, 1, true, 10032000, 400,
     300, 1, 10032000, false, 0, true, true, 2},  // Uses bad track id 0
};

TEST(MP4Metadata, test_case_mp4)
{
  const TestFileData* tests = nullptr;
  size_t length = 0;

  tests = testFiles;
  length = ArrayLength(testFiles);

  for (size_t test = 0; test < length; ++test) {
    nsTArray<uint8_t> buffer = ReadTestFile(tests[test].mFilename);
    ASSERT_FALSE(buffer.IsEmpty());
    RefPtr<ByteStream> stream =
        new TestStream(buffer.Elements(), buffer.Length());

    MP4Metadata::ResultAndByteBuffer metadataBuffer =
        MP4Metadata::Metadata(stream);
    EXPECT_EQ(NS_OK, metadataBuffer.Result());
    EXPECT_TRUE(metadataBuffer.Ref());

    MP4Metadata metadata(stream);
    nsresult res = metadata.Parse();
    EXPECT_EQ(tests[test].mParseResult, NS_SUCCEEDED(res))
        << tests[test].mFilename;
    if (!tests[test].mParseResult) {
      continue;
    }

    EXPECT_EQ(tests[test].mNumberAudioTracks,
              metadata.GetNumberTracks(TrackInfo::kAudioTrack).Ref())
        << tests[test].mFilename;
    EXPECT_EQ(tests[test].mNumberVideoTracks,
              metadata.GetNumberTracks(TrackInfo::kVideoTrack).Ref())
        << tests[test].mFilename;
    // If there is an error, we should expect an error code instead of zero
    // for non-Audio/Video tracks.
    const uint32_t None = (tests[test].mNumberVideoTracks == E) ? E : 0;
    EXPECT_EQ(None, metadata.GetNumberTracks(TrackInfo::kUndefinedTrack).Ref())
        << tests[test].mFilename;
    EXPECT_EQ(None, metadata.GetNumberTracks(TrackInfo::kTextTrack).Ref())
        << tests[test].mFilename;
    EXPECT_FALSE(metadata.GetTrackInfo(TrackInfo::kUndefinedTrack, 0).Ref());
    MP4Metadata::ResultAndTrackInfo trackInfo =
        metadata.GetTrackInfo(TrackInfo::kVideoTrack, 0);
    if (!!tests[test].mNumberVideoTracks) {
      ASSERT_TRUE(!!trackInfo.Ref());
      const VideoInfo* videoInfo = trackInfo.Ref()->GetAsVideoInfo();
      ASSERT_TRUE(!!videoInfo);
      EXPECT_TRUE(videoInfo->IsValid()) << tests[test].mFilename;
      EXPECT_TRUE(videoInfo->IsVideo()) << tests[test].mFilename;
      EXPECT_EQ(tests[test].mVideoDuration,
                videoInfo->mDuration.ToMicroseconds())
          << tests[test].mFilename;
      EXPECT_EQ(tests[test].mWidth, videoInfo->mDisplay.width)
          << tests[test].mFilename;
      EXPECT_EQ(tests[test].mHeight, videoInfo->mDisplay.height)
          << tests[test].mFilename;

      MP4Metadata::ResultAndIndice indices =
          metadata.GetTrackIndice(videoInfo->mTrackId);
      EXPECT_EQ(!!indices.Ref(), tests[test].mHasVideoIndice)
          << tests[test].mFilename;
      if (tests[test].mHasVideoIndice) {
        for (size_t i = 0; i < indices.Ref()->Length(); i++) {
          Index::Indice data;
          EXPECT_TRUE(indices.Ref()->GetIndice(i, data))
              << tests[test].mFilename;
          EXPECT_TRUE(data.start_offset <= data.end_offset)
              << tests[test].mFilename;
          EXPECT_TRUE(data.start_composition <= data.end_composition)
              << tests[test].mFilename;
        }
      }
    }
    trackInfo = metadata.GetTrackInfo(TrackInfo::kAudioTrack, 0);
    if (tests[test].mNumberAudioTracks == 0 ||
        tests[test].mNumberAudioTracks == E) {
      EXPECT_TRUE(!trackInfo.Ref()) << tests[test].mFilename;
    } else {
      ASSERT_TRUE(!!trackInfo.Ref());
      const AudioInfo* audioInfo = trackInfo.Ref()->GetAsAudioInfo();
      ASSERT_TRUE(!!audioInfo);
      EXPECT_TRUE(audioInfo->IsValid()) << tests[test].mFilename;
      EXPECT_TRUE(audioInfo->IsAudio()) << tests[test].mFilename;
      EXPECT_EQ(tests[test].mAudioDuration,
                audioInfo->mDuration.ToMicroseconds())
          << tests[test].mFilename;
      EXPECT_EQ(tests[test].mAudioProfile, audioInfo->mProfile)
          << tests[test].mFilename;
      if (tests[test].mAudioDuration != audioInfo->mDuration.ToMicroseconds()) {
        MOZ_RELEASE_ASSERT(false);
      }

      MP4Metadata::ResultAndIndice indices =
          metadata.GetTrackIndice(audioInfo->mTrackId);
      EXPECT_TRUE(!!indices.Ref()) << tests[test].mFilename;
      for (size_t i = 0; i < indices.Ref()->Length(); i++) {
        Index::Indice data;
        EXPECT_TRUE(indices.Ref()->GetIndice(i, data)) << tests[test].mFilename;
        EXPECT_TRUE(data.start_offset <= data.end_offset)
            << tests[test].mFilename;
        EXPECT_TRUE(int64_t(data.start_composition) <=
                    int64_t(data.end_composition))
            << tests[test].mFilename;
      }
    }
    EXPECT_FALSE(metadata.GetTrackInfo(TrackInfo::kTextTrack, 0).Ref())
        << tests[test].mFilename;
    // We can see anywhere in any MPEG4.
    EXPECT_TRUE(metadata.CanSeek()) << tests[test].mFilename;
    EXPECT_EQ(tests[test].mHasCrypto, metadata.Crypto().Ref()->valid)
        << tests[test].mFilename;
  }
}

// This test was disabled by Bug 1224019 for producing way too much output.
// This test no longer produces such output, as we've moved away from
// stagefright, but it does take a long time to run. I can be useful to enable
// as a sanity check on changes to the parser, but is too taxing to run as part
// of normal test execution.
#if 0
TEST(MP4Metadata, test_case_mp4_subsets) {
  static const size_t step = 1u;
  for (size_t test = 0; test < ArrayLength(testFiles); ++test) {
    nsTArray<uint8_t> buffer = ReadTestFile(testFiles[test].mFilename);
    ASSERT_FALSE(buffer.IsEmpty());
    ASSERT_LE(step, buffer.Length());
    // Just exercizing the parser starting at different points through the file,
    // making sure it doesn't crash.
    // No checks because results would differ for each position.
    for (size_t offset = 0; offset < buffer.Length() - step; offset += step) {
      size_t size = buffer.Length() - offset;
      while (size > 0) {
        RefPtr<TestStream> stream =
          new TestStream(buffer.Elements() + offset, size);

        MP4Metadata::ResultAndByteBuffer metadataBuffer =
          MP4Metadata::Metadata(stream);
        MP4Metadata metadata(stream);

        if (stream->mHighestSuccessfulEndOffset <= 0) {
          // No successful reads -> Cutting down the size won't change anything.
          break;
        }
        if (stream->mHighestSuccessfulEndOffset < size) {
          // Read up to a point before the end -> Resize down to that point.
          size = stream->mHighestSuccessfulEndOffset;
        } else {
          // Read up to the end (or after?!) -> Just cut 1 byte.
          size -= 1;
        }
      }
    }
  }
}
#endif

#if !defined(XP_WIN) || !defined(MOZ_ASAN)  // OOMs on Windows ASan
TEST(MoofParser, test_case_mp4)
{
  const TestFileData* tests = nullptr;
  size_t length = 0;

  tests = testFiles;
  length = ArrayLength(testFiles);

  for (size_t test = 0; test < length; ++test) {
    nsTArray<uint8_t> buffer = ReadTestFile(tests[test].mFilename);
    ASSERT_FALSE(buffer.IsEmpty());
    RefPtr<ByteStream> stream =
        new TestStream(buffer.Elements(), buffer.Length());

    MoofParser parser(stream, AsVariant(ParseAllTracks{}), false);
    EXPECT_EQ(0u, parser.mOffset) << tests[test].mFilename;
    EXPECT_FALSE(parser.ReachedEnd()) << tests[test].mFilename;
    EXPECT_TRUE(parser.mInitRange.IsEmpty()) << tests[test].mFilename;

    RefPtr<MediaByteBuffer> metadataBuffer = parser.Metadata();
    EXPECT_TRUE(metadataBuffer) << tests[test].mFilename;

    EXPECT_FALSE(parser.mInitRange.IsEmpty()) << tests[test].mFilename;
    const MediaByteRangeSet byteRanges(
        MediaByteRange(0, int64_t(buffer.Length())));
    EXPECT_EQ(tests[test].mValidMoof, parser.RebuildFragmentedIndex(byteRanges))
        << tests[test].mFilename;
    if (tests[test].mMoofReachedOffset == 0) {
      EXPECT_EQ(buffer.Length(), parser.mOffset) << tests[test].mFilename;
      EXPECT_TRUE(parser.ReachedEnd()) << tests[test].mFilename;
    } else {
      EXPECT_EQ(tests[test].mMoofReachedOffset, parser.mOffset)
          << tests[test].mFilename;
      EXPECT_FALSE(parser.ReachedEnd()) << tests[test].mFilename;
    }

    EXPECT_FALSE(parser.mInitRange.IsEmpty()) << tests[test].mFilename;
    EXPECT_TRUE(parser.GetCompositionRange(byteRanges).IsNull())
        << tests[test].mFilename;
    EXPECT_TRUE(parser.FirstCompleteMediaSegment().IsEmpty())
        << tests[test].mFilename;
    EXPECT_EQ(tests[test].mHeader, !parser.FirstCompleteMediaHeader().IsEmpty())
        << tests[test].mFilename;
  }
}

TEST(MoofParser, test_case_sample_description_entries)
{
  const TestFileData* tests = testFiles;
  size_t length = ArrayLength(testFiles);

  for (size_t test = 0; test < length; ++test) {
    nsTArray<uint8_t> buffer = ReadTestFile(tests[test].mFilename);
    ASSERT_FALSE(buffer.IsEmpty());
    RefPtr<ByteStream> stream =
        new TestStream(buffer.Elements(), buffer.Length());

    // Parse the first track. Treating it as audio is hacky, but this doesn't
    // affect how we read the sample description entries.
    uint32_t trackNumber = 1;
    MoofParser parser(stream, AsVariant(trackNumber), false);
    EXPECT_EQ(0u, parser.mOffset) << tests[test].mFilename;
    EXPECT_FALSE(parser.ReachedEnd()) << tests[test].mFilename;
    EXPECT_TRUE(parser.mInitRange.IsEmpty()) << tests[test].mFilename;

    // Explicitly don't call parser.Metadata() so that the parser itself will
    // read the metadata as if we're in a fragmented case. Otherwise the parser
    // won't read the sample description table.

    const MediaByteRangeSet byteRanges(
        MediaByteRange(0, int64_t(buffer.Length())));
    EXPECT_EQ(tests[test].mValidMoof, parser.RebuildFragmentedIndex(byteRanges))
        << tests[test].mFilename;

    // We only care about crypto data from the samples descriptions right now.
    // This test should be expanded should we read further information.
    if (tests[test].mHasCrypto) {
      uint32_t numEncryptedEntries = 0;
      // It's possible to have multiple sample description entries, but we only
      // expect one to carry crypto info for encrypted tracks.
      for (SampleDescriptionEntry entry : parser.mSampleDescriptions) {
        if (entry.mIsEncryptedEntry) {
          numEncryptedEntries++;
        }
      }
      EXPECT_EQ(1u, numEncryptedEntries) << tests[test].mFilename;
    }
  }
}
#endif  // !defined(XP_WIN) || !defined(MOZ_ASAN)

// We should gracefully handle track_id 0 since Bug 1519617. We'd previously
// used id 0 to trigger special handling in the MoofParser to read multiple
// track metadata, but since muxers use track id 0 in the wild, we want to
// make sure they can't accidentally trigger such handling.
TEST(MoofParser, test_case_track_id_0_does_not_read_multitracks)
{
  const char* zeroTrackIdFileName =
      "test_case_1519617-video-has-track_id-0.mp4";
  nsTArray<uint8_t> buffer = ReadTestFile(zeroTrackIdFileName);

  ASSERT_FALSE(buffer.IsEmpty());
  RefPtr<ByteStream> stream =
      new TestStream(buffer.Elements(), buffer.Length());

  // Parse track id 0. We expect to only get metadata from that track, not the
  // other track with id 2.
  const uint32_t videoTrackId = 0;
  MoofParser parser(stream, AsVariant(videoTrackId), false);

  // Explicitly don't call parser.Metadata() so that the parser itself will
  // read the metadata as if we're in a fragmented case. Otherwise we won't
  // read the trak data.

  const MediaByteRangeSet byteRanges(
      MediaByteRange(0, int64_t(buffer.Length())));
  EXPECT_TRUE(parser.RebuildFragmentedIndex(byteRanges))
      << "MoofParser should find a valid moof as the file contains one!";

  // Verify we only have data from track 0, if we parsed multiple tracks we'd
  // find some of the audio track metadata here. Only check for values that
  // differ between tracks.
  const uint32_t videoTimescale = 90000;
  const uint32_t videoSampleDuration = 3000;
  const uint32_t videoSampleFlags = 0x10000;
  const uint32_t videoNumSampleDescriptionEntries = 1;
  EXPECT_EQ(videoTimescale, parser.mMdhd.mTimescale)
      << "Wrong timescale for video track! If value is 22050, we've read from "
         "the audio track!";
  EXPECT_EQ(videoTrackId, parser.mTrex.mTrackId)
      << "Wrong track id for video track! If value is 2, we've read from the "
         "audio track!";
  EXPECT_EQ(videoSampleDuration, parser.mTrex.mDefaultSampleDuration)
      << "Wrong sample duration for video track! If value is 1024, we've read "
         "from the audio track!";
  EXPECT_EQ(videoSampleFlags, parser.mTrex.mDefaultSampleFlags)
      << "Wrong sample flags for video track! If value is 0x2000000 (note "
         "that's hex), we've read from the audio track!";
  EXPECT_EQ(videoNumSampleDescriptionEntries,
            parser.mSampleDescriptions.Length())
      << "Wrong number of sample descriptions for video track! If value is 2, "
         "then we've read sample description information from video and audio "
         "tracks!";
}

// We should gracefully handle track_id 0 since Bug 1519617. This includes
// handling crypto data from the sinf box in the MoofParser. Note, as of the
// time of writing, MP4Metadata uses the presence of a pssh box to determine
// if its crypto member is valid. However, even on files where the pssh isn't
// in the init segment, the MoofParser should still read the sinf, as in this
// testcase.
TEST(MoofParser, test_case_track_id_0_reads_crypto_metadata)
{
  const char* zeroTrackIdFileName =
      "test_case_1519617-cenc-init-with-track_id-0.mp4";
  nsTArray<uint8_t> buffer = ReadTestFile(zeroTrackIdFileName);

  ASSERT_FALSE(buffer.IsEmpty());
  RefPtr<ByteStream> stream =
      new TestStream(buffer.Elements(), buffer.Length());

  // Parse track id 0. We expect to only get metadata from that track, not the
  // other track with id 2.
  const uint32_t videoTrackId = 0;
  MoofParser parser(stream, AsVariant(videoTrackId), false);

  // Explicitly don't call parser.Metadata() so that the parser itself will
  // read the metadata as if we're in a fragmented case. Otherwise we won't
  // read the trak data.

  const MediaByteRangeSet byteRanges(
      MediaByteRange(0, int64_t(buffer.Length())));
  EXPECT_FALSE(parser.RebuildFragmentedIndex(byteRanges))
      << "MoofParser should not find a valid moof, this is just an init "
         "segment!";

  // Verify we only have data from track 0, if we parsed multiple tracks we'd
  // find some of the audio track metadata here. Only check for values that
  // differ between tracks.
  const size_t numSampleDescriptionEntries = 1;
  const uint32_t defaultPerSampleIVSize = 8;
  const size_t keyIdLength = 16;
  const uint32_t defaultKeyId[keyIdLength] = {
      0x43, 0xbe, 0x13, 0xd0, 0x26, 0xc9, 0x41, 0x54,
      0x8f, 0xed, 0xf9, 0x54, 0x1a, 0xef, 0x6b, 0x0e};
  EXPECT_TRUE(parser.mSinf.IsValid())
      << "Should have a sinf that has crypto data!";
  EXPECT_EQ(defaultPerSampleIVSize, parser.mSinf.mDefaultIVSize)
      << "Wrong default per sample IV size for track! If 0 indicates we failed "
         "to parse some crypto info!";
  for (size_t i = 0; i < keyIdLength; i++) {
    EXPECT_EQ(defaultKeyId[i], parser.mSinf.mDefaultKeyID[i])
        << "Mismatched default key ID byte at index " << i
        << " indicates we failed to parse some crypto info!";
  }
  ASSERT_EQ(numSampleDescriptionEntries, parser.mSampleDescriptions.Length())
      << "Wrong number of sample descriptions for track! If 0, indicates we "
         "failed to parse some expected crypto!";
  EXPECT_TRUE(parser.mSampleDescriptions[0].mIsEncryptedEntry)
      << "Sample description should be marked as encrypted!";
}

// The MoofParser may be asked to parse metadata for multiple tracks, but then
// be presented with fragments/moofs that contain data for only a subset of
// those tracks. I.e. metadata contains information for tracks with ids 1 and 2,
// but then the moof parser only receives moofs with data for track id 1. We
// should parse such fragmented media. In this test the metadata contains info
// for track ids 1 and 2, but track 2's track fragment headers (traf) have been
// over written with free space boxes (free).
TEST(MoofParser, test_case_moofs_missing_trafs)
{
  const char* noTrafsForTrack2MoofsFileName =
      "test_case_1519617-track2-trafs-removed.mp4";
  nsTArray<uint8_t> buffer = ReadTestFile(noTrafsForTrack2MoofsFileName);

  ASSERT_FALSE(buffer.IsEmpty());
  RefPtr<ByteStream> stream =
      new TestStream(buffer.Elements(), buffer.Length());

  // Create parser that will read metadata from all tracks.
  MoofParser parser(stream, AsVariant(ParseAllTracks{}), false);

  // Explicitly don't call parser.Metadata() so that the parser itself will
  // read the metadata as if we're in a fragmented case. Otherwise we won't
  // read the trak data.

  const MediaByteRangeSet byteRanges(
      MediaByteRange(0, int64_t(buffer.Length())));
  EXPECT_TRUE(parser.RebuildFragmentedIndex(byteRanges))
      << "MoofParser should find a valid moof, there's 2 in the file!";

  // Verify we've found 2 moofs and that the parser was able to parse them.
  const size_t numMoofs = 2;
  EXPECT_EQ(numMoofs, parser.Moofs().Length())
      << "File has 2 moofs, we should have read both";
  for (size_t i = 0; i < parser.Moofs().Length(); i++) {
    EXPECT_TRUE(parser.Moofs()[i].IsValid()) << "All moofs should be valid";
  }
}

// This test was disabled by Bug 1224019 for producing way too much output.
// This test no longer produces such output, as we've moved away from
// stagefright, but it does take a long time to run. I can be useful to enable
// as a sanity check on changes to the parser, but is too taxing to run as part
// of normal test execution.
#if 0
TEST(MoofParser, test_case_mp4_subsets) {
  const size_t step = 1u;
  for (size_t test = 0; test < ArrayLength(testFiles); ++test) {
    nsTArray<uint8_t> buffer = ReadTestFile(testFiles[test].mFilename);
    ASSERT_FALSE(buffer.IsEmpty());
    ASSERT_LE(step, buffer.Length());
    // Just exercizing the parser starting at different points through the file,
    // making sure it doesn't crash.
    // No checks because results would differ for each position.
    for (size_t offset = 0; offset < buffer.Length() - step; offset += step) {
      size_t size = buffer.Length() - offset;
      while (size > 0) {
        RefPtr<TestStream> stream =
          new TestStream(buffer.Elements() + offset, size);

        MoofParser parser(stream, AsVariant(ParseAllTracks{}), false);
        MediaByteRangeSet byteRanges;
        EXPECT_FALSE(parser.RebuildFragmentedIndex(byteRanges));
        parser.GetCompositionRange(byteRanges);
        RefPtr<MediaByteBuffer> metadataBuffer = parser.Metadata();
        parser.FirstCompleteMediaSegment();
        parser.FirstCompleteMediaHeader();

        if (stream->mHighestSuccessfulEndOffset <= 0) {
          // No successful reads -> Cutting down the size won't change anything.
          break;
        }
        if (stream->mHighestSuccessfulEndOffset < size) {
          // Read up to a point before the end -> Resize down to that point.
          size = stream->mHighestSuccessfulEndOffset;
        } else {
          // Read up to the end (or after?!) -> Just cut 1 byte.
          size -= 1;
        }
      }
    }
  }
}
#endif

uint8_t media_gtest_video_init_mp4[] = {
    0x00, 0x00, 0x00, 0x18, 0x66, 0x74, 0x79, 0x70, 0x69, 0x73, 0x6f, 0x6d,
    0x00, 0x00, 0x00, 0x01, 0x69, 0x73, 0x6f, 0x6d, 0x61, 0x76, 0x63, 0x31,
    0x00, 0x00, 0x02, 0xd1, 0x6d, 0x6f, 0x6f, 0x76, 0x00, 0x00, 0x00, 0x6c,
    0x6d, 0x76, 0x68, 0x64, 0x00, 0x00, 0x00, 0x00, 0xc8, 0x49, 0x73, 0xf8,
    0xc8, 0x4a, 0xc5, 0x7a, 0x00, 0x00, 0x02, 0x58, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x18,
    0x69, 0x6f, 0x64, 0x73, 0x00, 0x00, 0x00, 0x00, 0x10, 0x80, 0x80, 0x80,
    0x07, 0x00, 0x4f, 0xff, 0xff, 0x29, 0x15, 0xff, 0x00, 0x00, 0x02, 0x0d,
    0x74, 0x72, 0x61, 0x6b, 0x00, 0x00, 0x00, 0x5c, 0x74, 0x6b, 0x68, 0x64,
    0x00, 0x00, 0x00, 0x01, 0xc8, 0x49, 0x73, 0xf8, 0xc8, 0x49, 0x73, 0xf9,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x00, 0x00, 0x00, 0x02, 0x80, 0x00, 0x00, 0x01, 0x68, 0x00, 0x00,
    0x00, 0x00, 0x01, 0xa9, 0x6d, 0x64, 0x69, 0x61, 0x00, 0x00, 0x00, 0x20,
    0x6d, 0x64, 0x68, 0x64, 0x00, 0x00, 0x00, 0x00, 0xc8, 0x49, 0x73, 0xf8,
    0xc8, 0x49, 0x73, 0xf9, 0x00, 0x00, 0x75, 0x30, 0x00, 0x00, 0x00, 0x00,
    0x55, 0xc4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x68, 0x64, 0x6c, 0x72,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x76, 0x69, 0x64, 0x65,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x47, 0x50, 0x41, 0x43, 0x20, 0x49, 0x53, 0x4f, 0x20, 0x56, 0x69, 0x64,
    0x65, 0x6f, 0x20, 0x48, 0x61, 0x6e, 0x64, 0x6c, 0x65, 0x72, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x49, 0x6d, 0x69, 0x6e, 0x66, 0x00, 0x00, 0x00, 0x14,
    0x76, 0x6d, 0x68, 0x64, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x64, 0x69, 0x6e, 0x66,
    0x00, 0x00, 0x00, 0x1c, 0x64, 0x72, 0x65, 0x66, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0c, 0x75, 0x72, 0x6c, 0x20,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x09, 0x73, 0x74, 0x62, 0x6c,
    0x00, 0x00, 0x00, 0xad, 0x73, 0x74, 0x73, 0x64, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x9d, 0x61, 0x76, 0x63, 0x31,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x80, 0x01, 0x68, 0x00, 0x48, 0x00, 0x00, 0x00, 0x48, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x18, 0xff, 0xff, 0x00, 0x00, 0x00, 0x33, 0x61, 0x76,
    0x63, 0x43, 0x01, 0x64, 0x00, 0x1f, 0xff, 0xe1, 0x00, 0x1b, 0x67, 0x64,
    0x00, 0x1f, 0xac, 0x2c, 0xc5, 0x02, 0x80, 0xbf, 0xe5, 0xc0, 0x44, 0x00,
    0x00, 0x03, 0x00, 0x04, 0x00, 0x00, 0x03, 0x00, 0xf2, 0x3c, 0x60, 0xc6,
    0x58, 0x01, 0x00, 0x05, 0x68, 0xe9, 0x2b, 0x2c, 0x8b, 0x00, 0x00, 0x00,
    0x14, 0x62, 0x74, 0x72, 0x74, 0x00, 0x01, 0x5a, 0xc2, 0x00, 0x24, 0x74,
    0x38, 0x00, 0x09, 0x22, 0x00, 0x00, 0x00, 0x00, 0x10, 0x73, 0x74, 0x74,
    0x73, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x10, 0x63, 0x74, 0x74, 0x73, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x10, 0x73, 0x74, 0x73, 0x63, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x73, 0x74, 0x73,
    0x7a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x10, 0x73, 0x74, 0x63, 0x6f, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x6d, 0x76, 0x65,
    0x78, 0x00, 0x00, 0x00, 0x10, 0x6d, 0x65, 0x68, 0x64, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x05, 0x76, 0x18, 0x00, 0x00, 0x00, 0x20, 0x74, 0x72, 0x65,
    0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x03, 0xe8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
    0x00};

const uint32_t media_gtest_video_init_mp4_len = 745;

TEST(MP4Metadata, EmptyCTTS)
{
  RefPtr<MediaByteBuffer> buffer =
      new MediaByteBuffer(media_gtest_video_init_mp4_len);
  buffer->AppendElements(media_gtest_video_init_mp4,
                         media_gtest_video_init_mp4_len);
  RefPtr<BufferStream> stream = new BufferStream(buffer);

  MP4Metadata::ResultAndByteBuffer metadataBuffer =
      MP4Metadata::Metadata(stream);
  EXPECT_EQ(NS_OK, metadataBuffer.Result());
  EXPECT_TRUE(metadataBuffer.Ref());

  MP4Metadata metadata(stream);
  EXPECT_EQ(metadata.Parse(), NS_OK);
  EXPECT_EQ(1u, metadata.GetNumberTracks(TrackInfo::kVideoTrack).Ref());
  MP4Metadata::ResultAndTrackInfo track =
      metadata.GetTrackInfo(TrackInfo::kVideoTrack, 0);
  EXPECT_TRUE(track.Ref() != nullptr);
  // We can seek anywhere in any MPEG4.
  EXPECT_TRUE(metadata.CanSeek());
  EXPECT_FALSE(metadata.Crypto().Ref()->valid);
}

// Fixture so we test telemetry probes.
class MP4MetadataTelemetryFixture : public TelemetryTestFixture {};

TEST_F(MP4MetadataTelemetryFixture, Telemetry) {
  // Helper to fetch the metadata from a file and send telemetry in the process.
  auto UpdateMetadataAndHistograms = [](const char* testFileName) {
    nsTArray<uint8_t> buffer = ReadTestFile(testFileName);
    ASSERT_FALSE(buffer.IsEmpty());
    RefPtr<ByteStream> stream =
        new TestStream(buffer.Elements(), buffer.Length());

    MP4Metadata::ResultAndByteBuffer metadataBuffer =
        MP4Metadata::Metadata(stream);
    EXPECT_EQ(NS_OK, metadataBuffer.Result());
    EXPECT_TRUE(metadataBuffer.Ref());

    MP4Metadata metadata(stream);
    nsresult res = metadata.Parse();
    EXPECT_TRUE(NS_SUCCEEDED(res));
    auto audioTrackCount = metadata.GetNumberTracks(TrackInfo::kAudioTrack);
    ASSERT_NE(audioTrackCount.Ref(), MP4Metadata::NumberTracksError());
    auto videoTrackCount = metadata.GetNumberTracks(TrackInfo::kVideoTrack);
    ASSERT_NE(videoTrackCount.Ref(), MP4Metadata::NumberTracksError());

    // Need to read the track data to get telemetry to fire.
    for (uint32_t i = 0; i < audioTrackCount.Ref(); i++) {
      metadata.GetTrackInfo(TrackInfo::kAudioTrack, i);
    }
    for (uint32_t i = 0; i < videoTrackCount.Ref(); i++) {
      metadata.GetTrackInfo(TrackInfo::kVideoTrack, i);
    }
  };

  AutoJSContextWithGlobal cx(mCleanGlobal);

  // Checks the current state of the histograms relating to sample description
  // entries and verifies they're in an expected state.
  // aExpectedMultipleCodecCounts is a tuple where the first value represents
  // the number of expected 'false' count, and the second the expected 'true'
  // count for the sample description entries have multiple codecs histogram.
  // aExpectedMultipleCryptoCounts is the same, but for the sample description
  // entires have multiple crypto histogram.
  // aExpectedSampleDescriptionEntryCounts is a tuple with 6 values, each is
  // the expected number of sample description seen. I.e, the first value in the
  // tuple is the number of tracks we've seen with 0 sample descriptions, the
  // second value with 1 sample description, and so on up to 5 sample
  // descriptions. aFileName is the name of the most recent file we've parsed,
  // and is used to log if our telem counts are not in an expected state.
  auto CheckHistograms =
      [this, &cx](
          const Tuple<uint32_t, uint32_t>& aExpectedMultipleCodecCounts,
          const Tuple<uint32_t, uint32_t>& aExpectedMultipleCryptoCounts,
          const Tuple<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t,
                      uint32_t>& aExpectedSampleDescriptionEntryCounts,
          const char* aFileName) {
        // Get a snapshot of the current histograms
        JS::RootedValue snapshot(cx.GetJSContext());
        TelemetryTestHelpers::GetSnapshots(cx.GetJSContext(), mTelemetry,
                                           "" /* this string is unused */,
                                           &snapshot, false /* is_keyed */);

        // We'll use these to pull values out of the histograms.
        JS::RootedValue values(cx.GetJSContext());
        JS::RootedValue value(cx.GetJSContext());

        // Verify our multiple codecs count histogram.
        JS::RootedValue multipleCodecsHistogram(cx.GetJSContext());
        TelemetryTestHelpers::GetProperty(
            cx.GetJSContext(),
            "MEDIA_MP4_PARSE_SAMPLE_DESCRIPTION_ENTRIES_HAVE_MULTIPLE_CODECS",
            snapshot, &multipleCodecsHistogram);
        ASSERT_TRUE(multipleCodecsHistogram.isObject())
        << "Multiple codecs histogram should exist!";

        TelemetryTestHelpers::GetProperty(cx.GetJSContext(), "values",
                                          multipleCodecsHistogram, &values);
        // False count.
        TelemetryTestHelpers::GetElement(cx.GetJSContext(), 0, values, &value);
        uint32_t uValue = 0;
        JS::ToUint32(cx.GetJSContext(), value, &uValue);
        EXPECT_EQ(Get<0>(aExpectedMultipleCodecCounts), uValue)
            << "Unexpected number of false multiple codecs after parsing "
            << aFileName;
        // True count.
        TelemetryTestHelpers::GetElement(cx.GetJSContext(), 1, values, &value);
        JS::ToUint32(cx.GetJSContext(), value, &uValue);
        EXPECT_EQ(Get<1>(aExpectedMultipleCodecCounts), uValue)
            << "Unexpected number of true multiple codecs after parsing "
            << aFileName;

        // Verify our multiple crypto count histogram.
        JS::RootedValue multipleCryptoHistogram(cx.GetJSContext());
        TelemetryTestHelpers::GetProperty(
            cx.GetJSContext(),
            "MEDIA_MP4_PARSE_SAMPLE_DESCRIPTION_ENTRIES_HAVE_MULTIPLE_CRYPTO",
            snapshot, &multipleCryptoHistogram);
        ASSERT_TRUE(multipleCryptoHistogram.isObject())
        << "Multiple crypto histogram should exist!";

        TelemetryTestHelpers::GetProperty(cx.GetJSContext(), "values",
                                          multipleCryptoHistogram, &values);
        // False count.
        TelemetryTestHelpers::GetElement(cx.GetJSContext(), 0, values, &value);
        JS::ToUint32(cx.GetJSContext(), value, &uValue);
        EXPECT_EQ(Get<0>(aExpectedMultipleCryptoCounts), uValue)
            << "Unexpected number of false multiple cryptos after parsing "
            << aFileName;
        // True count.
        TelemetryTestHelpers::GetElement(cx.GetJSContext(), 1, values, &value);
        JS::ToUint32(cx.GetJSContext(), value, &uValue);
        EXPECT_EQ(Get<1>(aExpectedMultipleCryptoCounts), uValue)
            << "Unexpected number of true multiple cryptos after parsing "
            << aFileName;

        // Verify our sample description entry count histogram.
        JS::RootedValue numSamplesHistogram(cx.GetJSContext());
        TelemetryTestHelpers::GetProperty(
            cx.GetJSContext(), "MEDIA_MP4_PARSE_NUM_SAMPLE_DESCRIPTION_ENTRIES",
            snapshot, &numSamplesHistogram);
        ASSERT_TRUE(numSamplesHistogram.isObject())
        << "Num sample description entries histogram should exist!";

        TelemetryTestHelpers::GetProperty(cx.GetJSContext(), "values",
                                          numSamplesHistogram, &values);

        TelemetryTestHelpers::GetElement(cx.GetJSContext(), 0, values, &value);
        JS::ToUint32(cx.GetJSContext(), value, &uValue);
        EXPECT_EQ(Get<0>(aExpectedSampleDescriptionEntryCounts), uValue)
            << "Unexpected number of 0 sample entry descriptions after parsing "
            << aFileName;
        TelemetryTestHelpers::GetElement(cx.GetJSContext(), 1, values, &value);
        JS::ToUint32(cx.GetJSContext(), value, &uValue);
        EXPECT_EQ(Get<1>(aExpectedSampleDescriptionEntryCounts), uValue)
            << "Unexpected number of 1 sample entry descriptions after parsing "
            << aFileName;
        TelemetryTestHelpers::GetElement(cx.GetJSContext(), 2, values, &value);
        JS::ToUint32(cx.GetJSContext(), value, &uValue);
        EXPECT_EQ(Get<2>(aExpectedSampleDescriptionEntryCounts), uValue)
            << "Unexpected number of 2 sample entry descriptions after parsing "
            << aFileName;
        TelemetryTestHelpers::GetElement(cx.GetJSContext(), 3, values, &value);
        JS::ToUint32(cx.GetJSContext(), value, &uValue);
        EXPECT_EQ(Get<3>(aExpectedSampleDescriptionEntryCounts), uValue)
            << "Unexpected number of 3 sample entry descriptions after parsing "
            << aFileName;
        TelemetryTestHelpers::GetElement(cx.GetJSContext(), 4, values, &value);
        JS::ToUint32(cx.GetJSContext(), value, &uValue);
        EXPECT_EQ(Get<4>(aExpectedSampleDescriptionEntryCounts), uValue)
            << "Unexpected number of 4 sample entry descriptions after parsing "
            << aFileName;
        TelemetryTestHelpers::GetElement(cx.GetJSContext(), 5, values, &value);
        JS::ToUint32(cx.GetJSContext(), value, &uValue);
        EXPECT_EQ(Get<5>(aExpectedSampleDescriptionEntryCounts), uValue)
            << "Unexpected number of 5 sample entry descriptions after parsing "
            << aFileName;
      };

  // Clear histograms
  TelemetryTestHelpers::GetAndClearHistogram(
      cx.GetJSContext(), mTelemetry,
      NS_LITERAL_CSTRING(
          "MEDIA_MP4_PARSE_SAMPLE_DESCRIPTION_ENTRIES_HAVE_MULTIPLE_CODECS"),
      false /* is_keyed */);

  TelemetryTestHelpers::GetAndClearHistogram(
      cx.GetJSContext(), mTelemetry,
      NS_LITERAL_CSTRING(
          "MEDIA_MP4_PARSE_SAMPLE_DESCRIPTION_ENTRIES_HAVE_MULTIPLE_CRYPTO"),
      false /* is_keyed */);

  TelemetryTestHelpers::GetAndClearHistogram(
      cx.GetJSContext(), mTelemetry,
      NS_LITERAL_CSTRING("MEDIA_MP4_PARSE_NUM_SAMPLE_DESCRIPTION_ENTRIES"),
      false /* is_keyed */);

  // The snapshot won't have any data in it until we populate our histograms, so
  // we don't check for a baseline here. Just read out first MP4 metadata.

  // Grab one of the test cases we know should parse and parse it, this should
  // trigger telemetry gathering.

  // This file contains 2 moovs, each with a video and audio track with one
  // sample description entry. So we should see 4 tracks, each with a single
  // codec, no crypto, and a single sample description entry.
  UpdateMetadataAndHistograms("test_case_1185230.mp4");

  // Verify our histograms are updated.
  CheckHistograms(
      MakeTuple<uint32_t, uint32_t>(4, 0), MakeTuple<uint32_t, uint32_t>(4, 0),
      MakeTuple<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t>(
          0, 4, 0, 0, 0, 0),
      "test_case_1185230.mp4");

  // Parse another test case. This one has a single moov with a single video
  // track. However, the track has two sample description entries, and our
  // updated telemetry should reflect that.
  UpdateMetadataAndHistograms(
      "test_case_1513651-2-sample-description-entries.mp4");

  // Verify our histograms are updated.
  CheckHistograms(
      MakeTuple<uint32_t, uint32_t>(5, 0), MakeTuple<uint32_t, uint32_t>(5, 0),
      MakeTuple<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t>(
          0, 4, 1, 0, 0, 0),
      "test_case_1513651-2-sample-description-entries.mp4");
}
