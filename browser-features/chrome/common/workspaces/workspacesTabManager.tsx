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

interface TabEvent extends Event {
  target: XULElement;
  forUnsplit?: boolean;
}

export class WorkspacesTabManager {
  dataManagerCtx: WorkspacesDataManager;
  iconCtx: WorkspaceIcons;
  // Track recently opened tabs and per-workspace open timestamps to detect
  // "auto-created replacement tab" right after last-tab close.
  private recentOpenedAtByTab = new WeakMap<XULElement, number>();
  private recentOpenedAtPerWorkspace = new Map<TWorkspaceID, number>();
  constructor(iconCtx: WorkspaceIcons, dataManagerCtx: WorkspacesDataManager) {
    this.iconCtx = iconCtx;
    this.dataManagerCtx = dataManagerCtx;
    this.boundHandleTabClose = this.handleTabClose.bind(this);
    this.boundHandleTabOpen = this.handleTabOpen.bind(this);

    const initWorkspace = () => {
      (globalThis as unknown as {
        SessionStore: { promiseAllWindowsRestored: Promise<void> };
      }).SessionStore
        .promiseAllWindowsRestored
        .then(() => {
          this.initializeWorkspace();
          globalThis.addEventListener("TabClose", this.boundHandleTabClose);
          globalThis.addEventListener("TabOpen", this.boundHandleTabOpen);
        })
        .catch((error: Error) => {
          console.error("Error waiting for windows restore:", error);
          this.initializeWorkspace();
          globalThis.addEventListener("TabClose", this.boundHandleTabClose);
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
          // Keep first, remove the rest
          for (let i = 1; i < startupNewTabs.length; i++) {
            try {
              globalThis.gBrowser.removeTab(startupNewTabs[i]);
            } catch {
              // ignore
            }
          }
          console.debug(
            "WorkspacesTabManager: collapsed duplicated startup new tabs",
            { removedCount: startupNewTabs.length - 1 },
          );
        }
        Services.prefs.setBoolPref(WORKSPACE_PENDING_EXIT_PREF_NAME, false);
      }
    } catch (e) {
      console.debug(
        "WorkspacesTabManager: failed collapsing startup duplicates",
        e,
      );
    }

    const maybeSelectedWorkspace = this
      .getMaybeSelectedWorkspacebyVisibleTabs();
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
    globalThis.removeEventListener("TabClose", this.boundHandleTabClose);
    globalThis.removeEventListener("TabOpen", this.boundHandleTabOpen);
  }

  private handleTabClose = (event: TabEvent) => {
    const tab = event.target as XULElement;
    const workspaceId = this.getWorkspaceIdFromAttribute(tab);

    if (!workspaceId) return;

    const currentWorkspaceId = this.dataManagerCtx.getSelectedWorkspaceID();
    const isCurrentWorkspace = workspaceId === currentWorkspaceId;
    const workspaceTabs = Array.from(
      document?.querySelectorAll(
        `[${WORKSPACE_TAB_ATTRIBUTION_ID}="${workspaceId}"]`,
      ) as NodeListOf<XULElement>,
    ).filter((t) => t !== tab);

    // Debug: record branch conditions when a tab is closed
    try {
      console.debug("WorkspacesTabManager: TabClose conditions", {
        workspaceId,
        isCurrentWorkspace,
        workspaceTabsLength: workspaceTabs.length,
        exitOnLastTabClose: configStore.exitOnLastTabClose,
      });
    } catch (_logErr) {
      // ignore log error
    }

    if (isCurrentWorkspace) {
      try {
        // Determine emptiness considering Firefox's auto-created replacement tab.
        const now = Date.now();
        let willBeEmpty = workspaceTabs.length === 0;
        if (!willBeEmpty && workspaceTabs.length === 1) {
          const remaining = workspaceTabs[0] as XULElement;
          const openedAt = this.recentOpenedAtByTab.get(remaining) ?? 0;
          if (now - openedAt < 800) {
            willBeEmpty = true; // treat as empty: remaining tab was just auto-created
          }
        }

        if (willBeEmpty) {
          // If user prefers exiting when current workspace becomes empty, quit the app.
          if (configStore.exitOnLastTabClose) {
            try {
              // Mark pending-exit to collapse duplicated startup new tabs next run.
              Services.prefs.setBoolPref(
                WORKSPACE_PENDING_EXIT_PREF_NAME,
                true,
              );
              console.debug(
                "WorkspacesTabManager: exitOnLastTabClose is true; quitting",
                { workspaceId },
              );
              Services.startup.quit(Services.startup.eAttemptQuit as number);
              return;
            } catch (quitErr) {
              console.error(
                "WorkspacesTabManager: failed to quit on workspace empty",
                quitErr,
              );
              // fall through to create a replacement tab
            }
          }
          // Otherwise create exactly one replacement tab in the emptied workspace.
          const replacement = this.createTabForWorkspace(workspaceId, true);
          replacement.setAttribute(WORKSPACE_LAST_SHOW_ID, workspaceId);
          console.debug(
            "WorkspacesTabManager: created replacement tab in emptied workspace",
            { workspaceId },
          );
          this.dataManagerCtx.setCurrentWorkspaceID(workspaceId);
          this.updateTabsVisibility();
        }
      } catch (e) {
        console.error(
          "Error handling last tab close in workspace (stay mode):",
          e,
        );
        try {
          console.debug("gBrowser.addTab called in handleTabClose");
          const newTab = globalThis.gBrowser.addTab("about:newtab", {
            skipAnimation: true,
            triggeringPrincipal: Services.scriptSecurityManager
              .getSystemPrincipal(),
          });
          globalThis.gBrowser.selectedTab = newTab;
          console.debug(
            "WorkspacesTabManager: created about:newtab fallback after error (stay mode)",
          );
        } catch (innerError) {
          console.error(
            "Critical error handling tab close (stay mode):",
            innerError,
          );
        }
      }
    }
  };

  private handleTabOpen = (event: Event) => {
    try {
      const tab = (event as CustomEvent).target as XULElement;
      const now = Date.now();
      this.recentOpenedAtByTab.set(tab, now);
      const wsId = this.getWorkspaceIdFromAttribute(tab) ??
        this.dataManagerCtx.getSelectedWorkspaceID();
      this.recentOpenedAtPerWorkspace.set(wsId, now);
      // Debug: mark tab creation
      console.debug("WorkspacesTabManager: TabOpen recorded", {
        workspaceId: wsId,
        createdAt: now,
      });
    } catch (_openErr) {
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
      const hasVisibleTabInGroup = (
        group.tabs as Array<XULElement>
      ).some(
        (tab) => this.getWorkspaceIdFromAttribute(tab) === currentWorkspaceId,
      );
      group.style.display = hasVisibleTabInGroup ? "" : "none";
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
      console.debug(
        "WorkspacesTabManager: no workspace attribute on tab, returning null",
      );
      return null;
    }
    const clean = raw.replace(/[{}]/g, "");
    const parseResult = zWorkspaceID.decode(clean);
    if (!isRight(parseResult)) {
      console.warn(
        "WorkspacesTabManager: invalid workspace id format:",
        raw,
      );
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
   * Remove tab by workspace id.
   * @param workspaceId The workspace id.
   */
  public removeTabByWorkspaceId(workspaceId: TWorkspaceID) {
    const tabs = globalThis.gBrowser.tabs;
    const tabsToRemove = [];

    for (const tab of tabs) {
      if (this.getWorkspaceIdFromAttribute(tab) === workspaceId) {
        tabsToRemove.push(tab);
      }
    }

    if (tabsToRemove.length === 0) return;

    const currentWorkspaceId = this.dataManagerCtx.getSelectedWorkspaceID();
    if (workspaceId === currentWorkspaceId) {
      const defaultId = this.dataManagerCtx.getDefaultWorkspaceID();

      if (defaultId !== workspaceId) {
        const defaultTabs = document?.querySelectorAll(
          `[${WORKSPACE_TAB_ATTRIBUTION_ID}="${defaultId}"]`,
        ) as NodeListOf<XULElement>;

        if (defaultTabs?.length > 0) {
          globalThis.gBrowser.selectedTab = defaultTabs[0];
        } else {
          this.createTabForWorkspace(defaultId, true);
        }

        this.dataManagerCtx.setCurrentWorkspaceID(defaultId);
        this.updateTabsVisibility();
      } else {
        this.createTabForWorkspace(defaultId, true);
      }
    }

    for (let i = tabsToRemove.length - 1; i >= 0; i--) {
      try {
        globalThis.gBrowser.removeTab(tabsToRemove[i]);
      } catch (e) {
        console.error("Error removing tab:", e);
      }
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
    console.debug("createTabForWorkspace called with:", {
      workspaceId,
      select,
      url,
    });
    const targetURL = url ??
      Services.prefs.getStringPref("browser.startup.homepage");
    console.debug(
      "gBrowser.addTab called in createTabForWorkspace with url:",
      targetURL,
    );
    const tab = globalThis.gBrowser.addTab(targetURL, {
      skipAnimation: true,
      inBackground: false,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    this.setWorkspaceIdToAttribute(tab, workspaceId);

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
      const currentlySelectedTab = globalThis.gBrowser.selectedTab as
        | XULElement
        | null;
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
      const willChangeWorkspaceLastShowTab = document?.querySelector(
        `[${WORKSPACE_LAST_SHOW_ID}="${workspaceId}"]`,
      ) as XULElement;

      if (willChangeWorkspaceLastShowTab) {
        globalThis.gBrowser.selectedTab = willChangeWorkspaceLastShowTab;
      } else {
        const tabToSelect = this.workspaceHasTabs(workspaceId);
        if (tabToSelect) {
          globalThis.gBrowser.selectedTab = tabToSelect;
        } else {
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
    const workspaceTabs = document?.querySelectorAll(
      `[${WORKSPACE_TAB_ATTRIBUTION_ID}="${workspaceId}"]`,
    ) as XULElement[];

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
    const workspaceTabs = document?.querySelectorAll(
      `[${WORKSPACE_TAB_ATTRIBUTION_ID}="${workspaceId}"]`,
    ) as XULElement[];
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
      const oldWorkspaceTabs = document?.querySelectorAll(
        `[${WORKSPACE_TAB_ATTRIBUTION_ID}="${oldWorkspaceId}"]`,
      ) as XULElement[];

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
    return document?.querySelector("#workspaces-toolbar-button");
  }

  /**
   * Returns panel UI target element.
   * @returns The panel UI target element.
   */
  private get panelUITargetElement():
    | PanelMultiViewParentElement
    | undefined
    | null {
    return document?.querySelector("#customizationui-widget-panel");
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
