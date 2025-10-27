import { onCleanup } from "solid-js";
import { setWorkspacesDataStore, workspacesDataStore } from "./data/data";
import { TWorkspace, TWorkspaceID } from "./utils/type";
import {
  WorkspacesDataManager,
  WorkspacesDataManagerBase,
} from "./workspacesDataManagerBase";
import { WorkspacesTabManager } from "./workspacesTabManager";
import { WorkspaceIcons } from "./utils/workspace-icons";
import { WorkspaceManageModal } from "./workspace-modal";
import i18next from "i18next";

export class WorkspacesService implements WorkspacesDataManagerBase {
  dataManagerCtx: WorkspacesDataManager;
  tabManagerCtx: WorkspacesTabManager;
  iconCtx: WorkspaceIcons;
  modalCtx: WorkspaceManageModal;

  constructor(
    tabManagerCtx: WorkspacesTabManager,
    iconCtx: WorkspaceIcons,
    dataManagerCtx: WorkspacesDataManager,
  ) {
    this.tabManagerCtx = tabManagerCtx;
    this.iconCtx = iconCtx;
    this.dataManagerCtx = dataManagerCtx;
    this.modalCtx = new WorkspaceManageModal(this, this.iconCtx);
    // deno-lint-ignore no-explicit-any
    (globalThis as any).workspacesFuncs = {
      createNoNameWorkspace: this.createNoNameWorkspace.bind(this),
      changeWorkspaceToNext: this.changeWorkspaceToNext.bind(this),
      changeWorkspaceToPrevious: this.changeWorkspaceToPrevious.bind(this),
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
    const count = workspacesDataStore.data.size;
    const workspaceName = i18next.t("workspaces.service.new-workspace", {
      count,
    });

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
      const temp = new Map(prev);
      temp.set(targetWorkspaceID, newWorkspace);
      return temp;
    });
    this.tabManagerCtx.updateTabsVisibility();
    return result;
  }

  public changeWorkspace(id: TWorkspaceID) {
    this.tabManagerCtx.changeWorkspace(id);
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
