/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
import { batch, onCleanup } from "solid-js";
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
      console.error(
        "WorkspacesService: archive restore failed for some tabs",
        failedCount,
      );
      throw new Error("archive-restore-partial-failure");
    }

    await this.archiveService.removeSnapshot(archiveId);

    this.changeWorkspace(restoredWorkspaceId);
    return restoredWorkspaceId;
  }

  /**
   * Re-points tabs in other normal windows at the restored workspaces.
   *
   * Runs after the restore commit point and before the store drops the
   * pre-restore workspaces, so every ID written here already exists in the
   * store other windows observe. A tab whose ID matches a backup workspace ID
   * follows it onto the restored copy; tabs already on a restored workspace
   * are left alone; any other live ID (a workspace the backup does not
   * contain) moves to the fallback workspace.
   */
  private reassignOtherWindowsTabs(
    idMap: Map<TWorkspaceID, TWorkspaceID>,
    fallbackWorkspaceId: TWorkspaceID,
  ): void {
    const restoredIds = new Set<TWorkspaceID>(idMap.values());
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
        try {
          const workspaceId = tab.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID);
          // Unattributed tabs are claimed by that window's own visibility
          // pass once the store settles.
          if (workspaceId && !restoredIds.has(workspaceId as TWorkspaceID)) {
            tab.setAttribute(
              WORKSPACE_TAB_ATTRIBUTION_ID,
              idMap.get(workspaceId as TWorkspaceID) ?? fallbackWorkspaceId,
            );
          }
          // The last-shown marker references a workspace ID as well; keep it
          // only when it maps onto a restored workspace.
          const lastShown = tab.getAttribute(WORKSPACE_LAST_SHOW_ID);
          if (lastShown && !restoredIds.has(lastShown as TWorkspaceID)) {
            const mappedLastShown = idMap.get(lastShown as TWorkspaceID);
            if (mappedLastShown) {
              tab.setAttribute(WORKSPACE_LAST_SHOW_ID, mappedLastShown);
            } else {
              tab.removeAttribute(WORKSPACE_LAST_SHOW_ID);
            }
          }
        } catch (error) {
          console.error(
            "WorkspacesService: failed to re-attribute a tab in another window",
            error,
          );
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
   * Validation runs before any mutation: an invalid or empty file leaves both
   * the store and the tabs untouched. Tabs are restored into the currently
   * focused browser window; tabs in other windows are re-attributed onto the
   * restored workspaces (or the fallback one) but are never closed.
   *
   * The store is mirrored to a pref that every window observes, so it is
   * never left empty or half-built. The restore runs in two phases:
   *
   * 1. Pre-commit: the restored workspaces are added next to the existing
   *    ones in one batch (fresh IDs), and the captured tabs are restored into
   *    them. Pre-restore workspaces stay valid throughout, so no window sees
   *    its tabs' IDs disappear. Any failure here rolls back: the created tabs
   *    are removed and the previous store is put back in one batch.
   * 2. Commit point: once every tab is restored, the restore is committed and
   *    never rolled back. Other windows' tabs are re-attributed, the store is
   *    swapped for the restored workspaces in one batch, this window's
   *    replaced tabs are closed, and the restored workspace is shown. A
   *    failure in this phase is logged and the call still resolves, because
   *    rolling back after original tabs are closed would lose tabs.
   */
  public async restoreWorkspaceState(
    file: WorkspaceBackupFile,
    options?: { preserveTabs?: ReadonlySet<XULElement> },
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
    // Restoring a file without tabs would only close the current ones.
    const totalTabCount = file.windows.reduce(
      (count, window) => count + window.tabs.length,
      0,
    );
    if (totalTabCount === 0) {
      throw new Error("empty-backup");
    }

    // Tabs the backup never captured (private containers, history-disabled
    // tabs) must survive the restore: they cannot be recreated from the file.
    let privateContainerUserContextId: number | null;
    try {
      privateContainerUserContextId = PrivateContainer
        .getPrivateContainerUserContextId();
    } catch (error) {
      // Without a reliable container identity we cannot tell which tabs must
      // survive, so refuse before mutating anything.
      console.error(
        "WorkspacesService: private container lookup failed during restore",
        error,
      );
      throw new Error("private-container-detection-failed");
    }
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

    // 2. Snapshot what a rollback needs and decide which tabs the restore
    //    replaces, before mutating any state. Only tabs present now are ever
    //    closed; tabs opened while the restore runs are kept.
    const previousDefaultID = workspacesDataStore.defaultID as TWorkspaceID;
    const preRestoreTabs = new Set<XULElement>(globalThis.gBrowser.tabs);
    const previousCurrentID = selectedWorkspaceID();
    const previousSelectedTab = globalThis.gBrowser.selectedTab as
      | XULElement
      | null;
    const preserveTabs = options?.preserveTabs;
    const tabsToReplace = new Set<XULElement>(
      [...globalThis.gBrowser.tabs].filter((tab) =>
        !isExemptFromRestore(tab) && !preserveTabs?.has(tab)
      ),
    );

    const idMap = new Map<TWorkspaceID, TWorkspaceID>();
    const restoredOrder: TWorkspaceID[] = [];
    let outcomes: Awaited<ReturnType<typeof restoreTabsIntoWindow>> = [];

    const rollback = (): void => {
      // Every step is isolated so one failure cannot skip the others.
      try {
        // Remove every tab this restore created, successful or not. A failed
        // tab may still exist in the window with a half-applied state. If
        // restoreTabsIntoWindow threw, `outcomes` is empty, so also sweep tabs
        // that did not exist before and carry a restored workspace ID. Tabs
        // the user opened meanwhile normally carry a pre-restore ID and are
        // kept. The restored workspaces already exist during the single
        // `ensureSessionStore()` await in restoreTabsIntoWindow, so a tab the
        // user opens in one of them in that brief window is swept too; its
        // tab loop is synchronous. Revisit if an await is added inside it.
        const restoredIds = new Set<string>(restoredOrder);
        const created = new Set<XULElement>(
          outcomes.flatMap((outcome) => (outcome.tab ? [outcome.tab] : [])),
        );
        for (const tab of globalThis.gBrowser.tabs) {
          const workspaceId = tab.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID);
          if (
            !preRestoreTabs.has(tab) && workspaceId &&
            restoredIds.has(workspaceId)
          ) {
            created.add(tab);
          }
        }
        // Select a pre-restore tab first: removing the selected restored tab
        // would otherwise select (and start loading) the next restored tab,
        // once per tab being removed.
        const gBrowser = globalThis.gBrowser;
        if (created.has(gBrowser.selectedTab)) {
          const usable = (tab: XULElement): boolean =>
            tab.isConnected &&
            !(tab as XULElement & { closing?: boolean }).closing;
          // Prefer the tab that was selected before the restore (usually the
          // Hub page that started it), then any tab of the previous workspace.
          const fallbackTab = (previousSelectedTab &&
              usable(previousSelectedTab)
            ? previousSelectedTab
            : null) ??
            [...preRestoreTabs].find((tab) =>
              usable(tab) &&
              (!previousCurrentID ||
                tab.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID) ===
                  previousCurrentID)
            ) ??
            [...preRestoreTabs].find(usable);
          if (fallbackTab) {
            gBrowser.selectedTab = fallbackTab;
          }
        }
        this.tabManagerCtx.runWithTabCloseSuppressed(() => {
          for (const tab of created) {
            if (!tab.isConnected) {
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
        });
      } catch (error) {
        console.error(
          "WorkspacesService: failed to remove restored tabs on rollback",
          error,
        );
      }
      try {
        // Remove only what this restore added instead of writing back a
        // snapshot, so workspace edits other windows made meanwhile survive.
        const restoredIds = new Set<TWorkspaceID>(restoredOrder);
        batch(() => {
          setWorkspacesDataStore("data", (prev) => {
            const temp = this.cloneWorkspaceMap(prev);
            for (const id of restoredIds) {
              temp.delete(id);
            }
            return temp as unknown as TWorkspacesStoreData["data"];
          });
          setWorkspacesDataStore(
            "order",
            (prev) => prev.filter((id) => !restoredIds.has(id)),
          );
          if (restoredIds.has(workspacesDataStore.defaultID as TWorkspaceID)) {
            setWorkspacesDataStore("defaultID", previousDefaultID);
          }
        });
      } catch (error) {
        console.error(
          "WorkspacesService: failed to restore the workspace store",
          error,
        );
      }
      try {
        if (previousCurrentID && this.isWorkspaceID(previousCurrentID)) {
          this.changeWorkspace(previousCurrentID);
        }
      } catch (error) {
        console.error(
          "WorkspacesService: failed to reselect the previous workspace",
          error,
        );
      }
      try {
        this.tabManagerCtx.updateTabsVisibility();
      } catch (error) {
        console.error(
          "WorkspacesService: failed to refresh tabs on rollback",
          error,
        );
      }
    };

    // 3. Pre-commit: add the restored workspaces next to the existing ones and
    //    restore tabs into them. Any throw rolls back.
    try {
      batch(() => {
        for (const oldId of orderedIds) {
          const meta = metaById.get(oldId);
          if (!meta) {
            continue;
          }
          const restoredWorkspaceId = this.dataManagerCtx.createWorkspace(
            meta.name,
          );
          idMap.set(oldId, restoredWorkspaceId);
          restoredOrder.push(restoredWorkspaceId);
        }
        setWorkspacesDataStore("data", (prev) => {
          const temp = this.cloneWorkspaceMap(prev);
          for (const [oldId, restoredWorkspaceId] of idMap) {
            const meta = metaById.get(oldId);
            const workspace = temp.get(restoredWorkspaceId);
            if (meta && workspace) {
              temp.set(restoredWorkspaceId, {
                ...workspace,
                userContextId: meta.userContextId,
                icon: meta.icon ?? null,
              });
            }
          }
          return temp as unknown as TWorkspacesStoreData["data"];
        });
        setWorkspacesDataStore(
          "order",
          (prev) => [...prev, ...restoredOrder],
        );
      });

      const fallbackWorkspaceId = restoredOrder[0] as TWorkspaceID;

      // Restore tabs, mapping captured workspace IDs onto the new ones. Tabs
      // without a (resolvable) workspace join the first restored one so they
      // stay visible after the restore. The last-shown marker also references
      // a workspace ID; map it too or the saved tab selection is lost.
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

      // Replacing existing tabs is only safe when every restorable tab came
      // back. Tabs skipped for an unsupported URL are not failures.
      const failedCount = outcomes.filter((outcome) =>
        !outcome.ok && !outcome.skipped
      ).length;
      if (failedCount > 0) {
        // Keep the code bare so the UI can map it to a message; the count is
        // for diagnostics.
        console.error(
          "WorkspacesService: restore failed for some tabs",
          failedCount,
        );
        throw new Error("restore-partial-failure");
      }
      if (!outcomes.some((outcome) => outcome.ok)) {
        // Every tab was skipped: committing would close the current tabs and
        // restore nothing.
        throw new Error("empty-backup");
      }
    } catch (error) {
      try {
        rollback();
      } catch (rollbackError) {
        console.error(
          "WorkspacesService: restore rollback failed",
          rollbackError,
        );
      }
      throw error;
    }

    // 4. Commit point. From here on nothing is rolled back: closing the
    //    replaced tabs is irreversible, and undoing earlier steps would only
    //    leave a store that disagrees with the tabs.
    this.commitRestoredWorkspaces({
      idMap,
      restoredOrder,
      defaultBackupId: state.defaultID,
      currentBackupId: state.currentWorkspaceId,
      restoredTabs: new Set(
        outcomes.flatMap((outcome) => (outcome.tab ? [outcome.tab] : [])),
      ),
      tabsToReplace,
    });
  }

  /**
   * Post-commit half of restoreWorkspaceState(). Each step is isolated and
   * only logs on failure so the remaining steps still run.
   */
  private commitRestoredWorkspaces(commit: {
    idMap: Map<TWorkspaceID, TWorkspaceID>;
    restoredOrder: TWorkspaceID[];
    defaultBackupId: TWorkspaceID | null;
    currentBackupId: TWorkspaceID | null;
    restoredTabs: ReadonlySet<XULElement>;
    tabsToReplace: ReadonlySet<XULElement>;
  }): void {
    const { idMap, restoredOrder, restoredTabs, tabsToReplace } = commit;
    const fallbackWorkspaceId = restoredOrder[0] as TWorkspaceID;
    const defaultId =
      commit.defaultBackupId && idMap.has(commit.defaultBackupId)
        ? idMap.get(commit.defaultBackupId) as TWorkspaceID
        : fallbackWorkspaceId;
    const currentId = commit.currentBackupId &&
        idMap.has(commit.currentBackupId)
      ? idMap.get(commit.currentBackupId) as TWorkspaceID
      : defaultId;
    const logStepError = (step: string, error: unknown): void => {
      console.error(
        `WorkspacesService: restore committed, but ${step} failed`,
        error,
      );
    };

    // (a) Point every tab that survives the restore at a restored workspace
    //     while the pre-restore workspaces still exist, so no window observes
    //     a tab whose workspace vanished.
    try {
      // Unmapped tabs in other windows go to the default workspace: that is
      // the one those windows show after the store swap.
      this.reassignOtherWindowsTabs(idMap, defaultId);
    } catch (error) {
      logStepError("re-attributing other windows", error);
    }
    try {
      for (const tab of [...globalThis.gBrowser.tabs]) {
        if (restoredTabs.has(tab) || tabsToReplace.has(tab)) {
          continue;
        }
        // Preserved, exempt, and tabs opened during the restore (including
        // the settings page that started it) join the workspace shown after
        // the restore, so they stay visible.
        tab.setAttribute(WORKSPACE_TAB_ATTRIBUTION_ID, currentId);
      }
    } catch (error) {
      logStepError("parking preserved tabs", error);
    }

    // (b) Swap the store for the restored workspaces in a single batch,
    //     together with this window's selection, so neither this window nor
    //     the pref observers in other windows see an intermediate state.
    try {
      batch(() => {
        // Keep only the restored workspaces; pre-restore ones are dropped.
        const current = this.cloneWorkspaceMap(workspacesDataStore.data);
        const nextData = new Map<TWorkspaceID, TWorkspace>();
        for (const id of restoredOrder) {
          const workspace = current.get(id);
          if (workspace) {
            nextData.set(id, workspace);
          }
        }
        setWorkspacesDataStore(
          "data",
          () => nextData as unknown as TWorkspacesStoreData["data"],
        );
        setWorkspacesDataStore("order", () => [...restoredOrder]);
        setWorkspacesDataStore("defaultID", defaultId);
        this.dataManagerCtx.setCurrentWorkspaceID(currentId);
      });
    } catch (error) {
      logStepError("replacing the workspace store", error);
    }

    // (c) Close this window's replaced tabs.
    try {
      this.closeReplacedTabs(tabsToReplace, currentId);
    } catch (error) {
      logStepError("closing replaced tabs", error);
    }

    // (d) Show the restored workspace.
    try {
      this.changeWorkspace(currentId);
    } catch (error) {
      logStepError("switching to the restored workspace", error);
    }
    try {
      this.tabManagerCtx.updateTabsVisibility();
    } catch (error) {
      logStepError("refreshing tab visibility", error);
    }
  }

  private closeReplacedTabs(
    tabsToReplace: ReadonlySet<XULElement>,
    fallbackWorkspaceId: TWorkspaceID,
  ): void {
    const gBrowser = globalThis.gBrowser;
    // Closing the last tab of the current workspace would otherwise make
    // handleTabClose create a replacement tab or close the window
    // (exitOnLastTabClose / closeWindowWithLastTab).
    this.tabManagerCtx.runWithTabCloseSuppressed(() => {
      for (const tab of tabsToReplace) {
        if (!tab.isConnected) {
          continue;
        }
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
          // Keep the tab on a workspace that still exists.
          try {
            this.tabManagerCtx.setWorkspaceIdToAttribute(
              tab,
              fallbackWorkspaceId,
            );
          } catch (attributeError) {
            console.error(
              "WorkspacesService: failed to reassign replaced tab",
              attributeError,
            );
          }
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
      // Propagate: the caller must roll back rather than continue a destructive
      // restore with tabs that still point at workspaces that no longer exist.
      console.error(
        "WorkspacesService: failed to reassign tabs on reset",
        error,
      );
      throw error;
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
      // Keep the archive restore behavior this PR inherited: archived tabs
      // reopen with their original URLs and state (including internal pages),
      // which the backup allowlist would otherwise skip. Hardening archive
      // files the same way as backups is a separate change.
      trustedSource: true,
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
