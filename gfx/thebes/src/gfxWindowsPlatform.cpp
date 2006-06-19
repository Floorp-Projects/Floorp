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
 *   Stuart Parmenter <stuart@mozilla.com>
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

#include "gfxWindowsPlatform.h"

#include "gfxImageSurface.h"
#include "gfxWindowsSurface.h"

#include "nsUnicharUtils.h"

#include "nsIPref.h"
#include "nsServiceManagerUtils.h"

#include <string>

gfxWindowsPlatform::gfxWindowsPlatform()
{
    mFonts.Init(200);
    mFontWeights.Init(200);
    UpdateFontList();
}

gfxWindowsPlatform::~gfxWindowsPlatform()
{
}

already_AddRefed<gfxASurface>
gfxWindowsPlatform::CreateOffscreenSurface(PRUint32 width,
                                           PRUint32 height,
                                           gfxASurface::gfxImageFormat imageFormat)
{
    gfxASurface *surf = new gfxWindowsSurface(nsnull, width, height, imageFormat);
    NS_IF_ADDREF(surf);
    return surf;
}

int CALLBACK 
gfxWindowsPlatform::FontEnumProc(const ENUMLOGFONTEXW *lpelfe,
                                 const NEWTEXTMETRICEXW *nmetrics,
                                 DWORD fontType, LPARAM data)
{
    gfxWindowsPlatform *thisp = reinterpret_cast<gfxWindowsPlatform*>(data);

    const LOGFONTW& logFont = lpelfe->elfLogFont;
    const NEWTEXTMETRICW& metrics = nmetrics->ntmTm;

    // Ignore vertical fonts
    if (logFont.lfFaceName[0] == L'@') {
        return 1;
    }

    nsString name(logFont.lfFaceName);
    ToLowerCase(name);

    nsRefPtr<FontEntry> fe;
    if (!thisp->mFonts.Get(name, &fe)) {
        fe = new FontEntry(nsDependentString(logFont.lfFaceName), (PRUint16)fontType);
        thisp->mFonts.Put(name, fe);
    }

    // mark the charset bit
    fe->mCharset[metrics.tmCharSet] = 1;

    // put this in the weight table
    nsRefPtr<WeightTable> wt;
    if (!thisp->mFontWeights.Get(name, &wt)) {
        wt = new WeightTable();
        wt->SetWeight(PR_MAX(1, PR_MIN(9, metrics.tmWeight / 100)), PR_TRUE);
        thisp->mFontWeights.Put(name, wt);
    }

    fe->mFamily = logFont.lfPitchAndFamily & 0xF0;
    fe->mPitch = logFont.lfPitchAndFamily & 0x0F;

    // set the unicode ranges
    PRUint32 x = 0;
    for (PRUint32 i = 0; i < 4; ++i) {
        DWORD range = nmetrics->ntmFontSig.fsUsb[i];
        for (PRUint32 k = 0; k < 32; ++k) {
            fe->mUnicodeRanges[x++] = (range & (1 << k)) != 0;
        }
    }

    return 1;
}

struct FontListData {
    FontListData(const nsACString& aLangGroup, const nsACString& aGenericFamily, nsStringArray& aListOfFonts) :
        mLangGroup(aLangGroup), mGenericFamily(aGenericFamily), mStringArray(aListOfFonts) {}
    const nsACString& mLangGroup;
    const nsACString& mGenericFamily;
    nsStringArray& mStringArray;
};

PLDHashOperator PR_CALLBACK
gfxWindowsPlatform::HashEnumFunc(nsStringHashKey::KeyType aKey,
                                 nsRefPtr<FontEntry>& aFontEntry,
                                 void* userArg)
{
    FontListData *data = (FontListData*)userArg;

    if (aFontEntry->SupportsLangGroup(data->mLangGroup) &&
        aFontEntry->MatchesGenericFamily(data->mGenericFamily))
        data->mStringArray.AppendString(aFontEntry->mName);

    return PL_DHASH_NEXT;
}

nsresult
gfxWindowsPlatform::GetFontList(const nsACString& aLangGroup,
                                const nsACString& aGenericFamily,
                                nsStringArray& aListOfFonts)
{
    FontListData data(aLangGroup, aGenericFamily, aListOfFonts);

    mFonts.Enumerate(gfxWindowsPlatform::HashEnumFunc, &data);

    aListOfFonts.Sort();
    aListOfFonts.Compact();

    return NS_OK;
}

nsresult
gfxWindowsPlatform::UpdateFontList()
{
    mFonts.Clear();

    LOGFONTW logFont;
    logFont.lfCharSet = DEFAULT_CHARSET;
    logFont.lfFaceName[0] = 0;
    logFont.lfPitchAndFamily = 0;

    // Use the screen DC here.. should we use something else for printing?
    HDC dc = ::GetDC(nsnull);
    EnumFontFamiliesExW(dc, &logFont, (FONTENUMPROCW)gfxWindowsPlatform::FontEnumProc, (LPARAM)this, 0);
    ::ReleaseDC(nsnull, dc);

    return NS_OK;
}

struct FontMatch {
    FontMatch() : rank(0) {}
    PRBool operator <(const FontMatch& other) const { return (rank < other.rank); }
    PRBool operator >(const FontMatch& other) const { return (rank > other.rank); }
    PRBool operator ==(const FontMatch& other) const { return (rank == other.rank); }
    nsRefPtr<FontEntry> fontEntry;
    PRInt8 rank;
};

struct FontSearch {
    FontSearch(PRUnichar aCh, PRUint8 aRange, const char *aLangGroup, const char *aFamily) :
        ch(aCh), langGroup(aLangGroup), family(aFamily), range(aRange), highestRank(0), fontMatches(25) {
    }
    PRBool RankIsOK(PRUint32 rank) {
        return (rank >= (highestRank / 2) + 1);
    }
    const PRUint32 ch;
    const char *langGroup;
    const char *family;
    const PRUint8 range;
    PRInt8 highestRank;
    nsTArray<FontMatch> fontMatches;
};

PLDHashOperator PR_CALLBACK
gfxWindowsPlatform::FindFontForChar(nsStringHashKey::KeyType aKey,
                                    nsRefPtr<FontEntry>& aFontEntry,
                                    void* userArg)
{
    // bitmap fonts suck
    if (aFontEntry->IsCrappyFont())
        return PL_DHASH_NEXT;

    FontSearch *data = (FontSearch*)userArg;
    FontMatch fm;
    if (aFontEntry->SupportsRange(data->range))
        fm.rank += 20;

    if (aFontEntry->SupportsLangGroup(nsDependentCString(data->langGroup)))
        fm.rank += 10;

    if (data->family && aFontEntry->MatchesGenericFamily(nsDependentCString(data->family)))
        fm.rank += 5;

    // XXX this code doesn't really work like I would hope
    // we really just want to avoid non-unicode fonts.. i.e. wingdings, etc
    // find something better to replace it with
    /* rank symbol fonts lower than other stuff -- this might be a bad idea, but should
     * avoid things like wingdings showing up while rendering hindi scripts
     */
    //    if (aFontEntry->SupportsLangGroup(NS_LITERAL_CSTRING("x-symbol")))
    //        fm.rank -= 5;

    /* This will allow us to cut out some of the fonts, but not all
     * since some might get in early before we find the real highest rank.
     */
    if (fm.rank > data->highestRank)
        data->highestRank = fm.rank;

    if (!data->RankIsOK(fm.rank))
        return PL_DHASH_NEXT;

    if (fm.rank > 0) {
        fm.fontEntry = aFontEntry;
        data->fontMatches.AppendElement(fm);
    }

    return PL_DHASH_NEXT;
}

static void
AppendGenericFontFromPref(nsString& aFonts, const char *aLangGroup, const char *aGenericName)
{
    nsresult rv;

    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
    if (!prefs)
        return;

    nsCAutoString prefName;
    nsXPIDLString value;

    nsXPIDLString genericName;
    if (aGenericName) {
        genericName = NS_ConvertASCIItoUTF16(aGenericName);
    } else {
        prefName.AssignLiteral("font.default.");
        prefName.Append(aLangGroup);
        prefs->CopyUnicharPref(prefName.get(), getter_Copies(genericName));
    }

    nsCAutoString genericDotLang;
    genericDotLang.Assign(NS_ConvertUTF16toUTF8(genericName));
    genericDotLang.AppendLiteral(".");
    genericDotLang.Append(aLangGroup);

    prefName.AssignLiteral("font.name.");
    prefName.Append(genericDotLang);
    rv = prefs->CopyUnicharPref(prefName.get(), getter_Copies(value));
    if (NS_SUCCEEDED(rv)) {
        if (!aFonts.IsEmpty())
            aFonts.AppendLiteral(", ");
        aFonts.Append(value);
    }

    prefName.AssignLiteral("font.name-list.");
    prefName.Append(genericDotLang);
    rv = prefs->CopyUnicharPref(prefName.get(), getter_Copies(value));
    if (NS_SUCCEEDED(rv)) {
        if (!aFonts.IsEmpty())
            aFonts.AppendLiteral(", ");
        aFonts.Append(value);
    }
}

void
gfxWindowsPlatform::GetPrefFonts(const char *aLangGroup, nsString& aFonts)
{
    aFonts.Truncate();

    AppendGenericFontFromPref(aFonts, aLangGroup, nsnull);
    AppendGenericFontFromPref(aFonts, "x-unicode", nsnull);
}

void
gfxWindowsPlatform::FindOtherFonts(const PRUnichar* aString, PRUint32 aLength, const char *aLangGroup, const char *aGeneric, nsString& fonts)
{
    fonts.Truncate();

    PRBool surrogates = PR_FALSE;

    std::bitset<128> ranges(0);

    for (PRUint32 z = 0; z < aLength; ++z) {
        PRUint32 ch = aString[z];

        if ((z+1 < aLength) && IS_HIGH_SURROGATE(ch) && IS_LOW_SURROGATE(aString[z+1])) {
            z++;
            ch = SURROGATE_TO_UCS4(ch, aString[z]);
            surrogates = PR_TRUE;
        }

        PRUint8 range = CharRangeBit(ch);
        if (!ranges[range]) {
            FontSearch data(ch, CharRangeBit(ch), aLangGroup, aGeneric);

            mFonts.Enumerate(gfxWindowsPlatform::FindFontForChar, &data);
            
            data.fontMatches.Sort();

            PRUint32 nmatches = data.fontMatches.Length();
            if (nmatches > 0) {
                //printf("%d matches for 0x%04x\n", nmatches, ch);
                for (PRUint32 i = nmatches - 1; i > 0; i--) {
                    const FontMatch& fm = data.fontMatches[i];
                    if (data.RankIsOK(fm.rank)) {
                        if (!fonts.IsEmpty())
                            fonts.AppendLiteral(", ");
                        fonts.Append(fm.fontEntry->mName);
                    }
                }
            }
            ranges[range] = PR_TRUE;
        }
    }


    if (surrogates) {
        // append fonts that support surrogates on to the list
        FontSearch data(0xd801, 57, aLangGroup, aGeneric);

        mFonts.Enumerate(gfxWindowsPlatform::FindFontForChar, &data);

        data.fontMatches.Sort();

        PRUint32 nmatches = data.fontMatches.Length();
        if (nmatches > 0) {
            for (PRUint32 i = nmatches - 1; i > 0; i--) {
                const FontMatch& fm = data.fontMatches[i];
                if (data.RankIsOK(fm.rank)) {
                    if (!fonts.IsEmpty())
                        fonts.AppendLiteral(", ");
                    fonts.Append(fm.fontEntry->mName);
                }
            }
        }
    }
}

WeightTable *
gfxWindowsPlatform::GetFontWeightTable(const nsAString& aName)
{
    nsRefPtr<WeightTable> wt;
    if (!mFontWeights.Get(aName, &wt)) {
        return nsnull;
    }
    return wt;
}

void
gfxWindowsPlatform::PutFontWeightTable(const nsAString& aName, WeightTable *aWeightTable)
{
    mFontWeights.Put(aName, aWeightTable);
}
