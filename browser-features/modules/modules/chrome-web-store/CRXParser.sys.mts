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

/**
 * Parse CRX header and return metadata
 */
export function parseCRXHeader(data: ArrayBuffer): CRXHeader | null {
  if (data.byteLength < 16) {
    console.error("[CRXParser] File too small to be a valid CRX");
    return null;
  }

  const view = new DataView(data);

  // Verify magic number "Cr24"
  const magic = String.fromCharCode(
    view.getUint8(0),
    view.getUint8(1),
    view.getUint8(2),
    view.getUint8(3),
  );

  if (magic !== CRX_MAGIC) {
    console.error("[CRXParser] Invalid CRX magic:", magic);
    return null;
  }

  const version = view.getUint32(4, true);

  if (version === CRX2_VERSION) {
    // CRX2 format:
    // [4 bytes magic][4 bytes version][4 bytes pubkey len][4 bytes sig len][pubkey][sig][zip]
    const publicKeyLength = view.getUint32(8, true);
    const signatureLength = view.getUint32(12, true);
    const zipOffset = 16 + publicKeyLength + signatureLength;

    console.log(
      `[CRXParser] CRX2: pubkey=${publicKeyLength}, sig=${signatureLength}, zipOffset=${zipOffset}`,
    );

    return {
      version: 2,
      zipOffset,
      publicKeyLength,
      signatureLength,
    };
  }

  if (version === CRX3_VERSION) {
    // CRX3 format:
    // [4 bytes magic][4 bytes version][4 bytes header len][header protobuf][zip]
    const headerLength = view.getUint32(8, true);
    const zipOffset = 12 + headerLength;

    console.log(
      `[CRXParser] CRX3: headerLen=${headerLength}, zipOffset=${zipOffset}`,
    );

    return {
      version: 3,
      zipOffset,
      headerLength,
    };
  }

  console.error("[CRXParser] Unsupported CRX version:", version);
  return null;
}

/**
 * Extract the ZIP portion from a CRX file
 */
export function extractZipFromCRX(crxData: ArrayBuffer): ArrayBuffer | null {
  const header = parseCRXHeader(crxData);
  if (!header) {
    return null;
  }

  // Extract ZIP data
  const zipData = crxData.slice(header.zipOffset);

  // Verify ZIP signature "PK\x03\x04"
  if (zipData.byteLength < 4) {
    console.error("[CRXParser] ZIP data too small");
    return null;
  }

  const zipView = new DataView(zipData);
  const zipSig = zipView.getUint32(0, true);

  if (zipSig !== ZIP_SIGNATURE) {
    console.error(
      "[CRXParser] Invalid ZIP signature at offset",
      header.zipOffset,
      "got",
      zipSig.toString(16),
    );
    return null;
  }

  console.log(`[CRXParser] Extracted ZIP: ${zipData.byteLength} bytes`);
  return zipData;
}

/**
 * Verify if data is a valid CRX file
 */
export function isValidCRX(data: ArrayBuffer): boolean {
  return parseCRXHeader(data) !== null;
}
