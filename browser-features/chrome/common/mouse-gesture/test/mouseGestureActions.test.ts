// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  getAllGestureActions,
  gestureActions,
  executeGestureAction,
  getActionDisplayName,
  getActionDescription,
} from "../utils/gestures.ts";
import type { GestureActionRegistration } from "../utils/gestures.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

const tests: TestCase[] = [
  // --- getAllGestureActions ---
  {
    name: "getAllGestureActions returns a non-empty array",
    fn() {
      const actions = getAllGestureActions();
      assert(Array.isArray(actions), "should return an array");
      assert(actions.length > 0, "should have at least one registered action");
    },
  },
  {
    name: "each action has name and fn properties",
    fn() {
      const actions = getAllGestureActions();
      for (const action of actions) {
        assert(
          typeof action.name === "string" && action.name.length > 0,
          `action should have a non-empty name, got: ${action.name}`,
        );
        assert(
          typeof action.fn === "function",
          `action '${action.name}' should have a function 'fn'`,
        );
      }
    },
  },
  {
    name: "action names are unique",
    fn() {
      const actions = getAllGestureActions();
      const names = new Set<string>();
      for (const action of actions) {
        assert(
          !names.has(action.name),
          `duplicate action name found: ${action.name}`,
        );
        names.add(action.name);
      }
    },
  },

  // --- gestureActions registry ---
  {
    name: "gestureActions.getAllActions returns a Map",
    fn() {
      const all = gestureActions.getAllActions();
      assert(all instanceof Map, "should return a Map");
      assert(all.size > 0, "map should not be empty");
    },
  },
  {
    name: "gestureActions.getAction returns a function for known action",
    fn() {
      const actions = getAllGestureActions();
      const firstName = actions[0].name;
      const fn = gestureActions.getAction(firstName);
      assert(
        typeof fn === "function",
        "should return a function for known action",
      );
    },
  },
  {
    name: "gestureActions.getAction returns undefined for unknown action",
    fn() {
      const fn = gestureActions.getAction("__nonexistent_action_xyz__");
      assertEquals(fn, undefined, "should return undefined for unknown action");
    },
  },

  // --- registerAction ---
  {
    name: "registerAction adds a new action to the registry",
    fn() {
      const testAction: GestureActionRegistration = {
        name: "__test_custom_action__",
        fn: () => {},
      };
      gestureActions.registerAction(testAction);
      const retrieved = gestureActions.getAction("__test_custom_action__");
      assert(
        typeof retrieved === "function",
        "custom action should be retrievable after registration",
      );
    },
  },

  // --- executeGestureAction ---
  {
    name: "executeGestureAction returns false for unknown action",
    fn() {
      const result = executeGestureAction("__nonexistent__", window);
      assertEquals(result, false, "should return false for unknown action");
    },
  },
  {
    name: "executeGestureAction returns false for null window",
    fn() {
      const actions = getAllGestureActions();
      if (actions.length > 0) {
        // biome-ignore lint/suspicious/noExplicitAny: testing invalid input
        const result = executeGestureAction(actions[0].name, null as any);
        assertEquals(result, false, "should return false for null window");
      }
    },
  },

  // --- getActionDisplayName / getActionDescription ---
  {
    name: "getActionDisplayName returns a string",
    fn() {
      const name = getActionDisplayName("back");
      assert(typeof name === "string", "should return a string");
      assert(name.length > 0, "display name should not be empty");
    },
  },
  {
    name: "getActionDisplayName falls back to actionId for unknown key",
    fn() {
      const name = getActionDisplayName("__unknown_key_xyz__");
      assertEquals(
        name,
        "__unknown_key_xyz__",
        "should fall back to the actionId itself",
      );
    },
  },
  {
    name: "getActionDescription returns a string",
    fn() {
      const desc = getActionDescription("back");
      assert(typeof desc === "string", "should return a string");
    },
  },
  {
    name: "getActionDescription returns empty string for unknown key",
    fn() {
      const desc = getActionDescription("__unknown_key_xyz__");
      assertEquals(desc, "", "should return empty string as default");
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("mouseGestureActions.test.ts", tests);
}
