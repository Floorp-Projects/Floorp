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
// Constants
// =============================================================================

/** Chunk size for binary writes (64KB) */
const WRITE_CHUNK_SIZE = 65536;

/** Expected error code for end of stream */
const NS_BASE_STREAM_CLOSED = 0x80470002;

// =============================================================================
// File Operations
// =============================================================================

/**
 * Write ArrayBuffer data to a file (async version using IOUtils for better performance)
 * @param file - Target file
 * @param data - Data to write
 * @throws {CWSError} If writing fails
 */
export async function writeArrayBufferToFileAsync(
  file: nsIFile,
  data: ArrayBuffer,
): Promise<void> {
  try {
    const uint8Array = new Uint8Array(data);
    // IOUtils is much faster for large files
    await IOUtils.write(file.path, uint8Array);
  } catch (error) {
    throw new CWSError(
      CWSErrorCode.FILE_IO_ERROR,
      `Failed to write file: ${file.path}`,
      error,
    );
  }
}

/**
 * Write ArrayBuffer data to a file (sync version - SLOW for large files, use async when possible)
 * @param file - Target file
 * @param data - Data to write
 * @throws {CWSError} If writing fails
 */
export function writeArrayBufferToFile(file: nsIFile, data: ArrayBuffer): void {
  try {
    const uint8Array = new Uint8Array(data);

    const outputStream = Cc[
      "@mozilla.org/network/file-output-stream;1"
    ].createInstance(Ci.nsIFileOutputStream);

    // PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE
    outputStream.init(file, 0x02 | 0x08 | 0x20, 0o644, 0);

    const binaryStream = Cc["@mozilla.org/binaryoutputstream;1"].createInstance(
      Ci.nsIBinaryOutputStream,
    );
    binaryStream.setOutputStream(outputStream);

    // Write in chunks to avoid memory issues
    for (let i = 0; i < uint8Array.length; i += WRITE_CHUNK_SIZE) {
      const chunk = uint8Array.subarray(
        i,
        Math.min(i + WRITE_CHUNK_SIZE, uint8Array.length),
      );
      binaryStream.writeByteArray(Array.from(chunk));
    }
    binaryStream.close();
    outputStream.close();
  } catch (error) {
    throw new CWSError(
      CWSErrorCode.FILE_IO_ERROR,
      `Failed to write file: ${file.path}`,
      error,
    );
  }
}

/**
 * Read file contents to ArrayBuffer (async version using IOUtils for better performance)
 * @param file - File to read
 * @returns File contents as ArrayBuffer
 * @throws {CWSError} If reading fails
 */
export async function readFileToArrayBufferAsync(
  file: nsIFile,
): Promise<ArrayBuffer> {
  try {
    const uint8Array = await IOUtils.read(file.path);
    // Copy to a new ArrayBuffer to ensure proper type
    const buffer = new ArrayBuffer(uint8Array.byteLength);
    new Uint8Array(buffer).set(uint8Array);
    return buffer;
  } catch (error) {
    throw new CWSError(
      CWSErrorCode.FILE_IO_ERROR,
      `Failed to read file: ${file.path}`,
      error,
    );
  }
}

/**
 * Read file contents to ArrayBuffer (sync version - slower for large files)
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
 * Read nsIInputStream to ArrayBuffer with known size (optimized)
 * @param inputStream - Input stream to read
 * @param size - Known size of the stream content
 * @returns Stream contents as ArrayBuffer
 */
export function readInputStreamToBufferWithSize(
  inputStream: nsIInputStream,
  size: number,
): ArrayBuffer {
  const binaryStream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
    Ci.nsIBinaryInputStream,
  );
  binaryStream.setInputStream(inputStream);

  try {
    // Read entire content at once when size is known
    const bytes = binaryStream.readByteArray(size);
    return new Uint8Array(bytes).buffer;
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
    if (nsError.result !== NS_BASE_STREAM_CLOSED) {
      // Stream reading error - ignore silently
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
  } catch {
    // Ignore file removal errors
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
