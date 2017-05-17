/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
/* import-globals-from ../../../../../testing/modules/sinon-1.16.1.js */
Services.scriptloader.loadSubScript("resource://testing-common/sinon-1.16.1.js");

const TEST_QUOTA_USAGE_HOST = "example.com";
const TEST_QUOTA_USAGE_ORIGIN = "https://" + TEST_QUOTA_USAGE_HOST;
const TEST_QUOTA_USAGE_URL = TEST_QUOTA_USAGE_ORIGIN + "/browser/browser/components/preferences/in-content-old/tests/site_data_test.html";
const TEST_OFFLINE_HOST = "example.org";
const TEST_OFFLINE_ORIGIN = "https://" + TEST_OFFLINE_HOST;
const TEST_OFFLINE_URL = TEST_OFFLINE_ORIGIN + "/browser/browser/components/preferences/in-content-old/tests/offline/offline.html";
const REMOVE_DIALOG_URL = "chrome://browser/content/preferences/siteDataRemoveSelected.xul";

const { NetUtil } = Cu.import("resource://gre/modules/NetUtil.jsm", {});
const { DownloadUtils } = Cu.import("resource://gre/modules/DownloadUtils.jsm", {});
const { SiteDataManager } = Cu.import("resource:///modules/SiteDataManager.jsm", {});
const { OfflineAppCacheHelper } = Cu.import("resource:///modules/offlineAppCache.jsm", {});

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

const mockSiteDataManager = {

  _originalGetQuotaUsage: null,
  _originalRemoveQuotaUsage: null,

  _getQuotaUsage() {
    let results = [];
    this.fakeSites.forEach(site => {
      results.push({
        origin: site.principal.origin,
        usage: site.usage,
        persisted: site.persisted
      });
    });
    SiteDataManager._getQuotaUsagePromise = Promise.resolve(results);
    return SiteDataManager._getQuotaUsagePromise;
  },

  _removeQuotaUsage(site) {
    var target = site.principals[0].URI.host;
    this.fakeSites = this.fakeSites.filter(fakeSite => {
      return fakeSite.principal.URI.host != target;
    });
  },

  register() {
    this._originalGetQuotaUsage = SiteDataManager._getQuotaUsage;
    SiteDataManager._getQuotaUsage = this._getQuotaUsage.bind(this);
    this._originalRemoveQuotaUsage = SiteDataManager._removeQuotaUsage;
    SiteDataManager._removeQuotaUsage = this._removeQuotaUsage.bind(this);
    this.fakeSites = null;
  },

  unregister() {
    SiteDataManager._getQuotaUsage = this._originalGetQuotaUsage;
    SiteDataManager._removeQuotaUsage = this._originalRemoveQuotaUsage;
  }
};

function addPersistentStoragePerm(origin) {
  let uri = NetUtil.newURI(origin);
  let principal = Services.scriptSecurityManager.createCodebasePrincipal(uri, {});
  Services.perms.addFromPrincipal(principal, "persistent-storage", Ci.nsIPermissionManager.ALLOW_ACTION);
}

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

function openSettingsDialog() {
  let doc = gBrowser.selectedBrowser.contentDocument;
  let settingsBtn = doc.getElementById("siteDataSettings");
  let dialogOverlay = doc.getElementById("dialogOverlay");
  let dialogLoadPromise = promiseLoadSubDialog("chrome://browser/content/preferences/siteDataSettings.xul");
  let dialogInitPromise = TestUtils.topicObserved("sitedata-settings-init", () => true);
  let fullyLoadPromise = Promise.all([ dialogLoadPromise, dialogInitPromise ]).then(() => {
    is(dialogOverlay.style.visibility, "visible", "The Settings dialog should be visible");
  });
  settingsBtn.doCommand();
  return fullyLoadPromise;
}

function promiseSettingsDialogClose() {
  return new Promise(resolve => {
    let doc = gBrowser.selectedBrowser.contentDocument;
    let dialogOverlay = doc.getElementById("dialogOverlay");
    let win = content.gSubDialog._frame.contentWindow;
    win.addEventListener("unload", function unload() {
      if (win.document.documentURI === "chrome://browser/content/preferences/siteDataSettings.xul") {
        isnot(dialogOverlay.style.visibility, "visible", "The Settings dialog should be hidden");
        resolve();
      }
    }, { once: true });
  });
}

function promiseSitesUpdated() {
  return TestUtils.topicObserved("sitedatamanager:sites-updated", () => true);
}

function promiseCookiesCleared() {
  return TestUtils.topicObserved("cookie-changed", (subj, data) => {
    return data === "cleared";
  });
}

function assertSitesListed(doc, hosts) {
  let frameDoc = doc.getElementById("dialogFrame").contentDocument;
  let removeBtn = frameDoc.getElementById("removeSelected");
  let removeAllBtn = frameDoc.getElementById("removeAll");
  let sitesList = frameDoc.getElementById("sitesList");
  let totalSitesNumber = sitesList.getElementsByTagName("richlistitem").length;
  is(totalSitesNumber, hosts.length, "Should list the right sites number");
  hosts.forEach(host => {
    let site = sitesList.querySelector(`richlistitem[host="${host}"]`);
    ok(site, `Should list the site of ${host}`);
  });
  is(removeBtn.disabled, false, "Should enable the removeSelected button");
  is(removeAllBtn.disabled, false, "Should enable the removeAllBtn button");
}

registerCleanupFunction(function() {
  delete window.sinon;
  delete window.setImmediate;
  delete window.clearImmediate;
  mockOfflineAppCacheHelper.unregister();
});

// Test grouping and listing sites across scheme, port and origin attributes by host
add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
  const quotaUsage = 1024;
  mockSiteDataManager.register();
  mockSiteDataManager.fakeSites = [
    {
      usage: quotaUsage,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("https://account.xyz.com^userContextId=1"),
      persisted: true
    },
    {
      usage: quotaUsage,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("https://account.xyz.com"),
      persisted: false
    },
    {
      usage: quotaUsage,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("https://account.xyz.com:123"),
      persisted: false
    },
    {
      usage: quotaUsage,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("http://account.xyz.com"),
      persisted: false
    },
  ];

  let updatedPromise = promiseSitesUpdated();
  await openPreferencesViaOpenPreferencesAPI("advanced", "networkTab", { leaveOpen: true });
  await updatedPromise;
  await openSettingsDialog();
  let dialogFrame = gBrowser.selectedBrowser.contentDocument.getElementById("dialogFrame");
  let frameDoc = dialogFrame.contentDocument;

  let siteItems = frameDoc.getElementsByTagName("richlistitem");
  is(siteItems.length, 1, "Should group sites across scheme, port and origin attributes");

  let expected = "account.xyz.com";
  let hostCol = siteItems[0].getAttribute("host");
  is(hostCol, expected, "Should group and list sites by host");

  let prefStrBundle = frameDoc.getElementById("bundlePreferences");
  expected = prefStrBundle.getFormattedString("siteUsage",
    DownloadUtils.convertByteUnits(quotaUsage * mockSiteDataManager.fakeSites.length));
  let usageCol = siteItems[0].getAttribute("usage");
  is(usageCol, expected, "Should sum up usages across scheme, port and origin attributes");

  expected = prefStrBundle.getString("persistent");
  let statusCol = siteItems[0].getAttribute("status");
  is(statusCol, expected, "Should mark persisted status across scheme, port and origin attributes");

  mockSiteDataManager.unregister();
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Test listing site using quota usage or site using appcache
add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});

  // Open a test site which would save into appcache
  await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_OFFLINE_URL);
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);

  // Open a test site which would save into quota manager
  await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_QUOTA_USAGE_URL);
  await waitForEvent(gBrowser.selectedBrowser.contentWindow, "test-indexedDB-done");
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);

  let updatedPromise = promiseSitesUpdated();
  await openPreferencesViaOpenPreferencesAPI("advanced", "networkTab", { leaveOpen: true });
  await updatedPromise;
  await openSettingsDialog();
  let doc = gBrowser.selectedBrowser.contentDocument;
  let dialogFrame = doc.getElementById("dialogFrame");
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
  let updatedPromise = promiseSitesUpdated();
  await openPreferencesViaOpenPreferencesAPI("advanced", "networkTab", { leaveOpen: true });
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
  await waitForEvent(gBrowser.selectedBrowser.contentWindow, "test-indexedDB-done");
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);

  await openPreferencesViaOpenPreferencesAPI("advanced", "networkTab", { leaveOpen: true });

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
  let updatePromise = promiseSitesUpdated();
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

// Test sorting
add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
  mockSiteDataManager.register();
  mockSiteDataManager.fakeSites = [
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("https://account.xyz.com"),
      persisted: true
    },
    {
      usage: 1024 * 2,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("https://books.foo.com"),
      persisted: false
    },
    {
      usage: 1024 * 3,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("http://cinema.bar.com"),
      persisted: true
    },
  ];

  let updatePromise = promiseSitesUpdated();
  await openPreferencesViaOpenPreferencesAPI("advanced", "networkTab", { leaveOpen: true });
  await updatePromise;
  await openSettingsDialog();

  let doc = gBrowser.selectedBrowser.contentDocument;
  let dialogFrame = doc.getElementById("dialogFrame");
  let frameDoc = dialogFrame.contentDocument;
  let hostCol = frameDoc.getElementById("hostCol");
  let usageCol = frameDoc.getElementById("usageCol");
  let statusCol = frameDoc.getElementById("statusCol");
  let sitesList = frameDoc.getElementById("sitesList");

  // Test default sorting
  assertSortByUsage("descending");

  // Test sorting on the usage column
  usageCol.click();
  assertSortByUsage("ascending");
  usageCol.click();
  assertSortByUsage("descending");

  // Test sorting on the host column
  hostCol.click();
  assertSortByHost("ascending");
  hostCol.click();
  assertSortByHost("descending");

  // Test sorting on the permission status column
  statusCol.click();
  assertSortByStatus("ascending");
  statusCol.click();
  assertSortByStatus("descending");

  mockSiteDataManager.unregister();
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);

  function assertSortByHost(order) {
    let siteItems = sitesList.getElementsByTagName("richlistitem");
    for (let i = 0; i < siteItems.length - 1; ++i) {
      let aHost = siteItems[i].getAttribute("host");
      let bHost = siteItems[i + 1].getAttribute("host");
      let result = aHost.localeCompare(bHost);
      if (order == "ascending") {
        Assert.lessOrEqual(result, 0, "Should sort sites in the ascending order by host");
      } else {
        Assert.greaterOrEqual(result, 0, "Should sort sites in the descending order by host");
      }
    }
  }

  function assertSortByStatus(order) {
    let siteItems = sitesList.getElementsByTagName("richlistitem");
    for (let i = 0; i < siteItems.length - 1; ++i) {
      let aHost = siteItems[i].getAttribute("host");
      let bHost = siteItems[i + 1].getAttribute("host");
      let a = findSiteByHost(aHost);
      let b = findSiteByHost(bHost);
      let result = 0;
      if (a.persisted && !b.persisted) {
        result = 1;
      } else if (!a.persisted && b.persisted) {
        result = -1;
      }
      if (order == "ascending") {
        Assert.lessOrEqual(result, 0, "Should sort sites in the ascending order by permission status");
      } else {
        Assert.greaterOrEqual(result, 0, "Should sort sites in the descending order by permission status");
      }
    }
  }

  function assertSortByUsage(order) {
    let siteItems = sitesList.getElementsByTagName("richlistitem");
    for (let i = 0; i < siteItems.length - 1; ++i) {
      let aHost = siteItems[i].getAttribute("host");
      let bHost = siteItems[i + 1].getAttribute("host");
      let a = findSiteByHost(aHost);
      let b = findSiteByHost(bHost);
      let result = a.usage - b.usage;
      if (order == "ascending") {
        Assert.lessOrEqual(result, 0, "Should sort sites in the ascending order by usage");
      } else {
        Assert.greaterOrEqual(result, 0, "Should sort sites in the descending order by usage");
      }
    }
  }

  function findSiteByHost(host) {
    return mockSiteDataManager.fakeSites.find(site => site.principal.URI.host == host);
  }
});

// Test search on the host column
add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
  mockSiteDataManager.register();
  mockSiteDataManager.fakeSites = [
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("https://account.xyz.com"),
      persisted: true
    },
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("https://shopping.xyz.com"),
      persisted: false
    },
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("http://cinema.bar.com"),
      persisted: true
    },
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("http://email.bar.com"),
      persisted: false
    },
  ];
  let fakeHosts = mockSiteDataManager.fakeSites.map(site => site.principal.URI.host);

  let updatePromise = promiseSitesUpdated();
  await openPreferencesViaOpenPreferencesAPI("advanced", "networkTab", { leaveOpen: true });
  await updatePromise;
  await openSettingsDialog();

  let doc = gBrowser.selectedBrowser.contentDocument;
  let frameDoc = doc.getElementById("dialogFrame").contentDocument;
  let searchBox = frameDoc.getElementById("searchBox");

  searchBox.value = "xyz";
  searchBox.doCommand();
  assertSitesListed(doc, fakeHosts.filter(host => host.includes("xyz")));

  searchBox.value = "bar";
  searchBox.doCommand();
  assertSitesListed(doc, fakeHosts.filter(host => host.includes("bar")));

  searchBox.value = "";
  searchBox.doCommand();
  assertSitesListed(doc, fakeHosts);

  mockSiteDataManager.unregister();
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Test selecting and removing all sites one by one
add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
  mockSiteDataManager.register();
  mockSiteDataManager.fakeSites = [
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("https://account.xyz.com"),
      persisted: true
    },
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("https://shopping.xyz.com"),
      persisted: false
    },
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("http://cinema.bar.com"),
      persisted: true
    },
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("http://email.bar.com"),
      persisted: false
    },
  ];
  let fakeHosts = mockSiteDataManager.fakeSites.map(site => site.principal.URI.host);

  let updatePromise = promiseSitesUpdated();
  await openPreferencesViaOpenPreferencesAPI("advanced", "networkTab", { leaveOpen: true });
  await updatePromise;
  await openSettingsDialog();

  let doc = gBrowser.selectedBrowser.contentDocument;
  let frameDoc = null;
  let saveBtn = null;
  let cancelBtn = null;
  let settingsDialogClosePromise = null;

  // Test the initial state
  assertSitesListed(doc, fakeHosts);

  // Test the "Cancel" button
  settingsDialogClosePromise = promiseSettingsDialogClose();
  frameDoc = doc.getElementById("dialogFrame").contentDocument;
  cancelBtn = frameDoc.getElementById("cancel");
  removeAllSitesOneByOne();
  assertAllSitesNotListed();
  cancelBtn.doCommand();
  await settingsDialogClosePromise;
  await openSettingsDialog();
  assertSitesListed(doc, fakeHosts);

  // Test the "Save Changes" button but cancelling save
  let cancelPromise = promiseAlertDialogOpen("cancel");
  settingsDialogClosePromise = promiseSettingsDialogClose();
  frameDoc = doc.getElementById("dialogFrame").contentDocument;
  saveBtn = frameDoc.getElementById("save");
  removeAllSitesOneByOne();
  assertAllSitesNotListed();
  saveBtn.doCommand();
  await cancelPromise;
  await settingsDialogClosePromise;
  await openSettingsDialog();
  assertSitesListed(doc, fakeHosts);

  // Test the "Save Changes" button and accepting save
  let acceptPromise = promiseAlertDialogOpen("accept");
  settingsDialogClosePromise = promiseSettingsDialogClose();
  updatePromise = promiseSitesUpdated();
  frameDoc = doc.getElementById("dialogFrame").contentDocument;
  saveBtn = frameDoc.getElementById("save");
  removeAllSitesOneByOne();
  assertAllSitesNotListed();
  saveBtn.doCommand();
  await acceptPromise;
  await settingsDialogClosePromise;
  await updatePromise;
  await openSettingsDialog();
  assertAllSitesNotListed();

  mockSiteDataManager.unregister();
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);

  function removeAllSitesOneByOne() {
    frameDoc = doc.getElementById("dialogFrame").contentDocument;
    let removeBtn = frameDoc.getElementById("removeSelected");
    let sitesList = frameDoc.getElementById("sitesList");
    let sites = sitesList.getElementsByTagName("richlistitem");
    for (let i = sites.length - 1; i >= 0; --i) {
      sites[i].click();
      removeBtn.doCommand();
    }
  }

  function assertAllSitesNotListed() {
    frameDoc = doc.getElementById("dialogFrame").contentDocument;
    let removeBtn = frameDoc.getElementById("removeSelected");
    let removeAllBtn = frameDoc.getElementById("removeAll");
    let sitesList = frameDoc.getElementById("sitesList");
    let sites = sitesList.getElementsByTagName("richlistitem");
    is(sites.length, 0, "Should not list all sites");
    is(removeBtn.disabled, true, "Should disable the removeSelected button");
    is(removeAllBtn.disabled, true, "Should disable the removeAllBtn button");
  }
});

// Test selecting and removing partial sites
add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
  mockSiteDataManager.register();
  mockSiteDataManager.fakeSites = [
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("https://account.xyz.com"),
      persisted: true
    },
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("https://shopping.xyz.com"),
      persisted: false
    },
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("http://cinema.bar.com"),
      persisted: true
    },
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("http://email.bar.com"),
      persisted: false
    },
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("https://s3-us-west-2.amazonaws.com"),
      persisted: true
    },
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("https://127.0.0.1"),
      persisted: false
    },
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("https://[0:0:0:0:0:0:0:1]"),
      persisted: true
    },
  ];
  let fakeHosts = mockSiteDataManager.fakeSites.map(site => site.principal.URI.host);

  let updatePromise = promiseSitesUpdated();
  await openPreferencesViaOpenPreferencesAPI("advanced", "networkTab", { leaveOpen: true });
  await updatePromise;
  await openSettingsDialog();

  let doc = gBrowser.selectedBrowser.contentDocument;
  let frameDoc = null;
  let saveBtn = null;
  let cancelBtn = null;
  let removeDialogOpenPromise = null;
  let settingsDialogClosePromise = null;

  // Test the initial state
  assertSitesListed(doc, fakeHosts);

  // Test the "Cancel" button
  settingsDialogClosePromise = promiseSettingsDialogClose();
  frameDoc = doc.getElementById("dialogFrame").contentDocument;
  cancelBtn = frameDoc.getElementById("cancel");
  removeSelectedSite(fakeHosts.slice(0, 2));
  assertSitesListed(doc, fakeHosts.slice(2));
  cancelBtn.doCommand();
  await settingsDialogClosePromise;
  await openSettingsDialog();
  assertSitesListed(doc, fakeHosts);

  // Test the "Save Changes" button but canceling save
  removeDialogOpenPromise = promiseWindowDialogOpen("cancel", REMOVE_DIALOG_URL);
  settingsDialogClosePromise = promiseSettingsDialogClose();
  frameDoc = doc.getElementById("dialogFrame").contentDocument;
  saveBtn = frameDoc.getElementById("save");
  removeSelectedSite(fakeHosts.slice(0, 2));
  assertSitesListed(doc, fakeHosts.slice(2));
  saveBtn.doCommand();
  await removeDialogOpenPromise;
  await settingsDialogClosePromise;
  await openSettingsDialog();
  assertSitesListed(doc, fakeHosts);

  // Test the "Save Changes" button and accepting save
  removeDialogOpenPromise = promiseWindowDialogOpen("accept", REMOVE_DIALOG_URL);
  settingsDialogClosePromise = promiseSettingsDialogClose();
  frameDoc = doc.getElementById("dialogFrame").contentDocument;
  saveBtn = frameDoc.getElementById("save");
  removeSelectedSite(fakeHosts.slice(0, 2));
  assertSitesListed(doc, fakeHosts.slice(2));
  saveBtn.doCommand();
  await removeDialogOpenPromise;
  await settingsDialogClosePromise;
  await openSettingsDialog();
  assertSitesListed(doc, fakeHosts.slice(2));

  mockSiteDataManager.unregister();
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);

  function removeSelectedSite(hosts) {
    frameDoc = doc.getElementById("dialogFrame").contentDocument;
    let removeBtn = frameDoc.getElementById("removeSelected");
    let sitesList = frameDoc.getElementById("sitesList");
    hosts.forEach(host => {
      let site = sitesList.querySelector(`richlistitem[host="${host}"]`);
      if (site) {
        site.click();
        removeBtn.doCommand();
      } else {
        ok(false, `Should not select and remove inexistent site of ${host}`);
      }
    });
  }
});

// Test searching and then removing only visible sites
add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
  mockSiteDataManager.register(SiteDataManager);
  mockSiteDataManager.fakeSites = [
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("https://account.xyz.com"),
      persisted: true
    },
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("https://shopping.xyz.com"),
      persisted: false
    },
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("http://cinema.bar.com"),
      persisted: true
    },
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("http://email.bar.com"),
      persisted: false
    },
  ];
  let fakeHosts = mockSiteDataManager.fakeSites.map(site => site.principal.URI.host);

  let updatePromise = promiseSitesUpdated();
  await openPreferencesViaOpenPreferencesAPI("advanced", "networkTab", { leaveOpen: true });
  await updatePromise;
  await openSettingsDialog();

  // Search "foo" to only list foo.com sites
  let doc = gBrowser.selectedBrowser.contentDocument;
  let frameDoc = doc.getElementById("dialogFrame").contentDocument;
  let searchBox = frameDoc.getElementById("searchBox");
  searchBox.value = "xyz";
  searchBox.doCommand();
  assertSitesListed(doc, fakeHosts.filter(host => host.includes("xyz")));

  // Test only removing all visible sites listed
  updatePromise = promiseSitesUpdated();
  let acceptRemovePromise = promiseWindowDialogOpen("accept", REMOVE_DIALOG_URL);
  let settingsDialogClosePromise = promiseSettingsDialogClose();
  let removeAllBtn = frameDoc.getElementById("removeAll");
  let saveBtn = frameDoc.getElementById("save");
  removeAllBtn.doCommand();
  saveBtn.doCommand();
  await acceptRemovePromise;
  await settingsDialogClosePromise;
  await updatePromise;
  await openSettingsDialog();
  assertSitesListed(doc, fakeHosts.filter(host => !host.includes("xyz")));

  mockSiteDataManager.unregister();
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
