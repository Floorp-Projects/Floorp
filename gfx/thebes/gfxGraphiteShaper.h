/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_GRAPHITESHAPER_H
#define GFX_GRAPHITESHAPER_H

#include "gfxTypes.h"
#include "gfxFont.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"

struct gr_face;
struct gr_font;
struct gr_segment;

class gfxGraphiteShaper : public gfxFontShaper {
public:
    gfxGraphiteShaper(gfxFont *aFont);
    virtual ~gfxGraphiteShaper();

    virtual bool ShapeWord(gfxContext *aContext,
                           gfxShapedWord *aShapedWord,
                           const PRUnichar *aText);

    const void* GetTable(PRUint32 aTag, size_t *aLength);

    static void Shutdown();

    struct CallbackData {
        gfxFont           *mFont;
        gfxGraphiteShaper *mShaper;
        gfxContext        *mContext;
    };

    struct TableRec {
        hb_blob_t  *mBlob;
        const void *mData;
        PRUint32    mLength;
    };

protected:
    nsresult SetGlyphsFromSegment(gfxShapedWord *aShapedWord,
                                  gr_segment *aSegment);

    gr_face *mGrFace;
    gr_font *mGrFont;

    CallbackData mCallbackData;

    nsDataHashtable<nsUint32HashKey,TableRec> mTables;

    // Whether the font implements GetGlyphWidth, or we should read tables
    // directly to get ideal widths
    bool mUseFontGlyphWidths;

    // Convert HTML 'lang' (BCP47) to Graphite language code
    static PRUint32 GetGraphiteTagForLang(const nsCString& aLang);
    static nsTHashtable<nsUint32HashKey> sLanguageTags;
};

#endif /* GFX_GRAPHITESHAPER_H */
