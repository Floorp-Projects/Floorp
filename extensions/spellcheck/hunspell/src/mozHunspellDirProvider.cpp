/******* BEGIN LICENSE BLOCK *******
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
 * The Initial Developers of the Original Code are Kevin Hendricks (MySpell)
 * and László Németh (Hunspell). Portions created by the Initial Developers
 * are Copyright (C) 2002-2005 the Initial Developers. All Rights Reserved.
 * 
 * Contributor(s): Benjamin Smedberg (benjamin@smedbergs.us) (Original Code)
 *                 László Németh (nemethl@gyorsposta.hu)
 *                 Ryan VanderMeulen (ryanvm@gmail.com)
 * 
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 ******* END LICENSE BLOCK *******/

#include "mozHunspellDirProvider.h"
#include "nsXULAppAPI.h"
#include "nsString.h"

#include "mozISpellCheckingEngine.h"
#include "nsICategoryManager.h"

NS_IMPL_ISUPPORTS2(mozHunspellDirProvider,
		   nsIDirectoryServiceProvider,
		   nsIDirectoryServiceProvider2)

NS_IMETHODIMP
mozHunspellDirProvider::GetFile(const char *aKey, bool *aPersist,
			       nsIFile* *aResult)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
mozHunspellDirProvider::GetFiles(const char *aKey,
				nsISimpleEnumerator* *aResult)
{
  if (strcmp(aKey, DICTIONARY_SEARCH_DIRECTORY_LIST) != 0) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIProperties> dirSvc =
    do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID);
  if (!dirSvc)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsISimpleEnumerator> list;
  nsresult rv = dirSvc->Get(XRE_EXTENSIONS_DIR_LIST,
			    NS_GET_IID(nsISimpleEnumerator),
			    getter_AddRefs(list));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsISimpleEnumerator> e = new AppendingEnumerator(list);
  if (!e)
    return NS_ERROR_OUT_OF_MEMORY;

  *aResult = nsnull;
  e.swap(*aResult);
  return NS_SUCCESS_AGGREGATE_RESULT;
}

NS_IMPL_ISUPPORTS1(mozHunspellDirProvider::AppendingEnumerator,
		   nsISimpleEnumerator)

NS_IMETHODIMP
mozHunspellDirProvider::AppendingEnumerator::HasMoreElements(bool *aResult)
{
  *aResult = mNext ? PR_TRUE : PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
mozHunspellDirProvider::AppendingEnumerator::GetNext(nsISupports* *aResult)
{
  if (aResult)
    NS_ADDREF(*aResult = mNext);

  mNext = nsnull;

  nsresult rv;

  // Ignore all errors

  bool more;
  while (NS_SUCCEEDED(mBase->HasMoreElements(&more)) && more) {
    nsCOMPtr<nsISupports> nextbasesupp;
    mBase->GetNext(getter_AddRefs(nextbasesupp));

    nsCOMPtr<nsIFile> nextbase(do_QueryInterface(nextbasesupp));
    if (!nextbase)
      continue;

    nextbase->Clone(getter_AddRefs(mNext));
    if (!mNext)
      continue;

    mNext->AppendNative(NS_LITERAL_CSTRING("dictionaries"));

    bool exists;
    rv = mNext->Exists(&exists);
    if (NS_SUCCEEDED(rv) && exists)
      break;

    mNext = nsnull;
  }

  return NS_OK;
}

mozHunspellDirProvider::AppendingEnumerator::AppendingEnumerator
    (nsISimpleEnumerator* aBase) :
  mBase(aBase)
{
  // Initialize mNext to begin
  GetNext(nsnull);
}

char const *const
mozHunspellDirProvider::kContractID = "@mozilla.org/spellcheck/dir-provider;1";
