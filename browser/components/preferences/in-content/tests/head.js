/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/Promise.jsm");

// Tests within /browser/components/preferences/in-content/tests/
// test the "old" preferences organization, before it was reorganized.
// Thus, all of these tests should revert back to the "oldOrganization"
// before running.
Services.prefs.setBoolPref("browser.preferences.useOldOrganization", true);
registerCleanupFunction(function() {
  Services.prefs.clearUserPref("browser.preferences.useOldOrganization");
});

const kDefaultWait = 2000;
const REMOVE_DIALOG_URL = "chrome://browser/content/preferences/siteDataRemoveSelected.xul";
const { SiteDataManager } = Cu.import("resource:///modules/SiteDataManager.jsm", {});
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
  }, {capture: true, once: true});
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

      resolve(aEvent.detail.dialog._frame.contentWindow);
    });
  });
}

/**
 * Waits a specified number of miliseconds for a specified event to be
 * fired on a specified element.
 *
 * Usage:
 *    let receivedEvent = waitForEvent(element, "eventName");
 *    // Do some processing here that will cause the event to be fired
 *    // ...
 *    // Now yield until the Promise is fulfilled
 *    yield receivedEvent;
 *    if (receivedEvent && !(receivedEvent instanceof Error)) {
 *      receivedEvent.msg == "eventName";
 *      // ...
 *    }
 *
 * @param aSubject the element that should receive the event
 * @param aEventName the event to wait for
 * @param aTimeoutMs the number of miliseconds to wait before giving up
 * @returns a Promise that resolves to the received event, or to an Error
 */
function waitForEvent(aSubject, aEventName, aTimeoutMs, aTarget) {
  let eventDeferred = Promise.defer();
  let timeoutMs = aTimeoutMs || kDefaultWait;
  let stack = new Error().stack;
  let timerID = setTimeout(function wfe_canceller() {
    aSubject.removeEventListener(aEventName, listener);
    eventDeferred.reject(new Error(aEventName + " event timeout at " + stack));
  }, timeoutMs);

  var listener = function(aEvent) {
    if (aTarget && aTarget !== aEvent.target)
        return;

    // stop the timeout clock and resume
    clearTimeout(timerID);
    eventDeferred.resolve(aEvent);
  };

  function cleanup(aEventOrError) {
    // unhook listener in case of success or failure
    aSubject.removeEventListener(aEventName, listener);
    return aEventOrError;
  }
  aSubject.addEventListener(aEventName, listener);
  return eventDeferred.promise.then(cleanup, cleanup);
}

function openPreferencesViaOpenPreferencesAPI(aPane, aAdvancedTab, aOptions) {
  return new Promise(resolve => {
    gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
    openPreferences(aPane, aAdvancedTab ? {advancedTab: aAdvancedTab} : undefined);
    let newTabBrowser = gBrowser.selectedBrowser;

    newTabBrowser.addEventListener("Initialized", function() {
      newTabBrowser.contentWindow.addEventListener("load", function() {
        let win = gBrowser.contentWindow;
        let selectedPane = win.history.state;
        let doc = win.document;
        let selectedAdvancedTab = aAdvancedTab && doc.getElementById("advancedPrefs").selectedTab.id;
        if (!aOptions || !aOptions.leaveOpen)
          gBrowser.removeCurrentTab();
        resolve({selectedPane, selectedAdvancedTab});
      }, {once: true});
    }, {capture: true, once: true});

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
        }, {once: true});
      }
    });
  });
}

function promiseAlertDialogOpen(buttonAction) {
  return promiseWindowDialogOpen(buttonAction, "chrome://global/content/commonDialog.xul");
}

function openSettingsDialog() {
  let win = gBrowser.selectedBrowser.contentWindow;
  let doc = gBrowser.selectedBrowser.contentDocument;
  let settingsBtn = doc.getElementById("siteDataSettings");
  let dialogOverlay = win.gSubDialog._preloadDialog._overlay;
  let dialogLoadPromise = promiseLoadSubDialog("chrome://browser/content/preferences/siteDataSettings.xul");
  let dialogInitPromise = TestUtils.topicObserved("sitedata-settings-init", () => true);
  let fullyLoadPromise = Promise.all([ dialogLoadPromise, dialogInitPromise ]).then(() => {
    is(dialogOverlay.style.visibility, "visible", "The Settings dialog should be visible");
  });
  settingsBtn.doCommand();
  return fullyLoadPromise;
}

function assertSitesListed(doc, hosts) {
  let win = gBrowser.selectedBrowser.contentWindow;
  let frameDoc = win.gSubDialog._topDialog._frame.contentDocument;
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

function promiseSitesUpdated() {
  return TestUtils.topicObserved("sitedatamanager:sites-updated", () => true);
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
