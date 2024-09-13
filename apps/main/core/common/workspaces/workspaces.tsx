/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { workspace } from "./utils/type";
import { setworkspacesData, workspacesData } from "./data";
import { createEffect } from "solid-js";
import { WorkspacesServicesStaticNames } from "./utils/workspaces-static-names";
import { WorkspaceIcons } from "./utils/workspace-icons";

export class WorkspacesServices {
  private static instance: WorkspacesServices;
  static getInstance() {
    if (!WorkspacesServices.instance) {
      WorkspacesServices.instance = new WorkspacesServices();
    }
    return WorkspacesServices.instance;
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
  private get getGeneratedUuid(): string {
    return Services.uuid.generateUUID().toString();
  }

  /**
   * Returns current workspace UUID (id).
   * @returns The current workspace UUID (id).
   */
  public get getCurrentWorkspaceId(): string {
    return window.floorpSelectedWorkspace;
  }

  /**
   * Returns current workspace object.
   * @returns The current workspace object.
   */
  public get getCurrentWorkspace() {
    return this.getWorkspaceById(this.getCurrentWorkspaceId);
  }

  /**
   * set current workspace UUID (id).
   * @param workspaceId The workspace id.
   */
  public setCurrentWorkspaceId(workspaceId: string): void {
    window.floorpSelectedWorkspace = workspaceId;
  }

  /**
   * Returns all workspaces id.
   * @returns The all workspaces id.
   */
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
  public createWorkspace(name: string, isDefault = false): string {
    const workspace: workspace = {
      id: this.getGeneratedUuid,
      name,
      icon: null,
      emoji: null,
      color: this.getNewworkspacesServicesColor,
      isDefault,
    };
    setworkspacesData((prev) => {
      return [...prev, workspace];
    });
    this.changeWorkspace(workspace.id);
    return workspace.id;
  }

  /**
   * Returns new workspace object with default name.
   * @returns The new workspace id.
   */
  public createNoNameWorkspace = (isDefault = false): string => {
    return this.createWorkspace(
      this.l10n?.formatValueSync("workspace-new-default-name") ??
        (`New Workspaces (${workspacesData().length})` as string),
      isDefault,
    );
  };

  /**
   * Delete workspace by id.
   * @param workspaceId The workspace id.
   */
  public deleteWorkspace(workspaceId: string): void {
    setworkspacesData((prev) => {
      return prev.filter((workspace) => workspace.id !== workspaceId);
    });
  }

  /**
   * Change workspace. Selected workspace id will be stored in window object.
   * @param workspaceId The workspace id.
   */
  public changeWorkspace(workspaceId: string) {
    if (!workspaceId) {
      throw new Error("Workspace id is required to change workspace");
    }
    const selectedWorkspace = this.getCurrentWorkspaceId;
    this.setCurrentWorkspaceId(workspaceId);
    setworkspacesData((prev) => {
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
    this.changeWorkspaceToolbarState(workspaceId);
    this.checkTabsVisibility();
  }

  /**
   * Change workspace. Selected workspace id will be stored in window object.
   * @param workspaceId The workspace id.
   */
  public changeWorkspaceToolbarState(workspaceId = this.getCurrentWorkspaceId) {
    const gWorkspaceIcons = WorkspaceIcons.getInstance();
    const targetToolbarItem = document?.querySelector(
      "#workspaces-toolbar-button",
    ) as XULElement;

    const workspace = this.getWorkspaceById(workspaceId);
    targetToolbarItem?.setAttribute("label", workspace.name);
    (document?.documentElement as XULElement).style.setProperty(
      "--workspaces-toolbar-button-icon",
      `url(${gWorkspaceIcons.getWorkspaceIconUrl(workspace.icon)})`,
    );
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
  public getDefaultWorkspaceId() {
    return workspacesData().find((workspace) => workspace.isDefault)?.id;
  }

  /**
   * Open manage workspace dialog. This function should not be called directly on Preferences page.
   * @param workspaceId If workspaceId is provided, the dialog will select the workspace for editing.
   */
  public manageWorkspaceFromDialog(workspaceId: string) {
    console.log("manageWorkspaceFromDialog");
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
    setworkspacesData((prev) => {
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
    setworkspacesData(workspaces);
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
    setworkspacesData(workspaces);
  }

  /**
   * Move tabs to workspace from tab context menu.
   * @param workspaceId The workspace id.
   */
  public moveTabsToWorkspaceFromTabContextMenu(workspaceId: string) {
    console.log("moveTabsToWorkspaceFromTabContextMenu");
  }

  /* tab attribute */
  /**
   * Get workspaceId from tab attribute.
   * @param tab The tab.
   * @returns The workspace id.
   */
  getWorkspaceIdFromAttribute(tab: XULElement) {
    const workspaceId = tab.getAttribute(
      WorkspacesServicesStaticNames.workspacesTabAttributionId,
    );
    return workspaceId;
  }

  /**
   * Set workspaceId to tab attribute.
   * @param tab The tab.
   * @param workspaceId The workspace id.
   */
  setWorkspaceIdToAttribute(tab: XULElement, workspaceId: string) {
    tab.setAttribute(
      WorkspacesServicesStaticNames.workspacesTabAttributionId,
      workspaceId,
    );
  }

  /**
   * Check Tabs visibility.
   */
  public checkTabsVisibility() {
    // Get Current Workspace & Workspace Id
    const currentWorkspaceId = window.floorpSelectedWorkspace;

    // Last Show Workspace Attribute
    const selectedTab = window.gBrowser.selectedTab;
    if (
      selectedTab &&
      !selectedTab.hasAttribute(
        WorkspacesServicesStaticNames.workspaceLastShowId,
      ) &&
      selectedTab.getAttribute(
        WorkspacesServicesStaticNames.workspacesTabAttributionId,
      ) === currentWorkspaceId
    ) {
      const lastShowWorkspaceTabs = document?.querySelectorAll(
        `[${WorkspacesServicesStaticNames.workspaceLastShowId}="${currentWorkspaceId}"]`,
      );

      if (lastShowWorkspaceTabs) {
        for (const lastShowWorkspaceTab of lastShowWorkspaceTabs) {
          lastShowWorkspaceTab.removeAttribute(
            WorkspacesServicesStaticNames.workspaceLastShowId,
          );
        }
      }

      selectedTab.setAttribute(
        WorkspacesServicesStaticNames.workspaceLastShowId,
        currentWorkspaceId,
      );
    }

    // Check Tabs visibility
    const tabs = window.gBrowser.tabs;
    for (const tab of tabs) {
      // Set workspaceId if workspaceId is null
      const workspaceId = this.getWorkspaceIdFromAttribute(tab);
      if (
        !(
          workspaceId !== "" &&
          workspaceId !== null &&
          workspaceId !== undefined
        )
      ) {
        this.setWorkspaceIdToAttribute(tab, currentWorkspaceId);
      }

      const chackedWorkspaceId = this.getWorkspaceIdFromAttribute(tab);
      if (chackedWorkspaceId === currentWorkspaceId) {
        window.gBrowser.showTab(tab);
      } else {
        window.gBrowser.hideTab(tab);
      }
    }
  }

  constructor() {
    createEffect(() => {
      // Check if workspaces data is empty, if so, create default workspace.
      if (!workspacesData().length) {
        this.createNoNameWorkspace(true);
      }

      // Set default workspace id
      if (!this.getCurrentWorkspaceId) {
        console.log("Set default workspace id");
        this.setCurrentWorkspaceId(workspacesData()[0].id);
        this.changeWorkspace(this.getCurrentWorkspaceId);
      }

      // Check Tabs visibility
      this.checkTabsVisibility();
    });
    this.changeWorkspace(this.getCurrentWorkspaceId);
  }
}
