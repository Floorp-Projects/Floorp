const URL = "https://example.com/browser/dom/tests/browser/prerender.html";

// Wait for a process change and then fulfil the promise.
function awaitProcessChange(browser) {
  return new Promise(resolve => {
    browser.addEventListener("BrowserChangedProcess", function bcp(e) {
      browser.removeEventListener("BrowserChangedProcess", bcp);
      info("The browser changed process!");
      resolve();
    });
  });
}

// Wait for onbeforeunload dialog, and dismiss it immediately.
function awaitAndCloseBeforeUnloadDialog() {
  return new Promise(resolve => {
    function onDialogShown(node) {
      info("Found onbeforeunload dialog");
      Services.obs.removeObserver(onDialogShown,
        "tabmodal-dialog-loaded");
      let dismissButton = node.ui.button0;
      dismissButton.click();
      resolve();
    }

    Services.obs.addObserver(onDialogShown, "tabmodal-dialog-loaded");
  });
}

add_task(function* () {
  yield SpecialPowers.pushPrefEnv({
    set: [["browser.groupedhistory.enabled", true],
          ["dom.linkPrerender.enabled", true],
          ["dom.require_user_interaction_for_beforeunload", false]]
  });
});

// Test 1: Creating a prerendered browser, and clicking on a link to that browser,
// will cause changes.
add_task(function* () {
  let tab = gBrowser.addTab(URL);

  yield new Promise(resolve => {
    tab.linkedBrowser.messageManager.addMessageListener("Prerender:Request", function f() {
      tab.linkedBrowser.messageManager.removeMessageListener("Prerender:Request", f);
      info("Successfully received the prerender request");
      resolve();
    });
  });
  yield BrowserTestUtils.switchTab(gBrowser, tab);

  // Check that visibilityState is set correctly in the prerendered tab. We
  // check all of the tabs because we have no easy way to tell which one is
  // which.
  let result;
  for (let i = 0; i < gBrowser.tabs.length; ++i) {
    result = yield ContentTask.spawn(gBrowser.tabs[i].linkedBrowser, null, function* () {
      let interesting = content.document.location.toString() == "data:text/html,b";
      if (interesting) {
        is(content.document.visibilityState, "prerender");
      }
      return interesting;
    });
    if (result) {
      break;
    }
  }
  ok(result, "The prerender tab was found!");

  let dialogShown = awaitAndCloseBeforeUnloadDialog();
  ContentTask.spawn(tab.linkedBrowser, null, function*() {
    let anchor = content.document.querySelector("a");
    anchor.click();
  });
  yield dialogShown;

  yield awaitProcessChange(tab.linkedBrowser);

  yield ContentTask.spawn(tab.linkedBrowser, null, function*() {
    is(content.document.location.toString(), "data:text/html,b");
    is(content.document.visibilityState, "visible",
       "VisibilityState of formerly prerendered window must be visible");
  });

  let groupedSHistory = tab.linkedBrowser.frameLoader.groupedSessionHistory;
  is(groupedSHistory.count, 2, "Check total length of grouped shistory.");
  is(gBrowser.tabs.length, 3, "Check number of opened tabs.");

  // We don't touch the about:blank tab opened when browser mochitest runs.
  // The tabs under test are the other 2 tabs, so we're expecting 2 TabClose.
  let closed = new Promise(resolve => {
    let seen = 0;
    gBrowser.tabContainer.addEventListener("TabClose", function f() {
      if (++seen == 2) {
        gBrowser.tabContainer.removeEventListener("TabClose", f);
        resolve();
      }
    });
  });

  yield BrowserTestUtils.removeTab(tab);
  yield closed;

  is(gBrowser.tabs.length, 1);
});

// Test 2: Creating a prerendered browser, and navigating to a different url,
// succeeds, and closes the prerendered browser.
add_task(function* () {
  let tab = gBrowser.addTab(URL);

  yield Promise.all([
    BrowserTestUtils.browserLoaded(tab.linkedBrowser),
    new Promise(resolve => {
      tab.linkedBrowser.messageManager.addMessageListener("Prerender:Request", function f() {
        tab.linkedBrowser.messageManager.removeMessageListener("Prerender:Request", f);
        info("Successfully received the prerender request");
        resolve();
      });
    }),
    BrowserTestUtils.switchTab(gBrowser, tab),
  ]);

  let dialogShown = awaitAndCloseBeforeUnloadDialog();
  ContentTask.spawn(tab.linkedBrowser, null, function*() {
    let anchor = content.document.querySelector("a");
    anchor.setAttribute("href", "data:text/html,something_else");
    anchor.click();
  });
  yield dialogShown;

  yield Promise.all([
    BrowserTestUtils.browserLoaded(tab.linkedBrowser),
    new Promise(resolve => {
      let seen = false;
      gBrowser.tabContainer.addEventListener("TabClose", function f() {
        gBrowser.tabContainer.removeEventListener("TabClose", f);
        if (!seen) {
          seen = true;
          info("The tab was closed");
          resolve();
        }
      });
    }),
  ]);

  yield ContentTask.spawn(tab.linkedBrowser, null, function*() {
    is(content.document.location.toString(), "data:text/html,something_else");
  });

  yield BrowserTestUtils.removeTab(tab);
});
