Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "BrowserTestUtils",
  "resource://testing-common/BrowserTestUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ContentTask",
  "resource://testing-common/ContentTask.jsm");

const REFERRER_URL_BASE = "/browser/browser/base/content/test/referrer/";
const REFERRER_POLICYSERVER_URL =
  "test1.example.com" + REFERRER_URL_BASE + "file_referrer_policyserver.sjs";

let gTestWindow = null;

// We test that the UI code propagates three pieces of state - the referrer URI
// itself, the referrer policy, and the triggering principal. After that, we
// trust nsIWebNavigation to do the right thing with the info it's given, which
// is covered more exhaustively by dom/base/test/test_bug704320.html (which is
// a faster content-only test). So, here, we limit ourselves to cases that
// would break when the UI code drops either of these pieces; we don't try to
// duplicate the entire cross-product test in bug 704320 - that would be slow,
// especially when we're opening a new window for each case.
let _referrerTests = [
  // 1. Normal cases - no referrer policy, no special attributes.
  //    We expect a full referrer normally, and no referrer on downgrade.
  {
    fromScheme: "http://",
    toScheme: "http://",
    result: "http://test1.example.com/browser"  // full referrer
  },
  {
    fromScheme: "https://",
    toScheme: "http://",
    result: ""  // no referrer when downgrade
  },
  // 2. Origin referrer policy - we expect an origin referrer,
  //    even on downgrade.  But rel=noreferrer trumps this.
  {
    fromScheme: "https://",
    toScheme: "http://",
    policy: "origin",
    result: "https://test1.example.com"  // origin, even on downgrade
  },
  {
    fromScheme: "https://",
    toScheme: "http://",
    policy: "origin",
    rel: "noreferrer",
    result: ""  // rel=noreferrer trumps meta-referrer
  },
  // 3. Origin-when-cross-origin policy - this depends on the triggering
  //    principal.  We expect full referrer for same-origin requests,
  //    and origin referrer for cross-origin requests.
  {
    fromScheme: "https://",
    toScheme: "https://",
    policy: "origin-when-cross-origin",
    result: "https://test1.example.com/browser"  // same origin
  },
  {
    fromScheme: "http://",
    toScheme: "https://",
    policy: "origin-when-cross-origin",
    result: "http://test1.example.com"  // cross origin
  },
];

/**
 * Returns the test object for a given test number.
 * @param aTestNumber The test number - 0, 1, 2, ...
 * @return The test object, or undefined if the number is out of range.
 */
function getReferrerTest(aTestNumber) {
  return _referrerTests[aTestNumber];
}

/**
 * Returns a brief summary of the test, for logging.
 * @param aTestNumber The test number - 0, 1, 2...
 * @return The test description.
 */
function getReferrerTestDescription(aTestNumber) {
  let test = getReferrerTest(aTestNumber);
  return "policy=[" + test.policy + "] " +
         "rel=[" + test.rel + "] " +
         test.fromScheme + " -> " + test.toScheme;
}

/**
 * Clicks the link.
 * @param aWindow The window to click the link in.
 * @param aLinkId The id of the link element.
 * @param aOptions The options for synthesizeMouseAtCenter.
 */
function clickTheLink(aWindow, aLinkId, aOptions) {
  ContentTask.spawn(aWindow.gBrowser.selectedBrowser,
                    {id: aLinkId, options: aOptions},
                    function(data) {
    let element = content.document.getElementById(data.id);
    let options = data.options;

    // EventUtils.synthesizeMouseAtCenter(element, options, content);
    // Alas, EventUtils doesn't work in the content task environment.
    function doClick() {
      var domWindowUtils =
          content.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
          .getInterface(Components.interfaces.nsIDOMWindowUtils);
      var rect = element.getBoundingClientRect();
      var left = rect.left + rect.width / 2;
      var top = rect.top + rect.height / 2;
      var button = options.button || 0;
      function sendMouseEvent(type) {
        domWindowUtils.sendMouseEvent(type, left, top, button,
                                      1, 0, false, 0, 0, true);
      }
      if ("type" in options) {
        sendMouseEvent(options.type);  // e.g., "contextmenu"
      } else {
        sendMouseEvent("mousedown");
        sendMouseEvent("mouseup");
      }
    }

    // waitForFocus(doClick, content);
    let focusManager = Components.classes["@mozilla.org/focus-manager;1"].
                       getService(Components.interfaces.nsIFocusManager);
    let desiredWindow = {};
    focusManager.getFocusedElementForWindow(content, true, desiredWindow);
    desiredWindow = desiredWindow.value;
    if (desiredWindow == focusManager.focusedWindow) {
      // The window is already focused - click away.
      doClick();
    } else {
      // Focus the window first, then click.
      desiredWindow.addEventListener("focus", function onFocus() {
        desiredWindow.removeEventListener("focus", onFocus, true);
        setTimeout(doClick, 0);
      }, true);
      desiredWindow.focus();
    }
  });
}

/**
 * Extracts the referrer result from the target window.
 * @param aWindow The window where the referrer target has loaded.
 * @return {Promise}
 * @resolves When extacted, with the text of the (trimmed) referrer.
 */
function referrerResultExtracted(aWindow) {
  return ContentTask.spawn(aWindow.gBrowser.selectedBrowser, {}, function() {
    return content.document.getElementById("testdiv").textContent;
  });
}

/**
 * Waits for browser delayed startup to finish.
 * @param aWindow The window to wait for.
 * @return {Promise}
 * @resolves When the window is loaded.
 */
function delayedStartupFinished(aWindow) {
  return new Promise(function(resolve) {
    Services.obs.addObserver(function observer(aSubject, aTopic) {
      if (aWindow == aSubject) {
        Services.obs.removeObserver(observer, aTopic);
        resolve();
      }
    }, "browser-delayed-startup-finished", false);
  });
}

/**
 * Waits for some (any) tab to load. The caller triggers the load.
 * @param aWindow The window where to wait for a tab to load.
 * @return {Promise}
 * @resolves With the tab once it's loaded.
 */
function someTabLoaded(aWindow) {
  return new Promise(function(resolve) {
    aWindow.gBrowser.addEventListener("load", function onLoad(aEvent) {
      let tab = aWindow.gBrowser._getTabForContentWindow(
          aEvent.target.defaultView.top);
      if (tab) {
        aWindow.gBrowser.removeEventListener("load", onLoad, true);
        resolve(tab);
      }
    }, true);
  });
}

/**
 * Waits for a new window to open and load. The caller triggers the open.
 * @return {Promise}
 * @resolves With the new window once it's open and loaded.
 */
function newWindowOpened() {
  return new Promise(function(resolve) {
    Services.wm.addListener({
      onOpenWindow: function(aXULWindow) {
        Services.wm.removeListener(this);
        var newWindow = aXULWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                  .getInterface(Ci.nsIDOMWindow);
        delayedStartupFinished(newWindow).then(function() {
          resolve(newWindow);
        });
      },
      onCloseWindow: function(aXULWindow) { },
      onWindowTitleChange: function(aXULWindow, aNewTitle) { }
    });
  });
}

/**
 * Opens the context menu.
 * @param aWindow The window to open the context menu in.
 * @param aLinkId The id of the link to open the context menu on.
 * @return {Promise}
 * @resolves With the menu popup when the context menu is open.
 */
function contextMenuOpened(aWindow, aLinkId) {
  return new Promise(function(resolve) {
    aWindow.document.addEventListener("popupshown",
                                      function handleMenu(aEvent) {
      aWindow.document.removeEventListener("popupshown", handleMenu, false);
      resolve(aEvent.target);
    }, false);

    // Simulate right-click that opens the context menu.
    clickTheLink(aWindow, aLinkId, {type: "contextmenu", button: 2});
  });
}

/**
 * Performs a context menu command.
 * @param aWindow The window with the already open context menu.
 * @param aMenu The menu popup to hide.
 * @param aItemId The id of the menu item to activate.
 */
function doContextMenuCommand(aWindow, aMenu, aItemId) {
  let command = aWindow.document.getElementById(aItemId);
  command.doCommand();
  aMenu.hidePopup();
}

/**
 * Loads a single test case, i.e., a source url into gTestWindow.
 * @param aTestNumber The test case number - 0, 1, 2...
 * @return {Promise}
 * @resolves When the source url for this test case is loaded.
 */
function referrerTestCaseLoaded(aTestNumber) {
  let test = getReferrerTest(aTestNumber);
  let url = test.fromScheme + REFERRER_POLICYSERVER_URL +
            "?scheme=" + escape(test.toScheme) +
            "&policy=" + escape(test.policy || "") +
            "&rel=" + escape(test.rel || "");
  var browser = gTestWindow.gBrowser;
  browser.selectedTab = browser.addTab(url);
  return BrowserTestUtils.browserLoaded(browser.selectedBrowser);
}

/**
 * Checks the result of the referrer test, and moves on to the next test.
 * @param aTestNumber The test number - 0, 1, 2, ...
 * @param aNewWindow The new window where the referrer target opened, or null.
 * @param aNewTab The new tab where the referrer target opened, or null.
 * @param aStartTestCase The callback to start the next test, aTestNumber + 1.
 */
function checkReferrerAndStartNextTest(aTestNumber, aNewWindow, aNewTab,
                                       aStartTestCase) {
  referrerResultExtracted(aNewWindow || gTestWindow).then(function(result) {
    // Compare the actual result against the expected one.
    let test = getReferrerTest(aTestNumber);
    let desc = getReferrerTestDescription(aTestNumber);
    is(result, test.result, desc);

    // Clean up - close new tab / window, and then the source tab.
    aNewTab && (aNewWindow || gTestWindow).gBrowser.removeTab(aNewTab);
    aNewWindow && aNewWindow.close();
    is(gTestWindow.gBrowser.tabs.length, 2, "two tabs open");
    gTestWindow.gBrowser.removeTab(gTestWindow.gBrowser.tabs[1]);

    // Move on to the next test.  Or finish if we're done.
    var nextTestNumber = aTestNumber + 1;
    if (getReferrerTest(nextTestNumber)) {
      referrerTestCaseLoaded(nextTestNumber).then(function() {
        aStartTestCase(nextTestNumber);
      });
    } else {
      finish();
    }
  });
}

/**
 * Fires up the complete referrer test.
 * @param aStartTestCase The callback to start a single test case, called with
 * the test number - 0, 1, 2... Needs to trigger the navigation from the source
 * page, and call checkReferrerAndStartNextTest() when the target is loaded.
 */
function startReferrerTest(aStartTestCase) {
  waitForExplicitFinish();

  // Open the window where we'll load the source URLs.
  gTestWindow = openDialog(location, "", "chrome,all,dialog=no", "about:blank");
  registerCleanupFunction(function() {
    gTestWindow && gTestWindow.close();
  });

  // Load and start the first test.
  delayedStartupFinished(gTestWindow).then(function() {
    referrerTestCaseLoaded(0).then(function() {
      aStartTestCase(0);
    });
  });
}
