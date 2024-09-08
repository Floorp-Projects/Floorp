/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { getWorkspaceIconUrl } from "./utils/workspace-icons";
import { workspacesServices } from "./workspaces";

export function PopupToolbarElement(props: {
  workspaceId: string;
  isSelected: boolean;
  label: string;
  bmsMode: boolean;
}) {
  const gWorkspacesServices = workspacesServices.getInstance();
  const { workspaceId, isSelected, bmsMode } = props;
  return (
    <xul:toolbarbutton
      id={`workspace-${workspaceId}`}
      label={props.label}
      context="workspaces-toolbar-item-context-menu"
      class="toolbarbutton-1 chromeclass-toolbar-additional workspaceButton"
      style={{
        "list-style-image": `url(${getWorkspaceIconUrl(workspaceId)})`,
      }}
      data-selected={isSelected}
      data-workspaceId={workspaceId}
      oncommand={() => {
        gWorkspacesServices.changeWorkspace(workspaceId);
      }}
    />
  );
}
