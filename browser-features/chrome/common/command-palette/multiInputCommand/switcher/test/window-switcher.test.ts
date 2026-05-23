// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../test/utils/test_harness.ts";
import { windowSwitcherCommand, loadWindows } from "../window-switcher.ts";
import { getPaletteCommands } from "../../../../command-registry.ts";

const rawTests: TestCase[] = [
  {
    name: "windowSwitcherCommand has correct id",
    fn() {
      assertEquals(windowSwitcherCommand.id, "floorp-window-switcher");
    },
  },
  {
    name: "windowSwitcherCommand has required properties",
    fn() {
      assert(
        typeof windowSwitcherCommand.label === "string",
        "label should be a string",
      );
      assert(
        typeof windowSwitcherCommand.description === "string",
        "description should be a string",
      );
      assert(
        typeof windowSwitcherCommand.category === "string",
        "category should be a string",
      );
      assert(
        Array.isArray(windowSwitcherCommand.keywords),
        "keywords should be an array",
      );
      assert(
        typeof windowSwitcherCommand.fn === "function",
        "fn should be a function",
      );
    },
  },
  {
    name: "windowSwitcherCommand has correct category",
    fn() {
      assertEquals(windowSwitcherCommand.category, "switcher");
    },
  },
  {
    name: "windowSwitcherCommand has one step",
    fn() {
      assert(
        Array.isArray(windowSwitcherCommand.steps),
        "steps should be an array",
      );
      assertEquals(windowSwitcherCommand.steps!.length, 1);
    },
  },
  {
    name: "windowSwitcherCommand step has choicesLoader",
    fn() {
      const step = windowSwitcherCommand.steps![0];
      assert(
        typeof step.choicesLoader === "function",
        "choicesLoader should be a function",
      );
    },
  },
  {
    name: "windowSwitcherCommand step has required properties",
    fn() {
      const step = windowSwitcherCommand.steps![0];
      assert(typeof step.id === "string", "step id should be a string");
      assert(typeof step.label === "string", "step label should be a string");
      assert(
        typeof step.placeholder === "string",
        "step placeholder should be a string",
      );
    },
  },
  {
    name: "windowSwitcherCommand fn does not throw with empty args",
    fn() {
      assertDoesNotThrow(() => {
        windowSwitcherCommand.fn({} as Window, {});
      });
    },
  },
  {
    name: "windowSwitcherCommand fn does not throw with invalid window index",
    fn() {
      assertDoesNotThrow(() => {
        windowSwitcherCommand.fn({} as Window, { window: "999" });
      });
    },
  },
  {
    name: "loadWindows returns an array",
    async fn() {
      const result = await loadWindows();
      assert(Array.isArray(result), "loadWindows should return an array");
    },
  },
  {
    name: "loadWindows items have label and value",
    async fn() {
      const result = await loadWindows();
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
    name: "loadWindows items have description",
    async fn() {
      const result = await loadWindows();
      for (const item of result) {
        assert(
          typeof item.description === "string",
          "each item should have a string description",
        );
      }
    },
  },
  {
    name: "windowSwitcherCommand is registered in palette",
    fn() {
      const commands = getPaletteCommands();
      const found = commands.some((cmd) => cmd.id === "floorp-window-switcher");
      assert(
        found,
        "floorp-window-switcher should be registered in palette commands",
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
  runTests("window-switcher.test.ts", rawTests);
}
