/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * ZIP Utilities - Handle ZIP file operations for extension conversion
 */

import { readInputStream, createInputStream } from "./FileUtils.sys.mts";
import type { ChromeManifest } from "./types.ts";

/**
 * Read manifest.json from a ZIP file
 */
export function readManifestFromZip(
  zipReader: nsIZipReader,
): ChromeManifest | null {
  try {
    const manifestStream = zipReader.getInputStream("manifest.json");
    const manifestText = readInputStream(manifestStream);
    return JSON.parse(manifestText) as ChromeManifest;
  } catch (e) {
    console.error("[ZipUtils] Failed to read manifest.json:", e);
    return null;
  }
}

/**
 * Create a new XPI (ZIP) file from CRX contents with modified manifest
 * @param sourceZipReader ZipReader containing the original extension files
 * @param outputFile Target XPI file
 * @param newManifest Modified manifest object
 */
export function createXpiFromCrx(
  sourceZipReader: nsIZipReader,
  outputFile: nsIFile,
  newManifest: ChromeManifest,
): void {
  const zipWriter = Cc["@mozilla.org/zipwriter;1"].createInstance(
    Ci.nsIZipWriter,
  );

  // Open output file for writing
  const PR_RDWR = 0x04;
  const PR_CREATE_FILE = 0x08;
  const PR_TRUNCATE = 0x20;

  zipWriter.open(outputFile, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);

  try {
    // Copy all entries from source, replacing manifest.json
    const entries = sourceZipReader.findEntries("*");

    while (entries.hasMore()) {
      const entryName = entries.getNext();

      if (entryName === "manifest.json") {
        // Write transformed manifest
        writeManifestToZip(zipWriter, newManifest);
      } else {
        // Copy entry as-is
        copyZipEntry(sourceZipReader, zipWriter, entryName);
      }
    }
  } finally {
    zipWriter.close();
  }
}

/**
 * Write manifest.json to a ZIP file
 */
function writeManifestToZip(
  zipWriter: nsIZipWriter,
  manifest: ChromeManifest,
): void {
  const manifestJson = JSON.stringify(manifest, null, 2);
  const manifestBytes = new TextEncoder().encode(manifestJson);
  const manifestStream = createInputStream(manifestBytes);

  // Use DEFLATE compression (8) or STORE (0)
  const COMPRESSION_DEFAULT = 8;

  zipWriter.addEntryStream(
    "manifest.json",
    Date.now() * 1000, // PRTime is in microseconds
    COMPRESSION_DEFAULT,
    manifestStream,
    false,
  );
}

/**
 * Copy a single entry from source ZIP to destination ZIP
 */
function copyZipEntry(
  sourceZipReader: nsIZipReader,
  destZipWriter: nsIZipWriter,
  entryName: string,
): void {
  try {
    const entry = sourceZipReader.getEntry(entryName);

    // Skip directories - they're created implicitly
    if (entryName.endsWith("/")) {
      return;
    }

    const inputStream = sourceZipReader.getInputStream(entryName);
    const COMPRESSION_DEFAULT = 8;

    destZipWriter.addEntryStream(
      entryName,
      entry.lastModifiedTime,
      COMPRESSION_DEFAULT,
      inputStream,
      false,
    );
  } catch (e) {
    console.warn(`[ZipUtils] Failed to copy entry ${entryName}:`, e);
  }
}

/**
 * Open a ZIP file for reading
 */
export function openZipReader(file: nsIFile): nsIZipReader {
  const zipReader = Cc["@mozilla.org/libjar/zip-reader;1"].createInstance(
    Ci.nsIZipReader,
  );
  zipReader.open(file);
  return zipReader;
}

/**
 * Open a ZIP from ArrayBuffer (creates temporary file)
 */
export async function openZipReaderFromBuffer(
  buffer: ArrayBuffer,
  tempFile: nsIFile,
): Promise<nsIZipReader> {
  // Write buffer to temp file
  const { writeArrayBufferToFile } = await import("./FileUtils.sys.mts");
  writeArrayBufferToFile(tempFile, buffer);

  return openZipReader(tempFile);
}

/**
 * List all entries in a ZIP file
 */
export function listZipEntries(zipReader: nsIZipReader): string[] {
  const entries: string[] = [];
  const iterator = zipReader.findEntries("*");

  while (iterator.hasMore()) {
    entries.push(iterator.getNext());
  }

  return entries;
}

/**
 * Check if a ZIP contains a specific entry
 */
export function zipHasEntry(
  zipReader: nsIZipReader,
  entryName: string,
): boolean {
  return zipReader.hasEntry(entryName);
}
