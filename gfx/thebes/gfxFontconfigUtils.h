/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FONTCONFIG_UTILS_H
#define GFX_FONTCONFIG_UTILS_H

#include "gfxPlatform.h"

#include "nsAutoRef.h"
#include "gfxFT2FontBase.h"

#include <fontconfig/fontconfig.h>


template <>
class nsAutoRefTraits<FcPattern> : public nsPointerRefTraits<FcPattern>
{
public:
    static void Release(FcPattern *ptr) { FcPatternDestroy(ptr); }
    static void AddRef(FcPattern *ptr) { FcPatternReference(ptr); }
};

template <>
class nsAutoRefTraits<FcFontSet> : public nsPointerRefTraits<FcFontSet>
{
public:
    static void Release(FcFontSet *ptr) { FcFontSetDestroy(ptr); }
};

template <>
class nsAutoRefTraits<FcCharSet> : public nsPointerRefTraits<FcCharSet>
{
public:
    static void Release(FcCharSet *ptr) { FcCharSetDestroy(ptr); }
};

class gfxFontconfigFontBase : public gfxFT2FontBase {
public:
    gfxFontconfigFontBase(cairo_scaled_font_t *aScaledFont,
                          FcPattern *aPattern,
                          gfxFontEntry *aFontEntry,
                          const gfxFontStyle *aFontStyle)
      : gfxFT2FontBase(aScaledFont, aFontEntry, aFontStyle)
      , mPattern(aPattern) { }

    virtual FontType GetType() const override { return FONT_TYPE_FONTCONFIG; }
    virtual FcPattern *GetPattern() const { return mPattern; }

private:
    nsCountedRef<FcPattern> mPattern;
};

#endif /* GFX_FONTCONFIG_UTILS_H */
