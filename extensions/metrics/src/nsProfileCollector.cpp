/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Original Code is the Metrics extension.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
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

#include "nsProfileCollector.h"
#include "nsMetricsService.h"
#include "prsystem.h"
#include "nsIXULAppInfo.h"
#include "nsXULAppAPI.h"
#include "nsIServiceManager.h"
#include "nsComponentManagerUtils.h"
#include "nsIExtensionManager.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFService.h"
#include "rdf.h"
#include "nsIDOMPlugin.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIScreenManager.h"
#include "nsILocalFile.h"
#include "nsDirectoryServiceUtils.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"
#ifdef MOZ_PLACES_BOOKMARKS
#include "nsINavBookmarksService.h"
#include "nsINavHistoryService.h"
#include "nsILivemarkService.h"
#include "nsToolkitCompsCID.h"
#else
#include "nsIBookmarksService.h"
#include "nsIRDFContainer.h"
#endif

// We need to suppress inclusion of nsString.h
#define nsString_h___
#include "nsIPluginHost.h"
#undef nsString_h___

// Logs data on all installed plugins.
class nsProfileCollector::PluginEnumerator
{
 public:
  PluginEnumerator() : mMetricsService(nsnull) {}
  nsresult Init();

  // Creates a MetricsEventItem for each installed plugin, and appends them
  // to pluginsItem.
  nsresult LogPlugins(nsIMetricsEventItem *pluginsItem);

 private:
  // Creates and returns a MetricsEventItem for a single plugin, or
  // NULL on failure.
  already_AddRefed<nsIMetricsEventItem> CreatePluginItem(nsIDOMPlugin *plugin);

  nsRefPtr<nsMetricsService> mMetricsService;
};

// Logs data on all installed extensions
class nsProfileCollector::ExtensionEnumerator
{
 public:
  ExtensionEnumerator() : mMetricsService(nsnull) {}
  nsresult Init();

  // Creates a MetricsEventItem for each installed extension, and appends them
  // to extensionsItem.
  nsresult LogExtensions(nsIMetricsEventItem *extensionsItem);

 private:
  // Creates and returns a MetricsEventItem for a single extension,
  // or NULL on failure.
  already_AddRefed<nsIMetricsEventItem> CreateExtensionItem(
      nsIUpdateItem *extension);

  nsRefPtr<nsMetricsService> mMetricsService;
  nsCOMPtr<nsIRDFService> mRDFService;
  nsCOMPtr<nsIExtensionManager> mExtensionManager;
  nsCOMPtr<nsIRDFDataSource> mExtensionsDS;
  nsCOMPtr<nsIRDFResource> mDisabledResource;
};

// Counts the number of bookmark items and folders in a container
class nsProfileCollector::BookmarkCounter
{
 public:
  BookmarkCounter() {}
  nsresult Init();

  // These values correspond to indices of |count| for CountChildren().
  enum BookmarkType {
    ITEM,
    FOLDER,
    LIVEMARK,
    SEPARATOR,
    kLastBookmarkType
  };

  // Fills in |count| with the number of children of each BookmarkType.
  // If |deep| is true, then the count will include children of all subfolders.
#ifdef MOZ_PLACES_BOOKMARKS
  void CountChildren(PRInt64 root, PRBool deep, nsTArray<PRInt32> &count);
#else
  void CountChildren(nsIRDFResource *root,
                     PRBool deep, nsTArray<PRInt32> &count);
#endif

 private:
#ifdef MOZ_PLACES_BOOKMARKS
  void CountRecursive(nsINavHistoryContainerResultNode *root, PRBool deep,
                      nsTArray<PRInt32> &count);
#else
  void CountRecursive(nsIRDFResource *root, PRBool deep,
                      nsTArray<PRInt32> &count);
#endif

#ifdef MOZ_PLACES_BOOKMARKS
  nsCOMPtr<nsILivemarkService> mLivemarkService;
#else
  nsCOMPtr<nsIRDFDataSource> mDataSource;

  nsCOMPtr<nsIRDFResource> mRDFType;
  nsCOMPtr<nsIRDFResource> mNCBookmark;
  nsCOMPtr<nsIRDFResource> mNCFolder;
  nsCOMPtr<nsIRDFResource> mNCLivemark;
  nsCOMPtr<nsIRDFResource> mNCSeparator;
#endif
};

nsProfileCollector::nsProfileCollector()
    : mLoggedProfile(PR_FALSE)
{
}

nsProfileCollector::~nsProfileCollector()
{
}

NS_IMPL_ISUPPORTS1(nsProfileCollector, nsIMetricsCollector)

NS_IMETHODIMP
nsProfileCollector::OnAttach()
{
  return NS_OK;
}

NS_IMETHODIMP
nsProfileCollector::OnDetach()
{
  return NS_OK;
}

NS_IMETHODIMP
nsProfileCollector::OnNewLog()
{
  if (mLoggedProfile) {
    return NS_OK;
  }

  nsMetricsService *ms = nsMetricsService::get();

  nsCOMPtr<nsIMetricsEventItem> profileItem;
  ms->CreateEventItem(NS_LITERAL_STRING("profile"),
                      getter_AddRefs(profileItem));
  NS_ENSURE_STATE(profileItem);

  LogCPU(profileItem);
  LogMemory(profileItem);
  LogOS(profileItem);
  LogInstall(profileItem);
  LogExtensions(profileItem);
  LogPlugins(profileItem);
  LogDisplay(profileItem);
  LogBookmarks(profileItem);

  nsresult rv = ms->LogEvent(profileItem);
  NS_ENSURE_SUCCESS(rv, rv);

  mLoggedProfile = PR_TRUE;
  return NS_OK;
}

nsresult
nsProfileCollector::LogCPU(nsIMetricsEventItem *profile)
{
  nsCOMPtr<nsIWritablePropertyBag2> properties;
  nsMetricsUtils::NewPropertyBag(getter_AddRefs(properties));
  NS_ENSURE_STATE(properties);

  char buf[SYS_INFO_BUFFER_LENGTH];
  if (PR_GetSystemInfo(PR_SI_ARCHITECTURE, buf, sizeof(buf)) != PR_SUCCESS) {
    MS_LOG(("Failed to get architecture"));
    return NS_ERROR_FAILURE;
  }

  properties->SetPropertyAsACString(NS_LITERAL_STRING("arch"),
                                    nsDependentCString(buf));
  MS_LOG(("Logged CPU arch=%s", buf));

  nsresult rv = nsMetricsUtils::AddChildItem(
      profile, NS_LITERAL_STRING("cpu"), properties);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsProfileCollector::LogMemory(nsIMetricsEventItem *profile)
{
  nsCOMPtr<nsIWritablePropertyBag2> properties;
  nsMetricsUtils::NewPropertyBag(getter_AddRefs(properties));
  NS_ENSURE_STATE(properties);

  PRUint64 size = PR_GetPhysicalMemorySize();
  if (size == 0) {
    MS_LOG(("Failed to get physical memory size"));
    return NS_ERROR_FAILURE;
  }

  PRUint64 sizeMB = size >> 20;
  properties->SetPropertyAsUint64(NS_LITERAL_STRING("mb"), sizeMB);
  MS_LOG(("Logged memory mb=%ull", sizeMB));

  nsresult rv = nsMetricsUtils::AddChildItem(
      profile, NS_LITERAL_STRING("memory"), properties);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsProfileCollector::LogOS(nsIMetricsEventItem *profile)
{
  nsCOMPtr<nsIWritablePropertyBag2> properties;
  nsMetricsUtils::NewPropertyBag(getter_AddRefs(properties));
  NS_ENSURE_STATE(properties);

  char buf[SYS_INFO_BUFFER_LENGTH];
  if (PR_GetSystemInfo(PR_SI_SYSNAME, buf, sizeof(buf)) != PR_SUCCESS) {
    MS_LOG(("Failed to get OS name"));
    return NS_ERROR_FAILURE;
  }

  properties->SetPropertyAsACString(NS_LITERAL_STRING("name"),
                                    nsDependentCString(buf));
  MS_LOG(("Logged os name=%s", buf));

  if (PR_GetSystemInfo(PR_SI_RELEASE, buf, sizeof(buf)) != PR_SUCCESS) {
    MS_LOG(("Failed to get OS version"));
    return NS_ERROR_FAILURE;
  }

  properties->SetPropertyAsACString(NS_LITERAL_STRING("version"),
                                    nsDependentCString(buf));
  MS_LOG(("Logged os version=%s", buf));

  nsresult rv = nsMetricsUtils::AddChildItem(
      profile, NS_LITERAL_STRING("os"), properties);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsProfileCollector::LogInstall(nsIMetricsEventItem *profile)
{
  nsCOMPtr<nsIWritablePropertyBag2> properties;
  nsMetricsUtils::NewPropertyBag(getter_AddRefs(properties));
  NS_ENSURE_STATE(properties);

  nsCOMPtr<nsIXULAppInfo> appInfo =
    do_GetService(XULAPPINFO_SERVICE_CONTRACTID);
  NS_ENSURE_STATE(appInfo);

  nsCString buildID;
  appInfo->GetAppBuildID(buildID);
  properties->SetPropertyAsACString(NS_LITERAL_STRING("buildid"), buildID);
  MS_LOG(("Logged install buildid=%s", buildID.get()));

  // The file defaults/pref/channel-prefs.js is exlucded from any
  // security update, so we can use its creation time as an indicator
  // of when this installation was performed.

  nsCOMPtr<nsIFile> prefsDirectory;
  NS_GetSpecialDirectory(NS_APP_PREF_DEFAULTS_50_DIR,
                         getter_AddRefs(prefsDirectory));

  nsCOMPtr<nsILocalFile> channelPrefs = do_QueryInterface(prefsDirectory);
  NS_ENSURE_STATE(channelPrefs);

  nsresult rv = channelPrefs->Append(NS_LITERAL_STRING("channel-prefs.js"));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString nativePath;
  channelPrefs->GetNativePath(nativePath);
  PRFileInfo64 fileInfo;
  if (PR_GetFileInfo64(nativePath.get(), &fileInfo) == PR_SUCCESS) {
    // Convert the time to seconds since the epoch
    PRInt64 installTime = fileInfo.creationTime / PR_USEC_PER_SEC;

    static const int kSecondsPerDay = 60 * 60 * 24;

    // Round down to the nearest full day
    installTime = ((installTime / kSecondsPerDay) * kSecondsPerDay);
    properties->SetPropertyAsInt64(NS_LITERAL_STRING("installdate"),
                                   installTime);
    MS_LOG(("Logged install installdate=%lld", installTime));
  }

  // TODO: log default= based on default-browser selection
  properties->SetPropertyAsBool(NS_LITERAL_STRING("default"), PR_TRUE);

  rv = nsMetricsUtils::AddChildItem(profile,
                                    NS_LITERAL_STRING("install"), properties);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsProfileCollector::LogExtensions(nsIMetricsEventItem *profile)
{
  nsCOMPtr<nsIMetricsEventItem> extensions;
  nsMetricsService::get()->CreateEventItem(NS_LITERAL_STRING("extensions"),
                                           getter_AddRefs(extensions));
  NS_ENSURE_STATE(extensions);

  ExtensionEnumerator enumerator;
  nsresult rv = enumerator.Init();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = enumerator.LogExtensions(extensions);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = profile->AppendChild(extensions);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsProfileCollector::LogPlugins(nsIMetricsEventItem *profile)
{
  nsCOMPtr<nsIMetricsEventItem> plugins;
  nsMetricsService::get()->CreateEventItem(NS_LITERAL_STRING("plugins"),
                                           getter_AddRefs(plugins));
  NS_ENSURE_STATE(plugins);

  PluginEnumerator enumerator;
  nsresult rv = enumerator.Init();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = enumerator.LogPlugins(plugins);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = profile->AppendChild(plugins);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsProfileCollector::LogDisplay(nsIMetricsEventItem *profile)
{
  nsCOMPtr<nsIWritablePropertyBag2> properties;
  nsMetricsUtils::NewPropertyBag(getter_AddRefs(properties));
  NS_ENSURE_STATE(properties);

  nsCOMPtr<nsIScreenManager> screenManager =
    do_GetService("@mozilla.org/gfx/screenmanager;1");
  NS_ENSURE_STATE(screenManager);

  nsCOMPtr<nsIScreen> primaryScreen;
  screenManager->GetPrimaryScreen(getter_AddRefs(primaryScreen));
  NS_ENSURE_STATE(primaryScreen);

  PRInt32 left, top, width, height;
  nsresult rv = primaryScreen->GetRect(&left, &top, &width, &height);
  NS_ENSURE_SUCCESS(rv, rv);

  properties->SetPropertyAsInt32(NS_LITERAL_STRING("xsize"), width);
  properties->SetPropertyAsInt32(NS_LITERAL_STRING("ysize"), height);
  MS_LOG(("Logged display xsize=%d ysize=%d", width, height));

  PRUint32 numScreens = 0;
  if (NS_SUCCEEDED(screenManager->GetNumberOfScreens(&numScreens))) {
    properties->SetPropertyAsUint32(NS_LITERAL_STRING("screens"), numScreens);
    MS_LOG(("Logged display screens=%d", numScreens));
  } else {
    MS_LOG(("Could not get number of screens"));
  }

  rv = nsMetricsUtils::AddChildItem(profile,
                                    NS_LITERAL_STRING("display"), properties);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsProfileCollector::LogBookmarks(nsIMetricsEventItem *profile)
{
  nsresult rv;

  nsMetricsService *ms = nsMetricsService::get();
  NS_ENSURE_STATE(ms);

  nsCOMPtr<nsIMetricsEventItem> bookmarksItem;
  ms->CreateEventItem(NS_LITERAL_STRING("bookmarks"),
                      getter_AddRefs(bookmarksItem));
  NS_ENSURE_STATE(bookmarksItem);

#ifdef MOZ_PLACES_BOOKMARKS
  nsCOMPtr<nsINavBookmarksService> bmSvc =
    do_GetService(NS_NAVBOOKMARKSSERVICE_CONTRACTID);
  NS_ENSURE_STATE(bmSvc);

  PRInt64 root = 0;
  rv = bmSvc->GetPlacesRoot(&root);
  NS_ENSURE_SUCCESS(rv, rv);

  BookmarkCounter counter;
  rv = counter.Init();
  NS_ENSURE_SUCCESS(rv, rv);

  LogBookmarkLocation(bookmarksItem, NS_LITERAL_CSTRING("full-tree"),
                      &counter, root, PR_TRUE);

  rv = bmSvc->GetBookmarksRoot(&root);
  NS_ENSURE_SUCCESS(rv, rv);

  LogBookmarkLocation(bookmarksItem, NS_LITERAL_CSTRING("root"),
                      &counter, root, PR_FALSE);

  rv = bmSvc->GetToolbarFolder(&root);
  NS_ENSURE_SUCCESS(rv, rv);

  LogBookmarkLocation(bookmarksItem, NS_LITERAL_CSTRING("toolbar"),
                  &counter, root, PR_FALSE);

#else
  nsCOMPtr<nsIBookmarksService> bmSvc =
    do_GetService(NS_BOOKMARKS_SERVICE_CONTRACTID);
  NS_ENSURE_STATE(bmSvc);

  // This just ensures that the bookmarks are loaded, it doesn't
  // read them again if they've already been read.
  PRBool loaded;
  bmSvc->ReadBookmarks(&loaded);

  nsCOMPtr<nsIRDFService> rdfSvc =
    do_GetService("@mozilla.org/rdf/rdf-service;1");
  NS_ENSURE_STATE(rdfSvc);

  nsCOMPtr<nsIRDFResource> root;
  rdfSvc->GetResource(NS_LITERAL_CSTRING("NC:BookmarksRoot"),
                      getter_AddRefs(root));
  NS_ENSURE_STATE(root);

  BookmarkCounter counter;
  rv = counter.Init();
  NS_ENSURE_SUCCESS(rv, rv);

  LogBookmarkLocation(bookmarksItem, NS_LITERAL_CSTRING("full-tree"),
                      &counter, root, PR_TRUE);
  LogBookmarkLocation(bookmarksItem, NS_LITERAL_CSTRING("root"),
                      &counter, root, PR_FALSE);

  bmSvc->GetBookmarksToolbarFolder(getter_AddRefs(root));
  if (root) {
    LogBookmarkLocation(bookmarksItem, NS_LITERAL_CSTRING("toolbar"),
                        &counter, root, PR_FALSE);
  }
#endif

  rv = profile->AppendChild(bookmarksItem);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
nsProfileCollector::LogBookmarkLocation(nsIMetricsEventItem *bookmarksItem,
                                        const nsACString &location,
                                        BookmarkCounter *counter,
#ifdef MOZ_PLACES_BOOKMARKS
                                        PRInt64 root,
#else
                                        nsIRDFResource *root,
#endif
                                        PRBool deep)
{
  nsTArray<PRInt32> count;
  counter->CountChildren(root, deep, count);

  nsCOMPtr<nsIWritablePropertyBag2> props;
  nsMetricsUtils::NewPropertyBag(getter_AddRefs(props));
  if (!props) {
    return;
  }

  props->SetPropertyAsACString(NS_LITERAL_STRING("name"), location);
  props->SetPropertyAsInt32(NS_LITERAL_STRING("itemcount"),
                            count[BookmarkCounter::ITEM]);
  props->SetPropertyAsInt32(NS_LITERAL_STRING("foldercount"),
                            count[BookmarkCounter::FOLDER]);
  props->SetPropertyAsInt32(NS_LITERAL_STRING("livemarkcount"),
                            count[BookmarkCounter::LIVEMARK]);
  props->SetPropertyAsInt32(NS_LITERAL_STRING("separatorcount"),
                            count[BookmarkCounter::SEPARATOR]);

  nsMetricsUtils::AddChildItem(bookmarksItem,
                               NS_LITERAL_STRING("bookmarklocation"), props);
}


nsresult
nsProfileCollector::PluginEnumerator::Init()
{
  mMetricsService = nsMetricsService::get();
  NS_ENSURE_STATE(mMetricsService);

  return NS_OK;
}

nsresult
nsProfileCollector::PluginEnumerator::LogPlugins(
    nsIMetricsEventItem *pluginsItem)
{
  nsCOMPtr<nsIPluginHost> host = do_GetService("@mozilla.org/plugin/host;1");
  NS_ENSURE_STATE(host);

  PRUint32 pluginCount = 0;
  host->GetPluginCount(&pluginCount);
  if (pluginCount == 0) {
    return NS_OK;
  }

  nsIDOMPlugin **plugins = new nsIDOMPlugin*[pluginCount];
  NS_ENSURE_TRUE(plugins, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = host->GetPlugins(pluginCount, plugins);
  if (NS_SUCCEEDED(rv)) {
    for (PRUint32 i = 0; i < pluginCount; ++i) {
      nsIDOMPlugin *plugin = plugins[i];
      if (!plugin) {
        NS_WARNING("null plugin in array");
        continue;
      }

      nsCOMPtr<nsIMetricsEventItem> item = CreatePluginItem(plugin);
      if (item) {
        pluginsItem->AppendChild(item);
      }
      NS_RELEASE(plugin);
    }
  }

  delete[] plugins;
  return rv;
}

already_AddRefed<nsIMetricsEventItem>
nsProfileCollector::PluginEnumerator::CreatePluginItem(nsIDOMPlugin *plugin)
{
  nsCOMPtr<nsIMetricsEventItem> pluginItem;
  mMetricsService->CreateEventItem(NS_LITERAL_STRING("plugin"),
                                   getter_AddRefs(pluginItem));
  NS_ENSURE_TRUE(pluginItem, nsnull);

  nsCOMPtr<nsIWritablePropertyBag2> properties;
  nsMetricsUtils::NewPropertyBag(getter_AddRefs(properties));
  NS_ENSURE_TRUE(properties, nsnull);

  nsString name, filename;
  plugin->GetName(name);
  plugin->GetFilename(filename);

  nsCString hashedName, hashedFilename;
  nsresult rv = mMetricsService->HashUTF16(name, hashedName);
  NS_ENSURE_SUCCESS(rv, nsnull);
  rv = mMetricsService->HashUTF16(filename, hashedFilename);
  NS_ENSURE_SUCCESS(rv, nsnull);

  properties->SetPropertyAsACString(NS_LITERAL_STRING("name"), hashedName);
  properties->SetPropertyAsACString(NS_LITERAL_STRING("filename"),
                                    hashedFilename);
  MS_LOG(("Logged plugin name=%s (hashed to %s) filename=%s (hashed to %s)",
          NS_ConvertUTF16toUTF8(name).get(), hashedName.get(),
          NS_ConvertUTF16toUTF8(filename).get(), hashedFilename.get()));


  // TODO(bryner): log the version, if there's a way to find it

  pluginItem->SetProperties(properties);

  nsIMetricsEventItem *item = nsnull;
  pluginItem.swap(item);
  return item;
}


nsresult
nsProfileCollector::ExtensionEnumerator::Init()
{
  mMetricsService = nsMetricsService::get();
  NS_ENSURE_STATE(mMetricsService);

  mRDFService = do_GetService("@mozilla.org/rdf/rdf-service;1");
  NS_ENSURE_STATE(mRDFService);

  mRDFService->GetResource(
      NS_LITERAL_CSTRING("http://www.mozilla.org/2004/em-rdf#disabled"),
      getter_AddRefs(mDisabledResource));
  NS_ENSURE_STATE(mDisabledResource);

  mExtensionManager = do_GetService("@mozilla.org/extensions/manager;1");
  NS_ENSURE_STATE(mExtensionManager);

  mExtensionManager->GetDatasource(getter_AddRefs(mExtensionsDS));
  NS_ENSURE_STATE(mExtensionsDS);

  return NS_OK;
}

nsresult
nsProfileCollector::ExtensionEnumerator::LogExtensions(
    nsIMetricsEventItem *extensionsItem)
{
  PRUint32 count = 0;
  nsIUpdateItem **extensions = nsnull;
  nsresult rv = mExtensionManager->GetItemList(nsIUpdateItem::TYPE_EXTENSION,
                                               &count, &extensions);
  NS_ENSURE_SUCCESS(rv, rv);

  if (count == 0) {
    return NS_OK;
  }
  NS_ENSURE_STATE(extensions);

  for (PRUint32 i = 0; i < count; ++i) {
    nsIUpdateItem *extension = extensions[i];
    if (!extension) {
      NS_WARNING("null extension in array");
      continue;
    }

    nsCOMPtr<nsIMetricsEventItem> item = CreateExtensionItem(extension);
    if (item) {
      extensionsItem->AppendChild(item);
    }
    NS_RELEASE(extension);
  }

  NS_Free(extensions);
  return NS_OK;
}

already_AddRefed<nsIMetricsEventItem>
nsProfileCollector::ExtensionEnumerator::CreateExtensionItem(
    nsIUpdateItem *extension)
{
  nsCOMPtr<nsIMetricsEventItem> extensionItem;
  mMetricsService->CreateEventItem(NS_LITERAL_STRING("extension"),
                                   getter_AddRefs(extensionItem));
  NS_ENSURE_TRUE(extensionItem, nsnull);

  nsCOMPtr<nsIWritablePropertyBag2> properties;
  nsMetricsUtils::NewPropertyBag(getter_AddRefs(properties));
  NS_ENSURE_TRUE(properties, nsnull);

  nsString id, version;
  extension->GetId(id);
  NS_ENSURE_TRUE(!id.IsEmpty(), nsnull);

  nsCString hashedID;
  nsresult rv = mMetricsService->HashUTF16(id, hashedID);
  NS_ENSURE_SUCCESS(rv, nsnull);

  properties->SetPropertyAsACString(
      NS_LITERAL_STRING("extensionid"), hashedID);
  
  extension->GetVersion(version);
  if (!version.IsEmpty()) {
    properties->SetPropertyAsAString(NS_LITERAL_STRING("version"), version);
  }
  MS_LOG(("Logged extension extensionid=%s (hashed to %s) version=%s",
          NS_ConvertUTF16toUTF8(id).get(), hashedID.get(),
          NS_ConvertUTF16toUTF8(version).get()));

  nsCString resourceID("urn:mozilla:item:");
  resourceID.Append(NS_ConvertUTF16toUTF8(id));

  nsCOMPtr<nsIRDFResource> itemResource;
  mRDFService->GetResource(resourceID, getter_AddRefs(itemResource));
  NS_ENSURE_TRUE(itemResource, nsnull);

  nsCOMPtr<nsIRDFNode> itemDisabledNode;
  mExtensionsDS->GetTarget(itemResource, mDisabledResource, PR_TRUE,
                           getter_AddRefs(itemDisabledNode));
  nsCOMPtr<nsIRDFLiteral> itemDisabledLiteral =
    do_QueryInterface(itemDisabledNode);

  if (itemDisabledLiteral) {
    const PRUnichar *value = nsnull;
    itemDisabledLiteral->GetValueConst(&value);
    if (nsDependentString(value).Equals(NS_LITERAL_STRING("true"))) {
      properties->SetPropertyAsBool(NS_LITERAL_STRING("disabled"), PR_TRUE);
      MS_LOG(("Logged extension disabled=true"));
    }
  }

  extensionItem->SetProperties(properties);

  nsIMetricsEventItem *item = nsnull;
  extensionItem.swap(item);
  return item;
}


#ifdef MOZ_PLACES_BOOKMARKS
nsresult
nsProfileCollector::BookmarkCounter::Init()
{
  mLivemarkService = do_GetService(NS_LIVEMARKSERVICE_CONTRACTID);
  NS_ENSURE_STATE(mLivemarkService);

  return NS_OK;
}

void
nsProfileCollector::BookmarkCounter::CountChildren(PRInt64 root, PRBool deep,
                                                   nsTArray<PRInt32> &count)
{
  count.SetLength(kLastBookmarkType);
  for (PRInt32 i = 0; i < kLastBookmarkType; ++i) {
    count[i] = 0;
  }

  nsCOMPtr<nsINavHistoryService> histSvc =
    do_GetService(NS_NAVHISTORYSERVICE_CONTRACTID);
  if (!histSvc) {
    return;
  }

  nsCOMPtr<nsINavHistoryQuery> query;
  histSvc->GetNewQuery(getter_AddRefs(query));
  if (!query) {
    return;
  }

  query->SetFolders(&root, 1);
  query->SetOnlyBookmarked(PR_TRUE);

  nsCOMPtr<nsINavHistoryQueryOptions> options;
  histSvc->GetNewQueryOptions(getter_AddRefs(options));
  if (!options) {
    return;
  }

  const PRUint32 groupMode = nsINavHistoryQueryOptions::GROUP_BY_FOLDER;
  options->SetGroupingMode(&groupMode, 1);

  nsCOMPtr<nsINavHistoryResult> result;
  histSvc->ExecuteQuery(query, options, getter_AddRefs(result));
  if (!result) {
    return;
  }

  nsCOMPtr<nsINavHistoryQueryResultNode> rootNode;
  result->GetRoot(getter_AddRefs(rootNode));
  if (!rootNode) {
    return;
  }

  CountRecursive(rootNode, deep, count);
}

void
nsProfileCollector::BookmarkCounter::CountRecursive(
    nsINavHistoryContainerResultNode *root, PRBool deep,
    nsTArray<PRInt32> &count)
{
  root->SetContainerOpen(PR_TRUE);

  PRUint32 childCount = 0;
  root->GetChildCount(&childCount);
  for (PRUint32 i = 0; i < childCount; ++i) {
    nsCOMPtr<nsINavHistoryResultNode> child;
    root->GetChild(i, getter_AddRefs(child));
    if (!child) {
      continue;
    }

    PRUint32 type = 0;
    child->GetType(&type);

    if (type == nsINavHistoryResultNode::RESULT_TYPE_URI) {
      ++count[ITEM];
    } else if (type == nsINavHistoryResultNode::RESULT_TYPE_FOLDER) {
      nsCOMPtr<nsINavHistoryFolderResultNode> folder =
        do_QueryInterface(child);
      if (!folder) {
        continue;
      }

      PRInt64 folderID = 0;
      folder->GetFolderId(&folderID);

      PRBool isLivemark = PR_FALSE;
      mLivemarkService->IsLivemark(folderID, &isLivemark);
      if (isLivemark) {
        ++count[LIVEMARK];
      } else {
        ++count[FOLDER];
        if (deep) {
          CountRecursive(folder, deep, count);
        }
      }
    } else if (type == nsINavHistoryResultNode::RESULT_TYPE_SEPARATOR) {
      ++count[SEPARATOR];
    }
  }

  root->SetContainerOpen(PR_FALSE);
}

#else

nsresult
nsProfileCollector::BookmarkCounter::Init()
{
  mDataSource = do_GetService(NS_BOOKMARKS_DATASOURCE_CONTRACTID);
  NS_ENSURE_STATE(mDataSource);

  nsCOMPtr<nsIRDFService> rdfSvc =
    do_GetService("@mozilla.org/rdf/rdf-service;1");
  NS_ENSURE_STATE(rdfSvc);

  rdfSvc->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "type"),
                      getter_AddRefs(mRDFType));
  NS_ENSURE_STATE(mRDFType);

  rdfSvc->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Bookmark"),
                      getter_AddRefs(mNCBookmark));
  NS_ENSURE_STATE(mNCBookmark);

  rdfSvc->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Folder"),
                      getter_AddRefs(mNCFolder));
  NS_ENSURE_STATE(mNCFolder);

  rdfSvc->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Livemark"),
                      getter_AddRefs(mNCLivemark));
  NS_ENSURE_STATE(mNCLivemark);

  rdfSvc->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "BookmarkSeparator"),
                      getter_AddRefs(mNCSeparator));
  NS_ENSURE_STATE(mNCSeparator);

  return NS_OK;
}

void
nsProfileCollector::BookmarkCounter::CountChildren(nsIRDFResource *root,
                                                   PRBool deep,
                                                   nsTArray<PRInt32> &count)
{
  count.SetLength(kLastBookmarkType);
  for (PRUint32 i = 0; i < kLastBookmarkType; ++i) {
    count[i] = 0;
  }
  CountRecursive(root, deep, count);
}

void
nsProfileCollector::BookmarkCounter::CountRecursive(nsIRDFResource *root,
                                                    PRBool deep,
                                                    nsTArray<PRInt32> &count)
{
  nsCOMPtr<nsIRDFContainer> container =
    do_CreateInstance("@mozilla.org/rdf/container;1");
  if (!container) {
    return;
  }

  nsresult rv = container->Init(mDataSource, root);
  if (NS_FAILED(rv)) {
    return;
  }

  nsCOMPtr<nsISimpleEnumerator> elements;
  container->GetElements(getter_AddRefs(elements));
  if (!elements) {
    return;
  }

  nsCOMPtr<nsISupports> item;
  while (NS_SUCCEEDED(elements->GetNext(getter_AddRefs(item)))) {
    nsCOMPtr<nsIRDFResource> child = do_QueryInterface(item);
    if (!child) {
      continue;
    }

    // Figure out whether the child is a folder
    nsCOMPtr<nsIRDFNode> typeNode;
    mDataSource->GetTarget(child, mRDFType, PR_TRUE, getter_AddRefs(typeNode));
    if (!typeNode) {
      continue;
    }

    if (typeNode == mNCBookmark) {
      ++count[ITEM];
    } else if (typeNode == mNCFolder) {
      ++count[FOLDER];
      if (deep) {
        CountRecursive(child, deep, count);
      }
    } else if (typeNode == mNCLivemark) {
      ++count[LIVEMARK];
    } else if (typeNode == mNCSeparator) {
      ++count[SEPARATOR];
    }
  }
}
#endif
