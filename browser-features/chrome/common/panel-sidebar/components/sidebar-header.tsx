/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Show } from "solid-js";
import {
  isFloating,
  selectedPanelId,
  setIsFloating,
  setSelectedPanelId,
} from "../data/data";
import { PanelNavigator } from "../panel-navigator";
import type { CPanelSidebar } from "./panel-sidebar";

export function SidebarHeader(props: { ctx: CPanelSidebar }) {
  const gPanelSidebar = props.ctx;
  PanelNavigator.gPanelSidebar = gPanelSidebar;
  return (
    <xul:box id="panel-sidebar-header" align="center">
      <Show
        when={gPanelSidebar.getPanelData(selectedPanelId() ?? "")?.type ===
          "web"}
      >
        <xul:toolbarbutton
          id="panel-sidebar-back"
          class="panel-sidebar-actions toolbarbutton-1 chromeclass-toolbar-additional"
          onCommand={() => PanelNavigator.back(selectedPanelId() ?? "")}
        />
        <xul:toolbarbutton
          id="panel-sidebar-forward"
          onCommand={() => PanelNavigator.forward(selectedPanelId() ?? "")}
          class="panel-sidebar-actions"
        />
        <xul:toolbarbutton
          id="panel-sidebar-reload"
          onCommand={() => PanelNavigator.reload(selectedPanelId() ?? "")}
          class="panel-sidebar-actions"
        />
        <xul:toolbarbutton
          id="panel-sidebar-go-index"
          onCommand={() => PanelNavigator.goIndexPage(selectedPanelId() ?? "")}
          class="panel-sidebar-actions"
        />
      </Show>
      <xul:spacer flex="1" />
      <xul:toolbarbutton
        id="panel-sidebar-float"
        onCommand={() => setIsFloating(!isFloating())}
        class="panel-sidebar-actions"
      />
      <Show
        when={gPanelSidebar.getPanelData(selectedPanelId() ?? "")?.type ===
          "web"}
      >
        <xul:toolbarbutton
          id="panel-sidebar-open-in-main-window"
          onCommand={() =>
            gPanelSidebar.openInMainWindow(selectedPanelId() ?? "")}
          class="panel-sidebar-actions"
        />
      </Show>
      <xul:toolbarbutton
        id="panel-sidebar-close"
        onCommand={() => setSelectedPanelId(null)}
        class="panel-sidebar-actions"
      />
    </xul:box>
  );
}
