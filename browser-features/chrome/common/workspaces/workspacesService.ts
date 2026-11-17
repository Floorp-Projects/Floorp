/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
import { onCleanup } from "solid-js";
import {
  selectedWorkspaceID,
  setWorkspacesDataStore,
  workspacesDataStore,
} from "./data/data";
import type {
  TWorkspace,
  TWorkspaceID,
  TWorkspacesStoreData,
} from "./utils/type";
import type {
  WorkspacesDataManager,
  WorkspacesDataManagerBase,
} from "./workspacesDataManagerBase";
import type { WorkspacesTabManager } from "./workspacesTabManager";
import type { WorkspaceIcons } from "./utils/workspace-icons";
import {
  applyWorkspaceModalResult,
  WorkspaceManageModal,
} from "./workspace-modal";
import i18next from "i18next";
import {
  createWorkspaceSnapshot,
  restoreTabsIntoWindow,
  type RestoreTargetBrowser,
} from "./utils/workspace-snapshot";
import {
  applyWorkspaceSnapshotMetadata,
  WorkspacesArchiveService,
} from "./utils/workspaces-archive-service";
import { WorkspacesBackupService } from "./utils/workspaces-backup-service";
import type {
  WorkspaceBackupFile,
  WorkspaceBackupSummary,
  WorkspaceBackupWorkspace,
} from "./utils/backup-types";
import type { TWorkspaceSnapshotTab } from "./utils/type";
import type { WorkspaceArchiveSummary } from "./utils/archive-types";
import {
  WORKSPACE_ENABLED_PREF_NAME,
  WORKSPACE_LAST_SHOW_ID,
  WORKSPACE_TAB_ATTRIBUTION_ID,
  WORKSPACES_CHANGED_OBSERVER_TOPIC,
} from "./utils/workspaces-static-names";
import { ExplicitTabUserContextOperations } from "./utils/explicit-tab-user-context.ts";
import { PrivateContainer } from "../private-container/PrivateContainer.ts";
import type { WorkspacesSyncController } from "./workspacesSync.ts";

export class WorkspacesService implements WorkspacesDataManagerBase {
  dataManagerCtx: WorkspacesDataManager;
  tabManagerCtx: WorkspacesTabManager;
  iconCtx: WorkspaceIcons;
  modalCtx: WorkspaceManageModal;
  archiveService: WorkspacesArchiveService;
  backupService: WorkspacesBackupService;
  syncController?: WorkspacesSyncController;

  private explicitTabUserContextOperations =
    new ExplicitTabUserContextOperations();

  /**
   * Opens one tab without replacing an explicitly selected container with the
   * current workspace's default container. gBrowser.addTab dispatches TabOpen
   * synchronously, so this operation only exists for the duration of openTab.
   */
  withExplicitTabUserContext<T>(
    userContextId: number,
    openTab: () => T,
  ): T {
    return this.explicitTabUserContextOperations.run(userContextId, openTab);
  }

  private cloneWorkspaceMap(source: unknown): Map<TWorkspaceID, TWorkspace> {
    if (source instanceof Map) {
      return new Map(source as Map<TWorkspaceID, TWorkspace>);
    }
    if (source && typeof source === "object") {
      const entries = Object.entries(source as Record<string, TWorkspace>);
      return new Map(
        entries.map(([key, value]) => [key as TWorkspaceID, value]),
      );
    }
    return new Map<TWorkspaceID, TWorkspace>();
  }

  private getWorkspaceCount(): number {
    const data = workspacesDataStore.data;
    if (data instanceof Map) {
      return (data as Map<TWorkspaceID, TWorkspace>).size;
    }
    return 0;
  }

  private findFallbackWorkspaceID(
    excludeId: TWorkspaceID,
  ): TWorkspaceID | null {
    const fromOrder = workspacesDataStore.order.find((id) => id !== excludeId);
    if (fromOrder && this.isWorkspaceID(fromOrder)) {
      return fromOrder;
    }

    for (
      const id of (
        workspacesDataStore.data as unknown as Map<TWorkspaceID, TWorkspace>
      ).keys()
    ) {
      if (id !== excludeId) {
        return id;
      }
    }

    return null;
  }

  constructor(
    tabManagerCtx: WorkspacesTabManager,
    iconCtx: WorkspaceIcons,
    dataManagerCtx: WorkspacesDataManager,
  ) {
    this.tabManagerCtx = tabManagerCtx;
    this.iconCtx = iconCtx;
    this.dataManagerCtx = dataManagerCtx;
    this.modalCtx = new WorkspaceManageModal(this, this.iconCtx);
    this.archiveService = new WorkspacesArchiveService();
    this.backupService = new WorkspacesBackupService(this);
    // deno-lint-ignore no-explicit-any
    (globalThis as any).workspacesFuncs = {
      createNoNameWorkspace: this.createNoNameWorkspace.bind(this),
      changeWorkspaceToNext: this.changeWorkspaceToNext.bind(this),
      changeWorkspaceToPrevious: this.changeWorkspaceToPrevious.bind(this),
      captureWorkspaceSnapshot: this.captureWorkspaceSnapshot.bind(this),
      archiveWorkspace: this.archiveWorkspace.bind(this),
      listArchivedWorkspaces: this.listArchivedWorkspaces.bind(this),
      restoreArchivedWorkspace: this.restoreArchivedWorkspace.bind(this),
      deleteArchivedWorkspace: this.deleteArchivedWorkspace.bind(this),
      backupNow: this.backupNow.bind(this),
      listBackups: this.listBackups.bind(this),
      restoreBackup: this.restoreBackup.bind(this),
      deleteBackup: this.deleteBackup.bind(this),
      deleteAllBackups: this.deleteAllBackups.bind(this),
      resetWorkspaces: this.resetWorkspaces.bind(this),
      getSelectedWorkspaceID: this.getSelectedWorkspaceID.bind(this),
      getCurrentWorkspaceUserContextId: this.getCurrentWorkspaceUserContextId
        .bind(this),
      canOpenWorkspaceTab: this.canOpenWorkspaceTab.bind(this),
      changeWorkspace: this.changeWorkspace.bind(this),
      isWorkspaceID: this.isWorkspaceID.bind(this),
    };

    if (workspacesDataStore.data.size === 0) {
      const id = this.createNoNameWorkspace();
      this.setDefaultWorkspace(id);
      this.setCurrentWorkspaceID(id);
    }

    // Register persistTabAttribute early (after promiseInitialized) where the
    // runtime still offers it, so SessionStore saves and restores our tab
    // attributes itself.
    //
    // Firefox 152 removed it: SessionStore.sys.mjs has no such method, and
    // TabAttributes.sys.mjs now hard-codes PERSISTED_ATTRIBUTES to
    // ["customizemode"] with no way to register another name. Calling it
    // unguarded rejects at window init.
    //
    // Losing it costs us nothing, because the workspace attributes do not
    // depend on it: the runtime patches persist them directly —
    // tools/patches/browser-modules-sessionstore-TabState.sys.patch writes
    // tabData.floorpWorkspaceId / floorpLastShowWorkspaceId when tab state is
    // collected, and browser-chrome-...-tabbrowser.patch sets both attributes
    // back on the restored tab. This call stays as belt-and-braces for
    // runtimes that still have the API.
    globalThis.SessionStore.promiseInitialized
      .then(() => {
        if (typeof globalThis.SessionStore.persistTabAttribute === "function") {
          globalThis.SessionStore.persistTabAttribute(
            WORKSPACE_TAB_ATTRIBUTION_ID,
          );
          globalThis.SessionStore.persistTabAttribute(WORKSPACE_LAST_SHOW_ID);
        }
      })
      .catch((e: unknown) => {
        console.error(
          "[workspaces] failed to register persisted tab attributes",
          e,
        );
      });

    // Delay TabOpen handler and ProgressListener registration until AFTER
    // session restore completes. During restore (between promiseInitialized
    // and promiseAllWindowsRestored), SessionStore recreates tabs whose
    // floorpWorkspaceId attribute may not yet be set when TabOpen fires.
    // If we handle TabOpen during that window, we incorrectly assign restored
    // tabs to the default/current workspace, corrupting the workspace→tab
    // mapping. See https://github.com/Floorp-Projects/Floorp/issues/2343
    this.boundHandleTabOpen = this.handleTabOpen.bind(this);
    (
      globalThis as unknown as {
        SessionStore: {
          promiseInitialized: Promise<void>;
          promiseAllWindowsRestored: Promise<void>;
          persistTabAttribute: (attr: string) => void;
        };
      }
    ).SessionStore.promiseAllWindowsRestored.then(() => {
      globalThis.gBrowser.addTabsProgressListener(this.listener);
      globalThis.gBrowser.tabContainer.addEventListener(
        "TabOpen",
        this.boundHandleTabOpen,
      );
    });

    onCleanup(() => {
      globalThis.gBrowser.removeTabsProgressListener(this.listener);
      globalThis.gBrowser.tabContainer.removeEventListener(
        "TabOpen",
        this.boundHandleTabOpen,
      );
    });
  }
  setCurrentWorkspaceID(id: TWorkspaceID): void {
    this.dataManagerCtx.setCurrentWorkspaceID(id);
    globalThis.dispatchEvent(new Event("floorp-workspace-container-ready"));
  }
  setDefaultWorkspace(id: TWorkspaceID): void {
    this.dataManagerCtx.setDefaultWorkspace(id);
  }
  isWorkspaceID(id: string): id is TWorkspaceID {
    return this.dataManagerCtx.isWorkspaceID(id);
  }
  getRawWorkspace(id: TWorkspaceID): TWorkspace | undefined {
    return this.dataManagerCtx.getRawWorkspace(id);
  }
  getSelectedWorkspaceID(): TWorkspaceID {
    return this.dataManagerCtx.getSelectedWorkspaceID();
  }
  getDefaultWorkspaceID(): TWorkspaceID {
    return this.dataManagerCtx.getDefaultWorkspaceID();
  }
  // for override
  getCurrentWorkspaceUserContextId(): number {
    const id = this.getSelectedWorkspaceID();
    return this.getRawWorkspace(id)?.userContextId ?? 0;
  }

  /** Guard only implicit default selection; explicit none/other containers remain valid. */
  canOpenWorkspaceTab(workspaceId = this.getSelectedWorkspaceID()): boolean {
    if (!Services.prefs.getBoolPref(WORKSPACE_ENABLED_PREF_NAME, true)) {
      return true;
    }
    const controller = this.syncController;
    return Boolean(
      controller && !controller.defaultContainerPending(workspaceId),
    );
  }

  deleteWorkspace(workspaceID: TWorkspaceID): void {
    if (workspacesDataStore.data.size === 1) return;

    this.syncController?.deleteWorkspace(workspaceID).catch(
      (error: unknown) => {
        console.error(
          "[WorkspacesSync] Could not sync workspace deletion",
          error,
        );
      },
    );

    const fallbackWorkspaceID = this.findFallbackWorkspaceID(workspaceID);
    const defaultWorkspaceID = this.getDefaultWorkspaceID();
    if (
      defaultWorkspaceID === workspaceID &&
      fallbackWorkspaceID &&
      fallbackWorkspaceID !== workspaceID
    ) {
      this.setDefaultWorkspace(fallbackWorkspaceID);
    }

    this.tabManagerCtx.removeTabByWorkspaceId(
      workspaceID,
      fallbackWorkspaceID ?? undefined,
    );
    setWorkspacesDataStore(
      "order",
      (prev) => prev.filter((v) => v !== workspaceID),
    );
    this.dataManagerCtx.deleteWorkspace(workspaceID);
  }

  /**
   * Returns new workspace object with default name.
   * @returns The new workspace id.
   */
  public createNoNameWorkspace(): TWorkspaceID {
    const count = this.getWorkspaceCount();
    const workspaceName = i18next.t("workspaces.service.new-workspace", {
      count,
    } as Record<string, unknown>) as string;

    const id = this.dataManagerCtx.createWorkspace(workspaceName);
    setWorkspacesDataStore("order", (prev) => [...prev, id]);
    this.changeWorkspace(id);
    return id;
  }

  /**
   * @deprecated Use WorkspacesServices.createWorkspace instead.
   */
  createWorkspace(name: string): TWorkspaceID {
    //This function is deprecated because WorkspacesService is wrapping this function
    const id = this.dataManagerCtx.createWorkspace(name);
    setWorkspacesDataStore("order", (prev) => [...prev, id]);
    this.changeWorkspace(id);
    return id;
  }

  changeWorkspaceToNext(): void {
    const order = workspacesDataStore.order;
    const currentID = this.getSelectedWorkspaceID();
    const currentIndex = order.indexOf(currentID);
    const nextIndex = (currentIndex + 1) % order.length;
    const nextID = order[nextIndex];
    this.changeWorkspace(nextID);
  }

  changeWorkspaceToPrevious(): void {
    const order = workspacesDataStore.order;
    const currentID = this.getSelectedWorkspaceID();
    const currentIndex = order.indexOf(currentID);
    const previousIndex = (currentIndex - 1 + order.length) % order.length;
    const previousID = order[previousIndex];
    this.changeWorkspace(previousID);
  }

  reorderWorkspaceUp(id: TWorkspaceID): void {
    setWorkspacesDataStore("order", (prev) => {
      const index = prev.indexOf(id);
      if (index > 0) {
        [prev[index - 1], prev[index]] = [prev[index], prev[index - 1]];
      }
      return [...prev];
    });
  }

  reorderWorkspaceDown(id: TWorkspaceID): void {
    setWorkspacesDataStore("order", (prev) => {
      const index = prev.indexOf(id);
      if (index < prev.length - 1) {
        [prev[index], prev[index + 1]] = [prev[index + 1], prev[index]];
      }
      return [...prev];
    });
  }

  /**
   * Move workspace to a specific index position.
   * This method allows moving workspaces freely via drag & drop,
   * including the default workspace.
   * @param id The workspace ID to move.
   * @param targetIndex The target index position (0-based).
   */
  reorderWorkspaceTo(id: TWorkspaceID, targetIndex: number): void {
    setWorkspacesDataStore("order", (prev) => {
      const currentIndex = prev.indexOf(id);
      if (
        currentIndex === -1 ||
        targetIndex < 0 ||
        targetIndex >= prev.length
      ) {
        return prev;
      }
      if (currentIndex === targetIndex) {
        return prev;
      }
      const newOrder = [...prev];
      newOrder.splice(currentIndex, 1);
      newOrder.splice(targetIndex, 0, id);
      return newOrder;
    });
  }

  /**
   * Open manage workspace dialog. This function should not be called directly on Preferences page.
   * @param workspaceId If workspaceId is provided, the dialog will select the workspace for editing.
   */
  public async manageWorkspaceFromDialog(id?: TWorkspaceID) {
    const targetWorkspaceID = id ?? this.getSelectedWorkspaceID();
    if (!this.getRawWorkspace(targetWorkspaceID)) return;
    const result = await this.modalCtx.showWorkspacesModal(targetWorkspaceID);
    if (result === null) return;
    let didApply = false;
    setWorkspacesDataStore("data", (prev) => {
      const temp = this.cloneWorkspaceMap(prev);
      const latestWorkspace = temp.get(targetWorkspaceID);
      if (!latestWorkspace) return prev;
      const newWorkspace = applyWorkspaceModalResult(latestWorkspace, result);
      if (!newWorkspace) return prev;
      temp.set(targetWorkspaceID, newWorkspace);
      didApply = true;
      return temp as unknown as TWorkspacesStoreData["data"];
    });
    if (!didApply) return;
    this.tabManagerCtx.updateTabsVisibility();
    return result;
  }

  public changeWorkspace(id: TWorkspaceID) {
    this.tabManagerCtx.changeWorkspace(id);
    globalThis.dispatchEvent(new Event("floorp-workspace-container-ready"));
    Services.obs.notifyObservers(
      null as unknown as nsISupports,
      WORKSPACES_CHANGED_OBSERVER_TOPIC,
      id,
    );
  }

  public async captureWorkspaceSnapshot(id?: TWorkspaceID) {
    const targetWorkspaceId = id ?? this.getSelectedWorkspaceID();
    return await createWorkspaceSnapshot(
      targetWorkspaceId,
      this.dataManagerCtx,
    );
  }

  public async archiveWorkspace(id?: TWorkspaceID) {
    const targetWorkspaceId = id ?? this.getSelectedWorkspaceID();
    if (!this.isWorkspaceID(targetWorkspaceId)) {
      return null;
    }

    if (this.getWorkspaceCount() <= 1) {
      console.warn(
        "WorkspacesService: cannot archive the last remaining workspace",
      );
      return null;
    }

    const snapshot = await this.captureWorkspaceSnapshot(targetWorkspaceId);
    if (!snapshot) {
      return null;
    }

    const archiveId = await this.archiveService.saveSnapshot(snapshot);
    this.deleteWorkspace(targetWorkspaceId);
    return archiveId;
  }

  public async listArchivedWorkspaces(): Promise<WorkspaceArchiveSummary[]> {
    return await this.archiveService.listSnapshots();
  }

  public async deleteArchivedWorkspace(archiveId: string): Promise<boolean> {
    try {
      await this.archiveService.removeSnapshot(archiveId);
      return true;
    } catch (error) {
      console.error(
        "WorkspacesService: failed to delete archived workspace",
        error,
      );
      return false;
    }
  }

  public async restoreArchivedWorkspace(archiveId: string) {
    const snapshot = await this.archiveService.loadSnapshot(archiveId);
    if (!snapshot) {
      console.warn(
        "WorkspacesService: no snapshot found for archive",
        archiveId,
      );
      return null;
    }

    const restoredWorkspaceId = this.dataManagerCtx.createWorkspace(
      snapshot.workspace.name,
    );
    setWorkspacesDataStore("order", (prev) => [...prev, restoredWorkspaceId]);
    setWorkspacesDataStore("data", (prev) => {
      const temp = this.cloneWorkspaceMap(prev);
      const workspace = temp.get(restoredWorkspaceId);
      if (workspace) {
        temp.set(
          restoredWorkspaceId,
          applyWorkspaceSnapshotMetadata(workspace, snapshot.workspace),
        );
      }
      return temp as unknown as TWorkspacesStoreData["data"];
    });

    let outcomes: Awaited<ReturnType<typeof restoreTabsIntoWindow>>;
    try {
      outcomes = await this.restoreTabsFromSnapshot(
        restoredWorkspaceId,
        snapshot.tabs,
      );
    } catch (error) {
      // SessionStore was not ready: drop the workspace we just created so the
      // store does not keep an empty entry, and keep the archive snapshot.
      this.rollbackCreatedWorkspace(restoredWorkspaceId);
      throw error;
    }

    const failedCount = outcomes.filter((outcome) => !outcome.ok).length;
    if (failedCount > 0) {
      // Keep the archive snapshot: it is the only remaining copy of the tabs
      // that failed to restore.
      this.rollbackCreatedWorkspace(restoredWorkspaceId, outcomes);
      throw new Error(`archive-restore-partial-failure:${failedCount}`);
    }

    await this.archiveService.removeSnapshot(archiveId);

    this.changeWorkspace(restoredWorkspaceId);
    return restoredWorkspaceId;
  }

  /**
   * Re-points tabs in other normal windows at a workspace that still exists.
   *
   * The store is global but only the restoring window's tabs are re-attributed
   * by resetWorkspaces(); tabs in other windows would otherwise keep IDs that
   * no longer exist until that window happens to refresh.
   */
  private reassignOtherWindowsTabs(fallbackWorkspaceId: TWorkspaceID): void {
    const enumerator = Services.wm.getEnumerator("navigator:browser");
    while (enumerator.hasMoreElements()) {
      const window = enumerator.getNext() as Window & {
        closed?: boolean;
        gBrowser?: { tabs: XULElement[] };
      };
      if (
        !window || window.closed ||
        (window as unknown) === (globalThis as unknown)
      ) {
        continue;
      }
      try {
        const module = ChromeUtils.importESModule(
          "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
        ) as unknown as {
          PrivateBrowsingUtils: { isWindowPrivate(w: Window): boolean };
        };
        if (module.PrivateBrowsingUtils.isWindowPrivate(window)) {
          continue;
        }
      } catch (error) {
        // Unknown privacy: leave the window alone rather than touch it.
        console.error(
          "WorkspacesService: skipped a window during tab re-attribution",
          error,
        );
        continue;
      }
      const tabs = window.gBrowser?.tabs ?? [];
      for (const tab of tabs) {
        const workspaceId = tab.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID);
        if (workspaceId && !this.dataManagerCtx.isWorkspaceID(workspaceId)) {
          tab.setAttribute(WORKSPACE_TAB_ATTRIBUTION_ID, fallbackWorkspaceId);
        }
      }
    }
  }

  /**
   * Removes a workspace created by a failed restore attempt, together with the
   * tabs that attempt already opened. Leaving those tabs behind would orphan
   * them: their workspace is about to disappear from the store.
   */
  private rollbackCreatedWorkspace(
    workspaceId: TWorkspaceID,
    outcomes: Awaited<ReturnType<typeof restoreTabsIntoWindow>> = [],
  ): void {
    this.tabManagerCtx.runWithTabCloseSuppressed(() => {
      for (const outcome of outcomes) {
        const tab = outcome.tab;
        if (!tab || !tab.isConnected) {
          continue;
        }
        try {
          globalThis.gBrowser.removeTab(tab);
        } catch (error) {
          console.error(
            "WorkspacesService: failed to roll back a restored archive tab",
            error,
          );
        }
      }
    });
    try {
      this.dataManagerCtx.deleteWorkspace(workspaceId);
    } catch (error) {
      console.error(
        "WorkspacesService: failed to roll back a created workspace",
        error,
      );
    }
    setWorkspacesDataStore(
      "order",
      (prev) => prev.filter((id) => id !== workspaceId),
    );
  }

  public async backupNow(): Promise<WorkspaceBackupSummary> {
    return await this.backupService.backupNow("manual");
  }

  public async listBackups(): Promise<WorkspaceBackupSummary[]> {
    return await this.backupService.listBackups();
  }

  public async restoreBackup(
    fileName: string,
  ): Promise<{ success: boolean; error?: string }> {
    return await this.backupService.restoreBackup(fileName);
  }

  public async deleteBackup(fileName: string): Promise<boolean> {
    return await this.backupService.deleteBackup(fileName);
  }

  public async deleteAllBackups(): Promise<boolean> {
    return await this.backupService.deleteAllBackups();
  }

  /**
   * Replaces the entire workspace store with the contents of a backup file.
   *
   * Validation runs before any destructive step: an invalid file leaves both
   * the store and the tabs untouched. Tabs are restored into the currently
   * focused browser window; other windows keep their existing tabs, which is
   * the documented v2 limitation.
   */
  public async restoreWorkspaceState(
    file: WorkspaceBackupFile,
  ): Promise<void> {
    const state = file.workspaceState;
    if (!state) {
      throw new Error("workspace-state-missing");
    }

    // 1. Validate everything before touching the store or the tabs.
    const metaById = new Map<TWorkspaceID, WorkspaceBackupWorkspace>();
    for (const workspace of state.workspaces) {
      if (metaById.has(workspace.workspaceId)) {
        throw new Error("duplicate-workspace-id");
      }
      metaById.set(workspace.workspaceId, workspace);
    }

    // `order` must be a strict permutation of the workspace metadata. The
    // capture path always produces one, so anything else means a corrupt or
    // hand-edited file, and guessing an order before a destructive restore is
    // not acceptable.
    const orderSet = new Set(state.order);
    if (orderSet.size !== state.order.length) {
      throw new Error("duplicate-order-entry");
    }
    if (orderSet.size !== metaById.size) {
      throw new Error("order-workspace-mismatch");
    }
    for (const id of orderSet) {
      if (!metaById.has(id)) {
        throw new Error("order-references-unknown-workspace");
      }
    }
    const orderedIds: TWorkspaceID[] = [...state.order];
    if (orderedIds.length === 0) {
      throw new Error("empty-backup");
    }
    if (state.defaultID !== null && !metaById.has(state.defaultID)) {
      throw new Error("invalid-default-workspace");
    }
    if (
      state.currentWorkspaceId !== null &&
      !metaById.has(state.currentWorkspaceId)
    ) {
      throw new Error("invalid-current-workspace");
    }

    // 2. Snapshot everything a rollback needs before mutating any state.
    const previousData = this.cloneWorkspaceMap(workspacesDataStore.data);
    const previousOrder = [...workspacesDataStore.order] as TWorkspaceID[];
    const previousDefaultID = workspacesDataStore.defaultID as TWorkspaceID;
    const previousCurrentID = selectedWorkspaceID();
    const previousAttribution = new Map<
      XULElement,
      { workspaceId: string | null; lastShown: string | null }
    >();
    for (const tab of globalThis.gBrowser.tabs) {
      previousAttribution.set(tab, {
        workspaceId: tab.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID),
        lastShown: tab.getAttribute(WORKSPACE_LAST_SHOW_ID),
      });
    }

    // Tabs the backup never captured (private containers, history-disabled
    // tabs) must survive the restore: they cannot be recreated from the file.
    const privateContainerUserContextId = (() => {
      try {
        return PrivateContainer.getPrivateContainerUserContextId();
      } catch (error) {
        console.error(
          "WorkspacesService: private container lookup failed during restore",
          error,
        );
        return null;
      }
    })();
    const isExemptFromRestore = (tab: XULElement): boolean => {
      if (tab.hasAttribute("floorp-disablehistory")) {
        return true;
      }
      if (privateContainerUserContextId === null) {
        return false;
      }
      const userContextId = Number.parseInt(
        tab.getAttribute("usercontextid") ?? "0",
        10,
      );
      return userContextId === privateContainerUserContextId;
    };

    let outcomes: Awaited<ReturnType<typeof restoreTabsIntoWindow>> = [];

    const rollback = (): void => {
      // Remove every tab this restore created, successful or not. A failed tab
      // may still exist in the window with a half-applied state.
      for (const outcome of outcomes) {
        const tab = outcome.tab;
        if (!tab || !tab.isConnected) {
          continue;
        }
        try {
          globalThis.gBrowser.removeTab(tab);
        } catch (error) {
          console.error(
            "WorkspacesService: failed to roll back a restored tab",
            error,
          );
        }
      }
      // Restore the store and the tab attribution exactly as they were.
      setWorkspacesDataStore(
        "data",
        () => previousData as unknown as TWorkspacesStoreData["data"],
      );
      setWorkspacesDataStore("order", () => previousOrder);
      for (const [tab, attributes] of previousAttribution) {
        if (!tab.isConnected) {
          continue;
        }
        if (attributes.workspaceId) {
          tab.setAttribute(
            WORKSPACE_TAB_ATTRIBUTION_ID,
            attributes.workspaceId,
          );
        } else {
          tab.removeAttribute(WORKSPACE_TAB_ATTRIBUTION_ID);
        }
        if (attributes.lastShown) {
          tab.setAttribute(WORKSPACE_LAST_SHOW_ID, attributes.lastShown);
        } else {
          tab.removeAttribute(WORKSPACE_LAST_SHOW_ID);
        }
      }
      this.setDefaultWorkspace(previousDefaultID);
      if (previousCurrentID) {
        this.changeWorkspace(previousCurrentID);
      }
      this.tabManagerCtx.updateTabsVisibility();
    };

    // 3. Rebuild the store and restore tabs. Everything from here on mutates
    //    live state, so any throw must roll back.
    try {
      const tempWorkspaceId = this.resetWorkspaces();
      const idMap = new Map<TWorkspaceID, TWorkspaceID>();
      const restoredOrder: TWorkspaceID[] = [];

      for (const oldId of orderedIds) {
        const meta = metaById.get(oldId);
        if (!meta) {
          continue;
        }
        const restoredWorkspaceId = this.dataManagerCtx.createWorkspace(
          meta.name,
        );
        setWorkspacesDataStore(
          "order",
          (prev) => [...prev, restoredWorkspaceId],
        );
        setWorkspacesDataStore("data", (prev) => {
          const temp = this.cloneWorkspaceMap(prev);
          const workspace = temp.get(restoredWorkspaceId);
          if (workspace) {
            temp.set(restoredWorkspaceId, {
              ...workspace,
              userContextId: meta.userContextId,
              icon: meta.icon ?? null,
            });
          }
          return temp as unknown as TWorkspacesStoreData["data"];
        });
        idMap.set(oldId, restoredWorkspaceId);
        restoredOrder.push(restoredWorkspaceId);
      }

      const fallbackWorkspaceId = restoredOrder[0] as TWorkspaceID;

      // Tabs the backup never captured were moved onto the temporary
      // workspace by resetWorkspaces(). Park them on a real workspace so the
      // cleanup below cannot close them.
      for (const tab of globalThis.gBrowser.tabs) {
        if (!isExemptFromRestore(tab)) {
          continue;
        }
        if (
          tab.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID) !== tempWorkspaceId
        ) {
          continue;
        }
        tab.setAttribute(WORKSPACE_TAB_ATTRIBUTION_ID, fallbackWorkspaceId);
      }

      // 4. Restore tabs, mapping captured workspace IDs onto the new ones.
      //    Tabs without a (resolvable) workspace join the first restored one so
      //    they stay visible after the restore.
      // The last-shown marker also references a workspace ID; map it onto the
      // restored workspace or the saved tab selection is lost.
      const allTabs = file.windows.flatMap((window) => window.tabs).map(
        (tab) => ({
          ...tab,
          lastShownWorkspaceId: tab.lastShownWorkspaceId
            ? idMap.get(tab.lastShownWorkspaceId) ?? null
            : null,
        }),
      );
      outcomes = await restoreTabsIntoWindow(allTabs, {
        gBrowser: globalThis.gBrowser as unknown as RestoreTargetBrowser,
        resolveWorkspaceId: (tab) => {
          if (tab.workspaceId) {
            const mapped = idMap.get(tab.workspaceId);
            if (mapped) {
              return mapped;
            }
          }
          return fallbackWorkspaceId;
        },
      });

      // Replacing existing tabs is only safe when every captured tab came back.
      const failedCount = outcomes.filter((outcome) => !outcome.ok).length;
      if (failedCount > 0) {
        // The catch below performs the rollback exactly once.
        throw new Error(`restore-partial-failure:${failedCount}`);
      }

      // 5. Drop the temporary workspace together with the pre-restore tabs.
      //    Restored tabs already carry a new workspace ID, so they survive.
      this.closeTabsForWorkspace(tempWorkspaceId, fallbackWorkspaceId);
      this.dataManagerCtx.deleteWorkspace(tempWorkspaceId);
      setWorkspacesDataStore(
        "order",
        (prev) => prev.filter((id) => id !== tempWorkspaceId),
      );

      const defaultId = state.defaultID && idMap.has(state.defaultID)
        ? idMap.get(state.defaultID) as TWorkspaceID
        : fallbackWorkspaceId;
      this.setDefaultWorkspace(defaultId);
      const currentId = state.currentWorkspaceId &&
          idMap.has(state.currentWorkspaceId)
        ? idMap.get(state.currentWorkspaceId) as TWorkspaceID
        : defaultId;
      this.changeWorkspace(currentId);
      this.reassignOtherWindowsTabs(fallbackWorkspaceId);
      this.tabManagerCtx.updateTabsVisibility();
    } catch (error) {
      // A throw before the rollback point (store rebuild, SessionStore
      // initialization, unexpected tab failure) must not leave a half-restored
      // store behind.
      rollback();
      throw error;
    }
  }

  private closeTabsForWorkspace(
    workspaceId: TWorkspaceID,
    fallbackWorkspaceId: TWorkspaceID,
  ): void {
    const gBrowser = globalThis.gBrowser;
    const tabs = [...gBrowser.tabs].filter(
      (tab) => tab.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID) === workspaceId,
    );
    // The temporary workspace is current during a restore. Closing its last tab
    // would otherwise make handleTabClose create a replacement tab or close the
    // window (exitOnLastTabClose / closeWindowWithLastTab).
    this.tabManagerCtx.runWithTabCloseSuppressed(() => {
      for (const tab of tabs) {
        if (gBrowser.tabs.length <= 1) {
          this.tabManagerCtx.setWorkspaceIdToAttribute(
            tab,
            fallbackWorkspaceId,
          );
          continue;
        }
        try {
          gBrowser.removeTab(tab);
        } catch (error) {
          console.error(
            "WorkspacesService: failed to close replaced tab",
            error,
          );
        }
      }
    });
  }

  /**
   * Reset the workspace store to its default state and assign all tabs to the new workspace.
   * @returns The newly created workspace id.
   */
  public resetWorkspaces(): TWorkspaceID {
    console.info("WorkspacesService: resetting workspace data to defaults");

    setWorkspacesDataStore(
      "data",
      () =>
        new Map<
          TWorkspaceID,
          TWorkspace
        >() as unknown as TWorkspacesStoreData["data"],
    );
    setWorkspacesDataStore("order", () => [] as TWorkspacesStoreData["order"]);

    const newWorkspaceId = this.createNoNameWorkspace();
    this.setDefaultWorkspace(newWorkspaceId);
    this.setCurrentWorkspaceID(newWorkspaceId);

    try {
      const gBrowser = globalThis.gBrowser as
        | {
          tabs: XULElement[];
          showTab: (tab: XULElement) => void;
        }
        | undefined;

      if (gBrowser?.tabs) {
        for (const tab of gBrowser.tabs) {
          if (!tab) continue;
          this.tabManagerCtx.setWorkspaceIdToAttribute(tab, newWorkspaceId);
          tab.removeAttribute(WORKSPACE_LAST_SHOW_ID);
        }
        this.tabManagerCtx.updateTabsVisibility();
      }
    } catch (error) {
      console.error(
        "WorkspacesService: failed to reassign tabs on reset",
        error,
      );
    }

    return newWorkspaceId;
  }

  public async restoreTabsFromSnapshot(
    workspaceId: TWorkspaceID,
    tabs: TWorkspaceSnapshotTab[],
  ): Promise<Awaited<ReturnType<typeof restoreTabsIntoWindow>>> {
    if (tabs.length === 0) {
      return [];
    }

    const outcomes = await restoreTabsIntoWindow(tabs, {
      gBrowser: globalThis.gBrowser as unknown as RestoreTargetBrowser,
      resolveWorkspaceId: () => workspaceId,
    });
    this.tabManagerCtx.updateTabsVisibility();
    return outcomes;
  }

  /**
   * Location Change Listener.
   */
  private listener = {
    /**
     * Listener for location change. This function will monitor the location change and check the tabs visibility.
     * @returns void
     */
    onLocationChange: () => {
      this.tabManagerCtx.updateTabsVisibility();
    },
  };

  private boundHandleTabOpen: (event: Event) => void;

  /**
   * Attribute a new tab to its workspace. Its container must already have
   * been selected by the creation path, before constructing the browser.
   */
  private handleTabOpen = (event: Event) => {
    const tabEvent = event as CustomEvent;
    const tab = (tabEvent.target || tabEvent.detail) as XULElement;
    if (!tab) {
      return;
    }

    // Check if tab already has a workspace ID assigned
    const workspaceId = this.tabManagerCtx.getWorkspaceIdFromAttribute(tab);
    const currentWorkspaceId = this.getSelectedWorkspaceID();

    // If tab doesn't have a workspace ID, assign it to current workspace
    if (!workspaceId) {
      this.tabManagerCtx.setWorkspaceIdToAttribute(tab, currentWorkspaceId);
    }

    const explicitUserContextId = this.explicitTabUserContextOperations
      .consumeNext();
    if (explicitUserContextId !== null) {
      const openedTabUserContextId = Number.parseInt(
        tab.getAttribute("usercontextid") || "0",
        10,
      );
      if (openedTabUserContextId !== explicitUserContextId) {
        console.error(
          "WorkspacesService: Explicit tab container did not match TabOpen",
          {
            expectedUserContextId: explicitUserContextId,
            openedTabUserContextId,
          },
        );
      }
      return;
    }
  };
}
