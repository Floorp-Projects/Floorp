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
// Tests — Navigation bar existence and visibility
// ---------------------------------------------------------------------------

function testNavBarExists(): void {
  const navBar = document.getElementById("nav-bar");
  assert(navBar !== null, "#nav-bar should exist in browser chrome");
}

function testNavBarVisible(): void {
  const navBar = document.getElementById("nav-bar");
  if (!navBar) return;

  const rect = navBar.getBoundingClientRect();
  assert(rect.height > 0, "#nav-bar should have positive height (visible)");
}

// ---------------------------------------------------------------------------
// Tests — URL bar
// ---------------------------------------------------------------------------

function testUrlBarExists(): void {
  const urlbar = document.getElementById("urlbar");
  const urlbarContainer = document.getElementById("urlbar-container");
  assert(
    urlbar !== null || urlbarContainer !== null,
    "#urlbar or #urlbar-container should exist",
  );
}

function testUrlBarHasPositiveWidth(): void {
  const urlbar = document.getElementById("urlbar");
  const urlbarContainer = document.getElementById("urlbar-container");
  const target = urlbarContainer || urlbar;
  if (!target) return;

  const rect = target.getBoundingClientRect();
  assert(rect.width > 0, "URL bar should have positive width");
}

// ---------------------------------------------------------------------------
// Tests — Back / Forward buttons
// ---------------------------------------------------------------------------

function testBackButtonExists(): void {
  const btn = document.getElementById("back-button");
  assert(btn !== null, "#back-button should exist in navigation bar");
}

function testForwardButtonExists(): void {
  const btn = document.getElementById("forward-button");
  assert(btn !== null, "#forward-button should exist in navigation bar");
}

// ---------------------------------------------------------------------------
// Tests — Hamburger menu button
// ---------------------------------------------------------------------------

function testPanelUIMenuButtonExists(): void {
  const btn = document.getElementById("PanelUI-menu-button");
  assert(btn !== null, "#PanelUI-menu-button should exist");
}

function testPanelUIMenuButtonVisible(): void {
  const btn = document.getElementById("PanelUI-menu-button");
  if (!btn) return;

  const rect = btn.getBoundingClientRect();
  assert(
    rect.width > 0 && rect.height > 0,
    "#PanelUI-menu-button should be visible with positive dimensions",
  );
}

// ---------------------------------------------------------------------------
// Tests — Nav bar width vs window width
// ---------------------------------------------------------------------------

function testNavBarWidthApproximatesWindow(): void {
  const navBar = document.getElementById("nav-bar");
  if (!navBar) return;

  const rect = navBar.getBoundingClientRect();
  const windowWidth = window.innerWidth;
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
  const backBtn = document.getElementById("back-button");
  const urlbar = document.getElementById("urlbar-container") ||
    document.getElementById("urlbar");
  const menuBtn = document.getElementById("PanelUI-menu-button");

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
  const toolbar = document.getElementById("PersonalToolbar");
  assert(
    toolbar !== null,
    "#PersonalToolbar should exist (may be collapsed)",
  );
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "nav-bar exists", fn: testNavBarExists },
    { name: "nav-bar visible", fn: testNavBarVisible },
    { name: "URL bar exists", fn: testUrlBarExists },
    { name: "URL bar has positive width", fn: testUrlBarHasPositiveWidth },
    { name: "back button exists", fn: testBackButtonExists },
    { name: "forward button exists", fn: testForwardButtonExists },
    { name: "PanelUI menu button exists", fn: testPanelUIMenuButtonExists },
    { name: "PanelUI menu button visible", fn: testPanelUIMenuButtonVisible },
    { name: "nav-bar width approximates window", fn: testNavBarWidthApproximatesWindow },
    { name: "horizontal element ordering", fn: testHorizontalElementOrdering },
    { name: "PersonalToolbar exists", fn: testPersonalToolbarExists },
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
    throw new Error(`navigationBarUI.test.ts failures: ${failures.join(" | ")}`);
}
