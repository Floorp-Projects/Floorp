/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Constants for Chrome Web Store integration
 */

// =============================================================================
// CRX File Format Constants
// =============================================================================

/** CRX magic number "Cr24" */
export const CRX_MAGIC = "Cr24";

/** CRX version 2 identifier */
export const CRX2_VERSION = 2;

/** CRX version 3 identifier */
export const CRX3_VERSION = 3;

/** Supported CRX versions */
export const CRX_VERSIONS = [CRX2_VERSION, CRX3_VERSION] as const;

/** ZIP file signature "PK\x03\x04" */
export const ZIP_SIGNATURE = 0x04034b50;

// =============================================================================
// Firefox/Gecko Compatibility Settings
// =============================================================================

/** Minimum Gecko version required for extensions */
export const GECKO_MIN_VERSION = "157.0";

/** Suffix added to Chrome extension IDs for Firefox */
export const EXTENSION_ID_SUFFIX = "@chromium-extension";

// =============================================================================
// Download URLs
// =============================================================================

/**
 * Chrome reference used for store package selection and minimum-version gating.
 * This does not assert that Floorp implements every API in that Chrome build.
 */
export const STORE_DOWNLOAD_CLIENT_VERSION = "155.0.8059.84";

export interface ChromeDownloadTarget {
  readonly os: "win" | "mac" | "linux";
  readonly arch: "x86" | "x64" | "arm64";
}

/** Resolve the browser process target without assuming a Windows package. */
export function resolveChromeDownloadTarget(
  platform: string,
  xpcomABI: string,
): ChromeDownloadTarget {
  const os = platform === "win"
    ? "win"
    : platform === "macosx"
    ? "mac"
    : platform === "linux"
    ? "linux"
    : null;
  const processArch = xpcomABI.split("-", 1)[0];
  const arch = processArch === "x86_64"
    ? "x64"
    : processArch === "aarch64" || processArch === "arm64"
    ? "arm64"
    : processArch === "x86" || /^i[3-6]86$/.test(processArch)
    ? "x86"
    : null;
  if (!os || !arch) {
    throw new Error(
      `Unsupported Chrome Web Store download target: ${platform}/${xpcomABI}`,
    );
  }
  return { os, arch };
}

function crxDownloadURL(
  extensionId: string,
  target: ChromeDownloadTarget,
  fallback: boolean,
): string {
  if (!isChromeExtensionId(extensionId)) {
    throw new Error("Invalid Chrome extension ID");
  }
  if (
    !["win", "mac", "linux"].includes(target.os) ||
    !["x86", "x64", "arm64"].includes(target.arch)
  ) {
    throw new Error("Unsupported Chrome Web Store download target");
  }
  const parameters = new URLSearchParams({
    response: "redirect",
    prodversion: STORE_DOWNLOAD_CLIENT_VERSION,
    os: target.os,
    arch: target.arch,
    acceptformat: "crx2,crx3",
    x: `id=${extensionId}&uc`,
  });
  if (fallback) {
    parameters.set("prod", "chromecrx");
    parameters.set("prodchannel", "unknown");
  }
  return `https://clients2.google.com/service/update2/crx?${parameters}`;
}

/** Both attempts select packages for the same actual browser process target. */
export const CRX_DOWNLOAD_URLS = {
  /**
   * Primary download URL using Google's update endpoint
   * @param extensionId - Chrome extension ID (32 lowercase letters)
   * @param target - Actual browser process platform and architecture
   * @returns Download URL for the CRX file
   */
  primary: (extensionId: string, target: ChromeDownloadTarget): string =>
    crxDownloadURL(extensionId, target, false),

  /**
   * Alternate store query without switching to a different platform package
   * @param extensionId - Chrome extension ID (32 lowercase letters)
   * @returns Download URL for the CRX file
   */
  fallback: (extensionId: string, target: ChromeDownloadTarget): string =>
    crxDownloadURL(extensionId, target, true),
} as const;

// =============================================================================
// Unsupported Permissions
// =============================================================================

/**
 * Chrome-specific permissions that Firefox doesn't support
 */
export const UNSUPPORTED_PERMISSIONS = [
  // Enterprise/Chrome OS specific
  "enterprise.deviceAttributes",
  "enterprise.hardwarePlatform",
  "enterprise.networkingAttributes",
  "enterprise.platformKeys",
  "loginState",
  "platformKeys",

  // Chrome-specific APIs
  "fileBrowserHandler",
  "fileSystemProvider",
  "gcm",
  "processes",
  "signedInDevices",
  "ttsEngine",
  "wallpaper",

  // Printing (limited support in Firefox)
  "printing",
  "printingMetrics",
] as const;

/**
 * Permissions that are not allowed in optional_permissions
 */
export const UNSUPPORTED_OPTIONAL_PERMISSIONS = [
  ...UNSUPPORTED_PERMISSIONS,
  "contextMenus", // Cannot be optional in Firefox
] as const;

// =============================================================================
// URL Patterns
// =============================================================================

/**
 * Chrome Web Store URL patterns for extension pages
 */
export const CWS_URL_PATTERNS = [
  /** New Chrome Web Store (chromewebstore.google.com) */
  /^https:\/\/chromewebstore\.google\.com\/detail\/[^/]+\/([a-p]{32})(?=[/?#]|$)/,
  /** Old Chrome Web Store (chrome.google.com/webstore) */
  /^https:\/\/chrome\.google\.com\/webstore\/detail\/[^/]+\/([a-p]{32})(?=[/?#]|$)/,
] as const;

/**
 * Chrome Web Store base URLs
 */
export const CWS_BASE_URLS = [
  "https://chromewebstore.google.com/",
  "https://chrome.google.com/webstore/",
] as const;

// =============================================================================
// HTTP Headers
// =============================================================================

/**
 * User-Agent for CRX downloads (mimics Chrome)
 */
export const CHROME_USER_AGENT =
  "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Safari/537.36";

/**
 * Accept header for CRX downloads
 */
export const CRX_ACCEPT_HEADER = "application/x-chrome-extension";

// =============================================================================
// Validation Patterns
// =============================================================================

/** Pattern definition for unsupported code detection */
export interface UnsupportedCodePattern {
  readonly pattern: RegExp;
  readonly message: string;
}

/**
 * Unsupported code patterns in extensions
 * These patterns indicate Chrome-specific APIs that won't work in Firefox
 *
 * documentId is provided by the pinned Gecko Runtime.
 */
export const UNSUPPORTED_CODE_PATTERNS: readonly UnsupportedCodePattern[] = [
  {
    pattern: /chrome\.gcm\./,
    message: "chrome.gcm API",
  },
];

// =============================================================================
// File Size Limits
// =============================================================================

/** Maximum CRX file size (100MB) */
export const MAX_CRX_SIZE = 100 * 1024 * 1024;

/** Maximum individual file size in extension (50MB) */
export const MAX_FILE_SIZE = 50 * 1024 * 1024;

export const MAX_EXPANDED_CRX_SIZE = 500 * 1024 * 1024;
export const MAX_CRX_ENTRIES = 10000;

// =============================================================================
// Timeouts
// =============================================================================

/** Download timeout in milliseconds (5 minutes) */
export const DOWNLOAD_TIMEOUT_MS = 5 * 60 * 1000;

/** Installation timeout in milliseconds (2 minutes) */
export const INSTALL_TIMEOUT_MS = 2 * 60 * 1000;

// =============================================================================
// Utility Functions
// =============================================================================

/** Chrome extension IDs encode a hash using the lowercase a-p alphabet. */
const EXTENSION_ID_PATTERN = /^[a-p]{32}$/;

export function isChromeExtensionId(id: string): boolean {
  return EXTENSION_ID_PATTERN.test(id);
}

/**
 * Check if a URL is from Chrome Web Store
 * @param url - URL to check
 * @returns true if the URL is from Chrome Web Store
 */
export function isChromeWebStoreUrl(url: string): boolean {
  return CWS_BASE_URLS.some((base) => url.startsWith(base));
}

/**
 * Extract extension ID from Chrome Web Store URL
 * @param url - Chrome Web Store extension page URL
 * @returns Extension ID (32 lowercase letters) or null if not found
 */
export function extractExtensionId(url: string): string | null {
  for (const pattern of CWS_URL_PATTERNS) {
    const match = url.match(pattern);
    if (match?.[1] && isChromeExtensionId(match[1])) {
      return match[1];
    }
  }
  return null;
}

/**
 * Generate Firefox extension ID from Chrome extension ID
 * @param chromeId - Chrome extension ID (32 lowercase letters)
 * @returns Firefox-compatible extension ID
 */
export function generateFirefoxId(chromeId: string): string {
  return `${chromeId}${EXTENSION_ID_SUFFIX}`;
}

/**
 * Check if a permission is supported in Firefox
 * @param permission - Permission to check
 * @returns true if the permission is supported
 */
export function isPermissionSupported(permission: string): boolean {
  return !UNSUPPORTED_PERMISSIONS.includes(
    permission as (typeof UNSUPPORTED_PERMISSIONS)[number],
  );
}

/**
 * Check if a permission can be optional in Firefox
 * @param permission - Permission to check
 * @returns true if the permission can be optional
 */
export function isOptionalPermissionAllowed(permission: string): boolean {
  return !UNSUPPORTED_OPTIONAL_PERMISSIONS.includes(
    permission as (typeof UNSUPPORTED_OPTIONAL_PERMISSIONS)[number],
  );
}

// =============================================================================
// Error Messages - i18n Translation Keys
// =============================================================================

/**
 * AddonManager error codes to i18n translation keys
 * These keys correspond to entries in browser-chrome.json
 */
export const INSTALL_ERROR_MESSAGE_KEYS: Record<number, string> = {
  [-1]: "chromeWebStore.installErrors.downloadFailed",
  [-2]: "chromeWebStore.installErrors.signatureVerifyFailed",
  [-3]: "chromeWebStore.installErrors.corruptOrIncompatible",
  [-4]: "chromeWebStore.installErrors.incompatible",
  [-5]: "chromeWebStore.installErrors.blocked",
  [-6]: "chromeWebStore.installErrors.timeout",
  [-7]: "chromeWebStore.installErrors.installFailed",
  [-8]: "chromeWebStore.installErrors.parseFailed",
  [-9]: "chromeWebStore.installErrors.invalidFile",
  [-10]: "chromeWebStore.installErrors.networkError",
};

/**
 * i18n Translation Keys for Chrome Web Store messages
 */
export const CWS_I18N_KEYS = {
  // Button labels
  button: {
    addToFloorp: "chromeWebStore.button.addToFloorp",
    installing: "chromeWebStore.button.installing",
    success: "chromeWebStore.button.success",
    error: "chromeWebStore.button.error",
    installed: "chromeWebStore.button.installed",
    close: "chromeWebStore.button.close",
  },
  // Error messages
  error: {
    title: "chromeWebStore.error.title",
    compatibilityNote: "chromeWebStore.error.compatibilityNote",
    alreadyInstalled: "chromeWebStore.error.alreadyInstalled",
    downloadFailed: "chromeWebStore.error.downloadFailed",
    conversionFailed: "chromeWebStore.error.conversionFailed",
    conversionFailedGeneric: "chromeWebStore.error.conversionFailedGeneric",
    installError: "chromeWebStore.error.installError",
    installFailed: "chromeWebStore.error.installFailed",
    noData: "chromeWebStore.error.noData",
    addonManagerUnavailable: "chromeWebStore.error.addonManagerUnavailable",
    noBrowserWindow: "chromeWebStore.error.noBrowserWindow",
    createInstallFailed: "chromeWebStore.error.createInstallFailed",
    cancelledByUser: "chromeWebStore.error.cancelledByUser",
    downloadCancelled: "chromeWebStore.error.downloadCancelled",
    timeout: "chromeWebStore.error.timeout",
    xpiInstallError: "chromeWebStore.error.xpiInstallError",
    errorWithCode: "chromeWebStore.error.errorWithCode",
  },
} as const;
