// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { getFaviconURLForPanel } from "../../common/panel-sidebar/utils/favicon-getter.ts";
import { assert, assertEquals, type TestCase } from "../utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

async function testStaticPanelReturnsFavicon(): Promise<void> {
  const panel = {
    id: "test-static",
    type: "static" as const,
    url: "floorp//bookmarks",
    width: 300,
    userContextId: null,
    zoomLevel: null,
  };
  const result = await getFaviconURLForPanel(panel);
  assert(
    typeof result === "string" && result.length > 0,
    "static panel should return a non-empty favicon URL",
  );
}

async function testWebPanelWithHttpReturnsString(): Promise<void> {
  const panel = {
    id: "test-web",
    type: "web" as const,
    url: "https://example.com",
    width: 400,
    userContextId: null,
    zoomLevel: null,
  };
  const result = await getFaviconURLForPanel(panel);
  assert(typeof result === "string", "web panel should return a string");
}

async function testExtensionPanelReturnsString(): Promise<void> {
  const panel = {
    id: "test-ext",
    type: "extension" as const,
    url: "extension://fake/sidebar.html",
    width: 450,
    userContextId: null,
    zoomLevel: null,
    extensionId: "fake-extension-id",
  };
  const result = await getFaviconURLForPanel(panel);
  assert(
    typeof result === "string",
    "extension panel should return a string (may be empty)",
  );
}

async function testWebPanelWithNoUrlReturnsFallback(): Promise<void> {
  const panel = {
    id: "test-nourl",
    type: "web" as const,
    url: "",
    width: 300,
    userContextId: null,
    zoomLevel: null,
  };
  const result = await getFaviconURLForPanel(panel);
  assert(
    typeof result === "string",
    "panel with empty URL should return fallback string",
  );
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "static panel favicon", fn: testStaticPanelReturnsFavicon },
    { name: "web panel HTTP favicon", fn: testWebPanelWithHttpReturnsString },
    { name: "extension panel favicon", fn: testExtensionPanelReturnsString },
    {
      name: "web panel no URL fallback",
      fn: testWebPanelWithNoUrlReturnsFallback,
    },
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

  if (failures.length > 0) {
    throw new Error(
      `panelSidebarFavicon.test.ts failures: ${failures.join(" | ")}`,
    );
  }
}
