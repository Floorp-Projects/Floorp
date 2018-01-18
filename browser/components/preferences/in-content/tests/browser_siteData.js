/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
/* global sinon */
Services.scriptloader.loadSubScript("resource://testing-common/sinon-2.3.2.js");

const TEST_QUOTA_USAGE_HOST = "example.com";
const TEST_QUOTA_USAGE_ORIGIN = "https://" + TEST_QUOTA_USAGE_HOST;
const TEST_QUOTA_USAGE_URL = TEST_QUOTA_USAGE_ORIGIN + "/browser/browser/components/preferences/in-content/tests/site_data_test.html";
const TEST_OFFLINE_HOST = "example.org";
const TEST_OFFLINE_ORIGIN = "https://" + TEST_OFFLINE_HOST;
const TEST_OFFLINE_URL = TEST_OFFLINE_ORIGIN + "/browser/browser/components/preferences/in-content/tests/offline/offline.html";
const TEST_SERVICE_WORKER_URL = TEST_OFFLINE_ORIGIN + "/browser/browser/components/preferences/in-content/tests/service_worker_test.html";
const REMOVE_DIALOG_URL = "chrome://browser/content/preferences/siteDataRemoveSelected.xul";

const { NetUtil } = Cu.import("resource://gre/modules/NetUtil.jsm", {});
const { DownloadUtils } = Cu.import("resource://gre/modules/DownloadUtils.jsm", {});
const { SiteDataManager } = Cu.import("resource:///modules/SiteDataManager.jsm", {});
const { OfflineAppCacheHelper } = Cu.import("resource:///modules/offlineAppCache.jsm", {});

XPCOMUtils.defineLazyServiceGetter(this, "serviceWorkerManager", "@mozilla.org/serviceworkers/manager;1", "nsIServiceWorkerManager");

const mockOfflineAppCacheHelper = {
  clear: null,

  originalClear: null,

  register() {
    this.originalClear = OfflineAppCacheHelper.clear;
    this.clear = sinon.spy();
    OfflineAppCacheHelper.clear = this.clear;
  },

  unregister() {
    OfflineAppCacheHelper.clear = this.originalClear;
  }
};

function getPersistentStoragePermStatus(origin) {
  let uri = NetUtil.newURI(origin);
  let principal = Services.scriptSecurityManager.createCodebasePrincipal(uri, {});
  return Services.perms.testExactPermissionFromPrincipal(principal, "persistent-storage");
}

function getQuotaUsage(origin) {
  return new Promise(resolve => {
    let uri = NetUtil.newURI(origin);
    let principal = Services.scriptSecurityManager.createCodebasePrincipal(uri, {});
    Services.qms.getUsageForPrincipal(principal, request => resolve(request.result.usage));
  });
}

// XXX: The intermittent bug 1331851
// The implementation of nsICacheStorageConsumptionObserver must be passed as weak referenced,
// so we must hold this observer here well. If we didn't, there would be a chance that
// in Linux debug test run the observer was released before the operation at gecko was completed
// (may be because of a relatively quicker GC cycle or a relatively slower operation).
// As a result of that, we would never get the cache usage we want so the test would fail from timeout.
const cacheUsageGetter = {
  _promise: null,
  _resolve: null,
  get() {
    if (!this._promise) {
      this._promise = new Promise(resolve => {
        this._resolve = resolve;
        Services.cache2.asyncGetDiskConsumption(this);
      });
    }
    return this._promise;
  },
  // nsICacheStorageConsumptionObserver implementations
  onNetworkCacheDiskConsumption(usage) {
    cacheUsageGetter._promise = null;
    cacheUsageGetter._resolve(usage);
  },
  QueryInterface: XPCOMUtils.generateQI([
    Components.interfaces.nsICacheStorageConsumptionObserver,
    Components.interfaces.nsISupportsWeakReference
  ]),
};

function promiseCookiesCleared() {
  return TestUtils.topicObserved("cookie-changed", (subj, data) => {
    return data === "cleared";
  });
}

async function loadServiceWorkerTestPage(url) {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await BrowserTestUtils.waitForCondition(() => {
    return ContentTask.spawn(tab.linkedBrowser, {}, () =>
      content.document.body.getAttribute("data-test-service-worker-registered") === "true");
  }, `Fail to load service worker test ${url}`);
  await BrowserTestUtils.removeTab(tab);
}

function promiseServiceWorkerRegisteredFor(url) {
  return BrowserTestUtils.waitForCondition(() => {
    try {
      let principal = Services.scriptSecurityManager.createCodebasePrincipalFromOrigin(url);
      let sw = serviceWorkerManager.getRegistrationByPrincipal(principal, principal.URI.spec);
      if (sw) {
        ok(true, `Found the service worker registered for ${url}`);
        return true;
      }
    } catch (e) {}
    return false;
  }, `Should register service worker for ${url}`);
}

function promiseServiceWorkersCleared() {
  return BrowserTestUtils.waitForCondition(() => {
    let serviceWorkers = serviceWorkerManager.getAllRegistrations();
    if (serviceWorkers.length == 0) {
      ok(true, "Cleared all service workers");
      return true;
    }
    return false;
  }, "Should clear all service workers");
}

registerCleanupFunction(function() {
  delete window.sinon;
  mockOfflineAppCacheHelper.unregister();
});

// Test listing site using quota usage or site using appcache
add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});

  // Open a test site which would save into appcache
  await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_OFFLINE_URL);
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);

  // Open a test site which would save into quota manager
  await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_QUOTA_USAGE_URL);
  // eslint-disable-next-line mozilla/no-cpows-in-tests
  await BrowserTestUtils.waitForEvent(gBrowser.selectedBrowser.contentWindowAsCPOW, "test-indexedDB-done");
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);

  let updatedPromise = promiseSiteDataManagerSitesUpdated();
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  await updatedPromise;
  await openSiteDataSettingsDialog();
  // eslint-disable-next-line mozilla/no-cpows-in-tests
  let dialog = content.gSubDialog._topDialog;
  let dialogFrame = dialog._frame;
  let frameDoc = dialogFrame.contentDocument;

  let siteItems = frameDoc.getElementsByTagName("richlistitem");
  is(siteItems.length, 2, "Should list sites using quota usage or appcache");

  let appcacheSite = frameDoc.querySelector(`richlistitem[host="${TEST_OFFLINE_HOST}"]`);
  ok(appcacheSite, "Should list site using appcache");

  let qoutaUsageSite = frameDoc.querySelector(`richlistitem[host="${TEST_QUOTA_USAGE_HOST}"]`);
  ok(qoutaUsageSite, "Should list site using quota usage");

  // Always remember to clean up
  OfflineAppCacheHelper.clear();
  await new Promise(resolve => {
    let principal = Services.scriptSecurityManager
                            .createCodebasePrincipalFromOrigin(TEST_QUOTA_USAGE_ORIGIN);
    let request = Services.qms.clearStoragesForPrincipal(principal, null, true);
    request.callback = resolve;
  });
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Test buttons are disabled and loading message shown while updating sites
add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
  let updatedPromise = promiseSiteDataManagerSitesUpdated();
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  await updatedPromise;

  let actual = null;
  let expected = null;
  let doc = gBrowser.selectedBrowser.contentDocument;
  let clearBtn = doc.getElementById("clearSiteDataButton");
  let settingsButton = doc.getElementById("siteDataSettings");
  let prefStrBundle = doc.getElementById("bundlePreferences");
  let totalSiteDataSizeLabel = doc.getElementById("totalSiteDataSize");
  is(clearBtn.disabled, false, "Should enable clear button after sites updated");
  is(settingsButton.disabled, false, "Should enable settings button after sites updated");
  await SiteDataManager.getTotalUsage()
                       .then(usage => {
                         actual = totalSiteDataSizeLabel.textContent;
                         expected = prefStrBundle.getFormattedString(
                           "totalSiteDataSize", DownloadUtils.convertByteUnits(usage));
                          is(actual, expected, "Should show the right total site data size");
                       });

  Services.obs.notifyObservers(null, "sitedatamanager:updating-sites");
  is(clearBtn.disabled, true, "Should disable clear button while updating sites");
  is(settingsButton.disabled, true, "Should disable settings button while updating sites");
  actual = totalSiteDataSizeLabel.textContent;
  expected = prefStrBundle.getString("loadingSiteDataSize");
  is(actual, expected, "Should show the loading message while updating");

  Services.obs.notifyObservers(null, "sitedatamanager:sites-updated");
  is(clearBtn.disabled, false, "Should enable clear button after sites updated");
  is(settingsButton.disabled, false, "Should enable settings button after sites updated");
  await SiteDataManager.getTotalUsage()
                       .then(usage => {
                         actual = totalSiteDataSizeLabel.textContent;
                         expected = prefStrBundle.getFormattedString(
                           "totalSiteDataSize", DownloadUtils.convertByteUnits(usage));
                          is(actual, expected, "Should show the right total site data size");
                       });

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Test the function of the "Clear All Data" button
add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
  addPersistentStoragePerm(TEST_QUOTA_USAGE_ORIGIN);

  await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_QUOTA_USAGE_URL);
  // eslint-disable-next-line mozilla/no-cpows-in-tests
  await BrowserTestUtils.waitForEvent(gBrowser.selectedBrowser.contentWindowAsCPOW, "test-indexedDB-done");
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });

  // Test the initial states
  let cacheUsage = await cacheUsageGetter.get();
  let quotaUsage = await getQuotaUsage(TEST_QUOTA_USAGE_ORIGIN);
  let totalUsage = await SiteDataManager.getTotalUsage();
  Assert.greater(cacheUsage, 0, "The cache usage should not be 0");
  Assert.greater(quotaUsage, 0, "The quota usage should not be 0");
  Assert.greater(totalUsage, 0, "The total usage should not be 0");

  // Test cancelling "Clear All Data"
  // Click "Clear All Data" button and then cancel
  let doc = gBrowser.selectedBrowser.contentDocument;
  let cancelPromise = promiseAlertDialogOpen("cancel");
  let clearBtn = doc.getElementById("clearSiteDataButton");
  clearBtn.doCommand();
  await cancelPromise;

  // Test the items are not removed
  let status = getPersistentStoragePermStatus(TEST_QUOTA_USAGE_ORIGIN);
  is(status, Ci.nsIPermissionManager.ALLOW_ACTION, "Should not remove permission");

  cacheUsage = await cacheUsageGetter.get();
  quotaUsage = await getQuotaUsage(TEST_QUOTA_USAGE_ORIGIN);
  totalUsage = await SiteDataManager.getTotalUsage();
  Assert.greater(cacheUsage, 0, "The cache usage should not be 0");
  Assert.greater(quotaUsage, 0, "The quota usage should not be 0");
  Assert.greater(totalUsage, 0, "The total usage should not be 0");
  // Test cancelling "Clear All Data" ends

  // Test accepting "Clear All Data"
  // Click "Clear All Data" button and then accept
  let acceptPromise = promiseAlertDialogOpen("accept");
  let updatePromise = promiseSiteDataManagerSitesUpdated();
  let cookiesClearedPromise = promiseCookiesCleared();

  mockOfflineAppCacheHelper.register();
  clearBtn.doCommand();
  await acceptPromise;
  await updatePromise;
  mockOfflineAppCacheHelper.unregister();

  // Test all the items are removed
  await cookiesClearedPromise;

  ok(mockOfflineAppCacheHelper.clear.calledOnce, "Should clear app cache");

  status = getPersistentStoragePermStatus(TEST_QUOTA_USAGE_ORIGIN);
  is(status, Ci.nsIPermissionManager.UNKNOWN_ACTION, "Should remove permission");

  cacheUsage = await cacheUsageGetter.get();
  quotaUsage = await getQuotaUsage(TEST_QUOTA_USAGE_ORIGIN);
  totalUsage = await SiteDataManager.getTotalUsage();
  is(cacheUsage, 0, "The cache usage should be removed");
  is(quotaUsage, 0, "The quota usage should be removed");
  is(totalUsage, 0, "The total usage should be removed");
  // Test accepting "Clear All Data" ends

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Test clearing service wroker through the "Clear All" button
add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
  // Register a test service
  await loadServiceWorkerTestPage(TEST_SERVICE_WORKER_URL);
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  // Test the initial states
  await promiseServiceWorkerRegisteredFor(TEST_SERVICE_WORKER_URL);
  // Click the "Clear All" button
  let doc = gBrowser.selectedBrowser.contentDocument;
  let clearBtn = doc.getElementById("clearSiteDataButton");
  let acceptPromise = promiseAlertDialogOpen("accept");
  let updatePromise = promiseSiteDataManagerSitesUpdated();
  clearBtn.doCommand();
  await acceptPromise;
  await updatePromise;
  await promiseServiceWorkersCleared();
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Test clearing service wroker through the settings panel
add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
  // Register a test service worker
  await loadServiceWorkerTestPage(TEST_SERVICE_WORKER_URL);
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  // Test the initial states
  await promiseServiceWorkerRegisteredFor(TEST_SERVICE_WORKER_URL);
  // Open the Site Data Settings panel and remove the site
  await openSiteDataSettingsDialog();
  let acceptRemovePromise = promiseAlertDialogOpen("accept");
  let updatePromise = promiseSiteDataManagerSitesUpdated();
  ContentTask.spawn(gBrowser.selectedBrowser, { TEST_OFFLINE_HOST }, args => {
    let host = args.TEST_OFFLINE_HOST;
    let frameDoc = content.gSubDialog._topDialog._frame.contentDocument;
    let sitesList = frameDoc.getElementById("sitesList");
    let site = sitesList.querySelector(`richlistitem[host="${host}"]`);
    if (site) {
      let removeBtn = frameDoc.getElementById("removeSelected");
      let saveBtn = frameDoc.getElementById("save");
      site.click();
      removeBtn.doCommand();
      saveBtn.doCommand();
    } else {
      ok(false, `Should have one site of ${host}`);
    }
  });
  await acceptRemovePromise;
  await updatePromise;
  await promiseServiceWorkersCleared();
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
