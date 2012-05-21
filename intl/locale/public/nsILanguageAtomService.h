/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsILanguageAtomService_h_
#define nsILanguageAtomService_h_

/*
 * The nsILanguageAtomService provides a mapping from languages or charsets
 * to language groups, and access to the system locale language.
 */

#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"

#define NS_ILANGUAGEATOMSERVICE_IID \
  {0xAF4C48CF, 0x8F76, 0x4477, \
    { 0xA7, 0x0E, 0xAB, 0x09, 0x74, 0xE2, 0x41, 0xF0 }}

#define NS_LANGUAGEATOMSERVICE_CONTRACTID \
  "@mozilla.org/intl/nslanguageatomservice;1"

class nsILanguageAtomService : public nsISupports
{
 public: 
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ILANGUAGEATOMSERVICE_IID)

  virtual nsIAtom* LookupLanguage(const nsACString &aLanguage,
                                  nsresult *aError = nsnull) = 0;
  virtual already_AddRefed<nsIAtom>
  LookupCharSet(const char *aCharSet, nsresult *aError = nsnull) = 0;

  virtual nsIAtom* GetLocaleLanguage(nsresult *aError = nsnull) = 0;

  virtual nsIAtom* GetLanguageGroup(nsIAtom *aLanguage,
                                    nsresult *aError = nsnull) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsILanguageAtomService,
                              NS_ILANGUAGEATOMSERVICE_IID)

#endif
