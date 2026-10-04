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
import { WORKSPACE_TAB_ATTRIBUTION_ID } from "../utils/workspaces-static-names.ts";

export async function runAllTests(): Promise<void> {
  await runTests("workspaceLastTabPreservation.test.ts", [
    ...[
      { closeWithLastTab: false, exitWorkspace: false },
      { closeWithLastTab: false, exitWorkspace: true },
      { closeWithLastTab: true, exitWorkspace: false },
    ].map(({ closeWithLastTab, exitWorkspace }) => ({
      name:
        `last visible tab switches to preserved workspace (native ${closeWithLastTab}, workspace exit ${exitWorkspace})`,
      fn: () =>
        withWindows(closeWithLastTab, exitWorkspace, async (open) => {
          const win = await open();
          const hidden = addHiddenWorkspace(win);
          win.gBrowser.removeTab(win.gBrowser.selectedTab, { animate: false });
          await settle();
          assert(
            !win.closed,
            "native false or workspace exit false prevents closure",
          );
          assert(
            win.gBrowser.tabs.includes(hidden),
            "hidden user tab survives",
          );
          assertEquals(
            win.gBrowser.selectedTab,
            hidden,
            "remaining workspace becomes selected",
          );
          assert(
            !hidden.hasAttribute("hidden"),
            "remaining workspace becomes visible",
          );
        }),
    })),
    {
      name:
        "workspace exit preserves hidden tabs in the closed window's session",
      fn: () =>
        withWindows(true, true, async (open) => {
          const win = await open();
          const hidden = addHiddenWorkspace(win);
          const hiddenWorkspace = hidden.getAttribute(
            WORKSPACE_TAB_ATTRIBUTION_ID,
          );
          assert(hiddenWorkspace, "hidden tab belongs to a workspace");
          // Blank-only windows are intentionally omitted from closed-window data.
          win.gBrowser.getBrowserForTab(hidden).loadURI(
            Services.io.newURI("about:robots"),
            {
              triggeringPrincipal: Services.scriptSecurityManager
                .getSystemPrincipal(),
            },
          );
          await waitFor(
            () =>
              win.gBrowser.getBrowserForTab(hidden).currentURI?.spec ===
                "about:robots",
            "hidden tab has restorable content",
          );
          await settle();
          win.gBrowser.removeTab(win.gBrowser.selectedTab, { animate: false });
          await waitFor(() => win.closed, "workspace exit closes the window");
          const sessionStore = (globalThis as unknown as {
            SessionStore: {
              getClosedWindowData(): {
                tabs: {
                  floorpWorkspaceId?: string;
                  entries?: { url?: string }[];
                }[];
              }[];
            };
          }).SessionStore;
          await waitFor(
            () =>
              sessionStore.getClosedWindowData().some((closed) =>
                closed.tabs.some((tab) =>
                  tab.floorpWorkspaceId === hiddenWorkspace &&
                  tab.entries?.some((entry) => entry.url === "about:robots")
                )
              ),
            "hidden tab URL and workspace are included in the closed-window session",
          );
        }),
    },
  ]);
}
