// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  createTempFile,
  createUniqueTempFile,
  safeRemoveFile,
  safeRemoveFiles,
  fileExists,
  getFileSize,
  ensureFileRemoved,
  writeArrayBufferToFile,
  readFileToArrayBuffer,
  writeArrayBufferToFileAsync,
  readFileToArrayBufferAsync,
  createInputStream,
  createInputStreamFromString,
  readInputStream,
  readInputStreamToBuffer,
} from "./FileUtils.sys.mts";

type TestCase = { name: string; fn: () => void | Promise<void> };
function assert(condition: unknown, message: string): asserts condition {
  if (!condition) throw new Error(message);
}
function assertEquals<T>(actual: T, expected: T, message: string): void {
  if (actual !== expected)
    throw new Error(
      `${message} (expected: ${String(expected)}, actual: ${String(actual)})`,
    );
}

const tests: TestCase[] = [
  // --- createTempFile ---
  {
    name: "createTempFile returns an nsIFile with correct leaf name",
    fn() {
      const file = createTempFile("floorp-test-temp.dat");
      assert(file !== null, "should return a file object");
      assertEquals(file.leafName, "floorp-test-temp.dat", "leaf name should match");
    },
  },
  {
    name: "createTempFile file is in the temp directory",
    fn() {
      const file = createTempFile("floorp-test-loc.dat");
      const tmpDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
      assert(
        file.path.startsWith(tmpDir.path),
        "file should be under temp directory",
      );
    },
  },

  // --- createUniqueTempFile ---
  {
    name: "createUniqueTempFile generates unique names",
    fn() {
      const a = createUniqueTempFile("test", "xpi");
      const b = createUniqueTempFile("test", "xpi");
      assert(a.leafName !== b.leafName, "two calls should produce different names");
      assert(a.leafName.startsWith("test-"), "should start with prefix");
      assert(a.leafName.endsWith(".xpi"), "should end with extension");
    },
  },

  // --- writeArrayBufferToFile & readFileToArrayBuffer (sync) ---
  {
    name: "sync write then read round-trips data correctly",
    fn() {
      const file = createUniqueTempFile("rw-sync", "bin");
      try {
        const original = new Uint8Array([10, 20, 30, 40, 50]);
        writeArrayBufferToFile(file, original.buffer);

        assert(file.exists(), "file should exist after write");

        const readBack = readFileToArrayBuffer(file);
        const readBytes = new Uint8Array(readBack);
        assertEquals(readBytes.length, 5, "should read back 5 bytes");
        assertEquals(readBytes[0], 10, "first byte should match");
        assertEquals(readBytes[4], 50, "last byte should match");
      } finally {
        safeRemoveFile(file);
      }
    },
  },

  // --- writeArrayBufferToFileAsync & readFileToArrayBufferAsync ---
  {
    name: "async write then read round-trips data correctly",
    async fn() {
      const file = createUniqueTempFile("rw-async", "bin");
      try {
        const original = new Uint8Array([0xff, 0x00, 0xab, 0xcd]);
        await writeArrayBufferToFileAsync(file, original.buffer);

        assert(file.exists(), "file should exist after async write");

        const readBack = await readFileToArrayBufferAsync(file);
        const readBytes = new Uint8Array(readBack);
        assertEquals(readBytes.length, 4, "should read back 4 bytes");
        assertEquals(readBytes[0], 0xff, "first byte should match");
        assertEquals(readBytes[3], 0xcd, "last byte should match");
      } finally {
        safeRemoveFile(file);
      }
    },
  },

  // --- fileExists ---
  {
    name: "fileExists returns false for non-existent file",
    fn() {
      const file = createTempFile("floorp-nonexistent-xyz.dat");
      assertEquals(fileExists(file), false, "non-existent file should return false");
    },
  },
  {
    name: "fileExists returns true for existing file",
    fn() {
      const file = createUniqueTempFile("exists-check", "tmp");
      try {
        writeArrayBufferToFile(file, new ArrayBuffer(1));
        assertEquals(fileExists(file), true, "written file should exist");
      } finally {
        safeRemoveFile(file);
      }
    },
  },

  // --- getFileSize ---
  {
    name: "getFileSize returns correct size",
    fn() {
      const file = createUniqueTempFile("size-check", "bin");
      try {
        const data = new Uint8Array(128);
        writeArrayBufferToFile(file, data.buffer);
        assertEquals(getFileSize(file), 128, "file size should be 128");
      } finally {
        safeRemoveFile(file);
      }
    },
  },
  {
    name: "getFileSize returns -1 for non-existent file",
    fn() {
      const file = createTempFile("floorp-no-size.dat");
      assertEquals(getFileSize(file), -1, "should return -1");
    },
  },

  // --- safeRemoveFile ---
  {
    name: "safeRemoveFile removes existing file and returns true",
    fn() {
      const file = createUniqueTempFile("remove-test", "tmp");
      writeArrayBufferToFile(file, new ArrayBuffer(1));
      const result = safeRemoveFile(file);
      assertEquals(result, true, "should return true for removed file");
      assertEquals(file.exists(), false, "file should no longer exist");
    },
  },
  {
    name: "safeRemoveFile returns false for non-existent file",
    fn() {
      const file = createTempFile("floorp-nope.tmp");
      const result = safeRemoveFile(file);
      assertEquals(result, false, "should return false");
    },
  },

  // --- safeRemoveFiles ---
  {
    name: "safeRemoveFiles removes multiple files and returns count",
    fn() {
      const a = createUniqueTempFile("multi-rm-a", "tmp");
      const b = createUniqueTempFile("multi-rm-b", "tmp");
      writeArrayBufferToFile(a, new ArrayBuffer(1));
      writeArrayBufferToFile(b, new ArrayBuffer(1));

      const removed = safeRemoveFiles([a, b]);
      assertEquals(removed, 2, "should remove 2 files");
    },
  },

  // --- ensureFileRemoved ---
  {
    name: "ensureFileRemoved does not throw for non-existent file",
    fn() {
      const file = createTempFile("floorp-ensure-rm.tmp");
      // Should not throw
      ensureFileRemoved(file);
    },
  },

  // --- createInputStream & readInputStream ---
  {
    name: "createInputStream + readInputStream round-trips string via readInputStreamToBuffer",
    fn() {
      const text = "Hello, Floorp!";
      const bytes = new TextEncoder().encode(text);
      const stream = createInputStream(bytes);
      const buffer = readInputStreamToBuffer(stream);
      const decoded = new TextDecoder().decode(new Uint8Array(buffer));
      assertEquals(decoded, text, "should round-trip text through stream");
    },
  },
  {
    name: "createInputStreamFromString + readInputStream round-trips text",
    fn() {
      const text = "Test string with unicode: \u00e9\u00e8\u00ea";
      const stream = createInputStreamFromString(text);
      const result = readInputStream(stream);
      assertEquals(result, text, "should round-trip unicode text");
    },
  },

  // --- Empty data ---
  {
    name: "write and read empty ArrayBuffer",
    fn() {
      const file = createUniqueTempFile("empty-rw", "bin");
      try {
        writeArrayBufferToFile(file, new ArrayBuffer(0));
        assert(file.exists(), "file should exist even when empty");
        const readBack = readFileToArrayBuffer(file);
        assertEquals(readBack.byteLength, 0, "should read back 0 bytes");
      } finally {
        safeRemoveFile(file);
      }
    },
  },
];

export async function runAllTests(): Promise<void> {
  for (const t of tests) {
    try {
      await t.fn();
      console.log(`[PASS] ${t.name}`);
    } catch (e) {
      console.error(`[FAIL] ${t.name}:`, e);
      throw e;
    }
  }
}
