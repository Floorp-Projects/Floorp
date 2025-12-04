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
  INSTALL_ERROR_MESSAGE_KEYS,
  CWS_I18N_KEYS,
  CWS_EXPERIMENT_ID,
  generateFirefoxId,
} from "../modules/chrome-web-store/Constants.sys.mts";

import type { TranslationProvider } from "../modules/i18n/I18n-Utils.sys.mts";

// Lazy-loaded modules
const lazy: {
  AddonManager?: GeckoAddonManager;
  CRXConverter?: CRXConverterModule;
  setTimeout?: (callback: () => void, ms: number) => nsITimer;
  clearTimeout?: (timer: nsITimer) => void;
} = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
  clearTimeout: "resource://gre/modules/Timer.sys.mjs",
});

// Dynamically import our CRX converter
try {
  ChromeUtils.defineESModuleGetters(lazy, {
    CRXConverter:
      "resource://noraneko/modules/chrome-web-store/CRXConverter.sys.mjs",
  });
} catch {
  // CRXConverter module not available
}

/**
 * WeakSet to track installs from Chrome Web Store
 * This is used by the UI to customize the installation dialog
 */
const chromeWebStoreInstalls = new WeakSet<object>();

/**
 * Check if an install is from Chrome Web Store
 * Exported for use by UI components
 */
export function isChromeWebStoreInstall(install: object): boolean {
  return chromeWebStoreInstalls.has(install);
}

/** Maximum retry attempts for waiting i18n initialization */
const I18N_INIT_MAX_RETRIES = 10;
/** Delay between retries in milliseconds */
const I18N_INIT_RETRY_DELAY = 100;

/**
 * Get translation function from I18nUtils
 * @returns Translation function or null if not available
 */
function getTranslator(): TranslationProvider["t"] | null {
  try {
    const { I18nUtils } = ChromeUtils.importESModule(
      "resource://noraneko/modules/i18n/I18n-Utils.sys.mjs",
    );
    const provider = I18nUtils.getTranslationProvider();
    if (provider && typeof provider.t === "function") {
      return provider.t;
    }
  } catch {
    // Translation provider not available
  }
  return null;
}

/**
 * Wait for i18n to be initialized with retry logic
 */
async function waitForI18nInit(): Promise<void> {
  for (let i = 0; i < I18N_INIT_MAX_RETRIES; i++) {
    if (getTranslator() !== null) {
      return;
    }
    // Wait and retry
    await new Promise((resolve) => {
      if (lazy.setTimeout) {
        lazy.setTimeout(resolve as () => void, I18N_INIT_RETRY_DELAY);
      } else {
        resolve(undefined);
      }
    });
  }
}

/**
 * Translate a key with optional variables
 */
function t(key: string, vars?: Record<string, string | number>): string {
  const translator = getTranslator();
  if (translator) {
    const result = translator(key, vars);
    if (typeof result === "string") {
      return result;
    }
  }
  return key;
}

/**
 * Check if the Chrome Web Store feature is enabled via experiment
 * @returns true if the experiment is active (variant is not null)
 */
function isExperimentEnabled(): boolean {
  try {
    const { Experiments } = ChromeUtils.importESModule(
      "resource://noraneko/modules/experiments/Experiments.sys.mjs",
    );
    const variant = Experiments.getVariant(CWS_EXPERIMENT_ID);
    return variant !== null;
  } catch {
    // If experiments system fails, default to disabled for safety
    return false;
  }
}

export class NRChromeWebStoreParent extends JSWindowActorParent {
  async receiveMessage(message: {
    name: string;
    data?: InstallRequest | { keys: string[] } | { extensionId: string };
  }): Promise<
    | InstallResult
    | Record<string, string>
    | boolean
    | { installed: boolean }
    | null
  > {
    switch (message.name) {
      case "CWS:CheckExperiment":
        // Check if the Chrome Web Store feature is enabled via experiment
        return isExperimentEnabled();

      case "CWS:InstallExtension":
        // Check experiment before allowing installation
        if (!isExperimentEnabled()) {
          return {
            success: false,
            error: t(CWS_I18N_KEYS.error.experimentDisabled),
          };
        }
        if (
          message.data &&
          "extensionId" in message.data &&
          "metadata" in message.data
        ) {
          return await this.installExtension(message.data as InstallRequest);
        }
        return { success: false, error: t(CWS_I18N_KEYS.error.noData) };

      case "CWS:GetTranslations":
        // Wait for i18n initialization if not ready yet
        await waitForI18nInit();
        // Return translations for the requested keys
        if (message.data && "keys" in message.data) {
          const keys = (message.data as { keys: string[] }).keys;
          const translations: Record<string, string> = {};
          for (const key of keys) {
            translations[key] = t(key);
          }
          return translations;
        }
        return null;

      case "CWS:CheckAddonInstalled":
        if (message.data && "extensionId" in message.data) {
          const result = await this.checkExistingAddon(
            (message.data as { extensionId: string }).extensionId,
          );
          return { installed: !!result };
        }
        return { installed: false };

      default:
        return null;
    }
  }

  private async installExtension(
    request: InstallRequest,
  ): Promise<InstallResult> {
    const { extensionId, metadata } = request;

    try {
      // Step 1: Check if already installed
      const existingAddon = await this.checkExistingAddon(extensionId);
      if (existingAddon) {
        return {
          success: false,
          error: t(CWS_I18N_KEYS.error.alreadyInstalled, {
            name: existingAddon.name,
          }),
        };
      }

      // Step 2: Download CRX file
      const crxData = await this.downloadCRX(extensionId);

      if (!crxData || crxData.byteLength === 0) {
        return {
          success: false,
          error: t(CWS_I18N_KEYS.error.downloadFailed),
        };
      }

      // Step 3: Convert CRX to XPI (async to prevent UI freezing)
      let xpiData: ArrayBuffer | null = null;
      try {
        xpiData = await this.convertCRXtoXPI(crxData, extensionId, metadata);
      } catch (conversionError) {
        const errorMsg =
          conversionError instanceof Error
            ? conversionError.message
            : String(conversionError);
        return {
          success: false,
          error: t(CWS_I18N_KEYS.error.conversionFailed, {
            error: errorMsg,
          }),
        };
      }

      if (!xpiData) {
        return {
          success: false,
          error: t(CWS_I18N_KEYS.error.conversionFailedGeneric),
        };
      }

      // Step 4: Install the XPI
      const installResult = await this.installXPI(xpiData, metadata);

      return installResult;
    } catch (error) {
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      return {
        success: false,
        error: t(CWS_I18N_KEYS.error.installError, { error: errorMessage }),
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
          continue;
        }

        const arrayBuffer = await response.arrayBuffer();

        // Verify it's a CRX file (magic number check)
        if (this.verifyCRXHeader(arrayBuffer)) {
          return arrayBuffer;
        }
      } catch {
        // Download error, try next URL
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
      return false;
    }

    // Check CRX version (should be 2 or 3)
    const version = view.getUint32(4, true);
    if (!(CRX_VERSIONS as readonly number[]).includes(version)) {
      return false;
    }

    return true;
  }

  /**
   * Convert CRX to XPI format (async to prevent UI freezing)
   * Uses CRXConverter module if available, falls back to inline conversion
   */
  private async convertCRXtoXPI(
    crxData: ArrayBuffer,
    extensionId: string,
    metadata: ExtensionMetadata,
  ): Promise<ArrayBuffer | null> {
    // Use our CRX converter module if available
    if (lazy.CRXConverter) {
      return await lazy.CRXConverter.convert(crxData, extensionId, metadata);
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

      // TODO: Modify manifest.json to add browser_specific_settings.gecko.id
      // This requires ZIP manipulation which is complex without a library
      // For now, we'll return the ZIP as-is and rely on Firefox's compatibility

      return zipData;
    } catch {
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
        error: t(CWS_I18N_KEYS.error.addonManagerUnavailable),
      };
    }

    try {
      // Get the browser window for the installation prompt
      const browserWindow = this.browsingContext
        ?.topChromeWindow as Window | null;

      if (!browserWindow) {
        return {
          success: false,
          error: t(CWS_I18N_KEYS.error.noBrowserWindow),
        };
      }

      const tempFile = await this.createTempXPIFileAsync(xpiData, metadata.id);

      // Use AddonManager.getInstallForFile for file-based installation
      const install = await lazy.AddonManager.getInstallForFile(
        tempFile,
        "application/x-xpinstall",
      );

      if (!install) {
        return {
          success: false,
          error: t(CWS_I18N_KEYS.error.createInstallFailed),
        };
      }

      // Mark this install as coming from Chrome Web Store
      // This allows the UI to customize the installation dialog
      chromeWebStoreInstalls.add(install);

      // Set up install listeners with timeout
      const INSTALL_TIMEOUT_MS = 60000; // 60 seconds timeout

      return new Promise((resolve) => {
        let resolved = false;
        let timeoutId: nsITimer | null = null;

        const cleanup = () => {
          if (timeoutId && lazy.clearTimeout) {
            lazy.clearTimeout(timeoutId);
            timeoutId = null;
          }
          install.removeListener(listener);
        };

        const safeResolve = (result: InstallResult) => {
          if (!resolved) {
            resolved = true;
            cleanup();
            resolve(result);
          }
        };

        const listener: AddonInstallListener = {
          onInstallStarted: () => {
            // Installation started
          },
          onInstallEnded: (_install, addon) => {
            safeResolve({
              success: true,
              addonId: addon.id,
            });
          },

          onInstallFailed: (installObj) => {
            safeResolve({
              success: false,
              error: this.getInstallErrorMessage(installObj.error ?? -7),
            });
          },

          onInstallCancelled: () => {
            safeResolve({
              success: false,
              error: t(CWS_I18N_KEYS.error.cancelledByUser),
            });
          },

          onDownloadFailed: (installObj) => {
            safeResolve({
              success: false,
              error: this.getInstallErrorMessage(installObj.error ?? -7),
            });
          },

          onDownloadCancelled: () => {
            safeResolve({
              success: false,
              error: t(CWS_I18N_KEYS.error.downloadCancelled),
            });
          },
        };

        install.addListener(listener);

        // Set up timeout to prevent hanging
        if (lazy.setTimeout) {
          timeoutId = lazy.setTimeout(() => {
            safeResolve({
              success: false,
              error: t(CWS_I18N_KEYS.error.timeout),
            });
          }, INSTALL_TIMEOUT_MS);
        }

        // Get the browser element for the installation prompt
        const win = browserWindow as Window & {
          gBrowser?: { selectedBrowser?: unknown };
          __chromeWebStoreInstallInfo?: { name: string; id: string };
        };
        const browser = win.gBrowser?.selectedBrowser;

        if (browser && lazy.AddonManager) {
          // Set a flag on the window to indicate this is a Chrome Web Store install
          // This allows the UI customization script to detect and modify the dialog
          win.__chromeWebStoreInstallInfo = {
            name: metadata.name,
            id: metadata.id,
          };

          // Also notify via observer for cross-context communication
          Services.obs.notifyObservers(
            { wrappedJSObject: { name: metadata.name, id: metadata.id } },
            "floorp-chrome-web-store-install-started",
          );

          // Use installAddonFromAOM to show the standard addon installation popup
          // This triggers the doorhanger UI that Firefox uses for addon installations
          lazy.AddonManager.installAddonFromAOM(
            browser,
            null, // uri - not needed for file-based installs
            install,
          );
        } else {
          // Fallback to silent installation if browser element is not available
          install.install();
        }
      });
    } catch (error) {
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      return {
        success: false,
        error: t(CWS_I18N_KEYS.error.xpiInstallError, { error: errorMessage }),
      };
    }
  }

  /**
   * Create a temporary XPI file from the converted extension data (async for better performance)
   * @param xpiData - XPI file data as ArrayBuffer
   * @param extensionId - Chrome extension ID (used for filename)
   * @returns nsIFile pointing to the temporary file
   */
  private async createTempXPIFileAsync(
    xpiData: ArrayBuffer,
    extensionId: string,
  ): Promise<nsIFile> {
    // Create a temporary file for the XPI
    const tempDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
    const tempFile = tempDir.clone();
    tempFile.append(`floorp-cws-${extensionId}.xpi`);

    // Use IOUtils for fast async write
    const uint8Array = new Uint8Array(xpiData);
    await IOUtils.write(tempFile.path, uint8Array);

    return tempFile;
  }

  /**
   * Convert AddonManager error codes to user-friendly messages
   * @param errorCode - AddonManager error code
   * @returns Localized error message
   */
  private getInstallErrorMessage(errorCode: number): string {
    const key = INSTALL_ERROR_MESSAGE_KEYS[errorCode];
    if (key) {
      return t(key);
    }
    return t(CWS_I18N_KEYS.error.errorWithCode, { code: String(errorCode) });
  }
}
