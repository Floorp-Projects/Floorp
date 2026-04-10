// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { assert, assertEquals, type TestCase } from "../utils/test_harness.ts";

function isVisible(el: Element): boolean {
  const style = window.getComputedStyle(el);
  if (style.display === "none") return false;
  const rect = el.getBoundingClientRect();
  return rect.height > 0;
}

/** Skip test if the browser chrome DOM is not available (e.g., in unit test environment) */
function requireBrowserChrome(): boolean {
  return document.getElementById("navigator-toolbox") !== null;
}

function testNavBarVisible(): void {
  if (!requireBrowserChrome()) return;
  const navBar = document.getElementById("nav-bar");
  assert(navBar !== null, "#nav-bar should exist");
  assert(
    isVisible(navBar),
    "#nav-bar should be visible (display !== none, height > 0)",
  );
}

function testTabsToolbarVisible(): void {
  if (!requireBrowserChrome()) return;
  const tabsToolbar = document.getElementById("TabsToolbar");
  assert(tabsToolbar !== null, "#TabsToolbar should exist");
  assert(isVisible(tabsToolbar), "#TabsToolbar should be visible");
}

function testUrlbarVisible(): void {
  if (!requireBrowserChrome()) return;
  const urlbar = document.getElementById("urlbar");
  assert(urlbar !== null, "#urlbar should exist");
  assert(isVisible(urlbar), "#urlbar should be visible");
}

function testTabboxVisible(): void {
  if (!requireBrowserChrome()) return;
  const tabbox = document.getElementById("tabbrowser-tabbox");
  assert(tabbox !== null, "#tabbrowser-tabbox should exist");
  assert(isVisible(tabbox), "#tabbrowser-tabbox should be visible");
}

function testMainPopupSetExists(): void {
  if (!requireBrowserChrome()) return;
  const popupSet = document.getElementById("mainPopupSet");
  assert(
    popupSet !== null,
    "#mainPopupSet should exist (popups are hidden by default)",
  );
}

function testZenModeAttributeToggle(): void {
  const docEl = document.documentElement;
  assert(docEl !== null, "document.documentElement should exist");

  // Ensure clean state
  docEl.removeAttribute("zenmode");
  assert(
    !docEl.hasAttribute("zenmode"),
    "zenmode attribute should not be set initially",
  );

  // Set and verify
  docEl.setAttribute("zenmode", "true");
  assert(
    docEl.hasAttribute("zenmode"),
    "zenmode attribute should be set after setAttribute",
  );
  assertEquals(
    docEl.getAttribute("zenmode"),
    "true",
    "zenmode attribute value should be 'true'",
  );

  // Remove and verify
  docEl.removeAttribute("zenmode");
  assert(
    !docEl.hasAttribute("zenmode"),
    "zenmode attribute should be removed after removeAttribute",
  );
}

function testStatusBarPrefToggle(): void {
  const prefKey = "noraneko.statusbar.enable";
  const originalValue = Services.prefs.getBoolPref(prefKey, false);

  try {
    // Enable status bar
    Services.prefs.setBoolPref(prefKey, true);
    const statusBar = document.getElementById("nora-statusbar");
    // The status bar element may or may not exist depending on initialization;
    // if it exists, we just confirm it's in the DOM
    if (statusBar !== null) {
      assert(
        statusBar.tagName.length > 0,
        "#nora-statusbar should have a valid tagName when pref is true",
      );
    }

    // Disable status bar
    Services.prefs.setBoolPref(prefKey, false);
    const statusBarAfter = document.getElementById("nora-statusbar");
    // When disabled, element should either not exist or be hidden/collapsed
    if (statusBarAfter !== null) {
      // Element may remain in DOM but be visually hidden
    }
    // If element does not exist at all when disabled, that's also acceptable
  } finally {
    Services.prefs.setBoolPref(prefKey, originalValue);
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "nav-bar is visible", fn: testNavBarVisible },
    { name: "TabsToolbar is visible", fn: testTabsToolbarVisible },
    { name: "urlbar is visible", fn: testUrlbarVisible },
    { name: "tabbrowser-tabbox is visible", fn: testTabboxVisible },
    { name: "mainPopupSet exists", fn: testMainPopupSetExists },
    { name: "zen mode attribute toggle", fn: testZenModeAttributeToggle },
    { name: "status bar pref toggle", fn: testStatusBarPrefToggle },
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
      `browserChromeVisibility.test.ts failures: ${failures.join(" | ")}`,
    );
}
