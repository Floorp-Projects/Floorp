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
 * The Original Code is The Browser Profile Migrator.
 *
 * The Initial Developer of the Original Code is Ben Goodger.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Ben Goodger <ben@bengoodger.com>
 *  Asaf Romano <mozilla.mano@sent.com>
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

#include "nsBrowserProfileMigratorUtils.h"
#ifdef MOZ_PLACES
#include "nsINavBookmarksService.h"
#include "nsBrowserCompsCID.h"
#else
#include "nsIBookmarksService.h"
#endif
#include "nsIFile.h"
#include "nsIInputStream.h"
#include "nsILineInputStream.h"
#include "nsIProperties.h"
#include "nsIProfileMigrator.h"

#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsISupportsPrimitives.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsIRDFService.h"
#include "nsIStringBundle.h"
#include "nsISupportsArray.h"
#include "nsXPCOMCID.h"
#include "nsCRT.h"

#define MIGRATION_BUNDLE "chrome://browser/locale/migration/migration.properties"

static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

void SetUnicharPref(const char* aPref, const nsAString& aValue,
                    nsIPrefBranch* aPrefs)
{
  nsCOMPtr<nsISupportsString> supportsString =
    do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID);    
  if (supportsString) {
     supportsString->SetData(aValue); 
     aPrefs->SetComplexValue(aPref, NS_GET_IID(nsISupportsString),
                             supportsString);
  }
}

void SetProxyPref(const nsAString& aHostPort, const char* aPref, 
                  const char* aPortPref, nsIPrefBranch* aPrefs) 
{
  nsCOMPtr<nsIURI> uri;
  nsCAutoString host;
  PRInt32 portValue;

  // try parsing it as a URI first
  if (NS_SUCCEEDED(NS_NewURI(getter_AddRefs(uri), aHostPort))
      && NS_SUCCEEDED(uri->GetHost(host))
      && !host.IsEmpty()
      && NS_SUCCEEDED(uri->GetPort(&portValue))) {
    SetUnicharPref(aPref, NS_ConvertUTF8toUTF16(host), aPrefs);
    aPrefs->SetIntPref(aPortPref, portValue);
  }
  else {
    nsAutoString hostPort(aHostPort);  
    PRInt32 portDelimOffset = hostPort.RFindChar(':');
    if (portDelimOffset > 0) {
      SetUnicharPref(aPref, Substring(hostPort, 0, portDelimOffset), aPrefs);
      nsAutoString port(Substring(hostPort, portDelimOffset + 1));
      PRInt32 stringErr;
      portValue = port.ToInteger(&stringErr);
      aPrefs->SetIntPref(aPortPref, portValue);
    }
    else
      SetUnicharPref(aPref, hostPort, aPrefs); 
  }
}

void ParseOverrideServers(const nsAString& aServers, nsIPrefBranch* aBranch)
{
  // Windows (and Opera) formats its proxy override list in the form:
  // server;server;server where server is a server name or ip address, 
  // or "<local>". Mozilla's format is server,server,server, and <local>
  // must be translated to "localhost,127.0.0.1"
  nsAutoString override(aServers);
  PRInt32 left = 0, right = 0;
  for (;;) {
    right = override.FindChar(';', right);
    const nsAString& host = Substring(override, left, 
                                      (right < 0 ? override.Length() : right) - left);
    if (host.EqualsLiteral("<local>"))
      override.Replace(left, 7, NS_LITERAL_STRING("localhost,127.0.0.1"));
    if (right < 0)
      break;
    left = right + 1;
    override.Replace(right, 1, NS_LITERAL_STRING(","));
  }
  SetUnicharPref("network.proxy.no_proxies_on", override, aBranch); 
}

void GetMigrateDataFromArray(MigrationData* aDataArray, PRInt32 aDataArrayLength, 
                             PRBool aReplace, nsIFile* aSourceProfile, 
                             PRUint16* aResult)
{
  nsCOMPtr<nsIFile> sourceFile; 
  PRBool exists;
  MigrationData* cursor;
  MigrationData* end = aDataArray + aDataArrayLength;
  for (cursor = aDataArray; cursor < end && cursor->fileName; ++cursor) {
    // When in replace mode, all items can be imported. 
    // When in non-replace mode, only items that do not require file replacement
    // can be imported.
    if (aReplace || !cursor->replaceOnly) {
      aSourceProfile->Clone(getter_AddRefs(sourceFile));
      sourceFile->Append(nsDependentString(cursor->fileName));
      sourceFile->Exists(&exists);
      if (exists)
        *aResult |= cursor->sourceFlag;
    }
    nsCRT::free(cursor->fileName);
    cursor->fileName = nsnull;
  }
}

void
GetProfilePath(nsIProfileStartup* aStartup, nsCOMPtr<nsIFile>& aProfileDir)
{
  if (aStartup) {
    aStartup->GetDirectory(getter_AddRefs(aProfileDir));
  }
  else {
    nsCOMPtr<nsIProperties> dirSvc
      (do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID));
    if (dirSvc) {
      dirSvc->Get(NS_APP_USER_PROFILE_50_DIR, NS_GET_IID(nsIFile),
                  (void**) getter_AddRefs(aProfileDir));
    }
  }
}

nsresult 
AnnotatePersonalToolbarFolder(nsIFile* aSourceBookmarksFile,
                              nsIFile* aTargetBookmarksFile,
                              const char* aToolbarFolderName)
{
  nsCOMPtr<nsIInputStream> fileInputStream;
  nsresult rv = NS_NewLocalFileInputStream(getter_AddRefs(fileInputStream),
                                           aSourceBookmarksFile);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIOutputStream> outputStream;
  rv = NS_NewLocalFileOutputStream(getter_AddRefs(outputStream),
                                   aTargetBookmarksFile);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILineInputStream> lineInputStream =
    do_QueryInterface(fileInputStream, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString sourceBuffer;
  nsCAutoString targetBuffer;
  PRBool moreData = PR_FALSE;
  PRUint32 bytesWritten = 0;
  do {
    lineInputStream->ReadLine(sourceBuffer, &moreData);
    if (!moreData)
      break;

    PRInt32 nameOffset = sourceBuffer.Find(aToolbarFolderName);
    if (nameOffset >= 0) {
      // Found the personal toolbar name on a line, check to make sure it's
      // actually a folder. 
      NS_NAMED_LITERAL_CSTRING(folderPrefix, "<DT><H3 ");
      PRInt32 folderPrefixOffset = sourceBuffer.Find(folderPrefix);
      if (folderPrefixOffset >= 0)
        sourceBuffer.Insert(NS_LITERAL_CSTRING("PERSONAL_TOOLBAR_FOLDER=\"true\" "), 
                            folderPrefixOffset + folderPrefix.Length());
    }

    targetBuffer.Assign(sourceBuffer);
    targetBuffer.Append("\r\n");
    outputStream->Write(targetBuffer.get(), targetBuffer.Length(),
                        &bytesWritten);
  }
  while (1);
  
  outputStream->Close();
  
  return NS_OK;
}

nsresult
ImportBookmarksHTML(nsIFile* aBookmarksFile, 
                    const PRUnichar* aImportSourceNameKey)
{
  nsresult rv;

#ifndef MOZ_PLACES
  nsCOMPtr<nsIBookmarksService> bms = 
    do_GetService("@mozilla.org/browser/bookmarks-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupportsArray> params;
  rv = NS_NewISupportsArray(getter_AddRefs(params));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIRDFService> rdfs =
    do_GetService("@mozilla.org/rdf/rdf-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIRDFResource> prop;
  rv = rdfs->GetResource(NC_URI(URL), getter_AddRefs(prop));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIRDFLiteral> url;
  nsAutoString path;
  aBookmarksFile->GetPath(path);
  rdfs->GetLiteral(path.get(), getter_AddRefs(url));

  params->AppendElement(prop);
  params->AppendElement(url);
  
  nsCOMPtr<nsIRDFResource> importCmd;
  rv = rdfs->GetResource(NC_URI(command?cmd=import), getter_AddRefs(importCmd));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIRDFResource> root;
  rv = rdfs->GetResource(NS_LITERAL_CSTRING("NC:BookmarksRoot"),
                         getter_AddRefs(root));
  NS_ENSURE_SUCCESS(rv, rv);
#endif // MOZ_PLACES

  // Look for the localized name of the bookmarks toolbar
  nsCOMPtr<nsIStringBundleService> bundleService =
    do_GetService(kStringBundleServiceCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStringBundle> bundle;
  rv = bundleService->CreateBundle(MIGRATION_BUNDLE, getter_AddRefs(bundle));
  NS_ENSURE_SUCCESS(rv, rv);

  nsXPIDLString sourceName;
  bundle->GetStringFromName(aImportSourceNameKey, getter_Copies(sourceName));

  const PRUnichar* sourceNameStrings[] = { sourceName.get() };
  nsXPIDLString importedBookmarksTitle;
  bundle->FormatStringFromName(NS_LITERAL_STRING("importedBookmarksFolder").get(),
                               sourceNameStrings, 1, 
                               getter_Copies(importedBookmarksTitle));

#ifdef MOZ_PLACES
  // Get the bookmarks service
  nsCOMPtr<nsINavBookmarksService> bms =
    do_GetService(NS_NAVBOOKMARKSSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the file:// uri for the bookmarks file.
  nsCOMPtr<nsIURI> fileURI;
  rv = NS_NewFileURI(getter_AddRefs(fileURI), aBookmarksFile);
  NS_ENSURE_SUCCESS(rv, rv);

  // Create an imported bookmarks folder under the bookmarks menu.
  PRInt64 root;
  rv = bms->GetBookmarksRoot(&root);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt64 folder;
  rv = bms->CreateFolder(root, importedBookmarksTitle, -1, &folder);
  NS_ENSURE_SUCCESS(rv, rv);

  // Import the bookmarks into the folder.
  rv = bms->ImportBookmarksHTMLToFolder(fileURI, folder);
  NS_ENSURE_SUCCESS(rv, rv);
#else
  nsCOMPtr<nsIRDFResource> folder;
  bms->CreateFolderInContainer(importedBookmarksTitle.get(), root, -1,
                               getter_AddRefs(folder));

  nsCOMPtr<nsIRDFResource> folderProp;
  rv = rdfs->GetResource(NC_URI(Folder), getter_AddRefs(folderProp));
  NS_ENSURE_SUCCESS(rv, rv);

  params->AppendElement(folderProp);
  params->AppendElement(folder);

  nsCOMPtr<nsISupportsArray> sources;
  rv = NS_NewISupportsArray(getter_AddRefs(sources));
  NS_ENSURE_SUCCESS(rv, rv);
  sources->AppendElement(folder);

  nsCOMPtr<nsIRDFDataSource> ds = do_QueryInterface(bms, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ds->DoCommand(sources, importCmd, params);
#endif
  return rv;
}
