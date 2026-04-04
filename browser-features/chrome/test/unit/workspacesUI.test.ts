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

// ---------------------------------------------------------------------------
// Tests — gBrowser.tabs basics
// ---------------------------------------------------------------------------

function testGBrowserTabsExists(): void {
  assert(gBrowser !== undefined, "gBrowser should be defined");
  assert(Array.isArray(gBrowser.tabs) || gBrowser.tabs?.length !== undefined,
    "gBrowser.tabs should be array-like");
  assert(gBrowser.tabs.length > 0, "gBrowser.tabs should have at least one tab");
}

function testTabsAreXULElements(): void {
  for (const tab of gBrowser.tabs) {
    assert(tab !== null && tab !== undefined, "tab should not be null");
    assert(
      typeof tab.getAttribute === "function",
      "each tab should have getAttribute method (XUL element)",
    );
  }
}

function testTabsHaveLinkedBrowser(): void {
  for (const tab of gBrowser.tabs) {
    assert(
      tab.linkedBrowser !== undefined && tab.linkedBrowser !== null,
      "each tab should have a linkedBrowser property",
    );
  }
}

// ---------------------------------------------------------------------------
// Tests — DOM tab elements match gBrowser.tabs
// ---------------------------------------------------------------------------

function testDomTabCountMatchesGBrowser(): void {
  const domTabs = document.querySelectorAll(".tabbrowser-tab");
  assert(domTabs.length > 0, "should find at least one .tabbrowser-tab in DOM");
  assertEquals(
    domTabs.length,
    gBrowser.tabs.length,
    "DOM .tabbrowser-tab count should match gBrowser.tabs.length",
  );
}

// ---------------------------------------------------------------------------
// Tests — workspace attribute access
// ---------------------------------------------------------------------------

function testWorkspaceAttributeReadable(): void {
  for (const tab of gBrowser.tabs) {
    // getAttribute should return a string or null, and must not throw
    const value = tab.getAttribute("floorpWorkspaceId");
    assert(
      value === null || typeof value === "string",
      "floorpWorkspaceId attribute should be string or null",
    );
  }
}

// ---------------------------------------------------------------------------
// Tests — Services.obs workspace observer
// ---------------------------------------------------------------------------

function testWorkspaceObserverRegistration(): void {
  const topic = "floorp-workspace-changed";
  const observer = {
    observe(_subject: unknown, _topic: string, _data: string): void {
      // no-op
    },
  };

  // addObserver and removeObserver should not throw
  let addThrew = false;
  let removeThrew = false;
  try {
    Services.obs.addObserver(observer, topic);
  } catch {
    addThrew = true;
  }
  try {
    Services.obs.removeObserver(observer, topic);
  } catch {
    removeThrew = true;
  }

  assert(!addThrew, "Services.obs.addObserver should not throw for workspace topic");
  assert(!removeThrew, "Services.obs.removeObserver should not throw for workspace topic");
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "gBrowser.tabs exists and has tabs", fn: testGBrowserTabsExists },
    { name: "tabs are XUL elements", fn: testTabsAreXULElements },
    { name: "tabs have linkedBrowser", fn: testTabsHaveLinkedBrowser },
    { name: "DOM tab count matches gBrowser.tabs", fn: testDomTabCountMatchesGBrowser },
    { name: "workspace attribute readable", fn: testWorkspaceAttributeReadable },
    { name: "workspace observer registration", fn: testWorkspaceObserverRegistration },
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
    throw new Error(`workspacesUI.test.ts failures: ${failures.join(" | ")}`);
}
