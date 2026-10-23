/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
import { onCleanup } from "solid-js";
import { setWorkspacesDataStore, workspacesDataStore } from "./data/data";
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
  ensureSessionStore,
} from "./utils/workspace-snapshot";
import {
  applyWorkspaceSnapshotMetadata,
  WorkspacesArchiveService,
} from "./utils/workspaces-archive-service";
import type { TWorkspaceSnapshotTab } from "./utils/type";
import type { WorkspaceArchiveSummary } from "./utils/archive-types";
import {
  WORKSPACE_LAST_SHOW_ID,
  WORKSPACE_TAB_ATTRIBUTION_ID,
  WORKSPACES_CHANGED_OBSERVER_TOPIC,
} from "./utils/workspaces-static-names";
import { ExplicitTabUserContextOperations } from "./utils/explicit-tab-user-context.ts";
import {
  getWorkspaceSessionIdentities,
  getWorkspaceSessionName,
  releaseUnusedWorkspaceSession,
  resolveWorkspaceSessionSelection,
  WorkspaceSessionError,
} from "./utils/workspace-sessions.ts";
import type {
  ResolvedWorkspaceSession,
  WorkspaceOperationWindow,
  WorkspaceSessionReferenceWindow,
} from "./utils/workspace-session-types.ts";
import type { TFormResult } from "../modal-parent/utils/type.ts";
import { enabled } from "./data/config.ts";
import ModalParent from "../modal-parent/index.ts";
import { getWorkspaceSessionRuntime } from "./utils/workspace-session-runtime.ts";

export class WorkspacesService implements WorkspacesDataManagerBase {
  dataManagerCtx: WorkspacesDataManager;
  tabManagerCtx: WorkspacesTabManager;
  iconCtx: WorkspaceIcons;
  modalCtx: WorkspaceManageModal;
  archiveService: WorkspacesArchiveService;

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
    // deno-lint-ignore no-explicit-any
    (globalThis as any).workspacesFuncs = {
      createNoNameWorkspace: this.createNoNameWorkspace.bind(this),
      createWorkspaceFromDialog: this.createWorkspaceFromDialog.bind(this),
      changeWorkspaceToNext: this.changeWorkspaceToNext.bind(this),
      changeWorkspaceToPrevious: this.changeWorkspaceToPrevious.bind(this),
      captureWorkspaceSnapshot: this.captureWorkspaceSnapshot.bind(this),
      archiveWorkspace: this.archiveWorkspace.bind(this),
      listArchivedWorkspaces: this.listArchivedWorkspaces.bind(this),
      restoreArchivedWorkspace: this.restoreArchivedWorkspace.bind(this),
      deleteArchivedWorkspace: this.deleteArchivedWorkspace.bind(this),
      resetWorkspaces: this.resetWorkspaces.bind(this),
      getSelectedWorkspaceID: this.getSelectedWorkspaceID.bind(this),
      changeWorkspace: this.changeWorkspace.bind(this),
      isWorkspaceID: this.isWorkspaceID.bind(this),
      getCurrentWorkspaceUserContextId: this.getCurrentWorkspaceUserContextId
        .bind(this),
      recoverMissingSessionFromDialog: this.recoverMissingSessionFromDialog
        .bind(this),
      requestMoveTabsToWorkspace: this.tabManagerCtx.requestMoveTabsToWorkspace
        .bind(this.tabManagerCtx),
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
    if (!enabled()) return 0;
    const id = this.getSelectedWorkspaceID();
    return this.getRawWorkspace(id)?.userContextId ?? 0;
  }

  deleteWorkspace(workspaceID: TWorkspaceID): void {
    if (workspacesDataStore.data.size === 1) return;
    if (this.isWorkspaceInUseByOtherWindow(workspaceID)) {
      this.reportWorkspaceInUse();
      return;
    }

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

  /** Create through the UI without persisting anything until confirmation. */
  public async createWorkspaceFromDialog(): Promise<TWorkspaceID | null> {
    if (
      !enabled() || globalThis.window.closed ||
      getWorkspaceSessionRuntime().isPrivateWindow(globalThis.window)
    ) return null;
    const sourceWorkspace = this.getRawWorkspace(this.getSelectedWorkspaceID());
    const seed: TWorkspace = {
      name: i18next.t("workspaces.service.new-workspace", {
        count: this.getWorkspaceCount(),
      } as Record<string, unknown>) as string,
      icon: null,
      userContextId: sourceWorkspace?.userContextId ?? 0,
      isSelected: null,
      isDefault: null,
    };
    const result = await this.modalCtx.showCreateWorkspacesModal(seed);
    if (
      result === null || !enabled() || globalThis.window.closed ||
      getWorkspaceSessionRuntime().isPrivateWindow(globalThis.window)
    ) return null;

    const previousWorkspaceId = this.getSelectedWorkspaceID();
    const previousTab = globalThis.gBrowser.selectedTab;
    let session: ResolvedWorkspaceSession | null = null;
    let workspaceId: TWorkspaceID | null = null;
    let initialTab: XULElement | null = null;
    try {
      const normalized = this.resolveWorkspaceSessionResult(result);
      session = normalized.session;
      const workspace = applyWorkspaceModalResult(seed, normalized.result);
      if (!workspace) return null;

      workspaceId = this.dataManagerCtx.createWorkspace(workspace.name, {
        userContextId: workspace.userContextId,
        icon: workspace.icon,
      });
      const createdId = workspaceId;
      setWorkspacesDataStore("order", (prev) => [...prev, createdId]);

      // Creating a tab dispatches TabOpen before its workspace attribute is
      // assigned. Protect its real context from the previously selected WS.
      const captureInitialTab = (event: Event): void => {
        initialTab ??= event.target as XULElement;
      };
      globalThis.gBrowser.tabContainer.addEventListener(
        "TabOpen",
        captureInitialTab,
        true,
      );
      try {
        initialTab = this.withExplicitTabUserContext(
          session.userContextId,
          () => this.tabManagerCtx.createTabForWorkspace(createdId, false),
        );
      } finally {
        globalThis.gBrowser.tabContainer.removeEventListener(
          "TabOpen",
          captureInitialTab,
          true,
        );
      }
      this.changeWorkspace(createdId);
      if (this.getSelectedWorkspaceID() !== createdId) {
        throw new Error("Workspace activation failed");
      }
      return createdId;
    } catch (error) {
      const rollback = (step: () => void): void => {
        try {
          step();
        } catch (cleanupError) {
          console.error("[Workspaces] Creation rollback failed", cleanupError);
        }
      };
      // Restore the existing tab before closing the failed workspace's only
      // tab, so normal last-tab handlers cannot create a replacement or close
      // the window as part of cleanup.
      rollback(() => {
        if (this.isWorkspaceID(previousWorkspaceId)) {
          this.setCurrentWorkspaceID(previousWorkspaceId);
        }
        if (previousTab?.isConnected) {
          globalThis.gBrowser.selectedTab = previousTab;
        }
        this.tabManagerCtx.updateTabsVisibility();
      });
      if (initialTab) {
        const failedTab = initialTab;
        rollback(() => globalThis.gBrowser.removeTab(failedTab));
      }
      if (workspaceId) {
        const failedId = workspaceId;
        rollback(() => {
          setWorkspacesDataStore(
            "order",
            (prev) => prev.filter((id) => id !== failedId),
          );
          this.dataManagerCtx.deleteWorkspace(failedId);
        });
      }
      rollback(() => this.tabManagerCtx.updateTabsVisibility());
      if (session) this.releaseUnusedSession(session);
      this.reportWorkspaceSessionError(error);
      return null;
    }
  }

  private resolveWorkspaceSessionResult(result: TFormResult) {
    if (typeof result.name !== "string" || !result.name.trim()) {
      throw new WorkspaceSessionError("invalid");
    }
    const name = result.name.trim();
    const session = resolveWorkspaceSessionSelection(
      result.userContextId,
      name,
    );
    return {
      session,
      result: { ...result, name, userContextId: String(session.userContextId) },
    };
  }

  private isWorkspaceSessionReferenced(userContextId: number): boolean {
    for (
      const workspace of (workspacesDataStore.data as unknown as Map<
        TWorkspaceID,
        TWorkspace
      >).values()
    ) {
      if (workspace.userContextId === userContextId) return true;
    }
    for (const rawWindow of Services.wm.getEnumerator("navigator:browser")) {
      const window = rawWindow as unknown as WorkspaceSessionReferenceWindow;
      for (const tab of window.gBrowser?.tabs ?? []) {
        if (tab.userContextId === userContextId) return true;
      }
      if (
        window.document.querySelector(
          `browser[usercontextid="${userContextId}"]`,
        )
      ) {
        return true;
      }
    }
    return false;
  }

  private releaseUnusedSession(session: ResolvedWorkspaceSession): void {
    try {
      releaseUnusedWorkspaceSession(
        session,
        (id) => this.isWorkspaceSessionReferenced(id),
      );
    } catch (error) {
      console.error("[Workspaces] Session rollback failed", error);
    }
  }

  private reportWorkspaceSessionError(error: unknown): void {
    console.error("[Workspaces] Session operation failed", error);
    const reason = error instanceof WorkspaceSessionError
      ? error.reason
      : "create";
    Services.prompt.alert(
      globalThis.window as unknown as mozIDOMWindowProxy,
      i18next.t("workspaces.session.error-title"),
      i18next.t(`workspaces.session.error-${reason}`),
    );
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
    if (
      !enabled() || globalThis.window.closed ||
      getWorkspaceSessionRuntime().isPrivateWindow(globalThis.window)
    ) return null;
    const targetWorkspaceID = id ?? this.getSelectedWorkspaceID();
    if (!this.getRawWorkspace(targetWorkspaceID)) return;
    const result = await this.modalCtx.showWorkspacesModal(targetWorkspaceID);
    if (
      result === null || !enabled() || globalThis.window.closed ||
      getWorkspaceSessionRuntime().isPrivateWindow(globalThis.window)
    ) return null;
    if (!this.getRawWorkspace(targetWorkspaceID)) return;
    let session: ResolvedWorkspaceSession | null = null;
    try {
      const normalized = this.resolveWorkspaceSessionResult(result);
      session = normalized.session;
      let didApply = false;
      setWorkspacesDataStore("data", (prev) => {
        const temp = this.cloneWorkspaceMap(prev);
        const latestWorkspace = temp.get(targetWorkspaceID);
        if (!latestWorkspace) return prev;
        const newWorkspace = applyWorkspaceModalResult(
          latestWorkspace,
          normalized.result,
        );
        if (!newWorkspace) return prev;
        temp.set(targetWorkspaceID, newWorkspace);
        didApply = true;
        return temp as unknown as TWorkspacesStoreData["data"];
      });
      if (!didApply) {
        this.releaseUnusedSession(session);
        return;
      }
      this.tabManagerCtx.updateTabsVisibility();
      return normalized.result;
    } catch (error) {
      if (session) this.releaseUnusedSession(session);
      this.reportWorkspaceSessionError(error);
      return null;
    }
  }

  public changeWorkspace(id: TWorkspaceID) {
    if (!enabled() || !this.isWorkspaceID(id)) return;
    this.tabManagerCtx.changeWorkspace(id);
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
    if (this.isWorkspaceInUseByOtherWindow(targetWorkspaceId)) {
      this.reportWorkspaceInUse();
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
    if (
      !enabled() || globalThis.window.closed ||
      !this.isWorkspaceID(targetWorkspaceId)
    ) return null;

    const archiveId = await this.archiveService.saveSnapshot(snapshot);
    if (
      !enabled() || globalThis.window.closed ||
      !this.isWorkspaceID(targetWorkspaceId) ||
      this.isWorkspaceInUseByOtherWindow(targetWorkspaceId)
    ) {
      await this.archiveService.removeSnapshot(archiveId);
      if (
        enabled() && !globalThis.window.closed &&
        this.isWorkspaceID(targetWorkspaceId)
      ) this.reportWorkspaceInUse();
      return null;
    }
    this.deleteWorkspace(targetWorkspaceId);
    if (this.isWorkspaceID(targetWorkspaceId)) {
      await this.archiveService.removeSnapshot(archiveId);
      return null;
    }
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

    await this.restoreTabsFromSnapshot(restoredWorkspaceId, snapshot.tabs);

    const originalWorkspaceId = snapshot.workspace.workspaceId;
    if (!this.isWorkspaceID(originalWorkspaceId)) {
      const { WorkspaceBookmarks } = ChromeUtils.importESModule(
        "resource://noraneko/modules/WorkspaceBookmarks.sys.mjs",
      ) as typeof import("#modules/modules/WorkspaceBookmarks.sys.mts");
      await WorkspaceBookmarks.reassignWorkspace(
        originalWorkspaceId,
        restoredWorkspaceId,
      );
    }

    await this.archiveService.removeSnapshot(archiveId);

    this.changeWorkspace(restoredWorkspaceId);
    return restoredWorkspaceId;
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

  private async restoreTabsFromSnapshot(
    workspaceId: TWorkspaceID,
    tabs: TWorkspaceSnapshotTab[],
  ) {
    if (!tabs.length) {
      return;
    }

    const sessionStore = await ensureSessionStore();
    let targetTab: XULElement | null = null;

    for (const tabSnapshot of tabs) {
      const runtime = getWorkspaceSessionRuntime();
      const storedState = tabSnapshot.state
        ? structuredClone(tabSnapshot.state)
        : {
          entries: [{ url: tabSnapshot.url ?? "about:blank" }],
          index: 1,
          userContextId: tabSnapshot.userContextId,
        };
      storedState.userContextId = tabSnapshot.userContextId;
      storedState.floorpWorkspaceId = workspaceId;
      const extData = storedState.extData as Record<string, string> | undefined;
      const heldState = extData?.["floorp.workspace.pendingSession"];
      if (heldState && extData) {
        const original = JSON.parse(heldState) as Record<string, unknown>;
        original.floorpWorkspaceId = workspaceId;
        const originalAttributes = original.attributes as
          | Record<string, unknown>
          | undefined;
        if (originalAttributes) {
          originalAttributes[WORKSPACE_TAB_ATTRIBUTION_ID] = workspaceId;
        }
        if (tabSnapshot.lastShownWorkspaceId) {
          original.floorpLastShowWorkspaceId = workspaceId;
          if (originalAttributes) {
            originalAttributes[WORKSPACE_LAST_SHOW_ID] = workspaceId;
          }
        }
        extData["floorp.workspace.pendingSession"] = JSON.stringify(original);
      }
      const safeState = runtime.sanitizeRestoreState(storedState);
      const tab = this.withExplicitTabUserContext(
        safeState.userContextId ?? 0,
        () =>
          globalThis.gBrowser.addTab("about:blank", {
            skipAnimation: true,
            inBackground: true,
            userContextId: safeState.userContextId ?? 0,
            triggeringPrincipal: Services.scriptSecurityManager
              .getSystemPrincipal(),
          }),
      );

      this.tabManagerCtx.setWorkspaceIdToAttribute(tab, workspaceId);
      if (safeState.extData?.["floorp.workspace.pendingSession"]) {
        runtime.markPendingTab(tab, safeState);
      }

      if (tabSnapshot.lastShownWorkspaceId) {
        tab.setAttribute(
          WORKSPACE_LAST_SHOW_ID,
          workspaceId,
        );
      } else {
        tab.removeAttribute(WORKSPACE_LAST_SHOW_ID);
      }

      if (safeState) {
        try {
          const clonedState = structuredClone(safeState);
          const attributes =
            (clonedState.attributes as Record<string, unknown> | undefined) ??
              {};
          attributes[WORKSPACE_TAB_ATTRIBUTION_ID] = workspaceId;
          if (tabSnapshot.lastShownWorkspaceId) {
            attributes[WORKSPACE_LAST_SHOW_ID] = workspaceId;
          } else if (WORKSPACE_LAST_SHOW_ID in attributes) {
            delete attributes[WORKSPACE_LAST_SHOW_ID];
          }
          clonedState.attributes = attributes;
          sessionStore.setTabState(tab, JSON.stringify(clonedState));
        } catch (error) {
          console.error(
            "WorkspacesService: failed to restore tab state",
            error,
          );
        }
      }

      if (tabSnapshot.pinned) {
        try {
          globalThis.gBrowser.pinTab(tab);
        } catch (error) {
          console.error("WorkspacesService: failed to pin restored tab", error);
        }
      }

      if (tabSnapshot.isSelected) {
        targetTab = tab;
      }
    }

    if (targetTab) {
      globalThis.gBrowser.selectedTab = targetTab;
    }

    this.tabManagerCtx.updateTabsVisibility();
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
   * Handle TabOpen event to apply workspace container to new tabs.
   * This ensures tabs opened from bookmarks, external links, etc. use the current workspace's container.
   */
  private handleTabOpen = (event: Event) => {
    if (!enabled()) return;
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

  public isWorkspaceInUseByOtherWindow(workspaceId: TWorkspaceID): boolean {
    for (const rawWindow of Services.wm.getEnumerator("navigator:browser")) {
      const window = rawWindow as unknown as WorkspaceOperationWindow;
      if (
        window === globalThis.window || window.closed ||
        getWorkspaceSessionRuntime().isPrivateWindow(window)
      ) continue;
      if (window.workspacesFuncs?.getSelectedWorkspaceID?.() === workspaceId) {
        return true;
      }
      for (const tab of window.gBrowser?.tabs ?? []) {
        if (tab.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID) === workspaceId) {
          return true;
        }
      }
    }
    return false;
  }

  private reportWorkspaceInUse(): void {
    Services.prompt.alert(
      globalThis.window as unknown as mozIDOMWindowProxy,
      i18next.t("workspaces.session.in-use-title", {
        defaultValue: "Workspace is in use",
      }),
      i18next.t("workspaces.session.in-use", {
        defaultValue:
          "This workspace is selected or has tabs in another window. Close those tabs and switch that window before deleting or archiving it.",
      }),
    );
  }

  public async recoverMissingSessionFromDialog(
    oldId?: number,
    workspaceId = this.getSelectedWorkspaceID(),
  ): Promise<boolean> {
    if (
      !enabled() || !this.isWorkspaceID(workspaceId) ||
      getWorkspaceSessionRuntime().isPrivateWindow(globalThis.window)
    ) return false;
    const workspace = this.getRawWorkspace(workspaceId);
    if (!workspace) return false;
    const missingId = oldId ?? workspace.userContextId;
    const runtime = getWorkspaceSessionRuntime();
    const pending = runtime.pendingTabs(workspaceId, missingId);
    const modal = ModalParent.getInstance();
    const result = await new Promise<TFormResult | null>((resolve) => {
      void modal.showNoraModal(
        {
          title: i18next.t("workspaces.session.recover-title", {
            defaultValue: "Recover login environment",
          }),
          description: i18next.t("workspaces.session.recover-description", {
            defaultValue:
              "Choose where to reopen {{count}} pending tabs in {{name}}. URLs are reopened without copying forms, history or site data.",
            count: pending.length,
            name: workspace.name,
          }),
          forms: [{
            id: "userContextId",
            type: "select",
            required: true,
            value: "",
            label: i18next.t("workspaces.modal.container"),
            options: [
              {
                value: "",
                label: i18next.t("workspaces.modal.choose-session"),
              },
              {
                value: "new",
                label: i18next.t("workspaces.modal.new-session"),
              },
              { value: "0", label: i18next.t("workspaces.modal.no-container") },
              ...getWorkspaceSessionIdentities().map((identity) => ({
                value: String(identity.userContextId),
                label: getWorkspaceSessionName(identity.userContextId) ??
                  identity.name,
              })),
            ],
          }],
          submitLabel: i18next.t("workspaces.modal.save"),
          cancelLabel: i18next.t("workspaces.modal.cancel"),
        },
        { width: 540, height: 430 },
        resolve,
      ).catch(() => resolve(null));
    });
    if (
      !result || !enabled() || globalThis.window.closed ||
      !this.isWorkspaceID(workspaceId)
    ) return false;
    const latestPending = runtime.pendingTabs(workspaceId, missingId);
    if (
      latestPending.length !== pending.length ||
      latestPending.some(({ tab, state }) =>
        !pending.some((captured) =>
          captured.tab === tab &&
          JSON.stringify(captured.state) === JSON.stringify(state)
        )
      )
    ) return false;
    let session: ResolvedWorkspaceSession | null = null;
    try {
      session = resolveWorkspaceSessionSelection(
        result.userContextId,
        workspace.name,
      );
      const newId = session.userContextId;
      const recovered = runtime.recoverPendingTabs(
        workspaceId,
        missingId,
        newId,
      );
      if (recovered.failed) {
        throw new Error("Some pending tabs could not be recovered");
      }
      setWorkspacesDataStore("data", (prev) => {
        const map = this.cloneWorkspaceMap(prev);
        const latest = map.get(workspaceId);
        if (latest?.userContextId === missingId) {
          map.set(workspaceId, { ...latest, userContextId: newId });
        }
        return map as unknown as TWorkspacesStoreData["data"];
      });
      Services.obs.notifyObservers(
        null as unknown as nsISupports,
        WORKSPACES_CHANGED_OBSERVER_TOPIC,
        workspaceId,
      );
      this.tabManagerCtx.updateTabsVisibility();
      return true;
    } catch (error) {
      if (session) this.releaseUnusedSession(session);
      this.reportWorkspaceSessionError(error);
      return false;
    }
  }
}
