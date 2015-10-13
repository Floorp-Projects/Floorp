/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
  We have three ways for content to open new windows:
     1) window.open (with the default features)
     2) window.open (with non-default features)
     3) target="_blank" in <a> tags

     We also have two prefs that modify our window opening behaviours:

     1) browser.link.open_newwindow

        This has a numeric value that allows us to set our window-opening behaviours from
        content in three ways:
        1) Open links that would normally open a new window in the current tab
        2) Open links that would normally open a new window in a new window
        3) Open links that would normally open a new window in a new tab (default)

     2) browser.link.open_newwindow.restriction

        This has a numeric value that allows us to fine tune the browser.link.open_newwindow
        pref so that it can discriminate between different techniques for opening windows.

        0) All things that open windows should behave according to browser.link.open_newwindow.
        1) No things that open windows should behave according to browser.link.open_newwindow
           (essentially rendering browser.link.open_newwindow inert).
        2) Most things that open windows should behave according to browser.link.open_newwindow,
           _except_ for window.open calls with the "feature" parameter. This will open in a new
           window regardless of what browser.link.open_newwindow is set at. (default)

     This file attempts to test each window opening technique against all possible settings for
     each preference.
*/

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const kXULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const kContentDoc = "http://www.example.com/browser/dom/tests/browser/test_new_window_from_content_child.html";
const kContentScript = "chrome://mochitests/content/browser/dom/tests/browser/test_new_window_from_content_child.js";
const kNewWindowPrefKey = "browser.link.open_newwindow";
const kNewWindowRestrictionPrefKey = "browser.link.open_newwindow.restriction";
const kSameTab = "same tab";
const kNewWin = "new window";
const kNewTab = "new tab";

SpecialPowers.pushPrefEnv({"set": [["dom.require_user_interaction_for_beforeunload", false]]});

requestLongerTimeout(2);

// The following "matrices" represent the result of content attempting to
// open a window with window.open with the default feature set. The key of
// the kWinOpenDefault object represents the value of browser.link.open_newwindow.
// The value for each key is an array that represents the result (either opening
// the link in the same tab, a new window, or a new tab), where the index of each
// result maps to the browser.link.open_newwindow.restriction pref. I've tried
// to illustrate this more clearly in the kWinOpenDefault object.
const kWinOpenDefault = {
//                    open_newwindow.restriction
//                    0         1        2
// open_newwindow
                  1: [kSameTab, kNewWin, kSameTab],
                  2: [kNewWin, kNewWin, kNewWin],
                  3: [kNewTab, kNewWin, kNewTab],
};

const kWinOpenNonDefault = {
  1: [kSameTab, kNewWin, kNewWin],
  2: [kNewWin, kNewWin, kNewWin],
  3: [kNewTab, kNewWin, kNewWin],
};

const kTargetBlank = {
  1: [kSameTab, kSameTab, kSameTab],
  2: [kNewWin, kNewWin, kNewWin],
  3: [kNewTab, kNewTab, kNewTab],
};

// We'll be changing these preferences a lot, so we'll stash their original
// values and make sure we restore them at the end of the test.
var originalNewWindowPref = Services.prefs.getIntPref(kNewWindowPrefKey);
var originalNewWindowRestrictionPref =
  Services.prefs.getIntPref(kNewWindowRestrictionPrefKey);

registerCleanupFunction(function() {
  Services.prefs.setIntPref(kNewWindowPrefKey, originalNewWindowPref);
  Services.prefs.setIntPref(kNewWindowRestrictionPrefKey,
                            originalNewWindowRestrictionPref);
  // If there are any content tabs leftover, make sure they're not going to
  // block exiting with onbeforeunload.
  for (let tab of gBrowser.tabs) {
    let browser = gBrowser.getBrowserForTab(tab);
    if (browser.contentDocument.location == kContentDoc) {
      closeTab(tab);
    }
  }
});

/**
 * WindowOpenListener is a very simple nsIWindowMediatorListener that
 * listens for a new window opening to aExpectedURI. It has two Promises
 * attached to it - openPromise and closePromise. As you'd expect,
 * openPromise resolves when the window is opened, and closePromise
 * resolves if and when a window with the same URI closes. There is
 * no attempt to make sure that it's the same window opening and
 * closing - we just use the URI.
 *
 * @param aExpectedURI the URI to watch for in a new window.
 * @return nsIWindowMediatorListener
 */
function WindowOpenListener(aExpectedURI) {
  this._openDeferred = Promise.defer();
  this._closeDeferred = Promise.defer();
  this._expectedURI = aExpectedURI;
}

WindowOpenListener.prototype = {
  get openPromise() {
    return this._openDeferred.promise;
  },

  get closePromise() {
    return this._closeDeferred.promise;
  },

  onOpenWindow: function(aXULWindow) {
    let domWindow = aXULWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIDOMWindow);
    let location = domWindow.document.location;
    if (location == this._expectedURI) {
      let deferred = this._openDeferred;
      domWindow.addEventListener("load", function onWindowLoad() {
        domWindow.removeEventListener("load", onWindowLoad);
        deferred.resolve(domWindow);
      })
    }
  },

  onCloseWindow: function(aXULWindow) {
    this._closeDeferred.resolve();
  },
  onWindowTitleChange: function(aXULWindow, aNewTitle) {},

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWindowMediatorListener])
};

/**
 * Adds the testing tab, and injects our frame script which
 * allows us to send click events to links and other things.
 *
 * @return a Promise that resolves once the tab is loaded and ready.
 */
function loadAndSelectTestTab() {
  let tab = gBrowser.addTab(kContentDoc);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  browser.messageManager.loadFrameScript(kContentScript, false);

  let deferred = Promise.defer();
  browser.addEventListener("DOMContentLoaded", function onBrowserLoad(aEvent) {
    browser.removeEventListener("DOMContentLoaded", onBrowserLoad);
    deferred.resolve(tab);
  });

  return deferred.promise;
}

/**
 * Clears the onbeforeunload event handler from the testing tab,
 * and then closes the tab.
 *
 * @param aTab the testing tab to close.
 * @param a Promise that resolves once the tab has been closed.
 */
function closeTab(aTab) {
  let deferred = Promise.defer();
  let browserMM = gBrowser.getBrowserForTab(aTab).messageManager;
  browserMM.sendAsyncMessage("TEST:allow-unload");
  browserMM.addMessageListener("TEST:allow-unload:done", function(aMessage) {
    gBrowser.removeTab(aTab);
    deferred.resolve();
  })
  return deferred.promise;
}

/**
 * Sends a click event on some item into a tab.
 *
 * @param aTab the tab to send the click event to
 * @param aItemId the item within the tab content to click.
 */
function clickInTab(aTab, aItemId) {
  let browserMM = gBrowser.getBrowserForTab(aTab).messageManager;
  browserMM.sendAsyncMessage("TEST:click-item", {
    details: aItemId,
  });
}

/**
 * For some expectation when a link is clicked, creates and
 * returns a Promise that resolves when that expectation is
 * fulfilled. For example, aExpectation might be kSameTab, which
 * will cause this function to return a Promise that resolves when
 * the current tab attempts to browse to about:blank.
 *
 * This function also takes care of cleaning up once the result has
 * occurred - for example, if a new window was opened, this function
 * closes it before resolving.
 *
 * @param aTab the tab with the test document
 * @param aExpectation one of kSameTab, kNewWin, or kNewTab.
 * @return a Promise that resolves when the expectation is fulfilled,
 *         and cleaned up after.
 */
function prepareForResult(aTab, aExpectation) {
  let deferred = Promise.defer();
  let browser = gBrowser.getBrowserForTab(aTab);

  switch(aExpectation) {
    case kSameTab:
      // We expect about:blank to be loaded in the current tab. In order
      // to prevent us needing to reload the document and content script
      // after browsing away, we'll detect the attempt by using onbeforeunload,
      // and then cancel the unload. It's a hack, but it's also a pretty
      // cheap way of detecting when we're browsing away in the test tab.
      // The onbeforeunload event handler is set in the content script automatically.
      browser.addEventListener("DOMWillOpenModalDialog", function onModalDialog() {
        browser.removeEventListener("DOMWillOpenModalDialog", onModalDialog, true);
        executeSoon(() => {
          let stack = browser.parentNode;
          let dialogs = stack.getElementsByTagNameNS(kXULNS, "tabmodalprompt");
          dialogs[0].ui.button1.click()
          deferred.resolve();
        })
      }, true);
      break;
    case kNewWin:
      let listener = new WindowOpenListener("about:blank");
      Services.wm.addListener(listener);

      info("Waiting for a new about:blank window");
      listener.openPromise.then(function(aWindow) {
        info("Got the new about:blank window - closing it.");
        executeSoon(() => {
          aWindow.close();
        });
        listener.closePromise.then(() => {
          info("New about:blank window closed!");
          Services.wm.removeListener(listener);
          deferred.resolve();
        });
      });
      break;
    case kNewTab:
      gBrowser.tabContainer.addEventListener("TabOpen", function onTabOpen(aEvent) {
        let newTab = aEvent.target;
        let newBrowser = gBrowser.getBrowserForTab(newTab);
        if (newBrowser.contentDocument.location.href == "about:blank") {
          gBrowser.tabContainer.removeEventListener("TabOpen", onTabOpen);
          executeSoon(() => {
            gBrowser.removeTab(newTab);
            deferred.resolve();
          })
        }
      })
      break;
    default:
      ok(false, "prepareForResult can't handle an expectation of " + aExpectation)
      return;
  }

  return deferred.promise;
}

/**
 * Ensure that clicks on a link with ID aLinkID cause us to
 * perform as specified in the supplied aMatrix (kWinOpenDefault,
 * for example).
 *
 * @param aLinkId the id of the link within the testing page to test.
 * @param aMatrix a testing matrix for the
 *        browser.link.open_newwindow and browser.link.open_newwindow.restriction
 *        prefs to test against. See kWinOpenDefault for an example.
 */
function testLinkWithMatrix(aLinkId, aMatrix) {
  return Task.spawn(function* () {
    let tab = yield loadAndSelectTestTab();
    // This nested for-loop is unravelling the matrix const
    // we set up, and gives us three things through each tick
    // of the inner loop:
    // 1) newWindowPref: a browser.link.open_newwindow pref to try
    // 2) newWindowRestPref: a browser.link.open_newwindow.restriction pref to try
    // 3) expectation: what we expect the click outcome on this link to be,
    //                 which will either be kSameTab, kNewWin or kNewTab.

    for (let newWindowPref in aMatrix) {
      let expectations = aMatrix[newWindowPref];
      for (let i = 0; i < expectations.length; ++i) {
        let newWindowRestPref = i;
        let expectation = expectations[i];

        Services.prefs.setIntPref("browser.link.open_newwindow", newWindowPref);
        Services.prefs.setIntPref("browser.link.open_newwindow.restriction", newWindowRestPref);
        info("Clicking on " + aLinkId);
        info("Testing with browser.link.open_newwindow = " + newWindowPref + " and " +
             "browser.link.open_newwindow.restriction = " + newWindowRestPref);
        info("Expecting: " + expectation);
        let resultPromise = prepareForResult(tab, expectation);
        clickInTab(tab, aLinkId);
        yield resultPromise;
        ok(true, "Got expectation: " + expectation);
      }
    }
    yield closeTab(tab);
  });
}

add_task(function* test_window_open_with_defaults() {
  yield testLinkWithMatrix("winOpenDefault", kWinOpenDefault);
});

add_task(function* test_window_open_with_non_defaults() {
  yield testLinkWithMatrix("winOpenNonDefault", kWinOpenNonDefault);
});

add_task(function* test_target__blank() {
  yield testLinkWithMatrix("targetBlank", kTargetBlank);
});
