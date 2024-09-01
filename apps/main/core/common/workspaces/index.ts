/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { workspacesServices } from "./workspaces";
import { workspacesToolbarButton } from "./toolbar-element";
import { workspacesTabContextMenu } from "./tabContextMenu";

export function initWorkspaces() {
  workspacesTabContextMenu.getInstance();
  workspacesToolbarButton.getInstance();
  workspacesServices.getInstance();
}
