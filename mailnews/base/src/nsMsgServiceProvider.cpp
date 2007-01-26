/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alec Flett <alecf@netscape.com>
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

#include "nsMsgServiceProvider.h"
#include "nsIServiceManager.h"

#include "nsXPIDLString.h"

#include "nsRDFCID.h"
#include "nsIRDFService.h"
#include "nsIRDFRemoteDataSource.h"

#include "nsIFileChannel.h"
#include "nsIFileSpec.h"
#include "nsNetUtil.h"
#include "nsIMsgMailSession.h"
#include "nsMsgBaseCID.h"

#ifdef MOZ_XUL_APP
#include "nsMailDirServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsISimpleEnumerator.h"
#include "nsIDirectoryEnumerator.h"
#endif

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFCompositeDataSourceCID, NS_RDFCOMPOSITEDATASOURCE_CID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID, NS_RDFXMLDATASOURCE_CID);

nsMsgServiceProviderService::nsMsgServiceProviderService()
{}

nsMsgServiceProviderService::~nsMsgServiceProviderService()
{}

NS_IMPL_ISUPPORTS1(nsMsgServiceProviderService, nsIRDFDataSource)

nsresult
nsMsgServiceProviderService::Init()
{
  nsresult rv;
  nsCOMPtr<nsIRDFService> rdf = do_GetService(kRDFServiceCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  mInnerDataSource = do_CreateInstance(kRDFCompositeDataSourceCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef MOZ_XUL_APP
  LoadISPFiles();
#else
  nsCOMPtr<nsIMsgMailSession> mailSession = do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get defaults directory for isp files. MailSession service appends 'isp' to the 
  // the app defaults folder and returns it. Locale will be added to the path, if there is one.
  nsCOMPtr<nsIFile> dataFilesDir;
  rv = mailSession->GetDataFilesDir("isp", getter_AddRefs(dataFilesDir));
  NS_ENSURE_SUCCESS(rv,rv);

  // test if there is a locale provider
  PRBool isexists = PR_FALSE;
  rv = dataFilesDir->Exists(&isexists);
  NS_ENSURE_SUCCESS(rv,rv);
  if (isexists) {    
    // now enumerate every file in the directory, and suck it into the datasource
    PRBool hasMore = PR_FALSE;
    nsCOMPtr<nsISimpleEnumerator> dirIterator;
    rv = dataFilesDir->GetDirectoryEntries(getter_AddRefs(dirIterator));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIFile> dirEntry;

    while ((rv = dirIterator->HasMoreElements(&hasMore)) == NS_OK && hasMore) {
      rv = dirIterator->GetNext((nsISupports**)getter_AddRefs(dirEntry));
      if (NS_FAILED(rv))
        continue;

      nsCAutoString urlSpec;
      rv = NS_GetURLSpecFromFile(dirEntry, urlSpec);
      rv = LoadDataSource(urlSpec.get());
      NS_ASSERTION(NS_SUCCEEDED(rv), "Failed reading in the datasource\n");
    }
  }
#endif
  return NS_OK;
}

#ifdef MOZ_XUL_APP
/**
 * Looks for ISP configuration files in <.exe>\isp and any sub directories called isp
 * located in the user's extensions directory.
 */
void nsMsgServiceProviderService::LoadISPFiles()
{
  nsresult rv;
  nsCOMPtr<nsIProperties> dirSvc = do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return;

  // Walk through the list of isp directories
  nsCOMPtr<nsISimpleEnumerator> ispDirectories;
  rv = dirSvc->Get(ISP_DIRECTORY_LIST,
                   NS_GET_IID(nsISimpleEnumerator), getter_AddRefs(ispDirectories));
  if (NS_FAILED(rv))
    return;

  PRBool hasMore;
  nsCOMPtr<nsIFile> ispDirectory;
  while (NS_SUCCEEDED(ispDirectories->HasMoreElements(&hasMore)) && hasMore) 
  {
    nsCOMPtr<nsISupports> elem;
    ispDirectories->GetNext(getter_AddRefs(elem));

    ispDirectory = do_QueryInterface(elem);
    if (ispDirectory)
      LoadISPFilesFromDir(ispDirectory);
  }
}

void nsMsgServiceProviderService::LoadISPFilesFromDir(nsIFile* aDir)
{
  nsresult rv;

  PRBool check = PR_FALSE;
  rv = aDir->Exists(&check);
  if (NS_FAILED(rv) || !check)
    return;

  rv = aDir->IsDirectory(&check);
  if (NS_FAILED(rv) || !check)
    return;

  nsCOMPtr<nsISimpleEnumerator> e;
  rv = aDir->GetDirectoryEntries(getter_AddRefs(e));
  if (NS_FAILED(rv))
    return;

  nsCOMPtr<nsIDirectoryEnumerator> files(do_QueryInterface(e));
  if (!files)
    return;

  // we only care about the .rdf files in this directory
  nsCOMPtr<nsIFile> file;
  while (NS_SUCCEEDED(files->GetNextFile(getter_AddRefs(file))) && file) {
    nsAutoString leafName;
    file->GetLeafName(leafName);
    if (!StringEndsWith(leafName, NS_LITERAL_STRING(".rdf")))
      continue;

    nsCAutoString urlSpec;
    rv = NS_GetURLSpecFromFile(file, urlSpec);
    if (NS_SUCCEEDED(rv))
      LoadDataSource(urlSpec.get());
  }
}
#endif

nsresult
nsMsgServiceProviderService::LoadDataSource(const char *aURI)
{
  nsresult rv;

  nsCOMPtr<nsIRDFDataSource> ds =
      do_CreateInstance(kRDFXMLDataSourceCID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIRDFRemoteDataSource> remote =
      do_QueryInterface(ds, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = remote->Init(aURI);
  NS_ENSURE_SUCCESS(rv, rv);
  // for now load synchronously (async seems to be busted)
  rv = remote->Refresh(PR_TRUE);
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed refresh?\n");

  rv = mInnerDataSource->AddDataSource(ds);

  return rv;
}
