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
  getRawWorkspace(id: TWorkspaceID): TWorkspace | undefined;
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

    // Selected ID is invalid: try to recover instead of throwing an exception
    console.warn(
      "WorkspacesDataManager: Not valid selected workspace ID, attempting recovery:",
      selected,
    );

    // 1) Try default workspace ID if valid
    const defaultID = workspacesDataStore.defaultID as TWorkspaceID;
    if (this.isWorkspaceID(defaultID)) {
      // Reset selected to a valid default and return it
      setSelectedWorkspaceID(defaultID);
      return defaultID;
    }

    // 2) Try to pick any existing workspace from the data map
    const firstEntry = workspacesDataStore.data.keys().next();
    if (!firstEntry.done) {
      const anyID = firstEntry.value as TWorkspaceID;
      console.info(
        "WorkspacesDataManager: falling back to existing workspace ID:",
        anyID,
      );
      // Make this the new default & selected
      setWorkspacesDataStore("defaultID", anyID);
      setSelectedWorkspaceID(anyID);
      return anyID;
    }

    // 3) No existing workspace found — create a new one and use it
    console.info(
      "WorkspacesDataManager: no existing workspaces found, creating a new default workspace",
    );
    const newID = this.createWorkspace("Default Workspace");
    this.setDefaultWorkspace(newID);
    this.setCurrentWorkspaceID(newID);
    return newID;
  }

  public getDefaultWorkspaceID(): TWorkspaceID {
    const defaultID = workspacesDataStore.defaultID as TWorkspaceID;
    if (this.isWorkspaceID(defaultID)) {
      return defaultID;
    }

    // Default ID is invalid — attempt recovery instead of throwing.
    // Throwing here would risk crashing code paths that assume a default
    // workspace exists. Recovery strategy (in order):
    //  1) reuse any existing workspace ID from the store,
    //  2) if none exist, create a new "Default Workspace" and persist it.
    // This keeps the app stable when stored IDs are stale or corrupted.
    console.warn(
      "WorkspacesDataManager: Not valid default workspace ID, attempting recovery:",
      defaultID,
    );

    // Try to pick any existing workspace
    const firstEntry = workspacesDataStore.data.keys().next();
    if (!firstEntry.done) {
      const anyID = firstEntry.value as TWorkspaceID;
      console.info(
        "WorkspacesDataManager: using existing workspace ID as default:",
        anyID,
      );
      setWorkspacesDataStore("defaultID", anyID);
      return anyID;
    }

    // No existing workspace — create one
    console.info(
      "WorkspacesDataManager: creating a new default workspace because none exist",
    );
    const newID = this.createWorkspace("Default Workspace");
    this.setDefaultWorkspace(newID);
    return newID;
  }
}
