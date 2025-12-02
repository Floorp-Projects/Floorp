/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Type definitions for Chrome Web Store integration
 */

/**
 * Extension metadata extracted from Chrome Web Store page
 */
export interface ExtensionMetadata {
  id: string;
  name: string;
  version?: string;
  description?: string;
  iconUrl?: string;
  author?: string;
}

/**
 * Chrome extension manifest structure
 */
export interface ChromeManifest {
  manifest_version: number;
  name: string;
  version: string;
  description?: string;
  permissions?: string[];
  optional_permissions?: string[];
  host_permissions?: string[];
  background?: ChromeManifestBackground;
  content_scripts?: ChromeManifestContentScript[];
  action?: Record<string, unknown>;
  browser_action?: Record<string, unknown>;
  page_action?: Record<string, unknown>;
  icons?: Record<string, string>;
  web_accessible_resources?: WebAccessibleResources;
  browser_specific_settings?: BrowserSpecificSettings;
  [key: string]: unknown;
}

export interface ChromeManifestBackground {
  service_worker?: string;
  scripts?: string[];
  persistent?: boolean;
  type?: string;
}

export interface ChromeManifestContentScript {
  matches: string[];
  js?: string[];
  css?: string[];
  run_at?: string;
}

/**
 * MV2 format: string[]
 * MV3 format: Array<{ resources: string[]; matches: string[] }>
 */
export type WebAccessibleResources = string[] | WebAccessibleResourceEntry[];

export interface WebAccessibleResourceEntry {
  resources: string[];
  matches: string[];
}

export interface BrowserSpecificSettings {
  gecko?: GeckoSpecificSettings;
}

export interface GeckoSpecificSettings {
  id?: string;
  strict_min_version?: string;
  strict_max_version?: string;
}

/**
 * Result of CRX to XPI conversion
 */
export interface ConversionResult {
  success: boolean;
  xpiData?: ArrayBuffer;
  originalManifest?: ChromeManifest;
  transformedManifest?: ChromeManifest;
  error?: string;
}

/**
 * Result of extension installation
 */
export interface InstallResult {
  success: boolean;
  error?: string;
  addonId?: string;
}

/**
 * Request to install an extension
 */
export interface InstallRequest {
  extensionId: string;
  metadata: ExtensionMetadata;
}

/**
 * ZIP file entry for processing
 */
export interface ZipEntry {
  name: string;
  data: ArrayBuffer;
  isDirectory: boolean;
}

/**
 * CRX file header information
 */
export interface CRXHeader {
  version: 2 | 3;
  zipOffset: number;
  publicKeyLength?: number;
  signatureLength?: number;
  headerLength?: number;
}
