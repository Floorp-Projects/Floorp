/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozGenericWordUtils.h"

NS_IMPL_ISUPPORTS1(mozGenericWordUtils, mozISpellI18NUtil)

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

/* void GetRootForm (in wstring word, in uint32_t type, [array, size_is (count)] out wstring words, out uint32_t count); */
NS_IMETHODIMP mozGenericWordUtils::GetRootForm(const PRUnichar *word, uint32_t type, PRUnichar ***words, uint32_t *count)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void FromRootForm (in wstring word, [array, size_is (icount)] in wstring iwords, in uint32_t icount, [array, size_is (ocount)] out wstring owords, out uint32_t ocount); */
NS_IMETHODIMP mozGenericWordUtils::FromRootForm(const PRUnichar *word, const PRUnichar **iwords, uint32_t icount, PRUnichar ***owords, uint32_t *ocount)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void FindNextWord (in wstring word, in uint32_t length, in uint32_t offset, out uint32_t begin, out uint32_t end); */
NS_IMETHODIMP mozGenericWordUtils::FindNextWord(const PRUnichar *word, uint32_t length, uint32_t offset, int32_t *begin, int32_t *end)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

