/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { PanelSidebarStaticNames } from "./panel-sidebar-static-names.ts";
import type { Panel } from "./type.ts";

export const WEB_PANEL_CONTENT_BROWSER_ID = "floorp-webpanel-content-browser";

const PANEL_SIDEBAR_DATA_PREF_NAME =
  PanelSidebarStaticNames.panelSidebarDataPrefName;

export type WebPanelBrowserElement = XULBrowserElement & {
  audioMuted?: boolean;
  fullZoom?: number;
  reload?: () => void;
  goBack?: () => void;
  goForward?: () => void;
};

type XULSidebarBrowserElement = XULElement & {
  browsingContext: {
    associatedWindow: Window & {
      floorpWebPanelContentBrowser?: WebPanelBrowserElement;
      bmsLoadedURI?: string;
    };
  };
};

export function getPanelDataById(panelId: string): Panel | null {
  try {
    const raw = Services.prefs.getStringPref(
      PANEL_SIDEBAR_DATA_PREF_NAME,
      "{}",
    );
    const parsed = JSON.parse(raw) as { data?: Panel[] };
    return parsed.data?.find((panel) => panel.id === panelId) ?? null;
  } catch (error) {
    console.error("[WebPanelBrowser] Failed to read panel data:", error);
    return null;
  }
}

/** Chrome window embedded inside the sidebar `<browser>`. */
export function getWebPanelChromeWindow(
  webpanelId: string,
  parentWindow: Window = globalThis as unknown as Window,
): (Window & { floorpWebPanelContentBrowser?: WebPanelBrowserElement }) | null {
  const sidebarBrowser = parentWindow.document?.getElementById(
    `sidebar-panel-${webpanelId}`,
  ) as XULSidebarBrowserElement | null;

  if (!sidebarBrowser?.browsingContext?.associatedWindow) {
    return null;
  }

  return sidebarBrowser.browsingContext.associatedWindow;
}

/** Content `<browser>` that displays the web panel URL inside browser.xhtml. */
export function getWebPanelContentBrowser(
  webpanelId: string,
  parentWindow: Window = globalThis as unknown as Window,
): WebPanelBrowserElement | null {
  const chromeWindow = getWebPanelChromeWindow(webpanelId, parentWindow);
  if (!chromeWindow) {
    return null;
  }

  return (
    chromeWindow.floorpWebPanelContentBrowser ??
      (chromeWindow.document?.getElementById(
        WEB_PANEL_CONTENT_BROWSER_ID,
      ) as WebPanelBrowserElement | null)
  );
}

export function loadUriInWebPanelBrowser(
  browser: WebPanelBrowserElement,
  url: string,
): void {
  const principal = Services.scriptSecurityManager.getSystemPrincipal();
  const loadURIOptions = {
    triggeringPrincipal: principal,
  };

  if (typeof browser.loadURI !== "function") {
    throw new Error("browser.loadURI is not available");
  }

  browser.loadURI(Services.io.newURI(url), loadURIOptions);
}

export function applySavedZoomLevel(
  browser: WebPanelBrowserElement,
  panelId: string,
): void {
  const zoomLevel = getPanelDataById(panelId)?.zoomLevel;
  if (zoomLevel && typeof browser.fullZoom === "number") {
    browser.fullZoom = zoomLevel;
  }
}

export function saveZoomLevel(webpanelId: string, zoomLevel: number): void {
  try {
    const raw = Services.prefs.getStringPref(
      PANEL_SIDEBAR_DATA_PREF_NAME,
      "{}",
    );
    const parsed = JSON.parse(raw) as { data?: Panel[] };
    if (!parsed.data) {
      return;
    }

    for (const panel of parsed.data) {
      if (panel.id === webpanelId) {
        panel.zoomLevel = zoomLevel;
      }
    }

    Services.prefs.setStringPref(
      PANEL_SIDEBAR_DATA_PREF_NAME,
      JSON.stringify(parsed),
    );
  } catch (error) {
    console.error("[WebPanelBrowser] Failed to save zoom level:", error);
  }
}
