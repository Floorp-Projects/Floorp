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
#include "nsCRT.h"
#include "nsIBookmarksService.h"
#include "nsICookieManager2.h"
#include "nsIFile.h"
#include "nsIInputStream.h"
#include "nsILineInputStream.h"
#include "nsInt64.h"
#include "nsIPrefBranch.h"
#include "nsIPrefLocalizedString.h"
#include "nsIPrefService.h"
#include "nsIRDFService.h"
#include "nsIRegistry.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsISupportsPrimitives.h"
#include "nsIURL.h"
#include "nsNetscapeProfileMigratorBase.h"
#include "nsNetUtil.h"
#include "nsReadableUtils.h"
#include "nsXPIDLString.h"
#include "prtime.h"
#include "prprf.h"

#if defined(XP_MAC) || defined(XP_MACOSX)
#define NEED_TO_FIX_4X_COOKIES 1
#define SECONDS_BETWEEN_1900_AND_1970 2208988800UL
#endif /* XP_MAC */

static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

#define MIGRATION_BUNDLE "chrome://browser/locale/migration/migration.properties"

#define FILE_NAME_PREFS_5X NS_LITERAL_STRING("prefs.js")

///////////////////////////////////////////////////////////////////////////////
// nsNetscapeProfileMigratorBase
nsNetscapeProfileMigratorBase::nsNetscapeProfileMigratorBase()
{
  nsCOMPtr<nsIStringBundleService> bundleService(do_GetService(kStringBundleServiceCID));
  bundleService->CreateBundle(MIGRATION_BUNDLE, getter_AddRefs(mBundle));
}

nsresult
nsNetscapeProfileMigratorBase::GetProfileDataFromRegistry(nsILocalFile* aRegistryFile,
                                                          nsISupportsArray* aProfileNames,
                                                          nsISupportsArray* aProfileLocations)
{
  nsresult rv = NS_OK;

  // Open It
  nsCOMPtr<nsIRegistry> reg(do_CreateInstance("@mozilla.org/registry;1"));
  reg->Open(aRegistryFile);

  nsRegistryKey profilesTree;
  rv = reg->GetKey(nsIRegistry::Common, NS_LITERAL_STRING("Profiles").get(), &profilesTree);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIEnumerator> keys;
  reg->EnumerateSubtrees(profilesTree, getter_AddRefs(keys));

  keys->First();
  while (keys->IsDone() != NS_OK) {
    nsCOMPtr<nsISupports> key;
    keys->CurrentItem(getter_AddRefs(key));

    nsCOMPtr<nsIRegistryNode> node(do_QueryInterface(key));

    nsRegistryKey profile;
    node->GetKey(&profile);

    // "migrated" is "yes" for all valid Seamonkey profiles. It is only "no"
    // for 4.x profiles. 
    nsXPIDLString isMigrated;
    reg->GetString(profile, NS_LITERAL_STRING("migrated").get(), getter_Copies(isMigrated));

    if (isMigrated.Equals(NS_LITERAL_STRING("no"))) {
      keys->Next();
      continue;
    }

    // Get the profile name and add it to the names array
    nsXPIDLString profileName;
    node->GetName(getter_Copies(profileName));

    // Get the profile location and add it to the locations array
    nsXPIDLString directory;
    reg->GetString(profile, NS_LITERAL_STRING("directory").get(), getter_Copies(directory));

    nsCOMPtr<nsILocalFile> dir;
#ifdef XP_MACOSX
    rv = NS_NewNativeLocalFile(nsCString(), PR_TRUE, getter_AddRefs(dir));
    if (NS_FAILED(rv)) return rv;
    dir->SetPersistentDescriptor(NS_LossyConvertUCS2toASCII(directory));
#else
    rv = NS_NewLocalFile(directory, PR_TRUE, getter_AddRefs(dir));
    if (NS_FAILED(rv)) return rv;
#endif

    PRBool exists;
    dir->Exists(&exists);

    if (exists) {
      nsCOMPtr<nsISupportsString> profileNameString(do_CreateInstance("@mozilla.org/supports-string;1"));
      profileNameString->SetData(profileName);
      aProfileNames->AppendElement(profileNameString);

      aProfileLocations->AppendElement(dir);
    }

    keys->Next();
  }
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
    nsXPIDLString data;
    prefValue->ToString(getter_Copies(data));

    xform->stringValue = ToNewCString(NS_ConvertUCS2toUTF8(data));
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
    nsAutoString data; data.AssignWithConversion(xform->stringValue);
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
    nsAutoString data = NS_ConvertUTF8toUCS2(xform->stringValue);
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

  nsCOMPtr<nsIBookmarksService> bms(do_GetService("@mozilla.org/browser/bookmarks-service;1"));
  nsCOMPtr<nsISupportsArray> params;
  NS_NewISupportsArray(getter_AddRefs(params));

  nsCOMPtr<nsIRDFService> rdfs(do_GetService("@mozilla.org/rdf/rdf-service;1"));
  nsCOMPtr<nsIRDFResource> prop;
  rdfs->GetResource(NS_LITERAL_CSTRING("http://home.netscape.com/NC-rdf#URL"), 
                    getter_AddRefs(prop));
  nsCOMPtr<nsIRDFLiteral> url;
  nsAutoString path;
  bookmarksFile->GetPath(path);
  rdfs->GetLiteral(path.get(), getter_AddRefs(url));

  params->AppendElement(prop);
  params->AppendElement(url);
  
  nsCOMPtr<nsIRDFResource> importCmd;
  rdfs->GetResource(NS_LITERAL_CSTRING("http://home.netscape.com/NC-rdf#command?cmd=import"), 
                    getter_AddRefs(importCmd));

  nsCOMPtr<nsIRDFResource> root;
  rdfs->GetResource(NS_LITERAL_CSTRING("NC:BookmarksRoot"), getter_AddRefs(root));

  nsXPIDLString sourceName;
  mBundle->GetStringFromName(aImportSourceNameKey, getter_Copies(sourceName));

  const PRUnichar* sourceNameStrings[] = { sourceName.get() };
  nsXPIDLString importedBookmarksTitle;
  mBundle->FormatStringFromName(NS_LITERAL_STRING("importedBookmarksFolder").get(),
                                sourceNameStrings, 1, 
                                getter_Copies(importedBookmarksTitle));

  nsCOMPtr<nsIRDFResource> folder;
  bms->CreateFolderInContainer(importedBookmarksTitle.get(), root, -1, getter_AddRefs(folder));
  
  nsCOMPtr<nsIRDFResource> folderProp;
  rdfs->GetResource(NS_LITERAL_CSTRING("http://home.netscape.com/NC-rdf#Folder"), 
                    getter_AddRefs(folderProp));
  params->AppendElement(folderProp);
  params->AppendElement(folder);

  nsCOMPtr<nsISupportsArray> sources;
  NS_NewISupportsArray(getter_AddRefs(sources));
  sources->AppendElement(folder);

  nsCOMPtr<nsIRDFDataSource> ds(do_QueryInterface(bms));
  return ds->DoCommand(sources, importCmd, params);
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
  nsASingleFragmentCString::char_iterator iter;
  PRInt32 numInts;
  PRInt64 expires;
  PRBool isDomain;
  nsInt64 currentTime = nsInt64(PR_Now()) / nsInt64(1000000);

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
    buffer.BeginWriting(iter);
    *(iter += nameIndex - 1) = char(0);
    numInts = PR_sscanf(buffer.get() + expiresIndex, "%lld", &expires);
    if (numInts != 1 || nsInt64(expires) < currentTime)
      continue;

    isDomain = Substring(buffer, isDomainIndex, pathIndex - isDomainIndex - 1).Equals(kTrue);
    const nsASingleFragmentCString &host = Substring(buffer, hostIndex, isDomainIndex - hostIndex - 1);
    // check for bad legacy cookies (domain not starting with a dot, or containing a port),
    // and discard
    if (isDomain && !host.IsEmpty() && host.First() != '.' ||
        host.FindChar(':') != kNotFound)
      continue;

    // create a new nsCookie and assign the data.
    rv = cookieManager->Add(host,
                            Substring(buffer, pathIndex, secureIndex - pathIndex - 1),
                            Substring(buffer, nameIndex, cookieIndex - nameIndex - 1),
                            Substring(buffer, cookieIndex, buffer.Length() - cookieIndex),
                            Substring(buffer, secureIndex, expiresIndex - secureIndex - 1).Equals(kTrue),
                            PR_FALSE,
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

    if (extn.EqualsIgnoreCase("s")) {
      url->GetFileName(fileName);
      break;
    }
  }
  while (1);

  *aResult = ToNewCString(fileName);

  return NS_OK;
}

