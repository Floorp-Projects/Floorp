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
  ]);
}
