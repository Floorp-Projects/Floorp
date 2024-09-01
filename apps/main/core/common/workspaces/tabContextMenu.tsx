/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { render } from "@nora/solid-xul";
import { getWorkspaceIconUrl } from "./utils/workspace-icons";
import { workspacesServices } from "./workspaces";
import { For } from "solid-js";
import { workspacesData } from "./data";
import type { workspaces } from "./utils/type";

export class workspacesTabContextMenu {
  private static instance: workspacesTabContextMenu;
  public static getInstance() {
    if (!workspacesTabContextMenu.instance) {
      workspacesTabContextMenu.instance = new workspacesTabContextMenu();
    }
    return workspacesTabContextMenu.instance;
  }

  private menuItem(workspaces: workspaces) {
    const gWorkspaces = workspacesServices.getInstance();
    return (
      <For each={workspaces}>
        {(workspace) => (
          <xul:menuitem id="context_MoveTabToOtherWorkspace"
            class="menuitem-iconic"
            style={`list-style-image: url(${getWorkspaceIconUrl(workspace.icon)})`}
            oncommand={() => gWorkspaces.moveTabsToWorkspaceFromTabContextMenu(workspace.id)}
          />
        )}
      </For>
    );
  }

  public contextMenu() {
    return (
      <xul:menu id="context_MoveTabToOtherWorkspace" data-l10n-id="move-tab-another-workspace" accesskey="D">
        <xul:menupopup id="workspacesTabContextMenu"
          onpopupshowing={this.createTabworkspacesContextMenuItems} />
      </xul:menu>
    );
  }

  public createTabworkspacesContextMenuItems(this: workspacesTabContextMenu) {
    const gWorkspacesServices = workspacesServices.getInstance();
    const menuElem = document?.getElementById("workspacesTabContextMenu");
    while (menuElem?.firstChild) {
      const child = menuElem.firstChild as XULElement;
      child.remove();
    }

    console.log("contextTab", window.TabContextMenu.contextTab);

    //create context menu
    const tabWorkspaceId = gWorkspacesServices.getWorkspaceIdFromAttribute(
      window.TabContextMenu.contextTab,
    );

    const excludeHasTabWorkspaceIdWorkspaces = workspacesData().filter(
      (workspace) => workspace.id !== tabWorkspaceId,
    ) as workspaces;

    const parentElem = document?.getElementById("workspacesTabContextMenu");
    render(() => this.menuItem(excludeHasTabWorkspaceIdWorkspaces), parentElem, {
      hotCtx: import.meta.hot,
    });
  }

  constructor() {
    const parentElem = document?.getElementById("tabContextMenu");
    render(() => this.contextMenu(), parentElem, {
      hotCtx: import.meta.hot,
    });
  }
}
