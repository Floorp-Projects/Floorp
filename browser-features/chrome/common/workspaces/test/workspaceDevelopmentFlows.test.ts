// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import {
  settle,
  waitFor,
  withWindows,
} from "./workspace-last-tab-test-utils.ts";
import { WORKSPACE_TAB_ATTRIBUTION_ID } from "../utils/workspaces-static-names.ts";
import { withMenuCleanup } from "./workspace-development-menu-cleanup.ts";
import type { TestWindow } from "./workspace-last-tab-test-types.ts";
import type {
  WorkspaceFlowPopup,
  WorkspaceFlowWindow,
} from "./workspace-development-flow-types.ts";

function addTab(win: TestWindow): XULElement {
  return win.gBrowser.addTab("about:blank", {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    inBackground: true,
  }) as XULElement;
}

export async function runAllTests(): Promise<void> {
  await runTests("workspaceDevelopmentFlows.test.ts", [
    {
      name: "A-B-A restores selected tabs and workspace visibility",
      fn: () =>
        withWindows(false, false, async (open) => {
          const win = await open();
          const funcs = win.workspacesFuncs;
          assert(funcs, "workspace API is initialized");
          const a = funcs.getSelectedWorkspaceID();
          const aFirst = win.gBrowser.selectedTab as XULElement;
          const aSelected = addTab(win);
          win.gBrowser.selectedTab = aSelected;
          const b = funcs.createNoNameWorkspace();
          const bFirst = win.gBrowser.selectedTab as XULElement;
          const bSelected = addTab(win);
          win.gBrowser.selectedTab = bSelected;
          await settle();
          const count = win.gBrowser.tabs.length;
          for (
            const [id, selected, shown, hidden] of [
              [a, aSelected, [aFirst, aSelected], [bFirst, bSelected]],
              [b, bSelected, [bFirst, bSelected], [aFirst, aSelected]],
              [a, aSelected, [aFirst, aSelected], [bFirst, bSelected]],
            ] as const
          ) {
            funcs.changeWorkspace(id);
            await settle();
            assertEquals(
              funcs.getSelectedWorkspaceID(),
              id,
              "requested workspace is selected",
            );
            assertEquals(
              win.gBrowser.selectedTab,
              selected,
              "last selected tab is restored",
            );
            for (const tab of shown) {
              assert(
                !tab.hasAttribute("hidden"),
                "active workspace tabs are visible",
              );
            }
            for (const tab of hidden) {
              assert(
                tab.hasAttribute("hidden"),
                "other workspace tabs are hidden",
              );
            }
            assertEquals(
              win.gBrowser.tabs.length,
              count,
              "switching does not create or remove tabs",
            );
          }
        }),
    },
    {
      name: "native TabOpen assigns new tabs to the active workspace",
      fn: () =>
        withWindows(false, false, async (open) => {
          const win = await open();
          const funcs = win.workspacesFuncs;
          assert(funcs, "workspace API is initialized");
          const a = funcs.getSelectedWorkspaceID();
          const aTab = win.gBrowser.selectedTab as XULElement;
          const b = funcs.createNoNameWorkspace();
          const opened: EventTarget[] = [];
          const onOpen = (event: Event): void => {
            if (event.target) opened.push(event.target);
          };
          win.gBrowser.tabContainer.addEventListener("TabOpen", onOpen);
          try {
            const newB = addTab(win);
            await waitFor(
              () => newB.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID) === b,
              "TabOpen assigns current workspace B",
            );
            assertEquals(opened.length, 1, "one real TabOpen was observed");
            assertEquals(
              opened[0],
              newB,
              "TabOpen target is the new native tab",
            );
            assert(
              !newB.hasAttribute("hidden"),
              "new active-workspace tab is visible",
            );
            funcs.changeWorkspace(a);
            await settle();
            assert(newB.hasAttribute("hidden"), "B tab is hidden in A");
            assertEquals(
              win.gBrowser.selectedTab,
              aTab,
              "A selection is preserved",
            );
            const newA = addTab(win);
            await waitFor(
              () => newA.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID) === a,
              "next TabOpen follows workspace A",
            );
            assertEquals(
              opened.length,
              2,
              "two native opens produce exactly two events",
            );
            assertEquals(
              opened[1],
              newA,
              "second event identifies A's new tab",
            );
            assert(!newA.hasAttribute("hidden"), "new A tab is visible");
            assertEquals(
              newB.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID),
              b,
              "B attribution survives later opens",
            );
          } finally {
            win.gBrowser.tabContainer.removeEventListener("TabOpen", onOpen);
          }
        }),
    },
    {
      name: "tab context menu moves a native tab to another workspace",
      fn: () =>
        withWindows(false, false, async (open) => {
          const win = await open() as WorkspaceFlowWindow;
          const funcs = win.workspacesFuncs;
          assert(funcs, "workspace API is initialized");
          const a = funcs.getSelectedWorkspaceID();
          const fallback = win.gBrowser.selectedTab as XULElement;
          const moving = addTab(win);
          win.gBrowser.selectedTab = moving;
          const b = funcs.createNoNameWorkspace();
          funcs.changeWorkspace(a);
          await settle();
          assertEquals(
            win.gBrowser.selectedTab,
            moving,
            "context target starts selected in A",
          );
          const root = win.document.getElementById("tabContextMenu") as
            | WorkspaceFlowPopup
            | null;
          const menu = win.document.getElementById(
            "context_MoveTabToOtherWorkspace",
          );
          const popup = win.document.getElementById(
            "WorkspacesTabContextMenu",
          ) as WorkspaceFlowPopup | null;
          assert(
            root && menu && popup,
            "real workspace tab context menu is present",
          );
          const count = win.gBrowser.tabs.length;
          await withMenuCleanup(async () => {
            root.openPopup(moving, "after_start", 0, 0, true, false);
            await waitFor(
              () => root.state === "open",
              "native tab context menu opens",
            );
            assertEquals(
              win.TabContextMenu.contextTab,
              moving,
              "native context menu selects the intended tab",
            );
            popup.openPopup(menu, "end_before", 0, 0, false, false);
            await waitFor(
              () => popup.state === "open",
              "workspace submenu opens through popupshowing",
            );
            const key = `floorp.workspaces.tab.target.${encodeURIComponent(b)}`;
            const item = Array.from(popup.children).find((child) =>
              child.getAttribute("data-floorp-context-menu-key") === key
            );
            assert(item, "submenu contains the target workspace command");
            popup.activateItem(item);
            await waitFor(
              () => moving.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID) === b,
              "native menu command moves the tab",
            );
            assertEquals(
              funcs.getSelectedWorkspaceID(),
              a,
              "moving preserves the current workspace",
            );
            assertEquals(
              win.gBrowser.selectedTab,
              fallback,
              "remaining A tab becomes selected",
            );
            assert(
              moving.hasAttribute("hidden"),
              "moved B tab is hidden while A is active",
            );
            funcs.changeWorkspace(b);
            await settle();
            assert(
              !moving.hasAttribute("hidden"),
              "moved tab becomes visible in B",
            );
            assertEquals(
              win.gBrowser.tabs.length,
              count,
              "menu movement preserves the native tab count",
            );
          }, async () => {
            popup.hidePopup();
            root.hidePopup();
            await waitFor(
              () => popup.state === "closed" && root.state === "closed",
              "both native menus close",
            );
          });
        }),
    },
  ]);
}
