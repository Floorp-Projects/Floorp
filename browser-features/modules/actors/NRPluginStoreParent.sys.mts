/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * NRPluginStoreParent - Parent process actor for Floorp OS Plugin Store integration
 *
 * This actor handles plugin installation from the web store by redirecting
 * to the Sapphillon Frontend with plugin metadata as query parameters.
 *
 * @module NRPluginStoreParent
 */

import type {
  PluginMetadata,
  InstallResult,
} from "./NRPluginStoreChild.sys.mts";

// =============================================================================
// Types
// =============================================================================

interface InstallRequest {
  pluginId: string;
  metadata?: PluginMetadata;
}

interface BrowserWindow extends Window {
  gBrowser?: {
    selectedBrowser?: Element;
    tabContainer?: EventTarget;
  };
}

// =============================================================================
// Main Class
// =============================================================================

export class NRPluginStoreParent extends JSWindowActorParent {
  /**
   * Get the browser window associated with this actor
   */
  private getBrowserWindow(): BrowserWindow | null {
    const browser = this.browsingContext?.top?.embedderElement;
    return (browser?.ownerGlobal as BrowserWindow) ?? null;
  }

  receiveMessage(message: {
    name: string;
    data?: InstallRequest | { pluginId: string } | Record<string, unknown>;
  }):
    | Promise<
        InstallResult | { installed: boolean } | { version: string } | null
      >
    | InstallResult
    | { installed: boolean }
    | { version: string }
    | null {
    switch (message.name) {
      case "PluginStore:GetVersion":
        return this.getFloorpVersion();

      case "PluginStore:Install":
        if (message.data && "pluginId" in message.data) {
          return this.installPlugin(message.data as InstallRequest);
        }
        return { success: false, error: "Invalid request data" };

      case "PluginStore:Uninstall":
        if (message.data && "pluginId" in message.data) {
          return this.uninstallPlugin(
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

      default:
        return null;
    }
  }

  /**
   * Get Floorp version
   * TODO: Return actual Floorp version from AppInfo
   */
  private getFloorpVersion(): { version: string } {
    const randomVersion = `${Math.floor(Math.random() * 100)}.${Math.floor(Math.random() * 100)}.${Math.floor(Math.random() * 100)}`;
    console.log("[NRPluginStoreParent] getFloorpVersion:", randomVersion);
    return { version: randomVersion };
  }

  /**
   * Install a plugin (redirects to Sapphillon Frontend)
   */
  private installPlugin(request: InstallRequest): Promise<InstallResult> {
    const { pluginId, metadata } = request;

    console.log("[NRPluginStoreParent] installPlugin called");
    console.log("[NRPluginStoreParent] pluginId:", pluginId);
    console.log(
      "[NRPluginStoreParent] metadata:",
      JSON.stringify(metadata, null, 2),
    );

    const win = this.getBrowserWindow();

    if (!win) {
      console.error("[NRPluginStoreParent] Could not get browser window");
      return Promise.resolve({
        success: false,
        error: "Could not get browser window",
      });
    }

    // Build the frontend URL with query parameters
    const frontendBaseUrl = "http://localhost:8081/install-plugin";
    const params = new URLSearchParams();

    if (metadata?.uri) {
      params.set("uri", metadata.uri);
    }
    if (pluginId) {
      params.set("id", pluginId);
    }
    if (metadata?.name) {
      params.set("name", metadata.name);
    }
    if (metadata?.author) {
      params.set("author", metadata.author);
    }
    if (metadata?.version) {
      params.set("version", metadata.version);
    }
    if (metadata?.description) {
      params.set("description", metadata.description);
    }
    if (metadata?.category) {
      params.set("category", metadata.category);
    }
    if (metadata?.isOfficial !== undefined) {
      params.set("isOfficial", String(metadata.isOfficial));
    }
    if (metadata?.icon) {
      params.set("icon", metadata.icon);
    }
    if (metadata?.functions && metadata.functions.length > 0) {
      params.set(
        "functions",
        encodeURIComponent(JSON.stringify(metadata.functions)),
      );
    }

    const frontendUrl = `${frontendBaseUrl}?${params.toString()}`;
    console.log("[NRPluginStoreParent] Opening Frontend URL:", frontendUrl);

    // Open the frontend in a new tab
    try {
      const winAny = win as unknown as {
        openTrustedLinkIn?: (url: string, where: string) => void;
      };
      if (typeof winAny.openTrustedLinkIn === "function") {
        winAny.openTrustedLinkIn(frontendUrl, "tab");
      } else {
        win.open(frontendUrl, "_blank");
      }

      return Promise.resolve({
        success: true,
        pluginId,
      });
    } catch (error) {
      console.error("[NRPluginStoreParent] Failed to open Frontend:", error);
      return Promise.resolve({
        success: false,
        error: "Failed to open plugin installation page",
      });
    }
  }

  /**
   * Uninstall a plugin
   * TODO: Implement actual uninstallation via Sapphillon Backend
   */
  private uninstallPlugin(pluginId: string): InstallResult {
    console.log("[NRPluginStoreParent] uninstallPlugin called");
    console.log("[NRPluginStoreParent] pluginId:", pluginId);

    return {
      success: true,
      pluginId,
    };
  }

  /**
   * Check if a plugin is installed
   * TODO: Query Sapphillon Backend for actual installation status
   */
  private isPluginInstalled(pluginId: string): { installed: boolean } {
    console.log("[NRPluginStoreParent] isPluginInstalled called");
    console.log("[NRPluginStoreParent] pluginId:", pluginId);

    return {
      installed: false,
    };
  }
}
