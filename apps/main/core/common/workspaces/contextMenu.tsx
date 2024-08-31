/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Workspaces } from "./workspaces";

function ContextMenu(props: { disableBefore: boolean, disableAfter: boolean, contextWorkspaceId: string }) {
    const { disableBefore, disableAfter, contextWorkspaceId } = props;
    const gWorkspaces = Workspaces.getInstance();

    return (
        <>
            <xul:menuitem data-l10n-id="reorder-this-workspace-to-up" disabled={disableBefore}
                oncommand={() => gWorkspaces.reorderWorkspaceUp(contextWorkspaceId)} />
            <xul:menuitem data-l10n-id="reorder-this-workspace-to-down" disabled={disableAfter}
                oncommand={() => gWorkspaces.reorderWorkspaceDown(contextWorkspaceId)} />
            <xul:menuseparator class="workspaces-context-menu-separator" />
            <xul:menuitem data-l10n-id="rename-this-workspace"
                oncommand={() => gWorkspaces.renameWorkspaceWithCreatePrompt(contextWorkspaceId)} />
            <xul:menuitem data-l10n-id="delete-this-workspace"
                oncommand={() => gWorkspaces.deleteWorkspace(contextWorkspaceId)} />
            <xul:menuitem data-l10n-id="manage-this-workspaces"
                oncommand={() => gWorkspaces.manageWorkspaceFromDialog(contextWorkspaceId)} />
        </>
    );
};
