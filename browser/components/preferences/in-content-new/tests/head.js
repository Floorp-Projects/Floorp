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
  }, {capture: true, once: true});
}

function openAndLoadSubDialog(aURL, aFeatures = null, aParams = null, aClosingCallback = null) {
  let promise = promiseLoadSubDialog(aURL);
  content.gSubDialog.open(aURL, aFeatures, aParams, aClosingCallback);
  return promise;
}

function promiseLoadSubDialog(aURL) {
  return new Promise((resolve, reject) => {
    content.gSubDialog._frame.addEventListener("load", function load(aEvent) {
      if (aEvent.target.contentWindow.location == "about:blank")
        return;
      content.gSubDialog._frame.removeEventListener("load", load);

      is(content.gSubDialog._frame.contentWindow.location.toString(), aURL,
         "Check the proper URL is loaded");

      // Check visibility
      is_element_visible(content.gSubDialog._overlay, "Overlay is visible");

      // Check that stylesheets were injected
      let expectedStyleSheetURLs = content.gSubDialog._injectedStyleSheets.slice(0);
      for (let styleSheet of content.gSubDialog._frame.contentDocument.styleSheets) {
        let i = expectedStyleSheetURLs.indexOf(styleSheet.href);
        if (i >= 0) {
          info("found " + styleSheet.href);
          expectedStyleSheetURLs.splice(i, 1);
        }
      }
      is(expectedStyleSheetURLs.length, 0, "All expectedStyleSheetURLs should have been found");

      resolve(content.gSubDialog._frame.contentWindow);
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

function openPreferencesViaOpenPreferencesAPI(aPane, aOptions) {
  return new Promise(resolve => {
    gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
    openPreferences(aPane);
    let newTabBrowser = gBrowser.selectedBrowser;

    newTabBrowser.addEventListener("Initialized", function() {
      newTabBrowser.contentWindow.addEventListener("load", function() {
        let win = gBrowser.contentWindow;
        let selectedPane = win.history.state;
        if (!aOptions || !aOptions.leaveOpen)
          gBrowser.removeCurrentTab();
        resolve({selectedPane});
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
  let dialogOverlay = doc.getElementById("dialogOverlay");
  let dialogLoadPromise = promiseLoadSubDialog("chrome://browser/content/preferences/siteDataSettings.xul");
  let dialogInitPromise = TestUtils.topicObserved("sitedata-settings-init", () => true);
  let fullyLoadPromise = Promise.all([ dialogLoadPromise, dialogInitPromise ]).then(() => {
    is(dialogOverlay.style.visibility, "visible", "The Settings dialog should be visible");
  });
  settingsBtn.doCommand();
  return fullyLoadPromise;
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

function evaluateSearchResults(keyword, searchReults) {
  searchReults = Array.isArray(searchReults) ? searchReults : [searchReults];
  searchReults.push("header-searchResults");

  let searchInput = gBrowser.contentDocument.getElementById("searchInput");
  searchInput.focus();
  searchInput.value = keyword;
  searchInput.doCommand();

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
    this._SiteDataManager._getQuotaUsagePromise = Promise.resolve(results);
    return this._SiteDataManager._getQuotaUsagePromise;
  },

  _removeQuotaUsage(site) {
    var target = site.principals[0].URI.host;
    this.fakeSites = this.fakeSites.filter(fakeSite => {
      return fakeSite.principal.URI.host != target;
    });
  },

  register(SiteDataManager) {
    this._SiteDataManager = SiteDataManager;
    this._originalGetQuotaUsage = this._SiteDataManager._getQuotaUsage;
    this._SiteDataManager._getQuotaUsage = this._getQuotaUsage.bind(this);
    this._originalRemoveQuotaUsage = this._SiteDataManager._removeQuotaUsage;
    this._SiteDataManager._removeQuotaUsage = this._removeQuotaUsage.bind(this);
    this.fakeSites = null;
  },

  unregister() {
    this._SiteDataManager._getQuotaUsage = this._originalGetQuotaUsage;
    this._SiteDataManager._removeQuotaUsage = this._originalRemoveQuotaUsage;
  }
};
