/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { CPanelSidebar } from "./panel-sidebar.tsx";
import { createSignal, Show } from "solid-js";
import type { Panel } from "../utils/type.ts";
import { ContextMenuUtils } from "#features-chrome/utils/context-menu.tsx";
import { createRoot, getOwner, type Owner, runWithOwner } from "solid-js";
import i18next from "i18next";

const tr = (k: string) => (i18next as any).t(k);

export const [contextPanel, setContextPanel] = createSignal<Panel | null>(null);

export class SidebarContextMenuElem {
  ctx: CPanelSidebar;
  constructor(ctx: CPanelSidebar) {
    this.ctx = ctx;
    const owner: Owner | null = getOwner();
    const exec = () =>
      ContextMenuUtils.addToolbarContentMenuPopupSet(() =>
        this.sidebarContextMenu(),
      );
    if (owner) runWithOwner(owner, exec);
    else createRoot(exec);
  }

  public contextPanelId: string | null = null;

  private getPanelByOriginalTarget(target: XULElement | null) {
    if (!target) {
      return;
    }

    let currentElement: Element | null = target as unknown as Element;
    let panelId: string | undefined;

    for (let i = 0; i < 10 && currentElement && !panelId; i++) {
      panelId = currentElement.getAttribute("data-panel-id") || undefined;

      if (!panelId && currentElement.parentElement) {
        currentElement = currentElement.parentElement;
      }
    }

    if (!panelId) {
      return;
    }

    const gPanelSidebar = this.ctx;
    return gPanelSidebar.getPanelData(panelId);
  }

  private handlePopupShowing(e: Event) {
    if (!e.explicitOriginalTarget) {
      return;
    }

    const panel = this.getPanelByOriginalTarget(
      e.explicitOriginalTarget as XULElement,
    );

    if (!panel) {
      return;
    }

    setContextPanel(panel);
  }

  private handlePopupHiding() {
    setTimeout(() => {
      setContextPanel(null);
    }, 0);
  }

  private safeExecuteCommand(callback: () => void) {
    try {
      callback();
    } catch (error) {
      console.error("Command execution error:", error);
    } finally {
      if (typeof document !== "undefined" && document) {
        const contextMenu = document.getElementById("webpanel-context");
        if (contextMenu) {
          // @ts-ignore - Fix type error
          contextMenu.hidePopup();
        }
      }
    }
  }

  private handleUnloadCommand() {
    const gPanelSidebar = this.ctx;
    const panelId = contextPanel()?.id;
    if (panelId) {
      this.safeExecuteCommand(() => {
        gPanelSidebar.unloadPanel(panelId);
      });
    }
  }

  private handleDeleteCommand() {
    const gPanelSidebar = this.ctx;
    const panelId = contextPanel()?.id;
    if (panelId) {
      this.safeExecuteCommand(() => {
        gPanelSidebar.deletePanel(panelId);
      });
    }
  }

  private handleMuteCommand() {
    const gPanelSidebar = this.ctx;
    const panelId = contextPanel()?.id;
    if (panelId) {
      this.safeExecuteCommand(() => {
        gPanelSidebar.mutePanel(panelId);
      });
    }
  }

  private handleChangeZoomLevelCommand(type: "in" | "out" | "reset") {
    const gPanelSidebar = this.ctx;
    const panelId = contextPanel()?.id;
    if (panelId) {
      this.safeExecuteCommand(() => {
        gPanelSidebar.changeZoomLevel(panelId, type);
      });
    }
  }

  private handleChangeUserAgentCommand() {
    const gPanelSidebar = this.ctx;
    const panelId = contextPanel()?.id;
    if (panelId) {
      this.safeExecuteCommand(() => {
        gPanelSidebar.changeUserAgent(panelId);
      });
    }
  }

  private sidebarContextMenu() {
    return (
      <xul:popupset>
        <xul:menupopup
          id="webpanel-context"
          onPopupShowing={(e) => this.handlePopupShowing(e)}
          // onPopupHiding={() => this.handlePopupHiding()}
        >
          <xul:menuitem
            id="unloadWebpanelMenu"
            class="needLoadedWebpanel"
            label={tr("panelSidebar.contextMenu.unload")}
            accesskey="U"
            onCommand={() => this.handleUnloadCommand()}
          />
          <Show when={contextPanel()?.type === "web"}>
            <xul:menuseparator class="context-webpanel-separator" />
            <xul:menuitem
              id="muteMenu"
              class="needLoadedWebpanel"
              label={tr("panelSidebar.contextMenu.mute")}
              accesskey="M"
              onCommand={() => this.handleMuteCommand()}
            />
            <xul:menu
              id="changeZoomLevelMenu"
              class="needLoadedWebpanel needRunningExtensionsPanel"
              label={tr("panelSidebar.contextMenu.changeZoom")}
              accesskey="Z"
            >
              <xul:menupopup id="changeZoomLevelPopup">
                <xul:menuitem
                  id="zoomInMenu"
                  label={tr("panelSidebar.contextMenu.zoomIn")}
                  accesskey="I"
                  onCommand={() => this.handleChangeZoomLevelCommand("in")}
                />
                <xul:menuitem
                  id="zoomOutMenu"
                  label={tr("panelSidebar.contextMenu.zoomOut")}
                  accesskey="O"
                  onCommand={() => this.handleChangeZoomLevelCommand("out")}
                />
                <xul:menuitem
                  id="resetZoomMenu"
                  label={tr("panelSidebar.contextMenu.resetZoom")}
                  accesskey="R"
                  onCommand={() => this.handleChangeZoomLevelCommand("reset")}
                />
              </xul:menupopup>
            </xul:menu>
            <xul:menuitem
              id="changeUAWebpanelMenu"
              label={tr("panelSidebar.contextMenu.changeUA")}
              accesskey="R"
              onCommand={() => this.handleChangeUserAgentCommand()}
            />
          </Show>
          <xul:menuseparator class="context-webpanel-separator" />
          <xul:menuitem
            id="deleteWebpanelMenu"
            label={tr("panelSidebar.contextMenu.delete")}
            accesskey="D"
            onCommand={() => this.handleDeleteCommand()}
          />
        </xul:menupopup>
      </xul:popupset>
    );
  }
}
