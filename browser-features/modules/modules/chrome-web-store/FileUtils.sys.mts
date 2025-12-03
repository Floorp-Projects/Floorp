/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * File I/O utilities for Chrome Web Store integration
 * Provides helper functions for reading/writing files and streams in Gecko
 */

import { CWSError, CWSErrorCode } from "./types.ts";

// =============================================================================
// File Operations
// =============================================================================

/**
 * Write ArrayBuffer data to a file
 * @param file - Target file
 * @param data - Data to write
 * @throws {CWSError} If writing fails
 */
export function writeArrayBufferToFile(file: nsIFile, data: ArrayBuffer): void {
  let outputStream: nsIFileOutputStream | null = null;
  let binaryStream: nsIBinaryOutputStream | null = null;

  try {
    outputStream = Cc[
      "@mozilla.org/network/file-output-stream;1"
    ].createInstance(Ci.nsIFileOutputStream);

    // PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE
    outputStream.init(file, 0x02 | 0x08 | 0x20, 0o644, 0);

    binaryStream = Cc["@mozilla.org/binaryoutputstream;1"].createInstance(
      Ci.nsIBinaryOutputStream,
    );
    binaryStream.setOutputStream(outputStream);

    const uint8Array = new Uint8Array(data);
    binaryStream.writeByteArray(Array.from(uint8Array));
  } catch (error) {
    throw new CWSError(
      CWSErrorCode.FILE_IO_ERROR,
      `Failed to write file: ${file.path}`,
      error,
    );
  } finally {
    try {
      binaryStream?.close();
    } catch {
      // Ignore close errors
    }
    try {
      outputStream?.close();
    } catch {
      // Ignore close errors
    }
  }
}

/**
 * Read file contents to ArrayBuffer
 * @param file - File to read
 * @returns File contents as ArrayBuffer
 * @throws {CWSError} If reading fails
 */
export function readFileToArrayBuffer(file: nsIFile): ArrayBuffer {
  let inputStream: nsIFileInputStream | null = null;
  let binaryStream: nsIBinaryInputStream | null = null;

  try {
    inputStream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
      Ci.nsIFileInputStream,
    );

    // PR_RDONLY
    inputStream.init(file, 0x01, 0o444, 0);

    binaryStream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
      Ci.nsIBinaryInputStream,
    );
    binaryStream.setInputStream(inputStream);

    const size = file.fileSize;
    const bytes = binaryStream.readByteArray(size);

    return new Uint8Array(bytes).buffer;
  } catch (error) {
    throw new CWSError(
      CWSErrorCode.FILE_IO_ERROR,
      `Failed to read file: ${file.path}`,
      error,
    );
  } finally {
    try {
      binaryStream?.close();
    } catch {
      // Ignore close errors
    }
    try {
      inputStream?.close();
    } catch {
      // Ignore close errors
    }
  }
}

// =============================================================================
// Stream Operations
// =============================================================================

/**
 * Read nsIInputStream to string (UTF-8)
 * @param inputStream - Input stream to read
 * @returns Decoded string content
 */
export function readInputStream(inputStream: nsIInputStream): string {
  const buffer = readInputStreamToBuffer(inputStream);
  return new TextDecoder().decode(new Uint8Array(buffer));
}

/**
 * Read nsIInputStream to ArrayBuffer
 * @param inputStream - Input stream to read
 * @returns Stream contents as ArrayBuffer
 */
export function readInputStreamToBuffer(
  inputStream: nsIInputStream,
): ArrayBuffer {
  const binaryStream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
    Ci.nsIBinaryInputStream,
  );
  binaryStream.setInputStream(inputStream);

  const chunks: Uint8Array[] = [];
  let totalLength = 0;

  try {
    while (true) {
      const available = inputStream.available();
      if (available <= 0) {
        break;
      }

      const bytes = binaryStream.readByteArray(available);
      if (bytes.length === 0) {
        break;
      }

      const chunk = new Uint8Array(bytes);
      chunks.push(chunk);
      totalLength += chunk.length;
    }
  } catch (error) {
    // NS_BASE_STREAM_CLOSED is expected at end of stream
    const nsError = error as { result?: number };
    if (nsError.result !== 0x80470002) {
      // NS_BASE_STREAM_CLOSED
      console.error("[FileUtils] Error reading stream:", error);
    }
  } finally {
    try {
      binaryStream.close();
    } catch {
      // Ignore close errors
    }
    try {
      inputStream.close();
    } catch {
      // Ignore close errors
    }
  }

  // Concatenate all chunks
  const result = new Uint8Array(totalLength);
  let offset = 0;
  for (const chunk of chunks) {
    result.set(chunk, offset);
    offset += chunk.length;
  }

  return result.buffer;
}

/**
 * Create nsIInputStream from Uint8Array
 * @param data - Data to create stream from
 * @returns Input stream containing the data
 */
export function createInputStream(data: Uint8Array): nsIInputStream {
  const arrayStream = Cc[
    "@mozilla.org/io/arraybuffer-input-stream;1"
  ].createInstance(Ci.nsIArrayBufferInputStream);

  arrayStream.setData(data.buffer, 0, data.byteLength);
  return arrayStream;
}

/**
 * Create nsIInputStream from string (UTF-8)
 * @param content - String content
 * @returns Input stream containing the encoded string
 */
export function createInputStreamFromString(content: string): nsIInputStream {
  const bytes = new TextEncoder().encode(content);
  return createInputStream(bytes);
}

// =============================================================================
// Temporary File Management
// =============================================================================

/**
 * Create a temporary file in the system temp directory
 * @param filename - Name for the temporary file
 * @returns nsIFile pointing to the temporary file
 */
export function createTempFile(filename: string): nsIFile {
  const tempDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  const tempFile = tempDir.clone();
  tempFile.append(filename);
  return tempFile;
}

/**
 * Create a unique temporary file with a generated name
 * @param prefix - Prefix for the filename
 * @param extension - File extension (without dot)
 * @returns nsIFile pointing to the temporary file
 */
export function createUniqueTempFile(
  prefix: string,
  extension: string,
): nsIFile {
  const timestamp = Date.now();
  const random = Math.random().toString(36).substring(2, 8);
  const filename = `${prefix}-${timestamp}-${random}.${extension}`;
  return createTempFile(filename);
}

/**
 * Safely remove a file, ignoring errors
 * @param file - File to remove
 * @returns true if removed successfully, false otherwise
 */
export function safeRemoveFile(file: nsIFile): boolean {
  try {
    if (file.exists()) {
      file.remove(false);
      return true;
    }
  } catch (error) {
    console.debug(`[FileUtils] Failed to remove file ${file.path}:`, error);
  }
  return false;
}

/**
 * Safely remove multiple files
 * @param files - Array of files to remove
 * @returns Number of files successfully removed
 */
export function safeRemoveFiles(files: nsIFile[]): number {
  let removed = 0;
  for (const file of files) {
    if (safeRemoveFile(file)) {
      removed++;
    }
  }
  return removed;
}

// =============================================================================
// File Utilities
// =============================================================================

/**
 * Check if a file exists
 * @param file - File to check
 */
export function fileExists(file: nsIFile): boolean {
  try {
    return file.exists();
  } catch {
    return false;
  }
}

/**
 * Get file size safely
 * @param file - File to check
 * @returns File size in bytes, or -1 if error
 */
export function getFileSize(file: nsIFile): number {
  try {
    return file.fileSize;
  } catch {
    return -1;
  }
}

/**
 * Ensure a file doesn't exist (remove if it does)
 * @param file - File to ensure doesn't exist
 */
export function ensureFileRemoved(file: nsIFile): void {
  safeRemoveFile(file);
}
