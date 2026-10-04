/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ContextMenuUtils } from "#features-chrome/utils/context-menu.tsx";
import { signal } from "@preact/signals";
import type { WorkspacesService } from "../workspacesService.ts";
import { ContextMenu } from "./contextMenu.tsx";
import type { TWorkspaceID } from "../utils/type.ts";
import { workspacesDataStore } from "../data/data.ts";

type ChromeDocument = Document & { popupNode?: Element | null };

export class WorkspacesPopupContextMenu {
  ctx: WorkspacesService;
  private contextWorkspaceID = signal<TWorkspaceID | null>(null);

  constructor(ctx: WorkspacesService) {
    this.ctx = ctx;
    ContextMenuUtils.addToolbarContentMenuPopupSet(() => this.PopupSet());
  }

  /**
   * Create context menu items for workspaces.
   * @param event The event.
   */
  private createWorkspacesContextMenuItems(event: Event) {
    // Use popupNode if available (set by panel sidebar), otherwise explicitOriginalTarget (toolbar)
    const chromeDoc = document as ChromeDocument;
    let eventTargetElement = (chromeDoc.popupNode ??
      event.explicitOriginalTarget) as unknown as XULElement;

    // Traverse up to find the workspace div if we got a child element
    while (
      eventTargetElement && !eventTargetElement.id?.startsWith("workspace-")
    ) {
      eventTargetElement = eventTargetElement
        .parentElement as unknown as XULElement;
    }

    // Extract workspace ID with validation
    const contextWorkspaceId =
      eventTargetElement?.id?.replace("workspace-", "") ?? "";
    if (this.ctx.isWorkspaceID(contextWorkspaceId)) {
      this.contextWorkspaceID.value = contextWorkspaceId;
    } else {
      this.contextWorkspaceID.value = null;
      event.preventDefault();
    }
  }

  private PopupSet() {
    const workspaceId = this.contextWorkspaceID.value;
    const order = workspacesDataStore.order;
    const index = workspaceId === null ? -1 : order.indexOf(workspaceId);
    return (
      <xul:popupset>
        <xul:menupopup
          id="workspaces-toolbar-item-context-menu"
          onPopupShowing={(event) => {
            this.createWorkspacesContextMenuItems(event);
          }}
          onPopupHiding={() => {
            this.contextWorkspaceID.value = null;
          }}
        >
          {workspaceId !== null && (
            <ContextMenu
              disableBefore={index <= 0}
              disableAfter={index < 0 || index === order.length - 1}
              contextWorkspaceId={workspaceId}
              ctx={this.ctx}
            />
          )}
        </xul:menupopup>
      </xul:popupset>
    );
  }
}
