// SPDX-License-Identifier: MPL-2.0
// @colocated-env host

import { TAB_COLOR_LIKE_TOOLBAR_CSS } from "../utils/tab-color-like-toolbar.css.ts";
import { assert, runTests } from "../../../test/utils/test_harness.ts";

function testContainsTabSelectedBgcolorUnset(): void {
  assert(
    TAB_COLOR_LIKE_TOOLBAR_CSS.includes("--tab-selected-bgcolor: unset"),
    "should unset --tab-selected-bgcolor for system themes",
  );
}

function testContainsSelectedTabBackground(): void {
  assert(
    TAB_COLOR_LIKE_TOOLBAR_CSS.includes(
      ".tab-background:is([selected], [multiselected])",
    ),
    "should target selected tab backgrounds",
  );
}

function testUsesToolbarBackgroundColor(): void {
  assert(
    TAB_COLOR_LIKE_TOOLBAR_CSS.includes("--toolbar-background-color"),
    "should use Gecko 152 --toolbar-background-color",
  );
}

function testPrefersLegacyToolbarBgcolor(): void {
  assert(
    /var\(\s*--toolbar-bgcolor\s*,\s*var\(\s*--toolbar-background-color\s*\)\s*\)/
      .test(TAB_COLOR_LIKE_TOOLBAR_CSS),
    "should prefer legacy --toolbar-bgcolor before Gecko 152 --toolbar-background-color",
  );
}

function testTargetsTabContent(): void {
  assert(
    TAB_COLOR_LIKE_TOOLBAR_CSS.includes("> .tab-content"),
    "should paint selected tab via .tab-content",
  );
}

function testDoesNotModifyNavBar(): void {
  assert(
    !TAB_COLOR_LIKE_TOOLBAR_CSS.includes("#nav-bar"),
    "should not change nav-bar; tabs follow toolbar color only",
  );
}

export async function runAllTests(): Promise<void> {
  await runTests("tab-color-like-toolbar.test.ts", [
    {
      name: "unsets tab-selected-bgcolor",
      fn: testContainsTabSelectedBgcolorUnset,
    },
    {
      name: "targets selected tab-background",
      fn: testContainsSelectedTabBackground,
    },
    {
      name: "uses toolbar-background-color",
      fn: testUsesToolbarBackgroundColor,
    },
    {
      name: "prefers toolbar-bgcolor fallback order",
      fn: testPrefersLegacyToolbarBgcolor,
    },
    {
      name: "targets selected tab-content",
      fn: testTargetsTabContent,
    },
    {
      name: "does not modify nav-bar",
      fn: testDoesNotModifyNavBar,
    },
  ]);
}
