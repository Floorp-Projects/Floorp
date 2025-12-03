/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * CRX Parser - Parses Chrome CRX file format
 *
 * Supports both CRX2 and CRX3 formats
 */

import {
  CRX_MAGIC,
  CRX2_VERSION,
  CRX3_VERSION,
  ZIP_SIGNATURE,
} from "./Constants.sys.mts";
import type { CRXHeader } from "./types.ts";

// =============================================================================
// Constants
// =============================================================================

const LOG_PREFIX = "[CRXParser]";

/** Minimum size for a valid CRX file (header only) */
const MIN_CRX_SIZE = 16;

/** Minimum size for ZIP data */
const MIN_ZIP_SIZE = 4;

// =============================================================================
// Public API
// =============================================================================

/**
 * Parse CRX header and return metadata
 * @param data - Raw CRX file data
 * @returns Parsed header or null if invalid
 */
export function parseCRXHeader(data: ArrayBuffer): CRXHeader | null {
  if (data.byteLength < MIN_CRX_SIZE) {
    console.error(`${LOG_PREFIX} File too small: ${data.byteLength} bytes`);
    return null;
  }

  const view = new DataView(data);
  const magic = readMagicNumber(view);

  if (magic !== CRX_MAGIC) {
    console.error(`${LOG_PREFIX} Invalid magic number: "${magic}"`);
    return null;
  }

  const version = view.getUint32(4, true);

  switch (version) {
    case CRX2_VERSION:
      return parseCRX2Header(view);
    case CRX3_VERSION:
      return parseCRX3Header(view);
    default:
      console.error(`${LOG_PREFIX} Unsupported version: ${version}`);
      return null;
  }
}

/**
 * Extract the ZIP portion from a CRX file
 * @param crxData - Raw CRX file data
 * @returns ZIP data as ArrayBuffer or null if extraction fails
 */
export function extractZipFromCRX(crxData: ArrayBuffer): ArrayBuffer | null {
  const header = parseCRXHeader(crxData);
  if (!header) {
    return null;
  }

  // Extract ZIP data starting from the calculated offset
  const zipData = crxData.slice(header.zipOffset);

  if (zipData.byteLength < MIN_ZIP_SIZE) {
    console.error(
      `${LOG_PREFIX} ZIP data too small: ${zipData.byteLength} bytes`,
    );
    return null;
  }

  // Verify ZIP signature "PK\x03\x04"
  const zipView = new DataView(zipData);
  const zipSig = zipView.getUint32(0, true);

  if (zipSig !== ZIP_SIGNATURE) {
    console.error(
      `${LOG_PREFIX} Invalid ZIP signature at offset ${header.zipOffset}: 0x${zipSig.toString(16)}`,
    );
    return null;
  }

  console.log(`${LOG_PREFIX} Extracted ZIP: ${zipData.byteLength} bytes`);
  return zipData;
}

/**
 * Verify if data is a valid CRX file
 * @param data - Data to verify
 * @returns true if data is a valid CRX file
 */
export function isValidCRX(data: ArrayBuffer): boolean {
  return parseCRXHeader(data) !== null;
}

// =============================================================================
// Internal Functions
// =============================================================================

/**
 * Read the magic number from CRX header
 */
function readMagicNumber(view: DataView): string {
  return String.fromCharCode(
    view.getUint8(0),
    view.getUint8(1),
    view.getUint8(2),
    view.getUint8(3),
  );
}

/**
 * Parse CRX2 format header
 * Format: [magic(4)][version(4)][pubkeyLen(4)][sigLen(4)][pubkey][sig][zip]
 */
function parseCRX2Header(view: DataView): CRXHeader {
  const publicKeyLength = view.getUint32(8, true);
  const signatureLength = view.getUint32(12, true);
  const zipOffset = 16 + publicKeyLength + signatureLength;

  console.log(
    `${LOG_PREFIX} CRX2: pubkey=${publicKeyLength}, sig=${signatureLength}, zipOffset=${zipOffset}`,
  );

  return {
    version: 2,
    zipOffset,
    publicKeyLength,
    signatureLength,
  };
}

/**
 * Parse CRX3 format header
 * Format: [magic(4)][version(4)][headerLen(4)][header protobuf][zip]
 */
function parseCRX3Header(view: DataView): CRXHeader {
  const headerLength = view.getUint32(8, true);
  const zipOffset = 12 + headerLength;

  console.log(
    `${LOG_PREFIX} CRX3: headerLen=${headerLength}, zipOffset=${zipOffset}`,
  );

  return {
    version: 3,
    zipOffset,
    headerLength,
  };
}
