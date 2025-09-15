/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { setPanelSidebarData } from "./data/data.ts";

type XULBrowserElement = XULElement & {
  browsingContext: {
    associatedWindow: Window;
  };
};

export class WebsitePanel {
  private static instance: WebsitePanel;

  static getInstance() {
    if (!WebsitePanel.instance) {
      WebsitePanel.instance = new WebsitePanel();
    }
    return WebsitePanel.instance;
  }

  private getWindowByWebpanelId(id: string, parentWindow: Window) {
    const webpanelBrowserId = `sidebar-panel-${id}`;
    const webpanelBrowser = parentWindow?.document?.getElementById(
      webpanelBrowserId,
    ) as XULBrowserElement;

    if (!webpanelBrowser) {
      throw new Error("Target panel window not found");
    }

    return webpanelBrowser.browsingContext.associatedWindow;
  }

  private safe<T = void>(fn: () => T, errMsg: string) {
    try {
      return fn();
    } catch (e) {
      console.error(errMsg, e);
      return undefined as unknown as T;
    }
  }

  public toggleMutePanel(webpanelId: string) {
    return this.safe(() => {
      const targetPanelWindow = this.getWindowByWebpanelId(webpanelId, window);
      const tab = targetPanelWindow.gBrowser.selectedTab;
      tab.linkedBrowser.audioMuted = !tab.linkedBrowser.audioMuted;
    }, "Failed to toggle mute for webpanel");
  }

  public reloadPanel(webpanelId: string) {
    return this.safe(() => {
      const targetPanelWindow = this.getWindowByWebpanelId(webpanelId, window);
      targetPanelWindow.gBrowser.selectedTab.linkedBrowser.reload();
    }, "Failed to reload webpanel");
  }

  public goForwardPanel(webpanelId: string) {
    return this.safe(() => {
      const targetPanelWindow = this.getWindowByWebpanelId(webpanelId, window);
      targetPanelWindow.gBrowser.selectedTab.linkedBrowser.goForward();
    }, "Failed to go forward in webpanel");
  }

  public goBackPanel(webpanelId: string) {
    return this.safe(() => {
      const targetPanelWindow = this.getWindowByWebpanelId(webpanelId, window);
      targetPanelWindow.gBrowser.selectedTab.linkedBrowser.goBack();
    }, "Failed to go back in webpanel");
  }

  public goIndexPagePanel(webpanelId: string) {
    return this.safe(() => {
      const targetPanelWindow = this.getWindowByWebpanelId(webpanelId, window);
      const uri = targetPanelWindow.bmsLoadedURI;
      targetPanelWindow.gBrowser.loadURI(Services.io.newURI(uri), {
        triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      });
    }, "Failed to go to index page in webpanel");
  }

  private saveZoomLevel(webpanelId: string, zoomLevel: number) {
    setPanelSidebarData((prev) => {
      Object.values(prev).forEach((panel) => {
        if (panel.id === webpanelId) {
          panel.zoomLevel = zoomLevel;
        }
      });
      return prev;
    });
  }

  public zoomInPanel(webpanelId: string) {
    return this.safe(() => {
      const targetPanelWindow = this.getWindowByWebpanelId(webpanelId, window);
      targetPanelWindow.ZoomManager.enlarge();
      this.saveZoomLevel(webpanelId, targetPanelWindow.ZoomManager.zoom);
    }, "Failed to zoom in webpanel");
  }

  public zoomOutPanel(webpanelId: string) {
    return this.safe(() => {
      const targetPanelWindow = this.getWindowByWebpanelId(webpanelId, window);
      targetPanelWindow.ZoomManager.reduce();
      this.saveZoomLevel(webpanelId, targetPanelWindow.ZoomManager.zoom);
    }, "Failed to zoom out webpanel");
  }

  public resetZoomLevelPanel(webpanelId: string) {
    return this.safe(() => {
      const targetPanelWindow = this.getWindowByWebpanelId(webpanelId, window);
      targetPanelWindow.ZoomManager.zoom = 1;
      this.saveZoomLevel(webpanelId, 1);
    }, "Failed to reset zoom in webpanel");
  }
}
