// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

/**
 * End-to-end check: merges real browser windows through the real tabbrowser
 * adoption path, including closing the emptied windows.
 */

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import {
  listBrowserWindows,
  mergeAllWindows,
  type MergeTabBrowser,
} from "../controller.ts";

interface TestBrowser extends MergeTabBrowser {
  addTab(url: string, options: { triggeringPrincipal: unknown }): XULElement;
  removeTab(tab: XULElement): void;
}

interface TestWindow extends Window {
  gBrowser?: TestBrowser;
  gBrowserInit?: { delayedStartupFinished?: boolean };
}

function ownWindow(): TestWindow {
  return globalThis as unknown as TestWindow;
}

function browserOf(window: TestWindow, label: string): TestBrowser {
  const browser = window.gBrowser;
  assert(browser, `browser window should expose gBrowser (${label})`);
  return browser;
}

function openBrowserWindow(): TestWindow {
  const open = (globalThis as unknown as { OpenBrowserWindow(): Window })
    .OpenBrowserWindow;
  assert(typeof open === "function", "OpenBrowserWindow should be available");
  return open() as TestWindow;
}

async function waitFor(check: () => boolean, label: string): Promise<void> {
  for (let attempt = 0; attempt < 300; attempt++) {
    if (check()) return;
    await new Promise((resolve) => setTimeout(resolve, 50));
  }
  throw new Error(`Timed out waiting for: ${label}`);
}

/** Opens a second window whose tab strip holds at least `tabCount` tabs. */
async function openWindowWithTabs(tabCount: number): Promise<TestWindow> {
  const window = openBrowserWindow();
  await waitFor(() => !!window.gBrowser, "second window exposes gBrowser");
  // A window adds its session/newtab tabs during delayed startup; adding tabs
  // before that lands an unpredictable count in the middle of the merge.
  await waitFor(
    () => window.gBrowserInit?.delayedStartupFinished === true,
    "second window finishes delayed startup",
  );

  const browser = browserOf(window, "second window");
  const triggeringPrincipal = Services.scriptSecurityManager
    .getSystemPrincipal();

  while (browser.tabs.length < tabCount) {
    browser.addTab("about:blank", { triggeringPrincipal });
  }
  await waitFor(
    () => (window.gBrowser?.tabs.length ?? 0) >= tabCount,
    "second window reaches the requested tab count",
  );
  return window;
}

/** Source windows the merge should consume: every other open browser window. */
function sourceWindowCount(target: Window): number {
  return listBrowserWindows().filter(
    (window) => window !== target && !window.closed,
  ).length;
}

/** Drops everything the merge appended to the test window. */
async function removeTabsAddedSince(tabCountBefore: number): Promise<void> {
  const browser = browserOf(ownWindow(), "test window");
  const added = Array.from(browser.tabs).slice(tabCountBefore);
  for (const tab of added) {
    browser.removeTab(tab as XULElement);
  }
  await waitFor(
    () => browserOf(ownWindow(), "test window").tabs.length <= tabCountBefore,
    "moved tabs are removed again",
  );
}

async function testMergesRealBrowserWindows(): Promise<void> {
  const ownBrowser = browserOf(ownWindow(), "test window");
  const otherWindow = await openWindowWithTabs(3);

  // Delayed startup may still add tabs, so the baseline is taken right before
  // the (synchronous) merge rather than before opening the window.
  const tabsBefore = ownBrowser.tabs.length;
  try {
    const expectedSources = sourceWindowCount(globalThis as unknown as Window);

    const result = mergeAllWindows(globalThis as unknown as Window);

    assert(result.movedTabs >= 3, "the requested tabs moved");
    assertEquals(
      result.failedItems,
      0,
      `no tab was refused (sources=${expectedSources}, moved=${result.movedTabs}, closed=${result.closedWindows})`,
    );
    assertEquals(result.skippedWindows, 0, "no window skipped");
    assertEquals(
      result.closedWindows,
      expectedSources,
      "every emptied source window was closed",
    );
    assert(otherWindow.closed, "the source window should be closed");
    assertEquals(
      sourceWindowCount(globalThis as unknown as Window),
      0,
      "the target window is the only browser window left",
    );
    assertEquals(
      ownBrowser.tabs.length,
      tabsBefore + result.movedTabs,
      "the target window gained exactly the moved tabs",
    );
  } finally {
    if (!otherWindow.closed) otherWindow.close();
    await removeTabsAddedSince(tabsBefore);
  }
}

async function testSecondMergeIsANoOp(): Promise<void> {
  const ownBrowser = browserOf(ownWindow(), "test window");
  const otherWindow = await openWindowWithTabs(2);
  const tabsBefore = ownBrowser.tabs.length;
  try {
    const first = mergeAllWindows(globalThis as unknown as Window);
    assert(first.movedTabs >= 2, "first merge moves the opened tabs");
    assertEquals(first.failedItems, 0, "no tab was refused");
    assert(otherWindow.closed, "source window closed by the first merge");
    assertEquals(
      sourceWindowCount(globalThis as unknown as Window),
      0,
      "only the target window is left",
    );

    const second = mergeAllWindows(globalThis as unknown as Window);
    assertEquals(second.movedTabs, 0, "nothing left to move");
    assertEquals(second.skippedWindows, 0, "no browser window left to skip");
    assertEquals(second.closedWindows, 0, "no window closed");
  } finally {
    if (!otherWindow.closed) otherWindow.close();
    await removeTabsAddedSince(tabsBefore);
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "merges real browser windows into the calling window",
      fn: testMergesRealBrowserWindows,
    },
    { name: "second merge is a no-op", fn: testSecondMergeIsANoOp },
  ];
  await runTests("mergeRealWindows.test.ts", tests);
}
