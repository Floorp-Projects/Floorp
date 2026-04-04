// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

type TestCase = { name: string; fn: () => void | Promise<void> };

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) throw new Error(message);
}

function assertEquals<T>(actual: T, expected: T, message: string): void {
  if (actual !== expected)
    throw new Error(
      `${message} (expected: ${String(expected)}, actual: ${String(actual)})`,
    );
}

function testGBrowserDefined(): void {
  assert(
    typeof gBrowser !== "undefined" && gBrowser !== null,
    "gBrowser should be defined",
  );
}

function testTabsArrayLikeWithAtLeastOne(): void {
  assert(gBrowser.tabs !== null && gBrowser.tabs !== undefined, "gBrowser.tabs should exist");
  assert(gBrowser.tabs.length >= 1, `gBrowser.tabs should have at least 1 tab (got ${gBrowser.tabs.length})`);
}

function testSelectedTabNotNull(): void {
  assert(
    gBrowser.selectedTab !== null && gBrowser.selectedTab !== undefined,
    "gBrowser.selectedTab should not be null",
  );
}

function testSelectedBrowserHasCurrentURI(): void {
  const browser = gBrowser.selectedBrowser;
  assert(browser !== null && browser !== undefined, "gBrowser.selectedBrowser should exist");
  assert(
    "currentURI" in browser,
    "gBrowser.selectedBrowser should have a currentURI property",
  );
}

async function testOpenAndCloseTab(): Promise<void> {
  const initialCount = gBrowser.tabs.length;

  const newTab = gBrowser.addTab("about:blank", {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });
  assert(newTab !== null, "addTab should return a new tab");

  const afterOpenCount = gBrowser.tabs.length;
  assertEquals(
    afterOpenCount,
    initialCount + 1,
    "Tab count should increase by 1 after addTab",
  );

  gBrowser.removeTab(newTab);

  const afterCloseCount = gBrowser.tabs.length;
  assertEquals(
    afterCloseCount,
    initialCount,
    "Tab count should return to initial after removeTab",
  );
}

function testTabHasLinkedBrowser(): void {
  const tab = gBrowser.selectedTab;
  assert(tab !== null, "selectedTab should exist");
  assert(
    tab.linkedBrowser !== null && tab.linkedBrowser !== undefined,
    "selectedTab.linkedBrowser should exist",
  );
}

function testSelectedTabIsVisible(): void {
  const tab = gBrowser.selectedTab;
  assert(tab !== null, "selectedTab should exist");

  const rect = tab.getBoundingClientRect();
  assert(
    rect.width > 0,
    `selectedTab should have positive width (got ${rect.width})`,
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "gBrowser is defined", fn: testGBrowserDefined },
    { name: "gBrowser.tabs has at least 1 tab", fn: testTabsArrayLikeWithAtLeastOne },
    { name: "selectedTab is not null", fn: testSelectedTabNotNull },
    { name: "selectedBrowser has currentURI", fn: testSelectedBrowserHasCurrentURI },
    { name: "open and close tab", fn: testOpenAndCloseTab },
    { name: "tab has linkedBrowser", fn: testTabHasLinkedBrowser },
    { name: "selected tab is visible", fn: testSelectedTabIsVisible },
  ];

  const failures: string[] = [];
  for (const test of tests) {
    try {
      await test.fn();
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      failures.push(`${test.name}: ${message}`);
    }
  }
  if (failures.length > 0)
    throw new Error(
      `tabOperations.test.ts failures: ${failures.join(" | ")}`,
    );
}
