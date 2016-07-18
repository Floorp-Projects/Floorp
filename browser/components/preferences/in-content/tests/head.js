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
  gBrowser.selectedTab = gBrowser.addTab("about:preferences");
  let newTabBrowser = gBrowser.getBrowserForTab(gBrowser.selectedTab);
  newTabBrowser.addEventListener("Initialized", function () {
    newTabBrowser.removeEventListener("Initialized", arguments.callee, true);
    aCallback(gBrowser.contentWindow);
  }, true);
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

  var listener = function (aEvent) {
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
  aSubject.addEventListener(aEventName, listener, false);
  return eventDeferred.promise.then(cleanup, cleanup);
}

function openPreferencesViaOpenPreferencesAPI(aPane, aAdvancedTab, aOptions) {
  let deferred = Promise.defer();
  gBrowser.selectedTab = gBrowser.addTab("about:blank");
  openPreferences(aPane, aAdvancedTab ? {advancedTab: aAdvancedTab} : undefined);
  let newTabBrowser = gBrowser.selectedBrowser;

  newTabBrowser.addEventListener("Initialized", function PrefInit() {
    newTabBrowser.removeEventListener("Initialized", PrefInit, true);
    newTabBrowser.contentWindow.addEventListener("load", function prefLoad() {
      newTabBrowser.contentWindow.removeEventListener("load", prefLoad);
      let win = gBrowser.contentWindow;
      let selectedPane = win.history.state;
      let doc = win.document;
      let selectedAdvancedTab = aAdvancedTab && doc.getElementById("advancedPrefs").selectedTab.id;
      if (!aOptions || !aOptions.leaveOpen)
        gBrowser.removeCurrentTab();
      deferred.resolve({selectedPane: selectedPane, selectedAdvancedTab: selectedAdvancedTab});
    });
  }, true);

  return deferred.promise;
}

function waitForCondition(aConditionFn, aMaxTries=50, aCheckInterval=100) {
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
