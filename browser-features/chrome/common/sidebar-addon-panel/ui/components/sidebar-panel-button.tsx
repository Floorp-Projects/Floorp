/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createResource, Show, Suspense } from "solid-js";
import { getFaviconURLForPanel } from "../../core/utils/favicon-getter.ts";
import type { CPanelSidebar } from "./panel-sidebar";
import {
  panelSidebarData,
  selectedPanelId,
  setPanelSidebarData,
} from "../../core/data.ts";
import type { Panel } from "../../core/utils/type.ts";
import { isExtensionExist } from "../../core/extension-panels.ts";
import { getUserContextColor } from "../../core/utils/userContextColor-getter.ts";
import { setContextPanel } from "./sidebar-contextMenu.ts";

export function PanelSidebarButton(props: {
  panel: Panel;
  ctx: CPanelSidebar;
}) {
  const gPanelSidebar = props.ctx;
  const [faviconURL] = createResource(
    () => props.panel,
    async () => await getFaviconURLForPanel(props.panel),
  );

  const handleDragStart = (e: DragEvent) => {
    e.dataTransfer?.setData("text/floorp-panel-id", props.panel.id);
    (e.target as HTMLElement).classList.add("dragging");
  };

  const handleDragEnd = (e: DragEvent) => {
    (e.target as HTMLElement).classList.remove("dragging");
  };

  const handleDragOver = (e: DragEvent) => {
    e.preventDefault();
    e.stopPropagation();
    (e.target as HTMLElement).classList.add("drag-over");
  };

  const handleDragLeave = (e: DragEvent) => {
    e.preventDefault();
    e.stopPropagation();
    (e.target as HTMLElement).classList.remove("drag-over");
  };

  const handleDrop = (e: DragEvent) => {
    e.preventDefault();
    e.stopPropagation();
    (e.target as HTMLElement).classList.remove("drag-over");

    const sourceId = e.dataTransfer?.getData("text/floorp-panel-id");
    const targetId = props.panel.id;

    if (sourceId === targetId) return;

    const panels = [...panelSidebarData()];
    const sourceIndex = panels.findIndex((p) => p.id === sourceId);
    const targetIndex = panels.findIndex((p) => p.id === targetId);

    if (sourceIndex === -1 || targetIndex === -1) return;

    const [movedPanel] = panels.splice(sourceIndex, 1);
    panels.splice(targetIndex, 0, movedPanel);

    setPanelSidebarData(panels);
  };

  const handleContextMenu = (e: MouseEvent) => {
    e.preventDefault();
    setContextPanel(props.panel);

    const contextMenu = document?.getElementById(
      "webpanel-context",
    ) as XULPopupElement;
    if (contextMenu) {
      contextMenu.openPopupAtScreen(e.screenX, e.screenY, true);
    }
  };

  if (
    props.panel.type === "extension" &&
    !isExtensionExist(props.panel.extensionId as string)
  ) {
    return null;
  }

  return (
    <div
      draggable={true}
      onDragStart={handleDragStart}
      onDragEnd={handleDragEnd}
      onDragOver={handleDragOver}
      onDragLeave={handleDragLeave}
      onDrop={handleDrop}
      onContextMenu={handleContextMenu}
      class="relative"
    >
      <div
        id={props.panel.id}
        class={`${props.panel.type} panel-sidebar-panel flex items-center justify-center`}
        data-checked={selectedPanelId() === props.panel.id}
        data-panel-id={props.panel.id}
        onClick={() => {
          gPanelSidebar.changePanel(props.panel.id);
        }}
        style={{
          "border-right":
            props.panel.userContextId !== 0 &&
            props.panel.userContextId !== null &&
            props.panel.type === "web"
              ? `2px solid ${getUserContextColor(props.panel.userContextId ?? 0)}`
              : "none",
        }}
      >
        <img
          src={faviconURL() ?? ""}
          width="16"
          height="16"
          onError={(e) => {
            (e.target as HTMLImageElement).src =
              "chrome://devtools/skin/images/globe.svg";
          }}
        />
      </div>
    </div>
  );
}
