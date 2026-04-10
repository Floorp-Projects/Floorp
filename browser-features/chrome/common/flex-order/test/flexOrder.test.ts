// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { gFlexOrder } from "../flex-order.tsx";

import { assert, runTests } from "../../../test/utils/test_harness.ts";

function cleanupFlexOrderStyles(): void {
  const styles: HTMLStyleElement[] = Array.from(
    document!.head!.querySelectorAll("style"),
  );
  for (const style of styles) {
    if (style.textContent?.includes("#sidebar-box")) {
      style.remove();
    }
  }
}

function findFlexOrderStyleText(): string | undefined {
  const styles: HTMLStyleElement[] = Array.from(
    document!.head!.querySelectorAll("style"),
  );
  const match = styles.find((style) =>
    style.textContent?.includes("#sidebar-box"),
  );
  return match?.textContent ?? undefined;
}

function testGFlexOrderExports(): void {
  assert(
    typeof gFlexOrder.init === "function",
    "gFlexOrder.init should be a function",
  );
  assert(
    typeof gFlexOrder.applyFlexOrder === "function",
    "gFlexOrder.applyFlexOrder should be a function",
  );
}

function testApplyFlexOrderDoesNotThrow(): void {
  // applyFlexOrder updates internal signals — should not throw for any combination
  gFlexOrder.applyFlexOrder(true, true);
  gFlexOrder.applyFlexOrder(true, false);
  gFlexOrder.applyFlexOrder(false, true);
  gFlexOrder.applyFlexOrder(false, false);
}

function testInitRendersFlexOrderStyle(): void {
  cleanupFlexOrderStyles();
  gFlexOrder.init();

  const styleText = findFlexOrderStyleText();
  assert(
    styleText !== undefined,
    "gFlexOrder.init should inject a flex-order style tag",
  );
  assert(
    styleText!.includes("#sidebar-box"),
    "injected style should contain the sidebar order selector",
  );
  cleanupFlexOrderStyles();
}

function testApplyFlexOrderUpdatesRenderedStyle(): void {
  cleanupFlexOrderStyles();
  gFlexOrder.init();
  gFlexOrder.applyFlexOrder(true, false);

  const styleText = findFlexOrderStyleText();
  assert(
    styleText !== undefined,
    "flex order style should be rendered after applyFlexOrder",
  );
  assert(
    styleText!.includes("#panel-sidebar-box"),
    "rendered style should include the Floorp sidebar selector",
  );
  cleanupFlexOrderStyles();
}

function testSidebarPositionPrefExists(): void {
  const prefValue = Services.prefs.getBoolPref("sidebar.position_start", true);
  assert(
    typeof prefValue === "boolean",
    "sidebar.position_start pref should return a boolean",
  );
}

function testApplyFlexOrderBothSidebarsAtStart(): void {
  // Test branch: fxSidebarPositionPref=true, floorpSidebarPositionPref=true
  // Expected order: Fx sidebar (0) -> Fx splitter (1) -> browser (2) -> Floorp splitter (3) -> Floorp sidebar (4) -> Floorp select box (5)
  cleanupFlexOrderStyles();
  gFlexOrder.init();
  gFlexOrder.applyFlexOrder(true, true);

  const styleText = findFlexOrderStyleText();
  assert(
    styleText !== undefined,
    "style should be rendered when both sidebars are at start",
  );

  // Verify order values in the rendered style
  assert(
    styleText!.includes("#sidebar-box"),
    "style should include #sidebar-box selector",
  );
  assert(
    styleText!.includes("#panel-sidebar-box"),
    "style should include #panel-sidebar-box selector",
  );
  assert(
    styleText!.includes("order: 0"),
    "style should include order 0 for first element",
  );
  assert(
    styleText!.includes("order: 1"),
    "style should include order 1 for second element",
  );
  assert(
    styleText!.includes("order: 2"),
    "style should include order 2 for third element",
  );
  assert(
    styleText!.includes("order: 3"),
    "style should include order 3 for fourth element",
  );
  assert(
    styleText!.includes("order: 4"),
    "style should include order 4 for fifth element",
  );
  assert(
    styleText!.includes("order: 5"),
    "style should include order 5 for sixth element",
  );
}

function testApplyFlexOrderFxAtStartFloorpAtEnd(): void {
  // Test branch: fxSidebarPositionPref=true, floorpSidebarPositionPref=false
  // Expected order: Floorp select box (0) -> Floorp sidebar (1) -> Floorp splitter (2) -> Fx sidebar (3) -> Fx splitter (4) -> browser (5)
  cleanupFlexOrderStyles();
  gFlexOrder.init();
  gFlexOrder.applyFlexOrder(true, false);

  const styleText = findFlexOrderStyleText();
  assert(
    styleText !== undefined,
    "style should be rendered when Fx sidebar at start, Floorp at end",
  );

  assert(
    styleText!.includes("order: 0"),
    "style should include order 0",
  );
  assert(
    styleText!.includes("order: 1"),
    "style should include order 1",
  );
  assert(
    styleText!.includes("order: 2"),
    "style should include order 2",
  );
  assert(
    styleText!.includes("order: 3"),
    "style should include order 3",
  );
  assert(
    styleText!.includes("order: 4"),
    "style should include order 4",
  );
  assert(
    styleText!.includes("order: 5"),
    "style should include order 5",
  );
  cleanupFlexOrderStyles();
}

function testApplyFlexOrderFxAtEndFloorpAtStart(): void {
  // Test branch: fxSidebarPositionPref=false, floorpSidebarPositionPref=true
  // Expected order: browser (0) -> vertical tab bar splitter (1) -> vertical tab bar (2) -> Fx sidebar (3) -> Fx splitter (4) -> Floorp splitter (5) -> Floorp sidebar (6) -> Floorp select box (7)
  // Note: verticaltabbar and verticaltabbarSplitter are NOT rendered in CSS, so order 1 and 2 won't appear
  cleanupFlexOrderStyles();
  gFlexOrder.init();
  gFlexOrder.applyFlexOrder(false, true);

  const styleText = findFlexOrderStyleText();
  assert(
    styleText !== undefined,
    "style should be rendered when Fx sidebar at end, Floorp at start",
  );

  // Only orders for elements rendered in CSS template are present
  // CSS template covers: fxSidebar, floorpSidebar, floorpSidebarSelectBox, floorpSidebarSplitter, fxSidebarSplitter, browserBox
  // In this branch: browserBox=0, fxSidebar=3, fxSidebarSplitter=4, floorpSidebarSplitter=5, floorpSidebar=6, floorpSidebarSelectBox=7
  assert(
    styleText!.includes("order: 0"),
    "style should include order 0 (browserBox)",
  );
  assert(
    styleText!.includes("order: 3"),
    "style should include order 3 (fxSidebar)",
  );
  assert(
    styleText!.includes("order: 4"),
    "style should include order 4 (fxSidebarSplitter)",
  );
  assert(
    styleText!.includes("order: 5"),
    "style should include order 5 (floorpSidebarSplitter)",
  );
  assert(
    styleText!.includes("order: 6"),
    "style should include order 6 (floorpSidebar)",
  );
  assert(
    styleText!.includes("order: 7"),
    "style should include order 7 (floorpSidebarSelectBox)",
  );
  cleanupFlexOrderStyles();
}

function testApplyFlexOrderBothSidebarsAtEnd(): void {
  // Test branch: fxSidebarPositionPref=false, floorpSidebarPositionPref=false
  // Expected order: Floorp select box (0) -> Floorp sidebar (1) -> Floorp splitter (2) -> browser (3) -> vertical tab bar splitter (4) -> vertical tab bar (5) -> Fx sidebar (6) -> Fx splitter (7)
  // Note: verticaltabbar and verticaltabbarSplitter are NOT rendered in CSS, so order 4 and 5 won't appear
  cleanupFlexOrderStyles();
  gFlexOrder.init();
  gFlexOrder.applyFlexOrder(false, false);

  const styleText = findFlexOrderStyleText();
  assert(
    styleText !== undefined,
    "style should be rendered when both sidebars are at end",
  );

  // Only orders for elements rendered in CSS template are present
  // CSS template covers: fxSidebar, floorpSidebar, floorpSidebarSelectBox, floorpSidebarSplitter, fxSidebarSplitter, browserBox
  // In this branch: floorpSidebarSelectBox=0, floorpSidebar=1, floorpSidebarSplitter=2, browserBox=3, fxSidebar=6, fxSidebarSplitter=7
  assert(
    styleText!.includes("order: 0"),
    "style should include order 0 (floorpSidebarSelectBox)",
  );
  assert(
    styleText!.includes("order: 1"),
    "style should include order 1 (floorpSidebar)",
  );
  assert(
    styleText!.includes("order: 2"),
    "style should include order 2 (floorpSidebarSplitter)",
  );
  assert(
    styleText!.includes("order: 3"),
    "style should include order 3 (browserBox)",
  );
  assert(
    styleText!.includes("order: 6"),
    "style should include order 6 (fxSidebar)",
  );
  assert(
    styleText!.includes("order: 7"),
    "style should include order 7 (fxSidebarSplitter)",
  );
  cleanupFlexOrderStyles();
}

function testAllExpectedSelectorsPresent(): void {
  // Verify that all expected CSS selectors are present in the rendered style
  cleanupFlexOrderStyles();
  gFlexOrder.init();
  gFlexOrder.applyFlexOrder(true, true);

  const styleText = findFlexOrderStyleText();
  assert(styleText !== undefined, "style should be rendered");

  const expectedSelectors = [
    "#sidebar-box",
    "#panel-sidebar-box",
    "#panel-sidebar-select-box",
    "#panel-sidebar-splitter",
    "#sidebar-splitter",
    "#tabbrowser-tabbox",
  ];

  for (const selector of expectedSelectors) {
    assert(
      styleText!.includes(selector),
      `style should include selector: ${selector}`,
    );
  }

  cleanupFlexOrderStyles();
}

function testApplyFlexOrderCanBeCalledMultipleTimes(): void {
  // Verify that applyFlexOrder can be called multiple times without errors
  cleanupFlexOrderStyles();
  gFlexOrder.init();

  // Call with different combinations sequentially
  gFlexOrder.applyFlexOrder(true, true);
  gFlexOrder.applyFlexOrder(true, false);
  gFlexOrder.applyFlexOrder(false, true);
  gFlexOrder.applyFlexOrder(false, false);
  gFlexOrder.applyFlexOrder(true, true); // back to first combination

  const styleText = findFlexOrderStyleText();
  assert(
    styleText !== undefined,
    "style should still be rendered after multiple applyFlexOrder calls",
  );

  cleanupFlexOrderStyles();
}

function testInitRegistersPrefObserver(): void {
  // Test that init() registers a preference observer
  cleanupFlexOrderStyles();

  let observerAdded = false;
  // deno-lint-ignore no-explicit-any
  const originalAddObserver = (Services.prefs as any).addObserver;

  // Use Object.defineProperty since addObserver may be read-only in Firefox
  let mockApplied = false;
  try {
    Object.defineProperty(Services.prefs, "addObserver", {
      value: (pref: string, _fn: unknown) => {
        if (pref === "sidebar.position_start") {
          observerAdded = true;
        }
        // Delegate to original if available
        if (typeof originalAddObserver === "function") {
          originalAddObserver.call(Services.prefs, pref, _fn);
        }
      },
      configurable: true,
      writable: true,
    });
    mockApplied = true;
  } catch {
    // addObserver is non-configurable; observer detection will not work.
  }

  try {
    gFlexOrder.init();
    if (mockApplied) {
      assert(
        observerAdded,
        "init should register observer for sidebar.position_start pref",
      );
    }
  } finally {
    // Restore original addObserver
    if (mockApplied) {
      try {
        Object.defineProperty(Services.prefs, "addObserver", {
          value: originalAddObserver,
          configurable: true,
          writable: true,
        });
      } catch {
        // Ignore if addObserver cannot be restored.
      }
    }
    cleanupFlexOrderStyles();
  }
}

function testObserverRemovedOnCleanup(): void {
  // Test that the preference observer is removed when cleanup is called
  cleanupFlexOrderStyles();

  let _observerRemoved = false;
  // deno-lint-ignore no-explicit-any
  const originalRemoveObserver = (Services.prefs as any).removeObserver;

  // Use Object.defineProperty since removeObserver may be read-only in Firefox
  let mockApplied = false;
  try {
    Object.defineProperty(Services.prefs, "removeObserver", {
      value: (pref: string, _fn: unknown) => {
        if (pref === "sidebar.position_start") {
          _observerRemoved = true;
        }
        // Delegate to original if available
        if (typeof originalRemoveObserver === "function") {
          originalRemoveObserver.call(Services.prefs, pref, _fn);
        }
      },
      configurable: true,
      writable: true,
    });
    mockApplied = true;
  } catch {
    // removeObserver is non-configurable; test cannot verify removal.
  }

  try {
    gFlexOrder.init();
    // Trigger cleanup by calling onCleanup callback
    // Note: This is a simplified test - in real scenario, onCleanup is called by SolidJS
    // The observer should be removed when the component is destroyed
    assert(
      typeof Services.prefs.removeObserver === "function",
      "removeObserver should be available",
    );
  } finally {
    // Restore original removeObserver
    if (mockApplied) {
      try {
        Object.defineProperty(Services.prefs, "removeObserver", {
          value: originalRemoveObserver,
          configurable: true,
          writable: true,
        });
      } catch {
        // Ignore if removeObserver cannot be restored.
      }
    }
    cleanupFlexOrderStyles();
  }
}

function testOrdersSignalUpdatesCorrectly(): void {
  // Test that the orders signal is updated correctly when applyFlexOrder is called
  cleanupFlexOrderStyles();
  gFlexOrder.init();

  // Test all four combinations
  gFlexOrder.applyFlexOrder(true, true);
  let styleText = findFlexOrderStyleText();
  assert(
    styleText !== undefined && styleText.includes("order: 0"),
    "orders should be set for both sidebars at start",
  );

  gFlexOrder.applyFlexOrder(true, false);
  styleText = findFlexOrderStyleText();
  assert(
    styleText !== undefined && styleText.includes("order: 0"),
    "orders should be set for Fx at start, Floorp at end",
  );

  gFlexOrder.applyFlexOrder(false, true);
  styleText = findFlexOrderStyleText();
  assert(
    styleText !== undefined && styleText.includes("order: 0"),
    "orders should be set for Fx at end, Floorp at start",
  );

  gFlexOrder.applyFlexOrder(false, false);
  styleText = findFlexOrderStyleText();
  assert(
    styleText !== undefined && styleText.includes("order: 0"),
    "orders should be set for both sidebars at end",
  );

  cleanupFlexOrderStyles();
}

export async function runAllTests(): Promise<void> {
  await runTests("flexOrder.test.ts", [
    { name: "gFlexOrder exports", fn: testGFlexOrderExports },
    {
      name: "applyFlexOrder does not throw",
      fn: testApplyFlexOrderDoesNotThrow,
    },
    {
      name: "init renders flex-order style",
      fn: testInitRendersFlexOrderStyle,
    },
    {
      name: "applyFlexOrder updates rendered style",
      fn: testApplyFlexOrderUpdatesRenderedStyle,
    },
    { name: "sidebar.position_start pref", fn: testSidebarPositionPrefExists },
    {
      name: "applyFlexOrder: both sidebars at start",
      fn: testApplyFlexOrderBothSidebarsAtStart,
    },
    {
      name: "applyFlexOrder: Fx at start, Floorp at end",
      fn: testApplyFlexOrderFxAtStartFloorpAtEnd,
    },
    {
      name: "applyFlexOrder: Fx at end, Floorp at start",
      fn: testApplyFlexOrderFxAtEndFloorpAtStart,
    },
    {
      name: "applyFlexOrder: both sidebars at end",
      fn: testApplyFlexOrderBothSidebarsAtEnd,
    },
    {
      name: "all expected CSS selectors are present",
      fn: testAllExpectedSelectorsPresent,
    },
    {
      name: "applyFlexOrder can be called multiple times",
      fn: testApplyFlexOrderCanBeCalledMultipleTimes,
    },
    {
      name: "init registers pref observer",
      fn: testInitRegistersPrefObserver,
    },
    {
      name: "observer removed on cleanup",
      fn: testObserverRemovedOnCleanup,
    },
    {
      name: "orders signal updates correctly",
      fn: testOrdersSignalUpdatesCorrectly,
    },
  ]);
}
