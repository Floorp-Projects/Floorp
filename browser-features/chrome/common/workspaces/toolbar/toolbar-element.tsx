/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BrowserActionUtils } from "#features-chrome/utils/browser-action.tsx";
import { PopupElement } from "./popup-element.tsx";
import workspacesStyles from "./styles.css?inline";
import {
  createEffect,
  createRoot,
  getOwner,
  type JSX,
  onCleanup,
  runWithOwner,
} from "solid-js";
import type { WorkspacesService } from "../workspacesService.ts";
import { configStore, enabled } from "../data/config.ts";
import Workspaces from "../index.ts";
import { selectedWorkspaceID, workspacesDataStore } from "../data/data.ts";
import { getContainerColorName } from "../utils/container-color.ts";

const { CustomizableUI } = ChromeUtils.importESModule(
  "moz-src:///browser/components/customizableui/CustomizableUI.sys.mjs",
);

let lastDisplayedWorkspaceId: string | null = null;

export class WorkspacesToolbarButton {
  private StyleElement = () => {
    return <style>{workspacesStyles}</style>;
  };

  constructor(ctx: WorkspacesService) {
    const owner = getOwner();
    BrowserActionUtils.createMenuToolbarButton(
      "workspaces-toolbar-button",
      "workspaces-toolbar-button",
      "workspacesToolbarButtonPanel",
      <PopupElement ctx={ctx} />,
      null,
      () => {
        const exec = () => {
          setTimeout(() => this.updateButtonIfNeeded(true), 500);

          createEffect(() => {
            const _ = configStore.showWorkspaceNameOnToolbar;
            const __ = enabled();
            this.updateButtonIfNeeded();
          });

          createEffect(() => {
            const _ = selectedWorkspaceID();
            this.updateButtonIfNeeded();
          });

          // Watch for workspace data changes to update button when name changes
          createEffect(() => {
            const _ = workspacesDataStore.data;
            // Force update when workspace data changes
            this.updateButtonIfNeeded(true);
          });
        };
        if (owner) runWithOwner(owner, exec);
        else createRoot(exec);
      },
      CustomizableUI.AREA_TABSTRIP,
      this.StyleElement() as JSX.Element,
      0,
    );

    const setupInterval = () => {
      const intervalId = setInterval(() => this.updateButtonIfNeeded(), 500);
      onCleanup(() => clearInterval(intervalId));
    };
    if (owner) runWithOwner(owner, setupInterval);
    else createRoot(setupInterval);
  }

  private getButtonNode(): Element | null {
    if (!document) return null;
    return document.querySelector("#workspaces-toolbar-button");
  }

  private updateButtonIfNeeded(force = false) {
    const aNode = this.getButtonNode();
    if (!aNode) return;

    try {
      const ctx = Workspaces.getCtx();
      if (!ctx) return;

      const currentId = ctx.getSelectedWorkspaceID();
      if (!currentId) return;

      if (!force && lastDisplayedWorkspaceId === currentId) return;

      lastDisplayedWorkspaceId = currentId;

      const workspace = ctx.getRawWorkspace(currentId);
      if (!workspace) return;
      const icon = ctx.iconCtx.getWorkspaceIconUrl(workspace.icon);
      const xulElement = aNode as unknown as XULElement;

      xulElement.style.setProperty(
        "list-style-image",
        icon ? `url(${icon})` : `url("chrome://branding/content/icon32.png")`,
      );

      if (configStore.showWorkspaceNameOnToolbar) {
        xulElement.setAttribute("label", workspace.name);
      } else {
        xulElement.removeAttribute("label");
      }
      if (!enabled()) {
        xulElement.setAttribute("hidden", "true");
      } else {
        xulElement.removeAttribute("hidden");
      }

      // Set container color attributes
      const userContextId = workspace.userContextId ?? 0;
      const hasContainer = userContextId > 0;
      const containerColorName = getContainerColorName(userContextId);
      xulElement.setAttribute(
        "data-has-container",
        hasContainer ? "true" : "false",
      );
      xulElement.setAttribute(
        "data-container-color",
        containerColorName ?? "",
      );
    } catch (e) {
      console.error("Error updating workspace toolbar button:", e);
    }
  }
}
