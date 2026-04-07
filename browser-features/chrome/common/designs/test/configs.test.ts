// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  deepMerge,
  isPlainObject,
  getOldUICustomizationConfig,
  createDefaultOldObjectConfigs,
  getOldConfigs,
  getUICustomizationSetting,
} from "../configs.ts";
import {
  assertEquals,
  assert,
  runTests,
} from "../../../test/utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Helpers — set/clear prefs safely
// ---------------------------------------------------------------------------

function withBoolPref(prefName: string, value: boolean, fn: () => void): void {
  Services.prefs.setBoolPref(prefName, value);
  try {
    fn();
  } finally {
    Services.prefs.clearUserPref(prefName);
  }
}

function withBoolPrefs(prefs: [string, boolean][], fn: () => void): void {
  for (const [name, value] of prefs) {
    Services.prefs.setBoolPref(name, value);
  }
  try {
    fn();
  } finally {
    for (const [name] of prefs) {
      Services.prefs.clearUserPref(name);
    }
  }
}

function withIntPrefs(prefs: [string, number][], fn: () => void): void {
  for (const [name, value] of prefs) {
    Services.prefs.setIntPref(name, value);
  }
  try {
    fn();
  } finally {
    for (const [name] of prefs) {
      Services.prefs.clearUserPref(name);
    }
  }
}

// ---------------------------------------------------------------------------
// Tests — isPlainObject
// ---------------------------------------------------------------------------

function testIsPlainObjectTrue(): void {
  assert(isPlainObject({}), "empty object should be plain");
  assert(isPlainObject({ a: 1 }), "object with props should be plain");
  assert(
    isPlainObject(Object.create(null)),
    "null-prototype object should be plain",
  );
}

function testIsPlainObjectFalse(): void {
  assert(!isPlainObject(null), "null should not be plain");
  assert(!isPlainObject(undefined), "undefined should not be plain");
  assert(!isPlainObject([]), "array should not be plain");
  assert(!isPlainObject("string"), "string should not be plain");
  assert(!isPlainObject(42), "number should not be plain");
  assert(!isPlainObject(true), "boolean should not be plain");
}

// ---------------------------------------------------------------------------
// Tests — deepMerge
// ---------------------------------------------------------------------------

function testDeepMergeOverridePrimitive(): void {
  const base = { a: 1, b: "hello" };
  const override = { a: 2 };
  const result = deepMerge(base, override);
  assertEquals(result.a, 2, "primitive should be overridden");
  assertEquals(result.b, "hello", "non-overridden key preserved");
}

function testDeepMergeNestedObject(): void {
  const base = { nested: { x: 1, y: 2 } };
  const override = { nested: { y: 3 } };
  const result = deepMerge(base, override);
  assertEquals(result.nested.x, 1, "non-overridden nested key preserved");
  assertEquals(result.nested.y, 3, "nested key overridden");
}

function testDeepMergeNoMutation(): void {
  const base = { a: { b: 1 } };
  const override = { a: { b: 2 } };
  deepMerge(base, override);
  assertEquals(base.a.b, 1, "base should not be mutated");
}

function testDeepMergeUndefinedOverride(): void {
  const base = { a: 1 };
  const override = { a: undefined };
  const result = deepMerge(base, override);
  assertEquals(result.a, 1, "undefined override should not change base value");
}

function testDeepMergeNonObjectOverride(): void {
  const base = { a: 1 };
  const result = deepMerge(base, "not an object");
  assertEquals(result.a, 1, "non-object override should return base");
}

function testDeepMergeDeeplyNested(): void {
  const base = { l1: { l2: { l3: { value: "original" } } } };
  const override = { l1: { l2: { l3: { value: "overridden" } } } };
  const result = deepMerge(base, override);
  assertEquals(
    result.l1.l2.l3.value,
    "overridden",
    "deeply nested value should be overridden",
  );
}

function testDeepMergeNewKeys(): void {
  const base = { a: 1 };
  const override = { b: 2 };
  const result = deepMerge(base, override);
  assertEquals(result.a, 1, "base key preserved");
  assertEquals((result as Record<string, unknown>).b, 2, "new key added");
}

// ---------------------------------------------------------------------------
// Tests — getOldUICustomizationConfig
// ---------------------------------------------------------------------------

function testUICustomizationDefaultNavbarTop(): void {
  const result = getOldUICustomizationConfig();
  assertEquals(result.navbar.position, "top", "default navbar should be top");
  assertEquals(
    result.navbar.searchBarTop,
    false,
    "default searchBarTop should be false",
  );
}

function testUICustomizationNavbarBottom(): void {
  withBoolPref("floorp.navbar.bottom", true, () => {
    const result = getOldUICustomizationConfig();
    assertEquals(
      result.navbar.position,
      "bottom",
      "navbar bottom pref should set position to bottom",
    );
  });
}

function testUICustomizationSearchBarTop(): void {
  withBoolPref("floorp.search.top.mode", true, () => {
    const result = getOldUICustomizationConfig();
    assertEquals(
      result.navbar.searchBarTop,
      true,
      "search bar top should be true",
    );
  });
}

function testUICustomizationDisplayPrefs(): void {
  withBoolPrefs(
    [
      ["floorp.disable.fullscreen.notification", true],
      ["floorp.delete.browser.border", true],
    ],
    () => {
      const result = getOldUICustomizationConfig();
      assertEquals(
        result.display.disableFullscreenNotification,
        true,
        "fullscreen notification should be disabled",
      );
      assertEquals(
        result.display.deleteBrowserBorder,
        true,
        "browser border should be deleted",
      );
    },
  );
}

function testUICustomizationSpecialPrefs(): void {
  withBoolPrefs(
    [
      ["floorp.Tree-type.verticaltab.optimization", true],
      ["floorp.optimized.msbutton.ope", true],
      ["floorp.extensions.STG.like.floorp.workspaces.enabled", true],
    ],
    () => {
      const result = getOldUICustomizationConfig();
      assertEquals(
        result.special.optimizeForTreeStyleTab,
        true,
        "tree style tab optimization should be true",
      );
      assertEquals(
        result.special.hideForwardBackwardButton,
        true,
        "hide forward/backward should be true",
      );
      assertEquals(
        result.special.stgLikeWorkspaces,
        true,
        "STG like workspaces should be true",
      );
    },
  );
}

function testUICustomizationMultirowNewtab(): void {
  withBoolPref(
    "floorp.browser.tabbar.multirow.newtab-inside.enabled",
    true,
    () => {
      const result = getOldUICustomizationConfig();
      assertEquals(
        result.multirowTab.newtabInsideEnabled,
        true,
        "newtab inside should be enabled",
      );
    },
  );
}

// ---------------------------------------------------------------------------
// Tests — createDefaultOldObjectConfigs
// ---------------------------------------------------------------------------

function testDefaultConfigStructure(): void {
  const config = createDefaultOldObjectConfigs();
  assert(
    typeof config.globalConfigs === "object",
    "globalConfigs should be an object",
  );
  assert(typeof config.tabbar === "object", "tabbar should be an object");
  assert(typeof config.tab === "object", "tab should be an object");
  assert(
    typeof config.uiCustomization === "object",
    "uiCustomization should be an object",
  );
}

function testDefaultConfigGlobalConfigs(): void {
  const config = createDefaultOldObjectConfigs();
  assertEquals(
    typeof config.globalConfigs.userInterface,
    "string",
    "userInterface should be a string",
  );
  assertEquals(
    typeof config.globalConfigs.faviconColor,
    "boolean",
    "faviconColor should be a boolean",
  );
  assertEquals(
    config.globalConfigs.appliedUserJs,
    "",
    "appliedUserJs should default to empty string",
  );
}

function testDefaultConfigTabbarDefaults(): void {
  const config = createDefaultOldObjectConfigs();
  assertEquals(
    config.tabbar.tabbarStyle,
    "horizontal",
    "default tabbar style should be horizontal",
  );
  assertEquals(
    config.tabbar.tabbarPosition,
    "default",
    "default tabbar position should be default",
  );
  assertEquals(
    config.tabbar.multiRowTabBar.maxRow,
    3,
    "default max row should be 3",
  );
}

function testDefaultConfigTabDefaults(): void {
  const config = createDefaultOldObjectConfigs();
  assertEquals(
    config.tab.tabMinHeight,
    30,
    "default tab min height should be 30",
  );
  assertEquals(
    config.tab.tabMinWidth,
    76,
    "default tab min width should be 76",
  );
  assertEquals(
    config.tab.tabOpenPosition,
    -1,
    "default tab open position should be -1",
  );
}

function testDefaultConfigWithPrefs(): void {
  withIntPrefs(
    [
      ["floorp.browser.user.interface", 8],
      ["floorp.tabbar.style", 2],
      ["floorp.browser.tabbar.settings", 4],
    ],
    () => {
      const config = createDefaultOldObjectConfigs();
      assertEquals(
        config.globalConfigs.userInterface,
        "fluerial",
        "interface=8 should give fluerial",
      );
      assertEquals(
        config.tabbar.tabbarStyle,
        "vertical",
        "style=2 should give vertical",
      );
      assertEquals(
        config.tabbar.tabbarPosition,
        "bottom-of-window",
        "settings=4 should give bottom-of-window",
      );
    },
  );
}

// ---------------------------------------------------------------------------
// Tests — getOldConfigs (JSON string)
// ---------------------------------------------------------------------------

function testGetOldConfigsIsParsable(): void {
  const parsed = JSON.parse(getOldConfigs);
  assert(
    typeof parsed === "object",
    "getOldConfigs should be valid JSON object",
  );
  assert(
    typeof parsed.globalConfigs === "object",
    "parsed config should have globalConfigs",
  );
}

// ---------------------------------------------------------------------------
// Tests — getUICustomizationSetting
// ---------------------------------------------------------------------------

function testGetUICustomizationExistingSetting(): void {
  // The config should have a navbar with position "top" by default
  const result = getUICustomizationSetting("navbar", "position", "fallback");
  assertEquals(result, "top", "should return existing navbar position");
}

function testGetUICustomizationMissingCategory(): void {
  const result = getUICustomizationSetting(
    "nonExistentCategory",
    "someKey",
    "defaultValue",
  );
  assertEquals(
    result,
    "defaultValue",
    "missing category should return default",
  );
}

function testGetUICustomizationMissingSetting(): void {
  const result = getUICustomizationSetting("navbar", "nonExistentKey", 42);
  assertEquals(result, 42, "missing setting should return default");
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  await runTests("configs.test.ts", [
    // isPlainObject
    { name: "isPlainObject true cases", fn: testIsPlainObjectTrue },
    { name: "isPlainObject false cases", fn: testIsPlainObjectFalse },
    // deepMerge
    {
      name: "deepMerge override primitive",
      fn: testDeepMergeOverridePrimitive,
    },
    { name: "deepMerge nested object", fn: testDeepMergeNestedObject },
    { name: "deepMerge no mutation", fn: testDeepMergeNoMutation },
    {
      name: "deepMerge undefined override",
      fn: testDeepMergeUndefinedOverride,
    },
    {
      name: "deepMerge non-object override",
      fn: testDeepMergeNonObjectOverride,
    },
    { name: "deepMerge deeply nested", fn: testDeepMergeDeeplyNested },
    { name: "deepMerge new keys", fn: testDeepMergeNewKeys },
    // getOldUICustomizationConfig
    {
      name: "UI customization default navbar",
      fn: testUICustomizationDefaultNavbarTop,
    },
    {
      name: "UI customization navbar bottom",
      fn: testUICustomizationNavbarBottom,
    },
    {
      name: "UI customization search bar top",
      fn: testUICustomizationSearchBarTop,
    },
    {
      name: "UI customization display prefs",
      fn: testUICustomizationDisplayPrefs,
    },
    {
      name: "UI customization special prefs",
      fn: testUICustomizationSpecialPrefs,
    },
    {
      name: "UI customization multirow newtab",
      fn: testUICustomizationMultirowNewtab,
    },
    // createDefaultOldObjectConfigs
    { name: "default config structure", fn: testDefaultConfigStructure },
    {
      name: "default config globalConfigs",
      fn: testDefaultConfigGlobalConfigs,
    },
    {
      name: "default config tabbar defaults",
      fn: testDefaultConfigTabbarDefaults,
    },
    { name: "default config tab defaults", fn: testDefaultConfigTabDefaults },
    { name: "default config with prefs", fn: testDefaultConfigWithPrefs },
    // getOldConfigs
    { name: "getOldConfigs is parsable", fn: testGetOldConfigsIsParsable },
    // getUICustomizationSetting
    { name: "get setting existing", fn: testGetUICustomizationExistingSetting },
    {
      name: "get setting missing category",
      fn: testGetUICustomizationMissingCategory,
    },
    {
      name: "get setting missing key",
      fn: testGetUICustomizationMissingSetting,
    },
  ]);
}
