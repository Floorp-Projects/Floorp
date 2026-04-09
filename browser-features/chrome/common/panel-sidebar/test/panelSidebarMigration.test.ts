// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { convertSidebar } from "../data/migration.ts";
import {
  type TestCase,
  assertEquals,
} from "../../../test/utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

function testEmptySidebar(): void {
  const result = convertSidebar({ data: {}, index: [] });
  assertEquals(
    result.data.length,
    0,
    "empty sidebar should produce empty data",
  );
}

function testWebPanel(): void {
  const result = convertSidebar({
    data: { p1: { url: "https://example.com", width: 300 } },
    index: ["p1"],
  });
  assertEquals(result.data.length, 1, "should have one item");
  assertEquals(result.data[0].type, "web", "URL panel should be type web");
  assertEquals(result.data[0].url, "https://example.com", "URL preserved");
  assertEquals(result.data[0].width, 300, "width preserved");
  assertEquals(result.data[0].id, "p1", "id should match key");
}

function testExtensionPanel(): void {
  const result = convertSidebar({
    data: { ext1: { url: "extension://abc/sidebar.html" } },
    index: ["ext1"],
  });
  assertEquals(
    result.data[0].type,
    "extension",
    "extension URL should be type extension",
  );
}

function testExtensionDefaultWidth(): void {
  const result = convertSidebar({
    data: { ext1: { url: "extension://abc/sidebar.html" } },
    index: ["ext1"],
  });
  assertEquals(
    result.data[0].width,
    450,
    "extension with no width should default to 450",
  );
}

function testExtensionCustomWidth(): void {
  const result = convertSidebar({
    data: { ext1: { url: "extension://abc/sidebar.html", width: 600 } },
    index: ["ext1"],
  });
  assertEquals(
    result.data[0].width,
    600,
    "extension with custom width should preserve it",
  );
}

function testStaticPanel(): void {
  const result = convertSidebar({
    data: { s1: { url: "floorp//bookmarks" } },
    index: ["s1"],
  });
  assertEquals(
    result.data[0].type,
    "static",
    "floorp// URL should be type static",
  );
}

function testUserContextPreserved(): void {
  const result = convertSidebar({
    data: { p1: { url: "https://example.com", usercontext: 3 } },
    index: ["p1"],
  });
  assertEquals(
    result.data[0].userContextId,
    3,
    "usercontext should map to userContextId",
  );
}

function testUserContextNull(): void {
  const result = convertSidebar({
    data: { p1: { url: "https://example.com" } },
    index: ["p1"],
  });
  assertEquals(
    result.data[0].userContextId,
    null,
    "missing usercontext should be null",
  );
}

function testZoomLevelPreserved(): void {
  const result = convertSidebar({
    data: { p1: { url: "https://example.com", zoomLevel: 1.5 } },
    index: ["p1"],
  });
  assertEquals(result.data[0].zoomLevel, 1.5, "zoomLevel should be preserved");
}

function testIndexOrdering(): void {
  const result = convertSidebar({
    data: {
      a: { url: "https://a.com" },
      b: { url: "https://b.com" },
      c: { url: "https://c.com" },
    },
    index: ["c", "a", "b"],
  });
  assertEquals(result.data[0].id, "c", "first should be c");
  assertEquals(result.data[1].id, "a", "second should be a");
  assertEquals(result.data[2].id, "b", "third should be b");
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export function runAllTests(): void {
  const tests: TestCase[] = [
    { name: "empty sidebar", fn: testEmptySidebar },
    { name: "web panel", fn: testWebPanel },
    { name: "extension panel", fn: testExtensionPanel },
    { name: "extension default width", fn: testExtensionDefaultWidth },
    { name: "extension custom width", fn: testExtensionCustomWidth },
    { name: "static panel", fn: testStaticPanel },
    { name: "userContext preserved", fn: testUserContextPreserved },
    { name: "userContext null", fn: testUserContextNull },
    { name: "zoomLevel preserved", fn: testZoomLevelPreserved },
    { name: "index ordering", fn: testIndexOrdering },
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
      `panelSidebarMigration.test.ts failures: ${failures.join(" | ")}`,
    );
  }
}
