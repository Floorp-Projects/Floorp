// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../test/utils/test_harness.ts";
import {
  closedWindowSwitcherCommand,
  loadClosedWindows,
} from "../closed-window-switcher.ts";
import { getPaletteCommands } from "../../../../command-registry.ts";

const rawTests: TestCase[] = [
  {
    name: "closedWindowSwitcherCommand has correct id",
    fn() {
      assertEquals(
        closedWindowSwitcherCommand.id,
        "floorp-closed-window-switcher",
      );
    },
  },
  {
    name: "closedWindowSwitcherCommand has required properties",
    fn() {
      assert(
        typeof closedWindowSwitcherCommand.label === "string",
        "label should be a string",
      );
      assert(
        typeof closedWindowSwitcherCommand.description === "string",
        "description should be a string",
      );
      assert(
        typeof closedWindowSwitcherCommand.category === "string",
        "category should be a string",
      );
      assert(
        Array.isArray(closedWindowSwitcherCommand.keywords),
        "keywords should be an array",
      );
      assert(
        typeof closedWindowSwitcherCommand.fn === "function",
        "fn should be a function",
      );
    },
  },
  {
    name: "closedWindowSwitcherCommand has correct category",
    fn() {
      assertEquals(closedWindowSwitcherCommand.category, "switcher");
    },
  },
  {
    name: "closedWindowSwitcherCommand has one step",
    fn() {
      assert(
        Array.isArray(closedWindowSwitcherCommand.steps),
        "steps should be an array",
      );
      assertEquals(closedWindowSwitcherCommand.steps!.length, 1);
    },
  },
  {
    name: "closedWindowSwitcherCommand step has choicesLoader",
    fn() {
      const step = closedWindowSwitcherCommand.steps![0];
      assert(
        typeof step.choicesLoader === "function",
        "choicesLoader should be a function",
      );
    },
  },
  {
    name: "closedWindowSwitcherCommand step has required properties",
    fn() {
      const step = closedWindowSwitcherCommand.steps![0];
      assert(typeof step.id === "string", "step id should be a string");
      assert(typeof step.label === "string", "step label should be a string");
      assert(
        typeof step.placeholder === "string",
        "step placeholder should be a string",
      );
    },
  },
  {
    name: "closedWindowSwitcherCommand fn does not throw with empty args",
    fn() {
      assertDoesNotThrow(() => {
        closedWindowSwitcherCommand.fn({} as Window, {});
      });
    },
  },
  {
    name: "closedWindowSwitcherCommand fn does not throw with invalid closed window index",
    fn() {
      assertDoesNotThrow(() => {
        closedWindowSwitcherCommand.fn({} as Window, { closedWindow: "999" });
      });
    },
  },
  {
    name: "loadClosedWindows returns an array",
    async fn() {
      const result = await loadClosedWindows();
      assert(Array.isArray(result), "loadClosedWindows should return an array");
    },
  },
  {
    name: "loadClosedWindows items have label and value",
    async fn() {
      const result = await loadClosedWindows();
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
    name: "loadClosedWindows items have description",
    async fn() {
      const result = await loadClosedWindows();
      for (const item of result) {
        assert(
          typeof item.description === "string",
          "each item should have a string description",
        );
      }
    },
  },
  {
    name: "closedWindowSwitcherCommand is registered in palette",
    fn() {
      const commands = getPaletteCommands();
      const found = commands.some(
        (cmd) => cmd.id === "floorp-closed-window-switcher",
      );
      assert(
        found,
        "floorp-closed-window-switcher should be registered in palette commands",
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
  runTests("closed-window-switcher.test.ts", rawTests);
}
