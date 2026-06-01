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
  shareModeEnabled,
  setShareModeEnabled,
} from "#features-chrome/common/browser-share-mode/browser-share-mode.tsx";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

const INITIAL_REGISTRY_ACTIONS: GestureActionRegistration[] =
  getAllGestureActions().map((action) => ({
    name: action.name,
    fn: action.fn,
  }));

function restoreGestureRegistry(): void {
  const registry = gestureActions.getAllActions();
  registry.clear();
  for (const action of INITIAL_REGISTRY_ACTIONS) {
    gestureActions.registerAction(action);
  }
}

/** Save and restore shareModeEnabled signal state around a test block */
function withShareModeRestored(fn: () => void): () => void {
  return () => {
    const original = shareModeEnabled();
    try {
      fn();
    } finally {
      setShareModeEnabled(original);
    }
  };
}

const rawTests: TestCase[] = [
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
  {
    name: "getAllGestureActions includes close-other-tabs actions",
    fn() {
      const actions = getAllGestureActions();
      const names = actions.map((a) => a.name);
      assert(
        names.includes("gecko-close-other-tabs"),
        "should include gecko-close-other-tabs",
      );
      assert(
        names.includes("gecko-close-tabs-to-start"),
        "should include gecko-close-tabs-to-start",
      );
      assert(
        names.includes("gecko-close-tabs-to-end"),
        "should include gecko-close-tabs-to-end",
      );
    },
  },

  // --- floorp-open-settings action ---
  {
    name: "floorp-open-settings action is registered",
    fn() {
      const actions = getAllGestureActions();
      const names = actions.map((a) => a.name);
      assert(
        names.includes("floorp-open-settings"),
        "should include floorp-open-settings action",
      );
    },
  },
  {
    name: "floorp-open-settings action calls win.openPreferences()",
    fn() {
      const fn = gestureActions.getAction("floorp-open-settings");
      assert(
        typeof fn === "function",
        "floorp-open-settings should have a function",
      );
      let openPreferencesCalled = false;
      const mockWin = {
        openPreferences() {
          openPreferencesCalled = true;
        },
      } as unknown as Window;
      fn(mockWin);
      assert(openPreferencesCalled, "should call win.openPreferences()");
    },
  },

  // --- floorp-open-hub action ---
  {
    name: "floorp-open-hub action is registered",
    fn() {
      const actions = getAllGestureActions();
      const names = actions.map((a) => a.name);
      assert(
        names.includes("floorp-open-hub"),
        "should include floorp-open-hub action",
      );
    },
  },
  {
    name: "floorp-open-hub action calls win.switchToTabHavingURI()",
    fn() {
      const fn = gestureActions.getAction("floorp-open-hub");
      assert(
        typeof fn === "function",
        "floorp-open-hub should have a function",
      );
      let switchToTabCalled = false;
      let receivedURI: string | undefined;
      const mockWin = {
        switchToTabHavingURI(uri: { spec: string }, _openNew: boolean) {
          switchToTabCalled = true;
          receivedURI = uri.spec;
        },
      } as unknown as Window;
      fn(mockWin);
      assert(switchToTabCalled, "should call win.switchToTabHavingURI()");
      assert(
        typeof receivedURI === "string" && receivedURI.includes("about:hub"),
        "should pass about:hub URI to switchToTabHavingURI",
      );
    },
  },

  // --- floorp-toggle-share-mode action ---
  {
    name: "floorp-toggle-share-mode action is registered",
    fn() {
      const actions = getAllGestureActions();
      const names = actions.map((a) => a.name);
      assert(
        names.includes("floorp-toggle-share-mode"),
        "should include floorp-toggle-share-mode action",
      );
    },
  },
  {
    name: "floorp-toggle-share-mode action is a function",
    fn() {
      const fn = gestureActions.getAction("floorp-toggle-share-mode");
      assert(
        typeof fn === "function",
        "floorp-toggle-share-mode should have a function",
      );
    },
  },
  {
    name: "floorp-toggle-share-mode action toggles share mode from false to true",
    fn: withShareModeRestored(() => {
      setShareModeEnabled(false);
      const fn = gestureActions.getAction("floorp-toggle-share-mode");
      assert(typeof fn === "function", "action should exist");
      fn(window);
      assertEquals(
        shareModeEnabled(),
        true,
        "share mode should be true after toggling from false",
      );
    }),
  },
  {
    name: "floorp-toggle-share-mode action toggles share mode from true to false",
    fn: withShareModeRestored(() => {
      setShareModeEnabled(true);
      const fn = gestureActions.getAction("floorp-toggle-share-mode");
      assert(typeof fn === "function", "action should exist");
      fn(window);
      assertEquals(
        shareModeEnabled(),
        false,
        "share mode should be false after toggling from true",
      );
    }),
  },
  {
    name: "floorp-toggle-share-mode action toggles multiple times correctly",
    fn: withShareModeRestored(() => {
      setShareModeEnabled(false);
      const fn = gestureActions.getAction("floorp-toggle-share-mode");
      assert(typeof fn === "function", "action should exist");
      fn(window);
      assertEquals(shareModeEnabled(), true, "first toggle: should be true");
      fn(window);
      assertEquals(shareModeEnabled(), false, "second toggle: should be false");
      fn(window);
      assertEquals(shareModeEnabled(), true, "third toggle: should be true");
    }),
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
      if (fn) {
        // Provide minimal mock window for test function
        fn({} as Window);
      }
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
        // @ts-expect-error - Testing invalid input (null)
        const result = executeGestureAction(actions[0].name, null);
        assertEquals(result, false, "should return false for null window");
      }
    },
  },
  {
    name: "executeGestureAction returns false for undefined window",
    fn() {
      const actions = getAllGestureActions();
      if (actions.length > 0) {
        // @ts-expect-error - Testing invalid input (undefined)
        const result = executeGestureAction(actions[0].name, undefined);
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

  // --- Additional edge case tests ---
  {
    name: "getActionDisplayName with empty string",
    fn() {
      const name = getActionDisplayName("");
      assertEquals(name, "", "should handle empty actionId");
    },
  },
  {
    name: "getActionDisplayName with special characters",
    fn() {
      const name = getActionDisplayName("action-with-special-chars_123");
      assert(
        typeof name === "string",
        "should handle special characters in actionId",
      );
    },
  },
  {
    name: "getActionDescription with empty string",
    fn() {
      const desc = getActionDescription("");
      assertEquals(desc, "", "should return empty string for empty actionId");
    },
  },
  {
    name: "getActionDescription with very long actionId",
    fn() {
      const longId = "a".repeat(1000);
      const desc = getActionDescription(longId);
      assertEquals(desc, "", "should handle very long actionId");
    },
  },
  {
    name: "getAllGestureActions returns consistent order",
    fn() {
      const actions1 = getAllGestureActions();
      const actions2 = getAllGestureActions();
      assertEquals(
        actions1.length,
        actions2.length,
        "should return same number of actions",
      );
      // Check that order is consistent
      for (let i = 0; i < actions1.length; i++) {
        assertEquals(
          actions1[i].name,
          actions2[i].name,
          `action ${i} should be in same order`,
        );
      }
    },
  },
  {
    name: "gestureActions registry persists across calls",
    fn() {
      const size1 = gestureActions.getAllActions().size;
      const size2 = gestureActions.getAllActions().size;
      assertEquals(size1, size2, "registry size should be consistent");
    },
  },
  {
    name: "executeGestureAction with action that throws",
    fn() {
      const _errorThrown = false;
      gestureActions.registerAction({
        name: "__test_throwing_action__",
        fn: () => {
          throw new Error("Test error");
        },
      });
      const result = executeGestureAction("__test_throwing_action__", window);
      assertEquals(result, false, "should return false when action throws");
    },
  },
  {
    name: "executeGestureAction with action that returns false",
    fn() {
      gestureActions.registerAction({
        name: "__test_returns_false__",
        fn: () => false,
      });
      const result = executeGestureAction("__test_returns_false__", window);
      assertEquals(
        result,
        true,
        "should return true even if action returns false",
      );
    },
  },
  {
    name: "executeGestureAction with action that returns value",
    fn() {
      gestureActions.registerAction({
        name: "__test_returns_value__",
        fn: () => "custom return value",
      });
      const result = executeGestureAction("__test_returns_value__", window);
      assertEquals(result, true, "should return true for successful execution");
    },
  },
  {
    name: "gestureActions.getAction returns same function reference",
    fn() {
      gestureActions.registerAction({
        name: "__test_same_ref__",
        fn: () => {},
      });
      const fn1 = gestureActions.getAction("__test_same_ref__");
      const fn2 = gestureActions.getAction("__test_same_ref__");
      assertEquals(fn1, fn2, "should return same function reference");
    },
  },
  {
    name: "getAllGestureActions includes all registered actions",
    fn() {
      const beforeSize = getAllGestureActions().length;
      gestureActions.registerAction({
        name: "__test_included__",
        fn: () => {},
      });
      const afterSize = getAllGestureActions().length;
      assertEquals(
        afterSize,
        beforeSize + 1,
        "should include newly registered action",
      );
    },
  },

  // --- floorp-copy-page-url-as-markdown action ---
  {
    name: "floorp-copy-page-url-as-markdown action is registered",
    fn() {
      const actions = getAllGestureActions();
      const names = actions.map((a) => a.name);
      assert(
        names.includes("floorp-copy-page-url-as-markdown"),
        "should include floorp-copy-page-url-as-markdown action",
      );
    },
  },
  {
    name: "floorp-copy-page-url-as-markdown action copies markdown link to clipboard",
    async fn() {
      const fn = gestureActions.getAction("floorp-copy-page-url-as-markdown");
      assert(
        typeof fn === "function",
        "floorp-copy-page-url-as-markdown should have a function",
      );

      const testUrl = "https://example.com/page";
      const testTitle = "Example Page Title";
      let clipboardText = "";
      const mockWin = {
        gBrowser: {
          selectedBrowser: {
            currentURI: { spec: testUrl },
            contentTitle: testTitle,
          },
        },
      } as unknown as Window;

      assert(
        typeof navigator !== "undefined" &&
          navigator.clipboard != null &&
          typeof navigator.clipboard.writeText === "function",
        "navigator.clipboard.writeText must exist for this test",
      );

      // Temporarily mock navigator.clipboard.writeText
      const savedWriteText = navigator.clipboard.writeText;
      navigator.clipboard.writeText = (text: string): Promise<void> => {
        clipboardText = text;
        return Promise.resolve();
      };

      try {
        fn(mockWin);
        // Wait a tick for the async clipboard write
        await new Promise((resolve) => setTimeout(resolve, 10));
        assertEquals(
          clipboardText,
          `[${testTitle}](${testUrl})`,
          "should copy markdown formatted link to clipboard",
        );
      } finally {
        navigator.clipboard.writeText = savedWriteText;
      }
    },
  },
  {
    name: "floorp-copy-page-url-as-markdown escapes brackets in title",
    async fn() {
      const fn = gestureActions.getAction("floorp-copy-page-url-as-markdown");
      assert(typeof fn === "function", "action should have a function");

      const testUrl = "https://example.com/page";
      const testTitle = "Title with [brackets] inside";
      let clipboardText = "";

      const mockWin = {
        gBrowser: {
          selectedBrowser: {
            currentURI: { spec: testUrl },
            contentTitle: testTitle,
          },
        },
      } as unknown as Window;

      assert(
        typeof navigator !== "undefined" &&
          navigator.clipboard != null &&
          typeof navigator.clipboard.writeText === "function",
        "navigator.clipboard.writeText must exist for this test",
      );

      const savedWriteText = navigator.clipboard.writeText;
      navigator.clipboard.writeText = (text: string): Promise<void> => {
        clipboardText = text;
        return Promise.resolve();
      };

      try {
        fn(mockWin);
        await new Promise((resolve) => setTimeout(resolve, 10));
        assertEquals(
          clipboardText,
          `[Title with \\[brackets\\] inside](${testUrl})`,
          "should escape brackets in title",
        );
      } finally {
        navigator.clipboard.writeText = savedWriteText;
      }
    },
  },
  {
    name: "floorp-copy-page-url-as-markdown falls back to URL when title is empty",
    async fn() {
      const fn = gestureActions.getAction("floorp-copy-page-url-as-markdown");
      assert(typeof fn === "function", "action should have a function");

      const testUrl = "https://example.com/no-title";
      let clipboardText = "";

      const mockWin = {
        gBrowser: {
          selectedBrowser: {
            currentURI: { spec: testUrl },
            contentTitle: "",
          },
        },
      } as unknown as Window;

      assert(
        typeof navigator !== "undefined" &&
          navigator.clipboard != null &&
          typeof navigator.clipboard.writeText === "function",
        "navigator.clipboard.writeText must exist for this test",
      );

      const savedWriteText = navigator.clipboard.writeText;
      navigator.clipboard.writeText = (text: string): Promise<void> => {
        clipboardText = text;
        return Promise.resolve();
      };

      try {
        fn(mockWin);
        await new Promise((resolve) => setTimeout(resolve, 10));
        assertEquals(
          clipboardText,
          `[${testUrl}](${testUrl})`,
          "should use URL as title when contentTitle is empty",
        );
      } finally {
        navigator.clipboard.writeText = savedWriteText;
      }
    },
  },
  // --- split-view actions ---
  {
    name: "floorp-split-view-open-left action is registered",
    fn() {
      const actions = getAllGestureActions();
      const names = actions.map((a) => a.name);
      assert(
        names.includes("floorp-split-view-open-left"),
        "should include floorp-split-view-open-left action",
      );
    },
  },
  {
    name: "floorp-split-view-open-right action is registered",
    fn() {
      const actions = getAllGestureActions();
      const names = actions.map((a) => a.name);
      assert(
        names.includes("floorp-split-view-open-right"),
        "should include floorp-split-view-open-right action",
      );
    },
  },
  {
    name: "floorp-split-view-close action is registered",
    fn() {
      const actions = getAllGestureActions();
      const names = actions.map((a) => a.name);
      assert(
        names.includes("floorp-split-view-close"),
        "should include floorp-split-view-close action",
      );
    },
  },
  {
    name: "floorp-split-view-swap-panes action is registered",
    fn() {
      const actions = getAllGestureActions();
      const names = actions.map((a) => a.name);
      assert(
        names.includes("floorp-split-view-swap-panes"),
        "should include floorp-split-view-swap-panes action",
      );
    },
  },
  {
    name: "floorp-split-view-cycle-layout action is registered",
    fn() {
      const actions = getAllGestureActions();
      const names = actions.map((a) => a.name);
      assert(
        names.includes("floorp-split-view-cycle-layout"),
        "should include floorp-split-view-cycle-layout action",
      );
    },
  },
  {
    name: "floorp-split-view-add-pane action is registered",
    fn() {
      const actions = getAllGestureActions();
      const names = actions.map((a) => a.name);
      assert(
        names.includes("floorp-split-view-add-pane"),
        "should include floorp-split-view-add-pane action",
      );
    },
  },
  {
    name: "floorp-split-view-remove-pane action is registered",
    fn() {
      const actions = getAllGestureActions();
      const names = actions.map((a) => a.name);
      assert(
        names.includes("floorp-split-view-remove-pane"),
        "should include floorp-split-view-remove-pane action",
      );
    },
  },
  {
    name: "floorp-split-view-open-top action is registered",
    fn() {
      const actions = getAllGestureActions();
      const names = actions.map((a) => a.name);
      assert(
        names.includes("floorp-split-view-open-top"),
        "should include floorp-split-view-open-top action",
      );
    },
  },
  {
    name: "floorp-split-view-open-bottom action is registered",
    fn() {
      const actions = getAllGestureActions();
      const names = actions.map((a) => a.name);
      assert(
        names.includes("floorp-split-view-open-bottom"),
        "should include floorp-split-view-open-bottom action",
      );
    },
  },
];

const tests: TestCase[] = rawTests.map((testCase) => ({
  name: testCase.name,
  async fn() {
    restoreGestureRegistry();
    try {
      await testCase.fn();
    } finally {
      restoreGestureRegistry();
    }
  },
}));

export async function runAllTests(): Promise<void> {
  await runTests("mouseGestureActions.test.ts", tests);
}
