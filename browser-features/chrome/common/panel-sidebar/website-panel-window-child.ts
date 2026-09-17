/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { Panels } from "./utils/type.ts";
import {
  isWebPanelChildUrl,
  WEB_PANEL_URL_PARAM,
} from "./utils/web-panel-context.ts";
import {
  applySavedZoomLevel,
  loadUriInWebPanelBrowser,
  type WebPanelBrowserElement,
} from "./utils/web-panel-browser.ts";

const PANEL_SIDEBAR_DATA_PREF_NAME = "floorp.panelSidebar.data";

interface WebPanelTabBrowser {
  selectedTab: XULElement;
  readonly selectedBrowser: WebPanelBrowserElement;
  addTab(
    url: string,
    options: {
      userContextId: number;
      triggeringPrincipal: nsIPrincipal;
      skipAnimation: boolean;
    },
  ): XULElement;
  removeTab(tab: XULElement): void;
}

export function prepareWebPanelTab(
  tabBrowser: WebPanelTabBrowser,
  userContextId: number,
): WebPanelBrowserElement {
  const initialTab = tabBrowser.selectedTab;
  const initialUserContextId = Number(
    initialTab.getAttribute("usercontextid") || 0,
  );

  if (initialUserContextId !== userContextId) {
    const triggeringPrincipal = Services.scriptSecurityManager
      .createNullPrincipal({ userContextId });
    const replacementTab = tabBrowser.addTab("about:blank", {
      userContextId,
      triggeringPrincipal,
      skipAnimation: true,
    });
    tabBrowser.selectedTab = replacementTab;
    tabBrowser.removeTab(initialTab);
  }

  tabBrowser.selectedTab.setAttribute("BMS-webpanel-tab", "true");
  return tabBrowser.selectedBrowser;
}

export class WebsitePanelWindowChild {
  private static instance: WebsitePanelWindowChild;
  static getInstance() {
    if (!WebsitePanelWindowChild.instance) {
      WebsitePanelWindowChild.instance = new WebsitePanelWindowChild();
    }
    return WebsitePanelWindowChild.instance;
  }

  currentURL = new URL(globalThis.location.href);

  get panelSidebarData(): Panels {
    try {
      const parsed = JSON.parse(
        Services.prefs.getStringPref(PANEL_SIDEBAR_DATA_PREF_NAME, "{}"),
      ) as { data?: Panels };
      return parsed.data ?? [];
    } catch (error) {
      console.error(
        "[WebsitePanelWindowChild] Failed to read panel data:",
        error,
      );
      return [];
    }
  }

  get mainWindow() {
    return document?.getElementById("main-window") as HTMLDivElement | null;
  }

  get loadURL() {
    return this.webpanelData?.url ?? "";
  }

  get webpanelId() {
    return this.currentURL.searchParams.get(WEB_PANEL_URL_PARAM);
  }

  get userContextId() {
    return this.webpanelData?.userContextId ?? 0;
  }

  get userAgent() {
    return this.webpanelData?.userAgent;
  }

  get webpanelData() {
    const id = this.webpanelId;
    if (!id) {
      return null;
    }
    return this.panelSidebarData.find((panel) => panel.id === id) ?? null;
  }

  get isBmsWindow() {
    return isWebPanelChildUrl(this.currentURL.href);
  }

  constructor() {
    if (!this.webpanelId) {
      return;
    }

    globalThis.floorpWebPanelWindow = true;
    document?.documentElement?.setAttribute("taskbartab", this.webpanelId);

    void this.initWhenReady();
  }

  private async initWhenReady(): Promise<void> {
    try {
      await this.waitForDocumentReady();
      await this.waitForRequiredElements();
      this.createWebpanelWindow();
    } catch (error) {
      console.error("[WebsitePanelWindowChild] Failed to initialize:", error);
    }
  }

  private waitForDocumentReady(): Promise<void> {
    if (document.readyState !== "loading") {
      return Promise.resolve();
    }

    return new Promise((resolve) => {
      document.addEventListener("DOMContentLoaded", () => resolve(), {
        once: true,
      });
    });
  }

  private async waitForRequiredElements(): Promise<void> {
    for (let attempt = 0; attempt < 100; attempt++) {
      if (
        document.getElementById("main-window") &&
        document.getElementById("browser") &&
        globalThis.gBrowser?.selectedBrowser
      ) {
        return;
      }
      await new Promise((resolve) => globalThis.setTimeout(resolve, 50));
    }

    throw new Error("The web panel tabbrowser was not initialized");
  }

  private hideChromeUi(): void {
    document
      ?.getElementById("navigator-toolbox")
      ?.setAttribute("hidden", "true");
    document?.getElementById("browser")?.setAttribute("data-is-child", "true");
    document?.getElementById("TabsToolbar")?.setAttribute("hidden", "true");
    document?.getElementById("tabbrowser-tabs")?.setAttribute("hidden", "true");

    (
      document?.getElementById("nav-bar") as HTMLElement | null
    )?.style.setProperty("display", "none");

    document
      ?.querySelector(".titlebar-buttonbox-container[skipintoolbarset]")
      ?.remove();

    this.injectWebPanelStyles();
  }

  private injectWebPanelStyles(): void {
    if (document?.getElementById("floorp-webpanel-styles")) {
      return;
    }

    const style = document!.createElement("style");
    style.id = "floorp-webpanel-styles";
    style.textContent = `
      #main-window { min-height: 100%; }
      #browser[data-is-child] {
        flex: 1 !important;
        min-height: 100%;
      }
      #browser[data-is-child] > #sidebar-main,
      #browser[data-is-child] > #sidebar-box,
      #browser[data-is-child] > #sidebar-splitter,
      #browser[data-is-child] > #sidebar-launcher-splitter,
      #browser[data-is-child] > #ai-window-splitter,
      #browser[data-is-child] > #ai-window-box {
        display: none !important;
      }
      #browser[data-is-child] #tabbrowser-tabbox,
      #browser[data-is-child] #tabbrowser-tabpanels {
        flex: 1 !important;
        min-height: 100%;
      }
    `;
    document.documentElement?.appendChild(style);
  }

  private getContentBrowser(): WebPanelBrowserElement {
    const browser = prepareWebPanelTab(
      globalThis.gBrowser as unknown as WebPanelTabBrowser,
      this.userContextId,
    );
    globalThis.floorpWebPanelContentBrowser = browser;
    return browser;
  }

  setZoomLevel(browser: WebPanelBrowserElement) {
    const panelId = this.webpanelId;
    if (!panelId) {
      return;
    }
    applySavedZoomLevel(browser, panelId);
  }

  createWebpanelWindow() {
    const panelId = this.webpanelId;
    const loadURL = this.loadURL;
    const mainWindow = this.mainWindow;

    if (!loadURL || !panelId || !mainWindow) {
      throw new Error("Web panel prerequisites are missing");
    }

    globalThis.floorpBmsUserAgent = this.userAgent;
    globalThis.bmsLoadedURI = loadURL;

    this.hideChromeUi();
    mainWindow.setAttribute("windowtype", "navigator:webpanel");
    mainWindow.setAttribute(
      "chromehidden",
      "toolbar menubar directories extrachrome",
    );

    const browser = this.getContentBrowser();

    globalThis.requestAnimationFrame(() => {
      loadUriInWebPanelBrowser(browser, loadURL);
      this.setZoomLevel(browser);
    });

    Services.prefs.addObserver(PANEL_SIDEBAR_DATA_PREF_NAME, () => {
      this.setZoomLevel(browser);
    });
  }
}
