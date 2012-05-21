/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozGenericWordUtils.h"

NS_IMPL_CYCLE_COLLECTING_ADDREF(mozGenericWordUtils)
NS_IMPL_CYCLE_COLLECTING_RELEASE(mozGenericWordUtils)

NS_INTERFACE_MAP_BEGIN(mozGenericWordUtils)
  NS_INTERFACE_MAP_ENTRY(mozISpellI18NUtil)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, mozISpellI18NUtil)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(mozGenericWordUtils)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_0(mozGenericWordUtils)

  // do something sensible but generic ... eventually.  For now whine.

mozGenericWordUtils::mozGenericWordUtils()
{
  /* member initializers and constructor code */
}

mozGenericWordUtils::~mozGenericWordUtils()
{
  /* destructor code */
}

/* readonly attribute wstring language; */
NS_IMETHODIMP mozGenericWordUtils::GetLanguage(PRUnichar * *aLanguage)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void GetRootForm (in wstring word, in PRUint32 type, [array, size_is (count)] out wstring words, out PRUint32 count); */
NS_IMETHODIMP mozGenericWordUtils::GetRootForm(const PRUnichar *word, PRUint32 type, PRUnichar ***words, PRUint32 *count)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void FromRootForm (in wstring word, [array, size_is (icount)] in wstring iwords, in PRUint32 icount, [array, size_is (ocount)] out wstring owords, out PRUint32 ocount); */
NS_IMETHODIMP mozGenericWordUtils::FromRootForm(const PRUnichar *word, const PRUnichar **iwords, PRUint32 icount, PRUnichar ***owords, PRUint32 *ocount)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void FindNextWord (in wstring word, in PRUint32 length, in PRUint32 offset, out PRUint32 begin, out PRUint32 end); */
NS_IMETHODIMP mozGenericWordUtils::FindNextWord(const PRUnichar *word, PRUint32 length, PRUint32 offset, PRInt32 *begin, PRInt32 *end)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

