/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
/* import-globals-from ../../../../../testing/modules/sinon-1.16.1.js */
Services.scriptloader.loadSubScript("resource://testing-common/sinon-1.16.1.js");

const TEST_HOST = "example.com";
const TEST_ORIGIN = "http://" + TEST_HOST;
const TEST_BASE_URL = TEST_ORIGIN + "/browser/browser/components/preferences/in-content/tests/";

const { NetUtil } = Cu.import("resource://gre/modules/NetUtil.jsm", {});
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
  sites: new Map([
    [
      "https://account.xyz.com/",
      {
        usage: 1024 * 200,
        host: "account.xyz.com",
        status: Ci.nsIPermissionManager.ALLOW_ACTION
      }
    ],
    [
      "https://shopping.xyz.com/",
      {
        usage: 1024 * 100,
        host: "shopping.xyz.com",
        status: Ci.nsIPermissionManager.DENY_ACTION
      }
    ],
    [
      "https://video.bar.com/",
      {
        usage: 1024 * 20,
        host: "video.bar.com",
        status: Ci.nsIPermissionManager.ALLOW_ACTION
      }
    ],
    [
      "https://music.bar.com/",
      {
        usage: 1024 * 10,
        host: "music.bar.com",
        status: Ci.nsIPermissionManager.DENY_ACTION
      }
    ],
    [
      "https://books.foo.com/",
      {
        usage: 1024 * 2,
        host: "books.foo.com",
        status: Ci.nsIPermissionManager.ALLOW_ACTION
      }
    ],
    [
      "https://news.foo.com/",
      {
        usage: 1024,
        host: "news.foo.com",
        status: Ci.nsIPermissionManager.DENY_ACTION
      }
    ]
  ]),

  _originalGetSites: null,

  getSites() {
    let list = [];
    this.sites.forEach((data, origin) => {
      list.push({
        usage: data.usage,
        status: data.status,
        uri: NetUtil.newURI(origin)
      });
    });
    return Promise.resolve(list);
  },

  register() {
    this._originalGetSites = SiteDataManager.getSites;
    SiteDataManager.getSites = this.getSites.bind(this);
  },

  unregister() {
    SiteDataManager.getSites = this._originalGetSites;
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
    Services.qms.getUsageForPrincipal(principal, request => resolve(request.usage));
  });
}

function getCacheUsage() {
  return new Promise(resolve => {
    let obs = {
      onNetworkCacheDiskConsumption(usage) {
        resolve(usage);
      },
      QueryInterface: XPCOMUtils.generateQI([
        Components.interfaces.nsICacheStorageConsumptionObserver,
        Components.interfaces.nsISupportsWeakReference
      ]),
    };
    Services.cache2.asyncGetDiskConsumption(obs);
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

registerCleanupFunction(function() {
  delete window.sinon;
  delete window.setImmediate;
  delete window.clearImmediate;
  mockOfflineAppCacheHelper.unregister();
});

add_task(function* () {
  yield SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
  addPersistentStoragePerm(TEST_ORIGIN);

  yield BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_BASE_URL + "site_data_test.html");
  yield waitForEvent(gBrowser.selectedBrowser.contentWindow, "test-indexedDB-done");
  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);

  yield openPreferencesViaOpenPreferencesAPI("advanced", "networkTab", { leaveOpen: true });

  // Test the initial states
  let cacheUsage = yield getCacheUsage();
  let quotaUsage = yield getQuotaUsage(TEST_ORIGIN);
  let totalUsage = yield SiteDataManager.getTotalUsage();
  Assert.greater(cacheUsage, 0, "The cache usage should not be 0");
  Assert.greater(quotaUsage, 0, "The quota usage should not be 0");
  Assert.greater(totalUsage, 0, "The total usage should not be 0");

  // Test cancelling "Clear All Data"
  // Click "Clear All Data" button and then cancel
  let doc = gBrowser.selectedBrowser.contentDocument;
  let cancelPromise = promiseAlertDialogOpen("cancel");
  let clearBtn = doc.getElementById("clearSiteDataButton");
  clearBtn.doCommand();
  yield cancelPromise;

  // Test the items are not removed
  let status = getPersistentStoragePermStatus(TEST_ORIGIN);
  is(status, Ci.nsIPermissionManager.ALLOW_ACTION, "Should not remove permission");

  cacheUsage = yield getCacheUsage();
  quotaUsage = yield getQuotaUsage(TEST_ORIGIN);
  totalUsage = yield SiteDataManager.getTotalUsage();
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
  yield acceptPromise;
  yield updatePromise;
  mockOfflineAppCacheHelper.unregister();

  // Test all the items are removed
  yield cookiesClearedPromise;

  ok(mockOfflineAppCacheHelper.clear.calledOnce, "Should clear app cache");

  status = getPersistentStoragePermStatus(TEST_ORIGIN);
  is(status, Ci.nsIPermissionManager.UNKNOWN_ACTION, "Should remove permission");

  cacheUsage = yield getCacheUsage();
  quotaUsage = yield getQuotaUsage(TEST_ORIGIN);
  totalUsage = yield SiteDataManager.getTotalUsage();
  is(cacheUsage, 0, "The cahce usage should be removed");
  is(quotaUsage, 0, "The quota usage should be removed");
  is(totalUsage, 0, "The total usage should be removed");
  // Test accepting "Clear All Data" ends

  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(function* () {
  yield SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});

  mockSiteDataManager.register();
  let updatePromise = promiseSitesUpdated();
  yield openPreferencesViaOpenPreferencesAPI("advanced", "networkTab", { leaveOpen: true });
  yield updatePromise;

  // Open the siteDataSettings subdialog
  let doc = gBrowser.selectedBrowser.contentDocument;
  let settingsBtn = doc.getElementById("siteDataSettings");
  let dialogOverlay = doc.getElementById("dialogOverlay");
  let dialogPromise = promiseLoadSubDialog("chrome://browser/content/preferences/siteDataSettings.xul");
  settingsBtn.doCommand();
  yield dialogPromise;
  is(dialogOverlay.style.visibility, "visible", "The dialog should be visible");

  let dialogFrame = doc.getElementById("dialogFrame");
  let frameDoc = dialogFrame.contentDocument;
  let hostCol = frameDoc.getElementById("hostCol");
  let usageCol = frameDoc.getElementById("usageCol");
  let statusCol = frameDoc.getElementById("statusCol");
  let sitesList = frameDoc.getElementById("sitesList");
  let mockSites = mockSiteDataManager.sites;

  // Test default sorting
  assertSortByHost("ascending");

  // Test sorting on the host column
  hostCol.click();
  assertSortByHost("descending");
  hostCol.click();
  assertSortByHost("ascending");

  // Test sorting on the permission status column
  statusCol.click();
  assertSortByStatus("ascending");
  statusCol.click();
  assertSortByStatus("descending");

  // Test sorting on the usage column
  usageCol.click();
  assertSortByUsage("ascending");
  usageCol.click();
  assertSortByUsage("descending");

  mockSiteDataManager.unregister();
  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);

  function assertSortByHost(order) {
    let siteItems = sitesList.getElementsByTagName("richlistitem");
    for (let i = 0; i < siteItems.length - 1; ++i) {
      let aOrigin = siteItems[i].getAttribute("data-origin");
      let bOrigin = siteItems[i + 1].getAttribute("data-origin");
      let a = mockSites.get(aOrigin);
      let b = mockSites.get(bOrigin);
      let result = a.host.localeCompare(b.host);
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
      let aOrigin = siteItems[i].getAttribute("data-origin");
      let bOrigin = siteItems[i + 1].getAttribute("data-origin");
      let a = mockSites.get(aOrigin);
      let b = mockSites.get(bOrigin);
      let result = a.status - b.status;
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
      let aOrigin = siteItems[i].getAttribute("data-origin");
      let bOrigin = siteItems[i + 1].getAttribute("data-origin");
      let a = mockSites.get(aOrigin);
      let b = mockSites.get(bOrigin);
      let result = a.usage - b.usage;
      if (order == "ascending") {
        Assert.lessOrEqual(result, 0, "Should sort sites in the ascending order by usage");
      } else {
        Assert.greaterOrEqual(result, 0, "Should sort sites in the descending order by usage");
      }
    }
  }
});

add_task(function* () {
  yield SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});

  mockSiteDataManager.register();
  let updatePromise = promiseSitesUpdated();
  yield openPreferencesViaOpenPreferencesAPI("advanced", "networkTab", { leaveOpen: true });
  yield updatePromise;

  // Open the siteDataSettings subdialog
  let doc = gBrowser.selectedBrowser.contentDocument;
  let settingsBtn = doc.getElementById("siteDataSettings");
  let dialogOverlay = doc.getElementById("dialogOverlay");
  let dialogPromise = promiseLoadSubDialog("chrome://browser/content/preferences/siteDataSettings.xul");
  settingsBtn.doCommand();
  yield dialogPromise;
  is(dialogOverlay.style.visibility, "visible", "The dialog should be visible");

  let frameDoc = doc.getElementById("dialogFrame").contentDocument;
  let searchBox = frameDoc.getElementById("searchBox");
  let mockOrigins = Array.from(mockSiteDataManager.sites.keys());

  searchBox.value = "xyz";
  searchBox.doCommand();
  assertSitesListed(mockOrigins.filter(o => o.includes("xyz")));

  searchBox.value = "bar";
  searchBox.doCommand();
  assertSitesListed(mockOrigins.filter(o => o.includes("bar")));

  searchBox.value = "";
  searchBox.doCommand();
  assertSitesListed(mockOrigins);

  mockSiteDataManager.unregister();
  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);

  function assertSitesListed(origins) {
    let sitesList = frameDoc.getElementById("sitesList");
    let totalSitesNumber = sitesList.getElementsByTagName("richlistitem").length;
    is(totalSitesNumber, origins.length, "Should list the right sites number");
    origins.forEach(origin => {
      let site = sitesList.querySelector(`richlistitem[data-origin="${origin}"]`);
      ok(site instanceof XULElement, `Should list the site of ${origin}`);
    });
  }
});
