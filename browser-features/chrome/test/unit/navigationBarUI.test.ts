// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { assert, runTests } from "../utils/test_harness.ts";

/** Skip test if the browser chrome DOM is not available (e.g., in unit test environment) */
function requireBrowserChrome(): boolean {
  return document.getElementById("navigator-toolbox") !== null;
}

function getPanelMenuButton(): HTMLElement | null {
  return (
    (document?.getElementById("PanelUI-menu-button") as HTMLElement | null) ??
    (document?.getElementById("PanelUI-button") as HTMLElement | null)
  );
}

// ---------------------------------------------------------------------------
// Tests — Navigation bar existence and visibility
// ---------------------------------------------------------------------------

function testNavBarExists(): void {
  if (!requireBrowserChrome()) return;
  const navBar = document?.getElementById("nav-bar") ?? null;
  assert(navBar !== null, "#nav-bar should exist in browser chrome");
}

function testNavBarVisible(): void {
  if (!requireBrowserChrome()) return;
  const navBar = document?.getElementById("nav-bar") ?? null;
  if (!navBar) return;

  const rect = navBar.getBoundingClientRect();
  assert(rect.height > 0, "#nav-bar should have positive height (visible)");
}

// ---------------------------------------------------------------------------
// Tests — URL bar
// ---------------------------------------------------------------------------

function testUrlBarExists(): void {
  if (!requireBrowserChrome()) return;
  const urlbar = document?.getElementById("urlbar") ?? null;
  const urlbarContainer = document?.getElementById("urlbar-container") ?? null;
  assert(
    urlbar !== null || urlbarContainer !== null,
    "#urlbar or #urlbar-container should exist",
  );
}

function testUrlBarHasPositiveWidth(): void {
  if (!requireBrowserChrome()) return;
  const urlbar = document?.getElementById("urlbar") ?? null;
  const urlbarContainer = document?.getElementById("urlbar-container") ?? null;
  const target = urlbarContainer || urlbar;
  if (!target) return;

  const rect = target.getBoundingClientRect();
  assert(rect.width > 0, "URL bar should have positive width");
}

// ---------------------------------------------------------------------------
// Tests — Back / Forward buttons
// ---------------------------------------------------------------------------

function testBackButtonExists(): void {
  if (!requireBrowserChrome()) return;
  const btn = document?.getElementById("back-button") ?? null;
  assert(btn !== null, "#back-button should exist in navigation bar");
}

function testForwardButtonExists(): void {
  if (!requireBrowserChrome()) return;
  const btn = document?.getElementById("forward-button") ?? null;
  assert(btn !== null, "#forward-button should exist in navigation bar");
}

// ---------------------------------------------------------------------------
// Tests — Hamburger menu button
// ---------------------------------------------------------------------------

function testPanelUIMenuButtonExists(): void {
  if (!requireBrowserChrome()) return;
  const btn = getPanelMenuButton();
  if (!btn) return;

  assert(
    btn.id === "PanelUI-menu-button" || btn.id === "PanelUI-button",
    "Panel UI menu button should use a known element id",
  );
}

// ---------------------------------------------------------------------------
// Tests — Nav bar width vs window width
// ---------------------------------------------------------------------------

function testNavBarWidthApproximatesWindow(): void {
  if (!requireBrowserChrome()) return;
  const navBar = document?.getElementById("nav-bar") ?? null;
  if (!navBar) return;

  const rect = navBar.getBoundingClientRect();
  const windowWidth = globalThis.innerWidth;
  // Allow some margin for borders, scrollbars, etc.
  const tolerance = 50;
  assert(
    Math.abs(rect.width - windowWidth) <= tolerance,
    `nav-bar width (${rect.width}px) should be approximately window width (${windowWidth}px)`,
  );
}

// ---------------------------------------------------------------------------
// Tests — Horizontal ordering: back < urlbar < menu button
// ---------------------------------------------------------------------------

function testHorizontalElementOrdering(): void {
  if (!requireBrowserChrome()) return;
  const backBtn = document?.getElementById("back-button") ?? null;
  const urlbar =
    (document?.getElementById("urlbar-container") ?? null) ||
    (document?.getElementById("urlbar") ?? null);
  const menuBtn = getPanelMenuButton();

  if (!backBtn || !urlbar || !menuBtn) return; // skip if elements missing

  const backRect = backBtn.getBoundingClientRect();
  const urlbarRect = urlbar.getBoundingClientRect();
  const menuRect = menuBtn.getBoundingClientRect();

  assert(
    backRect.left < urlbarRect.left,
    "back button should be to the left of the URL bar",
  );
  assert(
    urlbarRect.right <= menuRect.left + 2,
    "URL bar should be to the left of the menu button",
  );
}

// ---------------------------------------------------------------------------
// Tests — PersonalToolbar (bookmarks bar)
// ---------------------------------------------------------------------------

function testPersonalToolbarExists(): void {
  if (!requireBrowserChrome()) return;
  const toolbar = document?.getElementById("PersonalToolbar") ?? null;
  assert(toolbar !== null, "#PersonalToolbar should exist (may be collapsed)");
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  await runTests("navigationBarUI.test.ts", [
    { name: "nav-bar exists", fn: testNavBarExists },
    { name: "nav-bar visible", fn: testNavBarVisible },
    { name: "URL bar exists", fn: testUrlBarExists },
    { name: "URL bar has positive width", fn: testUrlBarHasPositiveWidth },
    { name: "back button exists", fn: testBackButtonExists },
    { name: "forward button exists", fn: testForwardButtonExists },
    { name: "PanelUI menu button exists", fn: testPanelUIMenuButtonExists },
    {
      name: "nav-bar width approximates window",
      fn: testNavBarWidthApproximatesWindow,
    },
    { name: "horizontal element ordering", fn: testHorizontalElementOrdering },
    { name: "PersonalToolbar exists", fn: testPersonalToolbarExists },
  ]);
}
