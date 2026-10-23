// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import Workspaces from "../../workspaces/index.ts";
import { workspacesDataStore } from "../../workspaces/data/data.ts";
import { enabled, setEnabled } from "../../workspaces/data/config.ts";
import { WORKSPACE_DATA_PREF_NAME } from "../../workspaces/utils/workspaces-static-names.ts";
import { getWorkspaceSessionIdentityService } from "../../workspaces/utils/workspace-sessions.ts";
import { getWorkspaceSessionRuntime } from "../../workspaces/utils/workspace-session-runtime.ts";
import { config, setConfig } from "../../designs/configs.ts";
import { waitFor } from "../../workspaces/test/workspace-last-tab-test-utils.ts";
import { sessionName } from "../session-info.ts";
import type {
  IndicatorTestBrowser,
  IndicatorTestSidebar,
  IndicatorTestSplitView,
  IndicatorTestTab,
} from "./types.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

function tabBadge(tab: IndicatorTestTab): Element | null {
  return tab.querySelector(".floorp-session-badge");
}

function paneBadge(tab: IndicatorTestTab): Element | null {
  return document.getElementById(tab.linkedPanel)?.querySelector(
    ".browserStack > .floorp-pane-session-badge",
  ) ?? null;
}

function visiblyInside(element: Element | null, container: Element): boolean {
  if (!element) return false;
  const box = element.getBoundingClientRect();
  const bounds = container.getBoundingClientRect();
  if (
    box.width < 2 || box.height < 2 || bounds.width < 2 || bounds.height < 2
  ) {
    return false;
  }
  for (
    let ancestor: Element | null = element;
    ancestor;
    ancestor = ancestor.parentElement
  ) {
    const style = getComputedStyle(ancestor);
    if (style?.display === "none" || style?.visibility === "hidden") {
      return false;
    }
  }
  return box.left >= bounds.left - 1 && box.right <= bounds.right + 1 &&
    box.top >= bounds.top - 1 && box.bottom <= bounds.bottom + 1;
}

function hasReadablePaneName(tab: IndicatorTestTab, name: string): boolean {
  const panel = document.getElementById(tab.linkedPanel);
  const badge = paneBadge(tab);
  return Boolean(
    panel && badge && badge.textContent?.includes(name) &&
      badge.getAttribute("aria-label")?.includes(name) &&
      visiblyInside(badge, panel) && badge.scrollWidth <= badge.clientWidth + 2,
  );
}

async function testNativeIndicatorLayoutsAndRecoveryAction(): Promise<void> {
  await waitFor(
    () => Boolean(Workspaces.getCtx()),
    "workspace service initialized",
  );
  const service = Workspaces.getCtx();
  assert(service, "workspace service exists");
  const browser = globalThis.gBrowser as unknown as IndicatorTestBrowser;
  const sidebar =
    (globalThis as unknown as { SidebarController: IndicatorTestSidebar })
      .SidebarController;
  const originalSidebar = sidebar.getUIState();
  const originalConfig = structuredClone(config());
  const originalWorkspace = service.getSelectedWorkspaceID();
  const originalTab = browser.selectedTab;
  const originalTabs = new Set(browser.tabs);
  const originalWorkspaces = new Set(workspacesDataStore.order);
  const originalData = Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME);
  const originalEnabled = enabled();
  const createModal = service.modalCtx.showCreateWorkspacesModal;
  const recover = service.recoverMissingSessionFromDialog;
  const identities = getWorkspaceSessionIdentityService();
  const preferences = [
    "sidebar.revamp",
    "sidebar.verticalTabs",
    "browser.tabs.splitView.enabled",
  ].map((name) => ({
    name,
    existed: Services.prefs.prefHasUserValue(name),
    value: Services.prefs.getBoolPref(name, false),
  }));
  let createdContext: number | undefined;
  let split: IndicatorTestSplitView | null = null;
  try {
    setEnabled(true);
    setConfig((previous) => ({
      ...previous,
      tabbar: { ...previous.tabbar, tabbarStyle: "horizontal" },
    }));
    Services.prefs.setBoolPref("sidebar.verticalTabs", false);
    await waitFor(
      () => !browser.tabContainer.verticalMode,
      "horizontal native tabs initialized",
    );
    service.modalCtx.showCreateWorkspacesModal = () =>
      Promise.resolve({ name: "Session indicator DOM", userContextId: "new" });
    const workspaceId = await service.createWorkspaceFromDialog();
    assert(
      workspaceId,
      "temporary workspace created through the normal dialog path",
    );
    const matching = browser.selectedTab;
    createdContext = service.getRawWorkspace(workspaceId)?.userContextId;
    assert(
      createdContext && createdContext > 0,
      "workspace has a real named login environment",
    );
    const exception = browser.addTab("about:blank", {
      userContextId: 0,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    browser.selectedTab = exception;
    const standard = sessionName(0);
    await waitFor(
      () =>
        hasReadablePaneName(exception, standard) &&
        visiblyInside(tabBadge(exception), exception),
      "normal exception shows its actual session in chrome and beside the page",
    );
    assertEquals(
      paneBadge(exception)?.localName,
      "span",
      "ordinary session labels are informational",
    );
    assertEquals(
      paneBadge(exception)?.getAttribute("role"),
      "note",
      "ordinary session labels have an informational role",
    );
    assertEquals(
      tabBadge(exception)?.textContent,
      standard,
      "tab label names its actual session rather than workspace default",
    );
    assert(
      exception.hasAttribute("floorp-session-exception"),
      "exception marker is attached to the real native tab",
    );

    browser.pinTab(matching);
    browser.pinTab(exception);
    await waitFor(
      () =>
        visiblyInside(tabBadge(exception), exception) &&
        hasReadablePaneName(exception, standard),
      "selected pinned exception keeps a visible marker and a readable full session name",
    );
    const pinWidth = matching.getBoundingClientRect().width;
    assert(
      Math.abs(exception.getBoundingClientRect().width - pinWidth) <= 1,
      "session marker does not widen native pinned positions",
    );
    assert(
      tabBadge(exception)!.getBoundingClientRect().width <= 10,
      "pinned indicator uses the compact marker instead of a clipped text label",
    );

    Services.prefs.setBoolPref("sidebar.revamp", true);
    Services.prefs.setBoolPref("sidebar.verticalTabs", true);
    setConfig((previous) => ({
      ...previous,
      tabbar: { ...previous.tabbar, tabbarStyle: "vertical" },
    }));
    await waitFor(
      () => browser.tabContainer.verticalMode,
      "native vertical tabs initialized",
    );
    await sidebar.updateUIState({
      launcherExpanded: false,
      launcherVisible: true,
    });
    await waitFor(
      () => !browser.tabContainer.hasAttribute("expanded"),
      "vertical tab rail collapses",
    );
    assert(
      visiblyInside(tabBadge(exception), exception),
      "collapsed vertical pinned marker stays inside its native tab",
    );
    assert(
      hasReadablePaneName(exception, standard),
      "collapsed vertical mode retains the full selected session name beside the page",
    );

    browser.unpinTab(matching);
    browser.unpinTab(exception);
    await waitFor(
      () => visiblyInside(tabBadge(exception), exception),
      "collapsed vertical regular tab also retains its compact marker",
    );
    assert(
      tabBadge(exception)!.getBoundingClientRect().width <= 10,
      "collapsed vertical labels do not expand the icon rail",
    );
    Services.prefs.setBoolPref("sidebar.verticalTabs", false);
    setConfig((previous) => ({
      ...previous,
      tabbar: { ...previous.tabbar, tabbarStyle: "horizontal" },
    }));
    await waitFor(
      () => !browser.tabContainer.verticalMode,
      "horizontal native tabs restored",
    );
    if (typeof browser.addTabSplitView === "function") {
      Services.prefs.setBoolPref("browser.tabs.splitView.enabled", true);
      split = browser.addTabSplitView([matching, exception], {
        insertBefore: matching,
      });
      assert(split, "native split view created");
      browser.showSplitViewPanels?.([matching, exception]);
      browser.selectedTab = exception;
      const defaultName = sessionName(createdContext);
      await waitFor(
        () =>
          hasReadablePaneName(matching, defaultName) &&
          hasReadablePaneName(exception, standard),
        "each real split pane names its own session, including the matching default pane",
      );
      assertEquals(
        paneBadge(matching)?.getAttribute("role"),
        "note",
        "matching default split pane remains informational",
      );
      split.unsplitTabs();
      split = null;
    }

    const pending = browser.addTab("about:blank", {
      userContextId: 0,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    const oldId = 2_147_483_600;
    getWorkspaceSessionRuntime().markPendingTab(pending, {
      userContextId: oldId,
      floorpWorkspaceId: workspaceId,
      entries: [{ url: "https://example.invalid/held-indicator" }],
      index: 1,
    });
    browser.selectedTab = pending;
    await waitFor(
      () =>
        Boolean(
          paneBadge(pending)?.hasAttribute("data-pending") &&
            visiblyInside(
              paneBadge(pending),
              document.getElementById(pending.linkedPanel)!,
            ),
        ),
      "pending page shows a visible recovery warning",
    );
    const warning = paneBadge(pending) as HTMLButtonElement;
    assertEquals(
      warning.localName,
      "button",
      "only a pending label is an action button",
    );
    assert(warning.textContent?.startsWith("⚠"), "pending warning is explicit");
    let recoveredOldId: number | undefined;
    let recoveredWorkspace: string | undefined;
    service.recoverMissingSessionFromDialog = (id, target) => {
      recoveredOldId = id;
      recoveredWorkspace = target;
      return Promise.resolve(false);
    };
    warning.click();
    assertEquals(
      recoveredOldId,
      oldId,
      "recovery action uses the pending tab's original identity",
    );
    assertEquals(
      recoveredWorkspace,
      workspaceId,
      "recovery action targets the owning workspace",
    );
  } finally {
    service.modalCtx.showCreateWorkspacesModal = createModal;
    service.recoverMissingSessionFromDialog = recover;
    split?.unsplitTabs();
    service.changeWorkspace(originalWorkspace);
    browser.selectedTab = originalTab;
    for (const id of [...workspacesDataStore.order]) {
      if (!originalWorkspaces.has(id)) service.deleteWorkspace(id);
    }
    for (const tab of [...browser.tabs]) {
      if (!originalTabs.has(tab)) browser.removeTab(tab);
    }
    Services.prefs.setStringPref(WORKSPACE_DATA_PREF_NAME, originalData);
    service.changeWorkspace(originalWorkspace);
    browser.selectedTab = originalTab;
    if (createdContext) identities.remove(createdContext);
    setConfig(originalConfig);
    for (const preference of preferences) {
      if (preference.existed) {
        Services.prefs.setBoolPref(preference.name, preference.value);
      } else Services.prefs.clearUserPref(preference.name);
    }
    await sidebar.updateUIState(originalSidebar);
    setEnabled(originalEnabled);
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("sessionIndicatorDOM.test.ts", [{
    name:
      "Real exception, pinned, collapsed vertical and split-pane session labels remain visible and pending warnings recover the owning workspace",
    fn: testNativeIndicatorLayoutsAndRecoveryAction,
  }]);
}
