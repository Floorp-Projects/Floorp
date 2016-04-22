/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CORETEXTSHAPER_H
#define GFX_CORETEXTSHAPER_H

#include "gfxFont.h"

#include <ApplicationServices/ApplicationServices.h>

class gfxMacFont;

class gfxCoreTextShaper : public gfxFontShaper {
public:
    explicit gfxCoreTextShaper(gfxMacFont *aFont);

    virtual ~gfxCoreTextShaper();

    virtual bool ShapeText(DrawTarget      *aDrawTarget,
                           const char16_t *aText,
                           uint32_t         aOffset,
                           uint32_t         aLength,
                           Script           aScript,
                           bool             aVertical,
                           gfxShapedText   *aShapedText);

    // clean up static objects that may have been cached
    static void Shutdown();

protected:
    CTFontRef mCTFont;

    // attributes for shaping text with LTR or RTL directionality
    CFDictionaryRef mAttributesDictLTR;
    CFDictionaryRef mAttributesDictRTL;

    nsresult SetGlyphsFromRun(gfxShapedText *aShapedText,
                              uint32_t       aOffset,
                              uint32_t       aLength,
                              CTRunRef       aCTRun,
                              int32_t        aStringOffset);

    CTFontRef CreateCTFontWithFeatures(CGFloat aSize,
                                       CTFontDescriptorRef aDescriptor);

    CFDictionaryRef CreateAttrDict(bool aRightToLeft);
    CFDictionaryRef CreateAttrDictWithoutDirection();

    static CTFontDescriptorRef
    CreateFontFeaturesDescriptor(const std::pair<SInt16,SInt16> aFeatures[],
                                 size_t aCount);

    static CTFontDescriptorRef GetDefaultFeaturesDescriptor();
    static CTFontDescriptorRef GetDisableLigaturesDescriptor();
    static CTFontDescriptorRef GetIndicFeaturesDescriptor();
    static CTFontDescriptorRef GetIndicDisableLigaturesDescriptor();

    // cached font descriptor, created the first time it's needed
    static CTFontDescriptorRef    sDefaultFeaturesDescriptor;

    // cached descriptor for adding disable-ligatures setting to a font
    static CTFontDescriptorRef    sDisableLigaturesDescriptor;

    // feature descriptors for buggy Indic AAT font workaround
    static CTFontDescriptorRef    sIndicFeaturesDescriptor;
    static CTFontDescriptorRef    sIndicDisableLigaturesDescriptor;
};

#endif /* GFX_CORETEXTSHAPER_H */
