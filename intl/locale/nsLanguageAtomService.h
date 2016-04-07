/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsILanguageAtomService.h"
#include "nsIStringBundle.h"
#include "nsInterfaceHashtable.h"
#include "nsIAtom.h"
#include "nsUConvPropertySearch.h"
#include "mozilla/Attributes.h"

#define NS_LANGUAGEATOMSERVICE_CID \
  {0xB7C65853, 0x2996, 0x435E, {0x96, 0x54, 0xDC, 0xC1, 0x78, 0xAA, 0xB4, 0x8C}}

class nsLanguageAtomService final : public nsILanguageAtomService
{
public:
  NS_DECL_ISUPPORTS

  // nsILanguageAtomService
  virtual nsIAtom*
    LookupLanguage(const nsACString &aLanguage, nsresult *aError) override;

  virtual already_AddRefed<nsIAtom>
    LookupCharSet(const nsACString& aCharSet) override;

  virtual nsIAtom* GetLocaleLanguage(nsresult *aError) override;

  virtual nsIAtom* GetLanguageGroup(nsIAtom *aLanguage,
                                                nsresult *aError) override;

  nsLanguageAtomService();

private:
  ~nsLanguageAtomService() { }

protected:
  nsInterfaceHashtable<nsISupportsHashKey, nsIAtom> mLangToGroup;
  nsCOMPtr<nsIAtom> mLocaleLanguage;
};
