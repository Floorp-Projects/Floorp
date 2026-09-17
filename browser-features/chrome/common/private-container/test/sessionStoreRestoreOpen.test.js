// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
// @ts-check
/// <reference path="../../../@types/mochitest-compat.d.ts" />

/**
 * @typedef {{
 *   entries: Array<{ url: string }>;
 *   index: number;
 *   lastAccessed: number;
 *   pinned?: boolean;
 *   [key: string]: unknown;
 * }} SessionTabState
 */

/**
 * @typedef {{
 *   id: number;
 *   numberOfTabs: number;
 * }} SplitViewState
 */

/**
 * @typedef {{
 *   id: string;
 *   [key: string]: unknown;
 * }} TabGroupState
 */

/**
 * @typedef {{
 *   tabs: SessionTabState[];
 *   selected: number;
 *   groups: TabGroupState[];
 *   splitViews: SplitViewState[];
 * }} SessionWindowState
 */

/**
 * @typedef {XULElement & {
 *   pinned: boolean;
 *   selected: boolean;
 *   userContextId: number;
 * }} SessionRestoreTab
 */

/**
 * @typedef {GBrowser & {
 *   createTabsForSessionRestore: (
 *     restoreTabsLazily: boolean,
 *     selectTab: number,
 *     tabDataList: SessionTabState[],
 *     tabGroupDataList: TabGroupState[],
 *     splitViewDataList: SplitViewState[],
 *   ) => SessionRestoreTab[];
 * }} SessionRestoreGBrowser
 */

const { SessionStore } = ChromeUtils.importESModule(
  "resource:///modules/sessionstore/SessionStore.sys.mjs",
);

const WORKSPACE_STORE_PREF = "floorp.workspaces.v4.store";
const NORMAL_A = "data:text/plain,floorp-sessionstore-normal-a";
const NORMAL_B = "data:text/plain,floorp-sessionstore-normal-b";
const NORMAL_C = "data:text/plain,floorp-sessionstore-normal-c";
const PRIVATE_URL = "data:text/plain,floorp-sessionstore-private";

const browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
/** @type {unknown} */
let originalWindowState;
/** @type {string | undefined} */
let originalWorkspacePref;

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

/** @param {unknown} state */
async function restoreWindowState(state) {
  const restored = BrowserTestUtils.waitForEvent(
    browserWindow,
    "SSWindowRestored",
  );
  SessionStore.setWindowState(browserWindow, JSON.stringify(state), true);
  await restored;
}

/** @returns {SessionWindowState} */
function currentWindowState() {
  return /** @type {SessionWindowState} */ (
    SessionStore.getWindowState(browserWindow).windows[0]
  );
}

function restoredUrls() {
  return currentWindowState().tabs.map((tab) => tab.entries[0]?.url);
}

registerCleanupFunction(async function restoreOriginalState() {
  if (originalWorkspacePref === undefined) {
    Services.prefs.clearUserPref(WORKSPACE_STORE_PREF);
  } else {
    Services.prefs.setStringPref(WORKSPACE_STORE_PREF, originalWorkspacePref);
  }

  if (originalWindowState) {
    await restoreWindowState(originalWindowState);
  }
});

add_task(async function mixedRestoreKeepsNormalTabsAndMetadataConsistent() {
  originalWindowState = SessionStore.getWindowState(browserWindow);
  originalWorkspacePref = Services.prefs.prefHasUserValue(WORKSPACE_STORE_PREF)
    ? Services.prefs.getStringPref(WORKSPACE_STORE_PREF)
    : undefined;
  Services.prefs.setStringPref(WORKSPACE_STORE_PREF, "{not-json");

  await restoreWindowState({
    windows: [
      {
        tabs: [
          makeTabState(NORMAL_A, {
            pinned: true,
            splitViewId: 7,
            floorpSSB: "true",
          }),
          makeTabState(PRIVATE_URL, {
            floorpDisableHistory: "true",
            pinned: true,
            groupId: "private-group",
            splitViewId: 7,
          }),
          makeTabState(NORMAL_B, {
            groupId: "normal-group",
            splitViewId: 8,
          }),
          makeTabState(NORMAL_C, {
            groupId: "normal-group",
            splitViewId: 8,
          }),
        ],
        selected: 2,
        groups: [
          {
            id: "normal-group",
            name: "Normal group",
            color: "blue",
            collapsed: false,
          },
          {
            id: "private-group",
            name: "Private group",
            color: "red",
            collapsed: false,
          },
        ],
        splitViews: [
          { id: 7, numberOfTabs: 2 },
          { id: 8, numberOfTabs: 2 },
        ],
        _closedTabs: [],
        closedGroups: [],
      },
    ],
  });

  const state = currentWindowState();
  Assert.deepEqual(
    restoredUrls(),
    [NORMAL_A, NORMAL_B, NORMAL_C],
    "only normal tabs should be restored",
  );
  is(state.selected, 1, "selection should fall back to the preceding tab");
  ok(state.tabs[0].pinned, "a normal pinned tab should stay pinned");
  ok(!window.closed, "restoring an SSB tab must not close the window");
  is(
    gBrowser.tabs[0].getAttribute("floorpSSB"),
    "true",
    "the SSB marker should still be restored as a tab attribute",
  );
  is(
    state.groups.length,
    1,
    "orphaned private group metadata should be removed",
  );
  is(state.groups[0].id, "normal-group", "the normal group should remain");
  is(state.splitViews.length, 1, "a singleton split view should be removed");
  is(state.splitViews[0].id, 8, "the intact split view should remain");
  is(
    state.splitViews[0].numberOfTabs,
    2,
    "split view metadata should match surviving tabs",
  );
  ok(
    !JSON.stringify(state).includes(PRIVATE_URL),
    "private tab data must not survive in restored state",
  );
});

add_task(async function allPrivateRestoreKeepsTheExistingBlankTab() {
  await restoreWindowState({
    windows: [
      {
        tabs: [makeTabState("about:blank")],
        selected: 1,
        _closedTabs: [],
        closedGroups: [],
      },
    ],
  });

  await restoreWindowState({
    windows: [
      {
        tabs: [
          makeTabState(PRIVATE_URL, { floorpDisableHistory: true }),
        ],
        selected: 1,
        groups: [],
        splitViews: [],
        _closedTabs: [],
        closedGroups: [],
      },
    ],
  });

  is(gBrowser.tabs.length, 1, "one startup tab should remain");
  is(restoredUrls()[0], "about:blank", "the remaining tab should be blank");
  ok(
    !JSON.stringify(currentWindowState()).includes(PRIVATE_URL),
    "the all-private state must not leak its URL",
  );
});

add_task(async function privateOnlyWindowIsDroppedWhenANormalWindowSurvives() {
  await restoreWindowState({
    windows: [
      {
        title: PRIVATE_URL,
        tabs: [
          makeTabState(PRIVATE_URL, { floorpDisableHistory: "true" }),
        ],
        selected: 1,
        _closedTabs: [],
        closedGroups: [],
      },
      {
        tabs: [makeTabState(NORMAL_A)],
        selected: 1,
        _closedTabs: [],
        closedGroups: [],
      },
    ],
    selectedWindow: 1,
  });

  const windows = [];
  for (const win of Services.wm.getEnumerator("navigator:browser")) {
    windows.push(win);
  }
  is(
    windows.length,
    1,
    "a private-only window should not become an extra blank",
  );
  Assert.deepEqual(
    restoredUrls(),
    [NORMAL_A],
    "the surviving normal window should be restored into the existing window",
  );
  ok(
    !JSON.stringify(SessionStore.getBrowserState()).includes(PRIVATE_URL),
    "the removed private-only window must not remain in serialized state",
  );
});

add_task(function directTabbrowserCallerReceivesASanitizedPlaceholder() {
  const tabDataList = [
    makeTabState(PRIVATE_URL, {
      floorpDisableHistory: "true",
      floorpWorkspaceId: "private-workspace",
      floorpLastShowWorkspaceId: "private-workspace",
      floorpSSB: "true",
      pinned: true,
      userContextId: 5,
      groupId: "direct-group",
    }),
    makeTabState(NORMAL_B),
  ];
  const sessionRestoreBrowser = /** @type {SessionRestoreGBrowser} */ (
    /** @type {unknown} */ (gBrowser)
  );
  const tabs = sessionRestoreBrowser.createTabsForSessionRestore(
    true,
    1,
    tabDataList,
    [
      {
        id: "direct-group",
        name: "Direct group",
        color: "blue",
        collapsed: false,
      },
    ],
    [],
  );

  try {
    is(tabs.length, 2, "the direct caller should keep array cardinality");
    ok(
      tabs[0].selected,
      "a selected private slot should become selected blank",
    );
    Assert.deepEqual(
      tabDataList[0].entries,
      [],
      "the private history should be replaced with a blank state",
    );
    ok(
      !("floorpDisableHistory" in tabDataList[0]),
      "the private marker should not reach restoreTabs",
    );
    ok(!tabs[0].pinned, "private pinned state must not be retained");
    is(tabs[0].userContextId, 0, "private container identity must be removed");
    // The direct caller path does not run inside the SessionStore restore
    // window, so the workspaces service may already have assigned its current
    // workspace to the placeholder tab. What must hold is that the private
    // workspace identity from the sanitized state never survives.
    isnot(
      tabs[0].getAttribute("floorpWorkspaceId"),
      "private-workspace",
      "the private workspace identity must not be retained",
    );
    ok(
      !tabs[0].hasAttribute("floorpSSB"),
      "private SSB metadata must be removed",
    );
    is(
      tabDataList[1].entries[0].url,
      NORMAL_B,
      "normal direct-caller state should stay in its original slot",
    );
  } finally {
    // A browser window must keep one tab alive. The last returned tab is
    // therefore intentionally left for the next task/cleanup to replace.
    for (const tab of tabs.slice(0, -1)) {
      BrowserTestUtils.removeTab(tab);
    }
  }
});

add_task(async function reusedSelectedTabClearsAbsentFloorpMetadata() {
  Services.prefs.setStringPref(WORKSPACE_STORE_PREF, "{not-json");
  const selectedTab = /** @type {SessionRestoreTab} */ (
    /** @type {unknown} */ (
      await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank")
    )
  );
  selectedTab.setAttribute("floorpWorkspaceId", "stale-workspace");
  selectedTab.setAttribute(
    "floorpWorkspaceLastShowId",
    "stale-last-show-workspace",
  );
  selectedTab.setAttribute("floorpSSB", "stale-ssb");

  const sessionRestoreBrowser = /** @type {SessionRestoreGBrowser} */ (
    /** @type {unknown} */ (gBrowser)
  );
  const tabs = sessionRestoreBrowser.createTabsForSessionRestore(
    true,
    1,
    [
      makeTabState("about:blank", {
        userContextId: selectedTab.userContextId,
      }),
    ],
    [],
    [],
  );

  try {
    is(tabs[0], selectedTab, "the existing selected tab should be reused");
    for (
      const attribute of [
        "floorpWorkspaceId",
        "floorpWorkspaceLastShowId",
        "floorpSSB",
      ]
    ) {
      ok(
        !selectedTab.hasAttribute(attribute),
        `${attribute} should be removed when it is absent from restored state`,
      );
    }
  } finally {
    selectedTab.removeAttribute("floorpWorkspaceId");
    selectedTab.removeAttribute("floorpWorkspaceLastShowId");
    selectedTab.removeAttribute("floorpSSB");
    BrowserTestUtils.removeTab(selectedTab);
  }
});
