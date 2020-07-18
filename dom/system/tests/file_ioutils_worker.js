// Any copyright is dedicated to the Public Domain.
// - http://creativecommons.org/publicdomain/zero/1.0/

/* eslint-env mozilla/chrome-worker, node */
/* global finish, log*/

"use strict";

importScripts("chrome://mochikit/content/tests/SimpleTest/WorkerSimpleTest.js");
importScripts("resource://gre/modules/ObjectUtils.jsm");

// TODO: Remove this import for OS.File. It is currently being used as a
//       stop gap for missing IOUtils functionality.
importScripts("resource://gre/modules/osfile.jsm");

self.onmessage = async function(msg) {
  const tmpDir = OS.Constants.Path.tmpDir;

  // IOUtils functionality is the same when called from the main thread, or a
  // web worker. These tests are a modified subset of the main thread tests, and
  // serve as a confidence check that the implementation is thread-safe.
  await test_api_is_available_on_worker();
  await test_full_read_and_write();
  await test_move_file();

  finish();
  info("test_ioutils_worker.xhtml: Test finished");

  async function test_api_is_available_on_worker() {
    ok(self.IOUtils, "IOUtils is present in web workers");
  }

  async function test_full_read_and_write() {
    // Write a file.
    const tmpFileName = OS.Path.join(tmpDir, "test_ioutils_numbers.tmp");
    const bytes = Uint8Array.of(...new Array(50).keys());
    const bytesWritten = await self.IOUtils.writeAtomic(tmpFileName, bytes);
    is(
      bytesWritten,
      50,
      "IOUtils::writeAtomic can write entire byte array to file"
    );

    // Read it back.
    let fileContents = await self.IOUtils.read(tmpFileName);
    ok(
      ObjectUtils.deepEqual(bytes, fileContents) &&
        bytes.length == fileContents.length,
      "IOUtils::read can read back entire file"
    );

    const tooManyBytes = bytes.length + 1;
    fileContents = await self.IOUtils.read(tmpFileName, tooManyBytes);
    ok(
      ObjectUtils.deepEqual(bytes, fileContents) &&
        fileContents.length == bytes.length,
      "IOUtils::read can read entire file when requested maxBytes is too large"
    );

    await cleanup(tmpFileName);
  }

  async function test_move_file() {
    const src = OS.Path.join(tmpDir, "test_move_file_src.tmp");
    const dest = OS.Path.join(tmpDir, "test_move_file_dest.tmp");
    const bytes = Uint8Array.of(...new Array(50).keys());
    await self.IOUtils.writeAtomic(src, bytes);

    await self.IOUtils.move(src, dest);
    ok(
      !OS.File.exists(src) && OS.File.exists(dest),
      "IOUtils::move can move files from a worker"
    );

    await cleanup(dest);
  }

  async function cleanup(...files) {
    for (const file of files) {
      await self.IOUtils.remove(file, { ignoreAbsent: true, recursive: true });
      const exists = OS.File.exists(file);
      ok(!exists, `Removed temporary file: ${file}`);
    }
  }
};
