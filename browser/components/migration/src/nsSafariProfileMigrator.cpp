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

#include "nsBrowserProfileMigratorUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsILocalFileMac.h"
#include "nsIObserverService.h"
#include "nsIProfile.h"
#include "nsIProfileInternal.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsISupportsPrimitives.h"
#include "nsSafariProfileMigrator.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CFNumber.h>

#define SAFARI_PREFERENCES_FILE_NAME NS_LITERAL_STRING("com.apple.Safari.plist")

///////////////////////////////////////////////////////////////////////////////
// nsSafariProfileMigrator

NS_IMPL_ISUPPORTS1(nsSafariProfileMigrator, nsIBrowserProfileMigrator)

static nsIObserverService* sObserverService = nsnull;

nsSafariProfileMigrator::nsSafariProfileMigrator()
{
  CallGetService("@mozilla.org/observer-service;1", &sObserverService);
}

nsSafariProfileMigrator::~nsSafariProfileMigrator()
{
  NS_IF_RELEASE(sObserverService);
}

///////////////////////////////////////////////////////////////////////////////
// nsIBrowserProfileMigrator

NS_IMETHODIMP
nsSafariProfileMigrator::Migrate(PRUint32 aItems, PRBool aReplace, const PRUnichar* aProfile)
{
  nsresult rv = NS_OK;

  NOTIFY_OBSERVERS(MIGRATION_STARTED, nsnull);
  
  COPY_DATA(CopyPreferences,  aReplace, nsIBrowserProfileMigrator::SETTINGS,  NS_LITERAL_STRING("settings").get());
  COPY_DATA(CopyCookies,      aReplace, nsIBrowserProfileMigrator::COOKIES,   NS_LITERAL_STRING("cookies").get());
  COPY_DATA(CopyHistory,      aReplace, nsIBrowserProfileMigrator::HISTORY,   NS_LITERAL_STRING("history").get());
  COPY_DATA(CopyBookmarks,    aReplace, nsIBrowserProfileMigrator::BOOKMARKS, NS_LITERAL_STRING("bookmarks").get());

  NOTIFY_OBSERVERS(MIGRATION_ENDED, nsnull);

  return rv;
}

NS_IMETHODIMP
nsSafariProfileMigrator::GetSourceHasMultipleProfiles(PRBool* aResult)
{
  *aResult = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsSafariProfileMigrator::GetSourceProfiles(nsISupportsArray** aResult)
{
  *aResult = nsnull;
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsSafariProfileMigrator

nsresult
GetPListFromFile(nsILocalFile* aPListFile, CFPropertyListRef* aResult)
{
  PRBool exists;
  aPListFile->Exists(&exists);
  nsCAutoString filePath;
  aPListFile->GetNativePath(filePath);
  if (!exists)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsILocalFileMac> macFile(do_QueryInterface(aPListFile));
  CFURLRef urlRef;
  macFile->GetCFURL(&urlRef);
  
  CFDataRef resourceData;

  SInt32 errorCode;
  Boolean status = ::CFURLCreateDataAndPropertiesFromResource(kCFAllocatorDefault, 
                                                              urlRef, 
                                                              &resourceData, 
                                                              NULL, 
                                                              NULL, 
                                                              &errorCode);
  if (!status)
    return NS_ERROR_FAILURE;
                                                            
  CFStringRef errorString;
  *aResult = ::CFPropertyListCreateFromXMLData(kCFAllocatorDefault,
                                               resourceData,
                                               kCFPropertyListImmutable,
                                               &errorString);
  ::CFRelease(resourceData);
  ::CFRelease(urlRef);
  
  return NS_OK;
}

nsresult
nsSafariProfileMigrator::CopyPreferences(PRBool aReplace)
{
  nsCOMPtr<nsIProperties> fileLocator(do_GetService("@mozilla.org/file/directory_service;1"));
  nsCOMPtr<nsILocalFile> safariPrefsFile;
  fileLocator->Get(NS_MAC_USER_LIB_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(safariPrefsFile));
  safariPrefsFile->Append(NS_LITERAL_STRING("Preferences"));
  safariPrefsFile->Append(SAFARI_PREFERENCES_FILE_NAME);

  CFDictionaryRef safariPrefs; 
  nsresult rv = GetPListFromFile(safariPrefsFile, (CFDictionaryRef*)&safariPrefs);
  if (NS_FAILED(rv)) 
    return NS_OK;
    
  Boolean hasValue = ::CFDictionaryContainsKey(safariPrefs, CFSTR("AlwaysShowTabBar"));
  if (hasValue) {
    CFBooleanRef boolValue = ::CFDictionaryGetValue(safariPrefs, CFSTR("AlwaysShowTabBar"));
    printf("*** AlwaysShowTabBar = %d", boolValue == kCFBooleanTrue);
  }
  
  ::CFRelease(safariPrefs);

  return NS_OK;
}

nsresult
nsSafariProfileMigrator::CopyCookies(PRBool aReplace)
{
  return NS_OK;
}

nsresult
nsSafariProfileMigrator::CopyHistory(PRBool aReplace)
{
  return NS_OK;
}

nsresult 
nsSafariProfileMigrator::CopyBookmarks(PRBool aReplace)
{
  return NS_OK;
}

