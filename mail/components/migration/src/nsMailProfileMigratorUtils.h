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

#ifndef mailprofilemigratorutils___h___
#define mailprofilemigratorutils___h___

#define MIGRATION_ITEMBEFOREMIGRATE "Migration:ItemBeforeMigrate"
#define MIGRATION_ITEMAFTERMIGRATE  "Migration:ItemAfterMigrate"
#define MIGRATION_STARTED           "Migration:Started"
#define MIGRATION_ENDED             "Migration:Ended"
#define MIGRATION_PROGRESS          "Migration:Progress"

#define NOTIFY_OBSERVERS(message, item) \
  mObserverService->NotifyObservers(nsnull, message, item)

#define COPY_DATA(func, replace, itemIndex) \
  if (NS_SUCCEEDED(rv) && (aItems & itemIndex || !aItems)) { \
    nsAutoString index; \
    index.AppendInt(itemIndex); \
    NOTIFY_OBSERVERS(MIGRATION_ITEMBEFOREMIGRATE, index.get()); \
    rv = func(replace); \
    NOTIFY_OBSERVERS(MIGRATION_ITEMAFTERMIGRATE, index.get()); \
  }

#include "nsIPrefBranch.h"
#include "nsIFile.h"
#include "nsString.h"
class nsIProfileStartup;

// Proxy utilities shared by the Opera and IE migrators
void ParseOverrideServers(const char* aServers, nsIPrefBranch* aBranch);
void SetProxyPref(const nsACString& aHostPort, const char* aPref, 
                  const char* aPortPref, nsIPrefBranch* aPrefs);

struct MigrationData { 
  PRUnichar* fileName; 
  PRUint32 sourceFlag;
  PRBool replaceOnly;
};

class nsILocalFile;
void GetMigrateDataFromArray(MigrationData* aDataArray, 
                             PRInt32 aDataArrayLength,
                             PRBool aReplace,
                             nsIFile* aSourceProfile, 
                             PRUint16* aResult);


// get the base directory of the *target* profile
// this is already cloned, modify it to your heart's content
void GetProfilePath(nsIProfileStartup* aStartup, nsCOMPtr<nsIFile>& aProfileDir);

#endif

