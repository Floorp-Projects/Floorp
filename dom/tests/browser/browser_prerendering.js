const URL = "https://example.com/browser/dom/tests/browser/prerender.html";
const PRERENDERED_URL = "https://example.com/browser/dom/tests/browser/prerender_target.html";

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");

// Returns a promise which resolves to whether or not PRERENDERED_URL has been visited.
function prerenderedVisited() {
  let uri = Services.io.newURI(PRERENDERED_URL);
  return new Promise(resolve => {
    PlacesUtils.asyncHistory.isURIVisited(uri, (aUri, aIsVisited) => {
      resolve(aIsVisited);
    });
  });
}

// Wait for a process change and then fulfil the promise.
function awaitProcessChange(browser) {
  return new Promise(resolve => {
    browser.addEventListener("BrowserChangedProcess", function(e) {
      info("The browser changed process!");
      resolve();
    }, {once: true});
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

add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.groupedhistory.enabled", true],
          ["dom.linkPrerender.enabled", true],
          ["dom.require_user_interaction_for_beforeunload", false]]
  });
});

// Test 1: Creating a prerendered browser, and clicking on a link to that browser,
// will cause changes.
add_task(async function() {
  await PlacesUtils.history.clear();

  let tab = BrowserTestUtils.addTab(gBrowser, URL);

  is(await prerenderedVisited(), false, "Should not have been visited");

  await new Promise(resolve => {
    tab.linkedBrowser.messageManager.addMessageListener("Prerender:Request", function f() {
      tab.linkedBrowser.messageManager.removeMessageListener("Prerender:Request", f);
      info("Successfully received the prerender request");
      resolve();
    });
  });
  await BrowserTestUtils.switchTab(gBrowser, tab);

  is(await prerenderedVisited(), false, "Should not have been visited");

  // Check that visibilityState is set correctly in the prerendered tab. We
  // check all of the tabs because we have no easy way to tell which one is
  // which.
  let foundTab;
  for (let i = 0; i < gBrowser.tabs.length; ++i) {
    foundTab = await ContentTask.spawn(gBrowser.tabs[i].linkedBrowser, null,
                                       () => content.document.visibilityState == "prerender");
    if (foundTab) {
      break;
    }
  }
  ok(foundTab, "The prerender tab was found!");

  let dialogShown = awaitAndCloseBeforeUnloadDialog();
  ContentTask.spawn(tab.linkedBrowser, null, async function() {
    let anchor = content.document.querySelector("a");
    anchor.click();
  });
  await dialogShown;

  await awaitProcessChange(tab.linkedBrowser);

  await ContentTask.spawn(tab.linkedBrowser, PRERENDERED_URL, async function(PRERENDERED_URL) {
    is(content.document.location.toString(), PRERENDERED_URL);
    isnot(content.document.visibilityState, "prerender",
          "VisibilityState of formerly prerendered window must not be prerender");
  });

  is(await prerenderedVisited(), true, "Should have been visited");

  let groupedSHistory = tab.linkedBrowser.frameLoader.groupedSHistory;
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

  await BrowserTestUtils.removeTab(tab);
  await closed;

  is(await prerenderedVisited(), true, "Should have been visited");

  is(gBrowser.tabs.length, 1);
});

// Test 2: Creating a prerendered browser, and navigating to a different url,
// succeeds, and closes the prerendered browser.
add_task(async function() {
  await PlacesUtils.history.clear();

  let tab = BrowserTestUtils.addTab(gBrowser, URL);

  is(await prerenderedVisited(), false, "Should not have been visited");

  await Promise.all([
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
  ContentTask.spawn(tab.linkedBrowser, null, async function() {
    let anchor = content.document.querySelector("a");
    anchor.setAttribute("href", "data:text/html,something_else");
    anchor.click();
  });
  await dialogShown;

  is(await prerenderedVisited(), false, "Should not have been visited");

  await Promise.all([
    BrowserTestUtils.browserLoaded(tab.linkedBrowser),
    new Promise(resolve => {
      let seen = false;
      gBrowser.tabContainer.addEventListener("TabClose", function() {
        if (!seen) {
          seen = true;
          info("The tab was closed");
          resolve();
        }
      }, {once: true});
    }),
  ]);

  await ContentTask.spawn(tab.linkedBrowser, null, async function() {
    is(content.document.location.toString(), "data:text/html,something_else");
  });

  is(await prerenderedVisited(), false, "Should not have been visited");

  await BrowserTestUtils.removeTab(tab);
});
