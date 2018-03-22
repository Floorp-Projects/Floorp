/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_QUOTA_USAGE_HOST = "example.com";
const TEST_QUOTA_USAGE_ORIGIN = "https://" + TEST_QUOTA_USAGE_HOST;
const TEST_QUOTA_USAGE_URL = getRootDirectory(gTestPath).replace("chrome://mochitests/content", TEST_QUOTA_USAGE_ORIGIN) + "/site_data_test.html";
const TEST_OFFLINE_HOST = "example.org";
const TEST_OFFLINE_ORIGIN = "https://" + TEST_OFFLINE_HOST;
const TEST_OFFLINE_URL = getRootDirectory(gTestPath).replace("chrome://mochitests/content", TEST_OFFLINE_ORIGIN) + "/offline/offline.html";
const TEST_SERVICE_WORKER_URL = getRootDirectory(gTestPath).replace("chrome://mochitests/content", TEST_OFFLINE_ORIGIN) + "/service_worker_test.html";

const REMOVE_DIALOG_URL = "chrome://browser/content/preferences/siteDataRemoveSelected.xul";

const { DownloadUtils } = ChromeUtils.import("resource://gre/modules/DownloadUtils.jsm", {});
const { SiteDataManager } = ChromeUtils.import("resource:///modules/SiteDataManager.jsm", {});
const { OfflineAppCacheHelper } = ChromeUtils.import("resource://gre/modules/offlineAppCache.jsm", {});

ChromeUtils.defineModuleGetter(this, "SiteDataTestUtils",
                               "resource://testing-common/SiteDataTestUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "serviceWorkerManager", "@mozilla.org/serviceworkers/manager;1", "nsIServiceWorkerManager");

function promiseSiteDataManagerSitesUpdated() {
  return TestUtils.topicObserved("sitedatamanager:sites-updated", () => true);
}

function is_element_visible(aElement, aMsg) {
  isnot(aElement, null, "Element should not be null, when checking visibility");
  ok(!BrowserTestUtils.is_hidden(aElement), aMsg);
}

function is_element_hidden(aElement, aMsg) {
  isnot(aElement, null, "Element should not be null, when checking visibility");
  ok(BrowserTestUtils.is_hidden(aElement), aMsg);
}

function promiseLoadSubDialog(aURL) {
  return new Promise((resolve, reject) => {
    content.gSubDialog._dialogStack.addEventListener("dialogopen", function dialogopen(aEvent) {
      if (aEvent.detail.dialog._frame.contentWindow.location == "about:blank")
        return;
      content.gSubDialog._dialogStack.removeEventListener("dialogopen", dialogopen);

      is(aEvent.detail.dialog._frame.contentWindow.location.toString(), aURL,
        "Check the proper URL is loaded");

      // Check visibility
      is_element_visible(aEvent.detail.dialog._overlay, "Overlay is visible");

      // Check that stylesheets were injected
      let expectedStyleSheetURLs = aEvent.detail.dialog._injectedStyleSheets.slice(0);
      for (let styleSheet of aEvent.detail.dialog._frame.contentDocument.styleSheets) {
        let i = expectedStyleSheetURLs.indexOf(styleSheet.href);
        if (i >= 0) {
          info("found " + styleSheet.href);
          expectedStyleSheetURLs.splice(i, 1);
        }
      }
      is(expectedStyleSheetURLs.length, 0, "All expectedStyleSheetURLs should have been found");

      // Wait for the next event tick to make sure the remaining part of the
      // testcase runs after the dialog gets ready for input.
      executeSoon(() => resolve(aEvent.detail.dialog._frame.contentWindow));
    });
  });
}

function openPreferencesViaOpenPreferencesAPI(aPane, aOptions) {
  return new Promise(resolve => {
    let finalPrefPaneLoaded = TestUtils.topicObserved("sync-pane-loaded", () => true);
    gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
    openPreferences(aPane);
    let newTabBrowser = gBrowser.selectedBrowser;

    newTabBrowser.addEventListener("Initialized", function() {
      newTabBrowser.contentWindow.addEventListener("load", async function() {
        let win = gBrowser.contentWindow;
        let selectedPane = win.history.state;
        await finalPrefPaneLoaded;
        if (!aOptions || !aOptions.leaveOpen)
          gBrowser.removeCurrentTab();
        resolve({ selectedPane });
      }, { once: true });
    }, { capture: true, once: true });

  });
}

function openSiteDataSettingsDialog() {
  let doc = gBrowser.selectedBrowser.contentDocument;
  let settingsBtn = doc.getElementById("siteDataSettings");
  let dialogOverlay = content.gSubDialog._preloadDialog._overlay;
  let dialogLoadPromise = promiseLoadSubDialog("chrome://browser/content/preferences/siteDataSettings.xul");
  let dialogInitPromise = TestUtils.topicObserved("sitedata-settings-init", () => true);
  let fullyLoadPromise = Promise.all([dialogLoadPromise, dialogInitPromise]).then(() => {
    is(dialogOverlay.style.visibility, "visible", "The Settings dialog should be visible");
  });
  settingsBtn.doCommand();
  return fullyLoadPromise;
}

function promiseSettingsDialogClose() {
  return new Promise(resolve => {
    let win = gBrowser.selectedBrowser.contentWindow;
    let dialogOverlay = win.gSubDialog._topDialog._overlay;
    let dialogWin = win.gSubDialog._topDialog._frame.contentWindow;
    dialogWin.addEventListener("unload", function unload() {
      if (dialogWin.document.documentURI === "chrome://browser/content/preferences/siteDataSettings.xul") {
        isnot(dialogOverlay.style.visibility, "visible", "The Settings dialog should be hidden");
        resolve();
      }
    }, { once: true });
  });
}

function assertSitesListed(doc, hosts) {
  let frameDoc = content.gSubDialog._topDialog._frame.contentDocument;
  let removeAllBtn = frameDoc.getElementById("removeAll");
  let sitesList = frameDoc.getElementById("sitesList");
  let totalSitesNumber = sitesList.getElementsByTagName("richlistitem").length;
  is(totalSitesNumber, hosts.length, "Should list the right sites number");
  hosts.forEach(host => {
    let site = sitesList.querySelector(`richlistitem[host="${host}"]`);
    ok(site, `Should list the site of ${host}`);
  });
  is(removeAllBtn.disabled, false, "Should enable the removeAllBtn button");
}

const mockSiteDataManager = {

  _SiteDataManager: null,
  _originalQMS: null,
  _originalRemoveQuotaUsage: null,

  getUsage(onUsageResult) {
    let result = this.fakeSites.map(site => ({
      origin: site.principal.origin,
      usage: site.usage,
      persisted: site.persisted,
      lastAccessed: site.lastAccessed,
    }));
    onUsageResult({ result, resultCode: Cr.NS_OK });
  },

  _removeQuotaUsage(site) {
    var target = site.principals[0].URI.host;
    this.fakeSites = this.fakeSites.filter(fakeSite => {
      return fakeSite.principal.URI.host != target;
    });
  },

  register(siteDataManager, fakeSites) {
    this._SiteDataManager = siteDataManager;
    this._originalQMS = this._SiteDataManager._qms;
    this._SiteDataManager._qms = this;
    this._originalRemoveQuotaUsage = this._SiteDataManager._removeQuotaUsage;
    this._SiteDataManager._removeQuotaUsage = this._removeQuotaUsage.bind(this);
    // Add some fake data.
    this.fakeSites = fakeSites;
    for (let site of fakeSites) {
      if (!site.principal) {
        site.principal = Services.scriptSecurityManager
          .createCodebasePrincipalFromOrigin(site.origin);
      }

      let uri = site.principal.URI;
      try {
        site.baseDomain = Services.eTLD.getBaseDomainFromHost(uri.host);
      } catch (e) {
        site.baseDomain = uri.host;
      }

      // Add some cookies if needed.
      for (let i = 0; i < (site.cookies || 0); i++) {
        Services.cookies.add(uri.host, uri.pathQueryRef, Cu.now(), i,
          false, false, false, Date.now() + 1000 * 60 * 60);
      }
    }
  },

  async unregister() {
    await this._SiteDataManager.removeAll();
    this.fakeSites = null;
    this._SiteDataManager._qms = this._originalQMS;
    this._SiteDataManager._removeQuotaUsage = this._originalRemoveQuotaUsage;
  }
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
  BrowserTestUtils.removeTab(tab);
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

