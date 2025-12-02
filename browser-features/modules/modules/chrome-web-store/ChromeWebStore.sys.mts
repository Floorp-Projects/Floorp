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
 * - XPI file generation
 */

// Type exports
export type {
  CRXHeader,
  ChromeManifest,
  ExtensionMetadata,
  InstallResult,
  WebAccessibleResourceEntry,
} from "./types.ts";

// Constants
export {
  CRX_MAGIC,
  CRX_DOWNLOAD_URLS,
  EXTENSION_ID_SUFFIX,
  GECKO_MIN_VERSION,
  UNSUPPORTED_PERMISSIONS,
  CWS_URL_PATTERNS,
} from "./Constants.sys.mts";

// CRX Parsing
export {
  parseCRXHeader,
  extractZipFromCRX,
  isValidCRX,
} from "./CRXParser.sys.mts";

// Manifest Transformation
export {
  transformManifest,
  validateManifest,
} from "./ManifestTransformer.sys.mts";

// File Utilities
export {
  writeArrayBufferToFile,
  readFileToArrayBuffer,
  readInputStream,
  readInputStreamToBuffer,
  createInputStream,
} from "./FileUtils.sys.mts";

// ZIP Utilities
export {
  readManifestFromZip,
  createXpiFromCrx,
  openZipReader,
  openZipReaderFromBuffer,
  listZipEntries,
  zipHasEntry,
} from "./ZipUtils.sys.mts";
