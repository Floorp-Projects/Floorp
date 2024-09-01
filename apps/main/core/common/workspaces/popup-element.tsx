/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { For } from "solid-js";
import { workspacesServices } from "./workspaces";
import { workspaces } from "./data";
import { PopupToolbarElement } from "./popup-block-element";
import { config } from "./config";

export function PopupElement() {
  const gWorkspacesServices = workspacesServices.getInstance();
  return (
    <xul:panel
      id="workspacesToolbarButtonPanel"
      type="arrow"
      position={"bottomleft topleft"}
    >
      <xul:vbox id="workspacesToolbarButtonPanelBox">
        <xul:arrowscrollbox id="workspacesPopupBox" flex="1">
          <xul:vbox
            id="workspacesPopupContent"
            align="center"
            flex="1"
            orient="vertical"
            clicktoscroll={true}
            class="statusbar-padding"
          >
            <For each={workspaces()}>
              {(workspace) => (
                <PopupToolbarElement
                  workspaceId={workspace.id}
                  label={workspace.name}
                  isSelected={workspace.id === window.floorpWorkspaeId}
                  bmsMode={config().manageOnBms}
                />
              )}
            </For>
          </xul:vbox>
        </xul:arrowscrollbox>
        <xul:toolbarseparator
          class="toolbarbutton-1 chromeclass-toolbar-additional"
          id="workspacesPopupSeparator"
        />
        <xul:hbox id="workspacesPopupFooter" align="center" pack="center">
          <xul:toolbarbutton
            id="workspacesCreateNewWorkspaceButton"
            class="toolbarbutton-1 chromeclass-toolbar-additional"
            data-l10n-id="workspaces-create-new-workspace-button"
            context="tab-stacks-toolbar-item-context-menu"
            oncommand={gWorkspacesServices.createNoNameWorkspace}
          />
          <xul:toolbarbutton
            id="workspacesManageworkspacesServicesButton"
            class="toolbarbutton-1 chromeclass-toolbar-additional"
            data-l10n-id="workspaces-manage-workspaces-button"
            context="tab-stacks-toolbar-item-context-menu"
            oncommand={() =>
              gWorkspacesServices.manageWorkspaceFromDialog("workspacesId")
            }
          />
        </xul:hbox>
      </xul:vbox>
    </xul:panel>
  );
}
