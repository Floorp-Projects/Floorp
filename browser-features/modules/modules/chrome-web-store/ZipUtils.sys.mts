/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * ZIP Utilities - Handle ZIP file operations for extension conversion
 */

import {
  readInputStream,
  readInputStreamToBuffer,
  createInputStream,
  writeArrayBufferToFile,
  safeRemoveFile,
} from "./FileUtils.sys.mts";
import type { ChromeManifest, ZipEntry } from "./types.ts";
import { CWSError, CWSErrorCode } from "./types.ts";

// =============================================================================
// Constants
// =============================================================================

/** Default compression level (DEFLATE) */
const COMPRESSION_DEFAULT = 0;

/** File open flags for ZIP writer */
const PR_RDWR = 0x04;
const PR_CREATE_FILE = 0x08;
const PR_TRUNCATE = 0x20;

// =============================================================================
// Public API - Reading
// =============================================================================

/**
 * Open a ZIP file for reading
 * @param file - File to open
 * @returns ZIP reader instance
 * @throws {CWSError} If file cannot be opened
 */
export function openZipReader(file: nsIFile): nsIZipReader {
  try {
    const zipReader = Cc["@mozilla.org/libjar/zip-reader;1"].createInstance(
      Ci.nsIZipReader,
    );
    zipReader.open(file);
    return zipReader;
  } catch (error) {
    throw new CWSError(
      CWSErrorCode.ZIP_ERROR,
      `Failed to open ZIP file: ${file.path}`,
      error,
    );
  }
}

/**
 * Open a ZIP from ArrayBuffer (creates temporary file)
 * @param buffer - ZIP data as ArrayBuffer
 * @param tempFile - Temporary file to write data to
 * @returns ZIP reader instance
 */
export function openZipReaderFromBuffer(
  buffer: ArrayBuffer,
  tempFile: nsIFile,
): nsIZipReader {
  writeArrayBufferToFile(tempFile, buffer);
  return openZipReader(tempFile);
}

/**
 * Read manifest.json from a ZIP file
 * @param zipReader - Open ZIP reader
 * @returns Parsed manifest or null if not found
 */
export function readManifestFromZip(
  zipReader: nsIZipReader,
): ChromeManifest | null {
  try {
    if (!zipReader.hasEntry("manifest.json")) {
      return null;
    }

    const manifestStream = zipReader.getInputStream("manifest.json");
    const manifestText = readInputStream(manifestStream);
    return JSON.parse(manifestText) as ChromeManifest;
  } catch {
    return null;
  }
}

/**
 * List all entries in a ZIP file
 * @param zipReader - Open ZIP reader
 * @returns Array of entry names
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
 * @param zipReader - Open ZIP reader
 * @param entryName - Entry name to check
 */
export function zipHasEntry(
  zipReader: nsIZipReader,
  entryName: string,
): boolean {
  return zipReader.hasEntry(entryName);
}

/**
 * Read all entries from a ZIP file
 * @param zipReader - Open ZIP reader
 * @returns Array of ZIP entries with their data
 */
export function readAllZipEntries(zipReader: nsIZipReader): ZipEntry[] {
  const entries: ZipEntry[] = [];
  const iterator = zipReader.findEntries("*");

  while (iterator.hasMore()) {
    const entryName = iterator.getNext();

    try {
      const entry = zipReader.getEntry(entryName);
      const isDirectory = entryName.endsWith("/");

      if (isDirectory) {
        entries.push({
          name: entryName,
          data: new ArrayBuffer(0),
          isDirectory: true,
          lastModified: entry?.lastModifiedTime,
        });
      } else {
        const inputStream = zipReader.getInputStream(entryName);
        const data = readInputStreamToBuffer(inputStream);

        entries.push({
          name: entryName,
          data,
          isDirectory: false,
          lastModified: entry?.lastModifiedTime,
        });
      }
    } catch {
      // Skip entries that fail to read
    }
  }

  return entries;
}

// =============================================================================
// Public API - Writing
// =============================================================================

/**
 * Create a new XPI (ZIP) file from CRX contents with modified manifest
 * @param sourceZipReader - ZIP reader containing the original extension files
 * @param outputFile - Target XPI file
 * @param newManifest - Modified manifest object
 */
export function createXpiFromCrx(
  sourceZipReader: nsIZipReader,
  outputFile: nsIFile,
  newManifest: ChromeManifest,
): void {
  const zipWriter = createZipWriter(outputFile);

  try {
    const entries = sourceZipReader.findEntries("*");

    while (entries.hasMore()) {
      const entryName = entries.getNext();

      if (entryName === "manifest.json") {
        writeManifestToZip(zipWriter, newManifest);
      } else {
        copyZipEntry(sourceZipReader, zipWriter, entryName);
      }
    }
  } finally {
    zipWriter.close();
  }
}

/**
 * Create a ZIP writer for a file
 * @param outputFile - File to write to
 * @returns ZIP writer instance
 */
export function createZipWriter(outputFile: nsIFile): nsIZipWriter {
  // Ensure output file doesn't exist
  safeRemoveFile(outputFile);

  const zipWriter = Cc["@mozilla.org/zipwriter;1"].createInstance(
    Ci.nsIZipWriter,
  );

  zipWriter.open(outputFile, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);
  return zipWriter;
}

/**
 * Write manifest.json to a ZIP file
 * @param zipWriter - Open ZIP writer
 * @param manifest - Manifest object to write
 */
export function writeManifestToZip(
  zipWriter: nsIZipWriter,
  manifest: ChromeManifest,
): void {
  const manifestJson = JSON.stringify(manifest, null, 2);
  const manifestBytes = new TextEncoder().encode(manifestJson);
  const manifestStream = createInputStream(manifestBytes);

  zipWriter.addEntryStream(
    "manifest.json",
    Date.now() * 1000, // PRTime is in microseconds
    COMPRESSION_DEFAULT,
    manifestStream,
    false,
  );
}

/**
 * Add a file entry to a ZIP
 * @param zipWriter - Open ZIP writer
 * @param entryName - Name of the entry
 * @param data - Data as Uint8Array
 * @param lastModified - Optional last modified time (PRTime in microseconds)
 */
export function addZipEntry(
  zipWriter: nsIZipWriter,
  entryName: string,
  data: Uint8Array,
  lastModified?: number,
): void {
  const stream = createInputStream(data);
  zipWriter.addEntryStream(
    entryName,
    lastModified ?? Date.now() * 1000,
    COMPRESSION_DEFAULT,
    stream,
    false,
  );
}

/**
 * Add a string entry to a ZIP (UTF-8 encoded)
 * @param zipWriter - Open ZIP writer
 * @param entryName - Name of the entry
 * @param content - String content
 */
export function addZipEntryFromString(
  zipWriter: nsIZipWriter,
  entryName: string,
  content: string,
): void {
  const bytes = new TextEncoder().encode(content);
  addZipEntry(zipWriter, entryName, bytes);
}

/**
 * Add a JSON entry to a ZIP
 * @param zipWriter - Open ZIP writer
 * @param entryName - Name of the entry
 * @param data - Data to serialize as JSON
 */
export function addZipEntryFromJSON(
  zipWriter: nsIZipWriter,
  entryName: string,
  data: unknown,
): void {
  const json = JSON.stringify(data, null, 2);
  addZipEntryFromString(zipWriter, entryName, json);
}

// =============================================================================
// Internal Functions
// =============================================================================

/**
 * Copy a single entry from source ZIP to destination ZIP
 */
function copyZipEntry(
  sourceZipReader: nsIZipReader,
  destZipWriter: nsIZipWriter,
  entryName: string,
): void {
  try {
    // Skip directories - they're created implicitly
    if (entryName.endsWith("/")) {
      return;
    }

    const entry = sourceZipReader.getEntry(entryName);
    const inputStream = sourceZipReader.getInputStream(entryName);

    destZipWriter.addEntryStream(
      entryName,
      entry?.lastModifiedTime ?? Date.now() * 1000,
      COMPRESSION_DEFAULT,
      inputStream,
      false,
    );
  } catch {
    // Skip entries that fail to copy
  }
}

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * Get the file extension from an entry name
 * @param entryName - ZIP entry name
 */
export function getEntryExtension(entryName: string): string {
  const lastDot = entryName.lastIndexOf(".");
  if (lastDot === -1) return "";
  return entryName.substring(lastDot + 1).toLowerCase();
}

/**
 * Check if an entry is a JavaScript file
 * @param entryName - ZIP entry name
 */
export function isJavaScriptEntry(entryName: string): boolean {
  const ext = getEntryExtension(entryName);
  return ["js", "mjs", "jsx"].includes(ext);
}

/**
 * Check if an entry is a JSON file
 * @param entryName - ZIP entry name
 */
export function isJSONEntry(entryName: string): boolean {
  return getEntryExtension(entryName) === "json";
}
