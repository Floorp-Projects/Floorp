/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * NRPluginStoreParent - Parent process actor for Floorp OS Plugin Store integration
 *
 * This actor handles:
 * - Plugin installation from the web store (with rich doorhanger UI)
 * - Plugin management (uninstall)
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

// Firefox built-in icon URLs for categories
const CATEGORY_ICONS: Record<string, string> = {
  browser: "chrome://global/skin/icons/link.svg",
  system: "chrome://global/skin/icons/settings.svg",
  productivity: "chrome://global/skin/icons/lightbulb.svg",
  developer: "chrome://global/skin/icons/developer.svg",
  communication: "chrome://browser/skin/notification-icons/chat.svg",
  media: "chrome://global/skin/media/play-fill.svg",
  utilities: "chrome://global/skin/icons/settings.svg",
};

// Default plugin icon
const DEFAULT_PLUGIN_ICON = "chrome://mozapps/skin/extensions/extension.svg";

// Sapphillon gRPC-Web server configuration
const SAPPHILLON_GRPC_BASE_URL = "http://localhost:50051";

/**
 * Call Sapphillon gRPC-Web API using Connect protocol (JSON)
 *
 * @param service - Full service name (e.g., "sapphillon.v1.PluginService")
 * @param method - Method name (e.g., "InstallPlugin")
 * @param request - Request body as JSON object
 * @returns Promise with the response JSON
 */
async function callGrpcWeb<T>(
  service: string,
  method: string,
  request: Record<string, unknown>,
): Promise<T> {
  const url = `${SAPPHILLON_GRPC_BASE_URL}/${service}/${method}`;

  console.log(`[gRPC-Web] Calling ${service}/${method}`);
  console.log(`[gRPC-Web] URL: ${url}`);
  console.log(`[gRPC-Web] Request:`, JSON.stringify(request, null, 2));

  const response = await fetch(url, {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
      Accept: "application/json",
    },
    body: JSON.stringify(request),
  });

  if (!response.ok) {
    const errorText = await response.text();
    console.error(
      `[gRPC-Web] Error: ${response.status} ${response.statusText}`,
    );
    console.error(`[gRPC-Web] Error body:`, errorText);
    throw new Error(
      `gRPC-Web call failed: ${response.status} ${response.statusText}`,
    );
  }

  const result = (await response.json()) as T;
  console.log(`[gRPC-Web] Response:`, JSON.stringify(result, null, 2));
  return result;
}

/**
 * Get Firefox icon URL for a plugin based on category
 */
function getPluginIconUrl(category?: string): string {
  if (category && CATEGORY_ICONS[category]) {
    return CATEGORY_ICONS[category];
  }
  return DEFAULT_PLUGIN_ICON;
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
   * Get Floorp version (returns random value for now)
   */
  private getFloorpVersion(): { version: string } {
    const randomVersion = `${Math.floor(Math.random() * 100)}.${Math.floor(Math.random() * 100)}.${Math.floor(Math.random() * 100)}`;
    console.log("[NRPluginStoreParent] getFloorpVersion:", randomVersion);
    return { version: randomVersion };
  }

  /**
   * Create a centered modal dialog for plugin installation
   */
  private createModalDialog(
    doc: Document,
    metadata: PluginMetadata,
    onInstall: () => void,
    onCancel: () => void,
  ): HTMLElement {
    const HTML_NS = "http://www.w3.org/1999/xhtml";

    // Create overlay background - shadcn/ui style
    // Uses position: absolute instead of fixed to stay within content area
    // This allows tab switching while the dialog is open
    const overlay = doc.createElementNS(HTML_NS, "div") as HTMLElement;
    overlay.id = "floorp-os-plugin-install-overlay";
    overlay.style.cssText = `
      position: absolute;
      top: 0;
      left: 0;
      width: 100%;
      height: 100%;
      background: rgba(0, 0, 0, 0.8);
      display: flex;
      align-items: center;
      justify-content: center;
      z-index: 999999;
    `;

    // Create dialog box - shadcn/ui style
    const dialog = doc.createElementNS(HTML_NS, "div") as HTMLElement;
    dialog.style.cssText = `
      background: hsl(240 10% 3.9%);
      border: 1px solid hsl(240 3.7% 15.9%);
      border-radius: 8px;
      padding: 24px;
      min-width: 550px;
      max-width: 650px;
      max-height: 80vh;
      overflow-y: auto;
      box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.4), 0 4px 6px -4px rgba(0, 0, 0, 0.4);
      display: flex;
      flex-direction: column;
      gap: 24px;
      color: hsl(0 0% 98%);
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", sans-serif;
    `;

    // Header with title - shadcn/ui style
    const header = doc.createElementNS(HTML_NS, "div") as HTMLElement;
    header.style.cssText = `
      font-size: 18px;
      font-weight: 600;
      text-align: left;
      letter-spacing: -0.025em;
      line-height: 1;
    `;
    header.textContent = "Floorp OS プラグインをインストール";
    dialog.appendChild(header);

    // Plugin info section
    const infoSection = doc.createElementNS(HTML_NS, "div") as HTMLElement;
    infoSection.style.cssText = `
      display: flex;
      align-items: flex-start;
      gap: 16px;
      padding: 12px 0;
    `;

    // Icon - shadcn/ui style
    const iconContainer = doc.createElementNS(HTML_NS, "div") as HTMLElement;
    iconContainer.style.cssText = `
      width: 64px;
      height: 64px;
      min-width: 64px;
      border-radius: 6px;
      background: hsl(240 3.7% 15.9%);
      display: flex;
      align-items: center;
      justify-content: center;
      overflow: hidden;
    `;

    const isIconUrl =
      metadata.icon?.startsWith("http://") ||
      metadata.icon?.startsWith("https://");

    if (isIconUrl && metadata.icon) {
      const iconImg = doc.createElementNS(HTML_NS, "img") as HTMLImageElement;
      iconImg.style.cssText = `width: 40px; height: 40px; object-fit: contain; filter: brightness(0) invert(1); opacity: 0.9;`;
      iconImg.src = metadata.icon;
      iconImg.alt = metadata.name || "Plugin";
      iconImg.onerror = () => {
        iconImg.src = getPluginIconUrl(metadata.category);
      };
      iconContainer.appendChild(iconImg);
    } else {
      const fallbackImg = doc.createElementNS(
        HTML_NS,
        "img",
      ) as HTMLImageElement;
      fallbackImg.style.cssText = `width: 40px; height: 40px; object-fit: contain; -moz-context-properties: fill; fill: currentColor; opacity: 0.8;`;
      fallbackImg.src = getPluginIconUrl(metadata.category);
      fallbackImg.alt = metadata.name || "Plugin";
      iconContainer.appendChild(fallbackImg);
    }
    infoSection.appendChild(iconContainer);

    // Plugin details - shadcn/ui style
    const details = doc.createElementNS(HTML_NS, "div") as HTMLElement;
    details.style.cssText = `
      display: flex;
      flex-direction: column;
      gap: 4px;
      flex: 1;
    `;

    const nameEl = doc.createElementNS(HTML_NS, "div") as HTMLElement;
    nameEl.style.cssText = `font-size: 16px; font-weight: 500; color: hsl(0 0% 98%); letter-spacing: -0.01em;`;
    nameEl.textContent = metadata.name || "不明なプラグイン";
    details.appendChild(nameEl);

    const authorRow = doc.createElementNS(HTML_NS, "div") as HTMLElement;
    authorRow.style.cssText = `
      font-size: 14px;
      color: hsl(240 5% 64.9%);
      display: flex;
      align-items: center;
      gap: 8px;
      flex-wrap: wrap;
    `;
    if (metadata.author) {
      const authorEl = doc.createElementNS(HTML_NS, "span") as HTMLElement;
      authorEl.textContent = `作成者: ${metadata.author}`;
      authorRow.appendChild(authorEl);
    }
    if (metadata.version) {
      const versionBadge = doc.createElementNS(HTML_NS, "span") as HTMLElement;
      versionBadge.style.cssText = `
        background: hsl(240 3.7% 15.9%);
        color: hsl(240 5% 64.9%);
        padding: 2px 8px;
        border-radius: 9999px;
        font-size: 12px;
        font-weight: 500;
      `;
      versionBadge.textContent = `v${metadata.version}`;
      authorRow.appendChild(versionBadge);
    }
    if (metadata.isOfficial) {
      const officialBadge = doc.createElementNS(HTML_NS, "span") as HTMLElement;
      officialBadge.style.cssText = `
        background: hsl(0 0% 98%);
        color: hsl(240 5.9% 10%);
        padding: 2px 8px;
        border-radius: 9999px;
        font-size: 12px;
        font-weight: 500;
      `;
      officialBadge.textContent = "✓ 公式";
      authorRow.appendChild(officialBadge);
    }
    if (metadata.category) {
      const categoryBadge = doc.createElementNS(HTML_NS, "span") as HTMLElement;
      categoryBadge.style.cssText = `
        background: transparent;
        color: hsl(240 5% 64.9%);
        padding: 2px 8px;
        border: 1px solid hsl(240 3.7% 15.9%);
        border-radius: 9999px;
        font-size: 12px;
        font-weight: 500;
      `;
      const categoryLabels: Record<string, string> = {
        browser: "ブラウザ",
        system: "システム",
        productivity: "生産性",
        developer: "開発ツール",
        communication: "通信",
        media: "メディア",
        utilities: "ユーティリティ",
      };
      categoryBadge.textContent =
        categoryLabels[metadata.category] || metadata.category;
      authorRow.appendChild(categoryBadge);
    }
    details.appendChild(authorRow);

    infoSection.appendChild(details);
    dialog.appendChild(infoSection);

    // Description - shadcn/ui style
    if (metadata.description) {
      const descBox = doc.createElementNS(HTML_NS, "div") as HTMLElement;
      descBox.style.cssText = `
        font-size: 14px;
        line-height: 1.6;
        color: hsl(240 5% 64.9%);
      `;
      descBox.textContent = metadata.description;
      dialog.appendChild(descBox);
    }

    // Functions / Permissions section - shadcn/ui style
    if (metadata.functions && metadata.functions.length > 0) {
      const permSection = doc.createElementNS(HTML_NS, "div") as HTMLElement;
      permSection.style.cssText = `
        display: flex;
        flex-direction: column;
        gap: 12px;
      `;

      // Section header
      const permHeader = doc.createElementNS(HTML_NS, "div") as HTMLElement;
      permHeader.style.cssText = `
        font-size: 14px;
        font-weight: 500;
        color: hsl(0 0% 98%);
        display: flex;
        align-items: center;
        gap: 8px;
      `;
      const permIcon = doc.createElementNS(HTML_NS, "img") as HTMLImageElement;
      permIcon.style.cssText = `width: 16px; height: 16px; -moz-context-properties: fill; fill: currentColor; opacity: 0.8;`;
      permIcon.src = "chrome://global/skin/icons/security.svg";
      permIcon.alt = "";
      permHeader.appendChild(permIcon);
      const permTitle = doc.createElementNS(HTML_NS, "span") as HTMLElement;
      permTitle.textContent = `このプラグインが提供・アクセスする機能 (${metadata.functions.length})`;
      permHeader.appendChild(permTitle);
      permSection.appendChild(permHeader);

      // Functions list container - shadcn/ui style
      const funcList = doc.createElementNS(HTML_NS, "div") as HTMLElement;
      funcList.style.cssText = `
        background: hsl(240 10% 3.9%);
        border: 1px solid hsl(240 3.7% 15.9%);
        border-radius: 6px;
        padding: 4px;
        max-height: 200px;
        overflow-y: auto;
        display: flex;
        flex-direction: column;
        gap: 1px;
      `;

      for (const func of metadata.functions) {
        const funcItem = doc.createElementNS(HTML_NS, "div") as HTMLElement;
        funcItem.style.cssText = `
          display: flex;
          flex-direction: column;
          gap: 2px;
          padding: 8px 12px;
          border-radius: 4px;
          background: transparent;
        `;

        const funcName = doc.createElementNS(HTML_NS, "div") as HTMLElement;
        funcName.style.cssText = `
          font-size: 13px;
          font-weight: 500;
          color: hsl(0 0% 98%);
          font-family: ui-monospace, SFMono-Regular, "SF Mono", Menlo, Monaco, Consolas, monospace;
        `;
        funcName.textContent = func.name;
        funcItem.appendChild(funcName);

        if (func.description) {
          const funcDesc = doc.createElementNS(HTML_NS, "div") as HTMLElement;
          funcDesc.style.cssText = `
            font-size: 12px;
            color: hsl(240 5% 64.9%);
            line-height: 1.4;
          `;
          funcDesc.textContent = func.description;
          funcItem.appendChild(funcDesc);
        }

        if (func.parameters && func.parameters.length > 0) {
          const paramsRow = doc.createElementNS(HTML_NS, "div") as HTMLElement;
          paramsRow.style.cssText = `
            font-size: 11px;
            margin-top: 4px;
            display: flex;
            flex-wrap: wrap;
            gap: 4px;
          `;
          for (const param of func.parameters) {
            const paramBadge = doc.createElementNS(
              HTML_NS,
              "span",
            ) as HTMLElement;
            paramBadge.style.cssText = `
              background: hsl(240 3.7% 15.9%);
              color: hsl(240 5% 64.9%);
              padding: 2px 6px;
              border-radius: 4px;
              font-family: ui-monospace, SFMono-Regular, "SF Mono", Menlo, Monaco, Consolas, monospace;
            `;
            paramBadge.textContent = param;
            paramsRow.appendChild(paramBadge);
          }
          funcItem.appendChild(paramsRow);
        }

        funcList.appendChild(funcItem);
      }

      permSection.appendChild(funcList);
      dialog.appendChild(permSection);
    }

    // Warning message - shadcn/ui style (destructive variant)
    const warning = doc.createElementNS(HTML_NS, "div") as HTMLElement;
    warning.style.cssText = `
      display: flex;
      align-items: flex-start;
      gap: 12px;
      padding: 12px 16px;
      background: hsl(0 62.8% 30.6% / 0.15);
      border: 1px solid hsl(0 62.8% 30.6% / 0.3);
      border-radius: 6px;
    `;
    const warningIcon = doc.createElementNS(HTML_NS, "img") as HTMLImageElement;
    warningIcon.style.cssText = `width: 16px; height: 16px; min-width: 16px; -moz-context-properties: fill; fill: hsl(0 62.8% 60%);`;
    warningIcon.src = "chrome://global/skin/icons/warning.svg";
    warningIcon.alt = "";
    warning.appendChild(warningIcon);
    const warningText = doc.createElementNS(HTML_NS, "span") as HTMLElement;
    warningText.style.cssText = `font-size: 13px; line-height: 1.5; color: hsl(0 0% 98%);`;
    warningText.textContent =
      "このプラグインは Floorp OS の機能を拡張します。信頼できる提供元からのみインストールしてください。";
    warning.appendChild(warningText);
    dialog.appendChild(warning);

    // Buttons - shadcn/ui style
    const buttonRow = doc.createElementNS(HTML_NS, "div") as HTMLElement;
    buttonRow.style.cssText = `
      display: flex;
      justify-content: flex-end;
      gap: 8px;
      padding-top: 16px;
      border-top: 1px solid hsl(240 3.7% 15.9%);
      margin-top: 8px;
    `;

    // Details button - shadcn/ui outline style
    const detailsBtn = doc.createElementNS(HTML_NS, "button") as HTMLElement;
    const detailsBtnDefaultStyle = `
      height: 36px;
      padding: 0 16px;
      border-radius: 6px;
      border: 1px solid hsl(240 3.7% 15.9%);
      background: transparent;
      color: hsl(240 5% 64.9%);
      font-size: 14px;
      font-weight: 500;
      cursor: pointer;
      transition: background 0.15s, border-color 0.15s, color 0.15s;
      margin-right: auto;
      display: flex;
      align-items: center;
      gap: 6px;
    `;
    detailsBtn.style.cssText = detailsBtnDefaultStyle;

    // Info icon for details button
    const detailsIcon = doc.createElementNS(HTML_NS, "img") as HTMLImageElement;
    detailsIcon.style.cssText = `width: 14px; height: 14px; -moz-context-properties: fill; fill: hsl(240 5% 64.9%);`;
    detailsIcon.src = "chrome://global/skin/icons/info.svg";
    detailsIcon.alt = "";
    detailsBtn.appendChild(detailsIcon);

    const detailsBtnText = doc.createElementNS(HTML_NS, "span") as HTMLElement;
    detailsBtnText.textContent = "詳細情報";
    detailsBtn.appendChild(detailsBtnText);

    detailsBtn.addEventListener("click", () => {
      // TODO: Open plugin details page in a new tab
      const detailsUrl =
        metadata.uri ||
        `https://floorp.app/plugins/${metadata.id || "unknown"}`;
      console.log("[NRPluginStoreParent] Opening plugin details:", detailsUrl);
      // Open in new tab using openTrustedLinkIn if available
      const win = this.getBrowserWindow();
      if (
        win &&
        typeof (
          win as unknown as {
            openTrustedLinkIn?: (url: string, where: string) => void;
          }
        ).openTrustedLinkIn === "function"
      ) {
        (
          win as unknown as {
            openTrustedLinkIn: (url: string, where: string) => void;
          }
        ).openTrustedLinkIn(detailsUrl, "tab");
      } else {
        // Fallback: try to open using window.open
        win?.open(detailsUrl, "_blank");
      }
    });
    detailsBtn.addEventListener("mouseenter", () => {
      detailsBtn.style.cssText =
        detailsBtnDefaultStyle +
        "background: hsl(240 3.7% 15.9%); border-color: hsl(240 5% 26%); color: hsl(0 0% 98%);";
      detailsIcon.style.cssText = `width: 14px; height: 14px; -moz-context-properties: fill; fill: hsl(0 0% 98%);`;
    });
    detailsBtn.addEventListener("mouseleave", () => {
      detailsBtn.style.cssText = detailsBtnDefaultStyle;
      detailsIcon.style.cssText = `width: 14px; height: 14px; -moz-context-properties: fill; fill: hsl(240 5% 64.9%);`;
    });
    buttonRow.appendChild(detailsBtn);

    const cancelBtn = doc.createElementNS(HTML_NS, "button") as HTMLElement;
    const cancelBtnDefaultStyle = `
      height: 36px;
      padding: 0 16px;
      border-radius: 6px;
      border: 1px solid hsl(240 3.7% 15.9%);
      background: transparent;
      color: hsl(0 0% 98%);
      font-size: 14px;
      font-weight: 500;
      cursor: pointer;
      transition: background 0.15s, border-color 0.15s;
    `;
    cancelBtn.style.cssText = cancelBtnDefaultStyle;
    cancelBtn.textContent = "キャンセル";
    cancelBtn.addEventListener("click", onCancel);
    cancelBtn.addEventListener("mouseenter", () => {
      cancelBtn.style.cssText =
        cancelBtnDefaultStyle +
        "background: hsl(240 3.7% 15.9%); border-color: hsl(240 5% 26%);";
    });
    cancelBtn.addEventListener("mouseleave", () => {
      cancelBtn.style.cssText = cancelBtnDefaultStyle;
    });
    buttonRow.appendChild(cancelBtn);

    const installBtn = doc.createElementNS(HTML_NS, "button") as HTMLElement;
    const installBtnDefaultStyle = `
      height: 36px;
      padding: 0 16px;
      border-radius: 6px;
      border: none;
      background: hsl(0 0% 98%);
      color: hsl(240 5.9% 10%);
      font-size: 14px;
      font-weight: 500;
      cursor: pointer;
      transition: background 0.15s;
    `;
    installBtn.style.cssText = installBtnDefaultStyle;
    installBtn.textContent = "追加";
    installBtn.addEventListener("click", onInstall);
    installBtn.addEventListener("mouseenter", () => {
      installBtn.style.cssText =
        installBtnDefaultStyle + "background: hsl(0 0% 90%);";
    });
    installBtn.addEventListener("mouseleave", () => {
      installBtn.style.cssText = installBtnDefaultStyle;
    });
    buttonRow.appendChild(installBtn);

    dialog.appendChild(buttonRow);
    overlay.appendChild(dialog);

    // Close on overlay click (outside dialog)
    overlay.addEventListener("click", (e) => {
      if (e.target === overlay) {
        onCancel();
      }
    });

    return overlay;
  }

  /**
   * Install a plugin (shows centered modal dialog)
   */
  private installPlugin(request: InstallRequest): Promise<InstallResult> {
    const { pluginId, metadata } = request;

    console.log("[NRPluginStoreParent] installPlugin called");
    console.log("[NRPluginStoreParent] pluginId:", pluginId);
    console.log(
      "[NRPluginStoreParent] metadata:",
      JSON.stringify(metadata, null, 2),
    );

    // Get browser window
    const win = this.getBrowserWindow();

    if (!win) {
      console.error("[NRPluginStoreParent] Could not get browser window");
      return Promise.resolve({
        success: false,
        error: "Could not get browser window",
      });
    }

    const pluginName = metadata?.name ?? pluginId;
    const pluginAuthor = metadata?.author ?? "不明";
    const pluginDescription = metadata?.description ?? "";

    // Prepare full metadata with defaults
    const fullMetadata: PluginMetadata = {
      id: pluginId,
      name: pluginName,
      author: pluginAuthor,
      description: pluginDescription,
      version: metadata?.version,
      icon: metadata?.icon,
      category: metadata?.category,
      functions: metadata?.functions,
      isOfficial: metadata?.isOfficial,
      uri: metadata?.uri,
    };

    return new Promise<InstallResult>((resolve) => {
      let resolved = false;
      let modalOverlay: HTMLElement | null = null;
      let tabSelectHandler: ((e: Event) => void) | null = null;
      // Store reference to the browser that triggered the install
      const originBrowser = this.browsingContext?.top?.embedderElement;

      const cleanup = () => {
        // Remove tab select listener
        if (tabSelectHandler && win.gBrowser?.tabContainer) {
          win.gBrowser.tabContainer.removeEventListener(
            "TabSelect",
            tabSelectHandler,
          );
          tabSelectHandler = null;
        }
        if (modalOverlay && modalOverlay.parentNode) {
          modalOverlay.parentNode.removeChild(modalOverlay);
        }
        modalOverlay = null;
      };

      const handleInstall = async () => {
        if (resolved) return;
        resolved = true;
        cleanup();
        console.log("[NRPluginStoreParent] User confirmed installation");

        // Call Sapphillon backend to install the plugin
        if (fullMetadata.uri) {
          try {
            console.log(
              "[NRPluginStoreParent] Calling Sapphillon InstallPlugin API",
            );
            const grpcResponse = await callGrpcWeb<{
              plugin?: Record<string, unknown>;
              status?: { code: number; message: string };
            }>("sapphillon.v1.PluginService", "InstallPlugin", {
              uri: fullMetadata.uri,
            });

            console.log(
              "[NRPluginStoreParent] Sapphillon InstallPlugin response:",
              grpcResponse,
            );

            // Check status from response
            if (grpcResponse.status && grpcResponse.status.code !== 0) {
              console.error(
                "[NRPluginStoreParent] Sapphillon returned error:",
                grpcResponse.status.message,
              );
              resolve({
                success: false,
                error: grpcResponse.status.message || "Installation failed",
              });
              return;
            }
          } catch (error) {
            console.error(
              "[NRPluginStoreParent] Failed to call Sapphillon API:",
              error,
            );
            // Continue with success even if backend call fails (for now)
            // In production, you might want to fail the installation
          }
        } else {
          console.warn(
            "[NRPluginStoreParent] No URI provided, skipping Sapphillon API call",
          );
        }

        resolve({
          success: true,
          pluginId,
        });
      };

      const handleCancel = () => {
        if (resolved) return;
        resolved = true;
        cleanup();
        console.log("[NRPluginStoreParent] User cancelled installation");
        resolve({
          success: false,
          error: "Installation cancelled by user",
        });
      };

      try {
        const doc = win.document;
        if (!doc) {
          throw new Error("Document not available");
        }

        // Remove any existing modal
        const existingModal = doc.getElementById(
          "floorp-os-plugin-install-overlay",
        );
        if (existingModal) {
          existingModal.remove();
        }

        // Create and show modal
        modalOverlay = this.createModalDialog(
          doc,
          fullMetadata,
          handleInstall,
          handleCancel,
        );

        // Append to browser content area to allow tab switching
        // Priority: #appcontent (Firefox content area) > #browser > #main-window
        const target =
          doc.getElementById("appcontent") ||
          doc.getElementById("browser") ||
          doc.getElementById("main-window") ||
          doc.documentElement;
        if (target) {
          // Ensure the target has position: relative for absolute positioning to work
          const computedStyle = win.getComputedStyle(target);
          if (
            computedStyle &&
            (computedStyle.getPropertyValue("position") as string) === "static"
          ) {
            (target as HTMLElement).style.cssText += "position: relative;";
          }
          target.appendChild(modalOverlay);
        } else {
          throw new Error("Could not find target element to append modal");
        }

        // Add tab switch listener to show/hide dialog based on active tab
        if (win.gBrowser?.tabContainer && originBrowser) {
          tabSelectHandler = () => {
            if (!modalOverlay) return;
            const currentBrowser = win.gBrowser?.selectedBrowser;
            if (currentBrowser === originBrowser) {
              // Back to the original tab - show dialog
              (modalOverlay as HTMLElement).style.cssText =
                modalOverlay.style.cssText.replace(
                  /display\s*:\s*none\s*;?/gi,
                  "",
                ) + "; display: flex;";
            } else {
              // Switched to another tab - hide dialog
              (modalOverlay as HTMLElement).style.cssText =
                modalOverlay.style.cssText.replace(
                  /display\s*:\s*flex\s*;?/gi,
                  "",
                ) + "; display: none;";
            }
          };
          win.gBrowser.tabContainer.addEventListener(
            "TabSelect",
            tabSelectHandler,
          );
        }

        console.log("[NRPluginStoreParent] Modal dialog shown");
      } catch (error) {
        console.error("[NRPluginStoreParent] Failed to show modal:", error);
        if (!resolved) {
          resolved = true;
          resolve({
            success: false,
            error: "Failed to show installation dialog",
          });
        }
      }
    });
  }

  /**
   * Uninstall a plugin (console output only for now)
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
   * Check if a plugin is installed (always returns false for now)
   */
  private isPluginInstalled(pluginId: string): { installed: boolean } {
    console.log("[NRPluginStoreParent] isPluginInstalled called");
    console.log("[NRPluginStoreParent] pluginId:", pluginId);

    return {
      installed: false,
    };
  }
}
