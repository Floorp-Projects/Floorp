/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { render } from "@nora/solid-xul";
import { CPanelSidebar } from "./panel-sidebar";
import { createSignal, Show } from "solid-js";
import type { Panel } from "../utils/type";

export const [contextPanel, setContextPanel] = createSignal<Panel | null>(null);

export class SidebarContextMenuElem {

  ctx: CPanelSidebar
  constructor(ctx:CPanelSidebar) {
    this.ctx = ctx;
    const parentElem = document?.body;
    const beforeElem = document?.getElementById("window-modal-dialog");
    render(() => this.sidebarContextMenu(), parentElem, {
      marker: beforeElem as XULElement,
      // biome-ignore lint/suspicious/noExplicitAny: <explanation>
      hotCtx: (import.meta as any).hot,
    });
  }

  public contextPanelId: string | null = null;

  private getPanelByOriginalTarget(target: XULElement | null) {
    if (!target) {
      return;
    }

    // biome-ignore lint/suspicious/noExplicitAny: <explanation>
    const panelId = (target.dataset as any).panelId;
    const gPanelSidebar = this.ctx;
    return gPanelSidebar.getPanelData(panelId);
  }

  private handlePopupShowing(e: Event) {
    const panel = this.getPanelByOriginalTarget(
      e.explicitOriginalTarget as XULElement,
    );
    if (!panel) {
      return;
    }

    setContextPanel(panel);
  }

  private handlePopupHiding() {
    setContextPanel(null);
  }

  private handleUnloadCommand() {
    const gPanelSidebar = this.ctx;
    const panelId = contextPanel()?.id;
    if (panelId) {
      gPanelSidebar.unloadPanel(panelId);
    }
  }

  private handleDeleteCommand() {
    const gPanelSidebar = this.ctx;
    const panelId = contextPanel()?.id;
    if (panelId) {
      gPanelSidebar.deletePanel(panelId);
    }
  }

  private handleMuteCommand() {
    const gPanelSidebar = this.ctx;
    const panelId = contextPanel()?.id;
    if (panelId) {
      gPanelSidebar.mutePanel(panelId);
    }
  }

  private handleChangeZoomLevelCommand(type: "in" | "out" | "reset") {
    const gPanelSidebar = this.ctx;
    const panelId = contextPanel()?.id;
    if (panelId) {
      gPanelSidebar.changeZoomLevel(panelId, type);
    }
  }

  private sidebarContextMenu() {
    const gPanelSidebar = this.ctx;
    return (
      <xul:popupset>
        <xul:menupopup
          id="panel-sidebar-panel-context"
          onPopupShowing={(e) => this.handlePopupShowing(e)}
          onPopupHiding={() => this.handlePopupHiding()}
        >
          <xul:menuitem
            id="panel-sidebar-unload"
            label="Unload this webpanel"
            onCommand={() => this.handleUnloadCommand()}
          />
          <xul:menuitem
            id="panel-sidebar-keeppanelwidth"
            label="Keep panel width"
            onCommand={() => gPanelSidebar.saveCurrentSidebarWidth()}
          />
          <Show when={contextPanel()?.type === "web"}>
            <xul:menuitem
              id="panel-sidebar-panel-mute"
              label="Mute/Unmute this webpanel"
              onCommand={() => this.handleMuteCommand()}
            />
            <xul:menu
              id="panel-sidebar-change-zoom-level"
              label="Change zoom level"
            >
              <xul:menupopup id="panel-sidebar-change-zoom-level-popup">
                <xul:menuitem
                  id="panel-sidebar-zoom-in"
                  label="Zoom in"
                  onCommand={() => this.handleChangeZoomLevelCommand("in")}
                />
                <xul:menuitem
                  id="panel-sidebar-zoom-out"
                  label="Zoom out"
                  onCommand={() => this.handleChangeZoomLevelCommand("out")}
                />
                <xul:menuitem
                  id="panel-sidebar-reset-zoom"
                  label="Reset zoom"
                  onCommand={() => this.handleChangeZoomLevelCommand("reset")}
                />
              </xul:menupopup>
            </xul:menu>
            <xul:menuitem
              id="panel-sidebar-change-ua"
              label="Switch User agent to Mobile/Desktop Version at this Webpanel"
              accesskey="R"
            />
          </Show>
          <xul:menuseparator />
          <xul:menuitem
            id="panel-sidebar-delete"
            label="Delete this panel"
            onCommand={() => this.handleDeleteCommand()}
          />
        </xul:menupopup>
      </xul:popupset>
    );
  }
}
