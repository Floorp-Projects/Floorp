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
 * The Original Code is Mozilla Foundation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#if defined(MOZ_WIDGET_GTK2)
#include "gfxPlatformGtk.h"
#define gfxToolkitPlatform gfxPlatformGtk
#elif defined(MOZ_WIDGET_QT)
#include "gfxQtPlatform.h"
#include <qfontinfo.h>
#define gfxToolkitPlatform gfxQtPlatform
#elif defined(XP_WIN)
#ifdef WINCE
#define SHGetSpecialFolderPathW SHGetSpecialFolderPath
#endif
#include "gfxWindowsPlatform.h"
#define gfxToolkitPlatform gfxWindowsPlatform
#endif
#include "gfxTypes.h"
#include "gfxFT2Fonts.h"
#include <locale.h>
#include "cairo-ft.h"
#include FT_TRUETYPE_TAGS_H
#include FT_TRUETYPE_TABLES_H
#include "gfxFontUtils.h"
#include "nsTArray.h"
#include "nsUnicodeRange.h"
#include "nsIPrefService.h"
#include "nsIPrefLocalizedString.h"
#include "nsServiceManagerUtils.h"
#include "nsCRT.h"

#include "prlog.h"
#include "prinit.h"
static PRLogModuleInfo *gFontLog = PR_NewLogModule("ft2fonts");

static const char *sCJKLangGroup[] = {
    "ja",
    "ko",
    "zh-CN",
    "zh-HK",
    "zh-TW"
};

#define COUNT_OF_CJK_LANG_GROUP 5
#define CJK_LANG_JA    sCJKLangGroup[0]
#define CJK_LANG_KO    sCJKLangGroup[1]
#define CJK_LANG_ZH_CN sCJKLangGroup[2]
#define CJK_LANG_ZH_HK sCJKLangGroup[3]
#define CJK_LANG_ZH_TW sCJKLangGroup[4]

/**
 * FontEntry
 */

FontEntry::FontEntry(const FontEntry& aFontEntry) :
    gfxFontEntry(aFontEntry)
{
    mFTFace = aFontEntry.mFTFace;
    if (aFontEntry.mFontFace)
        mFontFace = cairo_font_face_reference(aFontEntry.mFontFace);
    else
        mFontFace = nsnull;
}

FontEntry::~FontEntry()
{
    // Do nothing for mFTFace here since FTFontDestroyFunc is called by cairo.
    mFTFace = nsnull;

    if (mFontFace) {
        cairo_font_face_destroy(mFontFace);
        mFontFace = nsnull;
    }
}

/* static */
FontEntry*  
FontEntry::CreateFontEntry(const gfxProxyFontEntry &aProxyEntry, 
                           nsISupports *aLoader, const PRUint8 *aFontData, 
                           PRUint32 aLength) {
    if (!gfxFontUtils::ValidateSFNTHeaders(aFontData, aLength))
        return nsnull;
    FT_Face face;
    FT_Error error =
        FT_New_Memory_Face(gfxToolkitPlatform::GetPlatform()->GetFTLibrary(), 
                           aFontData, aLength, 0, &face);
    if (error != FT_Err_Ok)
        return nsnull;
    FontEntry* fe = FontEntry::CreateFontEntryFromFace(face);
    fe->mItalic = aProxyEntry.mItalic;
    fe->mWeight = aProxyEntry.mWeight;
    fe->mStretch = aProxyEntry.mStretch;
    return fe;

}

static void
FTFontDestroyFunc(void *data)
{
    FT_Face face = (FT_Face)data;
    FT_Done_Face(face);
}

/* static */ FontEntry*  
FontEntry::CreateFontEntryFromFace(FT_Face aFace) {
    static cairo_user_data_key_t key;

    if (!aFace->family_name) {
        FT_Done_Face(aFace);
        return nsnull;
    }
    // Construct font name from family name and style name, regular fonts
    // do not have the modifier by convention.
    NS_ConvertUTF8toUTF16 fontName(aFace->family_name);
    if (aFace->style_name && strcmp("Regular", aFace->style_name)) {
        fontName.AppendLiteral(" ");
        AppendUTF8toUTF16(aFace->style_name, fontName);
    }
    FontEntry *fe = new FontEntry(fontName);
    fe->mItalic = aFace->style_flags & FT_STYLE_FLAG_ITALIC;
    fe->mFTFace = aFace;
    fe->mFontFace = cairo_ft_font_face_create_for_ft_face(aFace, 0);
    cairo_font_face_set_user_data(fe->mFontFace, &key,
                                  aFace, FTFontDestroyFunc);
    TT_OS2 *os2 = static_cast<TT_OS2*>(FT_Get_Sfnt_Table(aFace, ft_sfnt_os2));

    PRUint16 os2weight = 0;
    if (os2 && os2->version != 0xffff) {
        // Technically, only 100 to 900 are valid, but some fonts
        // have this set wrong -- e.g. "Microsoft Logo Bold Italic" has
        // it set to 6 instead of 600.  We try to be nice and handle that
        // as well.
        if (os2->usWeightClass >= 100 && os2->usWeightClass <= 900)
            os2weight = os2->usWeightClass;
        else if (os2->usWeightClass >= 1 && os2->usWeightClass <= 9)
            os2weight = os2->usWeightClass * 100;
    }

    if (os2weight != 0)
        fe->mWeight = os2weight;
    else if (aFace->style_flags & FT_STYLE_FLAG_BOLD)
        fe->mWeight = 700;
    else
        fe->mWeight = 400;

    NS_ASSERTION(fe->mWeight >= 100 && fe->mWeight <= 900, "Invalid final weight in font!");

    return fe;
}

FontEntry*
gfxFT2Font::GetFontEntry()
{
    return static_cast<FontEntry*> (mFontEntry.get());
}

cairo_font_face_t *
FontEntry::CairoFontFace()
{
    static cairo_user_data_key_t key;

    if (!mFontFace) {
        FT_Face face;
        FT_New_Face(gfxToolkitPlatform::GetPlatform()->GetFTLibrary(), mFilename.get(), mFTFontIndex, &face);
        mFTFace = face;
        mFontFace = cairo_ft_font_face_create_for_ft_face(face, 0);
        cairo_font_face_set_user_data(mFontFace, &key, face, FTFontDestroyFunc);
    }
    return mFontFace;
}

nsresult
FontEntry::ReadCMAP()
{
    if (mCmapInitialized) return NS_OK;

    // attempt this once, if errors occur leave a blank cmap
    mCmapInitialized = PR_TRUE;

    // Ensure existence of mFTFace
    CairoFontFace();
    NS_ENSURE_TRUE(mFTFace, NS_ERROR_FAILURE);

    FT_Error status;
    FT_ULong len = 0;
    status = FT_Load_Sfnt_Table(mFTFace, TTAG_cmap, 0, nsnull, &len);
    NS_ENSURE_TRUE(status == 0, NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(len != 0, NS_ERROR_FAILURE);

    nsAutoTArray<PRUint8,16384> buffer;
    if (!buffer.AppendElements(len))
        return NS_ERROR_FAILURE;
    PRUint8 *buf = buffer.Elements();

    status = FT_Load_Sfnt_Table(mFTFace, TTAG_cmap, 0, buf, &len);
    NS_ENSURE_TRUE(status == 0, NS_ERROR_FAILURE);

    PRPackedBool unicodeFont;
    PRPackedBool symbolFont;
    return gfxFontUtils::ReadCMAP(buf, len, mCharacterMap,
                                  unicodeFont, symbolFont);
}

FontEntry *
FontFamily::FindFontEntry(const gfxFontStyle& aFontStyle)
{
    PRBool needsBold = PR_FALSE;
    return static_cast<FontEntry*>(FindFontForStyle(aFontStyle, needsBold));
}

PRBool
FontFamily::FindWeightsForStyle(gfxFontEntry* aFontsForWeights[],
                                PRBool anItalic, PRInt16 aStretch)
{
    PRBool matchesSomething = PR_FALSE;

    for (PRUint32 j = 0; j < 2; j++) {
        // build up an array of weights that match the italicness we're looking for
        for (PRUint32 i = 0; i < mAvailableFonts.Length(); i++) {
            gfxFontEntry *fe = mAvailableFonts[i];
            const PRUint8 weight = (fe->mWeight / 100);
            if (fe->mItalic == anItalic) {
                aFontsForWeights[weight] = fe;
                matchesSomething = PR_TRUE;
            }
        }
        if (matchesSomething)
            break;
        anItalic = !anItalic;
    }

    return matchesSomething;
}

/**
 * gfxFT2FontGroup
 */

PRBool
gfxFT2FontGroup::FontCallback(const nsAString& fontName,
                             const nsACString& genericName,
                             void *closure)
{
    nsTArray<nsString> *sa = static_cast<nsTArray<nsString>*>(closure);

    if (!fontName.IsEmpty() && !sa->Contains(fontName)) {
        sa->AppendElement(fontName);
#ifdef DEBUG_pavlov
        printf(" - %s\n", NS_ConvertUTF16toUTF8(fontName).get());
#endif
    }

    return PR_TRUE;
}

gfxFT2FontGroup::gfxFT2FontGroup(const nsAString& families,
                               const gfxFontStyle *aStyle)
    : gfxFontGroup(families, aStyle)
{
#ifdef DEBUG_pavlov
    printf("Looking for %s\n", NS_ConvertUTF16toUTF8(families).get());
#endif
    nsTArray<nsString> familyArray;
    ForEachFont(FontCallback, &familyArray);

    if (familyArray.Length() == 0) {
        nsAutoString prefFamilies;
        gfxToolkitPlatform::GetPlatform()->GetPrefFonts(aStyle->langGroup.get(), prefFamilies, nsnull);
        if (!prefFamilies.IsEmpty()) {
            ForEachFont(prefFamilies, aStyle->langGroup, FontCallback, &familyArray);
        }
    }
    if (familyArray.Length() == 0) {
#if defined(MOZ_WIDGET_QT) /* FIXME DFB */
        printf("failde to find a font. sadface\n");
        // We want to get rid of this entirely at some point, but first we need real lists of fonts.
        QFont defaultFont;
        QFontInfo fi (defaultFont);
        familyArray.AppendElement(nsDependentString(static_cast<const PRUnichar *>(fi.family().utf16())));
#elif defined(MOZ_WIDGET_GTK2)
        FcResult result;
        FcChar8 *family = nsnull;
        FcPattern* pat = FcPatternCreate();
        FcPattern *match = FcFontMatch(nsnull, pat, &result);
        if (match)
            FcPatternGetString(match, FC_FAMILY, 0, &family);
        if (family)
            familyArray.AppendElement(NS_ConvertUTF8toUTF16((char*)family));
#elif defined(XP_WIN)
        HGDIOBJ hGDI = ::GetStockObject(SYSTEM_FONT);
        LOGFONTW logFont;
        if (hGDI && ::GetObjectW(hGDI, sizeof(logFont), &logFont)) 
            familyArray.AppendElement(nsDependentString(logFont.lfFaceName));
#else
#error "Platform not supported"
#endif
    }

    for (PRUint32 i = 0; i < familyArray.Length(); i++) {
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
     return new gfxFT2FontGroup(mFamilies, aStyle);
}

/**
 * We use this to append an LTR or RTL Override character to the start of the
 * string. This forces Pango to honour our direction even if there are neutral
 * characters in the string.
 */
static PRInt32 AppendDirectionalIndicatorUTF8(PRBool aIsRTL, nsACString& aString)
{
    static const PRUnichar overrides[2][2] = { { 0x202d, 0 }, { 0x202e, 0 }}; // LRO, RLO
    AppendUTF16toUTF8(overrides[aIsRTL], aString);
    return 3; // both overrides map to 3 bytes in UTF8
}

gfxTextRun *gfxFT2FontGroup::MakeTextRun(const PRUnichar* aString, PRUint32 aLength,
                                        const Parameters* aParams, PRUint32 aFlags)
{
    NS_ASSERTION(aLength > 0, "should use MakeEmptyTextRun for zero-length text");
    gfxTextRun *textRun = gfxTextRun::Create(aParams, aString, aLength, this, aFlags);
    if (!textRun)
        return nsnull;

    mString.Assign(nsDependentSubstring(aString, aString + aLength));

    InitTextRun(textRun);

    textRun->FetchGlyphExtents(aParams->mContext);

    return textRun;
}

gfxTextRun *gfxFT2FontGroup::MakeTextRun(const PRUint8 *aString, PRUint32 aLength,
                                        const Parameters *aParams, PRUint32 aFlags)
{
    NS_ASSERTION(aLength > 0, "should use MakeEmptyTextRun for zero-length text");
    NS_ASSERTION(aFlags & TEXT_IS_8BIT, "8bit should have been set");
    gfxTextRun *textRun = gfxTextRun::Create(aParams, aString, aLength, this, aFlags);
    if (!textRun)
        return nsnull;

    const char *chars = reinterpret_cast<const char *>(aString);

    mString.Assign(NS_ConvertASCIItoUTF16(nsDependentCSubstring(chars, chars + aLength)));

    InitTextRun(textRun);

    textRun->FetchGlyphExtents(aParams->mContext);

    return textRun;
}

void gfxFT2FontGroup::InitTextRun(gfxTextRun *aTextRun)
{
    CreateGlyphRunsFT(aTextRun);
}


// Helper function to return the leading UTF-8 character in a char pointer
// as 32bit number. Also sets the length of the current character (i.e. the
// offset to the next one) in the second argument
PRUint32 getUTF8CharAndNext(const PRUint8 *aString, PRUint8 *aLength)
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


static PRBool
AddFontNameToArray(const nsAString& aName,
                   const nsACString& aGenericName,
                   void *aClosure)
{
    if (!aName.IsEmpty()) {
        nsTArray<nsString> *list = static_cast<nsTArray<nsString> *>(aClosure);

        if (list->IndexOf(aName) == list->NoIndex)
            list->AppendElement(aName);
    }
 
    return PR_TRUE;
}
  
void
gfxFT2FontGroup::FamilyListToArrayList(const nsString& aFamilies,
                                       const nsCString& aLangGroup,
                                       nsTArray<nsRefPtr<FontEntry> > *aFontEntryList)
{
    nsAutoTArray<nsString, 15> fonts;
    ForEachFont(aFamilies, aLangGroup, AddFontNameToArray, &fonts);

    PRUint32 len = fonts.Length();
    for (PRUint32 i = 0; i < len; ++i) {
        const nsString& str = fonts[i];
        nsRefPtr<FontEntry> fe = gfxToolkitPlatform::GetPlatform()->FindFontEntry(str, mStyle);
        aFontEntryList->AppendElement(fe);
    }
}

void gfxFT2FontGroup::GetPrefFonts(const char *aLangGroup, nsTArray<nsRefPtr<FontEntry> >& aFontEntryList) {
    NS_ASSERTION(aLangGroup, "aLangGroup is null");
    gfxToolkitPlatform *platform = gfxToolkitPlatform::GetPlatform();
    nsAutoTArray<nsRefPtr<FontEntry>, 5> fonts;
    /* this lookup has to depend on weight and style */
    nsCAutoString key(aLangGroup);
    key.Append("-");
    key.AppendInt(GetStyle()->style);
    key.Append("-");
    key.AppendInt(GetStyle()->weight);
    if (!platform->GetPrefFontEntries(key, &fonts)) {
        nsString fontString;
        platform->GetPrefFonts(aLangGroup, fontString);
        if (fontString.IsEmpty())
            return;

        FamilyListToArrayList(fontString, nsDependentCString(aLangGroup),
                                      &fonts);

        platform->SetPrefFontEntries(key, fonts);
    }
    aFontEntryList.AppendElements(fonts);
}

static PRInt32 GetCJKLangGroupIndex(const char *aLangGroup) {
    PRInt32 i;
    for (i = 0; i < COUNT_OF_CJK_LANG_GROUP; i++) {
        if (!PL_strcasecmp(aLangGroup, sCJKLangGroup[i]))
            return i;
    }
    return -1;
}

// this function assigns to the array passed in.
void gfxFT2FontGroup::GetCJKPrefFonts(nsTArray<nsRefPtr<FontEntry> >& aFontEntryList) {
    gfxToolkitPlatform *platform = gfxToolkitPlatform::GetPlatform();

    nsCAutoString key("x-internal-cjk-");
    key.AppendInt(mStyle.style);
    key.Append("-");
    key.AppendInt(mStyle.weight);

    if (!platform->GetPrefFontEntries(key, &aFontEntryList)) {
        nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
        if (!prefs)
            return;

        nsCOMPtr<nsIPrefBranch> prefBranch;
        prefs->GetBranch(0, getter_AddRefs(prefBranch));
        if (!prefBranch)
            return;

        // Add the CJK pref fonts from accept languages, the order should be same order
        nsCAutoString list;
        nsCOMPtr<nsIPrefLocalizedString> val;
        nsresult rv = prefBranch->GetComplexValue("intl.accept_languages", NS_GET_IID(nsIPrefLocalizedString),
                                                  getter_AddRefs(val));
        if (NS_SUCCEEDED(rv) && val) {
            nsAutoString temp;
            val->ToString(getter_Copies(temp));
            LossyCopyUTF16toASCII(temp, list);
        }
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
                nsCAutoString lang(Substring(start, p));
                lang.CompressWhitespace(PR_FALSE, PR_TRUE);
                PRInt32 index = GetCJKLangGroupIndex(lang.get());
                if (index >= 0)
                    GetPrefFonts(sCJKLangGroup[index], aFontEntryList);
                p++;
            }
        }

        // Add the system locale
#ifdef XP_WIN
        switch (::GetACP()) {
            case 932: GetPrefFonts(CJK_LANG_JA, aFontEntryList); break;
            case 936: GetPrefFonts(CJK_LANG_ZH_CN, aFontEntryList); break;
            case 949: GetPrefFonts(CJK_LANG_KO, aFontEntryList); break;
            // XXX Don't we need to append CJK_LANG_ZH_HK if the codepage is 950?
            case 950: GetPrefFonts(CJK_LANG_ZH_TW, aFontEntryList); break;
        }
#else
        const char *ctype = setlocale(LC_CTYPE, NULL);
        if (ctype) {
            if (!PL_strncasecmp(ctype, "ja", 2)) {
                GetPrefFonts(CJK_LANG_JA, aFontEntryList);
            } else if (!PL_strncasecmp(ctype, "zh_cn", 5)) {
                GetPrefFonts(CJK_LANG_ZH_CN, aFontEntryList);
            } else if (!PL_strncasecmp(ctype, "zh_hk", 5)) {
                GetPrefFonts(CJK_LANG_ZH_HK, aFontEntryList);
            } else if (!PL_strncasecmp(ctype, "zh_tw", 5)) {
                GetPrefFonts(CJK_LANG_ZH_TW, aFontEntryList);
            } else if (!PL_strncasecmp(ctype, "ko", 2)) {
                GetPrefFonts(CJK_LANG_KO, aFontEntryList);
            }
        }
#endif

        // last resort...
        GetPrefFonts(CJK_LANG_JA, aFontEntryList);
        GetPrefFonts(CJK_LANG_KO, aFontEntryList);
        GetPrefFonts(CJK_LANG_ZH_CN, aFontEntryList);
        GetPrefFonts(CJK_LANG_ZH_HK, aFontEntryList);
        GetPrefFonts(CJK_LANG_ZH_TW, aFontEntryList);

        platform->SetPrefFontEntries(key, aFontEntryList);
    }
}

already_AddRefed<gfxFT2Font>
gfxFT2FontGroup::WhichFontSupportsChar(const nsTArray<nsRefPtr<FontEntry> >& aFontEntryList, PRUint32 aCh)
{
    for (PRUint32 i = 0; i < aFontEntryList.Length(); i++) {
        nsRefPtr<FontEntry> fe = aFontEntryList[i];
        if (fe->HasCharacter(aCh)) {
            nsRefPtr<gfxFT2Font> font =
                gfxFT2Font::GetOrMakeFont(fe, &mStyle);
            return font.forget();
        }
    }
    return nsnull;
}

already_AddRefed<gfxFont> 
gfxFT2FontGroup::WhichPrefFontSupportsChar(PRUint32 aCh)
{
    if (aCh > 0xFFFF)
        return nsnull;

    nsRefPtr<gfxFT2Font> selectedFont;

    // check out the style's language group
    nsAutoTArray<nsRefPtr<FontEntry>, 5> fonts;
    GetPrefFonts(mStyle.langGroup.get(), fonts);
    selectedFont = WhichFontSupportsChar(fonts, aCh);

    // otherwise search prefs
    if (!selectedFont) {
        PRUint32 unicodeRange = FindCharUnicodeRange(aCh);

        /* special case CJK */
        if (unicodeRange == kRangeSetCJK) {
            if (PR_LOG_TEST(gFontLog, PR_LOG_DEBUG))
                PR_LOG(gFontLog, PR_LOG_DEBUG, (" - Trying to find fonts for: CJK"));

            nsAutoTArray<nsRefPtr<FontEntry>, 15> fonts;
            GetCJKPrefFonts(fonts);
            selectedFont = WhichFontSupportsChar(fonts, aCh);
        } else {
            const char *langGroup = LangGroupFromUnicodeRange(unicodeRange);
            if (langGroup) {
                PR_LOG(gFontLog, PR_LOG_DEBUG, (" - Trying to find fonts for: %s", langGroup));

                nsAutoTArray<nsRefPtr<FontEntry>, 5> fonts;
                GetPrefFonts(langGroup, fonts);
                selectedFont = WhichFontSupportsChar(fonts, aCh);
            }
        }
    }

    if (selectedFont) {
        nsRefPtr<gfxFont> f = static_cast<gfxFont*>(selectedFont.get());
        return f.forget();
    }

    return nsnull;
}

already_AddRefed<gfxFont> 
gfxFT2FontGroup::WhichSystemFontSupportsChar(PRUint32 aCh)
{
    nsRefPtr<gfxFont> selectedFont;
    nsRefPtr<gfxFT2Font> refFont = GetFontAt(0);
    gfxToolkitPlatform *platform = gfxToolkitPlatform::GetPlatform();
    selectedFont = platform->FindFontForChar(aCh, refFont);
    if (selectedFont)
        return selectedFont.forget();
    return nsnull;
}

void gfxFT2FontGroup::CreateGlyphRunsFT(gfxTextRun *aTextRun)
{
    ComputeRanges(mRanges, mString.get(), 0, mString.Length());

    PRUint32 offset = 0;
    for (PRUint32 i = 0; i < mRanges.Length(); ++i) {
        const gfxTextRange& range = mRanges[i];
        PRUint32 rangeLength = range.Length();
        gfxFT2Font *font = static_cast<gfxFT2Font *>(range.font ? range.font.get() : GetFontAt(0));
        AddRange(aTextRun, font, mString.get(), offset, rangeLength);
        offset += rangeLength;
    }
    
}

void
gfxFT2FontGroup::AddRange(gfxTextRun *aTextRun, gfxFT2Font *font, const PRUnichar *str, PRUint32 offset, PRUint32 len)
{
    const PRUint32 appUnitsPerDevUnit = aTextRun->GetAppUnitsPerDevUnit();
    // we'll pass this in/figure it out dynamically, but at this point there can be only one face.
    FT_Face face = cairo_ft_scaled_font_lock_face(font->CairoScaledFont());

    gfxTextRun::CompressedGlyph g;

    aTextRun->AddGlyphRun(font, offset);
    for (PRUint32 i = 0; i < len; i++) {
        PRUint32 ch = str[offset + i];

        if (ch == 0) {
            // treat this null byte as a missing glyph, don't create a glyph for it
            aTextRun->SetMissingGlyph(offset + i, 0);
            continue;
        }

        NS_ASSERTION(!IsInvalidChar(ch), "Invalid char detected");
        FT_UInt gid = FT_Get_Char_Index(face, ch); // find the glyph id
        PRInt32 advance = 0;

        if (gid == font->GetSpaceGlyph()) {
            advance = (int)(font->GetMetrics().spaceWidth * appUnitsPerDevUnit);
        } else if (gid == 0) {
            advance = -1; // trigger the missing glyphs case below
        } else {
            // find next character and its glyph -- in case they exist
            // and exist in the current font face -- to compute kerning
            PRUint32 chNext = 0;
            FT_UInt gidNext = 0;
            FT_Pos lsbDeltaNext = 0;

            if (FT_HAS_KERNING(face) && i + 1 < len) {
                chNext = str[offset + i + 1];
                if (chNext != 0) {
                    gidNext = FT_Get_Char_Index(face, chNext);
                    if (gidNext && gidNext != font->GetSpaceGlyph()) {
                        FT_Load_Glyph(face, gidNext, FT_LOAD_DEFAULT);
                        lsbDeltaNext = face->glyph->lsb_delta;
                    }
                }
            }

            // now load the current glyph
            FT_Load_Glyph(face, gid, FT_LOAD_DEFAULT); // load glyph into the slot
            advance = face->glyph->advance.x;

            // now add kerning to the current glyph's advance
            if (chNext && gidNext) {
                FT_Vector kerning; kerning.x = 0;
                FT_Get_Kerning(face, gid, gidNext, FT_KERNING_DEFAULT, &kerning);
                advance += kerning.x;
                if (face->glyph->rsb_delta - lsbDeltaNext >= 32) {
                    advance -= 64;
                } else if (face->glyph->rsb_delta - lsbDeltaNext < -32) {
                    advance += 64;
                }
            }

            // now apply unit conversion and scaling
            advance = (advance >> 6) * appUnitsPerDevUnit;
        }
#ifdef DEBUG_thebes_2
        printf(" gid=%d, advance=%d (%s)\n", gid, advance,
               NS_LossyConvertUTF16toASCII(font->GetName()).get());
#endif

        if (advance >= 0 &&
            gfxTextRun::CompressedGlyph::IsSimpleAdvance(advance) &&
            gfxTextRun::CompressedGlyph::IsSimpleGlyphID(gid)) {
            aTextRun->SetSimpleGlyph(offset + i, g.SetSimpleGlyph(advance, gid));
        } else if (gid == 0) {
            // gid = 0 only happens when the glyph is missing from the font
            aTextRun->SetMissingGlyph(offset + i, ch);
        } else {
            gfxTextRun::DetailedGlyph details;
            details.mGlyphID = gid;
            NS_ASSERTION(details.mGlyphID == gid, "Seriously weird glyph ID detected!");
            details.mAdvance = advance;
            details.mXOffset = 0;
            details.mYOffset = 0;
            g.SetComplex(aTextRun->IsClusterStart(offset + i), PR_TRUE, 1);
            aTextRun->SetGlyphs(offset + i, g, &details);
        }
    }

    cairo_ft_scaled_font_unlock_face(font->CairoScaledFont());
}

/**
 * gfxFT2Font
 */
gfxFT2Font::gfxFT2Font(FontEntry *aFontEntry,
                     const gfxFontStyle *aFontStyle)
    : gfxFont(aFontEntry, aFontStyle),
    mScaledFont(nsnull),
    mHasSpaceGlyph(PR_FALSE),
    mSpaceGlyph(0),
    mHasMetrics(PR_FALSE),
    mAdjustedSize(0)
{
    mFontEntry = aFontEntry;
    NS_ASSERTION(mFontEntry, "Unable to find font entry for font.  Something is whack.");
}

gfxFT2Font::~gfxFT2Font()
{
    if (mScaledFont) {
        cairo_scaled_font_destroy(mScaledFont);
        mScaledFont = nsnull;
    }
}

// rounding and truncation functions for a Freetype floating point number 
// (FT26Dot6) stored in a 32bit integer with high 26 bits for the integer
// part and low 6 bits for the fractional part. 
#define MOZ_FT_ROUND(x) (((x) + 32) & ~63) // 63 = 2^6 - 1
#define MOZ_FT_TRUNC(x) ((x) >> 6)
#define CONVERT_DESIGN_UNITS_TO_PIXELS(v, s) \
        MOZ_FT_TRUNC(MOZ_FT_ROUND(FT_MulFix((v) , (s))))

const gfxFont::Metrics&
gfxFT2Font::GetMetrics()
{
    if (mHasMetrics)
        return mMetrics;

    mMetrics.emHeight = GetStyle()->size;

    FT_Face face = cairo_ft_scaled_font_lock_face(CairoScaledFont());

    if (!face) {
        // Abort here already, otherwise we crash in the following
        // this can happen if the font-size requested is zero.
        // The metrics will be incomplete, but then we don't care.
        return mMetrics;
    }

    mMetrics.emHeight = GetStyle()->size;

    FT_UInt gid; // glyph ID

    const double emUnit = 1.0 * face->units_per_EM;
    const double xScale = face->size->metrics.x_ppem / emUnit;
    const double yScale = face->size->metrics.y_ppem / emUnit;

    // properties of space
    gid = FT_Get_Char_Index(face, ' ');
    if (gid) {
        FT_Load_Glyph(face, gid, FT_LOAD_DEFAULT);
        // face->glyph->metrics.width doesn't work for spaces, use advance.x instead
        mMetrics.spaceWidth = face->glyph->advance.x >> 6;
        // save the space glyph
        mSpaceGlyph = gid;
    } else {
        NS_ERROR("blah");
    }
            
    // properties of 'x', also use its width as average width
    gid = FT_Get_Char_Index(face, 'x'); // select the glyph
    if (gid) {
        // Load glyph into glyph slot. Here, use no_scale to get font units.
        FT_Load_Glyph(face, gid, FT_LOAD_NO_SCALE);
        mMetrics.xHeight = face->glyph->metrics.height * yScale;
        mMetrics.aveCharWidth = face->glyph->metrics.width * xScale;
    } else {
        // this font doesn't have an 'x'...
        // fake these metrics using a fraction of the font size
        mMetrics.xHeight = mMetrics.emHeight * 0.5;
        mMetrics.aveCharWidth = mMetrics.emHeight * 0.5;
    }

    // compute an adjusted size if we need to
    if (mAdjustedSize == 0 && GetStyle()->sizeAdjust != 0) {
        gfxFloat aspect = mMetrics.xHeight / GetStyle()->size;
        mAdjustedSize = GetStyle()->GetAdjustedSize(aspect);
        mMetrics.emHeight = mAdjustedSize;
    }

    // now load the OS/2 TrueType table to load access some more properties
    TT_OS2 *os2 = (TT_OS2 *)FT_Get_Sfnt_Table(face, ft_sfnt_os2);
    if (os2 && os2->version != 0xFFFF) { // should be there if not old Mac font
        // if we are here we can improve the avgCharWidth
        mMetrics.aveCharWidth = os2->xAvgCharWidth * xScale;

        mMetrics.superscriptOffset = os2->ySuperscriptYOffset * yScale;
        mMetrics.superscriptOffset = PR_MAX(1, mMetrics.superscriptOffset);
        // some fonts have the incorrect sign (from gfxPangoFonts)
        mMetrics.subscriptOffset   = fabs(os2->ySubscriptYOffset * yScale);
        mMetrics.subscriptOffset   = PR_MAX(1, fabs(mMetrics.subscriptOffset));
        mMetrics.strikeoutOffset   = os2->yStrikeoutPosition * yScale;
        mMetrics.strikeoutSize     = os2->yStrikeoutSize * yScale;
    } else {
        // use fractions of emHeight instead of xHeight for these to be more robust
        mMetrics.superscriptOffset = mMetrics.emHeight * 0.5;
        mMetrics.subscriptOffset   = mMetrics.emHeight * 0.2;
        mMetrics.strikeoutOffset   = mMetrics.emHeight * 0.3;
        mMetrics.strikeoutSize     = face->underline_thickness * yScale;
    }
    // seems that underlineOffset really has to be negative
    mMetrics.underlineOffset = face->underline_position * yScale;
    mMetrics.underlineSize   = face->underline_thickness * yScale;

    // descents are negative in FT but Thebes wants them positive
    mMetrics.emAscent        = face->ascender * yScale;
    mMetrics.emDescent       = -face->descender * yScale;
    mMetrics.maxHeight       = face->height * yScale;
    mMetrics.maxAscent       = face->bbox.yMax * yScale;
    mMetrics.maxDescent      = -face->bbox.yMin * yScale;
    mMetrics.maxAdvance      = face->max_advance_width * xScale;
    // leading are not available directly (only for WinFNTs)
    double lineHeight = mMetrics.maxAscent + mMetrics.maxDescent;
    if (lineHeight > mMetrics.emHeight) {
        mMetrics.internalLeading = lineHeight - mMetrics.emHeight;
    } else {
        mMetrics.internalLeading = 0;
    }
    mMetrics.externalLeading = 0; // normal value for OS/2 fonts, too

    SanitizeMetrics(&mMetrics, PR_FALSE);

    /*
    printf("gfxOS2Font[%#x]::GetMetrics():\n"
           "  emHeight=%f == %f=gfxFont::style.size == %f=adjSz\n"
           "  maxHeight=%f  xHeight=%f\n"
           "  aveCharWidth=%f==xWidth  spaceWidth=%f\n"
           "  supOff=%f SubOff=%f   strOff=%f strSz=%f\n"
           "  undOff=%f undSz=%f    intLead=%f extLead=%f\n"
           "  emAsc=%f emDesc=%f maxH=%f\n"
           "  maxAsc=%f maxDes=%f maxAdv=%f\n",
           (unsigned)this,
           mMetrics.emHeight, GetStyle()->size, mAdjustedSize,
           mMetrics.maxHeight, mMetrics.xHeight,
           mMetrics.aveCharWidth, mMetrics.spaceWidth,
           mMetrics.superscriptOffset, mMetrics.subscriptOffset,
           mMetrics.strikeoutOffset, mMetrics.strikeoutSize,
           mMetrics.underlineOffset, mMetrics.underlineSize,
           mMetrics.internalLeading, mMetrics.externalLeading,
           mMetrics.emAscent, mMetrics.emDescent, mMetrics.maxHeight,
           mMetrics.maxAscent, mMetrics.maxDescent, mMetrics.maxAdvance
          );
    */

    // XXX mMetrics.height needs to be set.
    cairo_ft_scaled_font_unlock_face(CairoScaledFont());

    mHasMetrics = PR_TRUE;
    return mMetrics;
}


nsString
gfxFT2Font::GetUniqueName()
{
    return GetName();
}

PRUint32
gfxFT2Font::GetSpaceGlyph()
{
    NS_ASSERTION (GetStyle ()->size != 0,
    "forgot to short-circuit a text run with zero-sized font?");

    if(!mHasSpaceGlyph)
    {
        FT_UInt gid = 0; // glyph ID
        FT_Face face = cairo_ft_scaled_font_lock_face(CairoScaledFont());
        gid = FT_Get_Char_Index(face, ' ');
        FT_Load_Glyph(face, gid, FT_LOAD_DEFAULT);
        mSpaceGlyph = gid;
        mHasSpaceGlyph = PR_TRUE;
        cairo_ft_scaled_font_unlock_face(CairoScaledFont());
    }
    return mSpaceGlyph;
}

cairo_font_face_t *
gfxFT2Font::CairoFontFace()
{
    // XXX we need to handle fake bold here (or by having a sepaerate font entry)
    if (mStyle.weight >= 600 && mFontEntry->mWeight < 600) {
        //printf("** We want fake weight\n");
    }
    return GetFontEntry()->CairoFontFace();
}

cairo_scaled_font_t *
gfxFT2Font::CairoScaledFont()
{
    if (!mScaledFont) {
        cairo_matrix_t sizeMatrix;
        cairo_matrix_t identityMatrix;

        // XXX deal with adjusted size
        cairo_matrix_init_scale(&sizeMatrix, mStyle.size, mStyle.size);
        cairo_matrix_init_identity(&identityMatrix);

        // synthetic oblique by skewing via the font matrix
        PRBool needsOblique = (!mFontEntry->mItalic && (mStyle.style & (FONT_STYLE_ITALIC | FONT_STYLE_OBLIQUE)));

        if (needsOblique) {
            const double kSkewFactor = 0.25;

            cairo_matrix_t style;
            cairo_matrix_init(&style,
                              1,                //xx
                              0,                //yx
                              -1 * kSkewFactor,  //xy
                              1,                //yy
                              0,                //x0
                              0);               //y0
            cairo_matrix_multiply(&sizeMatrix, &sizeMatrix, &style);
        }

        cairo_font_options_t *fontOptions = cairo_font_options_create();
        mScaledFont = cairo_scaled_font_create(CairoFontFace(), &sizeMatrix,
                                               &identityMatrix, fontOptions);
        cairo_font_options_destroy(fontOptions);
    }

    NS_ASSERTION(mAdjustedSize == 0.0 ||
                 cairo_scaled_font_status(mScaledFont) == CAIRO_STATUS_SUCCESS,
                 "Failed to make scaled font");

    return mScaledFont;
}

PRBool
gfxFT2Font::SetupCairoFont(gfxContext *aContext)
{
    cairo_scaled_font_t *scaledFont = CairoScaledFont();

    if (cairo_scaled_font_status(scaledFont) != CAIRO_STATUS_SUCCESS) {
        // Don't cairo_set_scaled_font as that would propagate the error to
        // the cairo_t, precluding any further drawing.
        return PR_FALSE;
    }
    cairo_set_scaled_font(aContext->GetCairo(), scaledFont);
    return PR_TRUE;
}

/**
 * Look up the font in the gfxFont cache. If we don't find it, create one.
 * In either case, add a ref, append it to the aFonts array, and return it ---
 * except for OOM in which case we do nothing and return null.
 */
already_AddRefed<gfxFT2Font>
gfxFT2Font::GetOrMakeFont(const nsAString& aName, const gfxFontStyle *aStyle)
{
    FontEntry *fe = gfxToolkitPlatform::GetPlatform()->FindFontEntry(aName, *aStyle);
    if (!fe) {
        printf("Failed to find font entry for %s\n", NS_ConvertUTF16toUTF8(aName).get());
        return nsnull;
    }

    nsRefPtr<gfxFT2Font> font = GetOrMakeFont(fe, aStyle);
    return font.forget();
}

already_AddRefed<gfxFT2Font>
gfxFT2Font::GetOrMakeFont(FontEntry *aFontEntry, const gfxFontStyle *aStyle)
{
    nsRefPtr<gfxFont> font = gfxFontCache::GetCache()->Lookup(aFontEntry->Name(), aStyle);
    if (!font) {
        font = new gfxFT2Font(aFontEntry, aStyle);
        if (!font)
            return nsnull;
        gfxFontCache::GetCache()->AddNew(font);
    }
    gfxFont *f = nsnull;
    font.swap(f);
    return static_cast<gfxFT2Font *>(f);
}

