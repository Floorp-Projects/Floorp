/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Erik van der Poel
 */

#include "nsCOMPtr.h"
#include "nsICharsetConverterManager2.h"
#include "nsILanguageAtomService.h"
#include "nsIPersistentProperties2.h"
#include "nsISupportsArray.h"

class nsLanguageAtomService : public nsILanguageAtomService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSILANGUAGEATOMSERVICE

  nsLanguageAtomService();
  virtual ~nsLanguageAtomService();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  NS_IMETHOD InitLangTable();
  NS_IMETHOD InitLangGroupTable();

protected:
  nsCOMPtr<nsICharsetConverterManager2> mCharSets;
  nsCOMPtr<nsISupportsArray> mLangs;
  nsCOMPtr<nsIPersistentProperties> mLangGroups;
  nsCOMPtr<nsIAtom> mLocaleLangGroup;
  nsCOMPtr<nsIAtom> mUnicode;
};
