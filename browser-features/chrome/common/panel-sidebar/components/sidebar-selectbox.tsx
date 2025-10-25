/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { For } from "solid-js";
import { panelSidebarData } from "../data/data";
import { PanelSidebarButton } from "./sidebar-panel-button";
import { showPanelSidebarAddModal } from "./panel-sidebar-modal";
import type { CPanelSidebar } from "./panel-sidebar";
import { WorkspacesPanels } from "../../workspaces/toolbar/workspaces-panels.tsx";

export function SidebarSelectbox(props: { ctx: CPanelSidebar }) {
  return (
    <xul:vbox
      id="panel-sidebar-select-box"
      class="webpanel-box chromeclass-extrachrome chromeclass-directories instant customization-target"
    >
      <WorkspacesPanels />
      <For each={panelSidebarData()}>
        {(panel) => <PanelSidebarButton panel={panel} ctx={props.ctx} />}
      </For>
      <xul:toolbarbutton
        id="panel-sidebar-add"
        class="panel-sidebar-panel"
        onCommand={() => {
          showPanelSidebarAddModal();
        }}
      />
      <xul:spacer flex="1" />
      <xul:vbox id="panel-sidebar-bottomButtonBox">
        <xul:toolbarbutton
          class="panel-sidebar-panel"
          onCommand={() =>
            window.BrowserAddonUI.openAddonsMgr("addons://list/extension")}
          id="panel-sidebar-addons-icon"
        />
        <xul:toolbarbutton
          class="panel-sidebar-panel"
          onCommand={() =>
            window.gBrowser.addTrustedTab("about:logins", {
              inBackground: false,
            })}
          id="panel-sidebar-passwords-icon"
        />
        <xul:toolbarbutton
          class="panel-sidebar-panel"
          onCommand={() => window.openPreferences()}
          id="panel-sidebar-preferences-icon"
        />
      </xul:vbox>
    </xul:vbox>
  );
}
