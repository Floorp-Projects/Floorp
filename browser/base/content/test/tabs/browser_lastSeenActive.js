/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { SessionStoreTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SessionStoreTestUtils.sys.mjs"
);

const triggeringPrincipal_base64 = E10SUtils.SERIALIZED_SYSTEMPRINCIPAL;

SessionStoreTestUtils.init(this, window);
// take a state snapshot we can restore to after each test
const ORIG_STATE = SessionStore.getBrowserState();

const SECOND_MS = 1000;
const DAY_MS = 24 * 60 * 60 * 1000;
const today = new Date().getTime();
const yesterday = new Date(Date.now() - DAY_MS).getTime();

function tabEntry(url, lastAccessed) {
  return {
    entries: [{ url, triggeringPrincipal_base64 }],
    lastAccessed,
  };
}

/**
 * Make the given window focused and active
 */
async function switchToWindow(win) {
  info("switchToWindow, waiting for promiseFocus");
  await SimpleTest.promiseFocus(win);
  info("switchToWindow, waiting for correct Services.focus.activeWindow");
  await BrowserTestUtils.waitForCondition(
    () => Services.focus.activeWindow == win
  );
}

async function changeSizeMode(win, mode) {
  let promise = BrowserTestUtils.waitForEvent(win, "sizemodechange");
  win[mode]();
  await promise;
}

async function cleanup() {
  await switchToWindow(window);
  await SessionStoreTestUtils.promiseBrowserState(ORIG_STATE);
  is(
    BrowserWindowTracker.orderedWindows.length,
    1,
    "One window at the end of test cleanup"
  );
  info("cleanup, browser state restored");
}

function deltaTime(time, expectedTime) {
  return Math.abs(expectedTime - time);
}

function getWindowUrl(win) {
  return win.gBrowser.selectedBrowser?.currentURI?.spec;
}

function getWindowByTabUrl(url) {
  return BrowserWindowTracker.orderedWindows.find(
    win => getWindowUrl(win) == url
  );
}

add_task(async function restoredTabs() {
  const now = Date.now();
  await SessionStoreTestUtils.promiseBrowserState({
    windows: [
      {
        tabs: [
          tabEntry("data:,Window0-Tab0", yesterday),
          tabEntry("data:,Window0-Tab1", yesterday),
        ],
        selected: 2,
      },
    ],
  });
  is(
    gBrowser.visibleTabs[1],
    gBrowser.selectedTab,
    "The selected tab is the 2nd visible tab"
  );
  is(
    getWindowUrl(window),
    "data:,Window0-Tab1",
    "The expected tab is selected"
  );
  Assert.greaterOrEqual(
    gBrowser.selectedTab.lastSeenActive,
    now,
    "The selected tab's lastSeenActive is now"
  );
  Assert.greaterOrEqual(
    gBrowser.selectedTab.lastAccessed,
    now,
    "The selected tab's lastAccessed is now"
  );

  // tab restored from session but never seen or active
  is(
    gBrowser.visibleTabs[0].lastSeenActive,
    yesterday,
    "The restored tab's lastSeenActive is yesterday"
  );
  await cleanup();
});

add_task(async function switchingTabs() {
  let now = Date.now();
  let initialTab = gBrowser.selectedTab;
  let applicationStart = Services.startup.getStartupInfo().start.getTime();
  let openedTab = BrowserTestUtils.addTab(gBrowser, "data:,Tab1");
  await BrowserTestUtils.browserLoaded(openedTab.linkedBrowser);

  ok(!openedTab.selected, "The background tab we opened isn't selected");
  Assert.greaterOrEqual(
    initialTab.selected && initialTab.lastSeenActive,
    now,
    "The initial tab is selected and last seen now"
  );

  is(
    openedTab.lastSeenActive,
    applicationStart,
    `Background tab got default lastSeenActive value, delta: ${deltaTime(
      openedTab.lastSeenActive,
      applicationStart
    )}`
  );

  now = Date.now();
  await BrowserTestUtils.switchTab(gBrowser, openedTab);
  Assert.greaterOrEqual(
    openedTab.lastSeenActive,
    now,
    "The tab we switched to is last seen now"
  );

  await cleanup();
});

add_task(async function switchingWindows() {
  info("Restoring to the test browser state");
  await SessionStoreTestUtils.promiseBrowserState({
    windows: [
      {
        tabs: [tabEntry("data:,Window1-Tab0", yesterday)],
        selected: 1,
        sizemodeBeforeMinimized: "normal",
        sizemode: "maximized",
        zIndex: 1, // this will be the selected window
      },
      {
        tabs: [tabEntry("data:,Window2-Tab0", yesterday)],
        selected: 1,
        sizemodeBeforeMinimized: "normal",
        sizemode: "maximized",
        zIndex: 2,
      },
    ],
  });
  info("promiseBrowserState resolved");
  info(
    `BrowserWindowTracker.pendingWindows: ${BrowserWindowTracker.pendingWindows.size}`
  );
  await Promise.all(
    Array.from(BrowserWindowTracker.pendingWindows.values()).map(
      win => win.deferred.promise
    )
  );
  info("All the pending windows are resolved");
  info("Waiting for the firstBrowserLoaded in each of the windows");
  await Promise.all(
    BrowserWindowTracker.orderedWindows.map(win => {
      const selectedUrl = getWindowUrl(win);
      if (selectedUrl && selectedUrl !== "about:blank") {
        return Promise.resolve();
      }
      return BrowserTestUtils.firstBrowserLoaded(win, false);
    })
  );
  let expectedTabURLs = ["data:,Window1-Tab0", "data:,Window2-Tab0"];
  let [win1, win2] = expectedTabURLs.map(url => getWindowByTabUrl(url));
  if (BrowserWindowTracker.getTopWindow() !== win1) {
    info("Switch to win1 which isn't active/top after restoring session");
    // In theory the zIndex values in the session state should make win1 active
    // But in practice that isn't always true. To ensure we're testing from a known state,
    // ensure the first window is active before proceeding with the test
    await switchToWindow(win1);
    [win1, win2] = expectedTabURLs.map(url => getWindowByTabUrl(url));
  }

  let actualTabURLs = Array.from(BrowserWindowTracker.orderedWindows).map(win =>
    getWindowUrl(win)
  );
  Assert.deepEqual(
    actualTabURLs,
    expectedTabURLs,
    "Both windows are open with selected tab URLs in the expected order"
  );

  let lastSeenTimes = [win1, win2].map(
    win => win.gBrowser.selectedTab.lastSeenActive
  );

  info("Focusing the other window");
  await switchToWindow(win2);
  // wait a little so the timestamps will differ and then check again
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(res => setTimeout(res, 100));
  Assert.greater(
    win2.gBrowser.selectedTab.lastSeenActive,
    lastSeenTimes[1],
    "The foreground window selected tab is last seen more recently than it was before being focused"
  );
  Assert.greater(
    win2.gBrowser.selectedTab.lastSeenActive,
    win1.gBrowser.selectedTab.lastSeenActive,
    "The foreground window selected tab is last seen more recently than the backgrounded one"
  );

  lastSeenTimes = [win1, win2].map(
    win => win.gBrowser.selectedTab.lastSeenActive
  );
  // minimize the foreground window and focus the other
  let promiseSizeModeChange = BrowserTestUtils.waitForEvent(
    win2,
    "sizemodechange"
  );
  win2.minimize();
  info("Waiting for the sizemodechange on minimized window");
  await promiseSizeModeChange;
  await switchToWindow(win1);

  ok(
    !win2.gBrowser.selectedTab.linkedBrowser.docShellIsActive,
    "Docshell should be Inactive"
  );
  ok(win2.document.hidden, "Minimized windows's document should be hidden");

  // wait a little so the timestamps will differ and then check again
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(res => setTimeout(res, 100));
  Assert.greater(
    win1.gBrowser.selectedTab.lastSeenActive,
    win2.gBrowser.selectedTab.lastSeenActive,
    "The foreground window selected tab is last seen more recently than the minimized one"
  );
  Assert.greater(
    win1.gBrowser.selectedTab.lastSeenActive,
    lastSeenTimes[0],
    "The foreground window selected tab is last seen more recently than it was before being focused"
  );

  await cleanup();
});
