/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * The nsILanguageAtomService provides a mapping from languages or charsets
 * to language groups, and access to the system locale language.
 */

#ifndef nsLanguageAtomService_h_
#define nsLanguageAtomService_h_

#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsInterfaceHashtable.h"

class nsLanguageAtomService
{
public:
  static nsLanguageAtomService* GetService();

  nsIAtom* LookupLanguage(const nsACString &aLanguage);
  already_AddRefed<nsIAtom> LookupCharSet(const nsACString& aCharSet);
  nsIAtom* GetLocaleLanguage();
  nsIAtom* GetLanguageGroup(nsIAtom* aLanguage);
  already_AddRefed<nsIAtom> GetUncachedLanguageGroup(nsIAtom* aLanguage) const;

private:
  nsInterfaceHashtable<nsISupportsHashKey, nsIAtom> mLangToGroup;
  nsCOMPtr<nsIAtom> mLocaleLanguage;
};

#endif
