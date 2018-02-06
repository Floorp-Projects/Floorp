/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_QUOTA_USAGE_HOST = "example.com";
const TEST_QUOTA_USAGE_ORIGIN = "https://" + TEST_QUOTA_USAGE_HOST;
const TEST_QUOTA_USAGE_URL = TEST_QUOTA_USAGE_ORIGIN + "/browser/browser/components/preferences/in-content/tests/site_data_test.html";
const TEST_OFFLINE_HOST = "example.org";
const TEST_OFFLINE_ORIGIN = "https://" + TEST_OFFLINE_HOST;
const TEST_OFFLINE_URL = TEST_OFFLINE_ORIGIN + "/browser/browser/components/preferences/in-content/tests/offline/offline.html";
const TEST_SERVICE_WORKER_URL = TEST_OFFLINE_ORIGIN + "/browser/browser/components/preferences/in-content/tests/service_worker_test.html";

const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm", {});
const { DownloadUtils } = ChromeUtils.import("resource://gre/modules/DownloadUtils.jsm", {});
const { SiteDataManager } = ChromeUtils.import("resource:///modules/SiteDataManager.jsm", {});
const { OfflineAppCacheHelper } = ChromeUtils.import("resource:///modules/offlineAppCache.jsm", {});

function getPersistentStoragePermStatus(origin) {
  let uri = NetUtil.newURI(origin);
  let principal = Services.scriptSecurityManager.createCodebasePrincipal(uri, {});
  return Services.perms.testExactPermissionFromPrincipal(principal, "persistent-storage");
}

// Test listing site using quota usage or site using appcache
add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});

  // Open a test site which would save into appcache
  await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_OFFLINE_URL);
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);

  // Open a test site which would save into quota manager
  BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_QUOTA_USAGE_URL);
  await BrowserTestUtils.waitForContentEvent(
    gBrowser.selectedBrowser, "test-indexedDB-done", false, null, true);
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
