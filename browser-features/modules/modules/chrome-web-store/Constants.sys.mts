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
export const GECKO_MIN_VERSION = "115.0";

/** Suffix added to Chrome extension IDs for Firefox */
export const EXTENSION_ID_SUFFIX = "@chromium-extension";

// =============================================================================
// Download URLs
// =============================================================================

/**
 * CRX download URL generators
 */
export const CRX_DOWNLOAD_URLS = {
  /**
   * Primary: Google's update URL (used by Chrome)
   * @param extensionId - Chrome extension ID
   * @param chromeVersion - Chrome version to report
   */
  primary: (extensionId: string, chromeVersion = "130.0.0.0"): string =>
    `https://clients2.google.com/service/update2/crx?response=redirect&prodversion=${chromeVersion}&acceptformat=crx2,crx3&x=id%3D${extensionId}%26uc`,

  /**
   * Fallback: Direct download with more specific parameters
   * @param extensionId - Chrome extension ID
   */
  fallback: (extensionId: string): string =>
    `https://clients2.google.com/service/update2/crx?response=redirect&os=win&arch=x64&os_arch=x86_64&nacl_arch=x86-64&prod=chromecrx&prodchannel=unknown&prodversion=130.0.0.0&acceptformat=crx2,crx3&x=id%3D${extensionId}%26uc`,
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
  "declarativeNetRequestFeedback",
  "fileBrowserHandler",
  "fileSystemProvider",
  "fontSettings",
  "gcm",
  "offscreen",
  "processes",
  "signedInDevices",
  "tabCapture",
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
  /^https:\/\/chromewebstore\.google\.com\/detail\/[^/]+\/([a-z]{32})/,
  /** Old Chrome Web Store (chrome.google.com/webstore) */
  /^https:\/\/chrome\.google\.com\/webstore\/detail\/[^/]+\/([a-z]{32})/,
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

/**
 * Unsupported code patterns in extensions
 * These patterns indicate Chrome-specific APIs that won't work in Firefox
 */
export const UNSUPPORTED_CODE_PATTERNS = [
  {
    pattern: /["']documentId["']\s*:/,
    message: "documentId property in API calls",
  },
  {
    pattern: /documentId\s*:/,
    message: "documentId property assignment",
  },
  {
    pattern: /\.documentId\b/,
    message: "documentId property access",
  },
  {
    pattern: /chrome\.offscreen\./,
    message: "chrome.offscreen API",
  },
  {
    pattern: /chrome\.tabCapture\./,
    message: "chrome.tabCapture API",
  },
] as const;

// =============================================================================
// File Size Limits
// =============================================================================

/** Maximum CRX file size (100MB) */
export const MAX_CRX_SIZE = 100 * 1024 * 1024;

/** Maximum individual file size in extension (50MB) */
export const MAX_FILE_SIZE = 50 * 1024 * 1024;

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

/**
 * Check if a URL is from Chrome Web Store
 * @param url - URL to check
 */
export function isChromeWebStoreUrl(url: string): boolean {
  return CWS_BASE_URLS.some((base) => url.startsWith(base));
}

/**
 * Extract extension ID from Chrome Web Store URL
 * @param url - Chrome Web Store extension page URL
 * @returns Extension ID or null if not found
 */
export function extractExtensionId(url: string): string | null {
  for (const pattern of CWS_URL_PATTERNS) {
    const match = url.match(pattern);
    if (match?.[1]) {
      return match[1];
    }
  }
  return null;
}

/**
 * Generate Firefox extension ID from Chrome extension ID
 * @param chromeId - Chrome extension ID
 */
export function generateFirefoxId(chromeId: string): string {
  return `${chromeId}${EXTENSION_ID_SUFFIX}`;
}

/**
 * Check if a permission is supported in Firefox
 * @param permission - Permission to check
 */
export function isPermissionSupported(permission: string): boolean {
  return !UNSUPPORTED_PERMISSIONS.includes(
    permission as (typeof UNSUPPORTED_PERMISSIONS)[number],
  );
}

/**
 * Check if a permission can be optional in Firefox
 * @param permission - Permission to check
 */
export function isOptionalPermissionAllowed(permission: string): boolean {
  return !UNSUPPORTED_OPTIONAL_PERMISSIONS.includes(
    permission as (typeof UNSUPPORTED_OPTIONAL_PERMISSIONS)[number],
  );
}

// =============================================================================
// Error Messages
// =============================================================================

/**
 * AddonManager error codes to user-friendly messages (Japanese)
 */
export const INSTALL_ERROR_MESSAGES: Record<number, string> = {
  [-1]: "ダウンロードに失敗しました",
  [-2]: "拡張機能の署名を検証できませんでした",
  [-3]: "拡張機能が破損しているか、互換性がありません",
  [-4]: "この拡張機能は Floorp と互換性がありません",
  [-5]: "この拡張機能はブロックされています",
  [-6]: "拡張機能のインストールがタイムアウトしました",
  [-7]: "拡張機能のインストールに失敗しました",
  [-8]: "拡張機能を解析できませんでした",
  [-9]: "不正な拡張機能ファイルです",
  [-10]: "ネットワークエラーが発生しました",
};
