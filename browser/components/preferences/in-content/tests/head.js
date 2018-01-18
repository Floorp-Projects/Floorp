/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/Promise.jsm");

const kDefaultWait = 2000;

function is_hidden(aElement) {
  var style = aElement.ownerGlobal.getComputedStyle(aElement);
  if (style.display == "none")
    return true;
  if (style.visibility != "visible")
    return true;

  // Hiding a parent element will hide all its children
  if (aElement.parentNode != aElement.ownerDocument)
    return is_hidden(aElement.parentNode);

  return false;
}

function is_element_visible(aElement, aMsg) {
  isnot(aElement, null, "Element should not be null, when checking visibility");
  ok(!is_hidden(aElement), aMsg);
}

function is_element_hidden(aElement, aMsg) {
  isnot(aElement, null, "Element should not be null, when checking visibility");
  ok(is_hidden(aElement), aMsg);
}

function open_preferences(aCallback) {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:preferences");
  let newTabBrowser = gBrowser.getBrowserForTab(gBrowser.selectedTab);
  newTabBrowser.addEventListener("Initialized", function() {
    aCallback(gBrowser.contentWindow);
  }, { capture: true, once: true });
}

function openAndLoadSubDialog(aURL, aFeatures = null, aParams = null, aClosingCallback = null) {
  let promise = promiseLoadSubDialog(aURL);
  content.gSubDialog.open(aURL, aFeatures, aParams, aClosingCallback);
  return promise;
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

function waitForCondition(aConditionFn, aMaxTries = 50, aCheckInterval = 100) {
  return new Promise((resolve, reject) => {
    function tryNow() {
      tries++;
      let rv = aConditionFn();
      if (rv) {
        resolve(rv);
      } else if (tries < aMaxTries) {
        tryAgain();
      } else {
        reject("Condition timed out: " + aConditionFn.toSource());
      }
    }
    function tryAgain() {
      setTimeout(tryNow, aCheckInterval);
    }
    let tries = 0;
    tryAgain();
  });
}

function promiseWindowDialogOpen(buttonAction, url) {
  return new Promise(resolve => {
    Services.ww.registerNotification(function onOpen(subj, topic, data) {
      if (topic == "domwindowopened" && subj instanceof Ci.nsIDOMWindow) {
        subj.addEventListener("load", function onLoad() {
          if (subj.document.documentURI == url) {
            Services.ww.unregisterNotification(onOpen);
            let doc = subj.document.documentElement;
            doc.getButton(buttonAction).click();
            resolve();
          }
        }, { once: true });
      }
    });
  });
}

function promiseAlertDialogOpen(buttonAction) {
  return promiseWindowDialogOpen(buttonAction, "chrome://global/content/commonDialog.xul");
}

function addPersistentStoragePerm(origin) {
  let uri = NetUtil.newURI(origin);
  let principal = Services.scriptSecurityManager.createCodebasePrincipal(uri, {});
  Services.perms.addFromPrincipal(principal, "persistent-storage", Ci.nsIPermissionManager.ALLOW_ACTION);
}

function promiseSiteDataManagerSitesUpdated() {
  return TestUtils.topicObserved("sitedatamanager:sites-updated", () => true);
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

function assertSitesListed(doc, hosts) {
  let frameDoc = content.gSubDialog._topDialog._frame.contentDocument;
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

async function evaluateSearchResults(keyword, searchReults) {
  searchReults = Array.isArray(searchReults) ? searchReults : [searchReults];
  searchReults.push("header-searchResults");

  let searchInput = gBrowser.contentDocument.getElementById("searchInput");
  searchInput.focus();
  let searchCompletedPromise = BrowserTestUtils.waitForEvent(
    gBrowser.contentWindow, "PreferencesSearchCompleted", evt => evt.detail == keyword);
  EventUtils.sendString(keyword);
  await searchCompletedPromise;

  let mainPrefTag = gBrowser.contentDocument.getElementById("mainPrefPane");
  for (let i = 0; i < mainPrefTag.childElementCount; i++) {
    let child = mainPrefTag.children[i];
    if (searchReults.includes(child.id)) {
      is_element_visible(child, "Should be in search results");
    } else if (child.id) {
      is_element_hidden(child, "Should not be in search results");
    }
  }
}

const mockSiteDataManager = {

  _SiteDataManager: null,
  _originalQMS: null,
  _originalRemoveQuotaUsage: null,

  getUsage(onUsageResult) {
    let result = this.fakeSites.map(site => ({
      origin: site.principal.origin,
      usage: site.usage,
      persisted: site.persisted
    }));
    onUsageResult({ result, resultCode: Components.results.NS_OK });
  },

  _removeQuotaUsage(site) {
    var target = site.principals[0].URI.host;
    this.fakeSites = this.fakeSites.filter(fakeSite => {
      return fakeSite.principal.URI.host != target;
    });
  },

  register(SiteDataManager) {
    this._SiteDataManager = SiteDataManager;
    this._originalQMS = this._SiteDataManager._qms;
    this._SiteDataManager._qms = this;
    this._originalRemoveQuotaUsage = this._SiteDataManager._removeQuotaUsage;
    this._SiteDataManager._removeQuotaUsage = this._removeQuotaUsage.bind(this);
    this.fakeSites = null;
  },

  unregister() {
    this._SiteDataManager._qms = this._originalQMS;
    this._SiteDataManager._removeQuotaUsage = this._originalRemoveQuotaUsage;
  }
};
