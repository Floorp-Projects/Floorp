/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Constants for Chrome Web Store integration
 */

// CRX file format constants
export const CRX_MAGIC = "Cr24";
export const CRX2_VERSION = 2;
export const CRX3_VERSION = 3;
export const ZIP_SIGNATURE = 0x04034b50; // "PK\x03\x04"

// Firefox compatibility settings
export const GECKO_MIN_VERSION = "115.0";
export const EXTENSION_ID_SUFFIX = "@chromium-extension";

// CRX download URL generators
export const CRX_DOWNLOAD_URLS = {
  /**
   * Primary: Google's update URL (used by Chrome)
   */
  primary: (extensionId: string, chromeVersion = "130.0.0.0") =>
    `https://clients2.google.com/service/update2/crx?response=redirect&prodversion=${chromeVersion}&acceptformat=crx2,crx3&x=id%3D${extensionId}%26uc`,

  /**
   * Fallback: Direct download with more specific parameters
   */
  fallback: (extensionId: string) =>
    `https://clients2.google.com/service/update2/crx?response=redirect&os=win&arch=x64&os_arch=x86_64&nacl_arch=x86-64&prod=chromecrx&prodchannel=unknown&prodversion=130.0.0.0&acceptformat=crx2,crx3&x=id%3D${extensionId}%26uc`,
} as const;

// Chrome-specific permissions that Firefox doesn't support
export const UNSUPPORTED_PERMISSIONS = [
  "declarativeNetRequestFeedback",
  "enterprise.deviceAttributes",
  "enterprise.hardwarePlatform",
  "enterprise.networkingAttributes",
  "enterprise.platformKeys",
  "fileBrowserHandler",
  "fileSystemProvider",
  "fontSettings",
  "gcm",
  "loginState",
  "offscreen",
  "platformKeys",
  "printing",
  "printingMetrics",
  "processes",
  "signedInDevices",
  "tabCapture",
  "ttsEngine",
  "wallpaper",
] as const;

// Chrome Web Store URL patterns
export const CWS_URL_PATTERNS = [
  /^https:\/\/chromewebstore\.google\.com\/detail\/[^/]+\/([a-z]{32})/,
  /^https:\/\/chrome\.google\.com\/webstore\/detail\/[^/]+\/([a-z]{32})/,
] as const;

// User-Agent for CRX downloads (mimics Chrome)
export const CHROME_USER_AGENT =
  "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Safari/537.36";
