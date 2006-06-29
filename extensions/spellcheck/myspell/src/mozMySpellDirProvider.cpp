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
 * The Original Code is Mozilla Firefox.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation <http://www.mozilla.org>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Benjamin Smedberg <benjamin@smedbergs.us> (Original Code)
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
 * ***** END LICENSE BLOCK ***** */

#include "mozMySpellDirProvider.h"
#include "nsXULAppAPI.h"
#include "nsString.h"

#include "mozISpellCheckingEngine.h"
#include "nsICategoryManager.h"

NS_IMPL_ISUPPORTS2(mozMySpellDirProvider,
		   nsIDirectoryServiceProvider,
		   nsIDirectoryServiceProvider2)

NS_IMETHODIMP
mozMySpellDirProvider::GetFile(const char *aKey, PRBool *aPersist,
			       nsIFile* *aResult)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
mozMySpellDirProvider::GetFiles(const char *aKey,
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

NS_IMPL_ISUPPORTS1(mozMySpellDirProvider::AppendingEnumerator,
		   nsISimpleEnumerator)

NS_IMETHODIMP
mozMySpellDirProvider::AppendingEnumerator::HasMoreElements(PRBool *aResult)
{
  *aResult = mNext ? PR_TRUE : PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
mozMySpellDirProvider::AppendingEnumerator::GetNext(nsISupports* *aResult)
{
  if (aResult)
    NS_ADDREF(*aResult = mNext);

  mNext = nsnull;

  nsresult rv;

  // Ignore all errors

  PRBool more;
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

    PRBool exists;
    rv = mNext->Exists(&exists);
    if (NS_SUCCEEDED(rv) && exists)
      break;

    mNext = nsnull;
  }

  return NS_OK;
}

mozMySpellDirProvider::AppendingEnumerator::AppendingEnumerator
    (nsISimpleEnumerator* aBase) :
  mBase(aBase)
{
  // Initialize mNext to begin
  GetNext(nsnull);
}

NS_METHOD
mozMySpellDirProvider::Register(nsIComponentManager* aCompMgr,
				nsIFile* aPath, const char *aLoaderStr,
				const char *aType,
				const nsModuleComponentInfo *aInfo)
{
  nsresult rv;

  nsCOMPtr<nsICategoryManager> catMan =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
  if (!catMan)
    return NS_ERROR_FAILURE;

  rv = catMan->AddCategoryEntry(XPCOM_DIRECTORY_PROVIDER_CATEGORY,
				"spellcheck-directory-provider",
				kContractID, PR_TRUE, PR_TRUE, nsnull);
  return rv;
}

NS_METHOD
mozMySpellDirProvider::Unregister(nsIComponentManager* aCompMgr,
				  nsIFile* aPath,
				  const char *aLoaderStr,
				  const nsModuleComponentInfo *aInfo)
{
  nsresult rv;

  nsCOMPtr<nsICategoryManager> catMan =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
  if (!catMan)
    return NS_ERROR_FAILURE;

  rv = catMan->DeleteCategoryEntry(XPCOM_DIRECTORY_PROVIDER_CATEGORY,
				   "spellcheck-directory-provider",
				   PR_TRUE);
  return rv;
}

char const *const
mozMySpellDirProvider::kContractID = "@mozilla.org/spellcheck/dir-provider;1";
