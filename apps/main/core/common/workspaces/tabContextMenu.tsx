/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Workspaces } from "./workspaces";

function ContextMenu(props: { disableBefore: boolean, disableAfter: boolean, contextWorkspaceId: string }) {
    const { disableBefore, disableAfter, contextWorkspaceId } = props;
    const gWorkspaces = Workspaces.getInstance();

    return (
        <xul:menu id="context_MoveTabToOtherWorkspace" data-l10n-id="move-tab-another-workspace" accesskey="D">
            <xul:menupopup id="workspacesTabContextMenu"
                onpopupshowing={gWorkspaces.contextMenu.createTabWorkspacesContextMenuItems} />
        </xul:menu>
    );
};