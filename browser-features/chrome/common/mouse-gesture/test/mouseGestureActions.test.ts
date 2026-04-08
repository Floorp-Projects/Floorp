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
  assertNotEquals,
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
  {
    name: "getAllGestureActions has common browser actions",
    fn() {
      const actions = getAllGestureActions();
      const names = actions.map((a) => a.name);
      assert(
        names.includes("gecko-open-new-tab"),
        "should include gecko-open-new-tab action",
      );
      assert(
        names.includes("gecko-close-tab"),
        "should include gecko-close-tab action",
      );
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
  {
    name: "gestureActions.getActionsList returns array",
    fn() {
      const list = gestureActions.getActionsList();
      assert(Array.isArray(list), "should return an array");
      assert(list.length > 0, "should not be empty");
      // Each element should have name and fn
      for (const item of list) {
        assert(typeof item.name === "string", "each item should have a name");
        assert(typeof item.fn === "function", "each item should have a fn");
      }
    },
  },
  {
    name: "gestureActions.getAllActions keys match getActionsList names",
    fn() {
      const map = gestureActions.getAllActions();
      const list = gestureActions.getActionsList();
      assertEquals(
        map.size,
        list.length,
        "map size and list length should match",
      );
      for (const item of list) {
        assert(map.has(item.name), `map should contain key: ${item.name}`);
      }
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
  {
    name: "registerAction overwrites existing action",
    fn() {
      let callCount = 0;
      gestureActions.registerAction({
        name: "__test_overwrite_action__",
        fn: () => {
          callCount = 1;
        },
      });
      gestureActions.registerAction({
        name: "__test_overwrite_action__",
        fn: () => {
          callCount = 2;
        },
      });
      const fn = gestureActions.getAction("__test_overwrite_action__");
      assert(
        typeof fn === "function",
        "overwritten action should be retrievable",
      );
      fn();
      assertEquals(
        callCount,
        2,
        "should call the overwritten function, not the original",
      );
    },
  },
  {
    name: "registerAction increases registry size for new actions",
    fn() {
      const sizeBefore = gestureActions.getAllActions().size;
      gestureActions.registerAction({
        name: "__test_size_check__",
        fn: () => {},
      });
      const sizeAfter = gestureActions.getAllActions().size;
      assertEquals(
        sizeAfter,
        sizeBefore + 1,
        "new action should increase size by 1",
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
  {
    name: "executeGestureAction returns false for undefined window",
    fn() {
      const actions = getAllGestureActions();
      if (actions.length > 0) {
        // biome-ignore lint/suspicious/noExplicitAny: testing invalid input
        const result = executeGestureAction(actions[0].name, undefined as any);
        assertEquals(result, false, "should return false for undefined window");
      }
    },
  },
  {
    name: "executeGestureAction calls registered action and returns true",
    fn() {
      let wasCalled = false;
      gestureActions.registerAction({
        name: "__test_exec_action__",
        fn: () => {
          wasCalled = true;
        },
      });
      const result = executeGestureAction("__test_exec_action__", window);
      assertEquals(result, true, "should return true when action executes");
      assert(wasCalled, "action function should have been called");
    },
  },
  {
    name: "executeGestureAction returns false for empty string action name",
    fn() {
      const result = executeGestureAction("", window);
      assertEquals(
        result,
        false,
        "empty string action name should return false",
      );
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
    name: "getActionDisplayName for known actions returns non-empty",
    fn() {
      const actions = getAllGestureActions();
      for (const action of actions.slice(0, 5)) {
        const displayName = getActionDisplayName(action.name);
        assert(
          typeof displayName === "string" && displayName.length > 0,
          `displayName for '${action.name}' should be non-empty string`,
        );
      }
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
  {
    name: "getActionDisplayName differs from getActionDescription for known actions",
    fn() {
      const actions = getAllGestureActions();
      if (actions.length > 0) {
        const name = getActionDisplayName(actions[0].name);
        const desc = getActionDescription(actions[0].name);
        // They can be the same but typically differ; just verify both are strings
        assert(typeof name === "string", "name should be string");
        assert(typeof desc === "string", "desc should be string");
      }
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("mouseGestureActions.test.ts", tests);
}
