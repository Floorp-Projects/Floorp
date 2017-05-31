/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "MediaMIMETypes.h"

using namespace mozilla;

TEST(MediaMIMETypes, DependentMIMEType)
{
  static const struct
  {
    const char* mString;
    DependentMediaMIMEType mDependentMediaMIMEType;
  }
  tests[] =
  {
    { "audio/mp4",         MEDIAMIMETYPE("audio/mp4")         },
    { "video/mp4",         MEDIAMIMETYPE("video/mp4")         },
    { "application/x-mp4", MEDIAMIMETYPE("application/x-mp4") }
  };
  for (const auto& test : tests) {
    EXPECT_TRUE(test.mDependentMediaMIMEType.AsDependentString().EqualsASCII(test.mString));
    MediaMIMEType mimetype(test.mDependentMediaMIMEType);
    EXPECT_TRUE(mimetype.AsString().Equals(test.mDependentMediaMIMEType.AsDependentString()));
    EXPECT_EQ(mimetype, test.mDependentMediaMIMEType);
    EXPECT_EQ(mimetype, MediaMIMEType(test.mDependentMediaMIMEType));
  }
}

TEST(MediaMIMETypes, MakeMediaMIMEType_bad)
{
  static const char* tests[] =
  {
    "",
    " ",
    "/",
    "audio",
    "audio/",
    "mp4",
    "/mp4",
    "a/b"
  };

  for (const auto& test : tests) {
    Maybe<MediaMIMEType> type = MakeMediaMIMEType(test);
    EXPECT_TRUE(type.isNothing())
      << "MakeMediaMIMEType(\"" << test << "\").isNothing()";
  }
}

TEST(MediaMIMETypes, MediaMIMEType)
{
  static const struct
  {
    const char* mTypeString;
    const char* mAsString;
    bool mApplication;
    bool mAudio;
    bool mVideo;
    bool mEqualsLiteralVideoSlashMp4; // tests `== "video/mp4"`
  } tests[] =
  { // in                    AsString         app    audio  video  ==v/mp4
    { "video/mp4",           "video/mp4",     false, false, true,  true  },
    { "video/mp4; codecs=0", "video/mp4",     false, false, true,  true  },
    { "VIDEO/MP4",           "video/mp4",     false, false, true,  true  },
    { "audio/mp4",           "audio/mp4",     false, true,  false, false },
    { "application/x",       "application/x", true, false,  false, false }
  };

  for (const auto& test : tests) {
    Maybe<MediaMIMEType> type = MakeMediaMIMEType(test.mTypeString);
    EXPECT_TRUE(type.isSome())
      << "MakeMediaMIMEType(\"" << test.mTypeString << "\").isSome()";
    EXPECT_TRUE(type->AsString().EqualsASCII(test.mAsString))
      << "MakeMediaMIMEType(\"" << test.mTypeString << "\")->AsString() == \"" << test.mAsString << "\"";
    EXPECT_EQ(test.mApplication, type->HasApplicationMajorType())
      << "MakeMediaMIMEType(\"" << test.mTypeString << "\")->HasApplicationMajorType() == " << (test.mApplication ? "true" : "false");
    EXPECT_EQ(test.mAudio, type->HasAudioMajorType())
      << "MakeMediaMIMEType(\"" << test.mTypeString << "\")->HasAudioMajorType() == " << (test.mAudio ? "true" : "false");
    EXPECT_EQ(test.mVideo, type->HasVideoMajorType())
      << "MakeMediaMIMEType(\"" << test.mTypeString << "\")->HasVideoMajorType() == " << (test.mVideo ? "true" : "false");
    EXPECT_EQ(test.mEqualsLiteralVideoSlashMp4, *type == MEDIAMIMETYPE("video/mp4"))
      << "*MakeMediaMIMEType(\"" << test.mTypeString << "\") == MEDIAMIMETYPE(\"video/mp4\")";
  }
}

TEST(MediaMIMETypes, MediaCodecs)
{
  MediaCodecs empty("");
  EXPECT_TRUE(empty.IsEmpty());
  EXPECT_TRUE(empty.AsString().EqualsLiteral(""));
  EXPECT_FALSE(empty.Contains(NS_LITERAL_STRING("")));
  EXPECT_FALSE(empty.Contains(NS_LITERAL_STRING("c1")));
  int iterations = 0;
  for (const auto& codec : empty.Range()) {
    ++iterations;
    Unused << codec;
  }
  EXPECT_EQ(0, iterations);

  MediaCodecs space(" ");
  EXPECT_FALSE(space.IsEmpty());
  EXPECT_TRUE(space.AsString().EqualsLiteral(" "));
  EXPECT_TRUE(space.Contains(NS_LITERAL_STRING("")));
  EXPECT_FALSE(space.Contains(NS_LITERAL_STRING("c1")));
  iterations = 0;
  for (const auto& codec : space.Range()) {
    ++iterations;
    EXPECT_TRUE(codec.IsEmpty());
  }
  EXPECT_EQ(1, iterations);

  MediaCodecs one(" c1 ");
  EXPECT_FALSE(one.IsEmpty());
  EXPECT_TRUE(one.AsString().EqualsLiteral(" c1 "));
  EXPECT_FALSE(one.Contains(NS_LITERAL_STRING("")));
  EXPECT_TRUE(one.Contains(NS_LITERAL_STRING("c1")));
  iterations = 0;
  for (const auto& codec : one.Range()) {
    ++iterations;
    EXPECT_TRUE(codec.EqualsLiteral("c1"));
  }
  EXPECT_EQ(1, iterations);

  MediaCodecs two(" c1 , c2 ");
  EXPECT_FALSE(two.IsEmpty());
  EXPECT_TRUE(two.AsString().EqualsLiteral(" c1 , c2 "));
  EXPECT_FALSE(two.Contains(NS_LITERAL_STRING("")));
  EXPECT_TRUE(two.Contains(NS_LITERAL_STRING("c1")));
  EXPECT_TRUE(two.Contains(NS_LITERAL_STRING("c2")));
  iterations = 0;
  for (const auto& codec : two.Range()) {
    ++iterations;
    char buffer[] = "c0";
    buffer[1] += iterations;
    EXPECT_TRUE(codec.EqualsASCII(buffer));
  }
  EXPECT_EQ(2, iterations);

  EXPECT_TRUE(two.ContainsAll(two));
  EXPECT_TRUE(two.ContainsAll(one));
  EXPECT_FALSE(one.ContainsAll(two));
}

TEST(MediaMIMETypes, MakeMediaExtendedMIMEType_bad)
{
  static const char* tests[] =
  {
    "",
    " ",
    "/",
    "audio",
    "audio/",
    "mp4",
    "/mp4",
    "a/b"
  };

  for (const auto& test : tests) {
    Maybe<MediaExtendedMIMEType> type = MakeMediaExtendedMIMEType(test);
    EXPECT_TRUE(type.isNothing())
      << "MakeMediaExtendedMIMEType(\"" << test << "\").isNothing()";
  }
}

TEST(MediaMIMETypes, MediaExtendedMIMEType)
{
  // Some generic tests first.
  static const struct
  {
    const char* mTypeString;
    const char* mTypeAsString;
    bool mApplication;
    bool mAudio;
    bool mVideo;
    bool mEqualsLiteralVideoSlashMp4; // tests `== "video/mp4"`
    bool mHaveCodecs;
  } tests[] =
  { // in                    Type().AsString  app    audio  video ==v/mp4 codecs
    { "video/mp4",           "video/mp4",     false, false, true,  true,  false },
    { "video/mp4; codecs=0", "video/mp4",     false, false, true,  true,  true  },
    { "VIDEO/MP4",           "video/mp4",     false, false, true,  true,  false },
    { "audio/mp4",           "audio/mp4",     false, true,  false, false, false },
    { "video/webm",          "video/webm",    false, false, true,  false, false },
    { "audio/webm",          "audio/webm",    false, true,  false, false, false },
    { "application/x",       "application/x", true,  false, false, false, false }
  };

  for (const auto& test : tests) {
    Maybe<MediaExtendedMIMEType> type = MakeMediaExtendedMIMEType(test.mTypeString);
    EXPECT_TRUE(type.isSome())
      << "MakeMediaExtendedMIMEType(\"" << test.mTypeString << "\").isSome()";
    EXPECT_TRUE(type->OriginalString().EqualsASCII(test.mTypeString))
      << "MakeMediaExtendedMIMEType(\"" << test.mTypeString << "\")->AsString() == \"" << test.mTypeAsString << "\"";
    EXPECT_TRUE(type->Type().AsString().EqualsASCII(test.mTypeAsString))
      << "MakeMediaExtendedMIMEType(\"" << test.mTypeString << "\")->AsString() == \"" << test.mTypeAsString << "\"";
    EXPECT_EQ(test.mApplication, type->Type().HasApplicationMajorType())
      << "MakeMediaExtendedMIMEType(\"" << test.mTypeString << "\")->Type().HasApplicationMajorType() == " << (test.mApplication ? "true" : "false");
    EXPECT_EQ(test.mAudio, type->Type().HasAudioMajorType())
      << "MakeMediaExtendedMIMEType(\"" << test.mTypeString << "\")->Type().HasAudioMajorType() == " << (test.mAudio ? "true" : "false");
    EXPECT_EQ(test.mVideo, type->Type().HasVideoMajorType())
      << "MakeMediaExtendedMIMEType(\"" << test.mTypeString << "\")->Type().HasVideoMajorType() == " << (test.mVideo ? "true" : "false");
    EXPECT_EQ(test.mEqualsLiteralVideoSlashMp4, type->Type() == MEDIAMIMETYPE("video/mp4"))
      << "*MakeMediaExtendedMIMEType(\"" << test.mTypeString << "\")->Type() == MEDIAMIMETYPE(\"video/mp4\")";
    EXPECT_EQ(test.mHaveCodecs, type->HaveCodecs())
      << "MakeMediaExtendedMIMEType(\"" << test.mTypeString << "\")->HaveCodecs() == " << (test.mHaveCodecs ? "true" : "false");
    EXPECT_NE(test.mHaveCodecs, type->Codecs().IsEmpty())
      << "MakeMediaExtendedMIMEType(\"" << test.mTypeString << "\")->Codecs.IsEmpty() != " << (test.mHaveCodecs ? "true" : "false");
    EXPECT_FALSE(type->GetWidth())
      << "MakeMediaExtendedMIMEType(\"" << test.mTypeString << "\")->GetWidth()";
    EXPECT_FALSE(type->GetHeight())
      << "MakeMediaExtendedMIMEType(\"" << test.mTypeString << "\")->GetHeight()";
    EXPECT_FALSE(type->GetFramerate())
      << "MakeMediaExtendedMIMEType(\"" << test.mTypeString << "\")->GetFramerate()";
    EXPECT_FALSE(type->GetBitrate())
      << "MakeMediaExtendedMIMEType(\"" << test.mTypeString << "\")->GetBitrate()";
  }

  // Test all extra parameters.
  Maybe<MediaExtendedMIMEType> type =
    MakeMediaExtendedMIMEType(
      "video/mp4; codecs=\"a,b\"; width=1024; Height=768; FrameRate=60; BITRATE=100000");
  EXPECT_TRUE(type->HaveCodecs());
  EXPECT_FALSE(type->Codecs().IsEmpty());
  EXPECT_TRUE(type->Codecs().AsString().EqualsASCII("a,b"));
  EXPECT_TRUE(type->Codecs() == "a,b");
  EXPECT_TRUE(type->Codecs().Contains(NS_LITERAL_STRING("a")));
  EXPECT_TRUE(type->Codecs().Contains(NS_LITERAL_STRING("b")));
  EXPECT_TRUE(!!type->GetWidth());
  EXPECT_EQ(1024, *type->GetWidth());
  EXPECT_TRUE(!!type->GetHeight());
  EXPECT_EQ(768, *type->GetHeight());
  EXPECT_TRUE(!!type->GetFramerate());
  EXPECT_EQ(60, *type->GetFramerate());
  EXPECT_TRUE(!!type->GetBitrate());
  EXPECT_EQ(100000, *type->GetBitrate());
}
