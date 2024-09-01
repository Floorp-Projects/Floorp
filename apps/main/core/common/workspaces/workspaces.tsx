/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { workspace } from "./utils/type";
import { setWorkspaces, workspaces } from "./data";

export class Workspaces {
  private static instance: Workspaces;
  static getInstance() {
    if (!Workspaces.instance) {
      Workspaces.instance = new Workspaces();
    }
    return Workspaces.instance;
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

  get getAllWorkspacesId() {
    return workspaces().map((workspace) => workspace.id);
  }

  /**
   * Returns new workspace color.
   * @returns The new workspace color.
   */
  private get getNewWorkspacesColor(): string {
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
      color: this.getNewWorkspacesColor,
      isDefault: false,
    };
    setWorkspaces((prev) => {
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
    setWorkspaces((prev) => {
      return prev.filter((workspace) => workspace.id !== workspaceId);
    });
  }

  /**
   * Change workspace. Selected workspace id will be stored in window object.
   * @param workspaceId The workspace id.
   */
  public changeWorkspace(workspaceId: string) {
    window.floorpWorkspaeId = workspaceId;
    // useEffect will be call for re-rendering PopupElement.
    setWorkspaces((prev) => {
      return prev.map((workspace) => {
        if (workspace.id === workspaceId) {
          return { ...workspace, isSelected: true };
        }
        return { ...workspace, isSelected: false };
      });
    });
  }

  /**
   * Get selected workspace id.
   * @returns The selected workspace id.
   */
  public getWorkspaceById(workspaceId: string): workspace {
    const workspace = workspaces().find(
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
    return workspaces().find((workspace) => workspace.isDefault)?.id;
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
    setWorkspaces((prev) => {
      return prev.map((workspace) => {
        if (workspace.id === workspaceId) {
          return { ...workspace, name: newName };
        }
        return workspace;
      });
    });
  }

  /**
   * Modify Workspaces() array to reorder workspace up 1 index.
   * @param workspaceId The workspace id.
   */
  public reorderWorkspaceUp(workspaceId: string) {
    console.log("reorderWorkspaceUp");
  }

  /**
   * Modify Workspaces() array to reorder workspace down 1 index.
   * @param workspaceId The workspace id.
   */
  public reorderWorkspaceDown(workspaceId: string) {
    console.log("reorderWorkspaceDown");
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

  public contextMenu = {
    /**
     * Create context menu items for tabs in workspaces.
     * @param event The event.
     * @returns The context menu items.
     */
    createTabWorkspacesContextMenuItems: (event: Event) => {
      console.log("createTabWorkspacesContextMenuItems");
    },
  };

  constructor() {
    // Check if workspaces data is empty, if so, create default workspace.
    if (!workspaces().length) {
      this.createNoNameWorkspace();
    }
  }
}
