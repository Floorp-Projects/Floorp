// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import Workspaces from "../../workspaces/index.ts";
import { workspacesDataStore } from "../../workspaces/data/data.ts";
import { WORKSPACE_DATA_PREF_NAME } from "../../workspaces/utils/workspaces-static-names.ts";
import { waitFor } from "../../workspaces/test/workspace-last-tab-test-utils.ts";
import { switchWorkspaceByOffset } from "../runtime.ts";
import { getKeyboardShortcutAction } from "../../keyboard-shortcut/actions.ts";
import { testWindowsNativeWorkspaceShortcuts } from "./windows-native-input.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

async function testRealWindowEndStopActions(): Promise<void> {
  await waitFor(
    () => Boolean(Workspaces.getCtx()),
    "workspace service is ready",
  );
  const context = Workspaces.getCtx();
  assert(context, "workspace service exists");
  const originalWorkspace = context.getSelectedWorkspaceID();
  const originalTab = globalThis.gBrowser.selectedTab;
  const originalIds = new Set(workspacesDataStore.order);
  const originalData = Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME);
  try {
    const first = context.createNoNameWorkspace();
    const firstTab = globalThis.gBrowser.selectedTab;
    const second = context.createNoNameWorkspace();
    const secondTab = globalThis.gBrowser.selectedTab;
    assertEquals(
      switchWorkspaceByOffset(window, 1),
      false,
      "the final workspace does not wrap",
    );
    assertEquals(
      context.getSelectedWorkspaceID(),
      second,
      "end-stop keeps the final workspace selected",
    );
    assertEquals(
      switchWorkspaceByOffset(window, -1),
      true,
      "previous moves through the common switch path",
    );
    assertEquals(
      context.getSelectedWorkspaceID(),
      first,
      "previous follows workspace order",
    );
    assertEquals(
      globalThis.gBrowser.selectedTab,
      firstTab,
      "previous restores its selected tab",
    );
    getKeyboardShortcutAction("floorp-workspace-next-no-wrap")?.(window);
    assertEquals(
      context.getSelectedWorkspaceID(),
      second,
      "assignable keyboard action reaches same target",
    );
    assertEquals(
      globalThis.gBrowser.selectedTab,
      secondTab,
      "keyboard action restores the target tab",
    );
    getKeyboardShortcutAction("floorp-workspace-next-no-wrap")?.(window);
    assertEquals(
      context.getSelectedWorkspaceID(),
      second,
      "repeated end-stop keys cannot wrap",
    );
    context.changeWorkspace(workspacesDataStore.order[0]);
    assertEquals(
      switchWorkspaceByOffset(window, -1),
      false,
      "first workspace stops too",
    );
  } finally {
    context.changeWorkspace(originalWorkspace);
    globalThis.gBrowser.selectedTab = originalTab;
    for (const id of [...workspacesDataStore.order]) {
      if (!originalIds.has(id)) context.deleteWorkspace(id);
    }
    Services.prefs.setStringPref(WORKSPACE_DATA_PREF_NAME, originalData);
    context.changeWorkspace(originalWorkspace);
    globalThis.gBrowser.selectedTab = originalTab;
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("workspaceGestureActions.test.ts", [
    {
      name: "real window end-stop actions preserve tab selection",
      fn: testRealWindowEndStopActions,
    },
    ...(Services.appinfo.OS === "WINNT"
      ? [{
        name:
          "Windows OS keyboard input switches only its foreground workspace",
        fn: testWindowsNativeWorkspaceShortcuts,
      }]
      : []),
  ]);
}
