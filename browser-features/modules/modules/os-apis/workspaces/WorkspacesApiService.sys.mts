/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * WorkspacesApiService - API service for workspace operations
 *
 * This module provides workspace management capabilities to the OS Server API.
 * It accesses the workspacesDataStore and workspacesFuncs from the chrome context.
 */

export interface WorkspaceInfo {
  id: string;
  name: string;
  icon: string | null;
  userContextId: number;
  isDefault: boolean;
  isSelected: boolean;
}

export interface WorkspacesApiServiceType {
  listWorkspaces(): WorkspaceInfo[];
  getCurrentWorkspaceId(): string | null;
  switchToWorkspace(id: string): boolean;
  switchToNext(): boolean;
  switchToPrevious(): boolean;
}

/**
 * Get the main browser window
 */
function getMainWindow():
  | (typeof window & { workspacesFuncs?: WorkspacesFuncs })
  | null {
  const windows = Services.wm.getEnumerator("navigator:browser");
  while (windows.hasMoreElements()) {
    const win = windows.getNext() as typeof window;
    if (win && !win.closed) {
      return win as typeof window & { workspacesFuncs?: WorkspacesFuncs };
    }
  }
  return null;
}

interface WorkspacesFuncs {
  createNoNameWorkspace: () => string;
  changeWorkspaceToNext: () => void;
  changeWorkspaceToPrevious: () => void;
  getSelectedWorkspaceID: () => string;
  changeWorkspace: (id: string) => void;
  isWorkspaceID: (id: string) => boolean;
}

/**
 * Get workspace data from preferences
 */
function getWorkspaceData(): {
  data: Map<
    string,
    {
      name: string;
      icon?: string | null;
      userContextId: number;
      isDefault?: boolean;
      isSelected?: boolean;
    }
  >;
  order: string[];
  defaultID: string;
} | null {
  const WORKSPACE_DATA_PREF_NAME = "floorp.workspaces.v4.store";
  try {
    const prefValue = Services.prefs.getStringPref(
      WORKSPACE_DATA_PREF_NAME,
      "{}",
    );
    const parsed = JSON.parse(prefValue, (k, v) =>
      k === "data" ? new Map(v) : v,
    );
    if (parsed && parsed.data) {
      return parsed;
    }
  } catch (e) {
    console.error("[WorkspacesApiService] Failed to parse workspace data:", e);
  }
  return null;
}

/**
 * Get the selected workspace ID from the main window
 */
function getSelectedWorkspaceIdFromWindow(): string | null {
  const win = getMainWindow();
  if (!win) return null;

  try {
    if (
      win.workspacesFuncs &&
      typeof win.workspacesFuncs.getSelectedWorkspaceID === "function"
    ) {
      return win.workspacesFuncs.getSelectedWorkspaceID();
    }
  } catch (e) {
    console.error(
      "[WorkspacesApiService] Failed to get selected workspace ID:",
      e,
    );
  }
  return null;
}

export const WorkspacesApiService: WorkspacesApiServiceType = {
  /**
   * Get list of all workspaces
   */
  listWorkspaces(): WorkspaceInfo[] {
    const wsData = getWorkspaceData();
    if (!wsData) return [];

    const currentId = getSelectedWorkspaceIdFromWindow();
    const workspaces: WorkspaceInfo[] = [];

    for (const id of wsData.order) {
      const ws = wsData.data.get(id);
      if (ws) {
        workspaces.push({
          id,
          name: ws.name,
          icon: ws.icon ?? null,
          userContextId: ws.userContextId,
          isDefault: id === wsData.defaultID,
          isSelected: id === currentId,
        });
      }
    }

    return workspaces;
  },

  /**
   * Get the current (selected) workspace ID
   */
  getCurrentWorkspaceId(): string | null {
    return getSelectedWorkspaceIdFromWindow();
  },

  /**
   * Switch to a specific workspace by ID
   */
  switchToWorkspace(id: string): boolean {
    const win = getMainWindow();
    if (!win) {
      console.error("[WorkspacesApiService] No browser window available");
      return false;
    }

    try {
      if (
        win.workspacesFuncs &&
        typeof win.workspacesFuncs.changeWorkspace === "function"
      ) {
        // Validate the workspace ID exists
        if (
          typeof win.workspacesFuncs.isWorkspaceID === "function" &&
          !win.workspacesFuncs.isWorkspaceID(id)
        ) {
          console.error("[WorkspacesApiService] Invalid workspace ID:", id);
          return false;
        }
        win.workspacesFuncs.changeWorkspace(id);
        return true;
      }
    } catch (e) {
      console.error("[WorkspacesApiService] Failed to switch workspace:", e);
    }
    return false;
  },

  /**
   * Switch to the next workspace
   */
  switchToNext(): boolean {
    const win = getMainWindow();
    if (!win) {
      console.error("[WorkspacesApiService] No browser window available");
      return false;
    }

    try {
      if (
        win.workspacesFuncs &&
        typeof win.workspacesFuncs.changeWorkspaceToNext === "function"
      ) {
        win.workspacesFuncs.changeWorkspaceToNext();
        return true;
      }
    } catch (e) {
      console.error(
        "[WorkspacesApiService] Failed to switch to next workspace:",
        e,
      );
    }
    return false;
  },

  /**
   * Switch to the previous workspace
   */
  switchToPrevious(): boolean {
    const win = getMainWindow();
    if (!win) {
      console.error("[WorkspacesApiService] No browser window available");
      return false;
    }

    try {
      if (
        win.workspacesFuncs &&
        typeof win.workspacesFuncs.changeWorkspaceToPrevious === "function"
      ) {
        win.workspacesFuncs.changeWorkspaceToPrevious();
        return true;
      }
    } catch (e) {
      console.error(
        "[WorkspacesApiService] Failed to switch to previous workspace:",
        e,
      );
    }
    return false;
  },
};
