// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../test/utils/test_harness.ts";
import {
  closedTabSwitcherCommand,
  loadClosedTabs,
} from "../closed-tab-switcher.ts";
import { getPaletteCommands } from "../../../../command-registry.ts";

const rawTests: TestCase[] = [
  {
    name: "closedTabSwitcherCommand has correct id",
    fn() {
      assertEquals(closedTabSwitcherCommand.id, "floorp-closed-tab-switcher");
    },
  },
  {
    name: "closedTabSwitcherCommand has required properties",
    fn() {
      assert(
        typeof closedTabSwitcherCommand.label === "string",
        "label should be a string",
      );
      assert(
        typeof closedTabSwitcherCommand.description === "string",
        "description should be a string",
      );
      assert(
        typeof closedTabSwitcherCommand.category === "string",
        "category should be a string",
      );
      assert(
        Array.isArray(closedTabSwitcherCommand.keywords),
        "keywords should be an array",
      );
      assert(
        typeof closedTabSwitcherCommand.fn === "function",
        "fn should be a function",
      );
    },
  },
  {
    name: "closedTabSwitcherCommand has correct category",
    fn() {
      assertEquals(closedTabSwitcherCommand.category, "switcher");
    },
  },
  {
    name: "closedTabSwitcherCommand has one step",
    fn() {
      assert(
        Array.isArray(closedTabSwitcherCommand.steps),
        "steps should be an array",
      );
      assertEquals(closedTabSwitcherCommand.steps!.length, 1);
    },
  },
  {
    name: "closedTabSwitcherCommand step has choicesLoader",
    fn() {
      const step = closedTabSwitcherCommand.steps![0];
      assert(
        typeof step.choicesLoader === "function",
        "choicesLoader should be a function",
      );
    },
  },
  {
    name: "closedTabSwitcherCommand step has required properties",
    fn() {
      const step = closedTabSwitcherCommand.steps![0];
      assert(typeof step.id === "string", "step id should be a string");
      assert(typeof step.label === "string", "step label should be a string");
      assert(
        typeof step.placeholder === "string",
        "step placeholder should be a string",
      );
    },
  },
  {
    name: "closedTabSwitcherCommand fn does not throw with empty args",
    fn() {
      assertDoesNotThrow(() => {
        closedTabSwitcherCommand.fn({} as Window, {});
      });
    },
  },
  {
    name: "closedTabSwitcherCommand fn does not throw with invalid closed tab index",
    fn() {
      assertDoesNotThrow(() => {
        closedTabSwitcherCommand.fn({} as Window, { closedTab: "999" });
      });
    },
  },
  {
    name: "loadClosedTabs returns an array",
    async fn() {
      const result = await loadClosedTabs();
      assert(Array.isArray(result), "loadClosedTabs should return an array");
    },
  },
  {
    name: "loadClosedTabs items have label and value",
    async fn() {
      const result = await loadClosedTabs();
      for (const item of result) {
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
    name: "closedTabSwitcherCommand is registered in palette",
    fn() {
      const commands = getPaletteCommands();
      const found = commands.some(
        (cmd) => cmd.id === "floorp-closed-tab-switcher",
      );
      assert(
        found,
        "floorp-closed-tab-switcher should be registered in palette commands",
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
  runTests("closed-tab-switcher.test.ts", rawTests);
}
