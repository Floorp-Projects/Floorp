/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Workspaces } from "./workspaces";

export function PopupElement() {
    const gWorkspaces = Workspaces.getInstance();
    return (
        <xul:panel id="workspacesToolbarButtonPanel" type="arrow" position={"bottomleft topleft"}>
            <xul:vbox id="workspacesToolbarButtonPanelBox">
                <xul:arrowscrollbox id="workspacesPopupBox" flex="1">
                    <xul:vbox id="workspacesPopupContent" align="center" flex="1" orient="vertical"
                        clicktoscroll={true} class="statusbar-padding" />
                </xul:arrowscrollbox>
                <xul:toolbarseparator class="toolbarbutton-1 chromeclass-toolbar-additional" id="workspacesPopupSeparator" />
                <xul:hbox id="workspacesPopupFooter" align="center" pack="center">
                    <xul:toolbarbutton id="workspacesCreateNewWorkspaceButton" class="toolbarbutton-1 chromeclass-toolbar-additional"
                        data-l10n-id="workspaces-create-new-workspace-button" context="tab-stacks-toolbar-item-context-menu"
                        oncommand={gWorkspaces.createNoNameWorkspace()} />
                    <xul:toolbarbutton id="workspacesManageWorkspacesButton" class="toolbarbutton-1 chromeclass-toolbar-additional"
                        data-l10n-id="workspaces-manage-workspaces-button" context="tab-stacks-toolbar-item-context-menu"
                        oncommand={gWorkspaces.manageWorkspaceFromDialog()} />
                </xul:hbox>
            </xul:vbox>
        </xul:panel>
    )
}
