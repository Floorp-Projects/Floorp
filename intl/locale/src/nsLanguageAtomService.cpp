/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIComponentManager.h"
#include "nsLanguageAtomService.h"
#include "nsICharsetConverterManager.h"
#include "nsILocaleService.h"
#include "nsXPIDLString.h"
#include "nsUnicharUtils.h"
#include "nsIServiceManager.h"

NS_IMPL_ISUPPORTS1(nsLanguageAtomService, nsILanguageAtomService)

nsLanguageAtomService::nsLanguageAtomService()
{
  mLangs.Init();
}

nsresult
nsLanguageAtomService::InitLangGroupTable()
{
  if (mLangGroups) return NS_OK;
  nsresult rv;
  
  nsCOMPtr<nsIStringBundleService> bundleService =
    do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;


  rv = bundleService->CreateBundle("resource://gre/res/langGroups.properties",
                                   getter_AddRefs(mLangGroups));
  return rv;
}

nsIAtom*
nsLanguageAtomService::LookupLanguage(const nsAString &aLanguage,
                                      nsresult *aError)
{
  nsresult res = NS_OK;

  nsAutoString lowered(aLanguage);
  ToLowerCase(lowered);

  nsIAtom *lang = mLangs.GetWeak(lowered);

  if (!lang) {
    nsXPIDLString langGroupStr;

    if (lowered.EqualsLiteral("en-us")) {
      langGroupStr.Assign(NS_LITERAL_STRING("x-western"));
    } else if (lowered.EqualsLiteral("de-de")) {
      langGroupStr.Assign(NS_LITERAL_STRING("x-western"));
    } else if (lowered.EqualsLiteral("ja-jp")) {
      langGroupStr.Assign(NS_LITERAL_STRING("ja"));
    } else {
      if (!mLangGroups) {
        if (NS_FAILED(InitLangGroupTable())) {
          if (aError)
            *aError = NS_ERROR_FAILURE;

          return nsnull;
        }
      }
      res = mLangGroups->GetStringFromName(lowered.get(), getter_Copies(langGroupStr));
      if (NS_FAILED(res)) {
        PRInt32 hyphen = lowered.FindChar('-');
        if (hyphen >= 0) {
          nsAutoString truncated(lowered);
          truncated.Truncate(hyphen);
          res = mLangGroups->GetStringFromName(truncated.get(), getter_Copies(langGroupStr));
          if (NS_FAILED(res)) {
            langGroupStr.Assign(NS_LITERAL_STRING("x-western"));
          }
        } else {
          langGroupStr.Assign(NS_LITERAL_STRING("x-western"));
        }
      }
    }
    nsCOMPtr<nsIAtom> langGroup = do_GetAtom(langGroupStr);

    // The hashtable will keep an owning reference to the atom
    mLangs.Put(lowered, lang);
    lang = langGroup;
  }

  if (aError)
    *aError = res;

  return lang;
}

already_AddRefed<nsIAtom>
nsLanguageAtomService::LookupCharSet(const char *aCharSet, nsresult *aError)
{
  nsresult res;

  if (!mCharSets) {
    mCharSets = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID);
    if (!mCharSets) {
      if (aError)
        *aError = NS_ERROR_FAILURE;

      return nsnull;
    }
  }
  if (!mUnicode) {
    mUnicode = do_GetAtom("x-unicode");
  }

  nsCOMPtr<nsIAtom> langGroup;
  mCharSets->GetCharsetLangGroup(aCharSet, getter_AddRefs(langGroup));
  if (!langGroup) {
    if (aError)
      *aError = NS_ERROR_FAILURE;

    return nsnull;
  }
#if !defined(XP_BEOS)
  if (langGroup == mUnicode) {
    langGroup = GetLocaleLanguageGroup(&res);
    if (NS_FAILED(res)) {
      if (aError)
        *aError = res;

      return nsnull;
    }
  }
#endif

  // transfer reference to raw pointer
  nsIAtom *raw = nsnull;
  langGroup.swap(raw);

  if (aError)
    *aError = NS_OK;

  return raw;
}

nsIAtom*
nsLanguageAtomService::GetLocaleLanguageGroup(nsresult *aError)
{
  nsresult res = NS_OK;

  do {
    if (!mLocaleLangGroup) {
      nsCOMPtr<nsILocaleService> localeService;
      localeService = do_GetService(NS_LOCALESERVICE_CONTRACTID);
      if (!localeService) {
        res = NS_ERROR_FAILURE;
        break;
      }

      nsCOMPtr<nsILocale> locale;
      res = localeService->GetApplicationLocale(getter_AddRefs(locale));
      if (NS_FAILED(res))
        break;

      nsAutoString category;
      category.AssignWithConversion(NSILOCALE_MESSAGE);
      nsAutoString loc;
      res = locale->GetCategory(category, loc);
      if (NS_FAILED(res))
        break;

      mLocaleLangGroup = LookupLanguage(loc, &res);
    }
  } while (0);

  if (aError)
    *aError = res;

  return mLocaleLangGroup;
}
