/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
/* import-globals-from ../../../../../testing/modules/sinon-1.16.1.js */
Services.scriptloader.loadSubScript("resource://testing-common/sinon-1.16.1.js");

const TEST_HOST = "example.com";
const TEST_ORIGIN = "http://" + TEST_HOST;
const TEST_BASE_URL = TEST_ORIGIN + "/browser/browser/components/preferences/in-content-old/tests/";
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

function removePersistentStoragePerm(origin) {
  let uri = NetUtil.newURI(origin);
  let principal = Services.scriptSecurityManager.createCodebasePrincipal(uri, {});
  Services.perms.removeFromPrincipal(principal, "persistent-storage");
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

function assertSitesListed(doc, origins) {
  let frameDoc = doc.getElementById("dialogFrame").contentDocument;
  let removeBtn = frameDoc.getElementById("removeSelected");
  let removeAllBtn = frameDoc.getElementById("removeAll");
  let sitesList = frameDoc.getElementById("sitesList");
  let totalSitesNumber = sitesList.getElementsByTagName("richlistitem").length;
  is(totalSitesNumber, origins.length, "Should list the right sites number");
  origins.forEach(origin => {
    let site = sitesList.querySelector(`richlistitem[data-origin="${origin}"]`);
    let host = site.getAttribute("host");
    ok(origin.includes(host), `Should list the site of ${origin}`);
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

// Test buttons are disabled and loading message shown while updating sites
add_task(function *() {
  yield SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
  let updatedPromise = promiseSitesUpdated();
  yield openPreferencesViaOpenPreferencesAPI("advanced", "networkTab", { leaveOpen: true });
  yield updatedPromise;

  let actual = null;
  let expected = null;
  let doc = gBrowser.selectedBrowser.contentDocument;
  let clearBtn = doc.getElementById("clearSiteDataButton");
  let settingsButton = doc.getElementById("siteDataSettings");
  let prefStrBundle = doc.getElementById("bundlePreferences");
  let totalSiteDataSizeLabel = doc.getElementById("totalSiteDataSize");
  is(clearBtn.disabled, false, "Should enable clear button after sites updated");
  is(settingsButton.disabled, false, "Should enable settings button after sites updated");
  yield SiteDataManager.getTotalUsage()
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
  yield SiteDataManager.getTotalUsage()
                       .then(usage => {
                          actual = totalSiteDataSizeLabel.textContent;
                          expected = prefStrBundle.getFormattedString(
                           "totalSiteDataSize", DownloadUtils.convertByteUnits(usage));
                          is(actual, expected, "Should show the right total site data size");
                       });

  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(function* () {
  yield SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
  addPersistentStoragePerm(TEST_ORIGIN);

  yield BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_BASE_URL + "site_data_test.html");
  yield waitForEvent(gBrowser.selectedBrowser.contentWindow, "test-indexedDB-done");
  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);

  yield openPreferencesViaOpenPreferencesAPI("advanced", "networkTab", { leaveOpen: true });

  // Test the initial states
  let cacheUsage = yield cacheUsageGetter.get();
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

  cacheUsage = yield cacheUsageGetter.get();
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

  cacheUsage = yield cacheUsageGetter.get();
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
  yield openSettingsDialog();

  let doc = gBrowser.selectedBrowser.contentDocument;
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
  yield openSettingsDialog();

  let doc = gBrowser.selectedBrowser.contentDocument;
  let frameDoc = doc.getElementById("dialogFrame").contentDocument;
  let searchBox = frameDoc.getElementById("searchBox");
  let mockOrigins = Array.from(mockSiteDataManager.sites.keys());

  searchBox.value = "xyz";
  searchBox.doCommand();
  assertSitesListed(doc, mockOrigins.filter(o => o.includes("xyz")));

  searchBox.value = "bar";
  searchBox.doCommand();
  assertSitesListed(doc, mockOrigins.filter(o => o.includes("bar")));

  searchBox.value = "";
  searchBox.doCommand();
  assertSitesListed(doc, mockOrigins);

  mockSiteDataManager.unregister();
  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Test selecting and removing all sites one by one
add_task(function* () {
  yield SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
  let fakeOrigins = [
    "https://news.foo.com/",
    "https://mails.bar.com/",
    "https://videos.xyz.com/",
    "https://books.foo.com/",
    "https://account.bar.com/",
    "https://shopping.xyz.com/"
  ];
  fakeOrigins.forEach(origin => addPersistentStoragePerm(origin));

  let updatePromise = promiseSitesUpdated();
  yield openPreferencesViaOpenPreferencesAPI("advanced", "networkTab", { leaveOpen: true });
  yield updatePromise;
  yield openSettingsDialog();

  let doc = gBrowser.selectedBrowser.contentDocument;
  let frameDoc = null;
  let saveBtn = null;
  let cancelBtn = null;
  let settingsDialogClosePromise = null;

  // Test the initial state
  assertAllSitesListed();

  // Test the "Cancel" button
  settingsDialogClosePromise = promiseSettingsDialogClose();
  frameDoc = doc.getElementById("dialogFrame").contentDocument;
  cancelBtn = frameDoc.getElementById("cancel");
  removeAllSitesOneByOne();
  assertAllSitesNotListed();
  cancelBtn.doCommand();
  yield settingsDialogClosePromise;
  yield openSettingsDialog();
  assertAllSitesListed();

  // Test the "Save Changes" button but cancelling save
  let cancelPromise = promiseAlertDialogOpen("cancel");
  settingsDialogClosePromise = promiseSettingsDialogClose();
  frameDoc = doc.getElementById("dialogFrame").contentDocument;
  saveBtn = frameDoc.getElementById("save");
  removeAllSitesOneByOne();
  assertAllSitesNotListed();
  saveBtn.doCommand();
  yield cancelPromise;
  yield settingsDialogClosePromise;
  yield openSettingsDialog();
  assertAllSitesListed();

  // Test the "Save Changes" button and accepting save
  let acceptPromise = promiseAlertDialogOpen("accept");
  settingsDialogClosePromise = promiseSettingsDialogClose();
  updatePromise = promiseSitesUpdated();
  frameDoc = doc.getElementById("dialogFrame").contentDocument;
  saveBtn = frameDoc.getElementById("save");
  removeAllSitesOneByOne();
  assertAllSitesNotListed();
  saveBtn.doCommand();
  yield acceptPromise;
  yield settingsDialogClosePromise;
  yield updatePromise;
  yield openSettingsDialog();
  assertAllSitesNotListed();

  // Always clean up the fake origins
  fakeOrigins.forEach(origin => removePersistentStoragePerm(origin));
  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);

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

  function assertAllSitesListed() {
    frameDoc = doc.getElementById("dialogFrame").contentDocument;
    let removeBtn = frameDoc.getElementById("removeSelected");
    let removeAllBtn = frameDoc.getElementById("removeAll");
    let sitesList = frameDoc.getElementById("sitesList");
    let sites = sitesList.getElementsByTagName("richlistitem");
    is(sites.length, fakeOrigins.length, "Should list all sites");
    is(removeBtn.disabled, false, "Should enable the removeSelected button");
    is(removeAllBtn.disabled, false, "Should enable the removeAllBtn button");
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
add_task(function* () {
  yield SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
  let fakeOrigins = [
    "https://news.foo.com/",
    "https://mails.bar.com/",
    "https://videos.xyz.com/",
    "https://books.foo.com/",
    "https://account.bar.com/",
    "https://shopping.xyz.com/"
  ];
  fakeOrigins.forEach(origin => addPersistentStoragePerm(origin));

  let updatePromise = promiseSitesUpdated();
  yield openPreferencesViaOpenPreferencesAPI("advanced", "networkTab", { leaveOpen: true });
  yield updatePromise;
  yield openSettingsDialog();

  let doc = gBrowser.selectedBrowser.contentDocument;
  let frameDoc = null;
  let saveBtn = null;
  let cancelBtn = null;
  let removeDialogOpenPromise = null;
  let settingsDialogClosePromise = null;

  // Test the initial state
  assertSitesListed(doc, fakeOrigins);

  // Test the "Cancel" button
  settingsDialogClosePromise = promiseSettingsDialogClose();
  frameDoc = doc.getElementById("dialogFrame").contentDocument;
  cancelBtn = frameDoc.getElementById("cancel");
  removeSelectedSite(fakeOrigins.slice(0, 4));
  assertSitesListed(doc, fakeOrigins.slice(4));
  cancelBtn.doCommand();
  yield settingsDialogClosePromise;
  yield openSettingsDialog();
  assertSitesListed(doc, fakeOrigins);

  // Test the "Save Changes" button but canceling save
  removeDialogOpenPromise = promiseWindowDialogOpen("cancel", REMOVE_DIALOG_URL);
  settingsDialogClosePromise = promiseSettingsDialogClose();
  frameDoc = doc.getElementById("dialogFrame").contentDocument;
  saveBtn = frameDoc.getElementById("save");
  removeSelectedSite(fakeOrigins.slice(0, 4));
  assertSitesListed(doc, fakeOrigins.slice(4));
  saveBtn.doCommand();
  yield removeDialogOpenPromise;
  yield settingsDialogClosePromise;
  yield openSettingsDialog();
  assertSitesListed(doc, fakeOrigins);

  // Test the "Save Changes" button and accepting save
  removeDialogOpenPromise = promiseWindowDialogOpen("accept", REMOVE_DIALOG_URL);
  settingsDialogClosePromise = promiseSettingsDialogClose();
  frameDoc = doc.getElementById("dialogFrame").contentDocument;
  saveBtn = frameDoc.getElementById("save");
  removeSelectedSite(fakeOrigins.slice(0, 4));
  assertSitesListed(doc, fakeOrigins.slice(4));
  saveBtn.doCommand();
  yield removeDialogOpenPromise;
  yield settingsDialogClosePromise;
  yield openSettingsDialog();
  assertSitesListed(doc, fakeOrigins.slice(4));

  // Always clean up the fake origins
  fakeOrigins.forEach(origin => removePersistentStoragePerm(origin));
  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);

  function removeSelectedSite(origins) {
    frameDoc = doc.getElementById("dialogFrame").contentDocument;
    let removeBtn = frameDoc.getElementById("removeSelected");
    let sitesList = frameDoc.getElementById("sitesList");
    origins.forEach(origin => {
      let site = sitesList.querySelector(`richlistitem[data-origin="${origin}"]`);
      if (site) {
        site.click();
        removeBtn.doCommand();
      } else {
        ok(false, `Should not select and remove inexisted site of ${origin}`);
      }
    });
  }
});

add_task(function* () {
  yield SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
  let fakeOrigins = [
    "https://news.foo.com/",
    "https://books.foo.com/",
    "https://mails.bar.com/",
    "https://account.bar.com/",
    "https://videos.xyz.com/",
    "https://shopping.xyz.com/"
  ];
  fakeOrigins.forEach(origin => addPersistentStoragePerm(origin));

  let updatePromise = promiseSitesUpdated();
  yield openPreferencesViaOpenPreferencesAPI("advanced", "networkTab", { leaveOpen: true });
  yield updatePromise;
  yield openSettingsDialog();

  // Search "foo" to only list foo.com sites
  let doc = gBrowser.selectedBrowser.contentDocument;
  let frameDoc = doc.getElementById("dialogFrame").contentDocument;
  let searchBox = frameDoc.getElementById("searchBox");
  searchBox.value = "foo";
  searchBox.doCommand();
  assertSitesListed(doc, fakeOrigins.slice(0, 2));

  // Test only removing all visible sites listed
  updatePromise = promiseSitesUpdated();
  let acceptRemovePromise = promiseWindowDialogOpen("accept", REMOVE_DIALOG_URL);
  let settingsDialogClosePromise = promiseSettingsDialogClose();
  let removeAllBtn = frameDoc.getElementById("removeAll");
  let saveBtn = frameDoc.getElementById("save");
  removeAllBtn.doCommand();
  saveBtn.doCommand();
  yield acceptRemovePromise;
  yield settingsDialogClosePromise;
  yield updatePromise;
  yield openSettingsDialog();
  assertSitesListed(doc, fakeOrigins.slice(2));

  // Always clean up the fake origins
  fakeOrigins.forEach(origin => removePersistentStoragePerm(origin));
  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
