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

#include "nsAppDirectoryServiceDefs.h"
#include "nsBrowserProfileMigratorUtils.h"
#include "nsCRT.h"
#include "nsDogbertProfileMigrator.h"
#include "nsICookieManager2.h"
#include "nsIFile.h"
#include "nsIInputStream.h"
#include "nsILineInputStream.h"
#include "nsIObserverService.h"
#include "nsIOutputStream.h"
#include "nsIPrefBranch.h"
#include "nsIPrefLocalizedString.h"
#include "nsIPrefService.h"
#include "nsIProfile.h"
#include "nsIProfileInternal.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsISupportsPrimitives.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsReadableUtils.h"
#include "prprf.h"

#define PREF_FILE_HEADER_STRING "# Mozilla User Preferences    " 

#if defined(XP_UNIX) && !defined(XP_MACOSX)
#define PREF_FILE_NAME_IN_4x      NS_LITERAL_STRING("preferences.js")
#define COOKIES_FILE_NAME_IN_4x   NS_LITERAL_STRING("cookies")
#define BOOKMARKS_FILE_NAME_IN_4x NS_LITERAL_STRING("bookmarks.html")
#define PSM_CERT7_DB              NS_LITERAL_STRING("cert7.db")
#define PSM_KEY3_DB               NS_LITERAL_STRING("key3.db")
#define PSM_SECMODULE_DB          NS_LITERAL_STRING("secmodule.db")
#elif defined(XP_MAC) || defined(XP_MACOSX)
#define PREF_FILE_NAME_IN_4x      NS_LITERAL_STRING("Netscape Preferences")
#define COOKIES_FILE_NAME_IN_4x   NS_LITERAL_STRING("MagicCookie")
#define BOOKMARKS_FILE_NAME_IN_4x NS_LITERAL_STRING("Bookmarks.html")
#define SECURITY_PATH             "Security"
#define PSM_CERT7_DB              NS_LITERAL_STRING("Certificates7")
#define PSM_KEY3_DB               NS_LITERAL_STRING("Key Database3")
#define PSM_SECMODULE_DB          NS_LITERAL_STRING("Security Modules")
#else /* XP_WIN || XP_OS2 */
#define PREF_FILE_NAME_IN_4x      NS_LITERAL_STRING("prefs.js")
#define COOKIES_FILE_NAME_IN_4x   NS_LITERAL_STRING("cookies.txt")
#define BOOKMARKS_FILE_NAME_IN_4x NS_LITERAL_STRING("bookmark.htm")
#define PSM_CERT7_DB              NS_LITERAL_STRING("cert7.db")
#define PSM_KEY3_DB               NS_LITERAL_STRING("key3.db")
#define PSM_SECMODULE_DB          NS_LITERAL_STRING("secmod.db")
#endif /* XP_UNIX */

#define COOKIES_FILE_NAME_IN_5x   NS_LITERAL_STRING("cookies.txt")
#define BOOKMARKS_FILE_NAME_IN_5x NS_LITERAL_STRING("bookmarks.html")
#define PREF_FILE_NAME_IN_5x      NS_LITERAL_STRING("prefs.js")

///////////////////////////////////////////////////////////////////////////////
// nsDogbertProfileMigrator

NS_IMPL_ISUPPORTS1(nsDogbertProfileMigrator, nsIBrowserProfileMigrator)

nsDogbertProfileMigrator::nsDogbertProfileMigrator()
{
  mObserverService = do_GetService("@mozilla.org/observer-service;1");
}

nsDogbertProfileMigrator::~nsDogbertProfileMigrator()
{
}

///////////////////////////////////////////////////////////////////////////////
// nsIBrowserProfileMigrator

NS_IMETHODIMP
nsDogbertProfileMigrator::Migrate(PRUint16 aItems, PRBool aReplace, const PRUnichar* aProfile)
{
  nsresult rv = NS_OK;

  if (!mTargetProfile) 
    GetTargetProfile(aProfile, aReplace);
  if (!mSourceProfile)
    GetSourceProfile(aProfile);

  NOTIFY_OBSERVERS(MIGRATION_STARTED, nsnull);

  COPY_DATA(CopyPreferences,  aReplace, nsIBrowserProfileMigrator::SETTINGS);
  COPY_DATA(CopyCookies,      aReplace, nsIBrowserProfileMigrator::COOKIES);
  COPY_DATA(CopyBookmarks,    aReplace, nsIBrowserProfileMigrator::BOOKMARKS);

  NOTIFY_OBSERVERS(MIGRATION_ENDED, nsnull);

  return rv;
}

void
nsDogbertProfileMigrator::GetSourceProfile(const PRUnichar* aProfile)
{
  // XXXben I would actually prefer we do this by reading the 4.x registry, rather than
  // relying on the 5.x registry knowing about 4.x profiles, in case we remove profile
  // manager support from Firefox. 
  nsCOMPtr<nsIProfileInternal> pmi(do_GetService("@mozilla.org/profile/manager;1"));
  pmi->GetOriginalProfileDir(aProfile, getter_AddRefs(mSourceProfile));
}

NS_IMETHODIMP
nsDogbertProfileMigrator::GetMigrateData(const PRUnichar* aProfile, 
                                         PRBool aReplace,
                                         PRUint16* aResult)
{
  *aResult = 0;
  if (!mSourceProfile) {
    GetSourceProfile(aProfile);
    if (!mSourceProfile) 
      return NS_ERROR_FILE_NOT_FOUND;
  }

  PRBool exists;
  MigrationData data[] = { { ToNewUnicode(PREF_FILE_NAME_IN_4x),
                             nsIBrowserProfileMigrator::SETTINGS,
                             PR_TRUE },
                           { ToNewUnicode(COOKIES_FILE_NAME_IN_4x),
                             nsIBrowserProfileMigrator::COOKIES,
                             PR_FALSE },
                           { ToNewUnicode(BOOKMARKS_FILE_NAME_IN_4x),
                             nsIBrowserProfileMigrator::BOOKMARKS,
                             PR_FALSE } };
                                                                  
  // Frees file name strings allocated above.
  GetMigrateDataFromArray(data, sizeof(data)/sizeof(MigrationData), 
                          aReplace, mSourceProfile, aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsDogbertProfileMigrator::GetSourceExists(PRBool* aResult)
{
  nsCOMPtr<nsISupportsArray> profiles;
  GetSourceProfiles(getter_AddRefs(profiles));

  if (profiles) { 
    PRUint32 count;
    profiles->Count(&count);
    *aResult = count > 0;
  }
  else
    *aResult = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP
nsDogbertProfileMigrator::GetSourceHasMultipleProfiles(PRBool* aResult)
{
  nsCOMPtr<nsISupportsArray> profiles;
  GetSourceProfiles(getter_AddRefs(profiles));

  if (profiles) {
    PRUint32 count;
    profiles->Count(&count);
    *aResult = count > 1;
  }
  else
    *aResult = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP
nsDogbertProfileMigrator::GetSourceProfiles(nsISupportsArray** aResult)
{
  if (!mProfiles) {
    nsresult rv = NS_NewISupportsArray(getter_AddRefs(mProfiles));
    if (NS_FAILED(rv)) return rv;

    // XXXben - this is a little risky.. let's make this actually go and use the 
    // 4.x registry instead... 
    // Our profile manager stores information about the set of Dogbert Profiles we have.
    nsCOMPtr<nsIProfileInternal> pmi(do_CreateInstance("@mozilla.org/profile/manager;1"));
    PRUnichar** profileNames = nsnull;
    PRUint32 profileCount = 0;
    // Lordy, this API sucketh.
    rv = pmi->GetProfileListX(nsIProfileInternal::LIST_FOR_IMPORT, &profileCount, &profileNames);
    if (NS_FAILED(rv)) return rv;

    for (PRUint32 i = 0; i < profileCount; ++i) {
      nsCOMPtr<nsISupportsString> string(do_CreateInstance("@mozilla.org/supports-string;1"));
      string->SetData(nsDependentString(profileNames[i]));
      mProfiles->AppendElement(string);
    }
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(profileCount, profileNames);
  }
  
  NS_IF_ADDREF(*aResult = mProfiles);
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsDogbertProfileMigrator
#define F(a) nsDogbertProfileMigrator::a

static 
nsDogbertProfileMigrator::PrefTransform gTransforms[] = {
  // Simple Copy Prefs
  { "browser.anchor_color",           0, F(GetString),   F(SetString), PR_FALSE, -1 },
  { "browser.visited_color",          0, F(GetString),   F(SetString), PR_FALSE, -1 },
  { "browser.startup.homepage",       0, F(GetString),   F(SetString), PR_FALSE, -1 },
  { "security.enable_java",           0, F(GetBool),     F(SetBool),   PR_FALSE, -1 },
  { "network.cookie.cookieBehavior",  0, F(GetInt),      F(SetInt),    PR_FALSE, -1 },
  { "network.cookie.warnAboutCookies",0, F(GetBool),     F(SetBool),   PR_FALSE, -1 },
  { "javascript.enabled",             0, F(GetBool),     F(SetBool),   PR_FALSE, -1 },
  { "network.proxy.type",             0, F(GetInt),      F(SetInt),    PR_FALSE, -1 },
  { "network.proxy.no_proxies_on",    0, F(GetString),   F(SetString), PR_FALSE, -1 },
  { "network.proxy.autoconfig_url",   0, F(GetString),   F(SetString), PR_FALSE, -1 },
  { "network.proxy.ftp",              0, F(GetString),   F(SetString), PR_FALSE, -1 },
  { "network.proxy.ftp_port",         0, F(GetInt),      F(SetInt),    PR_FALSE, -1 },
  { "network.proxy.gopher",           0, F(GetString),   F(SetString), PR_FALSE, -1 },
  { "network.proxy.gopher_port",      0, F(GetInt),      F(SetInt),    PR_FALSE, -1 },
  { "network.proxy.http",             0, F(GetString),   F(SetString), PR_FALSE, -1 },
  { "network.proxy.http_port",        0, F(GetInt),      F(SetInt),    PR_FALSE, -1 },
  { "network.proxy.ssl",              0, F(GetString),   F(SetString), PR_FALSE, -1 },
  { "network.proxy.ssl_port",         0, F(GetInt),      F(SetInt),    PR_FALSE, -1 },

  // Prefs with Different Names
  { "network.hosts.socks_server",           "network.proxy.socks",                F(GetString),   F(SetString),  PR_FALSE, -1 },
  { "network.hosts.socks_serverport",       "network.proxy.socks_port",           F(GetInt),      F(SetInt),     PR_FALSE, -1 },
  { "browser.background_color",             "browser.display.background_color",   F(GetString),   F(SetString),  PR_FALSE, -1 },
  { "browser.foreground_color",             "browser.display.foreground_color",   F(GetString),   F(SetString),  PR_FALSE, -1 },
  { "browser.wfe.use_windows_colors",       "browser.display.use_system_colors",  F(GetBool),     F(SetBool),    PR_FALSE, -1 },
  { "browser.use_document_colors",          "browser.display.use_document_colors",F(GetBool),     F(SetBool),    PR_FALSE, -1 },
  { "browser.use_document.fonts",           "browser.display.use_document_fonts", F(GetInt),      F(SetInt),     PR_FALSE, -1 },
  { "browser.link_expiration",              "browser.history_expire_days",        F(GetInt),      F(SetInt),     PR_FALSE, -1 },
  { "browser.startup.page",                 "browser.startup.homepage",           F(GetHomepage), F(SetWStringFromASCII), PR_FALSE, -1 },
  { "general.always_load_images",           "network.image.imageBehavior",        F(GetImagePref),F(SetInt),     PR_FALSE, -1 },
};

nsresult
nsDogbertProfileMigrator::TransformPreferences(const nsAString& aSourcePrefFileName,
                                               const nsAString& aTargetPrefFileName)
{
  PrefTransform* transform;
  PrefTransform* end = gTransforms + sizeof(gTransforms)/sizeof(PrefTransform);

  // Load the source pref file
  nsCOMPtr<nsIPrefService> psvc(do_GetService(NS_PREFSERVICE_CONTRACTID));
  psvc->ResetPrefs();

  nsCOMPtr<nsIFile> sourcePrefsFile;
  mSourceProfile->Clone(getter_AddRefs(sourcePrefsFile));
  sourcePrefsFile->Append(aSourcePrefFileName);
  psvc->ReadUserPrefs(sourcePrefsFile);

  nsCOMPtr<nsIPrefBranch> branch(do_QueryInterface(psvc));
  for (transform = gTransforms; transform < end; ++transform)
    transform->prefGetterFunc(transform, branch);

  // Now that we have all the pref data in memory, load the target pref file,
  // and write it back out
  psvc->ResetPrefs();
  for (transform = gTransforms; transform < end; ++transform)
    transform->prefSetterFunc(transform, branch);

  nsCOMPtr<nsIFile> targetPrefsFile;
  mTargetProfile->Clone(getter_AddRefs(targetPrefsFile));
  targetPrefsFile->Append(aTargetPrefFileName);
  psvc->SavePrefFile(targetPrefsFile);

  return NS_OK;
}

nsresult
nsDogbertProfileMigrator::CopyPreferences(PRBool aReplace)
{
  nsresult rv = NS_OK;

  if (!aReplace)
    return rv;

  // 1) Copy Preferences
  TransformPreferences(PREF_FILE_NAME_IN_4x, PREF_FILE_NAME_IN_5x);

  // 2) Copy Certficates
  rv |= CopyFile(PSM_CERT7_DB,      PSM_CERT7_DB);
  rv |= CopyFile(PSM_KEY3_DB,       PSM_KEY3_DB);
  rv |= CopyFile(PSM_SECMODULE_DB,  PSM_SECMODULE_DB);

  return rv;
}

nsresult 
nsDogbertProfileMigrator::GetHomepage(void* aTransform, nsIPrefBranch* aBranch)
{
  PrefTransform* xform = (PrefTransform*)aTransform;
  PRInt32 val;
  nsresult rv = aBranch->GetIntPref(xform->sourcePrefName, &val);
  if (NS_SUCCEEDED(rv) && val == 0) {
    xform->stringValue = "about:blank";
    xform->prefHasValue = PR_TRUE;
  }
  return rv;
}

nsresult 
nsDogbertProfileMigrator::GetImagePref(void* aTransform, nsIPrefBranch* aBranch)
{
  PrefTransform* xform = (PrefTransform*)aTransform;
  PRBool loadImages;
  nsresult rv = aBranch->GetBoolPref(xform->sourcePrefName, &loadImages);
  if (NS_SUCCEEDED(rv)) {
    xform->intValue = loadImages ? 0 : 2;
    xform->prefHasValue = PR_TRUE;
  }
  return rv;
}

nsresult
nsDogbertProfileMigrator::CopyCookies(PRBool aReplace)
{
  nsresult rv;
  if (aReplace) {
#ifdef NEED_TO_FIX_4X_COOKIES
    nsresult rv = CopyFile(COOKIES_FILE_NAME_IN_4x, COOKIES_FILE_NAME_IN_5x);
    if (NS_FAILED(rv)) return rv;

    rv = FixDogbertCookies();
#else
    rv = CopyFile(COOKIES_FILE_NAME_IN_4x, COOKIES_FILE_NAME_IN_5x);
#endif
  }
  else {
    nsCOMPtr<nsICookieManager2> cookieManager(do_GetService(NS_COOKIEMANAGER_CONTRACTID));
    if (!cookieManager)
      return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr<nsIFile> dogbertCookiesFile;
    mSourceProfile->Clone(getter_AddRefs(dogbertCookiesFile));
    dogbertCookiesFile->Append(COOKIES_FILE_NAME_IN_4x);

    rv = ImportNetscapeCookies(dogbertCookiesFile);
  }
  return rv;
}

#if NEED_TO_FIX_4X_COOKIES
nsresult
nsDogbertProfileMigrator::FixDogbertCookies()
{
  nsCOMPtr<nsIFile> dogbertCookiesFile;
  mSourceProfile->Clone(getter_AddRefs(dogbertCookiesFile));
  dogbertCookiesFile->Append(COOKIES_FILE_NAME_IN_4x);

  nsCOMPtr<nsIInputStream> fileInputStream;
  NS_NewLocalFileInputStream(getter_AddRefs(fileInputStream), dogbertCookiesFile);
  if (!fileInputStream) return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIFile> firebirdCookiesFile;
  mTargetProfile->Clone(getter_AddRefs(firebirdCookiesFile));
  firebirdCookiesFile->Append(COOKIES_FILE_NAME_IN_5x);

  nsCOMPtr<nsIOutputStream> fileOutputStream;
  NS_NewLocalFileOutputStream(getter_AddRefs(fileOutputStream), firebirdCookiesFile);
  if (!fileOutputStream) return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsILineInputStream> lineInputStream(do_QueryInterface(fileInputStream));
  nsCAutoString buffer, outBuffer;
  PRBool moreData = PR_FALSE;
  PRUint32 written = 0;
  do {
    nsresult rv = lineInputStream->ReadLine(buffer, &moreData);
    if (NS_FAILED(rv)) return rv;
    
    if (!moreData)
      break;

    // skip line if it is a comment or null line
    if (buffer.IsEmpty() || buffer.CharAt(0) == '#' ||
        buffer.CharAt(0) == nsCRT::CR || buffer.CharAt(0) == nsCRT::LF) {
      fileOutputStream->Write(buffer.get(), buffer.Length(), &written);
      continue;
    }

    // locate expire field, skip line if it does not contain all its fields
    int hostIndex, isDomainIndex, pathIndex, xxxIndex, expiresIndex, nameIndex, cookieIndex;
    hostIndex = 0;
    if ((isDomainIndex = buffer.FindChar('\t', hostIndex)+1) == 0 ||
        (pathIndex = buffer.FindChar('\t', isDomainIndex)+1) == 0 ||
        (xxxIndex = buffer.FindChar('\t', pathIndex)+1) == 0 ||
        (expiresIndex = buffer.FindChar('\t', xxxIndex)+1) == 0 ||
        (nameIndex = buffer.FindChar('\t', expiresIndex)+1) == 0 ||
        (cookieIndex = buffer.FindChar('\t', nameIndex)+1) == 0 )
      continue;

    // separate the expires field from the rest of the cookie line
    nsCAutoString prefix, expiresString, suffix;
    buffer.Mid(prefix, hostIndex, expiresIndex-hostIndex-1);
    buffer.Mid(expiresString, expiresIndex, nameIndex-expiresIndex-1);
    buffer.Mid(suffix, nameIndex, buffer.Length()-nameIndex);

    // correct the expires field
    char* expiresCString = ToNewCString(expiresString);
    unsigned long expires = strtoul(expiresCString, nsnull, 10);
    nsCRT::free(expiresCString);

    // if the cookie is supposed to expire at the end of the session
    // expires == 0.  don't adjust those cookies.
    if (expires)
    	expires -= SECONDS_BETWEEN_1900_AND_1970;
    char dateString[36];
    PR_snprintf(dateString, sizeof(dateString), "%lu", expires);

    // generate the output buffer and write it to file
    outBuffer = prefix;
    outBuffer.Append('\t');
    outBuffer.Append(dateString);
    outBuffer.Append('\t');
    outBuffer.Append(suffix);

    fileOutputStream->Write(outBuffer.get(), outBuffer.Length(), &written);
  }
  while (1);
  
  return NS_OK;
}

#endif // NEED_TO_FIX_4X_COOKIES


nsresult
nsDogbertProfileMigrator::CopyBookmarks(PRBool aReplace)
{
  // If we're blowing away existing content, just copy the file, don't do fancy importing.
  if (aReplace)
    return MigrateDogbertBookmarks();

  return ImportNetscapeBookmarks(BOOKMARKS_FILE_NAME_IN_4x, 
                                 NS_LITERAL_STRING("sourceNameDogbert").get());
}

nsresult
nsDogbertProfileMigrator::MigrateDogbertBookmarks()
{
  nsresult rv;

  // Find out what the personal toolbar folder was called, this is stored in a pref
  // in 4.x
  nsCOMPtr<nsIPrefService> psvc(do_GetService(NS_PREFSERVICE_CONTRACTID));
  psvc->ResetPrefs();

  nsCOMPtr<nsIFile> dogbertPrefsFile;
  mSourceProfile->Clone(getter_AddRefs(dogbertPrefsFile));
  dogbertPrefsFile->Append(PREF_FILE_NAME_IN_4x);
  psvc->ReadUserPrefs(dogbertPrefsFile);

  nsXPIDLCString toolbarName;
  nsCOMPtr<nsIPrefBranch> branch(do_QueryInterface(psvc));
  rv = branch->GetCharPref("custtoolbar.personal_toolbar_folder", getter_Copies(toolbarName));
  // If the pref wasn't set in the user's 4.x preferences, there's no way we can "Fix" the
  // file when importing it to set the personal toolbar folder correctly, so don't bother
  // with the more involved file correction procedure and just copy the file over. 
  if (NS_FAILED(rv))
    return CopyFile(BOOKMARKS_FILE_NAME_IN_4x, BOOKMARKS_FILE_NAME_IN_5x);

  // Now read the 4.x bookmarks file, correcting the Personal Toolbar Folder line
  // and writing to the new location.
  nsCOMPtr<nsIFile> sourceBookmarksFile;
  mSourceProfile->Clone(getter_AddRefs(sourceBookmarksFile));
  sourceBookmarksFile->Append(BOOKMARKS_FILE_NAME_IN_4x);

  nsCOMPtr<nsIInputStream> fileInputStream;
  NS_NewLocalFileInputStream(getter_AddRefs(fileInputStream), sourceBookmarksFile);
  if (!fileInputStream) return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIFile> targetBookmarksFile;
  mTargetProfile->Clone(getter_AddRefs(targetBookmarksFile));
  targetBookmarksFile->Append(BOOKMARKS_FILE_NAME_IN_5x);

  nsCOMPtr<nsIOutputStream> outputStream;
  NS_NewLocalFileOutputStream(getter_AddRefs(outputStream), targetBookmarksFile);
  if (!outputStream) return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsILineInputStream> lineInputStream(do_QueryInterface(fileInputStream));
  nsCAutoString sourceBuffer;
  nsCAutoString targetBuffer;
  PRBool moreData = PR_FALSE;
  PRUint32 bytesWritten = 0;
  do {
    nsresult rv = lineInputStream->ReadLine(sourceBuffer, &moreData);
    if (NS_FAILED(rv)) return rv;

    if (!moreData)
      break;

    PRInt32 nameOffset = sourceBuffer.Find(toolbarName);
    if (nameOffset >= 0) {
      // Found the personal toolbar name on a line, check to make sure it's actually a folder. 
      NS_NAMED_LITERAL_CSTRING(folderPrefix, "<DT><H3 ");
      PRInt32 folderPrefixOffset = sourceBuffer.Find(folderPrefix);
      if (folderPrefixOffset >= 0)
        sourceBuffer.Insert(NS_LITERAL_CSTRING("PERSONAL_TOOLBAR_FOLDER=\"true\" "), 
                            folderPrefixOffset + folderPrefix.Length());
    }

    targetBuffer.Assign(sourceBuffer);
    targetBuffer.Append("\r\n");
    outputStream->Write(targetBuffer.get(), targetBuffer.Length(), &bytesWritten);
  }
  while (1);

  return NS_OK;
}
