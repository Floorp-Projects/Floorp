/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Workspaces } from "./workspaces";

function PopupSet(props: { disableBefore: boolean, disableAfter: boolean, contextWorkspaceId: string }) {
    const gWorkspaces = Workspaces.getInstance();

    return (
        <xul:popupset>
            <xul:menupopup id="workspaces-toolbar-item-context-menu" onpopupshowing={(event) => gWorkspaces.contextMenu.createWorkspacesContextMenuItems(event)} />
        </xul:popupset>
    );
};
