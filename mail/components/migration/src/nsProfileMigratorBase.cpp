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
#include "nsIPasswordManagerInternal.h"
#include "nsIPrefLocalizedString.h"
#include "nsIPrefService.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsISupportsPrimitives.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsProfileMigratorBase.h"
#include "nsIProfileMigrator.h"
#include "nsVoidArray.h"
#include "nsIMailProfileMigrator.h"

#include "nsIImportSettings.h"

#define kPersonalAddressbookUri "moz-abmdbdirectory://abook.mab"

nsProfileMigratorBase::nsProfileMigratorBase()
{
  mObserverService = do_GetService("@mozilla.org/observer-service;1");
  mProcessingMailFolders = PR_FALSE;
}

nsProfileMigratorBase::~nsProfileMigratorBase()
{           
}

nsresult nsProfileMigratorBase::ImportSettings(nsIImportModule * aImportModule)
{  
  nsresult rv; 
  
  nsAutoString index;
  index.AppendInt(nsIMailProfileMigrator::ACCOUNT_SETTINGS); 
  NOTIFY_OBSERVERS(MIGRATION_ITEMBEFOREMIGRATE, index.get());

  nsCOMPtr<nsIImportSettings> importSettings;
  rv = aImportModule->GetImportInterface(NS_IMPORT_SETTINGS_STR, getter_AddRefs(importSettings));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool importedSettings = PR_FALSE;

  rv = importSettings->Import(getter_AddRefs(mLocalFolderAccount), &importedSettings);

  NOTIFY_OBSERVERS(MIGRATION_ITEMAFTERMIGRATE, index.get());

  return rv;
}

nsresult nsProfileMigratorBase::ImportAddressBook(nsIImportModule * aImportModule)
{  
  nsresult rv; 
  
  nsAutoString index;
  index.AppendInt(nsIMailProfileMigrator::ADDRESSBOOK_DATA); 
  NOTIFY_OBSERVERS(MIGRATION_ITEMBEFOREMIGRATE, index.get());

  rv = aImportModule->GetImportInterface(NS_IMPORT_ADDRESS_STR, getter_AddRefs(mGenericImporter));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupportsCString> pabString = do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // we want to migrate the outlook express addressbook into our personal address book
  pabString->SetData(nsDependentCString(kPersonalAddressbookUri));
  mGenericImporter->SetData("addressDestination", pabString);

  PRBool importResult;
  PRBool wantsProgress;
  mGenericImporter->WantsProgress(&wantsProgress);
  rv = mGenericImporter->BeginImport(nsnull, nsnull, PR_TRUE, &importResult);

  if (wantsProgress)
    ContinueImport();
  else
    FinishCopyingAddressBookData();

  return rv;
}

nsresult nsProfileMigratorBase::FinishCopyingAddressBookData()
{
  nsAutoString index;
  index.AppendInt(nsIMailProfileMigrator::ADDRESSBOOK_DATA); 
  NOTIFY_OBSERVERS(MIGRATION_ITEMAFTERMIGRATE, index.get());

  // now kick off the mail migration code
  ImportMailData(mImportModule);

  return NS_OK;
}

nsresult nsProfileMigratorBase::ImportMailData(nsIImportModule * aImportModule)
{
  nsresult rv; 
  
  nsAutoString index;
  index.AppendInt(nsIMailProfileMigrator::MAILDATA); 
  NOTIFY_OBSERVERS(MIGRATION_ITEMBEFOREMIGRATE, index.get());

  rv = aImportModule->GetImportInterface(NS_IMPORT_MAIL_STR, getter_AddRefs(mGenericImporter));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupportsPRBool> migrating = do_CreateInstance(NS_SUPPORTS_PRBOOL_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // by setting the migration flag, we force the import utility to install local folders from OE
  // directly into Local Folders and not as a subfolder
  migrating->SetData(PR_TRUE);
  mGenericImporter->SetData("migration", migrating);

  PRBool importResult;
  PRBool wantsProgress;
  mGenericImporter->WantsProgress(&wantsProgress);
  rv = mGenericImporter->BeginImport(nsnull, nsnull, PR_TRUE, &importResult);

  mProcessingMailFolders = PR_TRUE;

  if (wantsProgress)
    ContinueImport();
  else
    FinishCopyingMailFolders();

  return rv;
}

nsresult nsProfileMigratorBase::FinishCopyingMailFolders()
{
  nsAutoString index;
  index.AppendInt(nsIMailProfileMigrator::MAILDATA); 
  NOTIFY_OBSERVERS(MIGRATION_ITEMAFTERMIGRATE, index.get());

  // migration is now done...notify the UI.
  NOTIFY_OBSERVERS(MIGRATION_ENDED, nsnull);
  return NS_OK;
}


