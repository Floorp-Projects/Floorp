/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { setWorkspaces } from "../workspaces-data";
import type { workspace } from "./type";

class WorkspacesServiceClass {
  private static instance: WorkspacesServiceClass;
  static getInstance() {
    if (!WorkspacesServiceClass.instance) {
      WorkspacesServiceClass.instance = new WorkspacesServiceClass();
    }
    return WorkspacesServiceClass.instance;
  }

  /**
   * Returns the workspace data preference name.
   *
   * @returns {string} The workspace data preference name.
   */
  get workspaceDataPrefName(): string {
    return "floorp.workspaces.v3.data";
  }

  /**
   * Returns the attribution ID for the workspaces tab.
   *
   * @returns {string} The attribution ID for the workspaces tab.
   */
  get workspacesTabAttributionId(): string {
    return "floorpWorkspaceId";
  }

  /**
   * Returns the last show ID for the workspace.
   *
   * @returns {string} The last show ID for the workspace.
   */
  get workspaceLastShowId(): string {
    return "floorpWorkspaceLastShowId";
  }

  /**
   * Returns new workspace UUID (id).
   *
   * @returns {string} The new workspace UUID (id).
   */
  private getGeneratedUuid(): string {
    return Services.uuid.generateUUID().toString();
  }

  //TODO: Implement this method
  /**
   * Returns new workspace color.
   *
   * @returns {string} The new workspace color.
   */
  private get getWorkspacesColor(): string {
    return "blue";
  }

  //TODO: Implement this method
  /**
   * Returns new workspace object.
   *
   * @param {string} name The name of the workspace.
   * @returns {workspace} The new workspace object.
   */
  public createWorkspace(name: string): workspace {
    const workspace: workspace = {
      id: this.getGeneratedUuid(),
      name,
      icon: null,
      emoji: null,
      color: this.getWorkspacesColor,
    };

    setWorkspaces((prev) => {
      const workspaces = prev.workspaces;
      workspaces.push(workspace);
      return { ...prev, workspaces };
    });

    return workspace;
  }
}

export const WorkspacesService = WorkspacesServiceClass.getInstance();
