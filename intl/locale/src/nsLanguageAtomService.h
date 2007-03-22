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

#include "nsCOMPtr.h"
#include "nsICharsetConverterManager.h"
#include "nsILanguageAtomService.h"
#include "nsIStringBundle.h"
#include "nsISupportsArray.h"
#include "nsCRT.h"
#include "nsInterfaceHashtable.h"
#include "nsIAtom.h"

#define NS_LANGUAGEATOMSERVICE_CID \
  {0xa6cf9120, 0x15b3, 0x11d2, {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

class nsLanguageAtomService : public nsILanguageAtomService
{
public:
  NS_DECL_ISUPPORTS

  // nsILanguageAtomService
  virtual NS_HIDDEN_(nsIAtom*)
    LookupLanguage(const nsAString &aLanguage, nsresult *aError);

  virtual NS_HIDDEN_(already_AddRefed<nsIAtom>)
    LookupCharSet(const char *aCharSet, nsresult *aError);

  virtual NS_HIDDEN_(nsIAtom*) GetLocaleLanguageGroup(nsresult *aError);

  nsLanguageAtomService() NS_HIDDEN;

private:
  NS_HIDDEN ~nsLanguageAtomService() { }

protected:
  NS_HIDDEN_(nsresult) InitLangGroupTable();

  nsCOMPtr<nsICharsetConverterManager> mCharSets;
  nsInterfaceHashtable<nsStringHashKey, nsIAtom> mLangs;
  nsCOMPtr<nsIStringBundle> mLangGroups;
  nsCOMPtr<nsIAtom> mLocaleLangGroup;
};
