// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { assert, runTests } from "../utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

const THEME_VARIABLE_CANDIDATES = [
  "--toolbar-bgcolor",
  "--toolbar-color",
  "--tab-selected-bgcolor",
  "--lwt-accent-color",
  "--lwt-text-color",
  "--arrowpanel-background",
  "--tabpanel-background-color",
  "--focus-outline-color",
] as const;

function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function waitForCondition(
  predicate: () => boolean,
  timeoutMs = 5000,
  intervalMs = 50,
): Promise<boolean> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    if (predicate()) {
      return true;
    }
    await sleep(intervalMs);
  }
  return predicate();
}

function getRootComputedStyle(): CSSStyleDeclaration {
  return globalThis.getComputedStyle(document.documentElement!)!;
}

function collectNonEmptyThemeVariables(
  style: CSSStyleDeclaration,
): string[] {
  return THEME_VARIABLE_CANDIDATES.filter(
    (name) => style.getPropertyValue(name).trim() !== "",
  );
}

function findNonEmptyThemeVariables(): string[] {
  const elements: Element[] = [document.documentElement!];
  for (const id of ["navigator-toolbox", "nav-bar", "browser", "TabsToolbar"]) {
    const element = document.getElementById(id);
    if (element) {
      elements.push(element);
    }
  }

  const found = new Set<string>();
  for (const element of elements) {
    const style = globalThis.getComputedStyle(element);
    if (!style) {
      continue;
    }
    for (const name of collectNonEmptyThemeVariables(style)) {
      found.add(name);
    }
  }
  return [...found];
}

// ---------------------------------------------------------------------------
// Tests — Standard Firefox CSS variables
// ---------------------------------------------------------------------------

function testToolbarBgColorVariable(): void {
  const style = getRootComputedStyle();
  const value = style.getPropertyValue("--toolbar-bgcolor");
  // In the browser chrome, --toolbar-bgcolor should be set by the theme
  assert(
    typeof value === "string",
    "--toolbar-bgcolor should be readable as a string",
  );
  // It may be empty if using a non-standard theme, but should not throw
}

function testToolbarColorVariable(): void {
  const style = getRootComputedStyle();
  const value = style.getPropertyValue("--toolbar-color");
  assert(
    typeof value === "string",
    "--toolbar-color should be readable as a string",
  );
}

function testTabSelectedBgColorVariable(): void {
  const style = getRootComputedStyle();
  const value = style.getPropertyValue("--tab-selected-bgcolor");
  assert(
    typeof value === "string",
    "--tab-selected-bgcolor should be readable",
  );
}

function testArrowpanelBgColorVariable(): void {
  const style = getRootComputedStyle();
  const value = style.getPropertyValue("--arrowpanel-background");
  assert(
    typeof value === "string",
    "--arrowpanel-background should be readable",
  );
}

// ---------------------------------------------------------------------------
// Tests — Zen mode CSS variable (conditional)
// ---------------------------------------------------------------------------

function testZenModeToolboxHeightVariable(): void {
  const isZenMode = document.documentElement!.hasAttribute("zenmode");
  if (!isZenMode) return; // zen mode not active — skip

  const style = getRootComputedStyle();
  const value = style.getPropertyValue("--zenmode-toolbox-height");
  assert(
    value !== "",
    "--zenmode-toolbox-height should be set when zen mode is active",
  );
}

// ---------------------------------------------------------------------------
// Tests — Panel sidebar CSS variable (conditional)
// ---------------------------------------------------------------------------

function testPanelSidebarDisplayVariable(): void {
  const sidebarBox = document.getElementById("panel-sidebar-box");
  if (!sidebarBox) return; // sidebar not present — skip

  const style = globalThis.getComputedStyle(sidebarBox);
  if (!style) return;
  const value = style.getPropertyValue("--panel-sidebar-display");
  // The variable controls sidebar visibility; it should be readable
  assert(
    typeof value === "string",
    "--panel-sidebar-display should be readable on the sidebar box",
  );
}

// ---------------------------------------------------------------------------
// Tests — getComputedStyle does not throw on common elements
// ---------------------------------------------------------------------------

function testComputedStyleOnNavBar(): void {
  const navBar = document.getElementById("nav-bar");
  if (!navBar) return;

  let threw = false;
  try {
    const style = globalThis.getComputedStyle(navBar)!;
    // Access a known property to ensure style resolution works
    style.getPropertyValue("background-color");
  } catch {
    threw = true;
  }
  assert(!threw, "getComputedStyle on #nav-bar should not throw");
}

function testComputedStyleOnTabsToolbar(): void {
  const toolbar = document.getElementById("TabsToolbar");
  if (!toolbar) return;

  let threw = false;
  try {
    const style = globalThis.getComputedStyle(toolbar)!;
    style.getPropertyValue("height");
  } catch {
    threw = true;
  }
  assert(!threw, "getComputedStyle on #TabsToolbar should not throw");
}

// ---------------------------------------------------------------------------
// Tests — CSS variable resolution returns non-empty for themed variables
// ---------------------------------------------------------------------------

async function testAtLeastOneThemeVariableIsSet(): Promise<void> {
  const hasThemeVariables = await waitForCondition(
    () => findNonEmptyThemeVariables().length > 0,
    5000,
    50,
  );
  assert(
    hasThemeVariables,
    "at least one standard theme CSS variable should have a non-empty value",
  );
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  await runTests("cssVariablesUI.test.ts", [
    { name: "--toolbar-bgcolor variable", fn: testToolbarBgColorVariable },
    { name: "--toolbar-color variable", fn: testToolbarColorVariable },
    {
      name: "--tab-selected-bgcolor variable",
      fn: testTabSelectedBgColorVariable,
    },
    {
      name: "--arrowpanel-background variable",
      fn: testArrowpanelBgColorVariable,
    },
    {
      name: "--zenmode-toolbox-height (conditional)",
      fn: testZenModeToolboxHeightVariable,
    },
    {
      name: "--panel-sidebar-display (conditional)",
      fn: testPanelSidebarDisplayVariable,
    },
    { name: "getComputedStyle on nav-bar", fn: testComputedStyleOnNavBar },
    {
      name: "getComputedStyle on TabsToolbar",
      fn: testComputedStyleOnTabsToolbar,
    },
    {
      name: "at least one theme variable is set",
      fn: testAtLeastOneThemeVariableIsSet,
    },
  ]);
}
