/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { safeRender } from "@nora/preact-xul";
import { addDisposer, rootEffect } from "@nora/preact-xul/lifetime";
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
      {!floating && (
        <SidebarSplitter onResizeEnd={() => ctx.saveCurrentSidebarWidth()} />
      )}
      <SidebarSelectbox ctx={ctx} />
    </>
  );
}

export class PanelSidebarElem {
  ctx: CPanelSidebar;
  private readonly sidebarReady: Promise<void>;
  private disposed = false;

  private get documentElement() {
    return document?.documentElement as unknown as XULElement;
  }

  constructor(ctx: CPanelSidebar) {
    this.ctx = ctx;
    const parentElem = document?.getElementById("browser");
    const beforeElem = document?.getElementById("tabbrowser-tabbox");

    // Wait for the sidebar controller to be initialized
    // This is a workaround to avoid Extension Sidebar Panels not being loaded
    const SidebarController = (globalThis as unknown as {
      SidebarController: { promiseInitialized: Promise<void> };
    }).SidebarController;
    let disposeSidebar = () => {};
    addDisposer(() => {
      this.disposed = true;
      disposeSidebar();
    });
    this.sidebarReady = SidebarController.promiseInitialized.then(() => {
      if (this.disposed || !parentElem) return;
      disposeSidebar = safeRender(
        <SidebarContent ctx={this.ctx} />,
        parentElem,
        beforeElem?.parentElement === parentElem ? beforeElem : undefined,
      );
    });

    if (document?.head) {
      addDisposer(safeRender(<style>{style}</style>, document.head));
    }

    rootEffect(() => {
      this.documentElement?.style.setProperty(
        "--panel-sidebar-display",
        selectedPanelId.value === null ? "none" : "flex",
      );
    });

    let wasEnabled = isPanelSidebarEnabled.peek();
    rootEffect(() => {
      const enabled = isPanelSidebarEnabled.value;
      const shouldRestore = enabled && !wasEnabled;
      wasEnabled = enabled;
      if (shouldRestore) void this.restoreSelectedPanel();
    });

    this.setVerticalTabBgColor();
    const onVerticalTabs = () => this.setVerticalTabBgColor();
    Services.prefs.addObserver("sidebar.verticalTabs", onVerticalTabs);
    addDisposer(() =>
      Services.prefs.removeObserver("sidebar.verticalTabs", onVerticalTabs)
    );
  }

  private async restoreSelectedPanel() {
    await this.sidebarReady;
    await new Promise<void>((resolve) =>
      requestAnimationFrame(() => resolve())
    );
    if (this.disposed || !isPanelSidebarEnabled.value) return;
    const panel = this.ctx.getPanelData(selectedPanelId.value ?? "");
    if (!panel) return;
    this.ctx.setSidebarWidth(panel);
    this.ctx.showPanel(panel);
  }

  private setVerticalTabBgColor() {
    const newValue = Services.prefs.getBoolPref("sidebar.verticalTabs");
    // Gecko 152 (Project Nova) renamed --toolbox-bgcolor -> --toolbox-background-color
    // and --toolbar-bgcolor -> --toolbar-background-color.
    //
    // FALLBACK ORDERING MATTERS: on Gecko 152 the legacy --toolbar-bgcolor and
    // the new --toolbar-background-color are NOT guaranteed to hold the same
    // value (e.g. default-theme dark: --toolbar-bgcolor = #171717 but
    // --toolbar-background-color = rgb(43,42,51)). Firefox 152 still paints the
    // selected .tab-background from --toolbar-bgcolor, and Lepton's
    // color_like_toolbar unsets --tab-selected-bgcolor so the tab resolves to
    // --toolbar-bgcolor. The panel-sidebar / status bar must track the SAME
    // token as the selected tab, so the legacy names are preferred and the new
    // 152 names are kept only as a final fallback.
    this.documentElement?.style.setProperty(
      "--panel-sidebar-background-color",
      newValue
        ? "var(--toolbox-bgcolor, var(--toolbox-background-color, var(--toolbar-bgcolor, var(--toolbar-background-color))))"
        : "var(--toolbar-bgcolor, var(--toolbar-background-color))",
    );
  }
}
