/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsThebesFontEnumerator.h"

#include "nsMemory.h"

#include "gfxPlatform.h"
#include "nsTArray.h"
#include "nsIAtom.h"

NS_IMPL_ISUPPORTS1(nsThebesFontEnumerator, nsIFontEnumerator)

nsThebesFontEnumerator::nsThebesFontEnumerator()
{
}

NS_IMETHODIMP
nsThebesFontEnumerator::EnumerateAllFonts(PRUint32 *aCount,
                                          PRUnichar ***aResult)
{
    return EnumerateFonts (nsnull, nsnull, aCount, aResult);
}

NS_IMETHODIMP
nsThebesFontEnumerator::EnumerateFonts(const char *aLangGroup,
                                       const char *aGeneric,
                                       PRUint32 *aCount,
                                       PRUnichar ***aResult)
{
    NS_ENSURE_ARG_POINTER(aCount);
    NS_ENSURE_ARG_POINTER(aResult);

    nsTArray<nsString> fontList;

    nsCAutoString generic;
    if (aGeneric)
        generic.Assign(aGeneric);
    else
        generic.SetIsVoid(true);

    nsCOMPtr<nsIAtom> langGroupAtom;
    if (aLangGroup) {
        nsCAutoString lowered;
        lowered.Assign(aLangGroup);
        ToLowerCase(lowered);
        langGroupAtom = do_GetAtom(lowered);
    }

    nsresult rv = gfxPlatform::GetPlatform()->GetFontList(langGroupAtom, generic, fontList);

    if (NS_FAILED(rv)) {
        *aCount = 0;
        *aResult = nsnull;
        /* XXX in this case, do we want to return the CSS generics? */
        return NS_OK;
    }

    PRUnichar **fs = static_cast<PRUnichar **>
                                (nsMemory::Alloc(fontList.Length() * sizeof(PRUnichar*)));
    for (PRUint32 i = 0; i < fontList.Length(); i++) {
        fs[i] = ToNewUnicode(fontList[i]);
    }

    *aResult = fs;
    *aCount = fontList.Length();

    return NS_OK;
}

NS_IMETHODIMP
nsThebesFontEnumerator::HaveFontFor(const char *aLangGroup,
                                    bool *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);

    *aResult = true;
    return NS_OK;
}

NS_IMETHODIMP
nsThebesFontEnumerator::GetDefaultFont(const char *aLangGroup,
                                       const char *aGeneric,
                                       PRUnichar **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsThebesFontEnumerator::UpdateFontList(bool *_retval)
{
    gfxPlatform::GetPlatform()->UpdateFontList();
    *_retval = false; // always return false for now
    return NS_OK;
}

NS_IMETHODIMP
nsThebesFontEnumerator::GetStandardFamilyName(const PRUnichar *aName,
                                              PRUnichar **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    NS_ENSURE_ARG_POINTER(aName);

    nsAutoString name(aName);
    if (name.IsEmpty()) {
        *aResult = nsnull;
        return NS_OK;
    }

    nsAutoString family;
    nsresult rv = gfxPlatform::GetPlatform()->
        GetStandardFamilyName(nsDependentString(aName), family);
    if (NS_FAILED(rv) || family.IsEmpty()) {
        *aResult = nsnull;
        return NS_OK;
    }
    *aResult = ToNewUnicode(family);
    return NS_OK;
}
