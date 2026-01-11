/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * NRPluginStoreParent - Parent process actor for Floorp OS Plugin Store integration
 *
 * This actor handles:
 * - Plugin installation from the web store
 * - Plugin management (enable, disable, uninstall)
 * - Communication with the OS server for plugin registration
 *
 * @module NRPluginStoreParent
 */

import type {
  PluginMetadata,
  InstallResult,
  PluginInfo,
} from "./NRPluginStoreChild.sys.mts";

// =============================================================================
// Types
// =============================================================================

interface InstallRequest {
  pluginId: string;
  metadata?: PluginMetadata;
}

interface PluginStorageData {
  installedPlugins: Map<string, StoredPlugin>;
}

interface StoredPlugin {
  id: string;
  name: string;
  version: string;
  metadata: PluginMetadata;
  installedAt: string;
  enabled: boolean;
}

// =============================================================================
// Storage
// =============================================================================

// Simple in-memory storage (in production, use persistent storage)
const pluginStorage: PluginStorageData = {
  installedPlugins: new Map(),
};

// Load from preferences on startup
function loadPluginStorage(): void {
  try {
    const stored = Services.prefs.getStringPref(
      "floorp.os.plugins.installed",
      "{}",
    );
    const data = JSON.parse(stored);
    if (data.installedPlugins) {
      for (const [id, plugin] of Object.entries(data.installedPlugins)) {
        pluginStorage.installedPlugins.set(id, plugin as StoredPlugin);
      }
    }
  } catch {
    // Ignore errors, start with empty storage
  }
}

function savePluginStorage(): void {
  try {
    const data = {
      installedPlugins: Object.fromEntries(pluginStorage.installedPlugins),
    };
    Services.prefs.setStringPref(
      "floorp.os.plugins.installed",
      JSON.stringify(data),
    );
  } catch (e) {
    console.error("[NRPluginStoreParent] Failed to save plugin storage:", e);
  }
}

// Initialize storage
loadPluginStorage();

// =============================================================================
// Main Class
// =============================================================================

export class NRPluginStoreParent extends JSWindowActorParent {
  async receiveMessage(message: {
    name: string;
    data?: InstallRequest | { pluginId: string } | Record<string, unknown>;
  }): Promise<
    | InstallResult
    | PluginInfo[]
    | { installed: boolean }
    | { version: string }
    | boolean
    | null
  > {
    switch (message.name) {
      case "PluginStore:GetVersion":
        return this.getFloorpVersion();

      case "PluginStore:Install":
        if (message.data && "pluginId" in message.data) {
          return await this.installPlugin(message.data as InstallRequest);
        }
        return { success: false, error: "Invalid request data" };

      case "PluginStore:Uninstall":
        if (message.data && "pluginId" in message.data) {
          return await this.uninstallPlugin(
            (message.data as { pluginId: string }).pluginId,
          );
        }
        return { success: false, error: "Invalid request data" };

      case "PluginStore:IsInstalled":
        if (message.data && "pluginId" in message.data) {
          return this.isPluginInstalled(
            (message.data as { pluginId: string }).pluginId,
          );
        }
        return { installed: false };

      case "PluginStore:GetInstalled":
        return this.getInstalledPlugins();

      case "PluginStore:Enable":
        if (message.data && "pluginId" in message.data) {
          return await this.enablePlugin(
            (message.data as { pluginId: string }).pluginId,
          );
        }
        return { success: false, error: "Invalid request data" };

      case "PluginStore:Disable":
        if (message.data && "pluginId" in message.data) {
          return await this.disablePlugin(
            (message.data as { pluginId: string }).pluginId,
          );
        }
        return { success: false, error: "Invalid request data" };

      default:
        return null;
    }
  }

  /**
   * Get Floorp version
   */
  private getFloorpVersion(): { version: string } {
    try {
      const version = Services.appinfo.version;
      return { version };
    } catch {
      return { version: "unknown" };
    }
  }

  /**
   * Install a plugin
   */
  private async installPlugin(request: InstallRequest): Promise<InstallResult> {
    const { pluginId, metadata } = request;

    try {
      // Check if already installed
      if (pluginStorage.installedPlugins.has(pluginId)) {
        return {
          success: false,
          error: "Plugin is already installed",
          pluginId,
        };
      }

      // Notify child about progress
      this.sendAsyncMessage("PluginStore:InstallProgress", {
        pluginId,
        status: "downloading",
        progress: 0,
      });

      // TODO: Download plugin from store API
      // For now, we just register the metadata

      this.sendAsyncMessage("PluginStore:InstallProgress", {
        pluginId,
        status: "installing",
        progress: 50,
      });

      // Register the plugin
      const storedPlugin: StoredPlugin = {
        id: pluginId,
        name: metadata?.name || pluginId,
        version: metadata?.version || "1.0.0",
        metadata: metadata || {
          id: pluginId,
          name: pluginId,
          description: "",
          version: "1.0.0",
          author: "Unknown",
          functions: [],
          category: "utilities",
          isOfficial: false,
        },
        installedAt: new Date().toISOString(),
        enabled: true,
      };

      pluginStorage.installedPlugins.set(pluginId, storedPlugin);
      savePluginStorage();

      // Notify OS server about new plugin
      await this.notifyOSServer("install", pluginId, storedPlugin);

      this.sendAsyncMessage("PluginStore:InstallProgress", {
        pluginId,
        status: "complete",
        progress: 100,
      });

      // Notify child about completion
      this.sendAsyncMessage("PluginStore:InstallComplete", {
        success: true,
        pluginId,
      });

      return {
        success: true,
        pluginId,
      };
    } catch (error) {
      const errorMessage =
        error instanceof Error ? error.message : String(error);

      this.sendAsyncMessage("PluginStore:InstallComplete", {
        success: false,
        pluginId,
        error: errorMessage,
      });

      return {
        success: false,
        error: errorMessage,
        pluginId,
      };
    }
  }

  /**
   * Uninstall a plugin
   */
  private async uninstallPlugin(pluginId: string): Promise<InstallResult> {
    try {
      if (!pluginStorage.installedPlugins.has(pluginId)) {
        return {
          success: false,
          error: "Plugin is not installed",
          pluginId,
        };
      }

      const plugin = pluginStorage.installedPlugins.get(pluginId);
      pluginStorage.installedPlugins.delete(pluginId);
      savePluginStorage();

      // Notify OS server about uninstall
      await this.notifyOSServer("uninstall", pluginId, plugin);

      return {
        success: true,
        pluginId,
      };
    } catch (error) {
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      return {
        success: false,
        error: errorMessage,
        pluginId,
      };
    }
  }

  /**
   * Check if a plugin is installed
   */
  private isPluginInstalled(pluginId: string): { installed: boolean } {
    return {
      installed: pluginStorage.installedPlugins.has(pluginId),
    };
  }

  /**
   * Get list of installed plugins
   */
  private getInstalledPlugins(): PluginInfo[] {
    const plugins: PluginInfo[] = [];
    for (const [id, plugin] of pluginStorage.installedPlugins) {
      plugins.push({
        id,
        name: plugin.name,
        version: plugin.version,
        installed: true,
        enabled: plugin.enabled,
      });
    }
    return plugins;
  }

  /**
   * Enable a plugin
   */
  private async enablePlugin(pluginId: string): Promise<InstallResult> {
    try {
      const plugin = pluginStorage.installedPlugins.get(pluginId);
      if (!plugin) {
        return {
          success: false,
          error: "Plugin is not installed",
          pluginId,
        };
      }

      plugin.enabled = true;
      pluginStorage.installedPlugins.set(pluginId, plugin);
      savePluginStorage();

      // Notify OS server
      await this.notifyOSServer("enable", pluginId, plugin);

      return {
        success: true,
        pluginId,
      };
    } catch (error) {
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      return {
        success: false,
        error: errorMessage,
        pluginId,
      };
    }
  }

  /**
   * Disable a plugin
   */
  private async disablePlugin(pluginId: string): Promise<InstallResult> {
    try {
      const plugin = pluginStorage.installedPlugins.get(pluginId);
      if (!plugin) {
        return {
          success: false,
          error: "Plugin is not installed",
          pluginId,
        };
      }

      plugin.enabled = false;
      pluginStorage.installedPlugins.set(pluginId, plugin);
      savePluginStorage();

      // Notify OS server
      await this.notifyOSServer("disable", pluginId, plugin);

      return {
        success: true,
        pluginId,
      };
    } catch (error) {
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      return {
        success: false,
        error: errorMessage,
        pluginId,
      };
    }
  }

  /**
   * Notify the OS server about plugin changes
   * This communicates with the Floorp OS backend
   */
  private async notifyOSServer(
    action: "install" | "uninstall" | "enable" | "disable",
    pluginId: string,
    plugin?: StoredPlugin,
  ): Promise<void> {
    try {
      // Get OS server port from preferences
      const port = Services.prefs.getIntPref("floorp.os.server.port", 9222);
      const baseUrl = `http://127.0.0.1:${port}`;

      // Send request to OS server
      const response = await fetch(`${baseUrl}/api/plugins/${action}`, {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify({
          pluginId,
          plugin: plugin
            ? {
                id: plugin.id,
                name: plugin.name,
                version: plugin.version,
                functions: plugin.metadata.functions,
                enabled: plugin.enabled,
              }
            : null,
        }),
      });

      if (!response.ok) {
        console.warn(
          `[NRPluginStoreParent] OS server responded with ${response.status}`,
        );
      }
    } catch (e) {
      // OS server might not be running, which is okay
      console.debug("[NRPluginStoreParent] Could not notify OS server:", e);
    }
  }
}
