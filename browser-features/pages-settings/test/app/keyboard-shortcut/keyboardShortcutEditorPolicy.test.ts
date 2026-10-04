// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";
import { getRecordedShortcutCode } from "../../../src/types/pref.ts";

function eventPolicy(
  code: string,
  options: { repeat?: boolean; altGraph?: boolean } = {},
): Pick<KeyboardEvent, "code" | "repeat" | "getModifierState"> {
  return {
    code,
    repeat: options.repeat ?? false,
    getModifierState: (modifier: string) =>
      modifier === "AltGraph" && options.altGraph === true,
  };
}

const tests: TestCase[] = [
  {
    name: "records exact physical code",
    fn: () =>
      assertEquals(
        getRecordedShortcutCode(eventPolicy("KeyZ")),
        "KeyZ",
        "recording stores KeyboardEvent.code without layout conversion",
      ),
  },
  {
    name: "records non-letter physical code",
    fn: () =>
      assertEquals(
        getRecordedShortcutCode(eventPolicy("Semicolon")),
        "Semicolon",
        "recording preserves punctuation physical code",
      ),
  },
  {
    name: "ignores repeat",
    fn: () =>
      assertEquals(
        getRecordedShortcutCode(eventPolicy("KeyZ", { repeat: true })),
        null,
        "repeat keydown is ignored while recording",
      ),
  },
  {
    name: "ignores AltGraph",
    fn: () =>
      assertEquals(
        getRecordedShortcutCode(eventPolicy("KeyZ", { altGraph: true })),
        null,
        "AltGraph is not recorded as Ctrl+Alt",
      ),
  },
  {
    name: "ignores pure modifier",
    fn: () =>
      assertEquals(
        getRecordedShortcutCode(eventPolicy("AltRight")),
        null,
        "pure modifiers do not finish recording",
      ),
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("keyboardShortcutEditorPolicy.test.ts", tests);
}
