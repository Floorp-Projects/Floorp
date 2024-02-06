/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"

#include "mozilla/intl/Bidi.h"
#include "mozilla/Span.h"
namespace mozilla::intl {

struct VisualRun {
  Span<const char16_t> string;
  BidiDirection direction;
};

/**
 * An iterator for visual runs in a paragraph. See Bug 1736597 for integrating
 * this into the public API.
 */
class MOZ_STACK_CLASS VisualRunIter {
 public:
  VisualRunIter(Bidi& aBidi, Span<const char16_t> aParagraph,
                BidiEmbeddingLevel aLevel)
      : mBidi(aBidi), mParagraph(aParagraph) {
    // Crash in case of errors by calling unwrap. If this were a real API, this
    // would be a TryCreate call.
    mBidi.SetParagraph(aParagraph, aLevel).unwrap();
    mRunCount = mBidi.CountRuns().unwrap();
  }

  Maybe<VisualRun> Next() {
    if (mRunIndex >= mRunCount) {
      return Nothing();
    }

    int32_t stringIndex = -1;
    int32_t stringLength = -1;

    BidiDirection direction =
        mBidi.GetVisualRun(mRunIndex, &stringIndex, &stringLength);

    Span<const char16_t> string(mParagraph.Elements() + stringIndex,
                                stringLength);
    mRunIndex++;
    return Some(VisualRun{string, direction});
  }

 private:
  Bidi& mBidi;
  Span<const char16_t> mParagraph = Span<const char16_t>();
  int32_t mRunIndex = 0;
  int32_t mRunCount = 0;
};

struct LogicalRun {
  Span<const char16_t> string;
  BidiEmbeddingLevel embeddingLevel;
};

/**
 * An iterator for logical runs in a paragraph. See Bug 1736597 for integrating
 * this into the public API.
 */
class MOZ_STACK_CLASS LogicalRunIter {
 public:
  LogicalRunIter(Bidi& aBidi, Span<const char16_t> aParagraph,
                 BidiEmbeddingLevel aLevel)
      : mBidi(aBidi), mParagraph(aParagraph) {
    // Crash in case of errors by calling unwrap. If this were a real API, this
    // would be a TryCreate call.
    mBidi.SetParagraph(aParagraph, aLevel).unwrap();
    mBidi.CountRuns().unwrap();
  }

  Maybe<LogicalRun> Next() {
    if (mRunIndex >= static_cast<int32_t>(mParagraph.Length())) {
      return Nothing();
    }

    int32_t logicalLimit;

    BidiEmbeddingLevel embeddingLevel;
    mBidi.GetLogicalRun(mRunIndex, &logicalLimit, &embeddingLevel);

    Span<const char16_t> string(mParagraph.Elements() + mRunIndex,
                                logicalLimit - mRunIndex);

    mRunIndex = logicalLimit;
    return Some(LogicalRun{string, embeddingLevel});
  }

 private:
  Bidi& mBidi;
  Span<const char16_t> mParagraph = Span<const char16_t>();
  int32_t mRunIndex = 0;
};

TEST(IntlBidi, SimpleLTR)
{
  Bidi bidi{};
  LogicalRunIter logicalRunIter(bidi, MakeStringSpan(u"this is a paragraph"),
                                BidiEmbeddingLevel::DefaultLTR());
  ASSERT_EQ(bidi.GetParagraphEmbeddingLevel(), 0);
  ASSERT_EQ(bidi.GetParagraphDirection(), Bidi::ParagraphDirection::LTR);

  {
    auto logicalRun = logicalRunIter.Next();
    ASSERT_TRUE(logicalRun.isSome());
    ASSERT_EQ(logicalRun->string, MakeStringSpan(u"this is a paragraph"));
    ASSERT_EQ(logicalRun->embeddingLevel, 0);
    ASSERT_EQ(logicalRun->embeddingLevel.Direction(), BidiDirection::LTR);
  }

  {
    auto logicalRun = logicalRunIter.Next();
    ASSERT_TRUE(logicalRun.isNothing());
  }
}

TEST(IntlBidi, SimpleRTL)
{
  Bidi bidi{};
  LogicalRunIter logicalRunIter(bidi, MakeStringSpan(u"فايرفوكس رائع"),
                                BidiEmbeddingLevel::DefaultLTR());
  ASSERT_EQ(bidi.GetParagraphEmbeddingLevel(), 1);
  ASSERT_EQ(bidi.GetParagraphDirection(), Bidi::ParagraphDirection::RTL);

  {
    auto logicalRun = logicalRunIter.Next();
    ASSERT_TRUE(logicalRun.isSome());
    ASSERT_EQ(logicalRun->string, MakeStringSpan(u"فايرفوكس رائع"));
    ASSERT_EQ(logicalRun->embeddingLevel.Direction(), BidiDirection::RTL);
    ASSERT_EQ(logicalRun->embeddingLevel, 1);
  }

  {
    auto logicalRun = logicalRunIter.Next();
    ASSERT_TRUE(logicalRun.isNothing());
  }
}

TEST(IntlBidi, MultiLevel)
{
  Bidi bidi{};
  LogicalRunIter logicalRunIter(
      bidi, MakeStringSpan(u"Firefox is awesome: رائع Firefox"),
      BidiEmbeddingLevel::DefaultLTR());
  ASSERT_EQ(bidi.GetParagraphEmbeddingLevel(), 0);
  ASSERT_EQ(bidi.GetParagraphDirection(), Bidi::ParagraphDirection::Mixed);

  {
    auto logicalRun = logicalRunIter.Next();
    ASSERT_TRUE(logicalRun.isSome());
    ASSERT_EQ(logicalRun->string, MakeStringSpan(u"Firefox is awesome: "));
    ASSERT_EQ(logicalRun->embeddingLevel, 0);
  }
  {
    auto logicalRun = logicalRunIter.Next();
    ASSERT_TRUE(logicalRun.isSome());
    ASSERT_EQ(logicalRun->string, MakeStringSpan(u"رائع"));
    ASSERT_EQ(logicalRun->embeddingLevel, 1);
  }
  {
    auto logicalRun = logicalRunIter.Next();
    ASSERT_TRUE(logicalRun.isSome());
    ASSERT_EQ(logicalRun->string, MakeStringSpan(u" Firefox"));
    ASSERT_EQ(logicalRun->embeddingLevel, 0);
  }
  {
    auto logicalRun = logicalRunIter.Next();
    ASSERT_TRUE(logicalRun.isNothing());
  }
}

TEST(IntlBidi, RtlOverride)
{
  Bidi bidi{};
  // Set the paragraph using the RTL embedding mark U+202B, and the LTR
  // embedding mark U+202A to increase the embedding level. This mark switches
  // the weakly directional character "_". This demonstrates that embedding
  // levels can be computed.
  LogicalRunIter logicalRunIter(
      bidi, MakeStringSpan(u"ltr\u202b___رائع___\u202a___ltr__"),
      BidiEmbeddingLevel::DefaultLTR());
  ASSERT_EQ(bidi.GetParagraphEmbeddingLevel(), 0);
  ASSERT_EQ(bidi.GetParagraphDirection(), Bidi::ParagraphDirection::Mixed);

  // Note that the Unicode Bidi Algorithm explicitly does NOT require any
  // specific placement or levels for the embedding controls (see
  // rule https://www.unicode.org/reports/tr9/#X9).
  // Further, the implementation notes at
  // https://www.unicode.org/reports/tr9/#Retaining_Explicit_Formatting_Characters
  // advise to "Resolve any LRE, RLE, LRO, RLO, PDF, or BN to the level of the
  // preceding character if there is one...", which means the embedding marks
  // here will each become part of the *preceding* run. This is how the Rust
  // unicode-bidi implementation behaves.
  // However, ICU4C behavior is such that they take on the level of the *next*
  // character, and become part of the following run.
  // For now, we accept either result here.
  {
    auto logicalRun = logicalRunIter.Next();
    ASSERT_TRUE(logicalRun.isSome());
    ASSERT_TRUE(logicalRun->string == MakeStringSpan(u"ltr") ||
                logicalRun->string == MakeStringSpan(u"ltr\u202b"));
    ASSERT_EQ(logicalRun->embeddingLevel, 0);
    ASSERT_EQ(logicalRun->embeddingLevel.Direction(), BidiDirection::LTR);
  }
  {
    auto logicalRun = logicalRunIter.Next();
    ASSERT_TRUE(logicalRun.isSome());
    ASSERT_TRUE(logicalRun->string == MakeStringSpan(u"\u202b___رائع___") ||
                logicalRun->string == MakeStringSpan(u"___رائع___\u202a"));
    ASSERT_EQ(logicalRun->embeddingLevel, 1);
    ASSERT_EQ(logicalRun->embeddingLevel.Direction(), BidiDirection::RTL);
  }
  {
    auto logicalRun = logicalRunIter.Next();
    ASSERT_TRUE(logicalRun.isSome());
    ASSERT_TRUE(logicalRun->string == MakeStringSpan(u"\u202a___ltr__") ||
                logicalRun->string == MakeStringSpan(u"___ltr__"));
    ASSERT_EQ(logicalRun->embeddingLevel, 2);
    ASSERT_EQ(logicalRun->embeddingLevel.Direction(), BidiDirection::LTR);
  }
  {
    auto logicalRun = logicalRunIter.Next();
    ASSERT_TRUE(logicalRun.isNothing());
  }
}

TEST(IntlBidi, VisualRuns)
{
  Bidi bidi{};

  VisualRunIter visualRunIter(
      bidi,
      MakeStringSpan(
          u"first visual run التشغيل البصري الثاني third visual run"),
      BidiEmbeddingLevel::DefaultLTR());
  {
    Maybe<VisualRun> run = visualRunIter.Next();
    ASSERT_TRUE(run.isSome());
    ASSERT_EQ(run->string, MakeStringSpan(u"first visual run "));
    ASSERT_EQ(run->direction, BidiDirection::LTR);
  }
  {
    Maybe<VisualRun> run = visualRunIter.Next();
    ASSERT_TRUE(run.isSome());
    ASSERT_EQ(run->string, MakeStringSpan(u"التشغيل البصري الثاني"));
    ASSERT_EQ(run->direction, BidiDirection::RTL);
  }
  {
    Maybe<VisualRun> run = visualRunIter.Next();
    ASSERT_TRUE(run.isSome());
    ASSERT_EQ(run->string, MakeStringSpan(u" third visual run"));
    ASSERT_EQ(run->direction, BidiDirection::LTR);
  }
  {
    Maybe<VisualRun> run = visualRunIter.Next();
    ASSERT_TRUE(run.isNothing());
  }
}

TEST(IntlBidi, VisualRunsWithEmbeds)
{
  // Compare this test to the logical order test.
  Bidi bidi{};
  VisualRunIter visualRunIter(
      bidi, MakeStringSpan(u"ltr\u202b___رائع___\u202a___ltr___"),
      BidiEmbeddingLevel::DefaultLTR());
  {
    Maybe<VisualRun> run = visualRunIter.Next();
    ASSERT_TRUE(run.isSome());
    ASSERT_TRUE(run->string == MakeStringSpan(u"ltr") ||
                run->string == MakeStringSpan(u"ltr\u202b"));
    ASSERT_EQ(run->direction, BidiDirection::LTR);
  }
  {
    Maybe<VisualRun> run = visualRunIter.Next();
    ASSERT_TRUE(run.isSome());
    ASSERT_TRUE(run->string == MakeStringSpan(u"\u202a___ltr___") ||
                run->string == MakeStringSpan(u"___ltr___"));
    ASSERT_EQ(run->direction, BidiDirection::LTR);
  }
  {
    Maybe<VisualRun> run = visualRunIter.Next();
    ASSERT_TRUE(run.isSome());
    ASSERT_TRUE(run->string == MakeStringSpan(u"\u202b___رائع___") ||
                run->string == MakeStringSpan(u"___رائع___\u202a"));
    ASSERT_EQ(run->direction, BidiDirection::RTL);
  }
  {
    Maybe<VisualRun> run = visualRunIter.Next();
    ASSERT_TRUE(run.isNothing());
  }
}

// The full Bidi class can be found in [1].
//
// [1]: https://www.unicode.org/Public/UNIDATA/extracted/DerivedBidiClass.txt
TEST(IntlBidi, GetBaseDirection)
{
  // Return Neutral as default if empty string is provided.
  ASSERT_EQ(Bidi::GetBaseDirection(nullptr), Bidi::BaseDirection::Neutral);

  // White space(WS) is classified as Neutral.
  ASSERT_EQ(Bidi::GetBaseDirection(MakeStringSpan(u" ")),
            Bidi::BaseDirection::Neutral);

  // 000A and 000D are paragraph separators(BS), which are also classified as
  // Neutral.
  ASSERT_EQ(Bidi::GetBaseDirection(MakeStringSpan(u"\u000A")),
            Bidi::BaseDirection::Neutral);
  ASSERT_EQ(Bidi::GetBaseDirection(MakeStringSpan(u"\u000D")),
            Bidi::BaseDirection::Neutral);

  // 0620..063f are Arabic letters, which is of type AL.
  ASSERT_EQ(Bidi::GetBaseDirection(MakeStringSpan(u"\u0620\u0621\u0622")),
            Bidi::BaseDirection::RTL);
  ASSERT_EQ(Bidi::GetBaseDirection(MakeStringSpan(u" \u0620\u0621\u0622")),
            Bidi::BaseDirection::RTL);
  ASSERT_EQ(Bidi::GetBaseDirection(MakeStringSpan(u"\u0620\u0621\u0622ABC")),
            Bidi::BaseDirection::RTL);

  // First strong character is of English letters.
  ASSERT_EQ(Bidi::GetBaseDirection(MakeStringSpan(u"ABC")),
            Bidi::BaseDirection::LTR);
  ASSERT_EQ(Bidi::GetBaseDirection(MakeStringSpan(u" ABC")),
            Bidi::BaseDirection::LTR);
  ASSERT_EQ(Bidi::GetBaseDirection(MakeStringSpan(u"ABC\u0620")),
            Bidi::BaseDirection::LTR);
}

}  // namespace mozilla::intl
