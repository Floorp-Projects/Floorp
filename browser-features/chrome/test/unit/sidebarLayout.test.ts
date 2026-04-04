// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

type TestCase = { name: string; fn: () => void | Promise<void> };

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) throw new Error(message);
}

function testSidebarBoxExists(): void {
  const sidebarBox = document.getElementById("sidebar-box");
  assert(sidebarBox !== null, "#sidebar-box should exist in the DOM");
}

function testSidebarPositionStart(): void {
  const sidebarBox = document.getElementById("sidebar-box");
  assert(sidebarBox !== null, "#sidebar-box should exist");

  const prefKey = "sidebar.position_start";
  const positionStart = Services.prefs.getBoolPref(prefKey, true);

  // Only test positioning when sidebar is visible (has width)
  const sidebarRect = sidebarBox.getBoundingClientRect();
  if (sidebarRect.width === 0) {
    // Sidebar is collapsed/hidden; skip positional assertion
    return;
  }

  const appcontent = document.getElementById("appcontent");
  if (appcontent === null) return;

  const contentRect = appcontent.getBoundingClientRect();

  if (positionStart) {
    assert(
      sidebarRect.left <= contentRect.left,
      `When sidebar.position_start is true, sidebar left (${sidebarRect.left}) should be <= content left (${contentRect.left})`,
    );
  } else {
    assert(
      sidebarRect.left >= contentRect.right - 1,
      `When sidebar.position_start is false, sidebar left (${sidebarRect.left}) should be >= content right (${contentRect.right})`,
    );
  }
}

function testPanelSidebarBoxGraceful(): void {
  const panelSidebarBox = document.getElementById("panel-sidebar-box");
  // This element may or may not exist depending on Floorp configuration
  if (panelSidebarBox !== null) {
    const rect = panelSidebarBox.getBoundingClientRect();
    const style = window.getComputedStyle(panelSidebarBox);
    // Either has width or has a display value set
    assert(
      rect.width > 0 || style.display !== "",
      "#panel-sidebar-box should have width > 0 or a display style set",
    );
  }
  // If it does not exist, test passes gracefully
}

function testSidebarSplitterExists(): void {
  const splitter = document.getElementById("sidebar-splitter");
  assert(splitter !== null, "#sidebar-splitter should exist in the DOM");
}

function testSidebarAndContentNoOverlap(): void {
  const sidebarBox = document.getElementById("sidebar-box");
  const appcontent = document.getElementById("appcontent");

  if (sidebarBox === null || appcontent === null) {
    // Elements not found; skip overlap check
    return;
  }

  const sidebarRect = sidebarBox.getBoundingClientRect();
  const contentRect = appcontent.getBoundingClientRect();

  // If sidebar is collapsed (width 0), no overlap is possible
  if (sidebarRect.width === 0) return;

  // Calculate horizontal overlap
  const overlapLeft = Math.max(sidebarRect.left, contentRect.left);
  const overlapRight = Math.min(sidebarRect.right, contentRect.right);
  const horizontalOverlap = Math.max(0, overlapRight - overlapLeft);

  // Allow up to 2px overlap for splitter/border rendering
  const tolerance = 2;
  assert(
    horizontalOverlap <= tolerance,
    `#sidebar-box and #appcontent should not overlap significantly (overlap: ${horizontalOverlap}px)`,
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "sidebar-box exists", fn: testSidebarBoxExists },
    { name: "sidebar position respects pref", fn: testSidebarPositionStart },
    { name: "panel-sidebar-box handled gracefully", fn: testPanelSidebarBoxGraceful },
    { name: "sidebar-splitter exists", fn: testSidebarSplitterExists },
    { name: "sidebar and content do not overlap", fn: testSidebarAndContentNoOverlap },
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
      `sidebarLayout.test.ts failures: ${failures.join(" | ")}`,
    );
}
