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
