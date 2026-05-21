/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { render } from "preact";
import { safeRender } from "@nora/preact-xul";
import { effect } from "@preact/signals";
import style from "./style.css?inline";
import { SidebarHeader } from "./sidebar-header";
import { SidebarSelectbox } from "./sidebar-selectbox";
import { SidebarSplitter } from "./sidebar-splitter";
import {
  isFloating,
  isPanelSidebarEnabled,
  selectedPanelId,
} from "../data/data";
import { FloatingSplitter } from "./floating-splitter";
import { BrowserBox } from "./browser-box";
import type { CPanelSidebar } from "./panel-sidebar";

type SidebarContentProps = { ctx: CPanelSidebar };

function SidebarContent({ ctx }: SidebarContentProps) {
  const enabled = isPanelSidebarEnabled.value;
  const floating = isFloating.value;

  if (!enabled) {
    return null;
  }

  return (
    <>
      <xul:vbox
        id="panel-sidebar-box"
        class="chromeclass-extrachrome chromeclass-directories instant customization-target"
        data-floating={floating.toString()}
        popover="manual"
      >
        <SidebarHeader ctx={ctx} />
        <BrowserBox />
        {floating && <FloatingSplitter />}
      </xul:vbox>
      {!floating && <SidebarSplitter />}
      <SidebarSelectbox ctx={ctx} />
    </>
  );
}

export class PanelSidebarElem {
  ctx: CPanelSidebar;

  private get documentElement() {
    return document?.documentElement as unknown as XULElement;
  }

  constructor(ctx: CPanelSidebar) {
    this.ctx = ctx;
    if (!isPanelSidebarEnabled.value) {
      return;
    }
    const parentElem = document?.getElementById("browser");
    const beforeElem = document?.getElementById("tabbrowser-tabbox");

    // Wait for the sidebar controller to be initialized
    // This is a workaround to avoid Extension Sidebar Panels not being loaded
    const SidebarController = (globalThis as unknown as {
      SidebarController: { promiseInitialized: Promise<void> };
    }).SidebarController;
    SidebarController.promiseInitialized.then(() => {
      // Create a container for the sidebar and insert it before the tabbox
      const sidebarContainer = document?.createXULElement("box") as unknown as XULElement;
      if (parentElem && beforeElem?.parentElement === parentElem) {
        parentElem.insertBefore(sidebarContainer, beforeElem);
      } else if (parentElem) {
        parentElem.appendChild(sidebarContainer);
      }
      render(<SidebarContent ctx={this.ctx} />, sidebarContainer as unknown as Element);
    });

    if (document?.head) {
      safeRender(<style>{style}</style>, document.head);
    }

    effect(() => {
      if (selectedPanelId.value === null) {
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
}
