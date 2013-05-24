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
#include <locale.h>
#include "gfxHarfBuzzShaper.h"
#include "gfxGraphiteShaper.h"
#include "nsGkAtoms.h"
#include "nsTArray.h"
#include "nsUnicodeRange.h"
#include "nsCRT.h"
#include "nsXULAppAPI.h"

#include "prlog.h"
#include "prinit.h"

#include "mozilla/Preferences.h"

// rounding and truncation functions for a Freetype floating point number
// (FT26Dot6) stored in a 32bit integer with high 26 bits for the integer
// part and low 6 bits for the fractional part.
#define MOZ_FT_ROUND(x) (((x) + 32) & ~63) // 63 = 2^6 - 1
#define MOZ_FT_TRUNC(x) ((x) >> 6)
#define CONVERT_DESIGN_UNITS_TO_PIXELS(v, s) \
        MOZ_FT_TRUNC(MOZ_FT_ROUND(FT_MulFix((v) , (s))))

#ifndef ANDROID // not needed on Android, we use the generic gfxFontGroup
/**
 * gfxFT2FontGroup
 */

static PRLogModuleInfo *
GetFontLog()
{
    static PRLogModuleInfo *sLog;
    if (!sLog)
        sLog = PR_NewLogModule("ft2fonts");
    return sLog;
}

bool
gfxFT2FontGroup::FontCallback(const nsAString& fontName,
                              const nsACString& genericName,
                              bool aUseFontSet,
                              void *closure)
{
    nsTArray<nsString> *sa = static_cast<nsTArray<nsString>*>(closure);

    if (!fontName.IsEmpty() && !sa->Contains(fontName)) {
        sa->AppendElement(fontName);
#ifdef DEBUG_pavlov
        printf(" - %s\n", NS_ConvertUTF16toUTF8(fontName).get());
#endif
    }

    return true;
}

gfxFT2FontGroup::gfxFT2FontGroup(const nsAString& families,
                                 const gfxFontStyle *aStyle,
                                 gfxUserFontSet *aUserFontSet)
    : gfxFontGroup(families, aStyle, aUserFontSet)
{
#ifdef DEBUG_pavlov
    printf("Looking for %s\n", NS_ConvertUTF16toUTF8(families).get());
#endif
    nsTArray<nsString> familyArray;
    ForEachFont(FontCallback, &familyArray);

    if (familyArray.Length() == 0) {
        nsAutoString prefFamilies;
        gfxToolkitPlatform::GetPlatform()->GetPrefFonts(aStyle->language, prefFamilies, nullptr);
        if (!prefFamilies.IsEmpty()) {
            ForEachFont(prefFamilies, aStyle->language, FontCallback, &familyArray);
        }
    }
    if (familyArray.Length() == 0) {
#if defined(MOZ_WIDGET_QT) /* FIXME DFB */
        printf("failde to find a font. sadface\n");
        // We want to get rid of this entirely at some point, but first we need real lists of fonts.
        QFont defaultFont;
        QFontInfo fi (defaultFont);
        familyArray.AppendElement(nsDependentString(static_cast<const PRUnichar *>(fi.family().utf16())));
#elif defined(MOZ_WIDGET_GTK)
        FcResult result;
        FcChar8 *family = nullptr;
        FcPattern* pat = FcPatternCreate();
        FcPattern *match = FcFontMatch(nullptr, pat, &result);
        if (match)
            FcPatternGetString(match, FC_FAMILY, 0, &family);
        if (family)
            familyArray.AppendElement(NS_ConvertUTF8toUTF16((char*)family));
#elif defined(XP_WIN)
        HGDIOBJ hGDI = ::GetStockObject(SYSTEM_FONT);
        LOGFONTW logFont;
        if (hGDI && ::GetObjectW(hGDI, sizeof(logFont), &logFont))
            familyArray.AppendElement(nsDependentString(logFont.lfFaceName));
#elif defined(ANDROID)
        familyArray.AppendElement(NS_LITERAL_STRING("Droid Sans"));
        familyArray.AppendElement(NS_LITERAL_STRING("Roboto"));
#else
#error "Platform not supported"
#endif
    }

    for (uint32_t i = 0; i < familyArray.Length(); i++) {
        nsRefPtr<gfxFT2Font> font = gfxFT2Font::GetOrMakeFont(familyArray[i], &mStyle);
        if (font) {
            mFonts.AppendElement(font);
        }
    }
    NS_ASSERTION(mFonts.Length() > 0, "We need at least one font in a fontgroup");
}

gfxFT2FontGroup::~gfxFT2FontGroup()
{
}

gfxFontGroup *
gfxFT2FontGroup::Copy(const gfxFontStyle *aStyle)
{
    return new gfxFT2FontGroup(mFamilies, aStyle, nullptr);
}

// Helper function to return the leading UTF-8 character in a char pointer
// as 32bit number. Also sets the length of the current character (i.e. the
// offset to the next one) in the second argument
uint32_t getUTF8CharAndNext(const uint8_t *aString, uint8_t *aLength)
{
    *aLength = 1;
    if (aString[0] < 0x80) { // normal 7bit ASCII char
        return aString[0];
    }
    if ((aString[0] >> 5) == 6) { // two leading ones -> two bytes
        *aLength = 2;
        return ((aString[0] & 0x1F) << 6) + (aString[1] & 0x3F);
    }
    if ((aString[0] >> 4) == 14) { // three leading ones -> three bytes
        *aLength = 3;
        return ((aString[0] & 0x0F) << 12) + ((aString[1] & 0x3F) << 6) +
               (aString[2] & 0x3F);
    }
    if ((aString[0] >> 4) == 15) { // four leading ones -> four bytes
        *aLength = 4;
        return ((aString[0] & 0x07) << 18) + ((aString[1] & 0x3F) << 12) +
               ((aString[2] & 0x3F) <<  6) + (aString[3] & 0x3F);
    }
    return aString[0];
}


static bool
AddFontNameToArray(const nsAString& aName,
                   const nsACString& aGenericName,
                   bool aUseFontSet,
                   void *aClosure)
{
    if (!aName.IsEmpty()) {
        nsTArray<nsString> *list = static_cast<nsTArray<nsString> *>(aClosure);

        if (list->IndexOf(aName) == list->NoIndex)
            list->AppendElement(aName);
    }

    return true;
}

void
gfxFT2FontGroup::FamilyListToArrayList(const nsString& aFamilies,
                                       nsIAtom *aLangGroup,
                                       nsTArray<nsRefPtr<gfxFontEntry> > *aFontEntryList)
{
    nsAutoTArray<nsString, 15> fonts;
    ForEachFont(aFamilies, aLangGroup, AddFontNameToArray, &fonts);

    uint32_t len = fonts.Length();
    for (uint32_t i = 0; i < len; ++i) {
        const nsString& str = fonts[i];
        nsRefPtr<gfxFontEntry> fe = (gfxToolkitPlatform::GetPlatform()->FindFontEntry(str, mStyle));
        aFontEntryList->AppendElement(fe);
    }
}

void gfxFT2FontGroup::GetPrefFonts(nsIAtom *aLangGroup, nsTArray<nsRefPtr<gfxFontEntry> >& aFontEntryList)
{
    NS_ASSERTION(aLangGroup, "aLangGroup is null");
    gfxToolkitPlatform *platform = gfxToolkitPlatform::GetPlatform();
    nsAutoTArray<nsRefPtr<gfxFontEntry>, 5> fonts;
    nsAutoCString key;
    aLangGroup->ToUTF8String(key);
    key.Append("-");
    key.AppendInt(GetStyle()->style);
    key.Append("-");
    key.AppendInt(GetStyle()->weight);
    if (!platform->GetPrefFontEntries(key, &fonts)) {
        nsString fontString;
        platform->GetPrefFonts(aLangGroup, fontString);
        if (fontString.IsEmpty())
            return;

        FamilyListToArrayList(fontString, aLangGroup, &fonts);

        platform->SetPrefFontEntries(key, fonts);
    }
    aFontEntryList.AppendElements(fonts);
}

static int32_t GetCJKLangGroupIndex(const char *aLangGroup) {
    int32_t i;
    for (i = 0; i < COUNT_OF_CJK_LANG_GROUP; i++) {
        if (!PL_strcasecmp(aLangGroup, sCJKLangGroup[i]))
            return i;
    }
    return -1;
}

// this function assigns to the array passed in.
void gfxFT2FontGroup::GetCJKPrefFonts(nsTArray<nsRefPtr<gfxFontEntry> >& aFontEntryList) {
    gfxToolkitPlatform *platform = gfxToolkitPlatform::GetPlatform();

    nsAutoCString key("x-internal-cjk-");
    key.AppendInt(mStyle.style);
    key.Append("-");
    key.AppendInt(mStyle.weight);

    if (!platform->GetPrefFontEntries(key, &aFontEntryList)) {
        NS_ENSURE_TRUE_VOID(Preferences::GetRootBranch());
        // Add the CJK pref fonts from accept languages, the order should be same order
        nsAdoptingCString list = Preferences::GetLocalizedCString("intl.accept_languages");
        if (!list.IsEmpty()) {
            const char kComma = ',';
            const char *p, *p_end;
            list.BeginReading(p);
            list.EndReading(p_end);
            while (p < p_end) {
                while (nsCRT::IsAsciiSpace(*p)) {
                    if (++p == p_end)
                        break;
                }
                if (p == p_end)
                    break;
                const char *start = p;
                while (++p != p_end && *p != kComma)
                    /* nothing */ ;
                nsAutoCString lang(Substring(start, p));
                lang.CompressWhitespace(false, true);
                int32_t index = GetCJKLangGroupIndex(lang.get());
                if (index >= 0) {
                    nsCOMPtr<nsIAtom> atom = do_GetAtom(sCJKLangGroup[index]);
                    GetPrefFonts(atom, aFontEntryList);
                }
                p++;
            }
        }

        // Add the system locale
#ifdef XP_WIN
        switch (::GetACP()) {
            case 932: GetPrefFonts(nsGkAtoms::Japanese, aFontEntryList); break;
            case 936: GetPrefFonts(nsGkAtoms::zh_cn, aFontEntryList); break;
            case 949: GetPrefFonts(nsGkAtoms::ko, aFontEntryList); break;
            // XXX Don't we need to append nsGkAtoms::zh_hk if the codepage is 950?
            case 950: GetPrefFonts(nsGkAtoms::zh_tw, aFontEntryList); break;
        }
#else
        const char *ctype = setlocale(LC_CTYPE, NULL);
        if (ctype) {
            if (!PL_strncasecmp(ctype, "ja", 2)) {
                GetPrefFonts(nsGkAtoms::Japanese, aFontEntryList);
            } else if (!PL_strncasecmp(ctype, "zh_cn", 5)) {
                GetPrefFonts(nsGkAtoms::zh_cn, aFontEntryList);
            } else if (!PL_strncasecmp(ctype, "zh_hk", 5)) {
                GetPrefFonts(nsGkAtoms::zh_hk, aFontEntryList);
            } else if (!PL_strncasecmp(ctype, "zh_tw", 5)) {
                GetPrefFonts(nsGkAtoms::zh_tw, aFontEntryList);
            } else if (!PL_strncasecmp(ctype, "ko", 2)) {
                GetPrefFonts(nsGkAtoms::ko, aFontEntryList);
            }
        }
#endif

        // last resort...
        GetPrefFonts(nsGkAtoms::Japanese, aFontEntryList);
        GetPrefFonts(nsGkAtoms::ko, aFontEntryList);
        GetPrefFonts(nsGkAtoms::zh_cn, aFontEntryList);
        GetPrefFonts(nsGkAtoms::zh_hk, aFontEntryList);
        GetPrefFonts(nsGkAtoms::zh_tw, aFontEntryList);

        platform->SetPrefFontEntries(key, aFontEntryList);
    }
}

already_AddRefed<gfxFT2Font>
gfxFT2FontGroup::WhichFontSupportsChar(const nsTArray<nsRefPtr<gfxFontEntry> >& aFontEntryList, uint32_t aCh)
{
    for (uint32_t i = 0; i < aFontEntryList.Length(); i++) {
        gfxFontEntry *fe = aFontEntryList[i].get();
        if (fe->HasCharacter(aCh)) {
            nsRefPtr<gfxFT2Font> font =
                gfxFT2Font::GetOrMakeFont(static_cast<FontEntry*>(fe), &mStyle);
            return font.forget();
        }
    }
    return nullptr;
}

already_AddRefed<gfxFont>
gfxFT2FontGroup::WhichPrefFontSupportsChar(uint32_t aCh)
{
    if (aCh > 0xFFFF)
        return nullptr;

    nsRefPtr<gfxFT2Font> selectedFont;

    // check out the style's language
    nsAutoTArray<nsRefPtr<gfxFontEntry>, 5> fonts;
    GetPrefFonts(mStyle.language, fonts);
    selectedFont = WhichFontSupportsChar(fonts, aCh);

    // otherwise search prefs
    if (!selectedFont) {
        uint32_t unicodeRange = FindCharUnicodeRange(aCh);

        /* special case CJK */
        if (unicodeRange == kRangeSetCJK) {
            if (PR_LOG_TEST(GetFontLog(), PR_LOG_DEBUG)) {
                PR_LOG(GetFontLog(), PR_LOG_DEBUG, (" - Trying to find fonts for: CJK"));
            }

            nsAutoTArray<nsRefPtr<gfxFontEntry>, 15> fonts;
            GetCJKPrefFonts(fonts);
            selectedFont = WhichFontSupportsChar(fonts, aCh);
        } else {
            nsIAtom *langGroup = LangGroupFromUnicodeRange(unicodeRange);
            if (langGroup) {
                PR_LOG(GetFontLog(), PR_LOG_DEBUG, (" - Trying to find fonts for: %s", nsAtomCString(langGroup).get()));

                nsAutoTArray<nsRefPtr<gfxFontEntry>, 5> fonts;
                GetPrefFonts(langGroup, fonts);
                selectedFont = WhichFontSupportsChar(fonts, aCh);
            }
        }
    }

    if (selectedFont) {
        nsRefPtr<gfxFont> f = static_cast<gfxFont*>(selectedFont.get());
        return f.forget();
    }

    return nullptr;
}

already_AddRefed<gfxFont>
gfxFT2FontGroup::WhichSystemFontSupportsChar(uint32_t aCh, int32_t aRunScript)
{
#if defined(XP_WIN) || defined(ANDROID)
    FontEntry *fe = static_cast<FontEntry*>
        (gfxPlatformFontList::PlatformFontList()->
            SystemFindFontForChar(aCh, aRunScript, &mStyle));
    if (fe) {
        nsRefPtr<gfxFT2Font> f = gfxFT2Font::GetOrMakeFont(fe, &mStyle);
        nsRefPtr<gfxFont> font = f.get();
        return font.forget();
    }
#else
    nsRefPtr<gfxFont> selectedFont;
    nsRefPtr<gfxFont> refFont = GetFontAt(0);
    gfxToolkitPlatform *platform = gfxToolkitPlatform::GetPlatform();
    selectedFont = platform->FindFontForChar(aCh, refFont);
    if (selectedFont)
        return selectedFont.forget();
#endif
    return nullptr;
}

#endif // !ANDROID

/**
 * gfxFT2Font
 */

bool
gfxFT2Font::ShapeText(gfxContext      *aContext,
                      const PRUnichar *aText,
                      uint32_t         aOffset,
                      uint32_t         aLength,
                      int32_t          aScript,
                      gfxShapedText   *aShapedText,
                      bool             aPreferPlatformShaping)
{
    bool ok = false;

    if (FontCanSupportGraphite()) {
        if (gfxPlatform::GetPlatform()->UseGraphiteShaping()) {
            if (!mGraphiteShaper) {
                mGraphiteShaper = new gfxGraphiteShaper(this);
            }
            ok = mGraphiteShaper->ShapeText(aContext, aText,
                                            aOffset, aLength,
                                            aScript, aShapedText);
        }
    }

    if (!ok && gfxPlatform::GetPlatform()->UseHarfBuzzForScript(aScript)) {
        if (!mHarfBuzzShaper) {
            gfxFT2LockedFace face(this);
            mFUnitsConvFactor = face.XScale();

            mHarfBuzzShaper = new gfxHarfBuzzShaper(this);
        }
        ok = mHarfBuzzShaper->ShapeText(aContext, aText,
                                        aOffset, aLength,
                                        aScript, aShapedText);
    }

    if (!ok) {
        AddRange(aText, aOffset, aLength, aShapedText);
    }

    PostShapingFixup(aContext, aText, aOffset, aLength, aShapedText);

    return true;
}

void
gfxFT2Font::AddRange(const PRUnichar *aText, uint32_t aOffset,
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
        PRUnichar ch = aText[i];

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
            PRUnichar chNext = 0;
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
{
    NS_ASSERTION(mFontEntry, "Unable to find font entry for font.  Something is whack.");
    mApplySyntheticBold = aNeedsBold;
    mCharGlyphCache.Init(64);
}

gfxFT2Font::~gfxFT2Font()
{
}

cairo_font_face_t *
gfxFT2Font::CairoFontFace()
{
    return GetFontEntry()->CairoFontFace();
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
        font = new gfxFT2Font(scaledFont, aFontEntry, aStyle, aNeedsBold);
        cairo_scaled_font_destroy(scaledFont);
        if (!font)
            return nullptr;
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
gfxFT2Font::SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf,
                                FontCacheSizes*   aSizes) const
{
    gfxFont::SizeOfExcludingThis(aMallocSizeOf, aSizes);
    aSizes->mFontInstances +=
        mCharGlyphCache.SizeOfExcludingThis(nullptr, aMallocSizeOf);
}

void
gfxFT2Font::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf,
                                FontCacheSizes*   aSizes) const
{
    aSizes->mFontInstances += aMallocSizeOf(this);
    SizeOfExcludingThis(aMallocSizeOf, aSizes);
}
