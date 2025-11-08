/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ContextMenuUtils } from "#features-chrome/utils/context-menu.tsx";
import { createRoot, getOwner, type Owner, runWithOwner } from "solid-js";
import type { WorkspacesService } from "../workspacesService.ts";
import { ContextMenu } from "./contextMenu.tsx";
import { createSignal, Show } from "solid-js";
import type { TWorkspaceID } from "../utils/type.ts";

export class WorkspacesPopupContextMenu {
  ctx: WorkspacesService;
  constructor(ctx: WorkspacesService) {
    this.ctx = ctx;
    const owner: Owner | null = getOwner();
    const exec = () =>
      ContextMenuUtils.addToolbarContentMenuPopupSet(() => this.PopupSet());
    if (owner) runWithOwner(owner, exec);
    else createRoot(exec);
  }
  contextWorkspaceID: TWorkspaceID | null = null;
  needDisableBefore = false;
  needDisableAfter = false;
  /**
   * Create context menu items for workspaces.
   * @param event The event.
   * @returns The context menu items.
   */
  private createWorkspacesContextMenuItems(event: Event) {
    //delete already exsist items
    const menuElem = document?.getElementById(
      "workspaces-toolbar-item-context-menu",
    );
    while (menuElem?.firstChild) {
      const firstChild = menuElem.firstChild as XULElement;
      firstChild.remove();
    }

    const eventTargetElement = event.explicitOriginalTarget as XULElement;
    const contextWorkspaceId = eventTargetElement.id.replace("workspace-", "");
    if (this.ctx.isWorkspaceID(contextWorkspaceId)) {
      this.contextWorkspaceID = contextWorkspaceId;
    }

    const beforeSiblingElem =
      eventTargetElement.previousElementSibling?.getAttribute(
        "data-workspaceId",
      ) || null;
    const afterSiblingElem =
      eventTargetElement.nextElementSibling?.getAttribute("data-workspaceId") ||
      null;

    const isAfterSiblingExist = afterSiblingElem != null;
    // Disable "move up" only if this is the first workspace (no previous sibling)
    this.needDisableBefore = beforeSiblingElem === null;
    // Disable "move down" only if this is the last workspace (no next sibling)
    this.needDisableAfter = !isAfterSiblingExist;
  }

  private PopupSet() {
    const [show, setShow] = createSignal(false);
    return (
      <xul:popupset>
        <xul:menupopup
          id="workspaces-toolbar-item-context-menu"
          onPopupShowing={(event) => {
            this.createWorkspacesContextMenuItems(event);
            setShow(true);
          }}
          onPopupHiding={() => {
            setShow(false);
          }}
        >
          <Show when={show()}>
            <ContextMenu
              disableBefore={this.needDisableBefore}
              disableAfter={this.needDisableAfter}
              contextWorkspaceId={this.contextWorkspaceID!}
              ctx={this.ctx}
            />
          </Show>
        </xul:menupopup>
      </xul:popupset>
    );
  }
}
