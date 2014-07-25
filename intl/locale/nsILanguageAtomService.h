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
  {0xcb3892a0, 0x6a76, 0x461c, \
    { 0xb0, 0x24, 0x23, 0x0e, 0xe3, 0xe0, 0x81, 0x1a }}

#define NS_LANGUAGEATOMSERVICE_CONTRACTID \
  "@mozilla.org/intl/nslanguageatomservice;1"

class nsILanguageAtomService : public nsISupports
{
 public: 
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ILANGUAGEATOMSERVICE_IID)

  virtual nsIAtom* LookupLanguage(const nsACString &aLanguage,
                                  nsresult *aError = nullptr) = 0;
  virtual already_AddRefed<nsIAtom>
  LookupCharSet(const nsACString& aCharSet) = 0;

  virtual nsIAtom* GetLocaleLanguage(nsresult *aError = nullptr) = 0;

  virtual nsIAtom* GetLanguageGroup(nsIAtom *aLanguage,
                                    nsresult *aError = nullptr) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsILanguageAtomService,
                              NS_ILANGUAGEATOMSERVICE_IID)

#endif
