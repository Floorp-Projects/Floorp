// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../../test/utils/test_harness.ts";
import { tabSwitcherCommand, loadTabs } from "../tab-switcher.ts";
import {
  getPaletteCommands,
  type PaletteCommand,
} from "../../../command-registry.ts";

const rawTests: TestCase[] = [
  {
    name: "tabSwitcherCommand has correct id",
    fn() {
      assertEquals(tabSwitcherCommand.id, "floorp-tab-switcher", "id should be floorp-tab-switcher");
    },
  },
  {
    name: "tabSwitcherCommand has required properties",
    fn() {
      assert(
        typeof tabSwitcherCommand.label === "string",
        "label should be a string",
      );
      assert(
        typeof tabSwitcherCommand.description === "string",
        "description should be a string",
      );
      assert(
        typeof tabSwitcherCommand.category === "string",
        "category should be a string",
      );
      assert(
        Array.isArray(tabSwitcherCommand.keywords),
        "keywords should be an array",
      );
      assert(
        typeof tabSwitcherCommand.fn === "function",
        "fn should be a function",
      );
    },
  },
  {
    name: "tabSwitcherCommand has correct category",
    fn() {
      assertEquals(tabSwitcherCommand.category, "switcher", "category should be switcher");
    },
  },
  {
    name: "tabSwitcherCommand has one step",
    fn() {
      assert(
        Array.isArray(tabSwitcherCommand.steps),
        "steps should be an array",
      );
      assertEquals(tabSwitcherCommand.steps!.length, 1, "should have exactly 1 step");
    },
  },
  {
    name: "tabSwitcherCommand step has choicesLoader",
    fn() {
      const step = tabSwitcherCommand.steps![0];
      assert(
        typeof step.choicesLoader === "function",
        "choicesLoader should be a function",
      );
    },
  },
  {
    name: "tabSwitcherCommand step has required properties",
    fn() {
      const step = tabSwitcherCommand.steps![0];
      assert(typeof step.id === "string", "step id should be a string");
      assert(typeof step.label === "string", "step label should be a string");
      assert(
        typeof step.placeholder === "string",
        "step placeholder should be a string",
      );
    },
  },
  {
    name: "tabSwitcherCommand fn does not throw with empty args",
    fn() {
      assertDoesNotThrow(() => {
        tabSwitcherCommand.fn({} as Window, {});
      });
    },
  },
  {
    name: "tabSwitcherCommand fn does not throw with invalid tab index",
    fn() {
      assertDoesNotThrow(() => {
        tabSwitcherCommand.fn({} as Window, { tab: "999" });
      });
    },
  },
  {
    name: "loadTabs returns an array",
    async fn() {
      const result = await loadTabs();
      assert(Array.isArray(result), "loadTabs should return an array");
    },
  },
  {
    name: "loadTabs items have label and value",
    async fn() {
      const result = await loadTabs();
      const choices = Array.isArray(result) ? result : result.choices;
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
    name: "tabSwitcherCommand is registered in palette",
    fn() {
      const commands = getPaletteCommands();
      const found = commands.some((cmd: PaletteCommand) => cmd.id === "floorp-tab-switcher");
      assert(
        found,
        "floorp-tab-switcher should be registered in palette commands",
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
  runTests("tab-switcher.test.ts", rawTests);
}
