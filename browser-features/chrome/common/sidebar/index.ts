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
  requestPanelSelection(panelId: string): Promise<void>;
  registerSidebarIcon(options: {
    name: string;
    i18nName: string;
    iconUrl: string;
    birpcMethodName: string;
  }): Promise<void>;
}

interface SidebarClientFunctions {
  onPanelDataUpdate(data: any): void;
  onPanelSelectionChange(panelId: string): void;
  // Dynamic callback methods will be registered based on birpcMethodName
  [key: string]: any;
}

interface SidebarIconRegistration {
  name: string;
  i18nName: string;
  iconUrl: string;
  birpcMethodName: string;
}

@noraComponent(import.meta.hot)
export default class Sidebar extends NoraComponentBase {
  private rpc: ReturnType<typeof createBirpc<SidebarServerFunctions, SidebarClientFunctions>> | null = null;
  private rpcRequestObserver: nsIObserver | null = null;
  private addonPanelRpc: ReturnType<typeof createBirpc<SidebarClientFunctions, SidebarServerFunctions>> | null = null;
  private registeredIcons: Map<string, SidebarIconRegistration> = new Map();

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
      requestPanelSelection: async (panelId: string): Promise<void> => {
        // Update selected panel
        setSelectedPanelId(panelId);
        // Notify addon panel about the change
        if (this.addonPanelRpc) {
          this.addonPanelRpc.onPanelSelectionChange(panelId);
        }
      },
      registerSidebarIcon: async (options: {
        name: string;
        i18nName: string;
        iconUrl: string;
        birpcMethodName: string;
      }): Promise<void> => {
        // Register the sidebar icon and store the callback method name
        this.registeredIcons.set(options.name, options);
        
        // Add dynamic method to addonPanelRpc if it exists
        if (this.addonPanelRpc) {
          (this.addonPanelRpc as any)[options.birpcMethodName] = () => {
            console.debug(`Sidebar: Icon ${options.name} callback triggered`);
            // Notify that this icon was activated
            Services.obs.notifyObservers(
              { 
                iconName: options.name,
                i18nName: options.i18nName,
                iconUrl: options.iconUrl 
              } as nsISupports,
              "noraneko-sidebar-icon-activated"
            );
          };
        }
        
        console.debug(`Sidebar: Registered icon ${options.name} with callback ${options.birpcMethodName}`);
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
    
    // Only notify via Services.obs for any other observers (no birpc for config)
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

  // Public API methods for sidebar icon registration and management
  public async registerSidebarIcon(options: {
    name: string;
    i18nName: string;
    iconUrl: string;
    birpcMethodName: string;
  }): Promise<void> {
    if (this.rpc) {
      await this.rpc.registerSidebarIcon(options);
    }
  }

  public getRegisteredIcons(): SidebarIconRegistration[] {
    return Array.from(this.registeredIcons.values());
  }
}