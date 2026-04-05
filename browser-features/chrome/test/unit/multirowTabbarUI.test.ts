// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { assert, runTests, type TestCase } from "../utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Tests — TabsToolbar existence and dimensions
// ---------------------------------------------------------------------------

function testTabsToolbarExists(): void {
  const toolbar = document.getElementById("TabsToolbar");
  assert(toolbar !== null, "#TabsToolbar should exist in browser chrome");
}

function testTabsToolbarHasDimensions(): void {
  const toolbar = document.getElementById("TabsToolbar");
  if (!toolbar) return;

  const rect = toolbar.getBoundingClientRect();
  assert(rect.width > 0, "#TabsToolbar should have positive width");
  assert(rect.height > 0, "#TabsToolbar should have positive height");
}

// ---------------------------------------------------------------------------
// Tests — Multirow detection via attribute
// ---------------------------------------------------------------------------

function testMultibarAttributeReadable(): void {
  const toolbar = document.getElementById("TabsToolbar");
  if (!toolbar) return;

  const multibar = toolbar.getAttribute("multibar");
  // Should be a string value or null — must not throw
  assert(
    multibar === null || typeof multibar === "string",
    "multibar attribute should be string or null",
  );
}

function testMultirowToolbarHeight(): void {
  const toolbar = document.getElementById("TabsToolbar");
  if (!toolbar) return;

  const rect = toolbar.getBoundingClientRect();
  const isMultirow = toolbar.hasAttribute("multibar");

  if (isMultirow) {
    // In multirow mode, toolbar height should be larger than a single row
    assert(
      rect.height > 40,
      "multirow TabsToolbar height should exceed single-row height (~40px)",
    );
  } else {
    // Single row: height should be within a reasonable range
    assert(
      rect.height >= 20 && rect.height <= 60,
      `single-row TabsToolbar height should be 20-60px (actual: ${rect.height}px)`,
    );
  }
}

// ---------------------------------------------------------------------------
// Tests — Tab elements
// ---------------------------------------------------------------------------

function testTabElementsExist(): void {
  const tabs = document.querySelectorAll(".tabbrowser-tab");
  assert(tabs.length >= 1, "should have at least one .tabbrowser-tab in DOM");
}

function testTabElementDimensions(): void {
  const tabs = document.querySelectorAll(".tabbrowser-tab");
  for (const tab of tabs) {
    const rect = tab.getBoundingClientRect();
    // Tabs may be scrolled out of view or collapsed, so only check non-hidden tabs
    if (rect.width === 0 && rect.height === 0) continue;
    assert(rect.width > 0, "visible tab should have positive width");
    assert(rect.height > 0, "visible tab should have positive height");
  }
}

// ---------------------------------------------------------------------------
// Tests — Pinned tabs DOM order
// ---------------------------------------------------------------------------

function testPinnedTabsDomOrder(): void {
  const allTabs = Array.from(document.querySelectorAll(".tabbrowser-tab"));
  if (allTabs.length === 0) return;

  let lastPinnedIndex = -1;
  let firstUnpinnedIndex = -1;

  for (let i = 0; i < allTabs.length; i++) {
    const isPinned = allTabs[i].hasAttribute("pinned");
    if (isPinned) {
      lastPinnedIndex = i;
    } else if (firstUnpinnedIndex === -1) {
      firstUnpinnedIndex = i;
    }
  }

  // If there are both pinned and unpinned tabs, pinned must come first
  if (lastPinnedIndex >= 0 && firstUnpinnedIndex >= 0) {
    assert(
      lastPinnedIndex < firstUnpinnedIndex,
      "all pinned tabs should appear before non-pinned tabs in DOM order",
    );
  }
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  await runTests("multirowTabbarUI.test.ts", [
    { name: "TabsToolbar exists", fn: testTabsToolbarExists },
    { name: "TabsToolbar has dimensions", fn: testTabsToolbarHasDimensions },
    { name: "multibar attribute readable", fn: testMultibarAttributeReadable },
    { name: "multirow toolbar height", fn: testMultirowToolbarHeight },
    { name: "tab elements exist", fn: testTabElementsExist },
    { name: "tab element dimensions", fn: testTabElementDimensions },
    { name: "pinned tabs DOM order", fn: testPinnedTabsDomOrder },
  ]);
}
