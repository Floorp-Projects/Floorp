/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CORETEXTSHAPER_H
#define GFX_CORETEXTSHAPER_H

#include "gfxTypes.h"
#include "gfxFont.h"
#include "gfxFontUtils.h"
#include "gfxPlatform.h"
#include "gfxMacPlatformFontList.h"

#include <Carbon/Carbon.h>

class gfxMacFont;

class gfxCoreTextShaper : public gfxFontShaper {
public:
    gfxCoreTextShaper(gfxMacFont *aFont);

    virtual ~gfxCoreTextShaper();

    virtual bool ShapeText(gfxContext      *aContext,
                           const PRUnichar *aText,
                           uint32_t         aOffset,
                           uint32_t         aLength,
                           int32_t          aScript,
                           gfxShapedText   *aShapedText);

    // clean up static objects that may have been cached
    static void Shutdown();

protected:
    CTFontRef mCTFont;
    CFDictionaryRef mAttributesDict;

    nsresult SetGlyphsFromRun(gfxShapedText *aShapedText,
                              uint32_t       aOffset,
                              uint32_t       aLength,
                              CTRunRef       aCTRun,
                              int32_t        aStringOffset);

    CTFontRef CreateCTFontWithDisabledLigatures(CGFloat aSize);

    static void CreateDefaultFeaturesDescriptor();

    static CTFontDescriptorRef GetDefaultFeaturesDescriptor() {
        if (sDefaultFeaturesDescriptor == nullptr) {
            CreateDefaultFeaturesDescriptor();
        }
        return sDefaultFeaturesDescriptor;
    }

    // cached font descriptor, created the first time it's needed
    static CTFontDescriptorRef    sDefaultFeaturesDescriptor;

    // cached descriptor for adding disable-ligatures setting to a font
    static CTFontDescriptorRef    sDisableLigaturesDescriptor;
};

#endif /* GFX_CORETEXTSHAPER_H */
