// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
// @ts-check
/// <reference path="../../../@types/mochitest-compat.d.ts" />

/**
 * @typedef {{
 *   entries: Array<{ url: string }>;
 *   index: number;
 *   lastAccessed: number;
 *   [key: string]: unknown;
 * }} SessionTabState
 */

/**
 * @typedef {{
 *   state: SessionTabState;
 *   title: string;
 *   pos: number;
 *   closedAt: number;
 * }} ClosedTabState
 */

/**
 * @typedef {{
 *   id: string;
 *   name: string;
 *   color: string;
 *   collapsed: boolean;
 *   closedAt: number;
 *   sourceWindowId: string;
 *   tabs: ClosedTabState[];
 *   splitViews: unknown[];
 * }} ClosedGroupState
 */

const { SessionStore } = ChromeUtils.importESModule(
  "resource:///modules/sessionstore/SessionStore.sys.mjs",
);

const NORMAL_URL = "data:text/plain,floorp-sessionstore-closed-normal";
const PRIVATE_URL = "data:text/plain,floorp-sessionstore-closed-private";
const LIVE_PRIVATE_URL = "about:mozilla";

const browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
/** @type {unknown} */
let originalWindowState;

/**
 * @param {string} url
 * @param {Record<string, unknown>} [extra]
 * @returns {SessionTabState}
 */
function makeTabState(url, extra = {}) {
  return {
    entries: [{ url }],
    index: 1,
    lastAccessed: Date.now(),
    ...extra,
  };
}

/**
 * @param {string} url
 * @param {Record<string, unknown>} [extra]
 * @returns {ClosedTabState}
 */
function makeClosedTab(url, extra = {}) {
  return {
    state: makeTabState(url, extra),
    title: url,
    pos: 0,
    closedAt: Date.now(),
  };
}

/**
 * @param {string} id
 * @param {ClosedTabState[]} tabs
 * @returns {ClosedGroupState}
 */
function makeClosedGroup(id, tabs) {
  return {
    id,
    name: id,
    color: "blue",
    collapsed: false,
    closedAt: Date.now(),
    sourceWindowId: "test-window",
    tabs,
    splitViews: [],
  };
}

/** @param {unknown} state */
async function restoreWindowState(state) {
  const restored = BrowserTestUtils.waitForEvent(
    browserWindow,
    "SSWindowRestored",
  );
  SessionStore.setWindowState(browserWindow, JSON.stringify(state), true);
  await restored;
}

registerCleanupFunction(async function restoreOriginalState() {
  if (originalWindowState) {
    await restoreWindowState(originalWindowState);
  }
});

add_task(async function closedPrivateContainerTabIsNotRecorded() {
  originalWindowState = SessionStore.getWindowState(browserWindow);
  const before = SessionStore.getClosedTabCountForWindow(browserWindow);
  const tab = /** @type {XULElement} */ (
    await BrowserTestUtils.openNewForegroundTab(gBrowser, LIVE_PRIVATE_URL, true)
  );
  tab.setAttribute("floorp-disablehistory", "true");
  BrowserTestUtils.removeTab(tab);

  await TestUtils.waitForCondition(
    () => SessionStore.getClosedTabCountForWindow(browserWindow) == before,
    "closing a private-container tab should not add closed-tab history",
  );
  is(
    SessionStore.getClosedTabCountForWindow(browserWindow),
    before,
    "private-container closed tabs should be discarded",
  );
});

add_task(async function legacyClosedTabsAndGroupsAreSanitized() {
  await restoreWindowState({
    windows: [
      {
        tabs: [makeTabState(NORMAL_URL)],
        selected: 1,
        _closedTabs: [
          makeClosedTab(NORMAL_URL),
          makeClosedTab(PRIVATE_URL, { floorpDisableHistory: "true" }),
        ],
        closedGroups: [
          makeClosedGroup("mixed-group", [
            makeClosedTab(NORMAL_URL, { groupId: "mixed-group" }),
            makeClosedTab(PRIVATE_URL, {
              groupId: "mixed-group",
              floorpDisableHistory: true,
            }),
          ]),
          makeClosedGroup("private-group", [
            makeClosedTab(PRIVATE_URL, {
              groupId: "private-group",
              floorpDisableHistory: "true",
            }),
          ]),
        ],
      },
    ],
  });

  const internal = SessionStore.getInternalObjectState(browserWindow);
  is(
    internal._closedTabs.length,
    1,
    "only the normal closed tab should remain",
  );
  is(
    internal._closedTabs[0].state.entries[0].url,
    NORMAL_URL,
    "normal closed-tab history should be preserved",
  );
  is(
    internal.closedGroups.length,
    1,
    "an empty private group should be dropped",
  );
  is(
    internal.closedGroups[0].tabs.length,
    1,
    "the mixed group should retain only its normal tab",
  );
  ok(
    !JSON.stringify(internal.closedGroups).includes(PRIVATE_URL),
    "closed group data must not retain private URLs",
  );
});

add_task(async function legacyClosedWindowIsSanitizedWithoutLosingNormalData() {
  await restoreWindowState({
    windows: [
      {
        tabs: [makeTabState(NORMAL_URL)],
        selected: 1,
        _closedTabs: [],
        closedGroups: [],
      },
    ],
    _closedWindows: [
      {
        title: PRIVATE_URL,
        tabs: [
          makeTabState(NORMAL_URL),
          makeTabState(PRIVATE_URL, { floorpDisableHistory: "true" }),
        ],
        selected: 2,
        _closedTabs: [
          makeClosedTab(NORMAL_URL),
          makeClosedTab(PRIVATE_URL, { floorpDisableHistory: true }),
        ],
        closedGroups: [
          makeClosedGroup("closed-window-mixed", [
            makeClosedTab(NORMAL_URL, { groupId: "closed-window-mixed" }),
            makeClosedTab(PRIVATE_URL, {
              groupId: "closed-window-mixed",
              floorpDisableHistory: "true",
            }),
          ]),
        ],
      },
      {
        title: PRIVATE_URL,
        tabs: [
          makeTabState(PRIVATE_URL, { floorpDisableHistory: "true" }),
        ],
        selected: 1,
        _closedTabs: [makeClosedTab(NORMAL_URL)],
        closedGroups: [
          makeClosedGroup("normal-closed-data", [
            makeClosedTab(NORMAL_URL, { groupId: "normal-closed-data" }),
          ]),
        ],
      },
      {
        title: PRIVATE_URL,
        tabs: [
          makeTabState(PRIVATE_URL, { floorpDisableHistory: "true" }),
        ],
        selected: 1,
        _closedTabs: [],
        closedGroups: [],
      },
    ],
  });

  const browserState = JSON.parse(SessionStore.getBrowserState());
  is(
    browserState._closedWindows.length,
    2,
    "a closed window containing only private data should be dropped",
  );
  const closedWindow = browserState._closedWindows[0];
  is(closedWindow.tabs.length, 1, "only the normal open tab should remain");
  is(
    closedWindow.tabs[0].entries[0].url,
    NORMAL_URL,
    "normal closed-window data should be preserved",
  );
  is(closedWindow.selected, 1, "closed-window selection should be remapped");
  is(
    closedWindow._closedTabs.length,
    1,
    "normal closed-tab data should remain",
  );
  is(
    closedWindow.closedGroups[0].tabs.length,
    1,
    "normal closed-group data should remain",
  );
  is(
    closedWindow.title,
    NORMAL_URL,
    "closed-window title should be recomputed from the surviving tab",
  );

  const closedDataOnlyWindow = browserState._closedWindows[1];
  is(
    closedDataOnlyWindow.tabs.length,
    0,
    "all private open tabs should be removed from a closed window",
  );
  is(
    closedDataOnlyWindow.selected,
    0,
    "a closed window without open tabs should have no selection",
  );
  ok(
    !("title" in closedDataOnlyWindow),
    "a closed window without normal open tabs should not retain its title",
  );
  is(
    closedDataOnlyWindow._closedTabs.length,
    1,
    "normal closed-tab data should keep the closed-window record alive",
  );
  is(
    closedDataOnlyWindow.closedGroups.length,
    1,
    "normal closed-group data should keep the closed-window record alive",
  );
  ok(
    !JSON.stringify(browserState._closedWindows).includes(PRIVATE_URL),
    "closed-window state must not retain private URLs or titles",
  );
});
