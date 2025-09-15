/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { CPanelSidebar } from "./components/panel-sidebar.tsx";
import { WebsitePanel } from "./website-panel-window-parent.ts";

export namespace PanelNavigator {
  export let gPanelSidebar: CPanelSidebar | null = null;

  function call<T = void>(
    sideBarId: string,
    websiteFn?: (id: string) => T,
    sidebarFn?: (id: string) => T,
  ): T | undefined {
    try {
      const wp = WebsitePanel.getInstance();
      if (websiteFn && wp) {
        try {
          return websiteFn(sideBarId);
        } catch (e) {
          console.warn("WebsitePanel handler failed, falling back:", e);
        }
      }
    } catch {
      // ignore
    }

    if (gPanelSidebar && sidebarFn) {
      try {
        return sidebarFn(sideBarId);
      } catch (e) {
        console.error("Sidebar handler failed:", e);
      }
    }

    return undefined;
  }

  /* Navigation */
  export function back(sideBarId: string) {
    return call(sideBarId, (id) => WebsitePanel.getInstance().goBackPanel(id), (id) =>
      gPanelSidebar?.getBrowserElement(id)?.goBack(),
    );
  }

  export function forward(sideBarId: string) {
    return call(sideBarId, (id) => WebsitePanel.getInstance().goForwardPanel(id), (id) =>
      gPanelSidebar?.getBrowserElement(id)?.goForward(),
    );
  }

  export function reload(sideBarId: string) {
    return call(sideBarId, (id) => WebsitePanel.getInstance().reloadPanel(id), (id) =>
      gPanelSidebar?.getBrowserElement(id)?.reload(),
    );
  }

  export function goIndexPage(sideBarId: string) {
    return call(
      sideBarId,
      (id) => WebsitePanel.getInstance().goIndexPagePanel(id),
      (id) => {
        const browser = gPanelSidebar?.getBrowserElement(id) as XULElement | undefined;
        if (browser) {
          browser.src = "";
          browser.src = gPanelSidebar?.getPanelData(id)?.url ?? "";
        }
      },
    );
  }

  /* Mute/Unmute */
  export function toggleMute(sideBarId: string) {
    return call(sideBarId, (id) => WebsitePanel.getInstance().toggleMutePanel(id), (id) =>
      gPanelSidebar?.getBrowserElement(id)?.toggleMute(),
    );
  }

  /* Zoom (prefer WebsitePanel API) */
  export function zoomIn(sideBarId: string) {
    try {
      WebsitePanel.getInstance().zoomInPanel(sideBarId);
    } catch (e) {
      console.error("zoomIn failed:", e);
    }
  }

  export function zoomOut(sideBarId: string) {
    try {
      WebsitePanel.getInstance().zoomOutPanel(sideBarId);
    } catch (e) {
      console.error("zoomOut failed:", e);
    }
  }

  export function zoomReset(sideBarId: string) {
    try {
      WebsitePanel.getInstance().resetZoomLevelPanel(sideBarId);
    } catch (e) {
      console.error("zoomReset failed:", e);
    }
  }
}
