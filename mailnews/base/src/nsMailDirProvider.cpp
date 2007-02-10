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
 * The Original Code is Mozilla Mail Directory Provider.
 *
 * The Initial Developer of the Original Code is
 *   Scott MacGregor <mscott@mozilla.org>.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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


#include "nsMailDirProvider.h"
#include "nsMailDirServiceDefs.h"
#ifdef MOZ_XUL_APP
#include "nsXULAppAPI.h"
#endif
#include "nsMsgBaseCID.h"
#include "nsArrayEnumerator.h"
#include "nsCOMArray.h"
#include "nsEnumeratorUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIChromeRegistry.h"
#include "nsICategoryManager.h"

NS_IMPL_ISUPPORTS2(nsMailDirProvider,
		   nsIDirectoryServiceProvider,
		   nsIDirectoryServiceProvider2)

NS_IMETHODIMP
nsMailDirProvider::GetFile(const char *aKey, PRBool *aPersist,
			       nsIFile* *aResult)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMailDirProvider::GetFiles(const char *aKey,
				nsISimpleEnumerator* *aResult)
{
  if (strcmp(aKey, ISP_DIRECTORY_LIST) != 0)
    return NS_ERROR_FAILURE;

  // The list of isp directories includes the isp directory
  // in the current process dir (i.e. <path to thunderbird.exe>\isp and 
  // <path to thunderbird.exe>\isp\locale 
  // and isp and isp\locale for each active extension

  nsCOMPtr<nsIProperties> dirSvc =
    do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID);
  if (!dirSvc)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIFile> currentProcessDir;
  nsresult rv = dirSvc->Get(NS_XPCOM_CURRENT_PROCESS_DIR,
                   NS_GET_IID(nsIFile), getter_AddRefs(currentProcessDir));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISimpleEnumerator> directoryEnumerator;
  rv = NS_NewSingletonEnumerator(getter_AddRefs(directoryEnumerator), currentProcessDir);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISimpleEnumerator> combinedEnumerator;

#ifdef MOZ_XUL_APP
  nsCOMPtr<nsISimpleEnumerator> extensionsEnum;
  rv = dirSvc->Get(XRE_EXTENSIONS_DIR_LIST,
			    NS_GET_IID(nsISimpleEnumerator),
			    getter_AddRefs(extensionsEnum));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewUnionEnumerator(getter_AddRefs(combinedEnumerator), directoryEnumerator, extensionsEnum);
  NS_ENSURE_SUCCESS(rv, rv);
#else
  directoryEnumerator.swap(combinedEnumerator);
#endif
   
  NS_IF_ADDREF(*aResult = new AppendingEnumerator(combinedEnumerator));
  return NS_SUCCESS_AGGREGATE_RESULT;
}

NS_IMPL_ISUPPORTS1(nsMailDirProvider::AppendingEnumerator,
		   nsISimpleEnumerator)

NS_IMETHODIMP
nsMailDirProvider::AppendingEnumerator::HasMoreElements(PRBool *aResult)
{
  *aResult = mNext || mNextWithLocale ? PR_TRUE : PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsMailDirProvider::AppendingEnumerator::GetNext(nsISupports* *aResult)
{
  // Set the return value to the next directory we want to enumerate over
  if (aResult)
    NS_ADDREF(*aResult = mNext);

  if (mNextWithLocale)
  {
    mNext = mNextWithLocale;
    mNextWithLocale = nsnull;
    return NS_OK;
  }

  mNext = nsnull;

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

    mNext->AppendNative(NS_LITERAL_CSTRING("isp"));
    PRBool exists;
    nsresult rv = mNext->Exists(&exists);
    if (NS_SUCCEEDED(rv) && exists)
    {
      if (!mLocale.IsEmpty())
      {
        mNext->Clone(getter_AddRefs(mNextWithLocale));
        mNextWithLocale->AppendNative(mLocale);
        rv = mNextWithLocale->Exists(&exists);
        if (NS_FAILED(rv) || !exists)
          mNextWithLocale = nsnull; // clear out mNextWithLocale, so we don't try to iterate over it
      } 
      break;
    }

    mNext = nsnull;
  }

  return NS_OK;
}

nsMailDirProvider::AppendingEnumerator::AppendingEnumerator
    (nsISimpleEnumerator* aBase) :
  mBase(aBase)
{
  nsCOMPtr<nsIXULChromeRegistry> packageRegistry = do_GetService("@mozilla.org/chrome/chrome-registry;1");
  if (packageRegistry)
    packageRegistry->GetSelectedLocale(NS_LITERAL_CSTRING("global"), mLocale);
  // Initialize mNext to begin
  GetNext(nsnull);
}

NS_METHOD
nsMailDirProvider::Register(nsIComponentManager* aCompMgr,
				nsIFile* aPath, const char *aLoaderStr,
				const char *aType,
				const nsModuleComponentInfo *aInfo)
{
  nsCOMPtr<nsICategoryManager> catMan =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
  if (!catMan)
    return NS_ERROR_FAILURE;

  return catMan->AddCategoryEntry(XPCOM_DIRECTORY_PROVIDER_CATEGORY,
				"mail-directory-provider",
				NS_MAILDIRPROVIDER_CONTRACTID, PR_TRUE, PR_TRUE, nsnull);
}

NS_METHOD
nsMailDirProvider::Unregister(nsIComponentManager* aCompMgr,
				  nsIFile* aPath,
				  const char *aLoaderStr,
				  const nsModuleComponentInfo *aInfo)
{
  nsCOMPtr<nsICategoryManager> catMan =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
  if (!catMan)
    return NS_ERROR_FAILURE;

  return catMan->DeleteCategoryEntry(XPCOM_DIRECTORY_PROVIDER_CATEGORY,
				   "mail-directory-provider",
				   PR_TRUE);
}
