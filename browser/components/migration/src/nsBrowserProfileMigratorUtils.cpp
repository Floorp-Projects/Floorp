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
#include "nsINavBookmarksService.h"
#include "nsBrowserCompsCID.h"
#include "nsToolkitCompsCID.h"
#include "nsIPlacesImportExportService.h"
#include "nsIFile.h"
#include "nsIProperties.h"
#include "nsIProfileMigrator.h"

#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsISupportsPrimitives.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsIRDFService.h"
#include "nsIStringBundle.h"
#include "nsXPCOMCID.h"

#define MIGRATION_BUNDLE "chrome://browser/locale/migration/migration.properties"

#define DEFAULT_BOOKMARKS NS_LITERAL_CSTRING("resource:///defaults/profile/bookmarks.html")

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
      nsresult stringErr;
      portValue = port.ToInteger(&stringErr);
      if (NS_SUCCEEDED(stringErr))
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
                             bool aReplace, nsIFile* aSourceProfile, 
                             PRUint16* aResult)
{
  nsCOMPtr<nsIFile> sourceFile; 
  bool exists;
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
    NS_Free(cursor->fileName);
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

