// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

/**
 * The merge action must be reachable from the native tab context menu, right
 * below "Select All Tabs".
 */

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import { getActionDisplayName } from "../../mouse-gesture/utils/gestures.ts";
import { MERGE_WINDOWS_ACTION_ID } from "../controller.ts";
import {
  installMergeAllWindowsMenuItem,
  MERGE_ALL_WINDOWS_MENU_ID,
  MERGE_ALL_WINDOWS_MENU_KEY,
} from "../tab-context-menu.tsx";

function tabContextMenu(): Element {
  const popup = document.getElementById("tabContextMenu");
  assert(popup, "browser chrome should provide #tabContextMenu");
  return popup;
}

function testPlacesEntryBelowSelectAllTabs(): void {
  const popup = tabContextMenu();
  const dispose = installMergeAllWindowsMenuItem(popup);
  try {
    const item = popup.querySelector(`#${MERGE_ALL_WINDOWS_MENU_ID}`);
    assert(item, "the merge entry should be mounted");

    assertEquals(
      item.previousElementSibling?.id,
      "context_selectAllTabs",
      "the entry sits directly below Select All Tabs",
    );
    assertEquals(
      item.getAttribute("data-floorp-context-menu-key"),
      MERGE_ALL_WINDOWS_MENU_KEY,
      "the entry is marked as Floorp-owned for the context menu customizer",
    );
    assertEquals(
      item.getAttribute("label"),
      getActionDisplayName(MERGE_WINDOWS_ACTION_ID),
      "the label comes from the action catalog locales",
    );
  } finally {
    dispose();
  }

  assertEquals(
    popup.querySelector(`#${MERGE_ALL_WINDOWS_MENU_ID}`),
    null,
    "disposing removes the entry again",
  );
}

function testInstallIsIdempotent(): void {
  const popup = tabContextMenu();
  const disposeFirst = installMergeAllWindowsMenuItem(popup);
  const disposeSecond = installMergeAllWindowsMenuItem(popup);
  try {
    assertEquals(
      popup.querySelectorAll(`#${MERGE_ALL_WINDOWS_MENU_ID}`).length,
      1,
      "a second install does not duplicate the entry",
    );
  } finally {
    disposeSecond();
    disposeFirst();
  }
}

function testMissingPopupIsSafe(): void {
  const dispose = installMergeAllWindowsMenuItem(null);
  dispose();
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "entry is placed below Select All Tabs",
      fn: testPlacesEntryBelowSelectAllTabs,
    },
    { name: "install is idempotent", fn: testInstallIsIdempotent },
    { name: "a missing tab context menu is safe", fn: testMissingPopupIsSafe },
  ];
  await runTests("tabContextMenu.test.ts", tests);
}
