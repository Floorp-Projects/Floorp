// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import enUS from "../../../src/lib/i18n/locales/en-US.json" with {
  type: "json",
};
import jaJP from "../../../src/lib/i18n/locales/ja-JP.json" with {
  type: "json",
};
import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";
import {
  DEFAULT_OPEN_NEW_WINDOW,
  normalizeOpenNewWindowValue,
} from "../../../src/app/design/tabWindowBehavior.ts";

function testOpenNewWindowValuesAreNormalized(): void {
  assertEquals(
    normalizeOpenNewWindowValue(1),
    1,
    "value 1 should keep current-window behavior",
  );
  assertEquals(
    normalizeOpenNewWindowValue(2),
    2,
    "value 2 should keep new-window behavior",
  );
  assertEquals(
    normalizeOpenNewWindowValue(3),
    3,
    "value 3 should keep new-tab behavior",
  );
  assertEquals(
    normalizeOpenNewWindowValue(null),
    DEFAULT_OPEN_NEW_WINDOW,
    "missing pref should use the new-tab default",
  );
  assertEquals(
    normalizeOpenNewWindowValue(99),
    DEFAULT_OPEN_NEW_WINDOW,
    "unknown pref values should use the new-tab default",
  );
}

function testTabWindowBehaviorTranslationsExist(): void {
  assertEquals(
    enUS.design.tabWindowBehavior.taskbarPreviews,
    "Show tab previews in the taskbar",
    "English taskbar preview label should exist",
  );
  assertEquals(
    jaJP.design.tabWindowBehavior.taskbarPreviews,
    "タスクバーにタブのプレビューを表示",
    "Japanese taskbar preview label should exist",
  );
  assertEquals(
    enUS.design.tabWindowBehavior.openNewTab,
    "New tab",
    "English new-tab label should exist",
  );
  assertEquals(
    jaJP.design.tabWindowBehavior.openNewTab,
    "新しいタブ",
    "Japanese new-tab label should exist",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "open-new-window values are normalized",
      fn: testOpenNewWindowValuesAreNormalized,
    },
    {
      name: "tab-window behavior translations exist",
      fn: testTabWindowBehaviorTranslationsExist,
    },
  ];
  await runTests("tabWindowBehavior.test.ts", tests);
}
