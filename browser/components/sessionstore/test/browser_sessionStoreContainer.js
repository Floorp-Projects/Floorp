/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(function* () {
  for (let i = 0; i < 3; ++i) {
    let tab = gBrowser.addTab("http://example.com/", { userContextId: i });
    let browser = tab.linkedBrowser;

    yield promiseBrowserLoaded(browser);

    let tab2 = gBrowser.duplicateTab(tab);
    Assert.equal(tab2.getAttribute("usercontextid"), i);
    let browser2 = tab2.linkedBrowser;
    yield promiseTabRestored(tab2)

    yield ContentTask.spawn(browser2, { expectedId: i }, function* (args) {
      let loadContext = docShell.QueryInterface(Ci.nsILoadContext);
      Assert.equal(loadContext.originAttributes.userContextId,
        args.expectedId, "The docShell has the correct userContextId");
    });

    yield promiseRemoveTab(tab);
    yield promiseRemoveTab(tab2);
  }
});

add_task(function* () {
  let tab = gBrowser.addTab("http://example.com/", { userContextId: 1 });
  let browser = tab.linkedBrowser;

  yield promiseBrowserLoaded(browser);

  gBrowser.selectedTab = tab;

  let tab2 = gBrowser.duplicateTab(tab);
  let browser2 = tab2.linkedBrowser;
  yield promiseTabRestored(tab2)

  yield ContentTask.spawn(browser2, { expectedId: 1 }, function* (args) {
    Assert.equal(docShell.getOriginAttributes().userContextId,
                 args.expectedId,
                 "The docShell has the correct userContextId");
  });

  yield promiseRemoveTab(tab);
  yield promiseRemoveTab(tab2);
});

add_task(function* () {
  let tab = gBrowser.addTab("http://example.com/", { userContextId: 1 });
  let browser = tab.linkedBrowser;

  yield promiseBrowserLoaded(browser);

  gBrowser.removeTab(tab);

  let tab2 = ss.undoCloseTab(window, 0);
  Assert.equal(tab2.getAttribute("usercontextid"), 1);
  yield promiseTabRestored(tab2);
  yield ContentTask.spawn(tab2.linkedBrowser, { expectedId: 1 }, function* (args) {
    Assert.equal(docShell.getOriginAttributes().userContextId,
                 args.expectedId,
                 "The docShell has the correct userContextId");
  });

  yield promiseRemoveTab(tab2);
});

// Opens "uri" in a new tab with the provided userContextId and focuses it.
// Returns the newly opened tab.
function* openTabInUserContext(userContextId) {
  // Open the tab in the correct userContextId.
  let tab = gBrowser.addTab("http://example.com", { userContextId });

  // Select tab and make sure its browser is focused.
  gBrowser.selectedTab = tab;
  tab.ownerGlobal.focus();

  let browser = gBrowser.getBrowserForTab(tab);
  yield BrowserTestUtils.browserLoaded(browser);
  return { tab, browser };
}

function waitForNewCookie() {
  return new Promise(resolve => {
    Services.obs.addObserver(function observer(subj, topic, data) {
      let cookie = subj.QueryInterface(Ci.nsICookie2);
      if (data == "added") {
        Services.obs.removeObserver(observer, topic);
        resolve();
      }
    }, "cookie-changed", false);
  });
}

add_task(function* test() {
  const USER_CONTEXTS = [
    "default",
    "personal",
    "work",
  ];

  const ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);
  const { TabStateFlusher } = Cu.import("resource:///modules/sessionstore/TabStateFlusher.jsm", {});

  // Make sure userContext is enabled.
  yield SpecialPowers.pushPrefEnv({
    "set": [ [ "privacy.userContext.enabled", true ] ]
  });

  let lastSessionRestore;
  for (let userContextId of Object.keys(USER_CONTEXTS)) {
    // Load the page in 3 different contexts and set a cookie
    // which should only be visible in that context.
    let cookie = USER_CONTEXTS[userContextId];

    // Open our tab in the given user context.
    let { tab, browser } = yield* openTabInUserContext(userContextId);

    yield Promise.all([
      waitForNewCookie(),
      ContentTask.spawn(browser, cookie, cookie => content.document.cookie = cookie)
    ]);

    // Ensure the tab's session history is up-to-date.
    yield TabStateFlusher.flush(browser);

    lastSessionRestore = ss.getWindowState(window);

    // Remove the tab.
    gBrowser.removeTab(tab);
  }

  let state = JSON.parse(lastSessionRestore);
  is(state.windows[0].cookies.length, USER_CONTEXTS.length,
    "session restore should have each container's cookie");
});

