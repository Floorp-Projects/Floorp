import { type TWorkspace, zWorkspaceID } from "./utils/type";
import {
  selectedWorkspaceID,
  setSelectedWorkspaceID,
  setWorkspacesDataStore,
  workspacesDataStore,
} from "./data/data";
import type { TWorkspaceID } from "./utils/type";
import { isRight } from "fp-ts/Either";

export interface WorkspacesDataManagerBase {
  createWorkspace(name: string): TWorkspaceID;
  deleteWorkspace(id: TWorkspaceID): void;
  setCurrentWorkspaceID(id: TWorkspaceID): void;
  setDefaultWorkspace(id: TWorkspaceID): void;
  isWorkspaceID(id: string): id is TWorkspaceID;
  getRawWorkspace(id: TWorkspaceID): TWorkspace;
  getSelectedWorkspaceID(): TWorkspaceID;
  getDefaultWorkspaceID(): TWorkspaceID;
}

export class WorkspacesDataManager implements WorkspacesDataManagerBase {
  /**
   * Returns new workspace object.
   * @param name The name of the workspace.
   * @returns The new workspace id.
   * @deprecated Use WorkspacesServices.createWorkspace
   */
  public createWorkspace(name: string): TWorkspaceID {
    const idResult = zWorkspaceID.decode(
      Services.uuid.generateUUID().toString().replaceAll(/[{}]/g, ""),
    );
    if (!isRight(idResult)) {
      throw new Error("Failed to generate valid workspace ID");
    }
    const id = idResult.right;
    const workspace: TWorkspace = {
      name,
      icon: null,
      userContextId: 0,
    };
    //TODO: If the id is duplicated, regenerate with limit to try
    setWorkspacesDataStore("data", (prev) => {
      const temp = new Map(prev);
      temp.set(id, workspace);
      return temp;
    });
    return id;
  }

  /**
   * Delete workspace by id.
   * @param workspaceId The workspace id.
   */
  public deleteWorkspace(id: TWorkspaceID): void {
    this.setCurrentWorkspaceID(workspacesDataStore.defaultID as TWorkspaceID);

    setWorkspacesDataStore("data", (prev) => {
      prev.delete(id);
      return new Map(prev);
    });
  }

  public setCurrentWorkspaceID(id: TWorkspaceID): void {
    setSelectedWorkspaceID(id);
  }

  public setDefaultWorkspace(id: TWorkspaceID): void {
    setWorkspacesDataStore("defaultID", id);
  }

  public isWorkspaceID(id: string): id is TWorkspaceID {
    // WorkspaceID の形式を検証
    const parseResult = zWorkspaceID.decode(id);
    if (!isRight(parseResult)) {
      console.warn("WorkspacesDataManager: invalid WorkspaceID format:", id);
      return false;
    }
    const parsedId = parseResult.right;
    const exists = workspacesDataStore.data.has(parsedId);
    if (!exists) {
      console.warn("WorkspacesDataManager: WorkspaceID not found:", parsedId);
    }
    return exists;
  }

  public getRawWorkspace(id: TWorkspaceID): TWorkspace | undefined {
    return workspacesDataStore.data.get(id);
  }

  public getSelectedWorkspaceID(): TWorkspaceID {
    const selected = selectedWorkspaceID();
    if (selected === null) {
      return this.getDefaultWorkspaceID();
    }

    if (this.isWorkspaceID(selected)) {
      return selected;
    }
    throw Error("Not Valid Selected Workspace ID : " + selected);
  }

  public getDefaultWorkspaceID(): TWorkspaceID {
    if (this.isWorkspaceID(workspacesDataStore.defaultID)) {
      return workspacesDataStore.defaultID;
    }
    throw Error(
      "Not Valid Default Workspace ID : " + workspacesDataStore.defaultID,
    );
  }
}
