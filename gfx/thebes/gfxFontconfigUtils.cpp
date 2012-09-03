/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "gfxFontconfigUtils.h"
#include "gfxFont.h"
#include "nsGkAtoms.h"

#include <locale.h>
#include <fontconfig/fontconfig.h>

#include "nsServiceManagerUtils.h"
#include "nsILanguageAtomService.h"
#include "nsTArray.h"
#include "mozilla/Preferences.h"

#include "nsIAtom.h"
#include "nsCRT.h"

using namespace mozilla;

/* static */ gfxFontconfigUtils* gfxFontconfigUtils::sUtils = nullptr;
static nsILanguageAtomService* gLangService = nullptr;

/* static */ void
gfxFontconfigUtils::Shutdown() {
    if (sUtils) {
        delete sUtils;
        sUtils = nullptr;
    }
    NS_IF_RELEASE(gLangService);
}

/* static */ uint8_t
gfxFontconfigUtils::FcSlantToThebesStyle(int aFcSlant)
{
    switch (aFcSlant) {
        case FC_SLANT_ITALIC:
            return NS_FONT_STYLE_ITALIC;
        case FC_SLANT_OBLIQUE:
            return NS_FONT_STYLE_OBLIQUE;
        default:
            return NS_FONT_STYLE_NORMAL;
    }
}

/* static */ uint8_t
gfxFontconfigUtils::GetThebesStyle(FcPattern *aPattern)
{
    int slant;
    if (FcPatternGetInteger(aPattern, FC_SLANT, 0, &slant) != FcResultMatch) {
        return NS_FONT_STYLE_NORMAL;
    }

    return FcSlantToThebesStyle(slant);
}

/* static */ int
gfxFontconfigUtils::GetFcSlant(const gfxFontStyle& aFontStyle)
{
    if (aFontStyle.style == NS_FONT_STYLE_ITALIC)
        return FC_SLANT_ITALIC;
    if (aFontStyle.style == NS_FONT_STYLE_OBLIQUE)
        return FC_SLANT_OBLIQUE;

    return FC_SLANT_ROMAN;
}

// OS/2 weight classes were introduced in fontconfig-2.1.93 (2003).
#ifndef FC_WEIGHT_THIN 
#define FC_WEIGHT_THIN              0 // 2.1.93
#define FC_WEIGHT_EXTRALIGHT        40 // 2.1.93
#define FC_WEIGHT_REGULAR           80 // 2.1.93
#define FC_WEIGHT_EXTRABOLD         205 // 2.1.93
#endif
// book was introduced in fontconfig-2.2.90 (and so fontconfig-2.3.0 in 2005)
#ifndef FC_WEIGHT_BOOK
#define FC_WEIGHT_BOOK              75
#endif
// extra black was introduced in fontconfig-2.4.91 (2007)
#ifndef FC_WEIGHT_EXTRABLACK
#define FC_WEIGHT_EXTRABLACK        215
#endif

/* static */ uint16_t
gfxFontconfigUtils::GetThebesWeight(FcPattern *aPattern)
{
    int weight;
    if (FcPatternGetInteger(aPattern, FC_WEIGHT, 0, &weight) != FcResultMatch)
        return NS_FONT_WEIGHT_NORMAL;

    if (weight <= (FC_WEIGHT_THIN + FC_WEIGHT_EXTRALIGHT) / 2)
        return 100;
    if (weight <= (FC_WEIGHT_EXTRALIGHT + FC_WEIGHT_LIGHT) / 2)
        return 200;
    if (weight <= (FC_WEIGHT_LIGHT + FC_WEIGHT_BOOK) / 2)
        return 300;
    if (weight <= (FC_WEIGHT_REGULAR + FC_WEIGHT_MEDIUM) / 2)
        // This includes FC_WEIGHT_BOOK
        return 400;
    if (weight <= (FC_WEIGHT_MEDIUM + FC_WEIGHT_DEMIBOLD) / 2)
        return 500;
    if (weight <= (FC_WEIGHT_DEMIBOLD + FC_WEIGHT_BOLD) / 2)
        return 600;
    if (weight <= (FC_WEIGHT_BOLD + FC_WEIGHT_EXTRABOLD) / 2)
        return 700;
    if (weight <= (FC_WEIGHT_EXTRABOLD + FC_WEIGHT_BLACK) / 2)
        return 800;
    if (weight <= FC_WEIGHT_BLACK)
        return 900;

    // including FC_WEIGHT_EXTRABLACK
    return 901;
}

/* static */ int
gfxFontconfigUtils::FcWeightForBaseWeight(int8_t aBaseWeight)
{
    NS_PRECONDITION(aBaseWeight >= 0 && aBaseWeight <= 10,
                    "base weight out of range");

    switch (aBaseWeight) {
        case 2:
            return FC_WEIGHT_EXTRALIGHT;
        case 3:
            return FC_WEIGHT_LIGHT;
        case 4:
            return FC_WEIGHT_REGULAR;
        case 5:
            return FC_WEIGHT_MEDIUM;
        case 6:
            return FC_WEIGHT_DEMIBOLD;
        case 7:
            return FC_WEIGHT_BOLD;
        case 8:
            return FC_WEIGHT_EXTRABOLD;
        case 9:
            return FC_WEIGHT_BLACK;
    }

    // extremes
    return aBaseWeight < 2 ? FC_WEIGHT_THIN : FC_WEIGHT_EXTRABLACK;
}

/* static */ int16_t
gfxFontconfigUtils::GetThebesStretch(FcPattern *aPattern)
{
    int width;
    if (FcPatternGetInteger(aPattern, FC_WIDTH, 0, &width) != FcResultMatch) {
        return NS_FONT_STRETCH_NORMAL;
    }

    if (width <= (FC_WIDTH_ULTRACONDENSED + FC_WIDTH_EXTRACONDENSED) / 2) {
        return NS_FONT_STRETCH_ULTRA_CONDENSED;
    }
    if (width <= (FC_WIDTH_EXTRACONDENSED + FC_WIDTH_CONDENSED) / 2) {
        return NS_FONT_STRETCH_EXTRA_CONDENSED;
    }
    if (width <= (FC_WIDTH_CONDENSED + FC_WIDTH_SEMICONDENSED) / 2) {
        return NS_FONT_STRETCH_CONDENSED;
    }
    if (width <= (FC_WIDTH_SEMICONDENSED + FC_WIDTH_NORMAL) / 2) {
        return NS_FONT_STRETCH_SEMI_CONDENSED;
    }
    if (width <= (FC_WIDTH_NORMAL + FC_WIDTH_SEMIEXPANDED) / 2) {
        return NS_FONT_STRETCH_NORMAL;
    }
    if (width <= (FC_WIDTH_SEMIEXPANDED + FC_WIDTH_EXPANDED) / 2) {
        return NS_FONT_STRETCH_SEMI_EXPANDED;
    }
    if (width <= (FC_WIDTH_EXPANDED + FC_WIDTH_EXTRAEXPANDED) / 2) {
        return NS_FONT_STRETCH_EXPANDED;
    }
    if (width <= (FC_WIDTH_EXTRAEXPANDED + FC_WIDTH_ULTRAEXPANDED) / 2) {
        return NS_FONT_STRETCH_EXTRA_EXPANDED;
    }
    return NS_FONT_STRETCH_ULTRA_EXPANDED;
}

/* static */ int
gfxFontconfigUtils::FcWidthForThebesStretch(int16_t aStretch)
{
    switch (aStretch) {
        default: // this will catch "normal" (0) as well as out-of-range values
            return FC_WIDTH_NORMAL;
        case NS_FONT_STRETCH_ULTRA_CONDENSED:
            return FC_WIDTH_ULTRACONDENSED;
        case NS_FONT_STRETCH_EXTRA_CONDENSED:
            return FC_WIDTH_EXTRACONDENSED;
        case NS_FONT_STRETCH_CONDENSED:
            return FC_WIDTH_CONDENSED;
        case NS_FONT_STRETCH_SEMI_CONDENSED:
            return FC_WIDTH_SEMICONDENSED;
        case NS_FONT_STRETCH_SEMI_EXPANDED:
            return FC_WIDTH_SEMIEXPANDED;
        case NS_FONT_STRETCH_EXPANDED:
            return FC_WIDTH_EXPANDED;
        case NS_FONT_STRETCH_EXTRA_EXPANDED:
            return FC_WIDTH_EXTRAEXPANDED;
        case NS_FONT_STRETCH_ULTRA_EXPANDED:
            return FC_WIDTH_ULTRAEXPANDED;
    }
}

// This makes a guess at an FC_WEIGHT corresponding to a base weight and
// offset (without any knowledge of which weights are available).

/* static */ int
GuessFcWeight(const gfxFontStyle& aFontStyle)
{
    /*
     * weights come in two parts crammed into one
     * integer -- the "base" weight is weight / 100,
     * the rest of the value is the "offset" from that
     * weight -- the number of steps to move to adjust
     * the weight in the list of supported font weights,
     * this value can be negative or positive.
     */
    int8_t weight = aFontStyle.ComputeWeight();

    // ComputeWeight trimmed the range of weights for us
    NS_ASSERTION(weight >= 0 && weight <= 10,
                 "base weight out of range");

    return gfxFontconfigUtils::FcWeightForBaseWeight(weight);
}

static void
AddString(FcPattern *aPattern, const char *object, const char *aString)
{
    FcPatternAddString(aPattern, object,
                       gfxFontconfigUtils::ToFcChar8(aString));
}

static void
AddWeakString(FcPattern *aPattern, const char *object, const char *aString)
{
    FcValue value;
    value.type = FcTypeString;
    value.u.s = gfxFontconfigUtils::ToFcChar8(aString);

    FcPatternAddWeak(aPattern, object, value, FcTrue);
}

static void
AddLangGroup(FcPattern *aPattern, nsIAtom *aLangGroup)
{
    // Translate from mozilla's internal mapping into fontconfig's
    nsAutoCString lang;
    gfxFontconfigUtils::GetSampleLangForGroup(aLangGroup, &lang);

    if (!lang.IsEmpty()) {
        AddString(aPattern, FC_LANG, lang.get());
    }
}

nsReturnRef<FcPattern>
gfxFontconfigUtils::NewPattern(const nsTArray<nsString>& aFamilies,
                               const gfxFontStyle& aFontStyle,
                               const char *aLang)
{
    static const char* sFontconfigGenerics[] =
        { "sans-serif", "serif", "monospace", "fantasy", "cursive" };

    nsAutoRef<FcPattern> pattern(FcPatternCreate());
    if (!pattern)
        return nsReturnRef<FcPattern>();

    FcPatternAddDouble(pattern, FC_PIXEL_SIZE, aFontStyle.size);
    FcPatternAddInteger(pattern, FC_SLANT, GetFcSlant(aFontStyle));
    FcPatternAddInteger(pattern, FC_WEIGHT, GuessFcWeight(aFontStyle));
    FcPatternAddInteger(pattern, FC_WIDTH, FcWidthForThebesStretch(aFontStyle.stretch));

    if (aLang) {
        AddString(pattern, FC_LANG, aLang);
    }

    bool useWeakBinding = false;
    for (uint32_t i = 0; i < aFamilies.Length(); ++i) {
        NS_ConvertUTF16toUTF8 family(aFamilies[i]);
        if (!useWeakBinding) {
            AddString(pattern, FC_FAMILY, family.get());

            // fontconfig generic families are typically implemented with weak
            // aliases (so that the preferred font depends on language).
            // However, this would give them lower priority than subsequent
            // non-generic families in the list.  To ensure that subsequent
            // families do not have a higher priority, they are given weak
            // bindings.
            for (uint32_t g = 0;
                 g < ArrayLength(sFontconfigGenerics);
                 ++g) {
                if (0 == FcStrCmpIgnoreCase(ToFcChar8(sFontconfigGenerics[g]),
                                            ToFcChar8(family.get()))) {
                    useWeakBinding = true;
                    break;
                }
            }
        } else {
            AddWeakString(pattern, FC_FAMILY, family.get());
        }
    }

    return pattern.out();
}

gfxFontconfigUtils::gfxFontconfigUtils()
    : mLastConfig(NULL)
{
    mFontsByFamily.Init(50);
    mFontsByFullname.Init(50);
    mLangSupportTable.Init(20);
    UpdateFontListInternal();
}

nsresult
gfxFontconfigUtils::GetFontList(nsIAtom *aLangGroup,
                                const nsACString& aGenericFamily,
                                nsTArray<nsString>& aListOfFonts)
{
    aListOfFonts.Clear();

    nsTArray<nsCString> fonts;
    nsresult rv = GetFontListInternal(fonts, aLangGroup);
    if (NS_FAILED(rv))
        return rv;

    for (uint32_t i = 0; i < fonts.Length(); ++i) {
        aListOfFonts.AppendElement(NS_ConvertUTF8toUTF16(fonts[i]));
    }

    aListOfFonts.Sort();

    int32_t serif = 0, sansSerif = 0, monospace = 0;

    // Fontconfig supports 3 generic fonts, "serif", "sans-serif", and
    // "monospace", slightly different from CSS's 5.
    if (aGenericFamily.IsEmpty())
        serif = sansSerif = monospace = 1;
    else if (aGenericFamily.LowerCaseEqualsLiteral("serif"))
        serif = 1;
    else if (aGenericFamily.LowerCaseEqualsLiteral("sans-serif"))
        sansSerif = 1;
    else if (aGenericFamily.LowerCaseEqualsLiteral("monospace"))
        monospace = 1;
    else if (aGenericFamily.LowerCaseEqualsLiteral("cursive") ||
             aGenericFamily.LowerCaseEqualsLiteral("fantasy"))
        serif = sansSerif = 1;
    else
        NS_NOTREACHED("unexpected CSS generic font family");

    // The first in the list becomes the default in
    // gFontsDialog.readFontSelection() if the preference-selected font is not
    // available, so put system configured defaults first.
    if (monospace)
        aListOfFonts.InsertElementAt(0, NS_LITERAL_STRING("monospace"));
    if (sansSerif)
        aListOfFonts.InsertElementAt(0, NS_LITERAL_STRING("sans-serif"));
    if (serif)
        aListOfFonts.InsertElementAt(0, NS_LITERAL_STRING("serif"));

    return NS_OK;
}

struct MozLangGroupData {
    nsIAtom* const& mozLangGroup;
    const char *defaultLang;
};

const MozLangGroupData MozLangGroups[] = {
    { nsGkAtoms::x_western,      "en" },
    { nsGkAtoms::x_central_euro, "pl" },
    { nsGkAtoms::x_cyrillic,     "ru" },
    { nsGkAtoms::x_baltic,       "lv" },
    { nsGkAtoms::x_devanagari,   "hi" },
    { nsGkAtoms::x_tamil,        "ta" },
    { nsGkAtoms::x_armn,         "hy" },
    { nsGkAtoms::x_beng,         "bn" },
    { nsGkAtoms::x_cans,         "iu" },
    { nsGkAtoms::x_ethi,         "am" },
    { nsGkAtoms::x_geor,         "ka" },
    { nsGkAtoms::x_gujr,         "gu" },
    { nsGkAtoms::x_guru,         "pa" },
    { nsGkAtoms::x_khmr,         "km" },
    { nsGkAtoms::x_knda,         "kn" },
    { nsGkAtoms::x_mlym,         "ml" },
    { nsGkAtoms::x_orya,         "or" },
    { nsGkAtoms::x_sinh,         "si" },
    { nsGkAtoms::x_telu,         "te" },
    { nsGkAtoms::x_tibt,         "bo" },
    { nsGkAtoms::Unicode,        0    },
    { nsGkAtoms::x_user_def,     0    }
};

static bool
TryLangForGroup(const nsACString& aOSLang, nsIAtom *aLangGroup,
                nsACString *aFcLang)
{
    // Truncate at '.' or '@' from aOSLang, and convert '_' to '-'.
    // aOSLang is in the form "language[_territory][.codeset][@modifier]".
    // fontconfig takes languages in the form "language-territory".
    // nsILanguageAtomService takes languages in the form language-subtag,
    // where subtag may be a territory.  fontconfig and nsILanguageAtomService
    // handle case-conversion for us.
    const char *pos, *end;
    aOSLang.BeginReading(pos);
    aOSLang.EndReading(end);
    aFcLang->Truncate();
    while (pos < end) {
        switch (*pos) {
            case '.':
            case '@':
                end = pos;
                break;
            case '_':
                aFcLang->Append('-');
                break;
            default:
                aFcLang->Append(*pos);
        }
        ++pos;
    }

    nsIAtom *atom =
        gLangService->LookupLanguage(*aFcLang);

    return atom == aLangGroup;
}

/* static */ void
gfxFontconfigUtils::GetSampleLangForGroup(nsIAtom *aLangGroup,
                                          nsACString *aFcLang)
{
    NS_PRECONDITION(aFcLang != nullptr, "aFcLang must not be NULL");

    const MozLangGroupData *langGroup = nullptr;

    for (unsigned int i = 0; i < ArrayLength(MozLangGroups); ++i) {
        if (aLangGroup == MozLangGroups[i].mozLangGroup) {
            langGroup = &MozLangGroups[i];
            break;
        }
    }

    if (!langGroup) {
        // Not a special mozilla language group.
        // Use aLangGroup as a language code.
        aLangGroup->ToUTF8String(*aFcLang);
        return;
    }

    // Check the environment for the users preferred language that corresponds
    // to this langGroup.
    if (!gLangService) {
        CallGetService(NS_LANGUAGEATOMSERVICE_CONTRACTID, &gLangService);
    }

    if (gLangService) {
        const char *languages = getenv("LANGUAGE");
        if (languages) {
            const char separator = ':';

            for (const char *pos = languages; true; ++pos) {
                if (*pos == '\0' || *pos == separator) {
                    if (languages < pos &&
                        TryLangForGroup(Substring(languages, pos),
                                        aLangGroup, aFcLang))
                        return;

                    if (*pos == '\0')
                        break;

                    languages = pos + 1;
                }
            }
        }
        const char *ctype = setlocale(LC_CTYPE, NULL);
        if (ctype &&
            TryLangForGroup(nsDependentCString(ctype), aLangGroup, aFcLang))
            return;
    }

    if (langGroup->defaultLang) {
        aFcLang->Assign(langGroup->defaultLang);
    } else {
        aFcLang->Truncate();
    }
}

nsresult
gfxFontconfigUtils::GetFontListInternal(nsTArray<nsCString>& aListOfFonts,
                                        nsIAtom *aLangGroup)
{
    FcPattern *pat = NULL;
    FcObjectSet *os = NULL;
    FcFontSet *fs = NULL;
    nsresult rv = NS_ERROR_FAILURE;

    aListOfFonts.Clear();

    pat = FcPatternCreate();
    if (!pat)
        goto end;

    os = FcObjectSetBuild(FC_FAMILY, NULL);
    if (!os)
        goto end;

    // take the pattern and add the lang group to it
    if (aLangGroup) {
        AddLangGroup(pat, aLangGroup);
    }

    fs = FcFontList(NULL, pat, os);
    if (!fs)
        goto end;

    for (int i = 0; i < fs->nfont; i++) {
        char *family;

        if (FcPatternGetString(fs->fonts[i], FC_FAMILY, 0,
                               (FcChar8 **) &family) != FcResultMatch)
        {
            continue;
        }

        // Remove duplicates...
        nsAutoCString strFamily(family);
        if (aListOfFonts.Contains(strFamily))
            continue;

        aListOfFonts.AppendElement(strFamily);
    }

    rv = NS_OK;

  end:
    if (NS_FAILED(rv))
        aListOfFonts.Clear();

    if (pat)
        FcPatternDestroy(pat);
    if (os)
        FcObjectSetDestroy(os);
    if (fs)
        FcFontSetDestroy(fs);

    return rv;
}

nsresult
gfxFontconfigUtils::UpdateFontList()
{
    return UpdateFontListInternal(true);
}

nsresult
gfxFontconfigUtils::UpdateFontListInternal(bool aForce)
{
    if (!aForce) {
        // This checks periodically according to fontconfig's configured
        // <rescan> interval.
        FcInitBringUptoDate();
    } else if (!FcConfigUptoDate(NULL)) { // check now with aForce
        mLastConfig = NULL;
        FcInitReinitialize();
    }

    // FcInitReinitialize() (used by FcInitBringUptoDate) creates a new config
    // before destroying the old config, so the only way that we'd miss an
    // update is if fontconfig did more than one update and the memory for the
    // most recent config happened to be at the same location as the original
    // config.
    FcConfig *currentConfig = FcConfigGetCurrent();
    if (currentConfig == mLastConfig)
        return NS_OK;

    // This FcFontSet is owned by fontconfig
    FcFontSet *fontSet = FcConfigGetFonts(currentConfig, FcSetSystem);

    mFontsByFamily.Clear();
    mFontsByFullname.Clear();
    mLangSupportTable.Clear();
    mAliasForMultiFonts.Clear();

    // Record the existing font families
    for (int f = 0; f < fontSet->nfont; ++f) {
        FcPattern *font = fontSet->fonts[f];

        FcChar8 *family;
        for (int v = 0;
             FcPatternGetString(font, FC_FAMILY, v, &family) == FcResultMatch;
             ++v) {
            FontsByFcStrEntry *entry = mFontsByFamily.PutEntry(family);
            if (entry) {
                bool added = entry->AddFont(font);

                if (!entry->mKey) {
                    // The reference to the font pattern keeps the pointer to
                    // string for the key valid.  If adding the font failed
                    // then the entry must be removed.
                    if (added) {
                        entry->mKey = family;
                    } else {
                        mFontsByFamily.RawRemoveEntry(entry);
                    }
                }
            }
        }
    }

    // XXX we don't support all alias names.
    // Because if we don't check whether the given font name is alias name,
    // fontconfig converts the non existing font to sans-serif.
    // This is not good if the web page specifies font-family
    // that has Windows font name in the first.
    NS_ENSURE_TRUE(Preferences::GetRootBranch(), NS_ERROR_FAILURE);
    nsAdoptingCString list = Preferences::GetCString("font.alias-list");

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
            nsAutoCString name(Substring(start, p));
            name.CompressWhitespace(false, true);
            mAliasForMultiFonts.AppendElement(name);
            p++;
        }
    }

    mLastConfig = currentConfig;
    return NS_OK;
}

nsresult
gfxFontconfigUtils::GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName)
{
    aFamilyName.Truncate();

    // The fontconfig has generic family names in the font list.
    if (aFontName.EqualsLiteral("serif") ||
        aFontName.EqualsLiteral("sans-serif") ||
        aFontName.EqualsLiteral("monospace")) {
        aFamilyName.Assign(aFontName);
        return NS_OK;
    }

    nsresult rv = UpdateFontListInternal();
    if (NS_FAILED(rv))
        return rv;

    NS_ConvertUTF16toUTF8 fontname(aFontName);

    // return empty string if no such family exists
    if (!IsExistingFamily(fontname))
        return NS_OK;

    FcPattern *pat = NULL;
    FcObjectSet *os = NULL;
    FcFontSet *givenFS = NULL;
    nsTArray<nsCString> candidates;
    FcFontSet *candidateFS = NULL;
    rv = NS_ERROR_FAILURE;

    pat = FcPatternCreate();
    if (!pat)
        goto end;

    FcPatternAddString(pat, FC_FAMILY, (FcChar8 *)fontname.get());

    os = FcObjectSetBuild(FC_FAMILY, FC_FILE, FC_INDEX, NULL);
    if (!os)
        goto end;

    givenFS = FcFontList(NULL, pat, os);
    if (!givenFS)
        goto end;

    // The first value associated with a FC_FAMILY property is the family
    // returned by GetFontList(), so use this value if appropriate.

    // See if there is a font face with first family equal to the given family.
    for (int i = 0; i < givenFS->nfont; ++i) {
        char *firstFamily;
        if (FcPatternGetString(givenFS->fonts[i], FC_FAMILY, 0,
                               (FcChar8 **) &firstFamily) != FcResultMatch)
            continue;

        nsDependentCString first(firstFamily);
        if (!candidates.Contains(first)) {
            candidates.AppendElement(first);

            if (fontname.Equals(first)) {
                aFamilyName.Assign(aFontName);
                rv = NS_OK;
                goto end;
            }
        }
    }

    // See if any of the first family names represent the same set of font
    // faces as the given family.
    for (uint32_t j = 0; j < candidates.Length(); ++j) {
        FcPatternDel(pat, FC_FAMILY);
        FcPatternAddString(pat, FC_FAMILY, (FcChar8 *)candidates[j].get());

        candidateFS = FcFontList(NULL, pat, os);
        if (!candidateFS)
            goto end;

        if (candidateFS->nfont != givenFS->nfont)
            continue;

        bool equal = true;
        for (int i = 0; i < givenFS->nfont; ++i) {
            if (!FcPatternEqual(candidateFS->fonts[i], givenFS->fonts[i])) {
                equal = false;
                break;
            }
        }
        if (equal) {
            AppendUTF8toUTF16(candidates[j], aFamilyName);
            rv = NS_OK;
            goto end;
        }
    }

    // No match found; return empty string.
    rv = NS_OK;

  end:
    if (pat)
        FcPatternDestroy(pat);
    if (os)
        FcObjectSetDestroy(os);
    if (givenFS)
        FcFontSetDestroy(givenFS);
    if (candidateFS)
        FcFontSetDestroy(candidateFS);

    return rv;
}

nsresult
gfxFontconfigUtils::ResolveFontName(const nsAString& aFontName,
                                    gfxPlatform::FontResolverCallback aCallback,
                                    void *aClosure,
                                    bool& aAborted)
{
    aAborted = false;

    nsresult rv = UpdateFontListInternal();
    if (NS_FAILED(rv))
        return rv;

    NS_ConvertUTF16toUTF8 fontname(aFontName);
    // Sometimes, the font has two or more names (e.g., "Sazanami Gothic" has
    // Japanese localized name).  We should not resolve to a single name
    // because different names sometimes have different behavior. e.g., with
    // the default settings of "Sazanami" on Fedora Core 5, the non-localized
    // name uses anti-alias, but the localized name uses it.  So, we should
    // check just whether the font is existing, without resolving to regular
    // name.
    //
    // The family names in mAliasForMultiFonts are names understood by
    // fontconfig.  The actual font to which they resolve depends on the
    // entire match pattern.  That info is not available here, but there
    // will be a font so leave the resolving to the gfxFontGroup.
    if (IsExistingFamily(fontname) ||
        mAliasForMultiFonts.Contains(fontname, gfxIgnoreCaseCStringComparator()))
        aAborted = !(*aCallback)(aFontName, aClosure);

    return NS_OK;
}

bool
gfxFontconfigUtils::IsExistingFamily(const nsCString& aFamilyName)
{
    return mFontsByFamily.GetEntry(ToFcChar8(aFamilyName)) != nullptr;
}

const nsTArray< nsCountedRef<FcPattern> >&
gfxFontconfigUtils::GetFontsForFamily(const FcChar8 *aFamilyName)
{
    FontsByFcStrEntry *entry = mFontsByFamily.GetEntry(aFamilyName);

    if (!entry)
        return mEmptyPatternArray;

    return entry->GetFonts();
}

// Fontconfig only provides a fullname property for fonts in formats with SFNT
// wrappers.  For other font formats (including PCF and PS Type 1), a fullname
// must be generated from the family and style properties.  Only the first
// family and style is checked, but that should be OK, as I don't expect
// non-SFNT fonts to have multiple families or styles.
bool
gfxFontconfigUtils::GetFullnameFromFamilyAndStyle(FcPattern *aFont,
                                                  nsACString *aFullname)
{
    FcChar8 *family;
    if (FcPatternGetString(aFont, FC_FAMILY, 0, &family) != FcResultMatch)
        return false;

    aFullname->Truncate();
    aFullname->Append(ToCString(family));

    FcChar8 *style;
    if (FcPatternGetString(aFont, FC_STYLE, 0, &style) == FcResultMatch &&
        strcmp(ToCString(style), "Regular") != 0) {
        aFullname->Append(' ');
        aFullname->Append(ToCString(style));
    }

    return true;
}

bool
gfxFontconfigUtils::FontsByFullnameEntry::KeyEquals(KeyTypePointer aKey) const
{
    const FcChar8 *key = mKey;
    // If mKey is NULL, key comes from the style and family of the first font.
    nsAutoCString fullname;
    if (!key) {
        NS_ASSERTION(mFonts.Length(), "No font in FontsByFullnameEntry!");
        GetFullnameFromFamilyAndStyle(mFonts[0], &fullname);

        key = ToFcChar8(fullname);
    }

    return FcStrCmpIgnoreCase(aKey, key) == 0;
}

void
gfxFontconfigUtils::AddFullnameEntries()
{
    // This FcFontSet is owned by fontconfig
    FcFontSet *fontSet = FcConfigGetFonts(NULL, FcSetSystem);

    // Record the existing font families
    for (int f = 0; f < fontSet->nfont; ++f) {
        FcPattern *font = fontSet->fonts[f];

        int v = 0;
        FcChar8 *fullname;
        while (FcPatternGetString(font,
                                  FC_FULLNAME, v, &fullname) == FcResultMatch) {
            FontsByFullnameEntry *entry = mFontsByFullname.PutEntry(fullname);
            if (entry) {
                // entry always has space for one font, so the first AddFont
                // will always succeed, and so the entry will always have a
                // font from which to obtain the key.
                bool added = entry->AddFont(font);
                // The key may be NULL either if this is the first font, or if
                // the first font does not have a fullname property, and so
                // the key is obtained from the font.  Set the key in both
                // cases.  The check that AddFont succeeded is required for
                // the second case.
                if (!entry->mKey && added) {
                    entry->mKey = fullname;
                }
            }

            ++v;
        }

        // Fontconfig does not provide a fullname property for all fonts.
        if (v == 0) {
            nsAutoCString name;
            if (!GetFullnameFromFamilyAndStyle(font, &name))
                continue;

            FontsByFullnameEntry *entry =
                mFontsByFullname.PutEntry(ToFcChar8(name));
            if (entry) {
                entry->AddFont(font);
                // Either entry->mKey has been set for a previous font or it
                // remains NULL to indicate that the key is obtained from the
                // first font.
            }
        }
    }
}

const nsTArray< nsCountedRef<FcPattern> >&
gfxFontconfigUtils::GetFontsForFullname(const FcChar8 *aFullname)
{
    if (mFontsByFullname.Count() == 0) {
        AddFullnameEntries();
    }

    FontsByFullnameEntry *entry = mFontsByFullname.GetEntry(aFullname);

    if (!entry)
        return mEmptyPatternArray;

    return entry->GetFonts();
}

static FcLangResult
CompareLangString(const FcChar8 *aLangA, const FcChar8 *aLangB) {
    FcLangResult result = FcLangDifferentLang;
    for (uint32_t i = 0; ; ++i) {
        FcChar8 a = FcToLower(aLangA[i]);
        FcChar8 b = FcToLower(aLangB[i]);

        if (a != b) {
            if ((a == '\0' && b == '-') || (a == '-' && b == '\0'))
                return FcLangDifferentCountry;

            return result;
        }
        if (a == '\0')
            return FcLangEqual;

        if (a == '-') {
            result = FcLangDifferentCountry;
        }
    }
}

/* static */
FcLangResult
gfxFontconfigUtils::GetLangSupport(FcPattern *aFont, const FcChar8 *aLang)
{
    // When fontconfig builds a pattern for a system font, it will set a
    // single LangSet property value for the font.  That value may be removed
    // and additional string values may be added through FcConfigSubsitute
    // with FcMatchScan.  Values that are neither LangSet nor string are
    // considered errors in fontconfig sort and match functions.
    //
    // If no string nor LangSet value is found, then either the font is a
    // system font and the LangSet has been removed through FcConfigSubsitute,
    // or the font is a web font and its language support is unknown.
    // Returning FcLangDifferentLang for these fonts ensures that this font
    // will not be assumed to satisfy the language, and so language will be
    // prioritized in sorting fallback fonts.
    FcValue value;
    FcLangResult best = FcLangDifferentLang;
    for (int v = 0;
         FcPatternGet(aFont, FC_LANG, v, &value) == FcResultMatch;
         ++v) {

        FcLangResult support;
        switch (value.type) {
            case FcTypeLangSet:
                support = FcLangSetHasLang(value.u.l, aLang);
                break;
            case FcTypeString:
                support = CompareLangString(value.u.s, aLang);
                break;
            default:
                // error. continue to see if there is a useful value.
                continue;
        }

        if (support < best) { // lower is better
            if (support == FcLangEqual)
                return support;
            best = support;
        }        
    }

    return best;
}

gfxFontconfigUtils::LangSupportEntry *
gfxFontconfigUtils::GetLangSupportEntry(const FcChar8 *aLang, bool aWithFonts)
{
    // Currently any unrecognized languages from documents will be converted
    // to x-unicode by nsILanguageAtomService, so there is a limit on the
    // langugages that will be added here.  Reconsider when/if document
    // languages are passed to this routine.

    LangSupportEntry *entry = mLangSupportTable.PutEntry(aLang);
    if (!entry)
        return nullptr;

    FcLangResult best = FcLangDifferentLang;

    if (!entry->IsKeyInitialized()) {
        entry->InitKey(aLang);
    } else {
        // mSupport is already initialized.
        if (!aWithFonts)
            return entry;

        best = entry->mSupport;
        // If there is support for this language, an empty font list indicates
        // that the list hasn't been initialized yet.
        if (best == FcLangDifferentLang || entry->mFonts.Length() > 0)
            return entry;
    }

    // This FcFontSet is owned by fontconfig
    FcFontSet *fontSet = FcConfigGetFonts(NULL, FcSetSystem);

    nsAutoTArray<FcPattern*,100> fonts;

    for (int f = 0; f < fontSet->nfont; ++f) {
        FcPattern *font = fontSet->fonts[f];

        FcLangResult support = GetLangSupport(font, aLang);

        if (support < best) { // lower is better
            best = support;
            if (aWithFonts) {
                fonts.Clear();
            } else if (best == FcLangEqual) {
                break;
            }
        }

        // The font list in the LangSupportEntry is expected to be used only
        // when no default fonts support the language.  There would be a large
        // number of fonts in entries for languages using Latin script but
        // these do not need to be created because default fonts already
        // support these languages.
        if (aWithFonts && support != FcLangDifferentLang && support == best) {
            fonts.AppendElement(font);
        }
    }

    entry->mSupport = best;
    if (aWithFonts) {
        if (fonts.Length() != 0) {
            entry->mFonts.AppendElements(fonts.Elements(), fonts.Length());
        } else if (best != FcLangDifferentLang) {
            // Previously there was a font that supported this language at the
            // level of entry->mSupport, but it has now disappeared.  At least
            // entry->mSupport needs to be recalculated, but this is an
            // indication that the set of installed fonts has changed, so
            // update all caches.
            mLastConfig = NULL; // invalidates caches
            UpdateFontListInternal(true);
            return GetLangSupportEntry(aLang, aWithFonts);
        }
    }

    return entry;
}

FcLangResult
gfxFontconfigUtils::GetBestLangSupport(const FcChar8 *aLang)
{
    UpdateFontListInternal();

    LangSupportEntry *entry = GetLangSupportEntry(aLang, false);
    if (!entry)
        return FcLangEqual;

    return entry->mSupport;
}

const nsTArray< nsCountedRef<FcPattern> >&
gfxFontconfigUtils::GetFontsForLang(const FcChar8 *aLang)
{
    LangSupportEntry *entry = GetLangSupportEntry(aLang, true);
    if (!entry)
        return mEmptyPatternArray;

    return entry->mFonts;
}

bool
gfxFontNameList::Exists(nsAString& aName) {
    for (uint32_t i = 0; i < Length(); i++) {
        if (aName.Equals(ElementAt(i)))
            return true;
    }
    return false;
}
