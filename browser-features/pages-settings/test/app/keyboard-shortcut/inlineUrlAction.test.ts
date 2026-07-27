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
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";
import {
  createDefaultKeyboardShortcutConfig,
  getKeyboardShortcutActionOptions,
  INLINE_TAB_URL_ACTION_ID,
  KEYBOARD_ONLY_ACTION_DEFINITIONS,
} from "../../../src/app/keyboard-shortcut/actionCatalog.ts";

function testKeyboardCatalogPreservesExistingOrderAndAddsInlineAction(): void {
  const labels: Record<string, string> = {
    [`keyboardShortcut.actionLabels.${INLINE_TAB_URL_ACTION_ID}`]:
      "localized inline action",
  };
  const existingOptions = [
    { id: "gecko-back", name: "localized back" },
    { id: "floorp-toggle-command-palette", name: "localized palette" },
  ];
  const actions = getKeyboardShortcutActionOptions(
    (key, fallback) => labels[key] ?? fallback,
    existingOptions,
  );
  assertEquals(
    JSON.stringify(actions),
    JSON.stringify([
      ...existingOptions,
      { id: INLINE_TAB_URL_ACTION_ID, name: "localized inline action" },
    ]),
    "existing actions should keep their order before keyboard-only actions",
  );
  assert(
    Object.isFrozen(KEYBOARD_ONLY_ACTION_DEFINITIONS),
    "mirrored keyboard-only metadata should be frozen",
  );
}

function testKeyboardCatalogDeduplicatesExistingAction(): void {
  const existingOptions = [
    { id: "gecko-back", name: "localized back" },
    { id: INLINE_TAB_URL_ACTION_ID, name: "existing inline action" },
  ];
  const actions = getKeyboardShortcutActionOptions(
    (_key, fallback) => fallback,
    existingOptions,
  );
  assertEquals(
    JSON.stringify(actions),
    JSON.stringify(existingOptions),
    "an existing action should not be appended a second time",
  );
}

function testPagesDefaultConfigIsUnchanged(): void {
  const firstEnabledDefault = createDefaultKeyboardShortcutConfig(true);
  const secondEnabledDefault = createDefaultKeyboardShortcutConfig(true);
  assertEquals(
    JSON.stringify(firstEnabledDefault),
    JSON.stringify({
      enabled: true,
      shortcuts: {
        "floorp-toggle-command-palette": {
          key: "F2",
          modifiers: { alt: false, ctrl: false, meta: false, shift: false },
          action: "floorp-toggle-command-palette",
        },
      },
    }),
    "enabled default config should retain the existing F2 binding only",
  );
  assertEquals(
    JSON.stringify(createDefaultKeyboardShortcutConfig(false)),
    JSON.stringify({
      enabled: false,
      shortcuts: {
        "floorp-toggle-command-palette": {
          key: "F2",
          modifiers: { alt: false, ctrl: false, meta: false, shift: false },
          action: "floorp-toggle-command-palette",
        },
      },
    }),
    "disabled default config should differ only by its enabled state",
  );
  assert(
    firstEnabledDefault !== secondEnabledDefault &&
      firstEnabledDefault.shortcuts !== secondEnabledDefault.shortcuts &&
      firstEnabledDefault.shortcuts["floorp-toggle-command-palette"] !==
        secondEnabledDefault.shortcuts["floorp-toggle-command-palette"],
    "each default config call should return fresh nested objects",
  );
}

function testPagesLocales(): void {
  assertEquals(
    enUS.keyboardShortcut.actionLabels[INLINE_TAB_URL_ACTION_ID],
    "Edit Tab URL",
    "English pages label should exist",
  );
  assertEquals(
    jaJP.keyboardShortcut.actionLabels[INLINE_TAB_URL_ACTION_ID],
    "タブの URL を編集",
    "Japanese pages label should exist",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "keyboard catalog preserves existing order and adds inline action",
      fn: testKeyboardCatalogPreservesExistingOrderAndAddsInlineAction,
    },
    {
      name: "keyboard catalog deduplicates existing action",
      fn: testKeyboardCatalogDeduplicatesExistingAction,
    },
    {
      name: "pages default config is unchanged",
      fn: testPagesDefaultConfigIsUnchanged,
    },
    { name: "EN/JA pages locales", fn: testPagesLocales },
  ];
  await runTests("inlineUrlAction.test.ts", tests);
}
