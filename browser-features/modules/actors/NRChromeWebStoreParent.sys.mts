/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * NRChromeWebStoreParent - Parent process actor for Chrome Web Store integration
 *
 * This actor handles:
 * - Downloading CRX files from Chrome Web Store
 * - Converting CRX to XPI format
 * - Installing the converted extension via AddonManager
 *
 * @module NRChromeWebStoreParent
 */

import type {
  ExtensionMetadata,
  InstallRequest,
  InstallResult,
  AddonInstallListener,
  GeckoAddonManager,
  CRXConverterModule,
  nsIFile,
} from "../modules/chrome-web-store/types.ts";

import {
  CRX_DOWNLOAD_URLS,
  CRX_MAGIC,
  CRX_VERSIONS,
  INSTALL_ERROR_MESSAGES,
  generateFirefoxId,
} from "../modules/chrome-web-store/Constants.sys.mts";

// Lazy-loaded modules
const lazy: {
  AddonManager?: GeckoAddonManager;
  CRXConverter?: CRXConverterModule;
} = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
});

// Dynamically import our CRX converter
try {
  ChromeUtils.defineESModuleGetters(lazy, {
    CRXConverter:
      "resource://noraneko/modules/chrome-web-store/CRXConverter.sys.mjs",
  });
} catch (e) {
  console.warn("[NRChromeWebStore] CRXConverter module not available:", e);
}

export class NRChromeWebStoreParent extends JSWindowActorParent {
  async receiveMessage(message: {
    name: string;
    data?: InstallRequest;
  }): Promise<InstallResult | null> {
    switch (message.name) {
      case "CWS:InstallExtension":
        if (message.data) {
          return await this.installExtension(message.data);
        }
        return { success: false, error: "No installation data provided" };

      default:
        return null;
    }
  }

  private async installExtension(
    request: InstallRequest,
  ): Promise<InstallResult> {
    const { extensionId, metadata } = request;

    console.log(
      `[NRChromeWebStore] Starting installation for ${extensionId}:`,
      metadata.name,
    );

    try {
      // Step 1: Check if already installed
      const existingAddon = await this.checkExistingAddon(extensionId);
      if (existingAddon) {
        return {
          success: false,
          error: `この拡張機能は既にインストールされています (${existingAddon.name})`,
        };
      }

      // Step 2: Download CRX file
      console.log("[NRChromeWebStore] Downloading CRX file...");
      const crxData = await this.downloadCRX(extensionId);

      if (!crxData || crxData.byteLength === 0) {
        return {
          success: false,
          error: "CRXファイルのダウンロードに失敗しました",
        };
      }

      console.log(`[NRChromeWebStore] Downloaded ${crxData.byteLength} bytes`);

      // Step 3: Convert CRX to XPI
      console.log("[NRChromeWebStore] Converting CRX to XPI...");
      let xpiData: ArrayBuffer | null = null;
      try {
        xpiData = this.convertCRXtoXPI(crxData, extensionId, metadata);
      } catch (e) {
        const errorMessage = e instanceof Error ? e.message : String(e);
        return {
          success: false,
          error: `拡張機能の変換に失敗しました: ${errorMessage}`,
        };
      }

      if (!xpiData) {
        return {
          success: false,
          error: "CRXからXPIへの変換に失敗しました",
        };
      }

      // Step 4: Install the XPI
      console.log("[NRChromeWebStore] Installing XPI...");
      const installResult = await this.installXPI(xpiData, metadata);

      return installResult;
    } catch (error) {
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      console.error("[NRChromeWebStore] Installation failed:", error);
      return {
        success: false,
        error: `インストールエラー: ${errorMessage}`,
      };
    }
  }

  /**
   * Check if the extension is already installed
   * @param extensionId - Chrome extension ID
   * @returns Addon info if installed, null otherwise
   */
  private async checkExistingAddon(
    extensionId: string,
  ): Promise<{ name: string } | null> {
    if (!lazy.AddonManager) {
      return null;
    }

    // Check for Firefox-style ID (generated from Chrome extension ID)
    const firefoxId = generateFirefoxId(extensionId);

    try {
      const addon = await lazy.AddonManager.getAddonByID(firefoxId);
      if (addon) {
        return { name: addon.name };
      }
    } catch {
      // Addon not found, which is expected
    }

    return null;
  }

  private async downloadCRX(extensionId: string): Promise<ArrayBuffer | null> {
    const urls = [
      CRX_DOWNLOAD_URLS.primary(extensionId),
      CRX_DOWNLOAD_URLS.fallback(extensionId),
    ];

    for (const url of urls) {
      try {
        console.log(
          `[NRChromeWebStore] Trying download from: ${url.substring(0, 80)}...`,
        );

        const response = await fetch(url, {
          method: "GET",
          headers: {
            // Mimic Chrome's User-Agent for compatibility
            "User-Agent":
              "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Safari/537.36",
            Accept: "application/x-chrome-extension",
          },
          redirect: "follow",
        });

        if (!response.ok) {
          console.warn(
            `[NRChromeWebStore] Download failed with status: ${response.status}`,
          );
          continue;
        }

        const contentType = response.headers.get("content-type") || "";
        console.log(`[NRChromeWebStore] Content-Type: ${contentType}`);

        const arrayBuffer = await response.arrayBuffer();

        // Verify it's a CRX file (magic number check)
        if (this.verifyCRXHeader(arrayBuffer)) {
          return arrayBuffer;
        }

        console.warn("[NRChromeWebStore] Downloaded file is not a valid CRX");
      } catch (error) {
        console.warn(`[NRChromeWebStore] Download error:`, error);
      }
    }

    return null;
  }

  /**
   * Verify that the data is a valid CRX file
   * @param data - Raw file data
   * @returns true if valid CRX file
   */
  private verifyCRXHeader(data: ArrayBuffer): boolean {
    if (data.byteLength < 16) {
      return false;
    }

    const view = new DataView(data);

    // CRX files start with "Cr24" magic number
    const magic = String.fromCharCode(
      view.getUint8(0),
      view.getUint8(1),
      view.getUint8(2),
      view.getUint8(3),
    );

    if (magic !== CRX_MAGIC) {
      console.warn(`[NRChromeWebStore] Invalid CRX magic: ${magic}`);
      return false;
    }

    // Check CRX version (should be 2 or 3)
    const version = view.getUint32(4, true);
    if (!(CRX_VERSIONS as readonly number[]).includes(version)) {
      console.warn(`[NRChromeWebStore] Unsupported CRX version: ${version}`);
      return false;
    }

    console.log(`[NRChromeWebStore] Valid CRX${version} file detected`);
    return true;
  }

  /**
   * Convert CRX to XPI format
   * Uses CRXConverter module if available, falls back to inline conversion
   */
  private convertCRXtoXPI(
    crxData: ArrayBuffer,
    extensionId: string,
    metadata: ExtensionMetadata,
  ): ArrayBuffer | null {
    // Use our CRX converter module if available
    if (lazy.CRXConverter) {
      return lazy.CRXConverter.convert(crxData, extensionId, metadata);
    }

    // Fallback: inline conversion
    return this.inlineConvertCRXtoXPI(crxData, extensionId, metadata);
  }

  private inlineConvertCRXtoXPI(
    crxData: ArrayBuffer,
    _extensionId: string,
    _metadata: ExtensionMetadata,
  ): ArrayBuffer | null {
    try {
      const view = new DataView(crxData);
      const version = view.getUint32(4, true);

      let zipOffset: number;

      if (version === 2) {
        // CRX2 format
        const publicKeyLength = view.getUint32(8, true);
        const signatureLength = view.getUint32(12, true);
        zipOffset = 16 + publicKeyLength + signatureLength;
      } else if (version === 3) {
        // CRX3 format
        const headerLength = view.getUint32(8, true);
        zipOffset = 12 + headerLength;
      } else {
        throw new Error(`Unsupported CRX version: ${version}`);
      }

      console.log(`[NRChromeWebStore] ZIP data starts at offset ${zipOffset}`);

      // Extract the ZIP portion
      const zipData = crxData.slice(zipOffset);

      // Verify ZIP signature
      const zipView = new DataView(zipData);
      const zipSignature = zipView.getUint32(0, true);
      if (zipSignature !== 0x04034b50) {
        // "PK\x03\x04"
        throw new Error("Invalid ZIP signature in CRX");
      }

      // For now, return the raw ZIP data
      // In production, we would modify the manifest.json inside the ZIP
      // to add Firefox-specific settings
      console.log(
        `[NRChromeWebStore] Extracted ZIP data: ${zipData.byteLength} bytes`,
      );

      // TODO: Modify manifest.json to add browser_specific_settings.gecko.id
      // This requires ZIP manipulation which is complex without a library
      // For now, we'll return the ZIP as-is and rely on Firefox's compatibility

      return zipData;
    } catch (error) {
      console.error("[NRChromeWebStore] Inline CRX conversion failed:", error);
      return null;
    }
  }

  private async installXPI(
    xpiData: ArrayBuffer,
    metadata: ExtensionMetadata,
  ): Promise<InstallResult> {
    if (!lazy.AddonManager) {
      return {
        success: false,
        error: "AddonManager is not available",
      };
    }

    try {
      // Get the browser window for the installation prompt
      const browserWindow = this.browsingContext
        ?.topChromeWindow as Window | null;

      if (!browserWindow) {
        return {
          success: false,
          error: "Could not find browser window for installation",
        };
      }

      console.log("[NRChromeWebStore] Creating install from ArrayBuffer...");

      // Use AddonManager.getInstallForFile for file-based installation
      const install = await lazy.AddonManager.getInstallForFile(
        this.createTempXPIFile(xpiData, metadata.id),
        "application/x-xpinstall",
      );

      if (!install) {
        return {
          success: false,
          error: "Failed to create installation object",
        };
      }

      // Set up install listeners
      return new Promise((resolve) => {
        const listener: AddonInstallListener = {
          onInstallEnded: (_install, addon) => {
            console.log(
              `[NRChromeWebStore] Installation completed: ${addon.id}`,
            );
            resolve({
              success: true,
              addonId: addon.id,
            });
          },

          onInstallFailed: (installObj) => {
            console.error(
              `[NRChromeWebStore] Installation failed: ${installObj.error}`,
            );
            resolve({
              success: false,
              error: this.getInstallErrorMessage(installObj.error ?? -7),
            });
          },

          onInstallCancelled: () => {
            console.log("[NRChromeWebStore] Installation cancelled by user");
            resolve({
              success: false,
              error: "ユーザーによりインストールがキャンセルされました",
            });
          },
        };

        install.addListener(listener);

        // Trigger the installation
        install.install();
      });
    } catch (error) {
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      console.error("[NRChromeWebStore] XPI installation error:", error);
      return {
        success: false,
        error: `XPIインストールエラー: ${errorMessage}`,
      };
    }
  }

  /**
   * Create a temporary XPI file from the converted extension data
   * @param xpiData - XPI file data as ArrayBuffer
   * @param extensionId - Chrome extension ID (used for filename)
   * @returns nsIFile pointing to the temporary file
   */
  private createTempXPIFile(
    xpiData: ArrayBuffer,
    extensionId: string,
  ): nsIFile {
    // Create a temporary file for the XPI
    const tempDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
    const tempFile = tempDir.clone();
    tempFile.append(`floorp-cws-${extensionId}.xpi`);

    // Write the XPI data to the file
    const outputStream = Cc[
      "@mozilla.org/network/file-output-stream;1"
    ].createInstance(Ci.nsIFileOutputStream);

    outputStream.init(tempFile, 0x02 | 0x08 | 0x20, 0o644, 0); // PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE

    const binaryOutputStream = Cc[
      "@mozilla.org/binaryoutputstream;1"
    ].createInstance(Ci.nsIBinaryOutputStream);
    binaryOutputStream.setOutputStream(outputStream);

    const uint8Array = new Uint8Array(xpiData);
    binaryOutputStream.writeByteArray(Array.from(uint8Array));
    binaryOutputStream.close();
    outputStream.close();

    console.log(`[NRChromeWebStore] Created temp XPI file: ${tempFile.path}`);

    return tempFile;
  }

  /**
   * Convert AddonManager error codes to user-friendly messages
   * @param errorCode - AddonManager error code
   * @returns Localized error message
   */
  private getInstallErrorMessage(errorCode: number): string {
    return (
      INSTALL_ERROR_MESSAGES[errorCode] ||
      `インストールエラー (コード: ${errorCode})`
    );
  }
}
