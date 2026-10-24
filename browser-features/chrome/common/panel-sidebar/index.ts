/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { CPanelSidebar } from "./components/panel-sidebar";
import { PanelSidebarElem } from "./components/sidebar";
import { SidebarContextMenuElem } from "./components/sidebar-contextMenu";
import { migratePanelSidebarData } from "./data/migration";
import { WebsitePanelWindowChild } from "./website-panel-window-child";
import { PanelSidebarAddModal } from "./components/panel-sidebar-modal";
import { PanelSidebarFloating } from "./components/floating";
import { PanelSidebarHover } from "./components/overlay-hover";
import { WebPanelPopupNotifications } from "./utils/web-panel-popup-notifications";
import { selectedPanelId } from "./data/data";
import { noraComponent, NoraComponentBase } from "#features-chrome/utils/base";
import { createEffect, onCleanup } from "solid-js";
import { observeWebPanelRequests } from "./utils/webRequest.ts";

//TODO: refactor needed

@noraComponent(import.meta.hot)
export default class PanelSidebar extends NoraComponentBase {
  init(): void {
    const webPanelChild = WebsitePanelWindowChild.getInstance();
    if (webPanelChild.isBmsWindow) {
      return;
    }

    onCleanup(observeWebPanelRequests(window));
    migratePanelSidebarData();
    const popupRouter = WebPanelPopupNotifications.install();
    // Show the prompts a web panel asked for while it was in the background.
    createEffect(() => {
      selectedPanelId();
      popupRouter.refresh();
    });

    const ctx = new CPanelSidebar();
    new PanelSidebarElem(ctx);
    new SidebarContextMenuElem(ctx);
    PanelSidebarAddModal.getInstance();
    PanelSidebarFloating.getInstance();
    new PanelSidebarHover(ctx);
  }
}
