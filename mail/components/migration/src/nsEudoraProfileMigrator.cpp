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
 * The Original Code is The Mail Profile Migrator.
 *
 * The Initial Developer of the Original Code is Scott MacGregor.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Scott MacGregor <mscott@mozilla.org>
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
#include "nsCRT.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIObserverService.h"
#include "nsIPrefLocalizedString.h"
#include "nsIPrefService.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsISupportsPrimitives.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsEudoraProfileMigrator.h"
#include "nsIProfileMigrator.h"
#include "nsVoidArray.h"

#include "nsIImportSettings.h"
#include "nsIFileSpec.h" // needed for an obsolete API


NS_IMPL_ISUPPORTS2(nsEudoraProfileMigrator, nsIMailProfileMigrator, nsITimerCallback)


nsEudoraProfileMigrator::nsEudoraProfileMigrator()
{
  mProcessingMailFolders = PR_FALSE;
  // get the import service
  mImportModule = do_CreateInstance("@mozilla.org/import/import-eudora;1");
}

nsEudoraProfileMigrator::~nsEudoraProfileMigrator()
{           
}

nsresult nsEudoraProfileMigrator::ContinueImport()
{
  return Notify(nsnull);
}

///////////////////////////////////////////////////////////////////////////////
// nsITimerCallback

NS_IMETHODIMP
nsEudoraProfileMigrator::Notify(nsITimer *timer)
{
  PRInt32 progress;
  mGenericImporter->GetProgress(&progress);
  
  nsAutoString index;
  index.AppendInt( progress ); 
  NOTIFY_OBSERVERS(MIGRATION_PROGRESS, index.get());
 
  if (progress == 100) // are we done yet?
  {
    if (mProcessingMailFolders)
      return FinishCopyingMailFolders();
    else
      return FinishCopyingAddressBookData();
  }
  else
  {
    // fire a timer to handle the next one. 
    mFileIOTimer = do_CreateInstance("@mozilla.org/timer;1");
    // if the progress = 100% let's pause for a second or two with a finished progessmeter before we move on
    mFileIOTimer->InitWithCallback(NS_STATIC_CAST(nsITimerCallback *, this), 100, nsITimer::TYPE_ONE_SHOT);
  }
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsIMailProfileMigrator

NS_IMETHODIMP
nsEudoraProfileMigrator::Migrate(PRUint16 aItems, nsIProfileStartup* aStartup, const PRUnichar* aProfile)
{
  nsresult rv = NS_OK;

  PRBool aReplace = PR_FALSE;

  if (aStartup) 
  {
    aReplace = PR_TRUE;
    rv = aStartup->DoStartup();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NOTIFY_OBSERVERS(MIGRATION_STARTED, nsnull);
  rv = ImportSettings(mImportModule);

  // now import address books
  // this routine will asynchronously import address book data and it will then kick off
  // the final migration step, copying the mail folders over.
  rv = ImportAddressBook(mImportModule);  

  // don't broadcast an on end migration here. We aren't done until our asynch import process says we are done.
  return rv;
}

NS_IMETHODIMP
nsEudoraProfileMigrator::GetMigrateData(const PRUnichar* aProfile, 
                                           PRBool aReplace, 
                                           PRUint16* aResult)
{
  // There's no harm in assuming everything is available.
  *aResult = nsIMailProfileMigrator::ACCOUNT_SETTINGS | nsIMailProfileMigrator::ADDRESSBOOK_DATA | 
             nsIMailProfileMigrator::MAILDATA;
  return NS_OK;
}

NS_IMETHODIMP
nsEudoraProfileMigrator::GetSourceExists(PRBool* aResult)
{
  *aResult = PR_FALSE;
  
  nsCOMPtr<nsIImportSettings> importSettings;
  mImportModule->GetImportInterface(NS_IMPORT_SETTINGS_STR, getter_AddRefs(importSettings));

  if (importSettings)
  {
    nsXPIDLString description;
    nsCOMPtr<nsIFileSpec> location;
    importSettings->AutoLocate(getter_Copies(description), getter_AddRefs(location), aResult);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEudoraProfileMigrator::GetSourceHasMultipleProfiles(PRBool* aResult)
{
  *aResult = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsEudoraProfileMigrator::GetSourceProfiles(nsISupportsArray** aResult)
{
  *aResult = nsnull;
  return NS_OK;
}

