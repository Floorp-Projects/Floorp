/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { WorkspacesServices } from "./workspaces";
import { WorkspacesToolbarButton } from "./toolbar-element";
import { WorkspacesTabContextMenu } from "./tabContextMenu";
import { WorkspacesPopupContxtMenu } from "./popupSet";
import { WorkspaceIcons } from "./utils/workspace-icons";

export function init() {
  const gWorkspaceIcons = WorkspaceIcons.getInstance();
  gWorkspaceIcons.initializeIcons().then(() => {
    WorkspacesServices.getInstance();
    WorkspacesTabContextMenu.getInstance();
    WorkspacesToolbarButton.getInstance();
    WorkspacesPopupContxtMenu.getInstance();
  });

  import.meta.hot?.accept((m) => {
    m?.init();
  });
}
