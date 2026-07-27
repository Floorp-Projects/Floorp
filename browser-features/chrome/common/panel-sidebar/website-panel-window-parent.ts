/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { setPanelSidebarData } from "./data/data.ts";
import {
  getPanelDataById,
  getWebPanelChromeWindow,
  getWebPanelContentBrowser,
  loadUriInWebPanelBrowser,
  saveZoomLevel,
  type WebPanelBrowserElement,
} from "./utils/web-panel-browser.ts";

export class WebsitePanel {
  private static instance: WebsitePanel;

  static getInstance() {
    if (!WebsitePanel.instance) {
      WebsitePanel.instance = new WebsitePanel();
    }
    return WebsitePanel.instance;
  }

  private getContentBrowser(webpanelId: string, parentWindow: Window) {
    const browser = getWebPanelContentBrowser(webpanelId, parentWindow);
    if (!browser) {
      throw new Error("Target panel content browser not found");
    }
    return browser;
  }

  public toggleMutePanel(webpanelId: string) {
    try {
      const browser = this.getContentBrowser(webpanelId, window);
      browser.audioMuted = !browser.audioMuted;
    } catch (e) {
      console.error("Failed to toggle mute for webpanel", e);
    }
  }

  public reloadPanel(webpanelId: string) {
    try {
      const browser = this.getContentBrowser(webpanelId, window);
      browser.reload?.();
    } catch (e) {
      console.error("Failed to reload webpanel", e);
    }
  }

  public goForwardPanel(webpanelId: string) {
    try {
      const browser = this.getContentBrowser(webpanelId, window);
      browser.goForward?.();
    } catch (e) {
      console.error("Failed to go forward in webpanel", e);
    }
  }

  public goBackPanel(webpanelId: string) {
    try {
      const browser = this.getContentBrowser(webpanelId, window);
      browser.goBack?.();
    } catch (e) {
      console.error("Failed to go back in webpanel", e);
    }
  }

  public goIndexPagePanel(webpanelId: string) {
    try {
      const browser = this.getContentBrowser(webpanelId, window);
      const chromeWindow = getWebPanelChromeWindow(webpanelId, window);
      const uri = chromeWindow?.bmsLoadedURI ??
        getPanelDataById(webpanelId)?.url ??
        "";
      if (!uri) {
        return;
      }
      loadUriInWebPanelBrowser(browser, uri);
    } catch (e) {
      console.error("Failed to go to index page in webpanel", e);
    }
  }

  private persistZoomLevel(webpanelId: string, zoomLevel: number) {
    setPanelSidebarData((prev) => {
      Object.values(prev).forEach((panel) => {
        if (panel.id === webpanelId) {
          panel.zoomLevel = zoomLevel;
        }
      });
      return prev;
    });
    saveZoomLevel(webpanelId, zoomLevel);
  }

  private adjustZoom(
    webpanelId: string,
    adjust: (browser: WebPanelBrowserElement) => number,
  ) {
    const browser = this.getContentBrowser(webpanelId, window);
    if (typeof browser.fullZoom !== "number") {
      throw new Error("Browser zoom is unavailable");
    }

    const newZoomLevel = adjust(browser);
    browser.fullZoom = newZoomLevel;
    this.persistZoomLevel(webpanelId, newZoomLevel);
  }

  public zoomInPanel(webpanelId: string) {
    try {
      this.adjustZoom(webpanelId, (browser) => (browser.fullZoom ?? 1) * 1.1);
    } catch (e) {
      console.error("Failed to zoom in webpanel", e);
    }
  }

  public zoomOutPanel(webpanelId: string) {
    try {
      this.adjustZoom(webpanelId, (browser) => (browser.fullZoom ?? 1) / 1.1);
    } catch (e) {
      console.error("Failed to zoom out webpanel", e);
    }
  }

  public resetZoomLevelPanel(webpanelId: string) {
    try {
      this.adjustZoom(webpanelId, () => 1);
    } catch (e) {
      console.error("Failed to reset zoom in webpanel", e);
    }
  }
}
