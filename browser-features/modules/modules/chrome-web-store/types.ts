/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Type definitions for Chrome Web Store integration
 */

// =============================================================================
// Error Types
// =============================================================================

/**
 * Error codes for Chrome Web Store operations
 */
export const CWSErrorCode = {
  INVALID_CRX: "INVALID_CRX",
  INVALID_MANIFEST: "INVALID_MANIFEST",
  UNSUPPORTED_CODE: "UNSUPPORTED_CODE",
  DOWNLOAD_FAILED: "DOWNLOAD_FAILED",
  CONVERSION_FAILED: "CONVERSION_FAILED",
  INSTALLATION_FAILED: "INSTALLATION_FAILED",
  ALREADY_INSTALLED: "ALREADY_INSTALLED",
  ZIP_ERROR: "ZIP_ERROR",
  FILE_IO_ERROR: "FILE_IO_ERROR",
  NETWORK_ERROR: "NETWORK_ERROR",
  VALIDATION_ERROR: "VALIDATION_ERROR",
} as const;

export type CWSErrorCodeType = (typeof CWSErrorCode)[keyof typeof CWSErrorCode];

/**
 * Custom error class for Chrome Web Store operations
 */
export class CWSError extends Error {
  constructor(
    public readonly code: CWSErrorCodeType,
    message: string,
    public readonly cause?: unknown,
  ) {
    super(message);
    this.name = "CWSError";
  }

  /**
   * Get localized error message for user display
   */
  getLocalizedMessage(): string {
    const messages: Record<CWSErrorCodeType, string> = {
      INVALID_CRX: "無効なCRXファイルです",
      INVALID_MANIFEST: "拡張機能のマニフェストが無効です",
      UNSUPPORTED_CODE: "この拡張機能は非対応のコードを含んでいます",
      DOWNLOAD_FAILED: "拡張機能のダウンロードに失敗しました",
      CONVERSION_FAILED: "拡張機能の変換に失敗しました",
      INSTALLATION_FAILED: "拡張機能のインストールに失敗しました",
      ALREADY_INSTALLED: "この拡張機能は既にインストールされています",
      ZIP_ERROR: "ZIPファイルの処理中にエラーが発生しました",
      FILE_IO_ERROR: "ファイル操作中にエラーが発生しました",
      NETWORK_ERROR: "ネットワークエラーが発生しました",
      VALIDATION_ERROR: "拡張機能の検証に失敗しました",
    };
    return messages[this.code] || this.message;
  }
}

// =============================================================================
// Extension Metadata Types
// =============================================================================

/**
 * Extension metadata extracted from Chrome Web Store page
 * Used for passing data between actors (immutable)
 */
export interface ExtensionMetadata {
  readonly id: string;
  readonly name: string;
  readonly version?: string;
  readonly description?: string;
  readonly iconUrl?: string;
  readonly author?: string;
}

/**
 * Mutable version of ExtensionMetadata for building during extraction
 */
export interface MutableExtensionMetadata {
  id: string;
  name: string;
  version?: string;
  description?: string;
  iconUrl?: string;
  author?: string;
}

// =============================================================================
// Chrome Manifest Types
// =============================================================================

/**
 * Chrome extension manifest structure (V2/V3 compatible)
 */
export interface ChromeManifest {
  manifest_version: 2 | 3;
  name: string;
  version: string;
  description?: string;
  permissions?: string[];
  optional_permissions?: string[];
  host_permissions?: string[];
  background?: ChromeManifestBackground;
  content_scripts?: ChromeManifestContentScript[];
  action?: ActionDefinition;
  browser_action?: ActionDefinition;
  page_action?: ActionDefinition;
  icons?: Record<string, string>;
  web_accessible_resources?: WebAccessibleResources;
  browser_specific_settings?: BrowserSpecificSettings;
  declarative_net_request?: DeclarativeNetRequestConfig;
  content_security_policy?: ContentSecurityPolicy;
  update_url?: string;
  [key: string]: unknown;
}

export interface ChromeManifestBackground {
  service_worker?: string;
  scripts?: string[];
  persistent?: boolean;
  type?: "module" | "classic";
}

export interface ChromeManifestContentScript {
  matches: string[];
  js?: string[];
  css?: string[];
  run_at?: "document_start" | "document_end" | "document_idle";
  all_frames?: boolean;
  match_about_blank?: boolean;
}

export interface ActionDefinition {
  default_icon?: string | Record<string, string>;
  default_title?: string;
  default_popup?: string;
  [key: string]: unknown;
}

export interface DeclarativeNetRequestConfig {
  rule_resources?: DNRRuleResource[];
}

export interface DNRRuleResource {
  id: string;
  enabled: boolean;
  path: string;
}

export type ContentSecurityPolicy =
  | string
  | {
      extension_pages?: string;
      sandbox?: string;
    };

/**
 * MV2 format: string[]
 * MV3 format: Array<{ resources: string[]; matches: string[] }>
 */
export type WebAccessibleResources = string[] | WebAccessibleResourceEntry[];

export interface WebAccessibleResourceEntry {
  resources: string[];
  matches: string[];
  use_dynamic_url?: boolean;
}

export interface BrowserSpecificSettings {
  gecko?: GeckoSpecificSettings;
}

export interface GeckoSpecificSettings {
  id?: string;
  strict_min_version?: string;
  strict_max_version?: string;
}

// =============================================================================
// DNR (Declarative Net Request) Types
// =============================================================================

/**
 * DNR rule structure
 */
export interface DNRRule {
  id: number;
  priority?: number;
  action: DNRRuleAction;
  condition: DNRRuleCondition;
}

export interface DNRRuleAction {
  type: "block" | "redirect" | "allow" | "upgradeScheme" | "modifyHeaders";
  redirect?: {
    url?: string;
    extensionPath?: string;
    regexSubstitution?: string;
  };
  requestHeaders?: HeaderOperation[];
  responseHeaders?: HeaderOperation[];
}

export interface HeaderOperation {
  header: string;
  operation: "append" | "set" | "remove";
  value?: string;
}

export interface DNRRuleCondition {
  urlFilter?: string;
  regexFilter?: string;
  isUrlFilterCaseSensitive?: boolean;
  domains?: string[];
  excludedDomains?: string[];
  initiatorDomains?: string[];
  excludedInitiatorDomains?: string[];
  requestDomains?: string[];
  excludedRequestDomains?: string[];
  requestMethods?: RequestMethod[];
  excludedRequestMethods?: RequestMethod[];
  resourceTypes?: ResourceType[];
  excludedResourceTypes?: ResourceType[];
  tabIds?: number[];
  excludedTabIds?: number[];
  [key: string]: unknown;
}

export type RequestMethod =
  | "connect"
  | "delete"
  | "get"
  | "head"
  | "options"
  | "patch"
  | "post"
  | "put"
  | "other";

export type ResourceType =
  | "main_frame"
  | "sub_frame"
  | "stylesheet"
  | "script"
  | "image"
  | "font"
  | "object"
  | "xmlhttprequest"
  | "ping"
  | "csp_report"
  | "media"
  | "websocket"
  | "webtransport"
  | "webbundle"
  | "other";

// =============================================================================
// Operation Result Types
// =============================================================================

/**
 * Result of CRX to XPI conversion
 */
export interface ConversionResult {
  readonly success: boolean;
  readonly xpiData?: ArrayBuffer;
  readonly originalManifest?: ChromeManifest;
  readonly transformedManifest?: ChromeManifest;
  readonly error?: CWSError;
  readonly warnings?: string[];
}

/**
 * Result of extension installation
 */
export interface InstallResult {
  readonly success: boolean;
  readonly error?: string;
  readonly addonId?: string;
}

/**
 * Request to install an extension
 */
export interface InstallRequest {
  readonly extensionId: string;
  readonly metadata: ExtensionMetadata;
}

/**
 * Code validation result
 */
export interface ValidationResult {
  readonly valid: boolean;
  readonly errors: ValidationError[];
  readonly warnings: ValidationWarning[];
}

export interface ValidationError {
  readonly file: string;
  readonly line?: number;
  readonly column?: number;
  readonly message: string;
  readonly code: string;
}

export interface ValidationWarning {
  readonly file: string;
  readonly message: string;
}

// =============================================================================
// Internal Types
// =============================================================================

/**
 * ZIP file entry for processing
 */
export interface ZipEntry {
  readonly name: string;
  readonly data: ArrayBuffer;
  readonly isDirectory: boolean;
  readonly lastModified?: number;
}

/**
 * CRX file header information
 */
export interface CRXHeader {
  readonly version: 2 | 3;
  readonly zipOffset: number;
  readonly publicKeyLength?: number;
  readonly signatureLength?: number;
  readonly headerLength?: number;
}

/**
 * Options for CRX conversion
 */
export interface ConversionOptions {
  /** Whether to validate JavaScript source code */
  readonly validateCode?: boolean;
  /** Whether to sanitize DNR rules */
  readonly sanitizeDNR?: boolean;
  /** Minimum Gecko version to target */
  readonly minGeckoVersion?: string;
}

/**
 * Options for manifest transformation
 */
export interface TransformOptions {
  /** Extension ID to use for Gecko settings */
  readonly extensionId: string;
  /** Minimum Gecko version */
  readonly minGeckoVersion?: string;
  /** Whether to keep unsupported permissions (for logging) */
  readonly keepUnsupportedPermissions?: boolean;
}

// =============================================================================
// AddonManager Types
// =============================================================================

/**
 * AddonManager install listener interface
 */
export interface AddonInstallListener {
  onInstallEnded?: (
    install: AddonInstall,
    addon: { id: string; name: string },
  ) => void;
  onInstallFailed?: (install: AddonInstall) => void;
  onInstallCancelled?: (install: AddonInstall) => void;
  onDownloadStarted?: (install: AddonInstall) => void;
  onDownloadProgress?: (install: AddonInstall) => void;
  onDownloadEnded?: (install: AddonInstall) => void;
  onDownloadFailed?: (install: AddonInstall) => void;
  onInstallStarted?: (install: AddonInstall) => void;
}

/**
 * AddonManager install object
 */
export interface AddonInstall {
  install(): Promise<void>;
  addListener(listener: AddonInstallListener): void;
  removeListener(listener: AddonInstallListener): void;
  error?: number;
  addon?: { id: string; name: string };
}

/**
 * Gecko AddonManager interface (subset)
 */
export interface GeckoAddonManager {
  getAddonByID(id: string): Promise<{ name: string; id: string } | null>;
  getInstallForFile(
    file: nsIFile,
    mimeType: string,
    telemetryInfo?: object,
  ): Promise<AddonInstall | null>;
}

/**
 * CRX Converter module interface
 */
export interface CRXConverterModule {
  convert(
    crxData: ArrayBuffer,
    extensionId: string,
    metadata: ExtensionMetadata,
  ): ArrayBuffer | null;
}

/**
 * nsIFile interface (minimal subset for TypeScript)
 */
export interface nsIFile {
  readonly path: string;
  clone(): nsIFile;
  append(name: string): void;
  exists(): boolean;
  remove(recursive: boolean): void;
}
