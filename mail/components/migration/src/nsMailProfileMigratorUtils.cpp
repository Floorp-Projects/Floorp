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

#include "nsMailProfileMigratorUtils.h"
#include "nsIFile.h"
#include "nsIProperties.h"
#include "nsIProfileMigrator.h"

#include "nsIServiceManagerUtils.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsXPCOMCID.h"
#include "nsCRT.h"

void SetProxyPref(const nsACString& aHostPort, const char* aPref, 
                  const char* aPortPref, nsIPrefBranch* aPrefs) 
{
  nsCAutoString hostPort(aHostPort);  
  PRInt32 portDelimOffset = hostPort.RFindChar(':');
  if (portDelimOffset > 0) {
    nsCAutoString host(Substring(hostPort, 0, portDelimOffset));
    nsCAutoString port(Substring(hostPort, portDelimOffset + 1, 
                                 hostPort.Length() - (portDelimOffset + 1)));
    
    aPrefs->SetCharPref(aPref, host.get());
    PRInt32 stringErr;
    PRInt32 portValue = port.ToInteger(&stringErr);
    aPrefs->SetIntPref(aPortPref, portValue);
  }
  else
    aPrefs->SetCharPref(aPref, hostPort.get());
}

void ParseOverrideServers(const char* aServers, nsIPrefBranch* aBranch)
{
  // Windows (and Opera) formats its proxy override list in the form:
  // server;server;server where server is a server name or ip address, 
  // or "<local>". Mozilla's format is server,server,server, and <local>
  // must be translated to "localhost,127.0.0.1"
  nsCAutoString override(aServers);
  PRInt32 left = 0, right = 0;
  for (;;) {
    right = override.FindChar(';', right);
    const nsACString& host = Substring(override, left, 
                                       (right < 0 ? override.Length() : right) - left);
    if (host.Equals("<local>"))
      override.Replace(left, 7, NS_LITERAL_CSTRING("localhost,127.0.0.1"));
    if (right < 0)
      break;
    left = right + 1;
    override.Replace(right, 1, NS_LITERAL_CSTRING(","));
  }
  aBranch->SetCharPref("network.proxy.no_proxies_on", override.get());
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
