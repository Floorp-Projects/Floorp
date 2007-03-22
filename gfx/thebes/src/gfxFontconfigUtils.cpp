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
 * The Original Code is Mozilla Japan code.
 *
 * The Initial Developer of the Original Code is Mozilla Japan.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Masayuki Nakano <masayuki@d-toybox.com>
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

#include "gfxFontconfigUtils.h"

#include <fontconfig/fontconfig.h>

#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsServiceManagerUtils.h"

#include "nsIAtom.h"
#include "nsCRT.h"

/* static */ gfxFontconfigUtils* gfxFontconfigUtils::sUtils = nsnull;

gfxFontconfigUtils::gfxFontconfigUtils()
{
    mAliasTable.Init(50);
    UpdateFontListInternal(PR_TRUE);
}

nsresult
gfxFontconfigUtils::GetFontList(const nsACString& aLangGroup,
                                const nsACString& aGenericFamily,
                                nsStringArray& aListOfFonts)
{
    aListOfFonts.Clear();

    nsresult rv = UpdateFontListInternal();
    if (NS_FAILED(rv))
        return rv;

    nsCStringArray tmpFonts;
    nsCStringArray *fonts = &mFonts;
    if (!aLangGroup.IsEmpty() || !aGenericFamily.IsEmpty()) {
        rv = GetFontListInternal(tmpFonts, &aLangGroup);
        if (NS_FAILED(rv))
            return rv;
        fonts = &tmpFonts;
    }

    for (PRInt32 i = 0; i < fonts->Count(); ++i)
         aListOfFonts.AppendString(NS_ConvertUTF8toUTF16(*fonts->CStringAt(i)));

    PRInt32 serif = 0, sansSerif = 0, monospace = 0, nGenerics;

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
    nGenerics = serif + sansSerif + monospace;

    if (serif)
        aListOfFonts.AppendString(NS_LITERAL_STRING("serif"));
    if (sansSerif)
        aListOfFonts.AppendString(NS_LITERAL_STRING("sans-serif"));
    if (monospace)
        aListOfFonts.AppendString(NS_LITERAL_STRING("monospace"));

    aListOfFonts.Sort();

    return NS_OK;
}

// this is in nsFontConfigUtils.h
extern void NS_AddLangGroup (FcPattern *aPattern, nsIAtom *aLangGroup);

nsresult
gfxFontconfigUtils::GetFontListInternal(nsCStringArray& aListOfFonts,
                                        const nsACString *aLangGroup)
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
    if (aLangGroup && !aLangGroup->IsEmpty()) {
        nsCOMPtr<nsIAtom> langAtom = do_GetAtom(*aLangGroup);
        //XXX fix me //NS_AddLangGroup(pat, langAtom);
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
        nsCAutoString strFamily(family);
        if (aListOfFonts.IndexOf(strFamily) >= 0)
            continue;

        aListOfFonts.AppendCString(strFamily);
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
    return UpdateFontListInternal(PR_TRUE);
}

nsresult
gfxFontconfigUtils::UpdateFontListInternal(PRBool aForce)
{
    if (!aForce && FcConfigUptoDate(NULL))
        return NS_OK;

    FcInitReinitialize();

    mFonts.Clear();
    mAliasForSingleFont.Clear();
    mAliasForMultiFonts.Clear();
    mNonExistingFonts.Clear();

    mAliasTable.Clear();

    nsresult rv = GetFontListInternal(mFonts);
    if (NS_FAILED(rv))
        return rv;

    // XXX we don't support all alias names.
    // Because if we don't check whether the given font name is alias name,
    // fontconfig converts the non existing font to sans-serif.
    // This is not good if the web page specifies font-family
    // that has Windows font name in the first.
    nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (!prefs)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIPrefBranch> prefBranch;
    prefs->GetBranch(0, getter_AddRefs(prefBranch));
    if (!prefBranch)
        return NS_ERROR_FAILURE;

    nsXPIDLCString list;
    rv = prefBranch->GetCharPref("font.alias-list", getter_Copies(list));
    if (NS_FAILED(rv))
        return NS_OK;

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
            nsCAutoString name(Substring(start, p));
            name.CompressWhitespace(PR_FALSE, PR_TRUE);
            mAliasForMultiFonts.AppendCString(name);
            p++;
        }
    }

    if (mAliasForMultiFonts.Count() == 0)
        return NS_OK;

    for (PRInt32 i = 0; i < mAliasForMultiFonts.Count(); i++) {
        nsRefPtr<gfxFontNameList> fonts = new gfxFontNameList;
        nsCAutoString fontname(*mAliasForMultiFonts.CStringAt(i));
        rv = GetResolvedFonts(fontname, fonts);
        if (NS_FAILED(rv))
            return rv;

        nsCAutoString key;
        ToLowerCase(fontname, key);
        mAliasTable.Put(key, fonts);
    }
    return NS_OK;
}

nsresult
gfxFontconfigUtils::GetResolvedFonts(const nsACString& aName,
                                     gfxFontNameList* aResult)
{
    FcPattern *pat = NULL;
    FcFontSet *fs = NULL;
    FcResult fresult;
    aResult->Clear();
    nsresult rv = NS_ERROR_FAILURE;

    pat = FcPatternCreate();
    if (!pat)
        goto end;

    FcDefaultSubstitute(pat);
    FcPatternAddString(pat, FC_FAMILY,
                       (FcChar8 *)nsPromiseFlatCString(aName).get());
    // Delete the lang param. We need lang independent alias list.
    FcPatternDel(pat, FC_LANG);
    FcConfigSubstitute(NULL, pat, FcMatchPattern);

    fs = FcFontSort(NULL, pat, FcTrue, NULL, &fresult);
    if (!fs)
        goto end;

    rv = NS_OK;
    for (int i = 0; i < fs->nfont; i++) {
        char *family;

        if (FcPatternGetString(fs->fonts[i], FC_FAMILY, 0,
                               (FcChar8 **) &family) != FcResultMatch ||
            mAliasForMultiFonts.IndexOfIgnoreCase(nsDependentCString(family)) >= 0 ||
            IsExistingFont(nsDependentCString(family)) == 0)
        {
            continue;
        }
        NS_ConvertUTF8toUTF16 actualName(family);
        if (aResult->Exists(actualName))
            continue;
        aResult->AppendElement(actualName);
    }

  end:
    if (pat)
        FcPatternDestroy(pat);
    if (fs)
        FcFontSetDestroy(fs);
    return rv;
}

nsresult
gfxFontconfigUtils::ResolveFontName(const nsAString& aFontName,
                                    gfxPlatform::FontResolverCallback aCallback,
                                    void *aClosure,
                                    PRBool& aAborted)
{
    aAborted = PR_FALSE;

    nsresult rv = UpdateFontListInternal();
    if (NS_FAILED(rv))
        return rv;

    NS_ConvertUTF16toUTF8 fontname(aFontName);
    if (mAliasForMultiFonts.IndexOfIgnoreCase(fontname) >= 0) {
        nsCAutoString key;
        ToLowerCase(fontname, key);
        nsRefPtr<gfxFontNameList> fonts;
        if (!mAliasTable.Get(key, &fonts))
            NS_ERROR("The mAliasTable was broken!");
        for (PRUint32 i = 0; i < fonts->Length(); i++) {
            aAborted = !(*aCallback)(fonts->ElementAt(i), aClosure);
            if (aAborted)
                break;
        }
    } else {
        PRInt32 result = IsExistingFont(fontname);
        if (result < 0)
            return NS_ERROR_FAILURE;

        if (result > 0)
            aAborted = !(*aCallback)(aFontName, aClosure);
    }

    return NS_OK;
}

PRInt32
gfxFontconfigUtils::IsExistingFont(const nsACString &aFontName)
{
    // Very many sites may specify the font-family only for Windows and Mac.
    // We should check negative cache at first.
    if (mNonExistingFonts.IndexOf(aFontName) >= 0)
        return 0;
    if (mAliasForSingleFont.IndexOf(aFontName) >= 0)
        return 1;
    if (mFonts.IndexOf(aFontName) >= 0)
        return 1;

    // XXX Sometimes, the font has two or more names (e.g., "Sazanami Gothic"
    // has Japanese localized name). The another name doesn't including the
    // cache. Therefore, we need to check the name.
    // But we don't need to resolve the name. Because both names are not same
    // behavior. E.g., the default settings of "Sazanami" on Fedora Core 5,
    // the non-localized name uses Anti-alias, but the localized name uses it.
    // So, we should check just whether the font is existing, don't resolve
    // to regular name.

    FcPattern *pat = NULL;
    FcObjectSet *os = NULL;
    FcFontSet *fs = NULL;
    PRInt32 result = -1;

    pat = FcPatternCreate();
    if (!pat)
        goto end;

    FcPatternAddString(pat, FC_FAMILY,
                       (FcChar8 *)nsPromiseFlatCString(aFontName).get());

    os = FcObjectSetBuild(FC_FAMILY, NULL);
    if (!os)
        goto end;

    fs = FcFontList(NULL, pat, os);
    if (!fs)
        goto end;

    result = fs->nfont;
    NS_ASSERTION(result == 0 || result == 1, "What's this case?");

    if (result > 0)
        mAliasForSingleFont.AppendCString(aFontName);
    else
        mNonExistingFonts.AppendCString(aFontName);

  end:
    if (pat)
        FcPatternDestroy(pat);
    if (os)
        FcObjectSetDestroy(os);
    if (fs)
        FcFontSetDestroy(fs);
    return result;
}

PRBool
gfxFontNameList::Exists(nsAString& aName) {
    for (PRUint32 i = 0; i < Length(); i++) {
        if (aName.Equals(ElementAt(i)))
            return PR_TRUE;
    }
    return PR_FALSE;
}

