/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PATH = "browser/browser/components/sessionstore/test/empty.html";

/* import-globals-from ../../../base/content/test/tabs/helper_origin_attrs_testing.js */
loadTestSubscript(
  "../../../base/content/test/tabs/helper_origin_attrs_testing.js"
);

var TEST_CASES = [
  "https://example.com/" + PATH,
  "https://example.org/" + PATH,
  "about:preferences",
  "about:config",
];

var remoteTypes;

var xulFrameLoaderCreatedCounter = {};

function handleEventLocal(aEvent) {
  if (aEvent.type != "XULFrameLoaderCreated") {
    return;
  }
  // Ignore <browser> element in about:preferences and any other special pages
  if ("gBrowser" in aEvent.target.ownerGlobal) {
    xulFrameLoaderCreatedCounter.numCalledSoFar++;
  }
}

var NUM_DIFF_TAB_MODES = NUM_USER_CONTEXTS + 1; /** regular tab */

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Set the pref to true so we know exactly how many tabs should be restoring at
      // any given time. This guarantees that a finishing load won't start another.
      ["browser.sessionstore.restore_on_demand", true],
      // Don't restore tabs lazily.
      ["browser.sessionstore.restore_tabs_lazily", true],
      // don't preload tabs so we don't have extra XULFrameLoaderCreated events
      // firing
      ["browser.newtab.preload", false],
    ],
  });

  requestLongerTimeout(7);
});

function setupRemoteTypes() {
  if (gFissionBrowser) {
    remoteTypes = [
      "webIsolated=https://example.com",
      "webIsolated=https://example.com^userContextId=1",
      "webIsolated=https://example.com^userContextId=2",
      "webIsolated=https://example.com^userContextId=3",
      "webIsolated=https://example.org",
      "webIsolated=https://example.org^userContextId=1",
      "webIsolated=https://example.org^userContextId=2",
      "webIsolated=https://example.org^userContextId=3",
    ];
  } else {
    remoteTypes = Array(
      NUM_DIFF_TAB_MODES * 2 /** 2 is the number of non parent uris */
    ).fill("web");
  }
  remoteTypes.push(...Array(NUM_DIFF_TAB_MODES * 2).fill(null)); // remote types for about: pages

  forgetClosedWindows();
  is(SessionStore.getClosedWindowCount(), 0, "starting with no closed windows");
}
/*
 * 1. Open several tabs in different containers and in regular tabs
 [page1, page2, page3] [ [(page1 - work) (page1 - home)] [(page2 - work) (page2 - home)] ]
 * 2. Close the window
 * 3. Restore session, window will have the following tabs
 * [initial blank page, page1, page1-work, page1-home, page2, page2-work, page2-home] 
 * 4. Verify correct remote types and that XULFrameLoaderCreated gets fired correct number of times
 */
add_task(async function testRestore() {
  setupRemoteTypes();
  let newWin = await promiseNewWindowLoaded();
  var regularPages = [];
  var containerPages = {};
  // Go through all the test cases and open same set of urls in regular tabs and in container tabs
  for (const uri of TEST_CASES) {
    // Open a url in a regular tab
    let regularPage = await openURIInRegularTab(uri, newWin);
    regularPages.push(regularPage);

    // Open the same url in different user contexts
    for (
      var user_context_id = 1;
      user_context_id <= NUM_USER_CONTEXTS;
      user_context_id++
    ) {
      let containerPage = await openURIInContainer(
        uri,
        newWin,
        user_context_id
      );
      containerPages[uri] = containerPage;
    }
  }
  await TabStateFlusher.flushWindow(newWin);

  // Close the window
  await BrowserTestUtils.closeWindow(newWin);
  await forceSaveState();

  is(
    SessionStore.getClosedWindowCount(),
    1,
    "Should have restore data for the closed window"
  );

  // Now restore the window
  newWin = SessionStore.undoCloseWindow(0);

  // Make sure to wait for the window to be restored.
  await Promise.all([
    BrowserTestUtils.waitForEvent(newWin, "SSWindowStateReady"),
  ]);
  await BrowserTestUtils.waitForEvent(
    newWin.gBrowser.tabContainer,
    "SSTabRestored"
  );

  var nonblank_pages_len =
    TEST_CASES.length + NUM_USER_CONTEXTS * TEST_CASES.length;
  is(
    newWin.gBrowser.tabs.length,
    nonblank_pages_len + 1 /* initial page */,
    "Correct number of tabs restored"
  );

  // Now we have pages opened in the following manner
  // [blank page, page1, page1-work, page1-home, page2, page2-work, page2-home]

  info(`Number of tabs restored: ${newWin.gBrowser.tabs.length}`);
  var currRemoteType, expectedRemoteType;
  let loaded;
  for (var tab_idx = 1; tab_idx < nonblank_pages_len; ) {
    info(`Accessing regular tab at index ${tab_idx}`);
    var test_page_data = regularPages.shift();
    let regular_tab = newWin.gBrowser.tabs[tab_idx];
    let regular_browser = regular_tab.linkedBrowser;

    // I would have used browserLoaded but for about:config it doesn't work
    let ready = BrowserTestUtils.waitForCondition(async () => {
      // Catch an error because the browser might change remoteness in between
      // calls, so we will just wait for the document to finish loadig.
      return SpecialPowers.spawn(regular_browser, [], () => {
        return content.document.readyState == "complete";
      }).catch(Cu.reportError);
    });
    newWin.gBrowser.selectedTab = regular_tab;
    await TabStateFlusher.flush(regular_browser);
    await ready;

    currRemoteType = regular_browser.remoteType;
    expectedRemoteType = remoteTypes.shift();
    is(
      currRemoteType,
      expectedRemoteType,
      `correct remote type for regular tab with uri ${test_page_data.uri}`
    );

    let page_uri = regular_browser.currentURI.spec;
    info(`Current uri = ${page_uri}`);

    // Iterate over container pages, starting after the regular page and ending before the next regular page
    var userContextId = 1;
    for (
      var container_tab_idx = tab_idx + 1;
      container_tab_idx < tab_idx + 1 + NUM_USER_CONTEXTS;
      container_tab_idx++, userContextId++
    ) {
      info(`Accessing container tab at index ${container_tab_idx}`);
      let container_tab = newWin.gBrowser.tabs[container_tab_idx];

      initXulFrameLoaderCreatedCounter(xulFrameLoaderCreatedCounter);
      container_tab.ownerGlobal.gBrowser.addEventListener(
        "XULFrameLoaderCreated",
        handleEventLocal
      );

      loaded = BrowserTestUtils.browserLoaded(
        container_tab.linkedBrowser,
        false,
        test_page_data.uri
      );

      newWin.gBrowser.selectedTab = container_tab;
      await TabStateFlusher.flush(container_tab.linkedBrowser);
      await loaded;
      let uri = container_tab.linkedBrowser.currentURI.spec;

      // Verify XULFrameLoaderCreated was fired once
      if (
        test_page_data.uri == "about:preferences" ||
        test_page_data.uri == "about:config" ||
        gFissionBrowser
      ) {
        todo_is(
          xulFrameLoaderCreatedCounter.numCalledSoFar,
          1,
          `XULFrameLoaderCreated was fired once, when restoring ${uri} in container ${userContextId} `
        );
      } else {
        is(
          xulFrameLoaderCreatedCounter.numCalledSoFar,
          1,
          `XULFrameLoaderCreated was fired once, when restoring ${uri} in container ${userContextId} `
        );
      }
      // While the above assertion is `todo_is`, ensure that we don't have a regression
      // from current behaviour by checking that the event is fired < 3 times.
      info(
        `XULFrameLoaderCreated has been fired ${xulFrameLoaderCreatedCounter.numCalledSoFar} times, when restoring ${uri} in container ${userContextId}`
      );
      ok(
        xulFrameLoaderCreatedCounter.numCalledSoFar <= 2,
        `XULFrameLoaderCreated was fired [1,2] times, when restoring ${uri} in container ${userContextId} `
      );
      container_tab.ownerGlobal.gBrowser.removeEventListener(
        "XULFrameLoaderCreated",
        handleEventLocal
      );

      // Verify correct remote type for container tab
      currRemoteType = container_tab.linkedBrowser.remoteType;
      expectedRemoteType = remoteTypes.shift();
      info(
        `Remote type for container tab ${userContextId} is ${currRemoteType}`
      );
      is(
        currRemoteType,
        expectedRemoteType,
        "correct remote type for container tab"
      );
    }
    // Advance to the next regular page in our tabs list
    tab_idx = container_tab_idx;
  }

  await BrowserTestUtils.closeWindow(newWin);
});
