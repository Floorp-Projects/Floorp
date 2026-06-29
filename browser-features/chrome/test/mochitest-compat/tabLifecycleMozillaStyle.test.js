// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
// @ts-check
/// <reference path="../../@types/mochitest-compat.d.ts" />

const TEST_URL = "about:blank";

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
  return [
    `addTab=${typeof gBrowser.addTab}`,
    `removeTab=${typeof gBrowser.removeTab}`,
    `tabs=${gBrowser.tabs?.length ?? "missing"}`,
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
  );
});
