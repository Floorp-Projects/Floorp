/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

ChromeUtils.defineESModuleGetters(this, {
  SessionStore: "resource:///modules/sessionstore/SessionStore.sys.mjs",
  TabStateFlusher: "resource:///modules/sessionstore/TabStateFlusher.sys.mjs",
});

const triggeringPrincipal_base64 = E10SUtils.SERIALIZED_SYSTEMPRINCIPAL;

function promiseBrowserStateRestored(state) {
  if (typeof state != "string") {
    state = JSON.stringify(state);
  }
  // We wait for the notification that restore is done, and for the notification
  // that the active tab is loaded and restored.
  let promise = Promise.all([
    TestUtils.topicObserved("sessionstore-browser-state-restored"),
    BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "SSTabRestored"),
  ]);
  SessionStore.setBrowserState(state);
  return promise;
}

function promiseRemoveThenUndoCloseTab(tab) {
  // We wait for the notification that restore is done, and for the notification
  // that the active tab is loaded and restored.
  let promise = Promise.all([
    TestUtils.topicObserved("sessionstore-closed-objects-changed"),
    BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "SSTabRestored"),
  ]);
  BrowserTestUtils.removeTab(tab);
  SessionStore.undoCloseTab(window, 0);
  return promise;
}

// Compare the current browser tab order against the session state ordering, they should always match.
function verifyTabState(state) {
  let newStateTabs = JSON.parse(state).windows[0].tabs;
  for (let i = 0; i < gBrowser.tabs.length; i++) {
    is(
      gBrowser.tabs[i].linkedBrowser.currentURI.spec,
      newStateTabs[i].entries[0].url,
      `tab pos ${i} matched ${gBrowser.tabs[i].linkedBrowser.currentURI.spec}`
    );
  }
}

const bulkLoad = [
  "http://mochi.test:8888/#5",
  "http://mochi.test:8888/#6",
  "http://mochi.test:8888/#7",
  "http://mochi.test:8888/#8",
];

const sessData = {
  windows: [
    {
      tabs: [
        {
          entries: [
            { url: "http://mochi.test:8888/#0", triggeringPrincipal_base64 },
          ],
        },
        {
          entries: [
            { url: "http://mochi.test:8888/#1", triggeringPrincipal_base64 },
          ],
        },
        {
          entries: [
            { url: "http://mochi.test:8888/#3", triggeringPrincipal_base64 },
          ],
        },
        {
          entries: [
            { url: "http://mochi.test:8888/#4", triggeringPrincipal_base64 },
          ],
        },
      ],
    },
  ],
};
// eslint-disable-next-line @microsoft/sdl/no-insecure-url
const urlbarURL = "http://example.com/#urlbar";

async function doTest(aInsertRelatedAfterCurrent, aInsertAfterCurrent) {
  const kDescription =
    "(aInsertRelatedAfterCurrent=" +
    aInsertRelatedAfterCurrent +
    ", aInsertAfterCurrent=" +
    aInsertAfterCurrent +
    "): ";

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.opentabfor.middleclick", true],
      ["browser.tabs.loadBookmarksInBackground", false],
      ["browser.tabs.insertRelatedAfterCurrent", aInsertRelatedAfterCurrent],
      ["browser.tabs.insertAfterCurrent", aInsertAfterCurrent],
    ],
  });

  let oldState = SessionStore.getBrowserState();

  await promiseBrowserStateRestored(sessData);

  // Create a *opener* tab page which has a link to "example.com".
  let pageURL = getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    "http://example.com"
  );
  pageURL = `${pageURL}file_new_tab_page.html`;
  let openerTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    pageURL
  );
  const openerTabIndex = 1;
  gBrowser.moveTabTo(openerTab, openerTabIndex);

  // Open a related tab via Middle click on the cell and test its position.
  let openTabIndex =
    aInsertRelatedAfterCurrent || aInsertAfterCurrent
      ? openerTabIndex + 1
      : gBrowser.tabs.length;
  let openTabDescription =
    aInsertRelatedAfterCurrent || aInsertAfterCurrent
      ? "immediately to the right"
      : "at rightmost";

  let newTabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    "http://example.com/#linkclick",
    true
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#link_to_example_com",
    { button: 1 },
    gBrowser.selectedBrowser
  );
  let openTab = await newTabPromise;
  is(
    openTab.linkedBrowser.currentURI.spec,
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    "http://example.com/#linkclick",
    "Middle click should open site to correct url."
  );
  is(
    openTab._tPos,
    openTabIndex,
    kDescription +
      "Middle click should open site in a new tab " +
      openTabDescription
  );
  if (aInsertRelatedAfterCurrent || aInsertAfterCurrent) {
    is(openTab.owner, openerTab, "tab owner is set correctly");
  }
  is(openTab.openerTab, openerTab, "opener tab is set");

  // Open an unrelated tab from the URL bar and test its position.
  openTabIndex = aInsertAfterCurrent
    ? openerTabIndex + 1
    : gBrowser.tabs.length;
  openTabDescription = aInsertAfterCurrent
    ? "immediately to the right"
    : "at rightmost";

  gURLBar.focus();
  gURLBar.select();
  newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, urlbarURL, true);
  EventUtils.sendString(urlbarURL);
  EventUtils.synthesizeKey("KEY_Alt", {
    altKey: true,
    code: "AltLeft",
    type: "keydown",
  });
  EventUtils.synthesizeKey("KEY_Enter", { altKey: true, code: "Enter" });
  EventUtils.synthesizeKey("KEY_Alt", {
    altKey: false,
    code: "AltLeft",
    type: "keyup",
  });
  let unrelatedTab = await newTabPromise;

  is(
    gBrowser.selectedBrowser.currentURI.spec,
    unrelatedTab.linkedBrowser.currentURI.spec,
    `${kDescription} ${urlbarURL} should be loaded in the current tab.`
  );
  is(
    unrelatedTab._tPos,
    openTabIndex,
    `${kDescription} Alt+Enter in the URL bar should open page in a new tab ${openTabDescription}`
  );
  is(unrelatedTab.owner, openerTab, "owner tab is set correctly");
  ok(!unrelatedTab.openerTab, "no opener tab is set");

  // Closing this should go back to the last selected tab, which just happens to be "openerTab"
  // but is not in fact the opener.
  BrowserTestUtils.removeTab(unrelatedTab);
  is(
    gBrowser.selectedTab,
    openerTab,
    kDescription + `openerTab should be selected after closing unrelated tab`
  );

  // Go back to the opener tab.  Closing the child tab should return to the opener.
  BrowserTestUtils.removeTab(openTab);
  is(
    gBrowser.selectedTab,
    openerTab,
    kDescription + "openerTab should be selected after closing related tab"
  );

  // Flush before messing with browser state.
  for (let tab of gBrowser.tabs) {
    await TabStateFlusher.flush(tab.linkedBrowser);
  }

  // Get the session state, verify SessionStore gives us expected data.
  let newState = SessionStore.getBrowserState();
  verifyTabState(newState);

  // Remove the tab at the end, then undo.  It should reappear where it was.
  await promiseRemoveThenUndoCloseTab(gBrowser.tabs[gBrowser.tabs.length - 1]);
  verifyTabState(newState);

  // Remove a tab in the middle, then undo.  It should reappear where it was.
  await promiseRemoveThenUndoCloseTab(gBrowser.tabs[2]);
  verifyTabState(newState);

  // Bug 1442679 - Test bulk opening with loadTabs loads the tabs in order

  let loadPromises = Promise.all(
    bulkLoad.map(url =>
      BrowserTestUtils.waitForNewTab(gBrowser, url, false, true)
    )
  );
  // loadTabs will insertAfterCurrent
  let nextTab = aInsertAfterCurrent
    ? gBrowser.selectedTab._tPos + 1
    : gBrowser.tabs.length;

  gBrowser.loadTabs(bulkLoad, {
    inBackground: true,
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });
  await loadPromises;
  for (let i = nextTab, j = 0; j < bulkLoad.length; i++, j++) {
    is(
      gBrowser.tabs[i].linkedBrowser.currentURI.spec,
      bulkLoad[j],
      `bulkLoad tab pos ${i} matched`
    );
  }

  // Now we want to test that positioning remains correct after a session restore.

  // Restore pre-test state so we can restore and test tab ordering.
  await promiseBrowserStateRestored(oldState);

  // Restore test state and verify it is as it was.
  await promiseBrowserStateRestored(newState);
  verifyTabState(newState);

  // Restore pre-test state for next test.
  await promiseBrowserStateRestored(oldState);
}

add_task(async function test_settings_insertRelatedAfter() {
  // Firefox default settings.
  await doTest(true, false);
});

add_task(async function test_settings_insertAfter() {
  await doTest(true, true);
});

add_task(async function test_settings_always_insertAfter() {
  await doTest(false, true);
});

add_task(async function test_settings_always_insertAtEnd() {
  await doTest(false, false);
});
