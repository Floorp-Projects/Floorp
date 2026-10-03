// SPDX-License-Identifier: MPL-2.0

import {
  assert,
  assertEquals,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import {
  WORKSPACE_DATA_PREF_NAME,
  WORKSPACE_ENABLED_PREF_NAME,
  WORKSPACE_PENDING_EXIT_PREF_NAME,
  WORKSPACE_TAB_ATTRIBUTION_ID,
  WORKSPACED_CONFIG_PREF_NAME,
} from "../utils/workspaces-static-names.ts";
import type { TestWindow } from "./workspace-last-tab-test-types.ts";

const CLOSE_PREF = "browser.tabs.closeWindowWithLastTab";

export async function waitFor(
  predicate: () => boolean,
  message: string,
  timeoutMs = 15_000,
): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  while (!predicate() && Date.now() < deadline) {
    await new Promise((resolve) => setTimeout(resolve, 50));
  }
  assert(predicate(), message);
}

export async function settle(): Promise<void> {
  // Window closure is deliberately deferred by the workspace manager.
  await new Promise((resolve) => setTimeout(resolve, 250));
}

export async function withWindows(
  closeWithLastTab: boolean,
  exitWorkspace: boolean,
  check: (open: () => Promise<TestWindow>) => Promise<void>,
  workspaceEnabled = true,
): Promise<void> {
  const windows: TestWindow[] = [];
  const previousClose = Services.prefs.getBoolPref(CLOSE_PREF);
  const hadClose = Services.prefs.prefHasUserValue(CLOSE_PREF);
  const previousEnabled = Services.prefs.getBoolPref(
    WORKSPACE_ENABLED_PREF_NAME,
    true,
  );
  const hadEnabled = Services.prefs.prefHasUserValue(
    WORKSPACE_ENABLED_PREF_NAME,
  );
  const previousConfig = Services.prefs.getStringPref(
    WORKSPACED_CONFIG_PREF_NAME,
  );
  const previousData = Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME);
  const previousPendingExit = Services.prefs.getBoolPref(
    WORKSPACE_PENDING_EXIT_PREF_NAME,
    false,
  );
  const hadPendingExit = Services.prefs.prefHasUserValue(
    WORKSPACE_PENDING_EXIT_PREF_NAME,
  );
  try {
    Services.prefs.setBoolPref(WORKSPACE_ENABLED_PREF_NAME, workspaceEnabled);
    Services.prefs.setBoolPref(CLOSE_PREF, closeWithLastTab);
    Services.prefs.setStringPref(
      WORKSPACED_CONFIG_PREF_NAME,
      JSON.stringify({
        ...JSON.parse(previousConfig),
        exitOnLastTabClose: exitWorkspace,
      }),
    );
    await check(async () => {
      const win = (globalThis as unknown as {
        OpenBrowserWindow(): TestWindow;
      }).OpenBrowserWindow();
      windows.push(win);
      await waitFor(
        () => Boolean(win.gBrowserInit?.delayedStartupFinished),
        "test window finishes native startup",
        30_000,
      );
      if (workspaceEnabled) {
        await waitFor(
          () =>
            Boolean(
              win.workspacesFuncs &&
                win.gBrowser?.selectedTab?.hasAttribute(
                  WORKSPACE_TAB_ATTRIBUTION_ID,
                ),
            ),
          "test window initializes Workspace and attributes its selected tab",
        );
      }
      await settle();
      assertEquals(
        Boolean(win.workspacesFuncs),
        workspaceEnabled,
        "workspace manager follows the feature preference",
      );
      assertEquals(
        win.gBrowser.tabs.length,
        1,
        "test window starts with one tab",
      );
      assertEquals(
        Services.prefs.getBoolPref(CLOSE_PREF),
        closeWithLastTab,
        "opening a window preserves the native preference",
      );
      return win;
    });
    assertEquals(
      Services.prefs.getBoolPref(CLOSE_PREF),
      closeWithLastTab,
      "tab and workspace changes preserve the native preference",
    );
  } finally {
    for (const win of windows) {
      if (!win.closed) win.close();
    }
    await waitFor(
      () => windows.every((win) => win.closed),
      "test windows close",
    );
    await settle();
    const actualClose = Services.prefs.getBoolPref(CLOSE_PREF);
    Services.prefs.setStringPref(WORKSPACE_DATA_PREF_NAME, previousData);
    Services.prefs.setStringPref(WORKSPACED_CONFIG_PREF_NAME, previousConfig);
    if (hadEnabled) {
      Services.prefs.setBoolPref(WORKSPACE_ENABLED_PREF_NAME, previousEnabled);
    } else Services.prefs.clearUserPref(WORKSPACE_ENABLED_PREF_NAME);
    if (hadClose) Services.prefs.setBoolPref(CLOSE_PREF, previousClose);
    else Services.prefs.clearUserPref(CLOSE_PREF);
    if (hadPendingExit) {
      Services.prefs.setBoolPref(
        WORKSPACE_PENDING_EXIT_PREF_NAME,
        previousPendingExit,
      );
    } else Services.prefs.clearUserPref(WORKSPACE_PENDING_EXIT_PREF_NAME);
    assertEquals(
      actualClose,
      closeWithLastTab,
      "closing windows preserves the native preference",
    );
  }
}

export function addHiddenWorkspace(win: TestWindow): XULElement {
  const funcs = win.workspacesFuncs;
  assert(funcs, "workspace API is available");
  const original = funcs.getSelectedWorkspaceID();
  funcs.createNoNameWorkspace();
  const hidden = win.gBrowser.selectedTab as XULElement;
  funcs.changeWorkspace(original);
  assert(hidden.hasAttribute("hidden"), "other workspace tab is hidden");
  return hidden;
}

export function nativeHiddenTabTests(
  workspaceEnabled: boolean,
  closeValues: boolean[] = [false, true],
): TestCase[] {
  return closeValues.flatMap((closeWithLastTab) =>
    [
      "extension",
      "unattributed",
      "collapsed-group",
      ...(workspaceEnabled ? [] : ["stale-workspace"]),
    ].map((hiddenKind) => ({
      name:
        `${hiddenKind} tab retains native behavior (workspaces ${workspaceEnabled}, native ${closeWithLastTab})`,
      fn: () =>
        withWindows(closeWithLastTab, false, async (open) => {
          const win = await open();
          const closing = win.gBrowser.selectedTab as XULElement;
          const other = win.gBrowser.addTab("about:blank", {
            triggeringPrincipal: Services.scriptSecurityManager
              .getSystemPrincipal(),
            inBackground: true,
          }) as XULElement;
          if (hiddenKind === "collapsed-group") {
            const group = win.gBrowser.addTabGroup([other]);
            group.collapsed = true;
            assert(
              !other.hasAttribute("hidden"),
              "collapsed group uses visibility, not the hidden attribute",
            );
          } else {
            if (hiddenKind === "unattributed") {
              other.removeAttribute(WORKSPACE_TAB_ATTRIBUTION_ID);
            } else if (hiddenKind === "stale-workspace") {
              closing.setAttribute(
                WORKSPACE_TAB_ATTRIBUTION_ID,
                "11111111-1111-4111-8111-111111111111",
              );
              other.setAttribute(
                WORKSPACE_TAB_ATTRIBUTION_ID,
                "22222222-2222-4222-8222-222222222222",
              );
            } else if (workspaceEnabled) {
              assertEquals(
                other.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID),
                closing.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID),
                "extension hides a tab in the current workspace",
              );
            }
            win.gBrowser.hideTab(
              other,
              "workspace-last-tab-test@example.invalid",
            );
            assert(
              other.hasAttribute("hidden"),
              "native hideTab hides the other tab",
            );
          }
          win.gBrowser.removeTab(closing, { animate: false });
          if (closeWithLastTab && hiddenKind !== "collapsed-group") {
            await waitFor(
              () => win.closed,
              "non-workspace hidden tab does not override native last-tab closure",
            );
          } else {
            await settle();
            assert(
              !win.closed,
              "native false or the collapsed group's open tab keeps the window alive",
            );
            assert(
              win.gBrowser.tabs.includes(other),
              "the other tab survives",
            );
          }
        }, workspaceEnabled),
    }))
  );
}
