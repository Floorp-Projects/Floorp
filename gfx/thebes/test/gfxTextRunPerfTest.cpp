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
 * The Initial Developer of the Original Code is Mozilla Corporation.
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

#include "prinrval.h"

#include "nsServiceManagerUtils.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

#include "gfxContext.h"
#include "gfxFont.h"
#include "gfxPlatform.h"

#include "gfxFontTest.h"

#if defined(XP_WIN)
#include "gfxWindowsFonts.h"
#elif defined(MOZ_ENABLE_PANGO)
#include "gfxPangoFonts.h"
#elif defined(XP_MACOSX)
#include "gfxAtsuiFonts.h"
#include "gfxTestCocoaHelper.h"
#endif

#ifdef MOZ_WIDGET_GTK2
#include "gtk/gtk.h"
#endif

struct TestEntry {
  const char* mFamilies;
  const char* mString;
};

TestEntry testList[] = {
#include "per-word-runs.h"
{ nsnull, nsnull } // terminator
};

already_AddRefed<gfxContext>
MakeContext ()
{
    const int size = 200;

    nsRefPtr<gfxASurface> surface;

    surface = gfxPlatform::GetPlatform()->CreateOffscreenSurface(gfxIntSize(size, size), gfxASurface::ImageFormatRGB24);
    gfxContext *ctx = new gfxContext(surface);
    NS_IF_ADDREF(ctx);
    return ctx;
}

nsRefPtr<gfxFontGroup> fontGroup;
const char* lastFamilies = nsnull;

void
RunTest (TestEntry *test, gfxContext *ctx) {
    if (!lastFamilies || strcmp(lastFamilies, test->mFamilies)) {
        gfxFontStyle style_western_normal_16 (FONT_STYLE_NORMAL,
                                              FONT_VARIANT_NORMAL,
                                              400,
                                              FONT_DECORATION_NONE,
                                              16.0,
                                              nsDependentCString("x-western"),
                                              0.0,
                                              PR_FALSE, PR_FALSE);

#if defined(XP_WIN)
        fontGroup = new gfxWindowsFontGroup(NS_ConvertUTF8toUTF16(test->mFamilies), &style_western_normal_16);
#elif defined(MOZ_ENABLE_PANGO)
        fontGroup = new gfxPangoFontGroup(NS_ConvertUTF8toUTF16(test->mFamilies), &style_western_normal_16);
#elif defined(XP_MACOSX)
        fontGroup = new gfxAtsuiFontGroup(NS_ConvertUTF8toUTF16(test->mFamilies), &style_western_normal_16);
#endif
    }

    nsAutoPtr<gfxTextRun> textRun;
    PRUint32 i;
    PRBool isASCII = PR_TRUE;
    for (i = 0; test->mString[i]; ++i) {
        if (test->mString[i] >= 0x80) {
            isASCII = PR_FALSE;
        }
    }
    gfxTextRunFactory::Parameters params = {
      ctx, nsnull, nsnull, nsnull, nsnull, 0, 60, 0
    };
    PRUint32 length;
    if (isASCII) {
        params.mFlags |= gfxTextRunFactory::TEXT_IS_ASCII |
                         gfxTextRunFactory::TEXT_IS_8BIT;
        length = strlen(test->mString);
        textRun = fontGroup->MakeTextRun(NS_REINTERPRET_CAST(const PRUint8*, test->mString), length, &params);
    } else {
        params.mFlags |= gfxTextRunFactory::TEXT_HAS_SURROGATES; // just in case
        NS_ConvertUTF8toUTF16 str(nsDependentCString(test->mString));
        length = str.Length();
        textRun = fontGroup->MakeTextRun(str.get(), length, &params);
    }

    // Should we test drawing?
    // textRun->Draw(ctx, gfxPoint(0,0), 0, length, nsnull, nsnull, nsnull);
    
    textRun->GetAdvanceWidth(0, length, nsnull);
}

PRUint32 iterations = 20;

int
main (int argc, char **argv) {
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

    // let's get all the xpcom goop out of the system
    fflush (stderr);
    fflush (stdout);

    nsRefPtr<gfxContext> context = MakeContext();

    // Start timing
    PRIntervalTime start = PR_IntervalNow();

    for (PRUint32 i = 0; i < iterations; ++i) {
        for (uint test = 0;
             test < NS_ARRAY_LENGTH(testList) - 1;
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
