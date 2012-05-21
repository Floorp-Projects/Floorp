/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FONT_TEST_H
#define GFX_FONT_TEST_H

#include "nsString.h"
#include "nsTArray.h"

#include "cairo.h"

#include "gfxFont.h"
#include "gfxUserFontSet.h"

struct THEBES_API gfxFontTestItem {
    gfxFontTestItem(const nsCString& fontName,
                    cairo_glyph_t *cglyphs, int nglyphs)
        : platformFont(fontName)
    {
        glyphs = new cairo_glyph_t[nglyphs];
        memcpy (glyphs, cglyphs, sizeof(cairo_glyph_t) * nglyphs);
        num_glyphs = nglyphs;
    }

    gfxFontTestItem(const gfxFontTestItem& other) {
        platformFont = other.platformFont;
        num_glyphs = other.num_glyphs;
        glyphs = new cairo_glyph_t[num_glyphs];
        memcpy (glyphs, other.glyphs, sizeof(cairo_glyph_t) * num_glyphs);
    }

    ~gfxFontTestItem() {
        delete [] glyphs;
    }

    nsCString platformFont;
    cairo_glyph_t *glyphs;
    int num_glyphs;
};


class THEBES_API gfxFontTestStore {
public:
    gfxFontTestStore() { }

    void AddItem (const nsCString& fontString,
                  cairo_glyph_t *cglyphs, int nglyphs)
    {
        items.AppendElement(gfxFontTestItem(fontString, cglyphs, nglyphs));
    }

    void AddItem (const nsString& fontString,
                  cairo_glyph_t *cglyphs, int nglyphs)
    {
        items.AppendElement(gfxFontTestItem(NS_ConvertUTF16toUTF8(fontString), cglyphs, nglyphs));
    }

    nsTArray<gfxFontTestItem> items;

public:
    static gfxFontTestStore *CurrentStore() {
        return sCurrentStore;
    }

    static gfxFontTestStore *NewStore() {
        if (sCurrentStore)
            delete sCurrentStore;

        sCurrentStore = new gfxFontTestStore;
        return sCurrentStore;
    }

    static void DeleteStore() {
        if (sCurrentStore)
            delete sCurrentStore;

        sCurrentStore = nsnull;
    }

protected:
    static gfxFontTestStore *sCurrentStore;
};


#endif /* GFX_FONT_TEST_H */
