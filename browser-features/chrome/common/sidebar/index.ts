/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createBirpc } from "birpc";
import { noraComponent, NoraComponentBase } from "#features-chrome/utils/base";
import { onCleanup } from "solid-js";
import {
  panelSidebarData,
  setPanelSidebarData,
  panelSidebarConfig,
  setPanelSidebarConfig,
  selectedPanelId,
  setSelectedPanelId,
} from "./core/data.ts";

// Define communication interfaces for sidebar core
interface SidebarServerFunctions {
  requestDataUpdate(): Promise<any>;
  requestConfigUpdate(): Promise<any>;
  requestPanelSelection(panelId: string): Promise<void>;
}

interface SidebarClientFunctions {
  onPanelDataUpdate(data: any): void;
  onPanelConfigUpdate(config: any): void;
  onPanelSelectionChange(panelId: string): void;
}

@noraComponent(import.meta.hot)
export default class Sidebar extends NoraComponentBase {
  private rpc: ReturnType<typeof createBirpc<SidebarServerFunctions, SidebarClientFunctions>> | null = null;
  private rpcRequestObserver: nsIObserver | null = null;
  private addonPanelRpc: ReturnType<typeof createBirpc<SidebarClientFunctions, SidebarServerFunctions>> | null = null;

  init(): void {
    // Initialize birpc communication using Services.obs
    this.setupBirpcCommunication();
  }

  private setupBirpcCommunication(): void {
    const serverFunctions: SidebarServerFunctions = {
      requestDataUpdate: async (): Promise<any> => {
        // Return current panel sidebar data
        return panelSidebarData();
      },
      requestConfigUpdate: async (): Promise<any> => {
        // Return current panel sidebar config
        return panelSidebarConfig();
      },
      requestPanelSelection: async (panelId: string): Promise<void> => {
        // Update selected panel
        setSelectedPanelId(panelId);
        // Notify addon panel about the change
        if (this.addonPanelRpc) {
          this.addonPanelRpc.onPanelSelectionChange(panelId);
        }
      },
    };

    // Create birpc instance to handle requests from addon panel
    this.rpc = createBirpc<SidebarServerFunctions, SidebarClientFunctions>(
      serverFunctions,
      {
        post: (data) => {
          // Send RPC responses back to addon panel
          Services.obs.notifyObservers(
            { data: JSON.stringify(data) } as nsISupports,
            "noraneko-sidebar-addon-panel-rpc-response"
          );
        },
        on: (fn) => {
          // Listen for RPC requests from addon panel
          this.rpcRequestObserver = (subject: nsISupports, topic: string, data: string) => {
            if (topic === "noraneko-sidebar-addon-panel-rpc") {
              const msgData = (subject as any).data;
              if (msgData) {
                try {
                  fn(JSON.parse(msgData));
                } catch (e) {
                  console.error("Sidebar: Failed to parse RPC request:", e);
                }
              }
            }
          };

          Services.obs.addObserver(this.rpcRequestObserver, "noraneko-sidebar-addon-panel-rpc", false);
        },
        serialize: (v) => JSON.stringify(v),
        deserialize: (v) => JSON.parse(v),
      }
    );

    // Create birpc instance to send updates to addon panel
    this.addonPanelRpc = createBirpc<SidebarClientFunctions, SidebarServerFunctions>(
      {},
      {
        post: (data) => {
          // Send updates to addon panel
          Services.obs.notifyObservers(
            { data: JSON.stringify(data) } as nsISupports,
            "noraneko-sidebar-addon-panel-rpc-response"
          );
        },
        on: () => {
          // Not used for this direction of communication
        },
        serialize: (v) => JSON.stringify(v),
        deserialize: (v) => JSON.parse(v),
      }
    );

    // Set up data watchers to notify addon panel of changes
    this.setupDataWatchers();

    // Set up cleanup
    onCleanup(() => {
      if (this.rpcRequestObserver) {
        Services.obs.removeObserver(this.rpcRequestObserver, "noraneko-sidebar-addon-panel-rpc");
        this.rpcRequestObserver = null;
      }
      this.rpc = null;
      this.addonPanelRpc = null;
    });
  }

  private setupDataWatchers(): void {
    // Watch for data changes and notify addon panel
    // Note: In a real implementation, you would set up reactive effects
    // to watch the data signals and call these methods when they change
    
    // For now, we'll provide methods that can be called manually
    // In the real implementation, these would be triggered by solid-js effects
  }

  // Public API methods that can be called by other components
  public async notifyDataChanged(data: any): Promise<void> {
    setPanelSidebarData(data);
    if (this.addonPanelRpc) {
      this.addonPanelRpc.onPanelDataUpdate(data);
    }
    
    // Also notify via Services.obs for any other observers
    Services.obs.notifyObservers(
      { data } as nsISupports,
      "noraneko-sidebar-data-changed"
    );
  }

  public async notifyConfigChanged(config: any): Promise<void> {
    setPanelSidebarConfig(config);
    if (this.addonPanelRpc) {
      this.addonPanelRpc.onPanelConfigUpdate(config);
    }
    
    // Also notify via Services.obs for any other observers
    Services.obs.notifyObservers(
      { config } as nsISupports,
      "noraneko-sidebar-config-changed"
    );
  }

  public async selectPanel(panelId: string): Promise<void> {
    setSelectedPanelId(panelId);
    if (this.addonPanelRpc) {
      this.addonPanelRpc.onPanelSelectionChange(panelId);
    }
    
    // Also notify via Services.obs for any other observers
    Services.obs.notifyObservers(
      { panelId } as nsISupports,
      "noraneko-sidebar-panel-selected"
    );
  }
}