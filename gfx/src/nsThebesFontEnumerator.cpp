/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsThebesFontEnumerator.h"
#include <stdint.h>                     // for uint32_t
#include "gfxPlatform.h"                // for gfxPlatform
#include "mozilla/Assertions.h"         // for MOZ_ASSERT_HELPER2
#include "nsCOMPtr.h"                   // for nsCOMPtr
#include "nsDebug.h"                    // for NS_ENSURE_ARG_POINTER
#include "nsError.h"                    // for NS_OK, NS_FAILED, nsresult
#include "nsIAtom.h"                    // for nsIAtom, do_GetAtom
#include "nsID.h"
#include "nsMemory.h"                   // for nsMemory
#include "nsString.h"               // for nsAutoCString, nsAutoString, etc
#include "nsTArray.h"                   // for nsTArray, nsTArray_Impl, etc
#include "nscore.h"                     // for char16_t, NS_IMETHODIMP

NS_IMPL_ISUPPORTS(nsThebesFontEnumerator, nsIFontEnumerator)

nsThebesFontEnumerator::nsThebesFontEnumerator()
{
}

NS_IMETHODIMP
nsThebesFontEnumerator::EnumerateAllFonts(uint32_t *aCount,
                                          char16_t ***aResult)
{
    return EnumerateFonts (nullptr, nullptr, aCount, aResult);
}

NS_IMETHODIMP
nsThebesFontEnumerator::EnumerateFonts(const char *aLangGroup,
                                       const char *aGeneric,
                                       uint32_t *aCount,
                                       char16_t ***aResult)
{
    NS_ENSURE_ARG_POINTER(aCount);
    NS_ENSURE_ARG_POINTER(aResult);

    nsTArray<nsString> fontList;

    nsAutoCString generic;
    if (aGeneric)
        generic.Assign(aGeneric);
    else
        generic.SetIsVoid(true);

    nsCOMPtr<nsIAtom> langGroupAtom;
    if (aLangGroup) {
        nsAutoCString lowered;
        lowered.Assign(aLangGroup);
        ToLowerCase(lowered);
        langGroupAtom = do_GetAtom(lowered);
    }

    nsresult rv = gfxPlatform::GetPlatform()->GetFontList(langGroupAtom, generic, fontList);

    if (NS_FAILED(rv)) {
        *aCount = 0;
        *aResult = nullptr;
        /* XXX in this case, do we want to return the CSS generics? */
        return NS_OK;
    }

    char16_t **fs = static_cast<char16_t **>
                                (nsMemory::Alloc(fontList.Length() * sizeof(char16_t*)));
    for (uint32_t i = 0; i < fontList.Length(); i++) {
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
                                       char16_t **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = nullptr;
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
nsThebesFontEnumerator::GetStandardFamilyName(const char16_t *aName,
                                              char16_t **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    NS_ENSURE_ARG_POINTER(aName);

    nsAutoString name(aName);
    if (name.IsEmpty()) {
        *aResult = nullptr;
        return NS_OK;
    }

    nsAutoString family;
    nsresult rv = gfxPlatform::GetPlatform()->
        GetStandardFamilyName(nsDependentString(aName), family);
    if (NS_FAILED(rv) || family.IsEmpty()) {
        *aResult = nullptr;
        return NS_OK;
    }
    *aResult = ToNewUnicode(family);
    return NS_OK;
}
