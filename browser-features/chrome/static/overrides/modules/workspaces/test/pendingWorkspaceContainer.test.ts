// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  shouldBlockWorkspaceOpenLink,
  workspaceDefaultIsPending,
} from "../open-link-user-context.ts";
import {
  assert,
  assertEquals as assertValue,
  runTests,
} from "../../../../../test/utils/test_harness.ts";
import Workspaces from "#features-chrome/common/workspaces";

function assertEquals(actual: unknown, expected: unknown): void {
  assertValue(
    actual,
    expected,
    "container operation must preserve the expected state",
  );
}

export async function runAllTests(): Promise<void> {
  await runTests("pendingWorkspaceContainer.test.ts", [{
    name:
      "early startup waits for workspace service while private and explicit opens remain available",
    fn: () => {
      const noService = workspaceDefaultIsPending(true, false, undefined);
      assertEquals(noService, true);
      assertEquals(workspaceDefaultIsPending(true, true, undefined), false);
      assertEquals(workspaceDefaultIsPending(false, false, undefined), false);
      assertEquals(workspaceDefaultIsPending(true, false, true), false);
      assertEquals(
        shouldBlockWorkspaceOpenLink(undefined, "tab", noService),
        true,
      );
      assertEquals(
        shouldBlockWorkspaceOpenLink({ userContextId: 0 }, "tab", noService),
        false,
      );
    },
  }, {
    name: "unresolved defaults block only implicit new tabs",
    fn: () => {
      assertEquals(shouldBlockWorkspaceOpenLink(undefined, "tab", true), true);
      assertEquals(shouldBlockWorkspaceOpenLink({}, "tabshifted", true), true);
      assertEquals(
        shouldBlockWorkspaceOpenLink(undefined, "current", true),
        false,
      );
      assertEquals(
        shouldBlockWorkspaceOpenLink({ userContextId: 0 }, "tab", true),
        false,
      );
      assertEquals(
        shouldBlockWorkspaceOpenLink({ userContextId: 7 }, "tab", true),
        false,
      );
      assertEquals(
        shouldBlockWorkspaceOpenLink(
          { targetBrowser: { userContextId: 0 } },
          "tab",
          true,
        ),
        false,
      );
      assertEquals(
        shouldBlockWorkspaceOpenLink(undefined, "tab", false),
        false,
      );
    },
  }, {
    name:
      "the new tab command opens the chosen tab and resolves its Gecko notification",
    fn: async () => {
      const workspace = Workspaces.getCtx();
      assert(workspace, "workspace service is initialized");
      const pref = "privacy.userContext.enabled";
      const previous = Services.prefs.prefHasUserValue(pref)
        ? Services.prefs.getBoolPref(pref)
        : null;
      const canOpen = workspace.canOpenWorkspaceTab;
      const previousTabs = new Set(gBrowser.tabs);
      const notifications: Promise<XULBrowserElement>[] = [];
      const onTabStart: nsIObserver = (subject) => {
        notifications.push(
          (subject as unknown as {
            wrappedJSObject: Promise<XULBrowserElement>;
          }).wrappedJSObject,
        );
      };
      let dialogs = 0;
      let accept = false;
      const onDialog: nsIObserver = (subject) => {
        const promptWindow = subject as unknown as Window;
        if (
          promptWindow.document.documentURI !==
            "chrome://global/content/selectDialog.xhtml"
        ) return;
        dialogs++;
        const dialog = promptWindow.document.querySelector(
          "dialog",
        ) as unknown as {
          acceptDialog(): void;
          cancelDialog(): void;
        };
        if (dialogs === 1) BrowserCommands.openTab();
        if (accept) dialog.acceptDialog();
        else dialog.cancelDialog();
      };
      const openFromUser = () => {
        const activation = window.windowUtils.setHandlingUserInput(true);
        try {
          BrowserCommands.openTab();
        } finally {
          activation.destruct();
        }
      };
      Services.obs.addObserver(onTabStart, "browser-open-newtab-start");
      Services.obs.addObserver(onDialog, "select-dialog-loaded");
      try {
        workspace.canOpenWorkspaceTab = () => false;
        Services.prefs.setBoolPref(pref, true);
        BrowserCommands.openTab();
        assertEquals(dialogs, 0);
        openFromUser();
        assertEquals(dialogs, 1);
        assertEquals(notifications.length, 0);
        assertEquals(gBrowser.tabs.length, previousTabs.size);
        accept = true;
        const newTabMenu = document.getElementById("menu_newNavigatorTab") as
          | XULElement
          | null;
        assert(newTabMenu, "File menu has a New Tab item");
        newTabMenu.doCommand();
        assertEquals(dialogs, 2);
        assertEquals(notifications.length, 1);
        assertEquals(await notifications[0], gBrowser.selectedBrowser);
        assertEquals(
          gBrowser.selectedTab.getAttribute("usercontextid") || "0",
          "0",
        );
        Services.prefs.setBoolPref(pref, false);
        const beforeLink = gBrowser.tabs.length;
        BrowserCommands.openTab({ url: "https://example.invalid/" });
        openTrustedLinkIn("https://example.invalid/", "tab");
        openUILinkIn("https://example.invalid/", "tab");
        assertEquals(gBrowser.tabs.length, beforeLink);
        assertEquals(notifications.length, 1);
        openFromUser();
        assertEquals(dialogs, 2);
        assertEquals(notifications.length, 2);
        assertEquals(await notifications[1], gBrowser.selectedBrowser);
      } finally {
        Services.obs.removeObserver(onDialog, "select-dialog-loaded");
        Services.obs.removeObserver(onTabStart, "browser-open-newtab-start");
        workspace.canOpenWorkspaceTab = canOpen;
        if (previous === null) Services.prefs.clearUserPref(pref);
        else Services.prefs.setBoolPref(pref, previous);
        for (const tab of [...gBrowser.tabs]) {
          if (!previousTabs.has(tab)) gBrowser.removeTab(tab);
        }
      }
    },
  }]);
}
