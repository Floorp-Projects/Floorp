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
  AddonInstallListener,
  ChromeConversionReport,
  ConversionResult,
  ExtensionMetadata,
  GeckoAddonManager,
  InstallRequest,
  InstallResult,
  nsIFile,
} from "../modules/chrome-web-store/types.ts";
import {
  ChromeExtensionUpdateRegistry,
  createVerifiedUpdateProof,
  type VerifiedUpdateProof,
} from "../modules/chrome-web-store/ChromeExtensionUpdateRegistry.sys.mts";

import {
  CWS_I18N_KEYS,
  generateFirefoxId,
  INSTALL_ERROR_MESSAGE_KEYS,
  isChromeExtensionId,
} from "../modules/chrome-web-store/Constants.sys.mts";

import type { TranslationProvider } from "../modules/i18n/I18n-Utils.sys.mts";

// Lazy-loaded modules
const lazy: {
  AddonManager?: GeckoAddonManager;
  CRXConverterClass?: typeof import("../modules/chrome-web-store/CRXConverter.sys.mts").CRXConverterClass;
  downloadChromeExtension?: (id: string) => Promise<ArrayBuffer>;
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
    CRXConverterClass:
      "resource://noraneko/modules/chrome-web-store/CRXConverter.sys.mjs",
    downloadChromeExtension:
      "resource://noraneko/modules/chrome-web-store/ChromeExtensionUpdater.sys.mjs",
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
        // Chrome Web Store feature is now stable, always enabled
        return true;

      case "CWS:InstallExtension":
        // Feature is now stable, no experiment check needed
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
      if (!isChromeExtensionId(extensionId) || metadata.id !== extensionId) {
        throw new Error("Invalid Chrome extension ID");
      }
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
      let report: ChromeConversionReport | undefined;
      try {
        const converted = await this.convertCRXtoXPI(
          crxData,
          extensionId,
          metadata,
        );
        if (
          !converted.success ||
          !converted.xpiData ||
          !converted.report?.signature
        ) {
          throw (
            converted.error ??
            new Error("Authenticated conversion metadata is missing")
          );
        }
        xpiData = converted.xpiData;
        report = converted.report;
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
      const proof = await createVerifiedUpdateProof(
        generateFirefoxId(extensionId),
        report!.originalVersion,
        xpiData,
        report!.signature!.publicKeySha256,
      );
      const installResult = await this.installXPI(xpiData, metadata, proof);

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
    try {
      if (!lazy.downloadChromeExtension) {
        throw new Error("Chrome extension downloader is unavailable");
      }
      return await lazy.downloadChromeExtension(extensionId);
    } catch (error) {
      console.error("[ChromeWebStore] Download failed", error);
      return null;
    }
  }

  /**
   * Convert CRX to XPI format (async to prevent UI freezing)
   * Requires the authenticated CRXConverter pipeline
   */
  private async convertCRXtoXPI(
    crxData: ArrayBuffer,
    extensionId: string,
    metadata: ExtensionMetadata,
  ): Promise<ConversionResult> {
    // Use our CRX converter module if available
    if (lazy.CRXConverterClass) {
      return await new lazy.CRXConverterClass().convert(
        crxData,
        extensionId,
        metadata,
      );
    }

    throw new Error("Chrome extension conversion runtime is unavailable");
  }

  private async installXPI(
    xpiData: ArrayBuffer,
    metadata: ExtensionMetadata,
    proof: VerifiedUpdateProof,
  ): Promise<InstallResult> {
    if (!lazy.AddonManager) {
      return {
        success: false,
        error: t(CWS_I18N_KEYS.error.addonManagerUnavailable),
      };
    }

    let tempFile: nsIFile | undefined;
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

      tempFile = await this.createTempXPIFileAsync(xpiData, metadata.id);

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

      return await new Promise((resolve) => {
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
            if (!result.success) {
              try {
                install.cancel();
              } catch {
                /* Native installation may already be terminal. */
              }
            }
            resolve(result);
          }
        };

        const listener: AddonInstallListener = {
          onInstallStarted: () => {
            // Installation started
          },
          onInstallEnded: (_install, addon) => {
            void ChromeExtensionUpdateRegistry.recordInstalled(proof)
              .catch((error) =>
                console.error(
                  "[ChromeWebStore] Failed to enroll verified installed package",
                  error,
                ),
              )
              .finally(() => safeResolve({ success: true, addonId: addon.id }));
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
          safeResolve({
            success: false,
            error: t(CWS_I18N_KEYS.error.noBrowserWindow),
          });
        }
      });
    } catch (error) {
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      return {
        success: false,
        error: t(CWS_I18N_KEYS.error.xpiInstallError, { error: errorMessage }),
      };
    } finally {
      if (tempFile) await IOUtils.remove(tempFile.path, { ignoreAbsent: true });
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
    tempFile.createUnique(0, 0o600);

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
