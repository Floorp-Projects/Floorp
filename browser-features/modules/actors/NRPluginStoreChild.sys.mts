/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * NRPluginStoreChild - Content process actor for Floorp OS Plugin Store integration
 *
 * This actor runs in the content process and provides functionality to:
 * - Expose Floorp Plugin Store API to web pages
 * - Handle plugin installation requests from the web store
 * - Communicate with the parent process for plugin management
 *
 * @module NRPluginStoreChild
 */

// =============================================================================
// Types
// =============================================================================

export interface PluginMetadata {
  id: string;
  name: string;
  description: string;
  version?: string;
  author: string;
  functions?: PluginFunction[];
  category?: string;
  isOfficial?: boolean;
  icon?: string;
  /** Plugin package URI for installation via Sapphillon backend */
  uri?: string;
}

export interface PluginFunction {
  name: string;
  description: string;
  parameters?: string[];
}

export interface InstallResult {
  success: boolean;
  error?: string;
  pluginId?: string;
}

// =============================================================================
// Main Class
// =============================================================================

export class NRPluginStoreChild extends JSWindowActorChild {
  private apiInjected = false;

  /**
   * Called when the actor is created
   */
  actorCreated(): void {
    console.debug(
      "[NRPluginStoreChild] Actor created for:",
      this.contentWindow?.location.href,
    );
  }

  /**
   * Handle DOM events
   */
  handleEvent(event: Event): void {
    console.debug("[NRPluginStoreChild] Event received:", event.type);
    switch (event.type) {
      case "DOMContentLoaded":
        this.onDOMContentLoaded();
        break;
    }
  }

  /**
   * Handle DOMContentLoaded event
   */
  private onDOMContentLoaded(): void {
    console.debug("[NRPluginStoreChild] DOMContentLoaded, injecting API...");
    this.injectFloorpAPI();
  }

  // ==========================================================================
  // API Methods - These are exposed to the web content
  // All async methods use callbacks since Promises cannot cross compartment boundaries
  // ==========================================================================

  /**
   * Check if the browser is Floorp
   */
  NRPluginStore_isFloorp(): boolean {
    return true;
  }

  /**
   * Get Floorp version (callback-based)
   */
  NRPluginStore_getVersion(callback: (result: string) => void): void {
    this.sendQuery("PluginStore:GetVersion", {})
      .then((result) => {
        callback(JSON.stringify(result));
      })
      .catch((error) => {
        callback(JSON.stringify({ error: String(error) }));
      });
  }

  /**
   * Install a plugin by ID (callback-based)
   */
  NRPluginStore_installPlugin(
    pluginId: string,
    metadataJson: string,
    callback: (result: string) => void,
  ): void {
    const metadata = metadataJson ? JSON.parse(metadataJson) : undefined;
    this.sendQuery("PluginStore:Install", {
      pluginId,
      metadata,
    })
      .then((result) => {
        callback(JSON.stringify(result));
      })
      .catch((error) => {
        callback(JSON.stringify({ success: false, error: String(error) }));
      });
  }

  /**
   * Uninstall a plugin by ID (callback-based)
   */
  NRPluginStore_uninstallPlugin(
    pluginId: string,
    callback: (result: string) => void,
  ): void {
    this.sendQuery("PluginStore:Uninstall", { pluginId })
      .then((result) => {
        callback(JSON.stringify(result));
      })
      .catch((error) => {
        callback(JSON.stringify({ success: false, error: String(error) }));
      });
  }

  /**
   * Check if a plugin is installed (callback-based)
   */
  NRPluginStore_isPluginInstalled(
    pluginId: string,
    callback: (result: string) => void,
  ): void {
    this.sendQuery("PluginStore:IsInstalled", { pluginId })
      .then((result) => {
        callback(JSON.stringify(result));
      })
      .catch((error) => {
        callback(JSON.stringify({ installed: false, error: String(error) }));
      });
  }

  // ==========================================================================
  // API Injection
  // ==========================================================================

  /**
   * Inject Floorp Plugin Store API into the page
   * This exposes a global `floorpPluginStore` object that the web page can use
   */
  private injectFloorpAPI(): void {
    if (this.apiInjected) return;

    const win = this.contentWindow;
    if (!win) return;

    try {
      // Export API functions to window using bind(this) pattern
      Cu.exportFunction(this.NRPluginStore_isFloorp.bind(this), win, {
        defineAs: "NRPluginStore_isFloorp",
      });

      Cu.exportFunction(this.NRPluginStore_getVersion.bind(this), win, {
        defineAs: "NRPluginStore_getVersion",
      });

      Cu.exportFunction(this.NRPluginStore_installPlugin.bind(this), win, {
        defineAs: "NRPluginStore_installPlugin",
      });

      Cu.exportFunction(this.NRPluginStore_uninstallPlugin.bind(this), win, {
        defineAs: "NRPluginStore_uninstallPlugin",
      });

      Cu.exportFunction(this.NRPluginStore_isPluginInstalled.bind(this), win, {
        defineAs: "NRPluginStore_isPluginInstalled",
      });

      // Create floorpPluginStore wrapper object using Cu.cloneInto
      // This provides a clean API interface for web content
      this.createPluginStoreObject(win);

      // Set isFloorp flag
      win.wrappedJSObject.isFloorp = true;

      this.apiInjected = true;
      console.debug(
        "[NRPluginStoreChild] API injected successfully, floorpPluginStore:",
        !!win.wrappedJSObject.floorpPluginStore,
      );

      // Dispatch ready event
      this.dispatchReadyEvent(win);
    } catch (e) {
      console.error("[NRPluginStoreChild] Failed to inject API:", e);
    }
  }

  /**
   * Create the floorpPluginStore object on the window using Cu.cloneInto
   */
  private createPluginStoreObject(win: Window): void {
    // Create empty object in content compartment
    const pluginStoreObj = Cu.cloneInto({}, win, { cloneFunctions: false });

    // Export each method to the object
    Cu.exportFunction(this.NRPluginStore_isFloorp.bind(this), pluginStoreObj, {
      defineAs: "isFloorp",
    });

    Cu.exportFunction(
      this.NRPluginStore_getVersion.bind(this),
      pluginStoreObj,
      { defineAs: "getVersion" },
    );

    Cu.exportFunction(
      this.NRPluginStore_installPlugin.bind(this),
      pluginStoreObj,
      { defineAs: "installPlugin" },
    );

    Cu.exportFunction(
      this.NRPluginStore_uninstallPlugin.bind(this),
      pluginStoreObj,
      { defineAs: "uninstallPlugin" },
    );

    Cu.exportFunction(
      this.NRPluginStore_isPluginInstalled.bind(this),
      pluginStoreObj,
      { defineAs: "isPluginInstalled" },
    );

    // Assign to window.floorpPluginStore
    win.wrappedJSObject.floorpPluginStore = pluginStoreObj;
  }

  /**
   * Dispatch the ready event to web content
   */
  private dispatchReadyEvent(win: Window): void {
    try {
      const detail = Cu.cloneInto({ version: "1.0.0" }, win);
      const EventCtor = win.CustomEvent || CustomEvent;
      const event = new EventCtor("floorpPluginStoreReady", { detail });
      win.dispatchEvent(event);
    } catch (e) {
      console.error("[NRPluginStoreChild] Failed to dispatch ready event:", e);
    }
  }

  /**
   * Dispatch a custom event to the web content
   */
  private dispatchEventToContent(
    eventName: string,
    data: Record<string, unknown>,
  ): void {
    const win = this.contentWindow;
    if (!win) return;

    try {
      const detail = Cu.cloneInto(data, win);
      const EventCtor = win.CustomEvent || CustomEvent;
      const event = new EventCtor(eventName, { detail });
      win.dispatchEvent(event);
    } catch (_e) {
      // Ignore errors
    }
  }

  /**
   * Handle messages from the parent process
   */
  receiveMessage(message: {
    name: string;
    data?: Record<string, unknown>;
  }): unknown {
    switch (message.name) {
      case "PluginStore:InstallProgress":
        this.dispatchEventToContent(
          "floorpPluginInstallProgress",
          message.data || {},
        );
        return true;

      case "PluginStore:InstallComplete":
        this.dispatchEventToContent(
          "floorpPluginInstallComplete",
          message.data || {},
        );
        return true;

      default:
        return null;
    }
  }
}
