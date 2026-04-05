// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { assert, assertEquals, type TestCase } from "../utils/test_harness.ts";

function testToolboxAboveBrowser(): void {
  const toolbox = document.getElementById("navigator-toolbox");
  const browser = document.getElementById("browser");
  assert(toolbox !== null, "#navigator-toolbox should exist");
  assert(browser !== null, "#browser should exist");

  const toolboxRect = toolbox.getBoundingClientRect();
  const browserRect = browser.getBoundingClientRect();
  assert(
    toolboxRect.bottom <= browserRect.top + 1,
    `#navigator-toolbox bottom (${toolboxRect.bottom}) should be at or above #browser top (${browserRect.top})`,
  );
}

function testNavBarInsideToolbox(): void {
  const navBar = document.getElementById("nav-bar");
  const toolbox = document.getElementById("navigator-toolbox");
  assert(navBar !== null, "#nav-bar should exist");
  assert(toolbox !== null, "#navigator-toolbox should exist");

  const navRect = navBar.getBoundingClientRect();
  const toolboxRect = toolbox.getBoundingClientRect();

  // Allow 1px tolerance for rounding
  const tolerance = 1;
  assert(
    navRect.top >= toolboxRect.top - tolerance &&
      navRect.bottom <= toolboxRect.bottom + tolerance &&
      navRect.left >= toolboxRect.left - tolerance &&
      navRect.right <= toolboxRect.right + tolerance,
    `#nav-bar rect (${navRect.top},${navRect.left},${navRect.bottom},${navRect.right}) should be contained within #navigator-toolbox rect (${toolboxRect.top},${toolboxRect.left},${toolboxRect.bottom},${toolboxRect.right})`,
  );
}

function testTabsToolbarHasPositiveDimensions(): void {
  const tabsToolbar = document.getElementById("TabsToolbar");
  assert(tabsToolbar !== null, "#TabsToolbar should exist");

  const rect = tabsToolbar.getBoundingClientRect();
  assert(
    rect.width > 0,
    `#TabsToolbar width should be positive (got ${rect.width})`,
  );
  assert(
    rect.height > 0,
    `#TabsToolbar height should be positive (got ${rect.height})`,
  );
}

function testUrlbarHasPositiveWidth(): void {
  const urlbar = document.getElementById("urlbar");
  assert(urlbar !== null, "#urlbar should exist");

  const rect = urlbar.getBoundingClientRect();
  assert(
    rect.width > 0,
    `#urlbar width should be positive (got ${rect.width})`,
  );
}

function testTabboxFillsWindowWidth(): void {
  const tabbox = document.getElementById("tabbrowser-tabbox");
  assert(tabbox !== null, "#tabbrowser-tabbox should exist");

  const rect = tabbox.getBoundingClientRect();
  const windowWidth = window.innerWidth;
  assert(
    rect.width >= windowWidth * 0.5,
    `#tabbrowser-tabbox width (${rect.width}) should fill at least 50% of window width (${windowWidth})`,
  );
}

function testWindowDimensionsPositive(): void {
  assert(
    window.innerWidth > 0,
    `window.innerWidth should be positive (got ${window.innerWidth})`,
  );
  assert(
    window.innerHeight > 0,
    `window.innerHeight should be positive (got ${window.innerHeight})`,
  );
}

function testToolboxWidthMatchesWindow(): void {
  const toolbox = document.getElementById("navigator-toolbox");
  assert(toolbox !== null, "#navigator-toolbox should exist");

  const rect = toolbox.getBoundingClientRect();
  const windowWidth = window.innerWidth;
  const tolerance = 2;
  assert(
    Math.abs(rect.width - windowWidth) <= tolerance,
    `#navigator-toolbox width (${rect.width}) should approximately equal window width (${windowWidth})`,
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "navigator-toolbox is above browser", fn: testToolboxAboveBrowser },
    {
      name: "nav-bar is inside navigator-toolbox",
      fn: testNavBarInsideToolbox,
    },
    {
      name: "TabsToolbar has positive dimensions",
      fn: testTabsToolbarHasPositiveDimensions,
    },
    { name: "urlbar has positive width", fn: testUrlbarHasPositiveWidth },
    {
      name: "tabbrowser-tabbox fills most of window width",
      fn: testTabboxFillsWindowWidth,
    },
    {
      name: "window dimensions are positive",
      fn: testWindowDimensionsPositive,
    },
    {
      name: "navigator-toolbox width matches window width",
      fn: testToolboxWidthMatchesWindow,
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
  if (failures.length > 0)
    throw new Error(
      `browserChromeLayout.test.ts failures: ${failures.join(" | ")}`,
    );
}
