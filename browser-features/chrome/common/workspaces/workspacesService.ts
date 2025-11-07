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
import { WorkspaceManageModal } from "./workspace-modal";
import i18next from "i18next";
import {
  createWorkspaceSnapshot,
  ensureSessionStore,
} from "./utils/workspace-snapshot";
import { WorkspacesArchiveService } from "./utils/workspaces-archive-service";
import type { TWorkspaceSnapshotTab } from "./utils/type";
import type { WorkspaceArchiveSummary } from "./utils/archive-types";
import {
  WORKSPACE_LAST_SHOW_ID,
  WORKSPACE_TAB_ATTRIBUTION_ID,
} from "./utils/workspaces-static-names";

export class WorkspacesService implements WorkspacesDataManagerBase {
  dataManagerCtx: WorkspacesDataManager;
  tabManagerCtx: WorkspacesTabManager;
  iconCtx: WorkspaceIcons;
  modalCtx: WorkspaceManageModal;
  archiveService: WorkspacesArchiveService;

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
      changeWorkspaceToNext: this.changeWorkspaceToNext.bind(this),
      changeWorkspaceToPrevious: this.changeWorkspaceToPrevious.bind(this),
      captureWorkspaceSnapshot: this.captureWorkspaceSnapshot.bind(this),
      archiveWorkspace: this.archiveWorkspace.bind(this),
      listArchivedWorkspaces: this.listArchivedWorkspaces.bind(this),
      restoreArchivedWorkspace: this.restoreArchivedWorkspace.bind(this),
    };

    if (workspacesDataStore.data.size === 0) {
      const id = this.createNoNameWorkspace();
      this.setDefaultWorkspace(id);
      this.setCurrentWorkspaceID(id);
    }

    window.gBrowser.addProgressListener(this.listener);

    onCleanup(() => {
      window.gBrowser.removeProgressListener(this.listener);
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
    const id = this.getSelectedWorkspaceID();
    return this.getRawWorkspace(id)?.userContextId ?? 0;
  }

  deleteWorkspace(workspaceID: TWorkspaceID): void {
    if (workspacesDataStore.data.size === 1) return;
    this.tabManagerCtx.removeTabByWorkspaceId(workspaceID);
    setWorkspacesDataStore("order", (prev) =>
      prev.filter((v) => v !== workspaceID),
    );
    this.dataManagerCtx.deleteWorkspace(workspaceID);
  }

  /**
   * Returns new workspace object with default name.
   * @returns The new workspace id.
   */
  public createNoNameWorkspace(): TWorkspaceID {
    const count = this.getWorkspaceCount();
    const workspaceName = i18next.t(
      "workspaces.service.new-workspace",
      undefined,
      { count } as Record<string, unknown>,
    ) as string;

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
   * Open manage workspace dialog. This function should not be called directly on Preferences page.
   * @param workspaceId If workspaceId is provided, the dialog will select the workspace for editing.
   */
  public async manageWorkspaceFromDialog(id?: TWorkspaceID) {
    const targetWorkspaceID = id ?? this.getSelectedWorkspaceID();
    const result = await this.modalCtx.showWorkspacesModal(targetWorkspaceID);
    if (result === null) return;
    const { name, icon, userContextId } = result;
    const workspace = this.getRawWorkspace(targetWorkspaceID);
    if (!workspace) return;
    const newWorkspace: TWorkspace = {
      ...workspace,
      name: name as string,
      icon: icon as string,
      userContextId: Number(userContextId),
    };
    setWorkspacesDataStore("data", (prev) => {
      const temp = this.cloneWorkspaceMap(prev);
      temp.set(targetWorkspaceID, newWorkspace);
      return temp as unknown as TWorkspacesStoreData["data"];
    });
    this.tabManagerCtx.updateTabsVisibility();
    return result;
  }

  public changeWorkspace(id: TWorkspaceID) {
    this.tabManagerCtx.changeWorkspace(id);
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
        temp.set(restoredWorkspaceId, {
          ...workspace,
          icon: snapshot.workspace.icon,
          userContextId: snapshot.workspace.userContextId,
        });
      }
      return temp as unknown as TWorkspacesStoreData["data"];
    });

    await this.restoreTabsFromSnapshot(restoredWorkspaceId, snapshot.tabs);

    await this.archiveService.removeSnapshot(archiveId);

    this.changeWorkspace(restoredWorkspaceId);
    return restoredWorkspaceId;
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
      const initialUrl = tabSnapshot.url ?? "about:blank";
      const tab = globalThis.gBrowser.addTab(initialUrl, {
        skipAnimation: true,
        inBackground: true,
        userContextId: tabSnapshot.userContextId,
        triggeringPrincipal:
          Services.scriptSecurityManager.getSystemPrincipal(),
      });

      this.tabManagerCtx.setWorkspaceIdToAttribute(tab, workspaceId);
      tab.setAttribute("usercontextid", String(tabSnapshot.userContextId));

      if (tabSnapshot.lastShownWorkspaceId) {
        tab.setAttribute(
          WORKSPACE_LAST_SHOW_ID,
          tabSnapshot.lastShownWorkspaceId,
        );
      } else {
        tab.removeAttribute(WORKSPACE_LAST_SHOW_ID);
      }

      if (tabSnapshot.state) {
        try {
          const clonedState = structuredClone(tabSnapshot.state);
          const attributes =
            (clonedState.attributes as Record<string, unknown> | undefined) ??
            {};
          attributes[WORKSPACE_TAB_ATTRIBUTION_ID] = workspaceId;
          if (tabSnapshot.lastShownWorkspaceId) {
            attributes[WORKSPACE_LAST_SHOW_ID] =
              tabSnapshot.lastShownWorkspaceId;
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
      } else if (tabSnapshot.url) {
        try {
          globalThis.gBrowser.getBrowserForTab(tab).loadURI(tabSnapshot.url, {
            triggeringPrincipal:
              Services.scriptSecurityManager.getSystemPrincipal(),
          });
        } catch (error) {
          console.error("WorkspacesService: failed to load tab URL", error);
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
}
