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

async function changeSizeMode(win, mode) {
  let promise = BrowserTestUtils.waitForEvent(win, "sizemodechange");
  win[mode]();
  await promise;
}

async function cleanup() {
  await SessionStoreTestUtils.promiseBrowserState(ORIG_STATE);
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
  ok(
    deltaTime(gBrowser.selectedTab.lastSeenActive, Date.now()) < SECOND_MS,
    "The selected tab's lastSeenActive is now"
  );
  ok(
    deltaTime(gBrowser.selectedTab.lastAccessed, Date.now()) < SECOND_MS,
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
  let initialTab = gBrowser.selectedTab;
  let applicationStart = Services.startup.getStartupInfo().start.getTime();
  let openedTab = BrowserTestUtils.addTab(gBrowser, "data:,Tab1");
  await BrowserTestUtils.browserLoaded(openedTab.linkedBrowser);

  ok(!openedTab.selected, "The background tab we opened isn't selected");
  ok(
    initialTab.selected &&
      deltaTime(initialTab.lastSeenActive, Date.now() < SECOND_MS),
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

  await BrowserTestUtils.switchTab(gBrowser, openedTab);
  ok(
    deltaTime(openedTab.lastSeenActive, Date.now()) < SECOND_MS,
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
  await Promise.all(
    Array.from(BrowserWindowTracker.pendingWindows.values()).map(
      win => win.deferred.promise
    )
  );
  info("All the pending windows are resolved");
  let expectedTabURLs = ["data:,Window1-Tab0", "data:,Window2-Tab0"];
  const win1 = getWindowByTabUrl("data:,Window1-Tab0");
  const win2 = getWindowByTabUrl("data:,Window2-Tab0");
  ok(win1, "Found the first window by its tab URL of data:,Window1-Tab0");
  ok(win2, "Found the 2nd window by its tab URL of data:,Window2-Tab0");

  // Log out activate events to help troubleshoot intermittents
  const onActivate = event => {
    if (event.target == win1) {
      info(`Window 1 activated`);
    }
    if (event.target == win2) {
      info(`Window 2 activated`);
    }
  };
  [win1, win2].forEach(win => win.addEventListener("activate", onActivate));

  info("Focusing win1");
  // In theory the zIndex values in the session state should make win1 active
  // But in practice that isn't always true. To ensure we're testing from a known state,
  // ensure the first window is active before proceeding with the test
  await SimpleTest.promiseFocus(win1);
  is(
    win1,
    Services.focus.activeWindow,
    "win1 is the focus manager's activeWindow"
  );
  is(
    win1,
    BrowserWindowTracker.getTopWindow(),
    "Got the right foreground window"
  );

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
  await SimpleTest.promiseFocus(win2);
  // wait a little so the timestamps will differ and then check again
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(res => setTimeout(res, 100));
  ok(
    win2.gBrowser.selectedTab.lastSeenActive > lastSeenTimes[1],
    "The foreground window selected tab is last seen more recently than it was before being focused"
  );
  ok(
    win2.gBrowser.selectedTab.lastSeenActive >
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
  await SimpleTest.promiseFocus(win1);

  await BrowserTestUtils.waitForCondition(() => {
    return BrowserWindowTracker.getTopWindow() == win1;
  }, "The first window is the top window again");

  ok(
    !win2.gBrowser.selectedTab.linkedBrowser.docShellIsActive,
    "Docshell should be Inactive"
  );
  ok(win2.document.hidden, "Minimized windows's document should be hidden");

  // wait a little so the timestamps will differ and then check again
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(res => setTimeout(res, 100));
  ok(
    win1.gBrowser.selectedTab.lastSeenActive >
      win2.gBrowser.selectedTab.lastSeenActive,
    "The foreground window selected tab is last seen more recently than the minimized one"
  );
  ok(
    win1.gBrowser.selectedTab.lastSeenActive > lastSeenTimes[0],
    "The foreground window selected tab is last seen more recently than it was before being focused"
  );

  [win1, win2].forEach(win => win.removeEventListener("activate", onActivate));
  await cleanup();
});
