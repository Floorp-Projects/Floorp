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
 * The Original Code is Mozilla Graphite integration code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonathan Kew <jfkthame@gmail.com>
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
