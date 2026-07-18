/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
import {
  createEffect,
  createRoot,
  getOwner,
  onCleanup,
  runWithOwner,
} from "solid-js";
import { selectedWorkspaceID } from "./data/data.ts";
import type {
  PanelMultiViewParentElement,
  TWorkspaceID,
} from "./utils/type.ts";
import { zWorkspaceID } from "./utils/type.ts";
import {
  WORKSPACE_LAST_SHOW_ID,
  WORKSPACE_PENDING_EXIT_PREF_NAME,
  WORKSPACE_TAB_ATTRIBUTION_ID,
} from "./utils/workspaces-static-names.ts";
import { configStore, enabled } from "./data/config.ts";
import type { WorkspaceIcons } from "./utils/workspace-icons.ts";
import type { WorkspacesDataManager } from "./workspacesDataManagerBase.tsx";
import { isRight } from "fp-ts/Either";
import {
  excludeTrackedReplacement,
  FirefoxTabReplacementTracker,
  hasOriginUserContextId,
} from "./utils/tab-replacement-lifecycle.ts";

interface TabEvent extends Event {
  target: XULElement;
  forUnsplit?: boolean;
}

export class WorkspacesTabManager {
  dataManagerCtx: WorkspacesDataManager;
  iconCtx: WorkspaceIcons;
  // When true, handleTabClose skips its workspace-empty logic. Used during
  // bulk tab removal (workspace deletion) so that closing tabs one-by-one
  // does not interfere with the deletion flow (fixes #2247).
  private suppressTabCloseHandling = false;
  private readonly firefoxReplacementTracker = new FirefoxTabReplacementTracker<
    XULElement
  >();
  constructor(iconCtx: WorkspaceIcons, dataManagerCtx: WorkspacesDataManager) {
    this.iconCtx = iconCtx;
    this.dataManagerCtx = dataManagerCtx;
    this.boundHandleTabClose = this.handleTabClose.bind(this);
    this.boundHandleTabOpen = this.handleTabOpen.bind(this);

    const initWorkspace = () => {
      (
        globalThis as unknown as {
          SessionStore: { promiseAllWindowsRestored: Promise<void> };
        }
      ).SessionStore.promiseAllWindowsRestored
        .then(() => {
          this.initializeWorkspace();
          globalThis.addEventListener(
            "TabClose",
            this.boundHandleTabClose as EventListener,
          );
          globalThis.addEventListener("TabOpen", this.boundHandleTabOpen);
        })
        .catch((error: Error) => {
          console.error("Error waiting for windows restore:", error);
          this.initializeWorkspace();
          globalThis.addEventListener(
            "TabClose",
            this.boundHandleTabClose as EventListener,
          );
          globalThis.addEventListener("TabOpen", this.boundHandleTabOpen);
        });
    };

    initWorkspace();

    createEffect(() => {
      try {
        const prefName = "browser.tabs.closeWindowWithLastTab";
        // Always disable Firefox's auto-close on last tab.
        const desiredValue = false;
        try {
          const current = Services.prefs.getBoolPref(prefName, true);
          if (current !== desiredValue) {
            Services.prefs.setBoolPref(prefName, desiredValue);
          }
        } catch {
          // Ensure pref is disabled even if reading fails
          Services.prefs.setBoolPref(prefName, desiredValue);
        }
      } catch (e) {
        console.warn(
          "WorkspacesTabManager: failed to set closeWindowWithLastTab pref",
          e,
        );
      }
    });

    const owner = getOwner?.();
    const exec = () =>
      createEffect(() => {
        if (!enabled()) {
          return;
        }

        if (selectedWorkspaceID()) {
          this.updateTabsVisibility();
        }
      });
    if (owner) runWithOwner(owner, exec);
    else createRoot(exec);

    onCleanup(() => {
      this.cleanup();
    });
  }

  private initializeWorkspace() {
    // Collapse duplicated startup "new" tabs when last exit was triggered by
    // workspace-empty quit. This prevents two blank/home tabs after restart.
    try {
      if (Services.prefs.getBoolPref(WORKSPACE_PENDING_EXIT_PREF_NAME, false)) {
        const homepage = Services.prefs.getStringPref(
          "browser.startup.homepage",
          "",
        );
        const isStartupNewURL = (url: string): boolean => {
          const u = url || "";
          return (
            u === "about:newtab" ||
            u === "about:home" ||
            u === "about:blank" ||
            (homepage !== "" && u === homepage)
          );
        };
        const tabs = (globalThis.gBrowser.tabs as XULElement[]) || [];
        const startupNewTabs: XULElement[] = [];
        for (const t of tabs) {
          try {
            const browser = globalThis.gBrowser.getBrowserForTab(t);
            const currentUrl = browser?.currentURI?.spec || "";
            if (isStartupNewURL(currentUrl)) {
              startupNewTabs.push(t);
            }
          } catch {
            // ignore
          }
        }
        if (startupNewTabs.length >= 2) {
          // Suppress handleTabClose to avoid interference during cleanup
          this.suppressTabCloseHandling = true;
          try {
            // Keep first, remove the rest
            for (let i = 1; i < startupNewTabs.length; i++) {
              try {
                globalThis.gBrowser.removeTab(startupNewTabs[i]);
              } catch {
                // ignore
              }
            }
          } finally {
            this.suppressTabCloseHandling = false;
          }
        }
        Services.prefs.setBoolPref(WORKSPACE_PENDING_EXIT_PREF_NAME, false);
      }
    } catch (e) {
      console.debug(
        "WorkspacesTabManager: failed collapsing startup duplicates",
        e,
      );
    }

    let maybeSelectedWorkspace = this.getWorkspaceIdFromAttribute(
      globalThis.gBrowser.selectedTab as XULElement,
    );

    if (!maybeSelectedWorkspace) {
      maybeSelectedWorkspace = this.getMaybeSelectedWorkspacebyVisibleTabs();
    }
    if (maybeSelectedWorkspace) {
      this.changeWorkspace(maybeSelectedWorkspace);
    } else {
      try {
        const defaultWorkspaceId = this.dataManagerCtx.getDefaultWorkspaceID();
        this.changeWorkspace(defaultWorkspaceId);
      } catch (e) {
        console.error("Failed to change workspace:", e);
        // Fallback: create default workspace when store is empty or invalid
        try {
          const createdId = this.dataManagerCtx.createWorkspace("Workspace");
          this.dataManagerCtx.setDefaultWorkspace(createdId);
          this.changeWorkspace(createdId);
        } catch (createErr) {
          console.error(
            "Failed to create and switch to default workspace:",
            createErr,
          );
        }
      }
    }
  }

  private boundHandleTabClose: (event: TabEvent) => void;
  private boundHandleTabOpen: (event: Event) => void;

  public cleanup() {
    globalThis.removeEventListener(
      "TabClose",
      this.boundHandleTabClose as EventListener,
    );
    globalThis.removeEventListener("TabOpen", this.boundHandleTabOpen);
  }

  private handleTabClose = (event: TabEvent) => {
    const tab = event.target as XULElement;
    // Consume the transaction before any close logic can synchronously create
    // another tab. This also runs for suppressed closes so their transaction
    // cannot be revived by a reentrant TabOpen.
    const trackedReplacement = this.firefoxReplacementTracker.finishTabClose(
      tab,
    );

    // Skip workspace-empty logic when bulk-removing tabs (e.g. workspace deletion)
    if (this.suppressTabCloseHandling) return;

    let workspaceId = this.getWorkspaceIdFromAttribute(tab);

    // If the tab has no workspace attribute, assign it to the current workspace
    // so that the "last tab close" logic can still fire correctly (fixes #2201).
    if (!workspaceId) {
      const currentId = this.dataManagerCtx.getSelectedWorkspaceID();
      if (currentId) {
        workspaceId = currentId;
      } else {
        return;
      }
    }
    if (!workspaceId) return;
    const closingWorkspaceId = workspaceId;

    const currentWorkspaceId = this.dataManagerCtx.getSelectedWorkspaceID();
    const isCurrentWorkspace = closingWorkspaceId === currentWorkspaceId;
    const allTabs = globalThis.gBrowser.tabs as XULElement[];
    const remainingTabs = allTabs.filter((t) => t !== tab);
    const replacementTab = trackedReplacement !== null &&
        remainingTabs.includes(trackedReplacement)
      ? trackedReplacement
      : null;
    const remainingUserTabs = excludeTrackedReplacement(
      remainingTabs,
      replacementTab,
    );

    const resolveWorkspaceIdForClose = (
      targetTab: XULElement,
    ): TWorkspaceID => {
      return this.getWorkspaceIdFromAttribute(targetTab) ?? currentWorkspaceId;
    };
    const workspaceTabs = remainingUserTabs.filter((t) => {
      return resolveWorkspaceIdForClose(t) === closingWorkspaceId;
    });

    if (isCurrentWorkspace && workspaceTabs.length === 0) {
      // Current workspace is becoming empty.
      // Check if there are tabs in OTHER workspaces.
      const otherWorkspaceTabs = remainingUserTabs.filter((t) => {
        const wsId = resolveWorkspaceIdForClose(t);
        return wsId !== closingWorkspaceId;
      });

      if (otherWorkspaceTabs.length > 0) {
        // There are tabs in other workspaces.
        // Check if exitOnLastTabClose is enabled - if not, just create a new tab
        // and switch to another workspace instead of closing the window.
        if (!configStore.exitOnLastTabClose) {
          if (replacementTab) {
            this.reuseOrReplaceTrackedReplacement(
              replacementTab,
              closingWorkspaceId,
              false,
            );
          }

          // Find the first workspace with tabs and switch to it
          const firstOtherTab = otherWorkspaceTabs[0];
          const targetWorkspaceId = this.getWorkspaceIdFromAttribute(
            firstOtherTab,
          );
          if (targetWorkspaceId) {
            this.changeWorkspace(targetWorkspaceId);
          }
          return;
        }

        // Reuse only the exact TabOpen object Firefox created for this close,
        // and only when its browsing context is in the workspace container.
        const replacement = this.reuseOrReplaceTrackedReplacement(
          replacementTab,
          closingWorkspaceId,
          true,
        );

        replacement.setAttribute(WORKSPACE_LAST_SHOW_ID, closingWorkspaceId);
        this.dataManagerCtx.setCurrentWorkspaceID(closingWorkspaceId);
        this.updateTabsVisibility();

        // Set pending exit pref to collapse duplicates on restart
        Services.prefs.setBoolPref(WORKSPACE_PENDING_EXIT_PREF_NAME, true);

        // If the user closes the last tab in the current workspace, close the window
        // but keep the session (including tabs in other workspaces).
        // Since we forced browser.tabs.closeWindowWithLastTab to false, we need to
        // close the window manually.
        setTimeout(() => {
          globalThis.close();
        }, 0);
      } else {
        // If no other workspace tabs exist, this is the last tab in the window.
        // Check if exitOnLastTabClose is enabled - if not, create a new tab
        // instead of closing the window.
        if (!configStore.exitOnLastTabClose) {
          this.reuseOrReplaceTrackedReplacement(
            replacementTab,
            closingWorkspaceId,
            true,
          );
          this.updateTabsVisibility();
          return;
        }

        // exitOnLastTabClose is true. Only the exact replacement for this
        // transaction is disposable; Floorp Start, user newtabs, and stale
        // blank tabs all keep the window alive (#2509).
        if (remainingUserTabs.length === 0) {
          if (replacementTab) {
            this.reuseOrReplaceTrackedReplacement(
              replacementTab,
              closingWorkspaceId,
              true,
            );
          }
          Services.prefs.setBoolPref(WORKSPACE_PENDING_EXIT_PREF_NAME, true);
          setTimeout(() => {
            globalThis.close();
          }, 0);
        } else {
          // Remaining user tabs exist (e.g., user just opened a new tab).
          // Keep any tracked native replacement only after validating its
          // origin context, then assign the first user tab to the workspace.
          if (replacementTab) {
            this.reuseOrReplaceTrackedReplacement(
              replacementTab,
              closingWorkspaceId,
              false,
            );
          }
          const newTab = remainingUserTabs[0];
          this.setWorkspaceIdToAttribute(newTab, closingWorkspaceId);
          globalThis.gBrowser.selectedTab = newTab;
          this.updateTabsVisibility();
        }
      }
    } else if (replacementTab) {
      // A Firefox replacement should normally imply the current workspace is
      // becoming empty. If visibility or attribution state says otherwise,
      // still fail closed rather than leaving an unvalidated native tab alive.
      this.reuseOrReplaceTrackedReplacement(
        replacementTab,
        closingWorkspaceId,
        false,
      );
      this.updateTabsVisibility();
    }
  };

  private handleTabOpen = (event: Event) => {
    try {
      const tab = (event as CustomEvent).target as XULElement;
      this.firefoxReplacementTracker.observeTabOpen(
        tab,
        globalThis.gBrowser.tabs as XULElement[],
      );
      const wsId = this.getWorkspaceIdFromAttribute(tab) ??
        this.dataManagerCtx.getSelectedWorkspaceID();
      if (!this.getWorkspaceIdFromAttribute(tab)) {
        this.setWorkspaceIdToAttribute(tab, wsId);
      }
    } catch {
      // ignore tab-open handler error
    }
  };

  public updateTabsVisibility() {
    const currentWorkspaceId = this.dataManagerCtx.getSelectedWorkspaceID();
    const selectedTab = globalThis.gBrowser.selectedTab;
    if (
      selectedTab &&
      !selectedTab.hasAttribute(WORKSPACE_LAST_SHOW_ID) &&
      selectedTab.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID) ===
        currentWorkspaceId
    ) {
      const lastShowWorkspaceTabs = document?.querySelectorAll(
        `[${WORKSPACE_LAST_SHOW_ID}="${currentWorkspaceId}"]`,
      );

      if (lastShowWorkspaceTabs) {
        for (const lastShowWorkspaceTab of lastShowWorkspaceTabs) {
          lastShowWorkspaceTab.removeAttribute(WORKSPACE_LAST_SHOW_ID);
        }
      }

      selectedTab.setAttribute(WORKSPACE_LAST_SHOW_ID, currentWorkspaceId);
    }

    // Check Tabs visibility
    const tabs = globalThis.gBrowser.tabs as Array<
      XULElement | undefined | null
    >;
    for (const tab of tabs) {
      if (!tab) continue;
      // Set workspaceId if workspaceId is null
      const workspaceId = this.getWorkspaceIdFromAttribute(tab);
      if (!workspaceId) {
        this.setWorkspaceIdToAttribute(tab, currentWorkspaceId);
      }

      const chackedWorkspaceId = this.getWorkspaceIdFromAttribute(tab);
      if (chackedWorkspaceId === currentWorkspaceId) {
        globalThis.gBrowser.showTab(tab);
      } else {
        globalThis.gBrowser.hideTab(tab);
      }
    }

    // Hide tab groups that have no visible tabs, show those that do
    const tabGroups = globalThis.gBrowser.tabGroups;
    for (const group of tabGroups) {
      const hasVisibleTabInGroup = (group.tabs as Array<XULElement>)
        .some((tab) =>
          this.getWorkspaceIdFromAttribute(tab) === currentWorkspaceId
        );
      group.style.display = hasVisibleTabInGroup ? "" : "none";
    }

    // Hide split view wrappers that have no visible tabs, show those that do
    const splitViewWrappers = document.querySelectorAll(
      "tab-split-view-wrapper",
    );
    for (const wrapper of splitViewWrappers) {
      const children = Array.from(wrapper.children) as Element[];
      const hasVisibleTabInWrapper = children.some(
        (child) => {
          if (child.tagName !== "tab") return false;
          return (
            this.getWorkspaceIdFromAttribute(child as XULElement) ===
              currentWorkspaceId
          );
        },
      );
      (wrapper as HTMLElement).style.display = hasVisibleTabInWrapper
        ? ""
        : "none";
    }
  }

  /**
   * Get workspaceId from tab attribute.
   * @param tab The tab.
   * @returns The workspace id.
   */
  getWorkspaceIdFromAttribute(tab: XULElement): TWorkspaceID | null {
    const raw = tab.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID);
    if (!raw) {
      return null;
    }
    const clean = raw.replace(/[{}]/g, "");
    const parseResult = zWorkspaceID.decode(clean);
    if (!isRight(parseResult)) {
      console.warn("WorkspacesTabManager: invalid workspace id format:", raw);
      return null;
    }
    const wsId = parseResult.right;
    if (!this.dataManagerCtx.isWorkspaceID(wsId)) {
      console.warn(
        "WorkspacesTabManager: workspace id not found in store:",
        wsId,
      );
      return null;
    }
    return wsId;
  }

  /**
   * Set workspaceId to tab attribute.
   * @param tab The tab.
   * @param workspaceId The workspace id.
   */
  setWorkspaceIdToAttribute(tab: XULElement, workspaceId: TWorkspaceID) {
    tab.setAttribute(WORKSPACE_TAB_ATTRIBUTION_ID, workspaceId);
  }

  /**
   * Reuse a tracked native replacement only when its authoritative browsing
   * context matches the workspace. A replacement with missing or mismatched
   * origin attributes is replaced before it is removed, so Firefox never sees
   * an empty window and creates another keep-alive tab during this handler.
   */
  private reuseOrReplaceTrackedReplacement(
    replacement: XULElement | null,
    workspaceId: TWorkspaceID,
    select: boolean,
  ): XULElement {
    const expectedUserContextId = this.dataManagerCtx.getRawWorkspace(
      workspaceId,
    )?.userContextId ?? 0;

    if (
      replacement &&
      this.tabHasOriginUserContextId(replacement, expectedUserContextId)
    ) {
      this.setWorkspaceIdToAttribute(replacement, workspaceId);
      if (select) {
        globalThis.gBrowser.selectedTab = replacement;
      }
      return replacement;
    }

    const workspaceTab = this.createTabForWorkspace(workspaceId, select);
    if (!this.tabHasOriginUserContextId(workspaceTab, expectedUserContextId)) {
      console.error(
        "[WorkspacesTabManager] Failed to create a tab in the workspace container",
      );
      throw new Error("Workspace tab browsing context mismatch");
    }
    if (!replacement) {
      return workspaceTab;
    }

    const wasSuppressingTabClose = this.suppressTabCloseHandling;
    this.suppressTabCloseHandling = true;
    try {
      globalThis.gBrowser.removeTab(replacement);
    } finally {
      this.suppressTabCloseHandling = wasSuppressingTabClose;
    }
    return workspaceTab;
  }

  private tabHasOriginUserContextId(
    tab: XULElement,
    expectedUserContextId: number,
  ): boolean {
    try {
      const browser = globalThis.gBrowser.getBrowserForTab(tab);
      return hasOriginUserContextId(browser, expectedUserContextId);
    } catch (error) {
      console.error(
        "[WorkspacesTabManager] Failed to inspect tab origin attributes",
        error,
      );
      return false;
    }
  }

  /**
   * Remove tab by workspace id.
   * @param workspaceId The workspace id.
   * @param fallbackWorkspaceId Optional fallback workspace id used when
   * deleting the current/default workspace.
   */
  public removeTabByWorkspaceId(
    workspaceId: TWorkspaceID,
    fallbackWorkspaceId?: TWorkspaceID,
  ) {
    const tabs = globalThis.gBrowser.tabs;
    const tabsToRemove = [];

    for (const tab of tabs) {
      const tabWorkspaceId = this.getWorkspaceIdFromAttribute(tab);
      if (tabWorkspaceId === workspaceId) {
        tabsToRemove.push(tab);
      }
    }

    if (tabsToRemove.length === 0) return;

    // Suppress handleTabClose workspace-empty logic so that closing tabs
    // during workspace deletion does not create spurious replacement tabs
    // or switch workspaces unexpectedly (fixes #2247).
    this.suppressTabCloseHandling = true;
    try {
      const currentWorkspaceId = this.dataManagerCtx.getSelectedWorkspaceID();
      if (workspaceId === currentWorkspaceId) {
        const defaultId = this.dataManagerCtx.getDefaultWorkspaceID();
        const targetWorkspaceId = defaultId !== workspaceId
          ? defaultId
          : fallbackWorkspaceId && fallbackWorkspaceId !== workspaceId
          ? fallbackWorkspaceId
          : null;

        if (targetWorkspaceId) {
          const defaultTabs = document?.querySelectorAll(
            `[${WORKSPACE_TAB_ATTRIBUTION_ID}="${targetWorkspaceId}"]`,
          ) as NodeListOf<XULElement>;

          if (defaultTabs?.length > 0) {
            globalThis.gBrowser.selectedTab = defaultTabs[0];
          } else {
            this.createTabForWorkspace(targetWorkspaceId, true);
          }

          this.dataManagerCtx.setCurrentWorkspaceID(targetWorkspaceId);
          this.updateTabsVisibility();
        }
      }

      for (let i = tabsToRemove.length - 1; i >= 0; i--) {
        try {
          globalThis.gBrowser.removeTab(tabsToRemove[i]);
        } catch (e) {
          console.error("Error removing tab:", e);
        }
      }
    } finally {
      this.suppressTabCloseHandling = false;
    }
  }

  /**
   * Create tab for workspace.
   * @param workspaceId The workspace id.
   * @param url The url will be opened in the tab.
   * @param select will select tab if true.
   * @returns The created tab.
   */
  createTabForWorkspace(
    workspaceId: TWorkspaceID,
    select = false,
    url?: string,
  ) {
    const targetURL = url ??
      Services.prefs.getStringPref("browser.startup.homepage");

    // Look up workspace's container to create the tab in the correct context.
    // Without this, replacement tabs for container workspaces are created in
    // the default container, causing proxy/VPN tabs to stop working (#2193).
    const workspace = this.dataManagerCtx.getRawWorkspace(workspaceId);
    const userContextId = workspace?.userContextId ?? 0;

    const tab = globalThis.gBrowser.addTab(targetURL, {
      skipAnimation: true,
      inBackground: false,
      userContextId: userContextId > 0 ? userContextId : undefined,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    this.setWorkspaceIdToAttribute(tab, workspaceId);

    if (userContextId > 0) {
      tab.setAttribute("usercontextid", String(userContextId));
    }

    if (select) {
      globalThis.gBrowser.selectedTab = tab;
    }
    return tab;
  }

  /**
   * Change workspace. Selected workspace id will be stored in window object.
   * @param workspaceId The workspace id.
   */
  public changeWorkspace(workspaceId: TWorkspaceID) {
    if (
      configStore.closePopupAfterClick &&
      this.targetToolbarItem?.hasAttribute("open")
    ) {
      this.panelUITargetElement?.hidePopup();
    }

    // Persist the currently selected tab as the last-shown for the
    // previously selected workspace before switching. This ensures we can
    // restore focus when returning to that workspace instead of creating
    // a new tab each time.
    try {
      const prevWorkspaceId = this.dataManagerCtx.getSelectedWorkspaceID();
      const currentlySelectedTab = globalThis.gBrowser
        .selectedTab as XULElement | null;
      if (
        currentlySelectedTab &&
        this.getWorkspaceIdFromAttribute(currentlySelectedTab) ===
          prevWorkspaceId
      ) {
        currentlySelectedTab.setAttribute(
          WORKSPACE_LAST_SHOW_ID,
          prevWorkspaceId,
        );
      }
    } catch (e) {
      console.debug(
        "WorkspacesTabManager: failed to persist previous workspace last-show",
        e,
      );
    }

    try {
      // Priority 1: If the currently selected tab already belongs to the
      // target workspace, keep it.  This preserves SessionStore's correct
      // startup selection and avoids overriding it with the first tab in
      // DOM order (which is a pinned tab after restore, causing #2053).
      const currentTab = globalThis.gBrowser.selectedTab as XULElement | null;
      const currentTabInTargetWorkspace = currentTab &&
        this.getWorkspaceIdFromAttribute(currentTab) === workspaceId;

      if (currentTabInTargetWorkspace) {
        // Keep the existing selection — SessionStore (or a previous switch)
        // already chose the correct tab for this workspace.
      } else {
        // Priority 2: Use the last-shown tab for this workspace.
        const willChangeWorkspaceLastShowTab = document?.querySelector(
          `[${WORKSPACE_LAST_SHOW_ID}="${workspaceId}"]`,
        ) as XULElement;

        if (willChangeWorkspaceLastShowTab) {
          globalThis.gBrowser.selectedTab = willChangeWorkspaceLastShowTab;
        } else {
          // Priority 3: Fall back to the first tab in this workspace.
          const tabToSelect = this.workspaceHasTabs(workspaceId);
          if (tabToSelect) {
            globalThis.gBrowser.selectedTab = tabToSelect;
          } else {
            // Priority 4: Claim an unattributed tab or create a new one.
            const nonWorkspaceTab = this.isThereNoWorkspaceTabs();
            if (nonWorkspaceTab !== true) {
              globalThis.gBrowser.selectedTab = nonWorkspaceTab as XULElement;
              this.setWorkspaceIdToAttribute(
                nonWorkspaceTab as XULElement,
                workspaceId,
              );
            } else {
              this.createTabForWorkspace(workspaceId, true);
            }
          }
        }
      }
      this.dataManagerCtx.setCurrentWorkspaceID(workspaceId);
      this.updateTabsVisibility();
    } catch (e) {
      console.error("Failed to change workspace:", e);

      try {
        const defaultId = this.dataManagerCtx.getDefaultWorkspaceID();

        if (defaultId !== workspaceId) {
          this.createTabForWorkspace(defaultId, true);
          this.dataManagerCtx.setCurrentWorkspaceID(defaultId);
          this.updateTabsVisibility();
        } else {
          this.createTabForWorkspace(workspaceId, true);
          this.dataManagerCtx.setCurrentWorkspaceID(workspaceId);
          this.updateTabsVisibility();
        }
      } catch (innerError) {
        console.error("Critical error handling workspace change:", innerError);

        try {
          console.debug("gBrowser.addTab called in changeWorkspace");
          const newTab = globalThis.gBrowser.addTab("about:newtab", {
            skipAnimation: true,
            triggeringPrincipal: Services.scriptSecurityManager
              .getSystemPrincipal(),
          });
          globalThis.gBrowser.selectedTab = newTab;
        } catch (finalError) {
          console.error("Fatal error creating new tab:", finalError);
        }
      }
    }
  }

  /**
   * Switch to another workspace tab.
   * @param workspaceId The workspace id.
   * @returns void
   */
  switchToAnotherWorkspaceTab(workspaceId: TWorkspaceID) {
    const workspaceTabs = Array.from(
      document?.querySelectorAll(
        `[${WORKSPACE_TAB_ATTRIBUTION_ID}="${workspaceId}"]`,
      ) ?? [],
    ) as unknown as XULElement[];

    if (!workspaceTabs?.length) {
      try {
        const tab = this.createTabForWorkspace(workspaceId);
        this.moveTabToWorkspace(workspaceId, tab);
        globalThis.gBrowser.selectedTab = tab;
      } catch (e) {
        console.error("Failed to create tab for workspace:", e);
        const defaultWorkspaceId = this.dataManagerCtx.getDefaultWorkspaceID();
        if (defaultWorkspaceId !== workspaceId) {
          this.changeWorkspace(defaultWorkspaceId);
        }
      }
    } else {
      globalThis.gBrowser.selectedTab = workspaceTabs[0];
    }
  }

  /**
   * Check if workspace has tabs.
   * @param workspaceId The workspace id.
   * @returns true if workspace has tabs.
   */
  public workspaceHasTabs(workspaceId: string) {
    const workspaceTabs = Array.from(
      document?.querySelectorAll(
        `[${WORKSPACE_TAB_ATTRIBUTION_ID}="${workspaceId}"]`,
      ) ?? [],
    ) as unknown as XULElement[];
    return workspaceTabs?.length > 0 ? workspaceTabs[0] : false;
  }

  /**
   * Check if there is no workspace tabs.
   * @returns true if there is no workspace tabs if false, return tab.
   */
  public isThereNoWorkspaceTabs() {
    for (
      const tab of globalThis.gBrowser.tabs as Array<
        XULElement | undefined | null
      >
    ) {
      if (!tab) continue;
      if (!tab.hasAttribute(WORKSPACE_TAB_ATTRIBUTION_ID)) {
        return tab;
      }
    }
    return true;
  }

  /**
   * Move tabs to workspace.
   * @param workspaceId The workspace id.
   */
  moveTabToWorkspace(workspaceId: TWorkspaceID, tab: XULElement) {
    const oldWorkspaceId = this.getWorkspaceIdFromAttribute(tab);
    this.setWorkspaceIdToAttribute(tab, workspaceId);

    if (tab === globalThis.gBrowser.selectedTab && oldWorkspaceId) {
      const oldWorkspaceTabs = Array.from(
        document?.querySelectorAll(
          `[${WORKSPACE_TAB_ATTRIBUTION_ID}="${oldWorkspaceId}"]`,
        ) ?? [],
      ) as unknown as XULElement[];

      if (oldWorkspaceTabs && oldWorkspaceTabs.length > 0) {
        this.switchToAnotherWorkspaceTab(oldWorkspaceId);
      } else {
        const defaultWorkspaceId = this.dataManagerCtx.getDefaultWorkspaceID();
        this.changeWorkspace(defaultWorkspaceId);
      }
    }
  }

  /**
   * Move tabs to workspace from tab context menu.
   * @param workspaceId The workspace id.
   */
  public moveTabsToWorkspaceFromTabContextMenu(workspaceId: TWorkspaceID) {
    const reopenedTabs = globalThis.TabContextMenu.contextTab.multiselected
      ? globalThis.gBrowser.selectedTabs
      : [globalThis.TabContextMenu.contextTab];

    for (const tab of reopenedTabs) {
      this.moveTabToWorkspace(workspaceId, tab);
      if (tab === globalThis.gBrowser.selectedTab) {
        this.switchToAnotherWorkspaceTab(workspaceId);
      }
    }
    this.updateTabsVisibility();
  }

  /**
   * Returns target toolbar item.
   * @returns The target toolbar item.
   */
  private get targetToolbarItem(): XULElement | undefined | null {
    return document?.querySelector("#workspaces-toolbar-button") as
      | XULElement
      | null
      | undefined;
  }

  /**
   * Returns panel UI target element.
   * @returns The panel UI target element.
   */
  private get panelUITargetElement():
    | PanelMultiViewParentElement
    | undefined
    | null {
    return document?.querySelector("#customizationui-widget-panel") as
      | PanelMultiViewParentElement
      | null
      | undefined;
  }

  private getMaybeSelectedWorkspacebyVisibleTabs(): TWorkspaceID | null {
    const tabs = (globalThis.gBrowser.visibleTabs as XULElement[]).slice(0, 10);
    const workspaceIdCounts = new Map<TWorkspaceID, number>();

    for (const tab of tabs) {
      const workspaceId = this.getWorkspaceIdFromAttribute(tab);
      if (workspaceId) {
        workspaceIdCounts.set(
          workspaceId,
          (workspaceIdCounts.get(workspaceId) || 0) + 1,
        );
      }
    }

    let mostFrequentId: TWorkspaceID | null = null;
    let maxCount = 0;

    workspaceIdCounts.forEach((count, id) => {
      if (count > maxCount) {
        maxCount = count;
        mostFrequentId = id;
      }
    });

    return mostFrequentId;
  }
}
