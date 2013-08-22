/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsString.h"
#include "nsDependentString.h"

#include "mozilla/Preferences.h"

#include "gfxContext.h"
#include "gfxFont.h"
#include "gfxPlatform.h"

#include "gfxFontTest.h"

using namespace mozilla;

enum {
    S_UTF8 = 0,
    S_ASCII = 1
};

class FrameTextRunCache;

struct LiteralArray {
    LiteralArray (unsigned long l1) {
        data.AppendElement(l1);
    }
    LiteralArray (unsigned long l1, unsigned long l2) {
        data.AppendElement(l1);
        data.AppendElement(l2);
    }
    LiteralArray (unsigned long l1, unsigned long l2, unsigned long l3) {
        data.AppendElement(l1);
        data.AppendElement(l2);
        data.AppendElement(l3);
    }
    LiteralArray (unsigned long l1, unsigned long l2, unsigned long l3, unsigned long l4) {
        data.AppendElement(l1);
        data.AppendElement(l2);
        data.AppendElement(l3);
        data.AppendElement(l4);
    }
    LiteralArray (unsigned long l1, unsigned long l2, unsigned long l3, unsigned long l4, unsigned long l5) {
        data.AppendElement(l1);
        data.AppendElement(l2);
        data.AppendElement(l3);
        data.AppendElement(l4);
        data.AppendElement(l5);
    }

    LiteralArray (const LiteralArray& other) {
        data = other.data;
    }

    nsTArray<unsigned long> data;
};

#define GLYPHS LiteralArray

struct TestEntry {
    TestEntry (const char *aUTF8FamilyString,
               const gfxFontStyle& aFontStyle,
               const char *aString)
        : utf8FamilyString(aUTF8FamilyString),
          fontStyle(aFontStyle),
          stringType(S_ASCII),
          string(aString),
          isRTL(false)
    {
    }

    TestEntry (const char *aUTF8FamilyString,
               const gfxFontStyle& aFontStyle,
               int stringType,
               const char *aString)
        : utf8FamilyString(aUTF8FamilyString),
          fontStyle(aFontStyle),
          stringType(stringType),
          string(aString),
          isRTL(false)
    {
    }

    struct ExpectItem {
        ExpectItem(const nsCString& aFontName,
                   const LiteralArray& aGlyphs)
            : fontName(aFontName), glyphs(aGlyphs)
        { }

        bool Compare(const nsCString& aFontName,
                       cairo_glyph_t *aGlyphs,
                       int num_glyphs)
        {
            // bit that allowed for empty fontname to match all is commented
            // out
            if (/*!fontName.IsEmpty() &&*/ !fontName.Equals(aFontName))
                return false;

            if (num_glyphs != int(glyphs.data.Length()))
                return false;

            for (int j = 0; j < num_glyphs; j++) {
                if (glyphs.data[j] != aGlyphs[j].index)
                return false;
            }

            return true;
        }

        nsCString fontName;
        LiteralArray glyphs;
    };

    void SetRTL()
    {
        isRTL = true;
    }

    // empty/NULL fontName means ignore font name
    void Expect (const char *platform,
                 const char *fontName,
                 const LiteralArray& glyphs)
    {
        if (fontName)
            Expect (platform, nsDependentCString(fontName), glyphs);
        else
            Expect (platform, nsCString(), glyphs);
    }

    void Expect (const char *platform,
                 const nsCString& fontName,
                 const LiteralArray& glyphs)
    {
#if defined(XP_WIN)
        if (strcmp(platform, "win32"))
            return;
#elif defined(XP_MACOSX)
        if (strcmp(platform, "macosx"))
            return;
#elif defined(XP_UNIX)
        if (strcmp(platform, "gtk2-pango"))
            return;
#else
        return;
#endif

        expectItems.AppendElement(ExpectItem(fontName, glyphs));
    }

    bool Check (gfxFontTestStore *store) {
        if (expectItems.Length() == 0 ||
            store->items.Length() != expectItems.Length())
        {
            return false;
        }

        for (uint32_t i = 0; i < expectItems.Length(); i++) {
            if (!expectItems[i].Compare(store->items[i].platformFont,
                                        store->items[i].glyphs,
                                        store->items[i].num_glyphs))
                return false;
        }

        return true;
    }

    const char *utf8FamilyString;
    gfxFontStyle fontStyle;

    int stringType;
    const char *string;
    bool isRTL;

    nsTArray<ExpectItem> expectItems;
};

static already_AddRefed<gfxContext>
MakeContext ()
{
    const int size = 200;

    nsRefPtr<gfxASurface> surface;

    surface = gfxPlatform::GetPlatform()->
        CreateOffscreenSurface(gfxIntSize(size, size),
                               gfxASurface::ContentFromFormat(gfxASurface::ImageFormatRGB24));
    nsRefPtr<gfxContext> ctx = new gfxContext(surface);
    return ctx.forget();
}

TestEntry*
AddTest (nsTArray<TestEntry>& testList,
         const char *utf8FamilyString,
         const gfxFontStyle& fontStyle,
         int stringType,
         const char *string)
{
    TestEntry te (utf8FamilyString,
                  fontStyle,
                  stringType,
                  string);

    testList.AppendElement(te);

    return &(testList[testList.Length()-1]);
}

void
DumpStore (gfxFontTestStore *store) {
    if (store->items.Length() == 0) {
        printf ("(empty)\n");
    }

    for (uint32_t i = 0;
         i < store->items.Length();
         i++)
    {
        printf ("Run[% 2d]: '%s' ", i, store->items[i].platformFont.BeginReading());

        for (int j = 0; j < store->items[i].num_glyphs; j++)
            printf ("%d ", int(store->items[i].glyphs[j].index));

        printf ("\n");
    }
}

void
DumpTestExpect (TestEntry *test) {
    for (uint32_t i = 0; i < test->expectItems.Length(); i++) {
        printf ("Run[% 2d]: '%s' ", i, test->expectItems[i].fontName.BeginReading());
        for (uint32_t j = 0; j < test->expectItems[i].glyphs.data.Length(); j++)
            printf ("%d ", int(test->expectItems[i].glyphs.data[j]));

        printf ("\n");
    }
}

void SetupTests(nsTArray<TestEntry>& testList);

static bool
RunTest (TestEntry *test, gfxContext *ctx) {
    nsRefPtr<gfxFontGroup> fontGroup;

    fontGroup = gfxPlatform::GetPlatform()->CreateFontGroup(NS_ConvertUTF8toUTF16(test->utf8FamilyString), &test->fontStyle, nullptr);

    nsAutoPtr<gfxTextRun> textRun;
    gfxTextRunFactory::Parameters params = {
      ctx, nullptr, nullptr, nullptr, 0, 60
    };
    uint32_t flags = gfxTextRunFactory::TEXT_IS_PERSISTENT;
    if (test->isRTL) {
        flags |= gfxTextRunFactory::TEXT_IS_RTL;
    }
    uint32_t length;
    if (test->stringType == S_ASCII) {
        flags |= gfxTextRunFactory::TEXT_IS_ASCII | gfxTextRunFactory::TEXT_IS_8BIT;
        length = strlen(test->string);
        textRun = fontGroup->MakeTextRun(reinterpret_cast<const uint8_t*>(test->string), length, &params, flags);
    } else {
        NS_ConvertUTF8toUTF16 str(nsDependentCString(test->string));
        length = str.Length();
        textRun = fontGroup->MakeTextRun(str.get(), length, &params, flags);
    }

    gfxFontTestStore::NewStore();
    textRun->Draw(ctx, gfxPoint(0,0), gfxFont::GLYPH_FILL, 0, length, nullptr, nullptr, nullptr);
    gfxFontTestStore *s = gfxFontTestStore::CurrentStore();

    if (!test->Check(s)) {
        DumpStore(s);
        printf ("  expected:\n");
        DumpTestExpect(test);
        gfxFontTestStore::DeleteStore();
        return false;
    }

    gfxFontTestStore::DeleteStore();

    return true;
}

TEST(Gfx, FontSelection) {
    int passed = 0;
    int failed = 0;

    // set up the tests
    nsTArray<TestEntry> testList;
    SetupTests(testList);

    nsRefPtr<gfxContext> context = MakeContext();

    for (uint32_t test = 0;
         test < testList.Length();
         test++)
    {
        bool result = RunTest (&testList[test], context);
        if (result) {
            passed++;
        } else {
            printf ("Test %d failed\n", test);
            failed++;
        }
    }
}

// The tests themselves

#include "gfxFontSelectionTests.h"
