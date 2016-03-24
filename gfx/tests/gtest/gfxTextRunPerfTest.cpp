/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"

#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsString.h"
#include "nsDependentString.h"

#include "prinrval.h"

#include "gfxContext.h"
#include "gfxFont.h"
#include "gfxPlatform.h"

#include "gfxFontTest.h"

using namespace mozilla;
using namespace mozilla::gfx;

struct TestEntry {
  const char* mFamilies;
  const char* mString;
};

TestEntry testList[] = {
#include "per-word-runs.h"
{ nullptr, nullptr } // terminator
};

static already_AddRefed<gfxContext>
MakeContext ()
{
    const int size = 200;

    RefPtr<DrawTarget> drawTarget = gfxPlatform::GetPlatform()->
        CreateOffscreenContentDrawTarget(IntSize(size, size),
                                         SurfaceFormat::B8G8R8X8);
    RefPtr<gfxContext> ctx = new gfxContext(drawTarget);

    return ctx.forget();
}

const char* lastFamilies = nullptr;

static void
RunTest (TestEntry *test, gfxContext *ctx) {
    RefPtr<gfxFontGroup> fontGroup;
    if (!lastFamilies || strcmp(lastFamilies, test->mFamilies)) {
        gfxFontStyle style_western_normal_16 (mozilla::gfx::FontStyle::NORMAL,
                                              400,
                                              0,
                                              16.0,
                                              NS_NewAtom(NS_LITERAL_STRING("en")),
                                              0.0,
                                              false, false,
                                              NS_LITERAL_STRING(""));

        fontGroup = gfxPlatform::GetPlatform()->CreateFontGroup(NS_ConvertUTF8toUTF16(test->mFamilies), &style_western_normal_16, nullptr, nullptr, 1.0);
    }

    nsAutoPtr<gfxTextRun> textRun;
    uint32_t i;
    bool isASCII = true;
    for (i = 0; test->mString[i]; ++i) {
        if (test->mString[i] & 0x80) {
            isASCII = false;
        }
    }
    gfxTextRunFactory::Parameters params = {
      ctx, nullptr, nullptr, nullptr, 0, 60
    };
    uint32_t flags = gfxTextRunFactory::TEXT_IS_PERSISTENT;
    uint32_t length;
    gfxFontTestStore::NewStore();
    if (isASCII) {
        flags |= gfxTextRunFactory::TEXT_IS_ASCII |
                 gfxTextRunFactory::TEXT_IS_8BIT;
        length = strlen(test->mString);
        textRun = fontGroup->MakeTextRun(reinterpret_cast<const uint8_t*>(test->mString), length, &params, flags);
    } else {
        NS_ConvertUTF8toUTF16 str(nsDependentCString(test->mString));
        length = str.Length();
        textRun = fontGroup->MakeTextRun(str.get(), length, &params, flags);
    }

    // Should we test drawing?
    // textRun->Draw(ctx, gfxPoint(0,0), 0, length, nullptr, nullptr, nullptr);

    textRun->GetAdvanceWidth(0, length, nullptr);
    gfxFontTestStore::DeleteStore();
}

uint32_t iterations = 1;

TEST(Gfx, TextRunPref) {
    RefPtr<gfxContext> context = MakeContext();

    // Start timing
    PRIntervalTime start = PR_IntervalNow();

    for (uint32_t i = 0; i < iterations; ++i) {
        for (uint test = 0;
             test < ArrayLength(testList) - 1;
             test++)
        {
            RunTest(&testList[test], context);
        }
    }

    PRIntervalTime end = PR_IntervalNow();

    printf("Elapsed time (ms): %d\n", PR_IntervalToMilliseconds(end - start));

}
