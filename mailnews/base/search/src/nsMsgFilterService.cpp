/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// this file implements the nsMsgFilterService interface 

#include "msgCore.h"
#include "nsMsgFilterService.h"
#include "nsFileStream.h"
#include "nsMsgFilterList.h"
#include "nsSpecialSystemDirectory.h"
#include "nsIPrompt.h"
#include "nsIDocShell.h"
#include "nsIMsgWindow.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIStringBundle.h"

NS_IMPL_ISUPPORTS1(nsMsgFilterService, nsIMsgFilterService)

nsMsgFilterService::nsMsgFilterService()
{
	NS_INIT_ISUPPORTS();
}

nsMsgFilterService::~nsMsgFilterService()
{
}

NS_IMETHODIMP nsMsgFilterService::OpenFilterList(nsIFileSpec *filterFile, nsIMsgFolder *rootFolder, nsIMsgWindow *aMsgWindow, nsIMsgFilterList **resultFilterList)
{
	nsresult ret = NS_OK;

    nsFileSpec filterSpec;
    filterFile->GetFileSpec(&filterSpec);
	nsIOFileStream *fileStream = new nsIOFileStream(filterSpec);
	if (!fileStream)
		return NS_ERROR_OUT_OF_MEMORY;

	nsMsgFilterList *filterList = new nsMsgFilterList();
	if (!filterList)
		return NS_ERROR_OUT_OF_MEMORY;
	NS_ADDREF(filterList);
    filterList->SetFolder(rootFolder);
    
    // temporarily tell the filter where it's file path is
    filterList->SetDefaultFile(filterFile);
    
    PRUint32 size;
    ret = filterFile->GetFileSize(&size);
	if (NS_SUCCEEDED(ret) && size > 0)
		ret = filterList->LoadTextFilters(fileStream);
  fileStream->close();
  delete fileStream;
  fileStream =nsnull;
	if (NS_SUCCEEDED(ret))
  {
		*resultFilterList = filterList;
        PRInt16 version;
        filterList->GetVersion(&version);
    if (version != kFileVersion)
    {

      SaveFilterList(filterList, filterFile);
    }
  }
	else
  {
    NS_RELEASE(filterList);
    if (ret == NS_MSG_FILTER_PARSE_ERROR && aMsgWindow)
    {
      ret = BackUpFilterFile(filterFile, aMsgWindow);
      NS_ENSURE_SUCCESS(ret, ret);
      ret = filterFile->Truncate(0);
      NS_ENSURE_SUCCESS(ret, ret);
      return OpenFilterList(filterFile, rootFolder, aMsgWindow, resultFilterList);
    }
    else if(ret == NS_MSG_CUSTOM_HEADERS_OVERFLOW && aMsgWindow)
      ThrowAlertMsg("filterCustomHeaderOverflow", aMsgWindow);
    else if(ret == NS_MSG_INVALID_CUSTOM_HEADER && aMsgWindow)
      ThrowAlertMsg("invalidCustomHeader", aMsgWindow);
  }
	return ret;
}

NS_IMETHODIMP nsMsgFilterService::CloseFilterList(nsIMsgFilterList *filterList)
{
	//NS_ASSERTION(PR_FALSE,"CloseFilterList doesn't do anything yet");
	return NS_OK;
}

/* save without deleting */
NS_IMETHODIMP	nsMsgFilterService::SaveFilterList(nsIMsgFilterList *filterList, nsIFileSpec *filterFile)
{
	nsresult ret = NS_OK;
  nsCOMPtr <nsIFileSpec> tmpFiltersFile;
  nsCOMPtr <nsIFileSpec> realFiltersFile;
  nsCOMPtr <nsIFileSpec> parentDir;

  nsSpecialSystemDirectory tmpFile(nsSpecialSystemDirectory::OS_TemporaryDirectory);
  tmpFile += "tmprules.dat";

  ret = NS_NewFileSpecWithSpec(tmpFile, getter_AddRefs(tmpFiltersFile));

  NS_ASSERTION(NS_SUCCEEDED(ret),"writing filters file: failed to append filename");
  if (NS_FAILED(ret)) 
    return ret;

  ret = tmpFiltersFile->MakeUnique();  //need a unique tmp file to prevent dataloss in multiuser environment
  NS_ENSURE_SUCCESS(ret, ret);

  nsFileSpec tmpFileSpec;
  tmpFiltersFile->GetFileSpec(&tmpFileSpec);

	nsIOFileStream *tmpFileStream = nsnull;
  
  if (NS_SUCCEEDED(ret))
    ret = filterFile->GetParent(getter_AddRefs(parentDir));

  if (NS_SUCCEEDED(ret))
    tmpFileStream = new nsIOFileStream(tmpFileSpec);
	if (!tmpFileStream)
		return NS_ERROR_OUT_OF_MEMORY;
  ret = filterList->SaveToFile(tmpFileStream);
  tmpFileStream->close();
  delete tmpFileStream;
  tmpFileStream = nsnull;
  
  if (NS_SUCCEEDED(ret))
  {
    // can't move across drives
    ret = tmpFiltersFile->CopyToDir(parentDir);
    if (NS_SUCCEEDED(ret))
    {
      filterFile->Delete(PR_FALSE);
      nsXPIDLCString tmpFileName;
      tmpFiltersFile->GetLeafName(getter_Copies(tmpFileName));
      parentDir->AppendRelativeUnixPath(tmpFileName.get());
      parentDir->Rename("rules.dat");
      tmpFiltersFile->Delete(PR_FALSE);
    }

  }
  NS_ASSERTION(NS_SUCCEEDED(ret), "error opening/saving filter list");
	return ret;
}

NS_IMETHODIMP nsMsgFilterService::CancelFilterList(nsIMsgFilterList *filterList)
{ 
	return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsMsgFilterService::BackUpFilterFile(nsIFileSpec *aFilterFile, nsIMsgWindow *aMsgWindow)
{
  nsresult rv;
  AlertBackingUpFilterFile(aMsgWindow);
  aFilterFile->CloseStream();

  nsCOMPtr<nsILocalFile> localFilterFile;
  nsFileSpec filterFileSpec;
  aFilterFile->GetFileSpec(&filterFileSpec);
  rv = NS_FileSpecToIFile(&filterFileSpec, getter_AddRefs(localFilterFile));
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsILocalFile> localParentDir;
  nsCOMPtr <nsIFileSpec> parentDir;
  rv = aFilterFile->GetParent(getter_AddRefs(parentDir));
  NS_ENSURE_SUCCESS(rv,rv);

  nsFileSpec parentDirSpec;
  parentDir->GetFileSpec(&parentDirSpec);

  rv = NS_FileSpecToIFile(&parentDirSpec, getter_AddRefs(localParentDir));
  NS_ENSURE_SUCCESS(rv,rv);

  //if back-up file exists delete the back up file otherwise copy fails. 
  nsCOMPtr <nsILocalFile> backupFile;
  rv = NS_FileSpecToIFile(&parentDirSpec, getter_AddRefs(backupFile));
  NS_ENSURE_SUCCESS(rv,rv);
  backupFile->AppendNative(NS_LITERAL_CSTRING("rulesbackup.dat"));
  PRBool exists;
  backupFile->Exists(&exists);
  if (exists)
    backupFile->Remove(PR_FALSE);

  return localFilterFile->CopyToNative(localParentDir, NS_LITERAL_CSTRING("rulesbackup.dat"));
}

nsresult nsMsgFilterService::AlertBackingUpFilterFile(nsIMsgWindow *aMsgWindow)
{
  return ThrowAlertMsg("filterListBackUpMsg", aMsgWindow);
}

nsresult //Do not use this routine if you have to call it very often because it creates a new bundle each time
nsMsgFilterService::GetStringFromBundle(const char *aMsgName, PRUnichar **aResult)
{ 
  nsresult rv=NS_OK;
  NS_ENSURE_ARG_POINTER(aResult);
  nsCOMPtr <nsIStringBundle> bundle;
  rv = GetFilterStringBundle(getter_AddRefs(bundle));
  if (NS_SUCCEEDED(rv) && bundle)
    rv=bundle->GetStringFromName(NS_ConvertASCIItoUCS2(aMsgName).get(), aResult);
  return rv;
  
}

nsresult
nsMsgFilterService::GetFilterStringBundle(nsIStringBundle **aBundle)
{
  nsresult rv=NS_OK;
  NS_ENSURE_ARG_POINTER(aBundle);
  nsCOMPtr<nsIStringBundleService> bundleService =
         do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  nsCOMPtr<nsIStringBundle> bundle;
  if (bundleService && NS_SUCCEEDED(rv))
    bundleService->CreateBundle("chrome://messenger/locale/filter.properties",
                                 getter_AddRefs(bundle));
  NS_IF_ADDREF(*aBundle = bundle);
  return rv;
}

nsresult
nsMsgFilterService::ThrowAlertMsg(const char*aMsgName, nsIMsgWindow *aMsgWindow)
{
  nsXPIDLString alertString;
  nsresult rv = GetStringFromBundle(aMsgName, getter_Copies(alertString));
  if (NS_SUCCEEDED(rv) && alertString && aMsgWindow)
  {
    nsCOMPtr <nsIDocShell> docShell;
    aMsgWindow->GetRootDocShell(getter_AddRefs(docShell));
    if (docShell)
    {
      nsCOMPtr<nsIPrompt> dialog(do_GetInterface(docShell));
      if (dialog && alertString)
        dialog->Alert(nsnull, alertString);
  }
  }
  return rv;
}

