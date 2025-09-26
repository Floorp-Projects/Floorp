/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createBirpc } from "birpc";
import { noraComponent, NoraComponentBase } from "#features-chrome/utils/base";
import { onCleanup } from "solid-js";
import { 
  CPanelSidebar,
  PanelSidebarElem,
  SidebarContextMenuElem,
  PanelSidebarAddModal,
  PanelSidebarFloating,
} from "./ui";
import { WebsitePanelWindowChild } from "./panel/website-panel-window-child";
import { migratePanelSidebarData } from "./data/migration.ts";

// Define communication interfaces for sidebar addon panel
interface SidebarAddonPanelServerFunctions {
  onPanelDataUpdate(data: any): void;
  onPanelConfigUpdate(config: any): void;
  onPanelSelectionChange(panelId: string): void;
}

interface SidebarAddonPanelClientFunctions {
  requestDataUpdate(): Promise<any>;
  requestConfigUpdate(): Promise<any>;
  requestPanelSelection(panelId: string): Promise<void>;
}

@noraComponent(import.meta.hot)
export default class SidebarAddonPanel extends NoraComponentBase {
  private rpc: ReturnType<typeof createBirpc<SidebarAddonPanelServerFunctions, SidebarAddonPanelClientFunctions>> | null = null;
  private observer: nsIObserver | null = null;
  private ctx: CPanelSidebar | null = null;

  init(): void {
    // Run data migration first
    migratePanelSidebarData();

    // Initialize UI components
    this.ctx = new CPanelSidebar();
    WebsitePanelWindowChild.getInstance();
    new PanelSidebarElem(this.ctx);
    new SidebarContextMenuElem(this.ctx);
    PanelSidebarAddModal.getInstance();
    PanelSidebarFloating.getInstance();

    // Set up birpc communication with sidebar core
    this.setupBirpcCommunication();
  }

  private setupBirpcCommunication(): void {
    const serverFunctions: SidebarAddonPanelServerFunctions = {
      onPanelDataUpdate: (data: any) => {
        // Handle panel data updates from sidebar core
        console.debug("SidebarAddonPanel: Received panel data update", data);
        // Update UI components with new data
        if (this.ctx) {
          // Trigger UI update
          Services.obs.notifyObservers(
            { type: "panel-data-update", data } as nsISupports,
            "noraneko-addon-panel-internal-update"
          );
        }
      },
      onPanelConfigUpdate: (config: any) => {
        // Handle panel config updates from sidebar core
        console.debug("SidebarAddonPanel: Received panel config update", config);
        Services.obs.notifyObservers(
          { type: "panel-config-update", config } as nsISupports,
          "noraneko-addon-panel-internal-update"
        );
      },
      onPanelSelectionChange: (panelId: string) => {
        // Handle panel selection changes from sidebar core
        console.debug("SidebarAddonPanel: Panel selection changed to", panelId);
        Services.obs.notifyObservers(
          { type: "panel-selection-change", panelId } as nsISupports,
          "noraneko-addon-panel-internal-update"
        );
      },
    };

    // Create birpc instance for communication with sidebar core
    this.rpc = createBirpc<SidebarAddonPanelServerFunctions, SidebarAddonPanelClientFunctions>(
      serverFunctions,
      {
        post: (data) => {
          // Send RPC messages to sidebar core via Services.obs
          Services.obs.notifyObservers(
            { type: "rpc-request", data: JSON.stringify(data) } as nsISupports,
            "noraneko-sidebar-addon-panel-rpc"
          );
        },
        on: (fn) => {
          // Create observer to receive RPC responses from sidebar core
          this.observer = {
            observe: (subject: nsISupports, topic: string, data: string) => {
              if (topic === "noraneko-sidebar-addon-panel-rpc-response") {
                const msgData = (subject as any).data;
                if (msgData) {
                  try {
                    fn(JSON.parse(msgData));
                  } catch (e) {
                    console.error("SidebarAddonPanel: Failed to parse RPC response:", e);
                  }
                }
              }
            },
            QueryInterface: () => this.observer,
          } as nsIObserver;

          Services.obs.addObserver(this.observer, "noraneko-sidebar-addon-panel-rpc-response", false);
        },
        serialize: (v) => JSON.stringify(v),
        deserialize: (v) => JSON.parse(v),
      }
    );

    // Set up cleanup
    onCleanup(() => {
      if (this.observer) {
        Services.obs.removeObserver(this.observer, "noraneko-sidebar-addon-panel-rpc-response");
        this.observer = null;
      }
      this.rpc = null;
      this.ctx = null;
    });
  }

  // Public API methods for requesting data from sidebar core
  public async requestDataUpdate(): Promise<any> {
    if (this.rpc) {
      return await this.rpc.requestDataUpdate();
    }
    return null;
  }

  public async requestConfigUpdate(): Promise<any> {
    if (this.rpc) {
      return await this.rpc.requestConfigUpdate();
    }
    return null;
  }

  public async requestPanelSelection(panelId: string): Promise<void> {
    if (this.rpc) {
      await this.rpc.requestPanelSelection(panelId);
    }
  }
}

/* Re-export UI components for backward compatibility */
export { CPanelSidebar } from "./ui/components/panel-sidebar.tsx";
export { SidebarContextMenuElem } from "./ui/components/sidebar-contextMenu.tsx";
export { PanelSidebarAddModal } from "./ui/components/panel-sidebar-modal.tsx";
export { PanelSidebarFloating } from "./ui/components/floating.tsx";
export { BrowserBox } from "./ui/components/browser-box.tsx";
export { FloatingSplitter } from "./ui/components/floating-splitter.tsx";
export { SidebarHeader } from "./ui/components/sidebar-header.tsx";
export { PanelSidebarButton, SidebarPanelButton } from "./ui/components/sidebar-panel-button.tsx";
export { SidebarSelectbox } from "./ui/components/sidebar-selectbox.tsx";
export { SidebarSplitter } from "./ui/components/sidebar-splitter.tsx";
export { PanelSidebarElem, PanelSidebarElem as Sidebar } from "./ui/components/sidebar.tsx";

/* Re-export panel APIs */
export * from "./panel";

/* Styles (now sourced from our own styles directory) */
export { default as style } from "./ui/styles/style.css?inline";