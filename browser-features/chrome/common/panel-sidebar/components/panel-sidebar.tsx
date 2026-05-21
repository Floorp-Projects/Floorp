/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { render } from "preact";
import { effect } from "@preact/signals";
import { ChromeSiteBrowser } from "../browsers/chrome-site-browser.tsx";
import { ExtensionSiteBrowser } from "../browsers/extension-site-browser.tsx";
import { WebSiteBrowser } from "../browsers/web-site-browser.tsx";
import {
  panelSidebarConfig,
  panelSidebarData,
  selectedPanelId,
  setPanelSidebarData,
  setSelectedPanelId,
} from "../data/data.ts";
import type { Panel } from "../utils/type.ts";
import { getExtensionSidebarAction } from "../extension-panels.ts";
import { WebsitePanel } from "../website-panel-window-parent.ts";
import "../utils/webRequest.ts";

export class CPanelSidebar {
  private panelDisposers: Map<string, () => void> = new Map();
  private panelContainers: Map<string, Element> = new Map();

  private get parentElement() {
    return document?.getElementById("panel-sidebar-browser-box") as
      unknown as XULElement | undefined;
  }

  private get sidebarElement() {
    return document?.getElementById("panel-sidebar-box") as
      unknown as XULElement | undefined;
  }

  constructor() {
    effect(() => {
      // Only clear selection marker from sidebar panels (not workspaces).
      const currentCheckedPanels = Array.from(
        document?.querySelectorAll(
          ".panel-sidebar-panel[data-checked][data-panel-id]",
        ) ?? [],
      );
      // Use forEach instead of map to avoid type error
      (currentCheckedPanels as unknown as XULElement[]).forEach((panel) => {
        panel.removeAttribute("data-checked");
      });

      const currentPanel = this.getPanelData(selectedPanelId.value ?? "");
      if (currentPanel) {
        document
          // Select by data-panel-id to avoid touching workspace entries.
          ?.querySelector(
            `.panel-sidebar-panel[data-panel-id="${currentPanel.id}"]`,
          )
          ?.setAttribute("data-checked", "true");
      }
    });
  }

  public getBrowserElement(id: string) {
    return document?.getElementById(`sidebar-panel-${id}`) as
      unknown as (XULElement & {
        contentWindow: Window;
        goBack: () => void;
        goForward: () => void;
        goIndex: () => void;
        reload: () => void;
        toggleMute: () => void;
      }) | undefined;
  }

  public getPanelData(id: string): Panel | undefined {
    return panelSidebarData.value.find((panel) => panel.id === id);
  }

  private createBrowserComponent(panel: Panel) {
    const components = {
      web: WebSiteBrowser,
      extension: ExtensionSiteBrowser,
      static: ChromeSiteBrowser,
    };

    const BrowserComponent = components[panel.type];
    if (!BrowserComponent) {
      throw new Error(`Unsupported panel type: ${panel.type}`);
    }

    return <BrowserComponent {...panel} />;
  }

  private resetBrowsersFlex(): void {
    for (const container of this.panelContainers.values()) {
      (container as unknown as XULElement).removeAttribute("flex");
    }
  }

  private renderBrowserComponent(panel: Panel): void {
    if (!this.parentElement) {
      throw new Error("Parent element not found");
    }

    // Dispose previous container for this panel if it exists
    const prevDispose = this.panelDisposers.get(panel.id);
    if (prevDispose) {
      try {
        prevDispose();
      } catch (e) {
        console.warn("panel dispose failed", e);
      }
      this.panelDisposers.delete(panel.id);
    }

    // Create a dedicated container so we can cleanly unmount
    const container = document?.createXULElement("vbox") as unknown as XULElement;
    container.setAttribute("flex", "1");
    this.parentElement.appendChild(container);
    this.panelContainers.set(panel.id, container);

    render(this.createBrowserComponent(panel), container as unknown as Element);

    this.panelDisposers.set(panel.id, () => {
      render(null, container as unknown as Element);
      container.remove();
      this.panelContainers.delete(panel.id);
    });

    this.initBrowser(panel);
  }

  private initBrowser(panel: Panel) {
    if (panel.type === "extension") {
      const browser = this.getBrowserElement(panel.id) as unknown as XULElement & {
        contentWindow: Window;
      };

      if (!browser) {
        throw new Error("Browser element not found");
      }

      if (!panel.extensionId) {
        throw new Error("Extension ID not found");
      }

      const sidebarAction = getExtensionSidebarAction(panel.extensionId);

      browser.addEventListener("DOMContentLoaded", () => {
        const oa = globalThis.E10SUtils.predictOriginAttributes({ browser });
        browser.setAttribute(
          "remoteType",
          globalThis.E10SUtils.getRemoteTypeForURI(
            panel.url ?? "",
            true,
            false,
            globalThis.E10SUtils.EXTENSION_REMOTE_TYPE,
            null,
            oa,
          ),
        );

        browser.contentWindow.loadPanel(
          panel.extensionId,
          sidebarAction.default_panel,
          "sidebar",
        );
      });
    }
  }

  public changePanel(panelId: string): void {
    if (panelId === selectedPanelId.value) {
      setSelectedPanelId(null);
      if (panelSidebarConfig.value.autoUnload) {
        this.unloadPanel(panelId);
      }
      return;
    }

    const panel = this.getPanelData(panelId);
    if (!panel) {
      throw new Error(`Panel not found: ${panelId}`);
    }

    setSelectedPanelId(panelId);
    this.setSidebarWidth(panel);
    this.resetBrowsersFlex();
    this.showPanel(panel);
  }

  public showPanel(panel: Panel): void {
    const container = this.panelContainers.get(panel.id);
    if (container) {
      this.resetBrowsersFlex();
      container.setAttribute("flex", "1");
      return;
    }
    this.renderBrowserComponent(panel);
  }

  public saveCurrentSidebarWidth() {
    const currentWidth = this.sidebarElement?.getAttribute("width");
    if (currentWidth) {
      setPanelSidebarData((prev) =>
        prev.map((panel) =>
          panel.id === selectedPanelId.value
            ? { ...panel, width: Number(currentWidth) }
            : panel
        )
      );
    }
  }

  private setSidebarWidth(panel: Panel) {
    this.sidebarElement?.style.setProperty(
      "width",
      `${panel.width !== 0 ? panel.width : panelSidebarConfig.value.globalWidth}px`,
    );
  }

  public openInMainWindow(panelId: string) {
    const url = this.getPanelData(panelId)?.url ?? "";
    const userContextId = this.getPanelData(panelId)?.userContextId ??
      undefined;
    globalThis.gBrowser.addTab(url, {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      inBackground: false,
      userContextId: userContextId,
    });
  }

  public deletePanel(panelId: string) {
    this.unloadPanel(panelId);
    setPanelSidebarData((prev) => prev.filter((p) => p.id !== panelId));
  }

  public unloadPanel(panelId: string) {
    // Cleanup preact root for this panel if present
    const dispose = this.panelDisposers.get(panelId);
    if (dispose) {
      try {
        dispose();
      } catch (e) {
        console.warn("panel dispose failed", e);
      }
      this.panelDisposers.delete(panelId);
    }

    setSelectedPanelId(null);
  }

  public mutePanel(panelId: string) {
    const gWebsitePanel = WebsitePanel.getInstance();
    gWebsitePanel.toggleMutePanel(panelId);
  }

  public changeZoomLevel(panelId: string, type: "in" | "out" | "reset") {
    const gWebsitePanel = WebsitePanel.getInstance();

    switch (type) {
      case "in":
        gWebsitePanel.zoomInPanel(panelId);
        break;
      case "out":
        gWebsitePanel.zoomOutPanel(panelId);
        break;
      case "reset":
        gWebsitePanel.resetZoomLevelPanel(panelId);
        break;
    }
  }

  public changeUserAgent(panelId: string) {
    const panel = this.getPanelData(panelId);
    if (!panel) {
      throw new Error(`Panel not found: ${panelId}`);
    }

    // Toggle the userAgent property for the specified panel
    setPanelSidebarData((prev) =>
      prev.map((p) => p.id === panelId ? { ...p, userAgent: !p.userAgent } : p)
    );

    // Unload and reload the panel to apply the new user agent
    this.unloadPanel(panelId);
    this.changePanel(panelId);
  }
}
