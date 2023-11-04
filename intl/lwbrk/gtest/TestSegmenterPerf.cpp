/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <fstream>

#include "gtest/gtest.h"
#include "gtest/MozGTestBench.h"  // For MOZ_GTEST_BENCH
#include "mozilla/intl/LineBreaker.h"
#include "mozilla/intl/Segmenter.h"
#include "mozilla/Preferences.h"
#include "nsAtom.h"
#include "nsLineBreaker.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla::intl {

using mozilla::intl::LineBreakRule;
using mozilla::intl::WordBreakRule;

constexpr size_t kIterations = 100;

static std::string ReadFileIntoString(const char* aPath) {
  std::ifstream file(aPath);
  std::stringstream sstr;
  sstr << file.rdbuf();
  return sstr.str();
}

class SegmenterPerf : public ::testing::Test {
 protected:
  void SetUp() override {
    // Test files are into xpcom/tests/gtest/wikipedia
    mArUtf8 = ReadFileIntoString("ar.txt");
    mDeUtf8 = ReadFileIntoString("de.txt");
    mJaUtf8 = ReadFileIntoString("ja.txt");
    mRuUtf8 = ReadFileIntoString("ru.txt");
    mThUtf8 = ReadFileIntoString("th.txt");
    mTrUtf8 = ReadFileIntoString("tr.txt");
    mViUtf8 = ReadFileIntoString("vi.txt");

    CopyUTF8toUTF16(mArUtf8, mArUtf16);
    CopyUTF8toUTF16(mDeUtf8, mDeUtf16);
    CopyUTF8toUTF16(mJaUtf8, mJaUtf16);
    CopyUTF8toUTF16(mRuUtf8, mRuUtf16);
    CopyUTF8toUTF16(mThUtf8, mThUtf16);
    CopyUTF8toUTF16(mTrUtf8, mTrUtf16);
    CopyUTF8toUTF16(mViUtf8, mViUtf16);

    mAr = NS_Atomize(u"ar");
    mDe = NS_Atomize(u"de");
    mJa = NS_Atomize(u"ja");
    mRu = NS_Atomize(u"ru");
    mTh = NS_Atomize(u"th");
    mTr = NS_Atomize(u"tr");
    mVi = NS_Atomize(u"vi");
  }

 public:
  std::string mArUtf8;
  std::string mDeUtf8;
  std::string mJaUtf8;
  std::string mRuUtf8;
  std::string mThUtf8;
  std::string mTrUtf8;
  std::string mViUtf8;

  nsString mArUtf16;
  nsString mDeUtf16;
  nsString mJaUtf16;
  nsString mRuUtf16;
  nsString mThUtf16;
  nsString mTrUtf16;
  nsString mViUtf16;

  RefPtr<nsAtom> mAr;
  RefPtr<nsAtom> mDe;
  RefPtr<nsAtom> mJa;
  RefPtr<nsAtom> mRu;
  RefPtr<nsAtom> mTh;
  RefPtr<nsAtom> mTr;
  RefPtr<nsAtom> mVi;
};

class AutoSetSegmenter final {
 public:
  explicit AutoSetSegmenter(bool aValue) {
    nsresult rv =
        mozilla::Preferences::SetBool("intl.icu4x.segmenter.enabled", aValue);
    EXPECT_TRUE(rv == NS_OK);
  }

  ~AutoSetSegmenter() {
    mozilla::Preferences::ClearUser("intl.icu4x.segmenter.enabled");
  }
};

static void TestSegmenterBench(const nsString& aStr, bool aIsJaOrZh,
                               size_t aCount = kIterations) {
  nsTArray<uint8_t> breakState;
  breakState.SetLength(aStr.Length());

  for (size_t i = 0; i < aCount; i++) {
    LineBreaker::ComputeBreakPositions(
        aStr.get(), aStr.Length(), WordBreakRule::Normal, LineBreakRule::Strict,
        aIsJaOrZh, breakState.Elements());
  }
}

MOZ_GTEST_BENCH_F(SegmenterPerf, PerfLineBreakAROld, [this] {
  AutoSetSegmenter set(false);
  TestSegmenterBench(mArUtf16, false);
});

MOZ_GTEST_BENCH_F(SegmenterPerf, PerfLineBreakDEOld, [this] {
  AutoSetSegmenter set(false);
  TestSegmenterBench(mDeUtf16, false);
});

MOZ_GTEST_BENCH_F(SegmenterPerf, PerfLineBreakJAOld, [this] {
  AutoSetSegmenter set(false);
  TestSegmenterBench(mJaUtf16, true);
});

MOZ_GTEST_BENCH_F(SegmenterPerf, PerfLineBreakRUOld, [this] {
  AutoSetSegmenter set(false);
  TestSegmenterBench(mRuUtf16, false);
});

MOZ_GTEST_BENCH_F(SegmenterPerf, PerfLineBreakTHOld, [this] {
  AutoSetSegmenter set(false);
  TestSegmenterBench(mThUtf16, false);
});

MOZ_GTEST_BENCH_F(SegmenterPerf, PerfLineBreakTROld, [this] {
  AutoSetSegmenter set(false);
  TestSegmenterBench(mTrUtf16, false);
});

MOZ_GTEST_BENCH_F(SegmenterPerf, PerfLineBreakVIOld, [this] {
  AutoSetSegmenter set(false);
  TestSegmenterBench(mViUtf16, false);
});

MOZ_GTEST_BENCH_F(SegmenterPerf, PerfLineBreakAR, [this] {
  AutoSetSegmenter set(false);
  TestSegmenterBench(mArUtf16, false);
});

MOZ_GTEST_BENCH_F(SegmenterPerf, PerfLineBreakDE, [this] {
  AutoSetSegmenter set(true);
  TestSegmenterBench(mDeUtf16, false);
});

MOZ_GTEST_BENCH_F(SegmenterPerf, PerfLineBreakJA, [this] {
  AutoSetSegmenter set(true);
  TestSegmenterBench(mJaUtf16, true);
});

MOZ_GTEST_BENCH_F(SegmenterPerf, PerfLineBreakRU, [this] {
  AutoSetSegmenter set(true);
  TestSegmenterBench(mRuUtf16, false);
});

MOZ_GTEST_BENCH_F(SegmenterPerf, PerfLineBreakTH, [this] {
  AutoSetSegmenter set(true);
  // LSTM segmenter is too slow
  TestSegmenterBench(mThUtf16, false, 3);
});

MOZ_GTEST_BENCH_F(SegmenterPerf, PerfLineBreakTR, [this] {
  AutoSetSegmenter set(true);
  TestSegmenterBench(mTrUtf16, false);
});

MOZ_GTEST_BENCH_F(SegmenterPerf, PerfLineBreakVI, [this] {
  AutoSetSegmenter set(true);
  TestSegmenterBench(mViUtf16, false);
});

class LBSink final : public nsILineBreakSink {
 public:
  LBSink() = default;
  ~LBSink() = default;

  virtual void SetBreaks(uint32_t, uint32_t, uint8_t*) override {}
  virtual void SetCapitalization(uint32_t, uint32_t, bool*) override {}
};

static void TestDOMSegmenterBench(const nsString& aStr, nsAtom* aLang,
                                  size_t aCount = kIterations) {
  LBSink sink;
  bool trailingBreak;

  for (size_t i = 0; i < aCount; i++) {
    nsLineBreaker breaker;
    breaker.AppendText(aLang, aStr.get(), aStr.Length(), 0, &sink);
    breaker.Reset(&trailingBreak);
  }
}

MOZ_GTEST_BENCH_F(SegmenterPerf, PerfDOMLineBreakAROld, [this] {
  AutoSetSegmenter set(false);
  TestDOMSegmenterBench(mArUtf16, mAr);
});

MOZ_GTEST_BENCH_F(SegmenterPerf, PerfDOMLineBreakDEOld, [this] {
  AutoSetSegmenter set(false);
  TestDOMSegmenterBench(mDeUtf16, mDe);
});

MOZ_GTEST_BENCH_F(SegmenterPerf, PerfDOMLineBreakJAOld, [this] {
  AutoSetSegmenter set(false);
  TestDOMSegmenterBench(mJaUtf16, mJa);
});

MOZ_GTEST_BENCH_F(SegmenterPerf, PerfDOMLineBreakRUOld, [this] {
  AutoSetSegmenter set(false);
  TestDOMSegmenterBench(mRuUtf16, mRu);
});

MOZ_GTEST_BENCH_F(SegmenterPerf, PerfDOMLineBreakTHOld, [this] {
  AutoSetSegmenter set(false);
  TestDOMSegmenterBench(mThUtf16, mTh);
});

MOZ_GTEST_BENCH_F(SegmenterPerf, PerfDOMLineBreakTROld, [this] {
  AutoSetSegmenter set(false);
  TestDOMSegmenterBench(mTrUtf16, mTr);
});

MOZ_GTEST_BENCH_F(SegmenterPerf, PerfDOMLineBreakVIOld, [this] {
  AutoSetSegmenter set(false);
  TestDOMSegmenterBench(mViUtf16, mVi);
});

MOZ_GTEST_BENCH_F(SegmenterPerf, PerfDOMLineBreakAR, [this] {
  AutoSetSegmenter set(true);
  TestDOMSegmenterBench(mArUtf16, mAr);
});

MOZ_GTEST_BENCH_F(SegmenterPerf, PerfDOMLineBreakDE, [this] {
  AutoSetSegmenter set(true);
  TestDOMSegmenterBench(mDeUtf16, mDe);
});

MOZ_GTEST_BENCH_F(SegmenterPerf, PerfDOMLineBreakJA, [this] {
  AutoSetSegmenter set(true);
  TestDOMSegmenterBench(mJaUtf16, mJa);
});

MOZ_GTEST_BENCH_F(SegmenterPerf, PerfDOMLineBreakRU, [this] {
  AutoSetSegmenter set(true);
  TestDOMSegmenterBench(mRuUtf16, mRu);
});

MOZ_GTEST_BENCH_F(SegmenterPerf, PerfDOMLineBreakTH, [this] {
  AutoSetSegmenter set(true);
  // LSTM segmenter is too slow
  TestDOMSegmenterBench(mThUtf16, mTh, 3);
});

MOZ_GTEST_BENCH_F(SegmenterPerf, PerfDOMLineBreakTR, [this] {
  AutoSetSegmenter set(true);
  TestDOMSegmenterBench(mTrUtf16, mTr);
});

MOZ_GTEST_BENCH_F(SegmenterPerf, PerfDOMLineBreakVI, [this] {
  AutoSetSegmenter set(true);
  TestDOMSegmenterBench(mViUtf16, mVi);
});

}  // namespace mozilla::intl
