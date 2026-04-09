// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { assert, type TestCase } from "../utils/test_harness.ts";

const ESSENTIAL_ELEMENTS: { id: string; description: string }[] = [
  { id: "main-window", description: "root window" },
  { id: "navigator-toolbox", description: "toolbar container" },
  { id: "nav-bar", description: "navigation bar" },
  { id: "urlbar", description: "URL bar" },
  { id: "PersonalToolbar", description: "bookmarks bar" },
  { id: "TabsToolbar", description: "tab toolbar" },
  { id: "tabbrowser-tabs", description: "tabs container" },
  { id: "tabbrowser-tabbox", description: "tab content box" },
  { id: "browser", description: "browser content" },
  { id: "sidebar-box", description: "Firefox sidebar box" },
  { id: "mainPopupSet", description: "popup container" },
];

const OPTIONAL_ELEMENTS: { id: string; description: string }[] = [
  { id: "PanelUI-menu-button", description: "hamburger menu" },
];

function buildElementExistsTest(id: string, description: string): TestCase {
  return {
    name: `#${id} (${description}) exists in DOM`,
    fn: () => {
      const el = document?.getElementById(id) ?? null;
      assert(el !== null, `#${id} (${description}) should exist in the DOM`);
      assert(
        el.tagName !== undefined && el.tagName.length > 0,
        `#${id} should have a non-empty tagName`,
      );
    },
  };
}

function buildOptionalElementValidityTest(
  id: string,
  description: string,
): TestCase {
  return {
    name: `#${id} (${description}) is valid when present`,
    fn: () => {
      const el = document?.getElementById(id) ?? null;
      if (!el) {
        return;
      }
      assert(
        el.tagName !== undefined && el.tagName.length > 0,
        `#${id} should have a non-empty tagName`,
      );
    },
  };
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    ...ESSENTIAL_ELEMENTS.map((e) =>
      buildElementExistsTest(e.id, e.description),
    ),
    ...OPTIONAL_ELEMENTS.map((e) =>
      buildOptionalElementValidityTest(e.id, e.description),
    ),
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
      `browserChromeElements.test.ts failures: ${failures.join(" | ")}`,
    );
}
