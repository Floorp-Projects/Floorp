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

#include "nsAppDirectoryServiceDefs.h"
#include "nsBrowserProfileMigratorUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDocShellCID.h"
#include "nsINavBookmarksService.h"
#include "nsBrowserCompsCID.h"
#include "nsIBrowserHistory.h"
#include "nsICookieManager2.h"
#include "nsIFileProtocolHandler.h"
#include "nsIFormHistory.h"
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
#include "nsIStringBundle.h"
#include "nsISupportsPrimitives.h"
#include "nsSafariProfileMigrator.h"
#include "nsToolkitCompsCID.h"
#include "nsNetUtil.h"
#include "nsTArray.h"
#include "jsapi.h"

#include "mozilla/Util.h"

#include <Carbon/Carbon.h>

#define SAFARI_PREFERENCES_FILE_NAME      NS_LITERAL_STRING("com.apple.Safari.plist")
#define SAFARI_BOOKMARKS_FILE_NAME        NS_LITERAL_STRING("Bookmarks.plist")
#define SAFARI_HISTORY_FILE_NAME          NS_LITERAL_STRING("History.plist")
#define SAFARI_COOKIES_FILE_NAME          NS_LITERAL_STRING("Cookies.plist")
#define SAFARI_COOKIE_BEHAVIOR_FILE_NAME  NS_LITERAL_STRING("com.apple.WebFoundation.plist")
#define SAFARI_DATE_OFFSET                978307200
#define SAFARI_HOME_PAGE_PREF             "HomePage"
#define MIGRATION_BUNDLE                  "chrome://browser/locale/migration/migration.properties"

using namespace mozilla;

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
nsSafariProfileMigrator::Migrate(PRUint16 aItems, nsIProfileStartup* aStartup,
                                 const PRUnichar* aProfile)
{
  nsresult rv = NS_OK;

  bool replace = false;

  if (aStartup) {
    replace = true;
    rv = aStartup->DoStartup();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NOTIFY_OBSERVERS(MIGRATION_STARTED, nsnull);

  COPY_DATA(CopyPreferences,  replace, nsIBrowserProfileMigrator::SETTINGS);
  COPY_DATA(CopyCookies,      replace, nsIBrowserProfileMigrator::COOKIES);
  COPY_DATA(CopyHistory,      replace, nsIBrowserProfileMigrator::HISTORY);
  COPY_DATA(CopyBookmarks,    replace, nsIBrowserProfileMigrator::BOOKMARKS);
  COPY_DATA(CopyFormData,     replace, nsIBrowserProfileMigrator::FORMDATA);
  COPY_DATA(CopyOtherData,    replace, nsIBrowserProfileMigrator::OTHERDATA);

  NOTIFY_OBSERVERS(MIGRATION_ENDED, nsnull);

  return rv;
}

NS_IMETHODIMP
nsSafariProfileMigrator::GetMigrateData(const PRUnichar* aProfile,
                                        bool aReplace,
                                        PRUint16* aResult)
{
  *aResult = 0;
  nsCOMPtr<nsIProperties> fileLocator(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID));
  nsCOMPtr<nsILocalFile> safariSettingsDir, safariCookiesDir;
  fileLocator->Get(NS_MAC_USER_LIB_DIR, NS_GET_IID(nsILocalFile),
                   getter_AddRefs(safariSettingsDir));
  safariSettingsDir->Append(NS_LITERAL_STRING("Safari"));
  fileLocator->Get(NS_MAC_USER_LIB_DIR, NS_GET_IID(nsILocalFile),
                   getter_AddRefs(safariCookiesDir));
  safariCookiesDir->Append(NS_LITERAL_STRING("Cookies"));

  // Safari stores most of its user settings under ~/Library/Safari/
  MigrationData data[] = { { ToNewUnicode(SAFARI_HISTORY_FILE_NAME),
                             nsIBrowserProfileMigrator::HISTORY,
                             false },
                           { ToNewUnicode(SAFARI_BOOKMARKS_FILE_NAME),
                             nsIBrowserProfileMigrator::BOOKMARKS,
                             false } };
  // Frees file name strings allocated above.
  GetMigrateDataFromArray(data, sizeof(data)/sizeof(MigrationData),
                          aReplace, safariSettingsDir, aResult);

  // Safari stores Cookies under ~/Library/Cookies/Cookies.plist
  MigrationData data2[] = { { ToNewUnicode(SAFARI_COOKIES_FILE_NAME),
                              nsIBrowserProfileMigrator::COOKIES,
                              false } };
  GetMigrateDataFromArray(data2, sizeof(data2)/sizeof(MigrationData),
                          aReplace, safariCookiesDir, aResult);

  // Safari stores Preferences under ~/Library/Preferences/
  nsCOMPtr<nsILocalFile> systemPrefsDir;
  fileLocator->Get(NS_OSX_USER_PREFERENCES_DIR, NS_GET_IID(nsILocalFile),
                   getter_AddRefs(systemPrefsDir));
  MigrationData data3[]= { { ToNewUnicode(SAFARI_PREFERENCES_FILE_NAME),
                             nsIBrowserProfileMigrator::SETTINGS,
                             false } };
  GetMigrateDataFromArray(data3, sizeof(data3)/sizeof(MigrationData),
                          aReplace, systemPrefsDir, aResult);

  // Don't offer to import the Safari user style sheet if the active profile
  // already has a content style sheet (userContent.css)
  bool hasContentStylesheet = false;
  if (NS_SUCCEEDED(ProfileHasContentStyleSheet(&hasContentStylesheet)) &&
      !hasContentStylesheet) {
    nsCOMPtr<nsILocalFile> safariUserStylesheetFile;
    if (NS_SUCCEEDED(GetSafariUserStyleSheet(getter_AddRefs(safariUserStylesheetFile))))
      *aResult |= nsIBrowserProfileMigrator::OTHERDATA;
  }
  
  // Don't offer to import that Safari form data if there isn't any
  if (HasFormDataToImport())
    *aResult |= nsIBrowserProfileMigrator::FORMDATA;

  return NS_OK;
}

NS_IMETHODIMP
nsSafariProfileMigrator::GetSourceExists(bool* aResult)
{
  PRUint16 data;
  GetMigrateData(nsnull, false, &data);

  *aResult = data != 0;

  return NS_OK;
}

NS_IMETHODIMP
nsSafariProfileMigrator::GetSourceProfiles(JS::Value* aResult)
{
  *aResult = JSVAL_NULL;
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsSafariProfileMigrator

CFPropertyListRef CopyPListFromFile(nsILocalFile* aPListFile)
{
  bool exists;
  aPListFile->Exists(&exists);
  if (!exists)
    return nsnull;

  nsCAutoString filePath;
  aPListFile->GetNativePath(filePath);

  nsCOMPtr<nsILocalFileMac> macFile(do_QueryInterface(aPListFile));
  CFURLRef urlRef;
  macFile->GetCFURL(&urlRef);

  // It is possible for CFURLCreateDataAndPropertiesFromResource to allocate resource
  // data and then return a failure so be careful to check both and clean up properly.
  SInt32 errorCode;
  CFDataRef resourceData = NULL;
  Boolean dataSuccess = ::CFURLCreateDataAndPropertiesFromResource(kCFAllocatorDefault,
                                                                   urlRef,
                                                                   &resourceData,
                                                                   NULL,
                                                                   NULL,
                                                                   &errorCode);

  CFPropertyListRef propertyList = NULL;
  if (resourceData) {
    if (dataSuccess) {
      propertyList = ::CFPropertyListCreateFromXMLData(kCFAllocatorDefault,
                                                       resourceData,
                                                       kCFPropertyListImmutable,
                                                       NULL);
    }
    ::CFRelease(resourceData);
  }

  ::CFRelease(urlRef);

  return propertyList;
}

CFDictionaryRef CopySafariPrefs()
{
  nsCOMPtr<nsIProperties> fileLocator(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID));
  nsCOMPtr<nsILocalFile> safariPrefsFile;
  fileLocator->Get(NS_OSX_USER_PREFERENCES_DIR,
                   NS_GET_IID(nsILocalFile),
                   getter_AddRefs(safariPrefsFile));

  safariPrefsFile->Append(SAFARI_PREFERENCES_FILE_NAME);

  return static_cast<CFDictionaryRef>(CopyPListFromFile(safariPrefsFile));
}

char*
GetNullTerminatedString(CFStringRef aStringRef)
{
  CFIndex bufferSize = ::CFStringGetLength(aStringRef) + 1;
  char* buffer = (char*)malloc(sizeof(char) * bufferSize);
  if (!buffer)
    return nsnull;
  if (::CFStringGetCString(aStringRef, buffer, bufferSize,
                           kCFStringEncodingASCII))
    buffer[bufferSize-1] = '\0';
  return buffer;
}

void
FreeNullTerminatedString(char* aString)
{
  free(aString);
  aString = nsnull;
}

bool
GetDictionaryStringValue(CFDictionaryRef aDictionary, CFStringRef aKey,
                         nsAString& aResult)
{
  CFStringRef value = (CFStringRef)::CFDictionaryGetValue(aDictionary, aKey);
  if (value) {
    nsAutoTArray<UniChar, 1024> buffer;
    CFIndex valueLength = ::CFStringGetLength(value);
    buffer.SetLength(valueLength);

    ::CFStringGetCharacters(value, CFRangeMake(0, valueLength), buffer.Elements());
    aResult.Assign(buffer.Elements(), valueLength);
    return true;
  }
  return false;
}

bool
GetDictionaryCStringValue(CFDictionaryRef aDictionary, CFStringRef aKey,
                          nsACString& aResult, CFStringEncoding aEncoding)
{
  CFStringRef value = (CFStringRef)::CFDictionaryGetValue(aDictionary, aKey);
  if (value) {
    nsAutoTArray<char, 1024> buffer;
    CFIndex valueLength = ::CFStringGetLength(value);
    buffer.SetLength(valueLength + 1);

    if (::CFStringGetCString(value, buffer.Elements(), valueLength + 1, aEncoding)) {
      aResult = buffer.Elements();
      return true;
    }
  }
  return false;
}

bool
GetArrayStringValue(CFArrayRef aArray, PRInt32 aIndex, nsAString& aResult)
{
  CFStringRef value = (CFStringRef)::CFArrayGetValueAtIndex(aArray, aIndex);
  if (value) {
    nsAutoTArray<UniChar, 1024> buffer;
    CFIndex valueLength = ::CFStringGetLength(value);
    buffer.SetLength(valueLength);

    ::CFStringGetCharacters(value, CFRangeMake(0, valueLength), buffer.Elements());
    aResult.Assign(buffer.Elements(), valueLength);
    return true;
  }
  return false;
}

#define _SPM(type) nsSafariProfileMigrator::type

static
nsSafariProfileMigrator::PrefTransform gTransforms[] = {
  { CFSTR("AlwaysShowTabBar"),            _SPM(BOOL),     "browser.tabs.autoHide",          _SPM(SetBoolInverted), false, { -1 } },
  { CFSTR("AutoFillPasswords"),           _SPM(BOOL),     "signon.rememberSignons",         _SPM(SetBool), false, { -1 } },
  { CFSTR("OpenNewTabsInFront"),          _SPM(BOOL),     "browser.tabs.loadInBackground",  _SPM(SetBoolInverted), false, { -1 } },
  { CFSTR("NSDefaultOpenDir"),            _SPM(STRING),   "browser.download.dir",           _SPM(SetDownloadFolder), false, { -1 } },
  { CFSTR("AutoOpenSafeDownloads"),       _SPM(BOOL),     nsnull,                           _SPM(SetDownloadHandlers), false, { -1 } },
  { CFSTR("DownloadsClearingPolicy"),     _SPM(INT),      "browser.download.manager.retention", _SPM(SetDownloadRetention), false, { -1 } },
  { CFSTR("WebKitDefaultTextEncodingName"),_SPM(STRING),  "intl.charset.default",           _SPM(SetDefaultEncoding), false, { -1 } },
  { CFSTR("WebKitStandardFont"),          _SPM(STRING),   "font.name.serif.",               _SPM(SetFontName), false, { -1 } },
  { CFSTR("WebKitDefaultFontSize"),       _SPM(INT),      "font.size.serif.",               _SPM(SetFontSize), false, { -1 } },
  { CFSTR("WebKitFixedFont"),             _SPM(STRING),   "font.name.fixed.",               _SPM(SetFontName), false, { -1 } },
  { CFSTR("WebKitDefaultFixedFontSize"),  _SPM(INT),      "font.size.fixed.",               _SPM(SetFontSize), false, { -1 } },
  { CFSTR("WebKitMinimumFontSize"),       _SPM(INT),      "font.minimum-size.",             _SPM(SetFontSize), false, { -1 } },
  { CFSTR("WebKitDisplayImagesKey"),      _SPM(BOOL),     "permissions.default.image",      _SPM(SetDisplayImages), false, { -1 } },
  { CFSTR("WebKitJavaScriptEnabled"),     _SPM(BOOL),     "javascript.enabled",             _SPM(SetBool), false, { -1 } },
  { CFSTR("WebKitJavaScriptCanOpenWindowsAutomatically"),
                                          _SPM(BOOL),     "dom.disable_open_during_load",   _SPM(SetBoolInverted), false, { -1 } }
};

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

struct charsetEntry {
  const char *webkitLabel;
  size_t webkitLabelLength;
  const char *mozLabel;
  const char *associatedLangGroup;
};

static const charsetEntry gCharsets[] = {
#define CHARSET_ENTRY(charsetLabel, associatedLangGroup) \
  {#charsetLabel, sizeof(#charsetLabel) - 1, #charsetLabel, #associatedLangGroup}
#define CHARSET_ENTRY2(webkitLabel, mozLabel, associatedLangGroup) \
  {#webkitLabel, sizeof(#webkitLabel) - 1, #mozLabel, #associatedLangGroup}

  CHARSET_ENTRY(ISO-8859-1,x-western),
  CHARSET_ENTRY2(MACINTOSH,x-mac-roman,x-western),
  // Since "x-unicode" in the font dialog means "Other Languages" (that is,
  // languages which don't have their own script), we're picking the default
  // font group - "Western".
  CHARSET_ENTRY(UTF-8,x-western),
  CHARSET_ENTRY2(SHIFT_JIS,Shift_JIS,ja),
  CHARSET_ENTRY(ISO-2022-JP,ja),
  CHARSET_ENTRY(EUC-JP,ja),
  CHARSET_ENTRY2(BIG5,Big5,zh-TW),
  CHARSET_ENTRY(CP950,zh-TW),
  CHARSET_ENTRY(Big5-HKSCS,zh-HK),
  CHARSET_ENTRY(ISO-2022-KR,ko),
  // XXX: fallback to the generic Korean encoding
  CHARSET_ENTRY2(X-MAC-KOREAN,ISO-2022-KR,ko),
  CHARSET_ENTRY(CP949,ko),
  CHARSET_ENTRY(ISO-8859-6,ar),
  CHARSET_ENTRY2(WINDOWS-1256,windows-1256,ar),
  CHARSET_ENTRY(ISO-8859-8,he),
  CHARSET_ENTRY2(WINDOWS-1255,windows-1255,he),
  CHARSET_ENTRY(ISO-8859-7,el),
  CHARSET_ENTRY2(WINDOWS-1253,windows-1253,el),
  CHARSET_ENTRY(ISO-8859-5,x-cyrillic),
  CHARSET_ENTRY2(X-MAC-CYRILLIC,x-mac-cyrillic,x-cyrillic),
  CHARSET_ENTRY(KOI8-R,x-cyrillic),
  CHARSET_ENTRY2(WINDOWS-1251,windows-1251,x-cyrillic),
  CHARSET_ENTRY(CP874,th),
  CHARSET_ENTRY2(GB_2312-80,GB2312,zh-CN),
  CHARSET_ENTRY(HZ-GB-2312,zh-CN),
  CHARSET_ENTRY2(GB18030,gb18030,zh-CN),
  CHARSET_ENTRY(ISO-8859-2,x-central-euro),
  CHARSET_ENTRY2(X-MAC-CENTRALEURROMAN,x-mac-ce,x-central-euro),
  CHARSET_ENTRY2(WINDOWS-1250,windows-1250,x-central-euro),
  CHARSET_ENTRY(ISO-8859-4,x-central-euro),
  CHARSET_ENTRY(ISO-8859-9,tr),
  CHARSET_ENTRY2(WINDOWS-125,windows-1254,tr),
  CHARSET_ENTRY2(WINDOWS-1257,windows-1257,x-baltic)
  
#undef CHARSET_ENTRY
#undef CHARSET_ENTRY2
};

nsresult
nsSafariProfileMigrator::SetDefaultEncoding(void* aTransform, nsIPrefBranch* aBranch)
{
  PrefTransform* xform = (PrefTransform*)aTransform;

  nsCAutoString associatedLangGroup;
  nsDependentCString encoding(xform->stringValue);
  PRUint32 encodingLength = encoding.Length();
  const char* encodingStr = encoding.get();

  PRInt16 charsetIndex = -1;
  for (PRUint16 i = 0; (charsetIndex == -1) &&
                       i < (sizeof(gCharsets) / sizeof(gCharsets[0])); ++i) {
    if (gCharsets[i].webkitLabelLength == encodingLength &&
        !strcmp(gCharsets[i].webkitLabel, encodingStr))
      charsetIndex = (PRInt16)i;
  }
  if (charsetIndex == -1) // Default to "Western"
    charsetIndex = 0;

  aBranch->SetCharPref(xform->targetPrefName, gCharsets[charsetIndex].mozLabel);

  // We also want to use the default encoding for picking the default language
  // in the fonts preferences dialog, and its associated preferences.

  aBranch->SetCharPref("font.language.group",
                       gCharsets[charsetIndex].associatedLangGroup);
  aBranch->SetCharPref("migration.associatedLangGroup",
                       gCharsets[charsetIndex].associatedLangGroup);

  return NS_OK;
}

nsresult
nsSafariProfileMigrator::SetDownloadFolder(void* aTransform, nsIPrefBranch* aBranch)
{
  PrefTransform* xform = (PrefTransform*)aTransform;

  nsCOMPtr<nsILocalFile> downloadFolder;
  nsresult rv = NS_NewNativeLocalFile(nsDependentCString(xform->stringValue),
                                      true, getter_AddRefs(downloadFolder));
  NS_ENSURE_SUCCESS(rv, rv);

  // If the Safari download folder is the desktop, set the folderList pref
  // appropriately so that "Desktop" is selected in the list in our Preferences
  // UI instead of just the raw path being shown.
  nsCOMPtr<nsIProperties> fileLocator(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID));
  nsCOMPtr<nsILocalFile> desktopFolder;
  fileLocator->Get(NS_OSX_USER_DESKTOP_DIR, NS_GET_IID(nsILocalFile),
                   getter_AddRefs(desktopFolder));

  bool equals;
  downloadFolder->Equals(desktopFolder, &equals);
  aBranch->SetIntPref("browser.download.folderList", equals ? 0 : 2);
  aBranch->SetComplexValue("browser.download.dir",
                           NS_GET_IID(nsILocalFile), downloadFolder);

  return NS_OK;
}

nsresult
nsSafariProfileMigrator::SetDownloadHandlers(void* aTransform, nsIPrefBranch* aBranch)
{
  PrefTransform* xform = (PrefTransform*)aTransform;
  if (!xform->boolValue) {
    // If we're not set to auto-open safe downloads, we need to clear out the
    // mime types list which contains default handlers.

    nsCOMPtr<nsIProperties> fileLocator(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID));
    nsCOMPtr<nsILocalFile> mimeRegistryFile;
    fileLocator->Get(NS_APP_USER_MIMETYPES_50_FILE, NS_GET_IID(nsILocalFile),
                     getter_AddRefs(mimeRegistryFile));

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
    rdfService->GetResource(NS_LITERAL_CSTRING("urn:mimetypes:root"),
                            getter_AddRefs(overridesListResource));

    nsCOMPtr<nsIRDFContainer> overridesList(do_CreateInstance("@mozilla.org/rdf/container;1"));
    overridesList->Init(mimeTypes, overridesListResource);

    nsCOMPtr<nsIRDFResource> handlerPropArc, externalApplicationArc;
    rdfService->GetResource(NC_URI(handlerProp), getter_AddRefs(handlerPropArc));
    rdfService->GetResource(NC_URI(externalApplication),
                            getter_AddRefs(externalApplicationArc));

    PRInt32 count;
    overridesList->GetCount(&count);
    for (PRInt32 i = count; i >= 1; --i) {
      nsCOMPtr<nsIRDFNode> currOverrideNode;
      overridesList->RemoveElementAt(i, false, getter_AddRefs(currOverrideNode));
      nsCOMPtr<nsIRDFResource> currOverride(do_QueryInterface(currOverrideNode));

      nsCOMPtr<nsIRDFNode> handlerPropNode;
      mimeTypes->GetTarget(currOverride, handlerPropArc, true,
                           getter_AddRefs(handlerPropNode));
      nsCOMPtr<nsIRDFResource> handlerPropResource(do_QueryInterface(handlerPropNode));

      if (handlerPropResource) {
        nsCOMPtr<nsIRDFNode> externalApplicationNode;
        mimeTypes->GetTarget(handlerPropResource, externalApplicationArc,
                             true, getter_AddRefs(externalApplicationNode));
        nsCOMPtr<nsIRDFResource> externalApplicationResource(do_QueryInterface(externalApplicationNode));

        // Strip the resources down so that the datasource is completely flushed.
        if (externalApplicationResource)
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
nsSafariProfileMigrator::CleanResource(nsIRDFDataSource* aDataSource,
                                       nsIRDFResource* aResource)
{
  nsCOMPtr<nsISimpleEnumerator> arcLabels;
  aDataSource->ArcLabelsOut(aResource, getter_AddRefs(arcLabels));
  if (!arcLabels)
    return;

  do {
    bool hasMore;
    arcLabels->HasMoreElements(&hasMore);

    if (!hasMore)
      break;

    nsCOMPtr<nsIRDFResource> currArc;
    arcLabels->GetNext(getter_AddRefs(currArc));

    if (currArc) {
      nsCOMPtr<nsIRDFNode> currTarget;
      aDataSource->GetTarget(aResource, currArc, true,
                             getter_AddRefs(currTarget));

      aDataSource->Unassert(aResource, currArc, currTarget);
    }
  }
  while (1);
}

nsresult
nsSafariProfileMigrator::SetDownloadRetention(void* aTransform,
                                              nsIPrefBranch* aBranch)
{
  // Safari stores Download Retention in the opposite order of Firefox, namely:
  // Retention Mode:        Safari      Firefox
  // Remove Manually        0           2
  // Remove on Exit         1           1
  // Remove on DL Complete  2           0
  PrefTransform* xform = (PrefTransform*)aTransform;
  aBranch->SetIntPref(xform->targetPrefName,
                      xform->intValue == 0 ? 2 : xform->intValue == 2 ? 0 : 1);
  return NS_OK;
}

nsresult
nsSafariProfileMigrator::SetDisplayImages(void* aTransform, nsIPrefBranch* aBranch)
{
  // Firefox has an elaborate set of Image preferences. The correlation is:
  // Mode:                            Safari    Firefox
  // Blocked                          FALSE     2
  // Allowed                          TRUE      1
  // Allowed, originating site only   --        3
  PrefTransform* xform = (PrefTransform*)aTransform;
  aBranch->SetIntPref(xform->targetPrefName, xform->boolValue ? 1 : 2);
  return NS_OK;
}

nsresult
nsSafariProfileMigrator::SetFontName(void* aTransform, nsIPrefBranch* aBranch)
{
  nsCString associatedLangGroup;
  nsresult rv = aBranch->GetCharPref("migration.associatedLangGroup",
                                     getter_Copies(associatedLangGroup));
  if (NS_FAILED(rv))
    return NS_OK;

  PrefTransform* xform = (PrefTransform*)aTransform;
  nsCAutoString prefName(xform->targetPrefName);
  prefName.Append(associatedLangGroup);

  return aBranch->SetCharPref(prefName.get(), xform->stringValue);
}

nsresult
nsSafariProfileMigrator::SetFontSize(void* aTransform, nsIPrefBranch* aBranch)
{
  nsCString associatedLangGroup;
  nsresult rv = aBranch->GetCharPref("migration.associatedLangGroup",
                                     getter_Copies(associatedLangGroup));
  if (NS_FAILED(rv))
    return NS_OK;

  PrefTransform* xform = (PrefTransform*)aTransform;
  nsCAutoString prefName(xform->targetPrefName);
  prefName.Append(associatedLangGroup);

  return aBranch->SetIntPref(prefName.get(), xform->intValue);
}

nsresult
nsSafariProfileMigrator::CopyPreferences(bool aReplace)
{
  nsCOMPtr<nsIPrefBranch> branch(do_GetService(NS_PREFSERVICE_CONTRACTID));

  CFDictionaryRef safariPrefs = CopySafariPrefs();
  if (!safariPrefs)
    return NS_ERROR_FAILURE;

  // Traverse the standard transforms
  PrefTransform* transform;
  PrefTransform* end = gTransforms +
                       sizeof(gTransforms) / sizeof(PrefTransform);

  for (transform = gTransforms; transform < end; ++transform) {
    Boolean hasValue = ::CFDictionaryContainsKey(safariPrefs, transform->keyName);
    if (!hasValue)
      continue;

    transform->prefHasValue = false;
    switch (transform->type) {
    case _SPM(STRING): {
        CFStringRef stringValue = (CFStringRef)
                                  ::CFDictionaryGetValue(safariPrefs,
                                                         transform->keyName);
        char* value = GetNullTerminatedString(stringValue);
        if (value) {
          transform->stringValue = value;
          transform->prefHasValue = true;
        }
      }
      break;
    case _SPM(INT): {
        CFNumberRef intValue = (CFNumberRef)
                               ::CFDictionaryGetValue(safariPrefs,
                                                      transform->keyName);
        PRInt32 value = 0;
        if (::CFNumberGetValue(intValue, kCFNumberSInt32Type, &value)) {
          transform->intValue = value;
          transform->prefHasValue = true;
        }
      }
      break;
    case _SPM(BOOL): {
        CFBooleanRef boolValue = (CFBooleanRef)
                                 ::CFDictionaryGetValue(safariPrefs,
                                                        transform->keyName);
        transform->boolValue = boolValue == kCFBooleanTrue;
        transform->prefHasValue = true;
      }
      break;
    default:
      break;
    }

    if (transform->prefHasValue)
      transform->prefSetterFunc(transform, branch);

    if (transform->type == _SPM(STRING))
      FreeNullTerminatedString(transform->stringValue);
  }

  ::CFRelease(safariPrefs);

  // Safari stores the Cookie "Accept/Don't Accept/Don't Accept Foreign" cookie
  // setting in a separate WebFoundation preferences PList.
  nsCOMPtr<nsIProperties> fileLocator(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID));
  nsCOMPtr<nsILocalFile> safariWebFoundationPrefsFile;
  fileLocator->Get(NS_OSX_USER_PREFERENCES_DIR, NS_GET_IID(nsILocalFile),
                   getter_AddRefs(safariWebFoundationPrefsFile));
  safariWebFoundationPrefsFile->Append(SAFARI_COOKIE_BEHAVIOR_FILE_NAME);

  CFDictionaryRef safariWebFoundationPrefs =
    static_cast<CFDictionaryRef>(CopyPListFromFile(safariWebFoundationPrefsFile));
  if (safariWebFoundationPrefs) {
    // Mapping of Safari preference values to Firefox preference values:
    //
    // Setting                    Safari          Firefox
    // Always Accept              always          0
    // Accept from Originating    current page    1
    // Never Accept               never           2
    nsAutoString acceptCookies;
    if (GetDictionaryStringValue(safariWebFoundationPrefs,
                                 CFSTR("NSHTTPAcceptCookies"), acceptCookies)) {
      PRInt32 cookieValue = 0;
      if (acceptCookies.EqualsLiteral("never"))
        cookieValue = 2;
      else if (acceptCookies.EqualsLiteral("current page"))
        cookieValue = 1;

      branch->SetIntPref("network.cookie.cookieBehavior", cookieValue);
    }

    ::CFRelease(safariWebFoundationPrefs);
  }

  return NS_OK;
}

nsresult
nsSafariProfileMigrator::CopyCookies(bool aReplace)
{
  nsCOMPtr<nsIProperties> fileLocator(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID));
  nsCOMPtr<nsILocalFile> safariCookiesFile;
  fileLocator->Get(NS_MAC_USER_LIB_DIR,
                   NS_GET_IID(nsILocalFile),
                   getter_AddRefs(safariCookiesFile));
  safariCookiesFile->Append(NS_LITERAL_STRING("Cookies"));
  safariCookiesFile->Append(SAFARI_COOKIES_FILE_NAME);

  CFArrayRef safariCookies = (CFArrayRef)CopyPListFromFile(safariCookiesFile);
  if (!safariCookies)
    return NS_OK;

  nsCOMPtr<nsICookieManager2> cookieManager(do_GetService(NS_COOKIEMANAGER_CONTRACTID));
  CFIndex count = ::CFArrayGetCount(safariCookies);
  for (PRInt32 i = 0; i < count; ++i) {
    CFDictionaryRef entry = (CFDictionaryRef)::CFArrayGetValueAtIndex(safariCookies, i);

    CFDateRef date = (CFDateRef)::CFDictionaryGetValue(entry, CFSTR("Expires"));

    nsCAutoString domain, path, name, value;
    if (date &&
        GetDictionaryCStringValue(entry, CFSTR("Domain"), domain,
                                  kCFStringEncodingUTF8) &&
        GetDictionaryCStringValue(entry, CFSTR("Path"), path,
                                  kCFStringEncodingUTF8) &&
        GetDictionaryCStringValue(entry, CFSTR("Name"), name,
                                  kCFStringEncodingASCII) &&
        GetDictionaryCStringValue(entry, CFSTR("Value"), value,
                                  kCFStringEncodingASCII)) {
      PRInt64 expiryTime;
      LL_D2L(expiryTime, (double)::CFDateGetAbsoluteTime(date));

      expiryTime += SAFARI_DATE_OFFSET;
      cookieManager->Add(domain, path, name, value,
                         false, // isSecure
                         false, // isHttpOnly
                         false, // isSession
                         expiryTime);
    }
  }
  ::CFRelease(safariCookies);

  return NS_OK;
}

NS_IMETHODIMP
nsSafariProfileMigrator::RunBatched(nsISupports* aUserData)
{
  PRUint8 batchAction;
  nsCOMPtr<nsISupportsPRUint8> strWrapper(do_QueryInterface(aUserData));
  NS_ASSERTION(strWrapper, "Unable to create nsISupportsPRUint8 wrapper!");
  nsresult rv = strWrapper->GetData(&batchAction);
  NS_ENSURE_SUCCESS(rv, rv);

  switch (batchAction) {
    case BATCH_ACTION_HISTORY:
      rv = CopyHistoryBatched(false);
      break;
    case BATCH_ACTION_HISTORY_REPLACE:
      rv = CopyHistoryBatched(true);
      break;
    case BATCH_ACTION_BOOKMARKS:
      rv = CopyBookmarksBatched(false);
      break;
    case BATCH_ACTION_BOOKMARKS_REPLACE:
      rv = CopyBookmarksBatched(true);
      break;
  }
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsSafariProfileMigrator::CopyHistory(bool aReplace)
{
  nsresult rv;
  nsCOMPtr<nsINavHistoryService> history =
    do_GetService(NS_NAVHISTORYSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint8 batchAction = aReplace ? BATCH_ACTION_HISTORY_REPLACE
                                 : BATCH_ACTION_HISTORY;
  nsCOMPtr<nsISupportsPRUint8> supports =
    do_CreateInstance(NS_SUPPORTS_PRUINT8_CONTRACTID);
  NS_ENSURE_TRUE(supports, NS_ERROR_OUT_OF_MEMORY);
  rv = supports->SetData(batchAction);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = history->RunInBatchMode(this, supports);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}
 
nsresult
nsSafariProfileMigrator::CopyHistoryBatched(bool aReplace)
{
  nsCOMPtr<nsIProperties> fileLocator(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID));
  nsCOMPtr<nsILocalFile> safariHistoryFile;
  fileLocator->Get(NS_MAC_USER_LIB_DIR, NS_GET_IID(nsILocalFile),
                   getter_AddRefs(safariHistoryFile));
  safariHistoryFile->Append(NS_LITERAL_STRING("Safari"));
  safariHistoryFile->Append(SAFARI_HISTORY_FILE_NAME);

  CFDictionaryRef safariHistory =
    static_cast<CFDictionaryRef>(CopyPListFromFile(safariHistoryFile));
  if (!safariHistory)
    return NS_OK;

  if (!::CFDictionaryContainsKey(safariHistory, CFSTR("WebHistoryDates"))) {
    ::CFRelease(safariHistory);
    return NS_OK;
  }

  nsCOMPtr<nsIBrowserHistory> history(do_GetService(NS_GLOBALHISTORY2_CONTRACTID));

  CFArrayRef children = (CFArrayRef)
                ::CFDictionaryGetValue(safariHistory, CFSTR("WebHistoryDates"));
  if (children) {
    CFIndex count = ::CFArrayGetCount(children);
    for (PRInt32 i = 0; i < count; ++i) {
      CFDictionaryRef entry = (CFDictionaryRef)::CFArrayGetValueAtIndex(children, i);

      CFStringRef lastVisitedDate = (CFStringRef)
                        ::CFDictionaryGetValue(entry, CFSTR("lastVisitedDate"));
      nsAutoString url, title;
      if (GetDictionaryStringValue(entry, CFSTR(""), url) &&
          GetDictionaryStringValue(entry, CFSTR("title"), title) &&
          lastVisitedDate) {

        double lvd = ::CFStringGetDoubleValue(lastVisitedDate) + SAFARI_DATE_OFFSET;
        PRTime lastVisitTime;
        PRInt64 temp, million;
        LL_D2L(temp, lvd);
        LL_I2L(million, PR_USEC_PER_SEC);
        LL_MUL(lastVisitTime, temp, million);

        nsCOMPtr<nsIURI> uri;
        NS_NewURI(getter_AddRefs(uri), url);
        if (uri)
          history->AddPageWithDetails(uri, title.get(), lastVisitTime);
      }
    }
  }

  ::CFRelease(safariHistory);

  return NS_OK;
}

nsresult
nsSafariProfileMigrator::CopyBookmarks(bool aReplace)
{
  nsresult rv;
  nsCOMPtr<nsINavBookmarksService> bookmarks =
    do_GetService(NS_NAVBOOKMARKSSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint8 batchAction = aReplace ? BATCH_ACTION_BOOKMARKS_REPLACE
                                 : BATCH_ACTION_BOOKMARKS;
  nsCOMPtr<nsISupportsPRUint8> supports =
    do_CreateInstance(NS_SUPPORTS_PRUINT8_CONTRACTID);
  NS_ENSURE_TRUE(supports, NS_ERROR_OUT_OF_MEMORY);
  rv = supports->SetData(batchAction);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = bookmarks->RunInBatchMode(this, supports);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsSafariProfileMigrator::CopyBookmarksBatched(bool aReplace)
{
  // If "aReplace" is true, merge into the root level of bookmarks. Otherwise, create
  // a folder called "Imported Safari Favorites" and place all the Bookmarks there.
  nsresult rv;

  nsCOMPtr<nsINavBookmarksService> bms =
    do_GetService(NS_NAVBOOKMARKSSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  PRInt64 bookmarksMenuFolderId;
  rv = bms->GetBookmarksMenuFolder(&bookmarksMenuFolderId);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt64 folder;
  if (!aReplace) {
    nsCOMPtr<nsIStringBundleService> bundleService =
      do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIStringBundle> bundle;
    rv = bundleService->CreateBundle(MIGRATION_BUNDLE, getter_AddRefs(bundle));
    NS_ENSURE_SUCCESS(rv, rv);

    nsString sourceNameSafari;
    rv = bundle->GetStringFromName(NS_LITERAL_STRING("sourceNameSafari").get(),
                                   getter_Copies(sourceNameSafari));
    NS_ENSURE_SUCCESS(rv, rv);

    const PRUnichar* sourceNameStrings[] = { sourceNameSafari.get() };
    nsString importedSafariBookmarksTitle;
    rv = bundle->FormatStringFromName(NS_LITERAL_STRING("importedBookmarksFolder").get(),
                                      sourceNameStrings, 1,
                                      getter_Copies(importedSafariBookmarksTitle));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = bms->CreateFolder(bookmarksMenuFolderId,
                           NS_ConvertUTF16toUTF8(importedSafariBookmarksTitle),
                           nsINavBookmarksService::DEFAULT_INDEX, &folder);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    // In replace mode we are merging at the top level.
    folder = bookmarksMenuFolderId;
  }

  nsCOMPtr<nsIProperties> fileLocator(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID));
  nsCOMPtr<nsILocalFile> safariBookmarksFile;
  fileLocator->Get(NS_MAC_USER_LIB_DIR, NS_GET_IID(nsILocalFile),
                   getter_AddRefs(safariBookmarksFile));
  safariBookmarksFile->Append(NS_LITERAL_STRING("Safari"));
  safariBookmarksFile->Append(SAFARI_BOOKMARKS_FILE_NAME);

  CFDictionaryRef safariBookmarks =
    static_cast<CFDictionaryRef>(CopyPListFromFile(safariBookmarksFile));
  if (!safariBookmarks)
    return NS_OK;

  // The Safari Bookmarks file looks like this:
  // At the top level are all the Folders, Special Folders and Proxies. Proxies
  // are references to other data sources such as History, Rendezvous etc.
  // We ignore these. Special folders exist for the Bookmarks Toolbar folder
  // (called "BookmarksBar" and the Bookmarks Menu (called "BookmarksMenu").
  // We put the contents of the "BookmarksBar" folder into our Personal Toolbar
  // and merge the contents of the "BookmarksMenu" folder and the other toplevel
  // non-special folders under our NC:BookmarksRoot.
  if (::CFDictionaryContainsKey(safariBookmarks, CFSTR("Children")) &&
      ::CFDictionaryContainsKey(safariBookmarks, CFSTR("WebBookmarkFileVersion")) ) {
    CFNumberRef intValue =
      (CFNumberRef)::CFDictionaryGetValue(safariBookmarks,
                                          CFSTR("WebBookmarkFileVersion"));
    PRInt32 value = 0;
    if (::CFNumberGetValue(intValue, kCFNumberSInt32Type, &value) && value ==1) {
      CFArrayRef children =
        (CFArrayRef)::CFDictionaryGetValue(safariBookmarks, CFSTR("Children"));
      if (children) {
        rv = ParseBookmarksFolder(children, folder, bms, true);
      }
    }
  }
  ::CFRelease(safariBookmarks);
  return rv;
}

nsresult
nsSafariProfileMigrator::ParseBookmarksFolder(CFArrayRef aChildren,
                                              PRInt64 aParentFolder,
                                              nsINavBookmarksService * aBookmarksService,
                                              bool aIsAtRootLevel)
{
  nsresult rv = NS_OK;

  CFIndex count = ::CFArrayGetCount(aChildren);
  for (PRInt32 i = 0; i < count; ++i) {
    CFDictionaryRef entry = (CFDictionaryRef)::CFArrayGetValueAtIndex(aChildren, i);

    nsAutoString type;
    if (!GetDictionaryStringValue(entry, CFSTR("WebBookmarkType"), type))
      continue;

    if (!type.EqualsLiteral("WebBookmarkTypeList") &&
        !type.EqualsLiteral("WebBookmarkTypeLeaf"))
      continue;

    if (::CFDictionaryContainsKey(entry, CFSTR("Children")) &&
        type.EqualsLiteral("WebBookmarkTypeList")) {
      nsAutoString title;
      if (!GetDictionaryStringValue(entry, CFSTR("Title"), title))
        continue;

      CFArrayRef children = (CFArrayRef)::CFDictionaryGetValue(entry,
                                                               CFSTR("Children"));

      // Look for the BookmarksBar Bookmarks and add them into the appropriate
      // Personal Toolbar Root
      if (title.EqualsLiteral("BookmarksBar") && aIsAtRootLevel) {
        PRInt64 toolbarFolder;
        aBookmarksService->GetToolbarFolder(&toolbarFolder);

        rv |= ParseBookmarksFolder(children,
                                   toolbarFolder,
                                   aBookmarksService,
                                   false);
      }
      // Look for the BookmarksMenu Bookmarks and flatten them into the top level
      else if (title.EqualsLiteral("BookmarksMenu") && aIsAtRootLevel) {
        rv |= ParseBookmarksFolder(children,
                                   aParentFolder,
                                   aBookmarksService,
                                   true);
      }
      else {
        // Encountered a Folder, so create one in our Bookmarks DataSource and then
        // parse the contents of the Safari one into it...
        PRInt64 folder;
        rv |= aBookmarksService->CreateFolder(aParentFolder, NS_ConvertUTF16toUTF8(title),
                                              nsINavBookmarksService::DEFAULT_INDEX,
                                              &folder);
        rv |= ParseBookmarksFolder(children,
                                   folder,
                                   aBookmarksService,
                                   false);
      }
    }
    else if (type.EqualsLiteral("WebBookmarkTypeLeaf")) {
      // Encountered a Bookmark, so add it to the current folder...
      CFDictionaryRef URIDictionary = (CFDictionaryRef)
                      ::CFDictionaryGetValue(entry, CFSTR("URIDictionary"));
      nsAutoString title;
      nsCAutoString url;
      if (GetDictionaryStringValue(URIDictionary, CFSTR("title"), title) &&
          GetDictionaryCStringValue(entry, CFSTR("URLString"), url, kCFStringEncodingUTF8)) {
        nsCOMPtr<nsIURI> uri;
        rv = NS_NewURI(getter_AddRefs(uri), url);
        if (NS_SUCCEEDED(rv)) {
          PRInt64 id;
          rv = aBookmarksService->InsertBookmark(aParentFolder, uri,
                                                 nsINavBookmarksService::DEFAULT_INDEX,
                                                 NS_ConvertUTF16toUTF8(title), &id);
        }
      }
    }
  }
  return rv;
}

// nsSafariProfileMigrator::HasFormDataToImport()
// if we add support for "~/Library/Safari/Form Values",
// keep in sync with CopyFormData()
// see bug #344284
bool
nsSafariProfileMigrator::HasFormDataToImport()
{
  bool hasFormData = false;

  // Safari stores this data in an array under the "RecentSearchStrings" key
  // in its Preferences file.
  CFDictionaryRef safariPrefs = CopySafariPrefs();
  if (safariPrefs) {
    if (::CFDictionaryContainsKey(safariPrefs, CFSTR("RecentSearchStrings")))
      hasFormData = true;
    ::CFRelease(safariPrefs);
  }
  return hasFormData;
}

// nsSafariProfileMigrator::CopyFormData()
// if we add support for "~/Library/Safari/Form Values",
// keep in sync with HasFormDataToImport()
// see bug #344284
nsresult
nsSafariProfileMigrator::CopyFormData(bool aReplace)
{
  nsresult rv = NS_ERROR_FAILURE;
  CFDictionaryRef safariPrefs = CopySafariPrefs();
  if (safariPrefs) {
    // We lump saved Searches in with Form Data since that's how we store it.
    // Safari stores this data in an array under the "RecentSearchStrings" key
    // in its Preferences file.
    Boolean hasSearchStrings = ::CFDictionaryContainsKey(safariPrefs,
                                                         CFSTR("RecentSearchStrings"));
    if (hasSearchStrings) {
      nsCOMPtr<nsIFormHistory2> formHistory(do_GetService("@mozilla.org/satchel/form-history;1"));
      if (formHistory) {
        CFArrayRef strings = (CFArrayRef)::CFDictionaryGetValue(safariPrefs,
                                                                CFSTR("RecentSearchStrings"));
        if (strings) {
          CFIndex count = ::CFArrayGetCount(strings);
          for (PRInt32 i = 0; i < count; ++i) {
            nsAutoString value;
            GetArrayStringValue(strings, i, value);
            formHistory->AddEntry(NS_LITERAL_STRING("searchbar-history"), value);
          }
        }
        rv = NS_OK;
      }
    }
    else
      rv = NS_OK;

    ::CFRelease(safariPrefs);
  }

  return rv;
}

// Returns whether or not the active profile has a content style sheet
// (That is chrome/userContent.css)
nsresult
nsSafariProfileMigrator::ProfileHasContentStyleSheet(bool *outExists)
{
  NS_ENSURE_ARG(outExists);

  // Get the profile's chrome/ directory native path
  nsCOMPtr<nsIFile> userChromeDir;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_CHROME_DIR,
                                      getter_AddRefs(userChromeDir));
  nsCAutoString userChromeDirPath;
  rv = userChromeDir->GetNativePath(userChromeDirPath);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString path(userChromeDirPath);
  path.Append("/userContent.css");

  nsCOMPtr<nsILocalFile> file;
  rv = NS_NewNativeLocalFile(path, false,
                            getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);

  file->Exists(outExists);
  return NS_OK;
}

nsresult
nsSafariProfileMigrator::GetSafariUserStyleSheet(nsILocalFile** aResult)
{
  *aResult = nsnull;

  CFDictionaryRef safariPrefs = CopySafariPrefs();
  if (!safariPrefs)
    return NS_ERROR_FAILURE;

  nsresult rv = NS_ERROR_FAILURE;
  // Check whether or not a user style sheet has been specified
  if (::CFDictionaryContainsKey
        (safariPrefs, CFSTR("WebKitUserStyleSheetEnabledPreferenceKey")) &&
      ::CFDictionaryContainsKey
        (safariPrefs, CFSTR("WebKitUserStyleSheetLocationPreferenceKey"))) {
    CFBooleanRef hasSheet = (CFBooleanRef)::CFDictionaryGetValue(safariPrefs,
                             CFSTR("WebKitUserStyleSheetEnabledPreferenceKey"));
    if (hasSheet == kCFBooleanTrue) {
      nsAutoString path;
      // Get its path
      if (GetDictionaryStringValue(safariPrefs,
                                   CFSTR("WebKitUserStyleSheetLocation" \
                                   "PreferenceKey"), path)) {
        nsCOMPtr<nsILocalFile> file;
        rv = NS_NewLocalFile(path, false, getter_AddRefs(file));
        if (NS_SUCCEEDED(rv)) {
          bool exists = false;
          file->Exists(&exists);
          if (exists) {
            NS_ADDREF(*aResult = file);
            rv = NS_OK;
          }
          else
            rv = NS_ERROR_FILE_NOT_FOUND;
        }
      }
    }
  }
  ::CFRelease(safariPrefs);

  return rv;
}

nsresult
nsSafariProfileMigrator::CopyOtherData(bool aReplace)
{
  // Get the Safari user style sheet and copy it into the active profile's
  // chrome folder
  nsCOMPtr<nsILocalFile> stylesheetFile;
  if (NS_SUCCEEDED(GetSafariUserStyleSheet(getter_AddRefs(stylesheetFile)))) {
    nsCOMPtr<nsIFile> userChromeDir;
    NS_GetSpecialDirectory(NS_APP_USER_CHROME_DIR,
                           getter_AddRefs(userChromeDir));

    stylesheetFile->CopyTo(userChromeDir, NS_LITERAL_STRING("userContent.css"));
  }
  return NS_OK;
}


NS_IMETHODIMP
nsSafariProfileMigrator::GetSourceHomePageURL(nsACString& aResult)
{
  aResult.Truncate();

  // Let's first check if there's a home page key in the com.apple.safari file...
  CFDictionaryRef safariPrefs = CopySafariPrefs();
  if (safariPrefs) {
    bool foundPref = GetDictionaryCStringValue(safariPrefs,
                                                 CFSTR(SAFARI_HOME_PAGE_PREF),
                                                 aResult, kCFStringEncodingUTF8);
    ::CFRelease(safariPrefs);
    if (foundPref)
      return NS_OK;
  }

  return NS_ERROR_FAILURE;
}
