/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createMemo, Show } from "solid-js";
import type { TWorkspaceID } from "../utils/type.js";
import type { WorkspacesService } from "../workspacesService";
import { getContainerColorName } from "../utils/container-color";
import { workspacesDataStore } from "../data/data.ts";
import {
  bindWorkspaceSyncFocusTooltip,
  createWorkspaceSyncIndicator,
} from "./workspace-sync-indicator.ts";

export function PopupToolbarElement(props: {
  workspaceId: TWorkspaceID;
  isSelected: boolean;
  bmsMode: boolean;
  ctx: WorkspacesService;
}) {
  const workspace = createMemo(() =>
    props.ctx.getRawWorkspace(props.workspaceId)
  );
  const syncIndicator = createWorkspaceSyncIndicator(
    props.ctx,
    () => props.workspaceId,
    () => workspace()?.name ?? "",
  );

  const handleDragStart = (event: DragEvent) => {
    const target = event.currentTarget as XULElement;
    if (event.dataTransfer) {
      event.dataTransfer.effectAllowed = "move";
      event.dataTransfer.setData("text/plain", props.workspaceId);
      target.setAttribute("dragging", "true");
    }
  };

  const handleDragEnd = (event: DragEvent) => {
    const target = event.currentTarget as XULElement;
    target.removeAttribute("dragging");
  };

  const handleDragOver = (event: DragEvent) => {
    event.preventDefault();
    if (event.dataTransfer) {
      event.dataTransfer.dropEffect = "move";
    }
    const target = event.currentTarget as XULElement;
    target.setAttribute("drag-over", "true");
  };

  const handleDragLeave = (event: DragEvent) => {
    const target = event.currentTarget as XULElement;
    target.removeAttribute("drag-over");
  };

  const handleDrop = (event: DragEvent) => {
    event.preventDefault();
    const target = event.currentTarget as XULElement;
    target.removeAttribute("drag-over");

    if (!event.dataTransfer) {
      return;
    }

    const draggedWorkspaceId = event.dataTransfer.getData("text/plain") as
      | TWorkspaceID
      | "";
    if (!draggedWorkspaceId || draggedWorkspaceId === props.workspaceId) {
      return;
    }

    if (!props.ctx.isWorkspaceID(draggedWorkspaceId)) {
      return;
    }

    const order = workspacesDataStore.order;
    const targetIndex = order.indexOf(props.workspaceId);
    if (targetIndex === -1) {
      return;
    }

    props.ctx.reorderWorkspaceTo(draggedWorkspaceId, targetIndex);
  };

  return (
    <Show when={workspace()}>
      {(ws) => {
        const icon = () => props.ctx.iconCtx.getWorkspaceIconUrl(ws().icon);
        const userContextId = () => ws().userContextId ?? 0;
        const hasContainer = () => userContextId() > 0;
        const containerColorName = () => getContainerColorName(userContextId());
        const button = (
          <xul:toolbarbutton
            id={`workspace-${props.workspaceId}`}
            label={ws().name}
            context="workspaces-toolbar-item-context-menu"
            class="toolbarbutton-1 chromeclass-toolbar-additional workspaceButton"
            tabindex="0"
            style={{
              "list-style-image": `url(${icon()})`,
            }}
            closemenu="none"
            data-selected={props.isSelected}
            data-workspaceId={props.workspaceId}
            data-workspace-sync={syncIndicator().state}
            tooltiptext={syncIndicator().tooltip}
            aria-label={syncIndicator().tooltip}
            data-has-container={hasContainer() ? "true" : "false"}
            data-container-color={containerColorName() ?? ""}
            draggable="true"
            onDragStart={handleDragStart}
            onDragEnd={handleDragEnd}
            onDragOver={handleDragOver}
            onDragLeave={handleDragLeave}
            onDrop={handleDrop}
            onCommand={() => {
              props.ctx.changeWorkspace(props.workspaceId);
            }}
          >
            <xul:image
              class="toolbarbutton-icon workspaceButtonIcon"
              src={icon()}
              aria-hidden="true"
            />
            <xul:vbox
              class="workspaceButtonContent"
              flex="1"
              aria-hidden="true"
            >
              <xul:label
                class="workspace-name"
                value={ws().name}
                crop="end"
              />
              <Show when={syncIndicator().state}>
                <xul:hbox class="workspace-sync-status-line">
                  <xul:box class="workspace-sync-status-icon" />
                  <xul:label
                    class="workspace-sync-status"
                    value={syncIndicator().status}
                    crop="end"
                  />
                </xul:hbox>
              </Show>
            </xul:vbox>
          </xul:toolbarbutton>
        ) as XULElement;
        bindWorkspaceSyncFocusTooltip(button, syncIndicator);
        return button;
      }}
    </Show>
  );
}
