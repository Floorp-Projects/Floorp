// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { STATIC_PANEL_DATA } from "../data/static-panels.ts";
import {
  type TestCase,
  assert,
} from "../../../test/utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

function testStaticPanelDataExists(): void {
  assert(
    typeof STATIC_PANEL_DATA === "object" && STATIC_PANEL_DATA !== null,
    "STATIC_PANEL_DATA should be an object",
  );
}

function testBookmarksPanelExists(): void {
  const panel = STATIC_PANEL_DATA["floorp//bookmarks"];
  assert(panel !== undefined, "bookmarks panel should exist");
  assert(typeof panel.url === "string", "should have url");
  assert(typeof panel.icon === "string", "should have icon");
  assert(typeof panel.l10n === "string", "should have l10n key");
  assert(typeof panel.defaultWidth === "number", "should have defaultWidth");
}

function testHistoryPanelExists(): void {
  assert(
    STATIC_PANEL_DATA["floorp//history"] !== undefined,
    "history panel should exist",
  );
}

function testBmtPanelExists(): void {
  assert(
    STATIC_PANEL_DATA["floorp//bmt"] !== undefined,
    "BMT panel should exist",
  );
}

function testAllPanelsHaveRequiredFields(): void {
  for (const [key, panel] of Object.entries(STATIC_PANEL_DATA)) {
    assert(
      typeof panel.url === "string" && panel.url.length > 0,
      `${key}: url should be non-empty string`,
    );
    assert(
      typeof panel.icon === "string" && panel.icon.length > 0,
      `${key}: icon should be non-empty string`,
    );
    assert(
      typeof panel.l10n === "string" && panel.l10n.length > 0,
      `${key}: l10n should be non-empty string`,
    );
    assert(
      typeof panel.defaultWidth === "number" && panel.defaultWidth > 0,
      `${key}: defaultWidth should be positive number`,
    );
  }
}

function testPanelKeysStartWithFloorp(): void {
  for (const key of Object.keys(STATIC_PANEL_DATA)) {
    assert(
      key.startsWith("floorp//"),
      `panel key "${key}" should start with "floorp//"`,
    );
  }
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export function runAllTests(): void {
  const tests: TestCase[] = [
    { name: "STATIC_PANEL_DATA exists", fn: testStaticPanelDataExists },
    { name: "bookmarks panel", fn: testBookmarksPanelExists },
    { name: "history panel", fn: testHistoryPanelExists },
    { name: "BMT panel", fn: testBmtPanelExists },
    {
      name: "all panels have required fields",
      fn: testAllPanelsHaveRequiredFields,
    },
    {
      name: "panel keys start with floorp//",
      fn: testPanelKeysStartWithFloorp,
    },
  ];

  const failures: string[] = [];

  for (const test of tests) {
    try {
      test.fn();
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      failures.push(`${test.name}: ${message}`);
    }
  }

  if (failures.length > 0) {
    throw new Error(
      `panelSidebarStaticPanels.test.ts failures: ${failures.join(" | ")}`,
    );
  }
}
