/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BrowserActionUtils } from "#features-chrome/utils/browser-action.tsx";
import { PopupElement } from "./popup-element.tsx";
import workspacesStyles from "./styles.css?inline";
import { effect } from "@preact/signals";
import {
  addDisposer,
  createNodeDisposer,
  createRootHMR,
} from "@nora/preact-xul/lifetime";
import type { WorkspacesService } from "../workspacesService.ts";
import { configStore, enabled } from "../data/config.ts";
import Workspaces from "../index.ts";
import { selectedWorkspaceID, workspacesDataStore } from "../data/data.ts";
import { getContainerColorName } from "../utils/container-color.ts";
import type { JSX } from "preact";

const { CustomizableUI } = ChromeUtils.importESModule(
  "moz-src:///browser/components/customizableui/CustomizableUI.sys.mjs",
);

let lastDisplayedWorkspaceId: string | null = null;

export class WorkspacesToolbarButton {
  private StyleElement = () => {
    return <style>{workspacesStyles}</style>;
  };

  constructor(ctx: WorkspacesService) {
    BrowserActionUtils.createMenuToolbarButton(
      "workspaces-toolbar-button",
      "workspaces-toolbar-button",
      "workspacesToolbarButtonPanel",
      <PopupElement ctx={ctx} />,
      null,
      (node) => {
        const timer = setTimeout(() => this.updateButtonIfNeeded(true), 500);
        const disposers: (() => void)[] = [];

        disposers.push(effect(() => {
          const _ = configStore.showWorkspaceNameOnToolbar;
          const __ = enabled.value;
          this.updateButtonIfNeeded();
        }));

        disposers.push(effect(() => {
          const _ = selectedWorkspaceID.value;
          this.updateButtonIfNeeded();
        }));

        // Watch for workspace data changes to update button when name changes
        disposers.push(effect(() => {
          const _ = workspacesDataStore.data;
          this.updateButtonIfNeeded(true);
        }));
        const dispose = () => {
          clearTimeout(timer);
          disposers.forEach((cleanup) => cleanup());
        };
        createRootHMR(() => {
          createNodeDisposer(node, dispose);
        }, import.meta.hot);
      },
      CustomizableUI.AREA_TABSTRIP,
      this.StyleElement() as JSX.Element,
      0,
    );

    const intervalId = setInterval(() => this.updateButtonIfNeeded(), 500);
    addDisposer(() => clearInterval(intervalId));
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
      if (!enabled.value) {
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
