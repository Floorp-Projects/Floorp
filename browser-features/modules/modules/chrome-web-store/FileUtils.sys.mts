/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * File I/O utilities for Chrome Web Store integration
 * Provides helper functions for reading/writing files and streams in Gecko
 */

/**
 * Write ArrayBuffer data to a file
 */
export function writeArrayBufferToFile(file: nsIFile, data: ArrayBuffer): void {
  const outputStream = Cc[
    "@mozilla.org/network/file-output-stream;1"
  ].createInstance(Ci.nsIFileOutputStream);

  // PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE
  outputStream.init(file, 0x02 | 0x08 | 0x20, 0o644, 0);

  const binaryStream = Cc["@mozilla.org/binaryoutputstream;1"].createInstance(
    Ci.nsIBinaryOutputStream,
  );
  binaryStream.setOutputStream(outputStream);

  const uint8Array = new Uint8Array(data);
  binaryStream.writeByteArray(Array.from(uint8Array));
  binaryStream.close();
  outputStream.close();
}

/**
 * Read file contents to ArrayBuffer
 */
export function readFileToArrayBuffer(file: nsIFile): ArrayBuffer {
  const inputStream = Cc[
    "@mozilla.org/network/file-input-stream;1"
  ].createInstance(Ci.nsIFileInputStream);

  // PR_RDONLY
  inputStream.init(file, 0x01, 0o444, 0);

  const binaryStream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
    Ci.nsIBinaryInputStream,
  );
  binaryStream.setInputStream(inputStream);

  const size = file.fileSize;
  const bytes = binaryStream.readByteArray(size);
  binaryStream.close();
  inputStream.close();

  return new Uint8Array(bytes).buffer;
}

/**
 * Read nsIInputStream to string (UTF-8)
 */
export function readInputStream(inputStream: nsIInputStream): string {
  const binaryStream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
    Ci.nsIBinaryInputStream,
  );
  binaryStream.setInputStream(inputStream);

  const available = inputStream.available();
  const bytes = binaryStream.readByteArray(available);
  binaryStream.close();

  return new TextDecoder().decode(new Uint8Array(bytes));
}

/**
 * Read nsIInputStream to ArrayBuffer
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
  } catch (e) {
    console.error("[FileUtils] Error reading stream:", e);
  } finally {
    binaryStream.close();
    inputStream.close();
  }

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
 */
export function createInputStream(data: Uint8Array): nsIInputStream {
  const arrayStream = Cc[
    "@mozilla.org/io/arraybuffer-input-stream;1"
  ].createInstance(Ci.nsIArrayBufferInputStream);

  arrayStream.setData(data.buffer, 0, data.byteLength);
  return arrayStream;
}

/**
 * Create a temporary file in the system temp directory
 */
export function createTempFile(filename: string): nsIFile {
  const tempDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  const tempFile = tempDir.clone();
  tempFile.append(filename);
  return tempFile;
}

/**
 * Safely remove a file, ignoring errors
 */
export function safeRemoveFile(file: nsIFile): void {
  try {
    if (file.exists()) {
      file.remove(false);
    }
  } catch {
    // Ignore cleanup errors
  }
}
