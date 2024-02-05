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

async function cleanup(...windows) {
  for (let win of windows) {
    let closedObjectsChangePromise = TestUtils.topicObserved(
      "sessionstore-closed-objects-changed"
    );
    await BrowserTestUtils.closeWindow(win);
    await closedObjectsChangePromise;
  }
  await SessionStoreTestUtils.promiseBrowserState(ORIG_STATE);
  info("cleanup, browser state restored");
}

function deltaTime(time, expectedTime) {
  return Math.abs(expectedTime - time);
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
    gBrowser.selectedTab.linkedBrowser.currentURI.spec,
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
  await SessionStoreTestUtils.promiseBrowserState({
    windows: [
      {
        tabs: [tabEntry("data:,Window1-Tab0", yesterday)],
        selected: 1,
        sizemodeBeforeMinimized: "normal",
        sizemode: "maximized",
      },
      {
        tabs: [tabEntry("data:,Window2-Tab0", yesterday)],
        selected: 1,
        sizemodeBeforeMinimized: "normal",
        sizemode: "maximized",
      },
    ],
    selectedWindow: 1, // this selects the window1 window
  });
  let deltaMs;
  const windows = BrowserWindowTracker.orderedWindows;

  is(
    windows[0].gBrowser.selectedBrowser.currentURI.spec,
    "data:,Window1-Tab0",
    "The expected window and tab is selected"
  );

  deltaMs = deltaTime(
    windows[0].gBrowser.selectedTab.lastSeenActive,
    Date.now()
  );
  ok(
    deltaMs < 1000,
    `The selected tab of the first window is last seen now. Actual delta: ${deltaMs}`
  );

  deltaMs = deltaTime(
    windows[1].gBrowser.selectedTab.lastSeenActive,
    Date.now()
  );
  ok(
    deltaMs < 1000,
    `The selected tab of the 2nd window is last seen now. Actual delta: ${deltaMs}`
  );

  await SimpleTest.promiseFocus(windows[1]);
  // wait a little and then check again
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(res => setTimeout(res, 250));
  ok(
    windows[1].gBrowser.selectedTab.lastSeenActive >
      windows[0].gBrowser.selectedTab.lastSeenActive,
    "The foreground window selected tab is last seen more recently than the backgrounded one"
  );

  // minimize the foreground window and focus the other
  let promiseSizeModeChange = BrowserTestUtils.waitForEvent(
    windows[1],
    "sizemodechange"
  );
  windows[1].minimize();
  await promiseSizeModeChange;
  ok(
    !windows[1].gBrowser.selectedTab.linkedBrowser.docShellIsActive,
    "Docshell should be Inactive"
  );
  ok(
    windows[1].document.hidden,
    "Minimized windows's document should be hidden"
  );
  await SimpleTest.promiseFocus(windows[0]);

  // wait a little and then check again
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(res => setTimeout(res, 250));
  ok(
    windows[0].gBrowser.selectedTab.lastSeenActive >
      windows[1].gBrowser.selectedTab.lastSeenActive,
    "The foreground window selected tab is last seen more recently than the minimized one"
  );

  await cleanup(windows[1]);
});
