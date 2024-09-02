/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { workspace } from "./utils/type";
import { setworkspacesServices, workspacesData } from "./data";

export class workspacesServices {
  private static instance: workspacesServices;
  static getInstance() {
    if (!workspacesServices.instance) {
      workspacesServices.instance = new workspacesServices();
    }
    return workspacesServices.instance;
  }

  /**
   * Returns the localization object.
   * @returns The localization object.
   */
  private get l10n(): Localization {
    const l10n = new Localization(
      ["browser/floorp.ftl", "branding/brand.ftl"],
      true,
    );
    return l10n;
  }

  /**
   * Returns new workspace UUID (id).
   * @returns The new workspace UUID (id).
   */
  private getGeneratedUuid(): string {
    return Services.uuid.generateUUID().toString();
  }

  get getAllworkspacesServicesId() {
    return workspacesData().map((workspace) => workspace.id);
  }

  /**
   * Returns new workspace color.
   * @returns The new workspace color.
   */
  private get getNewworkspacesServicesColor(): string {
    return "blue";
  }

  /**
   * Returns new workspace object.
   * @param name The name of the workspace.
   * @returns The new workspace id.
   */
  public createWorkspace(name: string): string {
    const workspace: workspace = {
      id: this.getGeneratedUuid(),
      name,
      icon: null,
      emoji: null,
      color: this.getNewworkspacesServicesColor,
      isDefault: false,
    };
    setworkspacesServices((prev) => {
      return [...prev, workspace];
    });
    return workspace.id;
  }

  /**
   * Returns new workspace object with default name.
   * @returns The new workspace id.
   */
  public createNoNameWorkspace = (): string => {
    return this.createWorkspace(
      this.l10n?.formatValueSync("workspace-new-default-name") ??
      ("新しいワークスペース" as string),
    );
  };

  /**
   * Delete workspace by id.
   * @param workspaceId The workspace id.
   */
  public deleteWorkspace(workspaceId: string): void {
    setworkspacesServices((prev) => {
      return prev.filter((workspace) => workspace.id !== workspaceId);
    });
  }

  /**
   * Change workspace. Selected workspace id will be stored in window object.
   * @param workspaceId The workspace id.
   */
  public changeWorkspace(workspaceId: string) {
    const selectedWorkspace = window.floorpWorkspaeId;
    window.floorpWorkspaeId = workspaceId;
    setworkspacesServices((prev) => {
      return prev.map((workspace) => {
        if (workspace.id === workspaceId) {
          return { ...workspace, isSelected: true };
        }
        if (workspace.id === selectedWorkspace) {
          return { ...workspace, isSelected: false };
        }
        return workspace;
      });
    });
  }

  /**
   * Get selected workspace id.
   * @returns The selected workspace id.
   */
  public getWorkspaceById(workspaceId: string): workspace {
    const workspace = workspacesData().find(
      (workspace) => workspace.id === workspaceId,
    );
    if (!workspace) {
      throw new Error(`Workspace with id ${workspaceId} not found`);
    }
    return workspace;
  }

  /**
   * Get default workspace id.
   * @returns The default workspace id.
   */
  getDefaultWorkspaceId() {
    return workspacesData().find((workspace) => workspace.isDefault)?.id;
  }

  /**
   * Open manage workspace dialog. This function should not be called directly on Preferences page.
   * @param workspaceId If workspaceId is provided, the dialog will select the workspace for editing.
   */
  public manageWorkspaceFromDialog(workspaceId: string) {
    let parentWindow = window as WindowProxy | null;
    const object = { workspaceId };
    if (
      parentWindow?.document?.documentURI ===
      "chrome://floorp/content/hiddenWindowMac.xhtml"
    ) {
      parentWindow = null;
    }
    if (parentWindow?.gDialogBox) {
      parentWindow.gDialogBox.open(
        "chrome://floorp/content/preferences/dialogs/manageWorkspace.xhtml",
        object,
      );
    } else {
      Services.ww.openWindow(
        parentWindow as mozIDOMWindowProxy,
        "chrome://floorp/content/preferences/dialogs/manageWorkspace.xhtml",
        "",
        "chrome,titlebar,dialog,centerscreen,modal",
        object as nsISupports,
      );
    }
  }

  /**
   * Open rename workspace dialog.
   * @param workspaceId Rename target workspace id.
   */
  public renameWorkspaceWithCreatePrompt(workspaceId: string) {
    const prompts = Services.prompt;
    const workspace = this.getWorkspaceById(workspaceId);
    const input = { value: workspace.name };
    const result = prompts.prompt(
      window as mozIDOMWindow,
      this.l10n.formatValueSync("rename-workspace-prompt-title") ?? "Rename",
      this.l10n.formatValueSync("rename-workspace-prompt-text") ?? "Name",
      input,
      "",
      { value: true },
    );

    if (result) {
      this.renameWorkspace(workspaceId, input.value);
    }
  }

  /**
   * Rename workspace.
   * @param workspaceId The workspace id.
   * @param newName The new name.
   */
  public renameWorkspace(workspaceId: string, newName: string) {
    setworkspacesServices((prev) => {
      return prev.map((workspace) => {
        if (workspace.id === workspaceId) {
          return { ...workspace, name: newName };
        }
        return workspace;
      });
    });
  }

  /**
   * Reorders a workspace to before one
   * @param workspaceId The workspace id.
   */
  public reorderWorkspaceUp(workspaceId: string) {
    const workspaces = workspacesData();
    const workspaceIndex = workspaces.findIndex(
      (workspace) => workspace.id === workspaceId,
    );
    if (workspaceIndex === 0) {
      throw new Error(`Workspace with id ${workspaceId} is already first`);
    }
    const workspace = workspaces[workspaceIndex];
    workspaces[workspaceIndex] = workspaces[workspaceIndex - 1];
    workspaces[workspaceIndex - 1] = workspace;
    setworkspacesServices(workspaces);
  }

  /**
   * Reorders a workspace to after one
   * @param workspaceId The workspace id.
   */
  public reorderWorkspaceDown(workspaceId: string) {
    const workspaces = workspacesData();
    const workspaceIndex = workspaces.findIndex(
      (workspace) => workspace.id === workspaceId,
    );
    if (workspaceIndex === workspaces.length - 1) {
      throw new Error(`Workspace with id ${workspaceId} is already last`);
    }
    const workspace = workspaces[workspaceIndex];
    workspaces[workspaceIndex] = workspaces[workspaceIndex + 1];
    workspaces[workspaceIndex + 1] = workspace;
    setworkspacesServices(workspaces);
  }

  /**
   * Get Workspace id from tab attribute.
   * @param tab The tab element.
   * @returns The workspace id.
   */
  public getWorkspaceIdFromAttribute(tab: XULElement): string {
    console.log("getWorkspaceIdFromAttribute");
    return "";
  }

  /**
   * Move tabs to workspace from tab context menu.
   * @param workspaceId The workspace id.
   */
  public moveTabsToWorkspaceFromTabContextMenu(workspaceId: string) {
    console.log("moveTabsToWorkspaceFromTabContextMenu");
  }

  constructor() {
    // Check if workspaces data is empty, if so, create default workspace.
    if (!workspacesData().length) {
      this.createNoNameWorkspace();
    }
  }
}
