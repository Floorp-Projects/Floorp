/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_GRAPHITESHAPER_H
#define GFX_GRAPHITESHAPER_H

#include "gfxFont.h"

struct gr_face;
struct gr_font;
struct gr_segment;

class gfxGraphiteShaper : public gfxFontShaper {
public:
    explicit gfxGraphiteShaper(gfxFont *aFont);
    virtual ~gfxGraphiteShaper();

    virtual bool ShapeText(gfxContext      *aContext,
                           const char16_t *aText,
                           uint32_t         aOffset,
                           uint32_t         aLength,
                           int32_t          aScript,
                           gfxShapedText   *aShapedText);

    static void Shutdown();

protected:
    nsresult SetGlyphsFromSegment(gfxContext      *aContext,
                                  gfxShapedText   *aShapedText,
                                  uint32_t         aOffset,
                                  uint32_t         aLength,
                                  const char16_t *aText,
                                  gr_segment      *aSegment);

    static float GrGetAdvance(const void* appFontHandle, uint16_t glyphid);

    gr_face *mGrFace; // owned by the font entry; shaper must call
                      // gfxFontEntry::ReleaseGrFace when finished with it
    gr_font *mGrFont; // owned by the shaper itself

    struct CallbackData {
        gfxFont           *mFont;
        gfxGraphiteShaper *mShaper;
        gfxContext        *mContext;
    };

    CallbackData mCallbackData;
    bool mFallbackToSmallCaps; // special fallback for the petite-caps case

    // Convert HTML 'lang' (BCP47) to Graphite language code
    static uint32_t GetGraphiteTagForLang(const nsCString& aLang);
    static nsTHashtable<nsUint32HashKey> *sLanguageTags;
};

#endif /* GFX_GRAPHITESHAPER_H */
