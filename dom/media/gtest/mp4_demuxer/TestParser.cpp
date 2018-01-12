/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "MediaData.h"
#include "MediaPrefs.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Preferences.h"
#include "BufferStream.h"
#include "MP4Metadata.h"
#include "MoofParser.h"

class TestStream;
namespace mozilla {
DDLoggedTypeNameAndBase(::TestStream, ByteStream);
} // namespace mozilla

using namespace mozilla;

static const uint32_t E = MP4Metadata::NumberTracksError();

class TestStream
  : public ByteStream
  , public DecoderDoctorLifeLogger<TestStream>
{
public:
  TestStream(const uint8_t* aBuffer, size_t aSize)
    : mHighestSuccessfulEndOffset(0)
    , mBuffer(aBuffer)
    , mSize(aSize)
  {
  }
  bool ReadAt(int64_t aOffset, void* aData, size_t aLength,
              size_t* aBytesRead) override
  {
    if (aOffset < 0 || aOffset > static_cast<int64_t>(mSize)) {
      return false;
    }
    // After the test, 0 <= aOffset <= mSize <= SIZE_MAX, so it's safe to cast to size_t.
    size_t offset = static_cast<size_t>(aOffset);
    // Don't read past the end (but it's not an error to try).
    if (aLength > mSize - offset) {
      aLength = mSize - offset;
    }
    // Now, 0 <= offset <= offset + aLength <= mSize <= SIZE_MAX.
    *aBytesRead = aLength;
    memcpy(aData, mBuffer + offset, aLength);
    if (mHighestSuccessfulEndOffset < offset + aLength)
    {
      mHighestSuccessfulEndOffset = offset + aLength;
    }
    return true;
  }
  bool CachedReadAt(int64_t aOffset, void* aData, size_t aLength,
                    size_t* aBytesRead) override
  {
    return ReadAt(aOffset, aData, aLength, aBytesRead);
  }
  bool Length(int64_t* aLength) override
  {
    *aLength = mSize;
    return true;
  }
  void DiscardBefore(int64_t aOffset) override
  {
  }

  // Offset past the last character ever read. 0 when nothing read yet.
  size_t mHighestSuccessfulEndOffset;
protected:
  virtual ~TestStream()
  {
  }

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
  EXPECT_TRUE(0u == metadata.GetNumberTracks(TrackInfo::kUndefinedTrack).Ref() ||
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

  MoofParser parser(stream, 0, false);
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

nsTArray<uint8_t>
ReadTestFile(const char* aFilename)
{
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

struct TestFileData
{
  const char* mFilename;
  bool mParseResult;
  uint32_t mNumberVideoTracks;
  bool mHasVideoIndice;
  int64_t mVideoDuration; // For first video track, -1 if N/A.
  int32_t mWidth;
  int32_t mHeight;
  uint32_t mNumberAudioTracks;
  int64_t mAudioDuration; // For first audio track, -1 if N/A.
  bool mHasCrypto;
  uint64_t mMoofReachedOffset; // or 0 for the end.
  bool mValidMoof;
  bool mHeader;
  int8_t mAudioProfile;
};

static const TestFileData testFiles[] = {
  // filename                     parse #V dur   w    h  #A dur  crypt  off   moof  headr  audio_profile
  { "test_case_1156505.mp4",      false, 0, false, -1,   0,   0, 0, -1, false, 152, false, false, 0 },  // invalid ''trak box
  { "test_case_1181213.mp4",      true, 1, true, 416666,
                                           320, 240, 1, 477460,
                                                             true,   0, false, false, 2 },
  { "test_case_1181215.mp4",      true, 0, false, -1,   0,   0, 0, -1, false,   0, false, false, 0 },
  { "test_case_1181220.mp4",      false, 0, false, -1,   0,   0, 0, -1, false,   0, false, false, 0 },  // invalid audio 'sinf' box
  { "test_case_1181223.mp4",      false, 0, false, 416666,
                                           320, 240, 0, -1, false,   0, false, false, 0 },
  { "test_case_1181719.mp4",      false, 0, false, -1,   0,   0, 0, -1, false,   0, false, false, 0 },
  { "test_case_1185230.mp4",      true, 2, true, 416666,
                                           320, 240, 2,  5, false,   0, false, false, 2 },
  { "test_case_1187067.mp4",      true, 1, true, 80000,
                                           160,  90, 0, -1, false,   0, false, false, 0 },
  { "test_case_1200326.mp4",      false, 0, false, -1,   0,   0, 0, -1, false,   0, false, false, 0 },
  { "test_case_1204580.mp4",      true, 1, true, 502500,
                                           320, 180, 0, -1, false,   0, false, false, 0 },
  { "test_case_1216748.mp4",      false, 0, false, -1,   0,   0, 0, -1, false, 152, false, false, 0 },  // invalid 'trak' box
  { "test_case_1296473.mp4",      false, 0, false, -1,   0,   0, 0, -1, false,   0, false, false, 0 },
  { "test_case_1296532.mp4",      true, 1, true, 5589333,
                                           560, 320, 1, 5589333,
                                                            true,    0, true,  true,  2  },
  { "test_case_1301065.mp4",      true, 0, false, -1,   0,   0, 1, 100079991719000000,
                                                            false,   0, false, false, 2 },
  { "test_case_1301065-u32max.mp4", true, 0, false, -1,   0,   0, 1, 97391548639,
                                                            false,   0, false, false, 2 },
  { "test_case_1301065-max-ez.mp4", true, 0, false, -1,   0,   0, 1, 209146758205306,
                                                            false,   0, false, false, 2 },
  { "test_case_1301065-harder.mp4", true, 0, false, -1,   0,   0, 1, 209146758205328,
                                                            false,   0, false, false, 2 },
  { "test_case_1301065-max-ok.mp4", true, 0, false, -1,   0,   0, 1, 9223372036854775804,
                                                            false,   0, false, false, 2 },
  // The duration is overflow for int64_t in TestFileData, parser uses uint64_t so
  // this file is ignore.
  //{ "test_case_1301065-overfl.mp4", 0, -1,   0,   0, 1, 9223372036854775827,
  //                                                          false,   0, false, false, 2 },
  { "test_case_1301065-i64max.mp4", true, 0, false, -1,   0,   0, 0, -1, false,   0, false, false, 0 },
  { "test_case_1301065-i64min.mp4", true, 0, false, -1,   0,   0, 0, -1, false,   0, false, false, 0 },
  { "test_case_1301065-u64max.mp4", true, 0, false, -1,   0,   0, 1,  0, false,   0, false, false, 2 },
  { "test_case_1329061.mov",        false, 0, false, -1,   0,   0, 1,  234567981,
                                                            false,   0, false, false, 2 },
  { "test_case_1351094.mp4",        true, 0, false, -1,   0,   0, 0, -1, false,   0, true,  true,  0 },
  { "test_case_1389299.mp4",        true, 1, true, 5589333,
                                           560, 320, 1, 5589333,
                                                            true,    0, true,  true,  2  },

  { "test_case_1389527.mp4",        true, 1, false, 5005000,
                                            80, 128, 1, 4992000, false,   0, false, false, 2 },
  { "test_case_1395244.mp4",        true, 1, true, 416666,
                                           320, 240, 1,477460, false,0, false, false, 2 },
  { "test_case_1388991.mp4",        true, 0, false, -1, 0, 0, 1, 30000181, false, 0, false, false, 2 },
  { "test_case_1380468.mp4",        false, 0, false,  0,   0,   0, 0,  0, false,   0, false, false, 0 },
  { "test_case_1410565.mp4",        false, 0, false, 0, 0, 0, 0, 0, false, 955100, true, true, 2 },    // negative 'timescale'
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
    RefPtr<ByteStream> stream = new TestStream(buffer.Elements(), buffer.Length());

    MP4Metadata::ResultAndByteBuffer metadataBuffer =
      MP4Metadata::Metadata(stream);
    EXPECT_EQ(NS_OK, metadataBuffer.Result());
    EXPECT_TRUE(metadataBuffer.Ref());

    MP4Metadata metadata(stream);
    nsresult res = metadata.Parse();
    EXPECT_EQ(tests[test].mParseResult, NS_SUCCEEDED(res)) << tests[test].mFilename;
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
                videoInfo->mDuration.ToMicroseconds()) << tests[test].mFilename;
      EXPECT_EQ(tests[test].mWidth, videoInfo->mDisplay.width) << tests[test].mFilename;
      EXPECT_EQ(tests[test].mHeight, videoInfo->mDisplay.height) << tests[test].mFilename;

      MP4Metadata::ResultAndIndice indices =
        metadata.GetTrackIndice(videoInfo->mTrackId);
      EXPECT_EQ(!!indices.Ref(), tests[test].mHasVideoIndice) << tests[test].mFilename;
      if (tests[test].mHasVideoIndice) {
        for (size_t i = 0; i < indices.Ref()->Length(); i++) {
          Index::Indice data;
          EXPECT_TRUE(indices.Ref()->GetIndice(i, data)) << tests[test].mFilename;
          EXPECT_TRUE(data.start_offset <= data.end_offset) << tests[test].mFilename;
          EXPECT_TRUE(data.start_composition <= data.end_composition) << tests[test].mFilename;
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
                audioInfo->mDuration.ToMicroseconds()) << tests[test].mFilename;
      EXPECT_EQ(tests[test].mAudioProfile, audioInfo->mProfile) << tests[test].mFilename;
      if (tests[test].mAudioDuration !=
          audioInfo->mDuration.ToMicroseconds()) {
        MOZ_RELEASE_ASSERT(false);
      }

      MP4Metadata::ResultAndIndice indices =
        metadata.GetTrackIndice(audioInfo->mTrackId);
      EXPECT_TRUE(!!indices.Ref()) << tests[test].mFilename;
      for (size_t i = 0; i < indices.Ref()->Length(); i++) {
        Index::Indice data;
        EXPECT_TRUE(indices.Ref()->GetIndice(i, data)) << tests[test].mFilename;
        EXPECT_TRUE(data.start_offset <= data.end_offset) << tests[test].mFilename;
        EXPECT_TRUE(int64_t(data.start_composition) <= int64_t(data.end_composition)) << tests[test].mFilename;
      }
    }
    EXPECT_FALSE(metadata.GetTrackInfo(TrackInfo::kTextTrack, 0).Ref()) << tests[test].mFilename;
    // We can see anywhere in any MPEG4.
    EXPECT_TRUE(metadata.CanSeek()) << tests[test].mFilename;
    EXPECT_EQ(tests[test].mHasCrypto, metadata.Crypto().Ref()->valid) << tests[test].mFilename;
  }
}

// Bug 1224019: This test produces way to much output, disabling for now.
#if 0
TEST(MPEG4Metadata, test_case_mp4_subsets)
{
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

TEST(MoofParser, test_case_mp4)
{
  const TestFileData* tests = nullptr;
  size_t length = 0;

  tests = testFiles;
  length = ArrayLength(testFiles);

  for (size_t test = 0; test < length; ++test) {
    nsTArray<uint8_t> buffer = ReadTestFile(tests[test].mFilename);
    ASSERT_FALSE(buffer.IsEmpty());
    RefPtr<ByteStream> stream = new TestStream(buffer.Elements(), buffer.Length());

    MoofParser parser(stream, 0, false);
    EXPECT_EQ(0u, parser.mOffset) << tests[test].mFilename;
    EXPECT_FALSE(parser.ReachedEnd()) << tests[test].mFilename;
    EXPECT_TRUE(parser.mInitRange.IsEmpty()) << tests[test].mFilename;

    RefPtr<MediaByteBuffer> metadataBuffer = parser.Metadata();
    EXPECT_TRUE(metadataBuffer) << tests[test].mFilename;

    EXPECT_FALSE(parser.mInitRange.IsEmpty()) << tests[test].mFilename;
    const MediaByteRangeSet byteRanges(
      MediaByteRange(0, int64_t(buffer.Length())));
    EXPECT_EQ(tests[test].mValidMoof,
              parser.RebuildFragmentedIndex(byteRanges))  << tests[test].mFilename;
    if (tests[test].mMoofReachedOffset == 0) {
      EXPECT_EQ(buffer.Length(), parser.mOffset) << tests[test].mFilename;
      EXPECT_TRUE(parser.ReachedEnd()) << tests[test].mFilename;
    } else {
      EXPECT_EQ(tests[test].mMoofReachedOffset, parser.mOffset) << tests[test].mFilename;
      EXPECT_FALSE(parser.ReachedEnd()) << tests[test].mFilename;
    }

    EXPECT_FALSE(parser.mInitRange.IsEmpty()) << tests[test].mFilename;
    EXPECT_TRUE(parser.GetCompositionRange(byteRanges).IsNull()) << tests[test].mFilename;
    EXPECT_TRUE(parser.FirstCompleteMediaSegment().IsEmpty()) << tests[test].mFilename;
    EXPECT_EQ(tests[test].mHeader,
              !parser.FirstCompleteMediaHeader().IsEmpty()) << tests[test].mFilename;
  }
}

// Bug 1224019: This test produces way to much output, disabling for now.
#if 0
TEST(MoofParser, test_case_mp4_subsets)
{
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

        MoofParser parser(stream, 0, false);
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
  0x00
};

const uint32_t media_gtest_video_init_mp4_len = 745;

TEST(MP4Metadata, EmptyCTTS)
{
  RefPtr<MediaByteBuffer> buffer = new MediaByteBuffer(media_gtest_video_init_mp4_len);
  buffer->AppendElements(media_gtest_video_init_mp4, media_gtest_video_init_mp4_len);
  RefPtr<BufferStream> stream = new BufferStream(buffer);

  MP4Metadata::ResultAndByteBuffer metadataBuffer =
    MP4Metadata::Metadata(stream);
  EXPECT_EQ(NS_OK, metadataBuffer.Result());
  EXPECT_TRUE(metadataBuffer.Ref());

  MP4Metadata metadata(stream);
  EXPECT_EQ(metadata.Parse(), NS_OK);
  EXPECT_EQ(1u, metadata.GetNumberTracks(TrackInfo::kVideoTrack).Ref());
  MP4Metadata::ResultAndTrackInfo track = metadata.GetTrackInfo(TrackInfo::kVideoTrack, 0);
  EXPECT_TRUE(track.Ref() != nullptr);
  // We can seek anywhere in any MPEG4.
  EXPECT_TRUE(metadata.CanSeek());
  EXPECT_FALSE(metadata.Crypto().Ref()->valid);
}

