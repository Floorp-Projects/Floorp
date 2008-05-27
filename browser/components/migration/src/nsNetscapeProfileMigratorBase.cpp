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
#include "nsICookieManager2.h"
#include "nsIFile.h"
#include "nsILineInputStream.h"
#include "nsIOutputStream.h"
#include "nsIPrefBranch.h"
#include "nsIPrefLocalizedString.h"
#include "nsIPrefService.h"
#include "NSReg.h"
#include "nsIServiceManager.h"
#include "nsISupportsPrimitives.h"
#include "nsIURL.h"
#include "nsNetscapeProfileMigratorBase.h"
#include "nsNetUtil.h"
#include "prtime.h"
#include "prprf.h"

#ifdef XP_MACOSX
#define NEED_TO_FIX_4X_COOKIES 1
#define SECONDS_BETWEEN_1900_AND_1970 2208988800UL
#endif /* XP_MACOSX */

#define FILE_NAME_PREFS_5X NS_LITERAL_STRING("prefs.js")

///////////////////////////////////////////////////////////////////////////////
// nsNetscapeProfileMigratorBase
nsNetscapeProfileMigratorBase::nsNetscapeProfileMigratorBase()
{
}

static nsresult
regerr2nsresult(REGERR errCode)
{
  switch (errCode) {
    case REGERR_PARAM:
    case REGERR_BADTYPE:
    case REGERR_BADNAME:
      return NS_ERROR_INVALID_ARG;

    case REGERR_MEMORY:
      return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_ERROR_FAILURE;
}

nsresult
nsNetscapeProfileMigratorBase::GetProfileDataFromRegistry(nsILocalFile* aRegistryFile,
                                                          nsISupportsArray* aProfileNames,
                                                          nsISupportsArray* aProfileLocations)
{
  nsresult rv;
  REGERR errCode;

  // Ensure aRegistryFile exists before open it
  PRBool regFileExists = PR_FALSE;
  rv = aRegistryFile->Exists(&regFileExists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!regFileExists)
    return NS_ERROR_FILE_NOT_FOUND;

  // Open It
  nsCAutoString regPath;
  rv = aRegistryFile->GetNativePath(regPath);
  NS_ENSURE_SUCCESS(rv, rv);

  if ((errCode = NR_StartupRegistry()))
    return regerr2nsresult(errCode);

  HREG reg;
  if ((errCode = NR_RegOpen(regPath.get(), &reg))) {
    NR_ShutdownRegistry();

    return regerr2nsresult(errCode);
  }

  RKEY profilesTree;
  if ((errCode = NR_RegGetKey(reg, ROOTKEY_COMMON, "Profiles", &profilesTree))) {
    NR_RegClose(reg);
    NR_ShutdownRegistry();

    return regerr2nsresult(errCode);
  }

  char profileStr[MAXREGPATHLEN];
  REGENUM enumState = nsnull;

  while (!NR_RegEnumSubkeys(reg, profilesTree, &enumState, profileStr,
                            sizeof(profileStr), REGENUM_CHILDREN))
  {
    RKEY profileKey;
    if (NR_RegGetKey(reg, profilesTree, profileStr, &profileKey))
      continue;

    // "migrated" is "yes" for all valid Seamonkey profiles. It is only "no"
    // for 4.x profiles. 
    char migratedStr[3];
    errCode = NR_RegGetEntryString(reg, profileKey, "migrated",
                                   migratedStr, sizeof(migratedStr));
    if ((errCode != REGERR_OK && errCode != REGERR_BUFTOOSMALL) ||
        strcmp(migratedStr, "no") == 0)
      continue;

    // Get the profile location and add it to the locations array
    REGINFO regInfo;
    regInfo.size = sizeof(REGINFO);

    if (NR_RegGetEntryInfo(reg, profileKey, "directory", &regInfo))
      continue;

    nsCAutoString dirStr;
    dirStr.SetLength(regInfo.entryLength);

    errCode = NR_RegGetEntryString(reg, profileKey, "directory",
                                   dirStr.BeginWriting(), regInfo.entryLength);
    // Remove trailing \0
    dirStr.SetLength(regInfo.entryLength-1);

    nsCOMPtr<nsILocalFile> dir;
#ifdef XP_MACOSX
    rv = NS_NewNativeLocalFile(EmptyCString(), PR_TRUE, getter_AddRefs(dir));
    if (NS_FAILED(rv)) break;
    dir->SetPersistentDescriptor(dirStr);
#else
    rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(dirStr), PR_TRUE,
                         getter_AddRefs(dir));
    if (NS_FAILED(rv)) break;
#endif

    PRBool exists;
    dir->Exists(&exists);

    if (exists) {
      aProfileLocations->AppendElement(dir);

      // Get the profile name and add it to the names array
      nsString profileName;
      CopyUTF8toUTF16(nsDependentCString(profileStr), profileName);

      nsCOMPtr<nsISupportsString> profileNameString(
        do_CreateInstance("@mozilla.org/supports-string;1"));

      profileNameString->SetData(profileName);
      aProfileNames->AppendElement(profileNameString);
    }
  }
  NR_RegClose(reg);
  NR_ShutdownRegistry();

  return rv;
}

#define GETPREF(xform, method, value) \
  nsresult rv = aBranch->method(xform->sourcePrefName, value); \
  if (NS_SUCCEEDED(rv)) \
    xform->prefHasValue = PR_TRUE; \
  return rv;

#define SETPREF(xform, method, value) \
  if (xform->prefHasValue) { \
    return aBranch->method(xform->targetPrefName ? xform->targetPrefName : xform->sourcePrefName, value); \
  } \
  return NS_OK;

nsresult 
nsNetscapeProfileMigratorBase::GetString(void* aTransform, nsIPrefBranch* aBranch)
{
  PrefTransform* xform = (PrefTransform*)aTransform;
  GETPREF(xform, GetCharPref, &xform->stringValue);
}

nsresult 
nsNetscapeProfileMigratorBase::SetString(void* aTransform, nsIPrefBranch* aBranch)
{
  PrefTransform* xform = (PrefTransform*)aTransform;
  SETPREF(xform, SetCharPref, xform->stringValue);
}

nsresult
nsNetscapeProfileMigratorBase::GetWString(void* aTransform, nsIPrefBranch* aBranch)
{
  PrefTransform* xform = (PrefTransform*)aTransform;
  nsCOMPtr<nsIPrefLocalizedString> prefValue;
  nsresult rv = aBranch->GetComplexValue(xform->sourcePrefName, 
                                         NS_GET_IID(nsIPrefLocalizedString),
                                         getter_AddRefs(prefValue));

  if (NS_SUCCEEDED(rv) && prefValue) {
    nsString data;
    prefValue->ToString(getter_Copies(data));

    xform->stringValue = ToNewCString(NS_ConvertUTF16toUTF8(data));
    xform->prefHasValue = PR_TRUE;
  }
  return rv;
}

nsresult 
nsNetscapeProfileMigratorBase::SetWStringFromASCII(void* aTransform, nsIPrefBranch* aBranch)
{
  PrefTransform* xform = (PrefTransform*)aTransform;
  if (xform->prefHasValue) {
    nsCOMPtr<nsIPrefLocalizedString> pls(do_CreateInstance("@mozilla.org/pref-localizedstring;1"));
    NS_ConvertUTF8toUTF16 data(xform->stringValue);
    pls->SetData(data.get());
    return aBranch->SetComplexValue(xform->targetPrefName ? xform->targetPrefName : xform->sourcePrefName, NS_GET_IID(nsIPrefLocalizedString), pls);
  }
  return NS_OK;
}

nsresult
nsNetscapeProfileMigratorBase::SetWString(void* aTransform, nsIPrefBranch* aBranch)
{
  PrefTransform* xform = (PrefTransform*)aTransform;
  if (xform->prefHasValue) {
    nsCOMPtr<nsIPrefLocalizedString> pls(do_CreateInstance("@mozilla.org/pref-localizedstring;1"));
    nsAutoString data = NS_ConvertUTF8toUTF16(xform->stringValue);
    pls->SetData(data.get());
    return aBranch->SetComplexValue(xform->targetPrefName ? xform->targetPrefName : xform->sourcePrefName, NS_GET_IID(nsIPrefLocalizedString), pls);
  }
  return NS_OK;
}


nsresult 
nsNetscapeProfileMigratorBase::GetBool(void* aTransform, nsIPrefBranch* aBranch)
{
  PrefTransform* xform = (PrefTransform*)aTransform;
  GETPREF(xform, GetBoolPref, &xform->boolValue);
}

nsresult 
nsNetscapeProfileMigratorBase::SetBool(void* aTransform, nsIPrefBranch* aBranch)
{
  PrefTransform* xform = (PrefTransform*)aTransform;
  SETPREF(xform, SetBoolPref, xform->boolValue);
}

nsresult 
nsNetscapeProfileMigratorBase::GetInt(void* aTransform, nsIPrefBranch* aBranch)
{
  PrefTransform* xform = (PrefTransform*)aTransform;
  GETPREF(xform, GetIntPref, &xform->intValue);
}

nsresult 
nsNetscapeProfileMigratorBase::SetInt(void* aTransform, nsIPrefBranch* aBranch)
{
  PrefTransform* xform = (PrefTransform*)aTransform;
  SETPREF(xform, SetIntPref, xform->intValue);
}

nsresult
nsNetscapeProfileMigratorBase::CopyFile(const nsAString& aSourceFileName, const nsAString& aTargetFileName)
{
  nsCOMPtr<nsIFile> sourceFile;
  mSourceProfile->Clone(getter_AddRefs(sourceFile));

  sourceFile->Append(aSourceFileName);
  PRBool exists = PR_FALSE;
  sourceFile->Exists(&exists);
  if (!exists)
    return NS_OK;

  nsCOMPtr<nsIFile> targetFile;
  mTargetProfile->Clone(getter_AddRefs(targetFile));
  
  targetFile->Append(aTargetFileName);
  targetFile->Exists(&exists);
  if (exists)
    targetFile->Remove(PR_FALSE);

  return sourceFile->CopyTo(mTargetProfile, aTargetFileName);
}

nsresult
nsNetscapeProfileMigratorBase::ImportNetscapeBookmarks(const nsAString& aBookmarksFileName,
                                                       const PRUnichar* aImportSourceNameKey)
{
  nsCOMPtr<nsIFile> bookmarksFile;
  mSourceProfile->Clone(getter_AddRefs(bookmarksFile));
  bookmarksFile->Append(aBookmarksFileName);
  
  return ImportBookmarksHTML(bookmarksFile, PR_FALSE, PR_FALSE, aImportSourceNameKey);
}

nsresult
nsNetscapeProfileMigratorBase::ImportNetscapeCookies(nsIFile* aCookiesFile)
{
  nsresult rv;
  nsCOMPtr<nsIInputStream> cookiesStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(cookiesStream), aCookiesFile);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsILineInputStream> lineInputStream(do_QueryInterface(cookiesStream));

  // This code is copied from mozilla/netwerk/cookie/src/nsCookieManager.cpp
  static NS_NAMED_LITERAL_CSTRING(kTrue, "TRUE");

  nsCAutoString buffer;
  PRBool isMore = PR_TRUE;
  PRInt32 hostIndex = 0, isDomainIndex, pathIndex, secureIndex, expiresIndex, nameIndex, cookieIndex;
  PRInt32 numInts;
  PRInt64 expires;
  PRBool isDomain;
  PRInt64 currentTime = PR_Now() / PR_USEC_PER_SEC;

  nsCOMPtr<nsICookieManager2> cookieManager(do_GetService(NS_COOKIEMANAGER_CONTRACTID, &rv));
  if (NS_FAILED(rv)) return rv;

  /* file format is:
   *
   * host \t isDomain \t path \t secure \t expires \t name \t cookie
   *
   * if this format isn't respected we move onto the next line in the file.
   * isDomain is "TRUE" or "FALSE" (default to "FALSE")
   * isSecure is "TRUE" or "FALSE" (default to "TRUE")
   * expires is a PRInt64 integer
   * note 1: cookie can contain tabs.
   * note 2: cookies are written in order of lastAccessed time:
   *         most-recently used come first; least-recently-used come last.
   */

  while (isMore && NS_SUCCEEDED(lineInputStream->ReadLine(buffer, &isMore))) {
    if (buffer.IsEmpty() || buffer.First() == '#')
      continue;

    // this is a cheap, cheesy way of parsing a tab-delimited line into
    // string indexes, which can be lopped off into substrings. just for
    // purposes of obfuscation, it also checks that each token was found.
    // todo: use iterators?
    if ((isDomainIndex = buffer.FindChar('\t', hostIndex)     + 1) == 0 ||
        (pathIndex     = buffer.FindChar('\t', isDomainIndex) + 1) == 0 ||
        (secureIndex   = buffer.FindChar('\t', pathIndex)     + 1) == 0 ||
        (expiresIndex  = buffer.FindChar('\t', secureIndex)   + 1) == 0 ||
        (nameIndex     = buffer.FindChar('\t', expiresIndex)  + 1) == 0 ||
        (cookieIndex   = buffer.FindChar('\t', nameIndex)     + 1) == 0)
      continue;

    // check the expirytime first - if it's expired, ignore
    // nullstomp the trailing tab, to avoid copying the string
    char *iter = buffer.BeginWriting();
    *(iter += nameIndex - 1) = char(0);
    numInts = PR_sscanf(buffer.get() + expiresIndex, "%lld", &expires);
    if (numInts != 1 || expires < currentTime)
      continue;

    isDomain = Substring(buffer, isDomainIndex, pathIndex - isDomainIndex - 1).Equals(kTrue);
    const nsDependentCSubstring host =
      Substring(buffer, hostIndex, isDomainIndex - hostIndex - 1);
    // check for bad legacy cookies (domain not starting with a dot, or containing a port),
    // and discard
    if (isDomain && !host.IsEmpty() && host.First() != '.' ||
        host.FindChar(':') != -1)
      continue;

    // create a new nsCookie and assign the data.
    rv = cookieManager->Add(host,
                            Substring(buffer, pathIndex, secureIndex - pathIndex - 1),
                            Substring(buffer, nameIndex, cookieIndex - nameIndex - 1),
                            Substring(buffer, cookieIndex, buffer.Length() - cookieIndex),
                            Substring(buffer, secureIndex, expiresIndex - secureIndex - 1).Equals(kTrue),
                            PR_FALSE, // isHttpOnly
                            PR_FALSE, // isSession
                            expires);
  }

  return rv;
}

nsresult
nsNetscapeProfileMigratorBase::GetSignonFileName(PRBool aReplace, char** aFileName)
{
  nsresult rv;
  if (aReplace) {
    // Find out what the signons file was called, this is stored in a pref
    // in Seamonkey.
    nsCOMPtr<nsIPrefService> psvc(do_GetService(NS_PREFSERVICE_CONTRACTID));
    psvc->ResetPrefs();

    nsCOMPtr<nsIFile> sourcePrefsName;
    mSourceProfile->Clone(getter_AddRefs(sourcePrefsName));
    sourcePrefsName->Append(FILE_NAME_PREFS_5X);
    psvc->ReadUserPrefs(sourcePrefsName);

    nsCOMPtr<nsIPrefBranch> branch(do_QueryInterface(psvc));
    rv = branch->GetCharPref("signon.SignonFileName", aFileName);
  }
  else 
    rv = LocateSignonsFile(aFileName);
  return rv;
}

nsresult
nsNetscapeProfileMigratorBase::LocateSignonsFile(char** aResult)
{
  nsCOMPtr<nsISimpleEnumerator> entries;
  nsresult rv = mSourceProfile->GetDirectoryEntries(getter_AddRefs(entries));
  if (NS_FAILED(rv)) return rv;

  nsCAutoString fileName;
  do {
    PRBool hasMore = PR_FALSE;
    rv = entries->HasMoreElements(&hasMore);
    if (NS_FAILED(rv) || !hasMore) break;

    nsCOMPtr<nsISupports> supp;
    rv = entries->GetNext(getter_AddRefs(supp));
    if (NS_FAILED(rv)) break;

    nsCOMPtr<nsIFile> currFile(do_QueryInterface(supp));

    nsCOMPtr<nsIURI> uri;
    rv = NS_NewFileURI(getter_AddRefs(uri), currFile);
    if (NS_FAILED(rv)) break;
    nsCOMPtr<nsIURL> url(do_QueryInterface(uri));

    nsCAutoString extn;
    url->GetFileExtension(extn);

    if (extn.Equals("s", CaseInsensitiveCompare)) {
      url->GetFileName(fileName);
      break;
    }
  }
  while (1);

  *aResult = ToNewCString(fileName);

  return NS_OK;
}

