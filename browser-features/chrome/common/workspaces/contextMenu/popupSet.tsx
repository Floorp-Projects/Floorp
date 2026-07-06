/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ContextMenuUtils } from "#features-chrome/utils/context-menu.tsx";
import { signal } from "@preact/signals";
import type { WorkspacesService } from "../workspacesService.ts";
import { ContextMenu } from "./contextMenu.tsx";
import type { TWorkspaceID } from "../utils/type.ts";

type ChromeDocument = Document & { popupNode?: Element | null };

export class WorkspacesPopupContextMenu {
  ctx: WorkspacesService;
  private show = signal(false);

  constructor(ctx: WorkspacesService) {
    this.ctx = ctx;
    ContextMenuUtils.addToolbarContentMenuPopupSet(() => this.PopupSet());
  }

  contextWorkspaceID: TWorkspaceID | null = null;
  needDisableBefore = false;
  needDisableAfter = false;

  /**
   * Create context menu items for workspaces.
   * @param event The event.
   */
  private createWorkspacesContextMenuItems(event: Event) {
    //delete already exsist items
    const menuElem = document?.getElementById(
      "workspaces-toolbar-item-context-menu",
    );
    while (menuElem?.firstChild) {
      const firstChild = menuElem.firstChild as unknown as XULElement;
      firstChild.remove();
    }

    // Use popupNode if available (set by panel sidebar), otherwise explicitOriginalTarget (toolbar)
    const chromeDoc = document as ChromeDocument;
    let eventTargetElement = (chromeDoc.popupNode ?? event.explicitOriginalTarget) as unknown as XULElement;

    // Traverse up to find the workspace div if we got a child element
    while (eventTargetElement && !eventTargetElement.id?.startsWith("workspace-")) {
      eventTargetElement = eventTargetElement.parentElement as unknown as XULElement;
    }

    // Extract workspace ID with validation
    const contextWorkspaceId = eventTargetElement?.id?.replace("workspace-", "") ?? "";
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
    this.needDisableBefore = beforeSiblingElem === null;
    this.needDisableAfter = !isAfterSiblingExist;
  }

  private PopupSet() {
    return (
      <xul:popupset>
        <xul:menupopup
          id="workspaces-toolbar-item-context-menu"
          onPopupShowing={(event) => {
            this.createWorkspacesContextMenuItems(event);
            this.show.value = true;
          }}
          onPopupHiding={() => {
            this.show.value = false;
          }}
        >
          {this.show.value && (
            <ContextMenu
              disableBefore={this.needDisableBefore}
              disableAfter={this.needDisableAfter}
              contextWorkspaceId={this.contextWorkspaceID!}
              ctx={this.ctx}
            />
          )}
        </xul:menupopup>
      </xul:popupset>
    );
  }
}
