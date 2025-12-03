/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Chrome Web Store Extension Support Module
 *
 * This module provides functionality to install Chrome extensions from
 * the Chrome Web Store in Floorp. It handles:
 * - CRX file parsing (CRX2 and CRX3 formats)
 * - Manifest transformation (Chrome → Firefox compatible)
 * - Code validation for compatibility
 * - DNR rules sanitization
 * - XPI file generation
 */

// =============================================================================
// Type Exports
// =============================================================================

export type {
  // Core types
  CRXHeader,
  ChromeManifest,
  ExtensionMetadata,
  MutableExtensionMetadata,
  InstallResult,
  InstallRequest,
  ConversionResult,
  ConversionOptions,
  TransformOptions,
  // Manifest types
  ChromeManifestBackground,
  ChromeManifestContentScript,
  WebAccessibleResourceEntry,
  BrowserSpecificSettings,
  GeckoSpecificSettings,
  ActionDefinition,
  DeclarativeNetRequestConfig,
  // DNR types
  DNRRule,
  DNRRuleAction,
  DNRRuleCondition,
  // Validation types
  ValidationResult,
  ValidationError,
  ValidationWarning,
  // Internal types
  ZipEntry,
  // AddonManager types
  AddonInstallListener,
  AddonInstall,
  GeckoAddonManager,
  CRXConverterModule,
  nsIFile,
} from "./types.ts";

// Error types
export { CWSError, CWSErrorCode } from "./types.ts";

// =============================================================================
// Constants
// =============================================================================

export {
  // CRX format constants
  CRX_MAGIC,
  CRX2_VERSION,
  CRX3_VERSION,
  CRX_VERSIONS,
  ZIP_SIGNATURE,
  // Gecko compatibility
  GECKO_MIN_VERSION,
  EXTENSION_ID_SUFFIX,
  // Download URLs
  CRX_DOWNLOAD_URLS,
  // Permissions
  UNSUPPORTED_PERMISSIONS,
  UNSUPPORTED_OPTIONAL_PERMISSIONS,
  // URL patterns
  CWS_URL_PATTERNS,
  CWS_BASE_URLS,
  // HTTP
  CHROME_USER_AGENT,
  CRX_ACCEPT_HEADER,
  // Validation
  UNSUPPORTED_CODE_PATTERNS,
  // Limits
  MAX_CRX_SIZE,
  MAX_FILE_SIZE,
  DOWNLOAD_TIMEOUT_MS,
  INSTALL_TIMEOUT_MS,
  // Error messages
  INSTALL_ERROR_MESSAGES,
  // Utility functions
  isChromeWebStoreUrl,
  extractExtensionId,
  generateFirefoxId,
  isPermissionSupported,
  isOptionalPermissionAllowed,
} from "./Constants.sys.mts";

// =============================================================================
// CRX Parsing
// =============================================================================

export {
  parseCRXHeader,
  extractZipFromCRX,
  isValidCRX,
} from "./CRXParser.sys.mts";

// =============================================================================
// CRX Conversion
// =============================================================================

export { CRXConverter, CRXConverterClass } from "./CRXConverter.sys.mts";

// =============================================================================
// Manifest Transformation
// =============================================================================

export {
  transformManifest,
  transformManifestFull,
  validateManifest,
  checkCompatibility,
  getFirefoxExtensionId,
  extractHostPermissions,
  extractApiPermissions,
} from "./ManifestTransformer.sys.mts";

// =============================================================================
// Code Validation
// =============================================================================

export {
  validateSourceCode,
  validateSourceCodeFull,
  validateMultipleFiles,
  isJavaScriptFile,
  mightContainUnsupportedPatterns,
  getValidationSummary,
} from "./CodeValidator.sys.mts";

// =============================================================================
// DNR Transformation
// =============================================================================

export {
  sanitizeDNRRules,
  validateDNRRule,
  countInvalidDomains,
} from "./DNRTransformer.sys.mts";

// =============================================================================
// File Utilities
// =============================================================================

export {
  writeArrayBufferToFile,
  readFileToArrayBuffer,
  readInputStream,
  readInputStreamToBuffer,
  createInputStream,
  createInputStreamFromString,
  createTempFile,
  createUniqueTempFile,
  safeRemoveFile,
  safeRemoveFiles,
  fileExists,
  getFileSize,
  ensureFileRemoved,
} from "./FileUtils.sys.mts";

// =============================================================================
// ZIP Utilities
// =============================================================================

export {
  // Reading
  openZipReader,
  openZipReaderFromBuffer,
  readManifestFromZip,
  listZipEntries,
  zipHasEntry,
  readAllZipEntries,
  // Writing
  createXpiFromCrx,
  createZipWriter,
  writeManifestToZip,
  addZipEntry,
  addZipEntryFromString,
  addZipEntryFromJSON,
  // Utilities
  getEntryExtension,
  isJavaScriptEntry,
  isJSONEntry,
} from "./ZipUtils.sys.mts";
