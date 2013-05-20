/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsString.h"
#include "nsDependentString.h"

#include "prinrval.h"

#include "gfxContext.h"
#include "gfxFont.h"
#include "gfxPlatform.h"

#include "gfxFontTest.h"

#if defined(XP_MACOSX)
#include "gfxTestCocoaHelper.h"
#endif

#ifdef MOZ_WIDGET_GTK
#include "gtk/gtk.h"
#endif

using namespace mozilla;

struct TestEntry {
  const char* mFamilies;
  const char* mString;
};

TestEntry testList[] = {
#include "per-word-runs.h"
{ nullptr, nullptr } // terminator
};

already_AddRefed<gfxContext>
MakeContext ()
{
    const int size = 200;

    nsRefPtr<gfxASurface> surface;

    surface = gfxPlatform::GetPlatform()->
        CreateOffscreenSurface(gfxIntSize(size, size),
                               gfxASurface::ContentFromFormat(gfxASurface::ImageFormatRGB24));
    gfxContext *ctx = new gfxContext(surface);
    NS_IF_ADDREF(ctx);
    return ctx;
}

nsRefPtr<gfxFontGroup> fontGroup;
const char* lastFamilies = nullptr;

void
RunTest (TestEntry *test, gfxContext *ctx) {
    if (!lastFamilies || strcmp(lastFamilies, test->mFamilies)) {
        gfxFontStyle style_western_normal_16 (FONT_STYLE_NORMAL,
                                              NS_FONT_STRETCH_NORMAL,
                                              400,
                                              16.0,
                                              NS_NewPermanentAtom(NS_LITERAL_STRING("en")),
                                              0.0,
                                              false, false, false,
                                              NS_LITERAL_STRING(""));

        fontGroup = gfxPlatform::GetPlatform()->CreateFontGroup(NS_ConvertUTF8toUTF16(test->mFamilies), &style_western_normal_16, nullptr);
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
}

uint32_t iterations = 20;

int
main (int argc, char **argv) {
#ifdef MOZ_WIDGET_GTK
    gtk_init(&argc, &argv); 
#endif
#ifdef XP_MACOSX
    CocoaPoolInit();
#endif

    // Initialize XPCOM
    nsresult rv = NS_InitXPCOM2(nullptr, nullptr, nullptr);
    if (NS_FAILED(rv))
        return -1; 

    if (!gfxPlatform::GetPlatform())
        return -1;

    // let's get all the xpcom goop out of the system
    fflush (stderr);
    fflush (stdout);

    nsRefPtr<gfxContext> context = MakeContext();

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

    fflush (stderr);
    fflush (stdout);
}
