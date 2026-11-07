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
  "moz-src:///browser/components/sessionstore/SessionStore.sys.mjs",
);
const { ContextualIdentityService } = ChromeUtils.importESModule(
  "moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs",
);

const NORMAL_URL = "data:text/plain,floorp-sessionstore-closed-normal";
const PRIVATE_URL = "data:text/plain,floorp-sessionstore-closed-private";

const browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
/** @type {unknown} */
let originalWindowState;
/** @type {unknown} */
let originalClosedWindows;
/** @type {number[]} */
const ownedUserContextIds = [];

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
  SessionStore.setWindowState(
    browserWindow,
    typeof state === "string" ? state : JSON.stringify(state),
    true,
  );
  await restored;
}

function privateUserContextId() {
  ContextualIdentityService.ensureDataReady();
  const identity = ContextualIdentityService._identities.find(
    /** @param {{ floorpPrivateContainer?: boolean }} candidate */
    (candidate) => candidate.floorpPrivateContainer === true,
  );
  if (
    !Number.isInteger(identity?.userContextId) || identity.userContextId <= 0
  ) {
    throw new Error("A real private-container identity is required");
  }
  return identity.userContextId;
}

function createClearContextId() {
  const identity = ContextualIdentityService.create(
    `floorp-sessionstore-clear-${crypto.randomUUID()}`,
    "fingerprint",
    "blue",
  );
  ownedUserContextIds.push(identity.userContextId);
  return identity.userContextId;
}

/** @param {number} userContextId */
async function clearUserContextData(userContextId) {
  let matchingNotification = false;
  const observer = {
    QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
    /**
     * @param {nsISupports | null} _subject
     * @param {string} _topic
     * @param {string} data
     */
    observe(_subject, _topic, data) {
      if (JSON.parse(data).userContextId === userContextId) {
        matchingNotification = true;
      }
    },
  };
  Services.obs.addObserver(observer, "clear-origin-attributes-data");
  try {
    const failedFlags = await new Promise((resolve) => {
      Services.clearData.deleteDataFromOriginAttributesPattern(
        { userContextId },
        resolve,
      );
    });
    is(failedFlags, 0, "the real native clear operation should complete");
    ok(matchingNotification, "the matching production clear event should fire");
  } finally {
    Services.obs.removeObserver(observer, "clear-origin-attributes-data");
  }
}

registerCleanupFunction(async function restoreOriginalState() {
  /** @type {unknown[]} */
  const errors = [];
  try {
    if (originalWindowState) {
      const state = typeof originalWindowState === "string"
        ? JSON.parse(originalWindowState)
        : JSON.parse(JSON.stringify(originalWindowState));
      state._closedWindows = originalClosedWindows;
      await restoreWindowState(state);
    }
  } catch (error) {
    errors.push(error);
  }
  for (const userContextId of ownedUserContextIds) {
    try {
      ContextualIdentityService.remove(userContextId);
    } catch (error) {
      errors.push(error);
    }
  }
  const cleared = await Promise.allSettled(
    ownedUserContextIds.map(clearUserContextData),
  );
  await new Promise((resolve) => setTimeout(resolve, 450));
  for (const result of cleared) {
    if (result.status === "rejected") {
      errors.push(result.reason);
    }
  }
  if (errors.length) {
    throw new AggregateError(errors);
  }
});

add_task(async function closedPrivateContainerTabIsNotRecorded() {
  originalWindowState = SessionStore.getWindowState(browserWindow);
  originalClosedWindows = JSON.parse(SessionStore.getBrowserState())
    ._closedWindows;
  const livePrivateUrl = `about:mozilla#floorp-private-${crypto.randomUUID()}`;
  const tab = /** @type {XULElement} */ (
    await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      livePrivateUrl,
      true,
    )
  );
  tab.setAttribute("floorp-disablehistory", "true");
  const tabClosed = BrowserTestUtils.waitForEvent(tab, "TabClose");
  BrowserTestUtils.removeTab(tab);
  await tabClosed;

  const closedTabs = SessionStore.getClosedTabDataForWindow(browserWindow);
  ok(
    !JSON.stringify(closedTabs).includes(livePrivateUrl),
    "the closed private-container URL should not be recorded",
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

  await clearUserContextData(privateUserContextId());
  const afterClear = JSON.parse(SessionStore.getBrowserState());
  is(
    afterClear._closedWindows.length,
    2,
    "a real private clear must retain both normal closed-window records",
  );
  const retained = afterClear._closedWindows.find(
    /** @param {{ tabs: unknown[] }} state */
    (state) => state.tabs.length === 0,
  );
  is(
    retained._closedTabs.length,
    1,
    "normal closed tabs survive private clear",
  );
  is(
    retained.closedGroups.length,
    1,
    "normal closed groups survive private clear",
  );
  ok(
    !JSON.stringify(afterClear._closedWindows).includes(PRIVATE_URL),
    "private data stays absent after the real clear event",
  );
});

add_task(async function containerClearPreservesOnlyUnrelatedClosedData() {
  const userContextId = createClearContextId();
  const cleared = { userContextId };
  const liveMixed = makeClosedGroup("live-mixed", [
    makeClosedTab(NORMAL_URL, { splitViewId: 701 }),
    makeClosedTab(PRIVATE_URL, { ...cleared, splitViewId: 701 }),
  ]);
  liveMixed.splitViews = [{ id: 701, numberOfTabs: 2 }];
  const closedMixed = makeClosedGroup("groups-only-mixed", [
    makeClosedTab(NORMAL_URL, { splitViewId: 702 }),
    makeClosedTab(PRIVATE_URL, { ...cleared, splitViewId: 702 }),
  ]);
  closedMixed.splitViews = [{ id: 702, numberOfTabs: 2 }];
  await restoreWindowState({
    windows: [{
      tabs: [makeTabState(NORMAL_URL)],
      selected: 1,
      _closedTabs: [
        makeClosedTab(NORMAL_URL),
        makeClosedTab(PRIVATE_URL, cleared),
      ],
      closedGroups: [
        liveMixed,
        makeClosedGroup("live-matching", [makeClosedTab(PRIVATE_URL, cleared)]),
      ],
    }],
    _closedWindows: [
      {
        title: PRIVATE_URL,
        tabs: [makeTabState(PRIVATE_URL, cleared)],
        selected: 1,
        _closedTabs: [
          makeClosedTab(NORMAL_URL),
          makeClosedTab(PRIVATE_URL, cleared),
        ],
        closedGroups: [
          makeClosedGroup("tabs-only-matching", [
            makeClosedTab(PRIVATE_URL, cleared),
          ]),
        ],
      },
      {
        title: PRIVATE_URL,
        tabs: [makeTabState(PRIVATE_URL, cleared)],
        selected: 1,
        _closedTabs: [makeClosedTab(PRIVATE_URL, cleared)],
        closedGroups: [
          closedMixed,
          makeClosedGroup("groups-only-matching", [
            makeClosedTab(PRIVATE_URL, cleared),
          ]),
        ],
      },
      {
        title: PRIVATE_URL,
        tabs: [makeTabState(PRIVATE_URL, cleared)],
        selected: 1,
        _closedTabs: [makeClosedTab(PRIVATE_URL, cleared)],
        closedGroups: [
          makeClosedGroup("all-matching", [
            makeClosedTab(PRIVATE_URL, cleared),
          ]),
        ],
      },
    ],
  });
  is(
    JSON.parse(SessionStore.getBrowserState())._closedWindows.length,
    3,
    "all three records must exist before clearing their actual user context",
  );
  await clearUserContextData(userContextId);
  const state = JSON.parse(SessionStore.getBrowserState());
  is(
    state._closedWindows.length,
    2,
    "only the entirely matching window is dropped",
  );
  const tabsOnly = state._closedWindows.find(
    /** @param {{ _closedTabs: unknown[] }} value */
    (value) => value._closedTabs.length === 1,
  );
  const groupsOnly = state._closedWindows.find(
    /** @param {{ closedGroups: Array<{ id: string }> }} value */
    (value) =>
      value.closedGroups.some((group) => group.id === "groups-only-mixed"),
  );
  is(
    tabsOnly.tabs.length,
    0,
    "normal closed tabs alone keep their window alive",
  );
  is(
    tabsOnly.closedGroups.length,
    0,
    "matching-only closed groups are removed",
  );
  is(
    groupsOnly.tabs.length,
    0,
    "normal closed groups alone keep their window alive",
  );
  is(groupsOnly._closedTabs.length, 0, "matching closed tabs are removed");
  is(groupsOnly.closedGroups.length, 1, "the matching-only group is dropped");
  is(
    groupsOnly.closedGroups[0].tabs.length,
    1,
    "mixed groups retain only normal tabs",
  );
  is(
    groupsOnly.closedGroups[0].splitViews.length,
    0,
    "closed-window groups lose orphan split metadata",
  );
  for (const closedWindow of state._closedWindows) {
    is(
      closedWindow.selected,
      0,
      "closed-data-only windows have no tab selection",
    );
    ok(!("title" in closedWindow), "cleared open-tab titles are removed");
  }
  const liveState = SessionStore.getInternalObjectState(browserWindow);
  is(liveState._closedTabs.length, 1, "live-window normal closed tabs survive");
  is(
    liveState.closedGroups.length,
    1,
    "live-window empty matching groups are removed",
  );
  is(
    liveState.closedGroups[0].tabs.length,
    1,
    "live-window mixed groups retain normal data",
  );
  is(
    liveState.closedGroups[0].splitViews.length,
    0,
    "live-window groups lose orphan split metadata",
  );
  ok(
    !("splitViewId" in liveState.closedGroups[0].tabs[0].state),
    "normal closed group tabs have no orphan split reference",
  );
  ok(
    !JSON.stringify({ closed: state._closedWindows, live: liveState }).includes(
      PRIVATE_URL,
    ),
    "no matching tab, group entry or window title survives the actual clear",
  );
});

add_task(async function containerClearRemapsClosedWindowMetadata() {
  const userContextId = createClearContextId();
  await restoreWindowState({
    windows: [{
      tabs: [makeTabState(NORMAL_URL)],
      selected: 1,
      _closedTabs: [],
      closedGroups: [],
    }],
    _closedWindows: [{
      title: PRIVATE_URL,
      tabs: [
        makeTabState(NORMAL_URL, { groupId: "mixed-open", splitViewId: 700 }),
        makeTabState(PRIVATE_URL, {
          userContextId,
          groupId: "mixed-open",
          splitViewId: 700,
        }),
        makeTabState(PRIVATE_URL, { userContextId, groupId: "matching-open" }),
      ],
      selected: 3,
      groups: [
        { id: "mixed-open", name: "mixed", color: "blue", collapsed: false },
        {
          id: "matching-open",
          name: PRIVATE_URL,
          color: "red",
          collapsed: false,
        },
      ],
      splitViews: [{ id: 700, numberOfTabs: 2 }],
      _closedTabs: [],
      closedGroups: [],
    }],
  });
  await clearUserContextData(userContextId);
  const state = JSON.parse(SessionStore.getBrowserState());
  is(
    state._closedWindows.length,
    1,
    "the normal surviving tab keeps its window alive",
  );
  const record = state._closedWindows[0];
  is(record.tabs.length, 1, "all matching open-tab states are removed");
  is(record.selected, 1, "selection is remapped to the normal surviving tab");
  is(
    record.title,
    NORMAL_URL,
    "the title is recomputed without a matching URI",
  );
  is(
    record.groups.length,
    1,
    "the entirely matching open group metadata is removed",
  );
  is(record.groups[0].id, "mixed-open", "the normal open group is preserved");
  is(
    record.splitViews.length,
    0,
    "the now single-tab split metadata is removed",
  );
  ok(
    !("splitViewId" in record.tabs[0]),
    "the surviving tab has no orphan split reference",
  );
  ok(
    !JSON.stringify(record).includes(PRIVATE_URL),
    "matching data is absent from every retained closed-window field",
  );
});

add_task(async function containerClearKeepsRecentCloseBookkeeping() {
  const userContextId = createClearContextId();
  const matching = { userContextId };
  const normalA = () => makeClosedTab(`${NORMAL_URL}-recent-a`);
  const normalB = () => makeClosedTab(`${NORMAL_URL}-recent-b`);
  const matchingTab = () => makeClosedTab(PRIVATE_URL, matching);
  /** @param {string} id */
  function matchingGroup(id) {
    return makeClosedGroup(id, [matchingTab(), matchingTab()]);
  }
  /** @param {string} id */
  function normalGroup(id) {
    return makeClosedGroup(id, [normalA(), normalB()]);
  }
  const cases = [
    {
      name: "older matching lists preserve the latest standalone pair",
      tabs: [normalA(), normalB(), matchingTab(), matchingTab()],
      groups: [matchingGroup("older-matching")],
      lastGroupId: null,
      count: 2,
      expectedStored: 2,
      expectedCount: 2,
      expectedGroupId: null,
    },
    {
      name: "older matching standalone tabs preserve the latest normal group",
      tabs: [matchingTab(), matchingTab()],
      groups: [normalGroup("latest-normal")],
      lastGroupId: "latest-normal",
      count: 2,
      expectedStored: 2,
      expectedCount: 2,
      expectedGroupId: "latest-normal",
    },
    {
      name: "only matching entries of the latest group reduce its count",
      tabs: [matchingTab()],
      groups: [
        makeClosedGroup("latest-mixed", [normalA(), matchingTab(), normalB()]),
        matchingGroup("older-matching"),
      ],
      lastGroupId: "latest-mixed",
      count: 3,
      expectedStored: 2,
      expectedCount: 2,
      expectedGroupId: "latest-mixed",
    },
    {
      name:
        "only matching entries of the latest standalone batch reduce its count",
      tabs: [normalA(), matchingTab(), normalB()],
      groups: [matchingGroup("older-matching")],
      lastGroupId: null,
      count: 3,
      expectedStored: 2,
      expectedCount: 2,
      expectedGroupId: null,
    },
    {
      name: "fully clearing the latest group resets its recent-close metadata",
      tabs: [normalA(), normalB()],
      groups: [matchingGroup("latest-matching")],
      lastGroupId: "latest-matching",
      count: 2,
      expectedStored: -1,
      expectedCount: 1,
      expectedGroupId: null,
    },
    {
      name:
        "a nonlocal group reference is not reset by unrelated local cleanup",
      tabs: [matchingTab(), matchingTab()],
      groups: [matchingGroup("older-matching"), normalGroup("older-normal")],
      lastGroupId: "nonlocal-saved-group-reference",
      count: 2,
      expectedStored: 2,
      expectedCount: 2,
      expectedGroupId: "nonlocal-saved-group-reference",
    },
  ];
  for (const testCase of cases) {
    await restoreWindowState({
      windows: [{
        tabs: [makeTabState(NORMAL_URL)],
        selected: 1,
        _closedTabs: testCase.tabs,
        closedGroups: testCase.groups,
        _lastClosedTabGroupCount: testCase.count,
        lastClosedTabGroupId: testCase.lastGroupId,
      }],
      _closedWindows: [],
    });
    is(
      SessionStore.getLastClosedTabCount(browserWindow),
      testCase.count,
      `${testCase.name}: the native API reads the seeded recent-close batch`,
    );
    await clearUserContextData(userContextId);
    const state = SessionStore.getInternalObjectState(browserWindow);
    is(
      state._lastClosedTabGroupCount,
      testCase.expectedStored,
      `${testCase.name}: the stored count belongs to the correct list`,
    );
    is(
      SessionStore.getLastClosedTabCount(browserWindow),
      testCase.expectedCount,
      `${testCase.name}: the public Undo Close count remains correct`,
    );
    is(
      SessionStore.getLastClosedTabGroupId(browserWindow),
      testCase.expectedGroupId,
      `${testCase.name}: the public recent group ID remains correct`,
    );
    is(
      SessionStore.getClosedTabCountForWindow(browserWindow),
      2,
      `${testCase.name}: both unrelated normal closed tabs remain`,
    );
    ok(
      !JSON.stringify({ tabs: state._closedTabs, groups: state.closedGroups })
        .includes(PRIVATE_URL),
      `${testCase.name}: all matching closed data is still removed`,
    );
  }
});
