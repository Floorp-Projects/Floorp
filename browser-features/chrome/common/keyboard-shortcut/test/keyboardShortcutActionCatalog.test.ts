// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import enUS from "#i18n/en-US/browser-chrome.json" with {
  type: "json",
};
import jaJP from "#i18n/ja-JP/browser-chrome.json" with {
  type: "json",
};
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import {
  getKeyboardOnlyActions,
  getKeyboardShortcutAction,
  INLINE_TAB_URL_ACTION_ID,
  KEYBOARD_ONLY_ACTIONS,
} from "../actions.ts";
import { defaultConfig } from "../config.ts";
import {
  gestureActions,
  getAllGestureActions,
} from "../../mouse-gesture/utils/gestures.ts";
import { getPaletteCommands } from "../../command-palette/command-registry.ts";

function testFrozenKeyboardOnlyCatalog(): void {
  assertEquals(
    INLINE_TAB_URL_ACTION_ID,
    "floorp-edit-tab-url",
    "inline URL action ID must remain stable",
  );
  assert(Object.isFrozen(KEYBOARD_ONLY_ACTIONS), "catalog should be frozen");

  const matches = getKeyboardOnlyActions().filter((action) =>
    action.name === INLINE_TAB_URL_ACTION_ID
  );
  assertEquals(matches.length, 1, "catalog should contain the action once");
  assert(
    typeof getKeyboardShortcutAction(INLINE_TAB_URL_ACTION_ID) === "function",
    "keyboard controller should resolve the keyboard-only action",
  );
}

function testNoDefaultBinding(): void {
  assert(
    !Object.hasOwn(defaultConfig.shortcuts, INLINE_TAB_URL_ACTION_ID),
    "inline URL action must not ship with a default shortcut",
  );
  assert(
    !Object.values(defaultConfig.shortcuts).some((shortcut) =>
      shortcut.action === INLINE_TAB_URL_ACTION_ID
    ),
    "inline URL action must not appear in any shipped default binding",
  );
}

function testAbsentFromGestureRegistry(): void {
  assertEquals(
    gestureActions.getAction(INLINE_TAB_URL_ACTION_ID),
    undefined,
    "keyboard-only action must not resolve from gestureActions",
  );
  assert(
    !getAllGestureActions().some((action) =>
      action.name === INLINE_TAB_URL_ACTION_ID
    ),
    "keyboard-only action must not enter the gesture catalog",
  );
}

function testAbsentFromCommandPalette(): void {
  assert(
    !getPaletteCommands().some((command) =>
      command.id === INLINE_TAB_URL_ACTION_ID
    ),
    "keyboard-only action must not enter the command palette",
  );
}

function testFailsClosedWithoutEnabledController(): void {
  const action = getKeyboardShortcutAction(INLINE_TAB_URL_ACTION_ID);
  assert(action, "keyboard-only action should resolve");
  const detachedWindow = new EventTarget() as unknown as Window;
  action(detachedWindow);
}

function testChromeLocales(): void {
  assertEquals(
    enUS.tabInlineEdit.keyboardAction,
    "Edit Tab URL",
    "English chrome action label should exist",
  );
  assertEquals(
    enUS.tabInlineEdit.inputLabel,
    "Tab URL",
    "English accessible input label should exist",
  );
  assertEquals(
    jaJP.tabInlineEdit.keyboardAction,
    "タブの URL を編集",
    "Japanese chrome action label should exist",
  );
  assertEquals(
    jaJP.tabInlineEdit.inputLabel,
    "タブの URL",
    "Japanese accessible input label should exist",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "frozen keyboard-only catalog", fn: testFrozenKeyboardOnlyCatalog },
    { name: "no default binding", fn: testNoDefaultBinding },
    { name: "absent from gesture registry", fn: testAbsentFromGestureRegistry },
    { name: "absent from command palette", fn: testAbsentFromCommandPalette },
    {
      name: "fails closed without enabled controller",
      fn: testFailsClosedWithoutEnabledController,
    },
    { name: "EN/JA chrome locales", fn: testChromeLocales },
  ];
  await runTests("keyboardShortcutActionCatalog.test.ts", tests);
}
