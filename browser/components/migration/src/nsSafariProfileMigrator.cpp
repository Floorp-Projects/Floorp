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
#include "nsDirectoryServiceDefs.h"
#include "nsIFileProtocolHandler.h"
#include "nsIIOService.h"
#include "nsILocalFileMac.h"
#include "nsIObserverService.h"
#include "nsIPrefService.h"
#include "nsIProfileMigrator.h"
#include "nsIProtocolHandler.h"
#include "nsIRDFContainer.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsISupportsPrimitives.h"
#include "nsSafariProfileMigrator.h"

#include <InternetConfig.h>

#define SAFARI_PREFERENCES_FILE_NAME NS_LITERAL_STRING("com.apple.Safari.plist")

///////////////////////////////////////////////////////////////////////////////
// nsSafariProfileMigrator

NS_IMPL_ISUPPORTS1(nsSafariProfileMigrator, nsIBrowserProfileMigrator)

nsSafariProfileMigrator::nsSafariProfileMigrator()
{
  mObserverService = do_GetService("@mozilla.org/observer-service;1");
}

nsSafariProfileMigrator::~nsSafariProfileMigrator()
{
}

///////////////////////////////////////////////////////////////////////////////
// nsIBrowserProfileMigrator

NS_IMETHODIMP
nsSafariProfileMigrator::Migrate(PRUint16 aItems, nsIProfileStartup* aStartup, const PRUnichar* aProfile)
{
  nsresult rv = NS_OK;

  PRBool aReplace = PR_FALSE;

  if (aStartup) {
    aReplace = PR_TRUE;
    rv = aStartup->DoStartup();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NOTIFY_OBSERVERS(MIGRATION_STARTED, nsnull);
  
  COPY_DATA(CopyPreferences,  aReplace, nsIBrowserProfileMigrator::SETTINGS);
  COPY_DATA(CopyCookies,      aReplace, nsIBrowserProfileMigrator::COOKIES);
  COPY_DATA(CopyHistory,      aReplace, nsIBrowserProfileMigrator::HISTORY);
  COPY_DATA(CopyBookmarks,    aReplace, nsIBrowserProfileMigrator::BOOKMARKS);

  NOTIFY_OBSERVERS(MIGRATION_ENDED, nsnull);

  return rv;
}

NS_IMETHODIMP
nsSafariProfileMigrator::GetMigrateData(const PRUnichar* aProfile, 
                                        PRBool aReplace, 
                                        PRUint16* aResult)
{
  *aResult = 0; // XXXben implement me
  return NS_OK;
}

NS_IMETHODIMP
nsSafariProfileMigrator::GetSourceExists(PRBool* aResult)
{
  *aResult = PR_FALSE; // XXXben implement me

  return NS_OK;
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

#define _SPM(type) nsSafariProfileMigrator::type

static
nsSafariProfileMigrator::PrefTransform gTransforms[] = {
  { CFSTR("AlwaysShowTabBar"),            _SPM(BOOL),   "browser.tabs.autoHide",          _SPM(SetBoolInverted), PR_FALSE, -1 },
  { CFSTR("AutoFillPasswords"),           _SPM(BOOL),   "signon.rememberSignons",         _SPM(SetBool), PR_FALSE, -1 },
  { CFSTR("OpenNewTabsInFront"),          _SPM(BOOL),   "browser.tabs.loadInBackground",  _SPM(SetBoolInverted), PR_FALSE, -1 },
  { CFSTR("NSDefaultOpenDir"),            _SPM(STRING), "browser.download.dir",           _SPM(SetDownloadFolder), PR_FALSE, -1 },
  { CFSTR("AutoOpenSafeDownloads"),       _SPM(BOOL),   "", _SPM(SetDownloadHandlers), PR_FALSE, -1 },
  { CFSTR("DownloadsClearingPolicy"),     _SPM(INT),    "", _SPM(SetDownloadRetention), PR_FALSE, -1 },
  { CFSTR("RecentSearchStrings"),         _SPM(STRING), "", _SPM(SetGoogleBar), PR_FALSE, -1 },
  { CFSTR("WebKitDefaultTextEncodingName"),_SPM(STRING), "", _SPM(SetDefaultEncoding), PR_FALSE, -1 },
  { CFSTR("WebKitStandardFont"),          _SPM(STRING), "", _SPM(SetString), PR_FALSE, -1 },
  { CFSTR("WebKitDefaultFontSize"),       _SPM(INT),    "", _SPM(SetInt), PR_FALSE, -1 },
  { CFSTR("WebKitFixedFont"),             _SPM(STRING), "", _SPM(SetString), PR_FALSE, -1 },
  { CFSTR("WebKitDefaultFixedFontSize"),  _SPM(INT),    "", _SPM(SetInt), PR_FALSE, -1 },
  { CFSTR("WebKitDisplayImagesKey"),      _SPM(BOOL),   "", _SPM(SetBool), PR_FALSE, -1 },
  { CFSTR("WebKitJavaEnabled"),           _SPM(BOOL),   "", _SPM(SetBool), PR_FALSE, -1 },
  { CFSTR("WebKitJavaScriptEnabled"),     _SPM(BOOL),   "", _SPM(SetBool), PR_FALSE, -1 },
  { CFSTR("WebKitMinimumFontSize"),       _SPM(INT),    "", _SPM(SetInt), PR_FALSE, -1 },
  { CFSTR("WebKitJavaScriptCanOpenWindowsAutomatically"), _SPM(BOOL), "", _SPM(SetBoolInverted), PR_FALSE, -1 }
};

#if 0
WebKitUserStyleSheetEnabledPreferenceKey - BOOLEAN
WebKitUserStyleSheetLocationPreferenceKey - STRING
#endif

nsresult
nsSafariProfileMigrator::SetBool(void* aTransform, nsIPrefBranch* aBranch)
{
  PrefTransform* xform = (PrefTransform*)aTransform;
  return aBranch->SetBoolPref(xform->targetPrefName, xform->boolValue);
}

nsresult
nsSafariProfileMigrator::SetBoolInverted(void* aTransform, nsIPrefBranch* aBranch)
{
  PrefTransform* xform = (PrefTransform*)aTransform;
  return aBranch->SetBoolPref(xform->targetPrefName, !xform->boolValue);
}

nsresult
nsSafariProfileMigrator::SetString(void* aTransform, nsIPrefBranch* aBranch)
{
  PrefTransform* xform = (PrefTransform*)aTransform;
  return aBranch->SetCharPref(xform->targetPrefName, xform->stringValue);
}

nsresult
nsSafariProfileMigrator::SetInt(void* aTransform, nsIPrefBranch* aBranch)
{
  PrefTransform* xform = (PrefTransform*)aTransform;
  return aBranch->SetIntPref(xform->targetPrefName, !xform->intValue);
}

nsresult
nsSafariProfileMigrator::SetDefaultEncoding(void* aTransform, nsIPrefBranch* aBranch)
{
  PrefTransform* xform = (PrefTransform*)aTransform;
  
  nsCAutoString encodingSuffix;
  nsDependentCString encoding(xform->stringValue);
  if (encoding.Equals(NS_LITERAL_CSTRING("MACINTOSH")) || 
      encoding.Equals(NS_LITERAL_CSTRING("ISO-8859-1")))
    encodingSuffix = "x-western";
  else if (encoding.Equals(NS_LITERAL_CSTRING("UTF-8")))
    encodingSuffix = "x-unicode";
  else if (encoding.Equals(NS_LITERAL_CSTRING("SHIFT_JIS")) ||
           encoding.Equals(NS_LITERAL_CSTRING("ISO-2822-JP")) |
           encoding.Equals(NS_LITERAL_CSTRING("EUC-JP"))) 
    encodingSuffix = "ja";
  else if (encoding.Equals(NS_LITERAL_CSTRING("BIG5")) ||
           encoding.Equals(NS_LITERAL_CSTRING("CP950")))
    encodingSuffix = "zh-TW";
  else if (encoding.Equals(NS_LITERAL_CSTRING("Big5-HKSCS")))
    encodingSuffix = "zh-HK";
  else if (encoding.Equals(NS_LITERAL_CSTRING("ISO-2022-KR")) ||
           encoding.Equals(NS_LITERAL_CSTRING("X-MAC-KOREAN")) ||
           encoding.Equals(NS_LITERAL_CSTRING("CP949")))
    encodingSuffix = "ko";
  else if (encoding.Equals(NS_LITERAL_CSTRING("ISO-8859-6")) ||
           encoding.Equals(NS_LITERAL_CSTRING("WINDOWS-1256")))
    encodingSuffix = "ar";
  else if (encoding.Equals(NS_LITERAL_CSTRING("ISO-8859-8")) ||
           encoding.Equals(NS_LITERAL_CSTRING("WINDOWS-1255")))
    encodingSuffix = "he";
  else if (encoding.Equals(NS_LITERAL_CSTRING("ISO-8859-7")) ||
           encoding.Equals(NS_LITERAL_CSTRING("WINDOWS-1253")))
    encodingSuffix = "el";
  else if (encoding.Equals(NS_LITERAL_CSTRING("ISO-8859-5")) ||
           encoding.Equals(NS_LITERAL_CSTRING("X-MAC-CYRILLIC")) ||
           encoding.Equals(NS_LITERAL_CSTRING("KOI8-R")) ||
           encoding.Equals(NS_LITERAL_CSTRING("WINDOWS-1251")))
    encodingSuffix = "x-cyrillic";
  else if (encoding.Equals(NS_LITERAL_CSTRING("CP874")))
    encodingSuffix = "th";
  else if (encoding.Equals(NS_LITERAL_CSTRING("GB_2312-80")) ||
           encoding.Equals(NS_LITERAL_CSTRING("HZ-GB-2312")) ||
           encoding.Equals(NS_LITERAL_CSTRING("GB18030")))
    encodingSuffix = "zh-CN";
  else if (encoding.Equals(NS_LITERAL_CSTRING("ISO-8859-2")) ||
           encoding.Equals(NS_LITERAL_CSTRING("X-MAC-CENTRALEURROMAN")) ||
           encoding.Equals(NS_LITERAL_CSTRING("WINDOWS-1250")) ||
           encoding.Equals(NS_LITERAL_CSTRING("ISO-8859-4")))
    encodingSuffix = "x-central-euro";
  else if (encoding.Equals(NS_LITERAL_CSTRING("ISO-8859-9")) ||
           encoding.Equals(NS_LITERAL_CSTRING("WINDOWS-1254")))
    encodingSuffix = "tr";
  else if (encoding.Equals(NS_LITERAL_CSTRING("WINDOWS-1257")))
    encodingSuffix = "x-baltic";
  
  return aBranch->SetCharPref("migration.fontEncodingSuffix", encodingSuffix.get());
}

nsresult
nsSafariProfileMigrator::SetDownloadFolder(void* aTransform, nsIPrefBranch* aBranch)
{
  PrefTransform* xform = (PrefTransform*)aTransform;

  nsCOMPtr<nsILocalFile> downloadFolder(do_CreateInstance("@mozilla.org/file/local;1"));
  downloadFolder->InitWithNativePath(nsDependentCString(xform->stringValue));
  
  // If the Safari download folder is the desktop, set the folderList pref appropriately so that
  // "Desktop" is selected in the list in our Preferences UI instead of just the raw path being
  // shown. 
  nsCOMPtr<nsIProperties> fileLocator(do_GetService("@mozilla.org/file/directory_service;1"));
  nsCOMPtr<nsILocalFile> desktopFolder;
  fileLocator->Get(NS_OSX_USER_DESKTOP_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(desktopFolder));
  
  PRBool equals;
  downloadFolder->Equals(desktopFolder, &equals);
  aBranch->SetIntPref("browser.download.folderList", equals ? 0 : 2);
  aBranch->SetComplexValue("browser.download.defaultFolder", NS_GET_IID(nsILocalFile), downloadFolder);
  
  return NS_OK;
}

nsresult
nsSafariProfileMigrator::SetDownloadHandlers(void* aTransform, nsIPrefBranch* aBranch)
{
  PrefTransform* xform = (PrefTransform*)aTransform;
  if (!xform->boolValue) {
    // If we're not set to auto-open safe downloads, we need to clear out the mime types 
    // list which contains default handlers. 
    
    nsCOMPtr<nsIProperties> fileLocator(do_GetService("@mozilla.org/file/directory_service;1"));
    nsCOMPtr<nsILocalFile> mimeRegistryFile;
    fileLocator->Get(NS_APP_USER_MIMETYPES_50_FILE, NS_GET_IID(nsILocalFile), getter_AddRefs(mimeRegistryFile));
    
    nsCOMPtr<nsIIOService> ioService(do_GetService("@mozilla.org/network/io-service;1"));
    nsCOMPtr<nsIProtocolHandler> ph;
    ioService->GetProtocolHandler("file", getter_AddRefs(ph));
    nsCOMPtr<nsIFileProtocolHandler> fph(do_QueryInterface(ph));
    
    nsCOMPtr<nsIRDFService> rdfService(do_GetService("@mozilla.org/rdf/rdf-service;1"));
    nsCOMPtr<nsIRDFDataSource> mimeTypes;
    
    nsCAutoString dsURL;
    fph->GetURLSpecFromFile(mimeRegistryFile, dsURL);
    rdfService->GetDataSourceBlocking(dsURL.get(), getter_AddRefs(mimeTypes));
    
    nsCOMPtr<nsIRDFResource> overridesListResource;
    rdfService->GetResource(NS_LITERAL_CSTRING("urn:mimetypes:root"), getter_AddRefs(overridesListResource));
    
    nsCOMPtr<nsIRDFContainer> overridesList(do_CreateInstance("@mozilla.org/rdf/container;1"));
    overridesList->Init(mimeTypes, overridesListResource);
    
    nsCOMPtr<nsIRDFResource> handlerPropArc, externalApplicationArc;
    rdfService->GetResource(NS_LITERAL_CSTRING("http://home.netscape.com/NC-rdf#handlerProp"), getter_AddRefs(handlerPropArc));
    rdfService->GetResource(NS_LITERAL_CSTRING("http://home.netscape.com/NC-rdf#externalApplication"), getter_AddRefs(externalApplicationArc));
    
    PRInt32 count;
    overridesList->GetCount(&count);
    for (PRInt32 i = count; i >= 1; --i) {
      nsCOMPtr<nsIRDFNode> currOverrideNode;
      overridesList->RemoveElementAt(i, PR_FALSE, getter_AddRefs(currOverrideNode));
      nsCOMPtr<nsIRDFResource> currOverride(do_QueryInterface(currOverrideNode));
      
      nsCOMPtr<nsIRDFNode> handlerPropNode;
      mimeTypes->GetTarget(currOverride, handlerPropArc, PR_TRUE, getter_AddRefs(handlerPropNode));
      nsCOMPtr<nsIRDFResource> handlerPropResource(do_QueryInterface(handlerPropNode));
      
      if (handlerPropResource) {
        nsCOMPtr<nsIRDFNode> externalApplicationNode;
        mimeTypes->GetTarget(handlerPropResource, externalApplicationArc, PR_TRUE, getter_AddRefs(externalApplicationNode));
        nsCOMPtr<nsIRDFResource> externalApplicationResource(do_QueryInterface(externalApplicationNode));

        // Strip the resources down so that the datasource is completely flushed. 
        if (externalApplicationResource);
          CleanResource(mimeTypes, externalApplicationResource);
        
        CleanResource(mimeTypes, handlerPropResource);
      }
      CleanResource(mimeTypes, currOverride);
    }

    nsCOMPtr<nsIRDFRemoteDataSource> rds(do_QueryInterface(mimeTypes));
    if (rds) 
      rds->Flush();
  }
  return NS_OK;
}

void 
nsSafariProfileMigrator::CleanResource(nsIRDFDataSource* aDataSource, nsIRDFResource* aResource)
{
  nsCOMPtr<nsISimpleEnumerator> arcLabels;
  aDataSource->ArcLabelsOut(aResource, getter_AddRefs(arcLabels));
  
  do {
    PRBool hasMore;
    arcLabels->HasMoreElements(&hasMore);
    
    if (!hasMore)
      break;
    
    nsCOMPtr<nsIRDFResource> currArc;
    arcLabels->GetNext(getter_AddRefs(currArc));
    
    if (currArc) {
      nsCOMPtr<nsIRDFNode> currTarget;
      aDataSource->GetTarget(aResource, currArc, PR_TRUE, getter_AddRefs(currTarget));
      
      aDataSource->Unassert(aResource, currArc, currTarget);
    }
  }
  while (1);
}

nsresult
nsSafariProfileMigrator::SetDownloadRetention(void* aTransform, nsIPrefBranch* aBranch)
{
  return NS_OK;
}

nsresult
nsSafariProfileMigrator::SetGoogleBar(void* aTransform, nsIPrefBranch* aBranch)
{
  return NS_OK;
}

nsresult
nsSafariProfileMigrator::CopyPreferences(PRBool aReplace)
{
  nsCOMPtr<nsIPrefBranch> branch(do_GetService(NS_PREFSERVICE_CONTRACTID));

  nsCOMPtr<nsIProperties> fileLocator(do_GetService("@mozilla.org/file/directory_service;1"));
  nsCOMPtr<nsILocalFile> safariPrefsFile;
  fileLocator->Get(NS_MAC_USER_LIB_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(safariPrefsFile));
  safariPrefsFile->Append(NS_LITERAL_STRING("Preferences"));
  safariPrefsFile->Append(SAFARI_PREFERENCES_FILE_NAME);

  CFDictionaryRef safariPrefs; 
  nsresult rv = GetPListFromFile(safariPrefsFile, (CFDictionaryRef*)&safariPrefs);
  if (NS_FAILED(rv)) 
    return NS_OK;
  
  // Traverse the standard transforms
  PrefTransform* transform;
  PrefTransform* end = gTransforms + sizeof(gTransforms)/sizeof(PrefTransform);

  for (transform = gTransforms; transform < end; ++transform) {
    Boolean hasValue = ::CFDictionaryContainsKey(safariPrefs, transform->keyName);
    if (!hasValue)
      continue;
      
    switch (transform->type) {
    case _SPM(STRING):
      break;
    case _SPM(INT):
      break;
    case _SPM(BOOL): {
      CFBooleanRef boolValue = ::CFDictionaryGetValue(safariPrefs, transform->keyName);
      transform->boolValue = boolValue == kCFBooleanTrue;
      }
      break;
    default:
      break;
    }
    
    transform->prefHasValue = PR_TRUE;
    transform->prefSetterFunc(transform, branch);
  }
    
  ::CFRelease(safariPrefs);
  
  // Now get some of the stuff that only InternetConfig has for us, such as
  // the default home page.
  
  ICInstance internetConfig;
  OSStatus error = ::ICStart(&internetConfig, 'FRFX');
  if (error == noErr) {
    ICAttr dummy;
    
    char* buf = nsnull;
    SInt32 size = 256;
    do {
      buf = (char*)malloc((unsigned int)size+1);
      if (!buf)
        break;
        
      error = ::ICGetPref(internetConfig, kICWWWHomePage, &dummy, buf, &size);
      if (error != noErr && error != icTruncatedErr) {
        free(buf);
        *buf = nsnull;
      }
      else 
        buf[size] = '\0';
      
      size *= 2;
    }
    while (error == icTruncatedErr);
    
    if (*buf != 0) {
      // This is kind of a hack... skip the first byte on the pascal string
      // (which is the length).
      branch->SetCharPref("browser.startup.homepage", buf + 1);
    }
    
    if (buf) {
      free(buf);
      *buf = nsnull;
    }
    
    ::ICStop(internetConfig);
  }


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

