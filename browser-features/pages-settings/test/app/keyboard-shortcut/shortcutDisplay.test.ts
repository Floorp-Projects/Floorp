// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import enUS from "../../../src/lib/i18n/locales/en-US.json" with {
  type: "json",
};
import jaJP from "../../../src/lib/i18n/locales/ja-JP.json" with {
  type: "json",
};
import {
  assert,
  assertEquals,
  assertStringArrayEqual,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";
import type { FirefoxKeyEntry } from "../../../../modules/common/NRKeySyncTypes.ts";
import type { ShortcutConfig } from "../../../src/types/pref.ts";
import {
  buildUnifiedShortcutRows,
  filterActionsForFirefoxPriority,
  filterUnifiedShortcutRows,
  getFirefoxKeyActionId,
  getFloorpCanonicalCode,
  groupActionsByCategory,
  groupUnifiedShortcutRows,
} from "../../../src/app/keyboard-shortcut/shortcutDisplay.ts";

function createShortcut(
  key: string,
  modifiers: Partial<ShortcutConfig["modifiers"]> = {},
): ShortcutConfig {
  return {
    key,
    modifiers: {
      alt: false,
      ctrl: true,
      meta: false,
      shift: false,
      ...modifiers,
    },
    action: "gecko-back",
  };
}

function createFirefoxKey(
  canonicalCode: string | null,
  overrides: Partial<FirefoxKeyEntry> = {},
): FirefoxKeyEntry {
  return {
    id: "key_test",
    keysetKind: "main",
    modifiers: "accel",
    key: "T",
    keycode: "",
    command: "",
    internal: false,
    reserved: false,
    customized: false,
    label: "Test",
    shortcutText: "Ctrl+T",
    canonicalCode,
    ...overrides,
  };
}

function testFirefoxKeyMappingUsesStableIdsAndCommands(): void {
  assertEquals(
    getFirefoxKeyActionId(
      createFirefoxKey("alt+ArrowLeft", {
        id: "key_back",
        command: "Unexpected:Command",
      }),
    ),
    "gecko-back",
    "Firefox key ID should have mapping priority",
  );
  assertEquals(
    getFirefoxKeyActionId(
      createFirefoxKey("ctrl+KeyR", {
        id: "key_custom_reload",
        command: "Browser:Reload",
      }),
    ),
    "gecko-reload",
    "stable Firefox command IDs should provide a fallback mapping",
  );
  assertEquals(
    getFirefoxKeyActionId(
      createFirefoxKey("ctrl+KeyX", {
        id: "key_unknown",
        command: "Browser:Unknown",
      }),
    ),
    null,
    "unknown Firefox keys should remain Firefox-specific",
  );
}

function testUnifiedRowsUseOneCanonicalNamePerSource(): void {
  const actions = [
    { id: "gecko-back", name: "Go Back" },
    { id: "gecko-reload", name: "Reload" },
  ];
  const firefoxKey = createFirefoxKey("alt+ArrowLeft", {
    id: "key_back",
    command: "Browser:Back",
    label: "Back",
  });

  const result = buildUnifiedShortcutRows(actions, {}, [firefoxKey]);

  assertStringArrayEqual(
    result.rows.map((row) => `${row.source}:${row.name}`),
    [
      "firefox:Go Back",
      "floorp:Reload",
    ],
    "a mapped Firefox key should replace an unconfigured Floorp add row",
  );
}

function testMultipleFloorpBindingsRenderPerBindingRows(): void {
  const actions = [{ id: "gecko-back", name: "Go Back" }];
  const shortcuts = {
    "gecko-back": [
      createShortcut("KeyB"),
      createShortcut("ArrowLeft", { ctrl: false, alt: true }),
    ],
  };
  const firefoxKey = createFirefoxKey("alt+ArrowLeft", {
    id: "key_back",
    command: "Browser:Back",
    label: "Back",
  });

  const result = buildUnifiedShortcutRows(actions, shortcuts, [firefoxKey]);
  const floorpRows = result.rows.filter((row) => row.source === "floorp");

  assertEquals(
    result.hiddenActionCount,
    1,
    "only the Firefox-owned binding should be hidden",
  );
  assertEquals(
    floorpRows.length,
    2,
    "one binding row and one add row should remain",
  );
  assertEquals(
    floorpRows[0].shortcut?.key,
    "KeyB",
    "non-conflicting binding should remain",
  );
  assertEquals(floorpRows[0].shortcutIndex, 0, "binding index is preserved");
  assertEquals(floorpRows[1].shortcut, undefined, "add row has no binding");
  assertEquals(floorpRows[1].key, "floorp-add:gecko-back", "add row is stable");
}

function testUnifiedRowsKeepFirefoxPriorityBehavior(): void {
  const actions = [
    { id: "gecko-back", name: "Go Back" },
    { id: "gecko-forward", name: "Go Forward" },
  ];
  const shortcuts = {
    "gecko-back": [createShortcut("KeyT")],
    "gecko-forward": [createShortcut("KeyR")],
  };
  const firefoxKey = createFirefoxKey("ctrl+KeyT", {
    id: "key_back",
    command: "Browser:Back",
    label: "Back",
  });

  const result = buildUnifiedShortcutRows(actions, shortcuts, [firefoxKey]);
  const backRows = result.rows.filter((row) => row.actionId === "gecko-back");

  assertEquals(
    result.hiddenActionCount,
    1,
    "the conflicting Floorp action should remain hidden",
  );
  assertStringArrayEqual(
    backRows.map((row) => row.source),
    ["firefox"],
    "Firefox should remain represented when it owns the configured combo",
  );
  assert(
    result.rows.some((row) =>
      row.source === "floorp" && row.actionId === "gecko-forward"
    ),
    "non-conflicting Floorp actions should stay visible",
  );
}

function testInternalFirefoxRowsStayVisible(): void {
  const actions = [{ id: "gecko-back", name: "Go Back" }];
  const internalKey = createFirefoxKey("ctrl+KeyT", {
    id: "key_idless_internal",
    command: "",
    internal: true,
  });

  const result = buildUnifiedShortcutRows(actions, {}, [internalKey]);

  assertEquals(
    result.rows.length,
    1,
    "internal Firefox shortcuts should remain visible",
  );
  assertEquals(
    result.rows[0].source,
    "firefox",
    "the visible row should be Firefox-owned",
  );
  assert(result.rows[0].firefoxKey?.internal, "the row should be internal");
}

function testFloorpCanonicalCodeNormalizesSimpleKeys(): void {
  assertEquals(
    getFloorpCanonicalCode(createShortcut("T")),
    "ctrl+KeyT",
    "letters should be normalized into event.code space",
  );
  assertEquals(
    getFloorpCanonicalCode(createShortcut("KeyT")),
    "ctrl+KeyT",
    "already normalized keys should be preserved",
  );
}

function testFirefoxPriorityHidesConfiguredAndEquivalentDuplicates(): void {
  const actions = [
    { id: "gecko-back", name: "Back" },
    { id: "gecko-open-new-tab", name: "New Tab" },
    { id: "gecko-forward", name: "Forward" },
    { id: "gecko-reload", name: "Reload" },
  ];
  const shortcuts = {
    "gecko-back": [createShortcut("KeyT")],
    "gecko-forward": [createShortcut("KeyR")],
  };
  const result = filterActionsForFirefoxPriority(actions, shortcuts, [
    createFirefoxKey("ctrl+KeyT"),
    createFirefoxKey(null, {
      id: "key_newNavigatorTab",
      command: "cmd_newNavigatorTab",
      shortcutText: "",
    }),
  ]);

  assertStringArrayEqual(
    result.visibleActions.map((action) => action.id),
    ["gecko-forward", "gecko-reload"],
    "only the configured Firefox duplicate should be hidden",
  );
  assertEquals(
    result.hiddenActionCount,
    2,
    "configured and equivalent duplicates should be counted",
  );
  assertEquals(
    result.visibleBindingsByAction["gecko-back"].length,
    0,
    "conflicting binding should be hidden",
  );
  assertEquals(
    result.visibleBindingsByAction["gecko-forward"].length,
    1,
    "non-conflicting binding should be visible",
  );
}

function testActionsAreGroupedWithoutEmptyCategories(): void {
  const actions = [
    { id: "gecko-open-new-tab", name: "New tab" },
    { id: "gecko-open-new-window", name: "New window" },
    { id: "floorp-toggle-zen-mode", name: "Zen mode" },
  ];
  const categories = groupActionsByCategory(actions);

  assertStringArrayEqual(
    categories.map((category) => category.id),
    ["tabs", "windows", "floorp"],
    "empty categories should not be rendered",
  );
  assertEquals(
    categories[0].actions[0].id,
    "gecko-open-new-tab",
    "category contents should preserve source order",
  );
}

function testShortcutRowsMatchNamesAndShortcutSeparators(): void {
  const categories = groupUnifiedShortcutRows([
    {
      key: "floorp:gecko-back:0",
      source: "floorp",
      actionId: "gecko-back",
      name: "Go Back",
      category: "page",
      shortcut: createShortcut("KeyT"),
    },
  ]);
  const translate = (key: string): string =>
    key === "keyboardShortcut.categories.page" ? "Page" : key;

  assertStringArrayEqual(
    filterUnifiedShortcutRows(categories, "go back", translate).flatMap(
      (category) => category.actions.map((row) => row.name),
    ),
    ["Go Back"],
    "action names should be searchable",
  );
  assertStringArrayEqual(
    filterUnifiedShortcutRows(categories, "ctrl+t", translate).flatMap(
      (category) => category.actions.map((row) => row.name),
    ),
    ["Go Back"],
    "+ in shortcut queries should behave like whitespace",
  );
  assertEquals(
    filterUnifiedShortcutRows(categories, "bookmark", translate).length,
    0,
    "non-matching queries should remove empty categories",
  );
}

function testShortcutDisplayLocales(): void {
  const categoryKeys = [
    "tabs",
    "windows",
    "page",
    "bookmarksAndHistory",
    "appearance",
    "scrolling",
    "floorp",
    "tools",
    "other",
  ] as const;

  for (
    const [localeName, locale] of [
      ["en-US", enUS],
      ["ja-JP", jaJP],
    ] as const
  ) {
    for (const key of categoryKeys) {
      assert(
        typeof locale.keyboardShortcut.categories[key] === "string",
        `${localeName} should define category ${key}`,
      );
    }
    assert(
      locale.keyboardShortcut.hiddenByFirefox.includes(
        "{{replacementCount}}",
      ),
      `${localeName} should include the hidden count`,
    );
    assertStringArrayEqual(
      [
        locale.keyboardShortcut.source.title,
        locale.keyboardShortcut.source.floorp,
        locale.keyboardShortcut.source.firefox,
      ],
      [
        localeName === "ja-JP" ? "ソース" : "Source",
        "Floorp",
        "Firefox",
      ],
      `${localeName} should define unified-table source labels`,
    );
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "Floorp canonical code normalizes simple keys",
      fn: testFloorpCanonicalCodeNormalizesSimpleKeys,
    },
    {
      name: "Firefox key mapping uses stable IDs and commands",
      fn: testFirefoxKeyMappingUsesStableIdsAndCommands,
    },
    {
      name: "unified rows use one canonical name per function",
      fn: testUnifiedRowsUseOneCanonicalNamePerSource,
    },
    {
      name: "multiple Floorp bindings render per-binding rows",
      fn: testMultipleFloorpBindingsRenderPerBindingRows,
    },
    {
      name: "unified rows keep Firefox priority behavior",
      fn: testUnifiedRowsKeepFirefoxPriorityBehavior,
    },
    {
      name: "internal Firefox rows stay visible",
      fn: testInternalFirefoxRowsStayVisible,
    },
    {
      name: "Firefox priority hides configured and equivalent duplicates",
      fn: testFirefoxPriorityHidesConfiguredAndEquivalentDuplicates,
    },
    {
      name: "shortcut rows match names and shortcut separators",
      fn: testShortcutRowsMatchNamesAndShortcutSeparators,
    },
    {
      name: "actions are grouped without empty categories",
      fn: testActionsAreGroupedWithoutEmptyCategories,
    },
    { name: "shortcut display locales", fn: testShortcutDisplayLocales },
  ];
  await runTests("shortcutDisplay.test.ts", tests);
}
