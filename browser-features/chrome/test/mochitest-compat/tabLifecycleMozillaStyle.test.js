// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
// @ts-check
/// <reference path="../../@types/mochitest-compat.d.ts" />

const TEST_URL = "about:blank";
let expectedCountAfterNoAwaitRemovals = -1;

/**
 * @param {() => boolean} predicate
 * @param {number} [timeoutMs]
 */
async function pollCondition(predicate, timeoutMs = 2000) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    if (predicate()) {
      return true;
    }
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  return predicate();
}

/**
 * @param {() => boolean} predicate
 * @param {string} message
 * @param {number} [timeoutMs]
 * @param {() => string} [details]
 */
async function waitForCondition(predicate, message, timeoutMs = 2000, details) {
  ok(
    await pollCondition(predicate, timeoutMs),
    details ? `${message} (${details()})` : message,
  );
}

function tabApiState() {
  const tabs = Array.from(gBrowser.tabs, (tab, index) => {
    const tabWithState = /** @type {XULElement & { closing?: boolean }} */ (
      tab
    );
    const browser = browserForTab(tab);
    return [
      `${index}:${browser?.currentURI?.spec ?? "unknown"}`,
      tab === gBrowser.selectedTab ? "selected" : "background",
      tab.hidden ? "hidden" : "visible",
      tab.pinned ? "pinned" : "unpinned",
      tabWithState.closing ? "closing" : "open",
    ].join(":");
  });
  return [
    `addTab=${typeof gBrowser.addTab}`,
    `removeTab=${typeof gBrowser.removeTab}`,
    `tabs=${gBrowser.tabs?.length ?? "missing"}`,
    `state=[${tabs.join(", ")}]`,
  ].join(", ");
}

async function waitForBrowserChromeReady() {
  const globals = /** @type {Record<string, unknown>} */ (globalThis);
  const startup =
    /** @type {{ delayedStartupPromise?: Promise<unknown> } | undefined} */ (
      globals.gBrowserInit
    );
  await startup?.delayedStartupPromise;

  await waitForCondition(
    () =>
      typeof gBrowser.addTab === "function" &&
      typeof gBrowser.removeTab === "function",
    "gBrowser tab mutation APIs should be available after delayed startup",
    10000,
    tabApiState,
  );
}

/**
 * @param {XULElement} target
 */
function hasTab(target) {
  for (const tab of gBrowser.tabs) {
    if (tab === target) {
      return true;
    }
  }
  return false;
}

function currentTabs() {
  return Array.from(gBrowser.tabs);
}

/**
 * @param {XULElement[]} previousTabs
 */
function findNewTab(previousTabs) {
  for (const tab of currentTabs()) {
    if (!previousTabs.includes(tab)) {
      return tab;
    }
  }
  return null;
}

/**
 * @param {XULElement} tab
 */
function browserForTab(tab) {
  const tabWithBrowser =
    /** @type {{ linkedBrowser?: { currentURI?: { spec?: string } } }} */ (
      tab
    );
  if (tabWithBrowser.linkedBrowser) {
    return tabWithBrowser.linkedBrowser;
  }

  try {
    return gBrowser.getBrowserForTab(tab);
  } catch {
    return undefined;
  }
}

add_task(async function browserStartsWithCleanSingleTab() {
  await waitForBrowserChromeReady();

  is(
    gBrowser.tabs.length,
    1,
    "Mozilla-style browser tests should start with one clean tab",
  );
  is(
    gBrowser.currentURI?.spec,
    TEST_URL,
    "Mozilla-style browser tests should start on about:blank",
  );
});

add_task(async function activeSplitViewUsesNativePrivateReceiver() {
  await waitForBrowserChromeReady();

  const activeSplitView = /** @type {{ activeSplitView?: unknown }} */ (
    gBrowser
  ).activeSplitView;
  ok(
    activeSplitView === null || typeof activeSplitView === "object",
    "gBrowser.activeSplitView should be readable with its native private receiver",
  );
});

add_task(
  async function browserBug565575PreservesUrlbarFocusAcrossTabSwitches() {
    await waitForBrowserChromeReady();

    const initialTab = gBrowser.selectedTab;
    /** @type {XULElement | null} */
    let openedTab = null;
    const restoreTabs = async () => {
      if (openedTab && hasTab(openedTab)) {
        await BrowserTestUtils.removeTab(openedTab);
        openedTab = null;
      }
      if (
        initialTab && hasTab(initialTab) &&
        gBrowser.selectedTab !== initialTab
      ) {
        await BrowserTestUtils.switchTab(gBrowser, initialTab);
      }
    };
    registerCleanupFunction(restoreTabs);

    try {
      const selectedBrowser = /** @type {{ focus(): void }} */ (
        /** @type {unknown} */ (gBrowser.selectedBrowser)
      );
      selectedBrowser.focus();

      openedTab = /** @type {XULElement} */ (
        await BrowserTestUtils.openNewForegroundTab(
          gBrowser,
          () => BrowserCommands.openTab(),
          false,
        )
      );
      ok(gURLBar.focused, "location bar is focused for a new tab");

      await BrowserTestUtils.switchTab(gBrowser, initialTab);
      ok(
        !gURLBar.focused,
        "location bar isn't focused for the previously selected tab",
      );

      await BrowserTestUtils.switchTab(gBrowser, openedTab);
      ok(
        gURLBar.focused,
        "location bar is re-focused when selecting the new tab",
      );
    } finally {
      await restoreTabs();
    }
  },
);

add_task(async function openTabAndCloseIt() {
  await waitForBrowserChromeReady();

  const initialCount = gBrowser.tabs.length;
  const initialSelectedTab = gBrowser.selectedTab;
  const initialTabs = currentTabs();

  /** @type {XULElement | null} */
  let openedTab = gBrowser.addTab(TEST_URL, {
    inBackground: false,
    skipAnimation: true,
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  }) ?? null;

  registerCleanupFunction(() => {
    if (openedTab && hasTab(openedTab) && browserForTab(openedTab)) {
      gBrowser.removeTab(openedTab);
      openedTab = null;
    }
    if (initialSelectedTab && hasTab(initialSelectedTab)) {
      gBrowser.selectedTab = initialSelectedTab;
    }
  });

  if (!openedTab) {
    await waitForCondition(
      () => gBrowser.tabs.length === initialCount + 1,
      "gBrowser.addTab should add a tab even when it does not return one",
      3000,
      tabApiState,
    );
    openedTab = findNewTab(initialTabs);
  }

  if (!openedTab) {
    throw new Error(`gBrowser.addTab should create a tab (${tabApiState()})`);
  }
  gBrowser.selectedTab = openedTab;

  const selectedOpenedTab = await pollCondition(
    () => gBrowser.selectedTab === openedTab,
    2000,
  );
  if (!selectedOpenedTab) {
    info("opened tab was not selected in this test runtime");
  }

  is(
    gBrowser.tabs.length,
    initialCount + 1,
    "opening a tab should increase tab count",
  );

  await waitForCondition(
    () => Boolean(openedTab && browserForTab(openedTab)),
    "opened tab should expose a linked browser",
    5000,
    tabApiState,
  );
  const browser = browserForTab(openedTab);
  if (!browser) {
    throw new Error("opened tab browser should be available");
  }

  await waitForCondition(
    () => browser.currentURI?.spec === TEST_URL,
    "new tab browser should reach the requested URI",
    5000,
  );
  is(browser.currentURI?.spec, TEST_URL, "new tab should load about:blank");

  const rect = openedTab.getBoundingClientRect();
  ok(rect.width > 0, "selected tab should be visible in browser chrome");

  gBrowser.removeTab(openedTab);
  openedTab = null;

  await waitForCondition(
    () => gBrowser.tabs.length === initialCount,
    "closing the tab should restore the initial tab count",
  );
});

add_task(async function browserTestUtilsAddTabReturnsTab() {
  await waitForBrowserChromeReady();

  const initialCount = gBrowser.tabs.length;
  /** @type {XULElement | null} */
  let tab = /** @type {XULElement} */ (
    BrowserTestUtils.addTab(gBrowser, TEST_URL)
  );

  registerCleanupFunction(async () => {
    if (tab && hasTab(tab)) {
      await BrowserTestUtils.removeTab(tab);
      tab = null;
    }
  });

  ok(tab, "BrowserTestUtils.addTab should return a tab synchronously");
  ok(
    typeof /** @type {unknown & { then?: unknown }} */ (tab).then !==
      "function",
    "BrowserTestUtils.addTab should not return a promise",
  );
  await BrowserTestUtils.switchTab(gBrowser, tab);
  await waitForCondition(
    () => Boolean(tab && browserForTab(tab)),
    "BrowserTestUtils.addTab tab should expose a linked browser",
    5000,
    tabApiState,
  );
  is(
    gBrowser.tabs.length,
    initialCount + 1,
    "addTab should increase tab count",
  );

  await BrowserTestUtils.removeTab(tab);
  tab = null;
  await waitForCondition(
    () => gBrowser.tabs.length === initialCount,
    "removeTab should restore the initial tab count",
  );
});

add_task(
  async function browserTestUtilsRemovesHiddenAndPinnedTabsWithoutAwait() {
    await waitForBrowserChromeReady();

    expectedCountAfterNoAwaitRemovals = gBrowser.tabs.length;
    const startingTab = gBrowser.selectedTab;
    /** @type {XULElement | null} */
    let hiddenTab = /** @type {XULElement} */ (
      BrowserTestUtils.addTab(gBrowser, "data:text/plain;hidden tab")
    );
    /** @type {XULElement | null} */
    let pinnedTab = null;

    registerCleanupFunction(() => {
      if (hiddenTab && hasTab(hiddenTab)) {
        BrowserTestUtils.removeTab(hiddenTab);
        hiddenTab = null;
      }
      if (pinnedTab && hasTab(pinnedTab)) {
        BrowserTestUtils.removeTab(pinnedTab);
        pinnedTab = null;
      }
    });

    const gBrowserWithTabMove = /** @type {{
      moveTabBefore(tab: XULElement, beforeTab: XULElement): void;
    }} */
      (/** @type {unknown} */ (gBrowser));
    gBrowserWithTabMove.moveTabBefore(hiddenTab, startingTab);
    gBrowser.hideTab(hiddenTab);
    pinnedTab = /** @type {XULElement} */ (
      BrowserTestUtils.addTab(gBrowser, "data:text/plain;pinned tab", {
        pinned: true,
      })
    );
    ok(hiddenTab.hidden, "the removal fixture should include a hidden tab");
    ok(pinnedTab.pinned, "the removal fixture should include a pinned tab");

    const hiddenResult = BrowserTestUtils.removeTab(hiddenTab);
    const pinnedResult = BrowserTestUtils.removeTab(pinnedTab);
    hiddenTab = null;
    pinnedTab = null;

    is(hiddenResult, undefined, "removeTab should synchronously return void");
    is(pinnedResult, undefined, "removeTab should not require awaiting");
  },
);

add_task(async function noAwaitTabRemovalsDrainBeforeNextTask() {
  await waitForBrowserChromeReady();

  is(
    gBrowser.tabs.length,
    expectedCountAfterNoAwaitRemovals,
    "hidden and pinned tab removals should drain before the next task",
  );
});

add_task(async function browserTestUtilsWithNewTabPassesBrowser() {
  await waitForBrowserChromeReady();

  const initialCount = gBrowser.tabs.length;
  await BrowserTestUtils.withNewTab(TEST_URL, async (browser) => {
    ok(browser, "withNewTab should pass the linked browser to the task");
    await waitForCondition(
      () =>
        /** @type {{ currentURI?: { spec?: string } }} */ (browser).currentURI
          ?.spec === TEST_URL,
      "withNewTab browser should reach the requested URI",
      5000,
    );
  });
  await waitForCondition(
    () => gBrowser.tabs.length === initialCount,
    "withNewTab should remove its temporary tab",
    5000,
    tabApiState,
  );
});
