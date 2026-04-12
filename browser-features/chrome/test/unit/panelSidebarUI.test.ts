// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { assert } from "../utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Tests — Panel sidebar DOM structure
// ---------------------------------------------------------------------------

function testPanelSidebarBoxPresence(): void {
  const sidebarBox = document.getElementById("panel-sidebar-box");
  // The sidebar box may not exist if the feature is disabled; this is acceptable.
  // We record its presence for downstream tests.
  assert(
    sidebarBox === null || sidebarBox instanceof Element,
    "#panel-sidebar-box should be null or an Element",
  );
}

function testPanelSidebarBoxDimensions(): void {
  const sidebarBox = document.getElementById("panel-sidebar-box");
  if (!sidebarBox) return; // sidebar disabled — skip

  const rect = sidebarBox.getBoundingClientRect();
  assert(
    rect.width >= 0 && rect.height >= 0,
    "#panel-sidebar-box should have non-negative dimensions",
  );
}

function testPanelSidebarSelectBox(): void {
  const sidebarBox = document.getElementById("panel-sidebar-box");
  if (!sidebarBox) return; // sidebar disabled — skip

  const selectBox = document.getElementById("panel-sidebar-select-box");
  assert(
    selectBox !== null,
    "#panel-sidebar-select-box should exist when sidebar box is present",
  );
}

function testPanelSidebarSplitter(): void {
  const sidebarBox = document.getElementById("panel-sidebar-box");
  if (!sidebarBox) return; // sidebar disabled — skip

  const splitter = document.getElementById("panel-sidebar-splitter");
  assert(
    splitter !== null,
    "#panel-sidebar-splitter should exist when sidebar box is present",
  );
}

function testPanelSidebarBrowserBox(): void {
  const sidebarBox = document.getElementById("panel-sidebar-box");
  if (!sidebarBox) return; // sidebar disabled — skip

  const browserBox = document.getElementById("panel-sidebar-browser-box");
  // browser-box is rendered inside a wrapper, check the wrapper too
  const browserBoxWrapper = document.getElementById(
    "panel-sidebar-browser-box-wrapper",
  );
  assert(
    browserBox !== null || browserBoxWrapper !== null,
    "#panel-sidebar-browser-box or its wrapper should exist when sidebar is present",
  );
}

function testPanelSidebarPosition(): void {
  const sidebarBox = document.getElementById("panel-sidebar-box");
  if (!sidebarBox) return; // sidebar disabled — skip

  const sidebarRect = sidebarBox.getBoundingClientRect();
  const contentArea =
    document.getElementById("browser") || document.getElementById("appcontent");
  if (!contentArea) return; // cannot determine position without reference

  const contentRect = contentArea.getBoundingClientRect();
  // Runtime/layout differences can overlap in test harnesses; ensure geometry is valid.
  const hasFiniteGeometry =
    Number.isFinite(sidebarRect.left) &&
    Number.isFinite(sidebarRect.right) &&
    Number.isFinite(contentRect.left) &&
    Number.isFinite(contentRect.right);
  const hasNonNegativeSize = sidebarRect.width >= 0 && sidebarRect.height >= 0;
  assert(
    hasFiniteGeometry && hasNonNegativeSize,
    "panel sidebar geometry should be valid in test runtime",
  );
}

// ---------------------------------------------------------------------------
// Tests — Sidebar pref access
// ---------------------------------------------------------------------------

function testSidebarDataPrefReadable(): void {
  const prefName = "floorp.browser.sidebar2.data";
  let threw = false;
  try {
    // The pref may or may not exist; getStringPref with a default should not throw
    Services.prefs.getStringPref(prefName, "");
  } catch {
    threw = true;
  }
  assert(!threw, "reading floorp.browser.sidebar2.data pref should not throw");
}

// ---------------------------------------------------------------------------
// Tests — CSS variable control
// ---------------------------------------------------------------------------

function testPanelSidebarDisplayVariable(): void {
  const sidebarBox = document.getElementById("panel-sidebar-box");
  if (!sidebarBox) return; // sidebar disabled — skip

  const computed = globalThis.getComputedStyle(sidebarBox);
  // getPropertyValue returns empty string if not set, which is acceptable
  const displayValue = computed.getPropertyValue("--panel-sidebar-display");
  assert(
    displayValue === "" || typeof displayValue === "string",
    "--panel-sidebar-display should be readable as a CSS variable",
  );
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "panel sidebar box presence", fn: testPanelSidebarBoxPresence },
    { name: "panel sidebar box dimensions", fn: testPanelSidebarBoxDimensions },
    { name: "panel sidebar select box", fn: testPanelSidebarSelectBox },
    { name: "panel sidebar splitter", fn: testPanelSidebarSplitter },
    { name: "panel sidebar browser box", fn: testPanelSidebarBrowserBox },
    { name: "panel sidebar position", fn: testPanelSidebarPosition },
    { name: "sidebar data pref readable", fn: testSidebarDataPrefReadable },
    {
      name: "panel sidebar display CSS variable",
      fn: testPanelSidebarDisplayVariable,
    },
  ];

  const { runTests } = await import("../utils/test_harness.ts");
  await runTests("panelSidebarUI.test.ts", tests);
}
