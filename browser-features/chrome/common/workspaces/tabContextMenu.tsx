/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { render } from "@nora/solid-xul";
import type { WorkspacesService } from "./workspacesService.ts";
import { workspacesDataStore } from "./data/data.ts";
import i18next from "i18next";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";

const translationKeys = {
  moveTabToAnotherWorkspace: "workspaces.menu.moveTabToAnotherWorkspace",
  invalidWorkspaceID: "workspaces.error.invalidWorkspaceID",
};

const getTranslatedText = (key: string): string => {
  return i18next.t(key);
};

export class WorkspacesTabContextMenu {
  ctx: WorkspacesService;
  constructor(ctx: WorkspacesService) {
    this.ctx = ctx;
    const parentElem = document?.getElementById("tabContextMenu");
    if (!parentElem) {
      console.error(
        "[WorkspacesTabContextMenu] #tabContextMenu not found; skip menu injection.",
      );
      return;
    }

    const marker = document?.getElementById("context_moveTabOptions");
    if (!marker) {
      console.warn(
        "[WorkspacesTabContextMenu] #context_moveTabOptions not found; menu will be appended at the end.",
      );
    } else if (marker.parentElement !== parentElem) {
      console.warn(
        "[WorkspacesTabContextMenu] Marker is not a child of #tabContextMenu; menu will be appended at the end.",
      );
    }

    try {
      render(() => this.contextMenu(), parentElem, {
        marker: marker?.parentElement === parentElem ? marker : undefined,
      });
    } catch (error) {
      const reason = error instanceof Error ? error : new Error(String(error));
      console.error(
        "[WorkspacesTabContextMenu] Failed to render context menu.",
        reason,
      );
    }

    addI18nObserver(() => {
      this.updateContextMenu();
    });
  }

  private updateContextMenu() {
    const menuElem = document?.getElementById(
      "context_MoveTabToOtherWorkspace",
    );
    if (menuElem) {
      menuElem.setAttribute(
        "label",
        getTranslatedText(translationKeys.moveTabToAnotherWorkspace),
      );
    }
  }

  public contextMenu() {
    return (
      <xul:menu
        id="context_MoveTabToOtherWorkspace"
        label={getTranslatedText(translationKeys.moveTabToAnotherWorkspace)}
        accesskey="D"
      >
        <xul:menupopup
          id="WorkspacesTabContextMenu"
          onPopupShowing={() => this.createTabworkspacesContextMenuItems()}
        />
      </xul:menu>
    );
  }

  public createTabworkspacesContextMenuItems() {
    const menuElem = document?.getElementById("WorkspacesTabContextMenu");
    if (!menuElem) {
      console.error(
        "[WorkspacesTabContextMenu] #WorkspacesTabContextMenu not found; skip context menu item creation.",
      );
      return;
    }
    while (menuElem?.firstChild) {
      const child = menuElem.firstChild as XULElement;
      child.remove();
    }

    //create context menu
    const tabWorkspaceId = this.ctx.tabManagerCtx.getWorkspaceIdFromAttribute(
      window.TabContextMenu.contextTab,
    );

    const excludeHasTabWorkspaceIdWorkspaces = workspacesDataStore.order.filter(
      (w) => w !== tabWorkspaceId,
    );

    for (const workspaceId of excludeHasTabWorkspaceIdWorkspaces) {
      if (!this.ctx.isWorkspaceID(workspaceId)) {
        console.error(
          `${getTranslatedText(translationKeys.invalidWorkspaceID)}: ${workspaceId}`,
        );
        continue;
      }
      const workspace = this.ctx.getRawWorkspace(workspaceId);
      if (!workspace) {
        continue;
      }
      const menuitem = document?.createXULElement?.("menuitem") as
        | XULElement
        | undefined;
      if (!menuitem) {
        continue;
      }
      menuitem.classList.add("menuitem-iconic");
      menuitem.setAttribute("label", workspace.name);
      const iconUrl = this.ctx.iconCtx.getWorkspaceIconUrl(workspace.icon);
      if (iconUrl) {
        menuitem.setAttribute("image", iconUrl);
      } else {
        menuitem.removeAttribute("image");
      }
      menuitem.addEventListener("command", () => {
        this.ctx.tabManagerCtx.moveTabsToWorkspaceFromTabContextMenu(
          workspaceId,
        );
      });
      menuElem.appendChild(menuitem);
    }
  }
}
