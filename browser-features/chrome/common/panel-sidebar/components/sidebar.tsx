/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { render } from "@nora/solid-xul";
import style from "./style.css?inline";
import { SidebarHeader } from "./sidebar-header.tsx";
import { SidebarSelectbox } from "./sidebar-selectbox.tsx";
import { SidebarSplitter } from "./sidebar-splitter.tsx";
import {
  createEffect,
  createRoot,
  getOwner,
  runWithOwner,
  Show,
} from "solid-js";
import {
  isFloating,
  isPanelSidebarEnabled,
  selectedPanelId,
} from "../data/data.ts";
import { FloatingSplitter } from "./floating-splitter.tsx";
import { BrowserBox } from "./browser-box.tsx";
import type { CPanelSidebar } from "./panel-sidebar.tsx";

export class PanelSidebarElem {
  ctx: CPanelSidebar;

  private get documentElement() {
    return document?.documentElement as XULElement;
  }

  constructor(ctx: CPanelSidebar) {
    this.ctx = ctx;
    if (!isPanelSidebarEnabled()) {
      return;
    }
    const parentElem = document?.getElementById("browser");
    const beforeElem = document?.getElementById("tabbrowser-tabbox");

    // Wait for the sidebar controller to be initialized
    // This is a workaround to avoid Extension Sidebar Panels not being loaded
    const owner = getOwner();
    const SidebarController = (
      globalThis as unknown as {
        SidebarController: { promiseInitialized: Promise<void> };
      }
    ).SidebarController;
    SidebarController.promiseInitialized.then(() => {
      const exec = () =>
        render(() => this.sidebar(), parentElem, {
          marker: beforeElem as XULElement,
        });
      if (owner) runWithOwner(owner, exec);
      else createRoot(exec);
    });

    render(() => this.style(), document?.head);

    const execEffect = () =>
      createEffect(() => {
        if (selectedPanelId() === null) {
          this.documentElement?.style.setProperty(
            "--panel-sidebar-display",
            "none",
          );
        } else {
          this.documentElement?.style.setProperty(
            "--panel-sidebar-display",
            "flex",
          );
        }
      });
    if (owner) runWithOwner(owner, execEffect);
    else createRoot(execEffect);

    this.setVerticalTabBgColor();
    Services.prefs.addObserver("sidebar.verticalTabs", () => {
      this.setVerticalTabBgColor();
    });
  }

  private setVerticalTabBgColor() {
    const newValue = Services.prefs.getBoolPref("sidebar.verticalTabs");
    this.documentElement?.style.setProperty(
      "--panel-sidebar-background-color",
      newValue ? "var(--toolbox-bgcolor)" : "var(--toolbar-bgcolor)",
    );
  }

  private style() {
    return <style>{style}</style>;
  }

  private sidebar() {
    return (
      <Show when={isPanelSidebarEnabled()}>
        <xul:vbox
          id="panel-sidebar-box"
          class="chromeclass-extrachrome chromeclass-directories instant customization-target"
          data-floating={isFloating().toString()}
          popover="manual"
        >
          <SidebarHeader ctx={this.ctx} />
          <BrowserBox />
          <Show when={isFloating()}>
            <FloatingSplitter />
          </Show>
        </xul:vbox>
        <Show when={!isFloating()}>
          <SidebarSplitter />
        </Show>
        <SidebarSelectbox ctx={this.ctx} />
      </Show>
    );
  }
}
