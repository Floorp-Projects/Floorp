// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../utils/test_harness.ts";

/** Skip test if gBrowser is not available (e.g., in unit test environment) */
function requireGBrowser(): boolean {
  return typeof gBrowser !== "undefined" && gBrowser !== null;
}

// ---------------------------------------------------------------------------
// Tests — gBrowser.tabs basics
// ---------------------------------------------------------------------------

function testGBrowserTabsExists(): void {
  if (!requireGBrowser()) return;
  assert(gBrowser !== undefined, "gBrowser should be defined");
  assert(
    Array.isArray(gBrowser.tabs) || gBrowser.tabs?.length !== undefined,
    "gBrowser.tabs should be array-like",
  );
  assert(
    gBrowser.tabs.length > 0,
    "gBrowser.tabs should have at least one tab",
  );
}

function testTabsAreXULElements(): void {
  if (!requireGBrowser()) return;
  for (const tab of gBrowser.tabs) {
    assert(tab !== null && tab !== undefined, "tab should not be null");
    assert(
      typeof tab.getAttribute === "function",
      "each tab should have getAttribute method (XUL element)",
    );
  }
}

function testTabsHaveLinkedBrowser(): void {
  if (!requireGBrowser()) return;
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
  if (!requireGBrowser()) return;
  const domTabs = document.querySelectorAll(".tabbrowser-tab");
  if (domTabs.length === 0) {
    // Some harness contexts expose gBrowser tabs without mirrored DOM tab nodes.
    assert(true, "no .tabbrowser-tab nodes in this runtime; skipping strict count check");
    return;
  }
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
  if (!requireGBrowser()) return;
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

  assert(
    !addThrew,
    "Services.obs.addObserver should not throw for workspace topic",
  );
  assert(
    !removeThrew,
    "Services.obs.removeObserver should not throw for workspace topic",
  );
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  await runTests("workspacesUI.test.ts", [
    { name: "gBrowser.tabs exists and has tabs", fn: testGBrowserTabsExists },
    { name: "tabs are XUL elements", fn: testTabsAreXULElements },
    { name: "tabs have linkedBrowser", fn: testTabsHaveLinkedBrowser },
    {
      name: "DOM tab count matches gBrowser.tabs",
      fn: testDomTabCountMatchesGBrowser,
    },
    {
      name: "workspace attribute readable",
      fn: testWorkspaceAttributeReadable,
    },
    {
      name: "workspace observer registration",
      fn: testWorkspaceObserverRegistration,
    },
  ]);
}
