// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import {
  addHiddenWorkspace,
  settle,
  waitFor,
  withWindows,
} from "./workspace-last-tab-test-utils.ts";

export async function runAllTests(): Promise<void> {
  await runTests("workspaceLastTabPolicy.test.ts", [
    ...[false, true].map((exitWorkspace) => ({
      name:
        `native false keeps the last tab's window open (workspace exit ${exitWorkspace})`,
      fn: () =>
        withWindows(false, exitWorkspace, async (open) => {
          const win = await open();
          const closing = win.gBrowser.selectedTab;
          win.gBrowser.removeTab(closing, { animate: false });
          await settle();
          assert(!win.closed, "explicit false keeps the window open");
          assertEquals(
            win.gBrowser.tabs.length,
            1,
            "exactly one replacement remains",
          );
          assert(
            win.gBrowser.selectedTab !== closing,
            "the old tab was removed",
          );
        }),
    })),
    ...[false, true].map((exitWorkspace) => ({
      name:
        `native true closes an ordinary window even with hidden tabs in another (workspace exit ${exitWorkspace})`,
      fn: () =>
        withWindows(true, exitWorkspace, async (open) => {
          const other = await open();
          const hidden = addHiddenWorkspace(other);
          const win = await open();
          win.gBrowser.removeTab(win.gBrowser.selectedTab, { animate: false });
          await waitFor(
            () => win.closed,
            "ordinary last-tab window closes (#2684)",
          );
          assert(!other.closed, "other window stays open");
          assert(
            other.gBrowser.tabs.includes(hidden),
            "other window's hidden tab survives",
          );
        }),
    })),
  ]);
}
