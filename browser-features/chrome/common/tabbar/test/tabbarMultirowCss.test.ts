// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { MULTIROW_TABBAR_BASE_CSS } from "../multirow-tabbar/multirow-tabbar-css.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** Extract all CSS selector blocks (text before `{`) from a stylesheet string. */
function extractSelectors(css: string): string[] {
  const selectors: string[] = [];
  // Match content before each `{` that is NOT inside a comment
  const cleaned = css.replace(/\/\*[\s\S]*?\*\//g, "");
  for (const match of cleaned.matchAll(/([^{}]+)\{/g)) {
    selectors.push(match[1].trim());
  }
  return selectors;
}

/** Check that a CSS declaration block contains a specific property-value pair. */
function cssHasDeclaration(css: string, property: string): boolean {
  // Match `property:` at word boundary, possibly followed by spaces/value/!important
  const regex = new RegExp(`\\b${property}\\s*:`, "i");
  return regex.test(css);
}

// ---------------------------------------------------------------------------
// Basic structure tests
// ---------------------------------------------------------------------------

function testMultirowCssIsNonEmpty(): void {
  assert(
    MULTIROW_TABBAR_BASE_CSS.trim().length > 0,
    "multirow base css should not be empty",
  );
}

function testMultirowCssIsString(): void {
  assertEquals(
    typeof MULTIROW_TABBAR_BASE_CSS,
    "string",
    "multirow base css should be exported as string",
  );
}

function testMultirowCssHasBalancedBraces(): void {
  const css = MULTIROW_TABBAR_BASE_CSS;
  const openBraces = (css.match(/{/g) ?? []).length;
  const closeBraces = (css.match(/}/g) ?? []).length;
  assertEquals(
    openBraces,
    closeBraces,
    "CSS should have balanced opening and closing braces",
  );
}

function testMultirowCssHasNoSyntaxErrors(): void {
  const css = MULTIROW_TABBAR_BASE_CSS;
  // Check for double semicolons or missing semicolons before closing braces
  // (a simple heuristic — not a full CSS parser)
  const ruleBlocks = css.split("}").slice(0, -1);
  for (const block of ruleBlocks) {
    const braceIndex = block.lastIndexOf("{");
    if (braceIndex === -1) continue;
    const body = block.slice(braceIndex + 1).trim();
    if (body.length === 0) continue; // empty rule body is ok
    // Each non-comment line should end with semicolon
    const lines = body
      .replace(/\/\*[\s\S]*?\*\//g, "")
      .split("\n")
      .map((l) => l.trim())
      .filter((l) => l.length > 0);
    for (const line of lines) {
      assert(
        line.endsWith(";"),
        `CSS declaration should end with semicolon: "${line}"`,
      );
    }
  }
}

// ---------------------------------------------------------------------------
// Selector coverage tests
// ---------------------------------------------------------------------------

function testContainsTabBrowserTabsSelector(): void {
  assert(
    MULTIROW_TABBAR_BASE_CSS.includes("#tabbrowser-tabs"),
    "should include #tabbrowser-tabs selector",
  );
}

function testContainsTabBrowserArrowscrollboxSelector(): void {
  assert(
    MULTIROW_TABBAR_BASE_CSS.includes("#tabbrowser-arrowscrollbox"),
    "should include #tabbrowser-arrowscrollbox selector",
  );
}

function testContainsTabbrowserTabSelector(): void {
  assert(
    MULTIROW_TABBAR_BASE_CSS.includes(".tabbrowser-tab"),
    "should include .tabbrowser-tab selector",
  );
}

function testContainsScrollboxSelector(): void {
  assert(
    MULTIROW_TABBAR_BASE_CSS.includes("scrollbox"),
    "should include scrollbox selector for flex wrapping",
  );
}

function testContainsDragTargetSelector(): void {
  assert(
    MULTIROW_TABBAR_BASE_CSS.includes("[dragtarget]"),
    "should include [dragtarget] attribute selector for drag styling",
  );
}

function testContainsPinnedTabSelector(): void {
  assert(
    MULTIROW_TABBAR_BASE_CSS.includes("[pinned]"),
    "should include [pinned] attribute selector for pinned tab styling",
  );
}

function testContainsOverflowSelector(): void {
  assert(
    MULTIROW_TABBAR_BASE_CSS.includes('[overflow="true"]'),
    "should include overflow attribute selector",
  );
}

function testContainsNewTabButtonSelector(): void {
  assert(
    MULTIROW_TABBAR_BASE_CSS.includes("#tabs-newtab-button"),
    "should include #tabs-newtab-button selector",
  );
}

function testContainsTitlebarButtonboxContainerSelector(): void {
  assert(
    MULTIROW_TABBAR_BASE_CSS.includes(".titlebar-buttonbox-container"),
    "should include .titlebar-buttonbox-container selector",
  );
}

function testContainsScrollboxClipSelector(): void {
  assert(
    MULTIROW_TABBAR_BASE_CSS.includes(".scrollbox-clip"),
    "should include .scrollbox-clip selector for scroll overflow control",
  );
}

function testContainsSlotSelector(): void {
  assert(
    MULTIROW_TABBAR_BASE_CSS.includes("slot"),
    "should include slot selector for flex-wrap propagation",
  );
}

function testContainsGroupLabelContainerSelector(): void {
  assert(
    MULTIROW_TABBAR_BASE_CSS.includes(".tab-group-label-container"),
    "should include .tab-group-label-container selector",
  );
}

function testContainsPeripherySelector(): void {
  assert(
    MULTIROW_TABBAR_BASE_CSS.includes("#tabbrowser-arrowscrollbox-periphery"),
    "should include #tabbrowser-arrowscrollbox-periphery selector",
  );
}

function testContainsPositionPinnedTabsSelector(): void {
  assert(
    MULTIROW_TABBAR_BASE_CSS.includes("[positionpinnedtabs]"),
    "should include [positionpinnedtabs] attribute selector",
  );
}

function testContainsTabsToolbarSelector(): void {
  assert(
    MULTIROW_TABBAR_BASE_CSS.includes("#TabsToolbar"),
    "should include #TabsToolbar selector",
  );
}

// ---------------------------------------------------------------------------
// CSS property / value tests
// ---------------------------------------------------------------------------

function testHasFlexWrap(): void {
  assert(
    cssHasDeclaration(MULTIROW_TABBAR_BASE_CSS, "flex-wrap"),
    "should include flex-wrap property for multi-row wrapping",
  );
}

function testHasDisplayFlex(): void {
  assert(
    MULTIROW_TABBAR_BASE_CSS.includes("display") &&
      MULTIROW_TABBAR_BASE_CSS.includes("flex"),
    "should include display: flex for scrollbox layout",
  );
}

function testHasOverflowVisible(): void {
  assert(
    MULTIROW_TABBAR_BASE_CSS.includes("overflow") &&
      MULTIROW_TABBAR_BASE_CSS.includes("visible"),
    "scrollbox-clip should have overflow: visible for multi-row",
  );
}

function testHasImportantDeclarations(): void {
  const importantCount = (MULTIROW_TABBAR_BASE_CSS.match(/!important/g) ?? [])
    .length;
  assert(
    importantCount > 0,
    "CSS should use !important to override Firefox defaults",
  );
}

function testHasMinHeight(): void {
  assert(
    cssHasDeclaration(MULTIROW_TABBAR_BASE_CSS, "min-height"),
    "should set min-height (using --tab-min-height var) for scrollbox",
  );
}

function testHasMaxHeightNone(): void {
  assert(
    MULTIROW_TABBAR_BASE_CSS.includes("max-height") &&
      MULTIROW_TABBAR_BASE_CSS.includes("none"),
    "#tabbrowser-tabs should have max-height: none to allow multi-row",
  );
}

function testHasPositionInitial(): void {
  assert(
    MULTIROW_TABBAR_BASE_CSS.includes("position") &&
      MULTIROW_TABBAR_BASE_CSS.includes("initial"),
    "should reset position to initial for tabs and pinned tabs",
  );
}

function testHasMarginInlineStartZero(): void {
  assert(
    MULTIROW_TABBAR_BASE_CSS.includes("margin-inline-start") &&
      MULTIROW_TABBAR_BASE_CSS.includes("0"),
    "should reset margin-inline-start to 0 for tab elements",
  );
}

function testUsesCSSCustomProperties(): void {
  assert(
    MULTIROW_TABBAR_BASE_CSS.includes("var(--"),
    "should use CSS custom properties (e.g. --tab-min-height)",
  );
}

// ---------------------------------------------------------------------------
// Edge case / quality tests
// ---------------------------------------------------------------------------

function testHasNoEmptyRules(): void {
  const selectors = extractSelectors(MULTIROW_TABBAR_BASE_CSS);
  for (const sel of selectors) {
    assert(
      sel.length > 0,
      "CSS should not contain empty selectors (between `}}`)",
    );
  }
}

function testCssHasScopedScrollboxRules(): void {
  // The scrollbox rules should be scoped to #tabbrowser-tabs / #tabbrowser-arrowscrollbox
  // to avoid affecting bookmark toolbar scrollboxes
  const css = MULTIROW_TABBAR_BASE_CSS;
  assert(
    css.includes("#tabbrowser-tabs > arrowscrollbox") ||
      css.includes("#tabbrowser-arrowscrollbox"),
    "scrollbox overflow rules should be scoped to tabbrowser context",
  );
}

function testZIndexOnDragTarget(): void {
  assert(
    MULTIROW_TABBAR_BASE_CSS.includes("z-index") &&
      MULTIROW_TABBAR_BASE_CSS.includes("[dragtarget]"),
    "drag target should have z-index for stacking during drag",
  );
}

function testTransformNoneOnPeriphery(): void {
  const css = MULTIROW_TABBAR_BASE_CSS;
  assert(
    css.includes("#tabbrowser-arrowscrollbox-periphery") &&
      css.includes("transform: none"),
    "periphery should have transform: none to prevent layout shift",
  );
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    // Basic structure
    { name: "CSS is non-empty", fn: testMultirowCssIsNonEmpty },
    { name: "CSS is a string", fn: testMultirowCssIsString },
    { name: "CSS has balanced braces", fn: testMultirowCssHasBalancedBraces },
    {
      name: "CSS has no obvious syntax errors",
      fn: testMultirowCssHasNoSyntaxErrors,
    },

    // Selector coverage
    {
      name: "contains #tabbrowser-tabs selector",
      fn: testContainsTabBrowserTabsSelector,
    },
    {
      name: "contains #tabbrowser-arrowscrollbox selector",
      fn: testContainsTabBrowserArrowscrollboxSelector,
    },
    {
      name: "contains .tabbrowser-tab selector",
      fn: testContainsTabbrowserTabSelector,
    },
    {
      name: "contains scrollbox selector",
      fn: testContainsScrollboxSelector,
    },
    {
      name: "contains [dragtarget] selector",
      fn: testContainsDragTargetSelector,
    },
    {
      name: "contains [pinned] selector",
      fn: testContainsPinnedTabSelector,
    },
    {
      name: "contains [overflow] selector",
      fn: testContainsOverflowSelector,
    },
    {
      name: "contains #tabs-newtab-button selector",
      fn: testContainsNewTabButtonSelector,
    },
    {
      name: "contains .titlebar-buttonbox-container selector",
      fn: testContainsTitlebarButtonboxContainerSelector,
    },
    {
      name: "contains .scrollbox-clip selector",
      fn: testContainsScrollboxClipSelector,
    },
    { name: "contains slot selector", fn: testContainsSlotSelector },
    {
      name: "contains .tab-group-label-container selector",
      fn: testContainsGroupLabelContainerSelector,
    },
    {
      name: "contains #tabbrowser-arrowscrollbox-periphery selector",
      fn: testContainsPeripherySelector,
    },
    {
      name: "contains [positionpinnedtabs] selector",
      fn: testContainsPositionPinnedTabsSelector,
    },
    {
      name: "contains #TabsToolbar selector",
      fn: testContainsTabsToolbarSelector,
    },

    // CSS property / value
    { name: "has flex-wrap property", fn: testHasFlexWrap },
    { name: "has display: flex", fn: testHasDisplayFlex },
    { name: "has overflow: visible", fn: testHasOverflowVisible },
    {
      name: "uses !important declarations",
      fn: testHasImportantDeclarations,
    },
    { name: "has min-height", fn: testHasMinHeight },
    { name: "has max-height: none", fn: testHasMaxHeightNone },
    { name: "has position: initial", fn: testHasPositionInitial },
    {
      name: "has margin-inline-start: 0",
      fn: testHasMarginInlineStartZero,
    },
    {
      name: "uses CSS custom properties",
      fn: testUsesCSSCustomProperties,
    },

    // Quality / edge cases
    { name: "has no empty rules", fn: testHasNoEmptyRules },
    {
      name: "scrollbox rules are scoped",
      fn: testCssHasScopedScrollboxRules,
    },
    { name: "z-index on drag target", fn: testZIndexOnDragTarget },
    {
      name: "transform: none on periphery",
      fn: testTransformNoneOnPeriphery,
    },
  ];

  await runTests("tabbarMultirowCss.test.ts", tests);
}
