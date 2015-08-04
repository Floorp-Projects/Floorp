/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozGenericWordUtils.h"

NS_IMPL_ISUPPORTS(mozGenericWordUtils, mozISpellI18NUtil)

  // do something sensible but generic ... eventually.  For now whine.

mozGenericWordUtils::mozGenericWordUtils()
{
  /* member initializers and constructor code */
}

mozGenericWordUtils::~mozGenericWordUtils()
{
  /* destructor code */
}

NS_IMETHODIMP mozGenericWordUtils::GetLanguage(char16_t * *aLanguage)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP mozGenericWordUtils::GetRootForm(const char16_t *word, uint32_t type, char16_t ***words, uint32_t *count)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP mozGenericWordUtils::FromRootForm(const char16_t *word, const char16_t **iwords, uint32_t icount, char16_t ***owords, uint32_t *ocount)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP mozGenericWordUtils::FindNextWord(const char16_t *word, uint32_t length, uint32_t offset, int32_t *begin, int32_t *end)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

