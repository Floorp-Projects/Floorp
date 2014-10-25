/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(MOZ_WIDGET_GTK)
#include "gfxPlatformGtk.h"
#define gfxToolkitPlatform gfxPlatformGtk
#elif defined(MOZ_WIDGET_QT)
#include <qfontinfo.h>
#include "gfxQtPlatform.h"
#define gfxToolkitPlatform gfxQtPlatform
#elif defined(XP_WIN)
#include "gfxWindowsPlatform.h"
#define gfxToolkitPlatform gfxWindowsPlatform
#elif defined(ANDROID)
#include "gfxAndroidPlatform.h"
#define gfxToolkitPlatform gfxAndroidPlatform
#endif

#include "gfxTypes.h"
#include "gfxFT2Fonts.h"
#include "gfxFT2FontBase.h"
#include "gfxFT2Utils.h"
#include "gfxFT2FontList.h"
#include "gfxTextRun.h"
#include <locale.h>
#include "nsGkAtoms.h"
#include "nsTArray.h"
#include "nsUnicodeRange.h"
#include "nsCRT.h"
#include "nsXULAppAPI.h"

#include "prlog.h"
#include "prinit.h"

#include "mozilla/MemoryReporting.h"
#include "mozilla/Preferences.h"
#include "mozilla/gfx/2D.h"

/**
 * gfxFT2Font
 */

bool
gfxFT2Font::ShapeText(gfxContext     *aContext,
                      const char16_t *aText,
                      uint32_t        aOffset,
                      uint32_t        aLength,
                      int32_t         aScript,
                      bool            aVertical,
                      gfxShapedText  *aShapedText)
{
    if (!gfxFont::ShapeText(aContext, aText, aOffset, aLength, aScript,
                            aVertical, aShapedText)) {
        // harfbuzz must have failed(?!), just render raw glyphs
        AddRange(aText, aOffset, aLength, aShapedText);
        PostShapingFixup(aContext, aText, aOffset, aLength, aShapedText);
    }

    return true;
}

void
gfxFT2Font::AddRange(const char16_t *aText, uint32_t aOffset,
                     uint32_t aLength, gfxShapedText *aShapedText)
{
    const uint32_t appUnitsPerDevUnit = aShapedText->GetAppUnitsPerDevUnit();
    // we'll pass this in/figure it out dynamically, but at this point there can be only one face.
    gfxFT2LockedFace faceLock(this);
    FT_Face face = faceLock.get();

    gfxShapedText::CompressedGlyph *charGlyphs =
        aShapedText->GetCharacterGlyphs();

    const gfxFT2Font::CachedGlyphData *cgd = nullptr, *cgdNext = nullptr;

    FT_UInt spaceGlyph = GetSpaceGlyph();

    for (uint32_t i = 0; i < aLength; i++, aOffset++) {
        char16_t ch = aText[i];

        if (ch == 0) {
            // treat this null byte as a missing glyph, don't create a glyph for it
            aShapedText->SetMissingGlyph(aOffset, 0, this);
            continue;
        }

        NS_ASSERTION(!gfxFontGroup::IsInvalidChar(ch), "Invalid char detected");

        if (cgdNext) {
            cgd = cgdNext;
            cgdNext = nullptr;
        } else {
            cgd = GetGlyphDataForChar(ch);
        }

        FT_UInt gid = cgd->glyphIndex;
        int32_t advance = 0;

        if (gid == 0) {
            advance = -1; // trigger the missing glyphs case below
        } else {
            // find next character and its glyph -- in case they exist
            // and exist in the current font face -- to compute kerning
            char16_t chNext = 0;
            FT_UInt gidNext = 0;
            FT_Pos lsbDeltaNext = 0;

            if (FT_HAS_KERNING(face) && i + 1 < aLength) {
                chNext = aText[i + 1];
                if (chNext != 0) {
                    cgdNext = GetGlyphDataForChar(chNext);
                    gidNext = cgdNext->glyphIndex;
                    if (gidNext && gidNext != spaceGlyph)
                        lsbDeltaNext = cgdNext->lsbDelta;
                }
            }

            advance = cgd->xAdvance;

            // now add kerning to the current glyph's advance
            if (chNext && gidNext) {
                FT_Vector kerning; kerning.x = 0;
                FT_Get_Kerning(face, gid, gidNext, FT_KERNING_DEFAULT, &kerning);
                advance += kerning.x;
                if (cgd->rsbDelta - lsbDeltaNext >= 32) {
                    advance -= 64;
                } else if (cgd->rsbDelta - lsbDeltaNext < -32) {
                    advance += 64;
                }
            }

            // convert 26.6 fixed point to app units
            // round rather than truncate to nearest pixel
            // because these advances are often scaled
            advance = ((advance * appUnitsPerDevUnit + 32) >> 6);
        }

        if (advance >= 0 &&
            gfxShapedText::CompressedGlyph::IsSimpleAdvance(advance) &&
            gfxShapedText::CompressedGlyph::IsSimpleGlyphID(gid)) {
            charGlyphs[aOffset].SetSimpleGlyph(advance, gid);
        } else if (gid == 0) {
            // gid = 0 only happens when the glyph is missing from the font
            aShapedText->SetMissingGlyph(aOffset, ch, this);
        } else {
            gfxTextRun::DetailedGlyph details;
            details.mGlyphID = gid;
            NS_ASSERTION(details.mGlyphID == gid,
                         "Seriously weird glyph ID detected!");
            details.mAdvance = advance;
            details.mXOffset = 0;
            details.mYOffset = 0;
            gfxShapedText::CompressedGlyph g;
            g.SetComplex(charGlyphs[aOffset].IsClusterStart(), true, 1);
            aShapedText->SetGlyphs(aOffset, g, &details);
        }
    }
}

gfxFT2Font::gfxFT2Font(cairo_scaled_font_t *aCairoFont,
                       FT2FontEntry *aFontEntry,
                       const gfxFontStyle *aFontStyle,
                       bool aNeedsBold)
    : gfxFT2FontBase(aCairoFont, aFontEntry, aFontStyle)
    , mCharGlyphCache(32)
{
    NS_ASSERTION(mFontEntry, "Unable to find font entry for font.  Something is whack.");
    mApplySyntheticBold = aNeedsBold;
}

gfxFT2Font::~gfxFT2Font()
{
}

/**
 * Look up the font in the gfxFont cache. If we don't find it, create one.
 * In either case, add a ref, append it to the aFonts array, and return it ---
 * except for OOM in which case we do nothing and return null.
 */
already_AddRefed<gfxFT2Font>
gfxFT2Font::GetOrMakeFont(const nsAString& aName, const gfxFontStyle *aStyle,
                          bool aNeedsBold)
{
#ifdef ANDROID
    FT2FontEntry *fe = static_cast<FT2FontEntry*>
        (gfxPlatformFontList::PlatformFontList()->
            FindFontForFamily(aName, aStyle, aNeedsBold));
#else
    FT2FontEntry *fe = static_cast<FT2FontEntry*>
        (gfxToolkitPlatform::GetPlatform()->FindFontEntry(aName, *aStyle));
#endif
    if (!fe) {
        NS_WARNING("Failed to find font entry for font!");
        return nullptr;
    }

    nsRefPtr<gfxFT2Font> font = GetOrMakeFont(fe, aStyle, aNeedsBold);
    return font.forget();
}

already_AddRefed<gfxFT2Font>
gfxFT2Font::GetOrMakeFont(FT2FontEntry *aFontEntry, const gfxFontStyle *aStyle,
                          bool aNeedsBold)
{
    nsRefPtr<gfxFont> font = gfxFontCache::GetCache()->Lookup(aFontEntry, aStyle);
    if (!font) {
        cairo_scaled_font_t *scaledFont = aFontEntry->CreateScaledFont(aStyle);
        if (!scaledFont) {
            return nullptr;
        }
        font = new gfxFT2Font(scaledFont, aFontEntry, aStyle, aNeedsBold);
        cairo_scaled_font_destroy(scaledFont);
        if (!font) {
            return nullptr;
        }
        gfxFontCache::GetCache()->AddNew(font);
    }
    return font.forget().downcast<gfxFT2Font>();
}

void
gfxFT2Font::FillGlyphDataForChar(uint32_t ch, CachedGlyphData *gd)
{
    gfxFT2LockedFace faceLock(this);
    FT_Face face = faceLock.get();

    if (!face->charmap || face->charmap->encoding != FT_ENCODING_UNICODE) {
        FT_Select_Charmap(face, FT_ENCODING_UNICODE);
    }
    FT_UInt gid = FT_Get_Char_Index(face, ch);

    if (gid == 0) {
        // this font doesn't support this char!
        NS_ASSERTION(gid != 0, "We don't have a glyph, but font indicated that it supported this char in tables?");
        gd->glyphIndex = 0;
        return;
    }

    FT_Int32 flags = gfxPlatform::GetPlatform()->FontHintingEnabled() ?
                     FT_LOAD_DEFAULT :
                     (FT_LOAD_NO_AUTOHINT | FT_LOAD_NO_HINTING);
    FT_Error err = FT_Load_Glyph(face, gid, flags);

    if (err) {
        // hmm, this is weird, we failed to load a glyph that we had?
        NS_WARNING("Failed to load glyph that we got from Get_Char_index");

        gd->glyphIndex = 0;
        return;
    }

    gd->glyphIndex = gid;
    gd->lsbDelta = face->glyph->lsb_delta;
    gd->rsbDelta = face->glyph->rsb_delta;
    gd->xAdvance = face->glyph->advance.x;
}

void
gfxFT2Font::AddSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                   FontCacheSizes* aSizes) const
{
    gfxFont::AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
    aSizes->mFontInstances +=
        mCharGlyphCache.SizeOfExcludingThis(nullptr, aMallocSizeOf);
}

void
gfxFT2Font::AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                   FontCacheSizes* aSizes) const
{
    aSizes->mFontInstances += aMallocSizeOf(this);
    AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
}

#ifdef USE_SKIA
mozilla::TemporaryRef<mozilla::gfx::GlyphRenderingOptions>
gfxFT2Font::GetGlyphRenderingOptions(const TextRunDrawParams* aRunParams)
{
  mozilla::gfx::FontHinting hinting;

  if (gfxPlatform::GetPlatform()->FontHintingEnabled()) {
    hinting = mozilla::gfx::FontHinting::NORMAL;
  } else {
    hinting = mozilla::gfx::FontHinting::NONE;
  }

  // We don't want to force the use of the autohinter over the font's built in hints
  return mozilla::gfx::Factory::CreateCairoGlyphRenderingOptions(hinting, false);
}
#endif

