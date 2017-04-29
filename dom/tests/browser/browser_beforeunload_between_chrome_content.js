const TEST_URL = "http://www.example.com/browser/dom/tests/browser/dummy.html";

function pageScript() {
  window.addEventListener("beforeunload", function (event) {
    var str = "Leaving?";
    event.returnValue = str;
    return str;
  }, true);
}

function injectBeforeUnload(browser) {
  return ContentTask.spawn(browser, null, function*() {
    content.window.addEventListener("beforeunload", function (event) {
      sendAsyncMessage("Test:OnBeforeUnloadReceived");
      var str = "Leaving?";
      event.returnValue = str;
      return str;
    }, true);
  });
}

// Wait for onbeforeunload dialog, and dismiss it immediately.
function awaitAndCloseBeforeUnloadDialog(doStayOnPage) {
  return new Promise(resolve => {
    function onDialogShown(node) {
      Services.obs.removeObserver(onDialogShown, "tabmodal-dialog-loaded");
      let button = doStayOnPage ? node.ui.button1 : node.ui.button0;
      button.click();
      resolve();
    }

    Services.obs.addObserver(onDialogShown, "tabmodal-dialog-loaded");
  });
}

SpecialPowers.pushPrefEnv(
  {"set": [["dom.require_user_interaction_for_beforeunload", false]]});

/**
 * Test navigation from a content page to a chrome page. Also check that only
 * one beforeunload event is fired.
 */
add_task(function* () {
  let beforeUnloadCount = 0;
  messageManager.addMessageListener("Test:OnBeforeUnloadReceived", function() {
    beforeUnloadCount++;
  });

  // Open a content page.
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  let browser = tab.linkedBrowser;

  ok(browser.isRemoteBrowser, "Browser should be remote.");

  yield injectBeforeUnload(browser);
  // Navigate to a chrome page.
  let dialogShown1 = awaitAndCloseBeforeUnloadDialog(false);
  yield BrowserTestUtils.loadURI(browser, "about:support");
  yield Promise.all([
    dialogShown1,
    BrowserTestUtils.browserLoaded(browser)
  ]);

  is(beforeUnloadCount, 1, "Should have received one beforeunload event.");
  ok(!browser.isRemoteBrowser, "Browser should not be remote.");

  // Go back to content page.
  ok(gBrowser.webNavigation.canGoBack, "Should be able to go back.");
  gBrowser.goBack();
  yield BrowserTestUtils.browserLoaded(browser);
  yield injectBeforeUnload(browser);

  // Test that going forward triggers beforeunload prompt as well.
  ok(gBrowser.webNavigation.canGoForward, "Should be able to go forward.");
  let dialogShown2 = awaitAndCloseBeforeUnloadDialog(false);
  gBrowser.goForward();
  yield Promise.all([
    dialogShown2,
    BrowserTestUtils.browserLoaded(browser)
  ]);
  is(beforeUnloadCount, 2, "Should have received two beforeunload events.");

  yield BrowserTestUtils.removeTab(tab);
});

/**
 * Test navigation from a chrome page to a content page. Also check that only
 * one beforeunload event is fired.
 */
add_task(function* () {
  let beforeUnloadCount = 0;
  messageManager.addMessageListener("Test:OnBeforeUnloadReceived", function() {
    beforeUnloadCount++;
  });

  // Open a chrome page.
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser,
                                                        "about:support");
  let browser = tab.linkedBrowser;

  ok(!browser.isRemoteBrowser, "Browser should not be remote.");
  yield ContentTask.spawn(browser, null, function* () {
    content.window.addEventListener("beforeunload", function (event) {
      sendAsyncMessage("Test:OnBeforeUnloadReceived");
      var str = "Leaving?";
      event.returnValue = str;
      return str;
    }, true);
  });

  // Navigate to a content page.
  let dialogShown1 = awaitAndCloseBeforeUnloadDialog(false);
  yield BrowserTestUtils.loadURI(browser, TEST_URL);
  yield Promise.all([
    dialogShown1,
    BrowserTestUtils.browserLoaded(browser)
  ]);
  is(beforeUnloadCount, 1, "Should have received one beforeunload event.");
  ok(browser.isRemoteBrowser, "Browser should be remote.");

  // Go back to chrome page.
  ok(gBrowser.webNavigation.canGoBack, "Should be able to go back.");
  gBrowser.goBack();
  yield BrowserTestUtils.browserLoaded(browser);
  yield ContentTask.spawn(browser, null, function* () {
    content.window.addEventListener("beforeunload", function (event) {
      sendAsyncMessage("Test:OnBeforeUnloadReceived");
      var str = "Leaving?";
      event.returnValue = str;
      return str;
    }, true);
  });

  // Test that going forward triggers beforeunload prompt as well.
  ok(gBrowser.webNavigation.canGoForward, "Should be able to go forward.");
  let dialogShown2 = awaitAndCloseBeforeUnloadDialog(false);
  gBrowser.goForward();
  yield Promise.all([
    dialogShown2,
    BrowserTestUtils.browserLoaded(browser)
  ]);
  is(beforeUnloadCount, 2, "Should have received two beforeunload events.");

  yield BrowserTestUtils.removeTab(tab);
});
