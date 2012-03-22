/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsString.h"
#include "nsDependentString.h"

#include "mozilla/Preferences.h"

#include "gfxContext.h"
#include "gfxFont.h"
#include "gfxPlatform.h"

#include "gfxFontTest.h"

#if defined(XP_MACOSX)
#include "gfxTestCocoaHelper.h"
#endif

#ifdef MOZ_WIDGET_GTK2
#include "gtk/gtk.h"
#endif

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

        for (PRUint32 i = 0; i < expectItems.Length(); i++) {
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

nsTArray<TestEntry> testList;

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

TestEntry*
AddTest (const char *utf8FamilyString,
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

void SetupTests();

void
DumpStore (gfxFontTestStore *store) {
    if (store->items.Length() == 0) {
        printf ("(empty)\n");
    }

    for (PRUint32 i = 0;
         i < store->items.Length();
         i++)
    {
        printf ("Run[% 2d]: '%s' ", i, nsPromiseFlatCString(store->items[i].platformFont).get());

        for (int j = 0; j < store->items[i].num_glyphs; j++)
            printf ("%d ", int(store->items[i].glyphs[j].index));

        printf ("\n");
    }
}

void
DumpTestExpect (TestEntry *test) {
    for (PRUint32 i = 0; i < test->expectItems.Length(); i++) {
        printf ("Run[% 2d]: '%s' ", i, nsPromiseFlatCString(test->expectItems[i].fontName).get());
        for (PRUint32 j = 0; j < test->expectItems[i].glyphs.data.Length(); j++)
            printf ("%d ", int(test->expectItems[i].glyphs.data[j]));

        printf ("\n");
    }
}

bool
RunTest (TestEntry *test, gfxContext *ctx) {
    nsRefPtr<gfxFontGroup> fontGroup;

    fontGroup = gfxPlatform::GetPlatform()->CreateFontGroup(NS_ConvertUTF8toUTF16(test->utf8FamilyString), &test->fontStyle, nsnull);

    nsAutoPtr<gfxTextRun> textRun;
    gfxTextRunFactory::Parameters params = {
      ctx, nsnull, nsnull, nsnull, 0, 60
    };
    PRUint32 flags = gfxTextRunFactory::TEXT_IS_PERSISTENT;
    if (test->isRTL) {
        flags |= gfxTextRunFactory::TEXT_IS_RTL;
    }
    PRUint32 length;
    if (test->stringType == S_ASCII) {
        flags |= gfxTextRunFactory::TEXT_IS_ASCII | gfxTextRunFactory::TEXT_IS_8BIT;
        length = strlen(test->string);
        textRun = fontGroup->MakeTextRun(reinterpret_cast<const PRUint8*>(test->string), length, &params, flags);
    } else {
        NS_ConvertUTF8toUTF16 str(nsDependentCString(test->string));
        length = str.Length();
        textRun = fontGroup->MakeTextRun(str.get(), length, &params, flags);
    }

    gfxFontTestStore::NewStore();
    textRun->Draw(ctx, gfxPoint(0,0), 0, length, nsnull, nsnull);
    gfxFontTestStore *s = gfxFontTestStore::CurrentStore();

    gTextRunCache->RemoveTextRun(textRun);

    if (!test->Check(s)) {
        DumpStore(s);
        printf ("  expected:\n");
        DumpTestExpect(test);
        return false;
    }

    return true;
}

int
main (int argc, char **argv) {
    int passed = 0;
    int failed = 0;

#ifdef MOZ_WIDGET_GTK2
    gtk_init(&argc, &argv); 
#endif
#ifdef XP_MACOSX
    CocoaPoolInit();
#endif

    // Initialize XPCOM
    nsresult rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
    if (NS_FAILED(rv))
        return -1;

    if (!gfxPlatform::GetPlatform())
        return -1;

    // let's get all the xpcom goop out of the system
    fflush (stderr);
    fflush (stdout);

    // don't need to query, we might need to set up some prefs later
    if (0) {
        nsresult rv;

        nsAdoptingCString str = Preferences::GetCString("font.name.sans-serif.x-western");
        printf ("sans-serif.x-western: %s\n", nsPromiseFlatCString(str).get());
    }

    // set up the tests
    SetupTests();

    nsRefPtr<gfxContext> context = MakeContext();

    for (uint test = 0;
         test < testList.Length();
         test++)
    {
        printf ("==== Test %d\n", test);
        bool result = RunTest (&testList[test], context);
        if (result) {
            printf ("Test %d succeeded\n", test);
            passed++;
        } else {
            printf ("Test %d failed\n", test);
            failed++;
        }
    }

    printf ("PASSED: %d FAILED: %d\n", passed, failed);
    fflush (stderr);
    fflush (stdout);
}

// The tests themselves

#include "gfxFontSelectionTests.h"
