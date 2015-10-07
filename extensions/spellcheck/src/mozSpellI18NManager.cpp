/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozSpellI18NManager.h"
#include "mozEnglishWordUtils.h"
#include "nsString.h"
#include "mozilla/RefPtr.h"

NS_IMPL_ISUPPORTS(mozSpellI18NManager, mozISpellI18NManager)

mozSpellI18NManager::mozSpellI18NManager()
{
}

mozSpellI18NManager::~mozSpellI18NManager()
{
}

NS_IMETHODIMP mozSpellI18NManager::GetUtil(const char16_t *aLanguage, mozISpellI18NUtil **_retval)
{
 if (!_retval) {
   return NS_ERROR_NULL_POINTER;
 }

 // XXX TODO Actually handle multiple languages.
 RefPtr<mozEnglishWordUtils> utils = new mozEnglishWordUtils;
 utils.forget(_retval);

 return NS_OK;
}
