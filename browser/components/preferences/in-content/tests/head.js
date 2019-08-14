/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.import("resource://gre/modules/Promise.jsm", this);

const kDefaultWait = 2000;

function is_element_visible(aElement, aMsg) {
  isnot(aElement, null, "Element should not be null, when checking visibility");
  ok(!BrowserTestUtils.is_hidden(aElement), aMsg);
}

function is_element_hidden(aElement, aMsg) {
  isnot(aElement, null, "Element should not be null, when checking visibility");
  ok(BrowserTestUtils.is_hidden(aElement), aMsg);
}

function open_preferences(aCallback) {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:preferences");
  let newTabBrowser = gBrowser.getBrowserForTab(gBrowser.selectedTab);
  newTabBrowser.addEventListener(
    "Initialized",
    function() {
      aCallback(gBrowser.contentWindow);
    },
    { capture: true, once: true }
  );
}

function openAndLoadSubDialog(
  aURL,
  aFeatures = null,
  aParams = null,
  aClosingCallback = null
) {
  let promise = promiseLoadSubDialog(aURL);
  content.gSubDialog.open(aURL, aFeatures, aParams, aClosingCallback);
  return promise;
}

function promiseLoadSubDialog(aURL) {
  return new Promise((resolve, reject) => {
    content.gSubDialog._dialogStack.addEventListener(
      "dialogopen",
      function dialogopen(aEvent) {
        if (
          aEvent.detail.dialog._frame.contentWindow.location == "about:blank"
        ) {
          return;
        }
        content.gSubDialog._dialogStack.removeEventListener(
          "dialogopen",
          dialogopen
        );

        is(
          aEvent.detail.dialog._frame.contentWindow.location.toString(),
          aURL,
          "Check the proper URL is loaded"
        );

        // Check visibility
        is_element_visible(aEvent.detail.dialog._overlay, "Overlay is visible");

        // Check that stylesheets were injected
        let expectedStyleSheetURLs = aEvent.detail.dialog._injectedStyleSheets.slice(
          0
        );
        for (let styleSheet of aEvent.detail.dialog._frame.contentDocument
          .styleSheets) {
          let i = expectedStyleSheetURLs.indexOf(styleSheet.href);
          if (i >= 0) {
            info("found " + styleSheet.href);
            expectedStyleSheetURLs.splice(i, 1);
          }
        }
        is(
          expectedStyleSheetURLs.length,
          0,
          "All expectedStyleSheetURLs should have been found"
        );

        // Wait for the next event tick to make sure the remaining part of the
        // testcase runs after the dialog gets ready for input.
        executeSoon(() => resolve(aEvent.detail.dialog._frame.contentWindow));
      }
    );
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
    if (aTarget && aTarget !== aEvent.target) {
      return;
    }

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
    let finalPaneEvent = Services.prefs.getBoolPref(
      "identity.fxaccounts.enabled"
    )
      ? "sync-pane-loaded"
      : "privacy-pane-loaded";
    let finalPrefPaneLoaded = TestUtils.topicObserved(
      finalPaneEvent,
      () => true
    );
    gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
    openPreferences(aPane);
    let newTabBrowser = gBrowser.selectedBrowser;

    newTabBrowser.addEventListener(
      "Initialized",
      function() {
        newTabBrowser.contentWindow.addEventListener(
          "load",
          async function() {
            let win = gBrowser.contentWindow;
            let selectedPane = win.history.state;
            await finalPrefPaneLoaded;
            if (!aOptions || !aOptions.leaveOpen) {
              gBrowser.removeCurrentTab();
            }
            resolve({ selectedPane });
          },
          { once: true }
        );
      },
      { capture: true, once: true }
    );
  });
}

async function evaluateSearchResults(keyword, searchReults) {
  searchReults = Array.isArray(searchReults) ? searchReults : [searchReults];
  searchReults.push("header-searchResults");

  let searchInput = gBrowser.contentDocument.getElementById("searchInput");
  searchInput.focus();
  let searchCompletedPromise = BrowserTestUtils.waitForEvent(
    gBrowser.contentWindow,
    "PreferencesSearchCompleted",
    evt => evt.detail == keyword
  );
  EventUtils.sendString(keyword);
  await searchCompletedPromise;

  let mainPrefTag = gBrowser.contentDocument.getElementById("mainPrefPane");
  for (let i = 0; i < mainPrefTag.childElementCount; i++) {
    let child = mainPrefTag.children[i];
    if (searchReults.includes(child.id)) {
      is_element_visible(child, `${child.id} should be in search results`);
    } else if (child.id) {
      is_element_hidden(child, `${child.id} should not be in search results`);
    }
  }
}

function waitForMutation(target, opts, cb) {
  return new Promise(resolve => {
    let observer = new MutationObserver(() => {
      if (!cb || cb(target)) {
        observer.disconnect();
        resolve();
      }
    });
    observer.observe(target, opts);
  });
}
