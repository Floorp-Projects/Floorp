/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Represents a detected external browser on the system
 */
export interface ExternalBrowser {
  /** Unique identifier for the browser (e.g., "chrome", "edge", "safari") */
  id: string;
  /** Human-readable display name */
  name: string;
  /** Path to the browser executable */
  path: string;
  /** Optional icon identifier for UI display */
  icon?: string;
  /** Whether the browser is available (exists and is executable) */
  available: boolean;
}

/**
 * Platform-specific browser detection configuration
 */
export interface BrowserDetectionConfig {
  /** Browser identifier */
  id: string;
  /** Display name */
  name: string;
  /** Icon identifier */
  icon?: string;
  /** Paths to check for Windows */
  windowsPaths?: string[];
  /** Paths to check for macOS */
  macPaths?: string[];
  /** Paths to check for Linux */
  linuxPaths?: string[];
  /** Windows registry key for detection (optional) */
  windowsRegistryKey?: string;
  /** macOS bundle identifier for detection via mdfind (optional) */
  macBundleId?: string;
}

/**
 * Options for opening a URL in an external browser
 */
export interface OpenInBrowserOptions {
  /** The URL to open */
  url: string;
  /** The browser to use (by id) */
  browserId?: string;
  /** Whether to open in private/incognito mode if supported */
  privateMode?: boolean;
}

/**
 * Result of attempting to open a URL in an external browser
 */
export interface OpenResult {
  /** Whether the operation succeeded */
  success: boolean;
  /** Error message if failed */
  error?: string;
}
