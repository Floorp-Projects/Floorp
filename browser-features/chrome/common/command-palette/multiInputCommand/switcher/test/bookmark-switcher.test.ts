// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../../test/utils/test_harness.ts";
import {
  bookmarkSwitcherCommand,
  loadBookmarks,
} from "../bookmark-switcher.ts";
import {
  getPaletteCommands,
  type PaletteCommand,
} from "../../../command-registry.ts";

const rawTests: TestCase[] = [
  {
    name: "bookmarkSwitcherCommand has correct id",
    fn() {
      assertEquals(bookmarkSwitcherCommand.id, "floorp-bookmark-switcher", "id should be floorp-bookmark-switcher");
    },
  },
  {
    name: "bookmarkSwitcherCommand has required properties",
    fn() {
      assert(
        typeof bookmarkSwitcherCommand.label === "string",
        "label should be a string",
      );
      assert(
        typeof bookmarkSwitcherCommand.description === "string",
        "description should be a string",
      );
      assert(
        typeof bookmarkSwitcherCommand.category === "string",
        "category should be a string",
      );
      assert(
        Array.isArray(bookmarkSwitcherCommand.keywords),
        "keywords should be an array",
      );
      assert(
        typeof bookmarkSwitcherCommand.fn === "function",
        "fn should be a function",
      );
    },
  },
  {
    name: "bookmarkSwitcherCommand has correct category",
    fn() {
      assertEquals(bookmarkSwitcherCommand.category, "switcher", "category should be switcher");
    },
  },
  {
    name: "bookmarkSwitcherCommand has one step",
    fn() {
      assert(
        Array.isArray(bookmarkSwitcherCommand.steps),
        "steps should be an array",
      );
      assertEquals(bookmarkSwitcherCommand.steps!.length, 1, "should have exactly 1 step");
    },
  },
  {
    name: "bookmarkSwitcherCommand step has choicesLoader",
    fn() {
      const step = bookmarkSwitcherCommand.steps![0];
      assert(
        typeof step.choicesLoader === "function",
        "choicesLoader should be a function",
      );
    },
  },
  {
    name: "bookmarkSwitcherCommand step has required properties",
    fn() {
      const step = bookmarkSwitcherCommand.steps![0];
      assert(typeof step.id === "string", "step id should be a string");
      assert(typeof step.label === "string", "step label should be a string");
      assert(
        typeof step.placeholder === "string",
        "step placeholder should be a string",
      );
    },
  },
  {
    name: "bookmarkSwitcherCommand fn does not throw with empty args",
    fn() {
      assertDoesNotThrow(() => {
        bookmarkSwitcherCommand.fn({} as Window, {});
      });
    },
  },
  {
    name: "bookmarkSwitcherCommand fn does not throw with invalid URL args",
    fn() {
      assertDoesNotThrow(() => {
        bookmarkSwitcherCommand.fn({} as Window, { bookmark: "not-a-url" });
      });
    },
  },
  {
    name: "loadBookmarks returns a result",
    async fn() {
      const result = await loadBookmarks();
      const isArray = Array.isArray(result);
      const isResultObject =
        typeof result === "object" && result !== null && "choices" in result;
      assert(
        isArray || isResultObject,
        "result should be an array or StepChoicesResult",
      );
    },
  },
  {
    name: "loadBookmarks items have label and value when present",
    async fn() {
      const result = await loadBookmarks();
      const choices = Array.isArray(result)
        ? result
        : (result as { choices: Array<{ label: string; value: string }> })
            .choices;
      for (const item of choices) {
        assert(
          typeof item.label === "string",
          "each item should have a string label",
        );
        assert(
          typeof item.value === "string",
          "each item should have a string value",
        );
      }
    },
  },
  {
    name: "bookmarkSwitcherCommand is registered in palette",
    fn() {
      const commands = getPaletteCommands();
      const found = commands.some(
        (cmd: PaletteCommand) => cmd.id === "floorp-bookmark-switcher",
      );
      assert(
        found,
        "floorp-bookmark-switcher should be registered in palette commands",
      );
    },
  },
];

function assertDoesNotThrow(fn: () => void): void {
  try {
    fn();
  } catch (err) {
    assert(false, `Expected no throw, but got: ${String(err)}`);
  }
}

export function runAllTests() {
  runTests("bookmark-switcher.test.ts", rawTests);
}
