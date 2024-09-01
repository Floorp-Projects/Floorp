/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { getWorkspaceIconUrl } from "./utils/workspace-icons";
import { workspacesServices } from "./workspaces";

const ContextMenu = (props: {
  disableBefore: boolean;
  disableAfter: boolean;
  contextWorkspaceId: string;
}) => {
  const gWorkspacesServices = workspacesServices.getInstance();
  const { disableBefore, disableAfter, contextWorkspaceId } = props;
  return (
    <xul:menu
      id="context_MoveTabToOtherWorkspace"
      data-l10n-id="move-tab-another-workspace"
      accesskey="D"
    >
      <xul:menupopup
        id="workspacesTabContextMenu"
        onpopupshowing={createTabworkspacesServicesContextMenuItems}
      />
    </xul:menu>
  );
};

function createTabworkspacesServicesContextMenuItems() {
  const gWorkspacesServices = workspacesServices.getInstance();
  const menuElem = document?.getElementById("workspacesTabContextMenu");
  while (menuElem?.firstChild) {
    const child = menuElem.firstChild as XULElement;
    child.remove();
  }

  //create context menu
  const allworkspacesServicesId = gWorkspacesServices.getAllworkspacesServicesId;
  for (const workspaceId of allworkspacesServicesId) {
    const tabWorkspaceId = gWorkspacesServices.getWorkspaceIdFromAttribute(
      window.TabContextMenu.contextTab,
    );

    if (tabWorkspaceId === workspaceId) {
      continue;
    }

    const workspaceData = gWorkspacesServices.getWorkspaceById(workspaceId);
    const workspaceName = workspaceData.name;
    const workspaceIcon = workspaceData.icon;
    const menuItem = window.MozXULElement.parseXULToFragment(`
        <menuitem id="context_MoveTabToOtherWorkspace"
                  class="menuitem-iconic"
                  style="list-style-image: url(${getWorkspaceIconUrl(
      workspaceIcon,
    )})"
                  oncommand="gWorkspacesServices.moveTabsToWorkspaceFromTabContextMenu('${workspaceId}')"
      />`);

    menuItem.firstChild.setAttribute("label", workspaceName);
    const parentElem = document?.getElementById("workspacesTabContextMenu");
    parentElem?.appendChild(menuItem);
  }
}

export class workspacesTabContextMenu {
  private static instance: workspacesTabContextMenu;
  public static getInstance() {
    if (!workspacesTabContextMenu.instance) {
      workspacesTabContextMenu.instance = new workspacesTabContextMenu();
    }
    return workspacesTabContextMenu.instance;
  }

  public createTabworkspacesServicesContextMenuItems(event: Event) {
    console.log("createTabworkspacesServicesContextMenuItems");
  }
}
