/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createMemo, Show } from "solid-js";
import type { TWorkspaceID } from "../utils/type.js";
import type { WorkspacesService } from "../workspacesService";

export function PopupToolbarElement(props: {
  workspaceId: TWorkspaceID;
  isSelected: boolean;
  bmsMode: boolean;
  ctx: WorkspacesService;
}) {
  const workspace = createMemo(() =>
    props.ctx.getRawWorkspace(props.workspaceId)
  );
  return (
    <Show when={workspace()}>
      {(ws) => {
        const icon = () => props.ctx.iconCtx.getWorkspaceIconUrl(ws.icon);
        return (
          <xul:toolbarbutton
            id={`workspace-${props.workspaceId}`}
            label={ws().name}
            context="workspaces-toolbar-item-context-menu"
            class="toolbarbutton-1 chromeclass-toolbar-additional workspaceButton"
            style={{
              "list-style-image": `url(${icon()})`,
            }}
            closemenu="none"
            data-selected={props.isSelected}
            data-workspaceId={props.workspaceId}
            onCommand={() => {
              props.ctx.changeWorkspace(props.workspaceId);
            }}
          />
        );
      }}
    </Show>
  );
}
